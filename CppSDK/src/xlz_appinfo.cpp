#include "xlz_appinfo.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <Windows.h>

#include "xlz_permission_names.h"

namespace xlz
{
	void AppInfoBuilder::SetSdkVersion(const char* sdkVersion)
	{
		// 设置SDK版本（对应：sdkv）
		sdkvUtf8_.assign(sdkVersion ? sdkVersion : "");
	}

	void AppInfoBuilder::SetAppName(const char* appNameUtf8)
{
	// 设置应用名（对应：置应用名 + 处理应用名）
	try
	{
		appNameUtf8_ = SanitizeAppNameUtf8(appNameUtf8 ? std::string(appNameUtf8) : std::string());
	}
	catch (const std::exception& e)
	{
		// 捕获异常并触发回调
		TriggerException(e.what());
	}
	catch (...)
	{
		// 捕获未知异常并触发回调
		TriggerException("Unknown error");
	}
}

	void AppInfoBuilder::SetAuthor(const char* authorUtf8)
	{
		// 设置应用作者（对应：置应用作者）
		authorUtf8_.assign(authorUtf8 ? authorUtf8 : "");
	}

	void AppInfoBuilder::SetAppVersion(const char* appVersionUtf8)
	{
		// 设置应用版本（对应：置应用版本）
		appvUtf8_.assign(appVersionUtf8 ? appVersionUtf8 : "");
	}

	void AppInfoBuilder::SetDescription(const char* descriptionUtf8)
	{
		// 设置应用说明（对应：置应用说明）
		describeUtf8_.assign(descriptionUtf8 ? descriptionUtf8 : "");
	}

	void AppInfoBuilder::SetSliderHandlerAddress(uintptr_t address)
	{
		// 置滑块处理函数地址（对应：getticketaddres）
		getticketaddres_ = address;
	}

	void AppInfoBuilder::SetSmsCodeHandlerAddress(uintptr_t address)
	{
		// 置短信验证码处理函数地址（对应：getvefcodeaddres）
		getvefcodeaddres_ = address;
	}

	void AppInfoBuilder::SetPluginEnabledAddress(uintptr_t address)
	{
		// 置插件被启用处理函数地址（对应：useproaddres）
		useproaddres_ = address;
	}

	void AppInfoBuilder::SetPluginDisabledAddress(uintptr_t address)
	{
		// 置插件被禁用处理函数地址（对应：banproaddres）
		banproaddres_ = address;
	}

	void AppInfoBuilder::SetPluginUninstallAddress(uintptr_t address)
	{
		// 置将被卸载处理函数地址（对应：unitproaddres）
		unitproaddres_ = address;
	}

	void AppInfoBuilder::SetPluginSettingAddress(uintptr_t address)
	{
		// 置点击插件设置处理函数地址（对应：setproaddres）
		setproaddres_ = address;
	}

	void AppInfoBuilder::SetPrivateMessageAddress(uintptr_t address)
	{
		// 置私聊消息处理函数地址（对应：friendmsaddres）
		friendmsaddres_ = address;
	}

	void AppInfoBuilder::SetGroupMessageAddress(uintptr_t address)
	{
		// 置群消息处理函数地址（对应：groupmsaddres）
		groupmsaddres_ = address;
	}

	void AppInfoBuilder::SetChannelUnifiedAddress(uintptr_t address)
	{
		// 置频道推送统一处理函数地址（对应：ChannelFunc）
		ChannelFunc_ = address;
	}

	void AppInfoBuilder::SetEventMessageAddress(uintptr_t address)
	{
		// 置事件消息处理函数地址（对应：eventmsaddres）
		eventmsaddres_ = address;
	}

	void AppInfoBuilder::SetOutputReplaceAddress(uintptr_t address)
	{
		// 置插件消息输出替换处理函数地址（对应：PmDealFunc）
		PmDealFunc_ = address;
	}

	void AppInfoBuilder::RequestPermissionByName(const char* apiChineseName, const char* reasonUtf8)
	{
		// 置应用权限申请（按中文API名）
		if (apiChineseName == nullptr || *apiChineseName == '\0')
		{
			return;
		}
		std::string reason = reasonUtf8 ? std::string(reasonUtf8) : std::string();
		if (reason.empty())
		{
			return;
		}
		PermissionItem item;
		item.apiNameZh.assign(apiChineseName);
		item.descUtf8 = reason;
		permissions_.push_back(std::move(item));
	}

	void AppInfoBuilder::RequestPermission(Permission permission, const char* reasonUtf8)
	{
		// 置应用权限申请（按权限枚举）
		const char* nameZh = GetPermissionChineseName(permission);
		if (nameZh == nullptr || *nameZh == '\0')
		{
			return;
		}
		RequestPermissionByName(nameZh, reasonUtf8);
	}

	std::string AppInfoBuilder::EscapeJsonStringUtf8(const std::string& utf8)
	{
		// 字符转义（对应：字符转义）
		std::string out;
		out.reserve(utf8.size() + 8);
		for (const unsigned char ch : utf8)
		{
			switch (ch)
			{
			case '\\':
				out.append("\\\\");
				break;
			case '"':
				out.append("\\\"");
				break;
			case '\r':
				out.append("\\r");
				break;
			case '\n':
				out.append("\\n");
				break;
			default:
				out.push_back(static_cast<char>(ch));
				break;
			}
		}
		return out;
	}

	std::string AppInfoBuilder::SanitizeAppNameUtf8(const std::string& utf8)
	{
		// 处理应用名（对应：处理应用名，去掉非法文件名字符与换行）
		std::string out = utf8;
		const char* bad = "/\\:*?\"<>|'";
		for (char& c : out)
		{
			if (c == '\r' || c == '\n')
			{
				c = ' ';
				continue;
			}
			if (std::strchr(bad, c) != nullptr)
			{
				c = ' ';
			}
		}
		return out;
	}

	void AppInfoBuilder::SetExceptionCallback(ExceptionCallback callback)
{
	// 设置异常回调函数（兼容旧接口）
	exceptionCallback_ = callback;
}

void AppInfoBuilder::TriggerException(const char* errorMessage) const
{
	// 触发异常回调
	if (exceptionCallback_ && !appNameUtf8_.empty())
	{
		exceptionCallback_(appNameUtf8_.c_str(), errorMessage);
	}

	// 直接弹出错误提示框
	if (!appNameUtf8_.empty())
	{
		// 构建标题和内容
		std::string title = "插件运行出错：" + appNameUtf8_;
		std::string content = "插件[" + appNameUtf8_ + "]运行时发生C++错误\n\n错误信息：" + std::string(errorMessage ? errorMessage : "Unknown error");

		// 弹出提示框
		MessageBoxA(NULL, content.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
	}
}

	std::string AppInfoBuilder::BuildJsonUtf8() const
	{
		// 取数据（对应：取数据）
		std::string json;
		json.reserve(4096);

		const std::string sdkv = EscapeJsonStringUtf8(sdkvUtf8_);
		const std::string appname = EscapeJsonStringUtf8(appNameUtf8_);
		const std::string author = EscapeJsonStringUtf8(authorUtf8_);
		const std::string appv = EscapeJsonStringUtf8(appvUtf8_);
		const std::string describe = EscapeJsonStringUtf8(describeUtf8_);

		json.append("{");
		json.append("\"sdkv\":\"").append(sdkv).append("\",");
		json.append("\"appname\":\"").append(appname).append("\",");

		json.append("\"author\":\"").append(author).append("\",");
		json.append("\"appv\":\"").append(appv).append("\",");
		json.append("\"describe\":\"").append(describe).append("\",");

		json.append("\"getticketaddres\":").append(std::to_string(static_cast<uint64_t>(getticketaddres_))).append(",");
		json.append("\"getvefcodeaddres\":").append(std::to_string(static_cast<uint64_t>(getvefcodeaddres_))).append(",");
		json.append("\"useproaddres\":").append(std::to_string(static_cast<uint64_t>(useproaddres_))).append(",");
		json.append("\"banproaddres\":").append(std::to_string(static_cast<uint64_t>(banproaddres_))).append(",");
		json.append("\"unitproaddres\":").append(std::to_string(static_cast<uint64_t>(unitproaddres_))).append(",");
		json.append("\"setproaddres\":").append(std::to_string(static_cast<uint64_t>(setproaddres_))).append(",");
		json.append("\"friendmsaddres\":").append(std::to_string(static_cast<uint64_t>(friendmsaddres_))).append(",");
		json.append("\"groupmsaddres\":").append(std::to_string(static_cast<uint64_t>(groupmsaddres_))).append(",");
		json.append("\"ChannelFunc\":").append(std::to_string(static_cast<uint64_t>(ChannelFunc_))).append(",");
		json.append("\"eventmsaddres\":").append(std::to_string(static_cast<uint64_t>(eventmsaddres_))).append(",");
		json.append("\"PmDealFunc\":").append(std::to_string(static_cast<uint64_t>(PmDealFunc_))).append(",");

		json.append("\"data\":{");
		json.append("\"needapilist\":{");

		bool first = true;
		for (const PermissionItem& p : permissions_)
		{
			if (p.apiNameZh.empty())
			{
				continue;
			}
			const std::string key = EscapeJsonStringUtf8(p.apiNameZh);
			const std::string desc = EscapeJsonStringUtf8(p.descUtf8);

			if (!first)
			{
				json.append(",");
			}
			first = false;
			json.append("\"").append(key).append("\":{");
			json.append("\"desc\":\"").append(desc).append("\"");
			json.append("}");
		}

		json.append("}");
		json.append("}");
		json.append("}");
		return json;
	}
}

