module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.I18n;

export import std.compat;
export import tomlkitpp;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

struct LanguageOption
{
    std::wstring code;        // "auto" | "zh-CN" | "en-US" | "ja-JP"
    std::wstring displayName;
};

class I18n
{
public:
    void load(const fs::path& appDir, std::wstring_view languageSetting);

    std::wstring tr(std::string_view section, std::string_view key, std::wstring_view fallback = L"") const;
    std::wstring trFormat(
        std::string_view section,
        std::string_view key,
        std::wstring_view placeholder,
        std::wstring_view value,
        std::wstring_view fallback = L"") const;

    const std::wstring& activeLanguage() const noexcept { return m_activeLanguage; }
    static std::wstring detectSystemLanguage();
    static std::vector<LanguageOption> availableLanguages(const fs::path& appDir);

private:
    std::wstring m_activeLanguage = L"en-US";
    tomlkit::Item m_primaryDoc = tomlkit::Item::document();
    tomlkit::Item m_fallbackDoc = tomlkit::Item::document();
};

NAMESPACE_END(lep)
