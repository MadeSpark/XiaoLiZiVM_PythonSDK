## 小栗子 QQ 机器人框架 Python 插件开发提示词模板

这是一个 **小栗子 QQ 机器人框架插件的 Python SDK 和 Demo 例子**。  
你可以先阅读仓库根目录下的 `README.md`，了解 **目录结构、回调约定（`apprun`、群消息/私聊/事件/启用禁用）**、**外部依赖要求（必须 32 位 + CPython 3.14 自由线程 `cp314t`）**、以及 **打包 DLL 的方法（`PythonSDK/tools/pack_payload.py`）**。

- **`Demo_TemplatePlugin`**：最基础模板，包含标准回调结构和 `xlz_sdk.Bridge` 用法。  
- **`Demo_关键词回复`**：关键词回复示例，可参考规则匹配、配置读写、消息发送。  
- **`Demo_外部依赖演示`**：外部依赖示例（Pillow），可参考三方包目录内置与上传图片流程。

现在请你根据上述 SDK 结构和示例，**编写一个新的 Python 插件**，并输出完整代码与打包步骤，满足以下要求。

---

## 插件基本信息（请在实现时填写）

插件名称：  
插件版本：  
插件作者：  
插件介绍：  

> 要求：在 `apprun(ctx)` 返回的字典中正确填写，并作为可修改常量保留。

---

## 具体开发要求

！！！请自行填写需求！！！

### 1. 基础要求

- 使用 `PythonSDK` 的标准结构开发：
  - 必须包含 `plugin_main.py`
  - 必须通过 `from xlz_sdk import Bridge` 初始化 `api`
  - 把 `xlz_sdk.py` 与 `xlz_constants.py` 一并复制到插件目录（打包时缺一不可）
- 在 `apprun(ctx)` 中：
  - 使用 `api = Bridge(ctx["dll_path"])`
  - 返回包含 `app_name / author / app_version / description / permissions` 的字典
- 回调函数按模板命名：
  - `on_enable()`（无参数，返回 `int`）
  - `on_disable()`（无参数）
  - `on_uninstall()`（无参数）
  - `on_setting()`（无参数）
  - `on_group_message(ev: dict) -> int`
  - `on_private_message(ev: dict) -> int`
  - `on_event_message(ev: dict) -> int`

### 2. 事件处理与返回值规范

- 群聊/私聊/事件回调：
  - 返回 `MessageHandle.IGNORE`（`0`）：继续传递
  - 返回 `MessageHandle.INTERCEPT`（`1`）：拦截
- 启用回调 `on_enable()`：
  - 返回 `EnableResponse.DONE`（`0`）：正常启用
  - 返回 `EnableResponse.REJECT`（`8`）：拒绝启用（框架将停止加载插件）
- 对 `ev` 参数使用 `dict.get()` 读取字段，避免键缺失异常。

### 2.1 事件常量（`xlz_constants.py`）

**禁止使用魔法数字**，事件类型 / 子类型一律用常量比较：

```python
from xlz_constants import MessageHandle, EnableResponse, GroupEvent, FriendEvent, LoginEvent

def on_event_message(ev: dict) -> int:
    et = ev.get("event_type")          # 与 IntEnum 可直接比较
    if et == GroupEvent.MEMBER_JOINED:
        api.output_log("有人入群")
    elif et == FriendEvent.FRIEND_REQUEST:
        api.output_log("收到好友请求")
    return MessageHandle.IGNORE
```

常量分 13 组：`MessageHandle`、`MessageType`、`ChannelPushType`、`GroupEvent`、`ZoneEvent`、
`FrameworkEvent`、`DiscussionGroupEvent`、`FriendEvent`、`LoginEvent`、`VirtualMachineEvent`、
`SubChannelEvent`、`ChannelEvent`、`EnableResponse`。详见 `README.md`「事件常量」章节与
`xlz_constants.py` 内的逐条注释（数值与框架内部一致，勿改）。

### 2.2 自由线程（可选）

插件已支持自由线程（运行时已释放 GIL，桥接层线程安全）：

- 回调里做耗时操作时用 `run_in_thread(fn, *args)`（返回 `Future`）或 `spawn(target, *args)`，
  不要阻塞框架消息线程；
- 需要事件整体异步派发时用 `api.set_async(True)`（代价：回调返回值不再能拦截后续插件）；
- `on_uninstall` / `on_disable` 里调用 `shutdown_threads(wait=False)` 回收线程池。

### 3. 权限与 API 调用

- 所有需要调用的框架 API，必须在 `permissions` 中声明。
- 优先使用 `xlz_sdk.py` 已封装接口，例如：
  - `output_log`
  - `send_group_message`
  - `send_private_message`
  - `upload_group_image`
  - `get_plugin_data_directory`
- 其它 API 与调用事项可参考：`CppSDK/README.md`。

### 4. 配置文件与可维护性

- 若插件有配置项：
  - 配置文件放在 `api.get_plugin_data_directory()` 下
  - 首次运行自动生成默认配置
  - 配置保存使用 UTF-8
- 推荐在代码中拆分：
  - 常量区（插件信息、默认配置）
  - 业务逻辑区（匹配与处理）
  - 桥接区（回调入口）

### 5. 外部依赖（重点）

- 如果使用第三方库（如 Pillow、requests 等），必须满足：
  - **CPython 3.14 自由线程**（`cp314t`）
  - **win32（32位）**
- 不能使用 `win_amd64`、GIL 版 `cp314` 或 `abi3` 的二进制依赖；`abi3t` 在 3.14.8t 下未验证
  （没有对应的扩展后缀/轮子标签），请按 `cp314t` 或纯 Python 处理。
- 依赖应直接放入插件目录（与 `plugin_main.py` 同级或子目录），打包时一起进入 DLL。

获取依赖有两种方式（都要保证 cp314t + win32）：

1. **命令安装**（推荐）

```bash
"C:\Python314t_32\python.exe" -m pip install pillow \
  --only-binary=:all: \
  --python-version 3.14 \
  --platform win32 \
  --abi cp314t \
  --target MyPlugin/
```

2. **PyPI 手动下载**

- 到 PyPI 项目页面下载对应 wheel
- 文件名必须匹配：`cp314` + `cp314t` + `win32`
- 例如：`xxx-cp314-cp314t-win32.whl`
- 将 wheel 解压到插件目录，或离线安装到插件目录后再打包

### 5.1 基础库说明（来自解释器，无需重复下载）

标准库随 Python 解释器提供（与你使用的 CPython 3.14t 运行时一起存在），不需要单独下载，也不应作为第三方依赖重复打包。

规则：

- 对 `json`、`re`、`pathlib`、`ctypes`、`email` 等标准库，默认视为解释器内置。
- 只对**第三方依赖**（非标准库）进行安装/下载建议与打包。
- 如需新增第三方依赖，必须明确告知用户缺少哪些依赖，并给出 `cp314t + win32` 建议包名。
- 若依赖需求不确定，也要先向用户说明原因并给出可选依赖清单。

基础库依赖列表（常用标准库，随 CPython 3.14t 解释器提供）：

| 分类 | 模块 |
|---|---|
| 文件与路径 | `os`, `pathlib`, `shutil`, `glob`, `tempfile` |
| 文本与数据 | `re`, `json`, `csv`, `configparser`, `datetime` |
| 编码与压缩 | `base64`, `hashlib`, `hmac`, `zlib`, `zipfile`, `tarfile` |
| 网络与协议 | `urllib`, `http`, `socket`, `ssl` |
| 并发与进程 | `threading`, `queue`, `subprocess`, `multiprocessing` |
| 系统接口 | `ctypes`, `platform`, `logging`, `traceback` |
| 工具与运行时 | `typing`, `dataclasses`, `functools`, `itertools`, `collections` |
| 持久化 | `sqlite3`, `pickle`, `shelve` |

> 备注：以上是插件开发常用清单，不是完整标准库；完整列表见 https://docs.python.org/3.14/library/index.html
>
> 注意：`distutils`、`imp`、`asynchat`、`asyncore`、`smtpd`（3.12 移除）以及
> `cgi`、`cgitb`、`pipes`、`telnetlib`、`2to3`/`lib2to3`（3.13 移除）在 3.14t 中不存在，
> 不要在新插件里 import。

### 6. 内嵌浏览器（webui，可选）

需要「在框架窗口里显示网页 / 用 HTML+JS 做界面」时才用。相关模块是同级的 `xlz_webui.py`
（模板里已自带，原样使用，不要改它）：它是 `ctypes` 直接调 `MbKit146.dll`（miniblink 内核，
与易语言 `MB浏览框` 同一套）的封装。

最小用法（**顺序不能变**）：

```python
from xlz_webui import WebView, MbKitError, MbKitThreadError

hwnd = api.get_framework_main_window_handle()   # 宿主主窗口句柄
wv = WebView(hwnd)                              # 自动搜索 MbKit146.dll；也可 dll_path=... 或环境变量 XLZ_MBKIT_DLL
wv.on_document_ready2(lambda url: api.output_log("页面就绪 " + url))
wv.on_console(lambda lv, text, src, line: api.output_log(text))
wv.js_bind_function("pyEcho", lambda s: "来自 Python：" + str(s), 1)
wv.load_html("<h1>hello</h1>")                  # 或 load_url("https://...") / load_file(r"D:\x.html")
```

必须遵守的三条：

1. **线程铁律**：`WebView(...)` 与 `bind()` 必须在「承载窗口的创建线程」上调用 —— 对宿主窗口
   就是框架消息线程，即 `on_*` 回调所在线程。**绝对不要**放进 `run_in_thread` / `spawn`，
   那是灰屏（白屏）的经典原因；本模块会直接抛 `MbKitThreadError`。跨线程操作已建好的 view
   （例如 `wv.post(fn)`）才允许。
2. **入口顺序**：构造 → 绑窗口 → 注册事件 / `js_bind_function` → `load_*`。要立刻执行 JS 必须
   等 `on_document_ready2` 回调，之前 DOM 还没生成。`js_bind_function` 的函数名只能是 ASCII
   字母 / 数字 / 下划线。
3. **热重载契约**：`on_uninstall` 里顺序固定为「① 停线程 / 定时器 → ② 销毁 view → ③ 内核全解绑」，
   即先 `shutdown_threads(wait=False)`，再 `xlz_webui.detach_all()`；每个回调进来先看
   `xlz_webui.is_unloading()`，是卸载中就直接返回。

其他要点：

- `MbKit146.dll` **不随插件打包**，需与插件 DLL 一起部署（插件目录、宿主目录或
  `%TEMP%\ElangMbKit`）；找不到会抛 `MbKitNotFoundError` 并列出搜索路径，不会崩插件。
- 版本要求 `MbKit/1.4.5`；`MbKit_Version()` 在**内核启动前返回空串**，版本校验发生在第一次
  启动内核之后 —— 不要在 `apprun` 里断言版本。
- 已知限制：内核一个进程只有一份（换内核只能重启宿主）；约 5 次快速热重载仍可能让宿主崩在
  `win32u.dll+0x11cc`（内核 WebView2 异步释放竞态，不在 python 侧可控范围）；view 销毁延迟约
  400 ms 回收；只有窗口模式验证过。
- 用不上就不要 `import` —— 模板里是注释状态，不影响打包，也不影响其它能力。

### 7. 打包与交付

开发完成后，必须给出可直接执行的打包命令：

```bash
python PythonSDK/tools/pack_payload.py <shell_dll> <payload_dir> <out_dll>
```

示例：

```bash
python PythonSDK/tools/pack_payload.py \
  PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll \
  PythonSDK/Demo_TemplatePlugin/ \
  output/MyPlugin.dll
```

并说明：输出 DLL 可直接放入框架插件目录测试。

---

## 输出格式要求（让 AI 按此输出）

1. 先给出插件功能说明（简短）  
2. 再给出完整 `plugin_main.py` 代码（可直接运行）  
3. 若有配置文件，给出默认配置样例  
4. 给出权限清单与用途说明  
5. 列出“标准库/第三方依赖”判断结果，并说明是否需要额外下载  
6. 最后给出打包命令与测试步骤

---

## 总结

请基于本仓库 `PythonSDK` 的模板和示例，实现一个符合小栗子框架规范的 Python 插件：

- 回调签名正确（尤其 `on_enable/on_disable/on_uninstall/on_setting` 为无参数）
- 权限声明完整
- 外部依赖符合 **cp314t + win32**（纯 Python 包可为 `py3-none-any`）
- 能通过 `pack_payload.py` 打包成可加载 DLL
- 会明确区分标准库与第三方依赖，并在缺依赖时告知用户