# MikoStorm - A Second Life Viewer

<p align="center">
  <img src="images/mikostorm.png">
</p>


[![Windows](https://img.shields.io/badge/Windows-Installer-orange)](https://github.com/DDynamic-Evolution/MikoStorm/releases/download/v1.15.20/MikoStorm-MikoStormOS-Release_AVX2-1-15-20-0_Setup.exe) [![Linux](https://img.shields.io/badge/Linux-tar.xz-orange)](https://github.com/DDynamic-Evolution/MikoStorm/releases/download/v1.15.20/MikoStorm-MikoStormOS-Release_AVX2-1-15-20-0.tar.xz)

MikoStorm is a fork of the [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm) for Second Life. The client codebase has been open source since 2007 and is available under the LGPL license.

[Mikostorm Startpage](https://ddynamic-evolution.github.io/)

This repository is not released by SecondLife or Firestorm!

MikoStorm aims to provide a viewer with many missing or unique features that enhance the Second Life experience. Anyone looking for a viewer with illegal features must look elsewhere — this is not the place for it.

### Not on the TPV List.. why?

Because I never applied and even tho I stay within TOS, I dont see a reason. It will be a specialized viewer.

### Issues

You can post them here on github or send me a mail. Email is in my profile here on github. 

### Documentation

- [Experimental Features](doc/experimental_features.md) — How to use all experimental features

### ATTENTION

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

## Links

- [Firestorm Viewer](https://github.com/FirestormViewer/phoenix-firestorm)
- [Second Life](https://secondlife.com/)
- [Model Context Protocol](https://modelcontextprotocol.io/)

## License

This project is licensed under the LGPL-2.1-only license. See [LICENSE](LICENSE) for details.
