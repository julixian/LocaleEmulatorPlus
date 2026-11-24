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
        kIdAppProfList = 2001,
        kIdAppProfAddBtn,
        kIdAppProfDupBtn,
        kIdAppProfDelBtn,
        kIdAppProfUpBtn,
        kIdAppProfDownBtn,
        kIdNameEdit,
        kIdLocaleCombo,
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

        std::vector<Profile> profiles;
        int selectedIndex = 0;
        bool suppressingFormEvents = false;
        AppProfileStorage existingStorage = AppProfileStorage::None;
        DirectoryWriteStatus targetDirStatus = DirectoryWriteStatus::Writable;
        bool changed = false;

        HFONT font = nullptr;
        HFONT boldFont = nullptr;
        HWND profList = nullptr;
        HWND delItemBtn = nullptr;
        HWND upItemBtn = nullptr;
        HWND downItemBtn = nullptr;
        HWND nameEdit = nullptr;
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

    std::wstring formatLocalTimeNow()
    {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        return std::format(L"{:02d}:{:02d}:{:02d}", st.wHour, st.wMinute, st.wSecond);
    }

    std::wstring makeUniqueProfileName(const std::vector<Profile>& list, const std::wstring& baseName)
    {
        auto existsFunc = [&](const std::wstring_view candidate) -> bool
            {
                for (const auto& p : list) {
                    if (equalsIgnoreCaseW(p.name, candidate)) {
                        return true;
                    }
                }
                return false;
            };
        if (!existsFunc(baseName)) {
            return baseName;
        }
        for (int n = 2; n < 1000; ++n) {
            const std::wstring candidate = std::format(L"{} ({})", baseName, n);
            if (!existsFunc(candidate)) {
                return candidate;
            }
        }
        return baseName + L" (new)";
    }

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

    void commitFormToSelected(AppDialogContext& ctx)
    {
        if (ctx.suppressingFormEvents || ctx.selectedIndex < 0 || ctx.selectedIndex >= (int)ctx.profiles.size()) {
            return;
        }
        Profile& p = ctx.profiles[(size_t)ctx.selectedIndex];
        p.name = trimW(getControlText(ctx.nameEdit));

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
    }

    void loadSelectedToForm(AppDialogContext& ctx)
    {
        if (ctx.selectedIndex < 0 || ctx.selectedIndex >= (int)ctx.profiles.size()) {
            return;
        }
        ctx.suppressingFormEvents = true;
        const Profile& p = ctx.profiles[(size_t)ctx.selectedIndex];

        setControlText(ctx.nameEdit, p.name);

        int selectedLoc = 0;
        for (size_t i = 0; i < ctx.pLocales->size(); ++i) {
            if (equalsIgnoreCaseW((*ctx.pLocales)[i].localeName, p.locale)) {
                selectedLoc = (int)i;
                break;
            }
        }
        SendMessageW(ctx.localeCombo, CB_SETCURSEL, selectedLoc, 0);

        int selectedTz = 0;
        for (size_t i = 0; i < ctx.pTimezones->size(); ++i) {
            if (equalsIgnoreCaseW((*ctx.pTimezones)[i].id, p.timezone)) {
                selectedTz = (int)i;
                break;
            }
        }
        SendMessageW(ctx.tzCombo, CB_SETCURSEL, selectedTz, 0);

        SendMessageW(ctx.regCombo, CB_SETCURSEL, std::clamp(p.registryRedirection, 0, 2), 0);
        SendMessageW(ctx.uiCombo, CB_SETCURSEL, std::clamp(p.uiLanguageMode, 0, 2), 0);
        setControlText(ctx.argsEdit, p.arguments);
        setControlText(ctx.stdEdit, p.standardNameOverride);
        setControlText(ctx.dstEdit, p.daylightNameOverride);
        SendMessageW(ctx.adminCheck, BM_SETCHECK, p.runAsAdmin ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(ctx.suspendCheck, BM_SETCHECK, p.runWithSuspend ? BST_CHECKED : BST_UNCHECKED, 0);
        ctx.suppressingFormEvents = false;
    }

    void refreshProfileListBox(AppDialogContext& ctx)
    {
        if (!ctx.profList) {
            return;
        }
        SendMessageW(ctx.profList, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < ctx.profiles.size(); ++i) {
            const std::wstring label = ctx.profiles[i].name.empty()
                ? std::format(L"#{}", i + 1)
                : ctx.profiles[i].name;
            SendMessageW(ctx.profList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
        }
        if (ctx.profiles.empty()) {
            ctx.selectedIndex = -1;
        }
        else {
            ctx.selectedIndex = std::clamp(ctx.selectedIndex, 0, (int)ctx.profiles.size() - 1);
            SendMessageW(ctx.profList, LB_SETCURSEL, ctx.selectedIndex, 0);
        }
        if (ctx.delItemBtn) EnableWindow(ctx.delItemBtn, ctx.profiles.size() > 1);
        if (ctx.upItemBtn) EnableWindow(ctx.upItemBtn, ctx.selectedIndex > 0);
        if (ctx.downItemBtn) EnableWindow(ctx.downItemBtn, ctx.selectedIndex >= 0 && ctx.selectedIndex + 1 < (int)ctx.profiles.size());
    }

    std::expected<fs::path, std::wstring> saveAppProfilesAlongsideElevated(
        const HWND hwnd,
        const fs::path& targetPath,
        const std::span<const Profile> profiles)
    {
        const fs::path sidePath = alongsideAppConfigPath(targetPath);
        wchar_t tempDir[MAX_PATH]{};
        if (GetTempPathW((DWORD)std::size(tempDir), tempDir) == 0) {
            return std::unexpected(L"无法获取系统临时目录。");
        }
        const fs::path tempFile = fs::path(tempDir) /
            std::format(L"lep_profile_{}_{}.lep.toml", GetCurrentProcessId(), GetTickCount());

        auto staged = saveAppProfilesToFile(tempFile, profiles, sidePath);
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

    bool validateProfiles(const HWND hwnd, const AppDialogContext& ctx)
    {
        const auto& i18n = *ctx.pI18n;
        for (size_t i = 0; i < ctx.profiles.size(); ++i) {
            const std::wstring name = trimW(ctx.profiles[i].name);
            if (name.empty()) {
                MessageBoxW(hwnd, i18n.tr("profiles", "name_empty").c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
                return false;
            }
            for (size_t j = i + 1; j < ctx.profiles.size(); ++j) {
                if (equalsIgnoreCaseW(name, trimW(ctx.profiles[j].name))) {
                    MessageBoxW(hwnd, i18n.tr("profiles", "name_duplicate").c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONWARNING);
                    return false;
                }
            }
        }
        return !ctx.profiles.empty();
    }

    bool saveCurrentAppProfiles(HWND hwnd, AppDialogContext& ctx, Profile& outActiveProfile)
    {
        const auto& i18n = *ctx.pI18n;
        commitFormToSelected(ctx);
        refreshProfileListBox(ctx);
        if (!validateProfiles(hwnd, ctx)) {
            return false;
        }
        outActiveProfile = ctx.profiles[(size_t)std::clamp(ctx.selectedIndex, 0, (int)ctx.profiles.size() - 1)];

        bool wantGlobal = SendMessageW(ctx.globalRadio, BM_GETCHECK, 0, 0) == BST_CHECKED;

        if (!wantGlobal) {
            auto sideRes = saveAppProfilesAlongside(ctx.targetPath, ctx.profiles);
            if (!sideRes) {
                if (ctx.targetDirStatus == DirectoryWriteStatus::NeedsAdmin && !isAdministrator()) {
                    const int answer = MessageBoxW(
                        hwnd,
                        i18n.tr("launch", "admin_dir_ask").c_str(),
                        i18n.tr("launch", "admin_dir_title").c_str(),
                        MB_YESNOCANCEL | MB_ICONQUESTION);
                    if (answer == IDYES) {
                        sideRes = saveAppProfilesAlongsideElevated(hwnd, ctx.targetPath, ctx.profiles);
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
                ctx.changed = true;
                EnableWindow(ctx.deleteBtn, TRUE);
                updateButtonShieldStates(ctx);
                setControlText(ctx.statusLabel,
                    std::format(L"{} ({} · {})", i18n.tr("profiles", "saved_ok"), sideRes->filename().wstring(), formatLocalTimeNow()));
                return true;
            }
        }

        if (wantGlobal) {
            if (auto res = saveAppProfilesInGlobal(ctx.globalConfigPath, *ctx.pGlobalConfig, ctx.targetPath, ctx.profiles); !res) {
                MessageBoxW(hwnd, res.error().c_str(), i18n.tr("app", "error").c_str(), MB_OK | MB_ICONERROR);
                return false;
            }
            const fs::path oldSide = alongsideAppConfigPath(ctx.targetPath);
            std::error_code ec;
            if (fs::is_regular_file(oldSide, ec)) {
                if (!DeleteFileW(oldSide.c_str()) && GetLastError() == ERROR_ACCESS_DENIED && !isAdministrator()) {
                    (void)deleteAppProfileFileElevated(oldSide, hwnd);
                }
            }
            ctx.existingStorage = AppProfileStorage::GlobalConfig;
            ctx.changed = true;
            EnableWindow(ctx.deleteBtn, TRUE);
            updateButtonShieldStates(ctx);
            setControlText(ctx.statusLabel,
                std::format(L"{} (config.toml · {})", i18n.tr("profiles", "saved_ok"), formatLocalTimeNow()));
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
            pCtx->font = createUiFont(dpi, 9, false);
            pCtx->boldFont = createUiFont(dpi, 9, true);
            auto s = [dpi](int v) { return scaleDpi(v, dpi); };

            const int margin = s(14);
            const int leftW = s(195);
            const int rightX = margin + leftW + s(14);
            const int labelW = s(126);
            const int fieldX = rightX + labelW + s(6);
            const int fieldW = s(386);
            const int rightTotalW = labelW + s(6) + fieldW;

            // 左侧：该程序的多个专属配置列表（底部与右侧保存位置框齐平）
            createLabel(hwnd, pCtx->boldFont, i18n.tr("profiles", "app_profiles_list", L"此程序的专属配置列表"), margin, margin, leftW, s(20));
            pCtx->profList = createListBox(hwnd, pCtx->font, margin, margin + s(24), leftW, s(272), kIdAppProfList);

            const int listBtnY1 = margin + s(302);
            createButton(hwnd, pCtx->font, i18n.tr("profiles", "add"), margin, listBtnY1, s(61), s(26), kIdAppProfAddBtn);
            createButton(hwnd, pCtx->font, i18n.tr("profiles", "duplicate"), margin + s(67), listBtnY1, s(61), s(26), kIdAppProfDupBtn);
            pCtx->delItemBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "delete"), margin + s(134), listBtnY1, s(61), s(26), kIdAppProfDelBtn);

            const int listBtnY2 = listBtnY1 + s(32);
            pCtx->upItemBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "move_up"), margin, listBtnY2, s(94), s(26), kIdAppProfUpBtn);
            pCtx->downItemBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "move_down"), margin + s(101), listBtnY2, s(94), s(26), kIdAppProfDownBtn);

            // 右侧：当前选中的专属配置编辑表单
            int y = margin;
            createLabel(hwnd, pCtx->font, i18n.tr("launch", "target_path"), rightX, y + s(3), labelW, s(20));
            createEdit(hwnd, pCtx->font, pCtx->targetPath.wstring(), fieldX, y, fieldW, s(24), 0, ES_READONLY);
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "name"), rightX, y + s(3), labelW, s(20));
            pCtx->nameEdit = createEdit(hwnd, pCtx->font, L"", fieldX, y, fieldW, s(24), kIdNameEdit);
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "locale"), rightX, y + s(4), labelW, s(20));
            pCtx->localeCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(240), kIdLocaleCombo);
            for (const auto& loc : *pCtx->pLocales) {
                SendMessageW(pCtx->localeCombo, CB_ADDSTRING, 0, (LPARAM)loc.displayName.c_str());
            }
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "timezone"), rightX, y + s(4), labelW, s(20));
            pCtx->tzCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(240), kIdTimezoneCombo);
            for (const auto& tz : *pCtx->pTimezones) {
                SendMessageW(pCtx->tzCombo, CB_ADDSTRING, 0, (LPARAM)tz.displayName.c_str());
            }
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "reg_mode"), rightX, y + s(4), labelW, s(20));
            pCtx->regCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(120), kIdRegModeCombo);
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_0").c_str());
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_1").c_str());
            SendMessageW(pCtx->regCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "reg_mode_2").c_str());
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "ui_mode"), rightX, y + s(4), labelW, s(20));
            pCtx->uiCombo = createComboBox(hwnd, pCtx->font, fieldX, y, fieldW, s(120), kIdUiModeCombo);
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_0").c_str());
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_1").c_str());
            SendMessageW(pCtx->uiCombo, CB_ADDSTRING, 0, (LPARAM)i18n.tr("profiles", "ui_mode_2").c_str());
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "arguments"), rightX, y + s(3), labelW, s(20));
            pCtx->argsEdit = createEdit(hwnd, pCtx->font, L"", fieldX, y, fieldW, s(24), kIdArgsEdit);
            y += s(32);

            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "tz_std_override"), rightX, y + s(3), labelW, s(20));
            const int dstLabelW = s(96);
            const int gap = s(10);
            const int labelGap = s(4);
            const int stdEditW = (fieldW - gap - dstLabelW - labelGap) / 2;
            const int dstLabelX = fieldX + stdEditW + gap;
            const int dstEditX = dstLabelX + dstLabelW + labelGap;
            const int dstEditW = fieldW - (dstEditX - fieldX);
            pCtx->stdEdit = createEdit(hwnd, pCtx->font, L"", fieldX, y, stdEditW, s(24), kIdStdOverrideEdit);
            createLabel(hwnd, pCtx->font, i18n.tr("profiles", "tz_dst_override"), dstLabelX, y + s(3), dstLabelW, s(20));
            pCtx->dstEdit = createEdit(hwnd, pCtx->font, L"", dstEditX, y, dstEditW, s(24), kIdDstOverrideEdit);
            y += s(32);

            pCtx->adminCheck = createCheckbox(hwnd, pCtx->font, i18n.tr("profiles", "run_as_admin"), fieldX, y, s(180), s(22), kIdAdminCheck);
            pCtx->suspendCheck = createCheckbox(hwnd, pCtx->font, i18n.tr("profiles", "run_with_suspend"), fieldX + s(186), y, s(210), s(22), kIdSuspendCheck);
            y += s(30);

            const bool needsAdmin = (pCtx->targetDirStatus == DirectoryWriteStatus::NeedsAdmin);
            const bool isWritable = (pCtx->targetDirStatus == DirectoryWriteStatus::Writable);
            const int groupH = s(74);
            createGroupBox(hwnd, pCtx->font, i18n.tr("profiles", "storage_group"), rightX, y, rightTotalW, groupH);
            pCtx->localRadio = createRadio(hwnd, pCtx->font, i18n.tr("profiles", "storage_alongside"), rightX + s(12), y + s(22), rightTotalW - s(24), s(22), kIdStorageLocalRadio, true);
            pCtx->globalRadio = createRadio(hwnd, pCtx->font, i18n.tr("profiles", "storage_global"), rightX + s(12), y + s(46), rightTotalW - s(24), s(22), kIdStorageGlobalRadio);

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

            const int fullW = leftW + s(14) + rightTotalW;
            const int bottomStatusY = std::max(y, listBtnY2 + s(34));
            pCtx->statusLabel = createLabel(hwnd, pCtx->font, L"", margin, bottomStatusY, fullW, s(20), kIdStatusLabel);
            const int btnRowY = bottomStatusY + s(26);

            pCtx->deleteBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "delete_all_app_profiles", i18n.tr("launch", "delete_app_profile")),
                margin, btnRowY, s(172), s(30), kIdDeleteBtn);
            EnableWindow(pCtx->deleteBtn, pCtx->existingStorage != AppProfileStorage::None);

            const int closeX = margin + fullW - s(80);
            const int saveRunX = closeX - s(112) - s(8);
            const int saveX = saveRunX - s(106) - s(8);
            pCtx->saveBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "save"), saveX, btnRowY, s(106), s(30), kIdSaveBtn, true);
            pCtx->saveRunBtn = createButton(hwnd, pCtx->font, i18n.tr("profiles", "save_and_run"), saveRunX, btnRowY, s(112), s(30), kIdSaveRunBtn);
            createButton(hwnd, pCtx->font, i18n.tr("app", "close", L"关闭"), closeX, btnRowY, s(80), s(30), kIdCloseBtn);

            refreshProfileListBox(*pCtx);
            loadSelectedToForm(*pCtx);
            updateButtonShieldStates(*pCtx);
            return 0;
        }

        case WM_COMMAND:
        {
            if (!pCtx) {
                break;
            }
            const WORD id = LOWORD(wParam);
            const WORD code = HIWORD(wParam);
            const auto& i18n = *pCtx->pI18n;

            if (id == kIdAppProfList && code == LBN_SELCHANGE) {
                const int newIdx = (int)SendMessageW(pCtx->profList, LB_GETCURSEL, 0, 0);
                if (newIdx >= 0 && newIdx < (int)pCtx->profiles.size() && newIdx != pCtx->selectedIndex) {
                    commitFormToSelected(*pCtx);
                    pCtx->selectedIndex = newIdx;
                    refreshProfileListBox(*pCtx);
                    loadSelectedToForm(*pCtx);
                }
                return 0;
            }
            if (id == kIdNameEdit && code == EN_CHANGE && !pCtx->suppressingFormEvents) {
                if (pCtx->selectedIndex >= 0 && pCtx->selectedIndex < (int)pCtx->profiles.size()) {
                    pCtx->profiles[(size_t)pCtx->selectedIndex].name = trimW(getControlText(pCtx->nameEdit));
                    const std::wstring label = pCtx->profiles[(size_t)pCtx->selectedIndex].name.empty()
                        ? std::format(L"#{}", pCtx->selectedIndex + 1)
                        : pCtx->profiles[(size_t)pCtx->selectedIndex].name;
                    SendMessageW(pCtx->profList, LB_DELETESTRING, pCtx->selectedIndex, 0);
                    SendMessageW(pCtx->profList, LB_INSERTSTRING, pCtx->selectedIndex, (LPARAM)label.c_str());
                    SendMessageW(pCtx->profList, LB_SETCURSEL, pCtx->selectedIndex, 0);
                }
                return 0;
            }
            if (id == kIdAppProfAddBtn) {
                commitFormToSelected(*pCtx);
                Profile p = !pCtx->pGlobalConfig->profiles.empty()
                    ? pCtx->pGlobalConfig->profiles.front()
                    : createDefaultProfile();
                p.name = makeUniqueProfileName(pCtx->profiles, i18n.tr("profiles", "new_profile_default", L"新配置"));
                pCtx->profiles.push_back(std::move(p));
                pCtx->selectedIndex = (int)pCtx->profiles.size() - 1;
                refreshProfileListBox(*pCtx);
                loadSelectedToForm(*pCtx);
                return 0;
            }
            if (id == kIdAppProfDupBtn) {
                if (pCtx->selectedIndex >= 0 && pCtx->selectedIndex < (int)pCtx->profiles.size()) {
                    commitFormToSelected(*pCtx);
                    Profile copy = pCtx->profiles[(size_t)pCtx->selectedIndex];
                    copy.name = makeUniqueProfileName(pCtx->profiles, copy.name.empty() ? i18n.tr("profiles", "new_profile_default", L"新配置") : copy.name);
                    pCtx->profiles.push_back(std::move(copy));
                    pCtx->selectedIndex = (int)pCtx->profiles.size() - 1;
                    refreshProfileListBox(*pCtx);
                    loadSelectedToForm(*pCtx);
                }
                return 0;
            }
            if (id == kIdAppProfDelBtn) {
                if (pCtx->profiles.size() > 1 && pCtx->selectedIndex >= 0 && pCtx->selectedIndex < (int)pCtx->profiles.size()) {
                    pCtx->profiles.erase(pCtx->profiles.begin() + pCtx->selectedIndex);
                    if (pCtx->selectedIndex >= (int)pCtx->profiles.size()) {
                        pCtx->selectedIndex = (int)pCtx->profiles.size() - 1;
                    }
                    refreshProfileListBox(*pCtx);
                    loadSelectedToForm(*pCtx);
                }
                return 0;
            }
            if (id == kIdAppProfUpBtn || id == kIdAppProfDownBtn) {
                if (pCtx->selectedIndex >= 0 && pCtx->selectedIndex < (int)pCtx->profiles.size()) {
                    commitFormToSelected(*pCtx);
                    const int target = (id == kIdAppProfUpBtn) ? (pCtx->selectedIndex - 1) : (pCtx->selectedIndex + 1);
                    if (target >= 0 && target < (int)pCtx->profiles.size()) {
                        std::swap(pCtx->profiles[(size_t)pCtx->selectedIndex], pCtx->profiles[(size_t)target]);
                        pCtx->selectedIndex = target;
                        refreshProfileListBox(*pCtx);
                        loadSelectedToForm(*pCtx);
                    }
                }
                return 0;
            }
            if (id == kIdStorageLocalRadio || id == kIdStorageGlobalRadio) {
                updateButtonShieldStates(*pCtx);
                return 0;
            }
            if (id == kIdSaveBtn) {
                Profile active;
                saveCurrentAppProfiles(hwnd, *pCtx, active);
                return 0;
            }
            if (id == kIdSaveRunBtn) {
                Profile active;
                if (saveCurrentAppProfiles(hwnd, *pCtx, active)) {
                    const auto res = launchTarget(LaunchRequest{
                        .appDir = pCtx->appDir,
                        .targetPath = pCtx->targetPath,
                        .profile = active,
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
            if (pCtx) {
                if (pCtx->font) {
                    DeleteObject(pCtx->font);
                    pCtx->font = nullptr;
                }
                if (pCtx->boldFont) {
                    DeleteObject(pCtx->boldFont);
                    pCtx->boldFont = nullptr;
                }
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

    if (const auto existing = findAppProfiles(finalTarget, globalConfig); existing && !existing->profiles.empty()) {
        ctx.profiles = existing->profiles;
        ctx.existingStorage = existing->storage;
    }
    else {
        Profile def = !globalConfig.profiles.empty()
            ? globalConfig.profiles.front()
            : createDefaultProfile();
        def.name = i18n.tr("profiles", "default_app_profile_name", L"默认配置");
        ctx.profiles.push_back(std::move(def));
    }

    const UINT dpi = getWindowDpi(owner);
    const DWORD exStyle = owner ? WS_EX_DLGMODALFRAME : 0;
    const DWORD style = (owner ? WS_POPUP : WS_OVERLAPPED) | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    RECT rc{ 0, 0, scaleDpi(755, dpi), scaleDpi(458, dpi) };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    const int width = std::max<int>(rc.right - rc.left, scaleDpi(775, dpi));
    const int height = std::max<int>(rc.bottom - rc.top, scaleDpi(504, dpi));
    const std::wstring title = std::format(L"{} - {}",
        i18n.tr("profiles", "app_editor_title"), finalTarget.filename().wstring());

    if (owner) {
        EnableWindow(owner, FALSE);
    }

    const HWND dlg = CreateWindowExW(
        exStyle,
        kClassName,
        title.c_str(),
        style,
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
