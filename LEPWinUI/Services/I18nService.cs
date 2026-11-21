using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using LEPWinUI.Models;
using Tomlyn;
using Tomlyn.Model;

namespace LEPWinUI.Services;

public sealed class I18nService
{
    private readonly Dictionary<string, Dictionary<string, string>> _sections = new(StringComparer.OrdinalIgnoreCase);

    public string ActiveCode { get; private set; } = "zh-CN";

    public void Load(string appDir, string languageCode)
    {
        _sections.Clear();
        string resolved = ResolveCode(languageCode);
        ActiveCode = resolved;

        string localesDir = Path.Combine(appDir, "locales");
        string filePath = Path.Combine(localesDir, $"{resolved}.toml");
        if (!File.Exists(filePath))
        {
            filePath = Path.Combine(localesDir, "zh-CN.toml");
        }
        if (!File.Exists(filePath))
        {
            return;
        }

        try
        {
            TomlTable doc = Toml.ToModel(File.ReadAllText(filePath, Encoding.UTF8));
            foreach (var kv in doc)
            {
                if (kv.Value is TomlTable sub)
                {
                    var map = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    foreach (var entry in sub)
                    {
                        if (entry.Value is string s)
                        {
                            map[entry.Key] = s;
                        }
                    }
                    _sections[kv.Key] = map;
                }
            }
        }
        catch
        {
        }
    }

    public string Tr(string section, string key, string? fallback = null)
    {
        if (_sections.TryGetValue(section, out var map) && map.TryGetValue(key, out string? val))
        {
            return val;
        }
        return fallback ?? key;
    }

    public static List<LanguageOption> AvailableLanguages(string appDir)
    {
        var list = new List<LanguageOption>
        {
            new() { Code = "auto", DisplayName = "跟随系统 / Follow System" },
            new() { Code = "zh-CN", DisplayName = "简体中文" },
            new() { Code = "en-US", DisplayName = "English" },
            new() { Code = "ja-JP", DisplayName = "日本語" },
        };

        string localesDir = Path.Combine(appDir, "locales");
        if (Directory.Exists(localesDir))
        {
            foreach (string file in Directory.GetFiles(localesDir, "*.toml"))
            {
                string code = Path.GetFileNameWithoutExtension(file);
                if (!list.Exists(x => x.Code.Equals(code, StringComparison.OrdinalIgnoreCase)))
                {
                    list.Add(new LanguageOption { Code = code, DisplayName = code });
                }
            }
        }
        return list;
    }

    private static string ResolveCode(string requested)
    {
        if (!string.IsNullOrWhiteSpace(requested) && !requested.Equals("auto", StringComparison.OrdinalIgnoreCase))
        {
            return requested;
        }
        string ui = CultureInfo.CurrentUICulture.Name;
        if (ui.StartsWith("zh", StringComparison.OrdinalIgnoreCase)) return "zh-CN";
        if (ui.StartsWith("ja", StringComparison.OrdinalIgnoreCase)) return "ja-JP";
        return "en-US";
    }
}
