using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using LEPWinUI.Models;
using Tomlyn;
using Tomlyn.Model;

namespace LEPWinUI.Services;

public static class ConfigService
{
    public static LepProfile CreateDefaultProfile(string name = "日本语") => new()
    {
        Name = name,
        Locale = "ja-JP",
        Timezone = "Tokyo Standard Time",
        RegistryRedirection = 2,
        UiLanguageMode = 1,
        RunAsAdmin = false,
        RunWithSuspend = false,
        ShowInMainMenu = true,
        Arguments = "",
    };

    public static GlobalConfig CreateDefaultGlobalConfig()
    {
        var cfg = new GlobalConfig
        {
            Version = SystemService.AppVersion,
            Language = "auto",
        };
        cfg.Profiles.Add(CreateDefaultProfile("日本语"));

        var zhCn = CreateDefaultProfile("简体中文");
        zhCn.Locale = "zh-CN";
        zhCn.Timezone = "China Standard Time";
        zhCn.ShowInMainMenu = false;
        cfg.Profiles.Add(zhCn);

        var zhTw = CreateDefaultProfile("繁體中文 (台灣)");
        zhTw.Locale = "zh-TW";
        zhTw.Timezone = "Taipei Standard Time";
        zhTw.ShowInMainMenu = false;
        cfg.Profiles.Add(zhTw);

        var koKr = CreateDefaultProfile("한국어");
        koKr.Locale = "ko-KR";
        koKr.Timezone = "Korea Standard Time";
        koKr.ShowInMainMenu = false;
        cfg.Profiles.Add(koKr);

        return cfg;
    }

    public static string AlongsideAppConfigPath(string targetPath) => targetPath + ".lep.toml";

    private static string GetString(TomlTable tbl, string key, string fallback = "") =>
        tbl.TryGetValue(key, out object? v) && v is string s ? s : fallback;

    private static int GetInt(TomlTable tbl, string key, int fallback, int min, int max)
    {
        if (tbl.TryGetValue(key, out object? v))
        {
            int parsed = v switch
            {
                long l => (int)l,
                int i => i,
                _ => fallback,
            };
            return Math.Clamp(parsed, min, max);
        }
        return fallback;
    }

    private static uint? GetOptionalUInt(TomlTable tbl, string key, bool allowZero = false)
    {
        if (tbl.TryGetValue(key, out object? v))
        {
            long val = v switch
            {
                long l => l,
                int i => i,
                _ => -1,
            };
            if (val > 0 || (allowZero && val == 0))
            {
                return (uint)val;
            }
        }
        return null;
    }

    private static bool GetBool(TomlTable tbl, string key, bool fallback) =>
        tbl.TryGetValue(key, out object? v) && v is bool b ? b : fallback;

    private static LepProfile ParseProfileTable(TomlTable tbl)
    {
        var p = CreateDefaultProfile();
        p.Name = GetString(tbl, "name", p.Name);
        p.Locale = GetString(tbl, "locale", p.Locale);
        p.Timezone = GetString(tbl, "timezone", p.Timezone);
        p.RegistryRedirection = GetInt(tbl, "registry_redirection", p.RegistryRedirection, 0, 2);
        p.UiLanguageMode = GetInt(tbl, "ui_language_mode", p.UiLanguageMode, 0, 2);
        p.RunAsAdmin = GetBool(tbl, "run_as_admin", p.RunAsAdmin);
        p.RunWithSuspend = GetBool(tbl, "run_with_suspend", p.RunWithSuspend);
        p.ShowInMainMenu = GetBool(tbl, "show_in_main_menu", p.ShowInMainMenu);
        p.Arguments = GetString(tbl, "arguments", "");
        p.AnsiCodePage = GetOptionalUInt(tbl, "ansi_code_page");
        p.OemCodePage = GetOptionalUInt(tbl, "oem_code_page");
        p.LocaleId = GetOptionalUInt(tbl, "locale_id");
        p.FontCharset = GetOptionalUInt(tbl, "font_charset", allowZero: true);
        p.StandardNameOverride = GetString(tbl, "standard_name_override", "");
        p.DaylightNameOverride = GetString(tbl, "daylight_name_override", "");
        return p;
    }

    private static TomlTable BuildProfileTable(LepProfile p, bool includeMenuFlag, string? targetPath = null)
    {
        var tbl = new TomlTable();
        if (!string.IsNullOrEmpty(targetPath))
        {
            tbl["target_path"] = targetPath;
        }
        tbl["name"] = p.Name;
        tbl["locale"] = p.Locale;
        tbl["timezone"] = p.Timezone;
        tbl["registry_redirection"] = (long)Math.Clamp(p.RegistryRedirection, 0, 2);
        tbl["ui_language_mode"] = (long)Math.Clamp(p.UiLanguageMode, 0, 2);
        tbl["run_as_admin"] = p.RunAsAdmin;
        tbl["run_with_suspend"] = p.RunWithSuspend;
        if (includeMenuFlag)
        {
            tbl["show_in_main_menu"] = p.ShowInMainMenu;
        }
        tbl["arguments"] = p.Arguments;
        if (p.AnsiCodePage.HasValue) tbl["ansi_code_page"] = (long)p.AnsiCodePage.Value;
        if (p.OemCodePage.HasValue) tbl["oem_code_page"] = (long)p.OemCodePage.Value;
        if (p.LocaleId.HasValue) tbl["locale_id"] = (long)p.LocaleId.Value;
        if (p.FontCharset.HasValue) tbl["font_charset"] = (long)p.FontCharset.Value;
        if (!string.IsNullOrEmpty(p.StandardNameOverride)) tbl["standard_name_override"] = p.StandardNameOverride;
        if (!string.IsNullOrEmpty(p.DaylightNameOverride)) tbl["daylight_name_override"] = p.DaylightNameOverride;
        return tbl;
    }

    private static void WriteTextAtomic(string path, string content)
    {
        string? dir = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(dir))
        {
            Directory.CreateDirectory(dir);
        }
        string temp = path + $".tmp.{Environment.ProcessId}.{Environment.TickCount64}";
        File.WriteAllText(temp, content, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.Move(temp, path, overwrite: true);
    }

    public static GlobalConfig LoadOrCreateGlobalConfig(string configPath)
    {
        if (!File.Exists(configPath))
        {
            var def = CreateDefaultGlobalConfig();
            try
            {
                SaveGlobalConfig(configPath, def);
            }
            catch
            {
            }
            return def;
        }

        try
        {
            string text = File.ReadAllText(configPath, Encoding.UTF8);
            TomlTable doc = Toml.ToModel(text);
            var cfg = new GlobalConfig
            {
                Version = GetString(doc, "version", SystemService.AppVersion),
                Language = GetString(doc, "language", "auto"),
            };

            if (doc.TryGetValue("profiles", out object? profsObj) && profsObj is TomlTableArray profArr)
            {
                foreach (TomlTable item in profArr)
                {
                    cfg.Profiles.Add(ParseProfileTable(item));
                }
            }

            if (doc.TryGetValue("app_profiles", out object? appsObj) && appsObj is TomlTableArray appArr)
            {
                foreach (TomlTable item in appArr)
                {
                    string target = GetString(item, "target_path");
                    if (!string.IsNullOrEmpty(target))
                    {
                        cfg.AppProfiles.Add(new AppProfileEntry
                        {
                            TargetPath = target,
                            Profile = ParseProfileTable(item),
                        });
                    }
                }
            }

            if (cfg.Profiles.Count == 0)
            {
                cfg.Profiles = CreateDefaultGlobalConfig().Profiles;
            }
            return cfg;
        }
        catch
        {
            return CreateDefaultGlobalConfig();
        }
    }

    public static void SaveGlobalConfig(string configPath, GlobalConfig config)
    {
        var doc = new TomlTable
        {
            ["version"] = SystemService.AppVersion,
            ["language"] = string.IsNullOrWhiteSpace(config.Language) ? "auto" : config.Language,
        };

        var profArr = new TomlTableArray();
        foreach (var p in config.Profiles)
        {
            profArr.Add(BuildProfileTable(p, includeMenuFlag: true));
        }
        doc["profiles"] = profArr;

        if (config.AppProfiles.Count > 0)
        {
            var appArr = new TomlTableArray();
            foreach (var entry in config.AppProfiles)
            {
                appArr.Add(BuildProfileTable(entry.Profile, includeMenuFlag: false, targetPath: entry.TargetPath));
            }
            doc["app_profiles"] = appArr;
        }

        string body = "# Locale Emulator Plus 全局配置文件（UTF-8 TOML）\n" + Toml.FromModel(doc);
        WriteTextAtomic(configPath, body);
    }

    public static ResolvedAppProfile? FindAppProfile(string targetPath, GlobalConfig globalConfig)
    {
        if (string.IsNullOrWhiteSpace(targetPath))
        {
            return null;
        }

        string sidePath = AlongsideAppConfigPath(targetPath);
        if (File.Exists(sidePath))
        {
            try
            {
                TomlTable doc = Toml.ToModel(File.ReadAllText(sidePath, Encoding.UTF8));
                TomlTable source = doc.TryGetValue("profile", out object? sub) && sub is TomlTable subTbl ? subTbl : doc;
                var p = ParseProfileTable(source);
                if (string.IsNullOrWhiteSpace(p.Name))
                {
                    p.Name = Path.GetFileName(targetPath);
                }
                return new ResolvedAppProfile
                {
                    Profile = p,
                    Storage = AppProfileStorage.AlongsideTarget,
                    ConfigFilePath = sidePath,
                };
            }
            catch
            {
            }
        }

        string norm = SystemService.NormalizePathForMatch(targetPath);
        var match = globalConfig.AppProfiles.FirstOrDefault(e => SystemService.NormalizePathForMatch(e.TargetPath) == norm);
        if (match is not null)
        {
            var p = match.Profile.Clone();
            if (string.IsNullOrWhiteSpace(p.Name))
            {
                p.Name = Path.GetFileName(targetPath);
            }
            return new ResolvedAppProfile
            {
                Profile = p,
                Storage = AppProfileStorage.GlobalConfig,
                ConfigFilePath = "",
            };
        }

        return null;
    }

    public static string SerializeAppProfileToml(LepProfile profile)
    {
        var doc = new TomlTable
        {
            ["version"] = SystemService.AppVersion,
            ["profile"] = BuildProfileTable(profile, includeMenuFlag: false),
        };
        return "# Locale Emulator Plus 程序专属配置文件\n" + Toml.FromModel(doc);
    }

    public static void SaveAppProfileAlongside(string targetPath, LepProfile profile)
    {
        string sidePath = AlongsideAppConfigPath(targetPath);
        WriteTextAtomic(sidePath, SerializeAppProfileToml(profile));
    }

    public static void SaveAppProfileInGlobal(string globalConfigPath, GlobalConfig globalConfig, string targetPath, LepProfile profile)
    {
        string norm = SystemService.NormalizePathForMatch(targetPath);
        var existing = globalConfig.AppProfiles.FirstOrDefault(e => SystemService.NormalizePathForMatch(e.TargetPath) == norm);
        if (existing is not null)
        {
            existing.TargetPath = targetPath;
            existing.Profile = profile.Clone();
        }
        else
        {
            globalConfig.AppProfiles.Add(new AppProfileEntry
            {
                TargetPath = targetPath,
                Profile = profile.Clone(),
            });
        }
        SaveGlobalConfig(globalConfigPath, globalConfig);
    }

    public static void RemoveGlobalAppProfileEntry(string globalConfigPath, GlobalConfig globalConfig, string targetPath)
    {
        string norm = SystemService.NormalizePathForMatch(targetPath);
        int removed = globalConfig.AppProfiles.RemoveAll(e => SystemService.NormalizePathForMatch(e.TargetPath) == norm);
        if (removed > 0)
        {
            SaveGlobalConfig(globalConfigPath, globalConfig);
        }
    }
}
