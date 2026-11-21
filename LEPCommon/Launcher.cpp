module;

#include "LEPMacros.hpp"
#include "LaunchProtocol.hpp"
#include <Windows.h>
#include <shellapi.h>

module LEPCommon.Launcher;

import LEPCommon.StringUtils;
import LEPCommon.SystemInfo;

NAMESPACE_BEGIN(lep)

namespace
{
    std::atomic<uint32_t> g_helperSequence{ 0 };

    template<size_t N>
    void copyWideFixed(WCHAR (&dst)[N], const std::wstring_view src)
    {
        std::memset(dst, 0, sizeof(dst));
        const size_t count = std::min(src.size(), N - 1);
        if (count > 0) {
            std::memcpy(dst, src.data(), count * sizeof(WCHAR));
        }
    }

    std::expected<LaunchResult, std::wstring> launchDirectX64(
        const fs::path& appDir,
        const LEP::LEPB& env,
        const std::wstring& appPath,
        const std::wstring& cmdLine,
        const std::wstring& workDir,
        const DWORD creationFlags,
        const bool waitForExit)
    {
        const fs::path loaderPath = appDir / L"LoaderDll_x64.dll";
        const HMODULE hLoader = LoadLibraryW(loaderPath.c_str());
        if (!hLoader) {
            return std::unexpected(std::format(
                L"无法加载 {}（Win32 错误 {}）。请确认核心 DLL 完整且未被杀毒软件拦截。",
                loaderPath.filename().wstring(), GetLastError()));
        }

        const auto pCreate = (LEP::LepCreateProcess2_t)GetProcAddress(hLoader, "LepCreateProcess2");
        if (!pCreate) {
            FreeLibrary(hLoader);
            return std::unexpected(L"LoaderDll_x64.dll 中未找到 LepCreateProcess2 导出。");
        }

        LEP::LEPB mutableEnv = env;
        std::wstring mutableCmd = cmdLine;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        const LEP::NTSTATUS status = pCreate(
            &mutableEnv,
            appPath.empty() ? nullptr : appPath.c_str(),
            mutableCmd.empty() ? nullptr : mutableCmd.data(),
            workDir.empty() ? nullptr : workDir.c_str(),
            creationFlags,
            &si,
            &pi,
            nullptr,
            nullptr,
            nullptr,
            nullptr);

        FreeLibrary(hLoader);

        if (status < 0) {
            return std::unexpected(std::format(L"LepCreateProcess2 (x64) 失败，NTSTATUS = 0x{:08X}", (uint32_t)status));
        }

        LaunchResult res{ .processId = pi.dwProcessId };
        if (pi.hThread) {
            CloseHandle(pi.hThread);
        }
        if (pi.hProcess) {
            if (waitForExit) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD exitCode = 0;
                if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
                    res.exitCode = exitCode;
                }
            }
            CloseHandle(pi.hProcess);
        }
        return res;
    }

    std::expected<LaunchResult, std::wstring> launchViaHelperX86(
        const fs::path& appDir,
        const LEP::LEPB& env,
        const std::wstring& appPath,
        const std::wstring& cmdLine,
        const std::wstring& workDir,
        const DWORD creationFlags,
        const bool waitForExit)
    {
        const fs::path helperExe = appDir / L"LEPHelper_x86.exe";
        std::error_code ec;
        if (!fs::is_regular_file(helperExe, ec)) {
            return std::unexpected(L"未找到 LEPHelper_x86.exe，无法启动 32 位目标程序。");
        }

        const uint32_t appBytes = (uint32_t)((appPath.size() + 1) * sizeof(wchar_t));
        const uint32_t cmdBytes = (uint32_t)((cmdLine.size() + 1) * sizeof(wchar_t));
        const uint32_t dirBytes = (uint32_t)((workDir.size() + 1) * sizeof(wchar_t));
        const uint32_t totalSize = (uint32_t)sizeof(LaunchHelperPacket) + appBytes + cmdBytes + dirBytes;
        if (totalSize > kLaunchHelperMaxSize) {
            return std::unexpected(L"启动命令行过长。");
        }

        const uint32_t seq = ++g_helperSequence;
        const std::wstring mapName = std::format(L"Local\\LEP_LAUNCH_{}_{}", GetCurrentProcessId(), seq);
        const HANDLE hMap = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, totalSize, mapName.c_str());
        if (!hMap) {
            return std::unexpected(std::format(L"CreateFileMappingW 失败，Win32 错误 {}", GetLastError()));
        }

        auto* pPacket = (LaunchHelperPacket*)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, totalSize);
        if (!pPacket) {
            const DWORD err = GetLastError();
            CloseHandle(hMap);
            return std::unexpected(std::format(L"MapViewOfFile 失败，Win32 错误 {}", err));
        }

        std::memset(pPacket, 0, totalSize);
        pPacket->magic = kLaunchHelperMagic;
        pPacket->version = kLaunchHelperVersion;
        pPacket->totalSize = totalSize;
        pPacket->creationFlags = creationFlags;
        pPacket->status = (int32_t)0xC0000001L; // STATUS_UNSUCCESSFUL
        std::memcpy(pPacket->lepbBytes, &env, sizeof(env));

        uint32_t cursor = (uint32_t)sizeof(LaunchHelperPacket);
        auto writeStrFunc = [&](const std::wstring& str, uint32_t& outOffset, uint32_t& outChars)
            {
                if (str.empty()) {
                    outOffset = 0;
                    outChars = 0;
                    return;
                }
                outOffset = cursor;
                outChars = (uint32_t)str.size();
                std::memcpy((uint8_t*)pPacket + cursor, str.c_str(), (str.size() + 1) * sizeof(wchar_t));
                cursor += (uint32_t)((str.size() + 1) * sizeof(wchar_t));
            };

        writeStrFunc(appPath, pPacket->appOffset, pPacket->appChars);
        writeStrFunc(cmdLine, pPacket->cmdOffset, pPacket->cmdChars);
        writeStrFunc(workDir, pPacket->dirOffset, pPacket->dirChars);

        std::wstring helperCmd = std::format(L"{} {}", quoteCommandLineArg(helperExe.wstring()), mapName);
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(
                helperExe.c_str(),
                helperCmd.data(),
                nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                appDir.c_str(),
                &si, &pi))
        {
            const DWORD err = GetLastError();
            UnmapViewOfFile(pPacket);
            CloseHandle(hMap);
            return std::unexpected(std::format(L"启动 LEPHelper_x86.exe 失败，Win32 错误 {}", err));
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        const int32_t status = pPacket->status;
        const uint32_t win32Err = pPacket->win32Error;
        const uint32_t targetPid = pPacket->targetPid;

        UnmapViewOfFile(pPacket);
        CloseHandle(hMap);

        if (win32Err != 0) {
            return std::unexpected(std::format(L"LEPHelper_x86.exe 报告错误，Win32 错误 {}", win32Err));
        }
        if (status < 0) {
            return std::unexpected(std::format(L"LepCreateProcess2 (x86) 失败，NTSTATUS = 0x{:08X}", (uint32_t)status));
        }

        LaunchResult res{ .processId = targetPid };
        if (waitForExit && targetPid != 0) {
            if (const HANDLE hTarget = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid)) {
                WaitForSingleObject(hTarget, INFINITE);
                DWORD exitCode = 0;
                if (GetExitCodeProcess(hTarget, &exitCode)) {
                    res.exitCode = exitCode;
                }
                CloseHandle(hTarget);
            }
        }
        return res;
    }

    std::expected<LaunchResult, std::wstring> elevateViaSelf(const LaunchRequest& req)
    {
        const fs::path selfExe = getModuleFilePath();
        std::wstring args = L"run ";
        args += L"--locale " + quoteCommandLineArg(req.profile.locale) + L" ";
        args += L"--tz " + quoteCommandLineArg(req.profile.timezone) + L" ";
        args += std::format(L"--reg {} --ui {} ", req.profile.registryRedirection, req.profile.uiLanguageMode);
        if (req.profile.runWithSuspend) {
            args += L"--suspend ";
        }
        args += quoteCommandLineArg(req.targetPath.wstring());

        const std::wstring combinedExtra = !req.profile.arguments.empty()
            ? req.profile.arguments
            : req.extraArguments;
        if (!trimW(combinedExtra).empty()) {
            args += L" -- " + trimW(combinedExtra);
        }

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = req.ownerWindow;
        sei.lpVerb = L"runas";
        sei.lpFile = selfExe.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            const DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                return std::unexpected(L"用户取消了管理员提权请求。");
            }
            return std::unexpected(std::format(L"请求管理员提权失败，Win32 错误 {}", err));
        }
        if (sei.hProcess) {
            if (req.waitForExit) {
                WaitForSingleObject(sei.hProcess, INFINITE);
            }
            CloseHandle(sei.hProcess);
        }
        return LaunchResult{};
    }

    // Microsoft Store 打包版位于受 WindowsApps 条件 ACL 保护的目录中，非打包子进程（如提权 rundll32.exe 代理）
    // 无法直接 LoadLibrary WindowsApps 内的 DLL。将 5 个核心运行时文件同步到 LocalState\runtime 后即可无障碍加载。
    fs::path resolveRuntimeDirectory(const fs::path& appDir)
    {
        if (!isPackagedApp()) {
            return appDir;
        }

        const fs::path runtimeDir = resolveGlobalConfigPath(appDir).parent_path() / L"runtime";
        std::error_code ec;
        fs::create_directories(runtimeDir, ec);
        if (ec) {
            return appDir;
        }

        constexpr std::array<std::wstring_view, 5> kRuntimeFiles = {
            L"LoaderDll_x64.dll",
            L"LocaleEmulatorPlus_x64.dll",
            L"LoaderDll_x86.dll",
            L"LocaleEmulatorPlus_x86.dll",
            L"LEPHelper_x86.exe",
        };

        for (const auto fileName : kRuntimeFiles) {
            const fs::path src = appDir / fileName;
            const fs::path dst = runtimeDir / fileName;
            if (!fs::is_regular_file(src, ec)) {
                continue;
            }
            bool needCopy = !fs::is_regular_file(dst, ec);
            if (!needCopy) {
                const auto srcSize = fs::file_size(src, ec);
                const auto dstSize = fs::file_size(dst, ec);
                const auto srcTime = fs::last_write_time(src, ec);
                const auto dstTime = fs::last_write_time(dst, ec);
                if (ec || srcSize != dstSize || srcTime != dstTime) {
                    needCopy = true;
                }
            }
            if (needCopy) {
                if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
                    // 若目标文件已存在且正被运行中的转区进程占用，继续沿用现有副本
                    if (!fs::is_regular_file(dst, ec)) {
                        return appDir;
                    }
                }
            }
        }

        return runtimeDir;
    }
}

std::expected<LEP::LEPB, std::wstring> buildEnvironmentBlock(const Profile& profile)
{
    const auto localeOpt = resolveLocaleDetails(profile.locale);
    if (!localeOpt && (!profile.ansiCodePage || !profile.localeId)) {
        return std::unexpected(std::format(L"无法解析区域名称：{}", profile.locale));
    }

    LEP::LEPB env{};
    env.Size = sizeof(env);
    env.Version = LEP::LEP_ENVIRONMENT_VERSION;
    env.AnsiCodePage = profile.ansiCodePage.value_or(localeOpt ? localeOpt->ansiCodePage : 932);
    env.OemCodePage = profile.oemCodePage.value_or(localeOpt ? localeOpt->oemCodePage : env.AnsiCodePage);
    env.LocaleId = profile.localeId.value_or(localeOpt ? localeOpt->lcid : 0x0411);
    env.FontCharset = profile.fontCharset.value_or(charsetFromAnsiCodePage(env.AnsiCodePage));
    env.RegistryRedirectionMode = (ULONG)std::clamp(profile.registryRedirection, 0, 2);
    env.HookUILanguageMode = (ULONG)std::clamp(profile.uiLanguageMode, 0, 2);

    const std::wstring tz = profile.timezone.empty() ? L"Tokyo Standard Time" : profile.timezone;
    copyWideFixed(env.TimezoneId, tz);
    if (!profile.standardNameOverride.empty()) {
        copyWideFixed(env.StandardNameOverride, profile.standardNameOverride);
    }
    if (!profile.daylightNameOverride.empty()) {
        copyWideFixed(env.DaylightNameOverride, profile.daylightNameOverride);
    }

    return env;
}

std::expected<LaunchResult, std::wstring> launchTarget(const LaunchRequest& req)
{
    if (req.targetPath.empty()) {
        return std::unexpected(L"目标程序路径为空。");
    }

    std::error_code ec;
    fs::path actualTarget = fs::absolute(req.targetPath, ec);
    if (ec || actualTarget.empty()) {
        actualTarget = req.targetPath;
    }

    std::wstring extraArgs = !req.extraArguments.empty() ? req.extraArguments : req.profile.arguments;

    // 若用户直接拖入 .lnk 快捷方式，先解出目标路径
    if (equalsIgnoreCaseW(actualTarget.extension().wstring(), L".lnk")) {
        if (const auto lnk = resolveShortcutTarget(actualTarget)) {
            actualTarget = lnk->targetPath;
            if (extraArgs.empty() && !lnk->arguments.empty()) {
                extraArgs = lnk->arguments;
            }
        }
    }

    if (!fs::is_regular_file(actualTarget, ec)) {
        return std::unexpected(std::format(L"找不到目标文件：{}", actualTarget.wstring()));
    }

    if (req.profile.runAsAdmin && !isAdministrator()) {
        LaunchRequest elevatedReq = req;
        elevatedReq.targetPath = actualTarget;
        elevatedReq.extraArguments = extraArgs;
        return elevateViaSelf(elevatedReq);
    }

    if (req.profile.runWithSuspend && req.confirmSuspended) {
        const int answer = MessageBoxW(
            req.ownerWindow,
            L"当前配置勾选了「创建后保持挂起（CREATE_SUSPENDED）」。\n\n"
            L"点击「是」后程序将以挂起状态创建，等待你用调试器附加后手动恢复执行。",
            L"Locale Emulator Plus",
            MB_YESNO | MB_ICONINFORMATION);
        if (answer != IDYES) {
            return LaunchResult{};
        }
    }

    const auto envExp = buildEnvironmentBlock(req.profile);
    if (!envExp) {
        return std::unexpected(envExp.error());
    }

    fs::path executable = actualTarget;
    std::wstring commandLine;
    if (equalsIgnoreCaseW(actualTarget.extension().wstring(), L".exe")) {
        commandLine = quoteCommandLineArg(actualTarget.wstring());
        const std::wstring trimmed = trimW(extraArgs);
        if (!trimmed.empty()) {
            commandLine += L" " + trimmed;
        }
    }
    else {
        const auto assoc = resolveFileAssociation(actualTarget);
        if (!assoc || assoc->executablePath.empty()) {
            return std::unexpected(std::format(L"找不到与 {} 关联的可执行程序。", actualTarget.extension().wstring()));
        }
        executable = assoc->executablePath;
        if (!assoc->commandTemplate.empty()) {
            commandLine = replaceAllW(assoc->commandTemplate, L"%1", actualTarget.wstring());
            commandLine = replaceAllW(commandLine, L"%L", actualTarget.wstring());
            commandLine = replaceAllW(commandLine, L"%*", trimW(extraArgs));
        }
        else {
            commandLine = quoteCommandLineArg(executable.wstring()) + L" " + quoteCommandLineArg(actualTarget.wstring());
        }
    }

    const std::wstring workDir = actualTarget.parent_path().wstring();
    const DWORD creationFlags = req.profile.runWithSuspend ? CREATE_SUSPENDED : 0;

    const PeArch arch = detectPeArchitecture(executable);
    if (arch == PeArch::Unknown) {
        return std::unexpected(std::format(L"无法识别目标程序的 PE 架构：{}", executable.wstring()));
    }

    const fs::path runtimeDir = resolveRuntimeDirectory(req.appDir);
    auto result = (arch == PeArch::X86)
        ? launchViaHelperX86(runtimeDir, *envExp, executable.wstring(), commandLine, workDir, creationFlags, req.waitForExit)
        : launchDirectX64(runtimeDir, *envExp, executable.wstring(), commandLine, workDir, creationFlags, req.waitForExit);

    // 若目标带 requireAdministrator 清单导致非管理员 CreateProcess 返回 ERROR_ELEVATION_REQUIRED (0xC00702E4 / 740)，自动提权重试
    if (!result && !isAdministrator() && result.error().contains(L"02E4")) {
        LaunchRequest elevatedReq = req;
        elevatedReq.targetPath = actualTarget;
        elevatedReq.extraArguments = extraArgs;
        return elevateViaSelf(elevatedReq);
    }

    return result;
}

NAMESPACE_END(lep)
