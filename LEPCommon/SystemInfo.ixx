module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.SystemInfo;

export import std.compat;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

enum class PeArch
{
    Unknown,
    X86,
    X64,
};

enum class DirectoryWriteStatus
{
    Writable,
    NeedsAdmin,
    ReadOnly,
};

struct LocaleEntry
{
    std::wstring localeName;
    std::wstring displayName;
    uint32_t lcid = 0;
    uint32_t ansiCodePage = 0;
    uint32_t oemCodePage = 0;
    uint32_t fontCharset = 0;
};

struct TimezoneEntry
{
    std::wstring id;
    std::wstring displayName;
};

struct ShortcutTargetInfo
{
    fs::path targetPath;
    std::wstring arguments;
};

struct FileAssociationInfo
{
    fs::path executablePath;
    std::wstring commandTemplate;
};

struct CoreComponentStatus
{
    std::wstring fileName;
    bool present = false;
};

// 模块与目录路径
fs::path getModuleFilePath(HMODULE hModule = nullptr);
fs::path getAppDirectory(HMODULE hModule = nullptr);
fs::path getLocalAppDataDirectory();
fs::path resolveGlobalConfigPath(const fs::path& appDir);

// 系统与运行环境探测（全部 Win7 安全）
bool isWindows11OrGreater();
bool isWindows10OrGreater();
bool isAdministrator();
bool isPackagedApp();
DirectoryWriteStatus probeDirectoryWriteStatus(const fs::path& directory);
bool isDirectoryWritable(const fs::path& directory);

// 目标程序探测
PeArch detectPeArchitecture(const fs::path& exePath);
std::optional<ShortcutTargetInfo> resolveShortcutTarget(const fs::path& lnkPath);
std::optional<FileAssociationInfo> resolveFileAssociation(const fs::path& filePath);
std::vector<CoreComponentStatus> inspectCoreComponents(const fs::path& appDir);

// 区域、代码页与时区枚举
uint32_t charsetFromAnsiCodePage(uint32_t ansiCodePage);
std::optional<LocaleEntry> resolveLocaleDetails(std::wstring_view localeName);
std::vector<LocaleEntry> enumerateLocales();
std::vector<TimezoneEntry> enumerateTimezones();

NAMESPACE_END(lep)
