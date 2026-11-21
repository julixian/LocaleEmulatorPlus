#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <Unknwn.h>

#pragma comment(linker, "/EXPORT:DllGetClassObject,PRIVATE")
#pragma comment(linker, "/EXPORT:DllCanUnloadNow,PRIVATE")

import std.compat;
import LEPShellExt.ExplorerCommand;

namespace
{
    // {A84F3C21-6E9D-4B2A-9F1E-3D7C5B8A2030}
    constexpr CLSID kClsidLepShellExt =
        { 0xA84F3C21, 0x6E9D, 0x4B2A, { 0x9F, 0x1E, 0x3D, 0x7C, 0x5B, 0x8A, 0x20, 0x30 } };

    std::atomic<long> g_serverLockCount{ 0 };

    class ShellExtClassFactory final : public IClassFactory
    {
    public:
        ShellExtClassFactory()
        {
            g_serverLockCount.fetch_add(1, std::memory_order_relaxed);
        }

        ~ShellExtClassFactory()
        {
            g_serverLockCount.fetch_sub(1, std::memory_order_relaxed);
        }

        IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override
        {
            if (!ppv) {
                return E_POINTER;
            }
            if (riid == IID_IUnknown || riid == IID_IClassFactory) {
                *ppv = static_cast<IClassFactory*>(this);
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

        IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
        {
            if (pUnkOuter != nullptr) {
                return CLASS_E_NOAGGREGATION;
            }
            return lep::shellext::createRootExplorerCommand(riid, ppv);
        }

        IFACEMETHODIMP LockServer(BOOL fLock) override
        {
            if (fLock) {
                g_serverLockCount.fetch_add(1, std::memory_order_relaxed);
            }
            else {
                g_serverLockCount.fetch_sub(1, std::memory_order_relaxed);
            }
            return S_OK;
        }

    private:
        std::atomic<long> m_refCount{ 1 };
    };
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReason, LPVOID)
{
    if (ulReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        lep::shellext::setModuleHandle(hModule);
    }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!ppv) {
        return E_POINTER;
    }
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, kClsidLepShellExt)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto* pFactory = new (std::nothrow) ShellExtClassFactory();
    if (!pFactory) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

extern "C" HRESULT __stdcall DllCanUnloadNow()
{
    const bool idle = lep::shellext::getActiveObjectCount() == 0 &&
        g_serverLockCount.load(std::memory_order_acquire) == 0;
    return idle ? S_OK : S_FALSE;
}
