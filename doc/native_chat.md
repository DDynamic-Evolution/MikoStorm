# Native Windows chat window

Status: 2026-10-28. **Windows AVX2 2.0.2** includes automatic Windows application
Light/Dark/High Contrast appearance and the searchable friend sidebar, along with
local/private/group chats and the Toybox button. The viewer was incrementally
compiled and linked, and the NSIS installer was built and verified. The older
2.0.1 installer predates the theme/friend additions.

## Use

Open **Communicate → Chat in Windows Window…**, or the **↗** button in Nearby
Chat's toolbar. In German: **Unterhalten → Chat als Windows-Fenster…**.
The new **Windows Chat / Windows-Chat** button is available in the Toybox; drag
it onto a toolbar. It opens/focuses the same window and shows its running state.
It is disabled on platforms without native-window support.
The command restores/focuses the same popout if it is already open.

## Windows appearance and friends (2.0.2)

The window follows the current Windows **application** light/dark preference on
creation and on settings/theme/system-color notifications. High Contrast takes
priority and uses system foreground, background and selection colors. The title
bar uses the supported DWM dark-mode attribute; systems that reject it keep their
ordinary native title bar. Without a readable app-mode setting, controls fall
back to system colors. The code only reads the preference and never changes
Windows theme, security or privacy settings.

The chat history, inputs, friend list, buttons and dropdown content use the
matching palette. Theme changes preserve conversation selection, drafts and
history. Current font size and Always on top behavior stay available. Display,
DPI, screen-reader and live theme-switch behavior need manual validation.

The sidebar shows friends known to the viewer, **online first**, then sorted by
name. A search field filters names without case sensitivity. Double-click a
friend, press Enter in the friend list, or select one and use **Open chat / Chat
öffnen**. Existing IMs are selected; otherwise the ordinary IM session manager
opens a new private session. Offline friends are selectable too. Opening never
sends a message, starts a call or changes friendship rights.

Friend data is copied on the viewer main thread about once per second while the
popout is open. Names use the normal avatar-name cache and existing RLV name
filtering; an unresolved name temporarily shows a loading label. The native UI
thread holds only copied IDs, labels, status and membership tokens. Open requests
recheck the current login generation, friend membership, recipient, RLV start-IM
rule and conversation capacity. A removed/readded friend or another account
cannot reuse an old queued request. No asynchronous name callback can open a chat
later under a different login.

Friend search/filter state and copied contacts clear on account change. Up to
4096 friend rows are sent to the native window; the heading shows displayed/total
counts so truncation and search are visible. Friend data stays in memory and is
not added to saved settings or another file. The normal viewer Friends UI remains
available. New group conversations still open through the ordinary viewer UI.

## Chat window and conversations

The popout is a real unowned Windows top-level window with normal resizing,
minimize/maximize, Alt+Tab and taskbar presence (subject to Windows taskbar
grouping). It can be placed on another monitor and stays open when the main
viewer window is minimized. It remains attached to the current viewer login.
Closing the popout leaves the viewer running; viewer cleanup closes and joins
its UI thread before chat/settings services are destroyed.

- A conversation dropdown separates **Local chat**, private **IMs** and **Groups**.
  Existing viewer IM/group sessions appear automatically (within about a second);
  new private chats can also open from the friend sidebar in version 2.0.2.
  New groups use the ordinary viewer UI; the popout does not join/leave/snooze them.
- Each conversation has its own current-session transcript and in-memory draft.
  Switching clears that conversation's native unread indicator; counts on other
  conversations are capped at 999 and are separate from viewer unread/toast state.
- Current-session live messages, names, configured timestamps and `/me` text.
  Cached/server history and group announcements are not imported.
- Local send, whisper and shout still use `FSNearbyChat::sendChat` on the main
  thread. The local mode dropdown is disabled for IM/group conversations.
- IM/group replies use `FSFloaterIM::sendNativeMessage` → `sendMsg`, keeping RLVa
  filtering, support-contact restrictions, text capability and moderator mute
  checks. The viewer editor's draft is not replaced. Incomplete sessions reject
  submission instead of queueing delayed sends; the native draft is retained.
  Native IM text supports normal OOC/MU-pose conversion, but not the viewer
  editor's channel redirection, /sysinfo expansion or completion features.
- Enter submits; Shift+Enter inserts a line break. The draft is kept if input is
  empty, the viewer is disconnected, the queue is full or the UTF-8 message is
  too long for the viewer's existing `MAX_STRING - 1` input path.
- A submission acknowledged by the viewer clears only the draft actually
  submitted; text edited while it was pending is kept. This acknowledgement does
  not assert that the simulator accepted a message or that RLV allowed it.
- Select/copy transcript text; HTTP/HTTPS links open through `LLWeb::loadURL`.
  Other URL schemes are not launched by the native bridge.
- Font sizes 10/12/14/16/18/20/24/28 points and optional Always on top.
- Normal window bounds, font size and topmost state are saved. The friend-sidebar
  layout has a minimum window size of 820 × 420 pixels. Removed/offscreen
  monitor positions are adjusted to the nearest available work area on reopen.
- Ad-hoc/conference chats, voice controls, avatar context menus, completion/emoji
  pickers and native typing notifications are outside this extension.

The main-thread idle loop still processes incoming chat and submissions, so
background throttling or a busy viewer may delay those updates. The native UI
message loop remains separate. Fullscreen switching and mixed-DPI behavior need
manual runtime verification in the new build.

## Chat rules and session isolation

The mirror is fed from `FSFloaterNearbyChat::addMessage` inside the accepted,
unmuted local-chat branch, after the normal handler's filtering. It does not
subscribe to the earlier raw notification signal. It respects `FSHideLocalChat`,
excludes muted messages, IM/group-IM types and history replay, and additionally
masks restricted names/locations through the existing RLVa helpers.

Local sending passes through the existing nearby-chat service, including its
RLVa restrictions and redirection. There is no separate network connection,
MCP dependency or raw simulator-message sending path.

The new IM/group mirror is fed only from `LLIMModel::addToHistory` after the normal
session message insertion, including regular own-message echoes. It applies mute
and RLVa receive/name/location gates again; it does not tap raw simulator events
or read transcript files. IMs forwarded into Nearby Chat are excluded from the
local mirror, so they are not duplicated. The ordinary viewer history remains
responsible for historical messages and logging.

The model holds at most 64 conversations including Local chat, without evicting
unsent drafts to admit more. Close an ordinary IM/group session to free a slot.
Each conversation retains up to 200 entries / 128 KiB UTF-8 in memory. An individual
entry is visibly clipped at 8192 bytes without cutting a UTF-8 codepoint.
The outgoing command queue and pending incoming queue are bounded. Native
history is plain text; received text is not loaded as RTF.

Closing an ordinary viewer session removes its native conversation and falls back
to Local chat if it was selected. A session-ID update preserves its draft/history,
but changes its lifetime token; queued commands for the old identity are rejected.
The native selector is disabled while a send is awaiting the main thread, and a
reply can only acknowledge its original conversation/lifetime.

History and drafts are cleared on logout/account/session change. Commands are
tagged with the displayed session generation and checked again before sending;
a stale native window cannot submit its previous session's text under a new
login. The closed window's bridge is cleared too, so reopening cannot recover a
previous account's draft. A temporary region pointer loss during an otherwise
live teleport disables sending without discarding the session history.

Draft/history stay in memory and are not added to saved settings or a new chat
log file. Existing viewer logging remains responsible for normal chat logging.
The popout does not reopen automatically after restarting the viewer.

## Implementation

- `fsnativechatwin32.cpp`: UI thread, Win32/Rich Edit controls, bounded copied-data
  bridge, main-thread session checks, formatting, settings and normal chat send.
- `fsnativechat.h`: main-thread entry points and no-op unsupported-platform stubs.
- `fsnativechatmodel.h`: transcripts, conversation/friend lifetimes and theme policy.
- `fsnativechattheme.h`: read-only Windows app-theme detection, DWM title bar,
  system/High Contrast fallback, owner drawing and GDI brush lifetime.
- `llappviewer.cpp`: initialize service; close/join at the beginning of cleanup.
- `fsfloaternearbychat.cpp`: mirror accepted messages; Windows-only toolbar action.
- `llimview.cpp`: accepted IM messages, clear-session and ID-update hooks.
- `fsfloaterim.cpp`: native send adapter retaining ordinary IM guards and delivery.
- `app_settings/commands.xml`: Toybox command using the existing chat icon.
- `llviewermenu.cpp`, English/German menus, strings and notifications: entry points
  and labels. The controls are disabled on platforms without implementation.
- `CMakeLists.txt`: Windows-only source and Comctl32 dependency.
- Settings: `FSNativeChatRect`, `FSNativeChatTopmost`, `FSNativeChatFontSize`.

The UI thread owns its HWNDs, fonts and Rich Edit module and uses a dedicated
`GetMessageW` loop. It never accesses viewer settings, agent, chat services,
translations or browser functions. The main thread drains copied commands and
hands back status; neither side holds the bridge mutex across a reentrant native
control call. Normal focus handling keeps native typing away from avatar keys.

## Validation

The full **2.0.2** AVX2 viewer compiled and linked successfully through CMD/VS2022.
All **21 Python integration checks** and all **four native test programs** passed:
**215 core** checks, nine avatar-log checks, eight camera checks and the Windows
theme renderer test. The renderer test used hidden Win32 controls and 100 refresh/
draw cycles to detect GDI resource growth, without changing Windows settings.

The completed NSIS payload was checked against the release executable, FMOD and
browser libraries, settings and translated UI, including the Toybox command and
new friend/theme feature markers. These tests do not establish visual correctness,
actual live theme notifications, or logged-in friend/private/group behavior.

Manual checks still required:

1. Switch Windows application Light/Dark and High Contrast while the popout is
   open/minimized. Check title bar, text, selection, checkbox, dropdowns, focus,
   links and copied text. Draft/history must remain intact; test DPI moves too.
2. Friend sorting/search with Unicode names, status/name changes, offline IM,
   duplicate display names, removal/readding, and pending open on logout/account
   change. Check RLV name/start-IM rules and the full-conversation error.
3. Open via menu, Nearby Chat and Toybox in both chat styles; drag the Toybox
   button to a toolbar, check running state, reopen/focus and close independently.
4. Private and group receive/send/own echo; switch with drafts, pending sends and
   new messages. Check moderator mute, session initialization/closure/rekey,
   limits and the exclusion of muted messages, announcements and replay.
5. Viewer minimized, two monitors including negative coordinates, monitor removal,
   maximize/restore and mixed-DPI moves; font/topmost persistence.
6. Local send/receive, whisper/shout, `/me`, channel syntax, gestures, Unicode and
   long input; keyboard focus, IME Enter, copy/paste and Shift+Enter.
7. Mute/debug/OAuth filtering and RLVa name/location/send restrictions; IM/group messages appear only in their own conversations; ordinary logging unaffected.
8. Disconnect/logout/account change with a pending send or unsent draft, also with
   the popout already closed; old transcript/draft must not reappear.
9. Incoming bursts, reading selected/scrolled text, bounded history, repeated
   open/close, missing Rich Edit and orderly viewer exit during window startup.

API references:
[Windows message loops](https://learn.microsoft.com/en-us/windows/win32/winmsg/using-messages-and-message-queues),
[Rich Edit](https://learn.microsoft.com/en-us/windows/win32/controls/rich-edit-controls),
[control subclassing](https://learn.microsoft.com/en-us/windows/win32/controls/subclassing-overview),
[Windows application color preference](https://learn.microsoft.com/en-us/windows/apps/develop/settings/settings-common#personalization---colors),
[DWM attributes](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute),
[theme notifications](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-themechanged),
[owner drawing](https://learn.microsoft.com/en-us/windows/win32/controls/wm-drawitem).
