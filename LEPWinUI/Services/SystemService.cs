using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Threading;
using LEPWinUI.Models;
using Microsoft.Win32;

namespace LEPWinUI.Services;

public static class SystemService
{
    public const string AppVersion = "3.0.2";
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
    private static extern int GetCurrentPackageFamilyName(ref uint packageFamilyNameLength, IntPtr packageFamilyName);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")]
    private static extern IntPtr SetWindowLongPtr64(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll")]
    private static extern bool EnableWindow(IntPtr hWnd, bool bEnable);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr FindWindowW(string? lpClassName, string? lpWindowName);

    [DllImport("user32.dll")]
    private static extern bool ChangeWindowMessageFilterEx(IntPtr hWnd, uint msg, uint action, IntPtr pChangeFilterStruct);

    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("ole32.dll")]
    private static extern int RevokeDragDrop(IntPtr hWnd);

    [DllImport("shell32.dll")]
    private static extern void DragAcceptFiles(IntPtr hWnd, bool fAccept);

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern uint DragQueryFileW(IntPtr hDrop, uint iFile, char[]? lpszFile, uint cch);

    [DllImport("shell32.dll")]
    private static extern void DragFinish(IntPtr hDrop);

    private delegate IntPtr SubclassProc(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam, nuint uIdSubclass, IntPtr dwRefData);

    [DllImport("comctl32.dll")]
    private static extern bool SetWindowSubclass(IntPtr hWnd, SubclassProc pfnSubclass, nuint uIdSubclass, IntPtr dwRefData);

    [DllImport("comctl32.dll")]
    private static extern bool RemoveWindowSubclass(IntPtr hWnd, SubclassProc pfnSubclass, nuint uIdSubclass);

    [DllImport("comctl32.dll")]
    private static extern IntPtr DefSubclassProc(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam);

    public sealed class ElevatedDragDropHook : IDisposable
    {
        private const uint WmDropFiles = 0x0233;
        private const uint WmCopyData = 0x004A;
        private const uint WmCopyGlobalData = 0x0049;
        private const uint MsgfltAllow = 1;
        private const nuint SubclassId = 0x4C455001;

        private readonly List<IntPtr> _hooked = new();
        private readonly SubclassProc _proc;
        private readonly Action<string> _onFileDropped;

        public ElevatedDragDropHook(IntPtr rootHwnd, Action<string> onFileDropped)
        {
            _onFileDropped = onFileDropped;
            _proc = OnWindowMessage;

            var targets = new List<IntPtr> { rootHwnd };
            EnumChildWindows(rootHwnd, (child, _) =>
            {
                targets.Add(child);
                return true;
            }, IntPtr.Zero);

            foreach (IntPtr h in targets)
            {
                try { RevokeDragDrop(h); } catch { }
                ChangeWindowMessageFilterEx(h, WmDropFiles, MsgfltAllow, IntPtr.Zero);
                ChangeWindowMessageFilterEx(h, WmCopyData, MsgfltAllow, IntPtr.Zero);
                ChangeWindowMessageFilterEx(h, WmCopyGlobalData, MsgfltAllow, IntPtr.Zero);
                DragAcceptFiles(h, true);
                if (SetWindowSubclass(h, _proc, SubclassId, IntPtr.Zero))
                {
                    _hooked.Add(h);
                }
            }
        }

        private IntPtr OnWindowMessage(IntPtr hWnd, uint uMsg, IntPtr wParam, IntPtr lParam, nuint uIdSubclass, IntPtr dwRefData)
        {
            if (uMsg == WmDropFiles)
            {
                uint count = DragQueryFileW(wParam, 0xFFFFFFFF, null, 0);
                if (count > 0)
                {
                    uint len = DragQueryFileW(wParam, 0, null, 0);
                    if (len > 0)
                    {
                        char[] buf = new char[len + 1];
                        uint copied = DragQueryFileW(wParam, 0, buf, (uint)buf.Length);
                        if (copied > 0)
                        {
                            string path = new string(buf, 0, (int)copied);
                            _onFileDropped(path);
                        }
                    }
                }
                DragFinish(wParam);
                return IntPtr.Zero;
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        public void Dispose()
        {
            foreach (IntPtr h in _hooked)
            {
                try { RemoveWindowSubclass(h, _proc, SubclassId); } catch { }
            }
            _hooked.Clear();
        }
    }

    public static void SetWindowOwner(IntPtr childHwnd, IntPtr ownerHwnd)
    {
        if (childHwnd != IntPtr.Zero && ownerHwnd != IntPtr.Zero)
        {
            SetWindowLongPtr64(childHwnd, -8, ownerHwnd); // GWLP_HWNDPARENT
        }
    }

    public static void SetWindowEnabled(IntPtr hwnd, bool enabled)
    {
        if (hwnd != IntPtr.Zero)
        {
            EnableWindow(hwnd, enabled);
        }
    }

    public static void BringToForeground(IntPtr hwnd)
    {
        if (hwnd != IntPtr.Zero)
        {
            SetForegroundWindow(hwnd);
        }
    }

    public static string? GetCurrentPackageFamilyNameString()
    {
        try
        {
            uint length = 0;
            int rc = GetCurrentPackageFamilyName(ref length, IntPtr.Zero);
            if (rc != ErrorInsufficientBuffer || length <= 1)
            {
                return null;
            }
            IntPtr buf = Marshal.AllocHGlobal((int)length * 2);
            try
            {
                rc = GetCurrentPackageFamilyName(ref length, buf);
                return rc == 0 ? Marshal.PtrToStringUni(buf) : null;
            }
            finally
            {
                Marshal.FreeHGlobal(buf);
            }
        }
        catch
        {
            return null;
        }
    }

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
        string? pfn = GetCurrentPackageFamilyNameString();
        if (string.IsNullOrEmpty(pfn))
        {
            return false;
        }
        if (pfn.StartsWith(SparsePackageName + "_", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        return true;
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
        if (IsPackagedApp())
        {
            try
            {
                return Windows.Storage.ApplicationData.Current.LocalFolder.Path;
            }
            catch
            {
                string? pfn = GetCurrentPackageFamilyNameString();
                if (!string.IsNullOrEmpty(pfn))
                {
                    string localBase = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                    return Path.Combine(localBase, "Packages", pfn, "LocalState");
                }
            }
        }

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

    public static string ResolveLogDirectory(string appDir, string configPath)
    {
        if (IsPackagedApp())
        {
            string? cfgDir = Path.GetDirectoryName(configPath);
            if (!string.IsNullOrEmpty(cfgDir))
            {
                return Path.Combine(cfgDir, "Log");
            }
        }
        return Path.Combine(appDir, "Log");
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

    public static void RestartExplorer()
    {
        foreach (Process proc in Process.GetProcessesByName("explorer"))
        {
            using (proc)
            {
                try
                {
                    proc.Kill();
                    proc.WaitForExit(3000);
                }
                catch
                {
                }
            }
        }

        Thread.Sleep(300);

        if (FindWindowW("Shell_TrayWnd", null) == IntPtr.Zero)
        {
            string winDir = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
            string explorerExe = Path.Combine(winDir, "explorer.exe");
            Process.Start(new ProcessStartInfo
            {
                FileName = explorerExe,
                UseShellExecute = true,
            });
        }
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
