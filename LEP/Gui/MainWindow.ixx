module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

export module LEP.Gui.MainWindow;

export import std.compat;
export import LEPCommon;

export NAMESPACE_BEGIN(lep::gui)

class MainWindow
{
public:
    MainWindow(fs::path appDir, fs::path initialTargetPath = {});
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    int run();

private:
    enum ControlId
    {
        kIdTabControl = 3001,
        kIdStatusBar,

        // 页 1：启动与快捷方式
        kIdLaunchTargetEdit = 3101,
        kIdLaunchBrowseBtn,
        kIdLaunchProfileCombo,
        kIdLaunchArgsEdit,
        kIdLaunchCmdEdit,
        kIdLaunchCopyCmdBtn,
        kIdLaunchRunBtn,
        kIdLaunchShortcutBtn,
        kIdLaunchAppProfileBtn,
        kIdLaunchDeleteAppBtn,
        kIdLaunchAppStatusLabel,

        // 页 2：配置管理
        kIdProfList = 3201,
        kIdProfAddBtn,
        kIdProfDupBtn,
        kIdProfDelBtn,
        kIdProfUpBtn,
        kIdProfDownBtn,
        kIdProfNameEdit,
        kIdProfLocaleCombo,
        kIdProfTzCombo,
        kIdProfRegCombo,
        kIdProfUiCombo,
        kIdProfArgsEdit,
        kIdProfStdEdit,
        kIdProfDstEdit,
        kIdProfAdminCheck,
        kIdProfSuspendCheck,
        kIdProfMenuCheck,
        kIdProfSaveBtn,
        kIdGlobalAppList,
        kIdGlobalAppEditBtn,
        kIdGlobalAppDelBtn,

        // 页 3：右键菜单
        kIdShellClassicUserStatus = 3301,
        kIdShellClassicUserInstallBtn,
        kIdShellClassicUserUninstallBtn,
        kIdShellClassicAllStatus,
        kIdShellClassicAllInstallBtn,
        kIdShellClassicAllUninstallBtn,
        kIdShellWin11Status,
        kIdShellWin11InstallBtn,
        kIdShellWin11UninstallBtn,
        kIdShellRestartExplorerBtn,

        // 页 4：设置与关于
        kIdSettingsLangCombo = 3401,
        kIdSettingsOpenConfigDirBtn,
        kIdSettingsOpenLogDirBtn,
        kIdSettingsOpenAppDirBtn,
        kIdSettingsCheckUpdateBtn,
    };

    static constexpr UINT kMsgUpdateCheckDone = WM_APP + 101;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void initUi();
    void rebuildUi();
    void switchTab(int index);
    void setStatus(std::wstring_view message);
    int s(int px96) const;

    // 四个标签页（各占独立 .cpp 实现文件，职责清晰）
    void buildLaunchTab(HWND page);
    void refreshLaunchTab();
    void updateLaunchCommandPreview();
    bool handleLaunchCommand(WORD id, WORD notifyCode);

    void buildProfilesTab(HWND page);
    void refreshProfilesTab();
    void loadSelectedProfileToForm();
    bool handleProfilesCommand(WORD id, WORD notifyCode);

    void buildShellTab(HWND page);
    void refreshShellTab();
    bool handleShellCommand(WORD id, WORD notifyCode);

    void buildSettingsTab(HWND page);
    void refreshSettingsTab();
    bool handleSettingsCommand(WORD id, WORD notifyCode);
    void handleUpdateCheckDone(LPARAM lParam);

    void setLaunchTarget(const fs::path& path);

    fs::path m_appDir;
    fs::path m_configPath;
    fs::path m_initialTarget;
    GlobalConfig m_config;
    I18n m_i18n;
    std::vector<LocaleEntry> m_locales;
    std::vector<TimezoneEntry> m_timezones;
    std::vector<LanguageOption> m_languages;

    HWND m_hwnd = nullptr;
    HWND m_tabCtrl = nullptr;
    HWND m_statusBar = nullptr;
    std::array<HWND, 4> m_pages{};
    int m_activeTab = 0;
    UINT m_dpi = 96;
    HFONT m_font = nullptr;
    HFONT m_boldFont = nullptr;

    struct LaunchProfileChoice
    {
        bool isAppProfile = false;
        std::wstring name;
        Profile profile;
    };

    struct GlobalAppGroup
    {
        std::wstring targetPath;
        size_t count = 0;
        std::wstring firstLocale;
    };

    // 页 1 控件句柄
    HWND m_launchTargetEdit = nullptr;
    HWND m_launchProfileCombo = nullptr;
    HWND m_launchArgsEdit = nullptr;
    HWND m_launchCmdEdit = nullptr;
    HWND m_launchAppProfileBtn = nullptr;
    HWND m_launchDeleteAppBtn = nullptr;
    HWND m_launchAppStatusLabel = nullptr;
    std::vector<LaunchProfileChoice> m_launchChoices;
    size_t m_launchAppProfileCount = 0;

    // 页 2 控件句柄
    HWND m_profList = nullptr;
    HWND m_profNameEdit = nullptr;
    HWND m_profLocaleCombo = nullptr;
    HWND m_profTzCombo = nullptr;
    HWND m_profRegCombo = nullptr;
    HWND m_profUiCombo = nullptr;
    HWND m_profArgsEdit = nullptr;
    HWND m_profStdEdit = nullptr;
    HWND m_profDstEdit = nullptr;
    HWND m_profAdminCheck = nullptr;
    HWND m_profSuspendCheck = nullptr;
    HWND m_profMenuCheck = nullptr;
    HWND m_globalAppList = nullptr;
    HWND m_globalAppEditBtn = nullptr;
    HWND m_globalAppDelBtn = nullptr;
    int m_selectedProfileIndex = -1;
    std::vector<GlobalAppGroup> m_globalAppGroups;

    // 页 3 控件句柄
    HWND m_shellUserStatus = nullptr;
    HWND m_shellUserInstallBtn = nullptr;
    HWND m_shellUserUninstallBtn = nullptr;
    HWND m_shellAllStatus = nullptr;
    HWND m_shellAllInstallBtn = nullptr;
    HWND m_shellAllUninstallBtn = nullptr;
    HWND m_shellWin11Status = nullptr;
    HWND m_shellWin11InstallBtn = nullptr;
    HWND m_shellWin11UninstallBtn = nullptr;

    // 页 4 控件句柄
    HWND m_settingsLangCombo = nullptr;
    HWND m_settingsCheckUpdateBtn = nullptr;
    bool m_updateCheckInFlight = false;
};

NAMESPACE_END(lep::gui)
