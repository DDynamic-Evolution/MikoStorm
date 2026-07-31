# Parallelization Plan: Octree Culling + State Sort

Goal: Eliminate the single-core bottleneck in `display()` by parallelizing
octree culling, `stateSort()`, and `postSort()`.

---

## Current Architecture (single-threaded)

```
updateCull()   → ONE thread: recursively traverses N octrees, writes to sCull
stateSort()    → ONE thread: iterates sCull groups, enqueues faces into pools
  └─ postSort() → ONE thread: rebuildGeom(), copies mDrawMap → sCull, sorts alpha
renderGeom()   → ONE thread: reads sCull->mRenderMap, issues draw calls
```

All data structures (`LLCullResult`, `LLSpatialGroup::mDrawMap`,
`LLSpatialGroup::mState`) lack mutexes/atomics – designed purely for
single-threaded use. `sCull` is a raw static pointer.

---

## Phase 0 — Infrastructure

### 0.1 Thread-safe local buffers for LLCullResult

`LLCullResult` is the central problem – all threads would write to the same
vectors.

**Solution: Thread-Local Result Buffers**

```cpp
class LLCullResult {
    // ... existing members ...

    struct ThreadLocalBuffer {
        sg_list_t       localVisibleGroups;
        sg_list_t       localDrawableGroups;
        drawable_list_t localVisibleList;
        bridge_list_t   localVisibleBridge;
        sg_list_t       localOcclusionGroups;
        // RenderMap:  [LLRenderPass::NUM_RENDER_TYPES]
        drawinfo_list_t localRenderMap[LLRenderPass::NUM_RENDER_TYPES];
    };
    std::vector<ThreadLocalBuffer> mThreadBuffers;

    void allocateThreadBuffers(U32 count);
    void flushToMain();   // merges all thread buffers into main lists
};
```

Each thread pushes to its own local buffer (no lock needed). After `join()`,
`flushToMain()` copies all local entries into the main vectors on the main
thread.

**Files:** `llcullresult.h`, `llcullresult.cpp`

### 0.2 Render ThreadPool

```cpp
// In llappviewer.cpp or pipeline.cpp
LL::ThreadPool gCullThreadPool("RenderCull", max(1, hardware_cores - 1));
```

Non-blocking WorkQueue-based pool. Tasks are posted via
`gCullThreadPool.getQueue().post(task)`. The main thread
blocks with `WorkQueue::waitForResult()` for completion.

### 0.3 Parallel-for Utility

```cpp
template <typename Iter, typename Func>
void ll_parallel_for(Iter begin, Iter end, Func&& fn,
                     LL::ThreadPool& pool, size_t chunkSize = 16);
```

Splits `[begin, end)` into chunks, posts each chunk as a task.
`waitForResult()` blocks until all chunks are done.

---

## Phase 1 — Parallelize Octree Culling

### Problem

`LLOctreeCull::traverse()` is a recursive depth-first walk through the
entire octree per partition. Each node goes through:

1. `earlyFail()` → `checkOcclusion()` (GL call – **must be on main thread!**)
2. `frustumCheck()` → SIMD AABB test (thread-safe, read-only)
3. `processGroup()` → `gPipeline.markNotCulled()` writes to `sCull`

### Solution: Two-Pass Culling

```
Pass 1 (Main Thread):
  for each partition:
    part->preCheckOcclusion()
    → traverses WITHOUT processGroup(), only earlyFail()
    → checkOcclusion() reads back GL queries and sets OCCLUDED flags
    → NO writes to sCull

Pass 2 (Parallel – gCullThreadPool):
  for each partition:
    part->parallelCull(camera, threadIdx)
    → traverse() WITHOUT earlyFail (occlusion state already known)
    → frustumCheck() + processGroup() writes to thread-local buffer
    → NO GL calls

Merge (Main Thread):
  sCull->flushFromThreadBuffers()
```

### Octree Splitting

**Tier 1 (simple): Split by Partition**
Each `LLSpatialPartition` (Terrain, Static, Avatar, Volume, etc.) is culled
as its own task. Typically 30-80 tasks in dense scenes.

```cpp
void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result, ...)
{
    grabReferences(result);
    sCull->clear();
    sCull->allocateThreadBuffers(numThreads);

    // Pass 1: Occlusion on main thread
    for each region:
        for each partition:
            part->preCheckOcclusion();

    // Pass 2: Parallel frustum culling
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

**Tier 2 (optimization): Split by Octree Subtree**
Split each partition's octree at level K into N subtrees. Each worker culls
one subtree. Disjoint subtrees → no conflicts.

### Changes to LLOctreeCull

```cpp
// NEW: Occlusion-only pre-pass (NO processGroup)
void LLOcclusionPrePass::traverse(const OctreeNode* n)
{
    LLViewerOctreeGroup* group = (LLViewerOctreeGroup*) n->getListener(0);
    group->checkOcclusion();           // GL call – main thread only
    if (group->isOcclusionState(OCCLUDED))
        return;                        // Prune subtree, OCCLUDED flag persists
    OctreeTraveler::traverse(n);       // Recurse only, no processGroup
}

// NEW: Parallel cull without earlyFail
void LLOctreeCull::parallelTraverse(const OctreeNode* n, int threadIdx)
{
    LLViewerOctreeGroup* group = (LLViewerOctreeGroup*) n->getListener(0);
    if (group->isOcclusionState(OCCLUDED))
        return;                        // Fast check, no GL call

    if (mRes == 2 || skip_frustum) {
        OctreeTraveler::traverse(n);
    } else {
        mRes = frustumCheck(group);
        if (mRes) OctreeTraveler::traverse(n);
    }
}

// processGroup writes to thread-local buffer
virtual void processGroup(LLViewerOctreeGroup* base_group, int threadIdx)
{
    LLSpatialGroup* group = (LLSpatialGroup*)base_group;
    gPipeline.markNotCulledLocal(group, *mCamera, threadIdx);
}
```

### Changes to LLPipeline::markNotCulled

```cpp
// NEW: Thread-local variant
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

### Files

| File | Change |
|------|--------|
| `llspatialpartition.h` | `LLOctreeCull::parallelTraverse()`, `LLOcclusionPrePass` |
| `llspatialpartition.cpp` | Implementations, `parallelCull()` |
| `llvieweroctree.h` | `LLViewerOctreeCull::parallelTraverse()` |
| `llvieweroctree.cpp` | Implementation |
| `pipeline.h` | `markNotCulledLocal()`, `gCullThreadPool` |
| `pipeline.cpp` | `updateCull()` changes |

---

## Phase 2 — Parallelize State Sort

### Problem

`stateSort()` iterates all visible groups and drawables, calling
`stateSort(drawable, camera)`. Hot path:
- `drawablep->setVisible()` – writes to drawable state
- `drawablep->updateDistance()` – distance/LOD computation
- `face->getPool()->enqueue(facep)` – writes to **shared** draw pools

### Solution: Parallel Group Processing + Per-Thread Pool Queues

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

    // Phase B: Bridges (main thread – few)
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

    // postSort on MAIN THREAD (merge + alpha sort)
    postSort(camera);
}
```

### Problem: `face->getPool()->enqueue(facep)`

Each `LLDrawPool` has an internal vector/set (`mDrawFace` or similar).

**Solution: Per-Thread Pool Queues**

```cpp
class LLDrawPool {
    // Before:
    std::vector<LLFace*> mDrawFace;

    // NEW:
    std::vector< std::vector<LLFace*> > mThreadDrawFaces;  // [threadIdx]

    void enqueue(LLFace* face, int threadIdx = 0) {
        mThreadDrawFaces[threadIdx].push_back(face);
    }

    void mergeThreadQueues() {
        for (int i = 1; i < (int)mThreadDrawFaces.size(); i++)
            mDrawFace.insert(mDrawFace.end(),
                mThreadDrawFaces[i].begin(), mThreadDrawFaces[i].end());
        // Clear thread queues, keep memory
        for (auto& q : mThreadDrawFaces) q.clear();
    }
};
```

After each parallel block: `mergeThreadQueues()` on the main thread
(each pool merges its per-thread vectors into the main vector).

### Files

| File | Change |
|------|--------|
| `pipeline.cpp` | `stateSort()` changes |
| `pipeline.h` | `markVisibleLocal()`, `stateSortLocal()` |
| `lldrawpool.h` | `mThreadDrawFaces`, `mergeThreadQueues()`, `enqueue(face, threadIdx)` |
| `lldrawpool.cpp` | `mergeThreadQueues()` implementation |

---

## Phase 3 — Parallelize PostSort

### 3.1 `group->rebuildGeom()` (parallel)

Each group builds its `mDrawMap` independently. `genDrawInfo()` allocates
vertex buffers and calls `registerFace()`.

```cpp
ll_parallel_for(sCull->beginDrawableGroups(), sCull->endDrawableGroups(),
    [&](sg_iterator iter, int) {
        LLSpatialGroup* group = *iter;
        if (!group->isDead() && !group->isOcclusionState(OCCLUDED))
            group->rebuildGeom();
    }, gCullThreadPool);
```

**Safety:** `registerFace()` pushes into `group->mDrawMap[passType]` –
since each group has its own `mDrawMap`, no cross-group conflict.

### 3.2 Build render map (parallel)

Copies `group->mDrawMap` → `sCull->mRenderMap[type]`.

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
        // Collect alpha groups
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

`std::sort` on ~100-500 alpha groups. Either on main thread (fast enough)
or with C++17 Parallel Policy:

```cpp
std::sort(std::execution::par,
    sCull->beginAlphaGroups(), sCull->endAlphaGroups(),
    LLSpatialGroup::CompareDepthGreater());
```

---

## Performance Estimate

On an 8-core system (1 main + 7 workers):

| Today (single) | Parallelized | Speedup |
|---|---|---|
| cull: 3-5ms | 0.5-1ms | 5-7x |
| stateSort: 8-15ms | 2-4ms | 4-5x |
| postSort: 5-10ms | 1.5-3ms | 3-5x |
| **Display(): ~20-35ms** | **~8-12ms** | **~3x in dense scenes** |

---

## Risks

| Problem | Solution |
|---------|----------|
| GL calls in checkOcclusion() | Main-thread pre-pass (Phase 1) |
| Data races on mState/mVisible | `std::atomic<U32>` or per-thread write-only + merge |
| Octree modification during cull | `sNoDelete` protects; read-only access is safe |
| Face/pool thread-safety | Per-thread pool queues + merge (Phase 2) |
| Cache thrashing / false sharing | Thread-local storage, padding |
| Load imbalance | Chunk size 8-16, possibly work-stealing |
| `sNoDelete` lifetime too short | Must remain true during ALL parallel work |

---

## Implementation Order

1. **Phase 0**: `LLCullResult` thread-local buffer + `gCullThreadPool` — ✅ done
2. **Phase 3.1**: parallelize `rebuildGeom()` (isolated, simple gain)
3. **Phase 1**: parallel octree culling (Tier 1: partition level) — ✅ done
4. **Phase 2**: parallel state sort + per-thread pool queues
5. **Phase 3.2**: parallel render map construction — ✅ done (rebuildGeom stays on a main-thread pre-pass; draw-info/alpha gathering runs in parallel via `push*Local` + `flushThreadBuffers`; `sIndicesDrawnCount` is atomic)
6. **Phase 1 Tier 2**: octree subtree splitting for better utilization
7. **Fine-tuning**: chunk sizes, thread counts, profiling

---

## Dependencies

| Phase | Required By |
|-------|-------------|
| 0.1 (LLCullResult Buffer) | All phases |
| 0.2 (ThreadPool) | All phases |
| 0.3 (parallel_for) | All phases |
| 1 (Culling) | only 0.1, 0.2, 0.3 |
| 2 (StateSort) | 0.1, 0.2, 0.3 + Phase 1 (sCull populated) |
| 3 (PostSort) | 0.1, 0.2, 0.3 + Phase 1 + Phase 2 |
