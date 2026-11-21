module;

#include <Windows.h>
#include <cstddef>

// Standalone declarations for LoadLibrary/GetProcAddress callers.
// No Core headers or import libraries are required.
export module LocaleEmulatorPlus;

import std.compat;

#pragma pack(push, 8)
export namespace LEP
{
    using NTSTATUS = LONG;
    inline constexpr ULONG LEP_ENVIRONMENT_VERSION = 4;

    struct LOCALE_EMULATOR_PLUS_ENVIRONMENT_BLOCK
    {
        ULONG Size;
        ULONG Version;
        ULONG AnsiCodePage;
        ULONG OemCodePage;
        ULONG LocaleId;
        ULONG FontCharset;
        ULONG RegistryRedirectionMode;
        ULONG HookUILanguageMode;
        WCHAR TimezoneId[128]; // Required, zero-terminated Windows ID.
        WCHAR StandardNameOverride[32]; // Empty uses the resolved default.
        WCHAR DaylightNameOverride[32]; // Empty uses the resolved default.
    };
    using LEPB = LOCALE_EMULATOR_PLUS_ENVIRONMENT_BLOCK;
    using PLEPB = LEPB*;
    using PLOCALE_EMULATOR_PLUS_ENVIRONMENT_BLOCK = PLEPB;

    // Both exports use this signature. NTSTATUS is signed, not a Win32
    // GetLastError code; success is status >= 0. WINAPI matters on x86.
    using LepCreateProcess_t = NTSTATUS (WINAPI*)(
        PLEPB EnvironmentBlock,
        PCWSTR ApplicationName,
        PWSTR CommandLine,
        PCWSTR CurrentDirectory,
        ULONG CreationFlags,
        LPSTARTUPINFOW StartupInfo,
        LPPROCESS_INFORMATION ProcessInformation,
        LPSECURITY_ATTRIBUTES ProcessAttributes,
        LPSECURITY_ATTRIBUTES ThreadAttributes,
        PVOID Environment,
        HANDLE Token);

    using LepCreateProcess2_t = LepCreateProcess_t;

    static_assert(sizeof(LEPB) == 416);
    static_assert(offsetof(LEPB, TimezoneId) == 32);
    static_assert(offsetof(LEPB, StandardNameOverride) == 288);
    static_assert(offsetof(LEPB, DaylightNameOverride) == 352);
    static_assert(sizeof(PROCESS_INFORMATION) == (sizeof(PVOID) == 8 ? 24 : 16));
}
#pragma pack(pop)
