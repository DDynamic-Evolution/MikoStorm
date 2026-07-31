# Changelog

## Unreleased

### Improvements

- **Parallel Octree Culling:** Removed the single-core bottleneck in `updateCull()`. Octree culling is now a two-pass operation: a main-thread occlusion pre-pass (reads back GL queries) followed by a fully parallel frustum-cull across a worker thread pool (`RenderCull`, up to 8 workers), with per-slot scratch result buffers merged on the main thread. HUD and shadow passes fall back to the original serial path. ([llparallelfor.h](indra/llcommon/llparallelfor.h), [pipeline.cpp](indra/newview/pipeline.cpp), [llspatialpartition.cpp](indra/newview/llspatialpartition.cpp))

- **Parallel Render Map Build:** The render map build in `postSort()` no longer runs on a single core. The GL-touching `rebuildGeom()` pass stays on the main thread, but the gathering of draw info and alpha groups is split across the cull worker pool into per-slot scratch buffers that are merged back (preserving the original draw order) before the alpha sort. Triangle statistics are counted atomically. ([pipeline.cpp](indra/newview/pipeline.cpp), [llspatialpartition.cpp](indra/newview/llspatialpartition.cpp))

- **Render Initialization:** Replaced GLH loader with glad for OpenGL 4.5. Removed ~1200 lines of hand-written PFN* declarations. Added glad headers directly to source tree. Fixed crash caused by GL calls before glad loader initialization. ([llgl.cpp](indra/llrender/llgl.cpp), [llglheaders.h](indra/llrender/llglheaders.h), [llwindowsdl2.cpp](indra/llwindow/llwindowsdl2.cpp))

### Bug Fixes

- **Restart Avoider:** Added maximum retry limit (10 attempts) for returning to the original region. Previously the viewer would retry indefinitely every 60 seconds if it could not return, wasting resources. A notification is now shown when the limit is reached. ([fsrestartavoid.h](indra/newview/fsrestartavoid.h), [fsrestartavoid.cpp](indra/newview/fsrestartavoid.cpp))

- **Per-Contact IM Sounds:** Fixed custom IM sounds not playing and not being removable. The `FSPerAccountIMSounds` setting was never declared in `settings_per_account.xml`, causing all `setLLSD`/`getLLSD` calls to silently fail. ([settings_per_account.xml](indra/newview/app_settings/settings_per_account.xml))
