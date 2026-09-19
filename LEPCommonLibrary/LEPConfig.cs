using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml.Linq;

namespace LEPCommonLibrary
{
    public static class LEPConfig
    {
        public static string GlobalConfigPath =
            Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location),
                         "LEPConfig.xml");

        public static LEPProfile GetProfile(string name)
        {
            try
            {
                return GetProfiles(GlobalConfigPath).Where(p => p.Name == name).ToArray()[0];
            }
            catch
            {
                return new LEPProfile();
            }
        }

        public static LEPProfile[] GetProfiles()
        {
            return GetProfiles(GlobalConfigPath);
        }

        public static LEPProfile[] GetProfiles(string configPath)
        {
            try
            {
                var dict = XDocument.Load(configPath);

                var pros = from i in dict.Descendants("LEPConfig").Elements("Profiles").Elements()
                           select i;

                var profiles =
                    pros.Select(p => new LEPProfile(p.Attribute("Name").Value,
                                                   p.Attribute("Guid").Value,
                                                   bool.Parse(p.Attribute("MainMenu").Value),
                                                   p.Element("Parameter").Value,
                                                   p.Element("Location").Value,
                                                   p.Element("Timezone").Value,
                                                   bool.Parse(p.TryGetValue("RunAsAdmin", "false")),
                                                   ParseMode(p.Element("RegistryRedirectionMode").Value, 2),
                                                   ParseMode(p.TryGetValue("HookUILanguageMode", "1"), 1),
                                                   bool.Parse(p.TryGetValue("RunWithSuspend", "false"))
                                         )
                        ).ToArray();

                return profiles;
            }
            catch
            {
                return new LEPProfile[0];
            }
        }

        public static bool CheckGlobalConfigFile(bool buildNewConfig)
        {
            if (buildNewConfig && !File.Exists(GlobalConfigPath))
                BuildGlobalConfigFile();

            return File.Exists(GlobalConfigPath);
        }

        public static void SaveGlobalConfigFile(params LEPProfile[] profiles)
        {
            WriteConfig(GlobalConfigPath, profiles);
        }

        public static void SaveApplicationConfigFile(string path, LEPProfile profile)
        {
            WriteConfig(path, profile);
        }

        private static void BuildGlobalConfigFile()
        {
            var defaultProfiles = new[]
                                  {
                                      new LEPProfile("Run in Japanese",
                                                    Guid.NewGuid().ToString(),
                                                    false,
                                                    string.Empty,
                                                    "ja-JP",
                                                    "Tokyo Standard Time",
                                                    false,
                                                    2,
                                                    1,
                                                    false
                                          ),
                                      new LEPProfile("Run in Japanese (Admin)",
                                                    Guid.NewGuid().ToString(),
                                                    false,
                                                    string.Empty,
                                                    "ja-JP",
                                                    "Tokyo Standard Time",
                                                    true,
                                                    2,
                                                    1,
                                                    false
                                          )
                                  };

            WriteConfig(GlobalConfigPath, defaultProfiles);
        }

        private static string TryGetValue(this XElement element, string name, string defaultValue = null)
        {
            var found = element.Element(name);
            if (found != null)
            {
                return found.Value;
            }
            return defaultValue;
        }

        private static int ParseMode(string value, int defaultValue)
        {
            int mode;
            return int.TryParse(value, out mode) && mode >= 0 && mode <= 2 ? mode : defaultValue;
        }

        private static void WriteConfig(string writeTo, params LEPProfile[] profiles)
        {
            var baseNode = new XElement("Profiles");

            foreach (var pro in profiles)
            {
                baseNode.Add(new XElement("Profile",
                                          new XAttribute("Name", pro.Name),
                                          new XAttribute("Guid", pro.Guid),
                                          new XAttribute("MainMenu", pro.ShowInMainMenu),
                                          new XElement("Parameter", pro.Parameter),
                                          new XElement("Location", pro.Location),
                                          new XElement("Timezone", pro.Timezone),
                                          new XElement("RunAsAdmin", pro.RunAsAdmin),
                                          new XElement("RegistryRedirectionMode", pro.RegistryRedirectionMode),
                                          new XElement("HookUILanguageMode", pro.HookUILanguageMode),
                                          new XElement("RunWithSuspend", pro.RunWithSuspend)
                                 )
                    );
            }

            var tree = new XElement("LEPConfig", baseNode);

            try
            {
                tree.Save(writeTo);
            }
            catch
            {
            }
        }
    }
}
