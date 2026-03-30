import sys
import os
from pathlib import Path

# 测试脚本：诊断 Pillow 导入问题

_plugin_dir = Path(__file__).parent
print(f"[TEST] Plugin dir: {_plugin_dir}")
print(f"[TEST] sys.path[0]: {sys.path[0] if sys.path else 'empty'}")

# 1. 检查 PIL 目录
pil_dir = _plugin_dir / "PIL"
print(f"[TEST] PIL dir exists: {pil_dir.exists()}")
if pil_dir.exists():
    pyd_files = list(pil_dir.glob("*.pyd"))
    print(f"[TEST] PYD files in PIL: {len(pyd_files)}")
    for pyd in pyd_files[:3]:
        print(f"  - {pyd.name}")

# 2. 添加到 sys.path
if str(_plugin_dir) not in sys.path:
    sys.path.insert(0, str(_plugin_dir))
print(f"[TEST] sys.path[0] after insert: {sys.path[0]}")

# 3. 设置 DLL 搜索路径
try:
    os.add_dll_directory(str(_plugin_dir))
    print(f"[TEST] add_dll_directory({_plugin_dir}): OK")
except Exception as e:
    print(f"[TEST] add_dll_directory failed: {e}")

# 4. 尝试导入 PIL
print("\n[TEST] Attempting: import PIL")
try:
    import PIL
    print(f"[TEST] PIL imported OK, version: {PIL.__version__}")
except Exception as e:
    print(f"[TEST] PIL import failed: {e}")
    sys.exit(1)

# 5. 尝试导入 _imaging
print("\n[TEST] Attempting: from PIL import _imaging")
try:
    from PIL import _imaging
    print(f"[TEST] _imaging imported OK")
except Exception as e:
    print(f"[TEST] _imaging import failed: {e}")
    print(f"[TEST] Trying direct import: import PIL._imaging")
    try:
        import PIL._imaging
        print(f"[TEST] PIL._imaging imported OK (direct)")
    except Exception as e2:
        print(f"[TEST] PIL._imaging direct import also failed: {e2}")

# 6. 尝试导入 Image
print("\n[TEST] Attempting: from PIL import Image")
try:
    from PIL import Image
    print(f"[TEST] Image imported OK")
except Exception as e:
    print(f"[TEST] Image import failed: {e}")
    print(f"[TEST] Trying: import PIL.Image")
    try:
        import PIL.Image as Image
        print(f"[TEST] PIL.Image imported OK (direct)")
    except Exception as e2:
        print(f"[TEST] PIL.Image direct import also failed: {e2}")

# 7. 尝试创建图片
print("\n[TEST] Attempting: Image.new()")
try:
    img = Image.new("RGB", (100, 100), color="white")
    print(f"[TEST] Image.new() OK, size: {img.size}")
except Exception as e:
    print(f"[TEST] Image.new() failed: {e}")

print("\n[TEST] All tests completed")
