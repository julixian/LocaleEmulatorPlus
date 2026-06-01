using Microsoft.Win32;
using System;

namespace LEPInstaller
{
    class ShellExtensionManager
    {
        private const string Clsid = "{517F67EF-8967-4E4F-9713-B60ED2E9288A}";
        private const string FileType = "*";
        private const string FriendlyName = "LocaleEmulatorPlus.LEPContextMenuHandler Class";
        private const string ApprovedKeyName = @"Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved";
        private static string KeyName => $@"Software\Classes\{FileType}\shellex\ContextMenuHandlers\{Clsid}";

        public static void RegisterShellExtContextMenuHandler(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;

            using (var key = rootName.CreateSubKey(KeyName))
            {
                key?.SetValue(null, FriendlyName);
            }

            using (var key = rootName.CreateSubKey(ApprovedKeyName))
            {
                key?.SetValue(Clsid, FriendlyName);
            }
        }

        public static void UnregisterShellExtContextMenuHandler(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;

            DeleteSubKeyTreeIfExists(rootName, KeyName);

            using (var key = rootName.OpenSubKey(ApprovedKeyName, true))
            {
                key?.DeleteValue(Clsid, false);
            }
        }

        public static bool IsInstalled(bool allUsers)
        {
            var rootName = allUsers ? Registry.LocalMachine : Registry.CurrentUser;

            using (var key = rootName.OpenSubKey(KeyName, false))
            {
                return key != null;
            }
        }

        private static void DeleteSubKeyTreeIfExists(RegistryKey rootName, string keyName)
        {
            try
            {
                rootName.DeleteSubKeyTree(keyName);
            }
            catch (ArgumentException)
            {
            }
        }
    }
}
