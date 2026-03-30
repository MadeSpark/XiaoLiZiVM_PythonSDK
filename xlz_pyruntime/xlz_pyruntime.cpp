#include "xlz_pyruntime.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>

// 动态加载Python.h的声明（避免编译时依赖）
typedef int (*Py_Initialize_t)(void);
typedef int (*Py_IsInitialized_t)(void);
typedef void (*Py_Finalize_t)(void);
typedef void (*Py_SetPath_t)(const wchar_t*);
typedef void (*Py_SetProgramName_t)(const wchar_t*);

static Py_Initialize_t g_Py_Initialize = nullptr;
static Py_IsInitialized_t g_Py_IsInitialized = nullptr;
static Py_Finalize_t g_Py_Finalize = nullptr;
static Py_SetPath_t g_Py_SetPath = nullptr;
static Py_SetProgramName_t g_Py_SetProgramName = nullptr;

static HMODULE g_python_dll = nullptr;
static std::atomic<int> g_py_initialized(0);
static std::atomic<int> g_log_reset_done(0);

// 日志文件路径
static const wchar_t* kLogFilePath = L"main\\corn\\xlz_pyruntime.log";

// 写日志函数
static void WriteLog(const char* message)
{
	std::ofstream log(kLogFilePath, std::ios::app);
	if (log.is_open())
	{
		// 获取当前时间
		time_t now = time(nullptr);
		struct tm timeinfo;
		localtime_s(&timeinfo, &now);
		char timestr[32];
		strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &timeinfo);
		
		log << "[" << timestr << "] " << message << "\n";
		log.close();
	}
}

// 仅在进程内首次调用时清空日志
static void ResetLogOnFirstUse()
{
	int expected = 0;
	if (g_log_reset_done.compare_exchange_strong(expected, 1))
	{
		std::ofstream log(kLogFilePath, std::ios::trunc);
		if (log.is_open())
		{
			log.close();
		}
	}
}

// 获取当前DLL所在目录
static bool GetCurrentDllDirectory(wchar_t* outPath, size_t maxLen)
{
	HMODULE hModule = nullptr;
	if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, 
		reinterpret_cast<LPCWSTR>(&GetCurrentDllDirectory), &hModule))
	{
		return false;
	}

	if (!::GetModuleFileNameW(hModule, outPath, static_cast<DWORD>(maxLen)))
	{
		return false;
	}

	wchar_t* lastBackslash = wcsrchr(outPath, L'\\');
	if (lastBackslash)
	{
		*(lastBackslash + 1) = L'\0';
	}

	return true;
}

// 从指定目录加载依赖DLL并记录日志
static bool LoadDependencyFromDir(const wchar_t* dirPath, const wchar_t* fileName, bool required)
{
	wchar_t fullPath[MAX_PATH];
	swprintf_s(fullPath, MAX_PATH, L"%s%s", dirPath, fileName);

	HMODULE h = ::LoadLibraryW(fullPath);
	std::ostringstream oss;
	if (h != nullptr)
	{
		oss << "LoadDependency: loaded ";
	}
	else
	{
		oss << "LoadDependency: failed ";
	}

	int len = WideCharToMultiByte(CP_UTF8, 0, fileName, -1, nullptr, 0, nullptr, nullptr);
	if (len > 0)
	{
		char* nameStr = new char[len];
		WideCharToMultiByte(CP_UTF8, 0, fileName, -1, nameStr, len, nullptr, nullptr);
		oss << nameStr;
		delete[] nameStr;
	}

	if (h == nullptr)
	{
		oss << ", error code: " << ::GetLastError();
		WriteLog(oss.str().c_str());
		return required ? false : true;
	}

	WriteLog(oss.str().c_str());
	return true;
}

// 加载Python DLL并获取函数指针
static int LoadPythonDll(void)
{
	if (g_python_dll != nullptr)
	{
		WriteLog("LoadPythonDll: python310.dll already loaded");
		return 0;  // 已加载
	}

	WriteLog("LoadPythonDll: attempting to load python310.dll");

	// 获取当前DLL的路径
	wchar_t dllPath[MAX_PATH];
	if (!GetCurrentDllDirectory(dllPath, MAX_PATH))
	{
		WriteLog("LoadPythonDll: failed to get current DLL directory");
		return -1;
	}

	// 构建Python目录路径：corn\python（DLL在corn目录下）
	wchar_t pythonDir[MAX_PATH];
	swprintf_s(pythonDir, MAX_PATH, L"%spython\\", dllPath);

	std::ostringstream oss;
	oss << "LoadPythonDll: Python directory: ";
	int len = WideCharToMultiByte(CP_UTF8, 0, pythonDir, -1, nullptr, 0, nullptr, nullptr);
	if (len > 0)
	{
		char* pathStr = new char[len];
		WideCharToMultiByte(CP_UTF8, 0, pythonDir, -1, pathStr, len, nullptr, nullptr);
		oss << pathStr;
		delete[] pathStr;
	}
	WriteLog(oss.str().c_str());

	// 先显式加载 Python 依赖
	if (!LoadDependencyFromDir(pythonDir, L"python3.dll", true))
	{
		WriteLog("LoadPythonDll: missing required dependency python3.dll");
		return -1;
	}
	if (!LoadDependencyFromDir(pythonDir, L"vcruntime140.dll", true))
	{
		WriteLog("LoadPythonDll: missing required dependency vcruntime140.dll");
		return -1;
	}
	if (!LoadDependencyFromDir(pythonDir, L"vcruntime140_1.dll", false))
	{
		WriteLog("LoadPythonDll: optional dependency vcruntime140_1.dll not loaded");
	}
	if (!LoadDependencyFromDir(pythonDir, L"msvcp140.dll", false))
	{
		WriteLog("LoadPythonDll: optional dependency msvcp140.dll not loaded");
	}
	if (!LoadDependencyFromDir(pythonDir, L"libffi-7.dll", false))
	{
		WriteLog("LoadPythonDll: optional dependency libffi-7.dll not loaded");
	}

	// 构建python310.dll的完整路径
	wchar_t pythonDllPath[MAX_PATH];
	swprintf_s(pythonDllPath, MAX_PATH, L"%spython310.dll", pythonDir);

	std::ostringstream oss2;
	oss2 << "LoadPythonDll: loading from path: ";
	len = WideCharToMultiByte(CP_UTF8, 0, pythonDllPath, -1, nullptr, 0, nullptr, nullptr);
	if (len > 0)
	{
		char* pathStr = new char[len];
		WideCharToMultiByte(CP_UTF8, 0, pythonDllPath, -1, pathStr, len, nullptr, nullptr);
		oss2 << pathStr;
		delete[] pathStr;
	}
	WriteLog(oss2.str().c_str());

	// 加载python310.dll
	g_python_dll = ::LoadLibraryW(pythonDllPath);
	if (g_python_dll == nullptr)
	{
		DWORD err = ::GetLastError();
		std::ostringstream errOss;
		errOss << "LoadPythonDll: failed to load python310.dll, error code: " << err;
		WriteLog(errOss.str().c_str());
		return -1;  // 加载失败
	}

	WriteLog("LoadPythonDll: python310.dll loaded successfully");

	// 获取函数指针
	g_Py_Initialize = reinterpret_cast<Py_Initialize_t>(
		::GetProcAddress(g_python_dll, "Py_Initialize"));
	g_Py_IsInitialized = reinterpret_cast<Py_IsInitialized_t>(
		::GetProcAddress(g_python_dll, "Py_IsInitialized"));
	g_Py_Finalize = reinterpret_cast<Py_Finalize_t>(
		::GetProcAddress(g_python_dll, "Py_Finalize"));
	g_Py_SetPath = reinterpret_cast<Py_SetPath_t>(
		::GetProcAddress(g_python_dll, "Py_SetPath"));
	g_Py_SetProgramName = reinterpret_cast<Py_SetProgramName_t>(
		::GetProcAddress(g_python_dll, "Py_SetProgramName"));

	if (!g_Py_Initialize)
	{
		WriteLog("LoadPythonDll: failed to get Py_Initialize");
		::FreeLibrary(g_python_dll);
		g_python_dll = nullptr;
		return -1;
	}
	if (!g_Py_IsInitialized)
	{
		WriteLog("LoadPythonDll: failed to get Py_IsInitialized");
		::FreeLibrary(g_python_dll);
		g_python_dll = nullptr;
		return -1;
	}
	if (!g_Py_Finalize)
	{
		WriteLog("LoadPythonDll: failed to get Py_Finalize");
		::FreeLibrary(g_python_dll);
		g_python_dll = nullptr;
		return -1;
	}

	WriteLog("LoadPythonDll: all function pointers obtained successfully");
	return 0;  // 成功
}

__declspec(dllexport) int __stdcall XLZ_PyInitialize(void)
{
	ResetLogOnFirstUse();
	WriteLog("XLZ_PyInitialize: called");

	// 检查是否已初始化
	if (g_py_initialized.load() != 0)
	{
		WriteLog("XLZ_PyInitialize: already initialized, returning 1");
		return 1;  // 已初始化
	}

	// 加载Python DLL
	WriteLog("XLZ_PyInitialize: loading Python DLL");
	if (LoadPythonDll() != 0)
	{
		WriteLog("XLZ_PyInitialize: failed to load Python DLL, returning -1");
		return -1;  // 加载失败
	}

	// 获取当前DLL的目录路径
	wchar_t dllDir[MAX_PATH];
	if (!GetCurrentDllDirectory(dllDir, MAX_PATH))
	{
		WriteLog("XLZ_PyInitialize: failed to get DLL directory");
		return -1;
	}

	// 构建Python路径：corn\python\python310.zip和python目录
	wchar_t pythonDir[MAX_PATH];
	swprintf_s(pythonDir, MAX_PATH, L"%spython\\", dllDir);

	wchar_t pythonPath[MAX_PATH * 2];
	swprintf_s(pythonPath, MAX_PATH * 2, L"%spython310.zip;%s", pythonDir, pythonDir);

	std::ostringstream oss;
	oss << "XLZ_PyInitialize: setting Python path to: ";
	int len = WideCharToMultiByte(CP_UTF8, 0, pythonPath, -1, nullptr, 0, nullptr, nullptr);
	if (len > 0)
	{
		char* pathStr = new char[len];
		WideCharToMultiByte(CP_UTF8, 0, pythonPath, -1, pathStr, len, nullptr, nullptr);
		oss << pathStr;
		delete[] pathStr;
	}
	WriteLog(oss.str().c_str());

	// 设置Python路径
	if (g_Py_SetPath)
	{
		WriteLog("XLZ_PyInitialize: calling Py_SetPath()");
		g_Py_SetPath(pythonPath);
		WriteLog("XLZ_PyInitialize: Py_SetPath() returned");
	}

	// 初始化Python解释器
	WriteLog("XLZ_PyInitialize: calling Py_Initialize()");
	g_Py_Initialize();
	WriteLog("XLZ_PyInitialize: Py_Initialize() returned");

	// 检查是否初始化成功
	WriteLog("XLZ_PyInitialize: checking if Python is initialized");
	if (!g_Py_IsInitialized())
	{
		WriteLog("XLZ_PyInitialize: Py_IsInitialized() returned false, initialization failed");
		::FreeLibrary(g_python_dll);
		g_python_dll = nullptr;
		return -1;  // 初始化失败
	}

	WriteLog("XLZ_PyInitialize: Python initialized successfully");
	g_py_initialized.store(1);
	return 0;  // 成功
}

__declspec(dllexport) int __stdcall XLZ_PyFinalize(void)
{
	WriteLog("XLZ_PyFinalize: called");

	// 检查是否已初始化
	if (g_py_initialized.load() == 0)
	{
		WriteLog("XLZ_PyFinalize: not initialized, returning 1");
		return 1;  // 未初始化
	}

	// 销毁Python解释器
	WriteLog("XLZ_PyFinalize: calling Py_Finalize()");
	if (g_Py_Finalize)
	{
		g_Py_Finalize();
	}

	// 卸载Python DLL
	WriteLog("XLZ_PyFinalize: unloading python310.dll");
	if (g_python_dll != nullptr)
	{
		::FreeLibrary(g_python_dll);
		g_python_dll = nullptr;
	}

	WriteLog("XLZ_PyFinalize: Python finalized successfully");
	g_py_initialized.store(0);
	return 0;  // 成功
}
