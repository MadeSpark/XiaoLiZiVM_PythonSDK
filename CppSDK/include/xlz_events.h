#pragma once

#include <cstdint>

// 字段命名见 开发SDK/SDK字段命名规范.md
// 规则：C++ 用 PascalCase，Python 事件键用同词根 snake_case；QQ 号统一 *Uin。
namespace xlz
{
#pragma pack(push, 1)

	// 私聊消息事件结构体（对应易语言：私聊消息数据）
	struct PrivateMessageEvent
	{
		int64_t SenderUin;                 // 发送人QQ
		int64_t MyUin;                     // 框架QQ
		uint32_t MessageReq;               // 消息Req
		int64_t MessageSeq;                // 消息Seq
		uint32_t MessageReceiveTime;       // 消息接收时间
		int64_t MessageGroupUin;           // 消息群号（群临时会话时可取）
		uint32_t MessageSendTime;          // 消息发送时间
		int64_t MessageRandom;             // 消息random
		uint32_t MessageClip;              // 消息分片序列
		uint32_t MessageClipCount;         // 消息分片数量
		int64_t MessageClipId;             // 消息分片标识
		const char* Message;               // 消息内容
		uint32_t BubbleId;                 // 发送人气泡id
		uint32_t MessageType;              // 消息type
		uint32_t MessageSubType;           // 消息subtype
		uint32_t MessageSubTemporaryType;  // 消息子临时类型
		uint32_t RedEnvelopeType;          // 红包类型
		void* SessionToken;                // 会话Token
		int64_t SourceEventUin;            // 来源事件QQ
		const char* SourceEventUinName;    // 来源事件QQ昵称
		const char* FileId;                // 保留参数1（文件id）
		const char* FileMd5;               // 保留参数2（文件md5）
		const char* FileName;              // 保留参数3（文件名）
		int64_t MsgGroupId;                // 消息群Id
	};

	// 群消息事件结构体（对应易语言：群消息数据）
	struct GroupMessageEvent
	{
		int64_t SenderUin;             // 发送人QQ（匿名消息时为匿名Id）
		int64_t MyUin;                 // 框架QQ
		int32_t MessageReq;            // 消息Req
		int32_t MessageReceiveTime;    // 消息接收时间
		int64_t MessageGroupUin;       // 消息群号
		const char* GroupName;         // 来源群名称
		const char* Card;              // 发送人群名片
		int32_t MessageSendTime;       // 消息发送时间
		int64_t MessageRandom;         // 消息random
		int32_t MessageClip;           // 消息分片序列
		int32_t MessageClipCount;      // 消息分片数量
		int64_t MessageClipId;         // 消息分片标识
		uint32_t MessageType;          // 消息type
		const char* SenderTitle;       // 发送人群头衔
		const char* Message;           // 消息内容
		const char* ReplyMessage;      // 回复对象消息内容
		int32_t BubbleId;              // 发送者气泡id
		int32_t GroupChatLevel;        // 群聊等级
		int32_t PendantId;             // 挂件Id
		const char* AnonymousNickname; // 匿名昵称
		void* AnonymousFlag;           // 匿名标识
		const char* ReservedParameters;// 保留参数
		int64_t AnonymousId;           // 框架QQ匿名Id
		int32_t FontId;                // 字体Id
	};

	// 事件消息结构体（对应易语言：事件消息数据）
	struct EventTypeBase
	{
		int64_t MyUin;                 // 框架QQ
		int64_t SourceGroupUin;        // 来源群号
		int64_t OperateUin;            // 操作QQ
		int64_t TriggerUin;            // 触发QQ
		int64_t MessageSeq;            // 消息Seq
		uint32_t MessageTime;          // 消息时间戳
		const char* GroupName;         // 来源群名
		const char* OperateUinName;    // 操作QQ昵称
		const char* TriggerUinName;    // 触发QQ昵称
		const char* Message;           // 消息内容
		uint32_t EventType;            // 消息类型
		uint32_t EventSubType;         // 消息子类型
	};

#pragma pack(pop)
}
