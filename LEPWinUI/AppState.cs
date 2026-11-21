using System;
using System.Collections.Generic;
using LEPWinUI.Models;
using LEPWinUI.Services;

namespace LEPWinUI;

public sealed class AppState
{
    public string AppDir { get; }
    public string ConfigPath { get; }
    public GlobalConfig Config { get; set; }
    public I18nService I18n { get; } = new();
    public List<LocaleEntry> Locales { get; }
    public List<TimezoneEntry> Timezones { get; }
    public List<LanguageOption> Languages { get; }
    public string? CurrentLaunchTarget { get; set; }

    public event Action? ConfigChanged;
    public event Action? LanguageChanged;

    public AppState()
    {
        AppDir = SystemService.AppDirectory;
        ConfigPath = SystemService.ResolveGlobalConfigPath(AppDir);
        Config = ConfigService.LoadOrCreateGlobalConfig(ConfigPath);
        I18n.Load(AppDir, Config.Language);
        Locales = SystemService.EnumerateLocales();
        Timezones = SystemService.EnumerateTimezones();
        Languages = I18nService.AvailableLanguages(AppDir);
    }

    public void ReloadConfig()
    {
        Config = ConfigService.LoadOrCreateGlobalConfig(ConfigPath);
        NotifyConfigChanged();
    }

    public void NotifyConfigChanged() => ConfigChanged?.Invoke();

    public void SetLanguage(string langCode)
    {
        if (string.Equals(Config.Language, langCode, StringComparison.OrdinalIgnoreCase))
        {
            return;
        }
        Config.Language = langCode;
        ConfigService.SaveGlobalConfig(ConfigPath, Config);
        I18n.Load(AppDir, langCode);
        LanguageChanged?.Invoke();
    }
}
