#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 初始化Python解释器（框架启动时调一次）
// 返回0表示成功，返回1表示已初始化，返回-1表示初始化失败
//
// 自由线程：初始化成功后，本实现会立刻释放调用线程持有的 GIL，
//          从而允许任意线程（框架线程 / 插件自建线程 / 异步工作线程）
//          通过 PyGILState_Ensure 正常获取 GIL 执行 Python。
__declspec(dllexport) int __stdcall XLZ_PyInitialize(void);

// 销毁Python解释器（框架关闭时调一次）
// 返回0表示成功，返回1表示未初始化
// 销毁前会自动重新获取 GIL，避免 Py_Finalize 崩溃。
__declspec(dllexport) int __stdcall XLZ_PyFinalize(void);

// 查询解释器是否已初始化（返回1=已初始化，0=未初始化）
__declspec(dllexport) int __stdcall XLZ_PyIsInitialized(void);

#ifdef __cplusplus
}
#endif
