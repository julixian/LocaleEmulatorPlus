module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <Shlwapi.h>
#include <shellapi.h>

module LEPShellExt.ExplorerCommand;

import LEPCommon;

NAMESPACE_BEGIN(lep::shellext)

namespace
{
    HMODULE g_hModule = nullptr;
    std::atomic<long> g_objectCount{ 0 };

    // {A84F3C21-6E9D-4B2A-9F1E-3D7C5B8A2030}
    constexpr GUID kRootCommandGuid =
        { 0xA84F3C21, 0x6E9D, 0x4B2A, { 0x9F, 0x1E, 0x3D, 0x7C, 0x5B, 0x8A, 0x20, 0x30 } };

    GUID makeSubCommandGuid(const uint16_t index)
    {
        GUID g = kRootCommandGuid;
        g.Data4[6] = (BYTE)(0x21 + (index >> 8));
        g.Data4[7] = (BYTE)(index & 0xFF);
        return g;
    }

    std::optional<fs::path> extractSingleFilePath(IShellItemArray* pItems)
    {
        if (!pItems) {
            return std::nullopt;
        }
        DWORD count = 0;
        if (FAILED(pItems->GetCount(&count)) || count != 1) {
            return std::nullopt;
        }
        IShellItem* pItem = nullptr;
        if (FAILED(pItems->GetItemAt(0, &pItem)) || !pItem) {
            return std::nullopt;
        }

        SFGAOF attrs = 0;
        const HRESULT hrAttr = pItem->GetAttributes(SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_STREAM, &attrs);
        const bool isRegularFile = SUCCEEDED(hrAttr) &&
            (attrs & SFGAO_FILESYSTEM) != 0 &&
            ((attrs & SFGAO_FOLDER) == 0 || (attrs & SFGAO_STREAM) != 0);

        std::optional<fs::path> result;
        if (isRegularFile) {
            PWSTR pPath = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath)) && pPath) {
                result = fs::path(pPath);
                CoTaskMemFree(pPath);
            }
        }
        pItem->Release();
        return result;
    }

    void launchLepExe(const fs::path& appDir, const std::wstring& arguments)
    {
        const fs::path lepExe = appDir / L"LEP.exe";
        std::wstring cmdLine = quoteCommandLineArg(lepExe.wstring());
        if (!arguments.empty()) {
            cmdLine += L" " + arguments;
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(
                lepExe.c_str(),
                cmdLine.data(),
                nullptr, nullptr, FALSE,
                0, nullptr,
                appDir.c_str(),
                &si, &pi))
        {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        else {
            ShellExecuteW(nullptr, L"open", lepExe.c_str(), arguments.c_str(), appDir.c_str(), SW_SHOWNORMAL);
        }
    }

    enum class SubAction
    {
        Separator,
        RunWithGlobalProfile,
        RunWithAppProfile,
        ManageAppProfile,
        ShortcutUi,
        OpenGlobalManager,
    };

    struct SubItemSpec
    {
        SubAction action = SubAction::Separator;
        std::wstring title;
        std::wstring profileName;
        std::wstring iconPath;
        fs::path fallbackTarget;
        uint16_t guidIndex = 0;
    };

    class SubExplorerCommand final : public IExplorerCommand
    {
    public:
        SubExplorerCommand(fs::path appDir, SubItemSpec spec)
            : m_appDir(std::move(appDir)), m_spec(std::move(spec))
        {
            g_objectCount.fetch_add(1, std::memory_order_relaxed);
        }

        ~SubExplorerCommand()
        {
            g_objectCount.fetch_sub(1, std::memory_order_relaxed);
        }

        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) {
                return E_POINTER;
            }
            if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
                *ppv = static_cast<IExplorerCommand*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        IFACEMETHODIMP_(ULONG) AddRef() override
        {
            return (ULONG)m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        IFACEMETHODIMP_(ULONG) Release() override
        {
            const long count = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (count == 0) {
                delete this;
            }
            return (ULONG)count;
        }

        IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* ppszName) override
        {
            if (!ppszName) {
                return E_POINTER;
            }
            if (m_spec.action == SubAction::Separator) {
                *ppszName = nullptr;
                return S_FALSE;
            }
            return SHStrDupW(m_spec.title.c_str(), ppszName);
        }

        IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* ppszIcon) override
        {
            if (!ppszIcon) {
                return E_POINTER;
            }
            if (m_spec.action == SubAction::Separator || m_spec.iconPath.empty()) {
                *ppszIcon = nullptr;
                return E_NOTIMPL;
            }
            return SHStrDupW(m_spec.iconPath.c_str(), ppszIcon);
        }

        IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip) override
        {
            if (ppszInfotip) {
                *ppszInfotip = nullptr;
            }
            return E_NOTIMPL;
        }

        IFACEMETHODIMP GetCanonicalName(GUID* pguidCommandName) override
        {
            if (!pguidCommandName) {
                return E_POINTER;
            }
            *pguidCommandName = makeSubCommandGuid(m_spec.guidIndex);
            return S_OK;
        }

        IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* pCmdState) override
        {
            if (!pCmdState) {
                return E_POINTER;
            }
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx*) override
        {
            const auto selected = extractSingleFilePath(psiItemArray);
            const fs::path target = selected ? *selected : m_spec.fallbackTarget;

            switch (m_spec.action)
            {
            case SubAction::RunWithGlobalProfile:
                if (!target.empty()) {
                    launchLepExe(m_appDir, std::format(L"run --profile {} {}",
                        quoteCommandLineArg(m_spec.profileName), quoteCommandLineArg(target.wstring())));
                }
                break;

            case SubAction::RunWithAppProfile:
                if (!target.empty()) {
                    launchLepExe(m_appDir, std::format(L"run {}", quoteCommandLineArg(target.wstring())));
                }
                break;

            case SubAction::ManageAppProfile:
                if (!target.empty()) {
                    launchLepExe(m_appDir, std::format(L"manage {}", quoteCommandLineArg(target.wstring())));
                }
                break;

            case SubAction::ShortcutUi:
                if (!target.empty()) {
                    launchLepExe(m_appDir, std::format(L"shortcut-ui {}", quoteCommandLineArg(target.wstring())));
                }
                break;

            case SubAction::OpenGlobalManager:
                launchLepExe(m_appDir, L"");
                break;

            case SubAction::Separator:
            default:
                break;
            }
            return S_OK;
        }

        IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override
        {
            if (!pFlags) {
                return E_POINTER;
            }
            *pFlags = (m_spec.action == SubAction::Separator) ? ECF_ISSEPARATOR : ECF_DEFAULT;
            return S_OK;
        }

        IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override
        {
            if (ppEnum) {
                *ppEnum = nullptr;
            }
            return E_NOTIMPL;
        }

    private:
        std::atomic<long> m_refCount{ 1 };
        fs::path m_appDir;
        SubItemSpec m_spec;
    };

    class SubCommandEnumerator final : public IEnumExplorerCommand
    {
    public:
        explicit SubCommandEnumerator(std::vector<IExplorerCommand*> commands)
            : m_commands(std::move(commands))
        {
            g_objectCount.fetch_add(1, std::memory_order_relaxed);
        }

        ~SubCommandEnumerator()
        {
            for (auto* pCmd : m_commands) {
                if (pCmd) {
                    pCmd->Release();
                }
            }
            g_objectCount.fetch_sub(1, std::memory_order_relaxed);
        }

        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) {
                return E_POINTER;
            }
            if (riid == IID_IUnknown || riid == IID_IEnumExplorerCommand) {
                *ppv = static_cast<IEnumExplorerCommand*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        IFACEMETHODIMP_(ULONG) AddRef() override
        {
            return (ULONG)m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        IFACEMETHODIMP_(ULONG) Release() override
        {
            const long count = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (count == 0) {
                delete this;
            }
            return (ULONG)count;
        }

        IFACEMETHODIMP Next(ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched) override
        {
            if (!pUICommand) {
                return E_POINTER;
            }
            ULONG fetched = 0;
            while (fetched < celt && m_index < m_commands.size()) {
                pUICommand[fetched] = m_commands[m_index];
                pUICommand[fetched]->AddRef();
                ++fetched;
                ++m_index;
            }
            if (pceltFetched) {
                *pceltFetched = fetched;
            }
            return (fetched == celt) ? S_OK : S_FALSE;
        }

        IFACEMETHODIMP Skip(ULONG celt) override
        {
            m_index = std::min(m_commands.size(), m_index + (size_t)celt);
            return (m_index < m_commands.size()) ? S_OK : S_FALSE;
        }

        IFACEMETHODIMP Reset() override
        {
            m_index = 0;
            return S_OK;
        }

        IFACEMETHODIMP Clone(IEnumExplorerCommand** ppEnum) override
        {
            if (!ppEnum) {
                return E_POINTER;
            }
            std::vector<IExplorerCommand*> copy;
            copy.reserve(m_commands.size());
            for (auto* pCmd : m_commands) {
                pCmd->AddRef();
                copy.push_back(pCmd);
            }
            auto* pClone = new (std::nothrow) SubCommandEnumerator(std::move(copy));
            if (!pClone) {
                return E_OUTOFMEMORY;
            }
            pClone->m_index = m_index;
            *ppEnum = pClone;
            return S_OK;
        }

    private:
        std::atomic<long> m_refCount{ 1 };
        std::vector<IExplorerCommand*> m_commands;
        size_t m_index = 0;
    };

    class RootExplorerCommand final : public IExplorerCommand, public IObjectWithSite
    {
    public:
        RootExplorerCommand()
            : m_appDir(getAppDirectory(g_hModule))
        {
            g_objectCount.fetch_add(1, std::memory_order_relaxed);
        }

        ~RootExplorerCommand()
        {
            if (m_pSite) {
                m_pSite->Release();
                m_pSite = nullptr;
            }
            g_objectCount.fetch_sub(1, std::memory_order_relaxed);
        }

        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) {
                return E_POINTER;
            }
            if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
                *ppv = static_cast<IExplorerCommand*>(this);
                AddRef();
                return S_OK;
            }
            if (riid == IID_IObjectWithSite) {
                *ppv = static_cast<IObjectWithSite*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = nullptr;
            return E_NOINTERFACE;
        }

        IFACEMETHODIMP_(ULONG) AddRef() override
        {
            return (ULONG)m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        IFACEMETHODIMP_(ULONG) Release() override
        {
            const long count = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (count == 0) {
                delete this;
            }
            return (ULONG)count;
        }

        IFACEMETHODIMP SetSite(IUnknown* pUnkSite) override
        {
            if (m_pSite) {
                m_pSite->Release();
                m_pSite = nullptr;
            }
            m_pSite = pUnkSite;
            if (m_pSite) {
                m_pSite->AddRef();
            }
            return S_OK;
        }

        IFACEMETHODIMP GetSite(REFIID riid, void** ppvSite) override
        {
            if (!ppvSite) {
                return E_POINTER;
            }
            *ppvSite = nullptr;
            return m_pSite ? m_pSite->QueryInterface(riid, ppvSite) : E_FAIL;
        }

        IFACEMETHODIMP GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName) override
        {
            if (!ppszName) {
                return E_POINTER;
            }
            if (const auto selected = extractSingleFilePath(psiItemArray)) {
                m_selectedPath = *selected;
            }
            const auto cfgPath = resolveGlobalConfigPath(m_appDir);
            const auto cfgExp = loadGlobalConfigReadOnly(cfgPath);
            I18n i18n;
            i18n.load(m_appDir, cfgExp ? cfgExp->language : L"auto");
            const std::wstring title = i18n.tr("context_menu", "root_title", L"Locale Emulator Plus");
            return SHStrDupW(title.c_str(), ppszName);
        }

        IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* ppszIcon) override
        {
            if (!ppszIcon) {
                return E_POINTER;
            }
            const std::wstring iconSpec = (m_appDir / L"LEP.exe").wstring() + L",0";
            return SHStrDupW(iconSpec.c_str(), ppszIcon);
        }

        IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* ppszInfotip) override
        {
            if (ppszInfotip) {
                *ppszInfotip = nullptr;
            }
            return E_NOTIMPL;
        }

        IFACEMETHODIMP GetCanonicalName(GUID* pguidCommandName) override
        {
            if (!pguidCommandName) {
                return E_POINTER;
            }
            *pguidCommandName = kRootCommandGuid;
            return S_OK;
        }

        IFACEMETHODIMP GetState(IShellItemArray* psiItemArray, BOOL /*fOkToBeSlow*/, EXPCMDSTATE* pCmdState) override
        {
            if (!pCmdState) {
                return E_POINTER;
            }
            const auto selected = extractSingleFilePath(psiItemArray);
            if (!selected) {
                m_selectedPath.clear();
                *pCmdState = ECS_HIDDEN;
                return S_OK;
            }
            m_selectedPath = *selected;
            *pCmdState = ECS_ENABLED;
            return S_OK;
        }

        IFACEMETHODIMP Invoke(IShellItemArray*, IBindCtx*) override
        {
            return S_OK;
        }

        IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags) override
        {
            if (!pFlags) {
                return E_POINTER;
            }
            *pFlags = ECF_HASSUBCOMMANDS;
            return S_OK;
        }

        IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum) override
        {
            if (!ppEnum) {
                return E_POINTER;
            }

            const fs::path cfgPath = resolveGlobalConfigPath(m_appDir);
            const auto cfgExp = loadGlobalConfigReadOnly(cfgPath);
            const GlobalConfig cfg = cfgExp ? *cfgExp : createDefaultGlobalConfig();

            I18n i18n;
            i18n.load(m_appDir, cfg.language);

            const std::wstring lepIcon = (m_appDir / L"LEP.exe").wstring() + L",0";
            std::vector<IExplorerCommand*> items;
            uint16_t seq = 1;

            auto pushItemFunc = [&](SubItemSpec spec)
                {
                    spec.guidIndex = seq++;
                    spec.fallbackTarget = m_selectedPath;
                    if (auto* pCmd = new (std::nothrow) SubExplorerCommand(m_appDir, std::move(spec))) {
                        items.push_back(pCmd);
                    }
                };

            // 1. 勾选显示在右键菜单中的全局配置（若全部未勾选则至少显示第一项）
            size_t addedProfiles = 0;
            for (const auto& prof : cfg.profiles) {
                if (!prof.showInMainMenu) {
                    continue;
                }
                pushItemFunc(SubItemSpec{
                    .action = SubAction::RunWithGlobalProfile,
                    .title = i18n.trFormat("context_menu", "run_with_profile", L"{name}", prof.name, L"用「" + prof.name + L"」运行"),
                    .profileName = prof.name,
                    .iconPath = lepIcon,
                });
                ++addedProfiles;
            }
            if (addedProfiles == 0 && !cfg.profiles.empty()) {
                const auto& prof = cfg.profiles.front();
                pushItemFunc(SubItemSpec{
                    .action = SubAction::RunWithGlobalProfile,
                    .title = i18n.trFormat("context_menu", "run_with_profile", L"{name}", prof.name, L"用「" + prof.name + L"」运行"),
                    .profileName = prof.name,
                    .iconPath = lepIcon,
                });
            }

            pushItemFunc(SubItemSpec{ .action = SubAction::Separator });

            // 2. 动态感知目标程序是否已有专属配置（同目录 .lep.toml 或全局 [[app_profiles]]）
            const auto appProf = findAppProfile(m_selectedPath, cfg);
            if (appProf) {
                pushItemFunc(SubItemSpec{
                    .action = SubAction::RunWithAppProfile,
                    .title = i18n.tr("context_menu", "run_app_profile", L"使用此程序的专属配置运行"),
                    .iconPath = lepIcon,
                });
                pushItemFunc(SubItemSpec{
                    .action = SubAction::ManageAppProfile,
                    .title = i18n.tr("context_menu", "edit_app_profile", L"编辑此程序的专属配置..."),
                });
            }
            else {
                pushItemFunc(SubItemSpec{
                    .action = SubAction::ManageAppProfile,
                    .title = i18n.tr("context_menu", "create_app_profile", L"为此程序创建专属配置..."),
                });
            }

            pushItemFunc(SubItemSpec{ .action = SubAction::Separator });

            // 3. 快捷方式与全局管理
            pushItemFunc(SubItemSpec{
                .action = SubAction::ShortcutUi,
                .title = i18n.tr("context_menu", "create_shortcut", L"为此程序创建快捷方式..."),
            });
            pushItemFunc(SubItemSpec{
                .action = SubAction::OpenGlobalManager,
                .title = i18n.tr("context_menu", "manage_global", L"管理全局配置..."),
            });

            auto* pEnum = new (std::nothrow) SubCommandEnumerator(std::move(items));
            if (!pEnum) {
                return E_OUTOFMEMORY;
            }
            *ppEnum = pEnum;
            return S_OK;
        }

    private:
        std::atomic<long> m_refCount{ 1 };
        IUnknown* m_pSite = nullptr;
        fs::path m_appDir;
        fs::path m_selectedPath;
    };
}

void setModuleHandle(const HMODULE hModule) noexcept
{
    g_hModule = hModule;
}

HMODULE getModuleHandle() noexcept
{
    return g_hModule;
}

long getActiveObjectCount() noexcept
{
    return g_objectCount.load(std::memory_order_acquire);
}

HRESULT createRootExplorerCommand(REFIID riid, void** ppv)
{
    auto* pCmd = new (std::nothrow) RootExplorerCommand();
    if (!pCmd) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = pCmd->QueryInterface(riid, ppv);
    pCmd->Release();
    return hr;
}

NAMESPACE_END(lep::shellext)
