# MikoStorm – Fast. Modern. Open Source
<p align="center">
  <img src="images/mikostorm.png" width="512" height="512">
</p>

[Download](https://github.com/DDynamic-Evolution/MikoStorm/releases) 

MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life. The client codebase has been open source since 2007 and is available under the LGPL license.

[Mikostorm Startpage](https://ddynamic-evolution.github.io/) - [Discord](https://discord.gg/2eKKTcwHF4)

This repository is not released by SecondLife or Firestorm!

MikoStorm adds camera, communication, rendering and audio features for everyday use, photography and machinima in Second Life and OpenSim.

This project is a one man show! So be patient

## Current Windows release: 2.0.2

- **Windows Chat popout:** local chat, private IMs and group conversations in a separate Windows window, with individual histories and drafts.
- **Windows appearance:** automatic Light/Dark application theme, High Contrast support and live theme updates.
- **Friends in the popout:** searchable friend list with online status; open an IM by double-clicking, pressing Enter or choosing **Open chat**.
- **Toybox button:** drag **Windows Chat** onto a toolbar to open or focus the popout.
- **Build features:** Windows x64 AVX2, OpenSim, FMOD, positional 3D Stream audio and optional MCP support.
- **Embedded browser:** enabled by default.

The current AVX2 installer is version **2.0.2**. The older legacy/NoAVX package remains **1.16.58** and predates the new chat features.

### Builds

Windows uses separate AVX2 and legacy profiles. See [Windows build profiles](doc/building_windows_profiles.md), [Building on Windows](doc/building_windows.md) and [Building on Linux](doc/building_linux.md) for setup and build instructions.

<img width="1839" height="919" alt="screen" src="https://github.com/user-attachments/assets/44ebaa9b-3fc1-490f-926e-2c3b8156c395" />

### Not on the TPV List.. why?

TPV Request canceled. Nothing is happening there and I dont have the time to meet people on Hippotropolis.
This will be a private project, more or less

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full release history.

### No 32Bit version

There is and never will be a 32bit version

### Issues

You can post them here on github or send me a mail. Email is in my profile here on github. 

### Documentation

- [Windows Chat](doc/native_chat.md) — Popout, themes, friends and conversations
- [MCP setup and tools](doc/mcp.md) — Optional local MCP server
- [Experimental Features](doc/experimental_features.md) — How to use all experimental features
- [Camera Timeline](#camera-timeline) — Record and playback camera movements

### ADDITIONAL FEATURES

This viewer contains experimental features like 
- a slider for **Animationspeed**
- **Set individual IM sounds for your contacts**
- **photogrammetry tool** for our fellow 3D Scanners an 3D Printers (i.e. to use with [3DF Zephyr](https://store.steampowered.com/app/438450/3DF_Zephyr_Lite_Steam_Edition/) [Meshlab](https://github.com/cnr-isti-vclab/meshlab)
- **multiuser Posing system** for photographers (Yes it has an IM request if someone wants to be posed)
- **Voicebox TTS** A tool to connect to an external voicebox server on your system, to read messages aloud.
- **A unique color scheme**
- **New render pipeline** from [Ayastorm](https://github.com/mayatonton/phoenix-firestorm/) which gives you alot more advantages over the original
- **LL-style Chat Window** — Ported Linden Lab chat UI with mini profile icons and range filtering
  - Three chat window styles: FS V1 (plain text), FS V7 (modern headers), LL Style
  - Mini profile icons next to speaker names in nearby chat
  - Range filtering: only shows speakers within your say range
  - Flash effect for new IMs in conversation list
  - Human/System & Object tab split for chat history
  - Configurable via Preferences > Chat > Chat Window Style (requires restart)
- **3D Stream** forked from [Ayastorm](https://github.com/mayatonton/phoenix-firestorm/) which lets you stream music from prims.
- **Script bulk upload** — Specially made for scripters who want to upload their script repositories to SL.
- **3D Stream Audio** — Positional streaming audio from prims with multi-speaker / 5.1ch support
  - Set a prim's **Description** field to `[3dstream:{url:http://example.com/stream.mp3}]`
  - Range: `[3dstream:{url:...}{range:30}]` (default 20m)
  - Distributed Stereo (multiple Speakers in Linkset)
  - Optional Parameters: `binaural:on|off`, `venue:NAME`, `upmix:on|off`, `volume:0.8`
  - Activate: Preferences > Sound > `3D Stream Enabled`
- **3D Stream Metadata Notifications** — Display title/artist metadata from 3D streams
  - Toast notifications when stream metadata changes (configurable)
  - Optional script channel output for custom integrations
  - Settings: Stream3DShowToast, Stream3DChatNotify, Stream3DChatChannel
- **3D Stream Preferences** — Configure 3D stream behavior in Preferences > Sound > 3D Stream
  - **Scan linked child prims for stream tags** (`Stream3DDescriptionScan`) — Scan child prims for distributed-stereo speaker layouts
  - **Scan avatar attachments for stream tags** (`Stream3DScanAttachments`) — Also scan worn objects and HUDs for stream tags (default: off)
  - **Max concurrent streams** (`Stream3DMaxConcurrent`) — Limit simultaneous 3D streams (default: 4, 0 = unlimited)
- **URL Phishing / Scam Link Detection** — Warns before opening suspicious URLs
- **Hide Local Chat** — Toggle to hide chat in quick preferences
- **MCP Server** — Model Context Protocol for AI assistants (see [MCP Documentation](#mcp-model-context-protocol))
- **Camera Timeline** — Record and playback camera movements for machinima and photography
  - Capture camera position, rotation, FOV as keyframes at any point on the timeline
  - Smooth step interpolation between keyframes for natural camera motion
  - Adjustable playback speed (0.25x–4x) and total duration (up to 600s)
  - Scrub the timeline to preview camera positions at any time
  - Select and delete individual keyframes to fine-tune your camera path
  - Save/Load camera timelines as XML files for reuse and sharing
  - Reset camera to previous position after playback stops

### Additional Rendering Features
- **HQ Depth of Field** — High-quality circular bokeh DoF with foreground blur and chromatic aberration
- **Shadow Softness** — Adjustable PCF shadow sample tap radius
- **3D Color Grading LUT** — Load .cube LUT files for cinematic color grading
- **HDR Tonemapping** — ACES and Khronos Neutral tonemapping operators
- **CAS Sharpening** — Contrast Adaptive Sharpening
- **PBR Materials** — Full GLTF/PBR material support with 16 shader variants

### Build Configuration

Windows supports separate AVX2 and legacy profiles through [build-windows.cmd](doc/building_windows_profiles.md).
- AVX2 optimization
- FMOD audio
- 3D Stream (Ayastorm)
- OpenSim support
- MCP (Model Context Protocol)

---

## MCP (Model Context Protocol)

Version **1.16.59** adds a live MCP control window, per-client
permissions, session/version negotiation, structured validated tools, bounded
inventory queries, cancellable teleport/notecard operations, and camera/audio
and performance tools. Open **Preferences > Network & Files > MCP Server > Open
MCP Control**. The default endpoint is `http://127.0.0.1:13231/mcp`; MCP remains
opt-in and local only. Clients must initialize a session before calling tools.

See [MCP setup, protocol, tools and test status](doc/mcp.md) for the handshake,
settings, permission masks, limits and examples. The Windows AVX2 release is compiled and packaged. Native validator checks and
the read-only HTTP smoke test passed without login. In-world actions still need
manual validation; the older 1.16.58 binaries do not contain this update.

## Native Windows Chat Popout

Version **2.0.1** includes local chat, private IMs and group conversations in a
separate native Windows window, with individual transcripts/drafts and a
conversation selector. Open **Communicate > Chat in Windows Window…**, the ↗
Nearby Chat button, or drag **Windows Chat** from the Toybox onto a toolbar.
The button opens/focuses the same window and indicates when it is open.

Local chat supports whisper/shout. IM/group replies use the existing IM send
path with text, moderator and RLVa checks. Font size, window placement and
Always on top are configurable. Build and regression checks passed; native
window interaction and in-world message behavior still need manual validation.
See [chat setup and validation status](doc/native_chat.md).

Version **2.0.2** also follows Windows Light/Dark/High Contrast settings and adds
a searchable friend sidebar with online status and open-IM actions. The viewer
is compiled and packaged; live theme changes and friend/chat interaction remain
to be validated manually in a logged-in session.

## Camera Timeline

Record and play back camera movements for machinima and photography. Capture your camera's position, rotation, and field of view as keyframes, then play them back with smooth interpolated motion.

### How to Open

Click the **Camera Timeline** icon in the toolbar (Toybox). The floater appears as a bar above the bottom toolbar.

### Quick Start

1. Position your camera where you want the first keyframe
2. Click **Capture Camera** to record the keyframe
3. Move your camera to the next position and capture again
4. Click **Play** to watch the playback (minimum 2 keyframes required)
5. Use **Save/Load** to store timelines as XML files for reuse

### Controls

| Control | Description |
|---------|-------------|
| **Capture Camera** | Records current camera position, rotation, and FOV as a keyframe |
| **Play** | Starts timeline playback (requires 2+ keyframes) |
| **Stop** | Stops playback and restores camera to its pre-playback state |
| **Duration** | Total timeline length in seconds (0.5–600s, default: 30s) |
| **Speed** | Playback speed multiplier (0.25x–4.0x, default: 1.0x) |
| **Timeline Slider** | Scrub through the timeline to preview camera positions |
| **Keyframes >>** | Expands the keyframe management panel |

### Keyframe Panel

Expand the panel to view and manage your keyframes:

- **Keyframe list** — Shows index, time (seconds), position (x,y,z), and FOV for each keyframe
- **Delete** — Remove the selected keyframe
- **Clear All** — Remove all keyframes
- **Save** — Export timeline to a `.camera_timeline.xml` file
- **Load** — Import a previously saved timeline

### Interpolation

Camera motion uses **smooth step easing** (`3t² - 2t³`) for natural acceleration and deceleration at each keyframe boundary. Position uses linear interpolation, rotation uses spherical linear interpolation (SLERP), and FOV is linearly interpolated.

### Tips

- Capture keyframes at regular intervals for smoother, more predictable motion
- Use the timeline slider to preview the interpolated position at any point before playing
- Save your timelines to share with other MikoStorm users
- The camera always restores to its original position when playback stops

---

## Links

- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)
- [Second Life](https://secondlife.com/)
- [Model Context Protocol](https://modelcontextprotocol.io/)

## License

This project is licensed under the LGPL-2.1-only license. See [LICENSE](LICENSE) for details.
