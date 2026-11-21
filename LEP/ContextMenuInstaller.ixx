module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>

export module LEP.ContextMenuInstaller;

export import std.compat;
export import LEPCommon;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

struct ContextMenuStatus
{
    bool packagedStoreMode = false;
    bool isWin11 = false;
    bool classicCurrentUser = false;
    bool classicAllUsers = false;
    bool win11SparseInstalled = false;
    bool sparseFilesPresent = false;
};

ContextMenuStatus queryContextMenuStatus(const fs::path& appDir);

// 经典右键菜单（Win7-Win10 直接一级，Win11 显示在「显示更多选项」）
std::expected<void, std::wstring> installClassicContextMenu(const fs::path& appDir, bool allUsers, HWND owner = nullptr);
std::expected<void, std::wstring> uninstallClassicContextMenu(bool allUsers, HWND owner = nullptr);

// Windows 11 一级右键菜单（MSIX 稀疏包 + LocalMachine\TrustedPeople 证书）
std::expected<void, std::wstring> installWin11SparsePackage(const fs::path& appDir, HWND owner = nullptr);
std::expected<void, std::wstring> uninstallWin11SparsePackage(const fs::path& appDir, HWND owner = nullptr);

// 供内部提权子进程直接执行的证书导入/清理操作
std::expected<void, std::wstring> importCertificateToTrustedPeople(const fs::path& cerPath);
std::expected<void, std::wstring> removeCertificateFromTrustedPeople(const fs::path& cerPath);

// 提权写入/删除受保护目录下的 .lep.toml（无需重启或关闭编辑窗口）
std::expected<void, std::wstring> writeAppProfileFileElevated(
    const fs::path& destLepTomlPath,
    const fs::path& srcTempFilePath,
    HWND owner = nullptr);
std::expected<void, std::wstring> deleteAppProfileFileElevated(
    const fs::path& targetLepTomlPath,
    HWND owner = nullptr);
std::expected<void, std::wstring> executeElevatedWriteAppProfileFile(
    const fs::path& destLepTomlPath,
    const fs::path& srcTempFilePath);
std::expected<void, std::wstring> executeElevatedDeleteAppProfileFile(
    const fs::path& targetLepTomlPath);
std::expected<bool, std::wstring> deleteAppProfileWithElevation(
    const fs::path& globalConfigPath,
    GlobalConfig& globalConfig,
    const fs::path& targetPath,
    HWND owner = nullptr);

// 重启资源管理器以释放被锁定的 DLL
std::expected<void, std::wstring> restartExplorer();

NAMESPACE_END(lep)
