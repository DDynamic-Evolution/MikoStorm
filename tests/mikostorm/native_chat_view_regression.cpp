// Real Windows controls; no viewer, account, network or MCP service is started.
#include "fsnativechatview.h"
#include <iostream>
#include <stdexcept>
namespace
{
int checks = 0;
void check(bool condition, const char* message)
{ ++checks; if (!condition) throw std::runtime_error(message); }
CHARRANGE selection(HWND history)
{ CHARRANGE value{}; SendMessageW(history, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&value)); return value; }
LONG anchor(HWND history)
{
    const auto line = SendMessageW(history, EM_GETFIRSTVISIBLELINE, 0, 0);
    return static_cast<LONG>(SendMessageW(history, EM_LINEINDEX, static_cast<WPARAM>(line), 0));
}
std::wstring transcript(int first, int last, int width = 80)
{
    std::wstring result;
    for (int i = first; i < last; ++i)
    {
        auto row = L"Message " + std::to_wstring(i) + L" \u00e4 \u03a9 \U0001f642 ";
        row.resize(static_cast<std::size_t>(width), L'x'); result += row + L"\r\n";
    }
    return result;
}
LRESULT CALLBACK tabMonitor(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR data)
{
    if (message == TCM_DELETEALLITEMS || message == TCM_SETCURSEL) ++*reinterpret_cast<int*>(data);
    return DefSubclassProc(window, message, wparam, lparam);
}
}
int main()
{
    HWND window = nullptr; HMODULE rich = nullptr;
    try
    {
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_TAB_CLASSES};
        check(InitCommonControlsEx(&common) != FALSE, "Initialize tab control");
        rich = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(rich != nullptr, "Load Windows Rich Edit");
        window = CreateWindowExW(0, L"STATIC", L"Native chat tests", WS_OVERLAPPEDWINDOW,
            0, 0, 1000, 300, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        check(window != nullptr, "Create hidden test window");
        HWND tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
            0, 0, 300, 30, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND history = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            0, 40, 800, 140, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        check(tabs && history, "Create hidden tabs and history");
        SendMessageW(history, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        SendMessageW(history, EM_EXLIMITTEXT, 0, 256 * 1024);
        int resets = 0;
        check(SetWindowSubclass(tabs, tabMonitor, 1, reinterpret_cast<DWORD_PTR>(&resets)) != FALSE, "Monitor tab updates");
        FSNativeChat::TabStrip strip;
        std::vector<FSNativeChat::TabItem> items{{"", L"Local", 1}, {"im", L"IM \u00e4", 2}, {"group", L"Group", 3}};
        check(strip.update(tabs, items, "im", 2), "Create conversations as tabs");
        check(strip.selected(tabs) && strip.selected(tabs)->key == "im", "Select correct IM identity");
        resets = 0;
        std::reverse(items.begin(), items.end()); items[0].label += L" (3)";
        check(strip.update(tabs, items, "im", 2), "Update unread labels");
        check(SendMessageW(tabs, TCM_GETCURSEL, 0, 0) == 1 && resets == 0, "Stable order and no rebuild/reselection on unread update");
        items.erase(items.begin() + 1); items.push_back({"im", L"Reopened IM", 4});
        check(strip.update(tabs, items, "im", 4), "Replace closed IM lifetime");
        check(!strip.contains("im", 2) && strip.contains("im", 4), "Closed lifetime is not reused");
        check(strip.selected(tabs) && strip.selected(tabs)->token == 4, "New lifetime selects correct tab");
        items.clear();
        for (int i = 0; i < 64; ++i) items.push_back({std::to_string(i), L"Conversation " + std::to_wstring(i), static_cast<std::uint64_t>(i + 10)});
        check(strip.update(tabs, items, "63", 73), "Populate overflow tabs");
        check(SendMessageW(tabs, TCM_GETITEMCOUNT, 0, 0) == 64 && strip.selected(tabs)->key == "63", "All 64 conversations remain selectable");
        strip.clear(tabs);
        check(!strip.selected(tabs) && !strip.contains("63", 73), "Clear account tab identities");

        using View = FSNativeChat::HistoryView;
        check(View::normalize(L"a\r\nb\nc\rd") == L"a\rb\rc\rd", "Canonical Rich Edit line endings");
        View local;
        local.replace(history, transcript(0, 150), 0, 150, 0, true);
        check(View::atBottom(history) && local.enabled(), "Initial conversation follows latest message");
        local.replace(history, transcript(0, 151), 0, 151, 0, false);
        check(View::atBottom(history) && local.newMessages() == 0, "Live messages autoscroll");
        SendMessageW(history, WM_VSCROLL, SB_TOP, 0);
        SendMessageW(history, EM_SETSEL, 20, 40);
        local.observeUser(history);
        const auto readingAnchor = anchor(history);
        local.replace(history, transcript(0, 152), 0, 152, 0, false);
        check(selection(history).cpMin == 20 && selection(history).cpMax == 40, "Incoming messages preserve selected text");
        check(anchor(history) == readingAnchor && !View::atBottom(history) && local.newMessages() == 1, "Reading older messages pauses following");
        local.remember(history);
        View group;
        group.replace(history, transcript(1000, 1160), 0, 160, 0, true);
        check(View::atBottom(history), "New tab has its own default following state");
        local.replace(history, transcript(0, 154), 0, 154, 0, true);
        check(anchor(history) == readingAnchor && selection(history).cpMin == 20 && local.newMessages() == 3, "Returning to tab restores reading state and counts inactive arrivals");
        local.jumpToLatest(history);
        check(View::atBottom(history) && local.enabled() && local.newMessages() == 0, "Latest button resumes following and clears count");
        local.enable(history, false);
        local.replace(history, transcript(0, 155), 0, 155, 0, false);
        check(!local.enabled() && local.newMessages() == 1, "Explicitly disabled autoscroll remains paused");
        local.enable(history, true);
        check(View::atBottom(history) && local.newMessages() == 0, "Autoscroll checkbox resumes at bottom");
        // Returning manually to the bottom also resumes when the preference is enabled.
        SendMessageW(history, WM_VSCROLL, SB_TOP, 0); local.observeUser(history);
        local.replace(history, transcript(0, 156), 0, 156, 0, false);
        check(local.newMessages() == 1, "Scroll up alone pauses autoscroll");
        SendMessageW(history, WM_VSCROLL, SB_BOTTOM, 0); local.observeUser(history);
        local.replace(history, transcript(0, 157), 0, 157, 0, false);
        check(View::atBottom(history) && local.newMessages() == 0, "Returning manually to bottom resumes following");

        // Prepared source only: exercise an inactive control without stealing focus.
        const HWND focusBefore = GetFocus(), foregroundBefore = GetForegroundWindow();
        View background;
        background.replace(history, transcript(0, 150), 0, 150, 0, true);
        check(GetFocus() != history, "Background case has no history focus");
        // A programmatic/layout viewport change must not become a user pause.
        SendMessageW(history, WM_VSCROLL, SB_TOP, 0); background.remember(history);
        background.replace(history, transcript(0, 151), 0, 151, 0, false);
        check(View::atBottom(history) && background.newMessages() == 0, "Background following survives internal viewport changes");
        check(GetFocus() == focusBefore && GetForegroundWindow() == foregroundBefore, "Background arrivals do not change focus or activation");
        SendMessageW(history, WM_VSCROLL, SB_TOP, 0); background.observeUser(history);
        background.replace(history, transcript(0, 152), 0, 152, 0, false);
        check(!View::atBottom(history) && background.newMessages() == 1, "Explicit reading pause survives background arrivals");
        background.jumpToLatest(history);
        background.replace(history, transcript(0, 153), 0, 153, 0, false);
        check(View::atBottom(history) && background.newMessages() == 0, "Latest resumes background following");
        background.enable(history, false);
        background.replace(history, transcript(0, 154), 0, 154, 0, false);
        check(!background.enabled() && background.newMessages() == 1, "Disabled background following stays disabled");

        View bounded;
        bounded.replace(history, transcript(0, 200), 0, 200, 0, true);
        const LONG rowSize = 81; // 80 UTF-16 code units plus canonical CR.
        SendMessageW(history, EM_SETSEL, 60 * rowSize, 60 * rowSize + 12);
        SendMessageW(history, EM_SCROLLCARET, 0, 0);
        bounded.observeUser(history);
        const LONG beforeTrim = anchor(history);
        bounded.replace(history, transcript(1, 201), rowSize, 201, 1, false);
        check(selection(history).cpMin == 59 * rowSize && selection(history).cpMax == 59 * rowSize + 12, "Trim shifts selection to same retained message");
        check(anchor(history) == beforeTrim - rowSize, "Trim preserves retained reading anchor");
        bounded.remember(history);
        group.replace(history, transcript(1000, 1160), 0, 160, 0, true);
        bounded.replace(history, transcript(20, 220), 0, 220, 20, true);
        check(selection(history).cpMin == 0 && anchor(history) == 0, "Expired inactive snapshot never restores unrelated selection");
        bounded.replace(history, transcript(300, 500), 0, 500, 300, false, true);
        check(selection(history).cpMin == 0 && anchor(history) == 0, "Queue gap safely resets stale reading anchors");
        bounded.replace(history, transcript(300, 500), 0, 10000, 300, false);
        check(bounded.newMessages() == 999, "Paused arrival counter is bounded");

        View large;
        large.replace(history, transcript(0, 180, 700), 0, 180, 0, true);
        SendMessageW(history, EM_SETSEL, 110000, 110012); SendMessageW(history, EM_SCROLLCARET, 0, 0);
        large.observeUser(history);
        const auto largeAnchor = anchor(history);
        check(largeAnchor > 65535, "Exercise character anchor beyond 16-bit range");
        large.replace(history, transcript(0, 181, 700), 0, 181, 0, false);
        check(anchor(history) == largeAnchor && selection(history).cpMin == 110000, "Large transcript position survives append without 16-bit truncation");
        DestroyWindow(window); window = nullptr; FreeLibrary(rich); rich = nullptr;
        std::cout << "Native chat tabs/autoscroll: " << checks << " checks passed\n";
    }
    catch (const std::exception& error)
    {
        if (window) DestroyWindow(window); if (rich) FreeLibrary(rich);
        std::cerr << error.what() << '\n'; return 1;
    }
}
