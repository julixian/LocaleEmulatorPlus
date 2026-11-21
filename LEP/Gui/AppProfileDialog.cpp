module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

module LEP.Gui.AppProfileDialog;

import LEP.Gui.Win32Ui;
import LEP.ContextMenuInstaller;

NAMESPACE_BEGIN(lep::gui)

namespace
{
    enum ControlId
    {
        kIdLocaleCombo = 2001,
        kIdTimezoneCombo,
        kIdRegModeCombo,
        kIdUiModeCombo,
        kIdArgsEdit,
        kIdStdOverrideEdit,
        kIdDstOverrideEdit,
        kIdAdminCheck,
        kIdSuspendCheck,
        kIdStorageLocalRadio,
        kIdStorageGlobalRadio,
        kIdSaveBtn,
        kIdSaveRunBtn,
        kIdDeleteBtn,
        kIdCloseBtn,
        kIdStatusLabel,
    };

    struct AppDialogContext
    {
        fs::path appDir;
        fs::path globalConfigPath;
        GlobalConfig* pGlobalConfig = nullptr;
        const I18n* pI18n = nullptr;
        const std::vector<LocaleEntry>* pLocales = nullptr;
        const std::vector<TimezoneEntry>* pTimezones = nullptr;
        fs::path targetPath;

        Profile currentProfile;
        AppProfileStorage existingStorage = AppProfileStorage::None;
        DirectoryWriteStatus targetDirStatus = DirectoryWriteStatus::Writable;
        bool changed = false;

        HFONT font = nullptr;
        HWND localeCombo = nullptr;
        HWND tzCombo = nullptr;
        HWND regCombo = nullptr;
        HWND uiCombo = nullptr;
        HWND argsEdit = nullptr;
        HWND stdEdit = nullptr;
        HWND dstEdit = nullptr;
        HWND adminCheck = nullptr;
        HWND suspendCheck = nullptr;
        HWND localRadio = nullptr;
        HWND globalRadio = nullptr;
        HWND saveBtn = nullptr;
        HWND saveRunBtn = nullptr;
        HWND deleteBtn = nullptr;
        HWND statusLabel = nullptr;
    };

    void updateButtonShieldStates(const AppDialogContext& ctx)
    {
        const bool needsAdminDir = (ctx.targetDirStatus == DirectoryWriteStatus::NeedsAdmin);
        const bool localSelected = ctx.localRadio &&
            SendMessageW(ctx.localRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const bool saveNeedsAdmin = needsAdminDir && localSelected;
        const bool deleteNeedsAdmin = needsAdminDir && (ctx.existingStorage == AppProfileStorage::AlongsideTarget);

        if (ctx.saveBtn) {
            SendMessageW(ctx.saveBtn, BCM_SETSHIELD, 0, saveNeedsAdmin ? TRUE : FALSE);
        }
        if (ctx.saveRunBtn) {
            SendMessageW(ctx.saveRunBtn, BCM_SETSHIELD, 0, saveNeedsAdmin ? TRUE : FALSE);
        }
        if (ctx.deleteBtn) {
            SendMessageW(ctx.deleteBtn, BCM_SETSHIELD, 0, deleteNeedsAdmin ? TRUE : FALSE);
        }
    }

    std::expected<fs::path, std::wstring> saveAppProfileAlongsideElevated(
        const HWND hwnd,
        const fs::path& targetPath,
        const Profile& profile)
    {
        const fs::path sidePath = alongsideAppConfigPath(targetPath);
        wchar_t tempDir[MAX_PATH]{};
        if (GetTempPathW((DWORD)std::size(tempDir), tempDir) == 0) {
            return std::unexpected(L"无法获取系统临时目录。");
        }
        const fs::path tempFile = fs::path(tempDir) /
            std::format(L"lep_profile_{}_{}.lep.toml", GetCurrentProcessId(), GetTickCount());

        auto staged = saveAppProfileToFile(tempFile, profile, sidePath);
        if (!staged) {
            return std::unexpected(staged.error());
        }

        auto writeRes = writeAppProfileFileElevated(sidePath, tempFile, hwnd);
        std::error_code ec;
        fs::remove(tempFile, ec);
        if (!writeRes) {
            return std::unexpected(writeRes.error());
        }
        return sidePath;
    }

    Profile readProfileFromControls(const AppDialogContext& ctx)
    {
        Profile p = ctx.currentProfile;
        if (p.name.empty()) {
            p.name = ctx.targetPath.filename().wstring();
        }

        const LRESULT locIdx = SendMessageW(ctx.localeCombo, CB_GETCURSEL, 0, 0);
        if (locIdx >= 0 && (size_t)locIdx < ctx.pLocales->size()) {
            p.locale = (*ctx.pLocales)[(size_t)locIdx].localeName;
        }
        const LRESULT tzIdx = SendMessageW(ctx.tzCombo, CB_GETCURSEL, 0, 0);
        if (tzIdx >= 0 && (size_t)tzIdx < ctx.pTimezones->size()) {
            p.timezone = (*ctx.pTimezones)[(size_t)tzIdx].id;
        }
        const LRESULT regIdx = SendMessageW(ctx.regCombo, CB_GETCURSEL, 0, 0);
        if (regIdx >= 0) {
            p.registryRedirection = (int)regIdx;
        }
        const LRESULT uiIdx = SendMessageW(ctx.uiCombo, CB_GETCURSEL, 0, 0);
        if (uiIdx >= 0) {
            p.uiLanguageMode = (int)uiIdx;
        }

        p.arguments = trimW(getControlText(ctx.argsEdit));
        p.standardNameOverride = trimW(getControlText(ctx.stdEdit));
        p.daylightNameOverride = trimW(getControlText(ctx.dstEdit));
        p.runAsAdmin = SendMessageW(ctx.adminCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        p.runWithSuspend = SendMessageW(ctx.suspendCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        return p;
    }

    bool saveCurrentAppProfile(HWND hwnd, AppDialogContext& ctx, Profile& outSavedProfile)
    {
        const auto& i18n = *ctx.pI18n;
        outSavedProfile = readProfileFromControls(ctx);
        bool wantGlobal = SendMessageW(ctx.globalRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;

        if (!wantGlobal) {
            auto sideRes = saveAppProfileAlongside(ctx.targetPath, outSavedProfile);
            if (!sideRes) {
                if (ctx.targetDirStatus == DirectoryWriteStatus::NeedsAdmin && !isAdministrator()) {
                    // 受系统权限保护的普通本地目录：原地提权写入、改存全局、取消三选一（不关闭/不重启编辑窗口）
                    const int answer = MessageBoxW(
                        hwnd,
                        i18n.tr("launch", "admin_dir_ask").c_str(),
                        i18n.tr("launch", "admin_dir_title").c_str(),
                        MB_YESNOCANCEL | MB_ICONQUESTION);
                    if (answer == IDYES) {
                        sideRes = saveAppProfileAlongsideElevated(hwnd, ctx.targetPath, outSavedProfile);
                        if (!sideRes) {
                            MessageBoxW(hwnd, sideRes.error().c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
                            return false;
                        }
                    }
                    else if (answer == IDNO) {
                        wantGlobal = true;
                        SendMessageW(ctx.localRadio, BM_SETCHECK, BST_UNCHECKED, 0);
                        SendMessageW(ctx.globalRadio, BM_SETCHECK, BST_CHECKED, 0);
                        updateButtonShieldStates(ctx);
                    }
                    else {
                        return false;
                    }
                }
                else {
                    // 只读介质（ISO/光盘/只读卷）时，询问是否回退保存到全局配置文件
                    const int answer = MessageBoxW(
                        hwnd,
                        i18n.tr("launch", "readonly_dir_ask").c_str(),
                        i18n.tr("launch", "readonly_dir_title").c_str(),
                        MB_YESNO | MB_ICONQUESTION);
                    if (answer != IDYES) {
                        return false;
                    }
                    wantGlobal = true;
                    SendMessageW(ctx.localRadio, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendMessageW(ctx.globalRadio, BM_SETCHECK, BST_CHECKED, 0);
                    updateButtonShieldStates(ctx);
                }
            }
            if (!wantGlobal && sideRes) {
                // 若此前在全局配置里也存在同路径记录，清理旧记录避免两处不一致
                const std::wstring norm = normalizePathForMatch(ctx.targetPath);
                const size_t before = ctx.pGlobalConfig->appProfiles.size();
                std::erase_if(ctx.pGlobalConfig->appProfiles, [&](const AppProfileEntry& e)
                    {
                        return normalizePathForMatch(e.targetPath) == norm;
                    });
                if (ctx.pGlobalConfig->appProfiles.size() != before) {
                    (void)saveGlobalConfig(ctx.globalConfigPath, *ctx.pGlobalConfig);
                }
                ctx.existingStorage = AppProfileStorage::AlongsideTarget;
                ctx.currentProfile = outSavedProfile;
                ctx.changed = true;
                EnableWindow(ctx.deleteBtn, TRUE);
                updateButtonShieldStates(ctx);
                setControlText(ctx.statusLabel, i18n.tr("profiles", "saved_ok") + L" (" + sideRes->filename().wstring() + L")");
                return true;
            }
        }

        if (wantGlobal) {
            if (auto res = saveAppProfileInGlobal(ctx.globalConfigPath, *ctx.pGlobalConfig, ctx.targetPath, outSavedProfile); !res) {
                MessageBoxW(hwnd, res.error().c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
                return false;
            }
            // 若此前在同目录存在旧的 .lep.toml，尝试移除（权限不足时自动提权删除，避免旧文件覆盖全局配置）
            const fs::path oldSide = alongsideAppConfigPath(ctx.targetPath);
            std::error_code ec;
            if (fs::is_regular_file(oldSide, ec)) {
                if (!DeleteFileW(oldSide.c_str()) && GetLastError() == ERROR_ACCESS_DENIED && !isAdministrator()) {
                    (void)deleteAppProfileFileElevated(oldSide, hwnd);
                }
            }
            ctx.existingStorage = AppProfileStorage::GlobalConfig;
            ctx.currentProfile = outSavedProfile;
            ctx.changed = true;
            EnableWindow(ctx.deleteBtn, TRUE);
            updateButtonShieldStates(ctx);
            setControlText(ctx.statusLabel, i18n.tr("profiles", "saved_ok") + L" (config.toml)");
            return true;
        }
        return false;
    }

    LRESULT CALLBACK appProfileDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* pCtx = (AppDialogContext*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg)
        {
        case WM_CREATE:
        {
            const auto* pCs = (const CREATESTRUCTW*)lParam;
            pCtx = (AppDialogContext*)pCs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pCtx);

            const auto& i18n = *pCtx->pI18n;
            const UINT dpi = getWindowDpi(hwnd);
            pCtx->font = createUiFont(dpi);
            auto s = [dpi](int v) { return scaleDpi(v, dpi); };

            const int margin = s(16);
            const int labelW = s(135);
            const int fieldX = margin + labelW + s(8);
            const int fieldW = s(380);
            int y = margin;

            createLabel(hwnd, pCtx->font, i18n.tr("launch", "target_path"), margin, y + s(3), labelW, s(20));
            createEdit(hwnd, pCtx->font, pCtx->targetPath.wstring(), fieldX, y, fieldW, s(24), 0, ES_READONLY);
            y += s(34);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "locale"), margin, y + s(4), labelW, s(20));
            pCtx->localeCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(240), kIdLocaleCombo);
            int selectedLoc = 0;
            for (size_t i = 0; i < pCtx->pLocales->size(); ++i) {
                const auto& loc = (*pCtx->pLocales)[i];
                SendMessageW(pCtx->localeCombo, CB_ADDSTRING, 0, (LPARAM)loc.displayName.c_str());
                if (equalsIgnoreCaseW(loc.localeName, pCtx->currentProfile.locale)) {
                    selectedLoc = (int)i;
                }
            }
            SendMessageW(pCtx->localeCombo, CB_SETCURSEL, selectedLoc, 0);
            y += s(34);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "timezone"), margin, y + s(4), labelW, s(20));
            pCtx->tzCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(240), kIdTimezoneCombo);
            int selectedTz = 0;
            for (size_t i = 0; i < pCtx->pTimezones->size(); ++i) {
                const auto& tz = (*pCtx->pTimezones)[i];
                SendMessageW(pCtx->tzCombo, CB_ADDSTRING, 0, (LPARAM)tz.displayName.c_str());
                if (equalsIgnoreCaseW(tz.id, pCtx->currentProfile.timezone)) {
                    selectedTz = (int)i;
                }
            }
            SendMessageW(pCtx->tzCombo, CB_SETCURSEL, selectedTz, 0);
            y += s(34);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "reg_mode"), margin, y + s(4), labelW, s(20));
            pCtx->regCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(120), kIdRegModeCombo);
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_0").c_str());
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_1").c_str());
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_2").c_str());
            SendMessageW(pCtx->regCombo, CB_SETCURSEL, std::clamp(pCtx->currentProfile.registryRedirection, 0, 2), 0);
            y += s(34);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "ui_mode"), margin, y + s(4), labelW, s(20));
            pCtx->uiCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(120), kIdUiModeCombo);
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_0").c_str());
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_1").c_str());
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_2").c_str());
            SendMessageW(pCtx->uiCombo, CB_SETCURSEL, std::clamp(pCtx->currentProfile.uiLanguageMode, 0, 2), 0);
            y += s(34);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "arguments"), margin, y + s(4), labelW, s(20));
            pCtx->argsEdit = createEdit(hwnd, pCtx->font, pCtx->currentProfile.arguments, fieldX, y, fieldW, s(24), kIdArgsEdit);
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "tz_std_override"), margin, y + s(4), labelW, s(20));
            const int dstLabelW = s(96);
            const int gap = s(10);
            const int labelGap = s(4);
            const int stdEditW = (fieldW - gap - dstLabelW - labelGap) / 2;
            const int dstLabelX = fieldX + stdEditW + gap;
            const int dstEditX = dstLabelX + dstLabelW + labelGap;
            const int dstEditW = fieldW - (dstEditX - fieldX);
            pCtx->stdEdit = createEdit(hwnd, pCtx->font, pCtx->currentProfile.standardNameOverride, fieldX, y, stdEditW, s(24), kIdStdOverrideEdit);
            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "tz_dst_override"), dstLabelX, y + s(4), dstLabelW, s(20));
            pCtx->dstEdit = createEdit(hwnd, pCtx->font, pCtx->currentProfile.daylightNameOverride, dstEditX, y, dstEditW, s(24), kIdDstOverrideEdit);
            y += s(34);

            pCtx->adminCheck = createCheckbox(hwnd, pCtx->font, i18n.tr("profiles", "run_as_admin"), fieldX, y, s(185), s(22), kIdAdminCheck);
            SendMessageW(pCtx->adminCheck, BM_SETCHECK, pCtx->currentProfile.runAsAdmin ? BST_CHECKED : BST_UNCHECKED, 0);
            pCtx->suspendCheck = createCheckbox(hwnd, pCtx->font, i18n.tr("profiles", "run_with_suspend"), fieldX + s(190), y, s(210), s(22), kIdSuspendCheck);
            SendMessageW(pCtx->suspendCheck, BM_SETCHECK, pCtx->currentProfile.runWithSuspend ? BST_CHECKED : BST_UNCHECKED, 0);
            y += s(32);

            // 保存位置选择组（区分「需要管理员权限」与「只读目录/ISO」）
            const bool needsAdmin = (pCtx->targetDirStatus == DirectoryWriteStatus::NeedsAdmin);
            const bool isWritable = (pCtx->targetDirStatus == DirectoryWriteStatus::Writable);
            const int groupW = labelW + s(8) + fieldW;
            const int groupH = s(76);
            createGroupBox(hwnd, pCtx->font, i18n.tr("profiles", "storage_group"), margin, y, groupW, groupH);
            pCtx->localRadio = createRadio(hwnd, pCtx->font, i18n.tr("profiles", "storage_alongside"), margin + s(12), y + s(22), groupW - s(24), s(22), kIdStorageLocalRadio, true);
            pCtx->globalRadio = createRadio(hwnd, pCtx->font, i18n.tr("profiles", "storage_global"), margin + s(12), y + s(46), groupW - s(24), s(22), kIdStorageGlobalRadio);

            const bool defaultGlobal = (pCtx->existingStorage == AppProfileStorage::GlobalConfig) ||
                (pCtx->existingStorage == AppProfileStorage::None && !isWritable);
            SendMessageW(defaultGlobal ? pCtx->globalRadio : pCtx->localRadio, BM_SETCHECK, BST_CHECKED, 0);
            if (needsAdmin) {
                setControlText(pCtx->localRadio,
                    std::format(L"{} [{}]", i18n.tr("profiles", "storage_alongside"), i18n.tr("profiles", "tag_needs_admin")));
            }
            else if (!isWritable) {
                setControlText(pCtx->localRadio,
                    std::format(L"{} [{}]", i18n.tr("profiles", "storage_alongside"), i18n.tr("profiles", "tag_readonly")));
            }
            y += groupH + s(10);

            pCtx->statusLabel = createLabel(hwnd, pCtx->font, L"", margin, y, groupW, s(20), kIdStatusLabel);
            y += s(26);

            pCtx->deleteBtn = createButton(hwnd, pCtx->font, i18n.tr("launch", "delete_app_profile"), margin, y, s(162), s(30), kIdDeleteBtn);
            EnableWindow(pCtx->deleteBtn, pCtx->existingStorage != AppProfileStorage::None);

            pCtx->saveBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "save"), s(237), y, s(102), s(30), kIdSaveBtn, true);
            pCtx->saveRunBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "save_and_run"), s(347), y, s(106), s(30), kIdSaveRunBtn);
            createButton(hwnd, pCtx->font, i18n.tr("app", "close", L"关闭"), s(461), y, s(78), s(30), kIdCloseBtn);
            updateButtonShieldStates(*pCtx);
            return 0;
        }

        case WM_COMMAND:
        {
            if (!pCtx) {
                break;
            }
            const WORD id = LOWORD(wParam);
            const auto& i18n = *pCtx->pI18n;

            if (id == kIdStorageLocalRadio || id == kIdStorageGlobalRadio) {
                updateButtonShieldStates(*pCtx);
                return 0;
            }
            if (id == kIdSaveBtn) {
                Profile saved;
                saveCurrentAppProfile(hwnd, *pCtx, saved);
                return 0;
            }
            if (id == kIdSaveRunBtn) {
                Profile saved;
                if (saveCurrentAppProfile(hwnd, *pCtx, saved)) {
                    const auto res = launchTarget(LaunchRequest{
                        .appDir = pCtx->appDir,
                        .targetPath = pCtx->targetPath,
                        .profile = saved,
                        .ownerWindow = hwnd,
                    });
                    if (!res) {
                        MessageBoxW(hwnd, res.error().c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
                    }
                    else {
                        DestroyWindow(hwnd);
                    }
                }
                return 0;
            }
            if (id == kIdDeleteBtn) {
                if (MessageBoxW(hwnd, i18n.tr("profiles", "confirm_delete_app").c_str(),
                        i18n.tr("app", "confirm").c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    if (auto res = deleteAppProfileWithElevation(pCtx->globalConfigPath, *pCtx->pGlobalConfig, pCtx->targetPath, hwnd); !res) {
                        MessageBoxW(hwnd, res.error().c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
                    }
                    else {
                        pCtx->existingStorage = AppProfileStorage::None;
                        pCtx->changed = true;
                        EnableWindow(pCtx->deleteBtn, FALSE);
                        updateButtonShieldStates(*pCtx);
                        setControlText(pCtx->statusLabel, i18n.tr("profiles", "app_profile_deleted"));
                    }
                }
                return 0;
            }
            if (id == kIdCloseBtn || id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (pCtx && pCtx->font) {
                DeleteObject(pCtx->font);
                pCtx->font = nullptr;
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

AppProfileDialogResult showAppProfileDialog(
    const HWND owner,
    const fs::path& appDir,
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const I18n& i18n,
    const std::vector<LocaleEntry>& locales,
    const std::vector<TimezoneEntry>& timezones,
    const fs::path& targetPath)
{
    static bool s_registered = false;
    constexpr PCWSTR kClassName = L"LEP_AppProfileDialogClass";
    if (!s_registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = appProfileDlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kClassName;
        RegisterClassExW(&wc);
        s_registered = true;
    }

    std::error_code ec;
    const fs::path absTarget = fs::absolute(targetPath, ec);
    const fs::path finalTarget = absTarget.empty() ? targetPath : absTarget;

    AppDialogContext ctx{};
    ctx.appDir = appDir;
    ctx.globalConfigPath = globalConfigPath;
    ctx.pGlobalConfig = &globalConfig;
    ctx.pI18n = &i18n;
    ctx.pLocales = &locales;
    ctx.pTimezones = &timezones;
    ctx.targetPath = finalTarget;
    ctx.targetDirStatus = probeDirectoryWriteStatus(finalTarget.parent_path());

    if (const auto existing = findAppProfile(finalTarget, globalConfig)) {
        ctx.currentProfile = existing->profile;
        ctx.existingStorage = existing->storage;
    }
    else {
        ctx.currentProfile = !globalConfig.profiles.empty()
            ? globalConfig.profiles.front()
            : createDefaultProfile();
        ctx.currentProfile.name = finalTarget.filename().wstring();
    }

    const UINT dpi = getWindowDpi(owner);
    const int width = scaleDpi(572, dpi);
    const int height = scaleDpi(495, dpi);
    const std::wstring title = std::format(L"{} - {}",
        i18n.tr("profiles", "app_editor_title"), finalTarget.filename().wstring());

    if (owner) {
        EnableWindow(owner, FALSE);
    }

    const HWND dlg = CreateWindowExW(
        owner ? WS_EX_DLGMODALFRAME : 0,
        kClassName,
        title.c_str(),
        (owner ? WS_POPUP : WS_OVERLAPPED) | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        owner, nullptr, GetModuleHandleW(nullptr), &ctx);

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

    return AppProfileDialogResult{ .changed = ctx.changed };
}

NAMESPACE_END(lep::gui)
