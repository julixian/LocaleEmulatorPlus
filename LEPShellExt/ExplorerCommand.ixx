module;

#include "../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <ShObjIdl_core.h>

export module LEPShellExt.ExplorerCommand;

export import std.compat;

export NAMESPACE_BEGIN(lep::shellext)

// 全局 COM 活动对象计数与 DLL 实例句柄（由 DllMain 维护）
void setModuleHandle(HMODULE hModule) noexcept;
HMODULE getModuleHandle() noexcept;
long getActiveObjectCount() noexcept;

// 创建顶层 IExplorerCommand 实例（对应 CLSID {A84F3C21-6E9D-4B2A-9F1E-3D7C5B8A2030}）
HRESULT createRootExplorerCommand(REFIID riid, void** ppv);

NAMESPACE_END(lep::shellext)
