#include "../LEPCommon/LaunchProtocol.hpp"
#include <Windows.h>

import std.compat;
import LocaleEmulatorPlus;

namespace fs = std::filesystem;

namespace
{
    fs::path getSelfDir()
    {
        std::wstring buf(512, L'\0');
        for (;;) {
            const DWORD len = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
            if (len == 0) {
                return {};
            }
            if (len < buf.size()) {
                buf.resize(len);
                return fs::path(buf).parent_path();
            }
            buf.resize(buf.size() * 2);
        }
    }

    bool validatePacketString(
        const lep::LaunchHelperPacket* pPacket,
        const uint32_t offset,
        const uint32_t chars,
        std::wstring& outStr)
    {
        outStr.clear();
        if (offset == 0) {
            return chars == 0;
        }
        const uint64_t bytesWithNull = ((uint64_t)chars + 1u) * sizeof(wchar_t);
        if (offset < sizeof(lep::LaunchHelperPacket) ||
            offset > pPacket->totalSize ||
            bytesWithNull > pPacket->totalSize - offset)
        {
            return false;
        }
        const auto* pChars = (const wchar_t*)((const uint8_t*)pPacket + offset);
        if (pChars[chars] != L'\0') {
            return false;
        }
        outStr.assign(pChars, chars);
        return true;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR pCmdLine, int)
{
    if (!pCmdLine || pCmdLine[0] == L'\0') {
        return 1;
    }

    // 去除首尾空白/引号，得到共享内存名称
    std::wstring mapName(pCmdLine);
    while (!mapName.empty() && (mapName.front() == L' ' || mapName.front() == L'"')) {
        mapName.erase(mapName.begin());
    }
    while (!mapName.empty() && (mapName.back() == L' ' || mapName.back() == L'"')) {
        mapName.pop_back();
    }
    if (mapName.empty()) {
        return 1;
    }

    const HANDLE hMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, mapName.c_str());
    if (!hMap) {
        return (int)GetLastError();
    }

    auto* pPacket = (lep::LaunchHelperPacket*)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (!pPacket) {
        const DWORD err = GetLastError();
        CloseHandle(hMap);
        return (int)err;
    }

    auto finishFunc = [&](const int32_t status, const uint32_t win32Err, const uint32_t pid) -> int
        {
            pPacket->status = status;
            pPacket->win32Error = win32Err;
            pPacket->targetPid = pid;
            UnmapViewOfFile(pPacket);
            CloseHandle(hMap);
            return (win32Err == 0 && status >= 0) ? 0 : 1;
        };

    if (pPacket->magic != lep::kLaunchHelperMagic ||
        pPacket->version != lep::kLaunchHelperVersion ||
        pPacket->totalSize < sizeof(lep::LaunchHelperPacket) ||
        pPacket->totalSize > lep::kLaunchHelperMaxSize)
    {
        return finishFunc((int32_t)0xC000000DL, ERROR_INVALID_PARAMETER, 0);
    }

    std::wstring appPath;
    std::wstring cmdLine;
    std::wstring workDir;
    if (!validatePacketString(pPacket, pPacket->appOffset, pPacket->appChars, appPath) ||
        !validatePacketString(pPacket, pPacket->cmdOffset, pPacket->cmdChars, cmdLine) ||
        !validatePacketString(pPacket, pPacket->dirOffset, pPacket->dirChars, workDir))
    {
        return finishFunc((int32_t)0xC000000DL, ERROR_INVALID_PARAMETER, 0);
    }

    const fs::path loaderPath = getSelfDir() / L"LoaderDll_x86.dll";
    const HMODULE hLoader = LoadLibraryW(loaderPath.c_str());
    if (!hLoader) {
        return finishFunc((int32_t)0xC0000135L, GetLastError(), 0);
    }

    const auto pCreate = (LEP::LepCreateProcess2_t)GetProcAddress(hLoader, "LepCreateProcess2");
    if (!pCreate) {
        const DWORD err = GetLastError();
        FreeLibrary(hLoader);
        return finishFunc((int32_t)0xC0000139L, err != 0 ? err : ERROR_PROC_NOT_FOUND, 0);
    }

    LEP::LEPB env{};
    static_assert(sizeof(env) == sizeof(pPacket->lepbBytes));
    std::memcpy(&env, pPacket->lepbBytes, sizeof(env));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    const LEP::NTSTATUS status = pCreate(
        &env,
        appPath.empty() ? nullptr : appPath.c_str(),
        cmdLine.empty() ? nullptr : cmdLine.data(),
        workDir.empty() ? nullptr : workDir.c_str(),
        pPacket->creationFlags,
        &si,
        &pi,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    FreeLibrary(hLoader);

    if (pi.hThread) {
        CloseHandle(pi.hThread);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
    }

    return finishFunc((int32_t)status, 0, pi.dwProcessId);
}
