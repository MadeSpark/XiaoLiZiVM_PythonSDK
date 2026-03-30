#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 初始化Python解释器（框架启动时调一次）
// 返回0表示成功，返回1表示已初始化，返回-1表示初始化失败
__declspec(dllexport) int __stdcall XLZ_PyInitialize(void);

// 销毁Python解释器（框架关闭时调一次）
// 返回0表示成功，返回1表示未初始化
__declspec(dllexport) int __stdcall XLZ_PyFinalize(void);

#ifdef __cplusplus
}
#endif
