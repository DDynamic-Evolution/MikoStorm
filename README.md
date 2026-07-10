# MikoStorm - A Second Life Viewer

[![Download Latest Release](https://img.shields.io/badge/Download-v1.0.15.14-blue)](https://github.com/DDynamic-Evolution/MikoStorm/releases/latest)

MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life. The client codebase has been open source since 2007 and is available under the LGPL license.

This repository is not released by SecondLife or Firestorm!

## Features

### Experimental Features
- **Poser Bone Anchor Points** — Joint markers (spheres at bone positions) in the Animation Poser now remain visible when switching between avatars, even without a specific joint selected.
- **Voicebox TTS** — HTTP-REST based text-to-speech reads nearby chat and IMs aloud. Start with `voicebox serve` ([voicebox.sh](https://voicebox.sh/)). Configurable server URL and voice profile in Preferences > Sound > TTS.
- **LL-style Chat Window** — Ported Linden Lab chat UI with mini profile icons and range filtering
  - Three chat window styles: FS V1 (plain text), FS V7 (modern headers), LL Style
  - Mini profile icons next to speaker names in nearby chat
  - Range filtering: only shows speakers within your say range
  - Flash effect for new IMs in conversation list
  - Human/System & Object tab split for chat history
  - Configurable via Preferences > Chat > Chat Window Style (requires restart)
- **3D Stream Metadata Notifications** — Display title/artist metadata from 3D streams
  - Toast notifications when stream metadata changes (configurable)
  - Optional script channel output for custom integrations
  - Settings: Stream3DShowToast, Stream3DChatNotify, Stream3DChatChannel
- **Script bulk upload** — Specially made for scripters who want to upload their script repositories to SL.
- **Automated Photogrammetry Capture** — Orbits the camera around the focus target and saves PNG images for photogrammetry reconstruction.
- **3D Stream Audio** — Positional streaming audio from prims with multi-speaker / 5.1ch support
  - Set a prim's **Description** field to `[3dstream:{url:http://example.com/stream.mp3}]`
  - Range: `[3dstream:{url:...}{range:30}]` (default 20m)
  - Distributed Stereo (multiple Speakers in Linkset)
  - Optional Parameters: `binaural:on|off`, `venue:NAME`, `upmix:on|off`, `volume:0.8`
  - Activate: Preferences > Sound > `3D Stream Enabled`
- **URL Phishing / Scam Link Detection** — Warns before opening suspicious URLs
- **Hide Local Chat** — Toggle to hide chat in quick preferences
- **MCP Server** — Model Context Protocol for AI assistants (see [MCP Documentation](#mcp-model-context-protocol))
- Removed discord and Flickr support
- Removed velo updater
- Hides your data
- Custom and definable login page
- Latest original code merged
- Removed Social Links

### Rendering Features
- **HQ Depth of Field** — High-quality circular bokeh DoF with foreground blur and chromatic aberration
- **Shadow Softness** — Adjustable PCF shadow sample tap radius
- **3D Color Grading LUT** — Load .cube LUT files for cinematic color grading
- **HDR Tonemapping** — ACES and Khronos Neutral tonemapping operators
- **CAS Sharpening** — Contrast Adaptive Sharpening
- **PBR Materials** — Full GLTF/PBR material support with 16 shader variants

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

## Build Instructions

- [Windows](doc/building_windows.md)
  
### Quick Build (Windows)

```bash
# Configure
cd build-vc170-64
configure.bat

# Build
build.bat
```

---

## Changelog

### v1.0.15.14
- **Critical IM Fix** — Poser permission request check no longer blocks all incoming IMs
- **Self Selection in Poser** — Self is now shown in the right avatar list (Avatars in Range) and can be selected directly
- **Permission Fix** — Self always has permission to pose, no permission request dialog needed

### v1.0.15.13
- **MCP Support** — Full Model Context Protocol implementation for AI assistant integration
- **Windows MCP Compatibility** — Fixed socket compatibility issues for Windows builds
- **Render Pipeline Fixes** — HQ DoF iteration cap, GLTF error handling, LUT error checking
- **UI Fixes** — FSFloaterPoser access fix, NACLfloaterexploresounds include fix
- **HWID Removal** — Removed HWID spoofing functionality
- **XUI Layout Fixes** — Fixed various UI layout overlaps
- **Color Grading LUT Panel** — Fixed panel positioning in preferences

### v1.0.15.7 - Rendering Features + UI
- **DoF Extras** — Added `RenderDepthOfFieldFront` (foreground blur) and `RenderDepthOfFieldChroma` (DoF-coupled chromatic aberration) settings
- **Shadow Softness** — New `RenderShadowSoftness` setting (F32, default 1.0, range 1.0–3.0)
- **3D Color Grading LUT** — Load .cube LUT files from `app_settings/luts/`

---

## Links

- [Releases](https://github.com/DDynamic-Evolution/MikoStorm/releases)
- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)
- [Second Life](https://secondlife.com/)
- [Model Context Protocol](https://modelcontextprotocol.io/)

## License

This project is licensed under the LGPL-2.1-only license. See [LICENSE](LICENSE) for details.
