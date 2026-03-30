# xlz_pyruntime.dll - Python运行时管理库

## 概述

`xlz_pyruntime.dll` 是一个轻量级的Python解释器管理库，用于在易语言框架中初始化和销毁CPython解释器。

## 功能

- 动态加载 `python315.dll`（从 `main\corn` 目录）
- 初始化Python解释器（仅执行一次）
- 销毁Python解释器（仅执行一次）
- 线程安全的引用计数管理

## 编译

1. 用Visual Studio 2019/2022打开 `xlz_pyruntime.sln`
2. 选择 `Win32` + `Release` 配置
3. 编译生成 `bin\Release\xlz_pyruntime.dll`

## 部署

将编译后的 `xlz_pyruntime.dll` 放在 `main\corn` 目录下，与 `python315.dll` 和 `python315.zip` 同级。

## 易语言调用

### 框架启动时

```易语言
// 初始化Python解释器
返回值 = 调用DLL("main\corn\xlz_pyruntime.dll", "XLZ_PyInitialize", )
// 返回值：0=成功，1=已初始化，-1=失败
```

### 框架关闭时

```易语言
// 销毁Python解释器
返回值 = 调用DLL("main\corn\xlz_pyruntime.dll", "XLZ_PyFinalize", )
// 返回值：0=成功，1=未初始化
```

## 导出函数

### XLZ_PyInitialize()

**签名：** `int __stdcall XLZ_PyInitialize(void)`

**功能：** 初始化Python解释器

**返回值：**
- `0` - 成功初始化
- `1` - 已初始化（重复调用）
- `-1` - 初始化失败（python315.dll加载失败或初始化出错）

**说明：** 
- 第一次调用时会加载 `python315.dll` 并初始化解释器
- 后续调用会返回1，不会重复初始化
- 必须在加载任何Python插件前调用

### XLZ_PyFinalize()

**签名：** `int __stdcall XLZ_PyFinalize(void)`

**功能：** 销毁Python解释器

**返回值：**
- `0` - 成功销毁
- `1` - 未初始化（重复调用或未初始化）

**说明：**
- 必须在框架关闭前调用
- 调用后所有Python插件将无法使用
- 不能在Python代码执行过程中调用

## 工作原理

1. **加载阶段**：`XLZ_PyInitialize()` 被调用时，动态加载 `python315.dll`，获取Python API函数指针
2. **初始化阶段**：调用 `Py_SetPath()` 设置Python路径（包含 `python315.zip`），然后调用 `Py_Initialize()`
3. **运行阶段**：Python解释器在进程内运行，所有插件共享同一个解释器实例
4. **销毁阶段**：`XLZ_PyFinalize()` 被调用时，调用 `Py_Finalize()` 清理解释器，卸载 `python315.dll`

## 多插件隔离

虽然所有插件共享同一个Python解释器，但每个插件DLL应该在自己的模块命名空间中执行代码，避免全局变量冲突。

参考插件DLL的实现方式：

```cpp
// 创建独立模块命名空间
PyObject* pModule = PyImport_AddModule("__xlz_plugin_unique_name__");
PyObject* pDict = PyModule_GetDict(pModule);

// 在这个命名空间中执行插件代码
PyRun_String(plugin_source_code, Py_file_input, pDict, pDict);
```

## 注意事项

- `python315.dll` 和 `python315.zip` 必须在 `main\corn` 目录下
- 必须先调用 `XLZ_PyInitialize()` 再加载任何Python插件
- 必须在框架完全关闭前调用 `XLZ_PyFinalize()`
- 不支持在同一进程中多次初始化/销毁解释器（Python本身的限制）
