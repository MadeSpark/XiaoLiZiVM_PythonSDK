# -*- coding: utf-8 -*-
"""xlz_webui.py —— 小栗子 Python 插件内嵌 webui（Miniblink / MbKit 内核）
================================================================================

这是「第 4 件套」：`plugin_main.py` + `xlz_sdk.py` + `xlz_constants.py` + `xlz_webui.py`。
前三个负责插件与框架通信，本文件负责**在一个窗口里画网页**（登录面板、控制台、
数据看板、二维码、富文本公告……）。

内核不是新写的：直接 ctypes 调用 MbKit146.dll（易语言模块 MB浏览框 用的同一份宿主库），
所以行为与易语言插件里的 MB浏览框 一致。约定遵循 开发SDK/SDK字段命名规范.md（Python 侧 snake_case）。


--------------------------------------------------------------------------------
一、最小可用例子
--------------------------------------------------------------------------------

    from xlz_sdk import Bridge
    from xlz_webui import WebView, detach_all

    api = Bridge(ctx["dll_path"])
    hwnd = api.get_framework_main_window_handle()   # 宿主的窗口句柄

    wv = WebView()                 # 先建对象
    wv.bind(hwnd)                  # ★ 必须在「创建该窗口的那个线程」里调用
    wv.on_document_ready2(lambda url: wv.run_js("document.title='hello'"))
    wv.load_html("<h1>你好</h1>")

    # 卸载时（on_uninstall）：
    detach_all()

`bind()` 会接管窗口的过程函数（子类化）来转发鼠标键盘、并在 WM_PAINT 里把内核画好的
内容 BitBlt 到窗口上。这是 miniblink「窗口模式」的必需动作，跳过它就会得到一块灰屏。


--------------------------------------------------------------------------------
二、线程铁律（灰屏的头号原因，务必读）
--------------------------------------------------------------------------------

miniblink 的 view 属于**创建它的那个线程**（内核按线程编组）。所以：

  * `bind()` 只能在「拥有该窗口」的线程里调用 —— 对宿主窗口来说就是宿主 UI 线程。
  * 在 Python 工作线程里调 `bind()`，内核会拒绝创建 view（`MbKit` 会写
    「内核属于线程 X（兼容模式，非编组），当前线程 Y 无法建 view」），
    窗口就永远是一块灰的、没有内容、也没有键盘响应。

本模块用 `GetWindowThreadProcessId(hwnd)` 与 `GetCurrentThreadId()` 比对来自证这件事，
不一致时直接抛 `MbKitThreadError`（不会悄悄灰屏）。要跨线程，用 `WebView.post()`：

    def worker():
        data = slow_query()
        wv.post(lambda: wv.run_js("render(%r)" % (data,)))   # 回到 owner 线程执行

`post()` 通过一个隐藏的 message-only 窗口 + `PostMessageW(WM_APP+n)` 实现，
不依赖你要自己开消息泵。**普通线程函数里绝对不要直接调 `load_url` / `run_js`。**
（`load_url` 之类本身是线程安全的 cdecl 调用，但绘制与窗口消息不是；统一走 `post()` 最省心。）


--------------------------------------------------------------------------------
三、热重载契约（重载插件不炸宿主）
--------------------------------------------------------------------------------

宿主重载插件时会先 `on_uninstall` 再重新 `apprun`。内核（node.dll / WebView2）是
进程级的，重载带不走它，所以必须按顺序收摊，否则会留下「窗口已死但内核还在往里画」
的野回调，表现是重载几次后宿主闪退（栈顶常常是 `win32u.dll+0x11cc`）。

`on_uninstall` / `on_disable` 里按这个顺序做：

    1. 先停掉所有会碰窗口的线程与定时器        shutdown_threads(wait=False)
    2. 把 webui 对象全部销毁（含解绑窗口过程）  detach_all()
    3. 其它资源（网络、文件、数据库）

`detach_all()` 已经做了第 2 步的全部动作（Python 侧逐个 `wkeSetHandle(0)` +
还原窗口过程 + `wkeDestroyWebView`，最后再调内核的 `MbKit_DetachAll()` 兜底），
它自己不会抛异常，可以放在 `finally` 里。

另外注意：**销毁之后不要再从别的线程碰这个 WebView**。本模块在 `destroy()` 里立起
alive 标志，之后内核即使因为竞态又回调进来，也只会走空壳返回，永远进不到你的回调函数。


--------------------------------------------------------------------------------
四、MbKit146.dll 的分发与版本
--------------------------------------------------------------------------------

Python 插件没有「内置资源」这一说（易语言模块是把 DLL 塞进 .e 里运行时释放），
所以请把 **MbKit146.dll 放在插件目录**里一起打包，并保证与框架自带的版本一致。

查找顺序（`find_dll()`）：

    1. `WebView(dll_path=...)` / `MbKit(dll_path=...)` 显式指定
    2. 环境变量 `XLZ_MBKIT_DLL`
    3. 进程内**已经载入**的那一份（GetModuleHandleW）—— 宿主或别的插件先载入了就用它
    4. 宿主 exe 所在目录
    5. 本文件所在目录（= 你的插件目录）
    6. %TEMP%\\ElangMbKit\\
    7. 裸名 "MbKit146.dll"（走系统 DLL 搜索顺序，含 PATH）

第 3 条最要紧：**一个进程里只有最先载入的那份 MbKit 生效**，多份共存没有意义。
如果发现版本串里没有 `MbKit/1.4.5`，会 `warnings.warn` 提示「多份 DLL 版本不一致」。
找不到 DLL 时抛 `MbKitNotFoundError`，附带全部尝试过的路径 —— 不会崩宿主。

`MbKit146.dll` 是 **32 位（Win32）**二进制，必须用宿主自带的 32 位 Python 加载。
在 64 位 Python 里 import 本模块没问题，但第一次真正加载会得到明确的报错文本。


--------------------------------------------------------------------------------
五、已知限制（诚实版）
--------------------------------------------------------------------------------

* 重载健壮性：内核侧 `wkeDestroyWebView` 是**延迟回收**（约 400ms，避开加载锁），
  WebView2 内核的窗口释放还有异步竞态。**连续 5 次以上「极速重载」仍可能让内核崩**
  （栈顶 `win32u.dll+0x11cc`）。这是内核的已知问题，本文件只能保证 Python 侧不留野回调。
* 只支持「窗口模式」。离屏 / 无窗口模式（`hwnd=0`）能建 view 但需要你自己 `pump()` 并
  `wkePaint` 取位图，本文件只提供底座，没有封装成可用面板。
* IME：只设置了候选窗口位置（CFS_CANDIDATEPOS），没设组合窗口，复杂输入法可能有偏移。
* 输入转发默认走 Python 回调（子类化 WNDPROC），每次鼠标移动都会过一趟 Python，
  比纯 C 实现慢；对交互式面板够用，做游戏/高频动画时请把 `input_hook=False` 关掉自己转发。
* `js_bind_function` 的**函数名只用 ASCII 字母/数字/下划线**（内核按 `const char*` 收）。
* 子类化会接管窗口过程：如果你的插件自己也子类化了同一个窗口，两者会互相抢，
  `bind()` 会检测到已存在的标记属性并抛 `MbKitError`，请改用 `input_hook=False`。
"""

import ctypes
import os
import sys
import threading
import time
import warnings

from ctypes import wintypes

__version__ = "1.0.0"

# ---------------------------------------------------------------- 公开常量

MBK_DLL_NAME = "MbKit146.dll"
MBK_VERSION_TAG = "MbKit/1.4.5"
ENV_DLL = "XLZ_MBKIT_DLL"          # 用环境变量指定 MbKit146.dll 路径
DEFAULT_WAIT_MS = 20000            # 等内核启动的默认超时（与易模块一致）

KERNEL_NONE = 0                    # 内核未就绪 / 上次启动失败
KERNEL_NODE = 1                    # node.dll（miniblink 原生内核）
KERNEL_WEBVIEW2 = 2                # edgewebview（系统 WebView2 Runtime）

# jsArgType 的取值
JS_TYPE_NUMBER = 0
JS_TYPE_STRING = 1
JS_TYPE_BOOLEAN = 2
JS_TYPE_OBJECT = 3
JS_TYPE_FUNCTION = 4
JS_TYPE_UNDEFINED = 5

# 内核返回值（mbkit_api.h）
MBKIT_OK = 0
MBKIT_E_GENERIC = -1
MBKIT_E_NODLL = -2
MBKIT_E_SYMBOL = -3
MBKIT_E_PARAM = -4
MBKIT_E_STATE = -5

__all__ = [
    "MbKitError", "MbKitNotFoundError", "MbKitThreadError",
    "MbKit", "WebView",
    "detach_all", "unload", "live_views", "find_dll",
    "kernel_version", "kernel_diag",
    "MBK_DLL_NAME", "MBK_VERSION_TAG", "ENV_DLL",
    "KERNEL_NONE", "KERNEL_NODE", "KERNEL_WEBVIEW2",
    "JS_TYPE_NUMBER", "JS_TYPE_STRING", "JS_TYPE_BOOLEAN",
    "JS_TYPE_OBJECT", "JS_TYPE_FUNCTION", "JS_TYPE_UNDEFINED",
]

# 64 位与 32 位的差别只在这里：指针宽度与 SetWindowLong 家族的名字
_IS64 = ctypes.sizeof(ctypes.c_void_p) == 8
_LRESULT = ctypes.c_ssize_t
_WPARAM = ctypes.c_size_t
_LPARAM = ctypes.c_ssize_t


# ---------------------------------------------------------------- 异常

class MbKitError(RuntimeError):
    """webui 相关错误的总基类（找不到 DLL、状态不对、内核拒绝……）"""


class MbKitNotFoundError(MbKitError):
    """找不到可用的 MbKit146.dll，或它无法被载入（位数不符/被杀软吃掉）"""


class MbKitThreadError(MbKitError):
    """跨线程操作：bind() 不在窗口所属线程里调用（灰屏的经典原因）"""


# ---------------------------------------------------------------- 回调强引用

# 内核登记过的函数指针永不释放：ctypes 回调对象一旦被 GC，内核再回调就是野指针。
# 销毁时只把 alive 标志立起来让 thunk 立刻返回，对象本身挪进这个「墓地」。
_CALLBACK_GRAVE = []
_GRAVE_LOCK = threading.Lock()


def _retire(obj):
    """把 ctypes 回调对象挪进墓地（保持强引用，直到进程结束）"""
    if obj is not None:
        with _GRAVE_LOCK:
            _CALLBACK_GRAVE.append(obj)


# ---------------------------------------------------------------- Win32 底座

class _Win32(object):
    """只声明本模块用到的 Win32 函数，并钉死 argtypes（不钉会在 64 位上截断指针）"""

    def __init__(self):
        k32 = ctypes.WinDLL("kernel32.dll")
        u32 = ctypes.WinDLL("user32.dll")
        g32 = ctypes.WinDLL("gdi32.dll")
        self._k32, self._u32, self._g32 = k32, u32, g32

        P, PP = ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)

        def sig(lib, name, restype, argtypes):
            fn = getattr(lib, name)
            fn.restype = restype
            fn.argtypes = argtypes
            return fn

        # ---- kernel32
        self.GetCurrentThreadId = sig(k32, "GetCurrentThreadId", wintypes.DWORD, [])
        self.GetCurrentProcessId = sig(k32, "GetCurrentProcessId", wintypes.DWORD, [])
        self.GetModuleHandleW = sig(k32, "GetModuleHandleW", wintypes.HMODULE, [wintypes.LPCWSTR])
        self.GetModuleFileNameW = sig(k32, "GetModuleFileNameW", wintypes.DWORD,
                                      [wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD])
        self.LoadLibraryW = sig(k32, "LoadLibraryW", wintypes.HMODULE, [wintypes.LPCWSTR])
        self.FreeLibrary = sig(k32, "FreeLibrary", wintypes.BOOL, [wintypes.HMODULE])
        self.GetProcAddress = sig(k32, "GetProcAddress", P, [wintypes.HMODULE, ctypes.c_char_p])
        self.CreateMutexW = sig(k32, "CreateMutexW", wintypes.HANDLE,
                                [P, wintypes.BOOL, wintypes.LPCWSTR])

        # ---- user32
        self.GetWindowThreadProcessId = sig(u32, "GetWindowThreadProcessId", wintypes.DWORD,
                                            [wintypes.HWND, PP])
        self.IsWindow = sig(u32, "IsWindow", wintypes.BOOL, [wintypes.HWND])
        self.GetClientRect = sig(u32, "GetClientRect", wintypes.BOOL, [wintypes.HWND, P])
        self.GetWindowRect = sig(u32, "GetWindowRect", wintypes.BOOL, [wintypes.HWND, P])
        self.ScreenToClient = sig(u32, "ScreenToClient", wintypes.BOOL,
                                  [wintypes.HWND, P])
        self.GetDC = sig(u32, "GetDC", wintypes.HDC, [wintypes.HWND])
        self.ReleaseDC = sig(u32, "ReleaseDC", ctypes.c_int, [wintypes.HWND, wintypes.HDC])
        self.InvalidateRect = sig(u32, "InvalidateRect", wintypes.BOOL,
                                  [wintypes.HWND, P, wintypes.BOOL])
        self.BeginPaint = sig(u32, "BeginPaint", wintypes.HDC, [wintypes.HWND, P])
        self.EndPaint = sig(u32, "EndPaint", wintypes.BOOL, [wintypes.HWND, P])
        self.GetFocus = sig(u32, "GetFocus", wintypes.HWND, [])
        self.SetFocus = sig(u32, "SetFocus", wintypes.HWND, [wintypes.HWND])
        self.SetCapture = sig(u32, "SetCapture", wintypes.HWND, [wintypes.HWND])
        self.ReleaseCapture = sig(u32, "ReleaseCapture", wintypes.BOOL, [])
        self.CallWindowProcW = sig(u32, "CallWindowProcW", _LRESULT,
                                   [P, wintypes.HWND, wintypes.UINT, _WPARAM, _LPARAM])
        self.DefWindowProcW = sig(u32, "DefWindowProcW", _LRESULT,
                                  [wintypes.HWND, wintypes.UINT, _WPARAM, _LPARAM])
        self.SetPropA = sig(u32, "SetPropA", wintypes.BOOL,
                            [wintypes.HWND, wintypes.LPCSTR, wintypes.HANDLE])
        self.GetPropA = sig(u32, "GetPropA", wintypes.HANDLE,
                            [wintypes.HWND, wintypes.LPCSTR])
        self.RemovePropA = sig(u32, "RemovePropA", wintypes.HANDLE,
                               [wintypes.HWND, wintypes.LPCSTR])
        self.PostMessageW = sig(u32, "PostMessageW", wintypes.BOOL,
                                [wintypes.HWND, wintypes.UINT, _WPARAM, _LPARAM])
        self.RegisterClassW = sig(u32, "RegisterClassW", wintypes.ATOM, [P])
        self.CreateWindowExW = sig(u32, "CreateWindowExW", wintypes.HWND,
                                   [wintypes.DWORD, wintypes.LPCWSTR, wintypes.LPCWSTR,
                                    wintypes.DWORD, ctypes.c_int, ctypes.c_int,
                                    ctypes.c_int, ctypes.c_int, wintypes.HWND, wintypes.HMENU,
                                    wintypes.HINSTANCE, P])
        self.DestroyWindow = sig(u32, "DestroyWindow", wintypes.BOOL, [wintypes.HWND])
        self.UpdateLayeredWindow = sig(u32, "UpdateLayeredWindow", wintypes.BOOL,
                                       [wintypes.HWND, wintypes.HDC, P, P, wintypes.HDC,
                                        P, wintypes.DWORD, P, wintypes.DWORD])
        self.ImmGetContext = sig(u32, "ImmGetContext", wintypes.HANDLE, [wintypes.HWND])
        self.ImmReleaseContext = sig(u32, "ImmReleaseContext", wintypes.BOOL,
                                     [wintypes.HWND, wintypes.HANDLE])
        self.ImmSetCandidateWindow = sig(u32, "ImmSetCandidateWindow", wintypes.BOOL,
                                         [wintypes.HANDLE, P])

        # SetWindowLong 家族：32 位是 SetWindowLongW，64 位必须用 SetWindowLongPtrW
        if _IS64:
            self.SetWindowLong = sig(u32, "SetWindowLongPtrW", P,
                                     [wintypes.HWND, ctypes.c_int, P])
            self.GetWindowLong = sig(u32, "GetWindowLongPtrW", P,
                                     [wintypes.HWND, ctypes.c_int])
        else:
            self.SetWindowLong = sig(u32, "SetWindowLongW", P,
                                     [wintypes.HWND, ctypes.c_int, P])
            self.GetWindowLong = sig(u32, "GetWindowLongW", P,
                                     [wintypes.HWND, ctypes.c_int])

        # ---- gdi32
        self.CreateCompatibleDC = sig(g32, "CreateCompatibleDC", wintypes.HDC, [wintypes.HDC])
        self.CreateCompatibleBitmap = sig(g32, "CreateCompatibleBitmap", wintypes.HBITMAP,
                                          [wintypes.HDC, ctypes.c_int, ctypes.c_int])
        self.SelectObject = sig(g32, "SelectObject", wintypes.HGDIOBJ,
                                [wintypes.HDC, wintypes.HGDIOBJ])
        self.DeleteObject = sig(g32, "DeleteObject", wintypes.BOOL, [wintypes.HGDIOBJ])
        self.DeleteDC = sig(g32, "DeleteDC", wintypes.BOOL, [wintypes.HDC])
        self.GetCurrentObject = sig(g32, "GetCurrentObject", wintypes.HGDIOBJ,
                                    [wintypes.HDC, wintypes.UINT])
        self.GetObjectA = sig(g32, "GetObjectA", ctypes.c_int,
                              [wintypes.HANDLE, ctypes.c_int, P])
        self.BitBlt = sig(g32, "BitBlt", wintypes.BOOL,
                          [wintypes.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                           ctypes.c_int, wintypes.HDC, ctypes.c_int, ctypes.c_int,
                           wintypes.DWORD])


_win32 = None
_win32_lock = threading.Lock()


def win32():
    """取 Win32 函数表（懒初始化，import 本模块时不碰 user32）"""
    global _win32
    if _win32 is None:
        with _win32_lock:
            if _win32 is None:
                _win32 = _Win32()
    return _win32


def current_thread_id():
    """当前线程 ID"""
    return int(win32().GetCurrentThreadId())


def window_thread_id(hwnd):
    """窗口所属线程 ID（0 = 取不到）"""
    if not hwnd:
        return 0
    pid = wintypes.DWORD(0)
    tid = win32().GetWindowThreadProcessId(wintypes.HWND(int(hwnd)), ctypes.byref(pid))
    return int(tid)


def _host_exe_dir():
    """宿主 exe 所在目录（用主模块路径，不用 cwd —— cwd 可能被改成任意地方）"""
    try:
        buf = ctypes.create_unicode_buffer(1024)
        n = win32().GetModuleFileNameW(None, buf, 1024)
        if n:
            return os.path.dirname(buf.value)
    except Exception:
        pass
    try:
        return os.path.dirname(os.path.abspath(sys.executable))
    except Exception:
        return ""


# ---------------------------------------------------------------- 结构体

class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


class POINT(ctypes.Structure):
    _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]


class SIZE(ctypes.Structure):
    _fields_ = [("cx", ctypes.c_long), ("cy", ctypes.c_long)]


class PAINTSTRUCT(ctypes.Structure):
    _fields_ = [("hdc", wintypes.HDC), ("fErase", wintypes.BOOL),
                ("rcPaint", RECT), ("fRestore", wintypes.BOOL),
                ("fIncUpdate", wintypes.BOOL), ("rgbReserved", ctypes.c_byte * 32)]


class BLENDFUNCTION(ctypes.Structure):
    _fields_ = [("BlendOp", ctypes.c_ubyte), ("BlendFlags", ctypes.c_ubyte),
                ("SourceConstantAlpha", ctypes.c_ubyte), ("AlphaFormat", ctypes.c_ubyte)]


class BITMAP(ctypes.Structure):
    _fields_ = [("bmType", ctypes.c_long), ("bmWidth", ctypes.c_long),
                ("bmHeight", ctypes.c_long), ("bmWidthBytes", ctypes.c_long),
                ("bmPlanes", wintypes.WORD), ("bmBitsPixel", wintypes.WORD),
                ("bmBits", ctypes.c_void_p)]


class CANDIDATEFORM(ctypes.Structure):
    _fields_ = [("dwIndex", wintypes.DWORD), ("dwStyle", wintypes.DWORD),
                ("ptCurrentPos", POINT), ("rcArea", RECT)]


class wkeCaretRect(ctypes.Structure):
    """miniblink 的 wkeGetCaretRect 填的是 {x, y, w, h}，不是 RECT"""
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int),
                ("w", ctypes.c_int), ("h", ctypes.c_int)]


# 过程函数与内核回调（★ 过程函数必须是 WINFUNCTYPE/stdcall，内核回调必须是 CFUNCTYPE/cdecl）
WNDPROC = ctypes.WINFUNCTYPE(_LRESULT, wintypes.HWND, wintypes.UINT, _WPARAM, _LPARAM)
WNDCLASSW_P = ctypes.c_void_p

MB_EVENT_CB = ctypes.WINFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int,
                                 ctypes.c_void_p, ctypes.c_void_p)

# wke 的 cdecl 回调：统一按「参数个数」声明，具体语义在调用点注释
JS_CB = ctypes.CFUNCTYPE(ctypes.c_longlong, ctypes.c_void_p, ctypes.c_void_p)
PAINT_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                            ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int)
DOC_READY2_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                                 ctypes.c_int)
CONSOLE_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                              ctypes.c_int, ctypes.c_void_p, ctypes.c_int,
                              ctypes.c_void_p)
LOADING_FINISH_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                                     ctypes.c_int, ctypes.c_void_p)
TITLE_CHANGED_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                                    ctypes.c_void_p)
URL_CHANGED_CB = ctypes.CFUNCTYPE(None, ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p,
                                  ctypes.c_void_p, ctypes.c_void_p)


# ---------------------------------------------------------------- 导出表
# 规则（别改）：MbKit_* 是 __stdcall（WinDLL + WINFUNCTYPE）
#              wke* / js* 是 __cdecl （CDLL  + CFUNCTYPE）
# 依据：gen_thunk.c 的 `void __cdecl JsBindGetter(...)`，以及易语言声明表里
#       wke*/js* 的导出名带 "@" 前缀（= cdecl）、MbKit_* 不带（= stdcall）。

_S, _C = "stdcall", "cdecl"

# 名字 -> (返回类型, [参数类型...])
_STD_TABLE = {
    # ---- 版本 / 诊断 -------------------------------------------------------
    "MbKit_Version":          (ctypes.c_char_p, []),
    "MbKit_Diag":             (ctypes.c_char_p, []),
    "MbKit_WebViewDiag":      (ctypes.c_char_p, []),
    "MbKit_LastError":        (ctypes.c_char_p, []),
    "MbKit_NodePath":         (ctypes.c_char_p, []),
    "MbKit_ConfigNodePath":   (ctypes.c_char_p, []),
    "MbKit_InstallVehProbe":  (ctypes.c_char_p, []),
    "MbKit_SelfModule":       (ctypes.c_uint, []),
    "MbKit_Pinned":           (ctypes.c_int, []),
    "MbKit_KernelInitCount":  (ctypes.c_int, []),
    "MbKit_SuppressedCount":  (ctypes.c_int, []),
    "MbKit_CbCount":          (ctypes.c_int, []),
    "MbKit_CbLive":           (ctypes.c_int, []),
    "MbKit_CbHits":           (ctypes.c_int, []),
    "MbKit_CbBlocked":        (ctypes.c_int, []),
    "MbKit_CbRegionBlocked":  (ctypes.c_int, []),
    # ---- 状态 ---------------------------------------------------------------
    "MbKit_KernelLoaded":     (ctypes.c_int, []),
    "MbKit_IsReady":          (ctypes.c_int, []),
    "MbKit_BootState":        (ctypes.c_int, []),
    "MbKit_BootWaits":        (ctypes.c_int, []),
    "MbKit_PumpCount":        (ctypes.c_int, []),
    "MbKit_WaitTimeouts":     (ctypes.c_int, []),
    "MbKit_KernelMode":       (ctypes.c_int, []),
    "MbKit_KernelState":      (ctypes.c_int, []),
    "MbKit_HasWebViewEnv":    (ctypes.c_int, []),
    "MbKit_LiveCount":        (ctypes.c_int, []),
    "MbKit_VehHits":          (ctypes.c_int, []),
    # ---- 内核选择 / 启动（必须在建 view 之前） ------------------------------
    "MbKit_Init":             (ctypes.c_int, [ctypes.c_char_p]),
    "MbKit_SetNodePath":      (ctypes.c_int, [ctypes.c_char_p]),
    "MbKit_SetForceNodeKernel": (ctypes.c_int, [ctypes.c_int]),
    "MbKit_EnsureKernelInit": (ctypes.c_int, []),
    "MbKit_EnsureOwner":      (ctypes.c_int, []),
    "MbKit_WaitKernel":       (ctypes.c_int, [ctypes.c_int]),
    "MbKit_Pump":             (ctypes.c_int, [ctypes.c_int]),
    # ---- 收摊 ---------------------------------------------------------------
    "MbKit_DetachAll":        (ctypes.c_int, []),
    "MbKit_DetachView":       (ctypes.c_int, [ctypes.c_void_p]),
    "MbKit_KillZombieWindows": (ctypes.c_int, []),
    "MbKit_KillCallerWindows": (ctypes.c_int, []),
}

_CDECL_TABLE = {
    # ---- 内核生命周期 -------------------------------------------------------
    "wkeInitialize":            (None, []),
    "wkeIsInitialize":          (ctypes.c_int, []),
    # ---- view 本体 ---------------------------------------------------------
    "wkeCreateWebView":         (ctypes.c_int, []),
    "wkeDestroyWebView":        (None, [ctypes.c_int]),
    "wkeSetHandle":             (None, [ctypes.c_int, ctypes.c_void_p]),
    "wkeSetHandleOffset":       (None, [ctypes.c_int, ctypes.c_int, ctypes.c_int]),
    "wkeGetHostHWND":           (ctypes.c_void_p, [ctypes.c_int]),
    "wkeResize":                (None, [ctypes.c_int, ctypes.c_int, ctypes.c_int]),
    "wkeGetWidth":              (ctypes.c_int, [ctypes.c_int]),
    "wkeGetHeight":             (ctypes.c_int, [ctypes.c_int]),
    "wkeGetContentWidth":       (ctypes.c_int, [ctypes.c_int]),
    "wkeGetContentHeight":      (ctypes.c_int, [ctypes.c_int]),
    "wkeSetTransparent":        (None, [ctypes.c_int, ctypes.c_int]),
    "wkeSetZoomFactor":         (None, [ctypes.c_int, ctypes.c_float]),
    "wkeSetUserAgentW":         (None, [ctypes.c_int, ctypes.c_wchar_p]),
    "wkeSetCookieEnabled":      (None, [ctypes.c_int, ctypes.c_int]),
    "wkeSetDebugConfig":        (None, [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]),
    "wkeSetHeadlessEnabled":    (None, [ctypes.c_int, ctypes.c_int]),
    "wkeSetDirty":              (None, [ctypes.c_int, ctypes.c_int]),
    "wkeSetFocus":              (None, [ctypes.c_int]),
    "wkeKillFocus":             (None, [ctypes.c_int]),
    # ---- 绘制（窗口模式的命门） --------------------------------------------
    "wkeRepaintIfNeeded":       (None, [ctypes.c_int]),
    "wkeGetViewDC":             (ctypes.c_void_p, [ctypes.c_int]),
    "wkePaint":                 (None, [ctypes.c_int, ctypes.c_void_p, ctypes.c_int]),
    "wkeGetCaretRect":          (None, [ctypes.c_int, ctypes.c_void_p]),
    # ---- 载入 ---------------------------------------------------------------
    "wkeLoadURLW":              (None, [ctypes.c_int, ctypes.c_wchar_p]),
    "wkeLoadFileW":             (None, [ctypes.c_int, ctypes.c_wchar_p]),
    "wkeLoadHTMLW":             (None, [ctypes.c_int, ctypes.c_wchar_p]),
    "wkeLoadHtmlWithBaseUrl":   (None, [ctypes.c_int, ctypes.c_wchar_p, ctypes.c_wchar_p]),
    "wkeReload":                (None, [ctypes.c_int]),
    "wkeStopLoading":           (None, [ctypes.c_int]),
    "wkeGoBack":                (ctypes.c_int, [ctypes.c_int]),
    "wkeGoForward":             (ctypes.c_int, [ctypes.c_int]),
    "wkeIsLoading":             (ctypes.c_int, [ctypes.c_int]),
    "wkeIsDocumentReady":       (ctypes.c_int, [ctypes.c_int]),
    "wkeGetURL":                (ctypes.c_void_p, [ctypes.c_int]),
    "wkeGetTitleW":             (ctypes.c_void_p, [ctypes.c_int]),
    "wkeGetSource":             (ctypes.c_void_p, [ctypes.c_int]),
    # ---- 执行 JS ------------------------------------------------------------
    "wkeRunJSW":                (ctypes.c_longlong, [ctypes.c_int, ctypes.c_wchar_p]),
    "wkeJsBindFunction":        (None, [ctypes.c_char_p, JS_CB, ctypes.c_void_p,
                                        ctypes.c_int]),
    # ---- 事件 ---------------------------------------------------------------
    "wkeOnDocumentReady2":      (None, [ctypes.c_int, DOC_READY2_CB, ctypes.c_void_p]),
    "wkeOnConsole":             (None, [ctypes.c_int, CONSOLE_CB, ctypes.c_void_p]),
    "wkeOnLoadingFinish":       (None, [ctypes.c_int, LOADING_FINISH_CB, ctypes.c_void_p]),
    "wkeOnTitleChanged":        (None, [ctypes.c_int, TITLE_CHANGED_CB, ctypes.c_void_p]),
    "wkeOnURLChanged2":         (None, [ctypes.c_int, URL_CHANGED_CB, ctypes.c_void_p]),
    "wkeOnPaintUpdated":        (None, [ctypes.c_int, PAINT_CB, ctypes.c_void_p]),
    # ---- 输入转发 -----------------------------------------------------------
    "wkeFireWindowsMessage":    (ctypes.c_int, [ctypes.c_int, wintypes.HWND, wintypes.UINT,
                                                _WPARAM, _LPARAM, ctypes.c_void_p]),
    "wkeFireMouseEvent":        (ctypes.c_int, [ctypes.c_int, ctypes.c_uint, ctypes.c_int,
                                                ctypes.c_int, ctypes.c_uint]),
    "wkeFireMouseWheelEvent":   (ctypes.c_int, [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                                ctypes.c_int, ctypes.c_uint]),
    "wkeFireKeyDownEvent":      (ctypes.c_int, [ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                                ctypes.c_int]),
    "wkeFireKeyUpEvent":        (ctypes.c_int, [ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                                ctypes.c_int]),
    "wkeFireKeyPressEvent":     (ctypes.c_int, [ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                                ctypes.c_int]),
    "wkeFireContextMenuEvent":  (ctypes.c_int, [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                                ctypes.c_uint]),
    # ---- 随 view 存自定义值（易模块就用它存老窗口过程） --------------------
    "wkeSetUserKeyValue":       (None, [ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p]),
    "wkeGetUserKeyValue":       (ctypes.c_void_p, [ctypes.c_int, ctypes.c_char_p]),
    # ---- JS 值 -------------------------------------------------------------
    "jsArg":                    (ctypes.c_longlong, [ctypes.c_void_p, ctypes.c_int]),
    "jsArgCount":               (ctypes.c_int, [ctypes.c_void_p]),
    "jsArgType":                (ctypes.c_int, [ctypes.c_void_p, ctypes.c_int]),
    # ★ 这几个「造值」的函数**不收** es（gen_thunk.c: `long long __cdecl jsInt(unsigned a0)`）
    "jsUndefined":              (ctypes.c_longlong, []),
    "jsNull":                   (ctypes.c_longlong, []),
    "jsTrue":                   (ctypes.c_longlong, []),
    "jsFalse":                  (ctypes.c_longlong, []),
    "jsBoolean":                (ctypes.c_longlong, [ctypes.c_int]),
    "jsInt":                    (ctypes.c_longlong, [ctypes.c_int]),
    "jsFloat":                  (ctypes.c_longlong, [ctypes.c_float]),
    "jsDouble":                 (ctypes.c_longlong, [ctypes.c_double]),
    "jsStringW":                (ctypes.c_longlong, [ctypes.c_void_p, ctypes.c_wchar_p]),
    "jsEvalW":                  (ctypes.c_longlong, [ctypes.c_void_p, ctypes.c_wchar_p]),
    "jsToBoolean":              (ctypes.c_int, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsToInt":                  (ctypes.c_int, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsToDouble":               (ctypes.c_double, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsToFloat":                (ctypes.c_float, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsToStringW":              (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsToTempStringW":          (ctypes.c_void_p, [ctypes.c_void_p, ctypes.c_longlong]),
    "jsTypeOf":                 (ctypes.c_int, [ctypes.c_longlong]),
}

# 供静态自检：所有被引用的导出名都必须在这里
_EXPORTS = {}
for _n, (_r, _a) in _STD_TABLE.items():
    _EXPORTS[_n] = (_S, _r, _a)
for _n, (_r, _a) in _CDECL_TABLE.items():
    _EXPORTS[_n] = (_C, _r, _a)
del _n, _r, _a
del _S, _C


# ---------------------------------------------------------------- DLL 定位

def _module_path(hm):
    """由 HMODULE 取完整路径"""
    try:
        buf = ctypes.create_unicode_buffer(1024)
        n = win32().GetModuleFileNameW(wintypes.HMODULE(hm), buf, 1024)
        if n:
            return buf.value
    except Exception:
        pass
    return ""


def _candidates(dll_path=None):
    """按优先级返回 [(说明, 路径)]；路径为 "" 表示走 LoadLibrary 的默认搜索"""
    out = []
    if dll_path:
        out.append(("显式指定", os.path.abspath(str(dll_path))))
    env = os.environ.get(ENV_DLL)
    if env:
        out.append(("环境变量 %s" % ENV_DLL, os.path.abspath(env)))
    hm = 0
    try:
        hm = int(win32().GetModuleHandleW(MBK_DLL_NAME) or 0)
    except Exception:
        hm = 0
    if hm:
        out.append(("进程内已载入", _module_path(hm)))
    host = _host_exe_dir()
    if host:
        out.append(("宿主 exe 目录", os.path.join(host, MBK_DLL_NAME)))
    try:
        here = os.path.dirname(os.path.abspath(__file__))
        out.append(("插件目录", os.path.join(here, MBK_DLL_NAME)))
    except Exception:
        pass
    for env_name in ("TEMP", "TMP"):
        tmp = os.environ.get(env_name)
        if tmp:
            out.append(("%" + env_name + "%\\ElangMbKit",
                        os.path.join(tmp, "ElangMbKit", MBK_DLL_NAME)))
            break
    out.append(("系统搜索顺序", MBK_DLL_NAME))
    return out


def find_dll(dll_path=None):
    """找 MbKit146.dll，返回第一个真正存在的完整路径；找不到抛 MbKitNotFoundError。

    优先用「进程内已载入」的那一份：同一个进程里只有最先载入的 MbKit 生效。
    """
    tried = []
    loaded = ""
    picked = ""
    for how, path in _candidates(dll_path):
        if how == "进程内已载入":
            loaded = path
        if path and path != MBK_DLL_NAME and os.path.isfile(path):
            tried.append("  * [%s] %s  <-- 命中" % (how, path))
            picked = path
            break
        tried.append("  . [%s] %s" % (how, path))

    if not picked and loaded:
        picked = loaded          # 已载入但取不到路径：直接按模块名再取一次
    if not picked:
        raise MbKitNotFoundError(
            "没找到 %s（内核宿主库）。请把它放到插件目录里一起打包。\n"
            "也可以用 WebView(dll_path=r'...\\%s') 或设置环境变量 %s 指定。\n"
            "已尝试的路径：\n%s"
            % (MBK_DLL_NAME, MBK_DLL_NAME, ENV_DLL, "\n".join(tried)))
    return picked


# ---------------------------------------------------------------- 文本工具

def _read_wstr(addr):
    """读宽字符串指针"""
    if not addr:
        return ""
    try:
        return ctypes.wstring_at(int(addr))
    except Exception:
        return ""


def _read_str(addr):
    """读窄字符串指针：先按 UTF-8，失败退 GBK（内核里两种都可能出现）"""
    if not addr:
        return ""
    try:
        head = ctypes.string_at(int(addr), 2)
    except Exception:
        return ""
    if len(head) == 2 and head[1] == 0 and head[0] not in (0,):
        return _read_wstr(addr)
    try:
        raw = ctypes.string_at(int(addr))
    except Exception:
        return ""
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("latin-1")


def _decode(b):
    """c_char_p 拿到的 bytes -> str"""
    if b is None:
        return ""
    if isinstance(b, str):
        return b
    for enc in ("utf-8", "gbk"):
        try:
            return b.decode(enc)
        except UnicodeDecodeError:
            continue
    return b.decode("latin-1")


# ---------------------------------------------------------------- MbKit 单例

class MbKit(object):
    """MbKit146.dll 的装载器与全局状态入口（一个进程一份，用 get() 取单例）

        k = MbKit.get()
        k.configure_kernel(node_path=r"D:\\node", force_node=True)   # 可选，必须在启动前
        k.start()                                                     # 启动内核 + 等就绪
        print(k.diag())
    """

    _instance = None
    _lock = threading.Lock()

    # ------------------------------------------------------------ 构造

    def __init__(self, dll_path=None, *, strict_version=True):
        self._lock = threading.RLock()
        self.path = find_dll(dll_path)
        self._std = None
        self._cdecl = None
        self._fns = {}                 # 名字 -> 函数对象（已钉好 argtypes）
        self._inited = False
        self._start_rc = None
        self._version = ""
        self._version_ok = True
        self._strict_version = bool(strict_version)
        self.warnings = []

        self._load(self.path, strict_version=strict_version)

    def _bind(self, lib, table):
        for name, (restype, argtypes) in table.items():
            try:
                fn = getattr(lib, name)
            except AttributeError:
                # 少导出：不是致命错误，但要用的时候会明确报错
                self.warnings.append("MbKit146.dll 缺少导出 %s" % name)
                continue
            fn.restype = restype
            fn.argtypes = argtypes
            self._fns[name] = fn

    def _load(self, path, strict_version=True):
        # 同一个文件同时用 WinDLL(stdcall) 与 CDLL(cdecl) 打开：
        # 系统只会映射一份镜像（引用计数 +1），函数地址完全相同。
        try:
            self._std = ctypes.WinDLL(path)
            self._cdecl = ctypes.CDLL(path)
        except OSError as e:
            raise MbKitNotFoundError(
                "载入 %s 失败：%s\n"
                "常见原因：① 该 DLL 是 32 位（Win32）编译的，而当前 Python 是 %d 位 —— "
                "miniblink 内核只有 32 位版本，必须用宿主自带的 32 位 Python；"
                "② 杀软/组策略拦截了 LoadLibrary；③ 文件被截断或不是 PE 文件。"
                % (path, e, 64 if _IS64 else 32))
        self._bind(self._std, _STD_TABLE)
        self._bind(self._cdecl, _CDECL_TABLE)

        self._version = self._call_str("MbKit_Version")
        if self._version and MBK_VERSION_TAG not in self._version:
            self._version_ok = False
            msg = ("进程里这份 MbKit146.dll 的版本串是「%s」，本 SDK 是按 %s 写的。"
                   "多个插件各带一份 MbKit 时只有最先载入的那份生效，请统一版本。"
                   % (self._version, MBK_VERSION_TAG))
            self.warnings.append(msg)
            if strict_version:
                warnings.warn(msg, RuntimeWarning, stacklevel=3)
        try:
            import xlz_sdk  # noqa: F401  —— 顺手确认同目录依赖在，缺了给个更直白的错
        except Exception:
            pass

    # ------------------------------------------------------------ 单例

    @classmethod
    def get(cls, dll_path=None, **kw):
        """取（并首次创建）全局单例；dll_path 只在第一次有意义"""
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = cls(dll_path, **kw)
        return cls._instance

    @classmethod
    def loaded(cls):
        """是否已经有单例"""
        return cls._instance is not None

    # ------------------------------------------------------------ 底层访问

    def fn(self, name):
        """取一个已钉好签名的导出函数（名字必须是 _STD_TABLE/_CDECL_TABLE 里的键）"""
        try:
            return self._fns[name]
        except KeyError:
            if name not in _EXPORTS:
                raise MbKitError(
                    "「%s」不在本模块的导出表里。要新增内核命令，先把它加进 "
                    "_STD_TABLE 或 _CDECL_TABLE 并钉好 argtypes。" % name)
            raise MbKitError("MbKit146.dll 里没有可用的导出 %s（版本不匹配？）" % name)

    def _call_str(self, name):
        try:
            return _decode(self.fn(name)())
        except Exception:
            return ""

    def _call_int(self, name, *args):
        try:
            return int(self.fn(name)(*args))
        except Exception:
            return -1

    # ------------------------------------------------------------ 内核选择

    def version(self):
        """宿主库版本串，如 "MbKit/1.4.5" """
        return self._version

    def version_ok(self):
        """版本串是否包含 MbKit/1.4.5（内核起来之前版本串是空的，那时一律算「未判定」= True）"""
        return self._version_ok

    def refresh_version(self):
        """重读版本串并复核（★ 内核起来之前 MbKit_Version() 返回空串，载入时根本判不了）"""
        v = self._call_str("MbKit_Version")
        if not v or v == self._version:
            return self._version
        self._version = v
        self._version_ok = MBK_VERSION_TAG in v
        if not self._version_ok:
            msg = ("进程里这份 MbKit146.dll 的版本串是「%s」，本 SDK 是按 %s 写的。"
                   "多个插件各带一份 MbKit 时只有最先载入的那份生效，请统一版本。"
                   % (v, MBK_VERSION_TAG))
            if msg not in self.warnings:
                self.warnings.append(msg)
                if self._strict_version:
                    warnings.warn(msg, RuntimeWarning, stacklevel=2)
        return self._version

    def diag(self):
        """宿主层一行式诊断（排查「内核没加载 / 网页空白」先看它）"""
        return self._call_str("MbKit_Diag")

    def last_error(self):
        """内核最近一次错误文本（很多命令失败时原因写在这里）"""
        return self._call_str("MbKit_LastError")

    def configure_kernel(self, node_path=None, *, force_node=False):
        """选内核：node_path = node.dll 所在目录或完整文件名；force_node = 不用 WebView2。

        返回 (ok, reason)。**必须在内核加载之前调用**，加载之后再调用会返回 (False, 原因)。
        内核一旦加载就常驻进程，「要换内核只能重启宿主 exe」。
        """
        with self._lock:
            if force_node:
                if self._call_int("MbKit_SetForceNodeKernel", 1) == 0:
                    return False, ("内核已经加载（或上次加载已失败），进程内只允许一份内核 —— "
                                   "要换内核只能重启宿主 exe")
            if node_path is not None:
                if self._call_int("MbKit_SetNodePath", _enc_path(node_path)) == 0:
                    return False, ("内核已经加载（或上次加载已失败），进程内只允许一份 node.dll —— "
                                   "要换内核只能重启宿主 exe")
            return True, ""

    def node_path(self):
        """当前实际使用的 node.dll 路径（会触发内核查找与加载）"""
        return self._call_str("MbKit_NodePath")

    def config_node_path(self):
        """自定义的 node 路径（未设置时为空串）"""
        return self._call_str("MbKit_ConfigNodePath")

    def webview2_supported(self):
        """本机是否装了 WebView2 Runtime（只读探测，不加载内核、不改设置）"""
        return self._call_int("MbKit_HasWebViewEnv") == 1

    def kernel_mode(self):
        """0 = 未就绪/上次失败，1 = node.dll，2 = edgewebview（WebView2）"""
        return self._call_int("MbKit_KernelMode")

    def kernel_mode_name(self):
        return {KERNEL_NONE: "未就绪", KERNEL_NODE: "node.dll",
                KERNEL_WEBVIEW2: "edgewebview"}.get(self.kernel_mode(), "未知")

    def kernel_loaded(self):
        """内核 DLL 是否已映射进进程"""
        return self._call_int("MbKit_KernelLoaded") == 1

    def boot_state(self):
        """内核启动状态机：0 未开始 1 进行中 2 成功 3 失败"""
        return self._call_int("MbKit_BootState")

    def is_ready(self):
        """内核是否已经可以被使用"""
        return self._call_int("MbKit_IsReady") == 1

    def wait_timeouts(self):
        """累计「等内核启动超时」次数（>0 说明卡在加载锁上）"""
        return self._call_int("MbKit_WaitTimeouts")

    def pump_count(self):
        """累计内核泵消息次数"""
        return self._call_int("MbKit_PumpCount")

    # ------------------------------------------------------------ 启动

    def start(self, wait_ms=DEFAULT_WAIT_MS, node_path=None, force_node=False):
        """启动内核并等就绪。返回 (ok, 说明)。

        幂等：重复调用只做一次真正的启动。**别在 DllMain / _启动子程序 里调**——
        那时加载锁由自己持有，任何线程都加载不了 node.dll（内核会报「等内核启动超时」）。
        """
        with self._lock:
            if self._inited:
                return True, "已启动"
            ok, reason = self.configure_kernel(node_path, force_node=force_node)
            if not ok:
                return False, reason
            rc = self._call_int("MbKit_Init", None)
            if rc != MBKIT_OK:
                return False, ("MbKit_Init 失败（rc=%d）：%s"
                               % (rc, self.last_error() or "无更多信息"))
            rc = self._call_int("MbKit_WaitKernel", int(wait_ms))
            self._start_rc = rc
            if rc != MBKIT_OK:
                return False, ("等内核就绪失败（rc=%d / 启动状态=%d / 超时次数=%d）：%s\n%s"
                               % (rc, self.boot_state(), self.wait_timeouts(),
                                  self.last_error() or "无更多信息", self.diag()))
            self._inited = True
            # 版本串要等内核起来才有内容（MbKit_Version 在 initialize 之前返回空串），
            # 所以真正的 1.4.5 校验发生在这里。
            self.refresh_version()
            return True, "就绪：%s 内核" % self.kernel_mode_name()

    def ensure_started(self, wait_ms=DEFAULT_WAIT_MS):
        """没启动就启动，抛 MbKitError 而不是返回 False"""
        ok, msg = self.start(wait_ms)
        if not ok:
            raise MbKitError("miniblink 内核没能启动 —— " + msg)
        return True

    def pump(self, ms=0):
        """泵一次内核消息（离屏模式 hwnd=0 时需要自己调；窗口模式由窗口消息驱动）"""
        return self._call_int("MbKit_Pump", int(ms))

    # ------------------------------------------------------------ 收摊

    def live_count(self):
        """内核里还活着的 view 数"""
        return self._call_int("MbKit_LiveCount")

    def detach_all(self):
        """内核侧全部解绑（兜底）。Python 侧的 WebView 请走模块级 detach_all()"""
        return self._call_int("MbKit_DetachAll")

    def cb_stats(self):
        """内核回调统计（CbHits 成功转发 / CbBlocked 被存活校验挡下 / Suppressed 合计）"""
        return {
            "count": self._call_int("MbKit_CbCount"),
            "live": self._call_int("MbKit_CbLive"),
            "hits": self._call_int("MbKit_CbHits"),
            "blocked": self._call_int("MbKit_CbBlocked"),
            "suppressed": self._call_int("MbKit_SuppressedCount"),
        }

    def self_diag(self):
        """多行自检报告，出问题时直接贴出来"""
        lines = [
            "MbKit 宿主库：%s" % self.path,
            "版本：%s%s" % (self.version() or "(空)",
                            "" if self.version_ok() else "  ★ 与本 SDK 期望的 %s 不一致"
                            % MBK_VERSION_TAG),
            "内核模式：%d（%s）  已载入=%s  就绪=%s  启动状态=%d"
            % (self.kernel_mode(), self.kernel_mode_name(),
               self.kernel_loaded(), self.is_ready(), self.boot_state()),
            "node 路径：%s" % (self.node_path() or "(未确定)"),
            "自定义 node 路径：%s" % (self.config_node_path() or "(未设置)"),
            "WebView2 Runtime：%s" % ("有" if self.webview2_supported() else "无"),
            "存活 view：%d  等待超时次数：%d  泵消息次数：%d"
            % (self.live_count(), self.wait_timeouts(), self.pump_count()),
            "回调统计：%s" % (self.cb_stats(),),
            "最近错误：%s" % (self.last_error() or "(无)"),
            "宿主自检：%s" % (self.diag() or "(空)"),
            "WebView2 自检：%s" % (self._call_str("MbKit_WebViewDiag") or "(空)"),
        ]
        for w in self.warnings:
            lines.append("⚠ " + w)
        return "\n".join(lines)


def _enc_path(p):
    """路径 -> ANSI/UTF-8 bytes（MbKit 收 const char*，Windows 上 ANSI 才是稳妥的）"""
    if p is None:
        return None
    if isinstance(p, bytes):
        return p
    p = str(p)
    try:
        return p.encode("mbcs")
    except Exception:
        return p.encode("utf-8", "replace")


def kernel():
    """取全局 MbKit 单例（第一次调用才真正载入 DLL）"""
    return MbKit.get()


def kernel_version():
    """宿主库版本串（找不到 DLL 时返回空串，不抛）"""
    try:
        return kernel().version()
    except MbKitError:
        return ""


def kernel_diag():
    """宿主层诊断文本（找不到 DLL 时返回错误说明，不抛）"""
    try:
        return kernel().diag()
    except MbKitError as e:
        return "MbKit146.dll 不可用：%s" % e


# ---------------------------------------------------------------- WebView

_VIEW_SEQ = [0]
_VIEWS = []                        # 强引用，防 GC（也用于 detach_all）
_VIEWS_LOCK = threading.Lock()
_WINDOW_OWNER = {}                 # hwnd -> WebView（防两个对象抢同一个窗口）
_UNLOADING = [False]               # 卸载中标志：置起来之后一切回调直接返回
_PROP_NAME = b"xlzWebUI"           # 标记属性，用来发现「窗口已被别的 webui 接管」
_WM_APP_POST = 0x8000 + 0x214E     # WM_APP + 5358，post() 用


def is_unloading():
    """是否处在「插件正在卸载」的窗口里"""
    return _UNLOADING[0]


def begin_unload():
    """进入卸载态：之后内核再回调进来一律立刻返回（热重载契约第 0 步）"""
    _UNLOADING[0] = True


def end_unload():
    """重新上膛（apprun/on_enable 里调，比如插件被禁用后又启用）"""
    _UNLOADING[0] = False


def live_views():
    """当前还活着的 WebView 列表（快照）"""
    with _VIEWS_LOCK:
        return [v for v in _VIEWS if v.is_alive()]


class WebView(object):
    """一个嵌在窗口里的网页视图

        wv = WebView()                       # 也可以 WebView(dll_path=..., transparent=True)
        wv.bind(hwnd)                        # ★ 必须在窗口所属线程里
        wv.load_html("<h1>hi</h1>", base_url=None)
        wv.on_document_ready2(lambda url: print("就绪", url))
        wv.run_js("1+1")
        wv.destroy()
    """

    # ------------------------------------------------------------ 构造

    def __init__(self, hwnd=0, *, dll_path=None, transparent=False,
                 input_hook=True, check_thread=True, strict_version=True):
        _VIEW_SEQ[0] += 1
        self.id = _VIEW_SEQ[0]
        self._alive = False
        self._view = 0                 # wke 句柄（0 = 还没建）
        self._hwnd = int(hwnd or 0)
        self._owner_tid = 0            # ★ 拥有本 view 的线程
        self._transparent = bool(transparent)
        self._input_hook = bool(input_hook)
        self._check_thread = bool(check_thread)

        self._mb = MbKit.get(dll_path, strict_version=strict_version)

        # 回调强引用（活着的放这里，销毁时整体挪进墓地）
        self._cbs = {}                 # 名字 -> ctypes 回调对象
        self._hooks = {}               # 事件名 -> 用户回调
        self._js_funcs = {}            # JS 函数名 -> 用户回调

        # 子类化相关
        self._old_proc = 0
        self._wndproc = None
        self._subclassed_hwnd = 0
        self._builtin_msg = False      # 是不是我们自己建的消息窗口

        # post() 用的隐藏窗口
        self._msg_hwnd = 0
        self._msg_wndproc = None
        self._post_queue = []
        self._post_lock = threading.Lock()

        with _VIEWS_LOCK:
            _VIEWS.append(self)

        if self._hwnd:
            self.bind(self._hwnd, transparent=self._transparent,
                      input_hook=self._input_hook)

    # ------------------------------------------------------------ 状态

    def is_alive(self):
        """Python 侧是否还活着（销毁后立刻变 False，回调全部失效）"""
        return self._alive

    def handle(self):
        """wke 的 view 句柄（0 = 还没建）"""
        return self._view

    def hwnd(self):
        """绑定的窗口句柄"""
        return self._hwnd

    def owner_thread_id(self):
        """拥有本 view 的线程 ID"""
        return self._owner_tid

    def in_owner_thread(self):
        """当前线程是不是 owner 线程"""
        return not self._owner_tid or current_thread_id() == self._owner_tid

    def _mb_call(self, name, *args):
        """调用内核命令；已销毁或不在 owner 线程就不碰（除非是标记了线程安全的）"""
        if not self._alive or not self._view:
            return -1
        return int(self._mb.fn(name)(*args))

    def _require_alive(self, what):
        if not self._alive or not self._view:
            raise MbKitError("webui 尚未 bind() 或已经 destroy()，不能%s" % what)

    def _warn_offthread(self, what):
        """非 owner 线程的操作：给一次明确告警（不抛，避免打断清理流程）"""
        if self._check_thread and not self.in_owner_thread():
            msg = ("%s 在非 owner 线程里调用（本 view 属线程 %s，当前线程 %s）。"
                   "绘制与窗口消息属于 owner 线程，跨线程请用 WebView.post()。"
                   % (what, self._owner_tid, current_thread_id()))
            self._mb.warnings.append(msg)
            warnings.warn(msg, RuntimeWarning, stacklevel=3)
            return False
        return True

    # ------------------------------------------------------------ 绑定 / 销毁

    def bind(self, hwnd=0, *, transparent=None, input_hook=None, raise_on_thread=True):
        """把 view 绑到窗口上。返回 True。

        ★ 必须在「拥有该窗口的那个线程」里调用（宿主窗口 = 宿主 UI 线程）。
          不一致时抛 MbKitThreadError —— 强行绑上去只会得到一块灰屏。

        hwnd=0 表示离屏模式（不绑窗口，要自己 pump + wkePaint 取位图）。
        """
        if transparent is not None:
            self._transparent = bool(transparent)
        if input_hook is not None:
            self._input_hook = bool(input_hook)
        hwnd = int(hwnd or 0)

        if self._alive:
            self.destroy()

        cur = current_thread_id()
        if hwnd:
            if not win32().IsWindow(wintypes.HWND(hwnd)):
                raise MbKitError("窗口句柄 0x%X 不存在（IsWindow 失败）" % hwnd)
            owner = window_thread_id(hwnd)
            if owner and owner != cur:
                msg = ("线程铁律：这个窗口属于线程 %d，而你在线程 %d 里调用 bind()。"
                       "miniblink 的 view 属于创建它的线程，跨线程建 view 会得到一块灰屏"
                       "（内核会拒绝并写「内核属于线程 X（兼容模式，非编组），当前线程 Y 无法建 view」）。"
                       "请把这个调用挪到窗口所属线程：宿主窗口用 post/回调派发到 UI 线程，"
                       "或者在自己的消息循环里处理 WM_APP。"
                       % (owner, cur))
                if raise_on_thread:
                    raise MbKitThreadError(msg)
                warnings.warn(msg, RuntimeWarning, stacklevel=2)
                self._mb.warnings.append(msg)
            # 一个窗口只能有一个浏览框（两个 WndProc 抢同一个窗口必然出事）
            with _VIEWS_LOCK:
                other = _WINDOW_OWNER.get(hwnd)
            if other is not None and other is not self and other.is_alive():
                raise MbKitError("窗口 0x%X 已被另一个 WebView(#%d) 占用，先 destroy 它"
                                 % (hwnd, other.id))
            self._owner_tid = owner or cur
        else:
            self._owner_tid = cur

        # 1) 内核就绪
        self._mb.ensure_started()
        # 2) 建 view
        view = int(self._mb.fn("wkeCreateWebView")())
        if not view:
            raise MbKitError(
                "wkeCreateWebView() 返回 0：内核没能建出 view。\n"
                "最常见原因是上面那条线程铁律，其次是内核启动失败。\n%s"
                % self._mb.self_diag())
        self._view = view
        self._alive = True
        self._hwnd = hwnd

        try:
            if hwnd:
                # 3) 透明 + 绑定窗口
                self._mb.fn("wkeSetUserKeyValue")(view, b"xlzHwnd", hwnd)
                if self._transparent:
                    self._mb.fn("wkeSetTransparent")(view, 1)
                    ex = int(win32().GetWindowLong(wintypes.HWND(hwnd), -20) or 0)
                    win32().SetWindowLong(wintypes.HWND(hwnd), -20,
                                          ctypes.c_void_p(ex | 0x00080000))  # WS_EX_LAYERED
                else:
                    self._mb.fn("wkeSetTransparent")(view, 0)
                self._mb.fn("wkeSetHandle")(view, hwnd)

                # 4) 绘制回调（窗口模式的命门：没有它窗口就是一块灰）
                self._install_paint()

                # 5) 窗口过程子类化（鼠标键盘、WM_PAINT 搬运）
                if self._input_hook:
                    self._install_wndproc(hwnd)

                # 6) 按客户区大小初始化内核尺寸
                rc = RECT()
                if win32().GetClientRect(wintypes.HWND(hwnd), ctypes.byref(rc)):
                    w = max(1, rc.right - rc.left)
                    h = max(1, rc.bottom - rc.top)
                    self._mb.fn("wkeResize")(view, w, h)
                    self._mb.fn("wkeRepaintIfNeeded")(view)

                # 7) post() 用的隐藏消息窗口
                self._ensure_msg_window()
            else:
                self._mb.fn("wkeSetHeadlessEnabled")(view, 1)

            with _VIEWS_LOCK:
                _WINDOW_OWNER[hwnd] = self
            return True
        except Exception:
            # 半路失败：立刻收摊，别留半死不活的 view
            try:
                self.destroy()
            except Exception:
                pass
            raise

    def destroy(self):
        """销毁 view：解绑回调 -> 还原窗口过程 -> 摘掉窗口 -> 内核回收。

        可以重复调用；不会抛异常（清理路径必须绝对安全）。
        """
        if not self._view and not self._alive:
            self._unregister()
            return True
        # 先立 flag：之后内核即使因为竞态又回调进来，thunk 里第一句就返回
        self._alive = False
        view = self._view
        hwnd = self._hwnd

        if self._check_thread and self._owner_tid and \
                current_thread_id() != self._owner_tid:
            warnings.warn(
                "destroy() 在非 owner 线程里调用（view 属线程 %s，当前 %s）。"
                "已跳过窗口过程还原，view 交给内核延迟回收。"
                % (self._owner_tid, current_thread_id()), RuntimeWarning, stacklevel=2)
            skip_window = True
        else:
            skip_window = False

        # 1) 收回所有回调（对象挪进墓地，指针永不失效）
        for name, cb in list(self._cbs.items()):
            _retire(cb)
        self._cbs.clear()
        self._hooks.clear()
        self._js_funcs.clear()

        # 2) 还原窗口过程（必须在 wkeSetHandle(0) 之前，否则老的 proc 可能已被覆盖）
        if not skip_window:
            try:
                self._remove_wndproc()
            except Exception:
                pass

        # 3) 解绑窗口 + 销毁 view
        if view:
            try:
                self._mb.fn("wkeSetHandle")(view, None)
            except Exception:
                pass
            try:
                self._mb.fn("wkeDestroyWebView")(view)
            except Exception:
                pass
            try:
                self._mb.fn("MbKit_DetachView")(view)   # 内核侧再兜一层
            except Exception:
                pass

        # 4) 收掉隐藏消息窗口
        if not skip_window:
            try:
                self._remove_msg_window()
            except Exception:
                pass

        self._view = 0
        self._hwnd = 0
        self._old_proc = 0
        self._subclassed_hwnd = 0
        self._unregister()
        return True

    def _unregister(self):
        with _VIEWS_LOCK:
            if self in _VIEWS:
                try:
                    _VIEWS.remove(self)
                except ValueError:
                    pass
            for k, v in list(_WINDOW_OWNER.items()):
                if v is self:
                    _WINDOW_OWNER.pop(k, None)

    # 兼容写法：有些人习惯 close()
    close = destroy

    # ------------------------------------------------------------ 绘制

    def _install_paint(self):
        """窗口模式必需：内核画完通知我们，我们把内容送到窗口上

        分层窗口（透明）走 UpdateLayeredWindow；普通窗口只 InvalidateRect，
        真正的搬运在 WM_PAINT 里（wkeGetViewDC + BitBlt）—— 与易模块完全一致。
        """
        def _on_paint(view, param, hdc, x, y, cx, cy):
            if not self._alive or is_unloading():
                return
            hwnd = int(param or 0)
            if not hwnd:
                return
            try:
                if self._transparent:
                    self._paint_layered(hwnd, int(hdc or 0))
                else:
                    rc = RECT(x, y, x + cx, y + cy)
                    win32().InvalidateRect(wintypes.HWND(hwnd), ctypes.byref(rc), True)
            except Exception:
                pass

        cb = PAINT_CB(_on_paint)
        self._cbs["paint"] = cb
        self._mb.fn("wkeOnPaintUpdated")(self._view, cb, self._hwnd)

    def _paint_layered(self, hwnd, hdc):
        """分层窗口的送图：UpdateLayeredWindow(hdc) ，失败再走内存 DC 中转"""
        w = win32()
        screen = w.GetDC(wintypes.HWND(hwnd))
        if not screen:
            return
        try:
            bmp = w.GetCurrentObject(wintypes.HDC(hdc), 0)      # OBJ_BITMAP
            bm = BITMAP()
            size = SIZE(0, 0)
            if bmp and w.GetObjectA(bmp, ctypes.sizeof(BITMAP), ctypes.byref(bm)):
                size.cx, size.cy = bm.bmWidth, bm.bmHeight
            if not size.cx or not size.cy:
                rc = RECT()
                w.GetClientRect(wintypes.HWND(hwnd), ctypes.byref(rc))
                size.cx, size.cy = rc.right - rc.left, rc.bottom - rc.top
            blend = BLENDFUNCTION(0, 0, 255, 1)                 # AC_SRC_ALPHA
            ok = w.UpdateLayeredWindow(wintypes.HWND(hwnd), screen, None,
                                       ctypes.byref(size), wintypes.HDC(hdc), None,
                                       0, ctypes.byref(blend), 2)   # ULW_ALPHA
            if not ok:
                mem = w.CreateCompatibleDC(screen)
                bmp2 = w.CreateCompatibleBitmap(screen, size.cx, size.cy)
                old = w.SelectObject(mem, bmp2)
                w.BitBlt(mem, 0, 0, size.cx, size.cy, wintypes.HDC(hdc), 0, 0, 0x00CC0020)
                w.BitBlt(wintypes.HDC(hdc), 0, 0, size.cx, size.cy, mem, 0, 0, 0x00CC0020)
                w.UpdateLayeredWindow(wintypes.HWND(hwnd), screen, None,
                                      ctypes.byref(size), mem, None, 0,
                                      ctypes.byref(blend), 2)
                if old:
                    w.SelectObject(mem, old)
                if bmp2:
                    w.DeleteObject(bmp2)
                if mem:
                    w.DeleteDC(mem)
        finally:
            w.ReleaseDC(wintypes.HWND(hwnd), screen)

    def repaint(self):
        """强制重画一帧（改了窗口大小/内容后想立刻看到效果时用）"""
        if not self._alive or not self._view:
            return
        self._mb.fn("wkeRepaintIfNeeded")(self._view)
        if self._hwnd:
            try:
                win32().InvalidateRect(wintypes.HWND(self._hwnd), None, True)
            except Exception:
                pass

    def resize(self, width, height):
        """内核视口尺寸（窗口模式下 WM_SIZE 会自动调，一般不用手调）"""
        self._require_alive("调整尺寸")
        self._mb.fn("wkeResize")(self._view, int(width), int(height))
        self._mb.fn("wkeRepaintIfNeeded")(self._view)

    def view_size(self):
        """(宽, 高)，内核视口尺寸"""
        if not self._alive or not self._view:
            return (0, 0)
        return (int(self._mb.fn("wkeGetWidth")(self._view)),
                int(self._mb.fn("wkeGetHeight")(self._view)))

    # ------------------------------------------------------------ 窗口过程子类化

    def _install_wndproc(self, hwnd):
        """接管窗口过程：转发输入 + 在 WM_PAINT 里 BitBlt

        与易模块 MB浏览框 的 `_子类化` / `_SubProc` 等价：
          SetWindowLongW(hwnd, GWLP_WNDPROC, &WndProc)，老过程存起来最后兜底调用。
        已经有的标记属性 = 窗口被别人接管了，直接报错（两个 WndProc 互抢必然花屏/死锁）。
        """
        w = win32()
        h = wintypes.HWND(hwnd)
        if w.GetPropA(h, _PROP_NAME):
            raise MbKitError(
                "窗口 0x%X 上已经有别的 webui / 宿主逻辑接过了窗口过程（标记属性 %s 存在）。"
                "请先用 WebView(input_hook=False) 自己转发消息，或先销毁那个浏览框。"
                % (hwnd, _PROP_NAME.decode()))

        proc = WNDPROC(self._wndproc_thunk)
        self._wndproc = proc
        self._cbs["wndproc"] = proc
        old = w.SetWindowLong(h, -4, ctypes.cast(proc, ctypes.c_void_p))
        self._old_proc = int(old or 0)
        self._subclassed_hwnd = hwnd
        # 存两份：进程内自查用（我们自己的字典），跨模块互斥用（窗口属性）
        self._mb.fn("wkeSetUserKeyValue")(self._view, b"xlzOldProc", self._old_proc)
        w.SetPropA(h, _PROP_NAME, ctypes.c_void_p(self.id))

    def _remove_wndproc(self):
        """还原窗口过程（必须在 destroy 里、wkeSetHandle(0) 之前）"""
        if not self._subclassed_hwnd:
            return
        w = win32()
        h = wintypes.HWND(self._subclassed_hwnd)
        try:
            if self._old_proc:
                w.SetWindowLong(h, -4, ctypes.c_void_p(self._old_proc))
            w.RemovePropA(h, _PROP_NAME)
        finally:
            self._subclassed_hwnd = 0

    def _call_old(self, hwnd, msg, wp, lp):
        """兜底：调原来的窗口过程（没有就 DefWindowProc）"""
        w = win32()
        try:
            if self._old_proc:
                return int(w.CallWindowProcW(ctypes.c_void_p(self._old_proc),
                                             hwnd, msg, wp, lp))
            return int(w.DefWindowProcW(hwnd, msg, wp, lp))
        except Exception:
            return 0

    def _wndproc_thunk(self, hwnd, msg, wp, lp):
        """★ 窗口过程：每次鼠标移动都会进来，所以这里要短、要稳、绝不能让异常逃出去。"""
        try:
            if not self._alive or is_unloading():
                return self._call_old(hwnd, msg, wp, lp)
            r = self._on_message(int(hwnd), int(msg), int(wp), int(lp))
            if r is None:
                return self._call_old(hwnd, msg, wp, lp)
            return int(r)
        except Exception:
            return self._call_old(hwnd, msg, wp, lp)

    def _on_message(self, hwnd, msg, wp, lp):
        """消息分发：返回 None = 交回原过程，返回整数 = 直接返回该值

        这一段的每条分支都对应易模块 MB浏览框._WndProc 里的一个 .判断，别乱改顺序。
        """
        w = win32()
        view = self._view

        # ---- WM_PAINT(15)：把内核离屏画好的位图搬到窗口上（缺了就是灰屏）
        if msg == 0x000F:
            self._mb.fn("wkeRepaintIfNeeded")(view)
            ps = PAINTSTRUCT()
            hdc = w.BeginPaint(wintypes.HWND(hwnd), ctypes.byref(ps))
            if hdc:
                try:
                    client = RECT()
                    w.GetClientRect(wintypes.HWND(hwnd), ctypes.byref(client))
                    rc = RECT(ps.rcPaint.left, ps.rcPaint.top,
                              ps.rcPaint.right, ps.rcPaint.bottom)
                    if rc.right != rc.left and rc.bottom != rc.top:
                        out = RECT()
                        if not _intersect(out, rc, client):
                            out = client
                        rc = out
                    else:
                        rc = client
                    cx = rc.right - rc.left
                    cy = rc.bottom - rc.top
                    if cx > 0 and cy > 0:
                        vdc = self._mb.fn("wkeGetViewDC")(view)
                        if vdc:
                            w.BitBlt(hdc, rc.left, rc.top, cx, cy, wintypes.HDC(vdc),
                                     rc.left - client.left, rc.top - client.top,
                                     0x00CC0020)   # SRCCOPY
                finally:
                    w.EndPaint(wintypes.HWND(hwnd), ctypes.byref(ps))
            return 0

        # ---- WM_ERASEBKGND(20)：背景由内核提供，别擦
        if msg == 0x0014:
            return 1

        # ---- WM_SIZE(5)
        if msg == 0x0005:
            cw, ch = _loword(lp), _hiword(lp)
            self._mb.fn("wkeResize")(view, cw, ch)
            self._mb.fn("wkeRepaintIfNeeded")(view)
            return None

        # ---- 键盘：WM_KEYDOWN(256) / WM_KEYUP(257) / WM_CHAR(258)
        if msg in (0x0100, 0x0101, 0x0102):
            flags = 0
            if _hiword(lp) & 0x4000:      # 扩展键
                flags |= 0x4000
            if _hiword(lp) & 0x0100:      # 键盘消息的「上一状态」位
                flags |= 0x0100
            key = wp
            if msg == 0x0100:
                r = self._mb.fn("wkeFireKeyDownEvent")(view, key, flags, 0)
            elif msg == 0x0101:
                r = self._mb.fn("wkeFireKeyUpEvent")(view, key, flags, 0)
            else:
                r = self._mb.fn("wkeFireKeyPressEvent")(view, key, flags, 0)
            return 0 if int(r) else None

        # ---- 鼠标：512..521（按下/抬起/移动/双击）
        if 0x0200 <= msg <= 0x0209:
            if msg in (0x0201, 0x0207, 0x0204):        # 左/中/右键按下
                if int(w.GetFocus() or 0) != hwnd:
                    w.SetFocus(wintypes.HWND(hwnd))
                w.SetCapture(wintypes.HWND(hwnd))
            elif msg in (0x0202, 0x0208, 0x0205):      # 抬起
                w.ReleaseCapture()
            x, y = _loword(lp), _hiword(lp)
            r = self._mb.fn("wkeFireMouseEvent")(view, msg, x, y, _mk_flags(wp))
            return 0 if int(r) else None

        # ---- WM_CONTEXTMENU(123)：lParam 是屏幕坐标，要转客户区
        if msg == 0x007B:
            x, y = _loword_signed(lp), _hiword_signed(lp)
            if x != -1 or y != -1:
                pt = POINT(x, y)
                w.ScreenToClient(wintypes.HWND(hwnd), ctypes.byref(pt))
                x, y = pt.x, pt.y
            r = self._mb.fn("wkeFireContextMenuEvent")(view, x, y, _mk_flags(wp))
            return 0 if int(r) else None

        # ---- WM_MOUSEWHEEL(522)：lParam 屏幕坐标，delta 在 HIWORD(wParam)
        if msg == 0x020A:
            pt = POINT(_loword_signed(lp), _hiword_signed(lp))
            w.ScreenToClient(wintypes.HWND(hwnd), ctypes.byref(pt))
            delta = _hiword_signed(wp)     # ★ 必须带符号：向下滚是 -120，不是 65416
            r = self._mb.fn("wkeFireMouseWheelEvent")(view, pt.x, pt.y, delta,
                                                      _mk_flags(wp))
            return 0 if int(r) else None

        # ---- 焦点
        if msg == 0x0007:                  # WM_SETFOCUS
            self._mb.fn("wkeSetFocus")(view)
            return 0
        if msg == 0x0008:                  # WM_KILLFOCUS
            self._mb.fn("wkeKillFocus")(view)
            return 0

        # ---- WM_SETCURSOR(32)：让内核决定鼠标形状（超链接变小手等）
        if msg == 0x0020:
            r = self._mb.fn("wkeFireWindowsMessage")(view, hwnd, 32, 0, 0, None)
            return 0 if int(r) else None

        # ---- WM_IME_STARTCOMPOSITION(269)：把输入法候选框挪到光标处
        if msg == 0x010D:
            try:
                caret = wkeCaretRect()
                self._mb.fn("wkeGetCaretRect")(view, ctypes.byref(caret))
                cf = CANDIDATEFORM()
                cf.dwIndex = 0
                cf.dwStyle = 0x0080            # CFS_CANDIDATEPOS
                cf.ptCurrentPos = POINT(caret.x, caret.y + caret.h)
                cf.rcArea = RECT(caret.x, caret.y, caret.x + caret.w,
                                 caret.y + caret.h)
                himc = w.ImmGetContext(wintypes.HWND(hwnd))
                if himc:
                    try:
                        w.ImmSetCandidateWindow(wintypes.HANDLE(himc), ctypes.byref(cf))
                    finally:
                        w.ImmReleaseContext(wintypes.HWND(hwnd),
                                            wintypes.HANDLE(himc))
            except Exception:
                pass
            return None

        # ---- WM_DESTROY(2)：窗口没了，view 必须跟着走（否则内核会往死窗口里画）
        if msg == 0x0002:
            r = self._call_old(hwnd, msg, wp, lp)
            try:
                self.destroy()
            except Exception:
                pass
            return r

        return None

    # ------------------------------------------------------------ 载入

    def load_url(self, url):
        """载入网址（http/https/file/data 都可以）"""
        self._require_alive("载入网址")
        self._mb.fn("wkeLoadURLW")(self._view, _w(url))
        return True

    def load_file(self, path):
        """载入本地文件（html/图片/PDF 都行）"""
        self._require_alive("载入文件")
        p = os.path.abspath(str(path))
        if not os.path.isfile(p):
            raise MbKitError("文件不存在：%s" % p)
        self._mb.fn("wkeLoadFileW")(self._view, p)
        return True

    def load_html(self, html, base_url=None):
        """载入一段 HTML 源码；base_url 给了就按它解析相对路径（css/js/图片）"""
        self._require_alive("载入 HTML")
        if base_url:
            self._mb.fn("wkeLoadHtmlWithBaseUrl")(self._view, _w(html), _w(base_url))
        else:
            self._mb.fn("wkeLoadHTMLW")(self._view, _w(html))
        return True

    # 别名：易模块叫「载入HTML2」
    load_html_with_base_url = load_html

    def reload(self):
        """刷新"""
        if self._alive and self._view:
            self._mb.fn("wkeReload")(self._view)

    def stop_loading(self):
        """停止载入"""
        if self._alive and self._view:
            self._mb.fn("wkeStopLoading")(self._view)

    def go_back(self):
        """后退，返回是否成功"""
        if not self._alive or not self._view:
            return False
        return bool(int(self._mb.fn("wkeGoBack")(self._view)))

    def go_forward(self):
        """前进，返回是否成功"""
        if not self._alive or not self._view:
            return False
        return bool(int(self._mb.fn("wkeGoForward")(self._view)))

    def is_loading(self):
        """是否还在载入"""
        if not self._alive or not self._view:
            return False
        return bool(int(self._mb.fn("wkeIsLoading")(self._view)))

    def is_document_ready(self):
        """文档是否就绪（DOM 可用）"""
        if not self._alive or not self._view:
            return False
        return bool(int(self._mb.fn("wkeIsDocumentReady")(self._view)))

    def get_url(self):
        """当前网址"""
        if not self._alive or not self._view:
            return ""
        return _read_str(self._mb.fn("wkeGetURL")(self._view))

    def get_title(self):
        """当前网页标题"""
        if not self._alive or not self._view:
            return ""
        return _read_wstr(self._mb.fn("wkeGetTitleW")(self._view))

    def get_source(self):
        """当前网页源码（很长，慎用）"""
        if not self._alive or not self._view:
            return ""
        return _read_str(self._mb.fn("wkeGetSource")(self._view))

    # ------------------------------------------------------------ 执行 JS

    def run_js(self, script):
        """执行 JS，返回 **js 值的句柄**（长整数）。

        想要结果文本就包一层：`run_js_to_string("document.title")`。
        """
        self._require_alive("执行 JS")
        self._warn_offthread("run_js")
        return int(self._mb.fn("wkeRunJSW")(self._view, _w(script)))

    def run_js_to_string(self, script):
        """执行 JS 并把结果转成 python 字符串"""
        self._require_alive("执行 JS")
        v = int(self._mb.fn("wkeRunJSW")(self._view, _w(script)))
        return self.js_to_string(v)

    def run_js_to_int(self, script, default=0):
        """执行 JS 并把结果转成 int"""
        self._require_alive("执行 JS")
        v = int(self._mb.fn("wkeRunJSW")(self._view, _w(script)))
        try:
            return int(self._mb.fn("jsToInt")(ctypes.c_void_p(0), v))
        except Exception:
            return default

    # JS 值转换：es（执行状态）一般传 0 也能用，内核只把它当上下文
    def js_to_string(self, value, es=0):
        """js 值 -> 文本"""
        p = self._mb.fn("jsToStringW")(ctypes.c_void_p(es), ctypes.c_longlong(value))
        return _read_wstr(p)

    def js_to_int(self, value, es=0):
        """js 值 -> int"""
        return int(self._mb.fn("jsToInt")(ctypes.c_void_p(es), ctypes.c_longlong(value)))

    def js_to_double(self, value, es=0):
        """js 值 -> float"""
        return float(self._mb.fn("jsToDouble")(ctypes.c_void_p(es),
                                               ctypes.c_longlong(value)))

    def js_to_bool(self, value, es=0):
        """js 值 -> bool"""
        return bool(int(self._mb.fn("jsToBoolean")(ctypes.c_void_p(es),
                                                   ctypes.c_longlong(value))))

    def js_bind_function(self, name, func, argc=0):
        """把 python 函数暴露给网页 JS（网页里就能直接调 `name(...)`）

            def add(a, b):
                return a + b
            wv.js_bind_function("pyAdd", add, 2)
            wv.load_html("<button onclick=\\"alert(pyAdd(1,2))\\">试</button>")

        * 函数名只用 ASCII 字母/数字/下划线（内核按 const char* 收）。
        * 回调在 **owner 线程**（内核线程）里执行，别在里面做耗时操作。
        * 参数自动转成 str / int / float / bool / None；返回值同理（None = undefined）。
        * 名字是进程级注册，**同一个名字后注册的会覆盖前面的**；销毁后不会自动解绑，
          但本模块会立起 alive 标志让 thunk 立刻返回 undefined，不会碰你的函数。
        """
        self._require_alive("绑定 JS 函数")
        if not name or not _is_ascii_name(name):
            raise MbKitError("JS 函数名只能用 ASCII 字母/数字/下划线：%r" % (name,))

        def _thunk(es, param):
            try:
                if not self._alive or is_unloading():
                    return 0
                fn = self._js_funcs.get(name)
                if fn is None:
                    return 0
                n = int(self._mb.fn("jsArgCount")(es))
                args = [self._js_arg_to_py(es, i) for i in range(n)]
                rv = fn(*args)
                return self._py_to_js(es, rv)
            except Exception:
                return 0

        cb = JS_CB(_thunk)
        self._js_funcs[name] = func
        self._cbs["js:" + name] = cb
        self._mb.fn("wkeJsBindFunction")(name.encode("ascii"), cb, ctypes.c_void_p(0),
                                         int(argc))
        return True

    def _js_arg_to_py(self, es, i):
        """第 i 个 JS 实参 -> python 值"""
        t = int(self._mb.fn("jsArgType")(es, i))
        val = self._mb.fn("jsArg")(es, i)
        if t == JS_TYPE_STRING:
            return _read_wstr(self._mb.fn("jsToStringW")(es, ctypes.c_longlong(val)))
        if t == JS_TYPE_NUMBER:
            return float(self._mb.fn("jsToDouble")(es, ctypes.c_longlong(val)))
        if t == JS_TYPE_BOOLEAN:
            return bool(int(self._mb.fn("jsToBoolean")(es, ctypes.c_longlong(val))))
        if t in (JS_TYPE_OBJECT, JS_TYPE_FUNCTION):
            return val            # 复杂对象给句柄，要细究自己用 js_to_string
        return None               # undefined / null

    def _py_to_js(self, es, v):
        """python 值 -> js 值（造值函数不收 es，只有 jsStringW 要）"""
        f = self._mb.fn
        if v is None:
            return int(f("jsUndefined")())
        if isinstance(v, bool):
            return int(f("jsBoolean")(1 if v else 0))
        if isinstance(v, int):
            return int(f("jsInt")(v))
        if isinstance(v, float):
            return int(f("jsDouble")(v))
        if isinstance(v, bytes):
            v = v.decode("utf-8", "replace")
        return int(f("jsStringW")(es, _w(str(v))))

    # ------------------------------------------------------------ 事件

    def _hook(self, event, func):
        """登记用户回调（None = 取消登记）"""
        if func is None:
            self._hooks.pop(event, None)
        else:
            self._hooks[event] = func
        return func

    def _fire(self, event, *args):
        """安全地调用户回调：卸载中 / 已销毁 / 回调里抛异常 都不影响内核"""
        if is_unloading() or not self._alive:
            return
        fn = self._hooks.get(event)
        if fn is None:
            return
        try:
            fn(*args)
        except Exception:
            pass

    def on_document_ready2(self, func):
        """文档就绪（DOM 可用，此时执行 JS 才安全）。回调参数：url

            wv.on_document_ready2(lambda url: wv.run_js("render()"))
        """
        self._require_alive("注册事件")
        self._hook("document_ready2", func)
        if "document_ready2" not in self._cbs:
            def _cb(view, param, url, flag):
                self._fire("document_ready2", _read_str(url) if url else "")
            c = DOC_READY2_CB(_cb)
            self._cbs["document_ready2"] = c
            self._mb.fn("wkeOnDocumentReady2")(self._view, c, None)
        return func

    def on_loading_finish(self, func):
        """载入结束（含失败）。回调参数：(url, 结果码 0成功/1失败/2取消)"""
        self._require_alive("注册事件")
        self._hook("loading_finish", func)
        if "loading_finish" not in self._cbs:
            def _cb(view, param, url, result, err):
                self._fire("loading_finish",
                           _read_str(url) if url else "",
                           int(result),
                           _read_str(err) if err else "")
            c = LOADING_FINISH_CB(_cb)
            self._cbs["loading_finish"] = c
            self._mb.fn("wkeOnLoadingFinish")(self._view, c, None)
        return func

    def on_console(self, func):
        """控制台输出（调试用 console.log 就能回收进 python）。回调参数：(级别, 文本, 来源, 行号)"""
        self._require_alive("注册事件")
        self._hook("console", func)
        if "console" not in self._cbs:
            def _cb(view, param, msg, level, name, line, stack):
                self._fire("console", int(level),
                           _read_wstr(msg) if msg else "",
                           _read_wstr(name) if name else "",
                           int(line))
            c = CONSOLE_CB(_cb)
            self._cbs["console"] = c
            self._mb.fn("wkeOnConsole")(self._view, c, None)
        return func

    def on_title_changed(self, func):
        """网页标题变化。回调参数：标题"""
        self._require_alive("注册事件")
        self._hook("title_changed", func)
        if "title_changed" not in self._cbs:
            def _cb(view, param, title):
                self._fire("title_changed", _read_wstr(title) if title else "")
            c = TITLE_CHANGED_CB(_cb)
            self._cbs["title_changed"] = c
            self._mb.fn("wkeOnTitleChanged")(self._view, c, None)
        return func

    def on_url_changed(self, func):
        """网址变化。回调参数：(新网址, 旧网址)"""
        self._require_alive("注册事件")
        self._hook("url_changed", func)
        if "url_changed" not in self._cbs:
            def _cb(view, param, url, old):
                self._fire("url_changed",
                           _read_str(url) if url else "",
                           _read_str(old) if old else "")
            c = URL_CHANGED_CB(_cb)
            self._cbs["url_changed"] = c
            self._mb.fn("wkeOnURLChanged2")(self._view, c, None)
        return func

    # ------------------------------------------------------------ 杂项属性

    def set_zoom(self, factor):
        """缩放（1.0 = 100%）"""
        if self._alive and self._view:
            self._mb.fn("wkeSetZoomFactor")(self._view, ctypes.c_float(float(factor)))

    def set_user_agent(self, ua):
        """自定义 User-Agent"""
        if self._alive and self._view:
            self._mb.fn("wkeSetUserAgentW")(self._view, _w(ua))

    def set_cookie_enabled(self, enabled=True):
        """开关 Cookie"""
        if self._alive and self._view:
            self._mb.fn("wkeSetCookieEnabled")(self._view, 1 if enabled else 0)

    def set_debug_config(self, key, value):
        """内核调试开关，形如 set_debug_config("showDevTools", "true")"""
        if self._alive and self._view:
            self._mb.fn("wkeSetDebugConfig")(self._view, _b(key), _b(value))

    def fire_message(self, hwnd, msg, wp=0, lp=0):
        """把一条窗口消息原样喂给内核（自己做 WndProc 时用；input_hook=False 配合）"""
        if not self._alive or not self._view:
            return 0
        return int(self._mb.fn("wkeFireWindowsMessage")(self._view, int(hwnd), int(msg),
                                                        wp, lp, None))

    # ------------------------------------------------------------ 跨线程

    def _ensure_msg_window(self):
        """在 owner 线程上建一个隐藏的 message-only 窗口，给 post() 用"""
        if self._msg_hwnd:
            return True
        try:
            w = win32()
            cls_name = "xlzWebUI_post_%d" % os.getpid()

            class WNDCLASSW(ctypes.Structure):
                _fields_ = [("style", wintypes.UINT),
                            ("lpfnWndProc", WNDPROC),
                            ("cbClsExtra", ctypes.c_int),
                            ("cbWndExtra", ctypes.c_int),
                            ("hInstance", wintypes.HINSTANCE),
                            ("hIcon", wintypes.HICON),
                            ("hCursor", wintypes.HANDLE),
                            ("hbrBackground", wintypes.HBRUSH),
                            ("lpszMenuName", wintypes.LPCWSTR),
                            ("lpszClassName", wintypes.LPCWSTR)]

            def _proc(hwnd, msg, wp, lp):
                try:
                    if msg == _WM_APP_POST:
                        self._drain_posts()
                        return 0
                except Exception:
                    pass
                return int(w.DefWindowProcW(hwnd, msg, wp, lp))

            proc = WNDPROC(_proc)
            self._msg_wndproc = proc        # 强引用：类一旦注册就一直用它
            wc = WNDCLASSW()
            wc.style = 0
            wc.lpfnWndProc = proc
            wc.hInstance = w.GetModuleHandleW(None)
            wc.lpszClassName = cls_name
            atom = w.RegisterClassW(ctypes.byref(wc))
            if not atom:
                return False            # 已经注册过（第二个 WebView）也能用，下面照样建窗口
            hwnd = w.CreateWindowExW(0, cls_name, cls_name, 0, 0, 0, 0, 0,
                                     wintypes.HWND(-3),    # HWND_MESSAGE
                                     None, wc.hInstance, None)
            if not hwnd:
                return False
            self._msg_hwnd = int(hwnd)
            return True
        except Exception:
            self._msg_hwnd = 0
            return False

    def _remove_msg_window(self):
        if self._msg_hwnd:
            try:
                win32().DestroyWindow(wintypes.HWND(self._msg_hwnd))
            except Exception:
                pass
            self._msg_hwnd = 0
        if self._msg_wndproc is not None:
            _retire(self._msg_wndproc)
            self._msg_wndproc = None

    def _drain_posts(self):
        with self._post_lock:
            items, self._post_queue = self._post_queue, []
        for fn in items:
            try:
                fn()
            except Exception:
                pass

    def post(self, func):
        """把 func 丢回 owner 线程执行（跨线程改 UI 的唯一正确姿势）

            def worker():
                data = fetch()
                wv.post(lambda: wv.run_js("update(%r)" % (data,)))

        * 已经在 owner 线程 = 立刻执行；
        * 否则 PostMessageW 到一个隐藏消息窗口（窗口模式，宿主消息循环会派发）；
        * 都没有（离屏模式）就排队，等你调 pump_posts()。
        """
        if not callable(func):
            raise MbKitError("post() 需要一个可调用对象")
        if not self._alive:
            return False
        if self.in_owner_thread():
            try:
                func()
            except Exception:
                pass
            return True
        if self._msg_hwnd:
            # ★ 先入队再投递：投递失败时条目已在队列里，绝不能再 append 一次
            #   （否则 pump_posts 会把同一个函数跑两遍）
            with self._post_lock:
                self._post_queue.append(func)
            try:
                ok = win32().PostMessageW(wintypes.HWND(self._msg_hwnd),
                                          _WM_APP_POST, 0, 0)
            except Exception:
                ok = 0
            if ok:
                return True
            return False
        with self._post_lock:
            self._post_queue.append(func)
        return False

    def pump_posts(self):
        """离屏/无消息循环时手工清空 post 队列（窗口模式下用不着）"""
        self._drain_posts()
        return True

    def pump(self, ms=0):
        """泵一次内核消息 + 清空 post 队列（离屏模式用）"""
        if not self._alive:
            return False
        self._mb.pump(ms)
        self._drain_posts()
        return True

    # ------------------------------------------------------------ 自检

    def self_diag(self):
        """本 view 的一行式状态（贴给 AI / 贴给人看都行）"""
        mb = self._mb
        return (
            "WebView#%d alive=%s view=0x%X hwnd=0x%X owner_tid=%s cur_tid=%s "
            "透明=%s 输入接管=%s 回调=%d/%d 内核=%s(%d) 尺寸=%s 就绪=%s 网址=%r"
            % (self.id, self._alive, self._view, self._hwnd, self._owner_tid,
               current_thread_id(), self._transparent, bool(self._subclassed_hwnd),
               len(self._cbs), len(self._hooks), mb.kernel_mode_name(),
               mb.kernel_mode(), self.view_size(), self.is_document_ready(),
               self.get_url()))

    def __repr__(self):
        return "<WebView#%d view=0x%X hwnd=0x%X %s>" % (
            self.id, self._view, self._hwnd, "alive" if self._alive else "destroyed")


# ---------------------------------------------------------------- 小工具

def _w(s):
    """字符串参数归一化（None -> ""）"""
    return "" if s is None else str(s)


def _b(s):
    """bytes 参数归一化"""
    if s is None:
        return b""
    if isinstance(s, bytes):
        return s
    try:
        return str(s).encode("mbcs")
    except Exception:
        return str(s).encode("utf-8", "replace")


def _loword(v):
    return int(v) & 0xFFFF


def _hiword(v):
    return (int(v) >> 16) & 0xFFFF


def _loword_signed(v):
    x = int(v) & 0xFFFF
    return x - 0x10000 if x & 0x8000 else x


def _hiword_signed(v):
    x = (int(v) >> 16) & 0xFFFF
    return x - 0x10000 if x & 0x8000 else x


def _mk_flags(wp):
    """wParam 里的 MK_* 位 -> 内核的鼠标 flags（两位一一对应，别改）"""
    f = 0
    if wp & 0x0008:      # MK_SHIFT
        f |= 0x0008
    if wp & 0x0004:      # MK_CONTROL
        f |= 0x0004
    if wp & 0x0001:      # MK_LBUTTON
        f |= 0x0001
    if wp & 0x0010:      # MK_MBUTTON
        f |= 0x0010
    if wp & 0x0002:      # MK_RBUTTON
        f |= 0x0002
    return f


def _is_ascii_name(name):
    if not isinstance(name, str) or not name:
        return False
    for ch in name:
        if not (ch.isalnum() and ord(ch) < 128) and ch != "_":
            return False
    return True


def _intersect(dst, a, b):
    """RECT 求交（dst 为输出）；无交集返回 False"""
    left = max(a.left, b.left)
    top = max(a.top, b.top)
    right = min(a.right, b.right)
    bottom = min(a.bottom, b.bottom)
    dst.left, dst.top, dst.right, dst.bottom = left, top, right, bottom
    return right > left and bottom > top


# ---------------------------------------------------------------- 模块级收摊

def detach_all():
    """销毁全部 webui 并解绑内核回调（热重载契约第 2 步，永不抛异常）

        def on_uninstall():
            begin_unload()          # 可选：先立卸载标志，回调立刻全停
            detach_all()
            return 0
    """
    begin_unload()
    n = 0
    with _VIEWS_LOCK:
        views = list(_VIEWS)
    for v in views:
        try:
            v.destroy()
            n += 1
        except Exception:
            pass
    # 内核侧兜底：代理回调 + view 计数一起清（延迟销毁，不会卡在加载锁上）
    try:
        if MbKit.loaded():
            MbKit.get().detach_all()
    except Exception:
        pass
    with _VIEWS_LOCK:
        _VIEWS.clear()
        _WINDOW_OWNER.clear()
    return n


def unload():
    """detach_all() 的别名，给 on_uninstall 里写一行用"""
    return detach_all()
