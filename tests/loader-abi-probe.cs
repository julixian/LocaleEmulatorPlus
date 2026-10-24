using System;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using Amemiya.Extensions;
using LEPProc;
using Microsoft.Win32;

internal static class LoaderAbiProbe
{
    [DllImport("kernel32.dll")] private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
    [DllImport("kernel32.dll")] private static extern bool GetExitCodeProcess(IntPtr process, out uint code);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] private static extern bool TerminateProcess(IntPtr process, uint code);

    private static void Assert(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }

    private static void Reject(Action action)
    {
        try { action(); }
        catch (ArgumentException) { return; }
        throw new Exception("Invalid configuration was accepted.");
    }

    private static void CheckLayout()
    {
        Assert(Marshal.SizeOf(typeof(LoaderWrapper.LEPB)) == 416, "LEPB size");
        string[] fields = { "Size", "Version", "AnsiCodePage", "OemCodePage", "LocaleId", "FontCharset",
            "RegistryRedirectionMode", "HookUILanguageMode", "TimezoneId", "StandardNameOverride", "DaylightNameOverride" };
        int[] offsets = { 0, 4, 8, 12, 16, 20, 24, 28, 32, 288, 352 };
        for (int i = 0; i < fields.Length; ++i)
            Assert(Marshal.OffsetOf(typeof(LoaderWrapper.LEPB), fields[i]).ToInt32() == offsets[i], fields[i]);
        Assert(Marshal.SizeOf(typeof(LoaderWrapper.PROCESS_INFORMATION)) == (IntPtr.Size == 8 ? 24 : 16), "Process information size");
        string[] processFields = { "hProcess", "hThread", "dwProcessId", "dwThreadId" };
        int[] processOffsets = { 0, IntPtr.Size, 2 * IntPtr.Size, 2 * IntPtr.Size + 4 };
        for (int i = 0; i < processFields.Length; ++i)
            Assert(Marshal.OffsetOf(typeof(LoaderWrapper.PROCESS_INFORMATION), processFields[i]).ToInt32() == processOffsets[i], processFields[i]);
    }

    private static void WriteName(byte[] buffer, int offset, string value)
    {
        Array.Copy(Encoding.Unicode.GetBytes(value), 0, buffer, offset, value.Length * 2);
    }

    private static void PublishExpected(LoaderWrapper wrapper)
    {
        byte[] expected = new byte[172];
        using (var root = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
        using (var key = root.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones\" + wrapper.Timezone))
        {
            Assert(key != null, "Time-zone key");
            var tzi = (byte[])key.GetValue("TZI");
            Assert(tzi.Length == 44, "TZI size");
            Array.Copy(tzi, 0, expected, 0, 4);
            Array.Copy(tzi, 12, expected, 68, 16);
            Array.Copy(tzi, 4, expected, 84, 4);
            Array.Copy(tzi, 28, expected, 152, 16);
            Array.Copy(tzi, 8, expected, 168, 4);
            bool tokyo = wrapper.Timezone == "Tokyo Standard Time";
            string standard = tokyo ? "東京標準時" : (string)key.GetValue("Std");
            string daylight = tokyo ? standard : (string)key.GetValue("Dlt");
            WriteName(expected, 4, wrapper.StandardNameOverride.Length != 0 ? wrapper.StandardNameOverride : standard);
            WriteName(expected, 88, wrapper.DaylightNameOverride.Length != 0 ? wrapper.DaylightNameOverride : daylight);
        }
        Environment.SetEnvironmentVariable("LEP_TEST_TIMEZONE_SNAPSHOT", BitConverter.ToString(expected).Replace("-", ""));
    }

    private static int Main(string[] args)
    {
        try
        {
            if (args.Length != 2) throw new Exception("Usage: loader-abi-probe <case> <native timezone-probe.exe>");
            CheckLayout();
            var wrapper = new LoaderWrapper();
            Assert(wrapper.Timezone == "Tokyo Standard Time", "Default ID");
            Reject(() => wrapper.Timezone = null);
            Reject(() => wrapper.Timezone = "");
            Reject(() => wrapper.Timezone = new string('x', 128));
            Reject(() => wrapper.Timezone = "Tokyo Standard Time\\subkey");
            Reject(() => wrapper.Timezone = "UTC\0suffix");
            Reject(() => wrapper.StandardNameOverride = new string('x', 32));
            Reject(() => wrapper.DaylightNameOverride = "name\0suffix");
            string test = args[0];
            if (test == "pacific") wrapper.Timezone = "Pacific Standard Time";
            if (test == "southern") wrapper.Timezone = "AUS Eastern Standard Time";
            if (test == "standard-only") wrapper.StandardNameOverride = "标准时间";
            if (test == "daylight-only") wrapper.DaylightNameOverride = "夏令时间";
            if (test == "overrides" || test == "pacific" || test == "southern")
            {
                wrapper.StandardNameOverride = "Custom standard";
                wrapper.DaylightNameOverride = "Custom daylight";
            }
            if (test == "max-names")
            {
                wrapper.StandardNameOverride = new string('标', 31);
                wrapper.DaylightNameOverride = new string('夏', 31);
            }
            PublishExpected(wrapper);
            var config = (LoaderWrapper.LEPB)typeof(LoaderWrapper).GetField("_lepb", BindingFlags.Instance | BindingFlags.NonPublic).GetValue(wrapper);
            var bytes = ArrayExtensions.StructToBytes(config);
            Assert(bytes.Length == 416 && BitConverter.ToUInt32(bytes, 4) == 4, "Serialized header");
            Assert(Encoding.Unicode.GetString(bytes, 32, 256).TrimEnd('\0') == wrapper.Timezone, "Serialized ID");
            Assert(Encoding.Unicode.GetString(bytes, 288, 64).TrimEnd('\0') == wrapper.StandardNameOverride, "Serialized standard override");
            Assert(Encoding.Unicode.GetString(bytes, 352, 64).TrimEnd('\0') == wrapper.DaylightNameOverride, "Serialized daylight override");
            IntPtr input = Marshal.AllocHGlobal(bytes.Length);
            try
            {
                Marshal.Copy(bytes, 0, input, bytes.Length);
                string executable = System.IO.Path.GetFullPath(args[1]);
                string command = "\"" + executable + "\" --child 0 \"" + executable + "\" \"" + executable + "\"";
                var startup = new LoaderWrapper.STARTUPINFO { cb = Marshal.SizeOf(typeof(LoaderWrapper.STARTUPINFO)) };
                LoaderWrapper.PROCESS_INFORMATION process;
                uint status = Environment.Is64BitProcess
                    ? LoaderWrapper.LepCreateProcessX64(input, executable, command, null, 0, ref startup, out process, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero)
                    : LoaderWrapper.LepCreateProcessX86(input, executable, command, null, 0, ref startup, out process, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                Assert(unchecked((int)status) >= 0, "Loader status: " + status.ToString("X8"));
                try
                {
                    if (WaitForSingleObject(process.hProcess, 20000) != 0)
                    {
                        TerminateProcess(process.hProcess, 90);
                        throw new Exception("Native child timed out.");
                    }
                    uint code;
                    Assert(GetExitCodeProcess(process.hProcess, out code) && code == 0, "Native child exit: " + code);
                }
                finally { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
            }
            finally { Marshal.FreeHGlobal(input); }
            Console.WriteLine((Environment.Is64BitProcess ? "x64/" : "x86/") + test + ": ABI and native child passed");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
