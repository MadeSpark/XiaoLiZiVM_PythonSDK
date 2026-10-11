#pragma once

#include <cstdint>
#include <cstring>

// 字段命名见 开发SDK/SDK字段命名规范.md
// 规则：QQ 号统一 *Uin；标识类用 Id；群名片用 Card、昵称用 Nickname。
namespace xlz
{
#pragma pack(push, 1)

	// 群信息（对应易语言：群信息）
	struct GroupInfo
	{
		int64_t GroupId;                 // 群ID
		int64_t GroupUin;                // 群号
		int64_t Reserved1;
		int64_t GroupInfoSeq;            // 群信息查询Seq
		int64_t Reserved2;
		int64_t GroupRankSeq;            // 群等级查询Seq
		int64_t Reserved3;
		int64_t ShutUpTimestamp;         // 全员禁言解除时间戳
		int64_t MyShutUpTimestamp;       // 解除禁言时间戳（自身被禁言时）
		int64_t Reserved4;
		int64_t Reserved5;
		int64_t Reserved6;
		int64_t CertificationType;       // 群信用等级
		int64_t Reserved8;
		int64_t Reserved9;
		int64_t Reserved10;
		int64_t Reserved11;
		int64_t GroupMemberCount;        // 群成员数量
		int64_t MemberNumSeq;            // 群成员数量查询Seq
		int64_t MemberCardSeq;           // 群成员名片查询Seq
		int64_t Reserved12;
		int64_t GroupOwnerUin;           // 群主QQ
		int64_t Reserved13;
		int64_t Reserved14;
		int64_t Reserved15;
		int64_t MyJoinTime;              // 入群时间戳
		const char* GroupName;           // 群名称
		const char* GroupNotice;         // 新人公告
	};

	// 群成员信息（对应易语言：群成员信息）
	struct GroupMemberInfo
	{
		const char* Uin;                 // 账号
		int32_t Age;                     // 年龄
		int32_t Gender;                  // 性别：255隐藏 0男 1女
		const char* Nickname;            // 昵称
		const char* Email;               // 邮箱
		const char* Card;                // 名片（无名片则为空）
		const char* Note;                // 备注
		const char* Title;               // 头衔（无专属头衔则为空）
		const char* Phone;               // 手机号
		int64_t TitleTimeout;            // 头衔到期时间
		int64_t ShutUpTimestamp;         // 解禁时间戳
		int64_t JoinTime;                // 加群时间
		int64_t ChatTime;                // 发言时间
		int64_t Level;                   // 群等级
	};

	// 群成员信息V2（对应易语言：群成员信息V2）
	struct GroupMemberInfoV2
	{
		const char* GroupCard;           // 群名片（无则为空白）
		const char* Nickname;            // 昵称
		const char* GroupChatLevel;      // 群聊等级（文本型）
		int64_t JoinTime;                // 入群时间戳
		int64_t LastSpeakTime;           // 最后发言时间戳
		int32_t GroupPosition;           // 管理权限：1普通 2管理员 3群主
		const char* GroupTitle;          // 头衔（无则为空白）
	};

	struct GroupListBlock
	{
		int32_t index;
		int32_t Amount;
		uint8_t pAddrList[1024 * 10];
	};

	struct GroupMemberListBlock
	{
		int32_t index;
		int32_t Amount;
		uint8_t pAddrList[1024 * 100];
	};

#pragma pack(pop)

	inline void* ReadPtr32(const void* base, size_t offsetBytes)
	{
		uint32_t v = 0;
		std::memcpy(&v, static_cast<const uint8_t*>(base) + offsetBytes, sizeof(v));
		return reinterpret_cast<void*>(static_cast<uintptr_t>(v));
	}

	inline const GroupInfo* GetGroupInfoPtr(const GroupListBlock& block, int32_t i)
	{
		return static_cast<const GroupInfo*>(ReadPtr32(block.pAddrList, static_cast<size_t>(i) * 4));
	}

	inline const GroupMemberInfo* GetGroupMemberInfoPtr(const GroupMemberListBlock& block, int32_t i)
	{
		return static_cast<const GroupMemberInfo*>(ReadPtr32(block.pAddrList, static_cast<size_t>(i) * 4));
	}
}
