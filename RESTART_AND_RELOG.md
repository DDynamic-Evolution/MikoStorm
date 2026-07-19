# Relog Feature

### Concept

A "relog" would return the user to the login screen without closing the viewer application. This would be useful for:

- Switching accounts without restarting
- Recovering from visual/state glitches
- Pairing with restart avoider (auto-relog instead of teleport-away-and-back)

### Current Logout Flow

When the user clicks "Exit MikoStorm":

1. `LLAppViewer::userQuit()` shows confirmation dialog
2. `LLAppViewer::requestQuit()` orchestrates shutdown:
   - Disconnects all IM sessions (`gIMMgr->disconnectAllSessions()`)
   - Closes all floaters
   - Saves teleport history, location history, final screenshot
   - Waits for pending uploads (5s timeout)
   - Sends `LogoutRequest` to server
3. `idleShutdown()` runs a multi-frame state machine:
   - Waits for `LogoutReply` from server (6s timeout)
   - Calls `forceQuit()` which sets `LLApp::setQuitting()`
4. `cleanup()` tears down everything:
   - Agent, regions, world, VO cache, viewer window, audio, textures, messaging
   - Process exits

### The `reset_login()` Function

Located at `indra/newview/llstartup.cpp:4118`, this function already implements most of the teardown needed to return to the login screen. It is currently **only called during login-phase error recovery** (certificate errors, capability failures, timeouts) - never after a successful in-world session.

```
reset_login() does:
  - gAgentWearables.cleanup()
  - gAgentCamera.cleanup()
  - gAgent.cleanup()
  - gSky.cleanup()
  - LLWorld::getInstance()->resetClass()
  - LLAppearanceMgr::getInstance()->cleanup()
  - Hides normal controls, shows login menu bar
  - LLFloaterReg::hideVisibleInstances()
  - Sets startup state to STATE_BROWSER_INIT
  - LLVoiceClient::getInstance()->terminate()
```

### What a Relog Would Need

1. **Send LogoutRequest** to server (`sendLogoutRequest()` or `sendSimpleLogoutRequest()`)
2. **Clean up in-world state** without destroying the application:
   - Call agent/camera/wearable cleanup
   - Call `LLWorld::getInstance()->resetClass()`
   - Fire `LLDestroyClassList` callbacks
   - Destroy `LLVOCache` singleton
3. **Reset all flags**:
   - `mLogoutRequestSent = false`
   - `mQuitRequested = false`
   - `mClosingFloaters = false`
   - `mSavedFinalSnapshot = false`
   - `gDoDisconnect = false`
   - `gLogoutInProgress = false`
   - `gDisconnected = false`
4. **Call `reset_login()`** to restore UI state
5. **Show login panel** via `FSPanelLogin::show()`

### Key Challenges

| Challenge | Detail |
|-----------|--------|
| **Singleton lifecycle** | `LLVOCache` and `LLDestroyClassList` use `deleteSingleton()` which is one-shot. Need to verify they can be recreated. |
| **Voice client** | `LLVoiceClient::terminate()` is a full shutdown. Must use `LLVivoxVoiceClient::logout()` instead to preserve voice for re-login. |
| **`gDisconnected` flag** | Checked in 100+ locations throughout the codebase. Must be carefully managed - set during cleanup, cleared when login succeeds. |
| **Texture/fetch threads** | Global threads that can't be easily restarted. Should flush/pause rather than destroy. |
| **Message system** | Must remain functional for re-login. `disconnectViewer()` partially cleans this up - need a lighter variant. |

### Key Functions

| Function | File | Purpose |
|----------|------|---------|
| `LLAppViewer::userQuit()` | `llappviewer.cpp:5042` | Shows quit confirmation |
| `LLAppViewer::requestQuit()` | `llappviewer.cpp:4963` | Graceful shutdown orchestrator |
| `LLAppViewer::idleShutdown()` | `llappviewer.cpp:6210` | Multi-frame shutdown state machine |
| `LLAppViewer::sendLogoutRequest()` | `llappviewer.cpp:6308` | Sends `LogoutRequest` to server |
| `LLAppViewer::sendSimpleLogoutRequest()` | `llappviewer.cpp:6364` | Simplified logout without marker file |
| `LLAppViewer::disconnectViewer()` | `llappviewer.cpp:6613` | In-world cleanup, sets `gDisconnected` |
| `LLAppViewer::forceQuit()` | `llappviewer.cpp:4942` | Sets app to quitting state |
| `reset_login()` | `llstartup.cpp:4118` | Resets to login state |
| `transition_back_to_login_panel()` | `llstartup.cpp:5201` | Wrapper around `reset_login()` |
| `FSPanelLogin::show()` | `fspanellogin.cpp:494` | Shows the login panel |
| `LLVivoxVoiceClient::logout()` | `llvoicevivox.cpp:2639` | Voice-only logout (preserves client) |

### Startup State Machine

| State | Value | Description |
|-------|-------|-------------|
| `STATE_FIRST` | 58 | Initial startup |
| `STATE_BROWSER_INIT` | 63 | Initialize browser for login |
| `STATE_LOGIN_SHOW` | 64 | Show login screen |
| `STATE_LOGIN_WAIT` | 65 | Waiting for user input |
| `STATE_LOGIN_CLEANUP` | 66 | Login screen closed, start login |
| `STATE_LOGIN_AUTH_INIT` | 70 | Start auth with servers |
| `STATE_WORLD_INIT` | 76 | Start building world |
| `STATE_STARTED` | 92 | Up and running in-world |

A relog would reset from `STATE_STARTED` back to `STATE_BROWSER_INIT`.

### Proposed Menu Item

Add to `menu_viewer.xml` near the existing "Exit MikoStorm":

```xml
<menu_item_call label="Relog" name="Relog">
    <menu_item_call.on_click function="File.Relog" />
</menu_item_call>
```

Callback registered in `llviewermenufile.cpp`, triggering the relog flow described above.

### Estimate

Medium-sized feature. The hardest parts are ensuring singleton lifecycles don't break and that `gDisconnected` state transitions are clean. A conservative approach would add a `gRelogInProgress` flag to gate the special behavior and fall back to full quit if anything goes wrong.
