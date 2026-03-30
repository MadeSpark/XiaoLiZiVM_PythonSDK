#pragma once

#include <cstdint>

namespace xlz
{
#pragma pack(push, 1)

	// 私聊消息事件结构体（对应：PrivateMessageEvent）
	struct PrivateMessageEvent
	{
		int64_t SenderQQ;
		int64_t ThisQQ;
		uint32_t MessageReq;
		int64_t MessageSeq;
		uint32_t MessageReceiveTime;
		int64_t MessageGroupQQ;
		uint32_t MessageSendTime;
		int64_t MessageRandom;
		uint32_t MessageClip;
		uint32_t MessageClipCount;
		int64_t MessageClipID;
		const char* MessageContent;
		uint32_t BubbleID;
		uint32_t MessageType;
		uint32_t MessageSubType;
		uint32_t MessageSubTemporaryType;
		uint32_t RedEnvelopeType;
		void* SessionToken;
		int64_t SourceEventQQ;
		const char* SourceEventQQName;
		const char* FileID;
		const char* FileMD5;
		const char* FileName;
		int64_t MsgGroupId;
	};

	// 群消息事件结构体（对应：GroupMessageEvent）
	struct GroupMessageEvent
	{
		int64_t SenderQQ;
		int64_t ThisQQ;
		int32_t MessageReq;
		int32_t MessageReceiveTime;
		int64_t MessageGroupQQ;
		const char* SourceGroupName;
		const char* SenderNickname;
		int32_t MessageSendTime;
		int64_t MessageRandom;
		int32_t MessageClip;
		int32_t MessageClipCount;
		int64_t MessageClipID;
		uint32_t MessageType;
		const char* SenderTitle;
		const char* MessageContent;
		const char* ReplyMessageContent;
		int32_t BubbleID;
		int32_t GroupChatLevel;
		int32_t PendantID;
		const char* AnonymousNickname;
		void* AnonymousFalg;
		const char* ReservedParameters;
		int64_t AnonymousId;
		int32_t FontId;
	};

	// 事件消息结构体（对应：EventTypeBase）
	struct EventTypeBase
	{
		int64_t ThisQQ;
		int64_t SourceGroupQQ;
		int64_t OperateQQ;
		int64_t TriggerQQ;
		int64_t MessageSeq;
		uint32_t MessageTime;
		const char* SourceGroupName;
		const char* OperateQQName;
		const char* TriggerQQName;
		const char* MessageContent;
		uint32_t EventType;
		uint32_t EventSubType;
	};

#pragma pack(pop)
}

