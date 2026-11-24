using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
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
    private readonly IntPtr _ownerHwnd;
    private readonly List<LepProfile> _profiles = new();
    private int _selectedIndex;
    private bool _suppressFormEvents;
    private bool _suppressListEvents;
    private AppProfileStorage _existingStorage = AppProfileStorage.None;

    public AppProfileWindow(AppState state, string targetPath, IntPtr ownerHwnd = default)
    {
        _state = state;
        _targetPath = Path.GetFullPath(targetPath);
        _dirStatus = SystemService.ProbeDirectoryWriteStatus(Path.GetDirectoryName(_targetPath));
        _ownerHwnd = ownerHwnd;

        var existing = ConfigService.FindAppProfiles(_targetPath, _state.Config);
        if (existing is not null && existing.Profiles.Count > 0)
        {
            foreach (var p in existing.Profiles)
            {
                _profiles.Add(p.Clone());
            }
            _existingStorage = existing.Storage;
        }
        else
        {
            var def = _state.Config.Profiles.Count > 0
                ? _state.Config.Profiles[0].Clone()
                : ConfigService.CreateDefaultProfile();
            def.Name = _state.I18n.Tr("profiles", "default_app_profile_name", "默认配置");
            _profiles.Add(def);
        }

        InitializeComponent();
        TryApplyBackdrop();
        ConfigureChrome();
        PopulateStaticControls();
        RefreshProfileList();
        LoadSelectedToForm();

        if (_ownerHwnd != IntPtr.Zero)
        {
            IntPtr selfHwnd = WinRT.Interop.WindowNative.GetWindowHandle(this);
            SystemService.SetWindowOwner(selfHwnd, _ownerHwnd);
            if (AppWindow.Presenter is OverlappedPresenter presenter)
            {
                presenter.IsModal = true;
                presenter.IsMinimizable = false;
            }
            SystemService.SetWindowEnabled(_ownerHwnd, false);
            Closed += (_, _) =>
            {
                SystemService.SetWindowEnabled(_ownerHwnd, true);
                SystemService.BringToForeground(_ownerHwnd);
            };
        }
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
        int targetW = (int)Math.Round(940 * scale);
        int targetH = (int)Math.Round(720 * scale);
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

    private void PopulateStaticControls()
    {
        var i18n = _state.I18n;
        AppProfilesListHeader.Text = i18n.Tr("profiles", "app_profiles_list", "此程序的专属配置列表");
        AddProfBtn.Content = i18n.Tr("profiles", "add");
        DupProfBtn.Content = i18n.Tr("profiles", "duplicate");
        DelProfBtn.Content = i18n.Tr("profiles", "delete");
        UpProfBtn.Content = i18n.Tr("profiles", "move_up");
        DownProfBtn.Content = i18n.Tr("profiles", "move_down");

        TargetPathBox.Header = i18n.Tr("launch", "target_path", "程序路径");
        TargetPathBox.Text = _targetPath;
        NameBox.Header = i18n.Tr("profiles", "name", "配置名称：");

        LocaleCombo.Header = i18n.Tr("profiles", "locale", "区域与代码页");
        LocaleCombo.ItemsSource = _state.Locales;

        TimezoneCombo.Header = i18n.Tr("profiles", "timezone", "时区");
        TimezoneCombo.ItemsSource = _state.Timezones;

        RegModeCombo.Header = i18n.Tr("profiles", "reg_mode", "注册表重定向");
        RegModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "reg_mode_0"),
            i18n.Tr("profiles", "reg_mode_1"),
            i18n.Tr("profiles", "reg_mode_2"),
        };

        UiModeCombo.Header = i18n.Tr("profiles", "ui_mode", "界面语言模拟");
        UiModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "ui_mode_0"),
            i18n.Tr("profiles", "ui_mode_1"),
            i18n.Tr("profiles", "ui_mode_2"),
        };

        ArgsBox.Header = i18n.Tr("profiles", "arguments", "默认附加参数");
        StdOverrideBox.Header = i18n.Tr("profiles", "tz_std_override", "标准时区名覆盖");
        DstOverrideBox.Header = i18n.Tr("profiles", "tz_dst_override", "夏令时名覆盖");
        RunAsAdminToggle.Header = i18n.Tr("profiles", "run_as_admin", "以管理员权限运行");
        RunSuspendedToggle.Header = i18n.Tr("profiles", "run_with_suspend", "创建后保持挂起（调试附加用）");

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

        DeleteBtn.Content = i18n.Tr("profiles", "delete_all_app_profiles", i18n.Tr("launch", "delete_app_profile", "删除此程序的专属配置"));
        DeleteBtn.IsEnabled = _existingStorage != AppProfileStorage.None;
        SaveBtn.Content = i18n.Tr("profiles", "save", "保存配置");
        SaveAndRunBtn.Content = i18n.Tr("profiles", "save_and_run", "保存并运行");
        CloseBtn.Content = i18n.Tr("app", "close", "关闭");
    }

    private string MakeUniqueName(string baseName)
    {
        if (!_profiles.Any(p => p.Name.Equals(baseName, StringComparison.OrdinalIgnoreCase)))
        {
            return baseName;
        }
        for (int n = 2; n < 1000; n++)
        {
            string candidate = $"{baseName} ({n})";
            if (!_profiles.Any(p => p.Name.Equals(candidate, StringComparison.OrdinalIgnoreCase)))
            {
                return candidate;
            }
        }
        return baseName + " (new)";
    }

    private void RefreshProfileList()
    {
        _suppressListEvents = true;
        var labels = _profiles
            .Select((p, idx) => string.IsNullOrWhiteSpace(p.Name) ? $"#{idx + 1}" : p.Name)
            .ToList();
        AppProfilesList.ItemsSource = labels;
        if (_profiles.Count > 0)
        {
            _selectedIndex = Math.Clamp(_selectedIndex, 0, _profiles.Count - 1);
            AppProfilesList.SelectedIndex = _selectedIndex;
        }
        else
        {
            _selectedIndex = -1;
        }
        DelProfBtn.IsEnabled = _profiles.Count > 1;
        UpProfBtn.IsEnabled = _selectedIndex > 0;
        DownProfBtn.IsEnabled = _selectedIndex >= 0 && _selectedIndex + 1 < _profiles.Count;
        _suppressListEvents = false;
    }

    private void CommitFormToSelected()
    {
        if (_suppressFormEvents || _selectedIndex < 0 || _selectedIndex >= _profiles.Count)
        {
            return;
        }
        var p = _profiles[_selectedIndex];
        p.Name = NameBox.Text.Trim();
        if (LocaleCombo.SelectedItem is LocaleEntry loc) p.Locale = loc.LocaleName;
        if (TimezoneCombo.SelectedItem is TimezoneEntry tz) p.Timezone = tz.Id;
        p.RegistryRedirection = Math.Clamp(RegModeCombo.SelectedIndex, 0, 2);
        p.UiLanguageMode = Math.Clamp(UiModeCombo.SelectedIndex, 0, 2);
        p.Arguments = ArgsBox.Text.Trim();
        p.StandardNameOverride = StdOverrideBox.Text.Trim();
        p.DaylightNameOverride = DstOverrideBox.Text.Trim();
        p.RunAsAdmin = RunAsAdminToggle.IsOn;
        p.RunWithSuspend = RunSuspendedToggle.IsOn;
    }

    private void LoadSelectedToForm()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _profiles.Count)
        {
            return;
        }
        _suppressFormEvents = true;
        var p = _profiles[_selectedIndex];
        NameBox.Text = p.Name;

        int locIdx = _state.Locales.FindIndex(l => l.LocaleName.Equals(p.Locale, StringComparison.OrdinalIgnoreCase));
        LocaleCombo.SelectedIndex = locIdx >= 0 ? locIdx : 0;

        int tzIdx = _state.Timezones.FindIndex(t => t.Id.Equals(p.Timezone, StringComparison.OrdinalIgnoreCase));
        TimezoneCombo.SelectedIndex = tzIdx >= 0 ? tzIdx : 0;

        RegModeCombo.SelectedIndex = Math.Clamp(p.RegistryRedirection, 0, 2);
        UiModeCombo.SelectedIndex = Math.Clamp(p.UiLanguageMode, 0, 2);
        ArgsBox.Text = p.Arguments;
        StdOverrideBox.Text = p.StandardNameOverride;
        DstOverrideBox.Text = p.DaylightNameOverride;
        RunAsAdminToggle.IsOn = p.RunAsAdmin;
        RunSuspendedToggle.IsOn = p.RunWithSuspend;
        _suppressFormEvents = false;
    }

    private void AppProfilesList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_suppressListEvents) return;
        int newIdx = AppProfilesList.SelectedIndex;
        if (newIdx >= 0 && newIdx < _profiles.Count && newIdx != _selectedIndex)
        {
            CommitFormToSelected();
            _selectedIndex = newIdx;
            RefreshProfileList();
            LoadSelectedToForm();
        }
    }

    private void NameBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_suppressFormEvents || _selectedIndex < 0 || _selectedIndex >= _profiles.Count)
        {
            return;
        }
        _profiles[_selectedIndex].Name = NameBox.Text.Trim();
        RefreshProfileList();
    }

    private void AddProfBtn_Click(object sender, RoutedEventArgs e)
    {
        CommitFormToSelected();
        var p = _state.Config.Profiles.Count > 0
            ? _state.Config.Profiles[0].Clone()
            : ConfigService.CreateDefaultProfile();
        p.Name = MakeUniqueName(_state.I18n.Tr("profiles", "new_profile_default", "新配置"));
        _profiles.Add(p);
        _selectedIndex = _profiles.Count - 1;
        RefreshProfileList();
        LoadSelectedToForm();
    }

    private void DupProfBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _profiles.Count) return;
        CommitFormToSelected();
        var copy = _profiles[_selectedIndex].Clone();
        string baseName = string.IsNullOrWhiteSpace(copy.Name)
            ? _state.I18n.Tr("profiles", "new_profile_default", "新配置")
            : copy.Name;
        copy.Name = MakeUniqueName(baseName);
        _profiles.Add(copy);
        _selectedIndex = _profiles.Count - 1;
        RefreshProfileList();
        LoadSelectedToForm();
    }

    private void DelProfBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_profiles.Count <= 1 || _selectedIndex < 0 || _selectedIndex >= _profiles.Count) return;
        _profiles.RemoveAt(_selectedIndex);
        if (_selectedIndex >= _profiles.Count)
        {
            _selectedIndex = _profiles.Count - 1;
        }
        RefreshProfileList();
        LoadSelectedToForm();
    }

    private void UpProfBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex <= 0 || _selectedIndex >= _profiles.Count) return;
        CommitFormToSelected();
        (_profiles[_selectedIndex - 1], _profiles[_selectedIndex]) =
            (_profiles[_selectedIndex], _profiles[_selectedIndex - 1]);
        _selectedIndex--;
        RefreshProfileList();
        LoadSelectedToForm();
    }

    private void DownProfBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex + 1 >= _profiles.Count) return;
        CommitFormToSelected();
        (_profiles[_selectedIndex + 1], _profiles[_selectedIndex]) =
            (_profiles[_selectedIndex], _profiles[_selectedIndex + 1]);
        _selectedIndex++;
        RefreshProfileList();
        LoadSelectedToForm();
    }

    private void ShowStatus(string message, InfoBarSeverity severity)
    {
        StatusInfoBar.Severity = severity;
        StatusInfoBar.Message = message;
        StatusInfoBar.IsOpen = true;
    }

    private bool ValidateProfiles()
    {
        var i18n = _state.I18n;
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 0; i < _profiles.Count; i++)
        {
            string name = _profiles[i].Name.Trim();
            if (string.IsNullOrEmpty(name))
            {
                ShowStatus(i18n.Tr("profiles", "name_empty"), InfoBarSeverity.Warning);
                return false;
            }
            if (!seen.Add(name))
            {
                ShowStatus(i18n.Tr("profiles", "name_duplicate"), InfoBarSeverity.Warning);
                return false;
            }
            _profiles[i].Name = name;
        }
        return _profiles.Count > 0;
    }

    private async Task<bool> SaveProfilesAsync()
    {
        var i18n = _state.I18n;
        CommitFormToSelected();
        RefreshProfileList();
        if (!ValidateProfiles())
        {
            return false;
        }

        bool wantGlobal = GlobalStorageRadio.IsChecked == true;

        if (!wantGlobal)
        {
            bool savedLocal = false;
            try
            {
                ConfigService.SaveAppProfilesAlongside(_targetPath, _profiles);
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
                        Content = i18n.Tr("launch", "admin_dir_ask_dialog", i18n.Tr("launch", "admin_dir_ask")),
                        PrimaryButtonText = i18n.Tr("launch", "admin_save_btn", "管理员写入"),
                        SecondaryButtonText = i18n.Tr("launch", "global_save_btn", "存到全局配置"),
                        CloseButtonText = i18n.Tr("launch", "cancel_btn", "取消"),
                        DefaultButton = ContentDialogButton.Primary,
                    };
                    var choice = await dlg.ShowAsync();
                    if (choice == ContentDialogResult.Primary)
                    {
                        var elev = await NativeBridgeService.WriteAppProfilesElevatedAsync(_state.AppDir, _targetPath, _profiles);
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
                DeleteBtn.IsEnabled = true;
                _state.NotifyConfigChanged();
                ShowStatus($"{i18n.Tr("profiles", "saved_ok")} ({Path.GetFileName(ConfigService.AlongsideAppConfigPath(_targetPath))} · {DateTime.Now:HH:mm:ss})", InfoBarSeverity.Success);
                return true;
            }
        }

        if (wantGlobal)
        {
            try
            {
                ConfigService.SaveAppProfilesInGlobal(_state.ConfigPath, _state.Config, _targetPath, _profiles);
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
                DeleteBtn.IsEnabled = true;
                _state.NotifyConfigChanged();
                ShowStatus($"{i18n.Tr("profiles", "saved_ok")} (config.toml · {DateTime.Now:HH:mm:ss})", InfoBarSeverity.Success);
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
        await SaveProfilesAsync();
    }

    private async void SaveAndRunBtn_Click(object sender, RoutedEventArgs e)
    {
        if (await SaveProfilesAsync())
        {
            string? activeAppProfileName = (_selectedIndex >= 0 && _selectedIndex < _profiles.Count)
                ? _profiles[_selectedIndex].Name
                : null;
            var res = NativeBridgeService.LaunchTarget(
                _state.AppDir,
                _targetPath,
                profileName: null,
                extraArgs: null,
                appProfileName: activeAppProfileName);
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
