# LocaleEmulatorPlus

简体中文 | [English](README.EN.md)

LocaleEmulatorPlus 为 Windows 程序提供按应用配置的区域、代码页和时区模拟。本仓库包含配置 GUI、资源管理器右键菜单扩展、安装器、更新器和 x86/x64 启动器。原生进程注入核心位于
[LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) 仓库。

## 功能

- 按程序配置和全局配置管理区域、代码页、字体、界面语言和时区。
- 通过 Explorer 右键菜单启动程序。
- 根据目标程序架构选择 `LEPProc_x86.exe` 或 `LEPProc_x64.exe`。
- 配置 GUI 和右键菜单提供多语言资源；右键菜单支持外部 `Lang/*.xml` 覆盖。

![](img/menu_example.png?raw=true)

PS: 在 Windows on arm 设备上转区 x86 程序需要为 `LEPProc_x86.exe` 和要转区的x86程序开启 `禁用混合执行模式`

![](img/Disable_Mixed_Execution_Mode.png?raw=true)

## 仓库结构

- `LEPCommonLibrary/`：共享配置、PE 类型判断和辅助代码。
- `LEPGUI/`：WPF 配置编辑器。
- `LEPProc/`：命令行和右键菜单启动器。
- `LEPContextMenuHandler/`：Explorer 右键菜单 COM shell 扩展。
- `LEPInstaller/`：右键菜单 shell 扩展安装器/卸载器。
- `LEPUpdater/`：更新器。
- `Build/Release/`：默认构建输出目录。

GUI 构建不强制要求原生 core DLL。默认会从相邻 core 仓库寻找已有输出：

```text
..\LocaleEmulatorPlus-Core\out\x86
..\LocaleEmulatorPlus-Core\out\x64
```

需要使用其他 core 仓库时，可以传入 `--core-dir` 或设置 `CORE_DIR`。原生 core 的构建细节请看 Core
仓库文档。

## 运行环境

GUI、安装器、启动器和 Explorer 右键菜单扩展均面向 .NET Framework 4.8。Windows 7/8/8.1 上使用 LEP 前，
请先安装 .NET Framework 4.8 Runtime，否则 Explorer 右键菜单可能无法显示或无法正确加载。不支持 Windows XP。

https://dotnet.microsoft.com/zh-cn/download/dotnet-framework/net48

## 构建

需求：

- Python 3.10+，无需额外 Python 包。
- 安装 Visual Studio，包含 MSBuild 和 .NET Framework 4.8 Targeting Pack。
- 如果希望输出目录可以直接运行，需要提前准备好原生 core DLL。

构建本 GUI 仓库：

```powershell
python .\build.py
```

```powershell
python .\build.py --configuration Debug
python .\build.py --core-dir D:\VSProj\LocaleEmulatorPlus-Core
python .\build.py --msbuild 'C:\Path\To\MSBuild.exe'
python .\build.py --dry-run
```

默认构建 Release，使用 `vswhere` 或 PATH 查找 MSBuild。`CONFIGURATION`、`CORE_DIR`、`MSBUILD` 也可通过环境变量设置，显式参数优先。`--dry-run` 只显示构建命令和文件复制计划；任一步骤失败均立即返回非零状态。脚本生成两个架构的启动器，不会启动安装器或部署到已安装目录。

输出目录：

```text
Build\Release
```

预期托管输出文件包括：

- `LEPGUI.exe`
- `LEPInstaller.exe`
- `LEPUpdater.exe`
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

`LEPProc` 分别构建 x86、x64 两个 exe，项目引用明确以 AnyCPU 构建 `LEPCommonLibrary`。安装器嵌入公共库和右键菜单 DLL，因此在这些组件构建后生成。所有托管项目都面向 .NET Framework 4.8。

## 许可证

本分支继承自 Locale Emulator。原始上游许可证说明和第三方来源请见 `README_ORG.md`。

## 乞讨
从原分支那里继承修复并扩展这份屎山代码消耗了我大量的精力（和token orz），如果你愿意大发慈悲帮我回点血，
我可以艾特[某位大神](https://github.com/SuQiandYing)或者它的星怒们给你们免费卖屁股()。

![donate](img/donate.png?raw=true)

Paypal: [paypal.me/julixian](https://www.paypal.com/paypalme/julixian)
