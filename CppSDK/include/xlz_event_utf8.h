#pragma once

#include <string>

#include "xlz_encoding.h"
#include "xlz_events.h"
#include "xlz_text_normalize.h"

namespace xlz
{
	// 取私聊消息内容(UTF-8)（对应：私聊消息数据.消息内容 + usc2toutf8）
	inline std::string GetPrivateMessageContentUtf8(const PrivateMessageEvent& ev)
	{
		const std::string utf8 = Usc2AnsiToUtf8(ev.MessageContent ? ev.MessageContent : "");
		return NormalizeNewlinesToLf(utf8.c_str());
	}

	// 取群消息内容(UTF-8)（对应：群消息数据.消息内容 + usc2toutf8）
	inline std::string GetGroupMessageContentUtf8(const GroupMessageEvent& ev)
	{
		const std::string utf8 = Usc2AnsiToUtf8(ev.MessageContent ? ev.MessageContent : "");
		return NormalizeNewlinesToLf(utf8.c_str());
	}

	// 取事件消息内容(UTF-8)（对应：事件消息数据.消息内容 + usc2toutf8）
	inline std::string GetEventMessageContentUtf8(const EventTypeBase& ev)
	{
		const std::string utf8 = Usc2AnsiToUtf8(ev.MessageContent ? ev.MessageContent : "");
		return NormalizeNewlinesToLf(utf8.c_str());
	}
}

