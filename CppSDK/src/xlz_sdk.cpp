#include "xlz_sdk.h"

#include "xlz_encoding.h"

#include <cstring>

namespace xlz
{
	void Sdk::Initialize(const char* apiDataJsonMaybe, const char* pluginKeyMaybe)
	{
		// 初始化SDK上下文（原始：corn.int / apprun 赋值）
		apiData_.assign(apiDataJsonMaybe ? apiDataJsonMaybe : "");
		pluginKey_.assign(pluginKeyMaybe ? pluginKeyMaybe : "");
		resolver_.SetApiData(apiData_.c_str());
	}

	const std::string& Sdk::PluginKey() const
	{
		// 返回插件Key（原始：pluginkey）
		return pluginKey_;
	}

	const std::string& Sdk::ApiData() const
	{
		// 返回API数据（原始：apidata）
		return apiData_;
	}

	ApiResolver& Sdk::Resolver()
	{
		// 返回解析器（原始：取API函数地址）
		return resolver_;
	}

	std::string Sdk::CallApiReturnUtf8(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const
	{
		// 调用并把返回LPStr(ansi/usc2混杂) 转为utf-8
		const uintptr_t addr = resolver_.GetAddress(apiChineseName);
		if (addr == 0)
		{
			return std::string();
		}
		const char* p = static_cast<const char*>(StdCallInvoker::CallPtr(addr, packedArgs));
		if (p == nullptr)
		{
			return std::string();
		}
		return Usc2AnsiToUtf8(p);
	}

	uint32_t Sdk::CallApiReturnU32(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const
	{
		// 调用并返回32位
		const uintptr_t addr = resolver_.GetAddress(apiChineseName);
		if (addr == 0)
		{
			return 0;
		}
		return StdCallInvoker::CallU32(addr, packedArgs);
	}

	void Sdk::CallApiVoid(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const
	{
		// 调用无返回
		const uintptr_t addr = resolver_.GetAddress(apiChineseName);
		if (addr == 0)
		{
			return;
		}
		StdCallInvoker::CallVoid(addr, packedArgs);
	}
}

