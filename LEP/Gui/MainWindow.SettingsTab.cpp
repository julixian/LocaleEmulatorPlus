module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <shellapi.h>

module LEP.Gui.MainWindow;

import LEP.Gui.Win32Ui;

NAMESPACE_BEGIN(lep::gui)

void MainWindow::buildSettingsTab(const HWND page)
{
    const int margin = s(12);
    const int boxW = s(696);
    const int innerX = margin + s(14);
    int y = margin;

    // 1. 语言与基本状态
    createLabel(page, m_font, m_i18n.tr("settings", "lang_label"), margin, y + s(4), s(110), s(20));
    m_settingsLangCombo = createComboBox(page, m_font, margin + s(116), y, s(220), s(180), kIdSettingsLangCombo);
    int selectedLang = 0;
    for (size_t i = 0; i < m_languages.size(); ++i) {
        SendMessageW(m_settingsLangCombo, CB_ADDSTRING, 0, (LPARAM)m_languages[i].displayName.c_str());
        if (equalsIgnoreCaseW(m_languages[i].code, m_config.language)) {
            selectedLang = (int)i;
        }
    }
    SendMessageW(m_settingsLangCombo, CB_SETCURSEL, selectedLang, 0);
    createLabel(page, m_font, m_i18n.tr("settings", "lang_apply_hint"), margin + s(348), y + s(4), s(350), s(20));
    y += s(38);

    createLabel(page, m_boldFont,
        std::format(L"{} {}", m_i18n.tr("settings", "version_label"), LEP_APP_VERSION_WSTR),
        margin, y, s(260), s(22));
    createLabel(page, m_font,
        isPackagedApp() ? m_i18n.tr("settings", "mode_store") : m_i18n.tr("settings", "mode_portable"),
        margin + s(270), y, s(420), s(22));
    y += s(28);

    createLabel(page, m_font, m_i18n.tr("settings", "config_path_label"), margin, y + s(3), s(110), s(20));
    createEdit(page, m_font, m_configPath.wstring(), margin + s(116), y, boxW - s(116), s(24), 0, ES_READONLY);
    y += s(40);

    // 2. 核心组件自检卡片
    const auto comps = inspectCoreComponents(m_appDir);
    const int boxH = s(36) + (int)comps.size() * s(24);
    createGroupBox(page, m_boldFont, m_i18n.tr("settings", "core_status_group"), margin, y, boxW, boxH);
    int cy = y + s(26);
    for (const auto& c : comps) {
        const std::wstring badge = c.present ? L"[OK] " : L"[MISSING] ";
        const std::wstring stateText = c.present ? m_i18n.tr("settings", "core_ok") : m_i18n.tr("settings", "core_missing");
        createLabel(page, m_font, badge + c.fileName + L"  -  " + stateText, innerX, cy, boxW - s(28), s(20));
        cy += s(24);
    }
    y += boxH + s(16);

    // 3. 快捷打开目录按钮
    createButton(page, m_font, m_i18n.tr("settings", "open_config_dir"), margin, y, s(140), s(30), kIdSettingsOpenConfigDirBtn);
    createButton(page, m_font, m_i18n.tr("settings", "open_log_dir"), margin + s(152), y, s(140), s(30), kIdSettingsOpenLogDirBtn);
    createButton(page, m_font, m_i18n.tr("settings", "open_app_dir"), margin + s(304), y, s(140), s(30), kIdSettingsOpenAppDirBtn);
}

void MainWindow::refreshSettingsTab()
{
    // 静态信息在构造与重建页面时已就绪
}

bool MainWindow::handleSettingsCommand(const WORD id, const WORD notifyCode)
{
    if (id == kIdSettingsLangCombo && notifyCode == CBN_SELCHANGE) {
        const LRESULT idx = SendMessageW(m_settingsLangCombo, CB_GETCURSEL, 0, 0);
        if (idx >= 0 && (size_t)idx < m_languages.size()) {
            const std::wstring newLang = m_languages[(size_t)idx].code;
            if (!equalsIgnoreCaseW(newLang, m_config.language)) {
                m_config.language = newLang;
                (void)saveGlobalConfig(m_configPath, m_config);
                m_i18n.load(m_appDir, m_config.language);
                rebuildUi();
            }
        }
        return true;
    }

    if (id == kIdSettingsOpenConfigDirBtn) {
        const fs::path dir = m_configPath.parent_path();
        std::error_code ec;
        fs::create_directories(dir, ec);
        ShellExecuteW(m_hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    if (id == kIdSettingsOpenLogDirBtn) {
        const fs::path dir = m_appDir / L"Log";
        std::error_code ec;
        fs::create_directories(dir, ec);
        ShellExecuteW(m_hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    if (id == kIdSettingsOpenAppDirBtn) {
        ShellExecuteW(m_hwnd, L"open", m_appDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    return false;
}

NAMESPACE_END(lep::gui)
