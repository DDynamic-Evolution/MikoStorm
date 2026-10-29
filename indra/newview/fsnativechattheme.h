// Windows UI-thread-only rendering. Read the app theme; never change OS settings.
#ifndef FS_NATIVE_CHAT_THEME_H
#define FS_NATIVE_CHAT_THEME_H
#include "fsnativechatmodel.h"
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <dwmapi.h>
#include <string>

namespace FSNativeChat
{
class NativeTheme
{
public:
    NativeTheme() { refresh(nullptr); }
    NativeTheme(const NativeTheme&) = delete;
    NativeTheme& operator=(const NativeTheme&) = delete;
    ~NativeTheme() { release(); }
    void refresh(HWND window)
    {
        HIGHCONTRASTW contrast{}; contrast.cbSize = sizeof(contrast);
        const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) && (contrast.dwFlags & HCF_HIGHCONTRASTON);
        DWORD light = 1, type = 0, size = sizeof(light); HKEY key = nullptr;
        bool known = false;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            known = RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, reinterpret_cast<BYTE*>(&light), &size) == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(light);
            RegCloseKey(key);
        }
        mode = chooseTheme(highContrast, known, light != 0);
        const bool dark = mode == ThemeMode::Dark;
        background = dark ? RGB(32,32,32) : GetSysColor(COLOR_BTNFACE);
        surface = dark ? RGB(43,43,43) : GetSysColor(COLOR_WINDOW);
        foreground = dark ? RGB(240,240,240) : GetSysColor(COLOR_WINDOWTEXT);
        buttonText = dark ? foreground : GetSysColor(COLOR_BTNTEXT);
        border = dark ? RGB(110,110,110) : GetSysColor(COLOR_WINDOWFRAME);
        disabled = dark ? RGB(165,165,165) : GetSysColor(COLOR_GRAYTEXT);
        highlight = GetSysColor(COLOR_HIGHLIGHT); highlightText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        // Allocate replacements before releasing brushes that controls may use.
        HBRUSH bg = CreateSolidBrush(background), field = CreateSolidBrush(surface), edge = CreateSolidBrush(border);
        if (bg && field && edge) { release(); backgroundBrush = bg; surfaceBrush = field; borderBrush = edge; }
        else { if (bg) DeleteObject(bg); if (field) DeleteObject(field); if (edge) DeleteObject(edge); }
        if (window)
        {
            const BOOL useDark = dark;
            // Unsupported systems retain their ordinary native title bar.
            DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
        }
    }
    void richEdit(HWND control) const
    {
        SendMessageW(control, EM_SETBKGNDCOLOR, 0, surface);
        CHARFORMAT2W format{}; format.cbSize = sizeof(format); format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
        format.crTextColor = foreground; format.crBackColor = surface;
        SendMessageW(control, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
        SendMessageW(control, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    }
    void richEditFont(HWND control, HFONT font) const
    {
        // WM_SETFONT can reset Rich Edit character colors to automatic/system
        // values. Restore the current palette before repainting, for both the
        // existing transcript and the default format of subsequent messages.
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        richEdit(control);
        RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    }
    HBRUSH colorControl(HDC dc, bool field, bool enabled) const
    {
        SetTextColor(dc, enabled ? (field ? foreground : buttonText) : disabled); SetBkColor(dc, field ? surface : background);
        return field ? surfaceBrush : backgroundBrush;
    }
    bool drawItem(const DRAWITEMSTRUCT& item, bool checkbox, bool checked) const
    {
        if (item.CtlType != ODT_BUTTON && item.CtlType != ODT_COMBOBOX && item.CtlType != ODT_LISTBOX && item.CtlType != ODT_TAB) return false;
        const int saved = SaveDC(item.hDC);
        const bool selected = (item.itemState & ODS_SELECTED) != 0;
        const bool enabled = (item.itemState & ODS_DISABLED) == 0;
        const bool button = item.CtlType == ODT_BUTTON;
        const COLORREF bg = selected ? highlight : button ? background : surface;
        HBRUSH brush = CreateSolidBrush(bg);
        FillRect(item.hDC, &item.rcItem, brush ? brush : surfaceBrush); if (brush) DeleteObject(brush);
        SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, enabled ? (selected ? highlightText : button ? buttonText : foreground) : disabled);
        const auto font = reinterpret_cast<HFONT>(SendMessageW(item.hwndItem, WM_GETFONT, 0, 0));
        if (font) SelectObject(item.hDC, font);
        RECT text = item.rcItem; InflateRect(&text, -6, -2);
        std::wstring label;
        if (button)
        {
            const int count = GetWindowTextLengthW(item.hwndItem);
            label.resize(static_cast<std::size_t>(count) + 1);
            label.resize(GetWindowTextW(item.hwndItem, label.data(), count + 1));
            if (checkbox)
            {
                RECT box{text.left, text.top + 3, text.left + 14, text.top + 17};
                FrameRect(item.hDC, &box, borderBrush);
                if (checked)
                {
                    const auto pen = CreatePen(PS_SOLID, 2, selected ? highlightText : buttonText);
                    if (pen)
                    {
                        const auto previous = SelectObject(item.hDC, pen);
                        MoveToEx(item.hDC, box.left + 3, box.top + 7, nullptr);
                        LineTo(item.hDC, box.left + 6, box.top + 10); LineTo(item.hDC, box.right - 2, box.top + 3);
                        SelectObject(item.hDC, previous); DeleteObject(pen);
                    }
                }
                text.left += 22;
            }
            else FrameRect(item.hDC, &item.rcItem, borderBrush);
        }
        else if (item.CtlType == ODT_TAB)
        {
            label.resize(2048);
            TCITEMW tab{}; tab.mask = TCIF_TEXT; tab.pszText = label.data(); tab.cchTextMax = static_cast<int>(label.size());
            if (SendMessageW(item.hwndItem, TCM_GETITEMW, item.itemID, reinterpret_cast<LPARAM>(&tab))) label.resize(wcslen(label.c_str()));
            else label.clear();
            FrameRect(item.hDC, &item.rcItem, borderBrush);
        }
        else if (item.itemID != static_cast<UINT>(-1))
        {
            const bool list = item.CtlType == ODT_LISTBOX;
            const auto count = SendMessageW(item.hwndItem, list ? LB_GETTEXTLEN : CB_GETLBTEXTLEN, item.itemID, 0);
            if (count >= 0 && count < 2048)
            {
                label.resize(static_cast<std::size_t>(count) + 1);
                const auto copied = SendMessageW(item.hwndItem, list ? LB_GETTEXT : CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(label.data()));
                label.resize(copied >= 0 ? static_cast<std::size_t>(copied) : 0);
            }
        }
        DrawTextW(item.hDC, label.c_str(), static_cast<int>(label.size()), &text,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (button && !checkbox ? DT_CENTER : DT_LEFT));
        if (item.itemState & ODS_FOCUS) { RECT focus = item.rcItem; InflateRect(&focus, -2, -2); DrawFocusRect(item.hDC, &focus); }
        if (saved) RestoreDC(item.hDC, saved);
        return true;
    }
    void paintComboArrow(HWND control) const
    {
        if (mode != ThemeMode::Dark) return;
        COMBOBOXINFO info{}; info.cbSize = sizeof(info);
        if (!GetComboBoxInfo(control, &info)) return;
        const HDC dc = GetDC(control); if (!dc) return;
        FillRect(dc, &info.rcButton, backgroundBrush);
        const LONG x = (info.rcButton.left + info.rcButton.right) / 2, y = (info.rcButton.top + info.rcButton.bottom) / 2;
        const POINT triangle[]{{x-4,y-2},{x+4,y-2},{x,y+3}};
        const auto oldBrush = SelectObject(dc, borderBrush), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, triangle, 3); SelectObject(dc, oldPen); SelectObject(dc, oldBrush); ReleaseDC(control, dc);
    }
    ThemeMode mode = ThemeMode::System;
    COLORREF background{}, surface{}, foreground{}, buttonText{}, border{}, disabled{}, highlight{}, highlightText{};
    HBRUSH backgroundBrush = nullptr, surfaceBrush = nullptr, borderBrush = nullptr;
private:
    void release()
    {
        if (backgroundBrush) DeleteObject(backgroundBrush); if (surfaceBrush) DeleteObject(surfaceBrush); if (borderBrush) DeleteObject(borderBrush);
        backgroundBrush = surfaceBrush = borderBrush = nullptr;
    }
};
}
#endif
