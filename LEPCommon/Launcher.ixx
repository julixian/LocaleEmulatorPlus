module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.Launcher;

export import std.compat;
export import LocaleEmulatorPlus;
export import LEPCommon.Config;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

struct LaunchRequest
{
    fs::path appDir;              // LEP.exe 所在目录（含 LoaderDll_*.dll 与 LEPHelper_x86.exe）
    fs::path targetPath;          // 要运行的目标文件（.exe / .lnk / 关联文件）
    Profile profile;              // 最终生效的转区配置
    std::wstring extraArguments;  // 命令行额外透传参数（仅在 profile.arguments 为空时生效，或追加）
    bool waitForExit = false;     // CLI --wait 时等待目标退出
    bool confirmSuspended = true; // 挂起模式是否弹窗提醒
    HWND ownerWindow = nullptr;
};

struct LaunchResult
{
    uint32_t processId = 0;
    uint32_t exitCode = 0; // 仅在 waitForExit == true 时有效
};

// 根据 Profile 构造 Core ABI v4 的 LEPB 结构体
std::expected<LEP::LEPB, std::wstring> buildEnvironmentBlock(const Profile& profile);

// 启动目标程序（自动识别 x86/x64 架构、处理 .lnk 和文件关联、自动 UAC 提权）
std::expected<LaunchResult, std::wstring> launchTarget(const LaunchRequest& req);

NAMESPACE_END(lep)
