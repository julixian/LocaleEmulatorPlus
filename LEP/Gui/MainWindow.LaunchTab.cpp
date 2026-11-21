module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

module LEP.Gui.MainWindow;

import LEP.Gui.Win32Ui;
import LEP.Gui.AppProfileDialog;
import LEP.ContextMenuInstaller;

NAMESPACE_BEGIN(lep::gui)

void MainWindow::buildLaunchTab(const HWND page)
{
    const int margin = s(12);
    const int boxW = s(696);
    int y = margin;

    // 1. 目标程序与参数卡片
    createGroupBox(page, m_boldFont, m_i18n.tr("launch", "target_group"), margin, y, boxW, s(192));
    int innerY = y + s(26);
    const int labelX = margin + s(14);
    const int labelW = s(95);
    const int fieldX = labelX + labelW + s(6);

    createLabel(page, m_font, m_i18n.tr("launch", "target_path"), labelX, innerY + s(3), labelW, s(20));
    m_launchTargetEdit = createEdit(page, m_font, L"", fieldX, innerY, s(472), s(24), kIdLaunchTargetEdit);
    createButton(page, m_font, m_i18n.tr("launch", "browse"), fieldX + s(480), innerY - s(1), s(86), s(26), kIdLaunchBrowseBtn);
    innerY += s(34);

    createLabel(page, m_font, m_i18n.tr("launch", "profile_label"), labelX, innerY + s(4), labelW, s(20));
    m_launchProfileCombo = createComboBox(page, m_font, fieldX, innerY, s(566), s(220), kIdLaunchProfileCombo);
    innerY += s(34);

    createLabel(page, m_font, m_i18n.tr("launch", "extra_args"), labelX, innerY + s(3), labelW, s(20));
    m_launchArgsEdit = createEdit(page, m_font, L"", fieldX, innerY, s(566), s(24), kIdLaunchArgsEdit);
    innerY += s(34);

    createLabel(page, m_font, m_i18n.tr("launch", "command_preview"), labelX, innerY + s(3), labelW, s(20));
    m_launchCmdEdit = createEdit(page, m_font, L"", fieldX, innerY, s(472), s(24), kIdLaunchCmdEdit, ES_READONLY);
    createButton(page, m_font, m_i18n.tr("launch", "copy_command"), fieldX + s(480), innerY - s(1), s(86), s(26), kIdLaunchCopyCmdBtn);

    y += s(206);

    // 2. 当前程序专属配置状态卡片
    createGroupBox(page, m_boldFont, m_i18n.tr("profiles", "app_editor_title"), margin, y, boxW, s(100));
    m_launchAppStatusLabel = createLabel(page, m_font, m_i18n.tr("launch", "app_profile_status_none"),
        labelX, y + s(26), boxW - s(28), s(22), kIdLaunchAppStatusLabel);
    m_launchAppProfileBtn = createButton(page, m_font, m_i18n.tr("launch", "create_app_profile"),
        labelX, y + s(56), s(200), s(30), kIdLaunchAppProfileBtn);
    m_launchDeleteAppBtn = createButton(page, m_font, m_i18n.tr("launch", "delete_app_profile"),
        labelX + s(212), y + s(56), s(165), s(30), kIdLaunchDeleteAppBtn);

    y += s(116);

    // 3. 底部主操作按钮（职责清晰分离）
    createButton(page, m_boldFont, m_i18n.tr("launch", "run_now"), margin, y, s(150), s(36), kIdLaunchRunBtn, true);
    createButton(page, m_font, m_i18n.tr("launch", "create_shortcut"), margin + s(162), y, s(170), s(36), kIdLaunchShortcutBtn);
}

void MainWindow::setLaunchTarget(const fs::path& path)
{
    if (path.empty()) {
        return;
    }
    fs::path resolved = path;
    std::wstring extraArgs;
    if (equalsIgnoreCaseW(path.extension().wstring(), L".lnk")) {
        if (const auto lnk = resolveShortcutTarget(path)) {
            resolved = lnk->targetPath;
            extraArgs = lnk->arguments;
        }
    }
    if (m_launchTargetEdit) {
        setControlText(m_launchTargetEdit, resolved.wstring());
    }
    if (m_launchArgsEdit && !extraArgs.empty()) {
        setControlText(m_launchArgsEdit, extraArgs);
    }
    refreshLaunchTab();
}

void MainWindow::refreshLaunchTab()
{
    if (!m_launchProfileCombo) {
        return;
    }

    const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
    const fs::path targetPath(targetText);
    const auto appProf = !targetText.empty() ? findAppProfile(targetPath, m_config) : std::nullopt;

    const LRESULT prevSel = SendMessageW(m_launchProfileCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(m_launchProfileCombo, CB_RESETCONTENT, 0, 0);

    if (appProf) {
        SendMessageW(m_launchProfileCombo, CB_ADDSTRING, 0, (LPARAM)m_i18n.tr("launch", "app_profile_item").c_str());
    }
    for (const auto& p : m_config.profiles) {
        SendMessageW(m_launchProfileCombo, CB_ADDSTRING, 0, (LPARAM)p.name.c_str());
    }

    const LRESULT count = SendMessageW(m_launchProfileCombo, CB_GETCOUNT, 0, 0);
    if (count > 0) {
        const LRESULT targetSel = (prevSel >= 0 && prevSel < count) ? prevSel : 0;
        SendMessageW(m_launchProfileCombo, CB_SETCURSEL, targetSel, 0);
    }

    if (appProf) {
        const bool localStorage = (appProf->storage == AppProfileStorage::AlongsideTarget);
        const std::wstring statusText = localStorage
            ? (m_i18n.tr("launch", "app_profile_status_local") + appProf->configFilePath.filename().wstring())
            : m_i18n.tr("launch", "app_profile_status_global");
        setControlText(m_launchAppStatusLabel, statusText);
        setControlText(m_launchAppProfileBtn, m_i18n.tr("launch", "edit_app_profile"));
        EnableWindow(m_launchDeleteAppBtn, TRUE);
        const bool deleteNeedsAdmin = localStorage &&
            probeDirectoryWriteStatus(targetPath.parent_path()) == DirectoryWriteStatus::NeedsAdmin;
        SendMessageW(m_launchDeleteAppBtn, BCM_SETSHIELD, 0, deleteNeedsAdmin ? TRUE : FALSE);
    }
    else {
        setControlText(m_launchAppStatusLabel, m_i18n.tr("launch", "app_profile_status_none"));
        setControlText(m_launchAppProfileBtn, m_i18n.tr("launch", "create_app_profile"));
        EnableWindow(m_launchDeleteAppBtn, FALSE);
        SendMessageW(m_launchDeleteAppBtn, BCM_SETSHIELD, 0, FALSE);
    }

    EnableWindow(m_launchAppProfileBtn, !targetText.empty());
    updateLaunchCommandPreview();
}

void MainWindow::updateLaunchCommandPreview()
{
    if (!m_launchCmdEdit) {
        return;
    }
    const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
    if (targetText.empty()) {
        setControlText(m_launchCmdEdit, L"");
        return;
    }

    const fs::path targetPath(targetText);
    const auto appProf = findAppProfile(targetPath, m_config);
    const LRESULT sel = SendMessageW(m_launchProfileCombo, CB_GETCURSEL, 0, 0);

    std::optional<std::wstring_view> profileName;
    if (appProf) {
        if (sel > 0 && (size_t)(sel - 1) < m_config.profiles.size()) {
            profileName = m_config.profiles[(size_t)(sel - 1)].name;
        }
    }
    else if (sel >= 0 && (size_t)sel < m_config.profiles.size()) {
        profileName = m_config.profiles[(size_t)sel].name;
    }

    const std::wstring extraArgs = trimW(getControlText(m_launchArgsEdit));
    const std::wstring cmd = buildFullLaunchCommandLine(getModuleFilePath(), targetPath, profileName, extraArgs);
    setControlText(m_launchCmdEdit, cmd);
}

bool MainWindow::handleLaunchCommand(const WORD id, const WORD notifyCode)
{
    if (id == kIdLaunchTargetEdit && notifyCode == EN_CHANGE) {
        refreshLaunchTab();
        return true;
    }
    if (id == kIdLaunchArgsEdit && notifyCode == EN_CHANGE) {
        updateLaunchCommandPreview();
        return true;
    }
    if (id == kIdLaunchProfileCombo && notifyCode == CBN_SELCHANGE) {
        updateLaunchCommandPreview();
        return true;
    }

    if (id == kIdLaunchBrowseBtn) {
        const std::wstring cur = trimW(getControlText(m_launchTargetEdit));
        const fs::path initDir = !cur.empty() ? fs::path(cur).parent_path() : fs::path{};
        if (const auto chosen = browseOpenTargetFile(m_hwnd, initDir)) {
            setLaunchTarget(*chosen);
        }
        return true;
    }

    if (id == kIdLaunchCopyCmdBtn) {
        const std::wstring cmd = getControlText(m_launchCmdEdit);
        if (!cmd.empty() && copyTextToClipboard(m_hwnd, cmd)) {
            setStatus(m_i18n.tr("launch", "copied_command"));
        }
        return true;
    }

    if (id == kIdLaunchAppProfileBtn) {
        const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
        if (targetText.empty()) {
            MessageBoxW(m_hwnd, m_i18n.tr("launch", "no_target").c_str(), m_i18n.tr("app", "info").c_str(), MB_OK | MB_ICONINFORMATION);
            return true;
        }
        const auto res = showAppProfileDialog(
            m_hwnd, m_appDir, m_configPath, m_config, m_i18n, m_locales, m_timezones, fs::path(targetText));
        if (res.changed) {
            refreshLaunchTab();
            if (findAppProfile(fs::path(targetText), m_config)) {
                SendMessageW(m_launchProfileCombo, CB_SETCURSEL, 0, 0);
                updateLaunchCommandPreview();
            }
            refreshProfilesTab();
        }
        return true;
    }

    if (id == kIdLaunchDeleteAppBtn) {
        const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
        if (targetText.empty()) {
            return true;
        }
        if (MessageBoxW(m_hwnd, m_i18n.tr("profiles", "confirm_delete_app").c_str(),
                m_i18n.tr("app", "confirm").c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            if (auto res = deleteAppProfileWithElevation(m_configPath, m_config, fs::path(targetText), m_hwnd); !res) {
                MessageBoxW(m_hwnd, res.error().c_str(), m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
            }
            else {
                setStatus(m_i18n.tr("profiles", "app_profile_deleted"));
                refreshLaunchTab();
                refreshProfilesTab();
            }
        }
        return true;
    }

    if (id == kIdLaunchRunBtn) {
        const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
        if (targetText.empty()) {
            MessageBoxW(m_hwnd, m_i18n.tr("launch", "no_target").c_str(), m_i18n.tr("app", "info").c_str(), MB_OK | MB_ICONINFORMATION);
            return true;
        }
        const fs::path targetPath(targetText);
        const auto appProf = findAppProfile(targetPath, m_config);
        const LRESULT sel = SendMessageW(m_launchProfileCombo, CB_GETCURSEL, 0, 0);

        Profile chosenProfile = createDefaultProfile();
        if (appProf && sel == 0) {
            chosenProfile = appProf->profile;
        }
        else {
            const size_t idx = appProf ? (size_t)(sel - 1) : (size_t)sel;
            if (idx < m_config.profiles.size()) {
                chosenProfile = m_config.profiles[idx];
            }
        }

        const auto launchRes = launchTarget(LaunchRequest{
            .appDir = m_appDir,
            .targetPath = targetPath,
            .profile = chosenProfile,
            .extraArguments = trimW(getControlText(m_launchArgsEdit)),
            .ownerWindow = m_hwnd,
        });
        if (!launchRes) {
            MessageBoxW(m_hwnd, (m_i18n.tr("launch", "launch_failed") + launchRes.error()).c_str(),
                m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
        }
        else {
            setStatus(m_i18n.tr("launch", "launch_ok"));
        }
        return true;
    }

    if (id == kIdLaunchShortcutBtn) {
        const std::wstring targetText = trimW(getControlText(m_launchTargetEdit));
        if (targetText.empty()) {
            MessageBoxW(m_hwnd, m_i18n.tr("launch", "no_target").c_str(), m_i18n.tr("app", "info").c_str(), MB_OK | MB_ICONINFORMATION);
            return true;
        }
        const fs::path targetPath(targetText);
        const fs::path suggested = suggestDefaultShortcutPath(targetPath);
        const auto chosenOut = browseSaveShortcutFile(m_hwnd, suggested);
        if (!chosenOut) {
            return true;
        }

        const auto appProf = findAppProfile(targetPath, m_config);
        const LRESULT sel = SendMessageW(m_launchProfileCombo, CB_GETCURSEL, 0, 0);
        std::optional<std::wstring> profileName;
        if (appProf) {
            if (sel > 0 && (size_t)(sel - 1) < m_config.profiles.size()) {
                profileName = m_config.profiles[(size_t)(sel - 1)].name;
            }
        }
        else if (sel >= 0 && (size_t)sel < m_config.profiles.size()) {
            profileName = m_config.profiles[(size_t)sel].name;
        }

        const auto res = createShortcut(ShortcutRequest{
            .lepExePath = getModuleFilePath(),
            .targetPath = targetPath,
            .shortcutOutputPath = *chosenOut,
            .profileName = profileName,
            .extraArguments = trimW(getControlText(m_launchArgsEdit)),
        });
        if (!res) {
            MessageBoxW(m_hwnd, (m_i18n.tr("launch", "shortcut_failed") + res.error()).c_str(),
                m_i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
        }
        else {
            setStatus(m_i18n.tr("launch", "shortcut_created") + res->wstring());
            MessageBoxW(m_hwnd, (m_i18n.tr("launch", "shortcut_created") + L"\n" + res->wstring()).c_str(),
                m_i18n.tr("app", "info").c_str(), MB_OK | MB_ICONINFORMATION);
        }
        return true;
    }

    return false;
}

NAMESPACE_END(lep::gui)
