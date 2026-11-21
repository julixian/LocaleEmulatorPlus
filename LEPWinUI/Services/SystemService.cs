using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Principal;
using LEPWinUI.Models;
using Microsoft.Win32;

namespace LEPWinUI.Services;

public static class SystemService
{
    public const string AppVersion = "3.0.0";
    public const string ShellExtClsid = "{A84F3C21-6E9D-4B2A-9F1E-3D7C5B8A2030}";
    public const string SparsePackageName = "julixian.LocaleEmulatorPlus.ShellExt";

    private const int ErrorInsufficientBuffer = 122;
    private const int ErrorAccessDenied = 5;
    private const int ErrorWriteProtect = 19;
    private const uint DriveCdRom = 5;
    private const uint FileReadOnlyVolume = 0x00080000;
    private const uint LocaleIdefaultAnsiCodepage = 0x00001004;
    private const uint LocaleReturnNumber = 0x20000000;

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetCurrentPackageFullName(ref uint packageFullNameLength, IntPtr packageFullName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern uint GetDriveTypeW(string lpRootPathName);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetVolumeInformationW(
        string lpRootPathName,
        IntPtr lpVolumeNameBuffer,
        uint nVolumeNameSize,
        IntPtr lpVolumeSerialNumber,
        IntPtr lpMaximumComponentLength,
        out uint lpFileSystemFlags,
        IntPtr lpFileSystemNameBuffer,
        uint nFileSystemNameSize);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int GetLocaleInfoEx(
        string lpLocaleName,
        uint lcType,
        out uint lpLCData,
        int cchData);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct OpenFileNameW
    {
        public int lStructSize;
        public IntPtr hwndOwner;
        public IntPtr hInstance;
        public string? lpstrFilter;
        public IntPtr lpstrCustomFilter;
        public int nMaxCustFilter;
        public int nFilterIndex;
        public IntPtr lpstrFile;
        public int nMaxFile;
        public IntPtr lpstrFileTitle;
        public int nMaxFileTitle;
        public string? lpstrInitialDir;
        public string? lpstrTitle;
        public uint Flags;
        public short nFileOffset;
        public short nFileExtension;
        public string? lpstrDefExt;
        public IntPtr lCustData;
        public IntPtr lpfnHook;
        public IntPtr lpTemplateName;
        public IntPtr pvReserved;
        public int dwReserved;
        public uint FlagsEx;
    }

    [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetOpenFileNameW(ref OpenFileNameW lpofn);

    [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetSaveFileNameW(ref OpenFileNameW lpofn);

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);

    public static double GetWindowScaleFactor(IntPtr hwnd)
    {
        try
        {
            uint dpi = GetDpiForWindow(hwnd);
            return dpi > 0 ? dpi / 96.0 : 1.0;
        }
        catch
        {
            return 1.0;
        }
    }

    public static string AppDirectory => AppContext.BaseDirectory.TrimEnd('\\', '/');

    public static bool IsPackagedApp()
    {
        try
        {
            uint length = 0;
            int rc = GetCurrentPackageFullName(ref length, IntPtr.Zero);
            return rc == ErrorInsufficientBuffer;
        }
        catch
        {
            return false;
        }
    }

    public static bool IsWindows11OrGreater() =>
        OperatingSystem.IsWindowsVersionAtLeast(10, 0, 22000);

    public static bool IsAdministrator()
    {
        using var identity = WindowsIdentity.GetCurrent();
        var principal = new WindowsPrincipal(identity);
        return principal.IsInRole(WindowsBuiltInRole.Administrator);
    }

    public static string GetLocalAppDataDirectory()
    {
        string baseDir = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(baseDir, "LocaleEmulatorPlus");
    }

    public static string ResolveGlobalConfigPath(string appDir)
    {
        if (IsPackagedApp())
        {
            string dir = GetLocalAppDataDirectory();
            Directory.CreateDirectory(dir);
            return Path.Combine(dir, "config.toml");
        }

        string portable = Path.Combine(appDir, "config.toml");
        if (File.Exists(portable) || ProbeDirectoryWriteStatus(appDir) == DirectoryWriteStatus.Writable)
        {
            return portable;
        }

        string fallback = GetLocalAppDataDirectory();
        Directory.CreateDirectory(fallback);
        return Path.Combine(fallback, "config.toml");
    }

    public static DirectoryWriteStatus ProbeDirectoryWriteStatus(string? directory)
    {
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
        {
            return DirectoryWriteStatus.ReadOnly;
        }

        string? root = Path.GetPathRoot(directory);
        if (!string.IsNullOrEmpty(root))
        {
            if (GetDriveTypeW(root) == DriveCdRom)
            {
                return DirectoryWriteStatus.ReadOnly;
            }
            if (GetVolumeInformationW(root, IntPtr.Zero, 0, IntPtr.Zero, IntPtr.Zero, out uint fsFlags, IntPtr.Zero, 0))
            {
                if ((fsFlags & FileReadOnlyVolume) != 0)
                {
                    return DirectoryWriteStatus.ReadOnly;
                }
            }
        }

        string probe = Path.Combine(directory, $".lep_write_probe_{Environment.ProcessId}_{Environment.TickCount64}.tmp");
        try
        {
            using var fs = new FileStream(probe, FileMode.CreateNew, FileAccess.Write, FileShare.None, 1, FileOptions.DeleteOnClose);
            return DirectoryWriteStatus.Writable;
        }
        catch (UnauthorizedAccessException)
        {
            return IsAdministrator() ? DirectoryWriteStatus.ReadOnly : DirectoryWriteStatus.NeedsAdmin;
        }
        catch (IOException ex)
        {
            int win32 = ex.HResult & 0xFFFF;
            if (win32 == ErrorWriteProtect)
            {
                return DirectoryWriteStatus.ReadOnly;
            }
            if (win32 == ErrorAccessDenied && !IsAdministrator())
            {
                return DirectoryWriteStatus.NeedsAdmin;
            }
            return DirectoryWriteStatus.ReadOnly;
        }
        catch
        {
            return DirectoryWriteStatus.ReadOnly;
        }
    }

    public static List<LocaleEntry> EnumerateLocales()
    {
        var list = new List<LocaleEntry>();
        foreach (var ci in CultureInfo.GetCultures(CultureTypes.SpecificCultures))
        {
            if (string.IsNullOrWhiteSpace(ci.Name))
            {
                continue;
            }
            uint acp = 0;
            GetLocaleInfoEx(ci.Name, LocaleIdefaultAnsiCodepage | LocaleReturnNumber, out acp, 2);
            if (acp == 0)
            {
                acp = (uint)ci.TextInfo.ANSICodePage;
            }
            string display = $"{ci.DisplayName} [{ci.Name} | CP{acp}]";
            list.Add(new LocaleEntry
            {
                LocaleName = ci.Name,
                DisplayName = display,
                AnsiCodePage = acp,
            });
        }

        return list
            .OrderBy(e => e.LocaleName.Equals("ja-JP", StringComparison.OrdinalIgnoreCase) ? 0
                : e.LocaleName.Equals("zh-CN", StringComparison.OrdinalIgnoreCase) ? 1
                : e.LocaleName.Equals("zh-TW", StringComparison.OrdinalIgnoreCase) ? 2
                : e.LocaleName.Equals("ko-KR", StringComparison.OrdinalIgnoreCase) ? 3
                : 4)
            .ThenBy(e => e.DisplayName, StringComparer.CurrentCultureIgnoreCase)
            .ToList();
    }

    public static List<TimezoneEntry> EnumerateTimezones()
    {
        return TimeZoneInfo.GetSystemTimeZones()
            .Select(tz => new TimezoneEntry
            {
                Id = tz.Id,
                DisplayName = tz.DisplayName,
            })
            .ToList();
    }

    public static List<CoreComponentStatus> InspectCoreComponents(string appDir)
    {
        string[] files =
        [
            "LEP.exe",
            "LEPShellExt.dll",
            "LEPHelper_x86.exe",
            "LoaderDll_x64.dll",
            "LocaleEmulatorPlus_x64.dll",
            "LoaderDll_x86.dll",
            "LocaleEmulatorPlus_x86.dll",
        ];

        return files.Select(name => new CoreComponentStatus
        {
            FileName = name,
            Present = File.Exists(Path.Combine(appDir, name)),
        }).ToList();
    }

    public static bool IsClassicContextMenuInstalled(bool allUsers)
    {
        using RegistryKey root = allUsers ? Registry.LocalMachine : Registry.CurrentUser;
        using RegistryKey? key = root.OpenSubKey(@"Software\Classes\*\shell\LocaleEmulatorPlus");
        return key is not null;
    }

    public static bool IsWin11SparsePackageInstalled()
    {
        if (IsPackagedApp())
        {
            return true;
        }
        if (!IsWindows11OrGreater())
        {
            return false;
        }
        using RegistryKey? pkgs = Registry.CurrentUser.OpenSubKey(
            @"Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\Repository\Packages");
        if (pkgs is null)
        {
            return false;
        }
        string prefix = SparsePackageName + "_";
        return pkgs.GetSubKeyNames().Any(n => n.StartsWith(prefix, StringComparison.OrdinalIgnoreCase));
    }

    public static string NormalizePathForMatch(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return "";
        }
        try
        {
            return Path.GetFullPath(path.Trim().Trim('"')).TrimEnd('\\').ToLowerInvariant();
        }
        catch
        {
            return path.Trim().Trim('"').TrimEnd('\\').ToLowerInvariant();
        }
    }

    public static string QuoteArg(string arg)
    {
        if (string.IsNullOrEmpty(arg))
        {
            return "\"\"";
        }
        if (!arg.Any(c => char.IsWhiteSpace(c) || c == '"'))
        {
            return arg;
        }
        return "\"" + arg.Replace("\"", "\\\"") + "\"";
    }

    public static string? BrowseOpenTargetFile(IntPtr ownerHwnd, string? initialDir = null)
    {
        const int maxChars = 4096;
        IntPtr buffer = Marshal.AllocHGlobal(maxChars * 2);
        try
        {
            Marshal.WriteInt16(buffer, 0);
            var ofn = new OpenFileNameW
            {
                lStructSize = Marshal.SizeOf<OpenFileNameW>(),
                hwndOwner = ownerHwnd,
                lpstrFilter = "Executable & Shortcuts (*.exe;*.lnk)\0*.exe;*.lnk\0All Files (*.*)\0*.*\0\0",
                lpstrFile = buffer,
                nMaxFile = maxChars,
                lpstrInitialDir = string.IsNullOrWhiteSpace(initialDir) ? null : initialDir,
                Flags = 0x00001000 | 0x00000800 | 0x00000008 | 0x00100000, // FILEMUSTEXIST | PATHMUSTEXIST | NOCHANGEDIR | NODEREFERENCELINKS
            };
            if (GetOpenFileNameW(ref ofn))
            {
                return Marshal.PtrToStringUni(buffer);
            }
            return null;
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }

    public static string? BrowseSaveShortcutFile(IntPtr ownerHwnd, string suggestedPath)
    {
        const int maxChars = 4096;
        IntPtr buffer = Marshal.AllocHGlobal(maxChars * 2);
        try
        {
            string fileName = Path.GetFileName(suggestedPath);
            byte[] bytes = System.Text.Encoding.Unicode.GetBytes(fileName + "\0");
            Marshal.Copy(bytes, 0, buffer, Math.Min(bytes.Length, maxChars * 2));

            var ofn = new OpenFileNameW
            {
                lStructSize = Marshal.SizeOf<OpenFileNameW>(),
                hwndOwner = ownerHwnd,
                lpstrFilter = "Windows Shortcut (*.lnk)\0*.lnk\0\0",
                lpstrDefExt = "lnk",
                lpstrFile = buffer,
                nMaxFile = maxChars,
                lpstrInitialDir = Path.GetDirectoryName(suggestedPath),
                Flags = 0x00000002 | 0x00000800 | 0x00000008, // OVERWRITEPROMPT | PATHMUSTEXIST | NOCHANGEDIR
            };
            if (GetSaveFileNameW(ref ofn))
            {
                return Marshal.PtrToStringUni(buffer);
            }
            return null;
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }
}
