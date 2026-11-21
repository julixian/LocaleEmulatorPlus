module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

module LEP.Gui.MainWindow;

import LEP.Gui.Win32Ui;
import LEP.Gui.AppProfileDialog;

NAMESPACE_BEGIN(lep::gui)

void MainWindow::buildProfilesTab(const HWND page)
{
    const int margin = s(10);
    const int leftW = s(205);
    const int rightX = margin + leftW + s(12);
    const int rightW = s(480);

    // 左侧：全局配置列表
    createLabel(page, m_boldFont, m_i18n.tr("profiles", "global_list"), margin, margin, leftW, s(20));
    m_profList = createListBox(page, m_font, margin, margin + s(22), leftW, s(210), kIdProfList);

    const int btnY = margin + s(238);
    createButton(page, m_font, m_i18n.tr("profiles", "add"), margin, btnY, s(64), s(26), kIdProfAddBtn);
    createButton(page, m_font, m_i18n.tr("profiles", "duplicate"), margin + s(70), btnY, s(64), s(26), kIdProfDupBtn);
    createButton(page, m_font, m_i18n.tr("profiles", "delete"), margin + s(140), btnY, s(65), s(26), kIdProfDelBtn);

    const int btnY2 = btnY + s(32);
    createButton(page, m_font, m_i18n.tr("profiles", "move_up"), margin, btnY2, s(98), s(26), kIdProfUpBtn);
    createButton(page, m_font, m_i18n.tr("profiles", "move_down"), margin + s(106), btnY2, s(99), s(26), kIdProfDownBtn);

    // 左下方：全局中保存的程序专属配置（如 ISO 只读目录回退保存项）
    const int appListY = btnY2 + s(38);
    createLabel(page, m_boldFont, m_i18n.tr("profiles", "app_list"), margin, appListY, leftW, s(20));
    m_globalAppList = createListBox(page, m_font, margin, appListY + s(22), leftW, s(82), kIdGlobalAppList);
    m_globalAppEditBtn = createButton(page, m_font, m_i18n.tr("profiles", "edit_selected_app"), margin, appListY + s(108), s(98), s(26), kIdGlobalAppEditBtn);
    m_globalAppDelBtn = createButton(page, m_font, m_i18n.tr("profiles", "remove_selected_app"), margin + s(106), appListY + s(108), s(99), s(26), kIdGlobalAppDelBtn);

    // 右侧：当前选中全局配置编辑表单
    const int labelW = s(122);
    const int fieldX = rightX + labelW + s(6);
    const int fieldW = rightW - labelW - s(6);
    int y = margin + s(4);

    createLabel(page, m_font, m_i18n.tr("profiles", "name"), rightX, y + s(3), labelW, s(20));
    m_profNameEdit = createEdit(page, m_font, L"", fieldX, y, fieldW, s(24), kIdProfNameEdit);
    y += s(34);

    createLabel(page, m_font, m_i18n.tr("profiles", "locale"), rightX, y + s(4), labelW, s(20));
    m_profLocaleCombo = createComboBox(page, m_font, fieldX, y, fieldW, s(240), kIdProfLocaleCombo);
    for (const auto& loc : m_locales) {
        SendMessageW(m_profLocaleCombo, CB_ADDSTRING, 0, (LPARAM)loc.displayName.c_str());
    }
    y += s(34);

    createLabel(page, m_font, m_i18n.tr("profiles", "timezone"), rightX, y + s(4), labelW, s(20));
    m_profTzCombo = createComboBox(page, m_font, fieldX, y, fieldW, s(240), kIdProfTzCombo);
    for (const auto& tz : m_timezones) {
        SendMessageW(m_profTzCombo, CB_ADDSTRING, 0, (LPARAM)tz.displayName.c_str());
    }
    y += s(34);

    createLabel(page, m_font, m_i18n.tr("profiles", "reg_mode"), rightX, y + s(4), labelW, s(20));
    m_profRegCombo = createComboBox(page, m_font, fieldX, y, fieldW, s(120), kIdProfRegCombo);
    SendMessageW(m_profRegCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "reg_mode_0").c_str());
    SendMessageW(m_profRegCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "reg_mode_1").c_str());
    SendMessageW(m_profRegCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "reg_mode_2").c_str());
    y += s(34);

    createLabel(page, m_font, m_i18n.tr("profiles", "ui_mode"), rightX, y + s(4), labelW, s(20));
    m_profUiCombo = createComboBox(page, m_font, fieldX, y, fieldW, s(120), kIdProfUiCombo);
    SendMessageW(m_profUiCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "ui_mode_0").c_str());
    SendMessageW(m_profUiCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "ui_mode_1").c_str());
    SendMessageW(m_profUiCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("profiles", "ui_mode_2").c_str());
    y += s(34);

    createLabel(page, m_font, m_i18n.tr("profiles", "arguments"), rightX, y + s(3), labelW, s(20));
    m_profArgsEdit = createEdit(page, m_font, L"", fieldX, y, fieldW, s(24), kIdProfArgsEdit);
    y += s(32);

    createLabel(page, m_font, m_i18n.tr("profiles", "tz_std_override"), rightX, y + s(3), labelW, s(20));
    const int dstLabelW = s(96);
    const int gap = s(10);
    const int labelGap = s(4);
    const int stdEditW = (fieldW - gap - dstLabelW - labelGap) / 2;
    const int dstLabelX = fieldX + stdEditW + gap;
    const int dstEditX = dstLabelX + dstLabelW + labelGap;
    const int dstEditW = fieldW - (dstEditX - fieldX);
    m_profStdEdit = createEdit(page, m_font, L"", fieldX, y, stdEditW, s(24), kIdProfStdEdit);
    createLabel(page, m_font, m_i18n.tr("profiles", "tz_dst_override"), dstLabelX, y + s(3), dstLabelW, s(20));
    m_profDstEdit = createEdit(page, m_font, L"", dstEditX, y, dstEditW, s(24), kIdProfDstEdit);
    y += s(34);

    m_profMenuCheck = createCheckbox(page, m_font, m_i18n.tr("profiles", "show_in_main_menu"), fieldX, y, fieldW, s(22), kIdProfMenuCheck);
    y += s(26);
    m_profAdminCheck = createCheckbox(page, m_font, m_i18n.tr("profiles", "run_as_admin"), fieldX, y, fieldW, s(22), kIdProfAdminCheck);
    y += s(26);
    m_profSuspendCheck = createCheckbox(page, m_font, m_i18n.tr("profiles", "run_with_suspend"), fieldX, y, fieldW, s(22), kIdProfSuspendCheck);
    y += s(38);

    createButton(page, m_boldFont, m_i18n.tr("profiles", "save"), fieldX, y, s(130), s(34), kIdProfSaveBtn, true);
}

void MainWindow::refreshProfilesTab()
{
    if (!m_profList) {
        return;
    }
    SendMessageW(m_profList, LB_RESETCONTENT, 0, 0);
    for (const auto& p : m_config.profiles) {
        SendMessageW(m_profList, LB_ADDSTRING, 0, (LPARAM)p.name.c_str());
    }

    if (m_config.profiles.empty()) {
        m_selectedProfileIndex = -1;
    }
    else {
        if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
            m_selectedProfileIndex = 0;
        }
        SendMessageW(m_profList, LB_SETCURSEL, m_selectedProfileIndex, 0);
        loadSelectedProfileToForm();
    }

    SendMessageW(m_globalAppList, LB_RESETCONTENT, 0, 0);
    for (const auto& entry : m_config.appProfiles) {
        const fs::path p(entry.targetPath);
        const std::wstring label = std::format(L"{} ({})", p.filename().wstring(), entry.profile.locale);
        SendMessageW(m_globalAppList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    }
    const bool hasApps = !m_config.appProfiles.empty();
    if (hasApps) {
        SendMessageW(m_globalAppList, LB_SETCURSEL, 0, 0);
    }
    EnableWindow(m_globalAppEditBtn, hasApps);
    EnableWindow(m_globalAppDelBtn, hasApps);
}

void MainWindow::loadSelectedProfileToForm()
{
    if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
        return;
    }
    const auto& p = m_config.profiles[(size_t)m_selectedProfileIndex];
    setControlText(m_profNameEdit, p.name);

    int locSel = 0;
    for (size_t i = 0; i < m_locales.size(); ++i) {
        if (equalsIgnoreCaseW(m_locales[i].localeName, p.locale)) {
            locSel = (int)i;
            break;
        }
    }
    SendMessageW(m_profLocaleCombo, CB_SETCURSEL, locSel, 0);

    int tzSel = 0;
    for (size_t i = 0; i < m_timezones.size(); ++i) {
        if (equalsIgnoreCaseW(m_timezones[i].id, p.timezone)) {
            tzSel = (int)i;
            break;
        }
    }
    SendMessageW(m_profTzCombo, CB_SETCURSEL, tzSel, 0);

    SendMessageW(m_profRegCombo, CB_SETCURSEL, std::clamp(p.registryRedirection, 0, 2), 0);
    SendMessageW(m_profUiCombo, CB_SETCURSEL, std::clamp(p.uiLanguageMode, 0, 2), 0);
    setControlText(m_profArgsEdit, p.arguments);
    setControlText(m_profStdEdit, p.standardNameOverride);
    setControlText(m_profDstEdit, p.daylightNameOverride);
    SendMessageW(m_profMenuCheck, BM_SETCHECK, p.showInMainMenu ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_profAdminCheck, BM_SETCHECK, p.runAsAdmin ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_profSuspendCheck, BM_SETCHECK, p.runWithSuspend ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool MainWindow::handleProfilesCommand(const WORD id, const WORD notifyCode)
{
    if (id == kIdProfList && notifyCode == LBN_SELCHANGE) {
        m_selectedProfileIndex = (int)SendMessageW(m_profList, LB_GETCURSEL, 0, 0);
        loadSelectedProfileToForm();
        return true;
    }

    if (id == kIdProfAddBtn) {
        const auto nameOpt = promptTextInput(m_hwnd, m_i18n.tr("profiles", "add"),
            m_i18n.tr("profiles", "name"), m_i18n.tr("profiles", "new_profile_default"));
        if (!nameOpt || nameOpt->empty()) {
            return true;
        }
        if (findGlobalProfileByName(m_config, *nameOpt)) {
            MessageBoxW(m_hwnd, m_i18n.tr("profiles", "name_duplicate").c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
            return true;
        }
        m_config.profiles.push_back(createDefaultProfile(*nameOpt));
        m_selectedProfileIndex = (int)m_config.profiles.size() - 1;
        if (auto res = saveGlobalConfig(m_configPath, m_config); !res) {
            MessageBoxW(m_hwnd, res.error().c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
        }
        else {
            setStatus(m_i18n.tr("profiles", "saved_ok"));
        }
        refreshProfilesTab();
        refreshLaunchTab();
        return true;
    }

    if (id == kIdProfDupBtn) {
        if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
            return true;
        }
        Profile copy = m_config.profiles[(size_t)m_selectedProfileIndex];
        const auto nameOpt = promptTextInput(m_hwnd, m_i18n.tr("profiles", "duplicate"),
            m_i18n.tr("profiles", "name"), copy.name + L" (2)");
        if (!nameOpt || nameOpt->empty()) {
            return true;
        }
        if (findGlobalProfileByName(m_config, *nameOpt)) {
            MessageBoxW(m_hwnd, m_i18n.tr("profiles", "name_duplicate").c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
            return true;
        }
        copy.name = *nameOpt;
        m_config.profiles.push_back(std::move(copy));
        m_selectedProfileIndex = (int)m_config.profiles.size() - 1;
        (void)saveGlobalConfig(m_configPath, m_config);
        setStatus(m_i18n.tr("profiles", "saved_ok"));
        refreshProfilesTab();
        refreshLaunchTab();
        return true;
    }

    if (id == kIdProfDelBtn) {
        if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
            return true;
        }
        if (MessageBoxW(m_hwnd, m_i18n.tr("profiles", "confirm_delete").c_str(),
                m_i18n.tr("app", "confirm").c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            m_config.profiles.erase(m_config.profiles.begin() + m_selectedProfileIndex);
            if (m_selectedProfileIndex >= (int)m_config.profiles.size()) {
                m_selectedProfileIndex = (int)m_config.profiles.size() - 1;
            }
            (void)saveGlobalConfig(m_configPath, m_config);
            refreshProfilesTab();
            refreshLaunchTab();
        }
        return true;
    }

    if (id == kIdProfUpBtn || id == kIdProfDownBtn) {
        if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
            return true;
        }
        const int target = (id == kIdProfUpBtn) ? (m_selectedProfileIndex - 1) : (m_selectedProfileIndex + 1);
        if (target < 0 || target >= (int)m_config.profiles.size()) {
            return true;
        }
        std::swap(m_config.profiles[(size_t)m_selectedProfileIndex], m_config.profiles[(size_t)target]);
        m_selectedProfileIndex = target;
        (void)saveGlobalConfig(m_configPath, m_config);
        refreshProfilesTab();
        refreshLaunchTab();
        return true;
    }

    if (id == kIdProfSaveBtn) {
        if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= (int)m_config.profiles.size()) {
            return true;
        }
        const std::wstring newName = trimW(getControlText(m_profNameEdit));
        if (newName.empty()) {
            MessageBoxW(m_hwnd, m_i18n.tr("profiles", "name_empty").c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
            return true;
        }
        for (int i = 0; i < (int)m_config.profiles.size(); ++i) {
            if (i != m_selectedProfileIndex && equalsIgnoreCaseW(m_config.profiles[(size_t)i].name, newName)) {
                MessageBoxW(m_hwnd, m_i18n.tr("profiles", "name_duplicate").c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
                return true;
            }
        }

        auto& p = m_config.profiles[(size_t)m_selectedProfileIndex];
        p.name = newName;
        if (const LRESULT idx = SendMessageW(m_profLocaleCombo, CB_GETCURSEL, 0, 0); idx >= 0 && (size_t)idx < m_locales.size()) {
            p.locale = m_locales[(size_t)idx].localeName;
        }
        if (const LRESULT idx = SendMessageW(m_profTzCombo, CB_GETCURSEL, 0, 0); idx >= 0 && (size_t)idx < m_timezones.size()) {
            p.timezone = m_timezones[(size_t)idx].id;
        }
        p.registryRedirection = (int)std::max<LRESULT>(0, SendMessageW(m_profRegCombo, CB_GETCURSEL, 0, 0));
        p.uiLanguageMode = (int)std::max<LRESULT>(0, SendMessageW(m_profUiCombo, CB_GETCURSEL, 0, 0));
        p.arguments = trimW(getControlText(m_profArgsEdit));
        p.standardNameOverride = trimW(getControlText(m_profStdEdit));
        p.daylightNameOverride = trimW(getControlText(m_profDstEdit));
        p.showInMainMenu = SendMessageW(m_profMenuCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        p.runAsAdmin = SendMessageW(m_profAdminCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        p.runWithSuspend = SendMessageW(m_profSuspendCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

        if (auto res = saveGlobalConfig(m_configPath, m_config); !res) {
            MessageBoxW(m_hwnd, res.error().c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
        }
        else {
            setStatus(m_i18n.tr("profiles", "saved_ok"));
            refreshProfilesTab();
            refreshLaunchTab();
        }
        return true;
    }

    if (id == kIdGlobalAppEditBtn || (id == kIdGlobalAppList && notifyCode == LBN_DBLCLK)) {
        const LRESULT idx = SendMessageW(m_globalAppList, LB_GETCURSEL, 0, 0);
        if (idx >= 0 && (size_t)idx < m_config.appProfiles.size()) {
            const fs::path target(m_config.appProfiles[(size_t)idx].targetPath);
            const auto res = showAppProfileDialog(
                m_hwnd, m_appDir, m_configPath, m_config, m_i18n, m_locales, m_timezones, target);
            if (res.changed) {
                refreshProfilesTab();
                refreshLaunchTab();
            }
        }
        return true;
    }

    if (id == kIdGlobalAppDelBtn) {
        const LRESULT idx = SendMessageW(m_globalAppList, LB_GETCURSEL, 0, 0);
        if (idx >= 0 && (size_t)idx < m_config.appProfiles.size()) {
            if (MessageBoxW(m_hwnd, m_i18n.tr("profiles", "confirm_delete_app").c_str(),
                    m_i18n.tr("app", "confirm").c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                m_config.appProfiles.erase(m_config.appProfiles.begin() + idx);
                (void)saveGlobalConfig(m_configPath, m_config);
                setStatus(m_i18n.tr("profiles", "app_profile_deleted"));
                refreshProfilesTab();
                refreshLaunchTab();
            }
        }
        return true;
    }

    return false;
}

NAMESPACE_END(lep::gui)
