api = None

# 自由线程：需要把耗时逻辑丢出框架消息线程时，直接用这些辅助函数
from xlz_sdk import run_in_thread, spawn, shutdown_threads
# 事件/状态常量（数值与框架内部一致，完整列表见 xlz_constants.py）
from xlz_constants import MessageHandle, EnableResponse, GroupEvent, FriendEvent

# 可选：内嵌浏览器窗口（webui）。默认不启用，模板行为与原来完全一致。
# 要用就把下面这一行和后面 3 处标了「可选：webui」的注释块一起放开，
# 并确认插件目录（或宿主 exe 目录 / %TEMP%\ElangMbKit）里有 MbKit146.dll。
# from xlz_webui import WebView, MbKitError, MbKitThreadError, detach_all as webui_detach_all
# _webui = None            # 本插件唯一的 WebView 实例


def _dbg(msg: str):
    return
    try:
        with open("debug.log", "a", encoding="utf-8") as f:
            f.write(f"[python] {msg}\n")
    except Exception:
        pass


def _slow_task(my_uin: int, group_uin: int, text: str):
    """演示：耗时逻辑放到工作线程里跑，跑完再回发消息（调用 API 是线程安全的）"""
    import time
    time.sleep(5)  # 模拟耗时操作（下载 / 计算 / 调用外部接口）
    if api is not None:
        api.send_group_message(my_uin, group_uin, f"耗时任务完成：{text}")


# ---------------------------------------------------------------- 可选：webui
# _WEBUI_DEMO_HTML = """<!doctype html><meta charset="utf-8">
# <style>body{font:14px/1.6 "Microsoft YaHei";margin:16px}</style>
# <h3>webui 起来了</h3>
# <button onclick="alert(pyEcho('按钮被点了'))">调用 Python</button>"""
#
#
# def _open_webui(html: str = ""):
#     """打开（或复用）内嵌浏览器窗口，返回 WebView
#
#     ★ 线程铁律：必须在「承载窗口的创建线程」里调用。就在 on_* 回调里这样直接调即可；
#       **不要**放进 run_in_thread / spawn —— 那正是灰屏的经典原因，
#       本模块会直接抛 MbKitThreadError，而不是让你对着一片白屏猜。
#     """
#     global _webui
#     if _webui is not None and _webui.is_alive():
#         _webui.load_html(html or _WEBUI_DEMO_HTML)     # 复用已有窗口
#         return _webui
#     hwnd = api.get_framework_main_window_handle()      # 宿主主窗口句柄（整数）
#     wv = WebView()                                     # 自动按搜索顺序找 MbKit146.dll
#     wv.bind(hwnd)                                      # 建 view + wkeSetHandle + 接管窗口过程
#     wv.on_document_ready2(lambda url: api.output_log(f"[webui] 页面就绪 {url}"))
#     wv.on_console(lambda lv, text, src, line: api.output_log(f"[webui] {text}"))
#     wv.js_bind_function("pyEcho", lambda s: "来自 Python：" + str(s), 1)
#     wv.load_html(html or _WEBUI_DEMO_HTML)             # 网页里直接调 pyEcho('x') 即可
#     _webui = wv
#     return wv


def apprun(ctx: dict) -> dict:
    """
    插件初始化入口
    返回值: dict，包含插件信息（app_name, author, app_version, description, permissions）
    """
    global api

    try:
        from xlz_sdk import Bridge
        api = Bridge(ctx["dll_path"])
        # 可选：开启异步事件派发
        #   开启后事件交由 C++ 工作线程执行，框架消息线程不会被慢速 Python 阻塞；
        #   代价是 on_xxx 的返回值不再用于拦截后续插件。
        # api.set_async(True)
        api.output_log("[PythonSDK] apprun 初始化成功")
        _dbg("Bridge init success")
    except Exception as e:
        api = None
        _dbg(f"Bridge init failed: {e}")

    return {
        "app_name": "测试应用",
        "author": "Python SDK",
        "app_version": "1.0.0",
        "description": "小栗子虚拟机SDK for Python（兼容层测试）",
        "permissions": ["输出日志", "发送群消息"],
    }


def on_enable() -> int:
    """
    插件被启用时调用（无参数）
    返回值: EnableResponse.DONE(0)=正常，EnableResponse.REJECT(8)=拒绝启用（框架将停止加载该插件）
    """
    api.output_log("[PythonSDK] 插件已启用，事件桥接正常")
    return EnableResponse.DONE


def on_disable():
    """
    插件被禁用时调用（无参数）
    """
    return 0


def on_uninstall():
    """
    插件被卸载时调用（无参数）
    """
    # 自由线程：回收线程池，避免残留线程
    try:
        shutdown_threads(wait=False)
    except Exception:
        pass
    # 可选：webui 的热重载契约 —— 顺序不能换：
    #   ① 先停一切会碰窗口的线程/定时器（上面 shutdown_threads 已做）；
    #   ② 再销毁 view；③ 最后全解绑。
    # detach_all() 一步做完 ②③：它会先立起「卸载中」标志（此后任何内核回调进来都直接返回），
    # 再销毁本插件所有 WebView，最后调内核的 MbKit_DetachAll 兜底。
    # 顺序反过来（先解绑再停线程）＝ 线程还在跑却去碰已销毁的控件，重载必炸。
    # try:
    #     webui_detach_all()
    # except Exception:
    #     pass
    return 0


def on_setting():
    """
    用户点击"插件设置"时调用（无参数）
    """
    return 0


def on_group_message(ev: dict) -> int:
    """
    收到群消息时调用
    参数: ev = {'my_uin': int, 'group_uin': int, 'sender_uin': int, 'message': str, 'group_name': str, 'card': str}
    返回值: MessageHandle.IGNORE(0)=继续传递给其他插件，MessageHandle.INTERCEPT(1)=拦截
    """
    api.output_log(f"[PythonSDK] 收到群消息: {ev}")
    if (ev.get("message") == "测试"):
        api.send_group_message(ev.get("my_uin"), ev.get("group_uin"), "成功")
    elif (ev.get("message") == "慢任务"):
        # 自由线程演示：耗时逻辑丢到工作线程，立刻返回，不阻塞框架消息线程
        run_in_thread(_slow_task, ev.get("my_uin"), ev.get("group_uin"), ev.get("message"))
    # 可选：webui —— 这里是「正文里直接调」，因为 on_group_message 就跑在框架消息线程上，
    # 正好是承载窗口的线程；换成 run_in_thread 调 _open_webui 就会踩线（灰屏）。
    # elif (ev.get("message") == "网页"):
    #     _open_webui()
    return MessageHandle.IGNORE


def on_private_message(ev: dict) -> int:
    """
    收到私聊消息时调用
    参数: ev = {'my_uin': int, 'sender_uin': int, 'message': str}
    返回值: MessageHandle.IGNORE(0)=继续传递给其他插件，MessageHandle.INTERCEPT(1)=拦截
    """
    return MessageHandle.IGNORE


def on_event_message(ev: dict) -> int:
    """
    收到事件消息时调用（如群成员变动、好友请求等）
    参数: ev = {'my_uin': int, 'event_type': int, 'event_sub_type': int, 'trigger_uin': int,
               'source_group_uin': int, 'operate_uin': int, 'message': str}
    event_type 即 xlz_constants 里的 GroupEvent / FriendEvent / LoginEvent …，可与 IntEnum 直接比较
    返回值: MessageHandle.IGNORE(0)=继续传递给其他插件，MessageHandle.INTERCEPT(1)=拦截
    """
    et = ev.get("event_type")
    if et == GroupEvent.MEMBER_JOINED:
        api.output_log(f"[PythonSDK] 有人入群，触发者: {ev.get('trigger_uin')}")
    elif et == FriendEvent.FRIEND_REQUEST:
        api.output_log(f"[PythonSDK] 收到好友请求: {ev.get('trigger_uin')}")
    return MessageHandle.IGNORE

