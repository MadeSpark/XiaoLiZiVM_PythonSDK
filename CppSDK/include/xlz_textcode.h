#pragma once

#include <cstdint>
#include <string>

namespace xlz
{
	// 艾特某人（对应：艾特某人 / GetAt）
	inline std::string MakeAt(int64_t qq)
	{
		return "[@" + std::to_string(qq) + "]";
	}

	// 艾特全体（对应：艾特全体 / GetAtAll）
	inline std::string MakeAtAll()
	{
		return "[@all]";
	}

	// 表情（对应：表情 / GetFace）
	inline std::string MakeFace(int id, const std::string& nameUtf8)
	{
		return "[Face,Id=" + std::to_string(id) + ",name=" + nameUtf8 + "]";
	}

	// 大表情（对应：大表情 / GetFace(id,name,hash,flag)）
	inline std::string MakeBigFace(int id, const std::string& nameUtf8, const std::string& hashUtf8, const std::string& flagUtf8)
	{
		return "[bigFace,Id=" + std::to_string(id) + ",name=" + nameUtf8 + ",hash=" + hashUtf8 + ",flag=" + flagUtf8 + "]";
	}

	// 小表情（对应：小表情 / GetSmallFace）
	inline std::string MakeSmallFace(int id, const std::string& nameUtf8)
	{
		return "[smallFace,name=" + nameUtf8 + ",Id=" + std::to_string(id) + "]";
	}

	// 抖一抖（对应：抖一抖 / GetShake）
	inline std::string MakeShake(const std::string& nameUtf8, int id, int type)
	{
		return "[Shake,name=" + nameUtf8 + " ,Type=" + std::to_string(type) + ",Id=" + std::to_string(id) + "]";
	}

	// 厘米秀（对应：厘米秀 / GetLimiShow）
	inline std::string MakeLimiShow(const std::string& nameUtf8, int id, int type, int64_t objectQq)
	{
		return "[limiShow,Id=" + std::to_string(id) + ",name=" + nameUtf8 + ",type=" + std::to_string(type) + ",object=" + std::to_string(objectQq) + "]";
	}

	// 闪照_本地（对应：闪照_本地 / GetFlashPicFile）
	inline std::string MakeFlashPicFile(const std::string& pathUtf8)
	{
		return "[flashPicFile,path=" + pathUtf8 + "]";
	}

	// 图片_本地（对应：图片_本地 / GetPicFile）
	inline std::string MakePicFile(const std::string& pathUtf8)
	{
		return "[picFile,path=" + pathUtf8 + "]";
	}

	// 语音_本地（对应：语音_本地 / GetAudioFile）
	inline std::string MakeAudioFile(const std::string& pathUtf8)
	{
		return "[AudioFile,path=" + pathUtf8 + "]";
	}

	// 小视频（对应：小视频 / GetLitleVideo）
	inline std::string MakeLittleVideo(const std::string& linkParamUtf8, const std::string& hash1Utf8, const std::string& hash2Utf8, int wide = 0, int high = 0, int time = 0)
	{
		return "[litleVideo,linkParam=" + linkParamUtf8 + ",hash1=" + hash1Utf8 + ",hash2=" + hash2Utf8 + ",wide=" + std::to_string(wide) + ",high=" + std::to_string(high) + ",time=" + std::to_string(time) + "]";
	}
}

