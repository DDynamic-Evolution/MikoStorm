# Lovense support in MikoStorm

Status: 2026-10-29. Windows AVX2 **2.0.5.0** was incrementally compiled and linked through CMD/VS2022 with OpenSim, FMOD, 3D Stream and MCP. The new NSIS installer was built and verified, including the English/German Lovense controls, settings and license notices.

Open **Communicate > Lovense...** (German: **Unterhalten > Lovense...**), or add **Lovense** from the Toybox to a toolbar.

1. Connect your devices in the Lovense Remote app.
2. Enable manual Lovense connections in the window. Nothing connects automatically on viewer launch, login or window opening, even when this preference was saved.
3. Enter your registered Lovense application/platform name and a session `authToken` obtained from your own trusted service. The password field is cleared on submission; no token is stored in settings, files or logs.
4. Press Connect and scan the locally generated QR code with Lovense Remote. Reload is available while waiting for the app.
5. Select devices explicitly (Ctrl/Shift for several), set a pattern and maximum strength, then press Start. No new/reconnected device is automatically selected or started.
6. Pause/Stop sends targeted Stop commands. Start resumes the pattern from its beginning. Changing the selection, pattern or strength stops playback until Start is pressed again. Closing the window, logout, connection errors and viewer shutdown end control.

## Application authentication

Lovense developer tokens belong on the application owner's trusted server, never embedded in a distributed viewer. The viewer calls only getSocketUrl using a session token; it does not ship the original viewer's developer credential.

A small optional owner-side utility is provided:

```text
python scripts/lovense_session_token.py
```

It prompts for your own developer token without echo, or accepts `MIKOSTORM_LOVENSE_DEVELOPER_TOKEN` from the trusted environment. It calls the official getToken endpoint over verified HTTPS and prints the resulting session authToken for the owner to paste into the viewer. Run it only on the trusted owner's machine/service. It creates no token files and is not started by the viewer or installer. Never send the developer token to the chat or commit it to Git. An opaque random application user ID is used by default; `--uid` can supply your own service's opaque ID. Avatar names/UUIDs and viewer chat are not sent.

The platform field must match your application registration. A deployed service can issue session tokens to users separately; its developer credential is never distributed with the viewer.

## Controls and saved settings

| Setting | Default | Meaning |
|---|---|---|
| MikoLovenseEnabled | false | Allow manually initiated connections; never auto-connect |
| MikoLovensePlatform | MikoStorm | Registered application name |
| MikoLovenseShowHelp | true | Show connection instructions |
| MikoLovenseMaxStrength | 5 | Upper bound from 1 to 20 |
| MikoLovensePattern | 1;2;4;2;0 | Up to 64 semicolon-separated levels from 0 to 20 |

The UI displays up to eight devices with names and battery levels. Only connected, explicitly selected IDs can receive a command. Patterns are clamped to the maximum strength and use the Lovense vibration-pattern API at a 100 ms interval. Each command expires after five seconds and is renewed every two seconds while running. This provides a bounded fallback after a crash/network loss; a Stop request cannot be guaranteed to arrive over an already broken connection. There is no implicit all-devices broadcast or automatic restart after an offline event. Disabling the feature through Debug Settings also ends control.

This is manual pattern playback, not synchronization with avatar animations, furniture, chat, IMs or MCP. Those systems do not control devices. No third-party shop, affiliate links, branding, updater or old SSL/Socket.IO binaries are imported.

## Implementation and validation

The worker owns asynchronous Boost.Beast/Asio TLS and Engine.IO v3 / default Socket.IO namespace handling, matching Lovense's required Socket.IO 2.x wire protocol. It uses MikoStorm's existing Boost/OpenSSL/CURL packages. Hostname and certificate verification remain enabled; returned endpoint hosts are restricted to Lovense domains, redirects are disabled and incoming messages are bounded. Failures are translated status messages; response bodies/authentication values are not logged. Network callbacks never capture the floater or access viewer singletons.

Only the MIT-licensed Project Nayuki QR encoder is reused verbatim, with its original notices preserved and added to the platform license files. QR content is rendered locally in memory and never loaded from a remote image URL.

Passed: 42 production protocol checks, six source/XML integration checks and CMD/VS2022 object compilation of the network module, floater, QR encoder, floater registry and app lifecycle file. These cover protocol bounds, target authorization, settings/localization and lifecycle wiring. The later authorized full incremental viewer build/link and NSIS compilation also passed. English labels/status/error/help text and English descriptions of all five saved settings were confirmed; matching German controls and the packaged resources were compared with sources. No MCP tests or viewer startup were performed. Live QR pairing, app interoperability and physical device control still require a current own session token and manual testing. These runtime results are not claimed by source tests.

Protocol references:

- [Lovense Basic Socket API](https://developer.lovense.com/docs/cam-solutions/socket-api)
- [Engine.IO v3](https://github.com/socketio/socket.io/blob/main/docs/engine.io-protocol/v3.md)
- [Socket.IO protocol](https://github.com/socketio/socket.io-protocol)
- [Project Nayuki QR generator](https://www.nayuki.io/page/qr-code-generator-library)
