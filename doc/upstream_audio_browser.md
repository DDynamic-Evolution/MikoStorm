# Firestorm audio and browser source update

Status: 2026-10-29. The updated **Windows AVX2 2.0.5.0** viewer is compiled and
linked through CMD/VS2022 with Dullahan 1.50/Chromium 152, WebRTC m144, FMOD 2.03.14,
OpenSim, 3D Stream and MCP. The new NSIS installer was built and verified,
including the browser/voice runtime files and dependency metadata.

## Upstream and scope

Reference: [Firestorm master at 3ded91f29695](https://github.com/FirestormViewer/phoenix-firestorm/tree/3ded91f296957f8151e44933a9b42c1ecc853db8).
The relevant local component files matched the earlier Firestorm snapshot
[07ef63ae57d6](https://github.com/FirestormViewer/phoenix-firestorm/tree/07ef63ae57d608f4a630498b17d4df0d25232623)
exactly after line-ending normalization. Local historical commit IDs differ from
those in the current public repository, so file contents were compared directly.
Official blob hashes were checked for the downloaded source snapshots.

- Dullahan: 1.26/Chromium 139 to **1.50.0/Chromium 152.0.7977.83**.
- WebRTC: m137 to **m144.7559.06.22.38629291819**.
- The Windows/macOS/Linux dependency archives and hashes are taken together from
  that upstream autobuild.xml. Other package definitions remain unchanged.
- Keep the existing custom **FMOD 2.03.14** package, 3D Stream and OpenSim/MCP
  options. Existing stream metadata fixes and native popout changes are retained.

Dullahan 1.50 was introduced by [b6f9d91d5e31](https://github.com/FirestormViewer/phoenix-firestorm/commit/b6f9d91d5e31427ee93e21937ac4c823b1eb7e61).
The CEF plugin source at that update matches the existing local plugin, so no
unrelated remote-debugging/PipeWire code was added. CEFPlugin.cmake now links the
explicit prebuilt library paths to select the matching wrapper/browser libraries.
The current Windows CEF runtime file layout matches upstream; the MikoStorm
packaging manifest was preserved.

## Audio changes

The llwebrtc wrapper and device implementation are updated together for the new
SDK/API. Voice now starts/stops devices with voice state, uses software audio
processing consistently, and handles device replacement/default-device changes
and redeployment more reliably. The upstream mute/capture sequencing avoids
holding the microphone open when voice is disabled.

Voice callbacks use a dedicated VoiceWebRTC work queue. Shutdown drains voice
connections with a bounded wait before the WebRTC subsystem terminates, rather
than pumping the viewer main loop. Thread/device/factory ownership transfers to
the shutdown worker so a detached timeout path does not reset objects it still
uses. Relevant changes include [the private voice queue](https://github.com/FirestormViewer/phoenix-firestorm/commit/8b1324c25a4e06ed7018629144f273a9846631f3)
and [default-device reinitialization](https://github.com/FirestormViewer/phoenix-firestorm/commit/82c2cfcff1d68c0b43f8126e88f4a696b72fb212).

The unrelated new viewer voice-statistics polling and moderation UI services
were excluded, because this fork does not contain their dependencies. Existing
server-mute/message handling remains unchanged. The local voice interface still
provides sipURIFromID for all build configurations, including OpenSim.

## Microphone setting

`StopMicCaptureWhenMuted` is a persisted S32 debug setting, initially 1 as upstream.
With 1, capture stops while muted, which helps Bluetooth headsets leave call mode.
With 0, capture stays warm while voice is enabled, reducing cold-start artifacts
on unmute. The control takes effect live. Tuning mode and normal mute behavior
follow the selected policy. No microphone permissions or OS settings are changed.

## Build validation

The incremental AVX2 build and link completed successfully. The new Dullahan,
WebRTC and retained FMOD package metadata match autobuild.xml. The CEF plugin
was recompiled/relinked against the refreshed browser headers and libraries;
archive timestamps were older than the prior objects, so replacement SDK
inputs were marked fresh for incremental tracking.

Six focused audio/dependency and 15 native-chat source integration checks pass.
The x64 viewer, newly linked WebRTC DLL and CEF plugin, browser host/DLL/context
snapshot/resources and complete dependency/copyright information were checked
in the Release folder against the actual build/SDK files. The copy-only target excludes About metadata, so its generated packages-info.txt
was synchronized into Release/app_settings before comparison. No MCP tests or viewer
startup were performed. The 2.0.5 NSIS installer was compiled and its integrity,
version and selected payload files were verified against the build and sources.
Legacy/NoAVX was not rebuilt.

Build integration corrections: remove a remaining call to the excluded viewer
voice-version feature. Metadata-only autobuild queries skip source-environment
loading, avoiding localized vcvars output decoding failures under MSBuild.
The seeded package registry was normalized to absolute AVX2 paths; read-only old
SDK headers were made replaceable. Missing files in the earlier copied cache
were restored before replacement continued.

Manual validation remains necessary: media/browser playback, local web/media
pages, voice connect/reconnect/teleport, selected/default device changes, USB
headset unplug/replug, Bluetooth mute/unmute, voice disabled, tuning mode and
shutdown. Check both AVX2 and legacy profiles when those builds are requested.
