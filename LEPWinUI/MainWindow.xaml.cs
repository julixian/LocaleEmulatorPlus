using System;
using System.IO;
using System.Linq;
using LEPWinUI.Pages;
using LEPWinUI.Services;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.ApplicationModel.DataTransfer;
using Windows.Graphics;
using Windows.Storage;
using WinRT.Interop;

namespace LEPWinUI;

public sealed partial class MainWindow : Window
{
    private readonly AppState _state;
    private string _currentTag = "launch";

    public IntPtr Hwnd => WindowNative.GetWindowHandle(this);

    public MainWindow(AppState state)
    {
        _state = state;
        InitializeComponent();

        TryApplyBackdrop();
        ConfigureWindowChrome();
        ApplyLocalization();

        _state.LanguageChanged += OnLanguageChanged;
        Closed += (_, _) => _state.LanguageChanged -= OnLanguageChanged;

        NavView.SelectedItem = NavLaunchItem;
        NavigateTo("launch");
    }

    private void TryApplyBackdrop()
    {
        if (MicaController.IsSupported())
        {
            SystemBackdrop = new MicaBackdrop { Kind = MicaKind.Base };
        }
        else if (DesktopAcrylicController.IsSupported())
        {
            SystemBackdrop = new DesktopAcrylicBackdrop();
        }
    }

    private void ConfigureWindowChrome()
    {
        Title = $"Locale Emulator Plus v{SystemService.AppVersion}";
        VersionBadgeText.Text = $"v{SystemService.AppVersion}";

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        double scale = SystemService.GetWindowScaleFactor(Hwnd);
        int targetW = (int)Math.Round(1140 * scale);
        int targetH = (int)Math.Round(760 * scale);
        var displayArea = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        if (displayArea is not null)
        {
            targetW = Math.Min(targetW, (int)(displayArea.WorkArea.Width * 0.92));
            targetH = Math.Min(targetH, (int)(displayArea.WorkArea.Height * 0.92));
        }
        AppWindow.Resize(new SizeInt32(targetW, targetH));
        string iconPath = Path.Combine(_state.AppDir, "Assets", "icon.ico");
        if (File.Exists(iconPath))
        {
            AppWindow.SetIcon(iconPath);
        }
        CenterOnScreen();
    }

    private void CenterOnScreen()
    {
        var displayArea = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        if (displayArea is not null)
        {
            var work = displayArea.WorkArea;
            int x = work.X + (work.Width - AppWindow.Size.Width) / 2;
            int y = work.Y + (work.Height - AppWindow.Size.Height) / 2;
            AppWindow.Move(new PointInt32(Math.Max(0, x), Math.Max(0, y)));
        }
    }

    private void ApplyLocalization()
    {
        var i18n = _state.I18n;
        TitleBarText.Text = i18n.Tr("app", "title", "Locale Emulator Plus");
        Title = $"{TitleBarText.Text} v{SystemService.AppVersion}";
        NavLaunchItem.Content = i18n.Tr("tabs", "launch", "启动与快捷方式");
        NavProfilesItem.Content = i18n.Tr("tabs", "profiles", "配置管理");
        NavShellItem.Content = i18n.Tr("tabs", "shell_menu", "右键菜单");
        NavSettingsItem.Content = i18n.Tr("tabs", "settings", "设置与关于");
    }

    private void OnLanguageChanged()
    {
        ApplyLocalization();
        NavigateTo(_currentTag, forceReload: true);
    }

    private void NavView_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItemContainer is NavigationViewItem item && item.Tag is string tag)
        {
            NavigateTo(tag);
        }
    }

    public void NavigateTo(string tag, bool forceReload = false)
    {
        _currentTag = tag;
        Type pageType = tag switch
        {
            "profiles" => typeof(ProfilesPage),
            "shell" => typeof(ShellMenuPage),
            "settings" => typeof(SettingsPage),
            _ => typeof(LaunchPage),
        };

        if (forceReload || ContentFrame.CurrentSourcePageType != pageType)
        {
            ContentFrame.Navigate(pageType, this);
        }
    }

    public void OpenAppProfileWindow(string targetPath)
    {
        var win = new AppProfileWindow(_state, targetPath);
        win.Closed += (_, _) => _state.ReloadConfig();
        win.Activate();
    }

    private void RootGrid_DragOver(object sender, DragEventArgs e)
    {
        if (e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            e.AcceptedOperation = DataPackageOperation.Copy;
        }
    }

    private async void RootGrid_Drop(object sender, DragEventArgs e)
    {
        if (!e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            return;
        }
        var items = await e.DataView.GetStorageItemsAsync();
        var file = items.OfType<StorageFile>().FirstOrDefault();
        if (file is null)
        {
            return;
        }
        _state.CurrentLaunchTarget = file.Path;
        NavView.SelectedItem = NavLaunchItem;
        NavigateTo("launch", forceReload: true);
    }
}
