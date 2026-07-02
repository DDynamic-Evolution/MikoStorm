### MikoStorm - A Second Life Viewer 
This is an experimental and privacy focused viewer.
MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life.
The client codebase has been open source since 2007 and is available under the LGPL license

## Experimental features
- [Voicebox TTS](https://voicebox.sh/linux-install) support. Reads your IMs and chat aloud if you run a voicebox server. I am planning on a wrapper for other TTS.
- Script bulk upload. Specially made for scripter who want to upload their script repositories to SL.
- Automated Photogrammetry Capture. Orbits the camera around the focus target (avatar, object, or another avatar) and saves PNG images for photogrammetry reconstruction. Access via the dock button (camera+gear icon) or command `photogrammetry`.
- Open Source3D Stream Audio — Positional streaming audio from prims with multi-speaker / 5.1ch support
- Todo switch left to right
  - **Usage:** Set a prim's **Description** field to `[3dstream:{url:http://example.com/stream.mp3}]`
  - **Range:** `[3dstream:{url:...}{range:30}]` (default 20m)
  - **Distributed Stereo** (multiple Speakers in Linkset):
    - Root-Prim: `[3dstream:{url:http://example.com/stream.mp3}]`
    - Speaker: `[3dstream:{ch:L}{range:30}]` / `[3dstream:{ch:R}{range:30}]`
  - **Optional Parameters:** `binaural:on|off`, `venue:NAME` (hall_medium, club, cathedral...), `upmix:on|off`, `volume:0.8`
  - **Activate:** Preferences > Sound > `3D Stream Enabled`
  - **Debug:** Console: `Stream3DDebugUrl "http://..."` + `Stream3DDebugPlay TRUE`
  - Alter Prefix `[ayastream:...]` funktioniert auch
- (planned) Rendering Controls — A unified floater for rendering toggles (SSAO, SSR, DoF, Motion Blur, Volumetric Lighting, Skin SSS, and more)
- (planned) Selectable Tone Mappers — Choose between Khronos Neutral, ACES, Filmic, and others
- (planned) Color Grading & LUT Loading — Adjust saturation, contrast, color temperature, and load custom .cube LUTs
- URL Phishing / Scam Link Detection — Warns before opening suspicious URLs; toggle in Firestorm → Protection tab 
- Removed discord and Flickr support.
- Removed velo updater
- Hides your data
- Custom and definable login page (Done!)
- Latest original code merged.
- Removed Social Links
- **NotecardWriter** — Schreibt Text per `~Name~Inhalt`-Kommando via `llOwnerSay` in eine Notecard im Inventar.
  - **Existierende Notecard:** Inhalt wird **angehängt** (append).
  - **Neue Notecard:** Wird automatisch erstellt.
  - **Rotation:** Bei Überlauf (>64k) wird `Name_1`, `Name_2`, … angelegt.
  - **Stumm:** Das Kommando erscheint nicht im Chat.
  - **Usage:** `llOwnerSay("~Einkaufsliste~Milch, Brot, Butter");`
  - **Dateien:** `indra/newview/fsnotecardwriter.h/.cpp`


## Build Instructions

- [Windows](doc/building_windows.md)
- I cant support Mac
- [Linux](doc/building_linux.md)



