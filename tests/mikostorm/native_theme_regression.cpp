// Hidden-window exercise of the production Windows renderer; no OS settings change.
#include "fsnativechattheme.h"
#include <iostream>
#include <stdexcept>
int main()
{
    try
    {
        FSNativeChat::NativeTheme theme;
        HWND window = CreateWindowExW(0, L"STATIC", L"Native theme regression", WS_OVERLAPPEDWINDOW,
            0, 0, 200, 120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!window) throw std::runtime_error("Hidden test window could not be created");
        HWND button = CreateWindowExW(0, L"BUTTON", L"Test", WS_CHILD | BS_OWNERDRAW, 0,0,80,24,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        HWND combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,0,30,100,60,window,nullptr,GetModuleHandleW(nullptr),nullptr);
        if (!button || !combo) { DestroyWindow(window); throw std::runtime_error("Hidden controls could not be created"); }
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
        };
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
