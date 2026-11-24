using System;
using System.Collections.Generic;
using System.IO;
using LEPWinUI.Models;
using LEPWinUI.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using Windows.ApplicationModel.DataTransfer;

namespace LEPWinUI.Pages;

public sealed partial class LaunchPage : Page
{
    private sealed record LaunchChoice(bool IsAppProfile, string Name, string Display);

    private MainWindow? _hostWindow;
    private readonly AppState _state = App.State;
    private bool _suppressSelection;
    private List<LaunchChoice> _choices = new();
    private int _appProfileCount;

    public LaunchPage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        _hostWindow = e.Parameter as MainWindow;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _state.ConfigChanged += RefreshState;
        ApplyLocalization();
        if (!string.IsNullOrWhiteSpace(_state.CurrentLaunchTarget))
        {
            TargetPathBox.Text = _state.CurrentLaunchTarget;
        }
        RefreshState();
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        _state.ConfigChanged -= RefreshState;
    }

    private void ApplyLocalization()
    {
        var i18n = _state.I18n;
        PageTitleText.Text = i18n.Tr("tabs", "launch", "启动与快捷方式");
        TargetGroupHeader.Text = i18n.Tr("launch", "target_group");
        TargetPathBox.Header = i18n.Tr("launch", "target_path");
        BrowseBtn.Content = i18n.Tr("launch", "browse");
        ProfileCombo.Header = i18n.Tr("launch", "profile_label");
        ExtraArgsBox.Header = i18n.Tr("launch", "extra_args");
        CommandPreviewBox.Header = i18n.Tr("launch", "command_preview");
        CopyCmdBtn.Content = i18n.Tr("launch", "copy_command");
        AppProfileHeader.Text = i18n.Tr("profiles", "app_editor_title");
        DeleteAppProfileBtn.Content = i18n.Tr("launch", "delete_app_profile");
        RunNowBtn.Content = i18n.Tr("launch", "run_now");
        CreateShortcutBtn.Content = i18n.Tr("launch", "create_shortcut");
    }

    private void RefreshState()
    {
        var i18n = _state.I18n;
        string target = TargetPathBox.Text.Trim();
        _state.CurrentLaunchTarget = target;

        var appProfs = !string.IsNullOrEmpty(target) ? ConfigService.FindAppProfiles(target, _state.Config) : null;

        _suppressSelection = true;
        int prevSel = ProfileCombo.SelectedIndex;
        var choices = new List<LaunchChoice>();
        _appProfileCount = 0;

        if (appProfs is not null && appProfs.Profiles.Count > 0)
        {
            _appProfileCount = appProfs.Profiles.Count;
            bool multi = _appProfileCount > 1;
            string fileName = Path.GetFileName(target);
            foreach (var p in appProfs.Profiles)
            {
                bool showNamed = multi || (!string.IsNullOrWhiteSpace(p.Name) && !p.Name.Equals(fileName, StringComparison.OrdinalIgnoreCase));
                string display = showNamed
                    ? i18n.TrFormat("launch", "app_profile_item_named", "{name}", p.Name, $"[专属] {p.Name}")
                    : i18n.Tr("launch", "app_profile_item");
                choices.Add(new LaunchChoice(IsAppProfile: true, Name: p.Name, Display: display));
            }
        }
        foreach (var p in _state.Config.Profiles)
        {
            choices.Add(new LaunchChoice(IsAppProfile: false, Name: p.Name, Display: p.Name));
        }

        _choices = choices;
        ProfileCombo.ItemsSource = choices.ConvertAll(c => c.Display);
        if (choices.Count > 0)
        {
            ProfileCombo.SelectedIndex = (prevSel >= 0 && prevSel < choices.Count) ? prevSel : 0;
        }
        _suppressSelection = false;

        if (appProfs is not null && appProfs.Profiles.Count > 0)
        {
            string statusText = appProfs.Storage == AppProfileStorage.AlongsideTarget
                ? i18n.Tr("launch", "app_profile_status_local") + Path.GetFileName(appProfs.ConfigFilePath)
                : i18n.Tr("launch", "app_profile_status_global");
            if (appProfs.Profiles.Count > 1)
            {
                statusText += $" ({appProfs.Profiles.Count})";
            }
            AppProfileStatusText.Text = statusText;
            EditAppProfileBtn.Content = i18n.Tr("launch", "edit_app_profile");
            DeleteAppProfileBtn.IsEnabled = true;
        }
        else
        {
            AppProfileStatusText.Text = i18n.Tr("launch", "app_profile_status_none");
            EditAppProfileBtn.Content = i18n.Tr("launch", "create_app_profile");
            DeleteAppProfileBtn.IsEnabled = false;
        }

        EditAppProfileBtn.IsEnabled = !string.IsNullOrEmpty(target);
        UpdateCommandPreview();
    }

    private (string? GlobalProfile, string? AppProfile) GetSelectedProfileArgs()
    {
        int sel = ProfileCombo.SelectedIndex;
        if (sel < 0 || sel >= _choices.Count)
        {
            return (null, null);
        }
        var choice = _choices[sel];
        if (choice.IsAppProfile)
        {
            return (null, _appProfileCount > 1 ? choice.Name : null);
        }
        return (choice.Name, null);
    }

    private void UpdateCommandPreview()
    {
        string target = TargetPathBox.Text.Trim();
        if (string.IsNullOrEmpty(target))
        {
            CommandPreviewBox.Text = "";
            return;
        }
        var (globalProf, appProf) = GetSelectedProfileArgs();
        CommandPreviewBox.Text = NativeBridgeService.BuildLaunchCommandPreview(_state.AppDir, target, globalProf, ExtraArgsBox.Text, appProf);
    }

    private void ShowInfo(string message, InfoBarSeverity severity)
    {
        FeedbackBar.Severity = severity;
        FeedbackBar.Message = message;
        FeedbackBar.IsOpen = true;
    }

    private void TargetPathBox_TextChanged(object sender, TextChangedEventArgs e) => RefreshState();

    private void ExtraArgsBox_TextChanged(object sender, TextChangedEventArgs e) => UpdateCommandPreview();

    private void ProfileCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_suppressSelection)
        {
            UpdateCommandPreview();
        }
    }

    private void BrowseBtn_Click(object sender, RoutedEventArgs e)
    {
        IntPtr hwnd = _hostWindow?.Hwnd ?? IntPtr.Zero;
        string? initDir = null;
        if (!string.IsNullOrWhiteSpace(TargetPathBox.Text))
        {
            initDir = Path.GetDirectoryName(TargetPathBox.Text.Trim());
        }
        string? picked = SystemService.BrowseOpenTargetFile(hwnd, initDir);
        if (!string.IsNullOrWhiteSpace(picked))
        {
            TargetPathBox.Text = picked;
        }
    }

    private void CopyCmdBtn_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(CommandPreviewBox.Text))
        {
            return;
        }
        var pkg = new DataPackage();
        pkg.SetText(CommandPreviewBox.Text);
        Clipboard.SetContent(pkg);
        ShowInfo(_state.I18n.Tr("launch", "copied_command"), InfoBarSeverity.Success);
    }

    private void EditAppProfileBtn_Click(object sender, RoutedEventArgs e)
    {
        string target = TargetPathBox.Text.Trim();
        if (string.IsNullOrEmpty(target))
        {
            ShowInfo(_state.I18n.Tr("launch", "no_target"), InfoBarSeverity.Warning);
            return;
        }
        _hostWindow?.OpenAppProfileWindow(target);
    }

    private async void DeleteAppProfileBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        string target = TargetPathBox.Text.Trim();
        if (string.IsNullOrEmpty(target))
        {
            return;
        }
        var dlg = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = i18n.Tr("app", "confirm"),
            Content = i18n.Tr("profiles", "confirm_delete_app"),
            PrimaryButtonText = i18n.Tr("profiles", "delete"),
            CloseButtonText = i18n.Tr("app", "close", "关闭"),
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dlg.ShowAsync() != ContentDialogResult.Primary)
        {
            return;
        }

        var res = await NativeBridgeService.DeleteAppProfileWithElevationAsync(
            _state.AppDir, _state.ConfigPath, _state.Config, target);
        if (!res.Ok)
        {
            ShowInfo(res.Error, InfoBarSeverity.Error);
            return;
        }
        _state.NotifyConfigChanged();
        ShowInfo(i18n.Tr("profiles", "app_profile_deleted"), InfoBarSeverity.Success);
    }

    private void RunNowBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        string target = TargetPathBox.Text.Trim();
        if (string.IsNullOrEmpty(target))
        {
            ShowInfo(i18n.Tr("launch", "no_target"), InfoBarSeverity.Warning);
            return;
        }

        var (globalProf, appProf) = GetSelectedProfileArgs();
        var res = NativeBridgeService.LaunchTarget(_state.AppDir, target, globalProf, ExtraArgsBox.Text, appProf);
        if (!res.Ok)
        {
            ShowInfo(i18n.Tr("launch", "launch_failed") + res.Error, InfoBarSeverity.Error);
        }
        else
        {
            ShowInfo(i18n.Tr("launch", "launch_ok"), InfoBarSeverity.Success);
        }
    }

    private async void CreateShortcutBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        string target = TargetPathBox.Text.Trim();
        if (string.IsNullOrEmpty(target))
        {
            ShowInfo(i18n.Tr("launch", "no_target"), InfoBarSeverity.Warning);
            return;
        }

        string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        string suggested = Path.Combine(desktop, $"{Path.GetFileNameWithoutExtension(target)} (LEP).lnk");
        string? outPath = SystemService.BrowseSaveShortcutFile(_hostWindow?.Hwnd ?? IntPtr.Zero, suggested);
        if (string.IsNullOrWhiteSpace(outPath))
        {
            return;
        }

        var (globalProf, appProf) = GetSelectedProfileArgs();
        var res = await NativeBridgeService.CreateShortcutAsync(_state.AppDir, target, outPath, globalProf, ExtraArgsBox.Text, appProf);
        if (!res.Ok)
        {
            ShowInfo(i18n.Tr("launch", "shortcut_failed") + res.OutputOrError, InfoBarSeverity.Error);
        }
        else
        {
            ShowInfo(i18n.Tr("launch", "shortcut_created") + outPath, InfoBarSeverity.Success);
        }
    }
}
