## 小栗子 QQ 机器人框架 C++ 插件开发提示词模板

这是一个 **小栗子 QQ 机器人框架插件的 C++ SDK 和 Demo 例子**。  
你可以先阅读仓库根目录下的 `README.md`，了解 **SDK 的目录结构、调用方式（`apprun`、各类回调）以及编码约定（UTF-8 与 `\uXXXX` usc2 混合文本）** 和 **Win32(x86) 编译要求**。

- **`Demo_TemplatePlugin`**：最基础的调用模板示例（导出 `apprun` 与基础回调）。  
- **`Demo_关键词回复`**：一个简单的“关键词回复”插件示例，可以参考其处理消息事件、发送群消息等写法。
- **Demo_WebUI**：内嵌浏览器（webui）示例插件，含承载窗口、四类事件、JS 调原生函数与标准卸载顺序。

现在请你根据上述 SDK 结构和示例，**用 C++ 编写一个新的小栗子 QQ 机器人框架插件**，并同时生成完整的 Visual Studio 工程（`.sln` / `.vcxproj` 等），满足以下功能与格式要求。

---

## 插件基本信息（请在实现时填写）

插件名称：  
插件版本：  
插件作者：  
插件介绍：  

> 要求：以上四项请按照小栗子框架 `apprun` 需要的 JSON 结构和 `README.md` 中的说明，在代码中正确返回，并预留为可修改的常量或配置，方便后续调整。

---

## 具体开发要求
！！！请自行填写需求！！！

### 1. 基础要求

- **SDK 使用**
  - 插件工程必须基于本仓库的 SDK（`include/` 与 `src/`）进行开发。
  - 导出并正确实现 `apprun` 以及各类事件回调（例如：`RecvicePrivateMsg`、`RecviceGroupMesg`、`RecviceEventCallBack`、`RotbotAppEnable` 等），参考 `Demo_TemplatePlugin` 与 `Demo_关键词回复` 的写法。

- **编码约定**
  - 插件内部统一使用 **UTF-8** 处理文本。
  - 在调用框架 API 前后，使用 SDK 提供的编码工具（如 `xlz_encoding.h/.cpp`、`xlz_event_utf8.h`）进行 UTF-8 与 usc2/ANSI 混合文本的转换。

- **事件常量（`xlz_constants.h`）**
  - 事件类型 / 子类型与回调返回值**禁止写魔法数字**，统一用 `xlz_constants.h` 里的 `enum class`：
    `xlz::GroupEvent::MemberJoined`、`xlz::FriendEvent::FriendRequest`、`xlz::MessageType::TemporarySession` 等。
  - 回调返回值用 `xlz::MessageHandle::Intercept` / `Ignore`（消息类）、`xlz::EnableResponse::Done` / `Reject`（启用类）。
  - 比较时注意整数转换：`ev->EventType == static_cast<uint32_t>(xlz::GroupEvent::MemberJoined)`。
  - 数值与易语言 SDK 常量表逐条一致（13 组 / 96 个），勿改。详见 `README.md`「事件常量」章节。

---

### 2. 配置读写与文本转义（重点）

1. **读写配置项时，必须做好转义和反转义**：
   - 例如方括号 `[`、`]` 等特殊字符，要在配置文件中替换为 `\u005B`、`\u005D` 这类 `\u` 形式的 usc2 编码。
   - 换行符、制表符以及其他可能破坏配置结构的字符，也需要进行统一转义和还原，**保证配置文件结构不被破坏**，避免导致配置在后续读取/写入时出错。

2. **封装配置工具**
   - 建议封装一组**配置读写工具函数/类**，专门负责：
     - 字符串转义（如 `[` → `\u005B`、`\r\n` → 对应的 `\uXXXX` 序列等）。
     - 字符串反转义（如 `\u005D` → `]` 等）。
     - 读写配置文件（例如按键值对或简单 JSON，任选一种结构，但要稳定、易扩展）。

---

### 3. API 调用与事件处理

- **API 使用**
  - 优先使用 `xlz_api_wrappers.h` 中已封装好的 API（如 `OutputLog`、`SendGroupMessage` 等）来实现插件功能，避免自己重复写调用器。
- **事件文本处理**
  - 在收到消息或事件时，使用 `xlz_event_utf8.h` 中的工具，把事件结构体里的文本安全转换为 UTF-8 后再进行关键词匹配、逻辑判断等。
- **业务逻辑**
  - 根据需要处理私聊、群聊、事件回调等，并调用发送消息、输出日志等接口。
  - 注意返回值/布尔值的语义与易语言逻辑的对应。
  - 记得参考`Demo_关键词回复\plugin.cpp`中`XLZ_API void XLZ_CALL AppDisabled()`函数的插件停用时资源释放处理逻辑，要释放所有在插件使用时创建的所有资源（例如已经创建的窗口，以及还在运行中的线程）

---

### 4. 编译与平台要求

- 必须使用 **Win32(x86)** 平台进行编译（禁止使用 x64），与 `README.md` 中的说明保持一致。
- 工程需适配 **Visual Studio 2019/2022**，并满足“C++ 桌面开发”环境下可直接编译。

---

### 5. 内嵌浏览器（webui，可选）

**仅在需求里出现“显示网页 / 用 HTML+JS 做界面 / 内嵌浏览器”时才做这一节**，否则整节跳过。

- **要加进工程的源文件**：`src/xlz_webui.cpp` 与 `include/xlz_webui.h`（前者必须参与编译，否则链接不到 `xlz::wke`）。
  这一层不链接内核 lib：内核经 `MbKit146.dll` 的朴素名导出用 `GetProcAddress` 动态解析，缺 DLL 时返回 `HostDllNotFound` 而不是崩溃。
- **命名空间**：全局函数在 `xlz::wke::`（`Initialize`、`WaitKernelReady`、`DetachAll`、`SelfDiag`、`JsStringW` …），
  视图类是 `xlz::wke::XlzWebView`（`Bind`/`Destroy`/`LoadHtml`/`RunJs`/`JsBindFunction`/`On*` 事件）。
- **入口顺序（照抄，不要调换）**：
  `MbKitLib::Instance().EnsureLoaded()` → `Initialize()` → `WaitKernelReady(20000)` → 建承载窗口 → `Bind(hwnd)` → 注册事件 / `JsBindFunction` → `Load*`。
- **线程铁律**：`Bind()` 必须在**承载窗口的创建线程**上调用（就是宿主调 `apprun`/回调的 UI 线程）。
  **禁止**放进 `DllMain`，**禁止**丢进 `std::thread`/线程池（否则灰屏 / `NotOnOwnerThread`）；
  跨线程操作 view 一律 `PostMessage` 回 UI 线程。`DllMain` 里也**不许**调 `WaitKernelReady` / `DetachAll` / 建窗。
- **热重载契约（三步，顺序不能换）**：在 `AppUninstall` / `AppDisabled` 里
  ① `xlz::wke::SetUnloading(true)` + 停掉一切会碰窗口的定时器/线程/任务（`KillTimer`、置停止位并 join）；
  ② 逐个 `XlzWebView::Destroy()`；
  ③ 最后 `xlz::wke::DetachAll()`，自建承载窗口再 `DestroyWindow`。
  **窗口销毁后绝不再访问其任何控件**；所有事件回调与 `JsBindFunction` 入口先查 `IsUnloading()`/自己的卸载标志，卸载中直接 `return`。
  > 实测坑：卸载期间定时器回调还在碰已销毁的控件 → 运行时抛「指定窗口或窗口组件不存在或尚未载入」。
  > 见到这条错误先查“定时器停没停 / 回调查没查卸载标志”，别先怀疑 webui。
- **`MbKit146.dll` 的分发**：放到 **宿主 exe 目录**（`<框架目录>\MbKit146.dll`），版本与本 SDK 对齐 **`MbKit/1.4.5`**（`VersionWarning()`/`SelfDiag()` 会提示）；
  更新时按**文件内容逐字节比对**，不能只比大小/时间戳；**全进程只有一份**（Windows 按基名命中，先载入的生效），想换内核只能改名或重启宿主。
  插件工程里**不要**把 `MbKit146.dll` 打进发布包。
- **已知限制（实现方案要绕开）**：内核一进程一份；快速连续热重载约 5 次仍可能崩在 `win32u.dll`（内核侧竞态，插件无法兜住）；
  view 销毁是延迟回收，刚销毁的窗口不要立刻复用；`JsBindFunction` 的函数名只能 ASCII；webui 回调在 UI 线程跑，**别在回调里做耗时操作**。
- **自查**：交付前跑 `python tools\check_webui_exports.py src\xlz_webui.cpp <gen_fwd.def>`，确认用到的内核导出名零拼错；
  编译用 `/p:PlatformToolset=v143`（机器上只有 v143 时），生成 `Win32` + `Release` 的 DLL。

---

## 工程与文件生成要求

除了生成插件的源代码（`.cpp`、`.h` 等）外，**务必同时生成以下工程文件**，以便我直接在 Microsoft Visual Studio 中打开和编译项目：

- **解决方案文件**
  - 生成一个完整可用的 `.sln`。
- **项目文件**
  - 生成对应的 `.vcxproj`（以及必要的 `.vcxproj.filters` 等）。
- **项目配置要求**
  - 平台配置中包含并默认使用 `Win32`（x86），禁止使用 `x64`。
  - 配置至少包含 `Release`，如方便也可包括 `Debug`（同样为 Win32）。
  - 正确包含 SDK 的头文件和源码目录（例如 `include/`、`src/`），并设置好相对路径。
  - 合理配置输出为 DLL，便于直接复制到小栗子框架的插件目录中使用。

---

## 总结

在保留“插件基本信息”填写区块（插件名称 / 版本 / 作者 / 介绍）的前提下，请基于本仓库的 C++ SDK 和 Demo 示例，实现一个符合小栗子 QQ 机器人框架规范的 Win32(x86) 插件，**正确处理文本编码和配置读写转义**，并生成可在 Visual Studio 中直接打开、编译的 `.sln` 和 `.vcxproj` 工程文件。

