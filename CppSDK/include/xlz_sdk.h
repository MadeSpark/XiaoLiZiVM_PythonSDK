#pragma once

#include <cstdint>
#include <string>

#include "xlz_api.h"

namespace xlz
{
	// SDK版本号
	constexpr const char* SDK_VERSION = "6.0.7";
	// 小栗子SDK上下文（保存pluginkey/apidata与调用入口）
	class Sdk
	{
	public:
		// 初始化SDK（注意：C#示例中参数名与赋值是反的，这里按实际使用保留两个字段）
		void Initialize(const char* apiDataJsonMaybe, const char* pluginKeyMaybe);

		// 取插件Key（原始：pluginkey）
		const std::string& PluginKey() const;

		// 取API数据（原始：apidata/jsonstr）
		const std::string& ApiData() const;

		// 取API解析器（原始：取API函数地址）
		ApiResolver& Resolver();

		// 调用API并返回UTF-8文本（原始：返回LPStr + usc2toutf8）
		std::string CallApiReturnUtf8(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const;

		// 调用API并返回32位整数（原始：整数型返回）
		uint32_t CallApiReturnU32(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const;

		// 调用API无返回（原始：调用子程序）
		void CallApiVoid(const char* apiChineseName, const std::vector<uint32_t>& packedArgs) const;

	private:
		std::string pluginKey_;
		std::string apiData_;
		ApiResolver resolver_;
	};
}

