#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "xlz_permissions.h"
#include "xlz_sdk.h"

namespace xlz
{
	// 应用信息构造器（对应：类模块_应用消息 / 取数据）
	class AppInfoBuilder
	{
	public:
		// 设置SDK版本（对应：sdkv / #SDK版本）
		void SetSdkVersion(const char* sdkVersion);

		// 设置应用名（对应：置应用名）
		void SetAppName(const char* appNameUtf8);

		// 设置应用作者（对应：置应用作者）
		void SetAuthor(const char* authorUtf8);

		// 设置应用版本（对应：置应用版本）
		void SetAppVersion(const char* appVersionUtf8);

		// 设置应用说明（对应：置应用说明）
		void SetDescription(const char* descriptionUtf8);

		// 设置滑块处理函数地址（对应：置滑块处理函数）
		void SetSliderHandlerAddress(uintptr_t address);

		// 设置短信验证码处理函数地址（对应：置短信验证码处理函数）
		void SetSmsCodeHandlerAddress(uintptr_t address);

		// 设置插件被启用处理函数地址（对应：置插件被启用处理函数）
		void SetPluginEnabledAddress(uintptr_t address);

		// 设置插件被禁用处理函数地址（对应：置插件被禁用处理函数）
		void SetPluginDisabledAddress(uintptr_t address);

		// 设置将被卸载处理函数地址（对应：置将被卸载处理函数）
		void SetPluginUninstallAddress(uintptr_t address);

		// 设置点击插件设置处理函数地址（对应：置点击插件设置处理函数）
		void SetPluginSettingAddress(uintptr_t address);

		// 设置私聊消息处理函数地址（对应：置私聊消息处理函数）
		void SetPrivateMessageAddress(uintptr_t address);

		// 设置群消息处理函数地址（对应：置群消息处理函数）
		void SetGroupMessageAddress(uintptr_t address);

		// 设置频道推送统一处理函数地址（对应：置频道推送统一处理函数）
		void SetChannelUnifiedAddress(uintptr_t address);

		// 设置事件消息处理函数地址（对应：置事件消息处理函数）
		void SetEventMessageAddress(uintptr_t address);

		// 设置插件消息输出替换处理函数地址（对应：置插件消息输出替换处理函数）
		void SetOutputReplaceAddress(uintptr_t address);

		// 申请权限（按中文API名，对应：置应用权限申请）
		void RequestPermissionByName(const char* apiChineseName, const char* reasonUtf8);

		// 申请权限（按权限枚举，对应：置应用权限申请）
		void RequestPermission(Permission permission, const char* reasonUtf8);

		// 取数据（返回给框架的json，对应：取数据）
		std::string BuildJsonUtf8() const;

		// 异常回调函数类型
		typedef void (*ExceptionCallback)(const char* pluginName, const char* errorMessage);

		// 设置异常回调函数
		void SetExceptionCallback(ExceptionCallback callback);

		// 触发异常回调
		void TriggerException(const char* errorMessage) const;

	private:
		struct PermissionItem
		{
			std::string apiNameZh;
			std::string descUtf8;
		};

		static std::string EscapeJsonStringUtf8(const std::string& utf8);
		static std::string SanitizeAppNameUtf8(const std::string& utf8);

		std::string sdkvUtf8_ = SDK_VERSION;
		std::string appNameUtf8_;
		std::string authorUtf8_;
		std::string appvUtf8_;
		std::string describeUtf8_;

		uintptr_t getticketaddres_ = 0;
		uintptr_t getvefcodeaddres_ = 0;
		uintptr_t useproaddres_ = 0;
		uintptr_t banproaddres_ = 0;
		uintptr_t unitproaddres_ = 0;
		uintptr_t setproaddres_ = 0;
		uintptr_t friendmsaddres_ = 0;
		uintptr_t groupmsaddres_ = 0;
		uintptr_t ChannelFunc_ = 0;
		uintptr_t eventmsaddres_ = 0;
		uintptr_t PmDealFunc_ = 0;

		ExceptionCallback exceptionCallback_ = nullptr;

		std::vector<PermissionItem> permissions_;
	};
}

