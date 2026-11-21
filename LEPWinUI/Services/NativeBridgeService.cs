using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using LEPWinUI.Models;

namespace LEPWinUI.Services;

public static class NativeBridgeService
{
    public static string GetLepExePath(string appDir) => Path.Combine(appDir, "LEP.exe");

    public static string BuildLaunchCommandPreview(string appDir, string targetPath, string? profileName, string? extraArgs)
    {
        var sb = new StringBuilder();
        sb.Append(SystemService.QuoteArg(GetLepExePath(appDir)));
        sb.Append(" run");
        if (!string.IsNullOrWhiteSpace(profileName))
        {
            sb.Append(" --profile ");
            sb.Append(SystemService.QuoteArg(profileName));
        }
        sb.Append(' ');
        sb.Append(SystemService.QuoteArg(targetPath));
        if (!string.IsNullOrWhiteSpace(extraArgs))
        {
            sb.Append(" -- ");
            sb.Append(extraArgs.Trim());
        }
        return sb.ToString();
    }

    public static (bool Ok, string Error) LaunchTarget(
        string appDir,
        string targetPath,
        string? profileName = null,
        string? extraArgs = null)
    {
        string lepExe = GetLepExePath(appDir);
        if (!File.Exists(lepExe))
        {
            return (false, $"未找到 {lepExe}");
        }

        var args = new StringBuilder("run");
        if (!string.IsNullOrWhiteSpace(profileName))
        {
            args.Append(" --profile ").Append(SystemService.QuoteArg(profileName));
        }
        args.Append(' ').Append(SystemService.QuoteArg(targetPath));
        if (!string.IsNullOrWhiteSpace(extraArgs))
        {
            args.Append(" -- ").Append(extraArgs.Trim());
        }

        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = lepExe,
                Arguments = args.ToString(),
                WorkingDirectory = appDir,
                UseShellExecute = false,
            };
            Process.Start(psi);
            return (true, "");
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }
    }

    public static async Task<(bool Ok, string OutputOrError)> RunLepCommandAsync(
        string appDir,
        string arguments,
        bool elevate = false)
    {
        string lepExe = GetLepExePath(appDir);
        if (!File.Exists(lepExe))
        {
            return (false, $"未找到 {lepExe}");
        }

        return await Task.Run(() =>
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = lepExe,
                    Arguments = arguments,
                    WorkingDirectory = appDir,
                    UseShellExecute = elevate,
                    Verb = elevate ? "runas" : "",
                    WindowStyle = ProcessWindowStyle.Hidden,
                    CreateNoWindow = !elevate,
                    RedirectStandardOutput = !elevate,
                    RedirectStandardError = !elevate,
                };
                if (!elevate)
                {
                    psi.StandardOutputEncoding = Encoding.UTF8;
                    psi.StandardErrorEncoding = Encoding.UTF8;
                }

                using Process? proc = Process.Start(psi);
                if (proc is null)
                {
                    return (false, "无法启动 LEP.exe 辅助进程。");
                }

                string stdout = elevate ? "" : proc.StandardOutput.ReadToEnd();
                string stderr = elevate ? "" : proc.StandardError.ReadToEnd();
                proc.WaitForExit();

                if (proc.ExitCode == 0)
                {
                    return (true, stdout.Trim());
                }
                string msg = !string.IsNullOrWhiteSpace(stderr) ? stderr.Trim()
                    : !string.IsNullOrWhiteSpace(stdout) ? stdout.Trim()
                    : $"LEP.exe 返回退出码 {proc.ExitCode}";
                return (false, msg);
            }
            catch (System.ComponentModel.Win32Exception ex) when (ex.NativeErrorCode == 1223)
            {
                return (false, "用户取消了管理员提权。");
            }
            catch (Exception ex)
            {
                return (false, ex.Message);
            }
        });
    }

    public static async Task<(bool Ok, string Error)> WriteAppProfileElevatedAsync(
        string appDir,
        string targetPath,
        LepProfile profile)
    {
        string sidePath = ConfigService.AlongsideAppConfigPath(targetPath);
        string tempPath = Path.Combine(Path.GetTempPath(), $"lep_profile_{Environment.ProcessId}_{Environment.TickCount64}.lep.toml");
        try
        {
            File.WriteAllText(tempPath, ConfigService.SerializeAppProfileToml(profile), new UTF8Encoding(false));
            string args = $"--write-profile-file {SystemService.QuoteArg(sidePath)} {SystemService.QuoteArg(tempPath)}";
            var res = await RunLepCommandAsync(appDir, args, elevate: !SystemService.IsAdministrator());
            return (res.Ok, res.Ok ? "" : res.OutputOrError);
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }
        finally
        {
            try { if (File.Exists(tempPath)) File.Delete(tempPath); } catch { }
        }
    }

    public static async Task<(bool Ok, string Error)> DeleteAppProfileWithElevationAsync(
        string appDir,
        string globalConfigPath,
        GlobalConfig globalConfig,
        string targetPath)
    {
        string sidePath = ConfigService.AlongsideAppConfigPath(targetPath);
        if (File.Exists(sidePath))
        {
            try
            {
                File.Delete(sidePath);
            }
            catch (UnauthorizedAccessException)
            {
                string args = $"--delete-profile-file {SystemService.QuoteArg(sidePath)}";
                var res = await RunLepCommandAsync(appDir, args, elevate: !SystemService.IsAdministrator());
                if (!res.Ok)
                {
                    return (false, res.OutputOrError);
                }
            }
            catch (Exception ex)
            {
                return (false, ex.Message);
            }
        }

        ConfigService.RemoveGlobalAppProfileEntry(globalConfigPath, globalConfig, targetPath);
        return (true, "");
    }

    public static Task<(bool Ok, string OutputOrError)> CreateShortcutAsync(
        string appDir,
        string targetPath,
        string outLnkPath,
        string? profileName,
        string? extraArgs)
    {
        var sb = new StringBuilder("shortcut ");
        sb.Append(SystemService.QuoteArg(targetPath));
        if (!string.IsNullOrWhiteSpace(profileName))
        {
            sb.Append(" --profile ").Append(SystemService.QuoteArg(profileName));
        }
        sb.Append(" --out ").Append(SystemService.QuoteArg(outLnkPath));
        if (!string.IsNullOrWhiteSpace(extraArgs))
        {
            sb.Append(" -- ").Append(extraArgs.Trim());
        }
        return RunLepCommandAsync(appDir, sb.ToString(), elevate: false);
    }
}
