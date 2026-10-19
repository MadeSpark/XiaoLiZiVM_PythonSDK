#pragma once

// ============================================================================
// xlz_webui.h —— webui（miniblink / MbKit 内核）接入层
// ----------------------------------------------------------------------------
// 对应易语言模块「MB浏览框」的 C++ 移植：内核一律经 MbKit146.dll 的朴素名
// stdcall 导出（-Wl,--kill-at，无 @N 修饰），用 GetProcAddress 动态解析，
// 不链接任何内核 lib，也不直接触碰 node.dll。
//
// 文本约定：对外一律 UTF-8（与 SDK 整体一致），内部按需转 UTF-16 后走 *W 变体。
//
// ★ 线程铁律（必须遵守）：
//   1. Bind() 必须在「承载窗口的创建线程」上调用（通常 = 宿主 UI 线程）。
//      WebView2(edgewebview) 内核是属主直通模式：view 的属主 = 创建它的线程，
//      在其它线程里 Bind 属于「承载窗口在工作线程」，直接灰屏。
//   2. DllMain（DLL_PROCESS_ATTACH/DETACH）内禁止调用 Bind/WaitKernelReady/
//      DetachAll：DETACH 时持有加载器锁，等内核会死锁（WaitKernel 超时 10~60s），
//      DetachAll 会触发内核回调并强制关消息泵。需要在卸载时做的清理请放在
//      插件卸载回调（AppUninstall/_AppUnload，由宿主在加载锁之外调用）里。
//   3. webui 回调（On* 事件、JsBindFunction 回调）里先检查「正在卸载」标志，
//      正在卸载时直接返回，绝不再访问窗口或 view。
//
// ★ 热重载契约（插件卸载回调里的固定收尾顺序）：
//   (1) 先停掉所有会访问承载窗口的定时器/线程/任务；
//   (2) 再 XlzWebView::Destroy()（逐个销毁自己创建的 view）；
//   (3) 最后全局 xlz::wke::DetachAll()（= WKE全解绑 / MbKit_DetachAll）；
//   (4) 窗口销毁后不要再访问其控件。
//   已知局限：宿主侧连续热重载 5 次在内核侧仍有不稳签名（win32u.dll+0x11cc，
//   WebView2 WM_DESTROY 异步 Release 与窗口销毁竞态），属于内核已知问题，如实标注。
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>

#include "xlz_exports.h"

namespace xlz
{
namespace wke
{
	// ---- 错误码（与 MbKit 返回码对齐 + 本层扩展）----
	enum class WebUiError : int32_t
	{
		Ok = 0,              // 成功（MBKIT_OK）
		Generic = -1,        // 内核通用错误（MBKIT_E_GENERIC）
		NoKernelDll = -2,    // 找不到/加载不了内核（MBKIT_E_NODLL）
		MissingSymbol = -3,  // 内核缺少导出（MBKIT_E_SYMBOL）
		BadParam = -4,       // 参数错误（MBKIT_E_PARAM）
		BadState = -5,       // 状态不对：未初始化/view 已失效（MBKIT_E_STATE）
		HostDllNotFound = -100, // MbKit146.dll 不存在且无可用内核 blob（本层）
		HostDllLoadFailed = -101, // MbKit146.dll 存在但 LoadLibrary 失败（本层）
		ViewNotBound = -102,   // view 尚未创建（本层）
		NotOnOwnerThread = -103 // Bind 不在承载窗口创建线程上（本层，按需使用）
	};

	// 内核模式（MbKit_KernelMode）
	enum class KernelMode : int32_t
	{
		NotLoaded = 0,   // 尚未就绪（或上次启动失败）
		NodeDll = 1,     // node.dll（miniblink）
		EdgeWebView = 2  // edgewebview（WebView2）
	};

	// 内核启动状态（MbKit_BootState）
	enum class BootState : int32_t
	{
		NotStarted = 0,  // 未启动
		Starting = 1,   // 启动中（异步）
		Done = 2,       // 完成
		Failed = 3      // 失败
	};

	// 载入完成结果（wkeOnLoadingFinish 的 result）
	enum class LoadResult : int32_t
	{
		Success = 0, // 成功
		Failed = 1,  // 失败
		Canceled = 2 // 取消
	};

	// ---------------------------------------------------------------------------
	// MbKitLib —— MbKit146.dll 加载单例（复刻易模块 _确保宿主库已载入）
	//
	// 查找顺序：
	//   0) SetKernelBlob() 提供的内存映像（调用方自带内核字节；不落地文件，
	//      直接 LoadLibrary 到进程后按基名命中规则生效）；
	//      或者 SetKernelSourceDir() 指定的旁路目录（例如 SDK 目录旁）。
	//   1) 宿主 exe 目录 \MbKit146.dll
	//   2) %TEMP%\ElangMbKit\MbKit146.dll（自动建目录）
	//   3) 裸名 MbKit146.dll（系统搜索序，含「已被进程载入」的命中）
	// 已被进程按基名载入过时直接复用（Windows 按基名命中，先载入的那份生效）。
	// 载入后校验 MbKit_Version() 版本串：不含 "MbKit/1.4.5" 时记入告警文本，
	// 由 SelfDiag()/VersionWarning() 输出（版本钉住 + 多份告警机制）。
	// ---------------------------------------------------------------------------
	class MbKitLib
	{
	public:
		// 单例入口
		static MbKitLib& Instance();

		// 可选：提供内核 blob（完整 MbKit146.dll 文件内容）。SDK 无法内嵌 blob，
		// 由插件/宿主自己携带并注入。调用它之后 EnsureLoaded 优先用该映像。
		// 返回 false = 参数非法。必须在任何 webui 调用之前设置。
		static bool SetKernelBlob(const void* blobBytes, uint32_t blobLength);

		// 可选：提供旁路目录（例如插件包自带 MbKit146.dll 的目录）。
		// 目录里必须有 MbKit146.dll 才会被采用；找不到自动回退默认顺序。
		static void SetKernelSourceDir(const char* dirUtf8);

		// 确保 MbKit146.dll 已载入（幂等；所有公开命令入口内部都会先调它）。
		// 成功返回 WebUiError::Ok；失败返回明确错误码，绝不空指针调用内核。
		WebUiError EnsureLoaded();

		// 当前是否已载入（不触发加载）
		bool IsLoaded() const;

		// 取导出地址（EnsureLoaded 之后才可用）；失败返回 nullptr。
		// 所有内核导出名必须是 gen_fwd.def 清单里的朴素名（静态核对零容忍）。
		void* GetProc(const char* name);

		// 版本串告警：进程里已载入的 MbKit146.dll 版本串不含 "MbKit/1.4.5" 时，
		// 返回该版本串（多份混用提示）；正常返回空串。
		const std::string& VersionWarning() const;

		// MbKit_Version() 的原始返回（UTF-8 保存），未载入时为空串
		const std::string& LoadedVersion() const;

		// 实际使用的 MbKit146.dll 全路径（UTF-8），未载入时为空串
		const std::string& LoadedPath() const;

		// 最近一次 EnsureLoaded 的失败详情（UTF-8 文本，便于日志）
		const std::string& LastLoadError() const;

	private:
		MbKitLib() = default;
		~MbKitLib() = default;
		MbKitLib(const MbKitLib&) = delete;
		MbKitLib& operator=(const MbKitLib&) = delete;

		struct Impl;
		Impl& ImplRef();
		Impl* impl_ = nullptr;
	};

	// ---------------------------------------------------------------------------
	// XlzWebView —— 与易模块 MB浏览框 核心命令一一对应的视图类
	//
	// 生命周期规则：
	//   - 事件回调 thunk 与本对象同生命周期：析构/Destroy() 时先撤回调再销毁 view；
	//   - 销毁后绝不再进入用户回调（代际计数 + 存活标志双重校验）；
	//   - 每个方法都先 EnsureLoaded + 检查导出存在，出错返回错误码而非崩溃。
	// ---------------------------------------------------------------------------
	class XlzWebView
	{
	public:
		// 文档就绪2 回调参数（wkeOnDocumentReady2）
		struct DocumentReady2Event
		{
			uint64_t WebView;   // wkeWebView 句柄
			uint64_t FrameHandle; // wkeWebFrameHandle
		};

		// 控制台消息回调参数（wkeOnConsole）
		struct ConsoleEvent
		{
			uint64_t WebView;
			int32_t Level;      // WKE消息级别_
			std::string MessageUtf8;
			std::string SourceUtf8; // 来源名
			int32_t Line;       // 行号
			std::string StackUtf8; // 堆栈跟踪
		};

		// 载入完成回调参数（wkeOnLoadingFinish）
		struct LoadingFinishEvent
		{
			uint64_t WebView;
			std::string UrlUtf8;
			LoadResult Result;  // 成功/失败/取消
			std::string FailedReasonUtf8;
		};

		// 标题改变回调参数（wkeOnTitleChanged）
		struct TitleChangedEvent
		{
			uint64_t WebView;
			std::string TitleUtf8;
		};

		// JS 绑定函数回调：入参为执行状态 es，返回 jsValue（长整数）
		using JsBoundFunction = std::function<int64_t(int64_t es)>;

		XlzWebView();
		~XlzWebView();

		XlzWebView(const XlzWebView&) = delete;
		XlzWebView& operator=(const XlzWebView&) = delete;

		// ---- 对应 MB浏览框.绑定：wkeCreateWebView + wkeSetHandle ----
		// hwnd 为承载窗口（必须在承载窗口的创建线程上调用！）；可重复调用
		// （内部先 Destroy 旧 view）。transparent=是否透明。
		WebUiError Bind(void* hwnd, bool transparent = false);

		// ---- 对应 MB浏览框.销毁：wkeSetHandle(0)+wkeDestroyWebView ----
		// 撤销全部事件回调 thunk 后销毁 view；销毁后绝不再进用户回调。
		void Destroy();

		// view 是否仍有效
		bool IsValid() const;

		// ---- 载入族（UTF-8 入参，内部按需转 UTF-16 走 *W 变体）----
		WebUiError LoadUrl(const char* urlUtf8);            // 载入URL → wkeLoadURLW
		WebUiError LoadFile(const char* filePathUtf8);      // 载入文件 → wkeLoadFileW
		WebUiError LoadHtml(const char* htmlUtf8);          // 载入HTML → wkeLoadHTMLW
		WebUiError LoadHtmlWithBaseUrl(const char* htmlUtf8, const char* baseUrlUtf8); // 载入HTML2 → wkeLoadHtmlWithBaseUrl

		// ---- JS 执行 ----
		// 运行JS → wkeRunJSW；返回 jsValue（0 = 失败/未绑定）
		int64_t RunJs(const char* jsUtf8);
		// 运行JS自框架 → wkeRunJsByFrame（可在 事件_文档就绪2 回调里注入）
		int64_t RunJsByFrame(uint64_t frameHandle, const char* jsUtf8, bool inClosure = false);

		// ---- 对应 JS绑定函数：wkeJsBindFunction ----
		// name 为 UTF-8；fn 的生命周期由本对象持有（keep-alive），Destroy 时解除。
		WebUiError JsBindFunction(const char* nameUtf8, JsBoundFunction fn, int32_t argc);

		// ---- 事件（std::function；thunk 与对象同生命周期）----
		WebUiError OnDocumentReady2(std::function<void(const DocumentReady2Event&)> fn);
		WebUiError OnConsole(std::function<void(const ConsoleEvent&)> fn);
		WebUiError OnLoadingFinish(std::function<void(const LoadingFinishEvent&)> fn);
		WebUiError OnTitleChanged(std::function<void(const TitleChangedEvent&)> fn);

		// ---- 次核心便捷命令 ----
		// 取窗口句柄 → wkeGetHostHWND
		void* GetHostWindow() const;
		// 是否文档已就绪 → wkeIsDocumentReady
		bool IsDocumentReady() const;
		// 取URL → wkeGetURL（UTF-8 出）
		std::string GetUrl() const;
		// 取标题 → wkeGetTitleW（UTF-8 出）
		std::string GetTitle() const;
		// 调整大小 → wkeResize
		WebUiError Resize(int32_t width, int32_t height);
		// 取源码 → wkeGetSource（UTF-8 出；node 内核为 UTF-8，edgewebview 内核返回原样字节）
		std::string GetSource() const;
		// 刷新/后退/前进 → wkeReload / wkeGoBack / wkeGoForward
		void Reload();
		bool GoBack();
		bool GoForward();
		// 退出/停止载入 → wkeStopLoading（交付文档速查表里的 mb.退出()）
		void StopLoading();

	// 内部使用：view 句柄（0 = 未绑定）
	uint64_t Handle() const;

	// 不透明实现体（定义在 xlz_webui.cpp；事件 thunk 需要可见此声明）
	struct Impl;
	Impl& ImplRef();
	Impl* impl_ = nullptr;
};

	// ---------------------------------------------------------------------------
	// 全局函数（对应易模块全局程序集的 WKE* 公开命令）
	// ---------------------------------------------------------------------------

	// 对应 WKE设置node路径：MbKit_SetNodePath + MbKit_SetForceNodeKernel。
	// 必须在碰内核（WKE初始化/Bind）之前调用，幂等（同路径返真）。
	// reasonUtf8 可空：失败时输出原因文本（UTF-8 出参，可选）。
	bool SetNodePath(const char* pathOrDirUtf8, std::string* reasonUtf8 = nullptr, bool forceNodeKernel = false);

	// 对应 WKE初始化：wkeIsInitialize/wkeInitialize(Ex)。异步只登记不等待。
	WebUiError Initialize(bool threadRepaint = false);

	// 对应 WKE是否支持WebView2：MbKit_HasWebViewEnv（只读探测）
	bool IsWebView2Supported();

	// 对应 WKE取内核模式：MbKit_KernelMode
	KernelMode GetKernelMode();

	// 对应 WKE取WebView2诊断：MbKit_WebViewDiag（UTF-8 出）
	std::string GetWebView2Diag();

	// 对应 WKE宿主自检：MbKit_Diag + 版本告警头（UTF-8 出；排障第一入口）
	std::string SelfDiag();

	// 对应 WKE等待内核就绪：MbKit_WaitKernel。
	// ⚠ 禁止在 DllMain 里调用（加载锁下会死锁/超时）。
	WebUiError WaitKernelReady(int32_t timeoutMs = 0);

	// 对应 WKE取内核启动状态：MbKit_BootState（0..3）
	BootState GetKernelBootState();

	// ★ 对应 WKE全解绑：MbKit_DetachAll（热重载契约的最后一环）。
	// ⚠ 禁止在 DllMain 里调用；应放在插件卸载回调的最后一步。
	// 返回被清理的 view 数。
	int32_t DetachAll();

	// ---- JS 取参/回值薄封装（在 JsBoundFunction 回调里用；es = 执行状态）----
	int32_t JsArgCount(int64_t es);                    // jsArgCount
	int32_t JsArgType(int64_t es, int32_t index);      // jsArgType（JS类型_ 常量）
	int64_t JsArg(int64_t es, int32_t index);          // jsArg（返回 jsValue）
	int32_t JsArgToInt(int64_t es, int32_t index);     // jsArg + jsToInt
	bool JsArgToBool(int64_t es, int32_t index);       // jsArg + jsToBoolean
	double JsArgToDouble(int64_t es, int32_t index);   // jsArg + jsToDouble
	std::string JsArgToStringUtf8(int64_t es, int32_t index); // jsArg + jsToTempStringW → UTF-8
	int64_t JsStringW(const char* textUtf8);           // jsStringW（文本 → jsValue）
	std::string JsToStringUtf8(int64_t es, int64_t jsValue); // jsToStringW → UTF-8（回调里回值给网页）

	// 设置「正在卸载」标志：webui 回调里应先检查它（卸载期间直接返回）。
	void SetUnloading(bool unloading);
	bool IsUnloading();
}
}
