# LocaleEmulatorPlus

English | [简体中文](README.zh-CN.md)

LocaleEmulatorPlus is a maintained fork of the original [Locale Emulator](https://github.com/xupefei/Locale-Emulator) GUI and launcher layer.
The original upstream README has been preserved as [README_ORG.md](README_ORG.md).

This repository contains the managed GUI, Explorer context-menu extension, installer, updater, and
launcher programs. The native process-injection core is kept in
[LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) repository.

## Inheritance

LocaleEmulatorPlus is derived from xupefei's Locale Emulator project. This fork keeps the proven
user workflow from the original project:

- per-application and global locale profiles;
- Explorer context-menu integration;
- a launcher that prepares a profile block and asks the native loader DLL to create the target
  process;
- language resources for both the WPF GUI and shell extension.

The project was renamed from `LocaleEmulator` / `LE` to `LocaleEmulatorPlus` / `LEP` so this fork
can evolve independently without pretending to be an official upstream release.

## Why This Fork Exists

The original Locale Emulator release is x86-focused and tied to an old build environment. This
fork keeps the user-facing workflow while making the codebase buildable with a modern local
toolchain and adding x64 launch support, but drops Windows XP support.

The main changes are:

- separate `LoaderDll_x86.dll` / `LoaderDll_x64.dll` and
  `LocaleEmulatorPlus_x86.dll` / `LocaleEmulatorPlus_x64.dll` native core binaries;
- separate `LEPProc_x86.exe` / `LEPProc_x64.exe` launchers, with automatic architecture selection
  based on the target executable;
- `LocaleEmulatorPlus` naming throughout the GUI and generated files;
- version reset to `1.0.0.0` for this fork;
- shell context-menu language resources embedded in the context-menu DLL, while still allowing
  external `Lang/*.xml` files to override them.

## Repository Layout

- `LEPCommonLibrary/` shared profile, PE-type, and helper code.
- `LEPGUI/` WPF profile editor.
- `LEPProc/` command-line/context-menu launcher.
- `LEPContextMenuHandler/` Explorer context-menu COM shell extension.
- `LEPInstaller/` installer/uninstaller for the context-menu shell extension.
- `LEPUpdater/` inherited updater project.
- `Build/Release/` default build output.

Native core DLLs are optional for the GUI build. By default, the build script looks for an adjacent
core checkout:

```text
..\LocaleEmulatorPlus-Core\out\x86
..\LocaleEmulatorPlus-Core\out\x64
```

Set `CORE_DIR` to another `LocaleEmulatorPlus-Core` checkout or prepared core output root when
needed. See the Core repository documentation for native core build details.

## Runtime

The GUI, installer, launcher, and Explorer context-menu handler target .NET Framework 4.8.
On Windows 7/8/8.1, install the .NET Framework 4.8 Runtime before using LEP, otherwise the
Explorer context menu may fail to appear or load correctly:

https://dotnet.microsoft.com/zh-cn/download/dotnet-framework/net48

## Build

Requirements:

- Visual Studio with MSBuild and .NET Framework 4.8 targeting pack.
- Optional prebuilt native core DLLs if you want the output folder to be directly runnable.

Build this GUI repository:

```bat
build.bat
```

Optional variables:

```bat
set CONFIGURATION=Release
set CORE_DIR=..\LocaleEmulatorPlus-Core
set MSBUILD=C:\Path\To\MSBuild.exe
build.bat
```

The output is written to:

```text
Build\Release
```

Expected managed output files include:

- `LEPGUI.exe`
- `LEPInstaller.exe`
- `LEPProc_x86.exe`
- `LEPProc_x64.exe`
- `LEPContextMenuHandler.dll`
- `LEPCommonLibrary.dll`
- `LEPVersion.xml`
- `Lang/`

Optional native core files are copied when present:

- `LoaderDll_x86.dll`
- `LoaderDll_x64.dll`
- `LocaleEmulatorPlus_x86.dll`
- `LocaleEmulatorPlus_x64.dll`

## Notes For Maintainers

`LEPProc` builds both x86 and x64 executables, but `LEPCommonLibrary` is AnyCPU. Old-style MSBuild
project references can look for `LEPCommonLibrary\bin\x86\Release` or
`LEPCommonLibrary\bin\x64\Release` during standalone project builds. `build.bat` intentionally
copies the AnyCPU output into those architecture folders before building `LEPProc`.

## License

This fork inherits source from Locale Emulator. See `README_ORG.md` for the original upstream
license notice and third-party attribution.
