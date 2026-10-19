#define XLZSDK_EXPORTS

#include "xlz_appinfo.h"
#include "xlz_api_wrappers.h"
#include "xlz_constants.h"
#include "xlz_encoding.h"
#include "xlz_event_utf8.h"
#include "xlz_events.h"
#include "xlz_exports.h"
#include "xlz_sdk.h"
#include "xlz_webui.h"
#include "xlz_zh_names_utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstring>
#include <string>

// ============================================================================
// Demo_WebUI —— webui（MbKit 内核）示例插件
//
// 演示内容：
//   1. 启用回调里创建承载窗口并绑定 WebView（线程铁律：Bind 必须在
//      承载窗口的创建线程上调用，本例即宿主调用 RotbotAppEnable 的线程）；
//   2. LoadHtml 载入本地页面 + JsBindFunction 注册网页可调用的原生函数；
//   3. 四类事件回调（文档就绪 / 控制台 / 载入完成 / 标题改变）；
//   4. ★ 热重载契约：AppUninstall 里的标准卸载顺序——
//      先停一切会碰窗口的定时器/线程 → Destroy 视图 → 最后 DetachAll。
// ============================================================================

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

// ---------------------------------------------------------------------------
// webui 状态
// ---------------------------------------------------------------------------
static xlz::wke::XlzWebView g_webView;      // WebView 实例（全局唯一）
static std::atomic<bool> g_webuiActive{ false }; // webui 是否在用（卸载期间先置假）
static std::atomic<bool> g_uninstalling{ false };

// 承载窗口（简单 Win32 窗口，窗口过程只做默认处理）
static const wchar_t* kWebHostClassName = L"XlzWebUIDemoHost";
static HWND g_webHostWnd = nullptr;
static const int kWebHostW = 900;
static const int kWebHostH = 600;

static LRESULT CALLBACK WebHostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	// 极简窗口过程：尺寸变化时同步 WebView 大小，其余交给系统
	if (msg == WM_SIZE && g_webuiActive.load() && g_webView.IsValid())
	{
		RECT rc = {};
		::GetClientRect(hwnd, &rc);
		g_webView.Resize(rc.right - rc.left, rc.bottom - rc.top);
		return 0;
	}
	if (msg == WM_DESTROY)
	{
		g_webHostWnd = nullptr;
		return 0;
	}
	return ::DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// JS 绑定函数：网页里 window.__xlzNative("hello") 会进到这里
// ---------------------------------------------------------------------------
static int64_t XlzNativeFunction(int64_t es)
{
	// ★ 卸载期间直接返回：契约要求（不能在卸载后还进用户回调）
	if (g_uninstalling.load() || !g_webuiActive.load())
	{
		return 0;
	}
	const int32_t argc = xlz::wke::JsArgCount(es);
	std::string argText;
	if (argc > 0)
	{
		argText = xlz::wke::JsArgToStringUtf8(es, 0);
	}
	const std::string log = "webui js __xlzNative called, argc=" + std::to_string(argc) +
		" arg0=" + argText;
	xlz::OutputLog(g_sdk, log.c_str());

	// 回一个字符串给网页（jsStringW：文本 → jsValue）
	return xlz::wke::JsStringW("received by native plugin");
}

// ---------------------------------------------------------------------------
// 事件回调
// ---------------------------------------------------------------------------
static void OnDocReady(const xlz::wke::XlzWebView::DocumentReady2Event& ev)
{
	if (g_uninstalling.load()) return;
	xlz::OutputLog(g_sdk, "webui document ready");
	// 文档就绪后注入一段 JS（演示 RunJs）
	(void)ev;
}

static void OnConsoleMsg(const xlz::wke::XlzWebView::ConsoleEvent& ev)
{
	if (g_uninstalling.load()) return;
	const std::string log = "webui console [" + std::to_string(ev.Level) + "] " + ev.MessageUtf8;
	xlz::OutputLog(g_sdk, log.c_str());
}

static void OnLoadingFinish(const xlz::wke::XlzWebView::LoadingFinishEvent& ev)
{
	if (g_uninstalling.load()) return;
	const std::string log = "webui load finish: " + ev.UrlUtf8 +
		" result=" + std::to_string(static_cast<int>(ev.Result));
	xlz::OutputLog(g_sdk, log.c_str());
}

static void OnTitleChanged(const xlz::wke::XlzWebView::TitleChangedEvent& ev)
{
	if (g_uninstalling.load()) return;
	const std::string log = "webui title: " + ev.TitleUtf8;
	xlz::OutputLog(g_sdk, log.c_str());
}

// ---------------------------------------------------------------------------
// 启用回调：建窗口 → 绑定 → 载页面（全部在当前线程 = 宿主调用线程）
// ---------------------------------------------------------------------------
static bool StartWebUi()
{
	// 1) 预载宿主库（可提前拿到版本告警；不强制）
	if (xlz::wke::MbKitLib::Instance().EnsureLoaded() != xlz::wke::WebUiError::Ok)
	{
		const std::string err = "MbKitLib load failed: " + xlz::wke::MbKitLib::Instance().LastLoadError();
		xlz::OutputLog(g_sdk, err.c_str());
		return false;
	}
	if (!xlz::wke::MbKitLib::Instance().VersionWarning().empty())
	{
		const std::string warn = "MbKit version warning: " + xlz::wke::MbKitLib::Instance().VersionWarning();
		xlz::OutputLog(g_sdk, warn.c_str());
	}

	// 2) 注册窗口类 + 创建承载窗口（★ 当前线程就是 Bind 允许的线程）
	WNDCLASSW wc = {};
	wc.lpfnWndProc = WebHostWndProc;
	wc.hInstance = ::GetModuleHandleW(nullptr);
	wc.lpszClassName = kWebHostClassName;
	wc.hCursor = ::LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
	::RegisterClassW(&wc);

	g_webHostWnd = ::CreateWindowExW(0, kWebHostClassName, L"XiaoLiZi WebUI Demo",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, kWebHostW, kWebHostH,
		nullptr, nullptr, wc.hInstance, nullptr);
	if (g_webHostWnd == nullptr)
	{
		xlz::OutputLog(g_sdk, "create host window failed");
		return false;
	}

	// 3) 绑定 WebView（线程铁律在此：必须在窗口创建线程上）
	const xlz::wke::WebUiError bindErr = g_webView.Bind(g_webHostWnd, false);
	if (bindErr != xlz::wke::WebUiError::Ok)
	{
		const std::string err = "webview bind failed, err=" + std::to_string(static_cast<int>(bindErr));
		xlz::OutputLog(g_sdk, err.c_str());
		::DestroyWindow(g_webHostWnd);
		return false;
	}

	// 4) 事件 + JS 绑定
	g_webView.OnDocumentReady2(OnDocReady);
	g_webView.OnConsole(OnConsoleMsg);
	g_webView.OnLoadingFinish(OnLoadingFinish);
	g_webView.OnTitleChanged(OnTitleChanged);
	g_webView.JsBindFunction("__xlzNative", XlzNativeFunction, 1);

	// 5) 等内核就绪（演示用；宿主 UI 线程里等是有消息泵的，安全）
	const xlz::wke::WebUiError waitErr = xlz::wke::WaitKernelReady(20000);
	if (waitErr != xlz::wke::WebUiError::Ok)
	{
		const std::string err = "wait kernel failed, err=" + std::to_string(static_cast<int>(waitErr));
		xlz::OutputLog(g_sdk, err.c_str());
	}

	// 6) 载入演示页面（本地 HTML：按钮调原生函数）
	static const char* kDemoHtml =
		"<!DOCTYPE html><html><head><meta charset='utf-8'>"
		"<title>XLZ WebUI Demo</title></head>"
		"<body style='font-family:sans-serif'>"
		"<h3>XiaoLiZi C++ SDK webui demo</h3>"
		"<button onclick='go()'>call native</button>"
		"<div id='r'>...</div>"
		"<script>function go(){"
		"  __xlzNative('hello from web').then(function(v){"
		"    document.getElementById('r').textContent = String(v);"
		"  });"
		"}</script></body></html>";
	g_webView.LoadHtmlWithBaseUrl(kDemoHtml, "http://xlz.demo/");

	::ShowWindow(g_webHostWnd, SW_SHOW);
	g_webuiActive.store(true);

	// 排障第一入口：一行式自检（含内核模式/路径/版本）
	const std::string diag = xlz::wke::SelfDiag();
	xlz::OutputLog(g_sdk, diag.c_str());
	return true;
}

// ---------------------------------------------------------------------------
// ★ 热重载契约：标准卸载顺序
// ---------------------------------------------------------------------------
static void StopWebUi()
{
	// 1) 先置「正在卸载」：所有 webui 回调自此直接返回
	xlz::wke::SetUnloading(true);
	g_uninstalling.store(true);
	g_webuiActive.store(false);

	// 2) 停掉一切会碰窗口的东西（本例没有定时器/工作线程，此处为模板位置；
	//    有定时器就 KillTimer，有线程就先发停止信号再 Join）

	// 3) 销毁视图（先撤内核回调 thunk，再销毁 view——SDK 内部已按序处理）
	g_webView.Destroy();

	// 4) 销毁承载窗口（此后绝不再访问已销毁窗口的任何控件）
	if (g_webHostWnd != nullptr)
	{
		::DestroyWindow(g_webHostWnd); // 窗口过程里已把句柄清空
	}

	// 5) 最后一步：全解绑（清理本进程全部 view；内核保持已初始化）
	const int32_t cleaned = xlz::wke::DetachAll();
	const std::string log = "webui detached, cleaned=" + std::to_string(cleaned);
	xlz::OutputLog(g_sdk, log.c_str());

	// 6) 解除卸载标志（下次启用还能用）
	xlz::wke::SetUnloading(false);
	g_uninstalling.store(false);
}

// ---------------------------------------------------------------------------
// 框架回调导出
// ---------------------------------------------------------------------------
static int XLZ_CALL HandleGroupMessage(const xlz::GroupMessageEvent* ev)
{
	if (ev == nullptr)
	{
		return (int)xlz::MessageHandle::Ignore;
	}
	xlz::ProcessEvents();
	return (int)xlz::MessageHandle::Ignore;
}

static int XLZ_CALL HandlePrivateMessage(const xlz::PrivateMessageEvent* ev)
{
	if (ev == nullptr)
	{
		return (int)xlz::MessageHandle::Ignore;
	}
	xlz::ProcessEvents();
	return (int)xlz::MessageHandle::Ignore;
}

static int XLZ_CALL HandleEventMessage(const xlz::EventTypeBase* ev)
{
	if (ev == nullptr)
	{
		return (int)xlz::MessageHandle::Ignore;
	}
	xlz::ProcessEvents();
	return (int)xlz::MessageHandle::Ignore;
}

XLZ_API int XLZ_CALL RecviceGroupMesg(void* data)
{
	return HandleGroupMessage(reinterpret_cast<const xlz::GroupMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecvicePrivateMsg(void* data)
{
	return HandlePrivateMessage(reinterpret_cast<const xlz::PrivateMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecviceEventCallBack(void* data)
{
	return HandleEventMessage(reinterpret_cast<const xlz::EventTypeBase*>(data));
}

XLZ_API int XLZ_CALL RotbotAppEnable()
{
	// 插件启用：启动 webui（在宿主调用线程 = 承载窗口创建线程）
	if (!StartWebUi())
	{
		// 启动失败不拒绝启用（webui 是可选能力），只记日志
		xlz::OutputLog(g_sdk, "webui start failed, plugin continues without webui");
	}
	xlz::OutputLog(g_sdk, "Plugin enabled (webui demo)");
	return (int)xlz::EnableResponse::Done;
}

XLZ_API int XLZ_CALL AppSetting()
{
	return 0;
}

XLZ_API void XLZ_CALL AppUninstall()
{
	// 插件卸载：执行热重载契约
	StopWebUi();
}

XLZ_API void XLZ_CALL AppDisabled()
{
	// 插件禁用：同样按契约清理（禁用后可能还会重载）
	StopWebUi();
}

XLZ_API void XLZ_CALL GetSMSVerificationCode(int64_t sourceQQ, void* phone)
{
	(void)sourceQQ;
	(void)phone;
}

XLZ_API void XLZ_CALL SliderRecognition(int64_t sourceQQ, void* url)
{
	(void)sourceQQ;
	(void)url;
}

XLZ_API const char* XLZ_CALL apprun(const char* pluginkey, const char* apidata)
{
	// 插件入口
	const auto looksLikeApiData = [](const char* s) -> bool
	{
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

	xlz::OutputLog(g_sdk, "apprun init (webui demo)");

	xlz::AppInfoBuilder info;
	info.SetAppName("XiaoLiZi C++ SDK WebUI Demo");
	info.SetAuthor("C++ SDK");
	info.SetAppVersion("1.0.0");
	info.SetDescription("webui (MbKit kernel) demo plugin");

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
