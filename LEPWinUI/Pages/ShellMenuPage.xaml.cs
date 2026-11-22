using System;
using System.Threading.Tasks;
using LEPWinUI.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace LEPWinUI.Pages;

public sealed partial class ShellMenuPage : Page
{
    private readonly AppState _state = App.State;

    public ShellMenuPage()
    {
        InitializeComponent();
        Loaded += (_, _) =>
        {
            ApplyLocalization();
            RefreshStatus();
        };
    }

    private void ApplyLocalization()
    {
        var i18n = _state.I18n;
        PageTitleText.Text = i18n.Tr("tabs", "shell_menu", "右键菜单");
        Win11HeaderText.Text = i18n.Tr("shell_menu", "win11_group");
        Win11HintText.Text = i18n.Tr("shell_menu", "win11_hint");
        Win11InstallBtn.Content = i18n.Tr("shell_menu", "install_win11");
        Win11UninstallBtn.Content = i18n.Tr("shell_menu", "uninstall_win11");

        ClassicHeaderText.Text = i18n.Tr("shell_menu", "classic_group");
        UserInstallBtn.Content = i18n.Tr("shell_menu", "install_user");
        UserUninstallBtn.Content = i18n.Tr("shell_menu", "uninstall_user");
        AllInstallBtn.Content = i18n.Tr("shell_menu", "install_all");
        AllUninstallBtn.Content = i18n.Tr("shell_menu", "uninstall_all");

        ToolsHeaderText.Text = i18n.Tr("shell_menu", "tools_group");
        RestartExplorerBtn.Content = i18n.Tr("shell_menu", "restart_explorer");
        RestartExplorerHintText.Text = i18n.Tr("shell_menu", "restart_explorer_hint");
    }

    private void RefreshStatus()
    {
        var i18n = _state.I18n;
        bool storeMode = SystemService.IsPackagedApp();
        bool isWin11 = SystemService.IsWindows11OrGreater();
        bool userInstalled = SystemService.IsClassicContextMenuInstalled(allUsers: false);
        bool allInstalled = SystemService.IsClassicContextMenuInstalled(allUsers: true);
        bool win11Installed = SystemService.IsWin11SparsePackageInstalled();

        ToolsCard.Visibility = storeMode ? Visibility.Collapsed : Visibility.Visible;

        UserStatusText.Text = userInstalled
            ? i18n.Tr("shell_menu", "status_installed")
            : i18n.Tr("shell_menu", "status_not_installed");
        UserUninstallBtn.IsEnabled = userInstalled;

        AllStatusText.Text = allInstalled
            ? i18n.Tr("shell_menu", "status_installed")
            : i18n.Tr("shell_menu", "status_not_installed");
        AllUninstallBtn.IsEnabled = allInstalled;

        if (storeMode)
        {
            Win11StatusText.Text = i18n.Tr("shell_menu", "status_store_managed");
            Win11ButtonsPanel.Visibility = Visibility.Collapsed;
        }
        else if (!isWin11)
        {
            Win11ButtonsPanel.Visibility = Visibility.Visible;
            Win11StatusText.Text = i18n.Tr("shell_menu", "status_win11_na");
            Win11InstallBtn.IsEnabled = false;
            Win11UninstallBtn.IsEnabled = false;
        }
        else
        {
            Win11ButtonsPanel.Visibility = Visibility.Visible;
            Win11StatusText.Text = win11Installed
                ? i18n.Tr("shell_menu", "status_installed")
                : i18n.Tr("shell_menu", "status_not_installed");
            Win11InstallBtn.IsEnabled = true;
            Win11UninstallBtn.IsEnabled = win11Installed;
        }
    }

    private async Task ExecuteShellActionAsync(string args)
    {
        var i18n = _state.I18n;
        var res = await NativeBridgeService.RunLepCommandAsync(_state.AppDir, args);
        FeedbackBar.Severity = res.Ok ? InfoBarSeverity.Success : InfoBarSeverity.Error;
        FeedbackBar.Message = res.Ok
            ? i18n.Tr("shell_menu", "op_ok")
            : i18n.Tr("shell_menu", "op_failed") + res.OutputOrError;
        FeedbackBar.IsOpen = true;
        RefreshStatus();
    }

    private async void Win11InstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("install --win11");
    private async void Win11UninstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("uninstall --win11");
    private async void UserInstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("install");
    private async void UserUninstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("uninstall");
    private async void AllInstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("install --all-users");
    private async void AllUninstallBtn_Click(object sender, RoutedEventArgs e) => await ExecuteShellActionAsync("uninstall --all-users");

    private async void RestartExplorerBtn_Click(object sender, RoutedEventArgs e)
    {
        var i18n = _state.I18n;
        RestartExplorerBtn.IsEnabled = false;
        try
        {
            await Task.Run(SystemService.RestartExplorer);
            FeedbackBar.Severity = InfoBarSeverity.Success;
            FeedbackBar.Message = i18n.Tr("shell_menu", "op_ok");
            FeedbackBar.IsOpen = true;
        }
        catch (Exception ex)
        {
            FeedbackBar.Severity = InfoBarSeverity.Error;
            FeedbackBar.Message = i18n.Tr("shell_menu", "op_failed") + ex.Message;
            FeedbackBar.IsOpen = true;
        }
        finally
        {
            RestartExplorerBtn.IsEnabled = true;
        }
    }
}
