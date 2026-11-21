module;

#include "../LEPCommon/LEPMacros.hpp"

export module LEP.Updater;

export import std.compat;

export NAMESPACE_BEGIN(lep)

struct ReleaseUpdateInfo
{
    std::wstring latestVersion;
    std::wstring releaseUrl;
    bool hasUpdate = false;
};

// 通过 GitHub Releases API 检查最新版本（纯 WinHTTP，Win7+ 兼容；仅用于非商店便携版）
std::expected<ReleaseUpdateInfo, std::wstring> checkGithubReleaseUpdate(
    std::wstring_view owner,
    std::wstring_view repo,
    std::wstring_view currentVersion);

NAMESPACE_END(lep)
