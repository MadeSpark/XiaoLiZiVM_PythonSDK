api = None


def _dbg(msg: str):
    return
    try:
        with open("debug.log", "a", encoding="utf-8") as f:
            f.write(f"[python] {msg}\n")
    except Exception:
        pass


def apprun(ctx: dict) -> dict:
    """
    插件初始化入口
    返回值: dict，包含插件信息（app_name, author, app_version, description, permissions）
    """
    global api

    try:
        from xlz_sdk import Bridge
        api = Bridge(ctx["dll_path"])
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


def on_enable(ev: dict) -> int:
    """
    插件被启用时调用（ev 为空字典 {}，框架不传任何参数）
    返回值: 0=正常，1=拒绝启用（框架将停止加载该插件）
    """
    api.output_log("[PythonSDK] 插件已启用，事件桥接正常")
    return 0


def on_group_message(ev: dict) -> int:
    """
    收到群消息时调用
    参数: ev = {'this_qq': int, 'group_qq': int, 'sender_qq': int, 'message': str, 'group_name': str, 'sender_nick': str}
    返回值: 0=继续传递给其他插件，1=拦截该消息不给后续插件处理
    """
    api.output_log(f"[PythonSDK] 收到群消息: {ev}")
    if (ev.get("message") == "测试"):
        api.send_group_message(ev.get("this_qq"), ev.get("group_qq"), "成功")
    return 0


def on_private_message(ev: dict) -> int:
    """
    收到私聊消息时调用
    参数: ev = {'this_qq': int, 'sender_qq': int, 'message': str}
    返回值: 0=继续传递给其他插件，1=拦截该消息不给后续插件处理
    """
    return 0


def on_event_message(ev: dict) -> int:
    """
    收到事件消息时调用（如群成员变动、好友请求等）
    参数: ev = {'this_qq': int, 'event_type': int, 'event_sub_type': int, 'trigger_qq': int, 'source_group_qq': int, 'operate_qq': int, 'message': str}
    返回值: 0=继续传递给其他插件，1=拦截该事件不给后续插件处理
    """
    return 0


def on_setting(ev: dict):
    """
    用户点击"插件设置"时调用（ev 为空字典 {}，框架不传任何参数）
    无返回值要求
    """
    pass


def on_disable(ev: dict):
    """
    插件被禁用时调用（ev 为空字典 {}，框架不传任何参数）
    无返回值要求
    """
    pass


def on_uninstall(ev: dict):
    """
    插件被卸载时调用（ev 为空字典 {}，框架不传任何参数）
    无返回值要求
    """
    pass
