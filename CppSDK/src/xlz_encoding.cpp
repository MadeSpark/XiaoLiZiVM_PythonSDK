#include "xlz_encoding.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>

namespace xlz
{
	static std::wstring AcpToWide(const char* acp)
	{
		// ANSI(本机代码页) 转 Unicode（原始：AnsiToUnicode）
		if (acp == nullptr)
		{
			return std::wstring();
		}

		const int len = static_cast<int>(::strlen(acp));
		if (len == 0)
		{
			return std::wstring();
		}

		const int wlen = ::MultiByteToWideChar(CP_ACP, 0, acp, len, nullptr, 0);
		if (wlen <= 0)
		{
			return std::wstring();
		}

		std::wstring ws;
		ws.resize(static_cast<size_t>(wlen));
		::MultiByteToWideChar(CP_ACP, 0, acp, len, ws.data(), wlen);
		return ws;
	}

	static std::string WideToUtf8(const std::wstring& ws)
	{
		// Unicode 转 UTF-8（原始：UnicodeToUtf8）
		if (ws.empty())
		{
			return std::string();
		}

		const int blen = ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
		if (blen <= 0)
		{
			return std::string();
		}

		std::string out;
		out.resize(static_cast<size_t>(blen));
		::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), blen, nullptr, nullptr);
		return out;
	}

	static std::wstring Utf8ToWide(const char* utf8)
	{
		// UTF-8 转 Unicode（原始：Utf8ToUnicode）
		if (utf8 == nullptr)
		{
			return std::wstring();
		}

		const int len = static_cast<int>(::strlen(utf8));
		if (len == 0)
		{
			return std::wstring();
		}

		const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8, len, nullptr, 0);
		if (wlen <= 0)
		{
			return std::wstring();
		}

		std::wstring ws;
		ws.resize(static_cast<size_t>(wlen));
		::MultiByteToWideChar(CP_UTF8, 0, utf8, len, ws.data(), wlen);
		return ws;
	}

	static bool WideCharToAcpChar(wchar_t wc, std::string& out)
	{
		// Unicode 单码元尝试转 ANSI；若不可表示，则返回 false（原始：UnicodeToAnsi + “??”检测）
		char buf[8]{};
		BOOL usedDefaultChar = FALSE;
		const int blen = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, &wc, 1, buf, static_cast<int>(sizeof(buf)), nullptr, &usedDefaultChar);
		if (blen <= 0 || usedDefaultChar != FALSE)
		{
			return false;
		}
		out.assign(buf, buf + blen);
		return true;
	}

	static bool IsHex4(const wchar_t* p)
	{
		for (int i = 0; i < 4; i++)
		{
			const wchar_t c = p[i];
			const bool ok = (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F');
			if (!ok)
			{
				return false;
			}
		}
		return true;
	}

	static uint16_t Hex4ToU16(const wchar_t* p)
	{
		uint16_t v = 0;
		for (int i = 0; i < 4; i++)
		{
			wchar_t c = p[i];
			uint16_t n = 0;
			if (c >= L'0' && c <= L'9')
			{
				n = static_cast<uint16_t>(c - L'0');
			}
			else if (c >= L'a' && c <= L'f')
			{
				n = static_cast<uint16_t>(10 + (c - L'a'));
			}
			else
			{
				n = static_cast<uint16_t>(10 + (c - L'A'));
			}
			v = static_cast<uint16_t>((v << 4) | n);
		}
		return v;
	}

	void ProcessEvents()
	{
		MSG msg{};
		while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
	}

	std::string Usc2AnsiToUtf8(const char* usc2Ansi)
	{
		// 可以将混杂有usc2的ansi文字编码为utf8（原始：Usc2ToUtf8）
		std::wstring ws = AcpToWide(usc2Ansi);
		if (ws.empty())
		{
			return std::string();
		}

		std::wstring out;
		out.reserve(ws.size());

		for (size_t i = 0; i < ws.size(); i++)
		{
			const wchar_t c = ws[i];
			if (c == L'\\' && (i + 1) < ws.size() && ws[i + 1] == L'u')
			{
				if ((i + 6) <= ws.size() && IsHex4(ws.data() + i + 2))
				{
					const uint16_t code1 = Hex4ToU16(ws.data() + i + 2);
					if (code1 >= 0xD800 && code1 <= 0xDBFF)
					{
						if ((i + 12) <= ws.size() && ws[i + 6] == L'\\' && ws[i + 7] == L'u' && IsHex4(ws.data() + i + 8))
						{
							const uint16_t code2 = Hex4ToU16(ws.data() + i + 8);
							if (code2 >= 0xDC00 && code2 <= 0xDFFF)
							{
								out.push_back(static_cast<wchar_t>(code1));
								out.push_back(static_cast<wchar_t>(code2));
								i += 11;
								continue;
							}
						}
					}
					out.push_back(static_cast<wchar_t>(code1));
					i += 5;
					continue;
				}
			}
			out.push_back(c);
		}

		return WideToUtf8(out);
	}

	std::string Utf8ToUsc2Ansi(const char* utf8, bool escapeTextCode)
	{
		// 可将utf8转换成混杂有usc2的ansi文字（原始：Utf8ToUsc2）
		std::wstring ws = Utf8ToWide(utf8);
		if (ws.empty())
		{
			return std::string();
		}

		std::string out;
		out.reserve(ws.size() * 3);

		for (size_t i = 0; i < ws.size(); i++)
		{
			const wchar_t wc = ws[i];

			if (wc >= 0xD800 && wc <= 0xDBFF && (i + 1) < ws.size())
			{
				const wchar_t wc2 = ws[i + 1];
				if (wc2 >= 0xDC00 && wc2 <= 0xDFFF)
				{
					char buf1[16]{};
					char buf2[16]{};
					::wsprintfA(buf1, "\\u%04X", static_cast<unsigned int>(static_cast<uint16_t>(wc)));
					::wsprintfA(buf2, "\\u%04X", static_cast<unsigned int>(static_cast<uint16_t>(wc2)));
					out.append(buf1);
					out.append(buf2);
					i += 1;
					continue;
				}
			}

			std::string acpChar;
			if (WideCharToAcpChar(wc, acpChar))
			{
				if (escapeTextCode)
				{
					if (acpChar == "\\")
					{
						out.append("\\u005C");
						continue;
					}
					if (acpChar == "[")
					{
						out.append("\\u005B");
						continue;
					}
					if (acpChar == "]")
					{
						out.append("\\u005D");
						continue;
					}
				}
				out.append(acpChar);
				continue;
			}

			char buf[16]{};
			::wsprintfA(buf, "\\u%04X", static_cast<unsigned int>(static_cast<uint16_t>(wc)));
			out.append(buf);
		}

		return out;
	}
}

