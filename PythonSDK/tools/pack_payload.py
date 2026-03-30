import argparse
import pathlib
import struct
import zipfile

MAGIC = b"XLZPYZIP"
VERSION = 1


def build_zip(payload_dir: pathlib.Path, out_zip: pathlib.Path):
    with zipfile.ZipFile(out_zip, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in payload_dir.rglob("*"):
            if p.is_file():
                zf.write(p, p.relative_to(payload_dir).as_posix())


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

    build_zip(payload_dir, tmp_zip)
    append_payload(base_dll, tmp_zip, out_dll)
    tmp_zip.unlink(missing_ok=True)

    print("打包完成:", out_dll)
    print("来源空壳:", base_dll)
    print("打包目录:", payload_dir)


if __name__ == "__main__":
    main()
