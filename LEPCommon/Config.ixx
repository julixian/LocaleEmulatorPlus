module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.Config;

export import std.compat;
export import tomlkitpp;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

struct Profile
{
    std::wstring name = L"日本语";
    std::wstring locale = L"ja-JP";
    std::wstring timezone = L"Tokyo Standard Time";
    int registryRedirection = 2; // 0=默认(3项), 1=基础(4项), 2=高级(8项)
    int uiLanguageMode = 2;      // 0=关闭, 1=Win32 API, 2=Native API + 虚拟MUI
    bool runAsAdmin = false;
    bool runWithSuspend = false;
    bool showInMainMenu = true;
    std::wstring arguments;

    // 可选高级覆盖字段（为 0 或空时按 locale/timezone 自动推导）
    std::optional<uint32_t> ansiCodePage;
    std::optional<uint32_t> oemCodePage;
    std::optional<uint32_t> localeId;
    std::optional<uint32_t> fontCharset;
    std::wstring standardNameOverride;
    std::wstring daylightNameOverride;
};

struct AppProfileEntry
{
    std::wstring targetPath;
    Profile profile;
};

struct GlobalConfig
{
    std::wstring version = LEP_APP_VERSION_WSTR;
    std::wstring language = L"auto"; // auto | zh-CN | en-US | ja-JP
    std::vector<Profile> profiles;
    std::vector<AppProfileEntry> appProfiles;
};

enum class AppProfileStorage
{
    None,
    AlongsideTarget, // <target>.lep.toml
    GlobalConfig,    // config.toml [[app_profiles]]
};

struct ResolvedAppProfile
{
    Profile profile;
    AppProfileStorage storage = AppProfileStorage::None;
    fs::path configFilePath;
};

struct ResolvedAppProfiles
{
    std::vector<Profile> profiles;
    AppProfileStorage storage = AppProfileStorage::None;
    fs::path configFilePath;
};

// 生成默认的日本语 / 繁体中文预置全局配置
GlobalConfig createDefaultGlobalConfig();
Profile createDefaultProfile(std::wstring_view name = L"日本语");

// 目标程序同目录专属配置文件路径（<target>.lep.toml）
fs::path alongsideAppConfigPath(const fs::path& targetPath);

// 全局配置读写（若文件不存在则自动创建默认配置）
std::expected<GlobalConfig, std::wstring> loadOrCreateGlobalConfig(const fs::path& configPath);
std::expected<GlobalConfig, std::wstring> loadGlobalConfigReadOnly(const fs::path& configPath);
std::expected<void, std::wstring> saveGlobalConfig(const fs::path& configPath, const GlobalConfig& config);

// 按名称查找全局配置（大小写不敏感）
std::optional<Profile> findGlobalProfileByName(const GlobalConfig& config, std::wstring_view name);

// 单程序多专属配置查找：同目录 .lep.toml 优先于全局 [[app_profiles]]（向下兼容旧版单 [profile]）
std::optional<ResolvedAppProfiles> findAppProfiles(const fs::path& targetPath, const GlobalConfig& globalConfig);
std::optional<Profile> findAppProfileByName(const fs::path& targetPath, const GlobalConfig& globalConfig, std::wstring_view name);
std::optional<ResolvedAppProfile> findAppProfile(const fs::path& targetPath, const GlobalConfig& globalConfig);

// 保存/删除单程序专属配置（支持多配置）
std::expected<fs::path, std::wstring> saveAppProfilesToFile(
    const fs::path& outputPath,
    std::span<const Profile> profiles,
    const fs::path& existingTemplatePath = {});
std::expected<fs::path, std::wstring> saveAppProfilesAlongside(
    const fs::path& targetPath,
    std::span<const Profile> profiles);
std::expected<void, std::wstring> saveAppProfilesInGlobal(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath,
    std::span<const Profile> profiles);

std::expected<fs::path, std::wstring> saveAppProfileToFile(
    const fs::path& outputPath,
    const Profile& profile,
    const fs::path& existingTemplatePath = {});
std::expected<fs::path, std::wstring> saveAppProfileAlongside(const fs::path& targetPath, const Profile& profile);
std::expected<void, std::wstring> saveAppProfileInGlobal(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath,
    const Profile& profile);
std::expected<bool, std::wstring> deleteAppProfile(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath);

NAMESPACE_END(lep)
