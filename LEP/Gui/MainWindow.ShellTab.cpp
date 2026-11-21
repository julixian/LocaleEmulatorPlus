module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

module LEP.Gui.MainWindow;

import LEP.Gui.Win32Ui;
import LEP.ContextMenuInstaller;

NAMESPACE_BEGIN(lep::gui)

void MainWindow::buildShellTab(const HWND page)
{
    const int margin = s(12);
    const int boxW = s(696);
    const int innerX = margin + s(14);
    int y = margin;

    // 1. 经典右键菜单卡片（Win7/10 一级；Win11 「显示更多选项」）
    createGroupBox(page, m_boldFont, m_i18n.tr("shell_menu", "classic_group"), margin, y, boxW, s(134));
    m_shellUserStatus = createLabel(page, m_font, L"", innerX, y + s(28), s(210), s(22), kIdShellClassicUserStatus);
    m_shellUserInstallBtn = createButton(page, m_font, m_i18n.tr("shell_menu", "install_user"),
        innerX + s(220), y + s(24), s(240), s(28), kIdShellClassicUserInstallBtn);
    m_shellUserUninstallBtn = createButton(page, m_font, m_i18n.tr("shell_menu", "uninstall_user"),
        innerX + s(468), y + s(24), s(196), s(28), kIdShellClassicUserUninstallBtn);

    m_shellAllStatus = createLabel(page, m_font, L"", innerX, y + s(74), s(210), s(22), kIdShellClassicAllStatus);
    m_shellAllInstallBtn = createButton(page, m_font, m_i18n.tr("shell_menu", "install_all"),
        innerX + s(220), y + s(70), s(240), s(28), kIdShellClassicAllInstallBtn);
    m_shellAllUninstallBtn = createButton(page, m_font, m_i18n.tr("shell_menu", "uninstall_all"),
        innerX + s(468), y + s(70), s(196), s(28), kIdShellClassicAllUninstallBtn);
    y += s(148);

    // 2. Windows 11 一级右键菜单（MSIX 稀疏包 / Store 自动托管）
    createGroupBox(page, m_boldFont, m_i18n.tr("shell_menu", "win11_group"), margin, y, boxW, s(146));
    m_shellWin11Status = createLabel(page, m_boldFont, L"", innerX, y + s(26), boxW - s(28), s(22), kIdShellWin11Status);
    m_shellWin11InstallBtn = createButton(page, m_boldFont, m_i18n.tr("shell_menu", "install_win11"),
        innerX, y + s(54), s(240), s(30), kIdShellWin11InstallBtn);
    m_shellWin11UninstallBtn = createButton(page, m_font, m_i18n.tr("shell_menu", "uninstall_win11"),
        innerX + s(252), y + s(54), s(220), s(30), kIdShellWin11UninstallBtn);
    createLabel(page, m_font, m_i18n.tr("shell_menu", "win11_hint"), innerX, y + s(92), boxW - s(28), s(46));
    y += s(160);

    // 3. 辅助工具卡片（一键重启资源管理器）
    createGroupBox(page, m_boldFont, m_i18n.tr("shell_menu", "tools_group"), margin, y, boxW, s(100));
    createButton(page, m_font, m_i18n.tr("shell_menu", "restart_explorer"),
        innerX, y + s(26), s(180), s(30), kIdShellRestartExplorerBtn);
    createLabel(page, m_font, m_i18n.tr("shell_menu", "restart_explorer_hint"),
        innerX, y + s(64), boxW - s(28), s(28));
}

void MainWindow::refreshShellTab()
{
    if (!m_shellUserStatus) {
        return;
    }

    const ContextMenuStatus st = queryContextMenuStatus(m_appDir);

    setControlText(m_shellUserStatus, st.classicCurrentUser
        ? m_i18n.tr("shell_menu", "status_installed")
        : m_i18n.tr("shell_menu", "status_not_installed"));
    EnableWindow(m_shellUserUninstallBtn, st.classicCurrentUser);

    setControlText(m_shellAllStatus, st.classicAllUsers
        ? m_i18n.tr("shell_menu", "status_installed")
        : m_i18n.tr("shell_menu", "status_not_installed"));
    EnableWindow(m_shellAllUninstallBtn, st.classicAllUsers);

    if (st.packagedStoreMode) {
        setControlText(m_shellWin11Status, m_i18n.tr("shell_menu", "status_store_managed"));
        EnableWindow(m_shellWin11InstallBtn, FALSE);
        EnableWindow(m_shellWin11UninstallBtn, FALSE);
    }
    else if (!st.isWin11) {
        setControlText(m_shellWin11Status, m_i18n.tr("shell_menu", "status_win11_na"));
        EnableWindow(m_shellWin11InstallBtn, FALSE);
        EnableWindow(m_shellWin11UninstallBtn, FALSE);
    }
    else {
        setControlText(m_shellWin11Status, st.win11SparseInstalled
            ? m_i18n.tr("shell_menu", "status_installed")
            : m_i18n.tr("shell_menu", "status_not_installed"));
        EnableWindow(m_shellWin11InstallBtn, TRUE);
        EnableWindow(m_shellWin11UninstallBtn, st.win11SparseInstalled);
    }
}

bool MainWindow::handleShellCommand(const WORD id, const WORD /*notifyCode*/)
{
    auto reportResultFunc = [&](const std::expected<void, std::wstring>& res)
        {
            if (!res) {
                MessageBoxW(m_hwnd, (m_i18n.tr("shell_menu", "op_failed") + res.error()).c_str(),
                    m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
            }
            else {
                setStatus(m_i18n.tr("shell_menu", "op_ok"));
            }
            refreshShellTab();
        };

    switch (id)
    {
    case kIdShellClassicUserInstallBtn:
        reportResultFunc(installClassicContextMenu(m_appDir, false, m_hwnd));
        return true;

    case kIdShellClassicUserUninstallBtn:
        reportResultFunc(uninstallClassicContextMenu(false, m_hwnd));
        return true;

    case kIdShellClassicAllInstallBtn:
        reportResultFunc(installClassicContextMenu(m_appDir, true, m_hwnd));
        return true;

    case kIdShellClassicAllUninstallBtn:
        reportResultFunc(uninstallClassicContextMenu(true, m_hwnd));
        return true;

    case kIdShellWin11InstallBtn:
        reportResultFunc(installWin11SparsePackage(m_appDir, m_hwnd));
        return true;

    case kIdShellWin11UninstallBtn:
        reportResultFunc(uninstallWin11SparsePackage(m_appDir, m_hwnd));
        return true;

    case kIdShellRestartExplorerBtn:
        reportResultFunc(restartExplorer());
        return true;

    default:
        return false;
    }
}

NAMESPACE_END(lep::gui)
