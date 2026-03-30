# XiaoLiZiVM_PythonSDK

小栗子虚拟机 Python 插件 SDK —�?让你�?**Python 3.10 (Win32)** 编写小栗子框架插件，支持外部依赖（Pillow、requests 等），无需 C++ 开发经验�?

---

## 目录结构

```
XiaoLiZiVM_PythonSDK/
├── PythonSDK/                  # Python 插件 SDK 核心
�?  ├── plugin.cpp              # C++ 空壳 DLL（桥接层，无需修改�?
�?  ├── Demo_TemplatePlugin/    # 插件模板（从这里开始）
�?  �?  ├── plugin_main.py      # 插件主逻辑
�?  �?  └── xlz_sdk.py          # SDK 桥接层（无需修改�?
�?  ├── Demo_外部依赖演示/       # 外部依赖（Pillow）使用示�?
�?  ├── Demo_关键词回�?         # 关键词回复示�?
�?  └── tools/
�?      └── pack_payload.py     # 打包工具（Python -> DLL�?
├── CppSDK/                     # C++ SDK（仅供参考，Python 插件无需使用�?
�?  └── README.md               # C++ API 完整说明
└── xlz_pyruntime/              # Python 运行时加载器（框架依赖，无需修改�?
```

---

## 快速开�?

### 1. 复制模板

复制 `PythonSDK/Demo_TemplatePlugin/` 目录，重命名为你的插件名，例�?`MyPlugin/`�?

### 2. 编辑 `plugin_main.py`

```python
from xlz_sdk import Bridge

api: Bridge = None

def apprun(ctx: dict) -> dict:
    """插件初始化，返回插件信息"""
    global api
    api = Bridge(ctx["dll_path"])
    api.output_log("[MyPlugin] 初始化成�?)
    return {
        "app_name": "我的插件",
        "author": "作者名",
        "app_version": "1.0.0",
        "description": "插件描述",
        "permissions": ["输出日志", "发送群消息"],  # 按需申请
    }

def on_group_message(ev: dict) -> int:
    """
    收到群消�?
    ev: {'this_qq': int, 'group_qq': int, 'sender_qq': int,
         'message': str, 'group_name': str, 'sender_nick': str}
    返回 0=继续传�? 1=拦截
    """
    if ev.get("message") == "测试":
        api.send_group_message(ev["this_qq"], ev["group_qq"], "收到�?)
        return 1
    return 0

def on_private_message(ev: dict) -> int:
    """收到私聊消息"""
    return 0

def on_enable(ev: dict) -> int:
    """插件启用"""
    return 0

def on_disable(ev: dict) -> int:
    """插件禁用"""
    return 0

def on_uninstall(ev: dict) -> int:
    """插件卸载"""
    return 0
```

### 3. 打包�?DLL

```bash
python PythonSDK/tools/pack_payload.py \
    PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll \
    MyPlugin/ \
    输出/MyPlugin.dll
```

参数说明�?
- `XiaoLiZiVM_PythonCompat.dll` �?编译好的 C++ 空壳 DLL（已�?`PythonSDK/bin/Release/` 提供�?
- `MyPlugin/` �?你的插件目录（含 `plugin_main.py`、`xlz_sdk.py` 及依赖）
- `输出/MyPlugin.dll` �?生成的插�?DLL

生成�?DLL 直接放入框架插件目录即可�?

---

## 可用回调事件

| 函数�?| 触发时机 | 参数字段 |
|---|---|---|
| `apprun(ctx)` | 插件加载/初始�?| `dll_path`, `pluginkey` |
| `on_enable(ev)` | 插件启用 | `this_qq` |
| `on_disable(ev)` | 插件禁用 | `this_qq` |
| `on_uninstall(ev)` | 插件卸载 | �?|
| `on_group_message(ev)` | 收到群消�?| `this_qq`, `group_qq`, `sender_qq`, `message`, `group_name`, `sender_nick` |
| `on_private_message(ev)` | 收到私聊消息 | `this_qq`, `sender_qq`, `message` |
| `on_event(ev)` | 其他事件 | `this_qq`, `event_type`, `event_subtype` |

所有回调返�?`0`（继续）�?`1`（拦截）�?

---

## 可申请的权限

�?`apprun` 返回值的 `permissions` 字段中填写中文权限名（列表）�?

```python
"permissions": [
    "输出日志",
    "发送群消息",
    "发送好友消�?,
    "上传群图�?,
    "上传好友图片",
    "上传群语�?,
    "上传好友语音",
    "上传群文�?,
    "撤回消息_群聊",
    "禁言群成�?,
    "删除群成�?,
    "全员禁言",
    "取群列表",
    "取群成员列表",
    "取群名片",
    "设置群名�?,
    "取昵称_从缓�?,
    "强制取昵�?,
    "取图片下载地址",
    "取好友文件下载地址",
    "处理好友验证事件",
    "处理群验证事�?,
    "QQ点赞",
    "群聊打卡",
    "分享音乐",
    "取群成员信息",
    "取插件数据目�?,
    "重载自身",
]
```

完整 API 列表及参数说明见 [CppSDK/README.md](CppSDK/README.md)（与 Python SDK 接口一一对应）�?

---

## 添加外部依赖

> **必须使用 32 位（Win32）、CPython 3.10 的包，否则无法加载�?*

### 安装方式

使用 pip 安装到插件目录：

```bash
# 必须指定�?2�?Python 3.10、目标目录为你的插件文件�?
"C:\Python310_32\python.exe" -m pip install pillow \
    --only-binary=:all: \
    --python-version 3.10 \
    --platform win32 \
    --target MyPlugin/
```

或者直接从 [PyPI](https://pypi.org/) 下载 `cp310-cp310-win32.whl` 文件手动解压到插件目录�?

### 使用方式

外部依赖放在插件目录内，打包时会一并打�?DLL�?

```
MyPlugin/
├── plugin_main.py
├── xlz_sdk.py
├── PIL/                  # Pillow 依赖
�?  ├── Image.py
�?  └── ...
└── pillow-12.x.dist-info/
```

�?`plugin_main.py` 中直�?import 即可�?

```python
from PIL import Image, ImageDraw, ImageFont
import io

def on_group_message(ev: dict) -> int:
    if ev["message"] == "生成图片":
        img = Image.new("RGB", (200, 200), color=(30, 30, 30))
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        img_code = api.upload_group_image(
            ev["this_qq"], ev["group_qq"], False, buf.getvalue()
        )
        api.send_group_message(ev["this_qq"], ev["group_qq"], img_code)
    return 0
```

### 注意事项

- **必须 32 位（win32�?*：框架是 32 位进程，64 �?`.pyd` 无法加载
- **必须 CPython 3.10**：运行时为内�?Python 3.10，其他版本不兼容
- �?Python 包（�?`.pyd`）不受此限制
- 需�?`ctypes` 的包可以直接使用，运行时已内置支�?

---

## 打包工具详细说明

`PythonSDK/tools/pack_payload.py` 将插件目录压缩后附加到空�?DLL 尾部�?

```bash
python pack_payload.py <shell_dll> <payload_dir> <out_dll>
```

| 参数 | 说明 |
|---|---|
| `shell_dll` | C++ 空壳 DLL（`XiaoLiZiVM_PythonCompat.dll`�?|
| `payload_dir` | 插件目录（含 `plugin_main.py`�?|
| `out_dll` | 输出 DLL 路径 |

**示例**�?

```bash
python PythonSDK/tools/pack_payload.py \
    PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll \
    PythonSDK/Demo_TemplatePlugin/ \
    output/MyPlugin.dll
```

---

## 编译空壳 DLL（仅需要时�?

如果你修改了 `plugin.cpp` 或需要重新编译空�?DLL�?

1. �?Visual Studio 2019/2022 打开 `PythonSDK/PythonCompatPlugin.sln`
2. 选择 `Win32` + `Release`
3. 编译，输出到 `PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll`

> **必须�?Win32，不能用 x64�?*

`xlz_pyruntime.dll` 同理，用 `xlz_pyruntime/xlz_pyruntime.sln` 编译�?

---

## xlz_sdk.py API 速查

| 方法 | 说明 |
|---|---|
| `output_log(msg)` | 输出日志到框架控制台 |
| `send_group_message(this_qq, group_qq, msg)` | 发送群消息 |
| `send_private_message(this_qq, friend_qq, msg)` | 发送私聊消�?|
| `upload_group_image(this_qq, group_qq, is_flash, pic_bytes)` | 上传群图片，返回图片代码 |
| `upload_friend_image(this_qq, friend_qq, is_flash, ptr, size)` | 上传好友图片 |
| `upload_group_audio(this_qq, group_qq, type, text, ptr, size)` | 上传群语�?|
| `upload_group_file(this_qq, group_qq, file_path, folder)` | 上传群文�?|
| `get_group_list(this_qq)` | 取群列表 |
| `get_group_member_list(this_qq, group_qq)` | 取群成员列表 |
| `get_group_card(this_qq, group_qq, member_qq)` | 取群名片 |
| `set_group_card(this_qq, group_qq, member_qq, card)` | 设置群名�?|
| `mute_group_member(this_qq, group_qq, member_qq, seconds)` | 禁言群成�?|
| `remove_group_member(this_qq, group_qq, member_qq, refuse)` | 踢出群成�?|
| `recall_group_message(this_qq, group_qq, random, req)` | 撤回群消�?|
| `get_image_download_url(image_code, this_qq, group_qq)` | 取图片下载地址 |
| `get_nickname_from_cache(other_qq)` | 从缓存取昵称 |
| `get_nickname_force(this_qq, other_qq)` | 强制取昵�?|
| `get_plugin_data_directory()` | 取插件数据目�?|
| `get_framework_version()` | 取框架版�?|
| `get_plugin_self_version()` | 取插件自身版本号 |
| `reload_itself(new_dll_path)` | 重载自身 |
| `qq_like(this_qq, other_qq)` | QQ点赞 |
| `mute_all(this_qq, group_qq, enabled)` | 全员禁言 |

完整参数说明�?[CppSDK/README.md](CppSDK/README.md)�?

---

## 常见问题

**Q: 插件加载后没有反应？**

查看框架日志目录下的 `debug.log`，确�?`apprun success` 是否出现�?

**Q: `import xxx` 报错找不到模块？**

确认依赖包是 `cp310-cp310-win32` 版本，且已放在插件目录内�?

**Q: `.pyd` 文件加载失败�?*

确认�?32 位（`win32`）版本，不是 `win_amd64`�?

**Q: 多个插件之间事件串运行？**

这是已知问题，SDK 已通过函数作用域隔离解决，确保使用最新版 `XiaoLiZiVM_PythonCompat.dll`�?

---

## License

MIT
