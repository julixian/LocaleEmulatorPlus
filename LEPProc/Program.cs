using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using LEPCommonLibrary;

namespace LEPProc
{
    internal static class Program
    {
        internal static string[] Args;

        /// <summary>
        ///     The main entry point for the application.
        /// </summary>
        [STAThread]
        private static void Main(string[] args)
        {
            SystemHelper.DisableDPIScale();

            if (RestartWithShellParent(args))
                return;

            try
            {
                Process.Start(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                        "LEPUpdater.exe"),
                    "schedule");
            }
            catch
            {
            }

            if (!GlobalHelper.CheckCoreDLLs())
            {
                MessageBox.Show(
                    "Some of the core Dlls are missing.\r\n" +
                    "Please whitelist these Dlls in your antivirus software, then download and re-install LEP.\r\n"
                    +
                    "\r\n" +
                    "These Dlls are:\r\n" +
                    GlobalHelper.GetCoreDllListForError(),
                    "Locale Emulator Plus Version " + GlobalHelper.GetLEPVersion(),
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);

                return;
            }
            
            //If global config does not exist, create a new one.
            LEPConfig.CheckGlobalConfigFile(true);

            if (args.Length == 0)
            {
                MessageBox.Show(
                    "Welcome to Locale Emulator Plus command line tool.\r\n" +
                    "\r\n" +
                    "Usage: LEPProc_x86.exe / LEPProc_x64.exe\r\n" +
                    "\tpath\r\n" +
                    "\t-run path [args]\r\n" +
                    "\t-runas guid path [args]\r\n" +
                    "\t-manage path\r\n" +
                    "\t-global\r\n" +
                    "\r\n" +
                    "path\tFull path of the target application.\r\n" +
                    "guid\tGuid of the target profile (in LEPConfig.xml).\r\n" +
                    "args\tAdditional arguments will be passed to the application.\r\n" +
                    "\r\n" +
                    "path\tRun an application with \r\n" +
                    "\t\t(i) its own profile (if any) or \r\n" +
                    "\t\t(ii) first global profile (if any) or \r\n" +
                    "\t\t(iii) default ja-JP profile.\r\n" +
                    "-run\tRun an application with it's own profile.\r\n" +
                    "-runas\tRun an application with a global profile of specific Guid.\r\n" +
                    "-manage\tModify the profile of one application.\r\n" +
                    "-global\tOpen Global Profile Manager.\r\n" +
                    "\r\n" +
                    "\r\n" +
                    "You can press CTRL+C to copy this message to your clipboard.\r\n",
                    "Locale Emulator Plus Version " + GlobalHelper.GetLEPVersion()
                );

                GlobalHelper.ShowErrorDebugMessageBox("SYSTEM_REPORT", 0);

                return;
            }

            try
            {
                Args = args;

                switch (Args[0])
                {
                    case "-run": //-run %APP%
                        RunWithIndependentProfile(Args[1]);
                        break;

                    case "-manage": //-manage %APP%
                        Process.Start(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                                "LEPGUI.exe"),
                            $"\"{Args[1]}.lep.config\"");
                        break;

                    case "-global": //-global
                        Process.Start(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                            "LEPGUI.exe"));
                        break;

                    case "-runas": //-runas %GUID% %APP%
                        RunWithGlobalProfile(Args[1], Args[2]);
                        break;

                    default:
                        if (File.Exists(Args[0]))
                        {
                            RunWithDefaultProfile(Args[0]);
                        }
                        break;
                }
            }
            catch
            {
            }
        }

        private static void RunWithDefaultProfile(string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);
            if (RestartWithMatchingArchitecture(path))
                return;

            var conf = path + ".lep.config";

            var appProfile = LEPConfig.GetProfiles(conf);
            var globalProfiles = LEPConfig.GetProfiles();

            // app profile > first global profile > new profile(ja-JP)
            var profile = appProfile.Any()
                ? appProfile.First()
                : globalProfiles.Any() ? globalProfiles.First() : new LEPProfile(true);

            DoRunWithLEPProfile(path, 1, profile);
        }

        private static void RunWithGlobalProfile(string guid, string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);
            if (RestartWithMatchingArchitecture(path))
                return;

            // We do not check whether the config exists because only when it exists can this method be called.

            var profile = LEPConfig.GetProfiles().First(p => p.Guid == guid);

            DoRunWithLEPProfile(path, 3, profile);
        }

        private static void RunWithIndependentProfile(string path)
        {
            path = SystemHelper.EnsureAbsolutePath(path);
            if (RestartWithMatchingArchitecture(path))
                return;

            var conf = path + ".lep.config";

            if (!File.Exists(conf))
            {
                Process.Start(
                    Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "LEPGUI.exe"),
                    $"\"{path}.lep.config\"");
            }
            else
            {
                var profile = LEPConfig.GetProfiles(conf)[0];
                DoRunWithLEPProfile(path, 2, profile);
            }
        }

        private static void DoRunWithLEPProfile(string absPath, int argumentsStart, LEPProfile profile)
        {
            try
            {
                if (profile.RunAsAdmin && !SystemHelper.IsAdministrator())
                {
                    ElevateProcess();

                    return;
                }

                if (profile.RunWithSuspend)
                {
                    if (DialogResult.No ==
                        MessageBox.Show(
                            "You are running a exectuable with CREATE_SUSPENDED flag.\r\n" +
                            "\r\n" +
                            "The exectuable will be executed after you click the \"Yes\" button, " +
                            "but as a background process which has no notifications at all." +
                            "You can attach it by using OllyDbg, or stop it with Task Manager.\r\n",
                            "Locale Emulator Plus Debug Mode Warning",
                            MessageBoxButtons.YesNo,
                            MessageBoxIcon.Information
                            ))
                        return;
                }

                var applicationName = string.Empty;
                var commandLine = string.Empty;

                if (Path.GetExtension(absPath).ToLower() == ".exe")
                {
                    applicationName = absPath;

                    commandLine = absPath.StartsWith("\"")
                        ? $"{absPath} "
                        : $"\"{absPath}\" ";

                    // use arguments in le.config, prior to command line arguments
                    commandLine += string.IsNullOrEmpty(profile.Parameter) && Args.Skip(argumentsStart).Any()
                        ? Args.Skip(argumentsStart).Aggregate((a, b) => $"{a} {b}")
                        : profile.Parameter;
                }
                else
                {
                    var jb = AssociationReader.GetAssociatedProgram(Path.GetExtension(absPath));

                    if (jb == null)
                        return;

                    applicationName = jb[0];

                    commandLine = jb[0].StartsWith("\"")
                        ? $"{jb[0]} "
                        : $"\"{jb[0]}\" ";
                    commandLine += jb[1].Replace("%1", absPath).Replace("%*", profile.Parameter);
                }

                var currentDirectory = Path.GetDirectoryName(absPath);
                var ansiCodePage = (uint) CultureInfo.GetCultureInfo(profile.Location).TextInfo.ANSICodePage;
                var oemCodePage = (uint) CultureInfo.GetCultureInfo(profile.Location).TextInfo.OEMCodePage;
                var localeID = (uint) CultureInfo.GetCultureInfo(profile.Location).TextInfo.LCID;
                var defaultCharset = (uint)
                    GetCharsetFromANSICodepage(CultureInfo.GetCultureInfo(profile.Location)
                        .TextInfo.ANSICodePage);

                var l = new LoaderWrapper
                {
                    ApplicationName = applicationName,
                    CommandLine = commandLine,
                    CurrentDirectory = currentDirectory,
                    AnsiCodePage = ansiCodePage,
                    OemCodePage = oemCodePage,
                    LocaleID = localeID,
                    DefaultCharset = defaultCharset,
                    RegistryRedirectionMode = (uint) profile.RegistryRedirectionMode,
                    HookUILanguageMode = (uint) profile.HookUILanguageMode,
                    Timezone = profile.Timezone,
                    DebugMode = profile.RunWithSuspend
                };

                uint ret;
                if ((ret = l.Start()) != 0)
                {
                    if (SystemHelper.IsAdministrator())
                    {
                        GlobalHelper.ShowErrorDebugMessageBox(commandLine, ret);
                    }
                    else
                    {
                        ElevateProcess();
                    }
                }
            }
            catch (Exception e)
            {
                MessageBox.Show(e.ToString());
            }
        }

        private static void ElevateProcess()
        {
            try
            {
                SystemHelper.RunWithElevatedProcess(
                    Assembly.GetExecutingAssembly().Location,
                    Args);
            }
            catch (Exception)
            {
            }
        }

        private static bool RestartWithMatchingArchitecture(string targetPath)
        {
            var expectedPath = GlobalHelper.GetLEPProcPathForTarget(targetPath);
            var currentPath = Assembly.GetExecutingAssembly().Location;

            if (string.Equals(expectedPath, currentPath, StringComparison.OrdinalIgnoreCase) || !File.Exists(expectedPath))
                return false;

            Process.Start(expectedPath, BuildCommandLine(Args));
            return true;
        }

        private static bool RestartWithShellParent(string[] args)
        {
            if (SystemHelper.IsAdministrator())
                return false;

            var shellWindow = GetShellWindow();
            if (shellWindow == IntPtr.Zero)
                return false;

            uint shellProcessId;
            GetWindowThreadProcessId(shellWindow, out shellProcessId);
            if (shellProcessId == 0 || GetParentProcessId() == shellProcessId)
                return false;

            var shellProcess = OpenProcess(PROCESS_CREATE_PROCESS, false, shellProcessId);
            if (shellProcess == IntPtr.Zero)
                return false;

            IntPtr attributeList = IntPtr.Zero;
            IntPtr parentProcessValue = IntPtr.Zero;
            var attributeListInitialized = false;
            try
            {
                var attributeListSize = IntPtr.Zero;
                InitializeProcThreadAttributeList(IntPtr.Zero, 1, 0, ref attributeListSize);
                if (attributeListSize == IntPtr.Zero)
                    return false;

                attributeList = Marshal.AllocHGlobal(attributeListSize);
                if (!InitializeProcThreadAttributeList(attributeList, 1, 0, ref attributeListSize))
                    return false;
                attributeListInitialized = true;

                parentProcessValue = Marshal.AllocHGlobal(IntPtr.Size);
                Marshal.WriteIntPtr(parentProcessValue, shellProcess);
                if (!UpdateProcThreadAttribute(
                        attributeList,
                        0,
                        (IntPtr) PROC_THREAD_ATTRIBUTE_PARENT_PROCESS,
                        parentProcessValue,
                        (IntPtr) IntPtr.Size,
                        IntPtr.Zero,
                        IntPtr.Zero))
                    return false;

                var executablePath = Assembly.GetExecutingAssembly().Location;
                var commandLine = new StringBuilder(QuoteArgument(executablePath));
                if (args.Length != 0)
                {
                    commandLine.Append(' ');
                    commandLine.Append(BuildCommandLine(args));
                }

                var startupInfo = new STARTUPINFOEX
                {
                    StartupInfo = {cb = Marshal.SizeOf(typeof(STARTUPINFOEX))},
                    AttributeList = attributeList
                };
                PROCESS_INFORMATION_NATIVE processInformation;
                if (!CreateProcess(
                        executablePath,
                        commandLine,
                        IntPtr.Zero,
                        IntPtr.Zero,
                        false,
                        EXTENDED_STARTUPINFO_PRESENT,
                        IntPtr.Zero,
                        null,
                        ref startupInfo,
                        out processInformation))
                    return false;

                CloseHandle(processInformation.Process);
                CloseHandle(processInformation.Thread);
                return true;
            }
            finally
            {
                if (attributeList != IntPtr.Zero)
                {
                    if (attributeListInitialized)
                        DeleteProcThreadAttributeList(attributeList);
                    Marshal.FreeHGlobal(attributeList);
                }

                if (parentProcessValue != IntPtr.Zero)
                    Marshal.FreeHGlobal(parentProcessValue);

                CloseHandle(shellProcess);
            }
        }

        private static uint GetParentProcessId()
        {
            PROCESS_BASIC_INFORMATION processInformation;
            int returnLength;
            var status = NtQueryInformationProcess(
                Process.GetCurrentProcess().Handle,
                0,
                out processInformation,
                Marshal.SizeOf(typeof(PROCESS_BASIC_INFORMATION)),
                out returnLength);
            return status >= 0 ? (uint) processInformation.InheritedFromUniqueProcessId.ToInt64() : 0;
        }

        private static string BuildCommandLine(string[] args)
        {
            return string.Join(" ", args.Select(QuoteArgument));
        }

        private static string QuoteArgument(string arg)
        {
            if (string.IsNullOrEmpty(arg))
                return "\"\"";

            if (arg.IndexOfAny(new[] {' ', '\t', '\n', '\v', '"'}) < 0)
                return arg;

            var result = new StringBuilder();
            result.Append('"');

            var backslashes = 0;
            foreach (var c in arg)
            {
                if (c == '\\')
                {
                    backslashes++;
                    continue;
                }

                if (c == '"')
                {
                    result.Append('\\', backslashes * 2 + 1);
                    result.Append('"');
                }
                else
                {
                    result.Append('\\', backslashes);
                    result.Append(c);
                }

                backslashes = 0;
            }

            result.Append('\\', backslashes * 2);
            result.Append('"');

            return result.ToString();
        }

        private static int GetCharsetFromANSICodepage(int ansicp)
        {
            const int ANSI_CHARSET = 0;
            const int DEFAULT_CHARSET = 1;
            const int SYMBOL_CHARSET = 2;
            const int SHIFTJIS_CHARSET = 128;
            const int HANGEUL_CHARSET = 129;
            const int HANGUL_CHARSET = 129;
            const int GB2312_CHARSET = 134;
            const int CHINESEBIG5_CHARSET = 136;
            const int OEM_CHARSET = 255;
            const int JOHAB_CHARSET = 130;
            const int HEBREW_CHARSET = 177;
            const int ARABIC_CHARSET = 178;
            const int GREEK_CHARSET = 161;
            const int TURKISH_CHARSET = 162;
            const int VIETNAMESE_CHARSET = 163;
            const int THAI_CHARSET = 222;
            const int EASTEUROPE_CHARSET = 238;
            const int RUSSIAN_CHARSET = 204;
            const int MAC_CHARSET = 77;
            const int BALTIC_CHARSET = 186;

            var charset = ANSI_CHARSET;

            switch (ansicp)
            {
                case 932: // Japanese
                    charset = SHIFTJIS_CHARSET;
                    break;
                case 936: // Simplified Chinese
                    charset = GB2312_CHARSET;
                    break;
                case 949: // Korean
                    charset = HANGEUL_CHARSET;
                    break;
                case 950: // Traditional Chinese
                    charset = CHINESEBIG5_CHARSET;
                    break;
                case 1250: // Eastern Europe
                    charset = EASTEUROPE_CHARSET;
                    break;
                case 1251: // Russian
                    charset = RUSSIAN_CHARSET;
                    break;
                case 1252: // Western European Languages
                    charset = ANSI_CHARSET;
                    break;
                case 1253: // Greek
                    charset = GREEK_CHARSET;
                    break;
                case 1254: // Turkish
                    charset = TURKISH_CHARSET;
                    break;
                case 1255: // Hebrew
                    charset = HEBREW_CHARSET;
                    break;
                case 1256: // Arabic
                    charset = ARABIC_CHARSET;
                    break;
                case 1257: // Baltic
                    charset = BALTIC_CHARSET;
                    break;
            }

            return charset;
        }

        private const uint PROCESS_CREATE_PROCESS = 0x0080;
        private const uint EXTENDED_STARTUPINFO_PRESENT = 0x00080000;
        private const long PROC_THREAD_ATTRIBUTE_PARENT_PROCESS = 0x00020000;

        [StructLayout(LayoutKind.Sequential)]
        private struct PROCESS_BASIC_INFORMATION
        {
            internal IntPtr Reserved1;
            internal IntPtr PebBaseAddress;
            internal IntPtr Reserved2_0;
            internal IntPtr Reserved2_1;
            internal IntPtr UniqueProcessId;
            internal IntPtr InheritedFromUniqueProcessId;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct STARTUPINFO_NATIVE
        {
            internal int cb;
            internal IntPtr Reserved;
            internal IntPtr Desktop;
            internal IntPtr Title;
            internal uint X;
            internal uint Y;
            internal uint XSize;
            internal uint YSize;
            internal uint XCountChars;
            internal uint YCountChars;
            internal uint FillAttribute;
            internal uint Flags;
            internal ushort ShowWindow;
            internal ushort Reserved2Size;
            internal IntPtr Reserved2;
            internal IntPtr StandardInput;
            internal IntPtr StandardOutput;
            internal IntPtr StandardError;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct STARTUPINFOEX
        {
            internal STARTUPINFO_NATIVE StartupInfo;
            internal IntPtr AttributeList;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct PROCESS_INFORMATION_NATIVE
        {
            internal IntPtr Process;
            internal IntPtr Thread;
            internal uint ProcessId;
            internal uint ThreadId;
        }

        [DllImport("user32.dll")]
        private static extern IntPtr GetShellWindow();

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, uint processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool InitializeProcThreadAttributeList(
            IntPtr attributeList,
            int attributeCount,
            int flags,
            ref IntPtr size);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool UpdateProcThreadAttribute(
            IntPtr attributeList,
            uint flags,
            IntPtr attribute,
            IntPtr value,
            IntPtr size,
            IntPtr previousValue,
            IntPtr returnSize);

        [DllImport("kernel32.dll")]
        private static extern void DeleteProcThreadAttributeList(IntPtr attributeList);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CreateProcess(
            string applicationName,
            StringBuilder commandLine,
            IntPtr processAttributes,
            IntPtr threadAttributes,
            bool inheritHandles,
            uint creationFlags,
            IntPtr environment,
            string currentDirectory,
            ref STARTUPINFOEX startupInfo,
            out PROCESS_INFORMATION_NATIVE processInformation);

        [DllImport("kernel32.dll")]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("ntdll.dll")]
        private static extern int NtQueryInformationProcess(
            IntPtr process,
            int processInformationClass,
            out PROCESS_BASIC_INFORMATION processInformation,
            int processInformationLength,
            out int returnLength);
    }
}
