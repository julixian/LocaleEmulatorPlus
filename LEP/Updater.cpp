module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <winhttp.h>

module LEP.Updater;

import LEPCommon;

NAMESPACE_BEGIN(lep)

namespace
{
    struct ScopedWinHttp
    {
        HINTERNET handle = nullptr;
        ~ScopedWinHttp()
        {
            if (handle) {
                WinHttpCloseHandle(handle);
            }
        }
    };

    std::array<int, 3> parseSemver(std::wstring_view text)
    {
        while (!text.empty() && (text.front() == L'v' || text.front() == L'V' || text.front() == L' ')) {
            text.remove_prefix(1);
        }
        std::array<int, 3> parts{ 0, 0, 0 };
        size_t partIdx = 0;
        int cur = 0;
        bool hasDigit = false;
        for (const wchar_t ch : text) {
            if (ch >= L'0' && ch <= L'9') {
                cur = cur * 10 + (ch - L'0');
                hasDigit = true;
            }
            else if (ch == L'.' && partIdx < 2) {
                parts[partIdx++] = cur;
                cur = 0;
                hasDigit = false;
            }
            else {
                break;
            }
        }
        if (hasDigit && partIdx < 3) {
            parts[partIdx] = cur;
        }
        return parts;
    }

    std::optional<std::string> extractJsonStringField(const std::string_view json, const std::string_view key)
    {
        const std::string needle = std::format("\"{}\"", key);
        size_t pos = json.find(needle);
        while (pos != std::string_view::npos) {
            size_t cursor = pos + needle.size();
            while (cursor < json.size() && (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\r' || json[cursor] == '\n')) {
                ++cursor;
            }
            if (cursor < json.size() && json[cursor] == ':') {
                ++cursor;
                while (cursor < json.size() && (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\r' || json[cursor] == '\n')) {
                    ++cursor;
                }
                if (cursor < json.size() && json[cursor] == '"') {
                    ++cursor;
                    std::string out;
                    while (cursor < json.size()) {
                        const char c = json[cursor++];
                        if (c == '\\' && cursor < json.size()) {
                            const char esc = json[cursor++];
                            if (esc == '"' || esc == '\\' || esc == '/') out.push_back(esc);
                            else if (esc == 'n') out.push_back('\n');
                            else if (esc == 'r') out.push_back('\r');
                            else if (esc == 't') out.push_back('\t');
                        }
                        else if (c == '"') {
                            return out;
                        }
                        else {
                            out.push_back(c);
                        }
                    }
                    return std::nullopt;
                }
            }
            pos = json.find(needle, pos + needle.size());
        }
        return std::nullopt;
    }
}

std::expected<ReleaseUpdateInfo, std::wstring> checkGithubReleaseUpdate(
    const std::wstring_view owner,
    const std::wstring_view repo,
    const std::wstring_view currentVersion)
{
    const std::wstring userAgent = std::format(L"LocaleEmulatorPlus/{}", currentVersion);

    // Win8.1+ 优先使用自动代理（遵循系统/浏览器代理），Win7 自动降级为 DEFAULT_PROXY
    constexpr DWORD kAccessAutoProxy = 4; // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    ScopedWinHttp session{ WinHttpOpen(userAgent.c_str(), kAccessAutoProxy, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
    if (!session.handle) {
        session.handle = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.handle) {
        return std::unexpected(std::format(L"初始化 WinHTTP 失败，错误码 {}", GetLastError()));
    }

    WinHttpSetTimeouts(session.handle, 8000, 8000, 8000, 10000);
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    ScopedWinHttp connect{ WinHttpConnect(session.handle, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0) };
    if (!connect.handle) {
        return std::unexpected(std::format(L"连接 api.github.com 失败，错误码 {}", GetLastError()));
    }

    const std::wstring path = std::format(L"/repos/{}/{}/releases/latest", owner, repo);
    ScopedWinHttp request{ WinHttpOpenRequest(
        connect.handle, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) };
    if (!request.handle) {
        return std::unexpected(std::format(L"创建请求失败，错误码 {}", GetLastError()));
    }

    const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
    if (!WinHttpSendRequest(request.handle, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.handle, nullptr))
    {
        return std::unexpected(std::format(L"请求 GitHub Releases 失败，错误码 {}", GetLastError()));
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        request.handle,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode != 200) {
        return std::unexpected(std::format(L"GitHub API 返回 HTTP {}", statusCode));
    }

    std::string body;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request.handle, &avail) || avail == 0) {
            break;
        }
        std::string chunk(avail, '\0');
        DWORD readBytes = 0;
        if (!WinHttpReadData(request.handle, chunk.data(), avail, &readBytes) || readBytes == 0) {
            break;
        }
        body.append(chunk.data(), readBytes);
        if (body.size() > 512 * 1024) {
            break;
        }
    }

    const auto tagOpt = extractJsonStringField(body, "tag_name");
    if (!tagOpt || tagOpt->empty()) {
        return std::unexpected(L"无法解析 GitHub Release 版本号。");
    }

    const auto urlOpt = extractJsonStringField(body, "html_url");
    std::wstring latestVer = utf82Wide(*tagOpt);
    if (!latestVer.empty() && (latestVer.front() == L'v' || latestVer.front() == L'V')) {
        latestVer.erase(latestVer.begin());
    }

    const auto curParts = parseSemver(currentVersion);
    const auto latestParts = parseSemver(latestVer);
    const bool hasUpdate = latestParts > curParts;

    return ReleaseUpdateInfo{
        .latestVersion = std::move(latestVer),
        .releaseUrl = urlOpt ? utf82Wide(*urlOpt) : std::format(L"https://github.com/{}/{}/releases", owner, repo),
        .hasUpdate = hasUpdate,
    };
}

NAMESPACE_END(lep)
