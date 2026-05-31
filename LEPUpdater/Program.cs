using System;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
using LEPCommonLibrary;
using LEPUpdater.Properties;

namespace LEPUpdater
{
    internal static class Program
    {
        private static readonly NotifyIcon _notifyIcon = new NotifyIcon {Icon = Resources.icon};
        private static bool auto;

        private static void Main(string[] args)
        {
            auto = args.Length != 0;

            // Check new version every week.
            if (auto && int.Parse(DateTime.Now.ToString("yyyyMMdd")) - GlobalHelper.GetLastUpdate() < 7)
            {
                Environment.Exit(0);
            }

            if (!SystemHelper.CheckPermission(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location)))
            {
                if (!SystemHelper.IsAdministrator())
                {
                    SystemHelper.RunWithElevatedProcess(
                                                        Path.Combine(Assembly.GetExecutingAssembly().Location),
                                                        args);

                    Environment.Exit(0);
                }
                else
                {
                    _notifyIcon.Visible = false;
                    Environment.Exit(0);
                }
            }

            CheckUpdates(GlobalHelper.GetLEPVersion());
        }

        private static void CheckUpdates(string appVer)
        {
            _notifyIcon.Visible = true;

            _notifyIcon.DoubleClick += (sender, e) =>
                                       {
                                           _notifyIcon.Visible = false;
                                           Environment.Exit(0);
                                       };

            ApplicationUpdater.CheckApplicationUpdate(appVer, _notifyIcon);

            Application.Run();
        }
    }
}