using System;
using System.IO;
using System.Threading.Tasks;
using LEPWinUI.Models;
using LEPWinUI.Services;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace LEPWinUI;

public sealed partial class AppProfileWindow : Window
{
    private readonly AppState _state;
    private readonly string _targetPath;
    private readonly DirectoryWriteStatus _dirStatus;
    private LepProfile _currentProfile;
    private AppProfileStorage _existingStorage = AppProfileStorage.None;

    public AppProfileWindow(AppState state, string targetPath)
    {
        _state = state;
        _targetPath = Path.GetFullPath(targetPath);
        _dirStatus = SystemService.ProbeDirectoryWriteStatus(Path.GetDirectoryName(_targetPath));

        var existing = ConfigService.FindAppProfile(_targetPath, _state.Config);
        if (existing is not null)
        {
            _currentProfile = existing.Profile;
            _existingStorage = existing.Storage;
        }
        else
        {
            _currentProfile = _state.Config.Profiles.Count > 0
                ? _state.Config.Profiles[0].Clone()
                : ConfigService.CreateDefaultProfile();
            _currentProfile.Name = Path.GetFileName(_targetPath);
        }

        InitializeComponent();
        TryApplyBackdrop();
        ConfigureChrome();
        PopulateControls();
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

    private void ConfigureChrome()
    {
        var i18n = _state.I18n;
        string title = $"{i18n.Tr("profiles", "app_editor_title", "程序专属配置")} - {Path.GetFileName(_targetPath)}";
        Title = title;
        TitleBarText.Text = title;
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        double scale = SystemService.GetWindowScaleFactor(WinRT.Interop.WindowNative.GetWindowHandle(this));
        int targetW = (int)Math.Round(780 * scale);
        int targetH = (int)Math.Round(710 * scale);
        var display = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        if (display is not null)
        {
            targetW = Math.Min(targetW, (int)(display.WorkArea.Width * 0.92));
            targetH = Math.Min(targetH, (int)(display.WorkArea.Height * 0.92));
        }
        AppWindow.Resize(new SizeInt32(targetW, targetH));
        string iconPath = Path.Combine(_state.AppDir, "Assets", "icon.ico");
        if (File.Exists(iconPath))
        {
            AppWindow.SetIcon(iconPath);
        }

        if (display is not null)
        {
            var work = display.WorkArea;
            AppWindow.Move(new PointInt32(
                Math.Max(0, work.X + (work.Width - AppWindow.Size.Width) / 2),
                Math.Max(0, work.Y + (work.Height - AppWindow.Size.Height) / 2)));
        }
    }

    private void PopulateControls()
    {
        var i18n = _state.I18n;
        TargetPathBox.Header = i18n.Tr("launch", "target_path", "程序路径");
        TargetPathBox.Text = _targetPath;

        LocaleCombo.Header = i18n.Tr("profiles", "locale", "区域与代码页");
        LocaleCombo.ItemsSource = _state.Locales;
        int locIdx = _state.Locales.FindIndex(l => l.LocaleName.Equals(_currentProfile.Locale, StringComparison.OrdinalIgnoreCase));
        LocaleCombo.SelectedIndex = locIdx >= 0 ? locIdx : 0;

        TimezoneCombo.Header = i18n.Tr("profiles", "timezone", "时区");
        TimezoneCombo.ItemsSource = _state.Timezones;
        int tzIdx = _state.Timezones.FindIndex(t => t.Id.Equals(_currentProfile.Timezone, StringComparison.OrdinalIgnoreCase));
        TimezoneCombo.SelectedIndex = tzIdx >= 0 ? tzIdx : 0;

        RegModeCombo.Header = i18n.Tr("profiles", "reg_mode", "注册表重定向");
        RegModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "reg_mode_0"),
            i18n.Tr("profiles", "reg_mode_1"),
            i18n.Tr("profiles", "reg_mode_2"),
        };
        RegModeCombo.SelectedIndex = Math.Clamp(_currentProfile.RegistryRedirection, 0, 2);

        UiModeCombo.Header = i18n.Tr("profiles", "ui_mode", "界面语言模拟");
        UiModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "ui_mode_0"),
            i18n.Tr("profiles", "ui_mode_1"),
            i18n.Tr("profiles", "ui_mode_2"),
        };
        UiModeCombo.SelectedIndex = Math.Clamp(_currentProfile.UiLanguageMode, 0, 2);

        ArgsBox.Header = i18n.Tr("profiles", "arguments", "默认附加参数");
        ArgsBox.Text = _currentProfile.Arguments;

        StdOverrideBox.Header = i18n.Tr("profiles", "tz_std_override", "标准时区名覆盖");
        StdOverrideBox.Text = _currentProfile.StandardNameOverride;

        DstOverrideBox.Header = i18n.Tr("profiles", "tz_dst_override", "夏令时名覆盖");
        DstOverrideBox.Text = _currentProfile.DaylightNameOverride;

        RunAsAdminToggle.Header = i18n.Tr("profiles", "run_as_admin", "以管理员权限运行");
        RunAsAdminToggle.IsOn = _currentProfile.RunAsAdmin;

        RunSuspendedToggle.Header = i18n.Tr("profiles", "run_with_suspend", "创建后保持挂起（调试附加用）");
        RunSuspendedToggle.IsOn = _currentProfile.RunWithSuspend;

        StorageGroupTitle.Text = i18n.Tr("profiles", "storage_group", "保存位置");
        string localText = i18n.Tr("profiles", "storage_alongside");
        if (_dirStatus == DirectoryWriteStatus.NeedsAdmin)
        {
            localText += $" [{i18n.Tr("profiles", "tag_needs_admin", "需要管理员权限")}]";
        }
        else if (_dirStatus == DirectoryWriteStatus.ReadOnly)
        {
            localText += $" [{i18n.Tr("profiles", "tag_readonly", "只读目录")}]";
        }
        LocalStorageRadio.Content = localText;
        GlobalStorageRadio.Content = i18n.Tr("profiles", "storage_global");

        bool defaultGlobal = _existingStorage == AppProfileStorage.GlobalConfig ||
            (_existingStorage == AppProfileStorage.None && _dirStatus != DirectoryWriteStatus.Writable);
        GlobalStorageRadio.IsChecked = defaultGlobal;
        LocalStorageRadio.IsChecked = !defaultGlobal;

        DeleteBtn.Content = i18n.Tr("launch", "delete_app_profile", "删除此程序的专属配置");
        DeleteBtn.IsEnabled = _existingStorage != AppProfileStorage.None;
        SaveBtn.Content = i18n.Tr("profiles", "save", "保存配置");
        SaveAndRunBtn.Content = i18n.Tr("profiles", "save_and_run", "保存并运行");
        CloseBtn.Content = i18n.Tr("app", "close", "关闭");
    }

    private LepProfile ReadFromForm()
    {
        var p = _currentProfile.Clone();
        if (string.IsNullOrWhiteSpace(p.Name))
        {
            p.Name = Path.GetFileName(_targetPath);
        }
        if (LocaleCombo.SelectedItem is LocaleEntry loc) p.Locale = loc.LocaleName;
        if (TimezoneCombo.SelectedItem is TimezoneEntry tz) p.Timezone = tz.Id;
        p.RegistryRedirection = Math.Clamp(RegModeCombo.SelectedIndex, 0, 2);
        p.UiLanguageMode = Math.Clamp(UiModeCombo.SelectedIndex, 0, 2);
        p.Arguments = ArgsBox.Text.Trim();
        p.StandardNameOverride = StdOverrideBox.Text.Trim();
        p.DaylightNameOverride = DstOverrideBox.Text.Trim();
        p.RunAsAdmin = RunAsAdminToggle.IsOn;
        p.RunWithSuspend = RunSuspendedToggle.IsOn;
        return p;
    }

    private void ShowStatus(string message, InfoBarSeverity severity)
    {
        StatusInfoBar.Severity = severity;
        StatusInfoBar.Message = message;
        StatusInfoBar.IsOpen = true;
    }

    private async Task<bool> SaveProfileAsync()
    {
        var i18n = _state.I18n;
        var profile = ReadFromForm();
        bool wantGlobal = GlobalStorageRadio.IsChecked == true;

        if (!wantGlobal)
        {
            bool savedLocal = false;
            try
            {
                ConfigService.SaveAppProfileAlongside(_targetPath, profile);
                savedLocal = true;
            }
            catch
            {
                if (_dirStatus == DirectoryWriteStatus.NeedsAdmin && !SystemService.IsAdministrator())
                {
                    var dlg = new ContentDialog
                    {
                        XamlRoot = RootGrid.XamlRoot,
                        Title = i18n.Tr("launch", "admin_dir_title", "目标目录需要管理员权限"),
                        Content = i18n.Tr("launch", "admin_dir_ask"),
                        PrimaryButtonText = i18n.Tr("profiles", "save", "管理员写入同目录"),
                        SecondaryButtonText = i18n.Tr("profiles", "storage_global", "改存到全局配置"),
                        CloseButtonText = i18n.Tr("app", "close", "关闭"),
                        DefaultButton = ContentDialogButton.Primary,
                    };
                    var choice = await dlg.ShowAsync();
                    if (choice == ContentDialogResult.Primary)
                    {
                        var elev = await NativeBridgeService.WriteAppProfileElevatedAsync(_state.AppDir, _targetPath, profile);
                        if (!elev.Ok)
                        {
                            ShowStatus(elev.Error, InfoBarSeverity.Error);
                            return false;
                        }
                        savedLocal = true;
                    }
                    else if (choice == ContentDialogResult.Secondary)
                    {
                        wantGlobal = true;
                        GlobalStorageRadio.IsChecked = true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    var dlg = new ContentDialog
                    {
                        XamlRoot = RootGrid.XamlRoot,
                        Title = i18n.Tr("launch", "readonly_dir_title", "目标目录不可写入"),
                        Content = i18n.Tr("launch", "readonly_dir_ask"),
                        PrimaryButtonText = i18n.Tr("app", "confirm", "确认"),
                        CloseButtonText = i18n.Tr("app", "close", "关闭"),
                        DefaultButton = ContentDialogButton.Primary,
                    };
                    if (await dlg.ShowAsync() != ContentDialogResult.Primary)
                    {
                        return false;
                    }
                    wantGlobal = true;
                    GlobalStorageRadio.IsChecked = true;
                }
            }

            if (savedLocal)
            {
                ConfigService.RemoveGlobalAppProfileEntry(_state.ConfigPath, _state.Config, _targetPath);
                _existingStorage = AppProfileStorage.AlongsideTarget;
                _currentProfile = profile;
                DeleteBtn.IsEnabled = true;
                _state.NotifyConfigChanged();
                ShowStatus($"{i18n.Tr("profiles", "saved_ok")} ({Path.GetFileName(ConfigService.AlongsideAppConfigPath(_targetPath))})", InfoBarSeverity.Success);
                return true;
            }
        }

        if (wantGlobal)
        {
            try
            {
                ConfigService.SaveAppProfileInGlobal(_state.ConfigPath, _state.Config, _targetPath, profile);
                string oldSide = ConfigService.AlongsideAppConfigPath(_targetPath);
                if (File.Exists(oldSide))
                {
                    try
                    {
                        File.Delete(oldSide);
                    }
                    catch (UnauthorizedAccessException)
                    {
                        await NativeBridgeService.RunLepCommandAsync(
                            _state.AppDir,
                            $"--delete-profile-file {SystemService.QuoteArg(oldSide)}",
                            elevate: !SystemService.IsAdministrator());
                    }
                    catch
                    {
                    }
                }
                _existingStorage = AppProfileStorage.GlobalConfig;
                _currentProfile = profile;
                DeleteBtn.IsEnabled = true;
                _state.NotifyConfigChanged();
                ShowStatus($"{i18n.Tr("profiles", "saved_ok")} (config.toml)", InfoBarSeverity.Success);
                return true;
            }
            catch (Exception ex)
            {
                ShowStatus(ex.Message, InfoBarSeverity.Error);
                return false;
            }
        }

        return false;
    }

    private async void SaveBtn_Click(object sender, RoutedEventArgs e)
    {
        await SaveProfileAsync();
    }

    private async void SaveAndRunBtn_Click(object sender, RoutedEventArgs e)
    {
        if (await SaveProfileAsync())
        {
            var res = NativeBridgeService.LaunchTarget(_state.AppDir, _targetPath);
            if (!res.Ok)
            {
                ShowStatus(res.Error, InfoBarSeverity.Error);
            }
            else
            {
                Close();
            }
        }
    }

    private async void DeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        var dlg = new ContentDialog
        {
            XamlRoot = RootGrid.XamlRoot,
            Title = i18n.Tr("app", "confirm", "确认"),
            Content = i18n.Tr("profiles", "confirm_delete_app"),
            PrimaryButtonText = i18n.Tr("profiles", "delete", "删除"),
            CloseButtonText = i18n.Tr("app", "close", "关闭"),
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dlg.ShowAsync() != ContentDialogResult.Primary)
        {
            return;
        }

        var res = await NativeBridgeService.DeleteAppProfileWithElevationAsync(
            _state.AppDir, _state.ConfigPath, _state.Config, _targetPath);
        if (!res.Ok)
        {
            ShowStatus(res.Error, InfoBarSeverity.Error);
            return;
        }

        _existingStorage = AppProfileStorage.None;
        DeleteBtn.IsEnabled = false;
        _state.NotifyConfigChanged();
        ShowStatus(i18n.Tr("profiles", "app_profile_deleted"), InfoBarSeverity.Success);
    }

    private void CloseBtn_Click(object sender, RoutedEventArgs e) => Close();
}
