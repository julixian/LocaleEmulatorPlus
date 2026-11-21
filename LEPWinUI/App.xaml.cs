using System;
using System.IO;
using LEPWinUI.Services;
using Microsoft.UI.Xaml;

namespace LEPWinUI;

public partial class App : Application
{
    public static AppState State { get; private set; } = null!;
    private Window? _mainWindow;

    public App()
    {
        InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        State = new AppState();

        string[] cmdArgs = Environment.GetCommandLineArgs();
        if (cmdArgs.Length >= 3)
        {
            string verb = cmdArgs[1].ToLowerInvariant();
            string target = cmdArgs[2];
            if (verb is "manage" or "edit")
            {
                _mainWindow = new AppProfileWindow(State, target);
                _mainWindow.Activate();
                return;
            }
            if (verb == "shortcut-ui")
            {
                State.CurrentLaunchTarget = target;
            }
        }
        else if (cmdArgs.Length == 2 && File.Exists(cmdArgs[1]))
        {
            State.CurrentLaunchTarget = cmdArgs[1];
        }

        _mainWindow = new MainWindow(State);
        _mainWindow.Activate();
    }
}
