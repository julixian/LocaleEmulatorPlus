# LocaleEmulatorPlus

[English](README.md) | 简体中文

LocaleEmulatorPlus 是原 [Locale Emulator](https://github.com/xupefei/Locale-Emulator) 的 GUI 与启动器部分的维护分支。原项目 README 已保留为 [README_ORG.md](README_ORG.md)。

本仓库包含托管 GUI、资源管理器右键菜单扩展、安装器、更新器和启动器程序。原生进程注入核心位于
[LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) 仓库。

## 继承关系

LocaleEmulatorPlus 派生自 xupefei 的 Locale Emulator 项目。这个分支保留原项目中已经验证过的用户工作流：

- 按程序配置和全局配置；
- Explorer 右键菜单集成；
- 启动器生成配置数据块，并调用原生 Loader DLL 创建目标进程；
- WPF GUI 和 shell 扩展分别使用语言资源。

项目从 `LocaleEmulator` / `LE` 改名为 `LocaleEmulatorPlus` / `LEP`，是为了让这个分支可以独立演进，
同时避免被误认为原项目的官方版本。

## 分支理由

原 Locale Emulator 主要面向 x86，依赖较旧的构建环境且已被 archived 。这个分支保留原有用户工作流，同时让代码可以使用
现代本地工具链构建，补充 x64 启动支持，并修复 LE 的若干 bug（但不再支持 Windows XP）。

主要变化包括：

- 分离的原生核心文件：`LoaderDll_x86.dll` / `LoaderDll_x64.dll` 和
  `LocaleEmulatorPlus_x86.dll` / `LocaleEmulatorPlus_x64.dll`；
- 分离的启动器：`LEPProc_x86.exe` / `LEPProc_x64.exe`，会根据目标 exe 架构自动选择；
- GUI 和生成文件整体改为 `LocaleEmulatorPlus` 命名；
- 本分支版本重置为 `1.0.0.0`；
- 右键菜单语言资源嵌入 `LEPContextMenuHandler.dll`，同时仍允许外部 `Lang/*.xml` 覆盖。

## 仓库结构

- `LEPCommonLibrary/`：共享配置、PE 类型判断和辅助代码。
- `LEPGUI/`：WPF 配置编辑器。
- `LEPProc/`：命令行和右键菜单启动器。
- `LEPContextMenuHandler/`：Explorer 右键菜单 COM shell 扩展。
- `LEPInstaller/`：右键菜单 shell 扩展安装器/卸载器。
- `LEPUpdater/`：继承下来的更新器项目。
- `Build/Release/`：默认构建输出目录。

GUI 构建不强制要求原生 core DLL。默认会从相邻 core 仓库寻找已有输出：

```text
..\LocaleEmulatorPlus-Core\out\x86
..\LocaleEmulatorPlus-Core\out\x64
```

需要使用其他 core 仓库或已准备好的 core 输出根目录时，可以设置 `CORE_DIR`。原生 core 的构建细节请看 Core
仓库文档。

## 运行环境

GUI、安装器、启动器和 Explorer 右键菜单扩展均面向 .NET Framework 4.8。Windows 7/8/8.1 上使用 LEP 前，
请先安装 .NET Framework 4.8 Runtime，否则 Explorer 右键菜单可能无法显示或无法正确加载：

https://dotnet.microsoft.com/zh-cn/download/dotnet-framework/net48

## 构建

需求：

- 安装 Visual Studio，包含 MSBuild 和 .NET Framework 4.8 Targeting Pack。
- 如果希望输出目录可以直接运行，需要提前准备好原生 core DLL。

构建本 GUI 仓库：

```bat
build.bat
```

可选变量：

```bat
set CONFIGURATION=Release
set CORE_DIR=..\LocaleEmulatorPlus-Core
set MSBUILD=C:\Path\To\MSBuild.exe
build.bat
```

输出目录：

```text
Build\Release
```

预期托管输出文件包括：

- `LEPGUI.exe`
- `LEPInstaller.exe`
- `LEPProc_x86.exe`
- `LEPProc_x64.exe`
- `LEPContextMenuHandler.dll`
- `LEPCommonLibrary.dll`
- `LEPVersion.xml`
- `Lang/`

如果原生 core 文件存在，会一并复制：

- `LoaderDll_x86.dll`
- `LoaderDll_x64.dll`
- `LocaleEmulatorPlus_x86.dll`
- `LocaleEmulatorPlus_x64.dll`

## 维护说明

`LEPProc` 会构建 x86 和 x64 两个 exe，但 `LEPCommonLibrary` 是 AnyCPU。单独构建旧式 MSBuild 项目时，
项目引用可能会查找 `LEPCommonLibrary\bin\x86\Release` 或 `LEPCommonLibrary\bin\x64\Release`。
`build.bat` 会在构建 `LEPProc` 前把 AnyCPU 输出复制到这些架构目录，以固定这个构建细节。

## 许可证

本分支继承自 Locale Emulator。原始上游许可证说明和第三方来源请见 `README_ORG.md`。

## 乞讨
从原分支那里继承修复并扩展这份屎山代码消耗了我大量的精力（和token orz），如果你愿意大发慈悲帮我回点血，
我可以艾特[某位大神](https://github.com/SuQiandYing)或者它的星怒们给你们免费卖屁股() 
![donate](https://pic.imgdb.cn/item/66d96ea4d9c307b7e99297a6.png)。
