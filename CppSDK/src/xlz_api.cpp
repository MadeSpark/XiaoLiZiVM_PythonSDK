#include "xlz_api.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cctype>
#include <cstring>
#include <string>

namespace xlz
{
	void ApiResolver::SetApiData(const char* apiDataJson)
	{
		// 设置API数据（原始：corn.int/apprun传入的apidata）
		apiData_.assign(apiDataJson ? apiDataJson : "");
	}

	static const char* SkipSpaces(const char* p)
	{
		while (p && *p && std::isspace(static_cast<unsigned char>(*p)) != 0)
		{
			++p;
		}
		return p;
	}

	static std::string Utf8ToJsonUnicodeEscapes(const char* utf8)
	{
		// 把UTF-8转换为json的 \\uXXXX 序列（用于匹配apidata里被转义的中文key）
		if (utf8 == nullptr || *utf8 == '\0')
		{
			return std::string();
		}

		const int wlenWithNull = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (wlenWithNull <= 1)
		{
			return std::string();
		}

		std::wstring ws;
		ws.resize(static_cast<size_t>(wlenWithNull - 1));
		::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, ws.data(), wlenWithNull);

		std::string out;
		out.reserve(ws.size() * 6);

		for (wchar_t wc : ws)
		{
			char buf[8]{};
			::wsprintfA(buf, "\\u%04X", static_cast<unsigned int>(static_cast<uint16_t>(wc)));
			out.append(buf);
		}
		return out;
	}

	static std::string Utf8ToAcpString(const char* utf8)
	{
		// 把UTF-8转换为ANSI(ACP)（用于匹配apidata里直接存GBK等本地编码的key）
		if (utf8 == nullptr || *utf8 == '\0')
		{
			return std::string();
		}

		const int wlenWithNull = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (wlenWithNull <= 1)
		{
			return std::string();
		}

		std::wstring ws;
		ws.resize(static_cast<size_t>(wlenWithNull - 1));
		::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, ws.data(), wlenWithNull);

		const int blenWithNull = ::WideCharToMultiByte(CP_ACP, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
		if (blenWithNull <= 0)
		{
			return std::string();
		}

		std::string out;
		out.resize(static_cast<size_t>(blenWithNull));
		const int wrote = ::WideCharToMultiByte(CP_ACP, 0, ws.data(), static_cast<int>(ws.size()), out.data(), blenWithNull, nullptr, nullptr);
		if (wrote <= 0)
		{
			return std::string();
		}
		out.resize(static_cast<size_t>(wrote));
		return out;
	}

	uintptr_t ApiResolver::GetAddress(const char* apiChineseName) const
	{
		// 从json文本中按 "key":123456 解析地址（原始：取API函数地址）
		if (apiChineseName == nullptr || *apiChineseName == '\0')
		{
			return 0;
		}
		if (apiData_.empty())
		{
			return 0;
		}

		const char* base = apiData_.c_str();

		const std::string keyQuotedUtf8 = std::string("\"") + apiChineseName + "\"";
		const char* hit = std::strstr(base, keyQuotedUtf8.c_str());
		size_t hitKeySize = keyQuotedUtf8.size();
		if (hit == nullptr)
		{
			const std::string keyQuotedEscaped = std::string("\"") + Utf8ToJsonUnicodeEscapes(apiChineseName) + "\"";
			hit = std::strstr(base, keyQuotedEscaped.c_str());
			hitKeySize = keyQuotedEscaped.size();
			if (hit == nullptr)
			{
				const std::string keyQuotedAcp = std::string("\"") + Utf8ToAcpString(apiChineseName) + "\"";
				hit = std::strstr(base, keyQuotedAcp.c_str());
				hitKeySize = keyQuotedAcp.size();
				if (hit == nullptr)
				{
					return 0;
				}
			}
		}

		const char* p = hit + hitKeySize;
		p = SkipSpaces(p);
		if (p == nullptr || *p != ':')
		{
			return 0;
		}
		++p;
		p = SkipSpaces(p);
		if (p == nullptr)
		{
			return 0;
		}

		if (*p == '"')
		{
			++p;
			p = SkipSpaces(p);
		}

		bool neg = false;
		if (*p == '-')
		{
			neg = true;
			++p;
		}

		uintptr_t value = 0;
		bool any = false;
		while (*p && std::isdigit(static_cast<unsigned char>(*p)) != 0)
		{
			any = true;
			value = value * 10 + static_cast<uintptr_t>(*p - '0');
			++p;
		}
		if (!any)
		{
			return 0;
		}
		if (neg)
		{
			return 0;
		}

		return value;
	}

#if defined(_M_IX86)
	static uint64_t CallStdCallRaw(uintptr_t address, const uint32_t* args, size_t argCount)
	{
		// x86 stdcall: 逐个push参数（逆序），call后由callee清栈；返回值在EAX/EDX
		uint32_t lo = 0;
		uint32_t hi = 0;
		__asm
		{
			mov ecx, argCount
			mov esi, args
			test ecx, ecx
			jz _call
		_push_loop:
			dec ecx
			mov eax, [esi + ecx * 4]
			push eax
			test ecx, ecx
			jnz _push_loop
		_call:
			mov eax, address
			call eax
			mov lo, eax
			mov hi, edx
		}
		return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
	}
#else
#error "This SDK must be built as Win32 (x86)."
#endif

	uint32_t StdCallInvoker::CallU32(uintptr_t address, const std::vector<uint32_t>& args)
	{
		// 调用并返回32位（原始：整数型返回）
		const uint64_t r = CallStdCallRaw(address, args.empty() ? nullptr : args.data(), args.size());
		return static_cast<uint32_t>(r & 0xFFFFFFFFu);
	}

	uint64_t StdCallInvoker::CallU64(uintptr_t address, const std::vector<uint32_t>& args)
	{
		// 调用并返回64位（原始：长整数型返回）
		return CallStdCallRaw(address, args.empty() ? nullptr : args.data(), args.size());
	}

	void* StdCallInvoker::CallPtr(uintptr_t address, const std::vector<uint32_t>& args)
	{
		// 调用并返回指针（原始：返回到整数/指针）
		const uint64_t r = CallStdCallRaw(address, args.empty() ? nullptr : args.data(), args.size());
		return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(r & 0xFFFFFFFFu)));
	}

	void StdCallInvoker::CallVoid(uintptr_t address, const std::vector<uint32_t>& args)
	{
		// 调用无返回（原始：调用子程序）
		CallStdCallRaw(address, args.empty() ? nullptr : args.data(), args.size());
	}

	void ArgPacker::PushU32(uint32_t v)
	{
		// 压入DWORD参数（原始：整数/指针）
		args_.push_back(v);
	}

	void ArgPacker::PushU64(uint64_t v)
	{
		// 压入QWORD参数（原始：长整数型，按低DWORD、再高DWORD）
		const uint32_t lo = static_cast<uint32_t>(v & 0xFFFFFFFFull);
		const uint32_t hi = static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFull);
		args_.push_back(lo);
		args_.push_back(hi);
	}

	void ArgPacker::PushF64(double v)
	{
		// 压入double参数（原始：双精度，按IEEE754位模式拆分）
		static_assert(sizeof(double) == 8, "double must be 8 bytes");
		uint64_t bits = 0;
		std::memcpy(&bits, &v, sizeof(bits));
		PushU64(bits);
	}

	void ArgPacker::PushPtr(const void* p)
	{
		// 压入指针参数（原始：到整数(&x)）
		args_.push_back(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)));
	}

	const std::vector<uint32_t>& ArgPacker::Data() const
	{
		// 返回参数数组（原始：调用子程序参数序列）
		return args_;
	}
}

