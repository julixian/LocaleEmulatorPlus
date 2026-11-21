# LocaleEmulatorPlus

简体中文 | [English](README.EN.md)

<img src="img/icon.png" width="128" alt="LocaleEmulatorPlus" />

LocaleEmulatorPlus 为 Windows 程序提供按应用配置的区域、代码页、字体和时区模拟。自 3.0.0 起全面重写为**纯原生 C++23 + Win32** 应用，无需安装 .NET 运行时，原生支持 Windows 7 至 Windows 11（x64）。原生进程注入核心位于 [LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) 仓库。

本项目继承并演进自原版 [Locale Emulator](https://github.com/xupefei/Locale-Emulator) 与 [Locale Emulator Core](https://github.com/xupefei/Locale-Emulator-Core)。

## 特性

- **纯原生零依赖**：C++23 + Win32 原生实现，静态链接运行库，无需 .NET Framework，支持 Windows 7 SP1 – Windows 11（64 位系统，同时支持启动 x64 与 x86 目标程序）。
- **统一入口 `LEP.exe`**：集成图形界面（启动、全局配置管理、右键菜单管理、设置）与命令行工具，告别碎片化的多个 exe。
- **Windows 11 一级右键菜单**：原生 `IExplorerCommand` Shell 扩展，支持 Windows 7/10 经典菜单以及 Windows 11 新版一级右键菜单（通过 MSIX 稀疏包注册，不再只藏在「显示更多选项」里）。
- **直观的每程序配置逻辑**：
  - 未为程序创建配置时，右键菜单仅显示「为此程序创建配置...」；创建后才显示「使用此程序的配置运行」和「编辑此程序的配置...」。
  - 配置编辑窗口明确区分「保存」「保存并运行」「创建快捷方式」，保存不再偷偷启动程序。
  - 目标所在目录只读时（如 ISO 镜像、光盘、受保护目录），自动提示回退将专属配置保存到全局 `config.toml`。
- **TOML 配置格式**：使用可读性更好的 `config.toml` 与 `<程序>.lep.toml`，默认提供简体中文、English、日本語界面语言（位于 `locales/*.toml`）。
- **Microsoft Store 就绪**：自动检测 MSIX 打包运行环境并将用户配置存放到 `%LOCALAPPDATA%\LocaleEmulatorPlus\config.toml`，附带商店打包清单 `packaging/AppxManifest.store.xml`。

<img src="img/menu_example.png" width="520" alt="menu_example" />

> **提示（Windows on ARM）**：在 Windows on ARM 设备上转区 x86 程序时，需要为 `LEPHelper_x86.exe` 和目标 x86 程序开启「禁用混合执行模式」。

<img src="img/Disable_Mixed_Execution_Mode.png" width="450" alt="Disable_Mixed_Execution_Mode" />

## 命令行用法

```powershell
# 使用全局配置名称启动程序
LEP.exe run --profile "Run in Japanese" "C:\Games\App.exe" --arg1

# 直接使用程序自带的 .lep.toml 或全局回退配置启动
LEP.exe run "C:\Games\App.exe"

# 打开某程序的专属配置编辑窗口
LEP.exe edit "C:\Games\App.exe"

# 创建指向 LEP 启动目标程序的快捷方式
LEP.exe shortcut "C:\Games\App.exe" --profile "Run in Japanese"

# 列出所有全局配置
LEP.exe list-profiles

# 注册 / 卸载右键菜单（默认当前用户经典菜单；--all-users 为所有用户注册，--win11 注册 Win11 一级稀疏包菜单）
LEP.exe install
LEP.exe uninstall
```

## 仓库结构

- `LEPCommon/`：共享 C++23 模块（TOML 配置读写、系统区域/时区枚举、多语言、快捷方式、进程启动器）。
- `LEP/`：主程序 `LEP.exe`（Win32 图形界面、CLI、右键菜单安装与稀疏包注册）。
- `LEPShellExt/`：原生 COM `IExplorerCommand` 右键菜单扩展 `LEPShellExt.dll`。
- `LEPHelper/`：轻量 32 位启动桥接器 `LEPHelper_x86.exe`（用于加载 `LoaderDll_x86.dll` 启动 32 位目标程序）。
- `assets/`：程序图标、清单与 `locales/*.toml` 语言文件。
- `packaging/`：Win11 稀疏包（`AppxManifest.sparse.xml`）与 Microsoft Store（`AppxManifest.store.xml`）清单及图标资源。

## 构建

需求：

- Windows x64，Python 3.10+
- [mcpp](https://github.com/mcpp-community/mcpp)（自动拉取 `llvm@23.1.3` 工具链与 `julixian.tomlkitpp` 依赖）
- Visual Studio（提供 MSVC v143 / 14.44 sysroot 与 Windows 10/11 SDK，含 `makeappx.exe`、`signtool.exe`）
- 相邻目录的 [LocaleEmulatorPlus-Core](https://github.com/julixian/LocaleEmulatorPlus-Core) 构建输出（`out/x86` 与 `out/x64`），也可通过 `--core-dir` 或 `CORE_DIR` 指定

构建命令：

```powershell
# 构建发布版并打包便携版 zip
python .\build.py --profile release --zip

# 同时生成用于 Microsoft Store 的完整 MSIX 包
python .\build.py --profile release --store-msix --zip

# 快速构建（跳过 LTO，适合开发调试）
python .\build.py --profile fast-release --skip-sparse
```

输出位于 `out/dist/`：

- `LEP.exe`
- `LEPShellExt.dll`
- `LEPHelper_x86.exe`
- `LEPShellExt.msix` / `LEPShellExt.cer`（Win11 一级右键菜单稀疏包及证书）
- `locales/`
- `LoaderDll_x86.dll`、`LoaderDll_x64.dll`、`LocaleEmulatorPlus_x86.dll`、`LocaleEmulatorPlus_x64.dll`（若 Core 目录存在）

## 许可证

本项目采用 [GNU General Public License v3.0](LICENSE.txt)（GPL-3.0-or-later）开源。

## 乞讨

从原分支那里继承修复并彻底重写消耗了我大量的精力（和 token orz），如果你愿意大发慈悲帮我回点血，我可以艾特[某位大神](https://github.com/SuQiandYing)或者它的星怒们给你们免费卖屁股()。

![donate](img/donate.png?raw=true)

Paypal: [paypal.me/julixian](https://www.paypal.com/paypalme/julixian)
