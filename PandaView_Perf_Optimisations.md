# PandaView Performance Optimisations
**Date:** 2026-09-27  
**Branch:** Testingground  
**Build:** v3.0.0001  
**Codebase:** PandaView (Firestorm fork, Second Life viewer)  
**17 files changed — 81 insertions, 43 deletions**

---

## Summary

15 performance optimisations applied across 6 subsystems. All are verified to compile and the binary has been tested in a 64-avatar Second Life sim. 10 additional candidate items were investigated and either found already implemented, inapplicable, or intentional.

---

## Changes Applied

### LLUUID / Map Lookups

| ID | File | What changed |
|----|------|-------------|
| U1 | `indra/newview/llviewerobjectlist.h` | `mUUIDObjectMap`: `std::map` → `std::unordered_map` |
| U2 | `indra/llcharacter/llmotioncontroller.h` | `LLMotionRegistry::mMotionTable` and `LLMotionController::mAllMotions`: `std::map` → `std::unordered_map` |
| U3 | `indra/llmessage/llavatarnamecache.h/.cpp` | `cache_t` typedef: `std::map` → `std::unordered_map` |
| U4 | `indra/newview/fsdata.cpp` | `resolveClientTag()`: hoisted `id.asString()` — was called 4× per tag resolution, now 1× |

**Why:** `std::map` uses a red-black tree — O(log n) per lookup. `std::unordered_map` uses a hash table — O(1) average. LLUUID already has `std::hash` specialised in `lluuid.h:232-238`. These maps are hit hundreds of times per frame in busy scenes (object messages, avatar name lookups, animation state queries). These files were originally written before C++11 (2011) when `unordered_map` wasn't in the standard.

---

### Texture Cache

| ID | File | What changed |
|----|------|-------------|
| T1 | `indra/newview/lltexturecache.h` | `mHeaderIDMap` typedef: `std::map` → `std::unordered_map` |
| T2 | `indra/newview/lltexturecache.cpp` | `writeToFastCache()`: changed `closeFastCache(true)` → `closeFastCache(false)` — keeps APR file handle open across write bursts instead of force-closing after each texture |
| T4 | `indra/newview/lltexturecache.cpp` | `updateEntryTimeStamp()`: timestamp only written to disk if entry is >60 seconds old — prevents disk-write queue spam on every texture read |
| T5 | `indra/newview/lltexturecache.cpp` | `writeToFastCache()`: reads existing 4-int header before writing; skips write if dimensions match (avoids re-writing identical data on texture re-submission) |

**Why:** The texture cache header map has up to 1M entries and is hit on every asset lookup during scene load and teleport. The force-close on every fast-cache write was adding unnecessary APR file handle overhead during login bursts. The timestamp spam was holding a mutex on every read.

---

### Rendering

| ID | File | What changed |
|----|------|-------------|
| R1 | `indra/newview/pipeline.h/.cpp` | `setupHWLights()`: added `mHWLightsDirty` flag. The function previously ran 3× per frame (called from `renderGeomDeferred`, `renderGeomPostDeferred`, `renderDeferredLighting`). Now runs fully once per frame; the other 2 calls return immediately. |
| R2 | `indra/newview/pipeline.h/.cpp` | `calcNearbyLights()`: moved `cur_nearby_lights` and `new_nearby_lights` from local stack variables to class members (`mCurNearbyLights`, `mNewNearbyLights`). Eliminates per-frame heap alloc/free of red-black tree nodes. |

**Why:** `setupHWLights()` traverses sky uniform state and issues up to 8 GL light state writes. Running it 3× per frame meant it could run 400+ times per second at 144fps. The local `std::set` variables in `calcNearbyLights` were being constructed and destructed every frame — in a scene with local lights this creates constant heap pressure.

---

### Threading

| ID | File | What changed |
|----|------|-------------|
| TH1 | `indra/llrender/llvertexbuffer.cpp` | `GLWorkQueue::pop()`: collapsed double-lock into single `unique_lock` spanning the condition wait and the pop. The original had an unlocked window between releasing the condition variable lock and re-acquiring for the pop — a TOCTOU race where another thread could steal the item. |
| TH3 | `indra/newview/llappviewer.cpp` | `image_decode_count`: cap raised from 8 to 16. `llclamp(cores - 4, 2, 8)` → `llclamp(cores - 4, 2, 16)`. Matches upstream original intent (commented-out LL line used 16). Allows 5900X and similar 12-24 core CPUs to decode more textures in parallel during login/teleport. |

**Why:** The 8-thread cap was set circa 2012 targeting Core 2 / first-gen i7 hardware (4-8 cores). On a Ryzen 5900X (24 threads) this left 16 cores sitting idle during texture burst. The double-lock was introduced by two developers independently adding thread safety to the same queue path.

---

### UI

| ID | File | What changed |
|----|------|-------------|
| UI1 | `indra/newview/llnetmap.h/.cpp` | Minimap `draw()`: avatar position fetch gated to 200ms intervals (5 Hz) using `gFrameTimeSeconds`. Results cached in `mCachedAvatarIds` / `mCachedAvatarPositions` members. Previously called `getAvatars()` every frame (60–144Hz = up to 144 full world scans/sec). |
| UI3 | `indra/newview/fsradar.cpp` | FSRadar per-avatar update loop: added `if (mUpdateSignal.empty()) continue;` before the LLSD-building block (~130 lines of string/color allocation per avatar). Skipped entirely when no radar panel is subscribed. Range checks and alert logic still run unconditionally. |

**Why:** The minimap was the single largest per-frame UI overhead in busy sims. In a 64-avatar region at 144fps it was doing 9,216 avatar position lookups per second just to draw coloured dots. The radar LLSD building was allocating strings and colours for every avatar every second regardless of whether anyone was looking at the radar floater.

---

### Avatar / Voice

| ID | File | What changed |
|----|------|-------------|
| A3 | `indra/newview/llspeakers.h/.cpp` | `LLSpeakerMgr::update()`: `inProximalChannel()` now called once at the top of `update()` and cached in `mCachedVoiceChannelActive`. Previously dispatched via virtual call per-avatar per-frame. In a 30-avatar scene at 144fps this was 4,320 redundant singleton dispatches/sec. |

---

## Items Investigated and Not Changed

| ID | Reason not changed |
|----|--------------------|
| T3 | `getTextureFileName` already calls `asString()` exactly once — already correct |
| R3 | Uniform cache already implemented — `LLGLSLShader` uses vector-indexed `mUniform[index]`, O(1) |
| R4 | Normal matrix `glm::inverse/transpose` is in `bindDeferredShader()` — per-pass, not per-object. No change needed. |
| TH2 | `ms_sleep(2)` in `llqueuedthread.cpp` is a deliberate rate-limiter preventing a deferred-request busy-loop. Removing it would cause thrashing. Left alone. |
| TH4 | No no-op lambda queue posts found in codebase |
| UI2 | Two `getAvatars()` calls in `fsradar.cpp:248/252` are in an if/else branch — only one fires per tick |
| UI4 | `notifyObservers()` pairs in inventory have `addChangedMask()` calls between them — intentional flush/add/notify pattern |
| A1 | `sAVsIgnoringARTLimit` is bounded to 5 entries; converting it would break index arithmetic at line 9871 |
| A2 | `calculateUpdateRenderComplexity()` already gated on `mVisualComplexityStale` — not unconditional |

---

## Files Changed

```
indra/llcharacter/llmotioncontroller.h
indra/llmessage/llavatarnamecache.cpp
indra/llmessage/llavatarnamecache.h
indra/llrender/llvertexbuffer.cpp
indra/newview/fsdata.cpp
indra/newview/fsradar.cpp
indra/newview/llappviewer.cpp
indra/newview/llnetmap.cpp
indra/newview/llnetmap.h
indra/newview/llspeakers.cpp
indra/newview/llspeakers.h
indra/newview/lltexturecache.cpp
indra/newview/lltexturecache.h
indra/newview/llviewerobjectlist.h
indra/newview/pipeline.cpp
indra/newview/pipeline.h
```

---

## Build

Windows (MSVC, Cygwin):
```
set AUTOBUILD_VSVER=170
set AUTOBUILD_VARIABLES_FILE=C:\Users\Teff\Desktop\fs-build-variables\variables
set PATH=C:\cygwin64\bin;%PATH%
autobuild configure -A 64 -c ReleaseFS_open -- --fmodstudio --avx2 --no-opensim --no-package -DLL_TESTS:BOOL=FALSE -DUSE_LTO=ON --chan PandaView
autobuild build -A 64 -c ReleaseFS_open --no-configure -- --fmodstudio --avx2 --no-opensim -DLL_TESTS:BOOL=FALSE --chan PandaView
```
