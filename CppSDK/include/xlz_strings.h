#pragma once

#include <string>

#include "xlz_encoding.h"

namespace xlz
{
	// 临时字符串包装（用于把utf-8转换为usc2混杂ansi并保持生命周期）
	class TempUsc2AnsiString
	{
	public:
		// 从utf-8构造（对应：utf8 -> usc2混杂ansi）
		explicit TempUsc2AnsiString(const char* utf8, bool escapeTextCode = false)
			: data_(Utf8ToUsc2Ansi(utf8, escapeTextCode))
		{
		}

		// 取C字符串指针（对应：LPStr参数）
		const char* CStr() const
		{
			return data_.c_str();
		}

	private:
		std::string data_;
	};
}

