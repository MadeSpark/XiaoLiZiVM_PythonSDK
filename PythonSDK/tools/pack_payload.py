import argparse
import pathlib
import struct
import zipfile

MAGIC = b"XLZPYZIP"
VERSION = 1

# 打包排除项：这些东西留在插件目录里只是开发期残留，打进成品会变成死重量，
# 甚至出现「字节码是别的 Python 版本编译的」这种跨版本垃圾（永不加载，但会误导排查）。
EXCLUDE_DIRS = {"__pycache__", ".git", ".vs", ".idea", ".vscode", ".mypy_cache", ".pytest_cache"}
EXCLUDE_SUFFIXES = {".pyc", ".pyo", ".pdb", ".lib", ".exp", ".obj", ".ilk"}
EXCLUDE_NAMES = {"debug.log", "xlz_pyruntime.log", "Thumbs.db", ".DS_Store"}
EXCLUDE_PREFIXES = (".xlz_pystate_", "_xlz_ret_")


def is_excluded(rel: pathlib.PurePosixPath) -> bool:
    if any(part in EXCLUDE_DIRS for part in rel.parts[:-1]):
        return True
    if rel.name in EXCLUDE_NAMES:
        return True
    if rel.suffix.lower() in EXCLUDE_SUFFIXES:
        return True
    return any(rel.name.startswith(p) for p in EXCLUDE_PREFIXES)


def build_zip(payload_dir: pathlib.Path, out_zip: pathlib.Path) -> int:
    skipped = 0
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(payload_dir.rglob("*")):
            if not p.is_file():
                continue
            rel = pathlib.PurePosixPath(p.relative_to(payload_dir).as_posix())
            if is_excluded(rel):
                skipped += 1
                continue
            zf.write(p, str(rel))
    return skipped


def append_payload(base_dll: pathlib.Path, payload_zip: pathlib.Path, out_dll: pathlib.Path):
    base = base_dll.read_bytes()
    payload = payload_zip.read_bytes()
    offset = len(base)
    footer = MAGIC + struct.pack("<IQQI", VERSION, offset, len(payload), 0)
    out_dll.write_bytes(base + payload + footer)


def main():
    ap = argparse.ArgumentParser(
        description="把 Python 插件目录（含子目录/依赖）打进 C++空壳 DLL 尾部"
    )
    ap.add_argument("shell_dll", help="C++空壳DLL路径")
    ap.add_argument("payload_dir", help="Python插件根目录（会递归打包所有文件）")
    ap.add_argument("out_dll", help="输出DLL路径")
    args = ap.parse_args()

    base_dll = pathlib.Path(args.shell_dll).resolve()
    payload_dir = pathlib.Path(args.payload_dir).resolve()
    out_dll = pathlib.Path(args.out_dll).resolve()

    if not base_dll.exists():
        raise FileNotFoundError(f"空壳DLL不存在: {base_dll}")
    if not payload_dir.exists() or not payload_dir.is_dir():
        raise NotADirectoryError(f"Python插件目录不存在: {payload_dir}")

    out_dll.parent.mkdir(parents=True, exist_ok=True)
    tmp_zip = out_dll.with_suffix(".payload.zip")

    skipped = build_zip(payload_dir, tmp_zip)
    append_payload(base_dll, tmp_zip, out_dll)
    tmp_zip.unlink(missing_ok=True)

    print("打包完成:", out_dll)
    print("来源空壳:", base_dll)
    print("打包目录:", payload_dir)
    if skipped:
        print(f"已跳过 {skipped} 个缓存/符号/日志文件（__pycache__、*.pyc、*.pdb、日志等）")


if __name__ == "__main__":
    main()
