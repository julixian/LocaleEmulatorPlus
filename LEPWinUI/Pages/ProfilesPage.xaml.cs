using System;
using System.Linq;
using System.Threading.Tasks;
using LEPWinUI.Models;
using LEPWinUI.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Navigation;

namespace LEPWinUI.Pages;

public sealed partial class ProfilesPage : Page
{
    private MainWindow? _hostWindow;
    private readonly AppState _state = App.State;
    private int _selectedIndex = -1;

    public ProfilesPage()
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
        _state.ConfigChanged += RefreshLists;
        ApplyLocalization();
        LocaleCombo.ItemsSource = _state.Locales;
        TimezoneCombo.ItemsSource = _state.Timezones;
        RefreshLists();
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        _state.ConfigChanged -= RefreshLists;
    }

    private void ApplyLocalization()
    {
        var i18n = _state.I18n;
        PageTitleText.Text = i18n.Tr("tabs", "profiles", "配置管理");
        GlobalListHeader.Text = i18n.Tr("profiles", "global_list");
        AppListHeader.Text = i18n.Tr("profiles", "app_list");
        AddBtn.Content = i18n.Tr("profiles", "add");
        DupBtn.Content = i18n.Tr("profiles", "duplicate");
        DelBtn.Content = i18n.Tr("profiles", "delete");
        UpBtn.Content = i18n.Tr("profiles", "move_up");
        DownBtn.Content = i18n.Tr("profiles", "move_down");
        EditAppBtn.Content = i18n.Tr("profiles", "edit_selected_app");
        RemoveAppBtn.Content = i18n.Tr("profiles", "remove_selected_app");

        NameBox.Header = i18n.Tr("profiles", "name");
        LocaleCombo.Header = i18n.Tr("profiles", "locale");
        TimezoneCombo.Header = i18n.Tr("profiles", "timezone");
        RegModeCombo.Header = i18n.Tr("profiles", "reg_mode");
        RegModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "reg_mode_0"),
            i18n.Tr("profiles", "reg_mode_1"),
            i18n.Tr("profiles", "reg_mode_2"),
        };
        UiModeCombo.Header = i18n.Tr("profiles", "ui_mode");
        UiModeCombo.ItemsSource = new[]
        {
            i18n.Tr("profiles", "ui_mode_0"),
            i18n.Tr("profiles", "ui_mode_1"),
            i18n.Tr("profiles", "ui_mode_2"),
        };
        ArgsBox.Header = i18n.Tr("profiles", "arguments");
        StdOverrideBox.Header = i18n.Tr("profiles", "tz_std_override");
        DstOverrideBox.Header = i18n.Tr("profiles", "tz_dst_override");
        ShowInMenuToggle.Header = i18n.Tr("profiles", "show_in_main_menu");
        RunAsAdminToggle.Header = i18n.Tr("profiles", "run_as_admin");
        RunSuspendedToggle.Header = i18n.Tr("profiles", "run_with_suspend");
        SaveBtn.Content = i18n.Tr("profiles", "save");
    }

    private void RefreshLists()
    {
        var profiles = _state.Config.Profiles.ToList();
        ProfilesList.ItemsSource = profiles;
        if (profiles.Count == 0)
        {
            _selectedIndex = -1;
        }
        else
        {
            if (_selectedIndex < 0 || _selectedIndex >= profiles.Count)
            {
                _selectedIndex = 0;
            }
            ProfilesList.SelectedIndex = _selectedIndex;
            LoadSelectedToForm();
        }

        var apps = _state.Config.AppProfiles
            .GroupBy(e => SystemService.NormalizePathForMatch(e.TargetPath))
            .Select(g => new GlobalAppSummary
            {
                TargetPath = g.First().TargetPath,
                Count = g.Count(),
                FirstLocale = g.First().Profile.Locale,
            })
            .ToList();
        GlobalAppsList.ItemsSource = apps;
        if (apps.Count > 0)
        {
            GlobalAppsList.SelectedIndex = 0;
        }
        EditAppBtn.IsEnabled = apps.Count > 0;
        RemoveAppBtn.IsEnabled = apps.Count > 0;
    }

    private void LoadSelectedToForm()
    {
        if (_selectedIndex < 0 || _selectedIndex >= _state.Config.Profiles.Count)
        {
            return;
        }
        var p = _state.Config.Profiles[_selectedIndex];
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
        ShowInMenuToggle.IsOn = p.ShowInMainMenu;
        RunAsAdminToggle.IsOn = p.RunAsAdmin;
        RunSuspendedToggle.IsOn = p.RunWithSuspend;
    }

    private void ShowInfo(string message, InfoBarSeverity severity)
    {
        FeedbackBar.Severity = severity;
        FeedbackBar.Message = message;
        FeedbackBar.IsOpen = true;
    }

    private void ProfilesList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (ProfilesList.SelectedIndex >= 0)
        {
            _selectedIndex = ProfilesList.SelectedIndex;
            LoadSelectedToForm();
        }
    }

    private async Task<string?> PromptNameAsync(string title, string initialValue)
    {
        var i18n = _state.I18n;
        var input = new TextBox { Text = initialValue, Header = i18n.Tr("profiles", "name") };
        var dlg = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = title,
            Content = input,
            PrimaryButtonText = i18n.Tr("app", "confirm"),
            CloseButtonText = i18n.Tr("app", "close", "关闭"),
            DefaultButton = ContentDialogButton.Primary,
        };
        if (await dlg.ShowAsync() == ContentDialogResult.Primary)
        {
            return input.Text.Trim();
        }
        return null;
    }

    private async void AddBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        string? name = await PromptNameAsync(i18n.Tr("profiles", "add"), i18n.Tr("profiles", "new_profile_default"));
        if (string.IsNullOrWhiteSpace(name)) return;
        if (_state.Config.Profiles.Any(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase)))
        {
            ShowInfo(i18n.Tr("profiles", "name_duplicate"), InfoBarSeverity.Warning);
            return;
        }
        _state.Config.Profiles.Add(ConfigService.CreateDefaultProfile(name));
        _selectedIndex = _state.Config.Profiles.Count - 1;
        ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
        _state.NotifyConfigChanged();
        ShowInfo(i18n.Tr("profiles", "saved_ok"), InfoBarSeverity.Success);
    }

    private async void DupBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _state.Config.Profiles.Count) return;
        var i18n = _state.I18n;
        var copy = _state.Config.Profiles[_selectedIndex].Clone();
        string? name = await PromptNameAsync(i18n.Tr("profiles", "duplicate"), copy.Name + " (2)");
        if (string.IsNullOrWhiteSpace(name)) return;
        if (_state.Config.Profiles.Any(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase)))
        {
            ShowInfo(i18n.Tr("profiles", "name_duplicate"), InfoBarSeverity.Warning);
            return;
        }
        copy.Name = name;
        _state.Config.Profiles.Add(copy);
        _selectedIndex = _state.Config.Profiles.Count - 1;
        ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
        _state.NotifyConfigChanged();
        ShowInfo(i18n.Tr("profiles", "saved_ok"), InfoBarSeverity.Success);
    }

    private async void DelBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _state.Config.Profiles.Count) return;
        var i18n = _state.I18n;
        var dlg = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = i18n.Tr("app", "confirm"),
            Content = i18n.Tr("profiles", "confirm_delete"),
            PrimaryButtonText = i18n.Tr("profiles", "delete"),
            CloseButtonText = i18n.Tr("app", "close", "关闭"),
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dlg.ShowAsync() != ContentDialogResult.Primary) return;

        _state.Config.Profiles.RemoveAt(_selectedIndex);
        if (_selectedIndex >= _state.Config.Profiles.Count)
        {
            _selectedIndex = _state.Config.Profiles.Count - 1;
        }
        ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
        _state.NotifyConfigChanged();
    }

    private void UpBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex <= 0 || _selectedIndex >= _state.Config.Profiles.Count) return;
        (_state.Config.Profiles[_selectedIndex - 1], _state.Config.Profiles[_selectedIndex]) =
            (_state.Config.Profiles[_selectedIndex], _state.Config.Profiles[_selectedIndex - 1]);
        _selectedIndex--;
        ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
        _state.NotifyConfigChanged();
    }

    private void DownBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _state.Config.Profiles.Count - 1) return;
        (_state.Config.Profiles[_selectedIndex + 1], _state.Config.Profiles[_selectedIndex]) =
            (_state.Config.Profiles[_selectedIndex], _state.Config.Profiles[_selectedIndex + 1]);
        _selectedIndex++;
        ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
        _state.NotifyConfigChanged();
    }

    private void SaveBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selectedIndex < 0 || _selectedIndex >= _state.Config.Profiles.Count) return;
        var i18n = _state.I18n;
        string newName = NameBox.Text.Trim();
        if (string.IsNullOrEmpty(newName))
        {
            ShowInfo(i18n.Tr("profiles", "name_empty"), InfoBarSeverity.Warning);
            return;
        }
        for (int i = 0; i < _state.Config.Profiles.Count; i++)
        {
            if (i != _selectedIndex && _state.Config.Profiles[i].Name.Equals(newName, StringComparison.OrdinalIgnoreCase))
            {
                ShowInfo(i18n.Tr("profiles", "name_duplicate"), InfoBarSeverity.Warning);
                return;
            }
        }

        var p = _state.Config.Profiles[_selectedIndex];
        p.Name = newName;
        if (LocaleCombo.SelectedItem is LocaleEntry loc) p.Locale = loc.LocaleName;
        if (TimezoneCombo.SelectedItem is TimezoneEntry tz) p.Timezone = tz.Id;
        p.RegistryRedirection = Math.Clamp(RegModeCombo.SelectedIndex, 0, 2);
        p.UiLanguageMode = Math.Clamp(UiModeCombo.SelectedIndex, 0, 2);
        p.Arguments = ArgsBox.Text.Trim();
        p.StandardNameOverride = StdOverrideBox.Text.Trim();
        p.DaylightNameOverride = DstOverrideBox.Text.Trim();
        p.ShowInMainMenu = ShowInMenuToggle.IsOn;
        p.RunAsAdmin = RunAsAdminToggle.IsOn;
        p.RunWithSuspend = RunSuspendedToggle.IsOn;

        try
        {
            ConfigService.SaveGlobalConfig(_state.ConfigPath, _state.Config);
            _state.NotifyConfigChanged();
            ShowInfo($"{i18n.Tr("profiles", "saved_ok")} (config.toml · {DateTime.Now:HH:mm:ss})", InfoBarSeverity.Success);
        }
        catch (Exception ex)
        {
            ShowInfo(ex.Message, InfoBarSeverity.Error);
        }
    }

    private void OpenSelectedGlobalApp()
    {
        if (GlobalAppsList.SelectedItem is GlobalAppSummary entry)
        {
            _hostWindow?.OpenAppProfileWindow(entry.TargetPath);
        }
    }

    private void EditAppBtn_Click(object sender, RoutedEventArgs e) => OpenSelectedGlobalApp();

    private void GlobalAppsList_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e) => OpenSelectedGlobalApp();

    private async void RemoveAppBtn_Click(object sender, RoutedEventArgs e)
    {
        if (GlobalAppsList.SelectedItem is not GlobalAppSummary entry) return;
        var i18n = _state.I18n;
        var dlg = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = i18n.Tr("app", "confirm"),
            Content = i18n.Tr("profiles", "confirm_delete_app"),
            PrimaryButtonText = i18n.Tr("profiles", "delete"),
            CloseButtonText = i18n.Tr("app", "close", "关闭"),
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dlg.ShowAsync() != ContentDialogResult.Primary) return;

        ConfigService.RemoveGlobalAppProfileEntry(_state.ConfigPath, _state.Config, entry.TargetPath);
        _state.NotifyConfigChanged();
        ShowInfo(i18n.Tr("profiles", "app_profile_deleted"), InfoBarSeverity.Success);
    }
}
