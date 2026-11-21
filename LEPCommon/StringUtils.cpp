module;

#include "LEPMacros.hpp"
#include <Windows.h>

module LEPCommon.StringUtils;

NAMESPACE_BEGIN(lep)

std::wstring utf82Wide(const std::string_view utf8)
{
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring result((size_t)needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), result.data(), needed);
    return result;
}

std::string wide2Utf8(const std::wstring_view wide)
{
    if (wide.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string result((size_t)needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), result.data(), needed, nullptr, nullptr);
    return result;
}

std::wstring trimW(const std::wstring_view text)
{
    size_t start = 0;
    while (start < text.size() && (text[start] == L' ' || text[start] == L'\t' || text[start] == L'\r' || text[start] == L'\n')) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && (text[end - 1] == L' ' || text[end - 1] == L'\t' || text[end - 1] == L'\r' || text[end - 1] == L'\n')) {
        --end;
    }
    return std::wstring(text.substr(start, end - start));
}

std::wstring toLowerW(const std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    std::wstring result(text);
    CharLowerBuffW(result.data(), (DWORD)result.size());
    return result;
}

bool equalsIgnoreCaseW(const std::wstring_view a, const std::wstring_view b)
{
    if (a.size() != b.size()) {
        return false;
    }
    if (a.empty()) {
        return true;
    }
    return CompareStringOrdinal(a.data(), (int)a.size(), b.data(), (int)b.size(), TRUE) == CSTR_EQUAL;
}

std::wstring replaceAllW(const std::wstring_view text, const std::wstring_view from, const std::wstring_view to)
{
    if (from.empty()) {
        return std::wstring(text);
    }
    std::wstring result;
    result.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t hit = text.find(from, pos);
        if (hit == std::wstring_view::npos) {
            result.append(text.substr(pos));
            break;
        }
        result.append(text.substr(pos, hit - pos));
        result.append(to);
        pos = hit + from.size();
    }
    return result;
}

std::wstring quoteCommandLineArg(const std::wstring_view arg)
{
    if (arg.empty()) {
        return L"\"\"";
    }
    const bool needsQuotes = arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
    if (!needsQuotes) {
        return std::wstring(arg);
    }

    std::wstring result;
    result.reserve(arg.size() + 4);
    result.push_back(L'"');

    size_t backslashes = 0;
    for (const wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        }
        else {
            result.append(backslashes, L'\\');
            result.push_back(ch);
        }
        backslashes = 0;
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring normalizePathForMatch(const fs::path& path)
{
    std::error_code ec;
    fs::path abs = fs::weakly_canonical(path, ec);
    if (ec || abs.empty()) {
        ec.clear();
        abs = fs::absolute(path, ec);
    }
    std::wstring text = abs.empty() ? path.wstring() : abs.wstring();
    for (wchar_t& ch : text) {
        if (ch == L'/') {
            ch = L'\\';
        }
    }
    // 去掉扩展长度前缀 \\?\ 以便与普通路径一致
    if (text.starts_with(L"\\\\?\\") && text.size() > 4 && !text.starts_with(L"\\\\?\\UNC\\")) {
        text.erase(0, 4);
    }
    return toLowerW(text);
}

NAMESPACE_END(lep)
