module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.Shortcut;

export import std.compat;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

struct ShortcutRequest
{
    fs::path lepExePath;
    fs::path targetPath;
    fs::path shortcutOutputPath;
    std::optional<std::wstring> profileName; // nullopt 表示使用默认优先级（程序专属 > 首个全局）
    std::wstring extraArguments;
    std::wstring description;
};

// 生成简洁直观、可直接复制使用的 LEP.exe 命令行（仅参数部分或完整命令行）
std::wstring buildLaunchArguments(
    const fs::path& targetPath,
    std::optional<std::wstring_view> profileName = std::nullopt,
    std::wstring_view extraArguments = L"");

std::wstring buildFullLaunchCommandLine(
    const fs::path& lepExePath,
    const fs::path& targetPath,
    std::optional<std::wstring_view> profileName = std::nullopt,
    std::wstring_view extraArguments = L"");

// 创建标准 Windows .lnk 快捷方式，自动沿用目标程序图标与工作目录
std::expected<fs::path, std::wstring> createShortcut(const ShortcutRequest& req);

// 推荐快捷方式默认保存路径（目标目录可写则同目录，只读/ISO 自动回退桌面）
fs::path suggestDefaultShortcutPath(const fs::path& targetPath);

NAMESPACE_END(lep)
