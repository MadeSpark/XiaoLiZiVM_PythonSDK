# XiaoLiZiVM_PythonSDK

小栗子虚拟机 Python 插件 SDK —— 让你用 **Python 3.14t (Win32, free-threaded)** 编写小栗子框架插件，支持外部依赖（Pillow、requests 等），无需 C++ 开发经验。

> 字段与参数命名统一遵循 [`../SDK字段命名规范.md`](../SDK字段命名规范.md)（QQ 号一律 `*_uin`，群名片一律 `card`）。
> 运行时为 **CPython 3.14.8 自由线程构建（PEP 703，`cp314t`，32 位）**，`sys._is_gil_enabled()` 为 `False`。详见[「运行时：Python 3.14t」](#运行时python-314t)与[「自由线程（并发）」](#自由线程并发)章节。
> 另有**可选的内嵌浏览器（webui）**：`xlz_webui.py` 让插件在框架窗口里挂真浏览器，
> 详见[「内嵌浏览器（webui，可选）」](#内嵌浏览器webui可选)章节。

---

## 目录结构

```
XiaoLiZiVM_PythonSDK/
├── Releases/                   # ★ 成品区（自动生成，可直接分发的作者工具包）
│   ├── README.md               #   包说明（自动生成）
│   ├── AI编写插件提示词.md       #   给 AI 的提示词（路径已改写为包内相对路径）
│   ├── template/               #   插件模板：整目录复制一份改名就是一个可直接打包的插件
│   │   ├── plugin_main.py      #     插件主逻辑（要改的就是它）
│   │   ├── xlz_sdk.py          #     桥接层（别改）
│   │   ├── xlz_constants.py    #     事件/状态常量（别改）
│   │   └── xlz_webui.py        #     内嵌浏览器（可选，别改；运行时需 MbKit146.dll）
│   └── tools/                  #   打包地基
│       ├── XiaoLiZiVM_PythonCompat.dll   # C++ 空壳成品
│       └── pack_payload.py     #   打包工具
├── rebuild.bat                 # 一键重建：编译 + 部署 + 打包 + 校验 + 组装成品区
├── tools/
│   └── rebuild_all.py          # rebuild.bat 的实体脚本
├── PythonSDK/                  # Python 插件 SDK 核心
│   ├── plugin.cpp              # C++ 空壳 DLL（桥接层，无需修改）
│   ├── Demo_TemplatePlugin/    # 插件模板（从这里开始）
│   │   ├── plugin_main.py      # 插件主逻辑
│   │   ├── xlz_sdk.py          # SDK 桥接层（无需修改）
│   │   ├── xlz_constants.py    # 事件/状态常量（无需修改）
│   │   └── xlz_webui.py        # 内嵌浏览器窗口（可选，无需修改）
│   ├── Demo_外部依赖演示/       # 外部依赖（Pillow）使用示例
│   ├── Demo_关键词回复/         # 关键词回复示例
│   ├── Demo_人机验证/           # 人机验证示例
│   └── tools/
│       └── pack_payload.py     # 打包工具（Python -> DLL）
├── CppSDK/                     # C++ SDK（仅供参考，Python 插件无需使用）
│   └── README.md               # C++ API 完整说明
└── xlz_pyruntime/              # Python 运行时加载器（框架依赖，无需修改）
```

---

## 成品区 `Releases/`

**只想写插件、不想碰这个仓库？直接拿 `Releases/` 这一个目录就够了。**

它只放作者需要的东西 —— 空壳 DLL、打包工具、插件模板（`template/`，含作者必带的 `xlz_sdk.py` /
`xlz_constants.py` 与可选的 `xlz_webui.py`）、给 AI 的提示词、包说明 —— 里面的 C++ SDK 源码、Demo
集合、VS 工程文件、`bin\obj` 全都不带，所以**可以单独拷贝 / 压缩 / 发给别人**，不需要 Visual Studio，
也不需要编译。

包内按用途分三层，一眼能看出哪个要动、哪个别动：

| 位置 | 放什么 | 要动吗 |
|---|---|---|
| 根目录 | `README.md`（说明）、`AI编写插件提示词.md`（给 AI） | 只读 |
| `template/` | 插件模板：`plugin_main.py` + `xlz_sdk.py` + `xlz_constants.py` + `xlz_webui.py`（可选） | **整目录复制一份改名**（如 `MyPlugin/`），然后改 `plugin_main.py` |
| `tools/` | `XiaoLiZiVM_PythonCompat.dll`（C++ 空壳）+ `pack_payload.py`（打包工具） | 只当打包输入 / 命令行调用 |

> `xlz_sdk.py` 与 `xlz_constants.py` 必须与 `plugin_main.py` 同级，所以不要单独挑文件拷 ——
> 直接 `xcopy /E /I template MyPlugin`。`xlz_webui.py` 同理（不用内嵌浏览器就完全不用碰它）。
>
> 内嵌浏览器（webui）的运行时内核 `MbKit146.dll` **不在本成品包里** —— 它属于内核分发，
> 要不要随包发、发哪个版本由发布负责人决定；插件拿不到它只会让 webui 不可用，不会崩插件。

包内文件与「30 秒上手」步骤见 [`Releases/README.md`](Releases/README.md)。

> ⚠️ **`Releases/` 是纯生成物，不要手工往里放东西。**
> 每次跑 `rebuild.bat`（或 `rebuild.bat --releases-only`）都会**整体重建**它，
> 并且带一道闸：PE 导出表校验不通过时**拒绝生成**，避免把取不到新接口的坏 DLL 放进成品区。
> 因为它可从源码随时重建，已在 `.gitignore` 里忽略。

| 命令 | 作用 |
|---|---|
| `rebuild.bat` | 完整重建（含组装成品区） |
| `rebuild.bat --releases-only` | 只按当前 DLL 重新组装成品区 |
| `rebuild.bat --no-releases` | 重建但不动成品区 |

---

## 运行时：Python 3.14t

宿主 `小栗子虚拟机.exe` 是 **32 位（Win32）进程**，所以内嵌解释器固定为 32 位构建。

| 项 | 值 |
|---|---|
| 解释器 | **CPython 3.14.8 free-threading build（PEP 703）** |
| 平台 | **Win32 / x86（32 位）** |
| 运行时目录 | `main\corn\python\` |
| 主 DLL | `python314t.dll`（稳定 ABI 转发 DLL 为 `python3t.dll`，没有 `python3.dll`） |
| 标准库 | `Lib\`（不再使用 `python314.zip`，也没有 `._pth`） |
| 扩展模块 | `DLLs\*.cp314t-win32.pyd` |
| GIL | `sys._is_gil_enabled() == False` |
| 最低系统 | **Windows 10+**（3.14 的硬性要求） |

几个容易踩坑的差异：

- **第三方二进制扩展必须是 `cp314t`**：wheel 文件名形如 `pillow-13.0.0-cp314-cp314t-win32.whl`。
  GIL 版 3.14 的 `cp314-cp314-win32` 扩展**不能加载**；`abi3` 也**不行**（它依赖不存在的
  `python3.dll`）。`abi3t` 在 3.14.8t 下**未验证**（没有对应的扩展后缀/轮子标签），不要依赖；
  请按 `cp314t` 或纯 Python（`py3-none-any`）处理。
- **`sys.abiflags` 在 3.14t 里不存在**：需要扩展后缀时用
  `sysconfig.get_config_var("EXT_SUFFIX")`（值为 `.cp314t-win32.pyd`）或 `SOABI`
  （值为 `cp314t-win32`），不要自己拼 `cp314`。
- **3.12 / 3.13 删除的标准库模块不可用**：`distutils`、`imp`、`asynchat`、`asyncore`、
  `smtpd`（3.12 移除），`cgi`、`cgitb`、`pipes`、`telnetlib`、`2to3`/`lib2to3`（3.13 移除）。
  老插件迁移时先全局搜索这些 import。
- **自由线程不等于所有扩展都能并行**：纯 Python 代码可真正多线程并行；若某个 C 扩展在
  调用时重新启用 GIL，则该扩展执行期间仍会串行化。
- **32 位地址空间只有约 2 GB**：实测 3.14t 可分配的大块内存比 3.10.11 低约 38%（小对象约低
  10%）；重型依赖、大图/大模型先在真实宿主里压测，别假设和旧版一样宽裕。
- **不是隔离模式**：包内没有 `site-packages`，但 `Py_Initialize()` 后 `sys.prefix` 为空、
  `sys.executable` 指向宿主 exe；`%APPDATA%\Python\Python314t-32\site-packages`（若存在）和
  `PYTHON*` 环境变量仍会生效。需要隔离时不要依赖这些路径。
- **单线程不会更快**：3.14t 的多线程收益来自并行（本次 4 线程约 2.9x、8 线程约 4.4x），
  单线程与 3.10.11 基本持平甚至略慢；别把“自由线程”当成单线程提速。
- **运行时会在 `Lib\` 下写 `__pycache__`**：发布包本身干净；安装目录只读时每次启动会重新编译。
- **`main\corn\sqlite3.dll` 是旧版同名 DLL**：若宿主在插件 import 前已加载它，`import sqlite3`
  可能失败；这是旧版已有问题，不是 3.14t 引入。不要把旧 `sqlite3.dll` 放进 `python\` 或抢先加载。

---

## 快速开始

### 1. 复制模板

复制 `PythonSDK/Demo_TemplatePlugin/` 目录，重命名为你的插件名，例如 `MyPlugin/`。

> 只拿成品区 `Releases/` 写插件的，复制的是 `Releases/template/`（内容相同，复制后目录里就是三件套）。

### 2. 编辑 `plugin_main.py`

```python
from xlz_sdk import Bridge

api: Bridge = None

def apprun(ctx: dict) -> dict:
    """插件初始化，返回插件信息"""
    global api
    api = Bridge(ctx["dll_path"])
    api.output_log("[MyPlugin] 初始化成功")
    return {
        "app_name": "我的插件",
        "author": "作者名",
        "app_version": "1.0.0",
        "description": "插件描述",
        "permissions": ["输出日志", "发送群消息"],  # 按需申请
    }

def on_group_message(ev: dict) -> int:
    """
    收到群消息
    ev: {'my_uin': int, 'group_uin': int, 'sender_uin': int,
         'message': str, 'group_name': str, 'card': str}
    返回 0=继续传递, 1=拦截
    """
    if ev.get("message") == "测试":
        api.send_group_message(ev["my_uin"], ev["group_uin"], "收到！")
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

def on_uninstall() -> int:
    """插件卸载"""
    return 0
```

### 3. 打包为 DLL

```bash
python PythonSDK/tools/pack_payload.py \
    PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll \
    MyPlugin/ \
    输出/MyPlugin.dll
```

参数说明：
- `XiaoLiZiVM_PythonCompat.dll` — 编译好的 C++ 空壳 DLL（已在 `PythonSDK/bin/Release/` 提供）
- `MyPlugin/` — 你的插件目录（含 `plugin_main.py`、`xlz_sdk.py` 及依赖）
- `输出/MyPlugin.dll` — 生成的插件 DLL

生成的 DLL 直接放入框架插件目录即可。

---

## 可用回调事件

| 函数名 | 触发时机 | 参数字段 |
|---|---|---|
| `apprun(ctx)` | 插件加载/初始化 | `dll_path`, `pluginkey` |
| `on_enable()` | 插件启用 | 无参数，返回 0=正常 / 1=拒绝启用 |
| `on_disable()` | 插件禁用 | 无参数 |
| `on_uninstall()` | 插件卸载 | 无参数 |
| `on_setting()` | 用户点击插件设置 | 无参数 |
| `on_group_message(ev)` | 收到群消息 | `my_uin`, `group_uin`, `sender_uin`, `message`, `group_name`, `card` |
| `on_private_message(ev)` | 收到私聊消息 | `my_uin`, `sender_uin`, `message` |
| `on_event_message(ev)` | 其他事件 | `my_uin`, `event_type`, `event_sub_type`, `trigger_uin`, `source_group_uin`, `operate_uin`, `message` |

所有回调返回 `0`（继续）或 `1`（拦截）。建议用常量而不是魔法数字，见下节。

---

## 事件常量（`xlz_constants.py`）

易语言 SDK 常量表里的事件 / 状态常量已迁移到 `xlz_constants.py`，**数值与框架内部定义逐条一致**（源：`cornsdk6.0.8.e` 常量表）。全部为 `IntEnum`，按易语言常量前缀分成 13 组：

| 分组（Python） | 对应易语言前缀 | 条数 | 用途 |
|---|---|---|---|
| `MessageHandle` | `消息处理_` | 2 | 消息回调返回值：`INTERCEPT`(1) / `IGNORE`(0) |
| `MessageType` | `消息类型_` | 8 | 私聊/临时会话的消息类型（`TEMPORARY_SESSION`=141 等） |
| `ChannelPushType` | `频道推送类型_` | 3 | `SUB_CHANNEL_MESSAGE` / `DIRECT_MESSAGE` / `EVENT_MESSAGE` |
| `GroupEvent` | `群事件_` | 41 | 群事件子类型（进群、退群、禁言、群名片变更…） |
| `ZoneEvent` | `空间事件_` | 1 | `RELATED_TO_ME`(29) |
| `FrameworkEvent` | `框架事件_` | 3 | `LOGIN_SUCCESS` / `LOGIN_FAILED` / `RESTARTING_FOR_UPDATE` |
| `DiscussionGroupEvent` | `讨论组事件_` | 5 | 讨论组成员/名称变更（300–304） |
| `FriendEvent` | `好友事件_` | 20 | 好友请求、签名变更、点赞、输入状态…（100–119） |
| `LoginEvent` | `登录事件_` | 5 | 电脑/移动端上下线、其他应用登录验证（200–204） |
| `VirtualMachineEvent` | `虚拟机事件_` | 1 | `ONEBOT_RAW_REQUEST`(205) |
| `SubChannelEvent` | `子频道事件_` | 4 | 子频道系统提示、贴表情、撤回、直播状态（0–3） |
| `ChannelEvent` | `频道事件_` | 1 | `USER_APPLY_JOIN_CHANNEL`(4) |
| `EnableResponse` | `启用响应_` | 2 | 插件启用回调返回值：`REJECT`(8) / `DONE`(0) |

> 说明：易语言常量表里「消息处理_拦截 / 启用响应_拒绝」等原本是**文本常量**（`"1"` / `"8"`），
> 这里按其作为**回调返回值的实际语义**暴露为整数枚举。

用法示例：

```python
from xlz_sdk import Bridge
from xlz_constants import GroupEvent, MessageHandle, EnableResponse

api: Bridge = None

def on_group_message(ev: dict) -> int:
    # ev 里的类型字段是真·整数，可直接与 IntEnum 比较
    if ev.get("message_sub_type") == GroupEvent.MEMBER_JOINED:
        api.output_log("有人入群")
    if ev.get("message") == "测试":
        api.send_group_message(ev["my_uin"], ev["group_uin"], "收到！")
        return MessageHandle.INTERCEPT          # == 1
    return MessageHandle.IGNORE                 # == 0

def on_enable() -> int:
    return EnableResponse.DONE                  # == 0
```

事件类型 / 子类型字段与枚举的对应关系：

- 群/私聊消息 `ev["message_type"]`、`ev["message_sub_type"]` → `MessageType`、`GroupEvent`
- 其他事件 `ev["event_type"]`、`ev["event_sub_type"]` → `GroupEvent` / `FriendEvent` / `LoginEvent` …
- 回调返回值 → `MessageHandle`（消息类）、`EnableResponse`（启用类）

`xlz_constants.py` 与 `xlz_sdk.py` 一样，**直接随插件目录打包**（4 个 Demo 均已包含）。C++ 端同名头文件是 `xlz_constants.h`（`GroupEvent::MemberJoined` ↔ `GroupEvent.MEMBER_JOINED`）。

---

## 可申请的权限

在 `apprun` 返回值的 `permissions` 字段中填写中文权限名（列表）：

```python
"permissions": [
    "输出日志",
    "发送群消息",
    "发送好友消息",
    "上传群图片",
    "上传好友图片",
    "上传群语音",
    "上传好友语音",
    "上传群文件",
    "撤回消息_群聊",
    "禁言群成员",
    "删除群成员",
    "全员禁言",
    "取群列表",
    "取群成员列表",
    "取群名片",
    "设置群名片",
    "取昵称_从缓存",
    "强制取昵称",
    "取图片下载地址",
    "取好友文件下载地址",
    "处理好友验证事件",
    "处理群验证事件",
    "QQ点赞",
    "群聊打卡",
    "分享音乐",
    "取群成员信息",
    "取插件数据目录",
    "重载自身",
]
```

完整 API 列表及参数说明见 [CppSDK/README.md](CppSDK/README.md)（与 Python SDK 接口一一对应）。

---

## 添加外部依赖

> **必须使用 32 位（Win32）、CPython 3.14 自由线程（`cp314t`）的包，否则无法加载。**

### 安装方式

使用 pip 安装到插件目录：

```bash
# 必须指定：32 位、CPython 3.14 自由线程（cp314t）、目标目录为你的插件文件夹
"C:\Python314t_32\python.exe" -m pip install pillow \
    --only-binary=:all: \
    --python-version 3.14 \
    --platform win32 \
    --abi cp314t \
    --target MyPlugin/
```

`--abi cp314t` 是关键：不加它，pip 可能选中 GIL 版的 `cp314-cp314-win32` 轮子，装进去也无法 import。

或者直接从 [PyPI](https://pypi.org/) 下载文件名带 `cp314-cp314t-win32` 的 wheel
（例如 `pillow-13.0.0-cp314-cp314t-win32.whl`）手动解压到插件目录。

### 使用方式

外部依赖放在插件目录内，打包时会一并打入 DLL：

```
MyPlugin/
├── plugin_main.py
├── xlz_sdk.py
├── PIL/                  # Pillow 依赖
│   ├── Image.py
│   └── ...
└── pillow-13.x.dist-info/
```

在 `plugin_main.py` 中直接 import 即可：

```python
from PIL import Image, ImageDraw, ImageFont
import io

def on_group_message(ev: dict) -> int:
    if ev["message"] == "生成图片":
        img = Image.new("RGB", (200, 200), color=(30, 30, 30))
        buf = io.BytesIO()
        img.save(buf, format="PNG")
        img_code = api.upload_group_image(
            ev["my_uin"], ev["group_uin"], False, buf.getvalue()
        )
        api.send_group_message(ev["my_uin"], ev["group_uin"], img_code)
    return 0
```

### 注意事项

- **必须 32 位（win32）**：框架是 32 位进程，64 位 `.pyd` 无法加载
- **必须 CPython 3.14 自由线程（`cp314t`）**：运行时是 3.14.8 free-threading 构建；GIL 版
  `cp314` 的 `.pyd` 不能加载，`abi3` 也不行（`abi3t` 在 3.14.8t 下未验证，不要依赖）
- 纯 Python 包（无 `.pyd`）不受此限制
- 需要 `ctypes` 的包可以直接使用，运行时已内置支持

---

## 打包工具详细说明

`PythonSDK/tools/pack_payload.py` 将插件目录压缩后附加到空壳 DLL 尾部。

```bash
python pack_payload.py <shell_dll> <payload_dir> <out_dll>
```

| 参数 | 说明 |
|---|---|
| `shell_dll` | C++ 空壳 DLL（`XiaoLiZiVM_PythonCompat.dll`） |
| `payload_dir` | 插件目录（含 `plugin_main.py`） |
| `out_dll` | 输出 DLL 路径 |

**示例**：

```bash
python PythonSDK/tools/pack_payload.py \
    PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll \
    PythonSDK/Demo_TemplatePlugin/ \
    output/MyPlugin.dll
```

---

## 重新编译 C++ 产物（改了 `plugin.cpp` 或 `CppSDK` 后必做）

> ⚠️ **源码改了 ≠ 产物改了。** `PythonSDK` 依赖 `CppSDK`（含 `..\CppSDK\include` 与 `..\CppSDK\src`），
> 改完 C++ 源码**必须重编**，否则 `bin\Release\` 里的 DLL 还是旧的 ——
> 新 `xlz_sdk.py` 调用的新接口在旧 DLL 里取不到，插件会在初始化时直接失败。

### 方式一：一键重建（推荐）

双击根目录的 `rebuild.bat`（脚本在 `tools/rebuild_all.py`）：

```bat
rebuild.bat                :: 完整重建：编 2 个 DLL + 部署 main\corn\ + 重打 4 个 Demo 成品 + 校验导出表 + 组装 Releases\
rebuild.bat --skip-build   :: 跳过 MSBuild，只做部署 + 打包 + 校验 + 组装成品区
rebuild.bat --releases-only :: 只按当前 DLL 重新组装 Releases\ 成品区（不编译、不部署、不重打 Demo）
rebuild.bat --dry-run      :: 只打印计划，不写任何文件
rebuild.bat --no-deploy    :: 不往 main\corn\ 拷 xlz_pyruntime.dll
rebuild.bat --no-releases  :: 重建但不动 Releases\ 成品区
```

脚本会自动用 `vswhere` 定位 MSBuild；编译完成后按 `exports.def` **逐条核对 PE 导出表**，
少一个就报错退出（能直接抓住"源码改了但没编进去"这类问题），**并且不会再组装 `Releases\`** ——
成品区里永远是能用的那份 DLL。

### 方式二：手动（Visual Studio）

1. 打开 `PythonSDK/PythonCompatPlugin.sln` → 选 `Win32` + `Release` → 生成
   输出 `PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll`
2. 打开 `xlz_pyruntime/xlz_pyruntime.sln` → 选 `Win32` + `Release` → 生成
   输出 `xlz_pyruntime/bin/Release/xlz_pyruntime.dll`，需拷到框架的 `main\corn\`
3. 用 `pack_payload.py` 重新打包各 Demo 成品
4. 跑 `rebuild.bat --releases-only` 重新组装 `Releases\` 成品区（不跑的话成品区还是旧 DLL）

> **必须选 Win32，不能用 x64。**

---

## xlz_sdk.py API 速查

| 方法 | 说明 |
|---|---|
| `output_log(msg)` | 输出日志到框架控制台 |
| `send_group_message(my_uin, group_uin, msg)` | 发送群消息 |
| `send_private_message(my_uin, friend_uin, msg)` | 发送私聊消息 |
| `upload_group_image(my_uin, group_uin, is_flash, pic_bytes)` | 上传群图片，返回图片代码 |
| `upload_friend_image(my_uin, friend_uin, is_flash, ptr, size)` | 上传好友图片 |
| `upload_group_audio(my_uin, group_uin, type, text, ptr, size)` | 上传群语音 |
| `upload_group_file(my_uin, group_uin, file_path, folder)` | 上传群文件 |
| `get_group_list(my_uin)` | 取群列表 |
| `get_group_member_list(my_uin, group_uin)` | 取群成员列表 |
| `get_group_card(my_uin, group_uin, member_uin)` | 取群名片 |
| `set_group_card(my_uin, group_uin, member_uin, card)` | 设置群名片 |
| `mute_group_member(my_uin, group_uin, member_uin, seconds)` | 禁言群成员 |
| `remove_group_member(my_uin, group_uin, member_uin, refuse)` | 踢出群成员 |
| `recall_group_message(my_uin, group_uin, random, req)` | 撤回群消息 |
| `get_image_download_url(image_code, my_uin, group_uin)` | 取图片下载地址 |
| `get_nickname_from_cache(other_uin)` | 从缓存取昵称 |
| `get_nickname_force(my_uin, other_uin)` | 强制取昵称 |
| `get_plugin_data_directory()` | 取插件数据目录 |
| `get_framework_version()` | 取框架版本 |
| `get_plugin_self_version()` | 取插件自身版本号 |
| `reload_itself(new_dll_path)` | 重载自身 |
| `qq_like(my_uin, other_uin)` | QQ点赞 |
| `mute_all(my_uin, group_uin, enabled)` | 全员禁言 |

完整参数说明见 [CppSDK/README.md](CppSDK/README.md)。

---

## 自由线程（并发）

> 运行时是 **CPython 3.14.8 free-threading（PEP 703，`cp314t`）**，GIL 默认关闭：
> `sys._is_gil_enabled()` 返回 `False`。三件事同时成立：
> 1. 运行时初始化后**主动释放 GIL**，框架消息线程之外的其他线程可以正常运行 Python 代码；
> 2. 桥接层（`plugin.cpp`）已**线程安全**，可在任意 Python 线程内直接调用 `api.*` 方法；
> 3. 事件可选择**异步派发**到 C++ 侧工作线程，慢速事件不再阻塞框架消息线程。
>
> 与旧 GIL 构建的区别：纯 Python 线程现在可以**真正同时执行字节码**，CPU 密集代码
> 不再被“同一时刻只有一个线程跑 Python”限制。

### 三种并发手段

| 手段 | 适用场景 | 用法 |
|---|---|---|
| `run_in_thread(fn, *a, **kw)` | 事件回调里做耗时活（网络/IO/图像处理），不想卡住框架消息线程 | 返回 `Future`，可 `.result()` 取结果、`.exception()` 取异常 |
| `spawn(target, *a, **kw)` | 起一个 detached 线程，不关心返回值 | 返回 `Thread`（daemon） |
| `api.set_async(True)` | 让**所有**事件都走异步派发，回调立即返回 | 全局开关，见下方"注意" |

```python
from xlz_sdk import Bridge, run_in_thread, spawn, shutdown_threads

api: Bridge = None

def apprun(ctx: dict) -> dict:
    global api
    api = Bridge(ctx["dll_path"])
    # 可选：开启异步事件派发（见下）
    # api.set_async(True)
    return {"app_name": "我的插件", "author": "作者名", "app_version": "1.0.0",
            "description": "插件描述", "permissions": ["输出日志", "发送群消息"]}

def _slow_task(my_uin: int, group_uin: int, message: str) -> None:
    # 这里可以放心做耗时操作，不会拖住框架消息线程
    import time
    time.sleep(3)
    api.send_group_message(my_uin, group_uin, f"处理完成：{message}")

def on_group_message(ev: dict) -> int:
    if ev.get("message") == "慢任务":
        # 把耗时逻辑丢到线程池，回调立刻返回，框架不会被卡住
        run_in_thread(_slow_task, ev["my_uin"], ev["group_uin"], ev["message"])
        return 1
    return 0

def on_uninstall() -> int:
    # 卸载时回收线程池（wait=False 表示不等待在跑的线程结束）
    shutdown_threads(wait=False)
    return 0
```

### 注意事项

- **`set_async(True)` 与"拦截"互斥**：异步模式下事件被投递到工作线程后回调**立即返回 0**，因此 `return 1` **不再能拦截后续插件**。需要拦截语义（抢答、独占处理）时请保持默认的同步模式。
- **桥接层已线程安全，但仍建议遵循"能不同线程碰同一可变状态就不碰"**：跨线程共享 Python 对象时请自行加锁（`threading.Lock`）。
- **`shutdown_threads()` 在 `on_uninstall` / `on_disable` 里调用**，避免插件卸载后还有线程池里的任务在跑。
- **这是真正的自由线程构建**：插件自己的后台线程可以并行跑 CPU 密集型纯 Python 代码，多个线程能同时执行字节码；但若某个 C 扩展在调用期间重新启用 GIL，则该扩展调用仍会串行化。与"框架消息线程是单线程"是两码事。

---

## 内嵌浏览器（webui，可选）

需要「在框架窗口里显示网页 / 用 HTML+JS 做界面」时才用。相关模块：`PythonSDK/Demo_TemplatePlugin/xlz_webui.py`
（成品区里是 `template/xlz_webui.py`），它是 `ctypes` 直接调 `MbKit146.dll`（miniblink 内核，与易语言
`MB浏览框` 同一套）的封装，**不经过 C++ 桥接层** —— 所以 C++ 侧一行都没改，Python 插件想用直接 `import`。

### 最小可用例子

```python
from xlz_webui import WebView, MbKitError, MbKitThreadError

def on_group_message(ev: dict) -> int:
    if ev.get("message") == "网页":
        hwnd = api.get_framework_main_window_handle()   # 宿主主窗口句柄
        wv = WebView(hwnd)                              # 自动按搜索顺序找 MbKit146.dll
        wv.on_document_ready2(lambda url: api.output_log("页面就绪 " + url))
        wv.on_console(lambda lv, text, src, line: api.output_log(text))
        wv.js_bind_function("pyEcho", lambda s: "来自 Python：" + str(s), 1)
        wv.load_html("<button onclick='alert(pyEcho(1))'>调用 Python</button>")
    return 0
```

模板（`plugin_main.py`）里已经放了一份**注释状态**的完整示例（`_open_webui()`），取消注释即可试。

### 方法一览

| 方法 | 说明 |
|---|---|
| `WebView(hwnd=0, *, dll_path=None, transparent=False, input_hook=True, check_thread=True, strict_version=True)` | 构造；`hwnd=0` 为无窗口（仅初始化 / 自检用） |
| `bind(hwnd, *, transparent=None, input_hook=None, raise_on_thread=True)` / `destroy()` / `close()` | 绑窗口（建 view + 接管窗口过程）/ 销毁并还原窗口 |
| `load_url` / `load_file` / `load_html(html, base_url=None)` | 三种加载方式（`load_html_with_base_url` 为别名） |
| `run_js` / `run_js_to_string` / `run_js_to_int` | 执行 JS，取回 `None` / 字符串 / 整数 |
| `js_bind_function(name, fn, argc=0)` | 把 Python 函数暴露给网页 JS（名字只能 ASCII） |
| `on_document_ready2(fn)` / `on_console(fn)` / `on_loading_finish(fn)` / `on_title_changed(fn)` | 事件回调；传 `None` 取消注册 |
| `reload` / `stop_loading` / `go_back` / `go_forward` / `is_loading` / `is_document_ready` | 导航控制 |
| `get_url` / `get_title` / `get_source` / `view_size` | 读取页面信息 |
| `set_zoom` / `set_user_agent` / `set_cookie_enabled` / `set_debug_config` | 杂项设置 |
| `post(fn)` / `pump(ms)` / `repaint()` / `resize(w, h)` | 跨线程投递 / 泵消息 / 重绘 / 改大小 |
| `WebView.self_diag()` / `MbKit.self_diag()` / `detach_all()` / `is_unloading()` | 自检 / 全局解绑 / 卸载标志 |

### 三条必须守住的规则

1. **线程铁律**：`WebView(...)` 与 `bind()` 必须在「承载窗口的创建线程」上调用 —— 对宿主窗口就是
   框架消息线程，也就是 `on_*` 回调所在的线程，所以**在回调里直接调是对的**。**绝对不要**放进
   `run_in_thread` / `spawn`：那是灰屏（白屏）的经典原因。`bind()` 会对比窗口创建线程与当前线程，
   不一致直接抛 `MbKitThreadError`（`bind(..., raise_on_thread=False)` 只降级为警告，后果自负）。
   跨线程想动已建好的 view，请用 `wv.post(fn)` 投递回宿主消息线程执行。
2. **入口顺序**：构造 → `bind(hwnd)` → 注册事件 / `js_bind_function` → `load_*`。要立刻执行 JS
   必须等 `on_document_ready2` 回调，之前 DOM 还没生成。
3. **热重载契约**：`on_uninstall` 里顺序不能换 ——
   ① 先停一切会碰窗口的线程 / 定时器（`shutdown_threads(wait=False)`）；
   ② 再 `xlz_webui.detach_all()`（内部会先立卸载标志 `is_unloading()`，再逐个销毁本插件的所有 view）；
   ③ 最后内核全解绑（`detach_all` 尾部完成）。
    注意：`detach_all()` 是终止性收摊，不会自动清除卸载标志；若同一进程里要“禁用后再启用”并复用本模块对象，请在 `on_enable()` 里调用 `xlz_webui.end_unload()`（热重载会重新载入模块，标志自动复位）。
   所有 `on_*` 回调进来先看卸载标志，是卸载中就立刻返回，**绝不在 view 销毁后进用户回调**。

```python
from xlz_webui import detach_all as webui_detach_all

def on_uninstall() -> int:
    shutdown_threads(wait=False)     # ① 停线程
    webui_detach_all()               # ② + ③ 销毁 view、内核全解绑（内部先立卸载标志）
    return 0
```

### `MbKit146.dll` 的放置与版本

- 搜索顺序：`dll_path=` 显式指定 → 环境变量 `XLZ_MBKIT_DLL` → 已加载模块 → 宿主 exe 目录 →
  插件目录 → `%TEMP%\ElangMbKit` → 裸名（走系统搜索路径）。
- **找不到不会崩插件**：构造时抛 `MbKitNotFoundError`，异常消息里列出所有搜索过的路径（命中的那条
  会标 `<-- 命中`），按提示放 DLL 即可。
- 版本要求 `MbKit/1.4.5`。注意 `MbKit_Version()` 在**内核启动之前返回空串**，所以版本校验发生在
  第一次 `bind()`（启动内核）之后 —— 别在 `apprun` 里断言版本；要复查用 `MbKit.refresh_version()`。
- 内核 DLL 是 32 位（Win32），必须和框架 / 插件同为 32 位环境。

### 已知限制（如实记录）

- 内核**一个进程只有一份**，换内核只能重启宿主 exe。
- 快速连续热重载插件（约 5 次）仍可能让宿主崩在 `win32u.dll+0x11cc` —— 属于内核侧 WebView2
  异步释放的竞态，Python 侧无法兜住，目前只能先「别连着重载」。
- view 销毁是**延迟回收**（约 400 ms），刚 `destroy()` 的窗口不要立刻复用。
- 只有**窗口模式**经过验证；`hwnd=0` 的无窗口模式仅能初始化内核，不保证渲染。
- 输入法只做了候选窗定位（未处理内联编辑）；`js_bind_function` 的函数名只能是 ASCII 字母 / 数字 / 下划线；窗口过程被托管后会带来每消息的额外开销（可接受，但别在 `WM_MOUSEMOVE` 里做重活）。

---

## 常见问题

**Q: webui 窗口一片白 / 灰？**

九成是 `bind()` 不在窗口创建线程（比如放进了 `run_in_thread`）。先看是否抛了 `MbKitThreadError`；
再确认 `hwnd` 用的是 `api.get_framework_main_window_handle()`，而不是自己 `CreateWindow` 出来的窗口。
详见[「内嵌浏览器（webui，可选）」](#内嵌浏览器webui可选)章节。

**Q: 报 `MbKitNotFoundError`？**

按异常里列的路径把 `MbKit146.dll` 放一份（插件目录或宿主目录最省事），或者设环境变量
`XLZ_MBKIT_DLL` / 传 `WebView(dll_path=...)`。

**Q: 重载几次插件后宿主直接崩了？**

已知内核竞态（`win32u.dll+0x11cc`），发生在快速连续重载时。目前只能节制重载频率，详见
[「内嵌浏览器（webui，可选）」](#内嵌浏览器webui可选)的「已知限制」。

**Q: 插件加载后没有反应？**

查看框架日志目录下的 `debug.log`，确认 `apprun success` 是否出现。

**Q: `import xxx` 报错找不到模块？**

确认依赖包是 `cp314-cp314t-win32`（或纯 Python `py3-none-any`）版本，且已放在插件目录内。

**Q: `.pyd` 文件加载失败？**

确认是 32 位（`win32`）且 ABI 为 `cp314t` 的版本：不能是 `win_amd64`，也不能是 GIL 版 `cp314` 或 `abi3`。

**Q: 多个插件之间事件串运行？**

这是已知问题，SDK 已通过函数作用域隔离解决，确保使用最新版 `XiaoLiZiVM_PythonCompat.dll`。

**Q: 回调里做耗时操作（网络请求 / 图像处理）会卡住框架吗？**

会——同步模式下事件在框架消息线程内执行。请把耗时逻辑用 `run_in_thread(fn, ...)` 丢到线程池，或整体开启 `api.set_async(True)`（此时无法再用返回值拦截后续插件）。详见[「自由线程（并发）」](#自由线程并发)章节。

**Q: 我可以在自己起的线程里调用 `api.send_group_message` 吗？**

可以。本版本起桥接层已线程安全，`api.*` 可在任意线程调用；但请勿跨线程共享未加锁的可变 Python 状态。

---

## License

GNU Lesser General Public License v3.0 (LGPL-3.0)

## C++ 兼容层修改发布说明

本项目的 C++ 兼容层（`PythonSDK/plugin.cpp` 与 `xlz_pyruntime/xlz_pyruntime.cpp`）按 LGPL-3.0 进行发布与约束。

若你修改了上述任一文件并对外发布（无论源码或二进制），你必须公开你对这两部分兼容层代码的修改源码。

这并不强制你开源整个项目；你自己的业务代码、上层插件逻辑可以保持闭源。但兼容层修改部分必须可获取。



