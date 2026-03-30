#pragma once

#include <cstdint>
#include <string>

#include "xlz_api.h"
#include "xlz_encoding.h"
#include "xlz_models_corn.h"
#include "xlz_sdk.h"
#include "xlz_strings.h"
#include "xlz_text_normalize.h"
#include "xlz_zh_names_utf8.h"

namespace xlz
{
	// 输出日志, 逻辑型, 公开
	inline void OutputLog(const Sdk& sdk, const char* messageUtf8, int textColor = 0, int backgroundColor = 16777215)
	{
		TempUsc2AnsiString msg(messageUtf8);
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushPtr(msg.CStr());
		a.PushU32(static_cast<uint32_t>(textColor));
		a.PushU32(static_cast<uint32_t>(backgroundColor));
		sdk.CallApiVoid(kApiName_OutputLog_Utf8, a.Data());
	}

	// 发送好友消息, 文本型, 公开, 成功返回的time用于撤回消息
	inline std::string SendPrivateMessage(const Sdk& sdk, int64_t thisQq, int64_t friendQq, const char* messageUtf8, int64_t* messageRandom, uint32_t* messageReq)
	{
		const std::string normalized = NormalizeNewlinesToLf(messageUtf8);
		TempUsc2AnsiString msg(normalized.c_str());
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(friendQq));
		a.PushPtr(msg.CStr());
		a.PushPtr(messageRandom);
		a.PushPtr(messageReq);
		return sdk.CallApiReturnUtf8(kApiName_SendFriendMessage_Utf8, a.Data());
	}

	// 发送群消息, 文本型, 公开
	inline std::string SendGroupMessage(const Sdk& sdk, int64_t thisQq, int64_t groupQq, const char* messageUtf8, bool anonymous)
	{
		const std::string normalized = NormalizeNewlinesToLf(messageUtf8);
		TempUsc2AnsiString msg(normalized.c_str());
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushPtr(msg.CStr());
		a.PushU32(anonymous ? 1u : 0u);
		return sdk.CallApiReturnUtf8(kApiName_SendGroupMessage_Utf8, a.Data());
	}

	// 取框架QQ, 文本型, 公开, 返回的数据是json格式,自行进行解析
	inline std::string GetFrameworkQQ(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnUtf8(kApiName_GetFrameworkQQ_Utf8, a.Data());
	}

	// 取群列表, 整数型, 公开, 失败或无权限返回数量0
	inline int32_t GetGroupList(const Sdk& sdk, int64_t thisQq, GroupListBlock* blocks)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(blocks);
		return static_cast<int32_t>(sdk.CallApiReturnU32(kApiName_GetGroupList_Utf8, a.Data()));
	}

	// 取群成员列表, 整数型, 公开, 失败或无权限返回数量0
	inline int32_t GetGroupMemberList(const Sdk& sdk, int64_t thisQq, int64_t groupQq, GroupMemberListBlock* blocks)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushPtr(blocks);
		return static_cast<int32_t>(sdk.CallApiReturnU32(kApiName_GetGroupMemberList_Utf8, a.Data()));
	}

	// 发送群临时消息, 文本型, 公开, 成功返回的time用于撤回消息
	inline std::string SendGroupTemporaryMessage(const Sdk& sdk, int64_t thisQq, int64_t groupId, int64_t otherQq, const char* contentUtf8, int64_t* outRandom, int32_t* outReq)
	{
		int64_t randomTmp = 0;
		int32_t reqTmp = 0;
		if (outRandom == nullptr)
		{
			outRandom = &randomTmp;
		}
		if (outReq == nullptr)
		{
			outReq = &reqTmp;
		}

		const std::string normalized = NormalizeNewlinesToLf(contentUtf8);
		TempUsc2AnsiString content(normalized.c_str());
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupId));
		a.PushU64(static_cast<uint64_t>(otherQq));
		a.PushPtr(content.CStr());
		a.PushPtr(outRandom);
		a.PushPtr(outReq);
		return sdk.CallApiReturnUtf8(kApiName_SendGroupTemporaryMessage_Utf8, a.Data());
	}

	// 发送群json消息, 文本型, 公开
	inline std::string SendGroupJsonMessage(const Sdk& sdk, int64_t thisQq, int64_t groupQq, const char* jsonUtf8, bool anonymous)
	{
		const std::string normalized = NormalizeNewlinesToLf(jsonUtf8);
		TempUsc2AnsiString json(normalized.c_str());
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushPtr(json.CStr());
		a.PushU32(anonymous ? 1u : 0u);
		return sdk.CallApiReturnUtf8(kApiName_SendGroupJsonMessage_Utf8, a.Data());
	}

	// 上传好友图片, 文本型, 公开, 成功返回图片代码
	inline std::string UploadFriendImage(const Sdk& sdk, int64_t thisQq, int64_t friendQq, bool isFlash, const void* picBytes, uint32_t picSize, int32_t wide = 0, int32_t high = 0, bool cartoon = false, const char* previewTextUtf8 = "")
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(friendQq));
		a.PushU32(isFlash ? 1u : 0u);
		a.PushPtr(picBytes);
		a.PushU32(picSize);
		std::string ret = sdk.CallApiReturnUtf8(kApiName_UploadFriendImage_Utf8, a.Data());

		if (!ret.empty() && ret.back() == ']')
		{
			ret.pop_back();
			ret.append(",wide=").append(std::to_string(wide));
			ret.append(",high=").append(std::to_string(high));
			ret.append(",cartoon=").append(cartoon ? "true" : "false");
			ret.append(",str=").append(previewTextUtf8 ? previewTextUtf8 : "");
			ret.push_back(']');
		}
		return ret;
	}

	// 上传群图片, 文本型, 公开, 成功返回图片代码(支持讨论组)
	inline std::string UploadGroupImage(const Sdk& sdk, int64_t thisQq, int64_t groupQq, bool isFlash, const void* picBytes, uint32_t picSize, int32_t wide = 0, int32_t high = 0, bool cartoon = false, const char* previewTextUtf8 = "")
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU32(isFlash ? 1u : 0u);
		a.PushPtr(picBytes);
		a.PushU32(picSize);
		std::string ret = sdk.CallApiReturnUtf8(kApiName_UploadGroupImage_Utf8, a.Data());

		if (!ret.empty() && ret.back() == ']')
		{
			ret.pop_back();
			ret.append(",wide=").append(std::to_string(wide));
			ret.append(",high=").append(std::to_string(high));
			ret.append(",cartoon=").append(cartoon ? "true" : "false");
			ret.append(",str=").append(previewTextUtf8 ? previewTextUtf8 : "");
			ret.push_back(']');
		}
		return ret;
	}

	// 上传好友语音, 文本型, 公开, 成功返回语音代码
	inline std::string UploadFriendAudio(const Sdk& sdk, int64_t thisQq, int64_t friendQq, int32_t audioType, const char* audioTextUtf8, const void* audioBytes, uint32_t audioSize, int32_t timeSeconds = 0)
	{
		TempUsc2AnsiString audioText(audioTextUtf8 ? audioTextUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(friendQq));
		a.PushU32(static_cast<uint32_t>(audioType));
		a.PushPtr(audioText.CStr());
		a.PushPtr(audioBytes);
		a.PushU32(audioSize);
		std::string ret = sdk.CallApiReturnUtf8(kApiName_UploadFriendAudio_Utf8, a.Data());

		if (!ret.empty() && ret.back() == ']')
		{
			ret.pop_back();
			ret.append(",time=").append(std::to_string(timeSeconds));
			ret.push_back(']');
		}
		return ret;
	}

	// 上传群语音, 文本型, 公开, 成功返回语音代码(支持讨论组)
	inline std::string UploadGroupAudio(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int32_t audioType, const char* audioTextUtf8, const void* audioBytes, uint32_t audioSize, int32_t timeSeconds = 0)
	{
		TempUsc2AnsiString audioText(audioTextUtf8 ? audioTextUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU32(static_cast<uint32_t>(audioType));
		a.PushPtr(audioText.CStr());
		a.PushPtr(audioBytes);
		a.PushU32(audioSize);
		std::string ret = sdk.CallApiReturnUtf8(kApiName_UploadGroupAudio_Utf8, a.Data());

		if (!ret.empty() && ret.back() == ']')
		{
			ret.pop_back();
			ret.append(",time=").append(std::to_string(timeSeconds));
			ret.push_back(']');
		}
		return ret;
	}

	// 上传群文件, 文本型, 公开, 本命令为耗时操作，请另开线程执行
	inline std::string UploadGroupFile(const Sdk& sdk, int64_t thisQq, int64_t groupQq, const char* filePathUtf8, const char* folderUtf8 = "")
	{
		TempUsc2AnsiString filePath(filePathUtf8 ? filePathUtf8 : "");
		TempUsc2AnsiString folder(folderUtf8 ? folderUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushPtr(filePath.CStr());
		a.PushPtr(folder.CStr());
		return sdk.CallApiReturnUtf8(kApiName_UploadGroupFile_Utf8, a.Data());
	}

	// 取管理层列表, 文本型, 公开, 仅针对群聊,第一行是群主，剩下的是管理员
	inline std::string GetAdministratorList(const Sdk& sdk, int64_t thisQq, int64_t groupQq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		return sdk.CallApiReturnUtf8(kApiName_GetAdministratorList_Utf8, a.Data());
	}

	// 取群名片, 文本型, 公开, 成功返回群名片
	inline std::string GetGroupCard(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t memberQq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(memberQq));
		return sdk.CallApiReturnUtf8(kApiName_GetGroupCard_Utf8, a.Data());
	}

	// 设置群名片, 文本型, 公开, 置群名片
	inline std::string SetGroupCard(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t memberQq, const char* newCardUtf8)
	{
		TempUsc2AnsiString card(newCardUtf8 ? newCardUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(memberQq));
		a.PushPtr(card.CStr());
		return sdk.CallApiReturnUtf8(kApiName_SetGroupCard_Utf8, a.Data());
	}

	// 取昵称_从缓存, 文本型, 公开, 成功返回昵称
	inline std::string GetNicknameFromCache(const Sdk& sdk, const char* otherQqTextUtf8)
	{
		TempUsc2AnsiString other(otherQqTextUtf8 ? otherQqTextUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushPtr(other.CStr());
		return sdk.CallApiReturnUtf8(kApiName_GetNicknameFromCache_Utf8, a.Data());
	}

	// 强制取昵称, 文本型, 公开, 成功返回昵称
	inline std::string GetNicknameForce(const Sdk& sdk, int64_t thisQq, const char* otherQqTextUtf8)
	{
		TempUsc2AnsiString other(otherQqTextUtf8 ? otherQqTextUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(other.CStr());
		return sdk.CallApiReturnUtf8(kApiName_GetNicknameForce_Utf8, a.Data());
	}

	// 取好友文件下载地址, 文本型, 公开, 获取好友分享的文件的下载地址
	inline std::string GetFriendFileDownloadUrl(const Sdk& sdk, int64_t thisQq, const char* fileIdUtf8, const char* fileNameUtf8)
	{
		TempUsc2AnsiString fileId(fileIdUtf8 ? fileIdUtf8 : "");
		TempUsc2AnsiString fileName(fileNameUtf8 ? fileNameUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(fileId.CStr());
		a.PushPtr(fileName.CStr());
		return sdk.CallApiReturnUtf8(kApiName_GetFriendFileDownloadUrl_Utf8, a.Data());
	}

	// 取图片下载地址, 文本型, 公开, 支持群聊、私聊、讨论组、闪照、秀图
	inline std::string GetImageDownloadUrl(const Sdk& sdk, const char* imageCodeUtf8, int64_t thisQq, int64_t groupQq)
	{
		TempUsc2AnsiString code(imageCodeUtf8 ? imageCodeUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushPtr(code.CStr());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		return sdk.CallApiReturnUtf8(kApiName_GetImageDownloadUrl_Utf8, a.Data());
	}

	// 撤回消息_群聊, 逻辑型, 公开
	inline bool RecallGroupMessage(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t messageRandom, int32_t messageReq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(messageRandom));
		a.PushU32(static_cast<uint32_t>(messageReq));
		return sdk.CallApiReturnU32(kApiName_RecallGroupMessage_Utf8, a.Data()) != 0;
	}

	// 禁言群成员, 逻辑型, 公开
	inline bool MuteGroupMember(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t memberQq, int32_t durationSeconds)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(memberQq));
		a.PushU32(static_cast<uint32_t>(durationSeconds));
		return sdk.CallApiReturnU32(kApiName_MuteGroupMember_Utf8, a.Data()) != 0;
	}

	// 删除群成员, 逻辑型, 公开
	inline bool RemoveGroupMember(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t memberQq, bool refuseNextJoin)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(memberQq));
		a.PushU32(refuseNextJoin ? 1u : 0u);
		return sdk.CallApiReturnU32(kApiName_RemoveGroupMember_Utf8, a.Data()) != 0;
	}

	// 处理好友验证事件, , 公开, 在好友验证事件下使用，无权限时不执行
	inline void HandleFriendVerificationEvent(const Sdk& sdk, int64_t thisQq, int64_t triggerQq, int64_t messageSeq, int32_t operateType)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(triggerQq));
		a.PushU64(static_cast<uint64_t>(messageSeq));
		a.PushU32(static_cast<uint32_t>(operateType));
		sdk.CallApiVoid(kApiName_HandleFriendVerificationEvent_Utf8, a.Data());
	}

	// 处理群验证事件, , 公开, 在群验证事件下使用，无权限时不执行
	inline void HandleGroupVerificationEvent(const Sdk& sdk, int64_t thisQq, int64_t sourceGroupQq, int64_t triggerQq, int64_t messageSeq, int32_t operateType, int32_t eventType, const char* refuseReasonUtf8 = "")
	{
		TempUsc2AnsiString reason(refuseReasonUtf8 ? refuseReasonUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(sourceGroupQq));
		a.PushU64(static_cast<uint64_t>(triggerQq));
		a.PushU64(static_cast<uint64_t>(messageSeq));
		a.PushU32(static_cast<uint32_t>(operateType));
		a.PushU32(static_cast<uint32_t>(eventType));
		a.PushPtr(reason.CStr());
		sdk.CallApiVoid(kApiName_HandleGroupVerificationEvent_Utf8, a.Data());
	}

	// 全员禁言, 逻辑型, 公开, 失败或无权限返回假
	inline bool MuteAll(const Sdk& sdk, int64_t thisQq, int64_t groupQq, bool enabled)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU32(enabled ? 1u : 0u);
		return sdk.CallApiReturnU32(kApiName_MuteAll_Utf8, a.Data()) != 0;
	}

	// QQ点赞, 文本型, 公开
	inline std::string QQLike(const Sdk& sdk, int64_t thisQq, int64_t otherQq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(otherQq));
		return sdk.CallApiReturnUtf8(kApiName_QQLike_Utf8, a.Data());
	}

	// 群聊打卡, 文本型, 公开, 返回json
	inline std::string GroupCheckIn(const Sdk& sdk, int64_t thisQq, int64_t groupQq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		return sdk.CallApiReturnUtf8(kApiName_GroupCheckIn_Utf8, a.Data());
	}

	// 分享音乐, 逻辑型, 公开, 失败或无权限返回假
	inline bool ShareMusic(const Sdk& sdk, int64_t thisQq, int64_t target, const char* musicNameUtf8, const char* artistNameUtf8, const char* jumpUrlUtf8, const char* coverUrlUtf8, const char* fileUrlUtf8 = "", int32_t appType = 0, int32_t shareType = 0)
	{
		TempUsc2AnsiString music(musicNameUtf8 ? musicNameUtf8 : "");
		TempUsc2AnsiString artist(artistNameUtf8 ? artistNameUtf8 : "");
		TempUsc2AnsiString jump(jumpUrlUtf8 ? jumpUrlUtf8 : "");
		TempUsc2AnsiString cover(coverUrlUtf8 ? coverUrlUtf8 : "");
		TempUsc2AnsiString file(fileUrlUtf8 ? fileUrlUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(target));
		a.PushPtr(music.CStr());
		a.PushPtr(artist.CStr());
		a.PushPtr(jump.CStr());
		a.PushPtr(cover.CStr());
		a.PushPtr(file.CStr());
		a.PushU32(static_cast<uint32_t>(appType));
		a.PushU32(static_cast<uint32_t>(shareType));
		return sdk.CallApiReturnU32(kApiName_ShareMusic_Utf8, a.Data()) != 0;
	}

	// 提取图片文字, 逻辑型, 公开, tcp
	inline bool ExtractTextFromImage(const Sdk& sdk, int64_t thisQq, const char* imageUrlUtf8, std::string& outTextUtf8)
	{
		TempUsc2AnsiString url(imageUrlUtf8 ? imageUrlUtf8 : "");
		const char* outRaw = nullptr;
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(url.CStr());
		a.PushPtr(&outRaw);
		const bool ok = sdk.CallApiReturnU32(kApiName_ExtractTextFromImage_Utf8, a.Data()) != 0;
		outTextUtf8 = ok ? Usc2AnsiToUtf8(outRaw ? outRaw : "") : std::string();
		return ok;
	}

	// 调用指定OneBot接口, 文本型, 公开, 调用指定OneBot接口
	inline std::string CallOneBotInterface(const Sdk& sdk, int64_t thisQq, const char* sendDataUtf8, bool noWait = false)
	{
		TempUsc2AnsiString data(sendDataUtf8 ? sendDataUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(data.CStr());
		a.PushU32(noWait ? 1u : 0u);
		return sdk.CallApiReturnUtf8(kApiName_CallOneBotInterface_Utf8, a.Data());
	}

	// 取群成员信息, 文本型, 公开, 获取一个群成员的相关信息
	inline std::string GetGroupMemberInfo(const Sdk& sdk, int64_t thisQq, int64_t groupQq, int64_t otherQq, GroupMemberInfoV2* outData)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushU64(static_cast<uint64_t>(groupQq));
		a.PushU64(static_cast<uint64_t>(otherQq));
		a.PushPtr(outData);
		return sdk.CallApiReturnUtf8(kApiName_GetGroupMemberInfo_Utf8, a.Data());
	}

	// 取插件数据目录, 文本型, 公开, 没有权限限制，建议将设置文件之类的都写这里面，结果结尾带\\（反斜杠）
	inline std::string GetPluginDataDirectory(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnUtf8(kApiName_GetPluginDataDirectory_Utf8, a.Data());
	}

	// 重载自身, , 公开, 无权限限制
	inline void ReloadItSelf(const Sdk& sdk, const char* newDllPathUtf8 = nullptr)
	{
		const char* pathUtf8 = (newDllPathUtf8 == nullptr || *newDllPathUtf8 == '\0') ? "empty" : newDllPathUtf8;
		TempUsc2AnsiString path(pathUtf8);
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushPtr(path.CStr());
		sdk.CallApiVoid(kApiName_ReloadItSelf_Utf8, a.Data());
	}

	// 取插件自身版本号, 文本型, 公开, 无权限限制,返回值=json文件当中写的版本号
	inline std::string GetPluginSelfVersion(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnUtf8(kApiName_GetPluginSelfVersion_Utf8, a.Data());
	}

	// 取框架主窗口句柄, 整数型, 公开, 无权限限制,可用于取消框架窗口的皮肤
	inline uint32_t GetFrameworkMainWindowHandle(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnU32(kApiName_GetFrameworkMainWindowHandle_Utf8, a.Data());
	}

	// 取QQ头像, 文本型, 公开, 获取QQ头像,无权限限制
	inline std::string GetQQAvatar(const Sdk& sdk, int64_t otherQq, bool hdOriginal = false)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(otherQq));
		a.PushU32(hdOriginal ? 1u : 0u);
		return sdk.CallApiReturnUtf8(kApiName_GetQQAvatar_Utf8, a.Data());
	}

	// 取插件文件名, 文本型, 公开, 无权限限制,可取到自身的真实插件文件名
	inline std::string GetPluginFileName(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnUtf8(kApiName_GetPluginFileName_Utf8, a.Data());
	}

	// 取框架版本, 文本型, 公开, 无权限限制
	inline std::string GetFrameworkVersion(const Sdk& sdk)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		return sdk.CallApiReturnUtf8(kApiName_GetFrameworkVersion_Utf8, a.Data());
	}

	// 创建网络文件路径, 文本型, 公开, 创建网络文件路径，无权限限制
	inline std::string CreateNetworkFilePath(const Sdk& sdk, int64_t thisQq, const char* filePathUtf8)
	{
		TempUsc2AnsiString path(filePathUtf8 ? filePathUtf8 : "");
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		a.PushPtr(path.CStr());
		return sdk.CallApiReturnUtf8(kApiName_CreateNetworkFilePath_Utf8, a.Data());
	}

	// 取当前OneBot客户端类型, 文本型, 公开, 取当前OneBot客户端类型，无权限限制。1Lagrange、2NapCat、3LLOneBot
	inline std::string GetCurrentOneBotClientType(const Sdk& sdk, int64_t thisQq)
	{
		ArgPacker a;
		a.PushPtr(sdk.PluginKey().c_str());
		a.PushU64(static_cast<uint64_t>(thisQq));
		return sdk.CallApiReturnUtf8(kApiName_GetCurrentOneBotClientType_Utf8, a.Data());
	}
}
