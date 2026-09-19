namespace LEPCommonLibrary
{
    public struct LEPProfile
    {
        public string Guid;
        public string Location;
        public string Name;
        public string Parameter;
        public int RegistryRedirectionMode;
        public int HookUILanguageMode;
        public bool RunAsAdmin;
        public bool RunWithSuspend;
        public bool ShowInMainMenu;
        public string Timezone;

        /// <summary>
        ///     Create a new LEPProfile object using default(ja-JP) settings.
        /// </summary>
        /// <param name="isDefault">A placeholder, not used at all.</param>
        public LEPProfile(bool isDefault)
            : this(
                "ja-JP",
                System.Guid.NewGuid().ToString(),
                false,
                string.Empty,
                "ja-JP",
                "Tokyo Standard Time",
                false,
                2,
                1,
                false)
        {
        }

        /// <summary>
        ///     Create a new LEPProfile using arguments.
        /// </summary>
        public LEPProfile(string name,
                         string guid,
                         bool showInMainMenu,
                         string parameter,
                         string location,
                         string timezone,
                         bool runAsAdmin,
                         int registryRedirectionMode,
                         int hookUILanguageMode,
                         bool runWithSuspend)
        {
            Name = name;
            Guid = guid;
            ShowInMainMenu = showInMainMenu;
            Parameter = parameter;
            Location = location;
            Timezone = timezone;
            RunAsAdmin = runAsAdmin;
            RegistryRedirectionMode = registryRedirectionMode;
            HookUILanguageMode = hookUILanguageMode;
            RunWithSuspend = runWithSuspend;
        }
    }
}
