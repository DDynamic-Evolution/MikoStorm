#include "llviewerprecompiledheaders.h"
#include "fsnativechat.h"
#include "fsnativechatmodel.h"
#include "fsnativechattheme.h"
#include "llcallingcard.h"
#include "llavatarnamecache.h"
#include "llavatarname.h"
#include "fsnearbychathub.h"
#include "fsfloaterim.h"
#include "llimview.h"
#include "llmutelist.h"
#include <chrono>
#include "llagent.h"
#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llchat.h"
#include "llnotificationsutil.h"
#include "llsdutil.h"
#include "llthread.h"
#include "llstartup.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llweb.h"
#include "rlvactions.h"
#include "rlvcommon.h"
#include "res/resource.h"
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <ole2.h>
#include <atomic>
#include <cstring>
#include <array>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// Each native window owns its HWNDs and message loop on a dedicated UI thread.
// Viewer settings, chat and browser calls stay on the viewer's main thread.
namespace
{
constexpr UINT Wake = WM_APP + 173;
constexpr std::array<int, 8> FontSizes{10, 12, 14, 16, 18, 20, 24, 28};
constexpr wchar_t WindowClass[] = L"MikoStorm.NativeNearbyChat.1";
constexpr int SendID = 101, TopID = 102, FontID = 103, InputID = 104, ModeID = 105, ConversationID = 106, FriendSearchID = 107, FriendListID = 108, OpenFriendID = 109;
std::wstring wide(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring output(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(), size);
    return output;
}
std::string narrow(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string output(size, '\0');
    if (size) WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(), size, nullptr, nullptr);
    return output;
}
std::wstring textOf(HWND control)
{
    const int count = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(count) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), count + 1);
    text.resize(copied); return text;
}
enum class Reply { Ready, Empty, TooLong, Offline, OpenFailed };
struct Labels
{
    std::wstring title, send, top, font, say, whisper, shout, ready, offline, empty, tooLong, busy, friends, search, openFriend, friendOnline, friendOffline, openFailed;
};
struct Choice
{
    std::string key, label;
    U64 token = 0;
    bool operator==(const Choice&) const = default;
};
struct Command
{
    enum Kind { Send, Link, Select, OpenFriend } kind;
    std::string text;
    int mode = 0;
    U64 generation = 0;
    std::string conversation;
    U64 token = 0;
};
struct Bridge
{
    std::mutex mutex;
    std::deque<std::string> lines;
    std::deque<Command> commands;
    std::optional<Reply> reply;
    bool reset = false, connected = false, settingsDirty = false;
    bool topmost = false;
    int points = 14;
    std::array<int, 4> rect{80, 80, 640, 500};
    std::string draft, error, conversation;
    U64 token = 0;
    bool local = true, accountReady = false;
    std::optional<std::vector<FSNativeChat::FriendEntry>> friends;
    std::size_t friendTotal = 0;
    std::optional<std::vector<Choice>> choices;
    U64 generation = 0;
    std::atomic<HWND> window{nullptr};
    std::atomic<bool> stop{false}, finished{false}, wakePending{false}, front{false}, focusInput{false};
    void wake()
    {
        const auto hwnd = window.load();
        if (hwnd && !wakePending.exchange(true) && !PostMessageW(hwnd, Wake, 0, 0)) wakePending = false;
    }
};

// UI THREAD ONLY. This class must never access gSavedSettings, gAgent or any
// viewer singleton. Bridge carries bounded, copied data in both directions.
class NativeWindow
{
public:
    NativeWindow(std::shared_ptr<Bridge> bridge, Labels labels) : mBridge(std::move(bridge)), mLabels(std::move(labels)) {}
    ~NativeWindow()
    {
        if (mWindow) DestroyWindow(mWindow);
        if (mFont) DeleteObject(mFont);
        if (mRichEdit) FreeLibrary(mRichEdit);
        if (mRegistered) UnregisterClassW(WindowClass, GetModuleHandleW(nullptr));
        if (mOle) OleUninitialize();
    }
    void run()
    {
        if (mBridge->stop) return;
        mOle = SUCCEEDED(OleInitialize(nullptr));
        mRichEdit = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!mRichEdit) { fail("Windows Rich Edit could not be loaded"); return; }
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = windowProc;
        wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = WindowClass;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_LL_ICON));
        wc.hIconSm = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_LL_ICON_SMALL));
        mRegistered = RegisterClassExW(&wc) != 0;
        if (!mRegistered && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { fail("Could not register native chat window"); return; }
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            mRect = mBridge->rect; mTopmost = mBridge->topmost; mPoints = mBridge->points;
        }
        RECT desired{mRect[0], mRect[1], mRect[0] + mRect[2], mRect[1] + mRect[3]};
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromRect(&desired, MONITOR_DEFAULTTONEAREST), &monitor))
        {
            mRect[2] = (std::min)(mRect[2], static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
            mRect[3] = (std::min)(mRect[3], static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top));
            mRect[0] = (std::max)(static_cast<int>(monitor.rcWork.left), (std::min)(mRect[0], static_cast<int>(monitor.rcWork.right) - mRect[2]));
            mRect[1] = (std::max)(static_cast<int>(monitor.rcWork.top), (std::min)(mRect[1], static_cast<int>(monitor.rcWork.bottom) - mRect[3]));
        }
        if (mBridge->stop) return;
        HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW | (mTopmost ? WS_EX_TOPMOST : 0), WindowClass, mLabels.title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, mRect[0], mRect[1], mRect[2], mRect[3], nullptr, nullptr, wc.hInstance, this);
        if (!hwnd) { fail("Native chat controls could not be created"); return; }
        mBridge->window = hwnd;
        if (mBridge->stop) { DestroyWindow(hwnd); return; }
        ShowWindow(hwnd, SW_SHOWNORMAL); UpdateWindow(hwnd); drain(); SetFocus(mInput);
        MSG message{}; BOOL result;
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0)
        {
            if (message.message == WM_KEYDOWN && message.wParam == VK_TAB && !(GetKeyState(VK_CONTROL) & 0x8000))
            {
                if (HWND next = GetNextDlgTabItem(hwnd, GetFocus(), (GetKeyState(VK_SHIFT) & 0x8000) != 0)) SetFocus(next);
                continue;
            }
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (result == -1) fail("Native chat message loop failed");
    }
private:
    void fail(const char* error)
    {
        const auto code = GetLastError();
        std::lock_guard<std::mutex> lock(mBridge->mutex);
        mBridge->error = std::string(error) + " (Win32 " + std::to_string(code) + ")";
    }
    HWND child(const wchar_t* kind, const wchar_t* text, DWORD style, int id)
    {
        HWND hwnd = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1,
            mWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        if (hwnd) SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        return hwnd;
    }
    bool controls()
    {
        mFriendLabel = child(L"STATIC", mLabels.friends.c_str(), 0, 0);
        mFriendSearch = child(L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, FriendSearchID);
        mFriendList = child(L"LISTBOX", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, FriendListID);
        mOpenFriend = child(L"BUTTON", mLabels.openFriend.c_str(), WS_TABSTOP | BS_OWNERDRAW, OpenFriendID);
        mConversations = child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, ConversationID);
        mMode = child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, ModeID);
        mTop = child(L"BUTTON", mLabels.top.c_str(), WS_TABSTOP | BS_AUTOCHECKBOX, TopID);
        mFontLabel = child(L"STATIC", mLabels.font.c_str(), 0, 0);
        mFontSize = child(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, FontID);
        mHistory = child(L"RICHEDIT50W", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0);
        mInput = child(L"EDIT", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, InputID);
        mSend = child(L"BUTTON", mLabels.send.c_str(), WS_TABSTOP | BS_OWNERDRAW, SendID);
        mStatus = child(L"STATIC", mLabels.offline.c_str(), 0, 0);
        if (!mFriendLabel || !mFriendSearch || !mFriendList || !mOpenFriend || !mConversations || !mMode || !mTop || !mFontLabel || !mFontSize || !mHistory || !mInput || !mSend || !mStatus) return false;
        SendMessageW(mFriendSearch, EM_SETLIMITTEXT, 128, 0);
        SendMessageW(mFriendSearch, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(mLabels.search.c_str()));
        if (!SetWindowSubclass(mFriendList, auxiliaryProc, 1, reinterpret_cast<DWORD_PTR>(this))) return false;
        for (const HWND combo : {mConversations, mMode, mFontSize})
            if (!SetWindowSubclass(combo, auxiliaryProc, 1, reinterpret_cast<DWORD_PTR>(this))) return false;
        SendMessageW(mMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mLabels.say.c_str()));
        SendMessageW(mMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mLabels.whisper.c_str()));
        SendMessageW(mMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mLabels.shout.c_str()));
        SendMessageW(mMode, CB_SETCURSEL, 0, 0);
        SendMessageW(mTop, BM_SETCHECK, mTopmost ? BST_CHECKED : BST_UNCHECKED, 0);
        for (int points : FontSizes)
        {
            const auto label = std::to_wstring(points);
            const LRESULT index = SendMessageW(mFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            SendMessageW(mFontSize, CB_SETITEMDATA, index, points);
            if (points == mPoints) SendMessageW(mFontSize, CB_SETCURSEL, index, 0);
        }
        SendMessageW(mHistory, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        SendMessageW(mHistory, EM_EXLIMITTEXT, 0, 256 * 1024);
        SendMessageW(mHistory, EM_AUTOURLDETECT, TRUE, 0);
        SendMessageW(mHistory, EM_SETEVENTMASK, 0, ENM_LINK);
        SendMessageW(mInput, EM_SETLIMITTEXT, 4096, 0);
        if (!SetWindowSubclass(mInput, inputProc, 1, reinterpret_cast<DWORD_PTR>(this))) return false;
        std::string draft;
        { std::lock_guard<std::mutex> lock(mBridge->mutex); draft = mBridge->draft; }
        // SetWindowText sends EN_CHANGE synchronously; never hold Bridge::mutex here.
        SetWindowTextW(mInput, wide(draft).c_str());
        mReady = true; applyFont(); applyTheme(); layout(); return true;
    }
    void applyFont()
    {
        UINT dpi = 96;
        using DpiFn = UINT(WINAPI*)(HWND);
        const auto address = GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
        DpiFn getDpi = nullptr;
        static_assert(sizeof(getDpi) == sizeof(address));
        std::memcpy(&getDpi, &address, sizeof(getDpi));
        if (getDpi) dpi = getDpi(mWindow);
        HFONT font = CreateFontW(-MulDiv(mPoints, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (!font) return;
        SendMessageW(mHistory, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(mInput, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        if (mFont) DeleteObject(mFont);
        mFont = font;
    }
    void applyTheme()
    {
        if (!mReady) return;
        mTheme.refresh(mWindow); mTheme.richEdit(mHistory);
        RedrawWindow(mWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    void layout()
    {
        if (!mReady) return;
        RECT area{}; GetClientRect(mWindow, &area); const int width = area.right, height = area.bottom;
        const int sidebar = (std::max)(180, (std::min)(240, width / 4));
        const int left = sidebar + 20, chatWidth = (std::max)(1, width - left - 10);
        MoveWindow(mFriendLabel, 10, 10, sidebar, 22, TRUE);
        MoveWindow(mFriendSearch, 10, 36, sidebar, 25, TRUE);
        MoveWindow(mFriendList, 10, 68, sidebar, (std::max)(1, height - 113), TRUE);
        MoveWindow(mOpenFriend, 10, (std::max)(80, height - 36), sidebar, 27, TRUE);
        MoveWindow(mConversations, left, 8, chatWidth, 360, TRUE);
        MoveWindow(mMode, left, 42, 110, 180, TRUE); MoveWindow(mTop, left + 122, 44, (std::max)(100, chatWidth - 300), 22, TRUE);
        MoveWindow(mFontLabel, width - 170, 46, 95, 20, TRUE); MoveWindow(mFontSize, width - 73, 42, 63, 220, TRUE);
        MoveWindow(mHistory, left, 76, chatWidth, (std::max)(1, height - 186), TRUE);
        MoveWindow(mInput, left, (std::max)(76, height - 100), (std::max)(1, chatWidth - 110), 64, TRUE);
        MoveWindow(mSend, width - 110, (std::max)(76, height - 100), 100, 28, TRUE);
        MoveWindow(mStatus, left, (std::max)(144, height - 27), chatWidth, 22, TRUE);
    }
    void updateFriends()
    {
        std::string selected;
        const auto old = SendMessageW(mFriendList, LB_GETCURSEL, 0, 0);
        if (old >= 0 && static_cast<std::size_t>(old) < mFilteredFriends.size()) selected = mFilteredFriends[static_cast<std::size_t>(old)].id;
        const auto query = textOf(mFriendSearch);
        SendMessageW(mFriendList, WM_SETREDRAW, FALSE, 0); SendMessageW(mFriendList, LB_RESETCONTENT, 0, 0);
        mFilteredFriends.clear();
        for (const auto& entry : mFriends)
        {
            const auto name = wide(entry.name);
            if (!query.empty() && FindNLSStringEx(LOCALE_NAME_USER_DEFAULT, FIND_FROMSTART | NORM_IGNORECASE,
                name.data(), static_cast<int>(name.size()), query.data(), static_cast<int>(query.size()), nullptr, nullptr, nullptr, 0) < 0) continue;
            const auto label = (entry.online ? mLabels.friendOnline : mLabels.friendOffline) + L" — " + name;
            const auto index = SendMessageW(mFriendList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            if (index < 0) continue;
            mFilteredFriends.push_back(entry);
            if (entry.id == selected) SendMessageW(mFriendList, LB_SETCURSEL, index, 0);
        }
        const auto caption = mLabels.friends + L" (" + std::to_wstring(mFilteredFriends.size()) + L"/" + std::to_wstring(mFriendTotal) + L")";
        SetWindowTextW(mFriendLabel, caption.c_str());
        SendMessageW(mFriendList, WM_SETREDRAW, TRUE, 0); InvalidateRect(mFriendList, nullptr, TRUE);
    }
    void openFriend()
    {
        if (!mAccountReady || mPending || mSwitching) return;
        const auto index = SendMessageW(mFriendList, LB_GETCURSEL, 0, 0);
        if (index < 0 || static_cast<std::size_t>(index) >= mFilteredFriends.size()) return;
        saveDraft();
        const auto& entry = mFilteredFriends[static_cast<std::size_t>(index)];
        Command command{Command::OpenFriend, "", 0, mGeneration}; command.conversation = entry.id; command.token = entry.token;
        bool full;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex); full = mBridge->commands.size() >= 16;
            if (!full) mBridge->commands.push_back(std::move(command));
        }
        if (full) { SetWindowTextW(mStatus, mLabels.busy.c_str()); return; }
        mSwitching = true;
        EnableWindow(mOpenFriend, FALSE); EnableWindow(mConversations, FALSE); EnableWindow(mInput, FALSE); EnableWindow(mSend, FALSE);
    }
    void settings()
    {
        if (!mReady) return;
        if (!IsIconic(mWindow) && !IsZoomed(mWindow))
        {
            RECT rect{}; if (GetWindowRect(mWindow, &rect)) mRect = {static_cast<int>(rect.left), static_cast<int>(rect.top), static_cast<int>(rect.right - rect.left), static_cast<int>(rect.bottom - rect.top)};
        }
        std::lock_guard<std::mutex> lock(mBridge->mutex);
        mBridge->rect = mRect; mBridge->topmost = mTopmost; mBridge->points = mPoints; mBridge->settingsDirty = true;
    }
    void saveDraft()
    {
        if (!mInput) return;
        auto draft = narrow(textOf(mInput));
        std::lock_guard<std::mutex> lock(mBridge->mutex); if (mGeneration == mBridge->generation && mConversation == mBridge->conversation && mToken == mBridge->token) mBridge->draft = std::move(draft);
    }
    void send()
    {
        if (mPending || mSwitching || !mConnected) return;
        const auto draft = textOf(mInput);
        if (draft.empty()) return;
        Command command{Command::Send, narrow(draft), static_cast<int>(SendMessageW(mMode, CB_GETCURSEL, 0, 0)), mGeneration};
        command.conversation = mConversation; command.token = mToken;
        bool full;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            full = mBridge->commands.size() >= 16;
            if (!full) mBridge->commands.push_back(std::move(command));
        }
        if (full) { SetWindowTextW(mStatus, mLabels.busy.c_str()); return; }
        mSubmitted = draft; mPending = true; EnableWindow(mSend, FALSE); EnableWindow(mConversations, FALSE); EnableWindow(mOpenFriend, FALSE);
    }
    void render(const std::deque<std::string>& lines, bool reset)
    {
        if (lines.empty() && !reset) return;
        SCROLLINFO scroll{}; scroll.cbSize = sizeof(scroll); scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        GetScrollInfo(mHistory, SB_VERT, &scroll);
        CHARRANGE selected{}; SendMessageW(mHistory, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selected));
        POINT position{}; SendMessageW(mHistory, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&position));
        const bool follow = selected.cpMin == selected.cpMax && scroll.nPos + static_cast<int>(scroll.nPage) >= scroll.nMax - 2;
        if (reset) mTranscript.clear();
        const auto dropped = mTranscript.dropped();
        for (const auto& line : lines) mTranscript.append(line);
        // Once old text expires, never keep a selection pointing at different
        // messages. Ordinary appends preserve both selection and scroll position.
        if (mTranscript.dropped() != dropped) { selected = {0, 0}; position = {0, 0}; }
        std::string text; for (const auto& line : mTranscript.lines()) text += line + "\r\n";
        const auto display = wide(text);
        SendMessageW(mHistory, WM_SETREDRAW, FALSE, 0);
        SetWindowTextW(mHistory, display.c_str());
        if (follow || reset)
        { SendMessageW(mHistory, EM_SETSEL, static_cast<WPARAM>(-1), -1); SendMessageW(mHistory, EM_SCROLLCARET, 0, 0); }
        else
        {
            const auto size = static_cast<LONG>(display.size());
            selected.cpMin = (std::min)(selected.cpMin, size); selected.cpMax = (std::min)(selected.cpMax, size);
            SendMessageW(mHistory, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selected));
            SendMessageW(mHistory, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&position));
        }
        SendMessageW(mHistory, WM_SETREDRAW, TRUE, 0); InvalidateRect(mHistory, nullptr, TRUE);
    }
    void updateChoices()
    {
        SendMessageW(mConversations, CB_RESETCONTENT, 0, 0);
        for (std::size_t i = 0; i < mChoices.size(); ++i)
        {
            const auto text = wide(mChoices[i].label);
            const auto index = SendMessageW(mConversations, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
            if (mChoices[i].key == mConversation && mChoices[i].token == mToken)
                SendMessageW(mConversations, CB_SETCURSEL, index, 0);
        }
    }
    void selectConversation()
    {
        if (mPending || mSwitching) return;
        const auto index = SendMessageW(mConversations, CB_GETCURSEL, 0, 0);
        if (index < 0 || static_cast<std::size_t>(index) >= mChoices.size()) return;
        saveDraft();
        const auto& choice = mChoices[static_cast<std::size_t>(index)];
        Command command{Command::Select, "", 0, mGeneration};
        command.conversation = choice.key; command.token = choice.token;
        bool full;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            full = mBridge->commands.size() >= 16;
            if (!full) mBridge->commands.push_back(std::move(command));
        }
        if (full) { updateChoices(); SetWindowTextW(mStatus, mLabels.busy.c_str()); return; }
        mSwitching = true;
        EnableWindow(mConversations, FALSE); EnableWindow(mInput, FALSE); EnableWindow(mSend, FALSE); EnableWindow(mOpenFriend, FALSE);
    }
    void drain()
    {
        mBridge->wakePending = false;
        if (mBridge->stop) { DestroyWindow(mWindow); return; }
        std::deque<std::string> lines; std::optional<Reply> reply; bool reset, local;
        const U64 oldGeneration = mGeneration;
        std::string draft; std::optional<std::vector<Choice>> choices;
        std::optional<std::vector<FSNativeChat::FriendEntry>> friends;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            lines.swap(mBridge->lines); reply = mBridge->reply; mBridge->reply.reset();
            reset = mBridge->reset; mBridge->reset = false; mConnected = mBridge->connected; mGeneration = mBridge->generation;
            mConversation = mBridge->conversation; mToken = mBridge->token; local = mBridge->local;
            if (reset) draft = mBridge->draft;
            choices.swap(mBridge->choices); friends.swap(mBridge->friends);
            mFriendTotal = mBridge->friendTotal; mAccountReady = mBridge->accountReady;
        }
        if (oldGeneration != mGeneration)
        {
            mFriends.clear(); mFilteredFriends.clear(); SetWindowTextW(mFriendSearch, L"");
            SendMessageW(mFriendList, LB_RESETCONTENT, 0, 0);
        }
        if (friends) mFriends = std::move(*friends);
        if (friends || oldGeneration != mGeneration) updateFriends();
        if (choices) mChoices = std::move(*choices);
        if (reset)
        {
            // The copied selection must be installed before EN_CHANGE can save.
            SetWindowTextW(mInput, wide(draft).c_str());
            mPending = false; mSwitching = false; mSubmitted.clear();
        }
        if (choices || reset) updateChoices();
        EnableWindow(mMode, local);
        render(lines, reset);
        if (reply)
        {
            if (*reply == Reply::Ready && textOf(mInput) == mSubmitted) SetWindowTextW(mInput, L"");
            mPending = false; mSubmitted.clear();
            const auto& status = *reply == Reply::Empty ? mLabels.empty : *reply == Reply::TooLong ? mLabels.tooLong : *reply == Reply::Offline ? mLabels.offline : *reply == Reply::OpenFailed ? mLabels.openFailed : mLabels.ready;
            SetWindowTextW(mStatus, status.c_str()); saveDraft();
        }
        else SetWindowTextW(mStatus, (mConnected ? mLabels.ready : mLabels.offline).c_str());
        EnableWindow(mOpenFriend, mAccountReady && !mPending && !mSwitching);
        EnableWindow(mFriendSearch, mAccountReady);
        EnableWindow(mConversations, !mPending && !mSwitching);
        EnableWindow(mSend, mConnected && !mPending && !mSwitching); EnableWindow(mInput, mConnected && !mSwitching);
        if (mBridge->focusInput.exchange(false) && mConnected && !mSwitching) SetFocus(mInput);
        if (mBridge->front.exchange(false))
        { ShowWindow(mWindow, IsIconic(mWindow) ? SW_RESTORE : SW_SHOW); SetForegroundWindow(mWindow); SetFocus(mInput); }
    }
    LRESULT handle(UINT msg, WPARAM wparam, LPARAM lparam)
    {
        switch (msg)
        {
        case WM_CREATE: return controls() ? 0 : -1;
        case Wake: drain(); return 0;
        case WM_THEMECHANGED: case WM_SYSCOLORCHANGE: case WM_SETTINGCHANGE: applyTheme(); break;
        case WM_ERASEBKGND:
        {
            RECT rect{}; GetClientRect(mWindow, &rect); FillRect(reinterpret_cast<HDC>(wparam), &rect, mTheme.backgroundBrush); return 1;
        }
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
        {
            const HWND control = reinterpret_cast<HWND>(lparam);
            const bool field = msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX || control == mHistory;
            return reinterpret_cast<LRESULT>(mTheme.colorControl(reinterpret_cast<HDC>(wparam), field, IsWindowEnabled(control) != FALSE));
        }
        case WM_MEASUREITEM:
        {
            auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam); item->itemHeight = 24; return TRUE;
        }
        case WM_DRAWITEM:
        {
            const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
            if (mTheme.drawItem(*item, item->CtlID == TopID, mTopmost)) return TRUE;
            break;
        }
        case WM_SIZE:
            layout();
            if (wparam == SIZE_RESTORED && mReady) settings();
            return 0;
        case WM_EXITSIZEMOVE: settings(); return 0;
        case WM_GETMINMAXINFO:
        { auto* limits = reinterpret_cast<MINMAXINFO*>(lparam); limits->ptMinTrackSize = {820, 420}; return 0; }
        case WM_DPICHANGED:
        {
            const auto* rect = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(mWindow, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (mReady) applyFont(); return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == SendID && HIWORD(wparam) == BN_CLICKED) send();
            else if (LOWORD(wparam) == ConversationID && HIWORD(wparam) == CBN_SELCHANGE) selectConversation();
            else if (LOWORD(wparam) == OpenFriendID && HIWORD(wparam) == BN_CLICKED) openFriend();
            else if (LOWORD(wparam) == FriendListID && HIWORD(wparam) == LBN_DBLCLK) openFriend();
            else if (LOWORD(wparam) == FriendSearchID && HIWORD(wparam) == EN_CHANGE) updateFriends();
            else if (LOWORD(wparam) == InputID && HIWORD(wparam) == EN_CHANGE) saveDraft();
            else if (LOWORD(wparam) == TopID && HIWORD(wparam) == BN_CLICKED)
            {
                mTopmost = SendMessageW(mTop, BM_GETCHECK, 0, 0) == BST_CHECKED; InvalidateRect(mTop, nullptr, TRUE);
                SetWindowPos(mWindow, mTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); settings();
            }
            else if (LOWORD(wparam) == FontID && HIWORD(wparam) == CBN_SELCHANGE)
            {
                const auto index = SendMessageW(mFontSize, CB_GETCURSEL, 0, 0);
                const auto points = SendMessageW(mFontSize, CB_GETITEMDATA, index, 0);
                if (points >= 10 && points <= 28) { mPoints = static_cast<int>(points); applyFont(); settings(); }
            }
            return 0;
        case WM_NOTIFY:
        {
            auto* header = reinterpret_cast<NMHDR*>(lparam);
            if (header->hwndFrom == mTop && header->code == NM_CUSTOMDRAW)
            {
                const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(lparam);
                if (draw->dwDrawStage == CDDS_PREPAINT)
                {
                    DRAWITEMSTRUCT item{}; item.CtlType = ODT_BUTTON; item.CtlID = TopID; item.hwndItem = mTop;
                    item.hDC = draw->hdc; item.rcItem = draw->rc;
                    item.itemState = (draw->uItemState & CDIS_DISABLED ? ODS_DISABLED : 0) | (draw->uItemState & CDIS_FOCUS ? ODS_FOCUS : 0);
                    mTheme.drawItem(item, true, mTopmost); return CDRF_SKIPDEFAULT;
                }
            }
            if (header->hwndFrom == mHistory && header->code == EN_LINK)
            {
                auto* link = reinterpret_cast<ENLINK*>(lparam);
                const LONG count = link->chrg.cpMax - link->chrg.cpMin;
                if (link->msg == WM_LBUTTONUP && count > 0 && count <= 2048)
                {
                    std::wstring url(static_cast<size_t>(count) + 1, L'\0'); TEXTRANGEW range{link->chrg, url.data()};
                    const auto copied = SendMessageW(mHistory, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
                    if (copied > 0)
                    {
                        url.resize(static_cast<size_t>(copied)); std::lock_guard<std::mutex> lock(mBridge->mutex);
                        if (mBridge->commands.size() < 16) mBridge->commands.push_back({Command::Link, narrow(url), 0, mGeneration});
                    }
                }
            }
            return 0;
        }
        case WM_CLOSE: settings(); saveDraft(); DestroyWindow(mWindow); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0; // This thread owns only the native chat.
        case WM_NCDESTROY:
        {
            HWND hwnd = mWindow; mBridge->window = nullptr; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); mWindow = nullptr;
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        }
        }
        return DefWindowProcW(mWindow, msg, wparam, lparam);
    }
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        auto* self = reinterpret_cast<NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE)
        {
            self = static_cast<NativeWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            self->mWindow = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        try { return self ? self->handle(msg, wparam, lparam) : DefWindowProcW(hwnd, msg, wparam, lparam); }
        catch (...)
        {
            if (self)
            {
                if (msg == WM_CLOSE) DestroyWindow(hwnd);
                else PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
    }
    static LRESULT CALLBACK auxiliaryProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR data)
    {
        auto* self = reinterpret_cast<NativeWindow*>(data);
        try
        {
            if (hwnd == self->mFriendList && msg == WM_KEYDOWN && wparam == VK_RETURN) { self->openFriend(); return 0; }
            if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, auxiliaryProc, id);
            const auto result = DefSubclassProc(hwnd, msg, wparam, lparam);
            if (msg == WM_PAINT && hwnd != self->mFriendList) self->mTheme.paintComboArrow(hwnd);
            return result;
        }
        catch (...) { return DefSubclassProc(hwnd, msg, wparam, lparam); }
    }
    static LRESULT CALLBACK inputProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR data)
    {
        auto* self = reinterpret_cast<NativeWindow*>(data);
        try
        {
            if (msg == WM_IME_STARTCOMPOSITION) self->mComposing = true;
            if (msg == WM_IME_ENDCOMPOSITION) self->mComposing = false;
            if (msg == WM_KEYDOWN && wparam == VK_RETURN && !self->mComposing && !(GetKeyState(VK_SHIFT) & 0x8000))
            { self->send(); return 0; }
            if (msg == WM_CHAR && wparam == L'\r' && !(GetKeyState(VK_SHIFT) & 0x8000)) return 0;
            if (msg == WM_KEYDOWN && wparam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
            { SendMessageW(hwnd, EM_SETSEL, 0, -1); return 0; }
        }
        catch (...) { return 0; }
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, inputProc, id);
        return DefSubclassProc(hwnd, msg, wparam, lparam);
    }
    std::shared_ptr<Bridge> mBridge;
    Labels mLabels;
    HWND mWindow = nullptr, mHistory = nullptr, mInput = nullptr, mSend = nullptr, mTop = nullptr, mFontLabel = nullptr, mFontSize = nullptr, mMode = nullptr, mStatus = nullptr, mConversations = nullptr;
    HWND mFriendLabel = nullptr, mFriendSearch = nullptr, mFriendList = nullptr, mOpenFriend = nullptr;
    FSNativeChat::NativeTheme mTheme;
    std::vector<FSNativeChat::FriendEntry> mFriends, mFilteredFriends;
    std::size_t mFriendTotal = 0;
    bool mAccountReady = false;
    HFONT mFont = nullptr;
    HMODULE mRichEdit = nullptr;
    bool mOle = false, mRegistered = false, mReady = false, mTopmost = false, mConnected = false, mPending = false, mComposing = false, mSwitching = false;
    int mPoints = 14;
    U64 mGeneration = 0, mToken = 0;
    std::string mConversation;
    std::vector<Choice> mChoices;
    std::array<int, 4> mRect{};
    FSNativeChat::Transcript mTranscript;
    std::wstring mSubmitted;
};

// VIEWER MAIN THREAD ONLY.
class Manager
{
    using Book = FSNativeChat::Conversations;
public:
    void initialize()
    { if (!mInitialized) { mInitialized = true; gIdleCallbacks.addFunction(idle, this); } }
    void shutdown()
    {
        if (!mInitialized) return;
        gIdleCallbacks.deleteFunction(idle, this); mInitialized = false;
        if (mBridge) { mBridge->stop = true; mBridge->wake(); }
        if (mThread.joinable()) mThread.join();
        persist(); mBridge.reset(); mBook.clear(); mPublishedRows.clear(); mFriends.clear(); mPublishedFriends.clear();
    }
    ~Manager() { if (mThread.joinable()) { mBridge->stop = true; mBridge->wake(); mThread.join(); } }
    void show()
    {
        initialize(); tick(); refreshSessions(true);
        if (mBridge && !mBridge->finished) { mBridge->front = true; mBridge->wake(); return; }
        if (mThread.joinable()) mThread.join();
        persist();
        auto bridge = std::make_shared<Bridge>();
        const auto rect = gSavedSettings.getLLSD("FSNativeChatRect");
        bridge->rect = {llclamp(rect["x"].asInteger(), -50000, 50000), llclamp(rect["y"].asInteger(), -50000, 50000),
            llclamp(rect["width"].asInteger(), 820, 3000), llclamp(rect["height"].asInteger(), 420, 2400)};
        bridge->points = llclamp(gSavedSettings.getS32("FSNativeChatFontSize"), 10, 28);
        if (std::find(FontSizes.begin(), FontSizes.end(), bridge->points) == FontSizes.end()) bridge->points = 14;
        bridge->topmost = gSavedSettings.getBOOL("FSNativeChatTopmost");
        auto translated = [](const char* key) { return wide(LLTrans::getString(key)); };
        Labels labels{translated("NativeChatTitle"), translated("NativeChatSend"), translated("NativeChatTopmost"), translated("NativeChatFont"),
            translated("NativeChatSay"), translated("NativeChatWhisper"), translated("NativeChatShout"), translated("NativeChatReady"),
            translated("NativeChatOffline"), translated("NativeChatEmpty"), translated("NativeChatTooLong"), translated("NativeChatBusy"),
            translated("NativeChatFriends"), translated("NativeChatFriendSearch"), translated("NativeChatOpenFriend"),
            translated("NativeChatFriendOnline"), translated("NativeChatFriendOffline"), translated("NativeChatFriendOpenFailed")};
        mBridge = bridge; publishSelection(true); mPublishedRows.clear(); publishRows(); refreshFriends(true);
        try
        {
            mThread = std::thread([bridge, labels = std::move(labels)]() {
                try { NativeWindow window(bridge, labels); window.run(); }
                catch (...) { std::lock_guard<std::mutex> lock(bridge->mutex); bridge->error = "Native chat UI thread failed"; }
                bridge->window = nullptr; bridge->finished = true;
            });
        }
        catch (...) { mBridge.reset(); LLNotificationsUtil::add("NativeChatOpenFailed"); }
    }
    bool isOpen() const { return mBridge && mBridge->window.load() && !mBridge->finished; }
    void append(const LLChat& chat, bool archive, const LLSD& args)
    {
        if (!mInitialized || !archive || args["is_replay"].asBoolean() || chat.mMuted ||
            chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP ||
            chat.mText.empty() || gSavedSettings.getBOOL("FSHideLocalChat")) return;
        synchronizeSession(); if (!mConnected) return;
        auto name = chat.mFromName, text = chat.mText;
        filter(name, text, chat.mFromID, chat.mRlvNamesFiltered);
        appendLine(mBook.local(), format(name, text,
            gSavedSettings.getBOOL("FSShowTimestampsNearbyChat") ? chat.mTimeStr : ""));
    }
    void appendIM(const LLUUID& id, const std::string& from, const LLUUID& sender,
                  const std::string& body, const std::string& time)
    {
        if (!mInitialized || body.empty()) return;
        synchronizeSession(); if (!mConnected) return;
        auto* session = LLIMModel::instance().findIMSession(id);
        if (!session || (sender != gAgent.getID() && sender.notNull() &&
            LLMuteList::instance().isMuted(sender, LLMute::flagTextChat))) return;
        if (RlvActions::isRlvEnabled() && sender != gAgent.getID() &&
            !RlvActions::canReceiveIM(session->isGroupSessionType() ? id : sender)) return;
        auto* conversation = ensureIM(session); if (!conversation) return;
        auto name = from, text = body;
        filter(name, text, sender, false);
        appendLine(*conversation, format(name, text,
            gSavedSettings.getBOOL("FSShowTimestampsIM") ? time : ""));
    }
    void removeIM(const LLUUID& id)
    {
        if (!mInitialized) return;
        persist(); mBook.erase(id.asString()); publishSelection(); publishRows();
    }
    void rekeyIM(const LLUUID& oldID, const LLUUID& newID)
    {
        if (!mInitialized) return;
        persist(); mBook.rekey(oldID.asString(), newID.asString()); publishSelection(); publishRows();
    }
private:
    static void filter(std::string& name, std::string& text, const LLUUID& sender, bool hidden)
    {
        if (!RlvActions::isRlvEnabled()) return;
        if (hidden || !RlvActions::canShowName(RlvActions::SNC_DEFAULT, sender)) name = LLTrans::getString("NativeChatHiddenName");
        if (!RlvActions::canShowLocation()) RlvUtil::filterLocation(text);
        if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) RlvUtil::filterNames(text);
    }
    static std::string format(const std::string& name, const std::string& text, const std::string& time)
    {
        const bool emote = text.compare(0, 4, "/me ") == 0 || text.compare(0, 4, "/me'") == 0;
        return (time.empty() ? "" : time + " ") + (name.empty() ? "" : name + (emote ? "" : ": ")) + (emote ? text.substr(3) : text);
    }
    Book::Conversation* ensureIM(LLIMModel::LLIMSession* session)
    {
        if (!session || (!session->isP2PSessionType() && !session->isGroupSessionType())) return nullptr;
        if (session->isP2PSessionType() && session->mOtherParticipantID.isNull()) return nullptr;
        auto label = session->mName;
        std::string unused;
        filter(label, unused, session->isGroupSessionType() ? LLUUID::null : session->mOtherParticipantID, false);
        LLStringUtil::replaceChar(label, '\r', ' '); LLStringUtil::replaceChar(label, '\n', ' ');
        label = utf8str_truncate(label, 256);
        return mBook.upsert(session->mSessionID.asString(), label, session->isGroupSessionType() ? Book::Kind::Group : Book::Kind::IM);
    }
    void refreshSessions(bool force = false)
    {
        if (!mConnected) return;
        const auto now = std::chrono::steady_clock::now();
        if (!force && now < mNextRefresh) return;
        mNextRefresh = now + std::chrono::seconds(1);
        for (const auto& pair : LLIMModel::instance().mId2SessionMap) ensureIM(pair.second);
        // Fallback for sessions deleted through a path other than clearSession.
        std::vector<std::string> removed;
        for (const auto& pair : mBook.items())
            if (!pair.first.empty() && !LLIMModel::instance().findIMSession(LLUUID(pair.first))) removed.push_back(pair.first);
        for (const auto& key : removed) mBook.erase(key);
        publishSelection(); publishRows();
    }
    void refreshFriends(bool force = false)
    {
        if (!mConnected || !mBridge || mBridge->finished) return;
        const auto now = std::chrono::steady_clock::now();
        if (!force && now < mNextFriendRefresh) return;
        mNextFriendRefresh = now + std::chrono::seconds(1);
        LLAvatarTracker::buddy_map_t buddies; LLAvatarTracker::instance().copyBuddyList(buddies);
        std::vector<FSNativeChat::FriendEntry> entries;
        for (const auto& pair : buddies)
        {
            if (!pair.second || pair.first == gAgent.getID()) continue;
            LLAvatarName avatar; const bool named = LLAvatarNameCache::get(pair.first, &avatar);
            auto name = named ? avatar.getCompleteName() : LLTrans::getString("NativeChatFriendUnnamed");
            std::string unused; filter(name, unused, pair.first, false);
            LLStringUtil::replaceChar(name, '\r', ' '); LLStringUtil::replaceChar(name, '\n', ' ');
            entries.push_back({pair.first.asString(), utf8str_truncate(name, 256), pair.second->isOnline(), 0});
        }
        std::sort(entries.begin(), entries.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.online != rhs.online) return lhs.online;
            const auto compare = LLStringUtil::compareInsensitive(lhs.name, rhs.name);
            return compare != 0 ? compare < 0 : lhs.id < rhs.id;
        });
        const auto total = entries.size(); mFriends.update(std::move(entries));
        const auto& current = mFriends.entries();
        if (current == mPublishedFriends && total == mPublishedFriendTotal) return;
        mPublishedFriends = current; mPublishedFriendTotal = total;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            mBridge->friends = current; mBridge->friendTotal = total;
        }
        mBridge->wake();
    }
    void openFriend(const Command& command)
    {
        persist();
        const LLUUID id(command.conversation);
        bool opened = false;
        if (mConnected && command.generation == mGeneration && mFriends.matches(command.conversation, command.token) &&
            id.notNull() && id != gAgent.getID() && LLAvatarTracker::instance().getBuddyInfo(id) && RlvActions::canStartIM(id))
        {
            const auto expected = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, id);
            if (mBook.find(expected.asString()) || mBook.items().size() < Book::MaxConversations)
            {
                LLAvatarName avatar;
                const auto name = LLAvatarNameCache::get(id, &avatar) ? avatar.getDisplayName() : LLTrans::getString("NativeChatFriendUnnamed");
                // Use the regular session lifecycle, without an asynchronous
                // name callback, sending text or stealing the native focus.
                const auto sessionID = LLIMMgr::instance().addSession(name, IM_NOTHING_SPECIAL, id);
                synchronizeSession();
                if (command.generation == mGeneration && mConnected && sessionID.notNull() && LLAvatarTracker::instance().getBuddyInfo(id))
                {
                    if (auto* item = ensureIM(LLIMModel::instance().findIMSession(sessionID)))
                        opened = mBook.select(item->key, item->token);
                }
            }
        }
        if (command.generation != mGeneration) return;
        publishSelection(true); publishRows();
        if (opened) { mBridge->focusInput = true; mBridge->wake(); }
        if (!opened)
        {
            { std::lock_guard<std::mutex> lock(mBridge->mutex); mBridge->reply = Reply::OpenFailed; }
            mBridge->wake();
        }
    }
    bool canSendSelected()
    {
        if (!mConnected) return false;
        const auto& selected = mBook.selected();
        if (selected.kind == Book::Kind::Local) return true;
        const auto* session = LLIMModel::instance().findIMSession(LLUUID(selected.key));
        return session && session->mTextIMPossible && session->mSessionInitialized;
    }
    void publishSelection(bool force = false)
    {
        if (!mBridge) return;
        const bool ready = canSendSelected();
        const auto& selected = mBook.selected();
        bool changed;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            changed = force || mBridge->generation != mGeneration || mBridge->conversation != selected.key || mBridge->token != selected.token;
            const bool stateChanged = mBridge->connected != ready || mBridge->accountReady != mConnected;
            mBridge->accountReady = mConnected;
            mBridge->connected = ready;
            if (changed)
            {
                mBridge->generation = mGeneration; mBridge->conversation = selected.key; mBridge->token = selected.token;
                mBridge->local = selected.kind == Book::Kind::Local; mBridge->draft = selected.draft;
                mBridge->lines = selected.history.lines(); mBridge->reset = true; mBridge->reply.reset();
            }
            changed = changed || stateChanged;
        }
        if (changed) mBridge->wake();
    }
    void publishRows()
    {
        if (!mBridge) return;
        std::vector<Choice> rows;
        for (const auto& pair : mBook.items())
        {
            const auto& item = pair.second;
            auto label = item.kind == Book::Kind::Local ? LLTrans::getString("NativeChatLocal") :
                LLTrans::getString(item.kind == Book::Kind::Group ? "NativeChatGroup" : "NativeChatIM") + " — " + item.label;
            if (item.unread) label += " (" + std::to_string(item.unread) + ")";
            rows.push_back({item.key, label, item.token});
        }
        if (rows == mPublishedRows) return;
        mPublishedRows = rows;
        { std::lock_guard<std::mutex> lock(mBridge->mutex); mBridge->choices = std::move(rows); }
        mBridge->wake();
    }
    void appendLine(Book::Conversation& item, const std::string& line)
    {
        item.history.append(line);
        if (item.key != mBook.selected().key) { item.unread = (std::min)(item.unread + 1, 999u); publishRows(); }
        if (mBridge && !mBridge->finished)
        {
            {
                std::lock_guard<std::mutex> lock(mBridge->mutex);
                if (mBridge->generation != mGeneration || mBridge->conversation != item.key || mBridge->token != item.token) return;
                if (mBridge->lines.size() >= FSNativeChat::Transcript::MaxEntries) mBridge->lines.pop_front();
                mBridge->lines.push_back(item.history.lines().back());
            }
            mBridge->wake();
        }
    }
    void synchronizeSession()
    {
        const auto agent = gAgent.getID(), session = gAgent.getSessionID();
        const bool sessionLive = LLStartUp::getStartupState() == STATE_STARTED && !gDisconnected && !LLApp::isExiting();
        const bool connected = sessionLive && gAgent.getRegion() && agent.notNull() && session.notNull();
        const bool changed = agent != mAgent || session != mSession || (mSessionLive && !sessionLive);
        mSessionLive = sessionLive;
        if (changed)
        {
            ++mGeneration; mAgent = agent; mSession = session; mBook.clear(); mPublishedRows.clear();
            mNextRefresh = {}; mNextFriendRefresh = {};
            mFriends.clear(); mPublishedFriends.clear(); mPublishedFriendTotal = 0;
            if (mBridge)
            {
                std::lock_guard<std::mutex> lock(mBridge->mutex); mBridge->commands.clear(); mBridge->reply.reset(); mBridge->friends = std::vector<FSNativeChat::FriendEntry>{}; mBridge->friendTotal = 0;
            }
        }
        mConnected = connected;
        publishSelection(changed);
        if (changed) publishRows();
    }
    void persist()
    {
        if (!mBridge) return;
        bool dirty, topmost; int points; std::array<int, 4> rect;
        {
            std::lock_guard<std::mutex> lock(mBridge->mutex);
            dirty = mBridge->settingsDirty; mBridge->settingsDirty = false;
            rect = mBridge->rect; points = mBridge->points; topmost = mBridge->topmost;
            if (mBridge->generation == mGeneration && mBook.matches(mBridge->conversation, mBridge->token))
                mBook.find(mBridge->conversation)->draft = mBridge->draft;
        }
        if (!dirty) return;
        gSavedSettings.setLLSD("FSNativeChatRect", LLSDMap("x", rect[0])("y", rect[1])("width", rect[2])("height", rect[3]));
        gSavedSettings.setS32("FSNativeChatFontSize", points); gSavedSettings.setBOOL("FSNativeChatTopmost", topmost);
    }
    void tick()
    {
        synchronizeSession(); persist(); refreshSessions(); refreshFriends(); if (!mBridge) return;
        std::deque<Command> commands; std::string error;
        { std::lock_guard<std::mutex> lock(mBridge->mutex); commands.swap(mBridge->commands); error.swap(mBridge->error); }
        if (!error.empty()) { LL_WARNS("NativeChat") << error << LL_ENDL; LLNotificationsUtil::add("NativeChatOpenFailed"); }
        for (const auto& command : commands)
        {
            synchronizeSession();
            if (command.generation != mGeneration || mBridge->stop) continue;
            if (command.kind == Command::Link)
            {
                auto scheme = command.text.substr(0, command.text.find(':')); LLStringUtil::toLower(scheme);
                if (scheme == "http" || scheme == "https") LLWeb::loadURL(command.text);
                continue;
            }
            if (command.kind == Command::OpenFriend) { openFriend(command); continue; }
            if (command.kind == Command::Select)
            {
                persist(); mBook.select(command.conversation, command.token);
                publishSelection(true); publishRows(); continue;
            }
            Reply reply = Reply::Ready;
            auto text = utf8str_to_wstring(command.text); LLWStringUtil::trim(text);
            if (!mConnected || !mBook.matches(command.conversation, command.token)) reply = Reply::Offline;
            else if (text.empty()) reply = Reply::Empty;
            else if (wstring_to_utf8str(text).size() >= MAX_STRING) reply = Reply::TooLong;
            else if (command.conversation.empty())
                FSNearbyChat::instance().sendChat(text, command.mode == 1 ? CHAT_TYPE_WHISPER : command.mode == 2 ? CHAT_TYPE_SHOUT : CHAT_TYPE_NORMAL);
            else
            {
                const LLUUID id(command.conversation);
                const auto* session = LLIMModel::instance().findIMSession(id);
                auto* floater = session && session->mSessionInitialized && session->mTextIMPossible ? FSFloaterIM::getInstance(id) : nullptr;
                if (!floater || !floater->sendNativeMessage(wstring_to_utf8str(text))) reply = Reply::Offline;
            }
            synchronizeSession();
            if (command.generation == mGeneration)
            {
                {
                    std::lock_guard<std::mutex> lock(mBridge->mutex);
                    if (mBridge->conversation != command.conversation || mBridge->token != command.token) continue;
                    mBridge->reply = reply;
                }
                mBridge->wake();
            }
        }
        if (mBridge->finished && mThread.joinable()) { mThread.join(); persist(); }
    }
    static void idle(void* data) { static_cast<Manager*>(data)->tick(); }
    bool mInitialized = false, mConnected = false, mSessionLive = false;
    U64 mGeneration = 0;
    LLUUID mAgent, mSession;
    std::shared_ptr<Bridge> mBridge;
    std::thread mThread;
    Book mBook;
    std::vector<Choice> mPublishedRows;
    FSNativeChat::Friends mFriends;
    std::vector<FSNativeChat::FriendEntry> mPublishedFriends;
    std::size_t mPublishedFriendTotal = 0;
    std::chrono::steady_clock::time_point mNextRefresh{}, mNextFriendRefresh{};
};
Manager& manager() { static Manager instance; return instance; }
}
namespace FSNativeChat
{
void initialize() { llassert(on_main_thread()); manager().initialize(); }
void shutdown() { llassert(on_main_thread()); manager().shutdown(); }
void show() { llassert(on_main_thread()); manager().show(); }
bool isOpen() { llassert(on_main_thread()); return manager().isOpen(); }
void append(const LLChat& chat, bool archive, const LLSD& args) { llassert(on_main_thread()); manager().append(chat, archive, args); }
void appendIM(const LLUUID& session, const std::string& name, const LLUUID& sender, const std::string& text, const std::string& time)
{ llassert(on_main_thread()); manager().appendIM(session, name, sender, text, time); }
void removeIM(const LLUUID& session) { llassert(on_main_thread()); manager().removeIM(session); }
void rekeyIM(const LLUUID& oldSession, const LLUUID& newSession) { llassert(on_main_thread()); manager().rekeyIM(oldSession, newSession); }
}
