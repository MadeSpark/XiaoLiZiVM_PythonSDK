# XiaoLiZiVM_CppSDK（Win32 C++ SDK）

## 目标

本目录提供一个 Win32(x86) 的 C++ SDK 参考实现与调用模板，整体对齐：

- C# 示例 SDK（导出入口、回调形态、Pack=1 结构体、StdCall 调用约定）

## 快速开始（最短路径）

1. 用 Visual Studio 打开解决方案，选择 `Win32` + `Release`。
2. 编译模板项目：`TemplatePlugin`（生成 `XiaoLiZiVM_CppSDK_Template.dll`）。
3. 把 DLL 放入框架的插件目录，框架加载后会调用 `apprun` 获取应用信息 JSON。
4. 在权限界面点“保存设置”，允许你需要的 API（如：输出日志/发送消息）。
5. 观察日志：插件启用后应能看到模板里写入的测试日志（你也可以自行改成你喜欢的格式）。

## 重要说明（强制要求）

### 只支持 Win32(x86)

SDK 的 stdcall 调用器基于 x86 汇编实现，必须使用 32 位编译：

- 平台必须选 `Win32`
- 不能用 `x64`

### 编码（非常关键）

框架/易语言侧常见的文本形态是：ANSI 文本中夹杂 `\\uXXXX`（作者称 usc2）。本 SDK 的约定是：

- 对外（你写插件代码）尽量统一用 UTF-8
- 调用 API 前：UTF-8 -> usc2/ansi 混合
- 取回返回值/事件文本后：usc2/ansi 混合 -> UTF-8

相关实现：

- [xlz_encoding.h](include/xlz_encoding.h)、[xlz_encoding.cpp](src/xlz_encoding.cpp)
- [xlz_event_utf8.h](include/xlz_event_utf8.h)

另外，API 中文名在 C/C++ 源码里直接写 `"中文"` 容易在 CP936 环境触发编译器词法问题，因此这里统一用 UTF-8 `\\x..` 常量：

- [xlz_zh_names_utf8.h](include/xlz_zh_names_utf8.h)

## 目录结构

- `include\\`：SDK 头文件（对外接口、权限枚举、文本编码转换、事件结构体等），共 17 个 `.h`
- `src\\`：SDK 实现（x86 stdcall 通用调用器、apidata 地址解析、编码转换、AppInfo JSON 构造），共 5 个 `.cpp`
- `Demo_TemplatePlugin/`：调用模板（Win32 DLL，导出 `apprun` 与各类回调；工程已含 `src` 全部实现）
- `Demo_WebUI/`：webui（内嵌浏览器）示例插件，见下文「内嵌浏览器（webui，可选）」
- `tools/`：辅助脚本（`build_releases.py` 生成 `Releases/`、`check_webui_exports.py` 校验内核导出名）
- `Releases/`：发布包（由 `tools/build_releases.py` 生成，勿手改）

## 编译要求（必须是 32 位）

- Visual Studio 2019/2022（需安装 C++ 桌面开发组件）
- 需要能使用 Win32(x86) 工具链（cl/link）
- 工具集：`Demo_TemplatePlugin`、`Demo_WebUI` 默认 `v142`（VS2019），`Demo_关键词回复` 是 `v145`；机器上没有对应工具集时用 msbuild `/p:PlatformToolset=<你的工具集>` 覆盖，**不用改工程文件**

说明：`msbuild` 默认不在 PATH 中，直接给全路径即可，例如
`"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"`；
命令行编译示例见「编译 Demo_WebUI」一节。

## 编码约定（重点）

- 框架/易语言侧文本：ANSI 文本中夹杂 `\\uXXXX`（作者称 usc2）
- SDK 对外建议统一使用 UTF-8（调用 API 前做 `utf-8 -> usc2混杂ansi`，回调收到后做 `usc2混杂ansi -> utf-8`）

相关实现：

- `xlz_encoding.h/.cpp`：`Usc2AnsiToUtf8`、`Utf8ToUsc2Ansi`、`ProcessEvents`
- `xlz_event_utf8.h`：从事件结构体读取并转换 UTF-8

## 入口与回调

模板项目导出：

- `apprun`：返回应用信息 JSON（含回调地址、needapilist 权限申请）
- `RecvicePrivateMsg / RecviceGroupMesg / RecviceEventCallBack`：事件回调入口
- `RotbotAppEnable / AppSetting / AppUninstall / AppDisabled`：插件生命周期回调
- `GetSMSVerificationCode / SliderRecognition`：短信/滑块回调（如需）

## API 调用方式

框架会在 `apidata` 中下发 “中文API名 -> 地址”。

SDK 提供两种用法：

1) 通用调用：`ApiResolver + StdCallInvoker + ArgPacker`
2) 少量示例封装：`xlz_api_wrappers.h`（如 输出日志/发送群消息 等）

权限枚举已在 `xlz_permissions.h` 中按英文命名并保留中文备注；权限中文名映射在 `xlz_permission_names.h`。

### 方式 1：通用调用（等价“调用子程序 + 取API函数地址”）

对应易语言逻辑：

- `pluginkey`、`apidata` 在 `apprun` 入参里传入
- 调用某个 API 时，先用 API 中文名从 `apidata` 中取地址，再按 stdcall 约定传参

SDK 内部对应：

- [ApiResolver](include/xlz_api.h)：从 `apidata` 解析地址（已兼容 UTF-8 / GBK / `\\uXXXX` 三种 key 形态）
- [StdCallInvoker](include/xlz_api.h)：x86 stdcall 调用
- [ArgPacker](include/xlz_api.h)：把各种参数打包成 DWORD 序列
- [Sdk](include/xlz_sdk.h)：保存 pluginkey/apidata 并提供 `CallApi...` 封装

### 方式 2：已封装的 API（推荐）

统一封装文件：

- [xlz_api_wrappers.h](include/xlz_api_wrappers.h)

封装规则（与“输出日志”一致）：

- 第一个参数永远是 `pluginkey`（SDK 内部自动填）
- 文本入参默认用 UTF-8（内部会转换为框架需要的文本形态）
- 返回 `std::string` 的接口，会把框架返回的 usc2/ansi 混合文本转换成 UTF-8 再返回
- `bool` 返回值按易语言逻辑（真/假）映射

#### 颜色参数（易语言格式）

输出日志的颜色参数沿用易语言整数颜色：

- `textColor`：默认 0
- `backgroundColor`：默认 `16777215`（白色）

## 数据结构（群列表/群成员列表）

易语言的“群信息/群成员信息/群成员信息V2”等结构体，在这里按 `Pack=1` 定义，并提供指针列表解析工具：

- [xlz_models_corn.h](include/xlz_models_corn.h)

典型用法：

```cpp
#include "xlz_api_wrappers.h"

xlz::GroupListBlock blocks[1]{};
const int count = xlz::GetGroupList(g_sdk, myUin, blocks);
for (int i = 0; i < count && i < blocks[0].Amount; ++i)
{
    const xlz::GroupInfo* info = xlz::GetGroupInfoPtr(blocks[0], i);
    // info->GroupUin / info->GroupName ...
}
```

## 已移植 API 列表（封装函数与参数）

说明：

- `myUin` 对应易语言的“框架QQ”
- 除特别说明外，所有 `const char* xxxUtf8` 均要求传入 UTF-8 字符串
- 以下函数都在 [xlz_api_wrappers.h](include/xlz_api_wrappers.h) 中

| 易语言 API | C++ 封装函数 | 返回值 | 关键参数（按易语言顺序） |
| --- | --- | --- | --- |
| 输出日志 | `OutputLog` | void | `messageUtf8, textColor, backgroundColor` |
| 发送好友消息 | `SendPrivateMessage` | std::string | `myUin, friendUin, messageUtf8, messageRandom*, messageReq*` |
| 发送群消息 | `SendGroupMessage` | std::string | `myUin, groupUin, messageUtf8, anonymous` |
| 取框架QQ | `GetFrameworkQQ` | std::string | （无） |
| 取群列表 | `GetGroupList` | int32_t | `myUin, blocks*` |
| 取群成员列表 | `GetGroupMemberList` | int32_t | `myUin, groupUin, blocks*` |
| 发送群临时消息 | `SendGroupTemporaryMessage` | std::string | `myUin, groupUin, otherUin, contentUtf8, outRandom*, outReq*` |
| 发送群json消息 | `SendGroupJsonMessage` | std::string | `myUin, groupUin, jsonUtf8, anonymous` |
| 上传好友图片 | `UploadFriendImage` | std::string | `myUin, friendUin, isFlash, picBytes, picSize, wide, high, cartoon, previewTextUtf8` |
| 上传群图片 | `UploadGroupImage` | std::string | `myUin, groupUin, isFlash, picBytes, picSize, wide, high, cartoon, previewTextUtf8` |
| 上传好友语音 | `UploadFriendAudio` | std::string | `myUin, friendUin, audioType, audioTextUtf8, audioBytes, audioSize, timeSeconds` |
| 上传群语音 | `UploadGroupAudio` | std::string | `myUin, groupUin, audioType, audioTextUtf8, audioBytes, audioSize, timeSeconds` |
| 上传群文件 | `UploadGroupFile` | std::string | `myUin, groupUin, filePathUtf8, folderUtf8` |
| 取管理层列表 | `GetAdministratorList` | std::string | `myUin, groupUin` |
| 取群名片 | `GetGroupCard` | std::string | `myUin, groupUin, memberUin` |
| 设置群名片 | `SetGroupCard` | std::string | `myUin, groupUin, memberUin, newCardUtf8` |
| 取昵称_从缓存 | `GetNicknameFromCache` | std::string | `otherUinTextUtf8` |
| 强制取昵称 | `GetNicknameForce` | std::string | `myUin, otherUinTextUtf8` |
| 取好友文件下载地址 | `GetFriendFileDownloadUrl` | std::string | `myUin, fileIdUtf8, fileNameUtf8` |
| 取图片下载地址 | `GetImageDownloadUrl` | std::string | `imageCodeUtf8, myUin, groupUin` |
| 撤回消息_群聊 | `RecallGroupMessage` | bool | `myUin, groupUin, messageRandom, messageReq` |
| 禁言群成员 | `MuteGroupMember` | bool | `myUin, groupUin, memberUin, durationSeconds` |
| 删除群成员 | `RemoveGroupMember` | bool | `myUin, groupUin, memberUin, refuseNextJoin` |
| 处理好友验证事件 | `HandleFriendVerificationEvent` | void | `myUin, triggerUin, messageSeq, operateType` |
| 处理群验证事件 | `HandleGroupVerificationEvent` | void | `myUin, sourceGroupUin, triggerUin, messageSeq, operateType, eventType, refuseReasonUtf8` |
| 全员禁言 | `MuteAll` | bool | `myUin, groupUin, enabled` |
| QQ点赞 | `QQLike` | std::string | `myUin, otherUin` |
| 群聊打卡 | `GroupCheckIn` | std::string | `myUin, groupUin` |
| 分享音乐 | `ShareMusic` | bool | `myUin, target, musicNameUtf8, artistNameUtf8, jumpUrlUtf8, coverUrlUtf8, fileUrlUtf8, appType, shareType` |
| 提取图片文字 | `ExtractTextFromImage` | bool | `myUin, imageUrlUtf8, outTextUtf8` |
| 调用指定OneBot接口 | `CallOneBotInterface` | std::string | `myUin, sendDataUtf8, noWait` |
| 取群成员信息 | `GetGroupMemberInfo` | std::string | `myUin, groupUin, otherUin, outData*` |
| 取插件数据目录 | `GetPluginDataDirectory` | std::string | （无） |
| 重载自身 | `ReloadItSelf` | void | `newDllPathUtf8`（空/NULL 等价 `"empty"`） |
| 取插件自身版本号 | `GetPluginSelfVersion` | std::string | （无） |
| 取框架主窗口句柄 | `GetFrameworkMainWindowHandle` | uint32_t | （无） |
| 取QQ头像 | `GetQQAvatar` | std::string | `otherUin, hdOriginal` |
| 取插件文件名 | `GetPluginFileName` | std::string | （无） |
| 取框架版本 | `GetFrameworkVersion` | std::string | （无） |

## 字段命名规范（重要）

本 SDK 的公开字段/参数统一遵循 [`../SDK字段命名规范.md`](../../SDK字段命名规范.md)：

- C++ 结构体成员 / 参数用 `PascalCase`；Python 用同词根的 `snake_case`。
- QQ 号统一 `*Uin`：**框架QQ = `MyUin`**、发送人QQ = `SenderUin`、群号 = `GroupUin`。
- 标识类用 `Id` 结尾（`BubbleId` / `MessageClipId` / `PendantId` / `FontId`）。
- **群名片 = `Card`**（旧名 `SenderNickname` 已废弃）；`Nickname` 仅表示原始昵称。
- 历史错拼已修正：`AnonymousFalg` → `AnonymousFlag`、`LastSpeackTime` → `LastSpeakTime`。

## 事件常量（`xlz_constants.h`）

易语言 SDK 常量表里的事件 / 状态常量已迁移到 `include/xlz_constants.h`，**数值与框架内部定义逐条一致**（源：`cornsdk6.0.8.e` 常量表）。全部为 `enum class : int32_t`，按易语言常量前缀分成 13 组：

| 分组（C++） | 对应易语言前缀 | 条数 | 用途 |
|---|---|---|---|
| `xlz::MessageHandle` | `消息处理_` | 2 | 消息回调返回值：`Intercept`(1) 拦截 / `Ignore`(0) 放行 |
| `xlz::MessageType` | `消息类型_` | 8 | 私聊/临时会话的消息类型（`TemporarySession`=141 等） |
| `xlz::ChannelPushType` | `频道推送类型_` | 3 | `SubChannelMessage` / `DirectMessage` / `EventMessage` |
| `xlz::GroupEvent` | `群事件_` | 41 | 群事件子类型（进群、退群、禁言、群名片变更…） |
| `xlz::ZoneEvent` | `空间事件_` | 1 | `RelatedToMe`(29) |
| `xlz::FrameworkEvent` | `框架事件_` | 3 | `LoginSuccess`/`LoginFailed`/`RestartingForUpdate` |
| `xlz::DiscussionGroupEvent` | `讨论组事件_` | 5 | 讨论组成员/名称变更（300–304） |
| `xlz::FriendEvent` | `好友事件_` | 20 | 好友请求、签名变更、点赞、输入状态…（100–119） |
| `xlz::LoginEvent` | `登录事件_` | 5 | 电脑/移动端上下线、其他应用登录验证（200–204） |
| `xlz::VirtualMachineEvent` | `虚拟机事件_` | 1 | `OneBotRawRequest`(205) |
| `xlz::SubChannelEvent` | `子频道事件_` | 4 | 子频道系统提示、贴表情、撤回、直播状态（0–3） |
| `xlz::ChannelEvent` | `频道事件_` | 1 | `UserApplyJoinChannel`(4) |
| `xlz::EnableResponse` | `启用响应_` | 2 | 插件启用回调返回值：`Reject`(8) / `Done`(0) |

> 说明：易语言常量表里「消息处理_拦截 / 启用响应_拒绝」等原本是**文本常量**（`"1"` / `"8"`），
> 这里按其作为**回调返回值的实际语义**暴露为整数枚举，可直接 `return (int)xlz::MessageHandle::Intercept;`。

用法示例：

```cpp
#include "xlz_events.h"
#include "xlz_constants.h"

XLZ_API int XLZ_CALL RecviceGroupMesg(void* data)
{
    const auto* ev = reinterpret_cast<const xlz::GroupMessageEvent*>(data);
    if (ev->MessageSubType == (uint32_t)xlz::GroupEvent::MemberJoined)
    {
        // 有人入群
    }
    if (ev->MessageType == (uint32_t)xlz::MessageType::TemporarySession)
    {
        // 临时会话消息
    }
    return (int)xlz::MessageHandle::Ignore; // 等价于 return 0
}
```

事件类型 / 子类型字段与枚举的对应关系：

- `GroupMessageEvent` / `PrivateMessageEvent` 的 `MessageType`、`MessageSubType` → `xlz::MessageType`、`xlz::GroupEvent`
- `EventTypeBase` 的 `EventType`、`EventSubType` → `xlz::GroupEvent` / `xlz::FriendEvent` / `xlz::LoginEvent` …
- 回调返回值 → `xlz::MessageHandle`（消息类）、`xlz::EnableResponse`（启用类）

Python 端对应模块为 `xlz_constants.py`，成员名由同名 `PascalCase` 自动转 `UPPER_SNAKE_CASE`
（`GroupEvent::MemberJoined` → `GroupEvent.MEMBER_JOINED`）。

## 内嵌浏览器（webui，可选）

需要「在插件里显示网页 / 用 HTML+JS 做界面」时才用。相关文件：

- [xlz_webui.h](include/xlz_webui.h) / [xlz_webui.cpp](src/xlz_webui.cpp)：把易语言「MB浏览框」的核心命令移植为
  `xlz::wke::XlzWebView` + 一组全局函数
- [Demo_WebUI/](Demo_WebUI/)：可编译运行的示例插件（建窗 → Bind → 载页面 → 三步卸载）
- [tools/check_webui_exports.py](tools/check_webui_exports.py)：静态校验「代码里用到的内核导出名」全在
  `gen_fwd.def` 里（零拼错容忍）

这一层**不链接任何内核 lib**：内核一律经 `MbKit146.dll` 的朴素名 stdcall 导出
（MinGW `-Wl,--kill-at`，无 `@N` 修饰）用 `GetProcAddress` 动态解析；`MbKit146.dll` 缺失时
返回 `WebUiError::HostDllNotFound` 而**不是**加载失败 —— 所以工程里带上 `xlz_webui.cpp`
也不会让不用 webui 的插件多出运行期依赖（只是 DLL 大一点）。

### 最小可用例子

```cpp
#include "xlz_webui.h"

static xlz::wke::XlzWebView g_webView;
static std::atomic<bool> g_uninstalling{ false };

static void OnDocReady(const xlz::wke::XlzWebView::DocumentReady2Event& ev)
{
    if (g_uninstalling.load()) return;          // ★ 卸载期间直接返回
    (void)ev;
}

static int64_t NativeFn(int64_t es)
{
    if (g_uninstalling.load()) return 0;
    const std::string s = xlz::wke::JsArgToStringUtf8(es, 0);   // 取第 0 个参数
    return xlz::wke::JsStringW(("来自原生：" + s).c_str());       // 回一个 jsValue
}

// 在插件启用回调（宿主 UI 线程）里：
void StartWebUi()
{
    using namespace xlz::wke;

    MbKitLib::Instance().EnsureLoaded();          // ① 载入 MbKit146.dll（幂等）
    Initialize();                                 // ② 登记内核初始化（异步，不阻塞）
    WaitKernelReady(20000);                       // ③ 等内核就绪（★ 不许在 DllMain 里调）

    HWND hwnd = /* 承载窗口：框架主窗口或自己 CreateWindowExW 出来的窗口 */;
    g_webView.Bind(hwnd, false);                  // ④ 建 view 并绑定窗口（★ 必须在窗口创建线程）

    g_webView.OnDocumentReady2(OnDocReady);
    g_webView.JsBindFunction("nativeFn", NativeFn, 1);   // 名字只能 ASCII
    g_webView.LoadHtml("<button onclick=\"nativeFn('hi')\">调原生</button>");
}
```

`Demo_WebUI/plugin.cpp` 里是一份更完整的例子（四类事件回调 + 承载窗口 + 标准卸载顺序）。

### 方法一览

全局函数（`xlz::wke::`）：

| 函数 | 说明 |
|---|---|
| `MbKitLib::Instance().EnsureLoaded()` | 载入/复用 `MbKit146.dll`；失败给出 `LastLoadError()` |
| `Initialize(bool threadRepaint = false)` | 对应 `WKE初始化`；**只登记不等待**（内部会调 `MbKit_EnsureKernelInit`） |
| `WaitKernelReady(int32_t timeoutMs = 0)` | 对应 `WKE等待内核就绪`；`0` = 用内核默认超时。**禁止在 DllMain 里调** |
| `GetKernelBootState()` / `GetKernelMode()` | 内核启动状态（0..3）/ 内核模式（node.dll、edgewebview） |
| `SelfDiag()` / `GetWebView2Diag()` / `IsWebView2Supported()` | 排障第一入口：一行式自检 / WebView2 诊断 / 环境探测 |
| `SetNodePath(path, &why, forceNodeKernel=false)` | 对应 `WKE设置node路径`，必须在内核启动前调 |
| `DetachAll()` | 对应 `WKE全解绑`：解绑+销毁本进程全部 view，返回清理数量。**禁止在 DllMain 里调** |
| `SetUnloading(bool)` / `IsUnloading()` | 卸载标志；所有 webui 回调进来先看它 |

`XlzWebView`（一个实例 = 一个 view，可重复 `Bind`）：

| 方法 | 说明 |
|---|---|
| `Bind(void* hwnd, bool transparent = false)` | 对应 `绑定`：`wkeCreateWebView` + `wkeSetHandle`；`hwnd=nullptr` 为无窗口（仅初始化） |
| `Destroy()` | 对应 `销毁`：先撤回调 thunk 再销毁 view；销毁后**绝不会**再进用户回调 |
| `LoadUrl` / `LoadFile` / `LoadHtml` / `LoadHtmlWithBaseUrl` | 四种载入方式（入参 UTF-8，内部走 `*W`） |
| `RunJs(js)` / `RunJsByFrame(frame, js)` | 运行 JS，返回 `jsValue`（0 = 失败/未绑定） |
| `JsBindFunction(name, fn, argc)` | 把原生函数暴露给网页；`fn` 的存活期由本对象持有 |
| `OnDocumentReady2` / `OnConsole` / `OnLoadingFinish` / `OnTitleChanged` | 四类事件；传 `nullptr`/空函数即取消 |
| `JsArgCount` / `JsArgToStringUtf8` / `JsArgToInt` / `JsStringW` / `JsToStringUtf8` | JS 取参/回值薄封装（在 `JsBindFunction` 回调里用） |
| `Reload` / `StopLoading` / `GoBack` / `GoForward` | 导航控制 |
| `GetUrl` / `GetTitle` / `GetSource` / `IsDocumentReady` / `GetHostWindow` | 读页面信息 |
| `Resize(w, h)` / `IsValid()` | 改大小 / 判断 view 是否仍有效 |

失败一律返回 `WebUiError`（`Ok`、`NoKernelDll`、`MissingSymbol`、`BadParam`、`BadState`、
`HostDllNotFound`、`HostDllLoadFailed`、`ViewNotBound`、`NotOnOwnerThread` …），**不会空指针调内核**。

### 三条必须守住的规则

1. **线程铁律**：`Bind()` 必须在**承载窗口的创建线程**上调用（通常是宿主 UI 线程，也就是
   `apprun` / 各回调所在的线程）—— 在回调里直接调是对的。**绝对不要**丢进 `std::thread` /
   线程池 / 自己起的消息线程：那是灰屏（白屏）的经典原因。跨线程想动已建好的 view，
   用 `PostMessage` 投递回宿主 UI 线程执行。`Bind()` 会比对窗口创建线程与当前线程，
   不一致返回 `WebUiError::NotOnOwnerThread`。
2. **入口顺序**：`EnsureLoaded()` → `Initialize()` → `WaitKernelReady()` → 建承载窗口 →
   `Bind(hwnd)` → 注册事件 / `JsBindFunction` → `Load*`。要立刻执行 JS 必须等
   `OnDocumentReady2` 回调，之前 DOM 还没生成（此时可用 `RunJsByFrame`）。
   `Bind()` 内部会幂等地再调一次 `Initialize()`，所以省略 ②③ 也能跑，但**在 UI 线程里等一次
   `WaitKernelReady` 体验更好**（首帧白屏时间短）。
3. **热重载契约**：插件卸载回调（`AppUninstall` / `AppDisabled`）里的顺序不能换 ——
   ① 先立卸载标志 `SetUnloading(true)`，并**停掉一切会碰窗口的定时器 / 线程 / 任务**
   （`KillTimer`、置停止位并 `join`）；
   ② 再逐个 `XlzWebView::Destroy()`（撤回调 → 销毁 view）；
   ③ 最后 `DetachAll()`（内核全解绑，内核保持已初始化），如有自建窗口再 `DestroyWindow`；
   **窗口销毁后绝不再访问其任何控件**。
   所有 webui 回调（`On*`、`JsBindFunction`）入口先查卸载标志，是卸载中**立刻返回**。
   > ★ 实测坑（真机复现）：卸载期间若定时器回调还在碰已销毁的窗口控件，运行时会报
   > 「指定窗口或窗口组件不存在或尚未载入」—— 这条错误十有八九不是 webui 的锅，而是
   > 「窗口已销毁但还有代码在访问它」。排查顺序：定时器有没有停 → 回调有没有先查卸载标志。

```cpp
void StopWebUi()   // 插件卸载回调里调
{
    xlz::wke::SetUnloading(true);   // ① 立标志：回调自此直接返回
    g_uninstalling.store(true);
    // ① 停定时器/线程：KillTimer(hwnd, kTimerId); worker.join();

    g_webView.Destroy();            // ② 销毁自己创建的 view
    if (g_hostWnd != nullptr)
    {
        ::DestroyWindow(g_hostWnd); // ③ 自建承载窗口一并销毁
        g_hostWnd = nullptr;
    }
    xlz::wke::DetachAll();          // ③ 内核全解绑

    xlz::wke::SetUnloading(false);  // 解除标志（下次启用还能用）
    g_uninstalling.store(false);
}
```

### `MbKit146.dll` 的放置与版本

- **放哪**：`<宿主 exe 目录>\MbKit146.dll`（框架/易语言模块用同一份）。SDK 的查找顺序是
  `SetKernelBlob()` 注入的内存映像 → `SetKernelSourceDir()` 指定的旁路目录 →
  **宿主 exe 目录** → `%TEMP%\ElangMbKit\MbKit146.dll` → 裸名（系统搜索序，含「已被进程载入」的命中）。
  找不到不崩：`EnsureLoaded()` 返回 `HostDllNotFound`，`LastLoadError()` 里列出试过的路径。
- **更新时按内容比对**：换内核**不能只比文件大小或时间戳**（历史事故：同尺寸的旧 DLL 覆盖了新内核，
  导致 WebView2 永不初始化）。正确做法是逐字节比对；文件被占用写不进去时，用已载入的那份并记告警。
- **版本钉住**：本 SDK 对齐 **`MbKit/1.4.5`**。载入后读 `MbKit_Version()` 校验，
  版本串不含 `MbKit/1.4.5` 时 `MbKitLib::VersionWarning()` 给出提示（`SelfDiag()` 会带在头部）——
  这通常意味着**进程里已经有一份别的内核**先载入了。
- **全进程只有一份**：Windows 按**基名**命中已载入模块、且**先载入的那份生效**，路径不参与判定。
  所以多个插件各带一份 `MbKit146.dll` 时只有第一份有效；想升级内核只能**改名（如 MbKit147.dll）
  或重启宿主 exe**。`MbKit_Version()` 在**内核启动之前返回空串**，别在 `apprun` 里断言版本。
- 内核是 **32 位（Win32）**，必须与框架 / 插件同为 32 位环境。`MbKit146.dll` 不随本 SDK 的
  `Releases/` 一起分发（成品区只放源码与工程），请从内核发布处单独获取。

### 已知限制（如实记录）

- 内核**一个进程只有一份**，换内核只能重启宿主 exe。
- **快速连续热重载插件（约 5 次）** 仍可能让宿主崩在 `win32u.dll+0x11cc` —— 属于内核侧
  WebView2 异步释放的竞态（`WM_DESTROY` 与 `Release` 竞争），插件侧无法兜住，目前只能
  「别连着重载」。
- view 销毁是**延迟回收**（约数百毫秒），刚 `Destroy()` 的窗口不要立刻复用。
- 只有**窗口模式**（`Bind(hwnd)`）经过验证；`hwnd=nullptr` 的无窗口模式仅能初始化内核，不保证渲染。
- `JsBindFunction` 的函数名只能是 ASCII 字母 / 数字 / 下划线；webui 回调在宿主 UI 线程上跑，
  **别在回调里做耗时操作**（要做得自己投递到工作线程）。

### 编译 Demo_WebUI

工程默认工具集是 `v142`（VS2019）。没装 VS2019 时用 `/p:PlatformToolset=v145` 覆盖即可
（用 VS2022 的话换成 `v143`），不必改工程文件：

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" ^
  Demo_WebUI\WebUIDemo.vcxproj /p:Configuration=Release /p:Platform=Win32 ^
  /p:PlatformToolset=v145
```

产物：`Demo_WebUI\bin\Release\XiaoLiZiVM_CppSDK_WebUI.dll`（编译产物，不提交；`bin\obj\` 已忽略）。

**已验证**：`/p:PlatformToolset=v145`（VS 18 / MSVC 14.44）下编译链接通过，仅 2 处
`C4244`（`int64_t`→`int32_t` 参数截断）警告（日志里因汇总会重复打印成 4 行），无错误。**未验证**：运行时渲染 ——
需要 `MbKit146.dll`，且该内核不随本 SDK 分发。

### 已知状态（如实记录）

- 工具集不统一：`Demo_TemplatePlugin`、`Demo_WebUI` 钉 `v142`（VS2019），`Demo_关键词回复` 的
  `KeywordReplyPlugin.vcxproj` 钉 `v145`。本机没有 VS2019 时前者会报 `MSB8020`，必须显式覆盖工具集
  （`/p:PlatformToolset=v145`）；后者可直接编译。
- 本机工作区的 `Demo_TemplatePlugin\bin\Release\XiaoLiZiVM_CppSDK_Template.dll` 是一份 **2026-03-03 的旧构建**
  （`bin/` 被 `.gitignore` 忽略、**未入库**，新克隆的仓库里没有这个文件）。它早于本次新增的 `src\xlz_webui.cpp`；
  `TemplatePlugin.vcxproj` 已把该源码加入编译项，
  因此**工程文件与这份预编译 DLL 不对应**。因为模板 `plugin.cpp` 并不调用 WebUI，
  运行行为不受影响；需要与工程一致的成品请自行重编（`/p:PlatformToolset=v145` 已验证可编译通过）。
- `Demo_关键词回复\KeywordReplyPlugin.vcxproj` **没有**加入 `xlz_webui.cpp`（它是更早的 demo，与
  `Demo_TemplatePlugin`、`Demo_WebUI` 现在不一致），这个 demo 本身也不调用 WebUI。
- `Demo_WebUI` 不提供预编译 DLL（`MbKit146.dll` 不随 SDK 分发，带成品也无从运行）。

自检（离线，不启动宿主）：

```bat
:: gen_fwd.def 是 MbKit146.dll 的权威导出清单，由发布方提供，不在本仓库内。
:: 用参数传入，或设环境变量 MBKIT_DEF 指向它。
python tools\check_webui_exports.py src\xlz_webui.cpp D:\path\to\gen_fwd.def
```
