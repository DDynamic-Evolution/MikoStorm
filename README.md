# MikoStorm – Fast. Modern. Open Source

[![Windowsx64](https://img.shields.io/badge/Windows-Installer-orange)](https://github.com/DDynamic-Evolution/MikoStorm/releases/download/v1.15.23/MikoStorm-MikoStormOS-Release_AVX2-1-15-23-0_Setup.exe) [![Linux](https://img.shields.io/badge/Linux-tar.xz-orange)](https://github.com/DDynamic-Evolution/MikoStorm/releases/download/v1.15.23/MikoStorm-MikoStormOS-Release_AVX2-1-15-23-0.tar.xz)

<p align="center">
  <img src="images/mikostorm.png">
</p>

MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life. The client codebase has been open source since 2007 and is available under the LGPL license.

[Mikostorm Startpage](https://ddynamic-evolution.github.io/) - [Discord](https://discord.gg/2eKKTcwHF4)

This repository is not released by SecondLife or Firestorm!

MikoStorm aims to provide a viewer with many missing or unique features that enhance the Second Life experience. Anyone looking for a viewer with illegal features must look elsewhere — this is not the place for it.

This is a one person project! So be patient
<img width="1839" height="919" alt="Bildschirmfoto vom 2026-07-21 20-31-12" src="https://github.com/user-attachments/assets/44ebaa9b-3fc1-490f-926e-2c3b8156c395" />

### Not on the TPV List.. why?

Because I never applied and even tho I stay within TOS, I don't see a reason to give my address and real name. 
It is a specialized viewer for creators and enthusiasts.

### No 32Bit version

There is and never will be a 32bit version

### Issues

You can post them here on github or send me a mail. Email is in my profile here on github. 

### Documentation

- [Experimental Features](doc/experimental_features.md) — How to use all experimental features

### ADDITIONAL FEATURES

This viewer contains experimental features like 
- a slider for **Animationspeed**
- **Set individual IM sounds for your contacts**
- **photogrammetry tool** for our fellow 3D Scanners an 3D Printers (i.e. to use with [3DF Zephyr](https://store.steampowered.com/app/438450/3DF_Zephyr_Lite_Steam_Edition/)
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

### FEATURES FROM THE FIRESTORM FORK

- Separate text entry for bottom chat bar and nearby chat
- Option to have group notices in the top right
- Mesh upload (including fix for CTS-627:Mesh upload crashes Linux)
- Spell Check Feature
- Beacons show in mouselook
- CTRL+H brings up chat history in mouselook
- CTRL+I brings up inventory in mouselook
- CTRL+T brings up conversations window in mouselook
- Local bitmap browser
- Spam blocker
- Script count from right click
- LSL Preprocessor
- Right click > Script options
- Texture refresh from right click
- Option to view Web Profiles or In-world Profiles
- Optional channel box in chat window for speaking to scripts
- Text search to Notecards
- Cam to on minimap
- Option to always add attachments instead of wearing them
- Inventory search: Multiple substring filters separated by '+'
- Ability to move folders from outside of root inventory
- Invite to group from profile or people tab
- Client-side visible hint for typing avatars
- Zoom in Mouselook with Right mouse button
- Easy toggle on/off of name tag display in Quick prefs
- Quick access to inventory filters next to inventory/recent/worn tabs
- Ability to fly over 4096m
- Day Cycle Slider in Quickprefs
- Option to display World Map without the text alongside it
- More compact conversation floater
- Inventory allows multi-selected items to pull up properties
- Improved Environment Settings Floater to minimise wasted space
- Ability to auto-clear red Map Marker after TPing
- Ability to adjust a worn attachment's position values in the edit window
- Keyboard shortcuts for Sunrise & Midnight
- Opensim hop protocol
- Optional Pie menu or Context right click menus.
- Color and opacity options for Pie menu
- Brand new built in AO
- Brand new built in full range Radar with lots of options
- Radar can report when an avatar enters or leaves draw distance and/or chat range
- Extra Estate Management tools
- Growl support
- Estate/parcel management tools in radar right click menus
- Region/parcel information in menu bar (optional)
- Lots of skin customization options and changes from LL V2
- V1 style Communicate window
- Ability to close floater windows
- Ability to show/hide ALL windows via bottom bar buttons
- Viewer remembers window state/locations on relogs
- Several additional skins with side tabs and without side tabs options
- Additional Graphics settings and options
- Seperate Movie/Music stream controls
- Ability to quickly open locations for crash logs, settings, cache, chat logs etc.
- Greater camera freedom
- V1 style Dialogs in top right of window
- V1 style nearby chat
- V1 style profile windows, ability to open multiple profiles
- Vertical side tabs for IM's
- Show look-at, and selection crosshairs beacons
- Hide my look-at, and selection crosshairs beacons
- Allow multiple viewer instances
- Allow login to other grids (has known issues)
- RLVa
- Disable TP screens, log in and log out screens
- Disable TP Sounds
- Rez objects under land group
- Filter duplicate landmarks on world map
- Ability to not send av physics to server
- Display name/user name options for displaying and sorting in multiple lists like friend list, radar etc
- Clicking your av keeps camera position option
- Disable minimum camera zoom distance
- Allow camera to move without constraints through prims
- Minimap rotation options
- Role Play Chat options
- Friend on/offline notices to nearby chat
- Show IM's in chat console
- Use full screen width for chat console
- Group moderators can be shown in bold text
- Disable all group chats
- When receive group notices is disabled, disables group chat as well
- Keyword alerts
- Improved Area Search
- Command line options for chat bar (not all functions work yet)
- Shared Parcel Windlight
- New Bridge for improved viewer functionality
- Window size presets for Machinima makers
- Configurable Auto responses to IM's
- Snapshots can upload directly to your Flickr account
- Set Default upload permissions
- Improved Camera control and movement control windows
- Separated Media/music stream controls
- Media Filter for your security
- Prim Alignment tool
- Inventory collapse/expand buttons
- Inventory right click> wear and replace/add outfits on inventory folders
- Cut/copy/paste on text editors like notecards, chat windows, scripts etc.
- Worn tab in inventory
- Ability to hide bottom chat bar via unrestricted resize
- Dragable group notices
- Group UUID and Link copy button for groups
- User UUID's in their profiles
- Stream artist song and artist information in chat
- All builds are Large Address Aware for systems with greater than 2 gig memory

### Build Configuration
- AVX2 optimization
- FMOD audio
- 3D Stream (PandaView)
- OpenSim support
- MCP (Model Context Protocol)

---

## MCP (Model Context Protocol)

MikoStorm includes a built-in MCP server that allows AI assistants like Claude Desktop, Cursor, and other MCP-compatible clients to interact with Second Life.

### Overview

The MCP server implements the [Model Context Protocol](https://modelcontextprotocol.io/) specification using JSON-RPC 2.0 over HTTP. It runs on `localhost` only and is disabled by default.

### Configuration

Enable the MCP server in **Preferences > Network & Files > MCP Server**:

| Setting | Description |
|---------|-------------|
| `MCPEnabled` | Enable/disable the MCP server (default: false) |
| `MCPPort` | Port number (default: 13231) |
| `MCPAuthToken` | Optional Bearer token for authentication |

### Endpoint

```
http://localhost:13231/mcp
```

### Available Tools

#### Chat Tools

| Tool | Description | Parameters |
|------|-------------|------------|
| `chat_say` | Send a message in local chat | `message` (required), `channel` (optional, default: 0) |
| `chat_shout` | Send a shout in local chat (up to 100m range) | `message` (required), `channel` (optional, default: 0) |

#### Avatar Tools

| Tool | Description | Parameters |
|------|-------------|------------|
| `avatar_sit` | Sit down on the ground or nearby object | None |
| `avatar_stand` | Stand up from sitting position | None |
| `avatar_walk_to` | Walk to a specific position in the current region | `x`, `y`, `z` (required) |
| `avatar_teleport` | Teleport to a location in any region | `region`, `x`, `y`, `z` (required) |
| `avatar_fly` | Enable or disable flying | `enabled` (required, boolean) |

#### Information Tools

| Tool | Description | Parameters |
|------|-------------|------------|
| `get_position` | Get current avatar position, region, and rotation | None |
| `get_region_info` | Get information about the current region | None |
| `get_nearby_agents` | List nearby avatars with positions and distances | None |

#### Inventory Tools

| Tool | Description | Parameters |
|------|-------------|------------|
| `notecard_write` | Create a new notecard in your inventory | `name`, `content` (required) |
| `inventory_list` | List inventory folders and items at the root level | None |

### Available Resources

Resources provide read-only access to viewer state:

| URI | Description |
|-----|-------------|
| `mikostorm://position` | Current avatar position and region |
| `mikostorm://region` | Current region information |
| `mikostorm://nearby` | List of nearby avatars |

### Example Configuration

#### Claude Desktop (claude_desktop_config.json)

```json
{
  "mcpServers": {
    "mikostorm": {
      "command": "curl",
      "args": ["-X", "POST", "-H", "Content-Type: application/json", "-d", "@-", "http://localhost:13231/mcp"]
    }
  }
}
```

#### Cursor (.cursor/mcp.json)

```json
{
  "mcpServers": {
    "mikostorm": {
      "url": "http://localhost:13231/mcp"
    }
  }
}
```

#### With Authentication

```json
{
  "mcpServers": {
    "mikostorm": {
      "url": "http://localhost:13231/mcp",
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN_HERE"
      }
    }
  }
}
```

### Protocol Methods

The server supports the following JSON-RPC methods:

| Method | Description |
|--------|-------------|
| `initialize` | Initialize the MCP session |
| `ping` | Health check |
| `tools/list` | List all available tools |
| `tools/call` | Call a specific tool |
| `resources/list` | List all available resources |
| `resources/read` | Read a specific resource |
| `notifications/initialized` | Notify that client is initialized |

### Example Requests

#### List Tools

```bash
curl -X POST http://localhost:13231/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
```

#### Send Chat Message

```bash
curl -X POST http://localhost:13231/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"chat_say","arguments":{"message":"Hello from AI!"}}}'
```

#### Get Position

```bash
curl -X POST http://localhost:13231/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"get_position"}}'
```

#### Teleport

```bash
curl -X POST http://localhost:13231/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"avatar_teleport","arguments":{"region":"Ahern","x":128,"y":128,"z":25}}}'
```

### Security

- The server binds to `localhost` only (127.0.0.1)
- No external network access
- Optional Bearer token authentication
- All tool calls are executed on the main thread with proper synchronization

---

## Links

- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)
- [Second Life](https://secondlife.com/)
- [Model Context Protocol](https://modelcontextprotocol.io/)

## License

This project is licensed under the LGPL-2.1-only license. See [LICENSE](LICENSE) for details.
