# MikoStorm - A Second Life Viewer 
[Download v1.0.15.5 (Installer)](https://github.com/DDynamic-Evolution/MikoStorm/releases/download/v1.0.15.5/MikoStorm-MikoStorm-Release_AVX2-1-0-15-5_Setup.exe)

This is an experimental and privacy focused viewer.
MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life.
The client codebase has been open source since 2007 and is available under the LGPL license

## Experimental features
- **Poser Bone Anchor Points** — Joint markers (spheres at bone positions) in the Animation Poser now remain visible when switching between avatars, even without a specific joint selected. This makes bone positions visible at all times while posing.
- **eSpeak-NG TTS** — Embedded text-to-speech reads your IMs and chat aloud. Supports multiple languages and voices.
- **LL-style Chat Window** — Ported Linden Lab chat UI with mini profile icons and range filtering
  - Three chat window styles: FS V1 (plain text), FS V7 (modern headers), LL Style
  - Mini profile icons next to speaker names in nearby chat
  - Range filtering: only shows speakers within your say range
  - Console suppression when LL-style chat is visible
  - Flash effect for new IMs in conversation list
  - Human/System & Object tab split for chat history
  - Configurable via Preferences > Chat > Chat Window Style (requires restart)
- **3D Stream Metadata Notifications** — Display title/artist metadata from 3D streams
  - Toast notifications when stream metadata changes (configurable)
  - Optional script channel output for custom integrations
  - Prim name included in notifications
  - Settings: Stream3DShowToast, Stream3DChatNotify, Stream3DChatChannel
  - New "3D Stream" preferences subtab with all controls
- **Script bulk upload** Specially made for scripter who want to upload their script repositories to SL.
- **Automated Photogrammetry Capture** Orbits the camera around the focus target (avatar, object, or another avatar) and saves PNG images for photogrammetry reconstruction. Access via the dock button (camera+gear icon) or command `photogrammetry`.
- **Open Source3D Stream Audio** Positional streaming audio from prims with multi-speaker / 5.1ch support
  - **Usage:** Set a prim's **Description** field to `[3dstream:{url:http://example.com/stream.mp3}]`
  - **Range:** `[3dstream:{url:...}{range:30}]` (default 20m)
  - **Distributed Stereo** (multiple Speakers in Linkset):
    - Root-Prim: `[3dstream:{url:http://example.com/stream.mp3}]`
    - Speaker: `[3dstream:{ch:L}{range:30}]` / `[3dstream:{ch:R}{range:30}]`
  - **Optional Parameters:** `binaural:on|off`, `venue:NAME` (hall_medium, club, cathedral...), `upmix:on|off`, `volume:0.8`
  - **Activate:** Preferences > Sound > `3D Stream Enabled`
  - **Debug:** Console: `Stream3DDebugUrl "http://..."` + `Stream3DDebugPlay TRUE`
  - Alter Prefix `[ayastream:...]` also working
- URL Phishing / Scam Link Detection — Warns before opening suspicious URLs; toggle in Firestorm → Protection tab 
- **Hide Local Chat** — Toggle to hide chat in quick preferences
- Removed discord and Flickr support.
- Removed velo updater
- Hides your data
- Custom and definable login page (Done!)
- Latest original code merged.
- Removed Social Links
- **Version System** — Manual version control via VIEWER_VERSION_FS.txt (no automatic git commit count)
- **Custom Icon** — MikoStorm_icon.png replaces all default viewer icons
- **MCP Server (linux only) buildflag --mcp** — Model Context Protocol (JSON-RPC 2.0) HTTP server on `localhost:13231` for AI assistants (Claude Desktop, Cursor, etc.)
  - Tools: `chat_say`, `chat_shout`, `avatar_sit/stand`, `avatar_walk_to`, `avatar_teleport`, `avatar_fly`, `get_position`, `get_region_info`, `get_nearby_agents`, `notecard_write`, `inventory_list`
  - Resources: `mikostorm://position`, `mikostorm://region`, `mikostorm://nearby`
  - Optional Bearer token auth, localhost-only binding, disabled by default
  - Settings in Preferences > Network & Files > MCP Server
  - ** use --nomcp on Windows! **


## Build Instructions

- [Linux](doc/building_linux.md)
- [Windows](doc/building_windows.md)
- No Support for Mac/Darwin
