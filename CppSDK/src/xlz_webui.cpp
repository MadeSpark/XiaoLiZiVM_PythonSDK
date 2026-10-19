#include "xlz_webui.h"

// ============================================================================
// xlz_webui.cpp —— webui（miniblink / MbKit 内核）接入层实现
// ----------------------------------------------------------------------------
// 所有内核调用都经 MbKitLib::EnsureLoaded() + GetProc() 动态解析（朴素名 stdcall），
// 找不到导出返回错误码，绝不空指针调用。
// 本文件不包含任何 DllMain；卸载收尾请放在插件的 AppUninstall/_AppUnload 里。
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

namespace xlz
{
namespace wke
{

// ---------------------------------------------------------------------------
// 内部工具：UTF-8 ↔ UTF-16
// ---------------------------------------------------------------------------
static std::wstring Utf8ToWide(const char* utf8)
{
	// UTF-8 转 UTF-16（调内核 *W 变体前）
	if (utf8 == nullptr)
	{
		return std::wstring();
	}
	const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (wlen <= 0)
	{
		return std::wstring();
	}
	std::wstring ws;
	ws.resize(static_cast<size_t>(wlen - 1));
	::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, ws.data(), wlen);
	return ws;
}

static std::string WideToUtf8(const wchar_t* wide)
{
	// UTF-16 转 UTF-8（内核返回文本后）
	if (wide == nullptr)
	{
		return std::string();
	}
	const int blen = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
	if (blen <= 0)
	{
		return std::string();
	}
	std::string out;
	out.resize(static_cast<size_t>(blen - 1));
	::WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), blen, nullptr, nullptr);
	return out;
}

static std::string AnsiToUtf8(const char* ansi)
{
	// ANSI(GBK) 转 UTF-8（MbKit_Diag 等返回的 ANSI 文本）
	if (ansi == nullptr)
	{
		return std::string();
	}
	const int wlen = ::MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
	if (wlen <= 0)
	{
		return std::string();
	}
	std::wstring ws;
	ws.resize(static_cast<size_t>(wlen - 1));
	::MultiByteToWideChar(CP_ACP, 0, ansi, -1, ws.data(), wlen);
	return WideToUtf8(ws.c_str());
}

static std::string PtrToUtf8(const char* p, bool maybeAnsi)
{
	// 内核返回的文本指针 → UTF-8（诊断类为 ANSI；字符串类按 *W 变体直接拿 UTF-16）
	if (p == nullptr)
	{
		return std::string();
	}
	return maybeAnsi ? AnsiToUtf8(p) : std::string(p);
}

// ---------------------------------------------------------------------------
// 「正在卸载」标志：所有 webui 回调 thunk 进入时先检查
// ---------------------------------------------------------------------------
static std::atomic<bool> g_unloading(false);

void SetUnloading(bool unloading)
{
	// 设置/清除「正在卸载」标志（插件卸载回调开头置真，收尾完成后可复位）
	g_unloading.store(unloading);
}

bool IsUnloading()
{
	// 查询「正在卸载」标志（回调里先检查，卸载期间直接返回）
	return g_unloading.load();
}

// ---------------------------------------------------------------------------
// 内核导出函数指针类型（全部 __stdcall 朴素名；与 mbkit_api.h 契约对齐）
// ---------------------------------------------------------------------------
typedef int          (__stdcall* Fn_Int_v)();                       // int f(void)
typedef const char*  (__stdcall* Fn_CStr_v)();                      // const char* f(void)
typedef int          (__stdcall* Fn_Int_i)(int);                    // int f(int)
typedef int          (__stdcall* Fn_Int_pc)(const char*);           // int f(const char*)
typedef void*        (__stdcall* Fn_Ptr_pv)(void*);                 // void* f(void*)
typedef int          (__stdcall* Fn_Int_pv)(void*);                 // int f(void*)
typedef int          (__stdcall* Fn_Int_pw)(void*, const wchar_t*); // int f(view, const wchar_t*)
typedef long long    (__stdcall* Fn_I64_pw)(void*, const wchar_t*); // jsValue f(view, w)
typedef long long    (__stdcall* Fn_I64_pi_pw)(void*, int64_t, const wchar_t*);
typedef long long    (__stdcall* Fn_I64_pi_pc_b)(void*, int64_t, const char*, unsigned char); // wkeRunJsByFrame(view, frame, utf8, closure)
typedef void         (__stdcall* Fn_v_pw)(void*, const wchar_t*);   // void f(view, w)（备用）
typedef void         (__stdcall* Fn_v_p_pF_pv)(void*, void* cb, void* user); // wkeOnXXX(view, cb, user)
typedef void         (__stdcall* Fn_v_pc_pF_pv_i)(const char*, void* cb, void* user, int argc); // wkeJsBindFunction
// —— js* 直通族说明 ——
// Win32(x86) 上 jsExecState 是 4 字节指针、jsValue 是 8 字节整数（wke 用 long long 语义）。
// 本层对外把 es 归一为 int64_t（Win32 下高 32 位恒 0），内部强转回指针调用。
typedef int32_t      (__stdcall* Fn_I32_ll)(int64_t);               // jsArgCount(es)
typedef int32_t      (__stdcall* Fn_I32_ll_i)(int64_t, int32_t);    // jsArgType/jsToInt...
typedef int64_t      (__stdcall* Fn_I64_ll_i)(int64_t, int32_t);    // jsArg(es, idx)
typedef const wchar_t* (__stdcall* Fn_WC_ll_I64)(int64_t, int64_t); // jsToTempStringW/jsToStringW
typedef int64_t      (__stdcall* Fn_I64_ll_wc)(int64_t, const wchar_t*); // jsStringW(es, w)
typedef void*        (__stdcall* Fn_Ptr_v)();                       // wkeCreateWebView()

// ---------------------------------------------------------------------------
// MbKitLib 实现
// ---------------------------------------------------------------------------
struct MbKitLib::Impl
{
	HMODULE module = nullptr;          // MbKit146.dll 句柄（进程内常驻，不 Free）
	std::mutex loadMutex;              // EnsureLoaded 串行化
	std::string versionWarning;        // 版本串不含 MbKit/1.4.5 时的告警文本
	std::string loadedVersion;         // MbKit_Version() 原始串（UTF-8）
	std::string loadedPath;            // 实际使用的 DLL 全路径（UTF-8）
	std::string lastLoadError;         // 最近一次失败详情
	bool loadAttempted = false;        // 是否已经试过一次（失败结果也要缓存，避免反复碰磁盘）
	std::mutex procMutex;              // GetProc 缓存表锁
	std::string procCacheNames[64];    // 简单定长缓存（名字）
	void* procCacheAddrs[64] = {};     // 与上者一一对应
};

MbKitLib& MbKitLib::Instance()
{
	// 单例入口（C++11 静态局部初始化线程安全）
	static MbKitLib s_instance;
	static Impl s_impl;
	s_instance.impl_ = &s_impl;
	return s_instance;
}

MbKitLib::Impl& MbKitLib::ImplRef()
{
	return *impl_;
}

// 静态注入点：内核 blob 与旁路目录（SDK 无法内嵌 blob，由外部提供）
static void* g_kernelBlobBytes = nullptr;   // 内核 blob（完整 DLL 文件内容）
static uint32_t g_kernelBlobLength = 0;
static std::wstring g_kernelSourceDir;      // 旁路目录（含 MbKit146.dll 才生效）

bool MbKitLib::SetKernelBlob(const void* blobBytes, uint32_t blobLength)
{
	// 提供内核 blob：必须是完整的 MbKit146.dll 文件映像（723,456 B，MbKit/1.4.5）
	if (blobBytes == nullptr || blobLength < 1024)
	{
		return false;
	}
	g_kernelBlobBytes = const_cast<void*>(blobBytes);
	g_kernelBlobLength = blobLength;
	return true;
}

void MbKitLib::SetKernelSourceDir(const char* dirUtf8)
{
	// 提供旁路目录（例如插件包自带 MbKit146.dll 的目录；UTF-8 入参）
	g_kernelSourceDir = Utf8ToWide(dirUtf8);
}

static bool FileMatchesBytes(const std::wstring& path, const void* bytes, uint32_t length)
{
	// 逐字节内容比对（不能只比大小：历史事故=同尺寸旧 DLL 覆盖新内核导致 WebView2 永不初始化）
	HANDLE f = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	bool same = false;
	if (::GetFileSize(f, nullptr) == static_cast<DWORD>(length))
	{
		const unsigned char* want = static_cast<const unsigned char*>(bytes);
		unsigned char buf[8192];
		DWORD done = 0;
		uint64_t off = 0;
		same = true;
		while (off < length)
		{
			const DWORD wantRead = static_cast<DWORD>((length - off < sizeof(buf)) ? (length - off) : sizeof(buf));
			if (!::ReadFile(f, buf, wantRead, &done, nullptr) || done != wantRead)
			{
				same = false;
				break;
			}
			if (std::memcmp(buf, want + off, wantRead) != 0)
			{
				same = false;
				break;
			}
			off += wantRead;
		}
	}
	::CloseHandle(f);
	return same;
}

static bool WriteBlobToFile(const std::wstring& path, const void* bytes, uint32_t length)
{
	// 把内核 blob 整文件写出（写失败但文件已存在时由调用方决定是否继续用旧文件）
	HANDLE f = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	DWORD written = 0;
	const bool ok = ::WriteFile(f, bytes, length, &written, nullptr) && written == length;
	::CloseHandle(f);
	return ok;
}

static void SetHidden(const std::wstring& path)
{
	// 释放到宿主目录里的 MbKit146.dll 置为隐藏，免得被当垃圾文件删掉
	const DWORD attr = ::GetFileAttributesW(path.c_str());
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_HIDDEN) == 0)
	{
		::SetFileAttributesW(path.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
	}
}

static std::wstring TryDirForHostLib(const std::wstring& dir)
{
	// 在目录里定位/释放 MbKit146.dll：返回可用的完整路径，失败返回空串。
	// 逻辑复刻易模块 _尝试从目录取宿主库 + _释放宿主库文件。
	if (dir.empty())
	{
		return std::wstring();
	}
	std::wstring path = dir;
	if (!path.empty() && path[path.size() - 1] != L'\\')
	{
		path += L'\\';
	}
	path += L"MbKit146.dll";

	if (g_kernelBlobBytes != nullptr && g_kernelBlobLength > 0)
	{
		// 有 blob：内容比对，不一致或不存在 → 整文件写出；
		// 写失败但文件存在（宿主已锁旧版）→ 继续用已载入那份，不让绑定整段失败。
		bool ok = FileMatchesBytes(path, g_kernelBlobBytes, g_kernelBlobLength);
		if (!ok)
		{
			ok = WriteBlobToFile(path, g_kernelBlobBytes, g_kernelBlobLength);
		}
		if (!ok && ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			ok = true; // 宿主 exe 可能已先载入并锁住旧 DLL：文件在就继续
		}
		if (!ok)
		{
			return std::wstring();
		}
		SetHidden(path);
	}
	else
	{
		// 无 blob：只要求文件存在（找不到时返回空，由调用方走下一个候选位）
		if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			return std::wstring();
		}
	}
	return path;
}

WebUiError MbKitLib::EnsureLoaded()
{
	// 确保宿主库已载入（幂等；对应易模块 _确保宿主库已载入）
	Impl& impl = ImplRef();
	std::lock_guard<std::mutex> lock(impl.loadMutex);
	if (impl.module != nullptr)
	{
		return WebUiError::Ok;
	}
	if (impl.loadAttempted)
	{
		// 已失败过一次：直接返回上次结果（避免每次公开命令都碰磁盘）
		return WebUiError::HostDllNotFound;
	}

	// 候选路径顺序（复刻易模块 _取宿主库文件路径 + 本层扩展）：
	std::wstring chosen;

	// 0a) 旁路目录（SetKernelSourceDir 指定，例如 SDK 目录旁）
	if (!g_kernelSourceDir.empty())
	{
		chosen = TryDirForHostLib(g_kernelSourceDir);
	}
	// 0b) 宿主 exe 目录 \MbKit146.dll
	if (chosen.empty())
	{
		wchar_t exeDir[MAX_PATH] = {};
		if (::GetModuleFileNameW(nullptr, exeDir, MAX_PATH) > 0)
		{
			wchar_t* slash = std::wcsrchr(exeDir, L'\\');
			if (slash != nullptr)
			{
				*slash = L'\0';
				chosen = TryDirForHostLib(exeDir);
			}
		}
	}
	// 1) %TEMP%\ElangMbKit\MbKit146.dll（自动建目录）
	if (chosen.empty())
	{
		wchar_t tempDir[MAX_PATH] = {};
		if (::GetTempPathW(MAX_PATH, tempDir) > 0)
		{
			std::wstring sub = tempDir;
			if (!sub.empty() && sub[sub.size() - 1] != L'\\')
			{
				sub += L'\\';
			}
			sub += L"ElangMbKit";
			::CreateDirectoryW(sub.c_str(), nullptr);
			chosen = TryDirForHostLib(sub);
		}
	}
	// 2) 裸名（系统搜索序；Windows 按基名命中已载入模块，先载入的那份生效）
	if (chosen.empty())
	{
		HMODULE already = ::GetModuleHandleW(L"MbKit146.dll");
		if (already != nullptr)
		{
			// 进程里已有一份（别的插件/宿主先载入的）：直接复用，不重复加载
			impl.module = already;
			chosen = L"(already loaded by base name)";
		}
		else
		{
			impl.loadAttempted = true;
			impl.lastLoadError = "MbKit146.dll not found (kernel dir / host exe dir / %TEMP%\\ElangMbKit\\ / search path); "
				"provide it via SetKernelBlob()/SetKernelSourceDir() or deploy it next to the host exe";
			return WebUiError::HostDllNotFound;
		}
	}

	if (impl.module == nullptr)
	{
		// LoadLibrary（Windows 按基名命中：路径不同但已载入时会复用同一份）
		impl.module = ::LoadLibraryW(chosen.c_str());
		if (impl.module == nullptr)
		{
			impl.loadAttempted = true;
			std::ostringstream oss;
			oss << "LoadLibraryW failed, GetLastError=" << ::GetLastError() << ", path=" << WideToUtf8(chosen.c_str());
			impl.lastLoadError = oss.str();
			return WebUiError::HostDllLoadFailed;
		}
	}

	impl.loadedPath = WideToUtf8(chosen.c_str());

	// 版本串校验（版本钉住 + 多份告警）：不含 "MbKit/1.4.5" 记入告警
	impl.versionWarning.clear();
	impl.loadedVersion.clear();
	Fn_CStr_v fnVersion = reinterpret_cast<Fn_CStr_v>(::GetProcAddress(impl.module, "MbKit_Version"));
	if (fnVersion != nullptr)
	{
		const char* ver = fnVersion();
		if (ver != nullptr)
		{
			// MbKit_Version 返回 ANSI 文本
			impl.loadedVersion = AnsiToUtf8(ver);
			if (std::strstr(ver, "MbKit/1.4.5") == nullptr)
			{
				impl.versionWarning = "Warning: another MbKit146.dll is loaded in this process, version string is \""
					+ impl.loadedVersion + "\" while this SDK expects MbKit/1.4.5. "
					"When multiple plugins each carry a copy, only the FIRST loaded one takes effect; "
					"please unify the SDK/webui version across plugins.";
			}
		}
	}
	return WebUiError::Ok;
}

bool MbKitLib::IsLoaded() const
{
	// 当前是否已载入（不触发加载）
	return impl_ != nullptr && impl_->module != nullptr;
}

void* MbKitLib::GetProc(const char* name)
{
	// 取导出地址（带小缓存）；所有名字必须出现在 gen_fwd.def 导出清单里
	Impl& impl = ImplRef();
	if (impl.module == nullptr || name == nullptr)
	{
		return nullptr;
	}
	{
		std::lock_guard<std::mutex> lock(impl.procMutex);
		for (int i = 0; i < 64; ++i)
		{
			if (impl.procCacheNames[i].empty())
			{
				break;
			}
			if (impl.procCacheNames[i] == name)
			{
				return impl.procCacheAddrs[i];
			}
		}
	}
	void* proc = reinterpret_cast<void*>(::GetProcAddress(impl.module, name));
	{
		std::lock_guard<std::mutex> lock(impl.procMutex);
		for (int i = 0; i < 64; ++i)
		{
			if (impl.procCacheNames[i].empty())
			{
				impl.procCacheNames[i] = name;
				impl.procCacheAddrs[i] = proc;
				break;
			}
		}
	}
	return proc;
}

const std::string& MbKitLib::VersionWarning() const
{
	// 版本告警文本（空串=正常）
	return impl_->versionWarning;
}

const std::string& MbKitLib::LoadedVersion() const
{
	// 已载入内核的版本串
	return impl_->loadedVersion;
}

const std::string& MbKitLib::LoadedPath() const
{
	// 实际使用的 DLL 全路径
	return impl_->loadedPath;
}

const std::string& MbKitLib::LastLoadError() const
{
	// 最近一次加载失败详情
	return impl_->lastLoadError;
}

// ---------------------------------------------------------------------------
// 内部：统一入口宏（先 EnsureLoaded 再取导出；缺导出返回错误码）
// ---------------------------------------------------------------------------
#define XLZ_WKE_ENSURE(retVal)                                                     \
	do {                                                                            \
		if (IsUnloading()) { retVal; } /* 卸载期间直接返回，不再碰内核 */            \
		const WebUiError _e = MbKitLib::Instance().EnsureLoaded();                  \
		if (_e != WebUiError::Ok) { retVal; }                                       \
	} while (false)

static void* MustProc(const char* name, bool* ok)
{
	// 取导出并标记存在性（找不到导出 → ok=false，返回 nullptr 而不是空指针调用）
	void* p = MbKitLib::Instance().GetProc(name);
	*ok = (p != nullptr);
	return p;
}

// ---------------------------------------------------------------------------
// 全局函数
// ---------------------------------------------------------------------------
bool SetNodePath(const char* pathOrDirUtf8, std::string* reasonUtf8, bool forceNodeKernel)
{
	// 对应 WKE设置node路径：强制开关在前、路径在后（与易模块一致）。
	// 必须在碰内核之前调用；内核已加载后返回假。
	XLZ_WKE_ENSURE(return false);

	bool ok = true;
	if (forceNodeKernel)
	{
		bool found = false;
		Fn_Int_i fnForce = reinterpret_cast<Fn_Int_i>(MustProc("MbKit_SetForceNodeKernel", &found));
		if (!found || fnForce(1) == 0)
		{
			if (reasonUtf8 != nullptr)
			{
				*reasonUtf8 = "kernel already loaded (or last load failed); only one kernel per process - "
					"restart the host exe to switch kernels (see SelfDiag())";
			}
			return false;
		}
	}
	{
		bool found = false;
		Fn_Int_pc fnSet = reinterpret_cast<Fn_Int_pc>(MustProc("MbKit_SetNodePath", &found));
		if (!found)
		{
			if (reasonUtf8 != nullptr)
			{
				*reasonUtf8 = "MbKit_SetNodePath export missing";
			}
			return false;
		}
		// MbKit_SetNodePath 收 ANSI(GBK) 文本：UTF-8 → UTF-16 → ACP
		const std::wstring w = Utf8ToWide(pathOrDirUtf8);
		std::string a;
		if (!w.empty())
		{
			const int blen = ::WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
			if (blen > 0)
			{
				a.resize(static_cast<size_t>(blen - 1));
				::WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, a.data(), blen, nullptr, nullptr);
			}
		}
		if (fnSet(a.empty() ? nullptr : a.c_str()) == 0)
		{
			if (reasonUtf8 != nullptr)
			{
				*reasonUtf8 = "kernel already loaded (or last load failed); only one node.dll per process - "
					"restart the host exe to switch kernels (see SelfDiag())";
			}
			ok = false;
		}
	}
	return ok;
}

WebUiError Initialize(bool threadRepaint)
{
	// 对应 WKE初始化：wkeIsInitialize / wkeInitialize(Ex)。
	// 异步只登记：不等内核（等内核用 WaitKernelReady，禁止在 DllMain 里调）。
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);

	bool found = false;
	Fn_Int_v fnIsInit = reinterpret_cast<Fn_Int_v>(MustProc("wkeIsInitialize", &found));
	if (!found)
	{
		return WebUiError::MissingSymbol;
	}
	if (fnIsInit() != 0)
	{
		return WebUiError::Ok; // 已初始化：幂等
	}
	if (threadRepaint)
	{
		// 在其他线程中重画：wkeInitializeEx(NULL)——空配置等价默认初始化，
		// 后台渲染标记由内核按调用线程处理（与易模块的 Ex 路径对齐）
		bool foundEx = false;
		typedef void (__stdcall* Fn_v_pv)(void*);
		Fn_v_pv fnInitEx = reinterpret_cast<Fn_v_pv>(MustProc("wkeInitializeEx", &foundEx));
		if (!foundEx)
		{
			return WebUiError::MissingSymbol;
		}
		fnInitEx(nullptr);
	}
	else
	{
		bool foundInit = false;
		typedef void (__stdcall* Fn_v_v)();
		Fn_v_v fnInit = reinterpret_cast<Fn_v_v>(MustProc("wkeInitialize", &foundInit));
		if (!foundInit)
		{
			return WebUiError::MissingSymbol;
		}
		fnInit();
	}
	// 异步启动内核（MbKit_EnsureKernelInit 只投递不阻塞；可在 UI 线程安全调用）
	bool foundEnsure = false;
	Fn_Int_v fnEnsure = reinterpret_cast<Fn_Int_v>(MustProc("MbKit_EnsureKernelInit", &foundEnsure));
	if (foundEnsure)
	{
		fnEnsure();
	}
	return WebUiError::Ok;
}

bool IsWebView2Supported()
{
	// 对应 WKE是否支持WebView2：只读探测，不加载内核
	XLZ_WKE_ENSURE(return false);
	bool found = false;
	Fn_Int_v fn = reinterpret_cast<Fn_Int_v>(MustProc("MbKit_HasWebViewEnv", &found));
	if (!found)
	{
		return false;
	}
	return fn() == 1;
}

KernelMode GetKernelMode()
{
	// 对应 WKE取内核模式：0/1/2
	XLZ_WKE_ENSURE(return KernelMode::NotLoaded);
	bool found = false;
	Fn_Int_v fn = reinterpret_cast<Fn_Int_v>(MustProc("MbKit_KernelMode", &found));
	if (!found)
	{
		return KernelMode::NotLoaded;
	}
	return static_cast<KernelMode>(fn());
}

std::string GetWebView2Diag()
{
	// 对应 WKE取WebView2诊断：一行式 edgewebview 诊断（ANSI → UTF-8）
	XLZ_WKE_ENSURE(return std::string());
	bool found = false;
	Fn_CStr_v fn = reinterpret_cast<Fn_CStr_v>(MustProc("MbKit_WebViewDiag", &found));
	if (!found)
	{
		return std::string();
	}
	return AnsiToUtf8(fn());
}

std::string SelfDiag()
{
	// 对应 WKE宿主自检：一行式宿主层诊断 + 版本告警头（排障第一入口）
	MbKitLib& lib = MbKitLib::Instance();
	const WebUiError e = lib.EnsureLoaded();
	if (e != WebUiError::Ok)
	{
		std::string out = "MbKit host library load failed: ";
		out += lib.LastLoadError();
		return out;
	}
	bool found = false;
	Fn_CStr_v fn = reinterpret_cast<Fn_CStr_v>(MustProc("MbKit_Diag", &found));
	std::string out;
	if (!found)
	{
		out = "MbKit_Diag export missing";
	}
	else
	{
		out = AnsiToUtf8(fn());
	}
	const std::string& warn = lib.VersionWarning();
	if (!warn.empty())
	{
		out = warn + "\n" + out;
	}
	return out;
}

WebUiError WaitKernelReady(int32_t timeoutMs)
{
	// 对应 WKE等待内核就绪：MbKit_WaitKernel。
	// ⚠ 禁止在 DllMain 里调用（加载锁下内核线程不跑，实测 10s/60s 卡死）。
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	bool found = false;
	Fn_Int_i fn = reinterpret_cast<Fn_Int_i>(MustProc("MbKit_WaitKernel", &found));
	if (!found)
	{
		return WebUiError::MissingSymbol;
	}
	return static_cast<WebUiError>(fn(timeoutMs));
}

BootState GetKernelBootState()
{
	// 对应 WKE取内核启动状态：0..3
	XLZ_WKE_ENSURE(return BootState::NotStarted);
	bool found = false;
	Fn_Int_v fn = reinterpret_cast<Fn_Int_v>(MustProc("MbKit_BootState", &found));
	if (!found)
	{
		return BootState::NotStarted;
	}
	return static_cast<BootState>(fn());
}

int32_t DetachAll()
{
	// ★ 对应 WKE全解绑：MbKit_DetachAll（热重载契约最后一环）。
	// 解绑并销毁全部 view，内核保持已初始化（下次 Attach 廉价）。
	// ⚠ 禁止在 DllMain 里调用；放在插件卸载回调最后一步。
	XLZ_WKE_ENSURE(return 0);
	bool found = false;
	Fn_Int_v fn = reinterpret_cast<Fn_Int_v>(MustProc("MbKit_DetachAll", &found));
	if (!found)
	{
		return 0;
	}
	return fn();
}

// ---------------------------------------------------------------------------
// JS 取参/回值薄封装
// ---------------------------------------------------------------------------
int32_t JsArgCount(int64_t es)
{
	// jsArgCount
	XLZ_WKE_ENSURE(return 0);
	bool found = false;
	Fn_I32_ll fn = reinterpret_cast<Fn_I32_ll>(MustProc("jsArgCount", &found));
	return found ? fn(es) : 0;
}

int32_t JsArgType(int64_t es, int32_t index)
{
	// jsArgType（返回 JS类型_ 常量）
	XLZ_WKE_ENSURE(return 0);
	bool found = false;
	Fn_I32_ll_i fn = reinterpret_cast<Fn_I32_ll_i>(MustProc("jsArgType", &found));
	return found ? fn(es, index) : 0;
}

int64_t JsArg(int64_t es, int32_t index)
{
	// jsArg（返回 jsValue）
	XLZ_WKE_ENSURE(return 0);
	bool found = false;
	Fn_I64_ll_i fn = reinterpret_cast<Fn_I64_ll_i>(MustProc("jsArg", &found));
	return found ? fn(es, index) : 0;
}

int32_t JsArgToInt(int64_t es, int32_t index)
{
	// jsArg + jsToInt
	const int64_t v = JsArg(es, index);
	XLZ_WKE_ENSURE(return 0);
	bool found = false;
	Fn_I32_ll_i fn = reinterpret_cast<Fn_I32_ll_i>(MustProc("jsToInt", &found));
	return found ? fn(es, v) : 0;
}

bool JsArgToBool(int64_t es, int32_t index)
{
	// jsArg + jsToBoolean
	const int64_t v = JsArg(es, index);
	XLZ_WKE_ENSURE(return false);
	bool found = false;
	Fn_I32_ll_i fn = reinterpret_cast<Fn_I32_ll_i>(MustProc("jsToBoolean", &found));
	return found ? (fn(es, v) != 0) : false;
}

double JsArgToDouble(int64_t es, int32_t index)
{
	// jsArg + jsToDouble
	const int64_t v = JsArg(es, index);
	XLZ_WKE_ENSURE(return 0.0);
	bool found = false;
	typedef double (__stdcall* Fn_D_ll_I64)(int64_t, int64_t);
	Fn_D_ll_I64 fn = reinterpret_cast<Fn_D_ll_I64>(MustProc("jsToDouble", &found));
	return found ? fn(es, v) : 0.0;
}

std::string JsArgToStringUtf8(int64_t es, int32_t index)
{
	// jsArg + jsToTempStringW → UTF-8
	const int64_t v = JsArg(es, index);
	XLZ_WKE_ENSURE(return std::string());
	bool found = false;
	Fn_WC_ll_I64 fn = reinterpret_cast<Fn_WC_ll_I64>(MustProc("jsToTempStringW", &found));
	if (!found)
	{
		return std::string();
	}
	return WideToUtf8(fn(es, v));
}

static int64_t JsStringWInternal(const char* textUtf8)
{
	// jsStringW(es, w)（文本 → jsValue）。es 用 0 = 内核约定的「当前执行状态」，
	// 即只在 JsBoundFunction 回调上下文里有效（与易模块 JS文本 一致）。
	bool found = false;
	Fn_I64_ll_wc fn = reinterpret_cast<Fn_I64_ll_wc>(MustProc("jsStringW", &found));
	if (!found)
	{
		return 0;
	}
	const std::wstring w = Utf8ToWide(textUtf8);
	return fn(0, w.c_str());
}

int64_t JsStringW(const char* textUtf8)
{
	// jsStringW（文本 → jsValue；须在 JsBoundFunction 回调线程里调用）
	XLZ_WKE_ENSURE(return 0);
	return JsStringWInternal(textUtf8);
}

std::string JsToStringUtf8(int64_t es, int64_t jsValue)
{
	// jsToStringW → UTF-8（回调里回值给网页）
	XLZ_WKE_ENSURE(return std::string());
	bool found = false;
	Fn_WC_ll_I64 fn = reinterpret_cast<Fn_WC_ll_I64>(MustProc("jsToStringW", &found));
	if (!found)
	{
		return std::string();
	}
	return WideToUtf8(fn(es, jsValue));
}

// ---------------------------------------------------------------------------
// XlzWebView 实现
// ---------------------------------------------------------------------------
struct XlzWebView::Impl
{
	void* view = nullptr;                       // wkeWebView 句柄
	uint32_t generation = 0;                    // 代际计数：每次 Bind/Destroy +1，
	                                            // 回调 thunk 持有代际，不匹配直接忽略
	std::atomic<bool> alive{ false };           // 存活标志：Destroy 后立即置假
	void* hostHwnd = nullptr;                   // 承载窗口

	// 事件 std::function 与其内核 thunk 的 keep-alive（对象同生命周期）
	std::function<void(const XlzWebView::DocumentReady2Event&)> onDocReady2;
	std::function<void(const XlzWebView::ConsoleEvent&)> onConsole;
	std::function<void(const XlzWebView::LoadingFinishEvent&)> onLoadingFinish;
	std::function<void(const XlzWebView::TitleChangedEvent&)> onTitleChanged;
	std::vector<std::pair<std::string, XlzWebView::JsBoundFunction>> jsBinds; // JS 绑定函数 keep-alive

	bool valid() const { return alive.load() && view != nullptr; }
};

// ---- 回调 thunk（__stdcall，与内核约定一致；代际+存活校验后才进用户回调）----

static void __stdcall ThunkDocumentReady2(void* webView, void* user, void* frameHandle)
{
	// wkeOnDocumentReady2(wkeWebView, user, wkeWebFrameHandle)
	if (IsUnloading()) return;
	XlzWebView::Impl* self = static_cast<XlzWebView::Impl*>(user);
	if (self == nullptr || !self->valid() || self->view != webView) return;
	if (!self->onDocReady2) return;
	XlzWebView::DocumentReady2Event ev;
	ev.WebView = reinterpret_cast<uint64_t>(webView);
	ev.FrameHandle = reinterpret_cast<uint64_t>(frameHandle);
	self->onDocReady2(ev);
}

static void __stdcall ThunkConsole(void* webView, void* user, int level, void* msgStr,
	void* sourceStr, unsigned int lineNo, void* stackStr)
{
	// wkeOnConsole(wkeWebView, user, level, 消息wkeString, 来源名wkeString, 行号, 堆栈wkeString)
	if (IsUnloading()) return;
	XlzWebView::Impl* self = static_cast<XlzWebView::Impl*>(user);
	if (self == nullptr || !self->valid() || self->view != webView) return;
	if (!self->onConsole) return;
	XlzWebView::ConsoleEvent ev;
	ev.WebView = reinterpret_cast<uint64_t>(webView);
	ev.Level = level;
	ev.Line = static_cast<int32_t>(lineNo);
	ev.MessageUtf8 = WideToUtf8(static_cast<const wchar_t*>(msgStr));
	ev.SourceUtf8 = WideToUtf8(static_cast<const wchar_t*>(sourceStr));
	ev.StackUtf8 = WideToUtf8(static_cast<const wchar_t*>(stackStr));
	self->onConsole(ev);
}

static void __stdcall ThunkLoadingFinish(void* webView, void* user, void* urlStr,
	int result, void* failedReasonStr)
{
	// wkeOnLoadingFinish(wkeWebView, user, URL wkeString, result 0成功/1失败/2取消, 失败原因 wkeString)
	if (IsUnloading()) return;
	XlzWebView::Impl* self = static_cast<XlzWebView::Impl*>(user);
	if (self == nullptr || !self->valid() || self->view != webView) return;
	if (!self->onLoadingFinish) return;
	XlzWebView::LoadingFinishEvent ev;
	ev.WebView = reinterpret_cast<uint64_t>(webView);
	ev.UrlUtf8 = WideToUtf8(static_cast<const wchar_t*>(urlStr));
	ev.Result = static_cast<LoadResult>(result);
	ev.FailedReasonUtf8 = WideToUtf8(static_cast<const wchar_t*>(failedReasonStr));
	self->onLoadingFinish(ev);
}

static void __stdcall ThunkTitleChanged(void* webView, void* user, void* titleStr)
{
	// wkeOnTitleChanged(wkeWebView, user, 标题 wkeString)
	if (IsUnloading()) return;
	XlzWebView::Impl* self = static_cast<XlzWebView::Impl*>(user);
	if (self == nullptr || !self->valid() || self->view != webView) return;
	if (!self->onTitleChanged) return;
	XlzWebView::TitleChangedEvent ev;
	ev.WebView = reinterpret_cast<uint64_t>(webView);
	ev.TitleUtf8 = WideToUtf8(static_cast<const wchar_t*>(titleStr));
	self->onTitleChanged(ev);
}

// JS 绑定函数 thunk：wkeJsBindFunction 的 fn 形态（__stdcall）：
//   jsValue __stdcall fn(jsExecState es, void* param)（param = wkeJsBindFunction 第3参）
static long long __stdcall ThunkJsBound(void* es, void* user)
{
	// wkeJsBindFunction(name, fn, user, argc) 里 fn(es, user)
	if (IsUnloading()) return 0;
	if (user == nullptr) return 0;
	auto* entry = static_cast<std::pair<std::string, XlzWebView::JsBoundFunction>*>(user);
	if (!entry->second) return 0;
	return entry->second(reinterpret_cast<int64_t>(es));
}

XlzWebView::XlzWebView()
{
	// 惰性分配实现体（轻量对象可值持有）
	impl_ = new Impl();
	impl_->generation = 1;
}

XlzWebView::~XlzWebView()
{
	// 析构 = Destroy（先撤回调 thunk，再销毁 view；销毁后绝不再进用户回调）
	Destroy();
	delete impl_;
	impl_ = nullptr;
}

XlzWebView::Impl& XlzWebView::ImplRef()
{
	return *impl_;
}

WebUiError XlzWebView::Bind(void* hwnd, bool transparent)
{
	// 对应 MB浏览框.绑定：wkeCreateWebView + wkeSetHandle(+wkeSetTransparent)。
	// ★ 必须在承载窗口的创建线程上调用（线程铁律）。
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();

	// 先初始化（异步登记，不阻塞）
	const WebUiError initErr = Initialize();
	if (initErr != WebUiError::Ok)
	{
		return initErr;
	}

	// 可重复绑定：先销毁旧 view（与易模块一致）
	if (impl.view != nullptr)
	{
		Destroy();
	}

	// 无窗口（离屏）用法：hwnd = nullptr 时只创建不 SetHandle
	bool foundCreate = false;
	Fn_Ptr_v fnCreate = reinterpret_cast<Fn_Ptr_v>(MustProc("wkeCreateWebView", &foundCreate));
	if (!foundCreate)
	{
		return WebUiError::MissingSymbol;
	}
	impl.view = fnCreate();
	if (impl.view == nullptr)
	{
		return WebUiError::Generic;
	}

	if (hwnd != nullptr)
	{
		if (!::IsWindow(static_cast<HWND>(hwnd)))
		{
			// 窗口无效：回收刚建的 view
			bool foundDestroy = false;
			typedef void (__stdcall* Fn_v_pv)(void*);
			Fn_v_pv fnDestroy = reinterpret_cast<Fn_v_pv>(MustProc("wkeDestroyWebView", &foundDestroy));
			if (foundDestroy)
			{
				fnDestroy(impl.view);
			}
			impl.view = nullptr;
			return WebUiError::BadParam;
		}
		bool foundSet = false;
		// wkeSetHandle(view, hwnd)：绑定到承载窗口（两参形态）
		typedef void (__stdcall* Fn_v_pv_pv)(void*, void*);
		Fn_v_pv_pv fnSetHandle = reinterpret_cast<Fn_v_pv_pv>(MustProc("wkeSetHandle", &foundSet));
		if (!foundSet)
		{
			return WebUiError::MissingSymbol;
		}
		fnSetHandle(impl.view, hwnd);
		impl.hostHwnd = hwnd;

		// 透明开关
		bool foundTrans = false;
		typedef void (__stdcall* Fn_v_pv_i)(void*, int);
		Fn_v_pv_i fnTrans = reinterpret_cast<Fn_v_pv_i>(MustProc("wkeSetTransparent", &foundTrans));
		if (foundTrans)
		{
			fnTrans(impl.view, transparent ? 1 : 0);
		}

		// 按客户区大小调一次（与易模块一致）
		RECT rc = {};
		if (::GetClientRect(static_cast<HWND>(hwnd), &rc))
		{
			bool foundResize = false;
			typedef int (__stdcall* Fn_Int_pi_ii)(void*, int, int);
			Fn_Int_pi_ii fnResize = reinterpret_cast<Fn_Int_pi_ii>(MustProc("wkeResize", &foundResize));
			if (foundResize)
			{
				fnResize(impl.view, rc.right - rc.left, rc.bottom - rc.top);
			}
		}
	}

	++impl.generation; // 新代际：旧 thunk（如果内核还缓存着）全部失效
	impl.alive.store(true);
	return WebUiError::Ok;
}

void XlzWebView::Destroy()
{
	// 对应 MB浏览框.销毁：撤事件回调 thunk → wkeSetHandle(0) → wkeDestroyWebView。
	// 先置存活标志：销毁过程里内核可能同步触发回调，一律不再进用户代码。
	Impl& impl = ImplRef();
	impl.alive.store(false);
	if (impl.view == nullptr)
	{
		return;
	}
	// 正在卸载期间也不再碰内核（由全局 DetachAll 兜底清理）
	if (!IsUnloading())
	{
		bool found = false;
		typedef void (__stdcall* Fn_v_p_pF_pv)(void*, void*, void*);
		// 撤销四类事件回调（传 NULL 清掉内核侧指向本对象的 thunk）
		Fn_v_p_pF_pv fnDoc = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnDocumentReady2", &found));
		if (found) fnDoc(impl.view, nullptr, nullptr);
		Fn_v_p_pF_pv fnConsole = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnConsole", &found));
		if (found) fnConsole(impl.view, nullptr, nullptr);
		Fn_v_p_pF_pv fnFinish = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnLoadingFinish", &found));
		if (found) fnFinish(impl.view, nullptr, nullptr);
		Fn_v_p_pF_pv fnTitle = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnTitleChanged", &found));
		if (found) fnTitle(impl.view, nullptr, nullptr);

		// wkeSetHandle(view, 0) 再销毁（与易模块一致）
		typedef void (__stdcall* Fn_v_pv_pv)(void*, void*);
		Fn_v_pv_pv fnSetHandle = reinterpret_cast<Fn_v_pv_pv>(MustProc("wkeSetHandle", &found));
		if (found) fnSetHandle(impl.view, nullptr);
		typedef void (__stdcall* Fn_v_pv)(void*);
		Fn_v_pv fnDestroy = reinterpret_cast<Fn_v_pv>(MustProc("wkeDestroyWebView", &found));
		if (found) fnDestroy(impl.view);
	}
	impl.view = nullptr;
	impl.hostHwnd = nullptr;
	++impl.generation;
}

bool XlzWebView::IsValid() const
{
	// view 是否仍有效
	return impl_ != nullptr && impl_->valid();
}

WebUiError XlzWebView::LoadUrl(const char* urlUtf8)
{
	// 载入URL → wkeLoadURLW
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	Fn_Int_pw fn = reinterpret_cast<Fn_Int_pw>(MustProc("wkeLoadURLW", &found));
	if (!found) return WebUiError::MissingSymbol;
	const std::wstring w = Utf8ToWide(urlUtf8);
	return static_cast<WebUiError>(fn(impl.view, w.c_str()));
}

WebUiError XlzWebView::LoadFile(const char* filePathUtf8)
{
	// 载入文件 → wkeLoadFileW
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	Fn_Int_pw fn = reinterpret_cast<Fn_Int_pw>(MustProc("wkeLoadFileW", &found));
	if (!found) return WebUiError::MissingSymbol;
	const std::wstring w = Utf8ToWide(filePathUtf8);
	return static_cast<WebUiError>(fn(impl.view, w.c_str()));
}

WebUiError XlzWebView::LoadHtml(const char* htmlUtf8)
{
	// 载入HTML → wkeLoadHTMLW
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	Fn_Int_pw fn = reinterpret_cast<Fn_Int_pw>(MustProc("wkeLoadHTMLW", &found));
	if (!found) return WebUiError::MissingSymbol;
	const std::wstring w = Utf8ToWide(htmlUtf8);
	return static_cast<WebUiError>(fn(impl.view, w.c_str()));
}

WebUiError XlzWebView::LoadHtmlWithBaseUrl(const char* htmlUtf8, const char* baseUrlUtf8)
{
	// 载入HTML2 → wkeLoadHtmlWithBaseUrl(view, baseUrl(w), html(utf8), ...)：
	// wkeLoadHtmlWithBaseUrl(wkeWebView, const wchar_t* baseUrl, const char* html) 传参顺序按内核契约。
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef void (__stdcall* Fn_v_p_pw_pc)(void*, const wchar_t*, const char*);
	Fn_v_p_pw_pc fn = reinterpret_cast<Fn_v_p_pw_pc>(MustProc("wkeLoadHtmlWithBaseUrl", &found));
	if (!found) return WebUiError::MissingSymbol;
	const std::wstring base = Utf8ToWide(baseUrlUtf8 ? baseUrlUtf8 : "");
	fn(impl.view, base.c_str(), htmlUtf8);
	return WebUiError::Ok;
}

int64_t XlzWebView::RunJs(const char* jsUtf8)
{
	// 运行JS → wkeRunJSW；返回 jsValue（0 = 失败/未绑定）
	XLZ_WKE_ENSURE(return 0);
	Impl& impl = ImplRef();
	if (!impl.valid()) return 0;
	bool found = false;
	Fn_I64_pw fn = reinterpret_cast<Fn_I64_pw>(MustProc("wkeRunJSW", &found));
	if (!found) return 0;
	const std::wstring w = Utf8ToWide(jsUtf8);
	return fn(impl.view, w.c_str());
}

int64_t XlzWebView::RunJsByFrame(uint64_t frameHandle, const char* jsUtf8, bool inClosure)
{
	// 运行JS自框架 → wkeRunJsByFrame(view, frame, js(utf8), inClosure)
	XLZ_WKE_ENSURE(return 0);
	Impl& impl = ImplRef();
	if (!impl.valid()) return 0;
	bool found = false;
	Fn_I64_pi_pc_b fn = reinterpret_cast<Fn_I64_pi_pc_b>(MustProc("wkeRunJsByFrame", &found));
	if (!found) return 0;
	return fn(impl.view, static_cast<int64_t>(frameHandle), jsUtf8, inClosure ? 1 : 0);
}

WebUiError XlzWebView::JsBindFunction(const char* nameUtf8, JsBoundFunction fn, int32_t argc)
{
	// 对应 JS绑定函数：wkeJsBindFunction(name, fnThunk, user, argc)。
	// fn 的生命周期由本对象持有（jsBinds keep-alive），Destroy 时清空。
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	bool found = false;
	Fn_v_pc_pF_pv_i fnBind = reinterpret_cast<Fn_v_pc_pF_pv_i>(MustProc("wkeJsBindFunction", &found));
	if (!found) return WebUiError::MissingSymbol;

	// keep-alive：条目挂在对象上（内核 user 指向它；Destroy 时先清 jsBinds）
	impl.jsBinds.emplace_back(std::string(nameUtf8 ? nameUtf8 : ""), std::move(fn));
	auto* entry = &impl.jsBinds.back();

	// wkeJsBindFunction(name, fn, user, argc)：名称按内核契约收 UTF-8 文本
	// （node.dll 与 edgewebview 后端统一按 utf8 解析绑定名）。
	fnBind(nameUtf8 ? nameUtf8 : "", reinterpret_cast<void*>(&ThunkJsBound), reinterpret_cast<void*>(entry), argc);
	return WebUiError::Ok;
}

WebUiError XlzWebView::OnDocumentReady2(std::function<void(const DocumentReady2Event&)> fn)
{
	// 事件_文档就绪2 → wkeOnDocumentReady2(view, thunk, user=this)
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef void (__stdcall* Fn_v_p_pF_pv)(void*, void*, void*);
	Fn_v_p_pF_pv fnOn = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnDocumentReady2", &found));
	if (!found) return WebUiError::MissingSymbol;
	impl.onDocReady2 = std::move(fn);
	fnOn(impl.view, reinterpret_cast<void*>(&ThunkDocumentReady2), &impl);
	return WebUiError::Ok;
}

WebUiError XlzWebView::OnConsole(std::function<void(const ConsoleEvent&)> fn)
{
	// 事件_控制台消息 → wkeOnConsole(view, thunk, user)
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef void (__stdcall* Fn_v_p_pF_pv)(void*, void*, void*);
	Fn_v_p_pF_pv fnOn = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnConsole", &found));
	if (!found) return WebUiError::MissingSymbol;
	impl.onConsole = std::move(fn);
	fnOn(impl.view, reinterpret_cast<void*>(&ThunkConsole), &impl);
	return WebUiError::Ok;
}

WebUiError XlzWebView::OnLoadingFinish(std::function<void(const LoadingFinishEvent&)> fn)
{
	// 事件_载入完成 → wkeOnLoadingFinish(view, thunk, user)
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef void (__stdcall* Fn_v_p_pF_pv)(void*, void*, void*);
	Fn_v_p_pF_pv fnOn = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnLoadingFinish", &found));
	if (!found) return WebUiError::MissingSymbol;
	impl.onLoadingFinish = std::move(fn);
	fnOn(impl.view, reinterpret_cast<void*>(&ThunkLoadingFinish), &impl);
	return WebUiError::Ok;
}

WebUiError XlzWebView::OnTitleChanged(std::function<void(const TitleChangedEvent&)> fn)
{
	// 事件_标题被改变 → wkeOnTitleChanged(view, thunk, user)
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef void (__stdcall* Fn_v_p_pF_pv)(void*, void*, void*);
	Fn_v_p_pF_pv fnOn = reinterpret_cast<Fn_v_p_pF_pv>(MustProc("wkeOnTitleChanged", &found));
	if (!found) return WebUiError::MissingSymbol;
	impl.onTitleChanged = std::move(fn);
	fnOn(impl.view, reinterpret_cast<void*>(&ThunkTitleChanged), &impl);
	return WebUiError::Ok;
}

void* XlzWebView::GetHostWindow() const
{
	// 取窗口句柄 → wkeGetHostHWND
	if (impl_ == nullptr || !impl_->valid()) return nullptr;
	bool found = false;
	Fn_Ptr_pv fn = reinterpret_cast<Fn_Ptr_pv>(MustProc("wkeGetHostHWND", &found));
	return found ? fn(impl_->view) : nullptr;
}

bool XlzWebView::IsDocumentReady() const
{
	// 是否文档已就绪 → wkeIsDocumentReady
	if (impl_ == nullptr || !impl_->valid()) return false;
	bool found = false;
	Fn_Int_pv fn = reinterpret_cast<Fn_Int_pv>(MustProc("wkeIsDocumentReady", &found));
	return found ? (fn(impl_->view) != 0) : false;
}

std::string XlzWebView::GetUrl() const
{
	// 取URL → wkeGetURL（node.dll 的 wkeGetURL 返回 UTF-8 const char*）
	if (impl_ == nullptr || !impl_->valid()) return std::string();
	bool found = false;
	typedef const char* (__stdcall* Fn_C_pc)(void*);
	Fn_C_pc fn = reinterpret_cast<Fn_C_pc>(MustProc("wkeGetURL", &found));
	if (!found) return std::string();
	return std::string(fn(impl_->view));
}

std::string XlzWebView::GetTitle() const
{
	// 取标题 → wkeGetTitleW（UTF-16 → UTF-8）
	if (impl_ == nullptr || !impl_->valid()) return std::string();
	bool found = false;
	typedef const wchar_t* (__stdcall* Fn_W_pv)(void*);
	Fn_W_pv fn = reinterpret_cast<Fn_W_pv>(MustProc("wkeGetTitleW", &found));
	if (!found) return std::string();
	return WideToUtf8(fn(impl_->view));
}

WebUiError XlzWebView::Resize(int32_t width, int32_t height)
{
	// 调整大小 → wkeResize(view, w, h)
	XLZ_WKE_ENSURE(return WebUiError::HostDllNotFound);
	Impl& impl = ImplRef();
	if (!impl.valid()) return WebUiError::ViewNotBound;
	bool found = false;
	typedef int (__stdcall* Fn_Int_pi_ii)(void*, int, int);
	Fn_Int_pi_ii fn = reinterpret_cast<Fn_Int_pi_ii>(MustProc("wkeResize", &found));
	if (!found) return WebUiError::MissingSymbol;
	fn(impl.view, width, height);
	return WebUiError::Ok;
}

std::string XlzWebView::GetSource() const
{
	// 取源码 → wkeGetSource（返回 UTF-8 const char*）
	if (impl_ == nullptr || !impl_->valid()) return std::string();
	bool found = false;
	typedef const char* (__stdcall* Fn_C_pc)(void*);
	Fn_C_pc fn = reinterpret_cast<Fn_C_pc>(MustProc("wkeGetSource", &found));
	if (!found) return std::string();
	return std::string(fn(impl_->view));
}

void XlzWebView::Reload()
{
	// 刷新 → wkeReload
	if (impl_ == nullptr || !impl_->valid()) return;
	bool found = false;
	typedef void (__stdcall* Fn_v_pv)(void*);
	Fn_v_pv fn = reinterpret_cast<Fn_v_pv>(MustProc("wkeReload", &found));
	if (found) fn(impl_->view);
}

bool XlzWebView::GoBack()
{
	// 后退 → wkeGoBack
	if (impl_ == nullptr || !impl_->valid()) return false;
	bool found = false;
	Fn_Int_pv fn = reinterpret_cast<Fn_Int_pv>(MustProc("wkeGoBack", &found));
	return found ? (fn(impl_->view) != 0) : false;
}

bool XlzWebView::GoForward()
{
	// 前进 → wkeGoForward
	if (impl_ == nullptr || !impl_->valid()) return false;
	bool found = false;
	Fn_Int_pv fn = reinterpret_cast<Fn_Int_pv>(MustProc("wkeGoForward", &found));
	return found ? (fn(impl_->view) != 0) : false;
}

void XlzWebView::StopLoading()
{
	// 退出/停止载入 → wkeStopLoading（交付文档速查表里的 mb.退出()）
	if (impl_ == nullptr || !impl_->valid()) return;
	bool found = false;
	typedef void (__stdcall* Fn_v_pv)(void*);
	Fn_v_pv fn = reinterpret_cast<Fn_v_pv>(MustProc("wkeStopLoading", &found));
	if (found) fn(impl_->view);
}

uint64_t XlzWebView::Handle() const
{
	// 内部使用：view 句柄
	return impl_ != nullptr ? reinterpret_cast<uint64_t>(impl_->view) : 0;
}

}
}
