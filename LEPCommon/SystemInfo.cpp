module;

#include "LEPMacros.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <ObjIdl.h>
#include <winnls.h>

module LEPCommon.SystemInfo;

import LEPCommon.StringUtils;

NAMESPACE_BEGIN(lep)

namespace
{
    struct OsVersionInfo
    {
        DWORD major = 0;
        DWORD minor = 0;
        DWORD build = 0;
    };

    OsVersionInfo queryRealOsVersion()
    {
        static const OsVersionInfo s_version = []()
            {
                OsVersionInfo info{};
                using RtlGetVersionFn = LONG (WINAPI*)(PRTL_OSVERSIONINFOW);
                if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
                    if (const auto fn = (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion")) {
                        RTL_OSVERSIONINFOW osvi{};
                        osvi.dwOSVersionInfoSize = sizeof(osvi);
                        if (fn(&osvi) == 0) {
                            info.major = osvi.dwMajorVersion;
                            info.minor = osvi.dwMinorVersion;
                            info.build = osvi.dwBuildNumber;
                        }
                    }
                }
                return info;
            }();
        return s_version;
    }

    uint32_t parseDecimalW(const std::wstring_view text)
    {
        uint32_t value = 0;
        for (const wchar_t ch : text) {
            if (ch < L'0' || ch > L'9') {
                break;
            }
            value = value * 10 + (uint32_t)(ch - L'0');
        }
        return value;
    }

    std::wstring queryLocaleString(const PCWSTR localeName, const LCTYPE lcType)
    {
        const int len = GetLocaleInfoEx(localeName, lcType, nullptr, 0);
        if (len <= 1) {
            return {};
        }
        std::wstring buf((size_t)len, L'\0');
        if (GetLocaleInfoEx(localeName, lcType, buf.data(), len) <= 0) {
            return {};
        }
        if (!buf.empty() && buf.back() == L'\0') {
            buf.pop_back();
        }
        return buf;
    }
}

fs::path getModuleFilePath(const HMODULE hModule)
{
    std::wstring buffer(512, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(hModule, buffer.data(), (DWORD)buffer.size());
        if (len == 0) {
            return {};
        }
        if (len < buffer.size()) {
            buffer.resize(len);
            return fs::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

fs::path getAppDirectory(const HMODULE hModule)
{
    const fs::path modulePath = getModuleFilePath(hModule);
    return modulePath.empty() ? fs::current_path() : modulePath.parent_path();
}

fs::path getLocalAppDataDirectory()
{
    PWSTR pRaw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pRaw)) && pRaw != nullptr) {
        fs::path result(pRaw);
        CoTaskMemFree(pRaw);
        return result / L"LocaleEmulatorPlus";
    }
    wchar_t envBuf[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", envBuf, MAX_PATH) > 0) {
        return fs::path(envBuf) / L"LocaleEmulatorPlus";
    }
    return getAppDirectory();
}

bool isWindows11OrGreater()
{
    const auto ver = queryRealOsVersion();
    return ver.major > 10 || (ver.major == 10 && ver.build >= 22000);
}

bool isWindows10OrGreater()
{
    const auto ver = queryRealOsVersion();
    return ver.major >= 10;
}

bool isAdministrator()
{
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID pAdminGroup = nullptr;
    if (!AllocateAndInitializeSid(
            &ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &pAdminGroup))
    {
        return false;
    }
    BOOL isMember = FALSE;
    CheckTokenMembership(nullptr, pAdminGroup, &isMember);
    FreeSid(pAdminGroup);
    return isMember != FALSE;
}

bool isPackagedApp()
{
    using GetCurrentPackageFullNameFn = LONG (WINAPI*)(UINT32*, PWSTR);
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return false;
    }
    const auto fn = (GetCurrentPackageFullNameFn)GetProcAddress(kernel32, "GetCurrentPackageFullName");
    if (!fn) {
        return false;
    }
    UINT32 length = 0;
    const LONG rc = fn(&length, nullptr);
    return rc == ERROR_INSUFFICIENT_BUFFER;
}

DirectoryWriteStatus probeDirectoryWriteStatus(const fs::path& directory)
{
    if (directory.empty()) {
        return DirectoryWriteStatus::ReadOnly;
    }
    std::error_code ec;
    if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec)) {
        return DirectoryWriteStatus::ReadOnly;
    }

    // 光驱/ISO 挂载盘或只读卷快速识别
    const fs::path root = directory.root_path();
    if (!root.empty()) {
        const UINT driveType = GetDriveTypeW(root.c_str());
        if (driveType == DRIVE_CDROM) {
            return DirectoryWriteStatus::ReadOnly;
        }
        DWORD fsFlags = 0;
        if (GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr, &fsFlags, nullptr, 0)) {
            if ((fsFlags & FILE_READ_ONLY_VOLUME) != 0) {
                return DirectoryWriteStatus::ReadOnly;
            }
        }
    }

    const DWORD pid = GetCurrentProcessId();
    const DWORD tick = GetTickCount();
    const fs::path probeFile = directory / std::format(L".lep_write_probe_{}_{}.tmp", pid, tick);
    const HANDLE hFile = CreateFileW(
        probeFile.c_str(),
        GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        const DWORD err = GetLastError();
        if (err == ERROR_WRITE_PROTECT) {
            return DirectoryWriteStatus::ReadOnly;
        }
        if (err == ERROR_ACCESS_DENIED && !isAdministrator()) {
            return DirectoryWriteStatus::NeedsAdmin;
        }
        return DirectoryWriteStatus::ReadOnly;
    }
    CloseHandle(hFile);
    return DirectoryWriteStatus::Writable;
}

bool isDirectoryWritable(const fs::path& directory)
{
    return probeDirectoryWriteStatus(directory) == DirectoryWriteStatus::Writable;
}

fs::path resolveGlobalConfigPath(const fs::path& appDir)
{
    // Microsoft Store (MSIX) 的安装目录为只读 WindowsApps，统一写入 LocalAppData
    if (isPackagedApp()) {
        const fs::path dir = getLocalAppDataDirectory();
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir / L"config.toml";
    }

    // 便携模式优先与主程序同目录；如果放在 Program Files 等只读目录，则自动回退 LocalAppData
    const fs::path portablePath = appDir / L"config.toml";
    std::error_code ec;
    if (fs::exists(portablePath, ec) || isDirectoryWritable(appDir)) {
        return portablePath;
    }

    const fs::path fallbackDir = getLocalAppDataDirectory();
    fs::create_directories(fallbackDir, ec);
    return fallbackDir / L"config.toml";
}

PeArch detectPeArchitecture(const fs::path& exePath)
{
    const HANDLE hFile = CreateFileW(
        exePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PeArch::Unknown;
    }

    IMAGE_DOS_HEADER dos{};
    DWORD bytesRead = 0;
    PeArch result = PeArch::Unknown;
    if (ReadFile(hFile, &dos, sizeof(dos), &bytesRead, nullptr) &&
        bytesRead == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE &&
        dos.e_lfanew >= (LONG)sizeof(dos))
    {
        LARGE_INTEGER offset{};
        offset.QuadPart = dos.e_lfanew;
        if (SetFilePointerEx(hFile, offset, nullptr, FILE_BEGIN)) {
            DWORD signature = 0;
            IMAGE_FILE_HEADER fileHeader{};
            if (ReadFile(hFile, &signature, sizeof(signature), &bytesRead, nullptr) &&
                bytesRead == sizeof(signature) &&
                signature == IMAGE_NT_SIGNATURE &&
                ReadFile(hFile, &fileHeader, sizeof(fileHeader), &bytesRead, nullptr) &&
                bytesRead == sizeof(fileHeader))
            {
                if (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
                    result = PeArch::X64;
                }
                else if (fileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
                    result = PeArch::X86;
                }
            }
        }
    }

    CloseHandle(hFile);
    return result;
}

std::optional<ShortcutTargetInfo> resolveShortcutTarget(const fs::path& lnkPath)
{
    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hrInit);

    std::optional<ShortcutTargetInfo> result;
    IShellLinkW* pLink = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLink)))) {
        IPersistFile* pFile = nullptr;
        if (SUCCEEDED(pLink->QueryInterface(IID_PPV_ARGS(&pFile)))) {
            if (SUCCEEDED(pFile->Load(lnkPath.c_str(), STGM_READ))) {
                wchar_t targetBuf[MAX_PATH * 2]{};
                wchar_t argsBuf[2048]{};
                if (SUCCEEDED(pLink->GetPath(targetBuf, (int)std::size(targetBuf), nullptr, SLGP_RAWPATH)) && targetBuf[0] != L'\0') {
                    pLink->GetArguments(argsBuf, (int)std::size(argsBuf));
                    result = ShortcutTargetInfo{
                        .targetPath = fs::path(targetBuf),
                        .arguments = std::wstring(argsBuf),
                    };
                }
            }
            pFile->Release();
        }
        pLink->Release();
    }

    if (needUninit) {
        CoUninitialize();
    }
    return result;
}

std::optional<FileAssociationInfo> resolveFileAssociation(const fs::path& filePath)
{
    const std::wstring ext = filePath.extension().wstring();
    if (ext.empty()) {
        return std::nullopt;
    }

    DWORD exeLen = 0;
    if (FAILED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, ext.c_str(), nullptr, nullptr, &exeLen)) || exeLen <= 1) {
        return std::nullopt;
    }
    std::wstring exeBuf(exeLen, L'\0');
    if (FAILED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, ext.c_str(), nullptr, exeBuf.data(), &exeLen))) {
        return std::nullopt;
    }
    if (!exeBuf.empty() && exeBuf.back() == L'\0') {
        exeBuf.pop_back();
    }

    std::wstring cmdBuf;
    DWORD cmdLen = 0;
    if (SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_COMMAND, ext.c_str(), nullptr, nullptr, &cmdLen)) && cmdLen > 1) {
        cmdBuf.resize(cmdLen, L'\0');
        if (SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_COMMAND, ext.c_str(), nullptr, cmdBuf.data(), &cmdLen))) {
            if (!cmdBuf.empty() && cmdBuf.back() == L'\0') {
                cmdBuf.pop_back();
            }
        }
        else {
            cmdBuf.clear();
        }
    }

    return FileAssociationInfo{
        .executablePath = fs::path(exeBuf),
        .commandTemplate = std::move(cmdBuf),
    };
}

std::vector<CoreComponentStatus> inspectCoreComponents(const fs::path& appDir)
{
    static constexpr std::wstring_view kCoreFiles[] = {
        L"LoaderDll_x64.dll",
        L"LocaleEmulatorPlus_x64.dll",
        L"LoaderDll_x86.dll",
        L"LocaleEmulatorPlus_x86.dll",
        L"LEPHelper_x86.exe",
        L"LEPShellExt.dll",
    };

    std::vector<CoreComponentStatus> results;
    results.reserve(std::size(kCoreFiles));
    std::error_code ec;
    for (const auto name : kCoreFiles) {
        results.push_back(CoreComponentStatus{
            .fileName = std::wstring(name),
            .present = fs::is_regular_file(appDir / name, ec),
        });
    }
    return results;
}

uint32_t charsetFromAnsiCodePage(const uint32_t ansiCodePage)
{
    switch (ansiCodePage)
    {
    case 932:  return SHIFTJIS_CHARSET;    // 128
    case 936:  return GB2312_CHARSET;      // 134
    case 949:  return HANGUL_CHARSET;      // 129
    case 950:  return CHINESEBIG5_CHARSET; // 136
    case 1250: return EASTEUROPE_CHARSET;  // 238
    case 1251: return RUSSIAN_CHARSET;     // 204
    case 1252: return ANSI_CHARSET;        // 0
    case 1253: return GREEK_CHARSET;       // 161
    case 1254: return TURKISH_CHARSET;     // 162
    case 1255: return HEBREW_CHARSET;      // 177
    case 1256: return ARABIC_CHARSET;      // 178
    case 1257: return BALTIC_CHARSET;      // 186
    case 1258: return VIETNAMESE_CHARSET;  // 163
    case 874:  return THAI_CHARSET;        // 222
    default:   return ANSI_CHARSET;
    }
}

std::optional<LocaleEntry> resolveLocaleDetails(const std::wstring_view localeName)
{
    if (localeName.empty()) {
        return std::nullopt;
    }
    const std::wstring owned(localeName);
    const LCID lcid = LocaleNameToLCID(owned.c_str(), 0);
    if (lcid == 0) {
        return std::nullopt;
    }

    const std::wstring acpStr = queryLocaleString(owned.c_str(), LOCALE_IDEFAULTANSICODEPAGE);
    const std::wstring ocpStr = queryLocaleString(owned.c_str(), LOCALE_IDEFAULTCODEPAGE);
    std::wstring display = queryLocaleString(owned.c_str(), LOCALE_SLOCALIZEDDISPLAYNAME);
    if (display.empty()) {
        display = owned;
    }

    const uint32_t acp = parseDecimalW(acpStr);
    const uint32_t ocp = parseDecimalW(ocpStr);
    if (acp == 0) {
        // 纯 Unicode-only 区域没有传统 ANSI 代码页，回退 UTF-8 / 1252 处理
        return LocaleEntry{
            .localeName = owned,
            .displayName = std::move(display),
            .lcid = (uint32_t)lcid,
            .ansiCodePage = 1252,
            .oemCodePage = ocp != 0 ? ocp : 437,
            .fontCharset = ANSI_CHARSET,
        };
    }

    return LocaleEntry{
        .localeName = owned,
        .displayName = std::move(display),
        .lcid = (uint32_t)lcid,
        .ansiCodePage = acp,
        .oemCodePage = ocp != 0 ? ocp : acp,
        .fontCharset = charsetFromAnsiCodePage(acp),
    };
}

std::vector<LocaleEntry> enumerateLocales()
{
    std::vector<LocaleEntry> all;

    auto callbackFunc = [](LPWSTR pLocaleName, DWORD /*dwFlags*/, LPARAM lParam) -> BOOL
        {
            if (!pLocaleName || pLocaleName[0] == L'\0') {
                return TRUE;
            }
            auto* pList = (std::vector<LocaleEntry>*)lParam;
            const LCID lcid = LocaleNameToLCID(pLocaleName, 0);
            if (lcid == 0 || lcid == LOCALE_CUSTOM_UNSPECIFIED) {
                return TRUE;
            }
            const std::wstring acpStr = queryLocaleString(pLocaleName, LOCALE_IDEFAULTANSICODEPAGE);
            const uint32_t acp = parseDecimalW(acpStr);
            if (acp == 0) {
                return TRUE;
            }
            const std::wstring ocpStr = queryLocaleString(pLocaleName, LOCALE_IDEFAULTCODEPAGE);
            std::wstring display = queryLocaleString(pLocaleName, LOCALE_SLOCALIZEDDISPLAYNAME);
            if (display.empty()) {
                display = pLocaleName;
            }
            const uint32_t ocp = parseDecimalW(ocpStr);
            pList->push_back(LocaleEntry{
                .localeName = pLocaleName,
                .displayName = std::format(L"{} [{} | CP{}]", display, pLocaleName, acp),
                .lcid = (uint32_t)lcid,
                .ansiCodePage = acp,
                .oemCodePage = ocp != 0 ? ocp : acp,
                .fontCharset = charsetFromAnsiCodePage(acp),
            });
            return TRUE;
        };

    EnumSystemLocalesEx(callbackFunc, LOCALE_SPECIFICDATA, (LPARAM)&all, nullptr);

    // 常用东亚/英文区域排在前面，方便用户一眼找到
    auto priorityFunc = [](const std::wstring_view name) -> int
        {
            if (equalsIgnoreCaseW(name, L"ja-JP")) return 0;
            if (equalsIgnoreCaseW(name, L"zh-TW")) return 1;
            if (equalsIgnoreCaseW(name, L"zh-HK")) return 2;
            if (equalsIgnoreCaseW(name, L"zh-CN")) return 3;
            if (equalsIgnoreCaseW(name, L"ko-KR")) return 4;
            if (equalsIgnoreCaseW(name, L"en-US")) return 5;
            if (equalsIgnoreCaseW(name, L"ru-RU")) return 6;
            return 100;
        };

    std::ranges::sort(all, [&](const LocaleEntry& a, const LocaleEntry& b)
        {
            const int pa = priorityFunc(a.localeName);
            const int pb = priorityFunc(b.localeName);
            if (pa != pb) {
                return pa < pb;
            }
            return a.displayName < b.displayName;
        });

    return all;
}

std::vector<TimezoneEntry> enumerateTimezones()
{
    std::vector<TimezoneEntry> list;
    HKEY hRoot = nullptr;
    if (RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones",
            0, KEY_READ, &hRoot) != ERROR_SUCCESS)
    {
        return list;
    }

    wchar_t subName[256]{};
    for (DWORD index = 0;; ++index) {
        DWORD nameLen = (DWORD)std::size(subName);
        if (RegEnumKeyExW(hRoot, index, subName, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            break;
        }
        HKEY hSub = nullptr;
        if (RegOpenKeyExW(hRoot, subName, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
            wchar_t displayBuf[256]{};
            DWORD displayBytes = sizeof(displayBuf);
            DWORD type = 0;
            std::wstring display;
            if (RegQueryValueExW(hSub, L"Display", nullptr, &type, (BYTE*)displayBuf, &displayBytes) == ERROR_SUCCESS &&
                type == REG_SZ)
            {
                display = displayBuf;
            }
            else {
                display = subName;
            }
            RegCloseKey(hSub);
            list.push_back(TimezoneEntry{
                .id = subName,
                .displayName = std::move(display),
            });
        }
    }
    RegCloseKey(hRoot);

    auto priorityFunc = [](const std::wstring_view id) -> int
        {
            if (equalsIgnoreCaseW(id, L"Tokyo Standard Time")) return 0;
            if (equalsIgnoreCaseW(id, L"Taipei Standard Time")) return 1;
            if (equalsIgnoreCaseW(id, L"China Standard Time")) return 2;
            if (equalsIgnoreCaseW(id, L"Korea Standard Time")) return 3;
            if (equalsIgnoreCaseW(id, L"UTC")) return 4;
            return 100;
        };

    std::ranges::sort(list, [&](const TimezoneEntry& a, const TimezoneEntry& b)
        {
            const int pa = priorityFunc(a.id);
            const int pb = priorityFunc(b.id);
            if (pa != pb) {
                return pa < pb;
            }
            return a.displayName < b.displayName;
        });

    return list;
}

NAMESPACE_END(lep)
