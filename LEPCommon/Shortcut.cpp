module;

#include "LEPMacros.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <ObjIdl.h>

module LEPCommon.Shortcut;

import LEPCommon.StringUtils;
import LEPCommon.SystemInfo;

NAMESPACE_BEGIN(lep)

std::wstring buildLaunchArguments(
    const fs::path& targetPath,
    const std::optional<std::wstring_view> profileName,
    const std::wstring_view extraArguments)
{
    std::wstring args = L"run ";
    if (profileName && !profileName->empty()) {
        args += L"--profile ";
        args += quoteCommandLineArg(*profileName);
        args += L" ";
    }
    args += quoteCommandLineArg(targetPath.wstring());

    const std::wstring trimmedExtra = trimW(extraArguments);
    if (!trimmedExtra.empty()) {
        args += L" -- ";
        args += trimmedExtra;
    }
    return args;
}

std::wstring buildFullLaunchCommandLine(
    const fs::path& lepExePath,
    const fs::path& targetPath,
    const std::optional<std::wstring_view> profileName,
    const std::wstring_view extraArguments)
{
    return quoteCommandLineArg(lepExePath.wstring()) + L" " +
        buildLaunchArguments(targetPath, profileName, extraArguments);
}

fs::path suggestDefaultShortcutPath(const fs::path& targetPath)
{
    const std::wstring fileName = targetPath.stem().wstring() + L" (LEP).lnk";
    const fs::path parentDir = targetPath.parent_path();
    if (!parentDir.empty() && isDirectoryWritable(parentDir)) {
        return parentDir / fileName;
    }

    PWSTR pDesktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pDesktop)) && pDesktop != nullptr) {
        fs::path desktopDir(pDesktop);
        CoTaskMemFree(pDesktop);
        return desktopDir / fileName;
    }
    return fs::current_path() / fileName;
}

std::expected<fs::path, std::wstring> createShortcut(const ShortcutRequest& req)
{
    if (req.lepExePath.empty() || req.targetPath.empty() || req.shortcutOutputPath.empty()) {
        return std::unexpected(L"快捷方式参数不完整。");
    }

    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hrInit);

    IShellLinkW* pLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLink));
    if (FAILED(hr) || !pLink) {
        if (needUninit) CoUninitialize();
        return std::unexpected(std::format(L"CoCreateInstance(ShellLink) 失败: 0x{:08X}", (uint32_t)hr));
    }

    const std::optional<std::wstring_view> profView = req.profileName
        ? std::optional<std::wstring_view>(*req.profileName)
        : std::nullopt;
    const std::wstring args = buildLaunchArguments(req.targetPath, profView, req.extraArguments);
    const std::wstring desc = req.description.empty()
        ? std::format(L"使用 Locale Emulator Plus 运行 {}", req.targetPath.filename().wstring())
        : req.description;
    const fs::path workDir = req.targetPath.parent_path();

    pLink->SetPath(req.lepExePath.c_str());
    pLink->SetArguments(args.c_str());
    pLink->SetDescription(desc.c_str());
    if (!workDir.empty()) {
        pLink->SetWorkingDirectory(workDir.c_str());
    }

    // 图标优先取目标 EXE 自身图标，非 EXE 取 LEP 图标
    if (equalsIgnoreCaseW(req.targetPath.extension().wstring(), L".exe")) {
        pLink->SetIconLocation(req.targetPath.c_str(), 0);
    }
    else {
        pLink->SetIconLocation(req.lepExePath.c_str(), 0);
    }

    IPersistFile* pPersist = nullptr;
    hr = pLink->QueryInterface(IID_PPV_ARGS(&pPersist));
    if (SUCCEEDED(hr) && pPersist) {
        std::error_code ec;
        if (req.shortcutOutputPath.has_parent_path()) {
            fs::create_directories(req.shortcutOutputPath.parent_path(), ec);
        }
        hr = pPersist->Save(req.shortcutOutputPath.c_str(), TRUE);
        pPersist->Release();
    }
    pLink->Release();

    if (needUninit) {
        CoUninitialize();
    }

    if (FAILED(hr)) {
        return std::unexpected(std::format(L"保存快捷方式失败: 0x{:08X}", (uint32_t)hr));
    }
    return req.shortcutOutputPath;
}

NAMESPACE_END(lep)
