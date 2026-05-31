using System;
using System.IO;
using System.Windows;
using LEPCommonLibrary;

namespace LEPGUI
{
    /// <summary>
    ///     Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        internal static string StandaloneFilePath = string.Empty;

        protected override void OnStartup(StartupEventArgs e)
        {
            AppDomain.CurrentDomain.UnhandledException +=
                (sender, args) => MessageBox.Show(((Exception) args.ExceptionObject).Message);

            base.OnStartup(e);
        }

        private void App_OnStartup(object sender, StartupEventArgs e)
        {
            if (e.Args.Length != 0)
            {
                StandaloneFilePath = SystemHelper.EnsureAbsolutePath(e.Args[0]);

                // This happens when user is trying to drop a exe onto LEPGUI.
                if (!StandaloneFilePath.EndsWith(".lep.config", true, null))
                    StandaloneFilePath += ".lep.config";
            }

            var isGlobalProfile = string.IsNullOrEmpty(StandaloneFilePath);

            LEPConfig.CheckGlobalConfigFile(true);

            // We check StandaloneFilePath before loading UI, because this wil be faster.
            if (
                !SystemHelper.CheckPermission(isGlobalProfile
                                                  ? Path.GetDirectoryName(LEPConfig.GlobalConfigPath)
                                                  : Path.GetDirectoryName(StandaloneFilePath)))
            {
                if (SystemHelper.IsAdministrator())
                {
                    // We can do nothing now.
                    if (isGlobalProfile)
                        MessageBox.Show(
                                        "Home directory is not writable. \r\n"
                                        + "Please move LEP to another location and try again.\r\n"
                                        + $"Home directory: {Path.GetDirectoryName(LEPConfig.GlobalConfigPath)}",
                                        "Locale Emulator Plus",
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);
                    else
                        MessageBox.Show(
                                        "The directory is not writable.\r\n" + "Please use global profile instead.\r\n"
                                        + $"Current Directory: {Path.GetDirectoryName(StandaloneFilePath)}",
                                        "Locale Emulator Plus",
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);

                    Current.Shutdown();
                }
                else
                {
                    // If we are not administrator, we can ask for administrator permission.
                    try
                    {
                        SystemHelper.RunWithElevatedProcess(
                                                            Path.Combine(
                                                                         Path.GetDirectoryName(LEPConfig.GlobalConfigPath),
                                                                         "LEPGUI.exe"),
                                                            e.Args);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show("LEPGUI requires administrator privilege to write to the current directory.",
                                        "Locale Emulator Plus",
                                        MessageBoxButton.OK,
                                        MessageBoxImage.Error);
                    }
                    finally
                    {
                        Current.Shutdown();
                    }
                }
            }

            I18n.LoadLanguage();

            Current.StartupUri = isGlobalProfile
                                     ? new Uri("GlobalConfig.xaml", UriKind.RelativeOrAbsolute)
                                     : new Uri("AppConfig.xaml", UriKind.RelativeOrAbsolute);
        }
    }
}