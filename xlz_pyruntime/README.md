# xlz_pyruntime.dll - Python 运行时管理库

## 概述

`xlz_pyruntime.dll` 是一个轻量级的 Python 解释器管理库，用于在易语言框架中初始化、查询和销毁
内嵌的 **CPython 3.14 自由线程（free-threading，PEP 703）Win32** 解释器。

## 功能

- 动态加载 `python\python314t.dll`（以及稳定 ABI 转发 DLL `python\python3t.dll`）
- `Py_SetPath()` 设置标准库与扩展模块搜索路径
- 初始化 Python 解释器（仅执行一次）
- 初始化后调用 `PyEval_SaveThread()` 释放 GIL
- 销毁 Python 解释器（仅执行一次）
- 线程安全的引用计数管理

## 编译

1. 用 Visual Studio 2022 / 2026 打开 `xlz_pyruntime.sln`
2. 选择 **`Win32` + `Release`** 配置（必须是 Win32；EasyLanguage 宿主是 32 位进程）
3. 编译生成 `bin\Release\xlz_pyruntime.dll`

## 部署

```
main\corn\
├── xlz_pyruntime.dll          ← 本库
└── python\                    ← CPython 3.14.8t Win32 运行时目录
    ├── python314t.dll
    ├── python3t.dll
    ├── Lib\                   ← 标准库
    ├── DLLs\                  ← 扩展模块（*.cp314t-win32.pyd）及其依赖 DLL
    ├── python.exe
    └── ...
```

`xlz_pyruntime.dll` 与 `python\` 目录同级即可；**没有** `python314.zip`，也**没有** `python314._pth`。
运行时目录必须包含 `Lib\` 和 `DLLs\` 两个子目录。

## 易语言调用

### 框架启动时

```易语言
// 初始化Python解释器
返回值 = 调用DLL("main\corn\xlz_pyruntime.dll", "XLZ_PyInitialize", )
// 返回值：0=成功，1=已初始化，-1=失败
```

### 查询状态

```易语言
返回值 = 调用DLL("main\corn\xlz_pyruntime.dll", "XLZ_PyIsInitialized", )
// 返回值：1=已初始化，0=未初始化
```

### 框架关闭时

```易语言
// 销毁Python解释器
返回值 = 调用DLL("main\corn\xlz_pyruntime.dll", "XLZ_PyFinalize", )
// 返回值：0=成功，1=未初始化
```

> **当前小栗子虚拟机宿主不调用 `XLZ_PyFinalize()`**：宿主 EXE 只引用 `XLZ_PyInitialize`，退出时进程直接终止。上面的关闭示例适用于自建宿主；不要依赖当前宿主在退出时执行 Finalize 清理。

## 导出函数

### XLZ_PyInitialize()

**签名：** `int __stdcall XLZ_PyInitialize(void)`

**功能：** 初始化 Python 解释器

**返回值：**
- `0` - 成功初始化
- `1` - 已初始化（重复调用）
- `-1` - 初始化失败（`python314t.dll` 加载失败或初始化出错）

**说明：**
- 第一次调用时会加载 `python314t.dll` 并初始化解释器
- 初始化成功后立即调用 `PyEval_SaveThread()` 释放 GIL；3.14t 下即使 GIL 已关闭也会保存线程状态，
  供 `XLZ_PyFinalize()` 恢复
- 后续调用返回 1，不会重复初始化
- 必须在加载任何 Python 插件前调用

### XLZ_PyIsInitialized()

**签名：** `int __stdcall XLZ_PyIsInitialized(void)`

**返回值：** `1` = 已初始化，`0` = 未初始化。

### XLZ_PyFinalize()

**签名：** `int __stdcall XLZ_PyFinalize(void)`

**功能：** 销毁 Python 解释器

**返回值：**
- `0` - 成功销毁
- `1` - 未初始化（重复调用或未初始化）

**说明：**
- 自建宿主应在框架关闭前调用；**当前小栗子虚拟机宿主不调用此函数**，退出即进程终止
- 若初始化时保存过线程状态，会先 `PyEval_RestoreThread()` 再 `Py_Finalize()`
- 调用后所有 Python 插件将无法使用
- 不能在 Python 代码执行过程中调用

## 工作原理

1. **加载阶段**：`XLZ_PyInitialize()` 动态加载 `python\python314t.dll`，并可选加载
   `python\python3t.dll`（稳定 ABI 转发 DLL；3.14t 没有 `python3.dll`），再 `GetProcAddress`
   取所需 API
2. **初始化阶段**：调用 `Py_SetPath()` 设置
   `python\Lib;python\DLLs;python\Lib\lib-dynload;python\`，然后调用 `Py_Initialize()`
3. **释放 GIL**：调用 `PyEval_SaveThread()` 并保存返回的 `PyThreadState*`
4. **运行阶段**：Python 解释器在进程内运行，所有插件共享同一个解释器实例
5. **销毁阶段**：先 `PyEval_RestoreThread()`，再 `Py_Finalize()` 清理解释器，最后卸载
   `python314t.dll`

### 关于 3.14t 的几个硬性事实

- `python314t.dll` 是 **x86 / PE32**，导出 1800+ 个符号，包含 `Py_SetPath`、`Py_Initialize`、
  `PyEval_SaveThread`、`PyEval_RestoreThread`、`PyGILState_Ensure/Release` 等。
- 3.14t **没有 `python3.dll`**；GIL 版 `abi3` 扩展链接的是 `python3.dll`，因此无法加载。
  `abi3t` 在 3.14.8t 下未验证（没有对应的扩展后缀）；请按 `cp314t` 或纯 Python 处理。
- 3.14t 完整安装树的标准库在 `Lib\`，扩展在 `DLLs\`，没有 embeddable zip 那种
  `python314.zip` + `._pth` 结构。
- `sys.abiflags` 在 3.14t 中不存在；扩展后缀为 `.cp314t-win32.pyd`
  （`sysconfig.get_config_var("EXT_SUFFIX")`）。
- `PyGILState_Check()` 在 3.14t 中恒返回 `1`；不要用它判断当前线程是否持有 GIL，也不要在
  `Py_Initialize()` 之后重复调用 `PyEval_SaveThread()`。
- 最低系统要求 **Windows 10+**（CPython 3.14 的硬性要求）。

## 日志

运行日志写入 `main\corn\xlz_pyruntime.log`（与 DLL 同目录）。初始化失败时先看这个文件。

## 多插件隔离

虽然所有插件共享同一个 Python 解释器，但每个插件 DLL 应该在自己的模块命名空间中执行代码，
避免全局变量冲突。

参考插件 DLL 的实现方式：

```cpp
// 创建独立模块命名空间
PyObject* pModule = PyImport_AddModule("__xlz_plugin_unique_name__");
PyObject* pDict = PyModule_GetDict(pModule);

// 在这个命名空间中执行插件代码
PyRun_String(plugin_source_code, Py_file_input, pDict, pDict);
```

## 注意事项

- `python\python314t.dll`、`python\python3t.dll`、`python\Lib\`、`python\DLLs\` 必须齐全
- 必须先调用 `XLZ_PyInitialize()` 再加载任何 Python 插件
- 自建宿主必须在框架完全关闭前调用 `XLZ_PyFinalize()`；**当前小栗子虚拟机宿主不调用它**，退出路径是进程终止
- 不支持在同一进程中多次初始化 / 销毁解释器（Python 本身的限制）
- 不要在 Python 代码执行过程中调用 `XLZ_PyFinalize()`
- 初始化是非隔离模式：`sys.prefix` 为空、`sys.executable` 指向宿主 exe；用户目录的
  `site-packages` 与 `PYTHON*` 环境变量仍可能生效，需要隔离时不要依赖这些路径
- `main\corn\sqlite3.dll` 是旧版同名 DLL；若它在插件 `import sqlite3` 前已被宿主加载，
  `import sqlite3` 可能失败。这是旧版已有问题，不是 3.14t 引入；不要把它放进 `python\` 或抢先加载
