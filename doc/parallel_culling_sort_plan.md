# Plan: Octree Culling + State Sort parallelisieren

Ziel: Den Singlecore-Flaschenhals in `display()` durch Parallelisierung von
Octree-Culling, `stateSort()` und `postSort()` entfernen.

---

## Architektur heute (single-threaded)

```
updateCull()   → EIN Thread: traversiert N Oktrees rekursiv, schreibt in sCull
stateSort()    → EIN Thread: iteriert sCull-Gruppen, enqueued Faces in Pools
  └─ postSort() → EIN Thread: rebuildGeom(), kopiert mDrawMap → sCull, sortiert Alpha
renderGeom()   → EIN Thread: liest sCull->mRenderMap, issued Draw Calls
```

Alle Datenstrukturen (`LLCullResult`, `LLSpatialGroup::mDrawMap`,
`LLSpatialGroup::mState`) sind ohne Mutexe/Atomics – rein single-threaded
ausgelegt. `sCull` ist ein roher static Pointer.

---

## Phase 0 — Infrastruktur

### 0.1 Thread-sichere lokale Puffer für LLCullResult

`LLCullResult` ist das zentrale Problem – alle Threads würden in dieselben
Vektoren schreiben.

**Lösung: Thread-Local Result Buffers**

```cpp
class LLCullResult {
    // ... existing members ...

    struct ThreadLocalBuffer {
        sg_list_t       localVisibleGroups;
        sg_list_t       localDrawableGroups;
        drawable_list_t localVisibleList;
        bridge_list_t   localVisibleBridge;
        sg_list_t       localOcclusionGroups;
        // RenderMap:  dimos[LLRenderPass::NUM_RENDER_TYPES]
        drawinfo_list_t localRenderMap[LLRenderPass::NUM_RENDER_TYPES];
    };
    std::vector<ThreadLocalBuffer> mThreadBuffers;

    void allocateThreadBuffers(U32 count);
    void flushToMain();   // merged alle Thread-Buffer in die Haupt-Listen
};
```

Jeder Thread pusht in seinen eigenen lokalen Buffer (kein Lock nötig).
Nach `join()` aller Threads kopiert `flushToMain()` auf dem Main Thread
alle lokalen Einträge in die Haupt-Vektoren.

**Dateien:** `llcullresult.h`, `llcullresult.cpp`

### 0.2 Render-ThreadPool

```cpp
// In llappviewer.cpp oder pipeline.cpp
LL::ThreadPool gCullThreadPool("RenderCull", max(1, hardware_cores - 1));
```

Nicht-blockierender WorkQueue-basierter Pool. Tasks werden via
`gCullThreadPool.getQueue().post(task)` eingestellt. Der Main Thread
wartet mit `WorkQueue::waitForResult()` auf Completion.

### 0.3 Parallel-for Utility

```cpp
template <typename Iter, typename Func>
void ll_parallel_for(Iter begin, Iter end, Func&& fn,
                     LL::ThreadPool& pool, size_t chunkSize = 16);
```

Teilt `[begin, end)` in Chunks, posted jeden Chunk als Task.
`waitForResult()` blockiert, bis alle Chunks erledigt sind.

---

## Phase 1 — Octree Culling parallelisieren

### Problem

`LLOctreeCull::traverse()` ist ein rekursiver Depth-First-Walk durch den
gesamten Oktree pro Partition. Jeder Knoten durchläuft:

1. `earlyFail()` → `checkOcclusion()` (GL-Aufruf – **muss auf Main Thread!**)
2. `frustumCheck()` → SIMD AABB-Test (thread-safe, nur lesend)
3. `processGroup()` → `gPipeline.markNotCulled()` schreibt in `sCull`

### Lösung: Zwei-Pass-Culling

```
Pass 1 (Main Thread):
  for each partition:
    part->preCheckOcclusion()
    → traversiert OHNE processGroup(), nur earlyFail()
    → checkOcclusion() liest GL Queries aus und setzt OCCLUDED-Flags
    → KEIN Schreiben in sCull

Pass 2 (Parallel – gCullThreadPool):
  for each partition:
    part->parallelCull(camera, threadIdx)
    → traverse() OHNE earlyFail (Occlusion-State ist schon gesetzt)
    → frustumCheck() + processGroup() schreibt in Thread-local Buffer
    → KEINE GL-Aufrufe

Merge (Main Thread):
  sCull->flushFromThreadBuffers()
```

### Octree-Splitting

**Stufe 1 (einfach): Split by Partition**
Jede `LLSpatialPartition` (Terrain, Static, Avatar, Volume, etc.) wird als
eigener Task gecullt. Typisch 30-80 Tasks in dichten Szenen.

```cpp
void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result, ...)
{
    grabReferences(result);
    sCull->clear();
    sCull->allocateThreadBuffers(numThreads);

    // Pass 1: Occlusion auf Main Thread
    for each region:
        for each partition:
            part->preCheckOcclusion();

    // Pass 2: Paralleles Frustum-Culling
    collect all visible partitions into `tasks`;
    ll_parallel_for(tasks.begin(), tasks.end(),
        [&](LLSpatialPartition* part, int threadIdx) {
            part->parallelCull(camera, threadIdx);
        }, gCullThreadPool);

    // Merge
    sCull->flushThreadBuffers();

    // Sky drawables (unchanged)
    ...
}
```

**Stufe 2 (Optimierung): Split by Octree-Subtree**
Oktree jeder Partition auf Ebene K in N Subtrees aufteilen. Jeder Worker
cullt einen Subtree. Disjunkte Subtrees → keine Konflikte.

### Änderungen in LLOctreeCull

```cpp
// NEU: Occlusion-only pre-pass (KEIN processGroup)
void LLOcclusionPrePass::traverse(const OctreeNode* n)
{
    LLViewerOctreeGroup* group = (LLViewerOctreeGroup*) n->getListener(0);
    group->checkOcclusion();           // GL-Aufruf – main thread only
    if (group->isOcclusionState(OCCLUDED))
        return;                        // Prune subtree, OCCLUDED-Flag bleibt
    OctreeTraveler::traverse(n);       // Nur Rekursion, kein processGroup
}

// NEU: Parallel-Cull ohne earlyFail
void LLOctreeCull::parallelTraverse(const OctreeNode* n, int threadIdx)
{
    LLViewerOctreeGroup* group = (LLViewerOctreeGroup*) n->getListener(0);
    if (group->isOcclusionState(OCCLUDED))
        return;                        // Schnelle Prüfung, kein GL-Aufruf

    if (mRes == 2 || skip_frustum) {
        OctreeTraveler::traverse(n);
    } else {
        mRes = frustumCheck(group);
        if (mRes) OctreeTraveler::traverse(n);
    }
}

// processGroup schreibt in Thread-local Buffer
virtual void processGroup(LLViewerOctreeGroup* base_group, int threadIdx)
{
    LLSpatialGroup* group = (LLSpatialGroup*)base_group;
    gPipeline.markNotCulledLocal(group, *mCamera, threadIdx);
}
```

### Änderungen in LLPipeline::markNotCulled

```cpp
// NEU: Thread-lokale Variante
void LLPipeline::markNotCulledLocal(LLSpatialGroup* group, LLCamera& camera,
                                    int threadIdx)
{
    if (group->isEmpty()) return;
    group->setVisible();
    group->updateDistance(camera);

    if (!group->getSpatialPartition()->mRenderByGroup)
        sCull->pushDrawableGroupLocal(group, threadIdx);
    else
        sCull->pushVisibleGroupLocal(group, threadIdx);

    if (group->needsUpdate() || ...)
        sCull->pushOcclusionGroupLocal(group, threadIdx);
}
```

### Dateien

| Datei | Änderung |
|-------|----------|
| `llspatialpartition.h` | `LLOctreeCull::parallelTraverse()`, `LLOcclusionPrePass` |
| `llspatialpartition.cpp` | Implementierungen, `parallelCull()` |
| `llvieweroctree.h` | `LLViewerOctreeCull::parallelTraverse()` |
| `llvieweroctree.cpp` | Implementierung |
| `pipeline.h` | `markNotCulledLocal()`, `gCullThreadPool` |
| `pipeline.cpp` | `updateCull()` Änderungen |

---

## Phase 2 — State Sort parallelisieren

### Problem

`stateSort()` iteriert alle sichtbaren Gruppen und Drawables und ruft
`stateSort(drawable, camera)` auf. Hot Path:
- `drawablep->setVisible()` – schreibt in Drawable-State
- `drawablep->updateDistance()` – Distance/LOD-Berechnung
- `face->getPool()->enqueue(facep)` – schreibt in **shared** Draw-Pools

### Lösung: Parallele Gruppen-Verarbeitung + Per-Thread Pool Queues

```cpp
void LLPipeline::stateSort(LLCamera& camera, LLCullResult& result)
{
    grabReferences(result);
    sCull->allocateThreadBuffers(numThreads);

    // Phase A: Drawable groups (parallel)
    ll_parallel_for(sCull->beginDrawableGroups(), sCull->endDrawableGroups(),
        [&](sg_iterator iter, int threadIdx) {
            LLSpatialGroup* group = *iter;
            if (group->isDead() || group->isOcclusionState(OCCLUDED)) return;
            group->setVisible();
            for (auto i = group->getDataBegin(); ...)
                markVisibleLocal(drawablep, camera, threadIdx);
            group->rebuildMesh();
        }, gCullThreadPool);
    sCull->flushThreadBuffers();

    // Phase B: Bridges (main thread – wenige)
    for (auto i = sCull->beginVisibleBridge(); ...)
        stateSort(bridge, camera, fov_changed);

    // Phase C: Visible groups (parallel)
    ll_parallel_for(sCull->beginVisibleGroups(), sCull->endVisibleGroups(),
        [&](sg_iterator iter, int threadIdx) {
            LLSpatialGroup* group = *iter;
            if (group->isDead() || group->isOcclusionState(OCCLUDED)) return;
            group->setVisible();
            stateSortLocal(group, camera, threadIdx);
            group->rebuildMesh();
        }, gCullThreadPool);
    sCull->flushThreadBuffers();

    // Phase D: Individual drawables (parallel)
    ll_parallel_for(sCull->beginVisibleList(), sCull->endVisibleList(),
        ..., gCullThreadPool);
    sCull->flushThreadBuffers();

    // postSort auf MAIN THREAD (merge + alpha sort)
    postSort(camera);
}
```

### Problem: `face->getPool()->enqueue(facep)`

Jeder `LLDrawPool` hat einen `std::set<LLFace*>`- oder Vektor-internen State.

**Lösung: Per-Thread Pool Queues**

```cpp
class LLDrawPool {
    // Bisher:
    std::vector<LLFace*> mDrawFace;

    // NEU:
    std::vector< std::vector<LLFace*> > mThreadDrawFaces;  // [threadIdx]

    void enqueue(LLFace* face, int threadIdx = 0) {
        mThreadDrawFaces[threadIdx].push_back(face);
    }

    void mergeThreadQueues() {
        for (int i = 1; i < (int)mThreadDrawFaces.size(); i++)
            mDrawFace.insert(mDrawFace.end(),
                mThreadDrawFaces[i].begin(), mThreadDrawFaces[i].end());
        // Thread-Queues leeren, Speicher behalten
        for (auto& q : mThreadDrawFaces) q.clear();
    }
};
```

Nach jedem parallel-Block: `mergeThreadQueues()` auf dem Main Thread
(jeder Pool merged seine per-Thread Vektoren in den Haupt-Vektor).

### Dateien

| Datei | Änderung |
|-------|----------|
| `pipeline.cpp` | `stateSort()` Änderungen |
| `pipeline.h` | `markVisibleLocal()`, `stateSortLocal()` |
| `lldrawpool.h` | `mThreadDrawFaces`, `mergeThreadQueues()`, `enqueue(face, threadIdx)` |
| `lldrawpool.cpp` | `mergeThreadQueues()` Implementierung |

---

## Phase 3 — PostSort parallelisieren

### 3.1 `group->rebuildGeom()` (parallel)

Jede Gruppe baut ihre `mDrawMap` unabhängig. `genDrawInfo()` allokiert
Vertex-Buffer und ruft `registerFace()` auf.

```cpp
ll_parallel_for(sCull->beginDrawableGroups(), sCull->endDrawableGroups(),
    [&](sg_iterator iter, int) {
        LLSpatialGroup* group = *iter;
        if (!group->isDead() && !group->isOcclusionState(OCCLUDED))
            group->rebuildGeom();
    }, gCullThreadPool);
```

**Sicherheit:** `registerFace()` pusht in `group->mDrawMap[passType]` –
da jede Gruppe einen eigenen `mDrawMap` hat, kein Konflikt zwischen Gruppen.

### 3.2 Render Map aufbauen (parallel)

Kopiert `group->mDrawMap` → `sCull->mRenderMap[type]`.

```cpp
ll_parallel_for(sCull->beginVisibleGroups(), sCull->endVisibleGroups(),
    [&](sg_iterator iter, int threadIdx) {
        LLSpatialGroup* group = *iter;
        if (group->isDead() || group->isOcclusionState(OCCLUDED)) return;
        for (auto& [type, vec] : group->mDrawMap) {
            if (!hasRenderType(type)) continue;
            for (LLDrawInfo* info : vec)
                sCull->pushDrawInfoLocal(type, info, threadIdx);
        }
        // Alpha-Gruppen sammeln
        if (hasRenderType(RENDER_TYPE_PASS_ALPHA)) {
            if (group->mDrawMap.count(PASS_ALPHA))
                sCull->pushAlphaGroupLocal(group, threadIdx);
            if (group->mDrawMap.count(PASS_ALPHA_RIGGED))
                sCull->pushRiggedAlphaGroupLocal(group, threadIdx);
        }
    }, gCullThreadPool);
sCull->flushThreadBuffers();
```

### 3.3 Alpha sort (main thread)

`std::sort` auf ca. 100-500 Alpha-Gruppen. Entweder main thread (schnell)
oder mit C++17 Parallel Policy:

```cpp
std::sort(std::execution::par,
    sCull->beginAlphaGroups(), sCull->endAlphaGroups(),
    LLSpatialGroup::CompareDepthGreater());
```

---

## Performance-Schätzung

Auf einem 8-Core-System (1 Main + 7 Worker):

| Heute (single) | Parallelisiert | Speedup |
|---|---|---|
| cull: 3-5ms | 0.5-1ms | 5-7x |
| stateSort: 8-15ms | 2-4ms | 4-5x |
| postSort: 5-10ms | 1.5-3ms | 3-5x |
| **Display(): ~20-35ms** | **~8-12ms** | **~3x in dichten Szenen** |

---

## Risiken

| Problem | Lösung |
|---------|--------|
| GL-Aufrufe in checkOcclusion() | Main-Thread-Vorbehandlung (Phase 1) |
| Data Races auf mState/mVisible | `std::atomic<U32>` oder per-Thread Write-only + Merge |
| Octree-Modifikation während Cull | `sNoDelete` schützt; reine Lesezugriffe parallel safe |
| Faces/Pools Thread-Safety | Per-Thread Pool-Queues + Merge (Phase 2) |
| Cache-Thrashing / False-Sharing | Thread-Local Storage, Padding |
| Load Imbalance | Chunk-Größe 8-16, ggf. Work-Stealing |
| `sNoDelete` zu kurz | Muss während ALLER parallelen Arbeiten true bleiben |

---

## Implementierungs-Reihenfolge

1. **Phase 0**: `LLCullResult` Thread-Local Buffer + `gCullThreadPool`
2. **Phase 3.1**: `rebuildGeom()` parallelisieren (isolierter, einfacher Gewinn)
3. **Phase 1**: Octree Culling parallel (Stufe 1: Partition-Ebene)
4. **Phase 2**: State Sort parallel + Per-Thread Pool Queues
5. **Phase 3.2**: Render Map parallel aufbauen
6. **Phase 1 Stufe 2**: Octree-Subtree-Splitting für bessere Auslastung
7. **Fine-Tuning**: Chunk-Größen, Thread-Zahlen, Profiling

---

## Abhängigkeiten

| Phase | Benötigt von |
|-------|-------------|
| 0.1 (LLCullResult Buffer) | Alle Phasen |
| 0.2 (ThreadPool) | Alle Phasen |
| 0.3 (parallel_for) | Alle Phasen |
| 1 (Culling) | nur 0.1, 0.2, 0.3 |
| 2 (StateSort) | 0.1, 0.2, 0.3 + Phase 1 (sCull befüllt) |
| 3 (PostSort) | 0.1, 0.2, 0.3 + Phase 1 + Phase 2 |
