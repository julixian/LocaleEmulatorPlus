using System;
using System.Diagnostics;
using System.IO;
using LEPWinUI.Models;
using LEPWinUI.Services;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace LEPWinUI.Pages;

public sealed partial class SettingsPage : Page
{
    private readonly AppState _state = App.State;
    private bool _initializing;

    public SettingsPage()
    {
        InitializeComponent();
        Loaded += (_, _) => Populate();
    }

    private void Populate()
    {
        _initializing = true;
        var i18n = _state.I18n;
        PageTitleText.Text = i18n.Tr("tabs", "settings", "设置与关于");

        LanguageCombo.Header = i18n.Tr("settings", "lang_label");
        LanguageCombo.ItemsSource = _state.Languages;
        int sel = _state.Languages.FindIndex(l => l.Code.Equals(_state.Config.Language, StringComparison.OrdinalIgnoreCase));
        LanguageCombo.SelectedIndex = sel >= 0 ? sel : 0;

        LangHintText.Text = i18n.Tr("settings", "lang_apply_hint");
        VersionText.Text = $"{i18n.Tr("settings", "version_label")} {SystemService.AppVersion}";
        ModeText.Text = SystemService.IsPackagedApp()
            ? i18n.Tr("settings", "mode_store")
            : i18n.Tr("settings", "mode_portable");

        ConfigPathBox.Header = i18n.Tr("settings", "config_path_label");
        ConfigPathBox.Text = _state.ConfigPath;

        OpenConfigDirBtn.Content = i18n.Tr("settings", "open_config_dir");
        OpenLogDirBtn.Content = i18n.Tr("settings", "open_log_dir");
        OpenAppDirBtn.Content = i18n.Tr("settings", "open_app_dir");

        CoreHeaderText.Text = i18n.Tr("settings", "core_status_group");
        CoreStatusList.Children.Clear();
        foreach (var comp in SystemService.InspectCoreComponents(_state.AppDir))
        {
            var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10 };
            var icon = new FontIcon
            {
                Glyph = comp.Present ? "\uE73E" : "\uE711",
                FontSize = 14,
                Foreground = new SolidColorBrush(comp.Present ? Colors.SeaGreen : Colors.IndianRed),
            };
            var label = new TextBlock
            {
                Text = $"{comp.FileName}  —  {(comp.Present ? i18n.Tr("settings", "core_ok") : i18n.Tr("settings", "core_missing"))}",
                VerticalAlignment = VerticalAlignment.Center,
            };
            row.Children.Add(icon);
            row.Children.Add(label);
            CoreStatusList.Children.Add(row);
        }

        _initializing = false;
    }

    private void LanguageCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_initializing) return;
        if (LanguageCombo.SelectedItem is LanguageOption opt)
        {
            _state.SetLanguage(opt.Code);
        }
    }

    private static void OpenFolder(string dir)
    {
        Directory.CreateDirectory(dir);
        Process.Start(new ProcessStartInfo { FileName = dir, UseShellExecute = true });
    }

    private void OpenConfigDirBtn_Click(object sender, RoutedEventArgs e)
    {
        string? dir = Path.GetDirectoryName(_state.ConfigPath);
        if (!string.IsNullOrEmpty(dir)) OpenFolder(dir);
    }

    private void OpenLogDirBtn_Click(object sender, RoutedEventArgs e) =>
        OpenFolder(Path.Combine(_state.AppDir, "Log"));

    private void OpenAppDirBtn_Click(object sender, RoutedEventArgs e) =>
        OpenFolder(_state.AppDir);
}
