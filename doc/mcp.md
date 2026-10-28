# MCP setup and tools

Status: 2026-10-28. Included in the compiled and verified Windows AVX2 2.0.2
NSIS release. Native validator tests and a read-only HTTP smoke test passed
against the new viewer at the login screen. The 1.16.58 binaries predate this
update; in-world actions still need manual validation.

## Controls

Preferences → Network & Files → MCP Server → **Open MCP Control** opens the
English/German control window. Start/stop, listener port and global token apply
immediately. Changing the port or global token restarts the listener and closes
existing sessions. The server remains disabled by default and binds only to
`127.0.0.1`; the default URL is `http://127.0.0.1:13231/mcp`.

The window copies the current URL, generates a cryptographically random global
token, creates/revokes named clients, edits their permissions and shows recent
calls. The audit holds 100 entries in memory, with time, client, tool and status;
it does not retain arguments, chat, IM text or tokens. The window shows the last
eight entries. Named client IDs are abbreviated there, not in authentication.

## Authentication and permissions

`Authorization: Bearer TOKEN` selects a named client or the global/default
client. Named tokens contain 256 random bits; only their SHA-256 hashes are saved
in `MCPClients`. The token is returned once to the control window and must be
copied before closing it. Revoking a named client immediately prevents its tools
from executing; permissions are checked again after handing work to the viewer's
main thread and before later operation stages.

The existing optional global `MCPAuthToken` setting remains compatible. An empty
global token permits local anonymous requests as the default client, including
when named profiles also exist. The default and media-plugin masks initially
retain the previous full functionality (`31`). New named clients created in the
window initially receive read access only (`1`).

| Bit | Permission | Examples |
| --- | --- | --- |
| 1 | Read viewer data | Position, inventory metadata, performance, camera state |
| 2 | Camera / audio control | Bone selection, camera preset, timeline playback, source mute |
| 4 | Move avatar | Sit, stand, walk, fly, teleport |
| 8 | Chat / IM | Read and send chat/IM, trigger watch, private chat resources |
| 16 | Write inventory | Create a new notecard |

Operation status/cancellation is restricted to the owning client **and session**;
it remains available without the Read bit. The media-plugin bridge has its own
mask (`MCPMediaPermissions`) and uses the same validation and tool dispatcher.
Normal viewer, grid, RLV and inventory permission checks still apply. No asset
export or permission bypass tools are introduced.

## HTTP and session lifecycle

The supported protocol versions are `2025-11-25` and `2025-06-18`. This is the
JSON-response option of Streamable HTTP; server-initiated SSE streams,
subscriptions and protocol progress notifications are not implemented.

1. POST an `initialize` JSON-RPC request without a session header:

   ```json
   {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"my-client","version":"1"}}}
   ```

2. Keep `MCP-Session-Id` from the response headers and `protocolVersion` from the
   result. An unsupported proposed version is answered with the server's newest
   supported version; the client must decide whether it can continue.
3. POST `{"jsonrpc":"2.0","method":"notifications/initialized"}` with the
   session header. Accepted notifications return HTTP 202 without a body.
4. Send subsequent requests with the same Bearer identity, `MCP-Session-Id` and
   negotiated `MCP-Protocol-Version` headers. Use `Content-Type: application/json`
   and an Accept header permitting `application/json`.
5. DELETE `/mcp` with that session header to close it. Sessions also expire after
   20 minutes without requests. A missing required session returns HTTP 400; an
   unknown/expired session returns 404. Unsupported/mismatched version headers
   return 400.

GET `/mcp` returns 405 with Allow headers. GET `/health` is a separate JSON health
endpoint and uses the same authentication. OPTIONS supports local browser
preflight. Host/Origin must be local; `Origin: null` needs an actual authenticated
Bearer request. Native clients may omit Origin.

HTTP framing limits remain 16 KiB headers and 1 MiB body, with approximately five
seconds receive and two seconds send deadlines. At most eight HTTP workers and
six simultaneous main-thread handoffs are allowed, leaving capacity for normal
cancellation traffic. Main-thread requests time out after approximately ten
seconds. There are at most 32 sessions and 30 ordinary calls per second/session.

JSON-RPC IDs are echoed from the original JSON, including 64-bit integer IDs.
Malformed calls use JSON-RPC errors. Known tools with invalid arguments return
`isError: true` and do not execute. Scalars are checked for their declared types,
finite numeric values, bounds, required/unknown properties, UUID syntax and
string/array sizes. Both successful and failed tool results include an object
under `structuredContent.data`; the text content serializes that same object.
Each tool advertises an input schema, output schema and behavior annotations.

`notifications/cancelled` uses the original request ID. It can prevent queued work
from starting; it does not undo actions already executing. It is not a way to
cancel initialization. The JSON-only transport currently emits no unsolicited
logging notifications; `logging/setLevel` keeps a session-local preference and
does not alter the viewer-wide logger.

## Long operations

`avatar_teleport` and `notecard_write` return `data.operation_id` immediately.
Poll `operation_status` with `{"operation_id":"..."}` or request cancellation
with `operation_cancel`. Status includes kind, state, progress (0–1), message,
elapsed time and available result IDs. Operations are bounded to 128 records;
terminal records are pruned when space is needed. Server stop, session closure
and permission revocation prevent further stages from being submitted.

A teleport resolves an exact region name from the current grid's world-map data,
including regions absent from the viewer's loaded neighborhood. It leaves the
normal SLURL callback intact, checks destination dimensions and reports timeout,
offline/unknown region or viewer refusal. Only one MCP teleport/lookup may run
at once. A region lookup times out after 20 seconds; operations after 120 seconds.
Arrival is detected from viewer teleport state and the destination region,
with a 32 m proximity fallback for an already reached target. The result records
the actual landing coordinates and distance; a forced landing point is identified
in the message. This is a local observation, not a simulator confirmation of an
exact requested landing point. Cross-grid Hypergrid-address resolution
is not offered by this tool.

Notecards use `LLNotecard` serialization and the 65536-byte body limit, the normal
creation callback, capability upload and inventory permission preferences.
Success includes the resulting item and asset IDs. A cancellation can suppress a
later upload, but an inventory item already requested may still be created, and
an upload already sent cannot be undone. Late completion is retained in status
rather than reported as an undone action. Stopping the whole service also stops
local status callbacks; the simulator may still finish a submitted upload.

`avatar_sit` is idempotent for an already seated avatar. `avatar_walk_to` rejects
missing regions and positions outside the current region. `avatar_fly` checks
whether the requested state was accepted.

## Bounded listings

- `inventory_list`: optional `folder`, `offset` and `max_results` (default 50,
  maximum 200). Uses the actual inventory root and reports `next_offset` and
  `cached_total`. The inventory resource returns only the first page.
- `inventory_search`: `query` required; optional `folder`, `type`, `max_results`
  and `cursor`. Repeat the same query/folder/type with the returned cursor until
  `complete` is true. A page visits at most 2048 entries, even if it finds no
  matches. Cursors belong to the client/session, expire after two minutes and are
  limited to 32 total. Depth is capped at 64 and visited folders at 20000; hitting
  those limits returns an explicit truncated result.
- `get_nearby_objects`: optional `offset`, `max_results`, `include_attachments`;
  scans at most 2048 object slots per page and returns `next_offset`.
- `audio_list_sources`: optional `offset`, `max_results`; returns local active
  source IDs, names, kind, playing/muted state and parcel audibility, without URLs
  or credentials. `next_offset` continues the list.
- Chat/IM buffers remain bounded to 100 records; nearby avatars, friends and IM
  session output are bounded to 200 entries. Friends/session results indicate
  truncation. These fixed readouts are not full inventory-style snapshots.

Listings use currently cached viewer data. Inventory/object changes between
pages can change order or membership; a cursor is not a server-side snapshot or
a request to fetch the entire inventory.

## Additional viewer tools

| Tool | Action |
| --- | --- |
| `camera_get_state` | Camera position, FOV (radians), bone-follow ID |
| `camera_list_joints` | Current avatar's numeric bone IDs |
| `camera_set_joint` | Set `joint_id`; -1 disables following |
| `camera_list_presets` | Existing local preset names (up to 200) |
| `camera_load_preset` | Request loading a listed `name`; no arbitrary paths |
| `timeline_get_state` | Read timeline state without opening a window |
| `timeline_start`, `timeline_stop` | Control current timeline; playback needs two keyframes |
| `timeline_capture` | Capture current camera while stopped; MCP limit 256 keyframes |
| `audio_list_sources` | Inspect active 3D sound sources |
| `audio_mute_source` | Set `source_id`, `muted` locally; parcel/media gates stay effective |
| `performance_get` | Median FPS of ten recording periods, derived frame ms, draw distance |

Audio tools are registered only with `USE_3D_STREAM`; the MCP window/service only
with `USE_MCP`. Muting is in-memory and remembered for at most 256 source IDs;
no stream URLs are returned by these tools. A preset load request is not a file
validation guarantee. Performance readouts are measurements, not tuning claims.

## Validation status and next checks

The Windows AVX2 viewer compiled and linked with OpenSim, FMOD, 3D Stream and
MCP enabled. All 21 Python integration checks passed, together with 215 core
checks and the avatar-log, camera and Windows theme regressions. The unchanged
MCP validator previously passed four native groups covering schemas, Unicode,
NaN and bounds. The optional focused CMake target is
`MIKOSTORM_MCP_TESTS=ON` / `INTEGRATION_TEST_llmcpvalidation`; it does not require
enabling the entire upstream `LL_TESTS` suite.

`tests/mikostorm/mcp_smoke.py` passed against the new running viewer without login.
It checks initialization, GET/health separation, protocol/session errors, 64-bit
IDs, schemas and read-only output. The harness used separate temporary settings
and a random token supplied through `MIKOSTORM_MCP_TOKEN`. It sent no chat,
movement or inventory commands. The test process was then terminated; graceful
shutdown and in-world actions were not runtime-tested.

Still verify real login, unloaded-region teleport, cancelled/failed notecard
upload, live rights revocation, camera/timeline controls and parcel-aware 3D
muting manually. Native compilation and this smoke test do not establish those
behaviors. The NSIS payload was checked against the new executable, libraries,
settings and English/German UI.

Protocol references:
[Lifecycle](https://modelcontextprotocol.io/specification/2025-11-25/basic/lifecycle),
[transports](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports),
[tools](https://modelcontextprotocol.io/specification/2025-11-25/server/tools),
[cancellation](https://modelcontextprotocol.io/specification/2025-11-25/basic/utilities/cancellation).
