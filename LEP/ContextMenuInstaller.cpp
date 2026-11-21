module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <wincrypt.h>
#include <TlHelp32.h>

module LEP.ContextMenuInstaller;

import LEPCommon;

NAMESPACE_BEGIN(lep)

namespace
{
    constexpr std::wstring_view kOldDotNetClsid = L"{517F67EF-8967-4E4F-9713-B60ED2E9288A}";
    constexpr std::wstring_view kClsidRegPath = L"Software\\Classes\\CLSID\\" LEP_SHELL_EXT_CLSID_WSTR;
    constexpr std::wstring_view kShellVerbRegPath = L"Software\\Classes\\*\\shell\\LocaleEmulatorPlus";
    constexpr std::wstring_view kApprovedRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

    bool regKeyExists(const HKEY root, const std::wstring_view subKey)
    {
        HKEY hKey = nullptr;
        const std::wstring keyStr(subKey);
        if (RegOpenKeyExW(root, keyStr.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

    std::expected<void, std::wstring> setRegStringValue(
        const HKEY root,
        const std::wstring_view subKey,
        const PCWSTR valueName,
        const std::wstring_view data)
    {
        HKEY hKey = nullptr;
        const std::wstring keyStr(subKey);
        const LONG rc = RegCreateKeyExW(
            root, keyStr.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
        if (rc != ERROR_SUCCESS) {
            return std::unexpected(std::format(L"写入注册表键 {} 失败，错误码 {}", keyStr, rc));
        }
        const std::wstring dataStr(data);
        const LONG setRc = RegSetValueExW(
            hKey, valueName, 0, REG_SZ,
            (const BYTE*)dataStr.c_str(),
            (DWORD)((dataStr.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        if (setRc != ERROR_SUCCESS) {
            return std::unexpected(std::format(L"设置注册表值失败，错误码 {}", setRc));
        }
        return {};
    }

    void deleteRegTreeIfExists(const HKEY root, const std::wstring_view subKey)
    {
        const std::wstring keyStr(subKey);
        RegDeleteTreeW(root, keyStr.c_str());
    }

    void removeLegacyDotNetRegistration(const HKEY root)
    {
        const std::wstring oldHandlerPath = std::format(L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\{}", kOldDotNetClsid);
        RegDeleteTreeW(root, oldHandlerPath.c_str());
        HKEY hApproved = nullptr;
        if (RegOpenKeyExW(root, std::wstring(kApprovedRegPath).c_str(), 0, KEY_SET_VALUE, &hApproved) == ERROR_SUCCESS) {
            RegDeleteValueW(hApproved, std::wstring(kOldDotNetClsid).c_str());
            RegCloseKey(hApproved);
        }
    }

    void notifyShellChanged()
    {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    std::expected<void, std::wstring> runSelfElevated(const std::wstring& args, const HWND owner)
    {
        const fs::path selfExe = getModuleFilePath();
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = owner;
        sei.lpVerb = L"runas";
        sei.lpFile = selfExe.c_str();
        sei.lpParameters = args.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            const DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                return std::unexpected(L"用户取消了管理员提权。");
            }
            return std::unexpected(std::format(L"提权执行失败，Win32 错误 {}", err));
        }

        DWORD exitCode = 0;
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            GetExitCodeProcess(sei.hProcess, &exitCode);
            CloseHandle(sei.hProcess);
        }
        if (exitCode != 0) {
            return std::unexpected(std::format(L"管理员子进程返回失败（退出码 {}）。", exitCode));
        }
        return {};
    }

    std::optional<std::vector<BYTE>> readFileBytes(const fs::path& path)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) {
            return std::nullopt;
        }
        return std::vector<BYTE>(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    bool isCertificatePresentInStore(const std::vector<BYTE>& certBytes, const PCWSTR storeName)
    {
        PCCERT_CONTEXT pCert = CertCreateCertificateContext(
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, certBytes.data(), (DWORD)certBytes.size());
        if (!pCert) {
            return false;
        }

        BYTE thumbprint[64]{};
        DWORD thumbSize = sizeof(thumbprint);
        const BOOL hasHash = CertGetCertificateContextProperty(pCert, CERT_SHA1_HASH_PROP_ID, thumbprint, &thumbSize);
        CertFreeCertificateContext(pCert);
        if (!hasHash) {
            return false;
        }

        const HCERTSTORE hStore = CertOpenStore(
            CERT_STORE_PROV_SYSTEM_W, 0, 0,
            CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG, storeName);
        if (!hStore) {
            return false;
        }

        CRYPT_HASH_BLOB blob{ .cbData = thumbSize, .pbData = thumbprint };
        PCCERT_CONTEXT pFound = CertFindCertificateInStore(
            hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH, &blob, nullptr);
        const bool found = (pFound != nullptr);
        if (pFound) {
            CertFreeCertificateContext(pFound);
        }
        CertCloseStore(hStore, 0);
        return found;
    }

    std::wstring escapePowerShellSingleQuoted(const std::wstring_view text)
    {
        return replaceAllW(text, L"'", L"''");
    }

    std::expected<void, std::wstring> runPowerShellHidden(const std::wstring& script)
    {
        std::wstring cmdLine = std::format(
            L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -Command \"{}\"",
            replaceAllW(script, L"\"", L"\\\""));

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hRead = nullptr;
        HANDLE hWrite = nullptr;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        if (hRead) {
            SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        // 若由 PowerShell 7 (pwsh) 派生，清除继承的 PSModulePath 以免干扰 Windows PowerShell 5.1 加载 Appx 模块
        std::wstring savedPsModulePath;
        bool hadPsModulePath = false;
        if (const DWORD envLen = GetEnvironmentVariableW(L"PSModulePath", nullptr, 0); envLen > 0) {
            savedPsModulePath.resize(envLen);
            const DWORD copied = GetEnvironmentVariableW(L"PSModulePath", savedPsModulePath.data(), envLen);
            if (copied > 0) {
                savedPsModulePath.resize(copied);
                hadPsModulePath = true;
                SetEnvironmentVariableW(L"PSModulePath", nullptr);
            }
        }

        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(
            nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        if (hadPsModulePath) {
            SetEnvironmentVariableW(L"PSModulePath", savedPsModulePath.c_str());
        }

        if (hWrite) {
            CloseHandle(hWrite);
        }

        if (!ok) {
            const DWORD err = GetLastError();
            if (hRead) CloseHandle(hRead);
            return std::unexpected(std::format(L"启动 PowerShell 失败，Win32 错误 {}", err));
        }

        std::string output;
        if (hRead) {
            char buf[512]{};
            DWORD readBytes = 0;
            while (ReadFile(hRead, buf, sizeof(buf), &readBytes, nullptr) && readBytes > 0) {
                output.append(buf, readBytes);
            }
            CloseHandle(hRead);
        }

        WaitForSingleObject(pi.hProcess, 60000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (exitCode != 0) {
            std::wstring msg = utf82Wide(output);
            if (msg.empty()) {
                msg = std::format(L"PowerShell 命令执行失败（退出码 {}）。", exitCode);
            }
            return std::unexpected(trimW(msg));
        }
        return {};
    }

    bool checkSparsePackageRegistry()
    {
        HKEY hPackages = nullptr;
        if (RegOpenKeyExW(
                HKEY_CURRENT_USER,
                L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages",
                0, KEY_READ, &hPackages) != ERROR_SUCCESS)
        {
            return false;
        }
        const std::wstring prefix = std::wstring(LEP_SPARSE_PACKAGE_NAME_WSTR) + L"_";
        wchar_t nameBuf[256]{};
        bool found = false;
        for (DWORD idx = 0;; ++idx) {
            DWORD len = (DWORD)std::size(nameBuf);
            if (RegEnumKeyExW(hPackages, idx, nameBuf, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
                break;
            }
            if (toLowerW(nameBuf).starts_with(toLowerW(prefix))) {
                found = true;
                break;
            }
        }
        RegCloseKey(hPackages);
        return found;
    }
}

ContextMenuStatus queryContextMenuStatus(const fs::path& appDir)
{
    std::error_code ec;
    ContextMenuStatus status{};
    status.packagedStoreMode = isPackagedApp();
    status.isWin11 = isWindows11OrGreater();
    status.classicCurrentUser = regKeyExists(HKEY_CURRENT_USER, kShellVerbRegPath);
    status.classicAllUsers = regKeyExists(HKEY_LOCAL_MACHINE, kShellVerbRegPath);
    status.win11SparseInstalled = status.packagedStoreMode || (status.isWin11 && checkSparsePackageRegistry());
    status.sparseFilesPresent =
        fs::is_regular_file(appDir / L"LEPShellExt.msix", ec) &&
        fs::is_regular_file(appDir / L"LEPShellExt.cer", ec);
    return status;
}

std::expected<void, std::wstring> installClassicContextMenu(const fs::path& appDir, const bool allUsers, const HWND owner)
{
    if (allUsers && !isAdministrator()) {
        return runSelfElevated(L"install --all-users", owner);
    }

    const fs::path dllPath = appDir / L"LEPShellExt.dll";
    const fs::path exePath = appDir / L"LEP.exe";
    std::error_code ec;
    if (!fs::is_regular_file(dllPath, ec)) {
        return std::unexpected(std::format(L"未找到 {}。", dllPath.wstring()));
    }

    const HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    // 清理旧版 2.x .NET 右键残留，防止经典菜单出现重复入口
    removeLegacyDotNetRegistration(root);

    if (auto res = setRegStringValue(root, kClsidRegPath, nullptr, L"Locale Emulator Plus Context Menu"); !res) {
        return res;
    }
    const std::wstring inprocPath = std::wstring(kClsidRegPath) + L"\\InprocServer32";
    if (auto res = setRegStringValue(root, inprocPath, nullptr, dllPath.wstring()); !res) {
        return res;
    }
    if (auto res = setRegStringValue(root, inprocPath, L"ThreadingModel", L"Apartment"); !res) {
        return res;
    }
    if (auto res = setRegStringValue(root, kApprovedRegPath, LEP_SHELL_EXT_CLSID_WSTR, L"Locale Emulator Plus Context Menu"); !res) {
        return res;
    }

    // SubCommands 为空字符串是 Win7/Win10 经典菜单渲染 ExplorerCommandHandler 级联子菜单的必要键
    if (auto res = setRegStringValue(root, kShellVerbRegPath, L"MUIVerb", L"Locale Emulator Plus"); !res) {
        return res;
    }
    if (auto res = setRegStringValue(root, kShellVerbRegPath, L"Icon", exePath.wstring() + L",0"); !res) {
        return res;
    }
    if (auto res = setRegStringValue(root, kShellVerbRegPath, L"SubCommands", L""); !res) {
        return res;
    }
    if (auto res = setRegStringValue(root, kShellVerbRegPath, L"ExplorerCommandHandler", LEP_SHELL_EXT_CLSID_WSTR); !res) {
        return res;
    }

    notifyShellChanged();
    return {};
}

std::expected<void, std::wstring> uninstallClassicContextMenu(const bool allUsers, const HWND owner)
{
    if (allUsers && !isAdministrator()) {
        return runSelfElevated(L"uninstall --all-users", owner);
    }

    const HKEY root = allUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    deleteRegTreeIfExists(root, kShellVerbRegPath);
    deleteRegTreeIfExists(root, kClsidRegPath);
    removeLegacyDotNetRegistration(root);

    HKEY hApproved = nullptr;
    if (RegOpenKeyExW(root, std::wstring(kApprovedRegPath).c_str(), 0, KEY_SET_VALUE, &hApproved) == ERROR_SUCCESS) {
        RegDeleteValueW(hApproved, LEP_SHELL_EXT_CLSID_WSTR);
        RegCloseKey(hApproved);
    }

    notifyShellChanged();
    return {};
}

std::expected<void, std::wstring> importCertificateToTrustedPeople(const fs::path& cerPath)
{
    const auto certBytes = readFileBytes(cerPath);
    if (!certBytes || certBytes->empty()) {
        return std::unexpected(std::format(L"无法读取证书文件：{}", cerPath.wstring()));
    }

    const HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE, L"TrustedPeople");
    if (!hStore) {
        return std::unexpected(std::format(L"打开本机 TrustedPeople 证书存储失败，Win32 错误 {}", GetLastError()));
    }

    const BOOL ok = CertAddEncodedCertificateToStore(
        hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        certBytes->data(), (DWORD)certBytes->size(),
        CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
    const DWORD err = ok ? 0 : GetLastError();
    CertCloseStore(hStore, 0);

    if (!ok) {
        return std::unexpected(std::format(L"导入证书失败，Win32 错误 {}", err));
    }
    return {};
}

std::expected<void, std::wstring> removeCertificateFromTrustedPeople(const fs::path& cerPath)
{
    const auto certBytes = readFileBytes(cerPath);
    if (!certBytes || certBytes->empty()) {
        return {};
    }

    PCCERT_CONTEXT pCert = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, certBytes->data(), (DWORD)certBytes->size());
    if (!pCert) {
        return {};
    }

    BYTE thumbprint[64]{};
    DWORD thumbSize = sizeof(thumbprint);
    const BOOL hasHash = CertGetCertificateContextProperty(pCert, CERT_SHA1_HASH_PROP_ID, thumbprint, &thumbSize);
    CertFreeCertificateContext(pCert);
    if (!hasHash) {
        return {};
    }

    const HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE, L"TrustedPeople");
    if (!hStore) {
        return std::unexpected(std::format(L"打开本机 TrustedPeople 证书存储失败，Win32 错误 {}", GetLastError()));
    }

    CRYPT_HASH_BLOB blob{ .cbData = thumbSize, .pbData = thumbprint };
    while (PCCERT_CONTEXT pFound = CertFindCertificateInStore(
        hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH, &blob, nullptr))
    {
        CertDeleteCertificateFromStore(pFound);
    }
    CertCloseStore(hStore, 0);
    return {};
}

std::expected<void, std::wstring> installWin11SparsePackage(const fs::path& appDir, const HWND owner)
{
    if (!isWindows11OrGreater()) {
        return std::unexpected(L"当前系统低于 Windows 11，请直接使用经典右键菜单。");
    }
    if (isPackagedApp()) {
        return {};
    }

    const fs::path msixPath = appDir / L"LEPShellExt.msix";
    const fs::path cerPath = appDir / L"LEPShellExt.cer";
    std::error_code ec;
    if (!fs::is_regular_file(msixPath, ec) || !fs::is_regular_file(cerPath, ec)) {
        return std::unexpected(L"未找到 LEPShellExt.msix 或 LEPShellExt.cer，无法注册 Windows 11 一级右键菜单。");
    }

    const auto certBytes = readFileBytes(cerPath);
    const bool alreadyTrusted = certBytes &&
        (isCertificatePresentInStore(*certBytes, L"TrustedPeople") || isCertificatePresentInStore(*certBytes, L"Root"));

    if (!alreadyTrusted) {
        if (isAdministrator()) {
            if (auto res = importCertificateToTrustedPeople(cerPath); !res) {
                return res;
            }
        }
        else {
            if (auto res = runSelfElevated(std::format(L"--trust-cert {}", quoteCommandLineArg(cerPath.wstring())), owner); !res) {
                return res;
            }
        }
    }

    const std::wstring script = std::format(
        L"$ErrorActionPreference='Stop'; "
        L"Get-AppxPackage -Name '{}' -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue; "
        L"Add-AppxPackage -Path '{}' -ExternalLocation '{}' -ForceApplicationShutdown; "
        L"$pkg = Get-AppxPackage -Name '{}' -ErrorAction SilentlyContinue | Select-Object -First 1; "
        L"if ($pkg -and (Test-Path 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences\\extensions')) {{ "
        L"  $p = 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences\\extensions\\' + $pkg.PackageFamilyName; "
        L"  New-Item -Path $p -Force | Out-Null; "
        L"  New-ItemProperty -Path $p -Name 'Enabled' -PropertyType DWord -Value 1 -Force | Out-Null; "
        L"}}",
        escapePowerShellSingleQuoted(LEP_SPARSE_PACKAGE_NAME_WSTR),
        escapePowerShellSingleQuoted(msixPath.wstring()),
        escapePowerShellSingleQuoted(appDir.wstring()),
        escapePowerShellSingleQuoted(LEP_SPARSE_PACKAGE_NAME_WSTR));

    if (auto res = runPowerShellHidden(script); !res) {
        return res;
    }

    notifyShellChanged();
    return {};
}

std::expected<void, std::wstring> uninstallWin11SparsePackage(const fs::path& appDir, const HWND owner)
{
    if (isPackagedApp()) {
        return {};
    }

    const std::wstring script = std::format(
        L"$ErrorActionPreference='Stop'; "
        L"$pkgs = Get-AppxPackage -Name '{}' -ErrorAction SilentlyContinue; "
        L"foreach ($pkg in $pkgs) {{ "
        L"  $p = 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ContextMenu\\Preferences\\extensions\\' + $pkg.PackageFamilyName; "
        L"  if (Test-Path $p) {{ Remove-Item -Path $p -Recurse -Force -ErrorAction SilentlyContinue; }} "
        L"}}; "
        L"$pkgs | Remove-AppxPackage",
        escapePowerShellSingleQuoted(LEP_SPARSE_PACKAGE_NAME_WSTR));

    if (auto res = runPowerShellHidden(script); !res) {
        return res;
    }

    const fs::path cerPath = appDir / L"LEPShellExt.cer";
    if (const auto certBytes = readFileBytes(cerPath);
        certBytes && isCertificatePresentInStore(*certBytes, L"TrustedPeople"))
    {
        if (isAdministrator()) {
            (void)removeCertificateFromTrustedPeople(cerPath);
        }
        else {
            (void)runSelfElevated(std::format(L"--untrust-cert {}", quoteCommandLineArg(cerPath.wstring())), owner);
        }
    }

    notifyShellChanged();
    return {};
}

std::expected<void, std::wstring> executeElevatedWriteAppProfileFile(
    const fs::path& destLepTomlPath,
    const fs::path& srcTempFilePath)
{
    if (!toLowerW(destLepTomlPath.wstring()).ends_with(L".lep.toml")) {
        return std::unexpected(L"仅允许提权写入 .lep.toml 配置文件。");
    }
    if (!CopyFileW(srcTempFilePath.c_str(), destLepTomlPath.c_str(), FALSE)) {
        return std::unexpected(std::format(L"写入配置文件失败，Win32 错误 {}", GetLastError()));
    }
    return {};
}

std::expected<void, std::wstring> executeElevatedDeleteAppProfileFile(const fs::path& targetLepTomlPath)
{
    if (!toLowerW(targetLepTomlPath.wstring()).ends_with(L".lep.toml")) {
        return std::unexpected(L"仅允许提权删除 .lep.toml 配置文件。");
    }
    if (!DeleteFileW(targetLepTomlPath.c_str())) {
        const DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return {};
        }
        return std::unexpected(std::format(L"删除配置文件失败，Win32 错误 {}", err));
    }
    return {};
}

std::expected<void, std::wstring> writeAppProfileFileElevated(
    const fs::path& destLepTomlPath,
    const fs::path& srcTempFilePath,
    const HWND owner)
{
    if (isAdministrator()) {
        return executeElevatedWriteAppProfileFile(destLepTomlPath, srcTempFilePath);
    }
    const std::wstring args = std::format(
        L"--write-profile-file {} {}",
        quoteCommandLineArg(destLepTomlPath.wstring()),
        quoteCommandLineArg(srcTempFilePath.wstring()));
    return runSelfElevated(args, owner);
}

std::expected<void, std::wstring> deleteAppProfileFileElevated(
    const fs::path& targetLepTomlPath,
    const HWND owner)
{
    if (isAdministrator()) {
        return executeElevatedDeleteAppProfileFile(targetLepTomlPath);
    }
    const std::wstring args = std::format(
        L"--delete-profile-file {}",
        quoteCommandLineArg(targetLepTomlPath.wstring()));
    return runSelfElevated(args, owner);
}

std::expected<bool, std::wstring> deleteAppProfileWithElevation(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath,
    const HWND owner)
{
    bool removedAny = false;
    const fs::path sidePath = alongsideAppConfigPath(targetPath);
    std::error_code ec;
    if (fs::is_regular_file(sidePath, ec)) {
        if (!DeleteFileW(sidePath.c_str())) {
            const DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED && !isAdministrator()) {
                if (auto elevRes = deleteAppProfileFileElevated(sidePath, owner); !elevRes) {
                    return std::unexpected(elevRes.error());
                }
            }
            else if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
                return std::unexpected(std::format(L"删除 {} 失败，Win32 错误 {}", sidePath.filename().wstring(), err));
            }
        }
        removedAny = true;
    }

    auto globalRes = deleteAppProfile(globalConfigPath, globalConfig, targetPath);
    if (!globalRes) {
        return std::unexpected(globalRes.error());
    }
    return removedAny || *globalRes;
}

std::expected<void, std::wstring> restartExplorer()
{
    const HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return std::unexpected(L"无法枚举系统进程。");
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::vector<DWORD> explorerPids;
    if (Process32FirstW(hSnap, &entry)) {
        do {
            if (equalsIgnoreCaseW(entry.szExeFile, L"explorer.exe")) {
                explorerPids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(hSnap, &entry));
    }
    CloseHandle(hSnap);

    for (const DWORD pid : explorerPids) {
        if (const HANDLE hProc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid)) {
            TerminateProcess(hProc, 1);
            WaitForSingleObject(hProc, 3000);
            CloseHandle(hProc);
        }
    }

    Sleep(300);

    // 若 Windows 未自动拉起 explorer.exe，则主动启动一次
    if (!FindWindowW(L"Shell_TrayWnd", nullptr)) {
        wchar_t winDir[MAX_PATH]{};
        GetWindowsDirectoryW(winDir, MAX_PATH);
        const fs::path explorerExe = fs::path(winDir) / L"explorer.exe";
        ShellExecuteW(nullptr, L"open", explorerExe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    return {};
}

NAMESPACE_END(lep)
