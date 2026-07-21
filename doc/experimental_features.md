# Experimental Features

This guide explains how to access and use the experimental features in MikoStorm.

---

## Animation Speed Slider

Control the playback speed of all animations globally.

- **Location:** Bottom toolbar (Toybox) — click the animation icon
- **How to use:** Drag the slider between **0.1x** (slow motion) and **10.0x** (fast forward). Click **Reset** to return to 1.0x.

---

## Individual IM Sounds

Assign custom notification sounds to specific friends.

- **Location:** Right-click a friend in **Contacts > Friends** → select **"Set IM Sound..."**
- **How to use:** Enter a Second Life Sound UUID in the floater, click **Preview** to test, then **OK** to save. Use **"Clear IM Sound"** from the same menu to remove a custom sound.

---

## Photogrammetry Tool

Automated camera orbit for 3D scanning / photogrammetry capture.

- **Location:** Toolbar button (Toybox) — click the photogrammetry icon
- **How to use:** Configure capture settings (delay, total images, height levels, resolution). Set an output directory. Click **Start Capture** to begin the automated screenshot sequence. Output is saved as image files to `{LindenUserDir}/photogrammetry/`.

---

## Multiuser Posing System

Pose multiple avatars simultaneously with joint-level control.

- **Location:** Toolbar button (Toybox) or top menu bar → **Poser**
- **How to use:** Select joints from the bone list. Use rotation/position/scale sliders or the visual gizmos (rotate/translate). Save/load poses to XML files. Supports mirroring, flipping, undo/redo, and BVH export. Other users receive an IM request when invited to be posed.
- they need to answer yes or nothing will be posed
- For this functionm the other user does NOT have to use the same viewer

---

## Voicebox TTS

Reads incoming chat messages aloud using an external TTS server.

- **Location:** Preferences > Sounds > **TTS** tab
- **How to use:**
  1. Check **"Enable Chat TTS"**
  2. Enter your Voicebox server URL (default: `http://127.0.0.1:5101`)
  3. Optionally enter a voice profile name
  4. Select which message types to read: Whisper, Normal, Shout, IMs
  5. Choose a name format: Full Name / First Name Only / No Name
- **Global controls:** Volume and mute in Preferences > Sound (AudioLevelTTS / MuteTTS)

---

## LL-style Chat Window

Alternative chat UI with mini profile icons and range filtering.

- **Location:** Preferences > Chat → **Chat Window Style** radio group
- **Options:**
  - **FS V1 style** — plain text
  - **FS V7 style** — modern headers
  - **LL style** — Linden Lab chat UI with mini profile icons, range filtering, and Human/System & Object tab split
- **Note:** Requires a viewer restart after changing.

---

## 3D Stream

Positional streaming audio from prims with multi-speaker support.

- **Location:** Status bar toggle button, or Preferences > Sounds > **3D Stream** tab
- **How to use:**
  1. Enable via the status bar toggle or Preferences > Sounds > 3D Stream > **"Enabled"**
  2. Set a prim's Description to: `[3dstream:{url:http://example.com/stream.mp3}]`
  3. Optional parameters: `{range:30}` (default 20m), `binaural:on|off`, `venue:NAME`, `upmix:on|off`, `volume:0.8`
  4. For distributed stereo (multiple speakers in a linkset): use `[3dstream-stereo:...]` tags
- **Preferences options:**
  - Scan child prims for stream tags
  - Scan avatar attachments for stream tags
  - Max concurrent streams (default: 4, 0 = unlimited)
  - Metadata toast notifications and chat channel forwarding

---

## Script Bulk Upload

Upload multiple scripts or assets at once.

- **Location:** Right-click in **Inventory** → **Upload > Bulk...** (for textures/meshes/sounds) or **Upload > Bulk Script** (for scripts)
- **How to use:** Select multiple files in the file picker. A confirmation dialog shows the estimated upload cost. Click **Upload**. For scripts, optionally enable the LSL preprocessor in Preferences > Firestorm.

---

## URL Phishing / Scam Link Detection

Warns before opening suspicious URLs.

- **Location:** Preferences > Firestorm > **Protection** tab
- **How to use:**
  1. Check **"Enable URL Security Checking"** — the viewer checks clicked URLs for phishing, typosquatting, and brand impersonation of Second Life domains
  2. Optionally check **"Preview real URL of bracket links in IMs and group chat"** and/or **"Preview real URL of bracket links in nearby and object chat"** — shows the real destination URL on hover before clicking

---

## Hide Local Chat

Toggle nearby chat visibility in the chat console.

- **Location:** Quick Preferences panel → **"Hide Local Chat"** checkbox
- **How to use:** Check the box to hide nearby chat messages from the chat console and toast notifications. Messages still appear as floating text above avatars. The chat input bar remains functional.

---

## Rendering Features

All rendering features are found in **Preferences > Graphics**.

### HQ Depth of Field

High-quality circular bokeh with foreground blur and chromatic aberration.

- **Location:** Preferences > Graphics > **Depth of Field** tab
- **How to use:** Check **"Enable Depth of Field"**. Adjust Camera F Number, Focal Length, and FOV. Toggle **"Front-of-focus blur"** and **"DoF chromatic aberration"** for additional effects. Adjust **DOF Rendering Quality** for performance/quality balance.
- **Note:** HQ DoF is enabled by default. To disable, set `RenderDepthOfFieldHighQuality` to `false` in Debug Settings.

### Shadow Softness

Adjustable PCF shadow sample tap radius.

- **Location:** Preferences > Graphics > Advanced section → **"Shadow Softness"** slider
- **Range:** 1.0 (sharp) to 3.0 (soft)
- **Note:** Ambient Occlusion must be enabled.

### 3D Color Grading LUT

Load .cube LUT files for cinematic color grading.

- **Location:** Preferences > Graphics > **Color Grading** tab
- **How to use:** Select a LUT from the dropdown, or click **Browse...** to load a custom `.cube` file. Adjust **LUT Intensity** to blend between original and graded colors. User-added LUTs can be removed with the **Remove** button.

### HDR Tonemapping

ACES and Khronos Neutral tonemapping operators.

- **Location:** Preferences > Graphics > Advanced section → **"Tone Mapper"** combo and **"Tone Mapping Mix"** slider
- **Options:** Khronos Neutral (default) or ACES
- **Mix slider:** 1.0 = full tonemapping, 0.0 = raw linear lighting

### CAS Sharpening

Contrast Adaptive Sharpening.

- **Location:** Preferences > Graphics > Advanced section → **"Sharpening"** slider
- **Range:** 0.0 (off) to 1.0 (maximum)
- **Default:** 0.4

---

## MCP (Model Context Protocol)

AI assistant integration via a built-in MCP server. See [README.md](../README.md#mcp-model-context-protocol) for full documentation.

- **Location:** Preferences > Network & Files > **MCP Server**
- **How to use:** Enable the server, set a port (default: 13231), optionally set an auth token. Configure your AI client (Claude Desktop, Cursor, etc.) to connect to `http://localhost:13231/mcp`.
