using System;
using System.Runtime.InteropServices;
using Amemiya.Extensions;

namespace LEPProc
{
    internal class LoaderWrapper
    {
        private const uint CREATE_NORMAL = 0x00000000;
        private const uint CREATE_SUSPENDED = 0x00000004;
        private const uint LEP_ENVIRONMENT_VERSION = 4;
        private LEPB _lepb;

        internal LoaderWrapper()
            : this(null, null, null)
        {
        }

        internal LoaderWrapper(string applicationName)
            : this(applicationName, null, null)
        {
        }

        internal LoaderWrapper(string applicationName, string commandLine, string currentDirectory)
        {
            ApplicationName = applicationName;
            CommandLine = commandLine;
            CurrentDirectory = currentDirectory;

            _lepb = new LEPB
                   {
                       Size = (uint)Marshal.SizeOf(typeof(LEPB)),
                       Version = LEP_ENVIRONMENT_VERSION,
                       AnsiCodePage = 932,
                       OemCodePage = 932,
                       LocaleId = 0x411,
                       FontCharset = 128,
                       RegistryRedirectionMode = 2,
                       HookUILanguageMode = 1,
                       StandardNameOverride = string.Empty,
                       DaylightNameOverride = string.Empty
                   };
            Timezone = "Tokyo Standard Time";
        }

        /// <summary>
        ///     Application that will be run.
        /// </summary>
        internal string ApplicationName { get; set; }

        /// <summary>
        ///     Command arguments.
        /// </summary>
        internal string CommandLine { get; set; }

        /// <summary>
        ///     Working directory.
        /// </summary>
        internal string CurrentDirectory { get; set; }

        /// <summary>
        ///     Whether the process should be created with CREATE_SUSPENDED. Useful when debugging with OllyDbg.
        /// </summary>
        internal bool DebugMode { get; set; }

        /// <summary>
        ///     New AnsiCodePage. Default value is 932.
        /// </summary>
        internal uint AnsiCodePage
        {
            get { return _lepb.AnsiCodePage; }
            set { _lepb.AnsiCodePage = value; }
        }

        /// <summary>
        ///     New OemCodePage. Default value is 932.
        /// </summary>
        internal uint OemCodePage
        {
            get { return _lepb.OemCodePage; }
            set { _lepb.OemCodePage = value; }
        }

        /// <summary>
        ///     New LocaleId. Default value is 0x411(1041).
        /// </summary>
        internal uint LocaleId
        {
            get { return _lepb.LocaleId; }
            set { _lepb.LocaleId = value; }
        }

        /// <summary>
        ///     New FontCharset. Default value is 128(Shift-JIS).
        /// </summary>
        internal uint FontCharset
        {
            get { return _lepb.FontCharset; }
            set { _lepb.FontCharset = value; }
        }

        /// <summary>
        ///     Registry redirection mode: 0 = legacy core 3, 1 = basic 4, 2 = advanced 8.
        /// </summary>
        internal uint RegistryRedirectionMode
        {
            get { return _lepb.RegistryRedirectionMode; }
            set { _lepb.RegistryRedirectionMode = value; }
        }

        /// <summary>
        ///     UI-language hook mode: 0 = off, 1 = Win32 APIs, 2 = native UI APIs and MUI.
        /// </summary>
        internal uint HookUILanguageMode
        {
            get { return _lepb.HookUILanguageMode; }
            set { _lepb.HookUILanguageMode = value; }
        }

        /// <summary>
        ///     String that represents a Timezone.
        ///     This can be found in HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
        /// </summary>
        internal string Timezone
        {
            get { return _lepb.TimezoneId; }
            set
            {
                ValidateEnvironmentString(value, 128);
                if (value.Length == 0 || value.IndexOf('\\') >= 0 || value.IndexOf('/') >= 0)
                    throw new ArgumentException("A Windows time-zone ID is required.", nameof(value));
                _lepb.TimezoneId = value;
            }
        }

        internal string StandardNameOverride
        {
            get { return _lepb.StandardNameOverride; }
            set { ValidateEnvironmentString(value, 32); _lepb.StandardNameOverride = value; }
        }

        internal string DaylightNameOverride
        {
            get { return _lepb.DaylightNameOverride; }
            set { ValidateEnvironmentString(value, 32); _lepb.DaylightNameOverride = value; }
        }

        private static void ValidateEnvironmentString(string value, int capacity)
        {
            if (value == null || value.Length >= capacity || value.IndexOf('\0') >= 0)
                throw new ArgumentException($"Expected fewer than {capacity} UTF-16 characters without embedded NULs.", nameof(value));
        }

        /// <summary>
        ///     Create process.
        /// </summary>
        /// <returns>Error number</returns>
        internal uint Start()
        {
            if (string.IsNullOrEmpty(ApplicationName))
                throw new Exception("ApplicationName cannot null.");

            var newLEPB = ArrayExtensions.StructToBytes(_lepb);

            var locLEPB = Marshal.AllocHGlobal(newLEPB.Length);
            Marshal.Copy(newLEPB, 0, locLEPB, newLEPB.Length);

            AttachConsole(ATTACH_PARENT_PROCESS);

            var startupInfo = new STARTUPINFO { cb = Marshal.SizeOf(typeof(STARTUPINFO)) };
            var processInformation = new PROCESS_INFORMATION();

            var ret = Environment.Is64BitProcess
                          ? LepCreateProcessX64(locLEPB,
                                               ApplicationName,
                                               CommandLine,
                                               CurrentDirectory,
                                               DebugMode ? CREATE_SUSPENDED : CREATE_NORMAL,
                                               ref startupInfo,
                                               out processInformation,
                                               IntPtr.Zero,
                                               IntPtr.Zero,
                                               IntPtr.Zero,
                                               IntPtr.Zero)
                          : LepCreateProcessX86(locLEPB,
                                               ApplicationName,
                                               CommandLine,
                                               CurrentDirectory,
                                               DebugMode ? CREATE_SUSPENDED : CREATE_NORMAL,
                                               ref startupInfo,
                                               out processInformation,
                                               IntPtr.Zero,
                                               IntPtr.Zero,
                                               IntPtr.Zero,
                                               IntPtr.Zero);

            Marshal.FreeHGlobal(locLEPB);

            if (ret == 0)
            {
                WaitForSingleObject(processInformation.hProcess, INFINITE);
                CloseHandle(processInformation.hProcess);
                CloseHandle(processInformation.hThread);
            }

            return ret;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct STARTUPINFO
        {
            public int cb;
            public String lpReserved;
            public String lpDesktop;
            public String lpTitle;
            public uint dwX;
            public uint dwY;
            public uint dwXSize;
            public uint dwYSize;
            public uint dwXCountChars;
            public uint dwYCountChars;
            public uint dwFillAttribute;
            public uint dwFlags;
            public short wShowWindow;
            public short cbReserved2;
            public IntPtr lpReserved2;
            public IntPtr hStdInput;
            public IntPtr hStdOutput;
            public IntPtr hStdError;
        }

        [DllImport("LoaderDll_x86.dll", CharSet = CharSet.Unicode, EntryPoint = "LepCreateProcess")]
        internal static extern uint LepCreateProcessX86(IntPtr lepb,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string applicationName,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string commandLine,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string currentDirectory,
                                                     uint creationFlags,
                                                     ref STARTUPINFO startupInfo,
                                                     out PROCESS_INFORMATION processInformation,
                                                     IntPtr processAttributes,
                                                     IntPtr threadAttributes,
                                                     IntPtr environment,
                                                     IntPtr token);

        [DllImport("LoaderDll_x64.dll", CharSet = CharSet.Unicode, EntryPoint = "LepCreateProcess")]
        internal static extern uint LepCreateProcessX64(IntPtr lepb,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string applicationName,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string commandLine,
                                                     [MarshalAs(UnmanagedType.LPWStr), In] string currentDirectory,
                                                     uint creationFlags,
                                                     ref STARTUPINFO startupInfo,
                                                     out PROCESS_INFORMATION processInformation,
                                                     IntPtr processAttributes,
                                                     IntPtr threadAttributes,
                                                     IntPtr environment,
                                                     IntPtr token);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern UInt32 WaitForSingleObject(IntPtr hHandle, UInt32 dwMilliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool CloseHandle(IntPtr hHandle);

        const uint INFINITE = 0xFFFFFFFF;

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AttachConsole(uint dwProcessId);

        const uint ATTACH_PARENT_PROCESS = 0x0ffffffff;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        internal struct LEPB
        {
            internal uint Size;
            internal uint Version;
            internal uint AnsiCodePage;
            internal uint OemCodePage;
            internal uint LocaleId;
            internal uint FontCharset;
            internal uint RegistryRedirectionMode;
            internal uint HookUILanguageMode;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] internal string TimezoneId;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] internal string StandardNameOverride;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] internal string DaylightNameOverride;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct PROCESS_INFORMATION
        {
            internal IntPtr hProcess;
            internal IntPtr hThread;
            internal uint dwProcessId;
            internal uint dwThreadId;
        }

    }
}
