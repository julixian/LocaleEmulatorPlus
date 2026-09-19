using System;
using System.Diagnostics;
using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Windows.Forms;
using Amemiya.Net;
using LEPCommonLibrary;

namespace LEPUpdater
{
    internal static class ApplicationUpdater
    {
        private const string LatestReleaseUrl =
            "https://api.github.com/repos/julixian/LocaleEmulatorPlus/releases/latest";

        [DataContract]
        private sealed class GitHubRelease
        {
            [DataMember(Name = "tag_name")]
            public string TagName { get; set; }

            [DataMember(Name = "html_url")]
            public string HtmlUrl { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }
        }

        internal static void CheckApplicationUpdate(string currentVersion, NotifyIcon notifyIcon)
        {
            try
            {
                GitHubRelease release;
                using (var client = new WebClientEx(10 * 1000))
                {
                    client.Headers[HttpRequestHeader.Accept] = "application/vnd.github+json";
                    client.Headers[HttpRequestHeader.UserAgent] = "LocaleEmulatorPlus-Updater";

                    using (var stream = client.DownloadDataStream(LatestReleaseUrl))
                    {
                        var serializer = new DataContractJsonSerializer(typeof(GitHubRelease));
                        release = (GitHubRelease)serializer.ReadObject(stream);
                    }
                }

                ProcessUpdate(release, currentVersion, notifyIcon);
            }
            catch (Exception)
            {
                notifyIcon.Visible = false;
                Environment.Exit(0);
            }
        }

        private static void ProcessUpdate(GitHubRelease release, string currentVersion, NotifyIcon notifyIcon)
        {
            Version latestVersion;
            Version installedVersion;
            if (release == null || !TryParseReleaseTag(release.TagName, out latestVersion) ||
                !Version.TryParse(currentVersion, out installedVersion))
            {
                throw new FormatException("Invalid release or installed version.");
            }

            if (latestVersion <= installedVersion)
            {
                GlobalHelper.SetLastUpdate(DateTimeOffset.UtcNow);
                notifyIcon.Visible = false;
                Environment.Exit(0);
            }

            Uri releasePage;
            if (!Uri.TryCreate(release.HtmlUrl, UriKind.Absolute, out releasePage) ||
                releasePage.Scheme != Uri.UriSchemeHttps ||
                !string.Equals(releasePage.Host, "github.com", StringComparison.OrdinalIgnoreCase) ||
                !releasePage.AbsolutePath.StartsWith("/julixian/LocaleEmulatorPlus/releases/",
                                                     StringComparison.OrdinalIgnoreCase))
            {
                throw new FormatException("Invalid release page URL.");
            }

            // Only a successfully read and parsed release counts as a completed check.
            GlobalHelper.SetLastUpdate(DateTimeOffset.UtcNow);

            notifyIcon.BalloonTipClicked += (sender, e) =>
                                            {
                                                Process.Start(releasePage.AbsoluteUri);
                                                notifyIcon.Visible = false;
                                                Environment.Exit(0);
                                            };
            notifyIcon.BalloonTipClosed += (sender, e) =>
                                           {
                                               notifyIcon.Visible = false;
                                               Environment.Exit(0);
                                           };

            var name = string.IsNullOrWhiteSpace(release.Name) ? release.TagName : release.Name.Trim();
            notifyIcon.ShowBalloonTip(0,
                                      $"New Version {latestVersion} Available (Current: {currentVersion})",
                                      $"{name}\r\n\r\nClick here to open the release page.",
                                      ToolTipIcon.Info);
            notifyIcon.Text = $"New Version {latestVersion} Available.";
        }

        private static bool TryParseReleaseTag(string tagName, out Version version)
        {
            version = null;
            if (string.IsNullOrEmpty(tagName) || tagName[0] != 'v')
                return false;

            var number = tagName.Substring(1);
            return Version.TryParse(number, out version) && version.Revision >= 0 &&
                   string.Equals(number, version.ToString(4), StringComparison.Ordinal);
        }
    }
}
