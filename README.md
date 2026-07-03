### MikoStorm - A Second Life Viewer 
This is an experimental and privacy focused viewer.
MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life.
The client codebase has been open source since 2007 and is available under the LGPL license

## Experimental features
- [Voicebox TTS](https://voicebox.sh/linux-install) support. Reads your IMs and chat aloud if you run a voicebox server. I am planning on a wrapper for other TTS.
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
- Removed discord and Flickr support.
- Removed velo updater
- Hides your data
- Custom and definable login page (Done!)
- Latest original code merged.
- Removed Social Links
- **Version System** — Manual version control via VIEWER_VERSION_FS.txt (no automatic git commit count)
- **Custom Icon** — MikoStorm_icon.png replaces all default viewer icons


## Build Instructions

- [Windows](doc/building_windows.md)
- [Linux](doc/building_linux.md)
- No Support for Mac/Darwin



# MikoStorm
