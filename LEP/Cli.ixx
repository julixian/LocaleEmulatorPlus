module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>

export module LEP.Cli;

export import std.compat;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

enum class GuiStartMode
{
    None,           // 纯 CLI 执行完毕，直接以 exitCode 退出
    MainWindow,     // 打开主界面（默认首页）
    ManageApp,      // 直接打开指定程序的专属配置编辑对话框
    ShortcutTarget, // 打开首页并自动填入目标路径（准备创建快捷方式）
};

struct CliDispatchResult
{
    GuiStartMode guiMode = GuiStartMode::None;
    fs::path guiTargetPath;
    int exitCode = 0;
};

// 解析并执行命令行；若需要进入 GUI 则返回对应的 GuiStartMode
CliDispatchResult dispatchCommandLine(const std::vector<std::wstring>& args, const fs::path& appDir);

NAMESPACE_END(lep)
