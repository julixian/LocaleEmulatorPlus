using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text.Json;
using LEPWinUI.Models;
using LEPWinUI.Services;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace LEPWinUI.Pages;

public sealed partial class SettingsPage : Page
{
    private const string LatestReleaseApiUrl = "https://api.github.com/repos/julixian/LocaleEmulatorPlus/releases/latest";
    private const string ReleasesFallbackUrl = "https://github.com/julixian/LocaleEmulatorPlus/releases";

    private static readonly HttpClient UpdateHttpClient = CreateHttpClient();

    private readonly AppState _state = App.State;
    private bool _initializing;

    private static HttpClient CreateHttpClient()
    {
        var client = new HttpClient { Timeout = TimeSpan.FromSeconds(10) };
        client.DefaultRequestHeaders.UserAgent.ParseAdd($"LocaleEmulatorPlus/{SystemService.AppVersion}");
        client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        return client;
    }

    public SettingsPage()
    {
        InitializeComponent();
        Loaded += (_, _) => Populate();
    }

    private void Populate()
    {
        _initializing = true;
        var i18n = _state.I18n;
        bool storeMode = SystemService.IsPackagedApp();
        PageTitleText.Text = i18n.Tr("tabs", "settings", "设置与关于");

        LanguageCombo.Header = i18n.Tr("settings", "lang_label");
        LanguageCombo.ItemsSource = _state.Languages;
        int sel = _state.Languages.FindIndex(l => l.Code.Equals(_state.Config.Language, StringComparison.OrdinalIgnoreCase));
        LanguageCombo.SelectedIndex = sel >= 0 ? sel : 0;

        LangHintText.Text = i18n.Tr("settings", "lang_apply_hint");
        VersionText.Text = $"{i18n.Tr("settings", "version_label")} {SystemService.AppVersion}";
        ModeText.Text = storeMode
            ? i18n.Tr("settings", "mode_store")
            : i18n.Tr("settings", "mode_portable");

        ConfigPathBox.Header = i18n.Tr("settings", "config_path_label");
        ConfigPathBox.Text = _state.ConfigPath;

        OpenConfigDirBtn.Content = i18n.Tr("settings", "open_config_dir");
        OpenLogDirBtn.Content = i18n.Tr("settings", "open_log_dir");
        OpenAppDirBtn.Content = i18n.Tr("settings", "open_app_dir");
        CheckUpdateBtn.Content = i18n.Tr("settings", "check_update");
        CheckUpdateBtn.Visibility = storeMode ? Visibility.Collapsed : Visibility.Visible;

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

    private void OpenFolder(string dir, bool ensureExists)
    {
        try
        {
            if (ensureExists && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }
            if (!Directory.Exists(dir))
            {
                FeedbackBar.Severity = InfoBarSeverity.Warning;
                FeedbackBar.Message = $"目录不存在：{dir}";
                FeedbackBar.IsOpen = true;
                return;
            }
            Process.Start(new ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = SystemService.QuoteArg(dir),
                UseShellExecute = true,
            });
        }
        catch (Exception ex)
        {
            FeedbackBar.Severity = InfoBarSeverity.Error;
            FeedbackBar.Message = ex.Message;
            FeedbackBar.IsOpen = true;
        }
    }

    private void OpenConfigDirBtn_Click(object sender, RoutedEventArgs e)
    {
        string? dir = Path.GetDirectoryName(_state.ConfigPath);
        if (!string.IsNullOrEmpty(dir)) OpenFolder(dir, ensureExists: true);
    }

    private void OpenLogDirBtn_Click(object sender, RoutedEventArgs e) =>
        OpenFolder(SystemService.ResolveLogDirectory(_state.AppDir, _state.ConfigPath), ensureExists: true);

    private void OpenAppDirBtn_Click(object sender, RoutedEventArgs e) =>
        OpenFolder(_state.AppDir, ensureExists: false);

    private static (int Major, int Minor, int Patch) ParseSemver(string text)
    {
        ReadOnlySpan<char> span = text.AsSpan().Trim();
        while (!span.IsEmpty && (span[0] == 'v' || span[0] == 'V'))
        {
            span = span[1..];
        }

        int[] parts = [0, 0, 0];
        int partIdx = 0;
        int cur = 0;
        bool hasDigit = false;
        foreach (char ch in span)
        {
            if (ch >= '0' && ch <= '9')
            {
                cur = cur * 10 + (ch - '0');
                hasDigit = true;
            }
            else if (ch == '.' && partIdx < 2)
            {
                parts[partIdx++] = cur;
                cur = 0;
                hasDigit = false;
            }
            else
            {
                break;
            }
        }
        if (hasDigit && partIdx < 3)
        {
            parts[partIdx] = cur;
        }
        return (parts[0], parts[1], parts[2]);
    }

    private async void CheckUpdateBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        CheckUpdateBtn.IsEnabled = false;
        FeedbackBar.Severity = InfoBarSeverity.Informational;
        FeedbackBar.Message = i18n.Tr("settings", "update_checking");
        FeedbackBar.IsOpen = true;

        try
        {
            using HttpResponseMessage resp = await UpdateHttpClient.GetAsync(LatestReleaseApiUrl);
            if (!resp.IsSuccessStatusCode)
            {
                throw new HttpRequestException($"HTTP {(int)resp.StatusCode}");
            }

            string json = await resp.Content.ReadAsStringAsync();
            using JsonDocument doc = JsonDocument.Parse(json);
            string tagName = doc.RootElement.TryGetProperty("tag_name", out JsonElement tagEl)
                ? (tagEl.GetString() ?? "")
                : "";
            if (string.IsNullOrWhiteSpace(tagName))
            {
                throw new InvalidDataException("tag_name is empty.");
            }

            string latestVer = tagName.Trim().TrimStart('v', 'V');
            string releaseUrl = doc.RootElement.TryGetProperty("html_url", out JsonElement urlEl)
                && !string.IsNullOrWhiteSpace(urlEl.GetString())
                ? urlEl.GetString()!
                : ReleasesFallbackUrl;

            bool hasUpdate = ParseSemver(latestVer).CompareTo(ParseSemver(SystemService.AppVersion)) > 0;
            if (hasUpdate)
            {
                FeedbackBar.Severity = InfoBarSeverity.Success;
                FeedbackBar.Message = $"{i18n.Tr("settings", "update_found_status")}: v{latestVer}";
                FeedbackBar.IsOpen = true;

                if (XamlRoot is not null)
                {
                    var dlg = new ContentDialog
                    {
                        XamlRoot = XamlRoot,
                        Title = i18n.Tr("app", "info"),
                        Content = $"{i18n.Tr("settings", "update_found_prefix")} v{latestVer}\n(v{SystemService.AppVersion} → v{latestVer})\n\n{i18n.Tr("settings", "update_open_prompt")}",
                        PrimaryButtonText = i18n.Tr("app", "confirm"),
                        CloseButtonText = i18n.Tr("app", "close"),
                        DefaultButton = ContentDialogButton.Primary,
                    };
                    if (await dlg.ShowAsync() == ContentDialogResult.Primary)
                    {
                        Process.Start(new ProcessStartInfo
                        {
                            FileName = releaseUrl,
                            UseShellExecute = true,
                        });
                    }
                }
            }
            else
            {
                FeedbackBar.Severity = InfoBarSeverity.Success;
                FeedbackBar.Message = $"{i18n.Tr("settings", "update_latest")} (v{SystemService.AppVersion})";
                FeedbackBar.IsOpen = true;
            }
        }
        catch (Exception ex)
        {
            FeedbackBar.Severity = InfoBarSeverity.Error;
            FeedbackBar.Message = $"{i18n.Tr("settings", "update_failed")}: {ex.Message}";
            FeedbackBar.IsOpen = true;
        }
        finally
        {
            CheckUpdateBtn.IsEnabled = true;
        }
    }
}
