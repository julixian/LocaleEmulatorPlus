module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>
#include <shellapi.h>

module LEP.Gui.MainWindow;

import LEP.Gui.Win32Ui;

NAMESPACE_BEGIN(lep::gui)

namespace
{
    constexpr PCWSTR kMainClassName = L"LEP_MainWindowClass_v3";
    constexpr PCWSTR kPageClassName = L"LEP_TabPageClass_v3";

    LRESULT CALLBACK tabPageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_COMMAND || msg == WM_NOTIFY || msg == WM_DROPFILES) {
            return SendMessageW(GetParent(hwnd), msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

MainWindow::MainWindow(fs::path appDir, fs::path initialTargetPath)
    : m_appDir(std::move(appDir)),
      m_configPath(resolveGlobalConfigPath(m_appDir)),
      m_initialTarget(std::move(initialTargetPath))
{
    if (auto cfg = loadOrCreateGlobalConfig(m_configPath)) {
        m_config = std::move(*cfg);
    }
    else {
        m_config = createDefaultGlobalConfig();
    }
    m_i18n.load(m_appDir, m_config.language);
    m_locales = enumerateLocales();
    m_timezones = enumerateTimezones();
    m_languages = I18n::availableLanguages(m_appDir);
}

MainWindow::~MainWindow()
{
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
    if (m_boldFont) {
        DeleteObject(m_boldFont);
        m_boldFont = nullptr;
    }
}

int MainWindow::s(const int px96) const
{
    return scaleDpi(px96, m_dpi);
}

void MainWindow::setStatus(const std::wstring_view message)
{
    if (!m_statusBar) {
        return;
    }
    const std::wstring text(message);
    SendMessageW(m_statusBar, SB_SETTEXTW, 0, (LPARAM)text.c_str());
}

void MainWindow::switchTab(const int index)
{
    if (index < 0 || index >= (int)m_pages.size()) {
        return;
    }
    m_activeTab = index;
    TabCtrl_SetCurSel(m_tabCtrl, index);
    for (int i = 0; i < (int)m_pages.size(); ++i) {
        if (m_pages[i]) {
            ShowWindow(m_pages[i], i == index ? SW_SHOW : SW_HIDE);
        }
    }
    if (index == 0) refreshLaunchTab();
    else if (index == 1) refreshProfilesTab();
    else if (index == 2) refreshShellTab();
    else if (index == 3) refreshSettingsTab();

    if (m_pages[index]) {
        SetWindowPos(m_pages[index], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        RedrawWindow(m_pages[index], nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

void MainWindow::initUi()
{
    m_dpi = getWindowDpi(m_hwnd);
    if (m_font) DeleteObject(m_font);
    if (m_boldFont) DeleteObject(m_boldFont);
    m_font = createUiFont(m_dpi, 9, false);
    m_boldFont = createUiFont(m_dpi, 9, true);

    RECT client{};
    GetClientRect(m_hwnd, &client);
    const int clientW = client.right - client.left;
    const int clientH = client.bottom - client.top;

    m_statusBar = CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        m_hwnd, (HMENU)(INT_PTR)kIdStatusBar, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(m_statusBar, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_statusBar, WM_SIZE, 0, 0);

    RECT sbRect{};
    GetWindowRect(m_statusBar, &sbRect);
    const int sbH = sbRect.bottom - sbRect.top;

    const int margin = s(10);
    m_tabCtrl = CreateWindowExW(
        0, WC_TABCONTROLW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        margin, margin, clientW - margin * 2, clientH - sbH - margin * 2,
        m_hwnd, (HMENU)(INT_PTR)kIdTabControl, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(m_tabCtrl, WM_SETFONT, (WPARAM)m_font, TRUE);

    const std::array<std::wstring, 4> tabTitles = {
        m_i18n.tr("tabs", "launch"),
        m_i18n.tr("tabs", "profiles"),
        m_i18n.tr("tabs", "shell_menu"),
        m_i18n.tr("tabs", "settings"),
    };
    for (int i = 0; i < 4; ++i) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(tabTitles[i].c_str());
        TabCtrl_InsertItem(m_tabCtrl, i, &item);
    }

    RECT pageRect{};
    GetClientRect(m_tabCtrl, &pageRect);
    TabCtrl_AdjustRect(m_tabCtrl, FALSE, &pageRect);
    MapWindowPoints(m_tabCtrl, m_hwnd, (POINT*)&pageRect, 2);

    const int pageX = pageRect.left + s(4);
    const int pageY = pageRect.top + s(4);
    const int pageW = (pageRect.right - pageRect.left) - s(8);
    const int pageH = (pageRect.bottom - pageRect.top) - s(8);

    for (int i = 0; i < 4; ++i) {
        m_pages[i] = CreateWindowExW(
            WS_EX_CONTROLPARENT,
            kPageClassName,
            nullptr,
            WS_CHILD | WS_CLIPSIBLINGS | (i == m_activeTab ? WS_VISIBLE : 0),
            pageX, pageY, pageW, pageH,
            m_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (m_pages[i]) {
            SetWindowPos(m_pages[i], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    buildLaunchTab(m_pages[0]);
    buildProfilesTab(m_pages[1]);
    buildShellTab(m_pages[2]);
    buildSettingsTab(m_pages[3]);

    refreshProfilesTab();

    if (!m_initialTarget.empty()) {
        setLaunchTarget(m_initialTarget);
    }

    switchTab(m_activeTab);
    setStatus(m_i18n.tr("app", "ready"));
}

void MainWindow::rebuildUi()
{
    const std::wstring currentTarget = m_launchTargetEdit ? getControlText(m_launchTargetEdit) : m_initialTarget.wstring();
    if (!currentTarget.empty()) {
        m_initialTarget = fs::path(currentTarget);
    }

    for (HWND& page : m_pages) {
        if (page) {
            DestroyWindow(page);
            page = nullptr;
        }
    }
    if (m_tabCtrl) {
        DestroyWindow(m_tabCtrl);
        m_tabCtrl = nullptr;
    }
    if (m_statusBar) {
        DestroyWindow(m_statusBar);
        m_statusBar = nullptr;
    }

    const std::wstring title = std::format(L"{} v{}", m_i18n.tr("app", "title"), LEP_APP_VERSION_WSTR);
    SetWindowTextW(m_hwnd, title.c_str());
    initUi();
}

LRESULT CALLBACK MainWindow::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* pThis = (MainWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        const auto* pCs = (const CREATESTRUCTW*)lParam;
        pThis = (MainWindow*)pCs->lpCreateParams;
        pThis->m_hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    if (pThis) {
        return pThis->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handleMessage(const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        allowElevatedDragDrop(m_hwnd);
        initUi();
        return 0;

    case WM_DROPFILES:
    {
        const HDROP hDrop = (HDROP)wParam;
        wchar_t pathBuf[MAX_PATH * 4]{};
        if (DragQueryFileW(hDrop, 0, pathBuf, (UINT)std::size(pathBuf)) > 0) {
            setLaunchTarget(fs::path(pathBuf));
            switchTab(0);
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_NOTIFY:
    {
        const auto* pHdr = (const NMHDR*)lParam;
        if (pHdr && pHdr->idFrom == kIdTabControl && pHdr->code == TCN_SELCHANGE) {
            switchTab(TabCtrl_GetCurSel(m_tabCtrl));
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        const WORD id = LOWORD(wParam);
        const WORD notify = HIWORD(wParam);
        if (handleLaunchCommand(id, notify)) return 0;
        if (handleProfilesCommand(id, notify)) return 0;
        if (handleShellCommand(id, notify)) return 0;
        if (handleSettingsCommand(id, notify)) return 0;
        break;
    }

    case WM_CLOSE:
        DestroyWindow(m_hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

int MainWindow::run()
{
    INITCOMMONCONTROLSEX icc{ .dwSize = sizeof(icc), .dwICC = ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    const HINSTANCE hInst = GetModuleHandleW(nullptr);
    static bool s_classesRegistered = false;
    if (!s_classesRegistered) {
        WNDCLASSEXW wcMain{};
        wcMain.cbSize = sizeof(wcMain);
        wcMain.lpfnWndProc = windowProc;
        wcMain.hInstance = hInst;
        wcMain.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
        wcMain.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(1));
        wcMain.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcMain.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wcMain.lpszClassName = kMainClassName;
        RegisterClassExW(&wcMain);

        WNDCLASSEXW wcPage{};
        wcPage.cbSize = sizeof(wcPage);
        wcPage.lpfnWndProc = tabPageProc;
        wcPage.hInstance = hInst;
        wcPage.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcPage.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wcPage.lpszClassName = kPageClassName;
        RegisterClassExW(&wcPage);

        s_classesRegistered = true;
    }

    const UINT screenDpi = getWindowDpi(nullptr);
    const int winW = scaleDpi(760, screenDpi);
    const int winH = scaleDpi(585, screenDpi);
    const std::wstring title = std::format(L"{} v{}", m_i18n.tr("app", "title"), LEP_APP_VERSION_WSTR);

    m_hwnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        kMainClassName,
        title.c_str(),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!m_hwnd) {
        return 1;
    }

    applyModernWindowChrome(m_hwnd);
    centerWindow(m_hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(m_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}

NAMESPACE_END(lep::gui)
