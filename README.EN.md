# LocaleEmulatorPlus

[简体中文](README.md) | English

LocaleEmulatorPlus provides per-application locale, code-page, and time-zone emulation for Windows programs.
This repository contains the configuration GUI, Explorer context-menu extension, installer, updater, and
x86/x64 launchers. The native process-injection core is kept in
[LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) repository.

## Features

- Per-application and global profiles for locale, code pages, fonts, UI language, and time zone.
- Launch programs through the Explorer context menu.
- Select `LEPProc_x86.exe` or `LEPProc_x64.exe` according to the target architecture.
- Localized GUI and context-menu resources, with external `Lang/*.xml` overrides for the context menu.

![](img/menu_example.png?raw=true)

PS: 在 Windows on arm 设备上转区 x86 程序需要为 `LEPProc_x86.exe` 和要转区的x86程序开启 `禁用混合执行模式`

![](img/Disable_Mixed_Execution_Mode.png?raw=true)

## Repository Layout

- `LEPCommonLibrary/` shared profile, PE-type, and helper code.
- `LEPGUI/` WPF profile editor.
- `LEPProc/` command-line/context-menu launcher.
- `LEPContextMenuHandler/` Explorer context-menu COM shell extension.
- `LEPInstaller/` installer/uninstaller for the context-menu shell extension.
- `LEPUpdater/` updater.
- `Build/Release/` default build output.

Native core DLLs are optional for the GUI build. By default, the build script looks for an adjacent
core checkout:

```text
..\LocaleEmulatorPlus-Core\out\x86
..\LocaleEmulatorPlus-Core\out\x64
```

Use `--core-dir` or set `CORE_DIR` to another `LocaleEmulatorPlus-Core` checkout when
needed. See the Core repository documentation for native core build details.

## Runtime

The GUI, installer, launcher, and Explorer context-menu handler target .NET Framework 4.8.
On Windows 7/8/8.1, install the .NET Framework 4.8 Runtime before using LEP, otherwise the
Explorer context menu may fail to appear or load correctly. Windows XP is not supported.

https://dotnet.microsoft.com/zh-cn/download/dotnet-framework/net48

## Build

Requirements:

- Python 3.10+; no third-party Python packages are required.
- Visual Studio with MSBuild and .NET Framework 4.8 targeting pack.
- Optional prebuilt native core DLLs if you want the output folder to be directly runnable.

Build this GUI repository:

```powershell
python .\build.py
```

```powershell
python .\build.py --configuration Debug
python .\build.py --core-dir D:\VSProj\LocaleEmulatorPlus-Core
python .\build.py --msbuild 'C:\Path\To\MSBuild.exe'
python .\build.py --dry-run
```

Release is the default configuration. MSBuild is discovered through `vswhere` or PATH.
`CONFIGURATION`, `CORE_DIR`, and `MSBUILD` environment variables are also supported; explicit
arguments take precedence. `--dry-run` prints commands and copy operations without building.
Any failed step returns a nonzero exit code immediately. Both launcher architectures are built;
the script does not run the installer or deploy to an installed application directory.

The output is written to:

```text
Build\Release
```

Expected managed output files include:

- `LEPGUI.exe`
- `LEPInstaller.exe`
- `LEPUpdater.exe`
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

`LEPProc` builds x86 and x64 executables and explicitly references the AnyCPU configuration of
`LEPCommonLibrary`. The installer embeds the common library and context-menu DLL, so those
components are built first. All managed projects target .NET Framework 4.8.

## License

This fork inherits source from Locale Emulator. See `README_ORG.md` for the original upstream
license notice and third-party attribution.

## Buy me a coffee

Paypal: [paypal.me/julixian](https://www.paypal.com/paypalme/julixian)
