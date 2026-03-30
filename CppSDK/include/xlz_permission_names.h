#pragma once

#include "xlz_permissions.h"
#include "xlz_zh_names_utf8.h"

namespace xlz
{
	// 权限中文名（用于生成needapilist的key，对应：_Api_names / 权限_XXX）
	inline const char* GetPermissionChineseName(Permission permission)
	{
		switch (permission)
		{
		case Permission::OutputLog: return kApiName_OutputLog_Utf8;
		case Permission::SendFriendMessage: return kApiName_SendFriendMessage_Utf8;
		case Permission::SendGroupMessage: return kApiName_SendGroupMessage_Utf8;
		default: return "";
		}
	}
}

