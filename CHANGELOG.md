# Changelog

## 1.16.41 (2026-08-29)

### Improvements

- **New Post-Processing FX section (Phase 1 of the Black Dragon graphics port):** a new **Post-Processing FX** group in Preferences → Graphics → Rendering that reimplements several Black Dragon post effects as native MikoStorm features:
  - **Upscaling (`RenderUpscaleEnabled`, off):** renders the 3D scene at a reduced internal resolution (`RenderResolutionMultiplier`, default `1.0`) and upscales back to screen resolution using the existing AMD Contrast Adaptive Sharpening (CAS) pass for true reconstruction upsampling, instead of plain bilinear stretch. Sharpness is adjustable (`RenderUpscaleSharpness`, default `0.4`).
  - **Color grading:** a master `RenderSatContrastStrength` (default `0.0` = off) gating independent saturation (`RenderSaturation`, `1.0`), contrast (`RenderContrast`, `1.0`) and brightness (`RenderBrightness`, `0.0`) controls.
  - **Vignette (`RenderVignetteAmount`, `0.0` = off)** darkens the screen edges.
  - **Film grain (`RenderFilmGrain`, `0.0` = off)** adds a subtle noise grain using a GPU-noise shader permutation.
  - All effects are off by default so the default image is unchanged; each is gated/driven by its own setting. Implemented as a new `postDeferredPostFx.glsl` composite pass chained in right before the final present, with a new `gPostFxProgram` shader program. Cross-platform (OpenGL), no Vulkan dependency. ([pipeline.cpp](indra/newview/pipeline.cpp), [pipeline.h](indra/newview/pipeline.h), [llviewershadermgr.cpp](indra/newview/llviewershadermgr.cpp), [llviewershadermgr.h](indra/newview/llviewershadermgr.h), [postDeferredPostFx.glsl](indra/newview/app_settings/shaders/class1/deferred/postDeferredPostFx.glsl), [settings.xml](indra/newview/app_settings/settings.xml), [panel_preferences_graphics1.xml](indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml))
- **Black Dragon overlay settings groundwork (Phase 0):** re-registered the previously skipped Black Dragon overlay settings keys so they are present and controllable as a foundation for later phases, including motion blur (`RenderMotionBlur`, `RenderMotionBlurStrength`), volumetric lighting (`RenderVolumetricLighting` and resolution/multiplier/falloff), and the AYA cinematic overlay intensity/atmosphere controls (`AYAR14*`, `AYAR16*` aerial perspective, `AYAR17*` color temperature, `AYAR18*` cloud volumetric). ([settings.xml](indra/newview/app_settings/settings.xml), [llviewercontrol.cpp](indra/newview/llviewercontrol.cpp))

## 1.16.40 (2026-08-28)

### Improvements

- **Black Dragon Mouselook Features:** Ported/reenvisioned four Black Dragon mouselook options, all settable in Preferences → Move → Mouselook:
  - **Realistic Mouselook** (`FSRealisticMouselook`, off): binds the mouselook camera to the animated head instead of a fixed head offset, so head bobbing and body motion are naturally followed and the body is no longer held below the camera.
  - **Mouselook Rotate Threshold** (`AvatarRotateThresholdMouselook`, `-1` = default): a separate, user-configurable pelvis-rotate threshold for mouselook, replacing the fixed follow factor.
  - **Head/Eyes Follow** (`FSMouselookHeadTracking`, on): lets the avatar head and eyes follow the mouse cursor in mouselook be disabled.
  - **Experimental First-Person Aiming** (`FSExperimentFirstPersonAiming`, off): plays a first-person aiming animation while in mouselook.
  - ([llagentcamera.cpp](indra/newview/llagentcamera.cpp), [llagent.cpp](indra/newview/llagent.cpp), [llvoavatar.cpp](indra/newview/llvoavatar.cpp), [settings.xml](indra/newview/app_settings/settings.xml), [panel_preferences_move.xml](indra/newview/skins/default/xui/en/panel_preferences_move.xml))

### Bug Fixes

- **Mouselook tab readability and alignment:** The Black Dragon mouselook options in Preferences → Move → Mouselook were not being laid out correctly. They used `left_pad`, which positions a control relative to the previous control's right edge, so they were pushed far to the right. They also overflowed the fixed-height (non-scrollable) tab and got clipped out of view. The whole Mouselook tab is now wrapped in a scroll container and the new options use an absolute left position, aligning with the rest of the options. ([panel_preferences_move.xml](indra/newview/skins/default/xui/en/panel_preferences_move.xml))

## 1.16.38 (2026-08-27)

### Improvements

- **Bone Camera (Camera Follow Joint):** New feature to fix the third-person camera to follow a specified skeleton bone, using the current camera preset offsets rotated by the avatar's facing. Ported from Black Dragon. Controlled by the `CameraFollowJoint` setting (default `-1` = off) with a bone picker added to the standalone Camera Controls floater, the Preferences → Move panel and a new "Bone Camera" sub-tab in the Phototools Cam tab. ([llagentcamera.cpp](indra/newview/llagentcamera.cpp), [llagentcamera.h](indra/newview/llagentcamera.h), [llfloatercamera.cpp](indra/newview/llfloatercamera.cpp), [llfloaterpreference.cpp](indra/newview/llfloaterpreference.cpp), [quickprefs.cpp](indra/newview/quickprefs.cpp), [llviewercontrol.cpp](indra/newview/llviewercontrol.cpp), [settings.xml](indra/newview/app_settings/settings.xml))

### Bug Fixes

- **Missing Bone Camera entries:** The joint picker was populated only once when the floater was built, so if the avatar skeleton had not loaded yet (e.g. Windows / certain locales where the floater opens early), the bone list stayed empty. The list is now rebuilt whenever the joint set changes instead of only once, so the bone camera picker fills in as soon as the avatar is ready. ([llfloatercamera.cpp](indra/newview/llfloatercamera.cpp), [quickprefs.cpp](indra/newview/quickprefs.cpp))

## 1.16.36 (2026-08-13)

### Bug Fixes

- **Revert Parallel Render Pipeline:** Reverted the parallel octree culling, render map build and stateSort changes that caused repeated crashes (teleport, stateSort, cull, postSort) on Windows and Linux. Restored the serial Vanilla pipeline. ([pipeline.cpp](indra/newview/pipeline.cpp), [llspatialpartition.cpp](indra/newview/llspatialpartition.cpp), [lldrawpool.cpp](indra/newview/lldrawpool.cpp))

## 1.16.35 (2026-08-13)

### Bug Fixes

- **Linux Crash on Missing Session Settings:** Fixed a startup crash (SIGSEGV) when account session settings could not be loaded. Absolute settings paths were re-expanded with the settings prefix, doubling the path (`/xandir_foxglove//home/.../settings_per_account.xml`), so session-only controls such as `FSHideLocalChat` and `FSNetworkWarning*` were never registered and their `LLCachedControl`/`getControl()` lookups aborted the viewer. Settings load now skips re-expansion of absolute paths, an empty/unresolvable session settings file falls back to `settings_firestorm.xml`, and the affected controls use defaults instead of aborting. ([llappviewer.cpp](indra/newview/llappviewer.cpp), [fsfloaternearbychat.cpp](indra/newview/fsfloaternearbychat.cpp))
- **Restore Firestorm RLV immediate delete:** Reverted a debug leftover in `RlvForceWear::done` that altered the Firestorm `immediate_delete` behavior. ([rlvhelper.cpp](indra/newview/rlvhelper.cpp))
- **Particle-count Self-Crash:** Fixed a viewer crash when counting particles. ([llviewerpartsim.cpp](indra/newview/llviewerpartsim.cpp))
- **Windows GL Initialization:** Fixed startup GL loading (SDL_GL_GetProcAddress/glad) and NSIS packaging; updated FMOD to 2.03.14. ([llgl.cpp](indra/llrender/llgl.cpp), [llwindowwin32.cpp](indra/llwindow/llwindowwin32.cpp))
- **MCP Fixes:** Fixed MCP tool execution and camera timeline bugs. ([llmcphttp.cpp](indra/newview/llmcphttp.cpp), [llfloatercameratimeline.cpp](indra/newview/llfloatercameratimeline.cpp))

### Improvements

- **Restart Avoidance Panel:** Reworked restart avoidance into a dedicated preferences tab with per-option controls. ([llpanelrestartavoidance.cpp](indra/newview/llpanelrestartavoidance.cpp), [llrestartavoidancemgr.cpp](indra/newview/llrestartavoidancemgr.cpp))
- **Teleport Timeout Retry:** Teleport now retries on timeout and cancels restart avoidance on manual movement; added a circuit timeout toggle. ([llagent.cpp](indra/newview/llagent.cpp), [llcircuit.cpp](indra/llmessage/llcircuit.cpp))
- **Linux Crash Diagnostics:** Added backtrace to the log, kept signal info, and enabled core dumps for crash debugging. ([llappviewerlinux.cpp](indra/newview/llappviewerlinux.cpp), [llapp.cpp](indra/llcommon/llapp.cpp))

## 1.16.24 (2026-08-04)

### Bug Fixes

- **Teleport Crash Fix:** Fixed a crash when teleporting (SIGSEGV on arrival). The parallel state sort (1.16.23) ran the drawable distance/LOD update on cull worker threads, which can call GL (avatar `updateMeshData()`/`flushBuffers()`). That GL-bound work now runs on the main thread via a deferred per-slot pass, preserving the serial draw order. ([pipeline.cpp](indra/newview/pipeline.cpp), [pipeline.h](indra/newview/pipeline.h))

## 1.16.23 (2026-08-04)

### Improvements

- **Parallel State Sort:** `stateSort()` no longer processes every visible group and drawable on a single core. GL-bound work (occlusion query readback via `checkOcclusion()`/`markOccluder()`, and `rebuildMesh()` vertex buffer updates) stays on the main thread as prepasses, while the per-drawable visibility/LOD work and the face-pool enqueues are split across the `RenderCull` worker pool. Faces are written into per-slot `LLFacePool` scratch queues and merged back on the main thread in slot order, preserving the original draw order. Shadow, cube-snapshot and HUD passes fall back to the serial path. ([pipeline.cpp](indra/newview/pipeline.cpp), [lldrawpool.cpp](indra/newview/lldrawpool.cpp))
- **Parallel Octree Culling:** Removed the single-core bottleneck in `updateCull()`. Octree culling is now a two-pass operation: a main-thread occlusion pre-pass (reads back GL queries) followed by a fully parallel frustum-cull across a worker thread pool (`RenderCull`, up to 8 workers), with per-slot scratch result buffers merged on the main thread. HUD and shadow passes fall back to the original serial path. ([llparallelfor.h](indra/llcommon/llparallelfor.h), [pipeline.cpp](indra/newview/pipeline.cpp), [llspatialpartition.cpp](indra/newview/llspatialpartition.cpp))
- **Parallel Render Map Build:** The render map build in `postSort()` no longer runs on a single core. The GL-touching `rebuildGeom()` pass stays on the main thread, but the gathering of draw info and alpha groups is split across the cull worker pool into per-slot scratch buffers that are merged back (preserving the original draw order) before the alpha sort. Triangle statistics are counted atomically. ([pipeline.cpp](indra/newview/pipeline.cpp), [llspatialpartition.cpp](indra/newview/llspatialpartition.cpp))
- **Render Initialization:** Replaced GLH loader with glad for OpenGL 4.5. Removed ~1200 lines of hand-written PFN* declarations. Added glad headers directly to source tree. Fixed crash caused by GL calls before glad loader initialization. ([llgl.cpp](indra/llrender/llgl.cpp), [llglheaders.h](indra/llrender/llglheaders.h), [llwindowsdl2.cpp](indra/llwindow/llwindowsdl2.cpp))

### Bug Fixes

- **Restart Avoider:** Added maximum retry limit (10 attempts) for returning to the original region. Previously the viewer would retry indefinitely every 60 seconds if it could not return, wasting resources. A notification is now shown when the limit is reached. ([llrestartavoidancemgr.h](indra/newview/llrestartavoidancemgr.h), [llrestartavoidancemgr.cpp](indra/newview/llrestartavoidancemgr.cpp))
- **Per-Contact IM Sounds:** Fixed custom IM sounds not playing and not being removable. The `FSPerAccountIMSounds` setting was never declared in `settings_per_account.xml`, causing all `setLLSD`/`getLLSD` calls to silently fail. ([settings_per_account.xml](indra/newview/app_settings/settings_per_account.xml))
