#pragma once

#include <string>

namespace xlz
{
	// 处理事件（防卡死：DoEvents，暂时转让控制权以处理消息队列）
	void ProcessEvents();

	// usc2混杂ansi文本转utf-8（易语言传入：ansi文本中夹杂 \\uXXXX）
	std::string Usc2AnsiToUtf8(const char* usc2Ansi);

	// utf-8转usc2混杂ansi文本（调用框架API前：utf-8 -> ansi文本中夹杂 \\uXXXX）
	std::string Utf8ToUsc2Ansi(const char* utf8, bool escapeTextCode = false);
}

