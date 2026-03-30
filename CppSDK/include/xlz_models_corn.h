#pragma once

#include <cstdint>
#include <cstring>

namespace xlz
{
#pragma pack(push, 1)

	struct GroupInfo
	{
		int64_t GroupID;
		int64_t GroupQQ;
		int64_t Reserved1;
		int64_t GroupInfoSeq;
		int64_t Reserved2;
		int64_t GroupRankSeq;
		int64_t Reserved3;
		int64_t ShutUpTimestamp;
		int64_t ThisShutUpTimestamp;
		int64_t Reserved4;
		int64_t Reserved5;
		int64_t Reserved6;
		int64_t CertificationType;
		int64_t Reserved8;
		int64_t Reserved9;
		int64_t Reserved10;
		int64_t Reserved11;
		int64_t GroupMemberCount;
		int64_t MemberNumSeq;
		int64_t MemberCardSeq;
		int64_t Reserved12;
		int64_t GroupOwnerUin;
		int64_t Reserved13;
		int64_t Reserved14;
		int64_t Reserved15;
		int64_t CmduinJoinTime;
		const char* GroupName;
		const char* GroupMemo;
	};

	struct GroupMemberInfo
	{
		const char* QQNumber;
		int32_t Age;
		int32_t Gender;
		const char* Name;
		const char* Email;
		const char* Nickname;
		const char* Note;
		const char* Title;
		const char* Phone;
		int64_t TitleTimeout;
		int64_t ShutUpTimestamp;
		int64_t JoinTime;
		int64_t ChatTime;
		int64_t Level;
	};

	struct GroupMemberInfoV2
	{
		const char* GroupCardName;
		const char* NickName;
		const char* GroupChatLevel;
		int64_t JoinTime;
		int64_t LastSpeackTime;
		int32_t GroupPosition;
		const char* GroupTitle;
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

