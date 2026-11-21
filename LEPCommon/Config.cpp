module;

#include "LEPMacros.hpp"
#include <Windows.h>

module LEPCommon.Config;

import LEPCommon.StringUtils;

NAMESPACE_BEGIN(lep)

namespace
{
    int clampMode(const int value, const int maxValue)
    {
        return std::clamp(value, 0, maxValue);
    }

    Profile parseProfileTable(const tomlkit::Item& tbl)
    {
        Profile p = createDefaultProfile();
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "name")) {
            p.name = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "locale")) {
            p.locale = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "timezone")) {
            p.timezone = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "registry_redirection")) {
            p.registryRedirection = clampMode((int)*v, 2);
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "ui_language_mode")) {
            p.uiLanguageMode = clampMode((int)*v, 2);
        }
        if (const auto v = tomlkit::tryFind<bool>(tbl, "run_as_admin")) {
            p.runAsAdmin = *v;
        }
        if (const auto v = tomlkit::tryFind<bool>(tbl, "run_with_suspend")) {
            p.runWithSuspend = *v;
        }
        if (const auto v = tomlkit::tryFind<bool>(tbl, "show_in_main_menu")) {
            p.showInMainMenu = *v;
        }
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "arguments")) {
            p.arguments = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "ansi_code_page"); v && *v > 0) {
            p.ansiCodePage = (uint32_t)*v;
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "oem_code_page"); v && *v > 0) {
            p.oemCodePage = (uint32_t)*v;
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "locale_id"); v && *v > 0) {
            p.localeId = (uint32_t)*v;
        }
        if (const auto v = tomlkit::tryFind<int64_t>(tbl, "font_charset"); v && *v >= 0) {
            p.fontCharset = (uint32_t)*v;
        }
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "standard_name_override")) {
            p.standardNameOverride = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<std::string>(tbl, "daylight_name_override")) {
            p.daylightNameOverride = utf82Wide(*v);
        }
        return p;
    }

    tomlkit::Item buildProfileTable(const Profile& p, const bool includeMenuFlag, const std::optional<std::wstring_view> targetPath = std::nullopt)
    {
        auto tbl = tomlkit::Item::table();
        if (targetPath) {
            tbl["target_path"] = wide2Utf8(*targetPath);
        }
        if (!p.name.empty()) {
            tbl["name"] = wide2Utf8(p.name);
        }
        tbl["locale"] = wide2Utf8(p.locale);
        tbl["timezone"] = wide2Utf8(p.timezone);
        tbl["registry_redirection"] = (int64_t)clampMode(p.registryRedirection, 2);
        tbl["ui_language_mode"] = (int64_t)clampMode(p.uiLanguageMode, 2);
        tbl["run_as_admin"] = p.runAsAdmin;
        tbl["run_with_suspend"] = p.runWithSuspend;
        if (includeMenuFlag) {
            tbl["show_in_main_menu"] = p.showInMainMenu;
        }
        tbl["arguments"] = wide2Utf8(p.arguments);

        if (p.ansiCodePage) {
            tbl["ansi_code_page"] = (int64_t)*p.ansiCodePage;
        }
        if (p.oemCodePage) {
            tbl["oem_code_page"] = (int64_t)*p.oemCodePage;
        }
        if (p.localeId) {
            tbl["locale_id"] = (int64_t)*p.localeId;
        }
        if (p.fontCharset) {
            tbl["font_charset"] = (int64_t)*p.fontCharset;
        }
        if (!p.standardNameOverride.empty()) {
            tbl["standard_name_override"] = wide2Utf8(p.standardNameOverride);
        }
        if (!p.daylightNameOverride.empty()) {
            tbl["daylight_name_override"] = wide2Utf8(p.daylightNameOverride);
        }
        return tbl;
    }

    GlobalConfig parseGlobalDocument(const tomlkit::Item& doc)
    {
        GlobalConfig cfg{};
        if (const auto v = tomlkit::tryFind<std::string>(doc, "version")) {
            cfg.version = utf82Wide(*v);
        }
        if (const auto v = tomlkit::tryFind<std::string>(doc, "language")) {
            cfg.language = utf82Wide(*v);
        }

        if (const auto* pProfiles = doc.find("profiles"); pProfiles && (pProfiles->isArray() || pProfiles->isArrayOfTables())) {
            for (const auto& elem : *pProfiles) {
                if (elem.isTable()) {
                    cfg.profiles.push_back(parseProfileTable(elem));
                }
            }
        }

        if (const auto* pApps = doc.find("app_profiles"); pApps && (pApps->isArray() || pApps->isArrayOfTables())) {
            for (const auto& elem : *pApps) {
                if (!elem.isTable()) {
                    continue;
                }
                const auto targetOpt = tomlkit::tryFind<std::string>(elem, "target_path");
                if (!targetOpt || targetOpt->empty()) {
                    continue;
                }
                cfg.appProfiles.push_back(AppProfileEntry{
                    .targetPath = utf82Wide(*targetOpt),
                    .profile = parseProfileTable(elem),
                });
            }
        }

        return cfg;
    }

    std::wstring formatLastErrorMessage(const DWORD err)
    {
        wchar_t* pBuf = nullptr;
        const DWORD len = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, err, 0, (LPWSTR)&pBuf, 0, nullptr);
        std::wstring text = len > 0 && pBuf ? std::wstring(pBuf, len) : std::format(L"Win32 error {}", err);
        if (pBuf) {
            LocalFree(pBuf);
        }
        return trimW(text);
    }
}

Profile createDefaultProfile(const std::wstring_view name)
{
    return Profile{
        .name = std::wstring(name),
        .locale = L"ja-JP",
        .timezone = L"Tokyo Standard Time",
        .registryRedirection = 2,
        .uiLanguageMode = 1,
        .runAsAdmin = false,
        .runWithSuspend = false,
        .showInMainMenu = true,
        .arguments = {},
    };
}

GlobalConfig createDefaultGlobalConfig()
{
    GlobalConfig cfg{};
    cfg.version = LEP_APP_VERSION_WSTR;
    cfg.language = L"auto";

    cfg.profiles.push_back(createDefaultProfile(L"日本语"));

    Profile zhCn = createDefaultProfile(L"简体中文");
    zhCn.locale = L"zh-CN";
    zhCn.timezone = L"China Standard Time";
    zhCn.showInMainMenu = false;
    cfg.profiles.push_back(std::move(zhCn));

    Profile zhTw = createDefaultProfile(L"繁體中文 (台灣)");
    zhTw.locale = L"zh-TW";
    zhTw.timezone = L"Taipei Standard Time";
    zhTw.showInMainMenu = false;
    cfg.profiles.push_back(std::move(zhTw));

    Profile koKr = createDefaultProfile(L"한국어");
    koKr.locale = L"ko-KR";
    koKr.timezone = L"Korea Standard Time";
    koKr.showInMainMenu = false;
    cfg.profiles.push_back(std::move(koKr));

    return cfg;
}

fs::path alongsideAppConfigPath(const fs::path& targetPath)
{
    return fs::path(targetPath.wstring() + L".lep.toml");
}

std::expected<GlobalConfig, std::wstring> loadGlobalConfigReadOnly(const fs::path& configPath)
{
    std::error_code ec;
    if (!fs::is_regular_file(configPath, ec)) {
        return createDefaultGlobalConfig();
    }
    const auto parsed = tomlkit::tryParseFile(configPath);
    if (!parsed) {
        return std::unexpected(utf82Wide(parsed.error().formatDiagnostic()));
    }
    GlobalConfig cfg = parseGlobalDocument(*parsed);
    if (cfg.profiles.empty()) {
        cfg.profiles = createDefaultGlobalConfig().profiles;
    }
    return cfg;
}

std::expected<GlobalConfig, std::wstring> loadOrCreateGlobalConfig(const fs::path& configPath)
{
    std::error_code ec;
    if (!fs::is_regular_file(configPath, ec)) {
        GlobalConfig defCfg = createDefaultGlobalConfig();
        // 首次启动尽力写入默认配置，写失败不阻塞只读场景
        (void)saveGlobalConfig(configPath, defCfg);
        return defCfg;
    }
    return loadGlobalConfigReadOnly(configPath);
}

std::expected<void, std::wstring> saveGlobalConfig(const fs::path& configPath, const GlobalConfig& config)
{
    std::error_code ec;
    if (configPath.has_parent_path()) {
        fs::create_directories(configPath.parent_path(), ec);
    }

    tomlkit::Item doc = tomlkit::Item::document();
    if (fs::is_regular_file(configPath, ec)) {
        if (auto existing = tomlkit::tryParseFile(configPath)) {
            doc = std::move(*existing);
        }
    }
    else {
        doc["version"] = wide2Utf8(LEP_APP_VERSION_WSTR);
        doc["version"].addLeadingComment("Locale Emulator Plus 全局配置文件（UTF-8 TOML）");
    }

    doc["version"] = wide2Utf8(LEP_APP_VERSION_WSTR);
    doc["language"] = wide2Utf8(config.language.empty() ? L"auto" : config.language);

    auto profilesArr = tomlkit::Item::arrayOfTables();
    for (const auto& prof : config.profiles) {
        profilesArr.push_back(buildProfileTable(prof, true));
    }
    doc["profiles"] = std::move(profilesArr);

    if (config.appProfiles.empty()) {
        doc.erase("app_profiles");
    }
    else {
        auto appsArr = tomlkit::Item::arrayOfTables();
        for (const auto& entry : config.appProfiles) {
            appsArr.push_back(buildProfileTable(entry.profile, false, entry.targetPath));
        }
        doc["app_profiles"] = std::move(appsArr);
    }

    const auto saved = tomlkit::trySaveFileAtomic(configPath, doc);
    if (!saved) {
        return std::unexpected(utf82Wide(saved.error().formatDiagnostic()));
    }
    return {};
}

std::optional<Profile> findGlobalProfileByName(const GlobalConfig& config, const std::wstring_view name)
{
    for (const auto& prof : config.profiles) {
        if (equalsIgnoreCaseW(prof.name, name)) {
            return prof;
        }
    }
    return std::nullopt;
}

std::optional<ResolvedAppProfile> findAppProfile(const fs::path& targetPath, const GlobalConfig& globalConfig)
{
    if (targetPath.empty()) {
        return std::nullopt;
    }

    // 1. 优先检查目标同目录的 <target>.lep.toml
    const fs::path sidePath = alongsideAppConfigPath(targetPath);
    std::error_code ec;
    if (fs::is_regular_file(sidePath, ec)) {
        if (const auto parsed = tomlkit::tryParseFile(sidePath)) {
            const tomlkit::Item* pTable = parsed->find("profile");
            const tomlkit::Item& sourceTable = (pTable && pTable->isTable()) ? *pTable : *parsed;
            Profile p = parseProfileTable(sourceTable);
            if (p.name.empty()) {
                p.name = targetPath.filename().wstring();
            }
            return ResolvedAppProfile{
                .profile = std::move(p),
                .storage = AppProfileStorage::AlongsideTarget,
                .configFilePath = sidePath,
            };
        }
    }

    // 2. 其次在全局 config.toml 的 [[app_profiles]] 中按规范化路径匹配
    const std::wstring targetNorm = normalizePathForMatch(targetPath);
    for (const auto& entry : globalConfig.appProfiles) {
        if (normalizePathForMatch(entry.targetPath) == targetNorm) {
            Profile p = entry.profile;
            if (p.name.empty()) {
                p.name = targetPath.filename().wstring();
            }
            return ResolvedAppProfile{
                .profile = std::move(p),
                .storage = AppProfileStorage::GlobalConfig,
                .configFilePath = {},
            };
        }
    }

    return std::nullopt;
}

std::expected<fs::path, std::wstring> saveAppProfileToFile(
    const fs::path& outputPath,
    const Profile& profile,
    const fs::path& existingTemplatePath)
{
    tomlkit::Item doc = tomlkit::Item::document();
    std::error_code ec;
    const fs::path& templatePath = existingTemplatePath.empty() ? outputPath : existingTemplatePath;
    if (fs::is_regular_file(templatePath, ec)) {
        if (auto existing = tomlkit::tryParseFile(templatePath)) {
            doc = std::move(*existing);
        }
    }
    else {
        doc["version"] = wide2Utf8(LEP_APP_VERSION_WSTR);
        doc["version"].addLeadingComment("Locale Emulator Plus 程序专属配置文件");
    }

    doc["version"] = wide2Utf8(LEP_APP_VERSION_WSTR);
    doc["profile"] = buildProfileTable(profile, false);

    const auto saved = tomlkit::trySaveFileAtomic(outputPath, doc);
    if (!saved) {
        const DWORD lastErr = GetLastError();
        if (lastErr != 0) {
            return std::unexpected(formatLastErrorMessage(lastErr));
        }
        return std::unexpected(utf82Wide(saved.error().formatDiagnostic()));
    }
    return outputPath;
}

std::expected<fs::path, std::wstring> saveAppProfileAlongside(const fs::path& targetPath, const Profile& profile)
{
    const fs::path sidePath = alongsideAppConfigPath(targetPath);
    return saveAppProfileToFile(sidePath, profile, sidePath);
}

std::expected<void, std::wstring> saveAppProfileInGlobal(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath,
    const Profile& profile)
{
    const std::wstring targetNorm = normalizePathForMatch(targetPath);
    bool updated = false;
    for (auto& entry : globalConfig.appProfiles) {
        if (normalizePathForMatch(entry.targetPath) == targetNorm) {
            entry.targetPath = targetPath.wstring();
            entry.profile = profile;
            updated = true;
            break;
        }
    }
    if (!updated) {
        globalConfig.appProfiles.push_back(AppProfileEntry{
            .targetPath = targetPath.wstring(),
            .profile = profile,
        });
    }
    return saveGlobalConfig(globalConfigPath, globalConfig);
}

std::expected<bool, std::wstring> deleteAppProfile(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath)
{
    bool removedAny = false;

    const fs::path sidePath = alongsideAppConfigPath(targetPath);
    std::error_code ec;
    if (fs::is_regular_file(sidePath, ec)) {
        if (!fs::remove(sidePath, ec) && ec) {
            return std::unexpected(utf82Wide(ec.message()));
        }
        removedAny = true;
    }

    const std::wstring targetNorm = normalizePathForMatch(targetPath);
    const size_t before = globalConfig.appProfiles.size();
    std::erase_if(globalConfig.appProfiles, [&](const AppProfileEntry& entry)
        {
            return normalizePathForMatch(entry.targetPath) == targetNorm;
        });
    if (globalConfig.appProfiles.size() != before) {
        removedAny = true;
        if (auto res = saveGlobalConfig(globalConfigPath, globalConfig); !res) {
            return std::unexpected(res.error());
        }
    }

    return removedAny;
}

NAMESPACE_END(lep)
