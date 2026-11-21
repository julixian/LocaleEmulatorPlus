module;

#include "../../LEPCommon/LEPMacros.hpp"
#include <Windows.h>
#include <CommCtrl.h>

export module LEP.Gui.Win32Ui;

export import std.compat;

export NAMESPACE_BEGIN(lep::gui)

namespace fs = std::filesystem;

// 启用高 DPI 感知（Per-Monitor V2 -> Per-Monitor -> System，Win7 安全降级）
void enableProcessDpiAwareness();

// 获取窗口当前 DPI（Win7 回退 GetDeviceCaps）
UINT getWindowDpi(HWND hwnd);
int scaleDpi(int px96, UINT dpi);

// 创建清晰的系统 UI 字体（自动按 DPI 缩放）
HFONT createUiFont(UINT dpi, int pointSize = 9, bool bold = false);

// 在 Win10/11 上自动尝试适配现代窗口外观（Win7 下静默忽略）
void applyModernWindowChrome(HWND hwnd);

// 允许管理员权限窗口接收来自普通权限资源管理器的文件拖放（放行 UIPI 拖放消息）
void allowElevatedDragDrop(HWND hwnd);

// 控件文本读写
std::wstring getControlText(HWND hwnd);
void setControlText(HWND hwnd, std::wstring_view text);

// 剪贴板写入
bool copyTextToClipboard(HWND owner, std::wstring_view text);

// 通用控件工厂（自动应用字体）
HWND createLabel(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id = 0, DWORD extraStyle = 0);
HWND createGroupBox(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id = 0);
HWND createEdit(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id, DWORD extraStyle = 0);
HWND createButton(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id, bool isDefault = false);
HWND createCheckbox(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id);
HWND createRadio(HWND parent, HFONT font, std::wstring_view text, int x, int y, int w, int h, int id, bool groupStart = false);
HWND createComboBox(HWND parent, HFONT font, int x, int y, int w, int h, int id, bool editable = false);
HWND createListBox(HWND parent, HFONT font, int x, int y, int w, int h, int id);

// 系统文件选择与轻量文本输入弹窗
std::optional<fs::path> browseOpenTargetFile(HWND owner, const fs::path& initialDir = {});
std::optional<fs::path> browseSaveShortcutFile(HWND owner, const fs::path& suggestedPath);
std::optional<std::wstring> promptTextInput(HWND owner, std::wstring_view title, std::wstring_view label, std::wstring_view initialValue = L"");

// 居中于父窗口或工作区
void centerWindow(HWND hwnd, HWND relativeTo = nullptr);

NAMESPACE_END(lep::gui)
