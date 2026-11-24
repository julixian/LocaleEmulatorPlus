using System;
using System.Collections.Generic;

namespace LEPWinUI.Models;

public enum AppProfileStorage
{
    None,
    AlongsideTarget,
    GlobalConfig,
}

public enum DirectoryWriteStatus
{
    Writable,
    NeedsAdmin,
    ReadOnly,
}

public sealed class LepProfile
{
    public string Name { get; set; } = "日本语";
    public string Locale { get; set; } = "ja-JP";
    public string Timezone { get; set; } = "Tokyo Standard Time";
    public int RegistryRedirection { get; set; } = 2;
    public int UiLanguageMode { get; set; } = 1;
    public bool RunAsAdmin { get; set; } = false;
    public bool RunWithSuspend { get; set; } = false;
    public bool ShowInMainMenu { get; set; } = true;
    public string Arguments { get; set; } = "";

    public uint? AnsiCodePage { get; set; }
    public uint? OemCodePage { get; set; }
    public uint? LocaleId { get; set; }
    public uint? FontCharset { get; set; }
    public string StandardNameOverride { get; set; } = "";
    public string DaylightNameOverride { get; set; } = "";

    public LepProfile Clone() => new()
    {
        Name = Name,
        Locale = Locale,
        Timezone = Timezone,
        RegistryRedirection = RegistryRedirection,
        UiLanguageMode = UiLanguageMode,
        RunAsAdmin = RunAsAdmin,
        RunWithSuspend = RunWithSuspend,
        ShowInMainMenu = ShowInMainMenu,
        Arguments = Arguments,
        AnsiCodePage = AnsiCodePage,
        OemCodePage = OemCodePage,
        LocaleId = LocaleId,
        FontCharset = FontCharset,
        StandardNameOverride = StandardNameOverride,
        DaylightNameOverride = DaylightNameOverride,
    };

    public override string ToString() => Name;
}

public sealed class AppProfileEntry
{
    public string TargetPath { get; set; } = "";
    public LepProfile Profile { get; set; } = new();

    public string DisplayLabel => $"{System.IO.Path.GetFileName(TargetPath)} ({Profile.Locale})";
    public override string ToString() => DisplayLabel;
}

public sealed class GlobalAppSummary
{
    public required string TargetPath { get; init; }
    public required int Count { get; init; }
    public required string FirstLocale { get; init; }

    public string DisplayLabel
    {
        get
        {
            string name = System.IO.Path.GetFileName(TargetPath);
            return Count > 1 ? $"{name} ({Count})" : $"{name} ({FirstLocale})";
        }
    }

    public override string ToString() => DisplayLabel;
}

public sealed class GlobalConfig
{
    public string Version { get; set; } = "3.0.2";
    public string Language { get; set; } = "auto";
    public List<LepProfile> Profiles { get; set; } = new();
    public List<AppProfileEntry> AppProfiles { get; set; } = new();
}

public sealed class ResolvedAppProfile
{
    public required LepProfile Profile { get; init; }
    public required AppProfileStorage Storage { get; init; }
    public string ConfigFilePath { get; init; } = "";
}

public sealed class ResolvedAppProfiles
{
    public required List<LepProfile> Profiles { get; init; }
    public required AppProfileStorage Storage { get; init; }
    public string ConfigFilePath { get; init; } = "";
}

public sealed class LocaleEntry
{
    public required string LocaleName { get; init; }
    public required string DisplayName { get; init; }
    public uint AnsiCodePage { get; init; }
    public override string ToString() => DisplayName;
}

public sealed class TimezoneEntry
{
    public required string Id { get; init; }
    public required string DisplayName { get; init; }
    public override string ToString() => DisplayName;
}

public sealed class LanguageOption
{
    public required string Code { get; init; }
    public required string DisplayName { get; init; }
    public override string ToString() => DisplayName;
}

public sealed class CoreComponentStatus
{
    public required string FileName { get; init; }
    public required bool Present { get; init; }
}
