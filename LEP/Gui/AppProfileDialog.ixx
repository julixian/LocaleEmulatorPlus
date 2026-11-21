module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>

export module LEP.Gui.AppProfileDialog;

export import std.compat;
export import LEPCommon;

export NAMESPACE_BEGIN(lep::gui)

struct AppProfileDialogResult
{
    bool changed = false; // 新增、修改或删除了配置
};

// 弹出「为此程序创建专属配置 / 编辑此程序的专属配置」模态对话框：
// - 自动探测目标目录是否可写（ISO / 只读目录自动切到「保存到全局配置文件」）
// - 「保存配置」只保存不偷偷启动，「保存并运行」明确触发启动
AppProfileDialogResult showAppProfileDialog(
    HWND owner,
    const fs::path& appDir,
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const I18n& i18n,
    const std::vector<LocaleEntry>& locales,
    const std::vector<TimezoneEntry>& timezones,
    const fs::path& targetPath);

NAMESPACE_END(lep::gui)
