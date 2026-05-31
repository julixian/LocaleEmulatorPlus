using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Windows.Forms;
using System.Xml.Linq;

namespace LEPCommonLibrary
{
    public static class GlobalHelper
    {
        public static string GlobalVersionPath =
            Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                         "LEPVersion.xml");

        public static string GetLEPVersion()
        {
            try
            {
                var doc = XDocument.Load(GlobalVersionPath);

                return doc.Descendants("LEPVersion").First().Attribute("Version").Value;
            }
            catch
            {
                return "0.0.0.0";
            }
        }

        public static int GetLastUpdate()
        {
            try
            {
                var doc = XDocument.Load(GlobalVersionPath);

                return int.Parse(doc.Descendants("LEPVersion").First().Attribute("LastUpdate").Value);
            }
            catch
            {
                return 0;
            }
        }

        public static void SetLastUpdate(int date)
        {
            try
            {
                var doc = XDocument.Load(GlobalVersionPath);

                doc.Descendants("LEPVersion").First().Attribute("LastUpdate").Value = date.ToString().PadLeft(8, '0');

                doc.Save(GlobalVersionPath);
            }
            catch
            {
            }
        }

        public static void ShowErrorDebugMessageBox(string commandLine, uint errorCode)
        {
            MessageBox.Show(
                            $"Error Number: {Convert.ToString(errorCode, 16).ToUpper()}\r\n"
                            + $"Commands: {commandLine}\r\n" + "\r\n"
                            + $"{string.Format($"{Environment.OSVersion} {(SystemHelper.Is64BitOS() ? "x64" : "x86")}", Environment.OSVersion, SystemHelper.Is64BitOS() ? "x64" : "x86")}\r\n"
                            + $"{GenerateSystemDllVersionList()}\r\n"
                            + "If you have any antivirus software running, please turn it off and try again.\r\n"
                            + "If this window still shows, try go to Safe Mode and drag the application "
                            + "executable onto LEPProc_x86.exe or LEPProc_x64.exe.\r\n"
                            + "If you have tried all above but none of them works, feel free to submit an issue at\r\n"
                            + "https://github.com/julixian/LocaleEmulatorPlus/issues.\r\n" + "\r\n" + "\r\n"
                            + "You can press CTRL+C to copy this message to your clipboard.\r\n",
                            "Locale Emulator Plus Version " + GetLEPVersion());
        }

        private static string GenerateSystemDllVersionList()
        {
            string[] dlls = {"NTDLL.DLL", "KERNELBASE.DLL", "KERNEL32.DLL", "USER32.DLL", "GDI32.DLL"};

            var result = new StringBuilder();

            foreach (var dll in dlls)
            {
                var version = FileVersionInfo.GetVersionInfo(
                                                             Path.Combine(
                                                                          Path.GetPathRoot(Environment.SystemDirectory),
                                                                          SystemHelper.Is64BitOS()
                                                                              ? @"Windows\SysWOW64\"
                                                                              : @"Windows\System32\",
                                                                          dll));

                result.Append(dll);
                result.Append(": ");
                result.Append(
                              $"{version.FileMajorPart}.{version.FileMinorPart}.{version.FileBuildPart}.{version.FilePrivatePart}");
                result.Append("\r\n");
            }

            return result.ToString();
        }

        public static bool CheckCoreDLLs()
        {
            var dlls = SystemHelper.Is64BitOS()
                           ? new[]
                             {
                                 "LoaderDll_x86.dll",
                                 "LocaleEmulatorPlus_x86.dll",
                                 "LoaderDll_x64.dll",
                                 "LocaleEmulatorPlus_x64.dll"
                             }
                           : new[]
                             {
                                 "LoaderDll_x86.dll",
                                 "LocaleEmulatorPlus_x86.dll"
                             };

            return
                dlls.Select(dll => Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), dll))
                    .All(File.Exists);
        }

        public static string GetCoreDllListForError()
        {
            return SystemHelper.Is64BitOS()
                       ? "LoaderDll_x86.dll\r\n" +
                         "LocaleEmulatorPlus_x86.dll\r\n" +
                         "LoaderDll_x64.dll\r\n" +
                         "LocaleEmulatorPlus_x64.dll"
                       : "LoaderDll_x86.dll\r\n" +
                         "LocaleEmulatorPlus_x86.dll";
        }

        public static string GetExecutableForAssociationTarget(string path)
        {
            if (string.IsNullOrEmpty(path))
                return string.Empty;

            var ext = Path.GetExtension(path).ToLower();
            if (ext == ".exe")
                return path;

            if (!AssociationReader.HaveAssociatedProgram(ext))
                return string.Empty;

            return AssociationReader.GetAssociatedProgram(ext)[0];
        }

        public static PEType GetTargetPEType(string path)
        {
            return PEFileReader.GetPEType(GetExecutableForAssociationTarget(path));
        }

        public static string GetLEPProcPathForTarget(string path)
        {
            var baseDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            var targetType = GetTargetPEType(path);
            var procName = targetType == PEType.X64 && SystemHelper.Is64BitOS()
                               ? "LEPProc_x64.exe"
                               : "LEPProc_x86.exe";

            return Path.Combine(baseDir, procName);
        }
    }
}
