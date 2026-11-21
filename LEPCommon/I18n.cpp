module;

#include "LEPMacros.hpp"
#include <Windows.h>

module LEPCommon.I18n;

import LEPCommon.StringUtils;

NAMESPACE_BEGIN(lep)

std::wstring I18n::detectSystemLanguage()
{
    const LANGID langId = GetUserDefaultUILanguage();
    const WORD primary = PRIMARYLANGID(langId);
    if (primary == LANG_CHINESE) {
        return L"zh-CN";
    }
    if (primary == LANG_JAPANESE) {
        return L"ja-JP";
    }
    return L"en-US";
}

void I18n::load(const fs::path& appDir, const std::wstring_view languageSetting)
{
    m_activeLanguage = (languageSetting.empty() || equalsIgnoreCaseW(languageSetting, L"auto"))
        ? detectSystemLanguage()
        : std::wstring(languageSetting);

    m_primaryDoc = tomlkit::Item::document();
    m_fallbackDoc = tomlkit::Item::document();

    const fs::path localesDir = appDir / L"locales";
    const fs::path primaryPath = localesDir / (m_activeLanguage + L".toml");
    const fs::path fallbackPath = localesDir / L"en-US.toml";

    std::error_code ec;
    if (fs::is_regular_file(primaryPath, ec)) {
        if (auto parsed = tomlkit::tryParseFile(primaryPath)) {
            m_primaryDoc = std::move(*parsed);
        }
    }
    if (fs::is_regular_file(fallbackPath, ec)) {
        if (auto parsed = tomlkit::tryParseFile(fallbackPath)) {
            m_fallbackDoc = std::move(*parsed);
        }
    }
}

std::wstring I18n::tr(const std::string_view section, const std::string_view key, const std::wstring_view fallback) const
{
    if (const auto val = tomlkit::tryFind<std::string>(m_primaryDoc, section, key)) {
        return utf82Wide(*val);
    }
    if (const auto val = tomlkit::tryFind<std::string>(m_fallbackDoc, section, key)) {
        return utf82Wide(*val);
    }
    if (!fallback.empty()) {
        return std::wstring(fallback);
    }
    return utf82Wide(key);
}

std::wstring I18n::trFormat(
    const std::string_view section,
    const std::string_view key,
    const std::wstring_view placeholder,
    const std::wstring_view value,
    const std::wstring_view fallback) const
{
    return replaceAllW(tr(section, key, fallback), placeholder, value);
}

std::vector<LanguageOption> I18n::availableLanguages(const fs::path& appDir)
{
    std::vector<LanguageOption> options = {
        { L"auto",  L"Auto / 跟随系统" },
        { L"zh-CN", L"简体中文 (zh-CN)" },
        { L"en-US", L"English (en-US)" },
        { L"ja-JP", L"日本語 (ja-JP)" },
    };

    const fs::path localesDir = appDir / L"locales";
    std::error_code ec;
    if (fs::is_directory(localesDir, ec)) {
        for (const auto& entry : fs::directory_iterator(localesDir, ec)) {
            if (!entry.is_regular_file() || !equalsIgnoreCaseW(entry.path().extension().wstring(), L".toml")) {
                continue;
            }
            const std::wstring stem = entry.path().stem().wstring();
            const bool exists = std::ranges::any_of(options, [&](const LanguageOption& opt)
                {
                    return equalsIgnoreCaseW(opt.code, stem);
                });
            if (!exists) {
                options.push_back(LanguageOption{ .code = stem, .displayName = stem });
            }
        }
    }
    return options;
}

NAMESPACE_END(lep)
