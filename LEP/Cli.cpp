module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <shellapi.h>

module LEP.Cli;

import LEPCommon;
import LEP.ContextMenuInstaller;

NAMESPACE_BEGIN(lep)

namespace
{
    void writeConsoleOrMessageBox(const std::wstring& title, const std::wstring& body, const bool isError = false)
    {
        auto tryWriteHandleFunc = [&](const HANDLE hOut) -> bool
            {
                if (!hOut || hOut == INVALID_HANDLE_VALUE) {
                    return false;
                }
                const DWORD fileType = GetFileType(hOut);
                if (fileType == FILE_TYPE_UNKNOWN && GetLastError() != NO_ERROR) {
                    return false;
                }
                DWORD mode = 0;
                if (fileType == FILE_TYPE_CHAR && GetConsoleMode(hOut, &mode)) {
                    DWORD written = 0;
                    const std::wstring line = body + L"\r\n";
                    return WriteConsoleW(hOut, line.c_str(), (DWORD)line.size(), &written, nullptr) != FALSE;
                }
                if (fileType == FILE_TYPE_PIPE || fileType == FILE_TYPE_DISK) {
                    const std::string utf8 = wide2Utf8(body + L"\n");
                    DWORD written = 0;
                    return WriteFile(hOut, utf8.data(), (DWORD)utf8.size(), &written, nullptr) != FALSE;
                }
                return false;
            };

        const DWORD stdId = isError ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
        if (tryWriteHandleFunc(GetStdHandle(stdId))) {
            return;
        }

        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            const bool wrote = tryWriteHandleFunc(GetStdHandle(stdId));
            FreeConsole();
            if (wrote) {
                return;
            }
        }

        MessageBoxW(nullptr, body.c_str(), title.c_str(),
            MB_OK | (isError ? MB_ICONERROR : MB_ICONINFORMATION));
    }

    std::wstring buildHelpText()
    {
        return L"Locale Emulator Plus " LEP_APP_VERSION_WSTR L"\n\n"
            L"用法：\n"
            L"  LEP.exe\n"
            L"      打开图形界面。\n\n"
            L"  LEP.exe <目标程序>\n"
            L"      按优先级运行（程序专属配置 > 第一个全局配置 > 默认日区）。\n\n"
            L"  LEP.exe run <目标程序> [选项] [-- 透传参数...]\n"
            L"      --profile <名称>      使用指定名称的全局配置\n"
            L"      --locale <区域名>     临时覆盖区域（如 ja-JP、zh-TW）\n"
            L"      --tz <时区ID>         临时覆盖时区（如 \"Tokyo Standard Time\"）\n"
            L"      --reg <0|1|2>         注册表重定向模式\n"
            L"      --ui <0|1|2>          界面语言模拟模式\n"
            L"      --admin               以管理员权限运行\n"
            L"      --suspend             以 CREATE_SUSPENDED 挂起创建\n"
            L"      --wait                等待目标进程退出并返回其退出码\n\n"
            L"  LEP.exe manage <目标程序>\n"
            L"      打开该程序的专属配置编辑窗口。\n\n"
            L"  LEP.exe shortcut <目标程序> [--profile <名称>] [--out <lnk路径>] [-- 透传参数...]\n"
            L"      直接创建桌面或指定路径的快捷方式。\n\n"
            L"  LEP.exe install [--all-users] [--win11]\n"
            L"  LEP.exe uninstall [--all-users] [--win11]\n"
            L"      安装或卸载右键菜单。\n\n"
            L"  LEP.exe list-profiles\n"
            L"      列出当前全部全局配置名称。\n";
    }

    int executeRunCommand(const std::vector<std::wstring>& args, const size_t startIndex, const fs::path& appDir)
    {
        const fs::path cfgPath = resolveGlobalConfigPath(appDir);
        const auto cfgExp = loadOrCreateGlobalConfig(cfgPath);
        const GlobalConfig cfg = cfgExp ? *cfgExp : createDefaultGlobalConfig();

        fs::path targetPath;
        std::optional<std::wstring> profileName;
        std::optional<std::wstring> overrideLocale;
        std::optional<std::wstring> overrideTz;
        std::optional<int> overrideReg;
        std::optional<int> overrideUi;
        std::optional<bool> overrideAdmin;
        std::optional<bool> overrideSuspend;
        bool waitForExit = false;
        std::wstring extraArgs;

        for (size_t i = startIndex; i < args.size(); ++i) {
            const auto& arg = args[i];
            if (arg == L"--") {
                for (size_t j = i + 1; j < args.size(); ++j) {
                    if (!extraArgs.empty()) {
                        extraArgs.push_back(L' ');
                    }
                    extraArgs += quoteCommandLineArg(args[j]);
                }
                break;
            }
            if ((arg == L"--profile" || arg == L"-p") && i + 1 < args.size()) {
                profileName = args[++i];
            }
            else if (arg == L"--locale" && i + 1 < args.size()) {
                overrideLocale = args[++i];
            }
            else if ((arg == L"--tz" || arg == L"--timezone") && i + 1 < args.size()) {
                overrideTz = args[++i];
            }
            else if (arg == L"--reg" && i + 1 < args.size()) {
                overrideReg = std::clamp((int)wcstol(args[++i].c_str(), nullptr, 10), 0, 2);
            }
            else if (arg == L"--ui" && i + 1 < args.size()) {
                overrideUi = std::clamp((int)wcstol(args[++i].c_str(), nullptr, 10), 0, 2);
            }
            else if (arg == L"--admin") {
                overrideAdmin = true;
            }
            else if (arg == L"--suspend") {
                overrideSuspend = true;
            }
            else if (arg == L"--wait") {
                waitForExit = true;
            }
            else if (targetPath.empty()) {
                targetPath = fs::path(arg);
            }
            else {
                if (!extraArgs.empty()) {
                    extraArgs.push_back(L' ');
                }
                extraArgs += quoteCommandLineArg(arg);
            }
        }

        if (targetPath.empty()) {
            writeConsoleOrMessageBox(L"Locale Emulator Plus", L"缺少目标程序路径。\n\n" + buildHelpText(), true);
            return 2;
        }

        Profile activeProfile = createDefaultProfile();
        if (profileName) {
            const auto found = findGlobalProfileByName(cfg, *profileName);
            if (!found) {
                writeConsoleOrMessageBox(L"Locale Emulator Plus",
                    std::format(L"找不到名为「{}」的全局配置。", *profileName), true);
                return 3;
            }
            activeProfile = *found;
        }
        else if (const auto appProf = findAppProfile(targetPath, cfg)) {
            activeProfile = appProf->profile;
        }
        else if (!cfg.profiles.empty()) {
            activeProfile = cfg.profiles.front();
        }

        if (overrideLocale) activeProfile.locale = *overrideLocale;
        if (overrideTz) activeProfile.timezone = *overrideTz;
        if (overrideReg) activeProfile.registryRedirection = *overrideReg;
        if (overrideUi) activeProfile.uiLanguageMode = *overrideUi;
        if (overrideAdmin) activeProfile.runAsAdmin = *overrideAdmin;
        if (overrideSuspend) activeProfile.runWithSuspend = *overrideSuspend;

        const auto launchRes = launchTarget(LaunchRequest{
            .appDir = appDir,
            .targetPath = targetPath,
            .profile = activeProfile,
            .extraArguments = extraArgs,
            .waitForExit = waitForExit,
            .confirmSuspended = !waitForExit,
        });
        if (!launchRes) {
            writeConsoleOrMessageBox(L"Locale Emulator Plus", launchRes.error(), true);
            return 1;
        }
        return waitForExit ? (int)launchRes->exitCode : 0;
    }

    int executeShortcutCommand(const std::vector<std::wstring>& args, const fs::path& appDir)
    {
        fs::path targetPath;
        fs::path outPath;
        std::optional<std::wstring> profileName;
        std::wstring extraArgs;

        for (size_t i = 2; i < args.size(); ++i) {
            const auto& arg = args[i];
            if (arg == L"--") {
                for (size_t j = i + 1; j < args.size(); ++j) {
                    if (!extraArgs.empty()) extraArgs.push_back(L' ');
                    extraArgs += quoteCommandLineArg(args[j]);
                }
                break;
            }
            if ((arg == L"--profile" || arg == L"-p") && i + 1 < args.size()) {
                profileName = args[++i];
            }
            else if ((arg == L"--out" || arg == L"-o") && i + 1 < args.size()) {
                outPath = fs::path(args[++i]);
            }
            else if (targetPath.empty()) {
                targetPath = fs::path(arg);
            }
        }

        if (targetPath.empty()) {
            writeConsoleOrMessageBox(L"Locale Emulator Plus", L"shortcut 命令缺少目标程序路径。", true);
            return 2;
        }

        std::error_code ec;
        const fs::path absTarget = fs::absolute(targetPath, ec);
        if (outPath.empty()) {
            outPath = suggestDefaultShortcutPath(absTarget.empty() ? targetPath : absTarget);
        }

        const auto res = createShortcut(ShortcutRequest{
            .lepExePath = getModuleFilePath(),
            .targetPath = absTarget.empty() ? targetPath : absTarget,
            .shortcutOutputPath = outPath,
            .profileName = profileName,
            .extraArguments = extraArgs,
        });
        if (!res) {
            writeConsoleOrMessageBox(L"Locale Emulator Plus", res.error(), true);
            return 1;
        }
        writeConsoleOrMessageBox(L"Locale Emulator Plus", L"快捷方式已创建：\n" + res->wstring(), false);
        return 0;
    }
}

CliDispatchResult dispatchCommandLine(const std::vector<std::wstring>& args, const fs::path& appDir)
{
    if (args.size() <= 1) {
        return CliDispatchResult{ .guiMode = GuiStartMode::MainWindow };
    }

    const std::wstring cmd = toLowerW(args[1]);

    if (cmd == L"--help" || cmd == L"-h" || cmd == L"help" || cmd == L"/?") {
        writeConsoleOrMessageBox(L"Locale Emulator Plus", buildHelpText(), false);
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 0 };
    }

    if (cmd == L"--version" || cmd == L"-v" || cmd == L"version") {
        writeConsoleOrMessageBox(L"Locale Emulator Plus", L"Locale Emulator Plus " LEP_APP_VERSION_WSTR, false);
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 0 };
    }

    if (cmd == L"--trust-cert" && args.size() >= 3) {
        const auto res = importCertificateToTrustedPeople(fs::path(args[2]));
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = res ? 0 : 1 };
    }

    if (cmd == L"--untrust-cert" && args.size() >= 3) {
        const auto res = removeCertificateFromTrustedPeople(fs::path(args[2]));
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = res ? 0 : 1 };
    }

    if (cmd == L"--write-profile-file" && args.size() >= 4) {
        const auto res = executeElevatedWriteAppProfileFile(fs::path(args[2]), fs::path(args[3]));
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = res ? 0 : 1 };
    }

    if (cmd == L"--delete-profile-file" && args.size() >= 3) {
        const auto res = executeElevatedDeleteAppProfileFile(fs::path(args[2]));
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = res ? 0 : 1 };
    }

    if (cmd == L"run") {
        return CliDispatchResult{
            .guiMode = GuiStartMode::None,
            .exitCode = executeRunCommand(args, 2, appDir),
        };
    }

    if (cmd == L"manage" || cmd == L"edit") {
        if (args.size() < 3) {
            return CliDispatchResult{ .guiMode = GuiStartMode::MainWindow };
        }
        return CliDispatchResult{
            .guiMode = GuiStartMode::ManageApp,
            .guiTargetPath = fs::path(args[2]),
        };
    }

    if (cmd == L"shortcut-ui") {
        return CliDispatchResult{
            .guiMode = GuiStartMode::ShortcutTarget,
            .guiTargetPath = args.size() >= 3 ? fs::path(args[2]) : fs::path{},
        };
    }

    if (cmd == L"shortcut") {
        return CliDispatchResult{
            .guiMode = GuiStartMode::None,
            .exitCode = executeShortcutCommand(args, appDir),
        };
    }

    if (cmd == L"install" || cmd == L"uninstall") {
        bool allUsers = false;
        bool win11 = false;
        for (size_t i = 2; i < args.size(); ++i) {
            const auto flag = toLowerW(args[i]);
            if (flag == L"--all-users") allUsers = true;
            if (flag == L"--win11") win11 = true;
        }
        const auto res = (cmd == L"install")
            ? (win11 ? installWin11SparsePackage(appDir) : installClassicContextMenu(appDir, allUsers))
            : (win11 ? uninstallWin11SparsePackage(appDir) : uninstallClassicContextMenu(allUsers));
        if (!res) {
            writeConsoleOrMessageBox(L"Locale Emulator Plus", res.error(), true);
            return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 1 };
        }
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 0 };
    }

    if (cmd == L"list-profiles") {
        const auto cfgExp = loadOrCreateGlobalConfig(resolveGlobalConfigPath(appDir));
        const GlobalConfig cfg = cfgExp ? *cfgExp : createDefaultGlobalConfig();
        std::wstring text;
        for (const auto& p : cfg.profiles) {
            text += std::format(L"- {} (locale={}, tz={})\n", p.name, p.locale, p.timezone);
        }
        writeConsoleOrMessageBox(L"Locale Emulator Plus", text, false);
        return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 0 };
    }

    // 默认兜底：如果直接传入一个文件路径（比如把 EXE 拖到 LEP.exe 图标上），按默认优先级运行
    std::error_code ec;
    if (fs::is_regular_file(fs::path(args[1]), ec)) {
        return CliDispatchResult{
            .guiMode = GuiStartMode::None,
            .exitCode = executeRunCommand(args, 1, appDir),
        };
    }

    writeConsoleOrMessageBox(L"Locale Emulator Plus",
        std::format(L"未知命令或找不到文件：{}\n\n{}", args[1], buildHelpText()), true);
    return CliDispatchResult{ .guiMode = GuiStartMode::None, .exitCode = 2 };
}

NAMESPACE_END(lep)
