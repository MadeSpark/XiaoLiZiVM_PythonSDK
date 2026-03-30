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

- `include\\`：SDK 头文件（对外接口、权限枚举、文本编码转换、事件结构体等）
- `src\\`：SDK 实现（x86 stdcall 通用调用器、apidata 地址解析、编码转换、AppInfo JSON 构造）
- `TemplatePlugin\\`：调用模板（Win32 DLL，导出 `apprun` 与各类回调）

## 编译要求（必须是 32 位）

- Visual Studio 2019/2022（需安装 C++ 桌面开发组件）
- 需要能使用 Win32(x86) 工具链（cl/link）

说明：当前环境中未检测到 `msbuild`，如果你希望命令行编译，请安装 **Visual Studio Build Tools** 并确保 `msbuild` 在 PATH 中。

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
const int count = xlz::GetGroupList(g_sdk, thisQq, blocks);
for (int i = 0; i < count && i < blocks[0].Amount; ++i)
{
    const xlz::GroupInfo* info = xlz::GetGroupInfoPtr(blocks[0], i);
    // info->GroupQQ / info->GroupName ...
}
```

## 已移植 API 列表（封装函数与参数）

说明：

- `thisQq` 对应易语言的“框架QQ”
- 除特别说明外，所有 `const char* xxxUtf8` 均要求传入 UTF-8 字符串
- 以下函数都在 [xlz_api_wrappers.h](include/xlz_api_wrappers.h) 中

| 易语言 API | C++ 封装函数 | 返回值 | 关键参数（按易语言顺序） |
| --- | --- | --- | --- |
| 输出日志 | `OutputLog` | void | `messageUtf8, textColor, backgroundColor` |
| 发送好友消息 | `SendPrivateMessage` | std::string | `thisQq, friendQq, messageUtf8, messageRandom*, messageReq*` |
| 发送群消息 | `SendGroupMessage` | std::string | `thisQq, groupQq, messageUtf8, anonymous` |
| 取框架QQ | `GetFrameworkQQ` | std::string | （无） |
| 取群列表 | `GetGroupList` | int32_t | `thisQq, blocks*` |
| 取群成员列表 | `GetGroupMemberList` | int32_t | `thisQq, groupQq, blocks*` |
| 发送群临时消息 | `SendGroupTemporaryMessage` | std::string | `thisQq, groupId, otherQq, contentUtf8, outRandom*, outReq*` |
| 发送群json消息 | `SendGroupJsonMessage` | std::string | `thisQq, groupQq, jsonUtf8, anonymous` |
| 上传好友图片 | `UploadFriendImage` | std::string | `thisQq, friendQq, isFlash, picBytes, picSize, wide, high, cartoon, previewTextUtf8` |
| 上传群图片 | `UploadGroupImage` | std::string | `thisQq, groupQq, isFlash, picBytes, picSize, wide, high, cartoon, previewTextUtf8` |
| 上传好友语音 | `UploadFriendAudio` | std::string | `thisQq, friendQq, audioType, audioTextUtf8, audioBytes, audioSize, timeSeconds` |
| 上传群语音 | `UploadGroupAudio` | std::string | `thisQq, groupQq, audioType, audioTextUtf8, audioBytes, audioSize, timeSeconds` |
| 上传群文件 | `UploadGroupFile` | std::string | `thisQq, groupQq, filePathUtf8, folderUtf8` |
| 取管理层列表 | `GetAdministratorList` | std::string | `thisQq, groupQq` |
| 取群名片 | `GetGroupCard` | std::string | `thisQq, groupQq, memberQq` |
| 设置群名片 | `SetGroupCard` | std::string | `thisQq, groupQq, memberQq, newCardUtf8` |
| 取昵称_从缓存 | `GetNicknameFromCache` | std::string | `otherQqTextUtf8` |
| 强制取昵称 | `GetNicknameForce` | std::string | `thisQq, otherQqTextUtf8` |
| 取好友文件下载地址 | `GetFriendFileDownloadUrl` | std::string | `thisQq, fileIdUtf8, fileNameUtf8` |
| 取图片下载地址 | `GetImageDownloadUrl` | std::string | `imageCodeUtf8, thisQq, groupQq` |
| 撤回消息_群聊 | `RecallGroupMessage` | bool | `thisQq, groupQq, messageRandom, messageReq` |
| 禁言群成员 | `MuteGroupMember` | bool | `thisQq, groupQq, memberQq, durationSeconds` |
| 删除群成员 | `RemoveGroupMember` | bool | `thisQq, groupQq, memberQq, refuseNextJoin` |
| 处理好友验证事件 | `HandleFriendVerificationEvent` | void | `thisQq, triggerQq, messageSeq, operateType` |
| 处理群验证事件 | `HandleGroupVerificationEvent` | void | `thisQq, sourceGroupQq, triggerQq, messageSeq, operateType, eventType, refuseReasonUtf8` |
| 全员禁言 | `MuteAll` | bool | `thisQq, groupQq, enabled` |
| QQ点赞 | `QQLike` | std::string | `thisQq, otherQq` |
| 群聊打卡 | `GroupCheckIn` | std::string | `thisQq, groupQq` |
| 分享音乐 | `ShareMusic` | bool | `thisQq, target, musicNameUtf8, artistNameUtf8, jumpUrlUtf8, coverUrlUtf8, fileUrlUtf8, appType, shareType` |
| 提取图片文字 | `ExtractTextFromImage` | bool | `thisQq, imageUrlUtf8, outTextUtf8` |
| 调用指定OneBot接口 | `CallOneBotInterface` | std::string | `thisQq, sendDataUtf8, noWait` |
| 取群成员信息 | `GetGroupMemberInfo` | std::string | `thisQq, groupQq, otherQq, outData*` |
| 取插件数据目录 | `GetPluginDataDirectory` | std::string | （无） |
| 重载自身 | `ReloadItSelf` | void | `newDllPathUtf8`（空/NULL 等价 `"empty"`） |
| 取插件自身版本号 | `GetPluginSelfVersion` | std::string | （无） |
| 取框架主窗口句柄 | `GetFrameworkMainWindowHandle` | uint32_t | （无） |
| 取QQ头像 | `GetQQAvatar` | std::string | `otherQq, hdOriginal` |
| 取插件文件名 | `GetPluginFileName` | std::string | （无） |
| 取框架版本 | `GetFrameworkVersion` | std::string | （无） |


