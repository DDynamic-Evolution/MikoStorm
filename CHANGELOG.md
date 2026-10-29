# Changelog

## 2.0.5 (2026-10-29)

- Add an optional Lovense window and Toybox/Communicate entry with English/German UI, QR pairing, device/battery list and explicit pattern start/pause/stop.
- Default to disconnected, require device selection, cap intensity and command duration, stop on window close/logout/disconnect, and keep session credentials in memory only. Use existing verified TLS/network dependencies and an owner-side session-token utility; do not embed third-party developer credentials.
- Incrementally compile and link Windows x64 AVX2 through CMD/VS2022 with OpenSim, FMOD 2.03.14, 3D Stream and MCP. Build the 2.0.5 NSIS installer and verify its integrity, version and payload, including English/German Lovense controls, settings, notices and current browser/voice runtimes. No MCP tests or viewer startup.
- Confirm English button/status/error/help text and English descriptions of all five Lovense settings. Prior focused validation passed 42 protocol and six integration checks; real pairing/device control remains untested. Manual pattern playback does not synchronize avatar animations.

## 2.0.4 (2026-10-29)

- Update Dullahan to 1.50/Chromium 152 and WebRTC to m144 with matching package hashes; retain FMOD 2.03.14.
- Include voice device/default-device/replug, microphone lifecycle and shutdown fixes, a dedicated voice queue and bounded connection draining. Add the live StopMicCaptureWhenMuted debug setting; preserve existing server-mute behavior.
- Deliver native conversation tabs for local chat, IMs and groups with stable ordering, unread indicators, separate drafts and reading state.
- Keep Autoscroll following incoming messages while the popout is inactive; deliberate scrolling or selection pauses it and **Latest** resumes following.
- Continue showing accepted local messages in the popout with **Hide Chat** enabled, preserving mute, replay, session and RLV filtering.
- Preserve text/background colors when changing font size or applying DPI updates.
- Incrementally build and link Windows x64 AVX2 through CMD/VS2022 with OpenSim, FMOD, 3D Stream and MCP. Compile the NSIS installer and verify its integrity, version and payload, including the new browser/voice runtimes. No MCP tests or viewer startup.
- Prior focused validation passed six audio/dependency source checks, 15 native-chat integration checks, 40 native chat/autoscroll checks and theme/font rendering regressions. Live in-world validation remains pending.

## 2.0.3 (2026-10-28, package verification pending)

- **Popout conversation tabs:** replace the conversation dropdown with native Windows tabs for local chat, private IMs and groups. Preserve tab order when unread counts change and use overflow arrows for more conversations.
- **Autoscroll:** follow new messages by default, pause when scrolling up or selecting text, and keep a separate reading position and preference for each conversation. Add an Autoscroll checkbox and **Latest** button with a bounded new-message count.
- Preserve reading anchors and selections when retained history moves; discard expired positions safely on history gaps or account/conversation changes.
- Add focused hidden-window tab/scroll regressions and extend native theme rendering checks. The earlier tab/autoscroll sources were compiled and the AVX2 viewer linked through CMD/VS2022. NSIS packaging was interrupted before final confirmation, payload verification and delivery; this is not a verified installer release.

## 2.0.2 (2026-10-28)

- **Automatic Windows appearance:** the chat popout follows the Windows application Light/Dark setting and updates while open. High Contrast uses system colors; a dark title bar is used where supported.
- **Friends sidebar:** show online status, sort online friends first and search names. Double-click a friend, press Enter or use **Open chat** to select an existing IM or start a new one.
- **Conversation state:** preserve separate drafts and histories while switching chats; clear friend data on account changes and reject requests for removed contacts.
- **Windows installer:** x64 AVX2 with OpenSim, FMOD, 3D Stream audio and optional MCP support. The embedded browser is enabled by default.

## 2.0.1 (2026-10-28)

- Add private IM and group conversations to the native Windows chat selector, with separate bounded transcripts, drafts and native unread indicators.
- Mirror accepted live messages and regular own echoes; exclude replay, announcements and duplicate IM forwarding into local chat.
- Reuse the existing IM send path and retain RLV, text capability, initialization, support-contact and moderator checks. Reject stale conversation/account commands.
- Add the translated Windows Chat Toybox button with open/focus, running-state and platform checks.
- Incrementally build Windows x64 AVX2 with MCP, OpenSim, FMOD and 3D Stream; create and verify the NSIS installer, including the new Toybox command.
- Pass 203 core checks (including the 11 new conversation cases), nine avatar-log checks, eight camera checks and 19 Python integration checks. In-world/native window behavior remains to be validated manually.

## 1.16.59 (2026-10-28)

### Native Windows local chat

- Add a separate native Windows local-chat window with dedicated message loop, input, whisper/shout, selectable text/HTTP links, font size, topmost mode and saved monitor placement.
- Open/focus it from Communicate or the nearby-chat toolbar, using translated English/German labels.
- Mirror the accepted, unmuted current-session local-chat path; retain RLVa filtering and normal viewer sending rules. Bound the transcript/queues and invalidate drafts/commands across logout/account changes.
- Include six transcript regression cases and five static wiring checks; compiled and tested with the AVX2 release. Native window interaction and in-world chat still need manual validation.


### MCP

- Fix repeat sit, validate walking/flying, resolve teleport regions outside the loaded neighborhood and serialize notecards correctly.
- Share argument validation, per-client permissions and structured output across HTTP and media-plugin tools; paginate inventory/object/audio queries.
- Add JSON-only MCP session/version lifecycle, exact JSON-RPC IDs, separate health endpoint, concurrent requests and queued-request cancellation.
- Track teleport/notecard status and best-effort cancellation, including session expiry/revocation and late simulator effects.
- Add live English/German MCP controls, token generation, hashed named-client tokens, grant/revoke UI and recent-call audit.
- Expose bone camera, presets/timeline, local 3D sound mute/readout and measured FPS.
- Validate with 192 core checks, avatar-log/camera regressions, four native MCP validator groups, 15 Python integration checks and a read-only HTTP smoke run without login.
- Build Windows x64 AVX2 with OpenSim, FMOD, 3D Stream and MCP; create and inspect the NSIS installer. Embedded browser stays enabled by default. Legacy/NoAVX remains 1.16.58.
- Correct the audit-list size conversion and empty-list bounds found during compilation.


## 1.16.58 (2026-10-27)

- MCP: bounded HTTP header/body parsing, partial-send handling, origin/host validation, request deadlines and cancellation of queued main-thread work during shutdown.
- Camera: shared numeric joint selection in Camera Controls, Phototools and Preferences; refresh late skeletons and keep the active selection stable.
- Asset checks: handle optional name/description output parameters safely while preserving grid-specific permission rules.
- Avatar log: record successful writes only, retry failed writes with backoff, disconnect pending name lookups and reload deduplication on path changes.
- 3D audio: separate tag parsing and discovery from stream lifecycle; budget object/structure scans and add discovery timing instrumentation; reject non-finite/malformed numeric tags.
- Windows: independent AVX2/legacy CMake presets and CMD build/packaging workflow with regression tests and SHA-256 output.

## 1.16.57 (2026-10-27)

### Bug Fixes

- Improve permission checks for texture saving and UUID previews, including asynchronous write callbacks.
- Require server-granted administrator access for appearance diagnostics and their file-write callback.

## 1.16.56 (2026-10-27)

### Bug Fixes

- **Bone Camera in Camera Controls:** Fixed the joint dropdown being drawn outside its parent panel, which made it unclickable in the compact camera controls used by the MikoStorm skin. The selector now has its own full-width row and stays inside the window at minimum size. Joint selections use numeric IDs consistently and are no longer reset every frame while browsing. The German translation inherits the corrected layout.

## 1.16.49 (2026-09-15)

### Bug Fixes

- **MikoStorm Visual Effects off by default:** The cinematic visual effects chain (color grading, vignette, film grain, chromatic aberration, AYA atmosphere/aerial/cloud/color temperature, volumetric light scattering and camera motion blur) is now disabled by default because it caused problems for other users. New installations start with `FSVisualEffectsEnabled = 0`; it can still be turned on via World → MikoStorm Visual Effects (also available in the German skin as Welt → MikoStorm visuelle Effekte). ([settings.xml](indra/newview/app_settings/settings.xml), [pipeline.cpp](indra/newview/pipeline.cpp), [menu_viewer.xml](indra/newview/skins/default/xui/en/menu_viewer.xml))

## 1.16.52 (2026-09-30)

### Improvements

- **Color Vision Deficiency Filter:** New option in Preferences → Graphics → Rendering that simulates the four common types of color blindness — protanopia, deuteranopia, tritanopia and monochromacy — using the Machado (2009) matrix-based simulations in a new post-processing shader pass. The `FSColorVisionFilter` setting (`0` = off) is exposed as a combo box labeled „Color Vision Filter" in the English skin and „Farbenblind-Filter" in the German skin (12 languages overall; the Azerbaijan skin inherits the English entry via fallback). Monochromacy maps to luminance (`0.2126, 0.7152, 0.0722`); the filter runs right before HDR clamp and is gated by the global MikoStorm Visual Effects switch. ([settings.xml](indra/newview/app_settings/settings.xml), [pipeline.h](indra/newview/pipeline.h), [pipeline.cpp](indra/newview/pipeline.cpp), [postDeferredPostFx.glsl](indra/newview/app_settings/shaders/class1/deferred/postDeferredPostFx.glsl), [panel_preferences_UI.xml](indra/newview/skins/default/xui/en/panel_preferences_UI.xml))
- **Performance Optimisations (PandaView):** 15 optimizations across 6 subsystems, verified in a 64-avatar test sim:
  - **Map → unordered_map:** `LLUUID` object map, motion registry/controller tables and avatar name cache now use hash tables (O(1)) instead of red-black trees (O(log n)); `resolveClientTag()` builds the `id` string only once. ([llviewerobjectlist.h](indra/newview/llviewerobjectlist.h), [llmotioncontroller.h](indra/llcharacter/llmotioncontroller.h), [llavatarnamecache.h](indra/llmessage/llavatarnamecache.h), [llavatarnamecache.cpp](indra/llmessage/llavatarnamecache.cpp), [fsdata.cpp](indra/newview/fsdata.cpp))
  - **Texture cache:** header map switched to hash table, fast-cache file handle is kept open across write bursts, timestamps are only persisted for entries older than 60 s, and identical header writes are skipped. ([lltexturecache.h](indra/newview/lltexturecache.h), [lltexturecache.cpp](indra/newview/lltexturecache.cpp))
  - **Rendering:** `setupHWLights()` now runs fully only once per frame via a new `mHWLightsDirty` flag (was 3×/frame), and `calcNearbyLights()` reuses class-member sets instead of allocating local `std::set`s every frame. ([pipeline.h](indra/newview/pipeline.h), [pipeline.cpp](indra/newview/pipeline.cpp))
  - **Threading:** `GLWorkQueue::pop()` collapsed a double-lock (TOCTOU race window) into a single `unique_lock`; `image_decode_count` cap raised from 8 to 16 so modern high-core CPUs decode more textures in parallel during login/teleport. ([llvertexbuffer.cpp](indra/llrender/llvertexbuffer.cpp), [llappviewer.cpp](indra/newview/llappviewer.cpp))
  - **UI:** minimap avatar positions are fetched at 5 Hz instead of every frame; FSRadar skips its ~130-line LLSD building block entirely when no radar panel is subscribed. ([llnetmap.h](indra/newview/llnetmap.h), [llnetmap.cpp](indra/newview/llnetmap.cpp), [fsradar.cpp](indra/newview/fsradar.cpp))
  - **Avatar/Voice:** `LLSpeakerMgr::update()` caches the proximal-channel active state once instead of dispatching a virtual call per avatar per frame. ([llspeakers.h](indra/newview/llspeakers.h), [llspeakers.cpp](indra/newview/llspeakers.cpp))
- **Installer basename without hardcoded prefix:** the setup executable name no longer forces the `MikoStorm-` prefix. ([viewer_manifest.py](indra/newview/viewer_manifest.py))

### Bug Fixes

- **Notecard/Script editor font sizes:** the font combo boxes in the built-in text editors offered no large/huge size and several language skins were missing the `value` attribute, so the combos could not resolve their selection. All 24 font combos across 12 languages now carry the correct `value`s („Sans Serif" → „SansSerif") and include the `Huge` entry; the default script font size is now Medium and C++ fallbacks ensure the combo reflects the actual `FSScriptingFontSize`/`FSN*FontSize` even when the font entry is missing. ([llpreviewnotecard.cpp](indra/newview/llpreviewnotecard.cpp), [llpreviewscript.cpp](indra/newview/llpreviewscript.cpp), [panel_preferences_UI.xml](indra/newview/skins/default/xui/en/panel_preferences_UI.xml), [panel_preferences_firestorm.xml](indra/newview/skins/default/xui/en/panel_preferences_firestorm.xml))

## 1.16.42 (2026-08-29)

### Improvements

- **Volumetric Light Scattering / God Rays (Phase 2):** reimplemented Black Dragon's deferred volumetric lighting as a native MikoStorm feature, controlled by the `RenderVolumetricLighting` setting (default on) and its tuning knobs `RenderVolumetricLightingResolution` (ray-march steps, 16), `RenderVolumetricLightingMultiplier` (intensity, `4.0`) and `RenderVolumetricLightingFalloffMultiplier` (`2.0`, distance/silhouette weighting). It accumulates forward-scattered sunlight (Henyey-Greenstein phase) along the view ray in the deferred light buffer, occluded by the existing cascaded shadow maps, and is wired into `renderDeferredLighting` right after the sun/atmospheric pass. Cross-platform (OpenGL). ([pipeline.cpp](indra/newview/pipeline.cpp), [pipeline.h](indra/newview/pipeline.h), [llviewershadermgr.cpp](indra/newview/llviewershadermgr.cpp), [llviewershadermgr.h](indra/newview/llviewershadermgr.h), [volumetricLightF.glsl](indra/newview/app_settings/shaders/class2/deferred/volumetricLightF.glsl))
  - Note: the GLSL is compiled at runtime by the viewer; the C++ compiles/links clean but the visual result should be tuned in-world via the Preferences → Graphics controls.

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
