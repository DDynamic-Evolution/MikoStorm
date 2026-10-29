# Native Windows chat window

Status: 2026-10-29. **Windows AVX2 2.0.5** includes native conversation tabs,
background autoscroll, popout messages with Hide Chat enabled, stable font colors,
automatic Windows appearance, the searchable friend sidebar and Toybox button.
The viewer was incrementally compiled/linked and the NSIS installer built and
verified. Older 2.0.2 installers predate the tab/background/font updates.

## Background following, Hide Chat and font colors (2.0.4)

Autoscroll now keeps following incoming messages while the popout is inactive or
another application has focus. It does not activate the popout or move keyboard
focus. Only deliberate history scrolling or selection changes update the pause/
follow decision. Scroll-to-bottom runs after Rich Edit redraw/layout is enabled
and positions the viewport directly, without depending on a visible caret.
Manually reading older/selected text or disabling Autoscroll still pauses it;
Latest resumes following. These states remain separate for each conversation.

**Hide Chat / FSHideLocalChat** controls the viewer chat presentation. The native
popout continues to receive accepted local messages while Hide Chat is enabled.
This does not turn viewer chat visibility or regular logging back on. The native
history keeps its existing in-memory limits. Muted messages, replay, forwarded
IMs and debug/OAuth messages excluded upstream remain excluded; session and RLVa
checks still apply. Toggling Hide Chat does not clear or duplicate native history.

Changing the popout font size also reapplies the current Rich Edit theme colors
before repainting. Existing text and the default format for later incoming
messages retain their foreground/background colors; font/DPI updates do not
refresh or replace the selected Windows palette.

These changes are compiled and linked into the **2.0.4.0 AVX2** viewer
through CMD/VS2022, with OpenSim, FMOD, 3D Stream and MCP. All 15 native-chat
Python integration checks and both focused native test programs passed: 40
chat/tab/autoscroll checks, plus the theme renderer with 100 resource-safe
refresh cycles and font/color checks at eight sizes across four palettes.
The new NSIS installer was built and verified. No MCP tests or viewer startup
were performed. In-world/visual
validation with actual local/IM/group conversations is still pending.

## Tabs and autoscroll (2.0.4)

Native Windows conversation tabs are included in the verified 2.0.4 installer.
Local chat, IMs and groups each have their own tab,
draft, transcript and reading state. Tabs keep their order while unread labels
change. Overflow arrows allow access to more tabs; labels use native tooltips.
Closing an ordinary viewer IM removes its tab. New private chats still open from
the friend sidebar, and new group conversations use the normal viewer UI.

**Autoscroll / Automatisch scrollen** starts enabled in each conversation. New
messages follow the bottom while no text is selected. Scrolling up or selecting
text pauses following so messages do not move away while being read. Return to
the bottom with the selection cleared to resume automatically, or click
**Latest / Neueste**. The button also re-enables Autoscroll and clears its pending
message count. Its count is capped at 999 and is separate from tab unread counts.
Unchecking Autoscroll keeps following disabled until re-enabled or Latest is used.

Reading position, text selection and the Autoscroll preference are remembered per
conversation while the native window is open. They are not saved to disk. Live
history trimming shifts anchors to the same retained text; expired selections,
inactive snapshots with pruned history and incoming queue gaps reset safely to
the oldest retained message while paused. Account changes clear all reading state,
and a closed/recreated conversation cannot inherit the old lifetime's position.

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

The chat history, inputs, friend list, buttons, tabs and remaining dropdown content use the
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

- Conversation tabs separate **Local chat**, private **IMs** and **Groups** in the
  current source (the released 2.0.2 installer uses a conversation dropdown).
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
subscribe to the earlier raw notification signal. The current source delivers to
the popout before the viewer-only `FSHideLocalChat` return. It excludes muted
messages, IM/group-IM types and history replay, and additionally
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
- `fsnativechatview.h`: native tab identities and per-conversation Rich Edit
  following, UTF-16 reading anchors, selection restoration and message counts.
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

The following tab/scroll results predate the background/Hide Chat source fixes.
For the earlier tabs/autoscroll update, the changed production
`fsnativechatwin32.cpp` translation unit compiles with the AVX2 Release project
through CMD/VS2022. The focused `native_chat_view_regression` uses actual hidden
Win32 tabs and Rich Edit controls to check stable identities/order, overflow,
following/pause/resume, per-tab restoration, UTF-16 positions beyond 65535,
trimming, expired snapshots, queue gaps and bounded counts. The theme renderer
also draws tabs across 100 resource-safe refresh cycles. Both native programs and
all 12 native-chat Python integration checks passed before the background fixes.
A later 2.0.3 AVX2 build linked the viewer and prepared/wrote the NSIS setup, but
its completion was not confirmed and the payload was not verified or delivered.
No MCP tests or viewer startup were performed for that build.

The following release validation describes the existing installer:

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
   open/minimized. Check title bar, text, selection, checkboxes, tabs, dropdowns, focus,
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
9. Tab overflow, keyboard tab selection, Unicode/duplicate labels and unread
   updates while browsing the overflow strip. Reading position, selection and
   Autoscroll preference must survive switching local/IM/group conversations.
10. Autoscroll while another application has focus, while typing in the main viewer
   and after minimizing/restoring. Incoming messages must reach the bottom without
   taking focus; deliberate reading pauses and the disabled checkbox must persist.
11. Enable/disable Hide Chat while receiving local chat. Native messages continue
   exactly once; viewer chat remains hidden. Mute, RLV and replay exclusions hold.
12. Autoscroll pause/resume via wheel, scrollbar and keyboard; Latest and checkbox;
   incoming bursts, reading selected/scrolled text, bounded history, repeated
   open/close, missing Rich Edit and orderly viewer exit during window startup.

API references:
[Tab controls](https://learn.microsoft.com/en-us/windows/win32/controls/tab-control-styles),
[Rich Edit redraw/layout](https://learn.microsoft.com/en-us/windows/win32/gdi/wm-setredraw),
[Rich Edit scroll-position limitations](https://learn.microsoft.com/en-us/windows/win32/controls/em-getscrollpos),
[Windows message loops](https://learn.microsoft.com/en-us/windows/win32/winmsg/using-messages-and-message-queues),
[Rich Edit](https://learn.microsoft.com/en-us/windows/win32/controls/rich-edit-controls),
[control subclassing](https://learn.microsoft.com/en-us/windows/win32/controls/subclassing-overview),
[Windows application color preference](https://learn.microsoft.com/en-us/windows/apps/develop/settings/settings-common#personalization---colors),
[DWM attributes](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute),
[theme notifications](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-themechanged),
[owner drawing](https://learn.microsoft.com/en-us/windows/win32/controls/wm-drawitem).
