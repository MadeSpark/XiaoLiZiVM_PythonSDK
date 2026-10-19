# -*- coding: utf-8 -*-
"""静态校验：xlz_webui.cpp 里所有 GetProcAddress/MustProc 用到的导出名
必须全部出现在 gen_fwd.def（MbKit146.dll 权威导出清单）里 —— 零拼错容忍。
用法：python check_webui_exports.py <xlz_webui.cpp路径> <gen_fwd.def路径>
两个参数都可省略：cpp 会按常见位置探测，gen_fwd.def 依次探测环境变量
MBKIT_DEF、CppSDK\gen_fwd.def、当前目录 gen_fwd.def（该清单由发布方提供，不在仓库内）。
"""
import io
import os
import re
import sys
from pathlib import Path

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# gen_fwd.def 是 MbKit146.dll 的权威导出清单，属发布方内部资料，不随本仓库分发。
# 无参调用时按顺序探测：环境变量 MBKIT_DEF -> 仓库内 gen_fwd.def -> 当前目录 gen_fwd.def。
DEF_ENV = "MBKIT_DEF"
DEF_CANDIDATES = (
    Path(__file__).resolve().parents[1] / "gen_fwd.def",
    Path.cwd() / "gen_fwd.def",
)


def default_def() -> Path:
    """返回探测到的 gen_fwd.def 路径；探测不到时返回一个不存在的占位路径。"""
    env = os.environ.get(DEF_ENV, "").strip()
    if env:
        return Path(env)
    for cand in DEF_CANDIDATES:
        if cand.is_file():
            return cand
    return DEF_CANDIDATES[0]


def locate(kind: str) -> Path:
    # kind: "cpp" or "def"；无参调用时在常见位置探测
    if kind == "cpp":
        for base in (Path.cwd(), Path.cwd().parent, Path.cwd().parent.parent):
            cand = base / "src" / "xlz_webui.cpp"
            if cand.exists():
                return cand
    return None


def main(argv) -> int:
    if len(argv) >= 3:
        cpp = Path(argv[1])
        def_path = Path(argv[2])
    else:
        cpp = locate("cpp")
        def_path = default_def()
    if cpp is None or not cpp.exists():
        print("❌ 找不到 xlz_webui.cpp（请显式传参：check_webui_exports.py <xlz_webui.cpp> <gen_fwd.def>）")
        return 1
    if not def_path.exists():
        print("❌ 找不到 gen_fwd.def（MbKit146.dll 的权威导出清单，不在本仓库内）")
        print(f"   探测位置: {def_path}")
        print(f"   请作为第二个参数传入，或设置环境变量 {DEF_ENV}=<gen_fwd.def 的绝对路径>")
        return 1

    # MustProc("名字" / GetProcAddress(h, "名字")
    cpp_text = cpp.read_text(encoding="utf-8")
    used = set(re.findall(r'MustProc\(\s*"([A-Za-z0-9_]+)"', cpp_text))
    used |= set(re.findall(r'GetProcAddress\w*\([^,]+,\s*"([A-Za-z0-9_]+)"', cpp_text))

    def_text = def_path.read_text(encoding="utf-8", errors="replace")
    exported = set()
    for line in def_text.splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line.startswith("LIBRARY"):
            continue
        # gen_fwd.def 每行形如 "  MbKit_Init" 或 "  wkeLoadURLW @123"
        name = line.split()[0]
        if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name):
            exported.add(name)

    missing = sorted(used - exported)
    print(f"cpp 引用导出名: {len(used)} 个")
    print(f"gen_fwd.def 导出: {len(exported)} 个")
    if missing:
        print("❌ 以下名字不在 gen_fwd.def（拼错或未导出）:")
        for m in missing:
            print(f"   {m}")
        return 1
    print("✅ 全部匹配（零拼错）")
    for n in sorted(used):
        print(f"   {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
