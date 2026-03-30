import json
import random
import re
from pathlib import Path

api = None
_rules = []


def _dbg(msg: str):
    return
    try:
        with open("debug.log", "a", encoding="utf-8") as f:
            f.write(f"[python] {msg}\n")
    except Exception:
        pass


def _cfg_path() -> Path:
    if api is None:
        return Path("keyword_reply_rules.json")
    data_dir = api.get_plugin_data_directory() or "."
    p = Path(data_dir)
    p.mkdir(parents=True, exist_ok=True)
    return p / "keyword_reply_rules.json"


def _default_config() -> dict:
    return {
        "enabled": True,
        "rules": [
            {
                "enabled": True,
                "type": "contains",
                "pattern": "测试",
                "replies": [
                    "Python关键词回复已连通",
                    "收到测试消息，插件运行正常",
                ],
            },
        ],
    }


def _load_rules():
    global _rules
    p = _cfg_path()
    if not p.exists():
        p.write_text(json.dumps(_default_config(), ensure_ascii=False, indent=2), encoding="utf-8")

    try:
        cfg = json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        cfg = _default_config()

    if not cfg.get("enabled", True):
        _rules = []
        return

    _rules = cfg.get("rules", []) or []


def _pick_reply(rule: dict) -> str:
    rs = rule.get("replies") or []
    if not rs:
        return ""
    return random.choice(rs)


def _apply_groups(reply: str, match: re.Match) -> str:
    out = reply
    for i in range(1, 10):
        tag = f"${i}"
        if tag in out:
            try:
                out = out.replace(tag, match.group(i) or "")
            except IndexError:
                out = out.replace(tag, "")
    return out


def _match(rule: dict, content: str):
    t = (rule.get("type") or "contains").lower()
    pat = (rule.get("pattern") or "").strip()
    if not pat:
        return False, None

    if t == "exact":
        return content == pat, None
    if t == "contains":
        return pat in content, None
    if t == "regex":
        m = re.search(pat, content)
        return m is not None, m

    return False, None


def apprun(ctx: dict) -> dict:
    """
    插件初始化入口
    返回值: dict，包含插件信息（app_name, author, app_version, description, permissions）
    """
    global api

    try:
        from xlz_sdk import Bridge
        api = Bridge(ctx["dll_path"])
        _load_rules()
        api.output_log("[Python关键词回复] 初始化成功")
        _dbg("Bridge init success")
    except Exception as e:
        api = None
        _dbg(f"Bridge init failed: {e}")

    return {
        "app_name": "Demo 关键词回复 (Python)",
        "author": "Python SDK",
        "app_version": "1.0.0",
        "description": "关键词回复：精确/包含/正则，支持多回复随机与$1..$9分组替换",
        "permissions": ["输出日志", "发送群消息", "取插件数据目录"],
    }


def on_enable() -> int:
    """
    插件被启用时调用（无参数）
    返回值: 0=正常，1=拒绝启用（框架将停止加载该插件）
    """
    if api is not None:
        api.output_log("[Python关键词回复] 插件已启用")
    return 0


def on_setting():
    """
    用户点击"插件设置"时调用（无参数）
    """
    cfg_file = _cfg_path()
    if not cfg_file.exists():
        cfg_file.write_text(json.dumps(_default_config(), ensure_ascii=False, indent=2), encoding="utf-8")

    # 优先使用记事本，避免 tkinter 在部分环境无响应
    try:
        import subprocess
        subprocess.Popen(["notepad.exe", str(cfg_file)])
        if api is not None:
            api.output_log(f"[Python关键词回复] 已打开配置文件: {cfg_file}")
        return
    except Exception:
        pass

    # 回退到 Tk 编辑器
    try:
        import tkinter as tk
        from tkinter import messagebox
        from tkinter.scrolledtext import ScrolledText
    except Exception:
        if api is not None:
            api.output_log("[Python关键词回复] 设置UI打开失败")
        return

    root = tk.Tk()
    root.title("Python 关键词回复 设置")
    root.geometry("860x620")

    text = ScrolledText(root, wrap="none")
    text.pack(fill="both", expand=True, padx=10, pady=10)
    text.insert("1.0", cfg_file.read_text(encoding="utf-8"))

    bar = tk.Frame(root)
    bar.pack(fill="x", padx=10, pady=(0, 10))

    def save_cfg():
        raw = text.get("1.0", "end-1c")
        try:
            obj = json.loads(raw)
            cfg_file.write_text(json.dumps(obj, ensure_ascii=False, indent=2), encoding="utf-8")
            _load_rules()
            if api is not None:
                api.output_log("[Python关键词回复] 配置已保存并重载")
            messagebox.showinfo("提示", "保存成功")
        except Exception as e:
            messagebox.showerror("配置错误", str(e))

    def reset_default():
        text.delete("1.0", "end")
        text.insert("1.0", json.dumps(_default_config(), ensure_ascii=False, indent=2))

    tk.Button(bar, text="保存", command=save_cfg).pack(side="left")
    tk.Button(bar, text="恢复默认", command=reset_default).pack(side="left", padx=(8, 0))

    root.mainloop()


def on_group_message(ev: dict) -> int:
    """
    收到群消息时调用
    参数: ev = {'this_qq': int, 'group_qq': int, 'sender_qq': int, 'message': str, 'group_name': str, 'sender_nick': str}
    返回值: 0=继续传递给其他插件，1=拦截该消息不给后续插件处理
    """
    if api is None:
        return 0

    content = (ev.get("message") or "").strip()
    if not content:
        return 0

    for rule in _rules:
        if not rule.get("enabled", True):
            continue

        ok, m = _match(rule, content)
        if not ok:
            continue

        reply = _pick_reply(rule)
        if not reply:
            continue

        if m is not None:
            reply = _apply_groups(reply, m)

        api.send_group_message(
            ev.get("this_qq", 0),
            ev.get("group_qq", 0),
            reply,
            False,
        )
        api.output_log(f"[Python关键词回复] 收到群消息: {ev}")
        api.output_log(f"[Python关键词回复] 发送群消息: {reply}")

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
    收到事件消息时调用
    参数: ev = {'this_qq': int, 'event_type': int, 'event_sub_type': int, 'trigger_qq': int, 'source_group_qq': int, 'operate_qq': int, 'message': str}
    返回值: 0=继续传递给其他插件，1=拦截该事件不给后续插件处理
    """
    return 0


def on_disable():
    """
    插件被禁用时调用（无参数）
    """
    return 0


def on_uninstall():
    """
    插件被卸载时调用（无参数）
    """
    return 0
