#pragma once

#include <Windows.h>
#include <cstdint>

namespace lep
{
    inline constexpr uint32_t kLaunchHelperMagic = 0x31484C4Cu; // 'LLH1'
    inline constexpr uint32_t kLaunchHelperVersion = 1u;
    inline constexpr uint32_t kLaunchHelperMaxSize = 1024u * 1024u;

    // 64 位 LEP.exe 与 32 位 LEPHelper_x86.exe 之间的跨架构共享内存契约（无指针、固定宽度）
    struct alignas(8) LaunchHelperPacket
    {
        uint32_t magic;
        uint32_t version;
        uint32_t totalSize;
        uint32_t creationFlags;
        int32_t status;       // 输出：LoaderDll!LepCreateProcess2 返回的 NTSTATUS
        uint32_t win32Error;  // 输出：加载 LoaderDll 等步骤的 Win32 错误码
        uint32_t targetPid;   // 输出：目标进程 PID
        uint32_t reserved0;
        uint8_t lepbBytes[416];
        uint32_t appOffset;   // 字节偏移，0 表示 nullptr
        uint32_t appChars;    // 不含末尾 NUL 的字符数
        uint32_t cmdOffset;
        uint32_t cmdChars;
        uint32_t dirOffset;
        uint32_t dirChars;
        uint32_t reserved1;
        uint32_t reserved2;
    };

    static_assert(sizeof(LaunchHelperPacket) == 480, "LaunchHelperPacket ABI mismatch");
}
