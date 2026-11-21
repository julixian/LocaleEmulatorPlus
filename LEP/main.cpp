#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <shellapi.h>
#include <CommCtrl.h>

import std.compat;
import LEPCommon;
import LEP.Cli;
import LEP.Gui.Win32Ui;
import LEP.Gui.AppProfileDialog;
import LEP.Gui.MainWindow;

namespace
{
    std::vector<std::wstring> parseProcessArgs()
    {
        int argc = 0;
        LPWSTR* pArgv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::wstring> args;
        if (pArgv) {
            args.reserve((size_t)argc);
            for (int i = 0; i < argc; ++i) {
                args.emplace_back(pArgv[i]);
            }
            LocalFree(pArgv);
        }
        return args;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    lep::gui::enableProcessDpiAwareness();

    const lep::fs::path appDir = lep::getAppDirectory();
    const auto args = parseProcessArgs();
    const auto dispatch = lep::dispatchCommandLine(args, appDir);

    if (dispatch.guiMode == lep::GuiStartMode::None) {
        return dispatch.exitCode;
    }

    if (dispatch.guiMode == lep::GuiStartMode::ManageApp) {
        INITCOMMONCONTROLSEX icc{ .dwSize = sizeof(icc), .dwICC = ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        const lep::fs::path cfgPath = lep::resolveGlobalConfigPath(appDir);
        auto cfgExp = lep::loadOrCreateGlobalConfig(cfgPath);
        lep::GlobalConfig cfg = cfgExp ? std::move(*cfgExp) : lep::createDefaultGlobalConfig();

        lep::I18n i18n;
        i18n.load(appDir, cfg.language);

        const auto locales = lep::enumerateLocales();
        const auto timezones = lep::enumerateTimezones();

        lep::gui::showAppProfileDialog(
            nullptr, appDir, cfgPath, cfg, i18n, locales, timezones, dispatch.guiTargetPath);
        return 0;
    }

    lep::gui::MainWindow mainWin(appDir, dispatch.guiTargetPath);
    return mainWin.run();
}
