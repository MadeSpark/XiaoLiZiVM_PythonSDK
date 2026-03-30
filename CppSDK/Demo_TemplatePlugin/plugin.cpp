#define XLZSDK_EXPORTS

#include "xlz_appinfo.h"
#include "xlz_api_wrappers.h"
#include "xlz_encoding.h"
#include "xlz_event_utf8.h"
#include "xlz_events.h"
#include "xlz_exports.h"
#include "xlz_sdk.h"
#include "xlz_zh_names_utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <string>

static xlz::Sdk g_sdk;
static std::string g_appInfoJsonAcp;

static std::string Utf8ToAcpLossy(const std::string& utf8)
{
	// UTF-8转ANSI（用于返回给框架的apprun字符串）
	const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	if (wlen <= 0)
	{
		return std::string();
	}
	std::wstring ws;
	ws.resize(static_cast<size_t>(wlen));
	::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), ws.data(), wlen);

	const int blen = ::WideCharToMultiByte(CP_ACP, 0, ws.data(), wlen, nullptr, 0, nullptr, nullptr);
	if (blen <= 0)
	{
		return std::string();
	}
	std::string out;
	out.resize(static_cast<size_t>(blen));
	::WideCharToMultiByte(CP_ACP, 0, ws.data(), wlen, out.data(), blen, nullptr, nullptr);
	return out;
}

static int XLZ_CALL HandleGroupMessage(const xlz::GroupMessageEvent* ev)
{
	// 群消息处理（对应：置群消息处理函数 / 群聊回调）
	if (ev == nullptr)
	{
		return 0;
	}
	const std::string contentUtf8 = xlz::GetGroupMessageContentUtf8(*ev);
	const std::string log = "RecvGroupMsg | Group=" + std::to_string(static_cast<long long>(ev->MessageGroupQQ)) +
		" | Sender=" + std::to_string(static_cast<long long>(ev->SenderQQ)) +
		" | Content=" + contentUtf8;
	xlz::OutputLog(g_sdk, log.c_str());
	xlz::ProcessEvents();
	return 0;
}

static int XLZ_CALL HandlePrivateMessage(const xlz::PrivateMessageEvent* ev)
{
	// 私聊消息处理（对应：置私聊消息处理函数 / 私聊回调）
	if (ev == nullptr)
	{
		return 0;
	}
	xlz::ProcessEvents();
	return 0;
}

static int XLZ_CALL HandleEventMessage(const xlz::EventTypeBase* ev)
{
	// 事件消息处理（对应：置事件消息处理函数 / 事件回调）
	if (ev == nullptr)
	{
		return 0;
	}
	xlz::ProcessEvents();
	return 0;
}

XLZ_API int XLZ_CALL RecviceGroupMesg(void* data)
{
	// 接收群消息（对应：RecviceGroupMesg）
	return HandleGroupMessage(reinterpret_cast<const xlz::GroupMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecvicePrivateMsg(void* data)
{
	// 接收私聊消息（对应：RecvicePrivateMsg）
	return HandlePrivateMessage(reinterpret_cast<const xlz::PrivateMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecviceEventCallBack(void* data)
{
	// 接收事件消息（对应：RecviceEventCallBack）
	return HandleEventMessage(reinterpret_cast<const xlz::EventTypeBase*>(data));
}

XLZ_API int XLZ_CALL RotbotAppEnable()
{
	// 插件启用回调（对应：RotbotAppEnable）
	xlz::OutputLog(g_sdk, "Plugin enabled");
	return 0;
}

XLZ_API int XLZ_CALL AppSetting()
{
	// 点击插件设置（对应：AppSetting）
	return 0;
}

XLZ_API void XLZ_CALL AppUninstall()
{
	// 插件卸载（对应：AppUninstall）
}

XLZ_API void XLZ_CALL AppDisabled()
{
	// 插件禁用（对应：AppDisabled）
}

XLZ_API void XLZ_CALL GetSMSVerificationCode(int64_t sourceQQ, void* phone)
{
	// 短信验证码回调（对应：GetSMSVerificationCode）
	(void)sourceQQ;
	(void)phone;
}

XLZ_API void XLZ_CALL SliderRecognition(int64_t sourceQQ, void* url)
{
	// 滑块验证回调（对应：SliderRecognition）
	(void)sourceQQ;
	(void)url;
}

XLZ_API const char* XLZ_CALL apprun(const char* pluginkey, const char* apidata)
{
	// 插件入口（对应：apprun）
	const auto looksLikeApiData = [](const char* s) -> bool
	{
		// 判断入参是否为apidata（包含大量“API名->地址”的JSON文本）
		if (s == nullptr)
		{
			return false;
		}
		if (std::strchr(s, '{') == nullptr || std::strchr(s, ':') == nullptr)
		{
			return false;
		}
		if (std::strstr(s, "output") != nullptr)
		{
			return false;
		}
		return true;
	};

	const char* apiDataJson = nullptr;
	const char* pluginKey = nullptr;
	const size_t len1 = pluginkey ? std::strlen(pluginkey) : 0;
	const size_t len2 = apidata ? std::strlen(apidata) : 0;
	if (!looksLikeApiData(pluginkey) && looksLikeApiData(apidata))
	{
		pluginKey = pluginkey;
		apiDataJson = apidata;
	}
	else if (looksLikeApiData(pluginkey) && !looksLikeApiData(apidata))
	{
		pluginKey = apidata;
		apiDataJson = pluginkey;
	}
	else if (len2 >= len1)
	{
		pluginKey = pluginkey;
		apiDataJson = apidata;
	}
	else
	{
		pluginKey = apidata;
		apiDataJson = pluginkey;
	}
	g_sdk.Initialize(apiDataJson, pluginKey);

	xlz::OutputLog(g_sdk, "apprun init");

	xlz::AppInfoBuilder info;
	info.SetAppName("XiaoLiZi C++ SDK Template");
	info.SetAuthor("C++ SDK");
	info.SetAppVersion("1.0.0");
	info.SetDescription("C++ Win32 SDK Template");

	info.SetPluginEnabledAddress(reinterpret_cast<uintptr_t>(&RotbotAppEnable));
	info.SetPrivateMessageAddress(reinterpret_cast<uintptr_t>(&RecvicePrivateMsg));
	info.SetGroupMessageAddress(reinterpret_cast<uintptr_t>(&RecviceGroupMesg));
	info.SetEventMessageAddress(reinterpret_cast<uintptr_t>(&RecviceEventCallBack));
	info.SetPluginSettingAddress(reinterpret_cast<uintptr_t>(&AppSetting));
	info.SetPluginUninstallAddress(reinterpret_cast<uintptr_t>(&AppUninstall));
	info.SetPluginDisabledAddress(reinterpret_cast<uintptr_t>(&AppDisabled));
	info.SetSmsCodeHandlerAddress(reinterpret_cast<uintptr_t>(&GetSMSVerificationCode));
	info.SetSliderHandlerAddress(reinterpret_cast<uintptr_t>(&SliderRecognition));

	info.RequestPermissionByName(xlz::kApiName_OutputLog_Utf8, "debug log");
	info.RequestPermissionByName(xlz::kApiName_SendFriendMessage_Utf8, "send private message");
	info.RequestPermissionByName(xlz::kApiName_SendGroupMessage_Utf8, "send group message");

	const std::string jsonUtf8 = info.BuildJsonUtf8();
	g_appInfoJsonAcp = Utf8ToAcpLossy(jsonUtf8);
	return g_appInfoJsonAcp.c_str();
}

