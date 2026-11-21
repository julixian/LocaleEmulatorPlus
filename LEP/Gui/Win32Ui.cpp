module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <shellapi.h>

module LEP.Gui.Win32Ui;

import LEPCommon;

NAMESPACE_BEGIN(lep::gui)

void enableProcessDpiAwareness()
{
    if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiCtxFn = BOOL (WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (const auto fn = (SetDpiCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext")) {
            if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                return;
            }
            if (fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                return;
            }
        }
    }
    SetProcessDPIAware();
}

UINT getWindowDpi(const HWND hwnd)
{
    if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindowFn = UINT (WINAPI*)(HWND);
        if (const auto fn = (GetDpiForWindowFn)GetProcAddress(user32, "GetDpiForWindow")) {
            if (hwnd) {
                const UINT dpi = fn(hwnd);
                if (dpi != 0) {
                    return dpi;
                }
            }
        }
    }
    const HDC hdc = GetDC(hwnd);
    const UINT dpi = hdc ? (UINT)GetDeviceCaps(hdc, LOGPIXELSX) : 96u;
    if (hdc) {
        ReleaseDC(hwnd, hdc);
    }
    return dpi != 0 ? dpi : 96u;
}

int scaleDpi(const int px96, const UINT dpi)
{
    return MulDiv(px96, (int)dpi, 96);
}

HFONT createUiFont(const UINT dpi, const int pointSize, const bool bold)
{
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(pointSize, (int)dpi, 72);
    lf.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;

    const LANGID uiLang = GetUserDefaultUILanguage();
    const WORD primary = PRIMARYLANGID(uiLang);
    const PCWSTR face = (primary == LANG_CHINESE) ? L"Microsoft YaHei UI"
        : (primary == LANG_JAPANESE) ? L"Yu Gothic UI"
        : L"Segoe UI";
    wcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
    return CreateFontIndirectW(&lf);
}

void applyModernWindowChrome(const HWND hwnd)
{
    if (!hwnd) {
        return;
    }
    const HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) {
        return;
    }
    using DwmSetAttrFn = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    if (const auto fn = (DwmSetAttrFn)GetProcAddress(dwm, "DwmSetWindowAttribute")) {
        // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2（仅 Win11 生效，旧系统安全忽略）
        const DWORD roundPref = 2;
        fn(hwnd, 33, &roundPref, sizeof(roundPref));
    }
    FreeLibrary(dwm);
}

void allowElevatedDragDrop(const HWND hwnd)
{
    if (!hwnd) {
        return;
    }
    constexpr UINT kWmCopyGlobalData = 0x0049;
    if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using ChangeFilterExFn = BOOL (WINAPI*)(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT);
        if (const auto fn = (ChangeFilterExFn)GetProcAddress(user32, "ChangeWindowMessageFilterEx")) {
            fn(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
            fn(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
            fn(hwnd, kWmCopyGlobalData, MSGFLT_ALLOW, nullptr);
        }
        else {
            using ChangeFilterFn = BOOL (WINAPI*)(UINT, DWORD);
            if (const auto fnOld = (ChangeFilterFn)GetProcAddress(user32, "ChangeWindowMessageFilter")) {
                fnOld(WM_DROPFILES, MSGFLT_ADD);
                fnOld(WM_COPYDATA, MSGFLT_ADD);
                fnOld(kWmCopyGlobalData, MSGFLT_ADD);
            }
        }
    }
    DragAcceptFiles(hwnd, TRUE);
}

std::wstring getControlText(const HWND hwnd)
{
    if (!hwnd) {
        return {};
    }
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) {
        return {};
    }
    std::wstring text((size_t)len + 1, L'\0');
    const int copied = GetWindowTextW(hwnd, text.data(), len + 1);
    text.resize((size_t)std::max(0, copied));
    return text;
}

void setControlText(const HWND hwnd, const std::wstring_view text)
{
    if (!hwnd) {
        return;
    }
    const std::wstring owned(text);
    SetWindowTextW(hwnd, owned.c_str());
}

bool copyTextToClipboard(const HWND owner, const std::wstring_view text)
{
    if (!OpenClipboard(owner)) {
        return false;
    }
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    const HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    if (void* pDst = GlobalLock(hMem)) {
        std::memcpy(pDst, text.data(), text.size() * sizeof(wchar_t));
        ((wchar_t*)pDst)[text.size()] = L'\0';
        GlobalUnlock(hMem);
    }
    const bool ok = SetClipboardData(CF_UNICODETEXT, hMem) != nullptr;
    if (!ok) {
        GlobalFree(hMem);
    }
    CloseClipboard();
    return ok;
}

namespace
{
    HWND createStyledControl(
        const PCWSTR className,
        const HWND parent,
        const HFONT font,
        const std::wstring_view text,
        const DWORD style,
        const DWORD exStyle,
        const int x,
        const int y,
        const int w,
        const int h,
        const int id)
    {
        const std::wstring title(text);
        const HWND ctrl = CreateWindowExW(
            exStyle, className, title.c_str(),
            WS_CHILD | WS_VISIBLE | style,
            x, y, w, h,
            parent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
        if (ctrl && font) {
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)font, TRUE);
        }
        return ctrl;
    }
}

HWND createLabel(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id, const DWORD extraStyle)
{
    return createStyledControl(WC_STATICW, parent, font, text, SS_LEFT | SS_NOPREFIX | extraStyle, 0, x, y, w, h, id);
}

HWND createGroupBox(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id)
{
    return createStyledControl(WC_BUTTONW, parent, font, text, BS_GROUPBOX, 0, x, y, w, h, id);
}

HWND createEdit(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id, const DWORD extraStyle)
{
    return createStyledControl(WC_EDITW, parent, font, text, WS_TABSTOP | ES_AUTOHSCROLL | extraStyle, WS_EX_CLIENTEDGE, x, y, w, h, id);
}

HWND createButton(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id, const bool isDefault)
{
    return createStyledControl(WC_BUTTONW, parent, font, text, WS_TABSTOP | (isDefault ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, x, y, w, h, id);
}

HWND createCheckbox(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id)
{
    return createStyledControl(WC_BUTTONW, parent, font, text, WS_TABSTOP | BS_AUTOCHECKBOX, 0, x, y, w, h, id);
}

HWND createRadio(const HWND parent, const HFONT font, const std::wstring_view text, const int x, const int y, const int w, const int h, const int id, const bool groupStart)
{
    return createStyledControl(WC_BUTTONW, parent, font, text, WS_TABSTOP | BS_AUTORADIOBUTTON | (groupStart ? WS_GROUP : 0), 0, x, y, w, h, id);
}

HWND createComboBox(const HWND parent, const HFONT font, const int x, const int y, const int w, const int h, const int id, const bool editable)
{
    const DWORD style = WS_TABSTOP | WS_VSCROLL | (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST);
    return createStyledControl(WC_COMBOBOXW, parent, font, L"", style, 0, x, y, w, h, id);
}

HWND createListBox(const HWND parent, const HFONT font, const int x, const int y, const int w, const int h, const int id)
{
    return createStyledControl(WC_LISTBOXW, parent, font, L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE, x, y, w, h, id);
}

std::optional<fs::path> browseOpenTargetFile(const HWND owner, const fs::path& initialDir)
{
    wchar_t fileBuf[MAX_PATH * 4]{};
    const std::wstring dirStr = initialDir.wstring();

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"可执行文件与快捷方式 (*.exe;*.lnk)\0*.exe;*.lnk\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = (DWORD)std::size(fileBuf);
    ofn.lpstrInitialDir = dirStr.empty() ? nullptr : dirStr.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_NODEREFERENCELINKS;

    if (!GetOpenFileNameW(&ofn)) {
        return std::nullopt;
    }
    return fs::path(fileBuf);
}

std::optional<fs::path> browseSaveShortcutFile(const HWND owner, const fs::path& suggestedPath)
{
    wchar_t fileBuf[MAX_PATH * 4]{};
    const std::wstring fileName = suggestedPath.filename().wstring();
    const std::wstring initDir = suggestedPath.parent_path().wstring();
    wcsncpy_s(fileBuf, fileName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Windows 快捷方式 (*.lnk)\0*.lnk\0";
    ofn.lpstrDefExt = L"lnk";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = (DWORD)std::size(fileBuf);
    ofn.lpstrInitialDir = initDir.empty() ? nullptr : initDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn)) {
        return std::nullopt;
    }
    return fs::path(fileBuf);
}

void centerWindow(const HWND hwnd, const HWND relativeTo)
{
    if (!hwnd) {
        return;
    }
    RECT winRect{};
    GetWindowRect(hwnd, &winRect);
    const int winW = winRect.right - winRect.left;
    const int winH = winRect.bottom - winRect.top;

    RECT refRect{};
    if (relativeTo && IsWindow(relativeTo)) {
        GetWindowRect(relativeTo, &refRect);
    }
    else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &refRect, 0);
    }

    const int x = refRect.left + ((refRect.right - refRect.left) - winW) / 2;
    const int y = refRect.top + ((refRect.bottom - refRect.top) - winH) / 2;
    SetWindowPos(hwnd, nullptr, std::max(0, x), std::max(0, y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

namespace
{
    struct InputDialogState
    {
        std::wstring label;
        std::wstring value;
        bool accepted = false;
        HWND editCtrl = nullptr;
        HFONT font = nullptr;
    };

    LRESULT CALLBACK inputDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* pState = (InputDialogState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg)
        {
        case WM_CREATE:
        {
            const auto* pCs = (const CREATESTRUCTW*)lParam;
            pState = (InputDialogState*)pCs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pState);

            const UINT dpi = getWindowDpi(hwnd);
            pState->font = createUiFont(dpi);
            auto s = [dpi](int v) { return scaleDpi(v, dpi); };

            createLabel(hwnd, pState->font, pState->label, s(16), s(16), s(330), s(20));
            pState->editCtrl = createEdit(hwnd, pState->font, pState->value, s(16), s(40), s(330), s(26), 1001);
            createButton(hwnd, pState->font, L"确定", s(182), s(80), s(78), s(28), IDOK, true);
            createButton(hwnd, pState->font, L"取消", s(268), s(80), s(78), s(28), IDCANCEL);

            SetFocus(pState->editCtrl);
            SendMessageW(pState->editCtrl, EM_SETSEL, 0, -1);
            return 0;
        }

        case WM_COMMAND:
        {
            const WORD id = LOWORD(wParam);
            if (id == IDOK && pState) {
                pState->value = trimW(getControlText(pState->editCtrl));
                pState->accepted = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (pState && pState->font) {
                DeleteObject(pState->font);
                pState->font = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

std::optional<std::wstring> promptTextInput(
    const HWND owner,
    const std::wstring_view title,
    const std::wstring_view label,
    const std::wstring_view initialValue)
{
    static bool s_registered = false;
    constexpr PCWSTR kClassName = L"LEP_InputDialogClass";
    if (!s_registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = inputDialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);
        s_registered = true;
    }

    InputDialogState state{
        .label = std::wstring(label),
        .value = std::wstring(initialValue),
    };

    const UINT dpi = getWindowDpi(owner);
    const int width = scaleDpi(380, dpi);
    const int height = scaleDpi(160, dpi);
    const std::wstring titleStr(title);

    if (owner) {
        EnableWindow(owner, FALSE);
    }

    const HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kClassName,
        titleStr.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        owner, nullptr, GetModuleHandleW(nullptr), &state);

    if (dlg) {
        applyModernWindowChrome(dlg);
        centerWindow(dlg, owner);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(dlg, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    return state.accepted ? std::optional<std::wstring>(std::move(state.value)) : std::nullopt;
}

NAMESPACE_END(lep::gui)
