# -*- coding: utf-8 -*-
"""
小栗子虚拟机 Python SDK —— 一键重建脚本

做 6 件事：
  1. MSBuild 编译 PythonSDK/PythonCompatPlugin.sln (Release|Win32)
       -> PythonSDK\\bin\\Release\\XiaoLiZiVM_PythonCompat.dll   （C++ 空壳，Python 插件的地基）
  2. MSBuild 编译 xlz_pyruntime/xlz_pyruntime.sln (Release|Win32)
       -> xlz_pyruntime\\bin\\Release\\xlz_pyruntime.dll          （Python 运行时加载器）
  3. 把 xlz_pyruntime.dll 部署到框架目录 小栗子虚拟机_AI工作区\\main\\corn\\
  4. 用 PythonSDK/tools/pack_payload.py 重打 4 个 Demo 成品 DLL
  5. 解析编译产物的 PE 导出表，按 exports.def 逐条核对（少一个就报错）
  6. 组装 Releases\\ 成品区（作者工具包）—— 导出表校验不通过就不生成，避免发出坏包

关于 Releases\\（成品区）：
    一个「不需要编译、拿来就能写插件」的独立目录，只放作者要用的东西：
        空壳 DLL + pack_payload.py + xlz_sdk.py + xlz_constants.py + plugin_main.py
        + 自动改写路径的 AI 编写提示词 + 自动生成的 README
    它是**纯生成物**，每次跑本脚本都会被整体重建，请勿手工往里面放东西。

用法：
    python tools\\rebuild_all.py               # 完整重建（推荐）
    python tools\\rebuild_all.py --skip-build  # 跳过 MSBuild，只做部署 + 打包 + 校验 + 成品区
    python tools\\rebuild_all.py --dry-run     # 只打印计划，不写任何文件
    python tools\\rebuild_all.py --no-deploy   # 不往 main\\corn\\ 拷 xlz_pyruntime.dll
    python tools\\rebuild_all.py --no-releases # 不组装 Releases\\ 成品区
    python tools\\rebuild_all.py --releases-only  # 只重新组装成品区（不编译、不部署、不打 Demo）

注意：PythonSDK 依赖 CppSDK（..\\CppSDK\\include 与 ..\\CppSDK\\src）。
      改了 CppSDK 头文件后，必须跑本脚本重编，否则 DLL 还是旧的 —— 源码改了不等于产物改了。
"""

import argparse
import datetime
import hashlib
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------- 路径

TOOLS = Path(__file__).resolve().parent          # .../XiaoLiZiVM_PythonSDK/tools
ROOT = TOOLS.parent                              # .../XiaoLiZiVM_PythonSDK
SDK = ROOT / "PythonSDK"
OUT = SDK / "bin" / "Release"

SHELL_DLL = OUT / "XiaoLiZiVM_PythonCompat.dll"                  # C++ 空壳
RUNTIME_DLL = ROOT / "xlz_pyruntime" / "bin" / "Release" / "xlz_pyruntime.dll"
CORN_DIR = ROOT.parent.parent / "main" / "corn"                  # 小栗子虚拟机_AI工作区\main\corn
CORN_DLL = CORN_DIR / "xlz_pyruntime.dll"
PACK_TOOL = SDK / "tools" / "pack_payload.py"

# 解决方案：(显示名, .sln 路径, 编译产物)
SOLUTIONS = [
    ("PythonCompatPlugin（C++ 空壳 DLL）", SDK / "PythonCompatPlugin.sln", SHELL_DLL),
    ("xlz_pyruntime（Python 运行时加载器）", ROOT / "xlz_pyruntime" / "xlz_pyruntime.sln", RUNTIME_DLL),
]

# Demo 成品：(源目录, 输出 DLL 名)  —— 配对关系由解析现有成品尾部 payload 反推确认
PACKS = [
    ("Demo_关键词回复", "关键词回复_Python.dll"),
    ("Demo_人机验证", "人机验证_Python.dll"),
    ("Demo_外部依赖演示", "pillow演示.dll"),
    ("Demo_TemplatePlugin", "XiaoLiZiVM_PythonCompat_packed.dll"),
]

# ---------------------------------------------------------------- Releases 成品区
# 一个纯生成物目录：只放「写 Python 插件」需要的东西，不带 CppSDK / Demo / 工程文件。
RELEASES = ROOT / "Releases"
TEMPLATE_DIR = SDK / "Demo_TemplatePlugin"
PROMPT_SRC = SDK / "AI编写插件提示词.md"

RELEASE_PROMPT_NAME = "AI编写插件提示词.md"
RELEASE_README_NAME = "README.md"

# 成品区子目录：把「一堆平铺文件」按用途分成三层，一眼能看出哪个要动、哪个别动
RELEASE_TEMPLATE_SUB = "template"       # 插件模板：整目录复制即是一个可直接打包的插件
RELEASE_TOOLS_SUB = "tools"             # 打包工具 + C++ 空壳（都是「打包地基」，归一起）

# (源文件, 包内相对路径) —— 原样拷贝；包内路径可带子目录，拷贝时自动建父目录
RELEASE_FILES = [
    (SHELL_DLL, RELEASE_TOOLS_SUB + "/XiaoLiZiVM_PythonCompat.dll"),
    (TEMPLATE_DIR / "plugin_main.py", RELEASE_TEMPLATE_SUB + "/plugin_main.py"),
    (TEMPLATE_DIR / "xlz_sdk.py", RELEASE_TEMPLATE_SUB + "/xlz_sdk.py"),
    (TEMPLATE_DIR / "xlz_constants.py", RELEASE_TEMPLATE_SUB + "/xlz_constants.py"),
    (PACK_TOOL, RELEASE_TOOLS_SUB + "/pack_payload.py"),
]

# 提示词改写表：(原片段, 替换片段, 期望命中次数)
# 期望次数只是护栏：源提示词改了措辞导致对不上时，本脚本会直接报错，
# 而不是把带着开发仓库路径的提示词悄悄放进成品区。
# 注意：源提示词是 Markdown，部分行尾带两个空格（硬换行）。插入多行块时要把行尾空格
#       一并吃进 pattern 并在结尾补 \n，否则表格会与下一段粘在一起。
PROMPT_REWRITES = [
    (
        "这是一个 **小栗子 QQ 机器人框架插件的 Python SDK 和 Demo 例子**。  ",
        "这是一个 **小栗子 QQ 机器人框架插件的 Python SDK 成品包（作者工具包）**，无需编译，拿来即可写插件。\n"
        "\n"
        "**包内结构**：\n"
        "\n"
        "```\n"
        "Releases/\n"
        "├── README.md                       本文件\n"
        "├── AI编写插件提示词.md               本提示词\n"
        "├── template/                        ★ 插件模板（从这里开始）\n"
        "│   ├── plugin_main.py                 要改的就是它\n"
        "│   ├── xlz_sdk.py                     桥接层，别改\n"
        "│   └── xlz_constants.py               事件 / 状态常量，别改\n"
        "└── tools/                           打包地基\n"
        "    ├── XiaoLiZiVM_PythonCompat.dll    C++ 空壳（打包输入）\n"
        "    └── pack_payload.py                打包工具\n"
        "```\n"
        "\n"
        "| 路径 | 用途 | 要动吗 |\n"
        "|---|---|---|\n"
        "| `template/` | 可直接打包的最小插件模板目录 | 整目录复制后改名 |\n"
        "| `template/plugin_main.py` | 插件主逻辑 | **要改** |\n"
        "| `template/xlz_sdk.py` | 桥接层，`from xlz_sdk import Bridge` | 别改 |\n"
        "| `template/xlz_constants.py` | 事件 / 状态常量（13 组 96 个） | 别改 |\n"
        "| `tools/XiaoLiZiVM_PythonCompat.dll` | C++ 空壳（打包地基） | 只当打包输入 |\n"
        "| `tools/pack_payload.py` | 打包工具：把插件目录压进空壳 DLL | 命令行调用 |\n"
        "| `README.md` | 包说明、打包步骤、回调约定 | 先读它 |\n"
        "\n"
        "> `xlz_sdk.py` 与 `xlz_constants.py` 必须与 `plugin_main.py` 同级 —— `template/` 已按此摆好，"
        "**整目录复制即可**（别单挑文件）。\n",
        1,
    ),
    (
        "你可以先阅读仓库根目录下的 `README.md`，了解 **目录结构、回调约定",
        "你可以先阅读本目录下的 `README.md`，了解 **回调约定",
        1,
    ),
    (
        "- **`Demo_TemplatePlugin`**：最基础模板，包含标准回调结构和 `xlz_sdk.Bridge` 用法。",
        "- **`template/plugin_main.py`**：最基础模板，包含标准回调结构和 `xlz_sdk.Bridge` 用法。",
        1,
    ),
    (
        "- **`Demo_关键词回复`**：关键词回复示例，可参考规则匹配、配置读写、消息发送。",
        "- **`template/`**：整目录复制一份改名（如 `MyPlugin/`），就是一个可直接打包的插件。",
        1,
    ),
    (
        "- **`Demo_外部依赖演示`**：外部依赖示例（Pillow），可参考三方包目录内置与上传图片流程。",
        "- 完整示例（关键词回复、外部依赖 / Pillow、人机验证）见开发仓库 `XiaoLiZiVM_PythonSDK` 的 `PythonSDK` 目录。",
        1,
    ),
    (
        "- 使用 `PythonSDK` 的标准结构开发：",
        "- 使用本成品包的标准结构开发：",
        1,
    ),
    (
        "  - 把 `xlz_sdk.py` 与 `xlz_constants.py` 一并复制到插件目录（打包时缺一不可）",
        "  - 从 `template/` 整目录复制出插件目录（`xlz_sdk.py` 与 `xlz_constants.py` 必须与 `plugin_main.py` 同级，打包时缺一不可）",
        1,
    ),
    ("PythonSDK/tools/pack_payload.py", "tools/pack_payload.py", 3),
    ("PythonSDK/bin/Release/XiaoLiZiVM_PythonCompat.dll", "tools/XiaoLiZiVM_PythonCompat.dll", 1),
    ("PythonSDK/Demo_TemplatePlugin/", "MyPlugin/", 1),
    (
        "- 其它 API 与调用事项可参考：`CppSDK/README.md`。",
        "- 其它 API 与调用事项可参考开发仓库的 `CppSDK/README.md`。",
        1,
    ),
    (
        "请基于本仓库 `PythonSDK` 的模板和示例，实现一个符合小栗子框架规范的 Python 插件：",
        "请基于本成品包的 `template/plugin_main.py` 模板与 `template/xlz_sdk.py` / "
        "`template/xlz_constants.py`，实现一个符合小栗子框架规范的 Python 插件：",
        1,
    ),
]

# 改写后不允许残留的开发仓库路径（上面两条是「刻意保留、已改成"开发仓库"引用」的说明文字，不在此列）
PROMPT_FORBIDDEN = [
    "仓库根目录",
    "PythonSDK/tools",
    "PythonSDK/bin",
    "PythonSDK/Demo",
    "Demo_TemplatePlugin",
    "Demo_关键词回复",
    "Demo_外部依赖演示",
]

RELEASE_README_TEMPLATE = r"""# 小栗子虚拟机 · Python 插件 SDK 成品包

> **本目录由 `tools/rebuild_all.py` 自动生成，请勿手工编辑。**
> 要刷新：在 `XiaoLiZiVM_PythonSDK\` 下双击 `rebuild.bat`，或跑
> `python tools\rebuild_all.py --releases-only`。

| 项 | 值 |
|---|---|
| 生成时间 | @BUILD_TIME@ |
| 空壳 DLL | `tools/XiaoLiZiVM_PythonCompat.dll` · @DLL_SIZE@ 字节 · @DLL_TIME@ |
| 空壳 SHA-256（前 16 位） | `@DLL_SHA16@` |
| 目标平台 | Win32（32 位）· CPython 3.10 |

---

## 这是什么

一个**不需要编译**的 Python 插件作者工具包。流程只有四步：

```
复制 template/  ->  写 plugin_main.py  ->  tools/pack_payload.py 压进空壳 DLL  ->  丢进框架插件目录
```

框架本体、C++ SDK 源码、Demo 集合**都不在**这个包里，所以这个目录可以单独拷贝、压缩、发给别人。

## 包内结构

```
Releases/
├── README.md                       本文件（自动生成）
├── AI编写插件提示词.md               交给 AI 的提示词
├── template/                        ★ 插件模板 —— 从这里开始
│   ├── plugin_main.py                 要改的就是它
│   ├── xlz_sdk.py                     桥接层，原样别动
│   └── xlz_constants.py               事件 / 状态常量，原样别动
└── tools/                           打包地基
    ├── XiaoLiZiVM_PythonCompat.dll    C++ 空壳（打包输入）
    └── pack_payload.py                打包工具
```

| 路径 | 作用 | 写插件时要动吗 |
|---|---|---|
| `template/` | 一个可直接打包的最小插件**模板目录** | 整目录复制后改名 |
| `template/plugin_main.py` | 插件主逻辑（回调骨架已齐） | **要改** |
| `template/xlz_sdk.py` | 桥接层，`from xlz_sdk import Bridge` | 别动 |
| `template/xlz_constants.py` | 事件 / 状态常量（13 组 96 个） | 别动 |
| `tools/XiaoLiZiVM_PythonCompat.dll` | C++ 空壳（打包地基） | 只作为打包输入 |
| `tools/pack_payload.py` | 打包工具 | 命令行调用 |
| `AI编写插件提示词.md` | 交给 AI 的提示词 | 填好需求后整份丢给 AI |
| `README.md` | 本文件 | — |

> `xlz_sdk.py` 与 `xlz_constants.py` **必须**和 `plugin_main.py` 同级，打包时缺一不可。
> `template/` 已经按这个要求摆好了 —— 所以**别单独挑文件拷，直接整目录复制**。

## 30 秒上手

以写插件 `MyPlugin` 为例：

```bat
:: 1) 复制整个模板目录 -> 你的插件目录
xcopy /E /I template MyPlugin

:: 2) 改写 MyPlugin\plugin_main.py
::    （或者把 AI编写插件提示词.md 整份丢给 AI，让它按你的需求写）

:: 3) 打包：tools\pack_payload.py <空壳DLL> <插件目录> <输出DLL>
python tools\pack_payload.py tools\XiaoLiZiVM_PythonCompat.dll MyPlugin output\MyPlugin.dll

:: 4) 把 output\MyPlugin.dll 放进框架的插件目录，启用即可
```

## 回调约定

| 回调 | 参数 | 返回值 |
|---|---|---|
| `apprun(ctx)` | `ctx["dll_path"]` | 插件信息 dict：`app_name` / `author` / `app_version` / `description` / `permissions` |
| `on_enable()` | 无 | `EnableResponse.DONE`(0) / `EnableResponse.REJECT`(8) |
| `on_disable()` / `on_uninstall()` / `on_setting()` | 无 | `0` |
| `on_group_message(ev)` | dict | `MessageHandle.IGNORE`(0) 继续 / `MessageHandle.INTERCEPT`(1) 拦截 |
| `on_private_message(ev)` | dict | 同上 |
| `on_event_message(ev)` | dict | 同上 |

事件字典字段一律 `snake_case`：

| 字段 | 含义 |
|---|---|
| `my_uin` | 框架（机器人）自己的 QQ 号 |
| `group_uin` / `sender_uin` | 群号 / 发送者 QQ 号 |
| `card` | 发送人群名片（注意：不是 QQ 昵称） |
| `group_name` | 群名 |
| `message` | 消息文本 |
| `event_type` / `event_sub_type` | 事件类型 / 子类型（与 `xlz_constants` 的 IntEnum 同值） |
| `trigger_uin` / `operate_uin` / `source_group_uin` | 事件触发者 / 操作者 / 来源群 |

## 事件常量

`xlz_constants.py` 共 13 组、96 个常量，数值与框架内部定义严格一致（**勿改**）：

`MessageHandle`、`MessageType`、`ChannelPushType`、`GroupEvent`、`ZoneEvent`、`FrameworkEvent`、
`DiscussionGroupEvent`、`FriendEvent`、`LoginEvent`、`VirtualMachineEvent`、`SubChannelEvent`、
`ChannelEvent`、`EnableResponse`。

```python
from xlz_constants import GroupEvent, FriendEvent, MessageHandle

et = ev.get("event_type")            # 与 IntEnum 可直接比较
if et == GroupEvent.MEMBER_JOINED:
    api.output_log("有人入群")
elif et == FriendEvent.FRIEND_REQUEST:
    api.output_log("收到好友请求")
return MessageHandle.IGNORE
```

## 自由线程

运行时已释放 GIL，桥接层线程安全，**可以在任意线程里调 API**（`output_log` / `send_group_message` …）。

```python
from xlz_sdk import run_in_thread, spawn, shutdown_threads

run_in_thread(slow_task, a, b)     # 丢进线程池，返回 Future
spawn(worker, x)                   # 起一个独立 Thread
api.set_async(True)                # 事件改由 C++ 工作线程派发，回调立即返回 0
shutdown_threads(wait=False)       # on_uninstall / on_disable 里回收线程池
```

> `set_async(True)` 的代价：`on_xxx` 的返回值**不再用于拦截**后续插件。依赖抢答 / 独占的插件请保持默认同步模式。

## 外部依赖

必须是 **CPython 3.10 + win32** 的 wheel，解到插件目录里一起打包：

```bat
python.exe -m pip install pillow --only-binary=:all: --python-version 3.10 --platform win32 --target MyPlugin
```

`json` / `re` / `pathlib` / `ctypes` / `sqlite3` 等标准库随解释器提供，**不要**重复打包。

## 这个包是自动生成的

`rebuild.bat` 每次重建时都会：

1. 重编 `XiaoLiZiVM_PythonCompat.dll`（Release | Win32）
2. 校验 PE 导出表 —— 少一个导出就**不生成**本目录，避免发出坏包
3. 把 `template\`（三件套）与 `tools\`（空壳 + 打包工具）同步进来
4. 由 `PythonSDK/AI编写插件提示词.md` 自动改写生成 `AI编写插件提示词.md`（仓库路径 -> 包内相对路径）

所以这个目录不会出现「源码改了但成品还是旧的」。

---

### 本次生成的文件指纹（SHA-256）

```
@FINGERPRINTS@
```
"""


def log(msg=""):
    print(msg, flush=True)


def mtime(p):
    try:
        return datetime.datetime.fromtimestamp(p.stat().st_mtime).strftime("%Y-%m-%d %H:%M:%S")
    except OSError:
        return "-"


# ---------------------------------------------------------------- MSBuild 定位

def find_msbuild():
    """按 vswhere -> 常见安装目录 -> PATH 的顺序找 MSBuild.exe"""
    pf = os.environ.get("ProgramFiles", r"C:\Program Files")
    pf86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")

    vswhere = Path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.exists():
        try:
            r = subprocess.run(
                [str(vswhere), "-latest", "-prerelease", "-products", "*",
                 "-requires", "Microsoft.Component.MSBuild",
                 "-find", r"MSBuild\**\Bin\MSBuild.exe"],
                capture_output=True, text=True, timeout=60)
            for line in r.stdout.splitlines():
                line = line.strip()
                if line and Path(line).exists():
                    return Path(line)
        except Exception:
            pass

    hits = []
    for base in (pf, pf86):
        vs = Path(base) / "Microsoft Visual Studio"
        if not vs.is_dir():
            continue
        for pat in ("*/*/MSBuild/Current/Bin/MSBuild.exe", "*/*/MSBuild/15.0/Bin/MSBuild.exe"):
            hits.extend(sorted(vs.glob(pat)))
    if hits:
        return hits[-1]

    w = shutil.which("msbuild") or shutil.which("MSBuild.exe")
    return Path(w) if w else None


# ---------------------------------------------------------------- PE 导出表

def pe_export_names(path):
    """极简 PE 导出名解析（不依赖任何第三方库，32/64 位通吃）"""
    d = Path(path).read_bytes()
    if d[:2] != b"MZ":
        return []
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        return []
    coff = pe + 4
    nsec = struct.unpack_from("<H", d, coff + 2)[0]
    optsz = struct.unpack_from("<H", d, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", d, opt)[0]
    ddir = opt + (112 if magic == 0x20B else 96)     # 数据目录起点，导出表是第 0 项
    rva = struct.unpack_from("<I", d, ddir)[0]
    if rva == 0:
        return []

    secoff = opt + optsz
    secs = []
    for i in range(nsec):
        o = secoff + i * 40
        vsz, va, rsz, rp = struct.unpack_from("<IIII", d, o + 8)
        secs.append((va, max(vsz, rsz), rp))

    def r2o(x):
        for va, size, rp in secs:
            if va <= x < va + size:
                return rp + (x - va)
        return None

    eo = r2o(rva)
    if eo is None:
        return []
    _nf, nnam, _f, namerva = struct.unpack_from("<IIII", d, eo + 20)
    nt = r2o(namerva)
    if nt is None:
        return []
    names = []
    for i in range(nnam):
        no = r2o(struct.unpack_from("<I", d, nt + i * 4)[0])
        if no is None:
            continue
        names.append(d[no:d.index(b"\0", no)].decode("latin1"))
    return names


def def_names(def_path):
    """从 .def 里取出对外导出的干净名"""
    names = []
    for line in Path(def_path).read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line or line.upper().startswith(("LIBRARY", "EXPORTS", ";")):
            continue
        names.append(line.split("=")[0].strip())
    return names


# ---------------------------------------------------------------- Releases 成品区

def sha256_file(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()


def build_release_prompt():
    """把开发仓库版提示词改写成成品包版：仓库路径 -> 包内相对路径。

    改写表对不上就抛错 —— 宁可让脚本失败，也不要把带错路径的提示词放进成品区。
    输出是**确定性**的（不写生成时间），这样成品区的 diff 只会反映真实内容变化。
    """
    if not PROMPT_SRC.exists():
        raise RuntimeError("提示词源文件不存在: %s" % PROMPT_SRC)

    # 源文件是 CRLF；先归一到 LF 再做整片替换，避免行尾干扰命中
    text = PROMPT_SRC.read_text(encoding="utf-8", newline="").replace("\r\n", "\n")

    for i, (old, new, want) in enumerate(PROMPT_REWRITES, 1):
        got = text.count(old)
        if got != want:
            raise RuntimeError(
                "提示词改写表第 %d 条命中 %d 次（期望 %d 次）：%s\n"
                "        源文件 PythonSDK/AI编写插件提示词.md 可能已改过措辞，"
                "请同步 tools/rebuild_all.py 里的 PROMPT_REWRITES。" % (i, got, want, old[:56]))
        text = text.replace(old, new)

    left = [b for b in PROMPT_FORBIDDEN if b in text]
    if left:
        raise RuntimeError("改写后提示词仍残留开发仓库路径：%s\n"
                           "        请补进 PROMPT_REWRITES。" % "、".join(left))

    header = ("<!-- 本文件由 tools/rebuild_all.py 自动生成，请勿手工编辑。 -->\n"
              "<!-- 源文件: PythonSDK/AI编写插件提示词.md（仓库路径已改写为成品包内的相对路径） -->\n"
              "\n")
    return header + text


def _release_report(rows, build_time, dll):
    """rows: [(包内文件名, 字节数, sha256)]"""
    lines = []
    for name, size, sha in rows:
        lines.append("%-34s %9d 字节  %s" % (name, size, sha))
    return "\n".join(lines)


def assemble_releases(dry_run=False):
    """组装 Releases/ 成品区。返回 True 表示成功（或 dry-run 计划已打印）。"""
    # 安全闸：只允许操作 <SDK 根>\Releases，杜绝 Path 拼错时误删别处
    if RELEASES.name != "Releases" or RELEASES.parent.resolve() != ROOT.resolve():
        log("[错误] 拒绝操作：目标目录异常 %s" % RELEASES)
        return False

    miss = [str(src) for src, _dst in RELEASE_FILES if not src.exists()]
    if not SHELL_DLL.exists():
        miss.append(str(SHELL_DLL))
    if not PROMPT_SRC.exists():
        miss.append(str(PROMPT_SRC))
    if miss:
        log("[错误] 成品区源文件缺失，已中止：")
        for m in miss:
            log("   - %s" % m)
        return False

    build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    dll_sha = sha256_file(SHELL_DLL)
    try:
        prompt_text = build_release_prompt()
    except RuntimeError as e:
        log("[错误] 生成成品区提示词失败：")
        for line in str(e).splitlines():
            log("   " + line)
        return False

    if dry_run:
        log("[计划组装] %s" % RELEASES)
        for src, dst in RELEASE_FILES:
            log("       %-34s <- %s" % (dst, src))
        log("       %-34s <- %s（路径改写）" % (RELEASE_PROMPT_NAME, PROMPT_SRC))
        log("       %-34s <- 自动生成（含 sha256 指纹）" % RELEASE_README_NAME)
        return True

    # 先在临时目录里建好，成功了再整体替换 —— 中途失败不会留下半成品
    tmp = ROOT / (RELEASES.name + ".__tmp__")
    old = ROOT / (RELEASES.name + ".__old__")
    for d in (tmp, old):
        if d.exists():
            shutil.rmtree(d, ignore_errors=True)
    tmp.mkdir(parents=True)

    try:
        rows = []
        for src, dst in RELEASE_FILES:
            target = tmp / dst
            target.parent.mkdir(parents=True, exist_ok=True)   # 包内路径可能带子目录
            shutil.copy2(src, target)
            rows.append((dst, target.stat().st_size, sha256_file(target)))

        (tmp / RELEASE_PROMPT_NAME).write_text(prompt_text, encoding="utf-8", newline="\r\n")
        rows.append((RELEASE_PROMPT_NAME, (tmp / RELEASE_PROMPT_NAME).stat().st_size,
                     sha256_file(tmp / RELEASE_PROMPT_NAME)))

        readme = (RELEASE_README_TEMPLATE
                  .replace("@BUILD_TIME@", build_time)
                  .replace("@DLL_SIZE@", str(SHELL_DLL.stat().st_size))
                  .replace("@DLL_TIME@", mtime(SHELL_DLL))
                  .replace("@DLL_SHA16@", dll_sha[:16])
                  .replace("@FINGERPRINTS@", _release_report(rows, build_time, SHELL_DLL)))
        (tmp / RELEASE_README_NAME).write_text(readme, encoding="utf-8", newline="\r\n")

        # 整体替换：旧目录先改名，再让 tmp 顶上，最后删旧 —— 中间任何一步挂了都能回滚
        if RELEASES.exists():
            RELEASES.rename(old)
        tmp.rename(RELEASES)
        if old.exists():
            shutil.rmtree(old, ignore_errors=True)
    except Exception as e:
        if not RELEASES.exists() and old.exists():
            try:
                old.rename(RELEASES)
                log("[警告] 替换失败，已回滚为上一次的成品区。")
            except OSError:
                log("[警告] 替换失败且回滚也失败，旧成品区残留在 %s" % old)
        shutil.rmtree(tmp, ignore_errors=True)
        log("[错误] 组装成品区失败：%s" % e)
        return False

    log("[成品区] %s" % RELEASES)
    log("       共 %d 个文件（其中 README.md 自动生成）" % (len(rows) + 1))
    for name, size, sha in rows:
        log("       %-34s %9d 字节  %s…" % (name, size, sha[:16]))
    log("       %-34s %9d 字节" % (RELEASE_README_NAME, (RELEASES / RELEASE_README_NAME).stat().st_size))
    log("       空壳 mtime: %s" % mtime(SHELL_DLL))
    return True


# ---------------------------------------------------------------- 主流程

def main():
    ap = argparse.ArgumentParser(description="重建小栗子虚拟机 Python SDK 的全部 C++ 产物")
    ap.add_argument("--dry-run", action="store_true", help="只打印计划，不执行任何构建/写入")
    ap.add_argument("--skip-build", action="store_true", help="跳过 MSBuild（用现有 DLL 只做部署+打包+校验）")
    ap.add_argument("--no-deploy", action="store_true", help="不把 xlz_pyruntime.dll 拷到 main\\corn\\")
    ap.add_argument("--no-releases", action="store_true", help="不组装 Releases\\ 成品区")
    ap.add_argument("--releases-only", action="store_true",
                    help="只重新组装 Releases\\ 成品区（不编译、不部署、不重打 Demo）")
    args = ap.parse_args()

    if args.releases_only:
        args.skip_build = True
        args.no_deploy = True

    mode = ("DRY-RUN（不写文件）" if args.dry_run else
            ("仅组装成品区" if args.releases_only else
             ("SKIP-BUILD（跳过 MSBuild）" if args.skip_build else "完整重建")))

    log("=" * 74)
    log("小栗子虚拟机 Python SDK —— 重建")
    log("=" * 74)
    log("SDK 根目录 : %s" % ROOT)
    log("模式       : %s" % mode)
    log()

    # ---- 前置检查 ----
    errs = []
    if not PACK_TOOL.exists():
        errs.append("找不到打包工具: %s" % PACK_TOOL)
    for demo, _out in PACKS:
        if not (SDK / demo).is_dir():
            errs.append("找不到 Demo 目录: %s" % (SDK / demo))
    if not (ROOT / "CppSDK" / "include").is_dir():
        errs.append("找不到 CppSDK/include —— PythonSDK 依赖它，缺少会导致编译失败: %s"
                    % (ROOT / "CppSDK" / "include"))
    if errs:
        log("[错误] 前置检查未通过：")
        for e in errs:
            log("   - " + e)
        return 2

    # ---- 1/2. 编译 ----
    msbuild = None
    if not args.skip_build:
        msbuild = find_msbuild()
        if msbuild is None:
            log("[错误] 没找到 MSBuild.exe。")
            log("       请安装 Visual Studio（含“使用 C++ 的桌面开发”工作负载），")
            log("       或在 Visual Studio 里直接打开下面的 .sln 手动“生成解决方案”。")
            return 3

    before = {}
    for title, sln, out in SOLUTIONS:
        before[str(out)] = out.stat().st_mtime if out.exists() else None

        if args.skip_build:
            log("[跳过编译] %s" % title)
        elif args.dry_run:
            log("[计划编译] %s" % title)
            log("           %s %s /p:Configuration=Release /p:Platform=Win32" % (msbuild, sln))
        else:
            log("[编译] %s" % title)
            log("       %s" % sln)
            cmd = [str(msbuild), str(sln), "/p:Configuration=Release", "/p:Platform=Win32",
                   "/v:minimal", "/nologo", "/m"]
            r = subprocess.run(cmd)
            if r.returncode != 0:
                log("[错误] 编译失败（exit=%d）：%s" % (r.returncode, sln))
                return 4

        if not args.dry_run and not out.exists():
            log("[错误] 编译结束但产物不存在: %s" % out)
            return 5
        if not args.dry_run:
            log("       产物: %s   (%s)" % (out.name, mtime(out)))
    log()

    # ---- 3. 部署运行时 ----
    if args.no_deploy:
        log("[跳过部署] xlz_pyruntime.dll -> main\\corn\\")
    elif args.dry_run:
        log("[计划部署] %s" % RUNTIME_DLL)
        log("        ->  %s" % CORN_DLL)
    else:
        if not RUNTIME_DLL.exists():
            log("[错误] 运行时 DLL 不存在: %s" % RUNTIME_DLL)
            return 5
        if CORN_DIR.is_dir():
            shutil.copy2(RUNTIME_DLL, CORN_DLL)
            log("[部署] %s" % CORN_DLL)
            log("       时间: %s" % mtime(CORN_DLL))
        else:
            log("[警告] 目标目录不存在，跳过部署: %s" % CORN_DIR)
            log("       发布版的 main\\corn\\ 请自行同步。")
    log()

    # ---- 4. 重打 Demo 成品 ----
    if args.releases_only:
        log("[跳过] Demo 成品打包（仅组装成品区模式）")
    else:
        log("[打包] Demo 成品 DLL")
        for demo, outname in PACKS:
            out_dll = OUT / outname
            if args.dry_run:
                log("       [计划] %-22s -> %s" % (demo, outname))
                continue
            cmd = [sys.executable, str(PACK_TOOL), str(SHELL_DLL), str(SDK / demo), str(out_dll)]
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode != 0:
                log("[错误] 打包失败: %s" % demo)
                log((r.stdout or "") + (r.stderr or ""))
                return 6
            log("       %-22s -> %-34s (%s, %d 字节)"
                % (demo, outname, mtime(out_dll), out_dll.stat().st_size))
    log()

    # ---- 5. 导出表校验 ----
    exports_ok = True
    if not args.dry_run:
        log("[校验] PE 导出表 vs exports.def")
        checks = [
            (SHELL_DLL, SDK / "exports.def", "XLZ_Bridge_SetAsync 等新接口是否真的编进去了"),
            (RUNTIME_DLL, ROOT / "xlz_pyruntime" / "exports.def", "XLZ_PyIsInitialized 是否真的编进去了"),
        ]
        bad = 0
        for dll, def_file, why in checks:
            want = def_names(def_file)
            have = set(pe_export_names(dll))
            miss = [n for n in want if n not in have]
            if miss:
                bad += 1
                log("   [缺失] %s" % dll.name)
                for n in miss:
                    log("          - %s" % n)
            else:
                log("   [OK]   %s —— %d 个导出全部就位（%s）" % (dll.name, len(want), why))
        exports_ok = (bad == 0)
        log()

        changed = []
        for title, _sln, out in SOLUTIONS:
            if before.get(str(out)) != out.stat().st_mtime:
                changed.append(out.name)
        if not changed:
            log("[注意] 产物时间戳没有变化 —— MSBuild 认为无需重编（源码未变）或编译未真正执行。")
        log()

        log("[提示] 发布区/打包用的 main\\corn\\ 与 staging 目录需自行同步 xlz_pyruntime.dll。")
        log()

    # ---- 6. 组装 Releases 成品区 ----
    if args.no_releases:
        log("[跳过] Releases 成品区（--no-releases）")
    elif not exports_ok:
        log("[跳过] Releases 成品区：导出表校验未通过 —— 拒绝把坏包放进成品区。")
        log("       修好源码后重跑本脚本（或先跑 --releases-only 确认）。")
    else:
        log("[组装] Releases 成品区")
        if not assemble_releases(dry_run=args.dry_run):
            return 7
    log()

    log("=" * 74)
    if args.dry_run:
        log("（dry-run 结束，未写入任何文件）")
    elif not exports_ok:
        log("失败：导出表校验未通过，成品区未生成。")
    else:
        log("完成。")
    log("=" * 74)
    return 0 if exports_ok else 7


if __name__ == "__main__":
    sys.exit(main())
