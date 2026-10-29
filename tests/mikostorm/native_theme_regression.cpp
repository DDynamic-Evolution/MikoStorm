// Hidden-window exercise of the production Windows renderer; no OS settings change.
#include "fsnativechattheme.h"
#include <iostream>
#include <stdexcept>
// Prepared for the next authorized native build; do not change Windows settings.
static void checkFontColors(HWND parent, FSNativeChat::NativeTheme& theme)
{
    HMODULE rich = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!rich) throw std::runtime_error("Rich Edit unavailable for font/color checks");
    HWND history = CreateWindowExW(0, L"RICHEDIT50W", L"", WS_CHILD | ES_MULTILINE | ES_READONLY,
        0, 0, 160, 80, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!history) { FreeLibrary(rich); throw std::runtime_error("Create Rich Edit for font/color checks"); }
    SendMessageW(history, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
    struct Colors { COLORREF text, background; };
    const Colors palettes[]{{RGB(240,240,240),RGB(43,43,43)}, {RGB(0,0,0),RGB(255,255,255)},
        {RGB(255,255,0),RGB(0,0,0)}, {GetSysColor(COLOR_WINDOWTEXT),GetSysColor(COLOR_WINDOW)}};
    HFONT previousFont = nullptr;
    for (const auto& colors : palettes)
    {
        theme.foreground = colors.text; theme.surface = colors.background;
        auto checkColors = [&]() {
            for (WPARAM scope : {WPARAM{SCF_DEFAULT}, WPARAM{SCF_SELECTION}})
            {
                CHARFORMAT2W format{}; format.cbSize = sizeof(format);
                SendMessageW(history, EM_GETCHARFORMAT, scope, reinterpret_cast<LPARAM>(&format));
                if ((format.dwMask & (CFM_COLOR | CFM_BACKCOLOR)) != (CFM_COLOR | CFM_BACKCOLOR) ||
                    (format.dwEffects & (CFE_AUTOCOLOR | CFE_AUTOBACKCOLOR)) ||
                    format.crTextColor != colors.text || format.crBackColor != colors.background)
                    throw std::runtime_error("Font size changed Rich Edit text/default theme colors");
            }
        };
        LONG previousHeight = 0;
        for (int points : {10,12,14,16,18,20,24,28})
        {
            SetWindowTextW(history, L"Transcript \u00e4 \u03a9");
            SendMessageW(history, EM_SETSEL, 0, -1);
            HFONT font = CreateFontW(-MulDiv(points,96,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            if (!font) throw std::runtime_error("Create font for color regression");
            theme.richEditFont(history, font);
            if (previousFont) DeleteObject(previousFont);
            previousFont = font; checkColors();
            CHARFORMAT2W size{}; size.cbSize = sizeof(size);
            SendMessageW(history, EM_GETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&size));
            if (size.yHeight <= previousHeight) throw std::runtime_error("Font size did not increase");
            previousHeight = size.yHeight;
            SetWindowTextW(history, L"Next incoming message"); checkColors();
        }
    }
    DestroyWindow(history); if (previousFont) DeleteObject(previousFont); FreeLibrary(rich);
}
int main()
{
    try
    {
        INITCOMMONCONTROLSEX common{sizeof(common), ICC_TAB_CLASSES};
        if (!InitCommonControlsEx(&common)) throw std::runtime_error("Initialize tab control");
        FSNativeChat::NativeTheme theme;
        HWND window = CreateWindowExW(0, L"STATIC", L"Native theme regression", WS_OVERLAPPEDWINDOW,
            0, 0, 200, 120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window) throw std::runtime_error("Hidden test window could not be created");
        HWND button = CreateWindowExW(0, L"BUTTON", L"Test", WS_CHILD | BS_OWNERDRAW, 0,0,80,24,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        HWND combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,0,30,100,60,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        HWND tabs = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | TCS_OWNERDRAWFIXED, 0, 60, 150, 30, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        TCITEMW tab{}; tab.mask = TCIF_TEXT; tab.pszText = const_cast<wchar_t*>(L"IM \u00e4 (3)");
        SendMessageW(tabs, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tab));
        if (!button || !combo || !tabs) { DestroyWindow(window); throw std::runtime_error("Hidden controls could not be created"); }
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Unicode: \u00e4 \u03a9"));
        HDC dc = GetDC(button);
        auto paint = [&]() {
            theme.refresh(window);
            DRAWITEMSTRUCT item{}; item.CtlType=ODT_BUTTON; item.hwndItem=button; item.hDC=dc; item.rcItem={0,0,80,24};
            if (!theme.drawItem(item, false, false)) throw std::runtime_error("Button draw failed");
            item.itemState = ODS_SELECTED | ODS_FOCUS;
            theme.drawItem(item, true, true);
            item.CtlType=ODT_COMBOBOX; item.hwndItem=combo; item.itemID=0; theme.drawItem(item, false, false);
            theme.paintComboArrow(combo);
            item.CtlType=ODT_TAB; item.hwndItem=tabs;
            if (!theme.drawItem(item, false, false)) throw std::runtime_error("Tab draw failed");
        };
        checkFontColors(window, theme);
        paint(); paint();
        const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (int i=0;i<100;++i) paint();
        const auto after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        ReleaseDC(button,dc); DestroyWindow(window);
        if (after > before + 2) throw std::runtime_error("GDI objects leaked across theme refreshes");
        std::cout << "Windows theme: hidden controls and 100 resource-safe refreshes passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
