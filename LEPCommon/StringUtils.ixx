module;

#include "LEPMacros.hpp"
#include <Windows.h>

export module LEPCommon.StringUtils;

export import std.compat;

export NAMESPACE_BEGIN(lep)

namespace fs = std::filesystem;

std::wstring utf82Wide(std::string_view utf8);
std::string wide2Utf8(std::wstring_view wide);

std::wstring trimW(std::wstring_view text);
std::wstring toLowerW(std::wstring_view text);
bool equalsIgnoreCaseW(std::wstring_view a, std::wstring_view b);
std::wstring replaceAllW(std::wstring_view text, std::wstring_view from, std::wstring_view to);

// 按 Windows CreateProcessW / CommandLineToArgvW 规则转义单个命令行参数
std::wstring quoteCommandLineArg(std::wstring_view arg);

// 将相对路径转为完整路径并统一反斜杠，用于程序专属配置路径匹配
std::wstring normalizePathForMatch(const fs::path& path);

NAMESPACE_END(lep)
