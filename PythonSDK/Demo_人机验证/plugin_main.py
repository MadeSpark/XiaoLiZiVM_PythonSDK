import configparser, math, os, random, subprocess, sys
from io import BytesIO
from pathlib import Path

api = None
PLUGIN_NAME = "人机验证"
PLUGIN_VERSION = "1.0.0"
PLUGIN_AUTHOR = "MadeSpark"
PLUGIN_DESCRIPTION = "检测到有人入群时进行人机验证"
PLUGIN_PERMISSIONS = ["输出日志", "发送群消息", "上传群图片", "取插件数据目录", "撤回消息_群聊", "禁言群成员"]
TRIGGER_MESSAGE = "验证码测试"
SETTINGS_FILE_NAME = "插件设置.ini"
SETTINGS_SECTION = "插件设置"
GROUP_CONFIG_SECTION = "配置"
GROUP_SWITCH_KEY = "开关"
GROUP_SWITCH_ON = "开启"
GROUP_SWITCH_OFF = "关闭"
CAPTCHA_KEY = "验证码"
JOIN_EVENT_TYPES = {2, 25}
DEFAULT_ADMINS = "3553142133 2152256010"
DEFAULT_ENABLE_COMMAND = "开启人机验证"
DEFAULT_DISABLE_COMMAND = "关闭人机验证"
# 验证码最小值；与 CAPTCHA_MAX 一起决定生成几位数字验证码
CAPTCHA_MIN = 100000
# 验证码最大值；当前 100000~999999 表示固定生成 6 位数字验证码
CAPTCHA_MAX = 999999
# 验证码GIF宽度，数值越大图片越宽
GIF_WIDTH = 320
# 验证码GIF高度，数值越大图片越高
GIF_HEIGHT = 120
# GIF总帧数；越大动画越长、体积通常也越大
GIF_FRAME_COUNT = 18
# 每帧持续时间（毫秒）；越小播放越快
GIF_FRAME_DURATION_MS = 90
# 纵向切片总数；用于把验证码图像切成多条碎片
SLICE_COUNT = 8
# 每一帧里真正显示正确位置的切片数量；越小越难识别
REAL_SLICE_COUNT_PER_FRAME = 4
# 验证码内容距离图片边缘的内边距
GIF_PADDING = 24
# GIF背景主色，格式为 RGB 三元组
BACKGROUND_COLOR = (18, 24, 38)
# 标题等前景文字颜色，格式为 RGB 三元组
FOREGROUND_COLOR = (242, 245, 250)
# 验证码字符与干扰线使用的高亮颜色列表，会循环取用
ACCENT_COLORS = [(87,214,255),(255,170,76),(134,239,172),(255,105,180),(196,181,253)]
_PLUGIN_DIR = Path(__file__).parent
if str(_PLUGIN_DIR) not in sys.path:
    sys.path.insert(0, str(_PLUGIN_DIR))
try:
    os.add_dll_directory(str(_PLUGIN_DIR))
except Exception:
    pass

def _log(msg:str):
    if api is not None:
        api.output_log(f"[{PLUGIN_NAME}] {msg}")

def _safe_int(v, d:int=0)->int:
    try: return int(v)
    except Exception: return d

def _load_pillow():
    from PIL import Image, ImageDraw, ImageFilter, ImageFont
    return Image, ImageDraw, ImageFilter, ImageFont

def _plugin_data_dir()->Path:
    p = Path(api.get_plugin_data_directory() or ".")
    p.mkdir(parents=True, exist_ok=True)
    #_log(f"插件数据目录: {p}")
    return p

def _read_ini(path:Path)->configparser.ConfigParser:
    cfg = configparser.ConfigParser()
    if path.exists(): cfg.read(path, encoding="utf-8")
    return cfg

def _write_ini(path:Path, cfg:configparser.ConfigParser):
    with path.open("w", encoding="utf-8") as f: cfg.write(f)

def _settings_path()->Path:
    return _plugin_data_dir() / SETTINGS_FILE_NAME

def _group_ini_path(this_qq:int, group_qq:int)->Path:
    p = _plugin_data_dir() / str(this_qq)
    p.mkdir(parents=True, exist_ok=True)
    return p / f"{group_qq}.ini"

def _load_settings()->configparser.ConfigParser:
    path = _settings_path()
    cfg = _read_ini(path)
    if not cfg.has_section(SETTINGS_SECTION):
        cfg[SETTINGS_SECTION] = {
            "管理员（多个请用空格分割）": DEFAULT_ADMINS,
            "开启命令": DEFAULT_ENABLE_COMMAND,
            "关闭命令": DEFAULT_DISABLE_COMMAND,
        }
        _write_ini(path, cfg)
    return cfg

def _load_group_cfg(this_qq:int, group_qq:int)->configparser.ConfigParser:
    path = _group_ini_path(this_qq, group_qq)
    cfg = _read_ini(path)
    if not cfg.has_section(GROUP_CONFIG_SECTION):
        cfg[GROUP_CONFIG_SECTION] = {GROUP_SWITCH_KEY: GROUP_SWITCH_OFF}
        _write_ini(path, cfg)
    return cfg

def _save_group_cfg(this_qq:int, group_qq:int, cfg:configparser.ConfigParser):
    _write_ini(_group_ini_path(this_qq, group_qq), cfg)

def _is_group_enabled(this_qq:int, group_qq:int)->bool:
    return _load_group_cfg(this_qq, group_qq).get(GROUP_CONFIG_SECTION, GROUP_SWITCH_KEY, fallback=GROUP_SWITCH_OFF) == GROUP_SWITCH_ON

def _set_group_enabled(this_qq:int, group_qq:int, enabled:bool):
    cfg = _load_group_cfg(this_qq, group_qq)
    cfg[GROUP_CONFIG_SECTION][GROUP_SWITCH_KEY] = GROUP_SWITCH_ON if enabled else GROUP_SWITCH_OFF
    _save_group_cfg(this_qq, group_qq, cfg)

def _get_pending_code(this_qq:int, group_qq:int, user_qq:int)->str:
    cfg = _load_group_cfg(this_qq, group_qq)
    sec = str(user_qq)
    return cfg.get(sec, CAPTCHA_KEY, fallback="").strip() if cfg.has_section(sec) else ""

def _set_pending_code(this_qq:int, group_qq:int, user_qq:int, code:str):
    cfg = _load_group_cfg(this_qq, group_qq)
    sec = str(user_qq)
    if not cfg.has_section(sec): cfg.add_section(sec)
    cfg.set(sec, CAPTCHA_KEY, code)
    _save_group_cfg(this_qq, group_qq, cfg)

def _clear_pending_code(this_qq:int, group_qq:int, user_qq:int):
    cfg = _load_group_cfg(this_qq, group_qq)
    sec = str(user_qq)
    if cfg.has_section(sec):
        cfg.remove_section(sec)
        _save_group_cfg(this_qq, group_qq, cfg)

def _message_mention(user_qq:int)->str:
    return f"[CQ:at,qq={user_qq}]"

def _extract_this_qq(ev:dict)->int:
    return _safe_int(ev.get("this_qq"))

def _extract_group_qq(ev:dict)->int:
    return _safe_int(ev.get("group_qq") or ev.get("source_group_qq"))

def _extract_user_qq(ev:dict)->int:
    return _safe_int(ev.get("sender_qq") or ev.get("trigger_qq"))

def _extract_msg_random(ev:dict)->int:
    return _safe_int(ev.get("message_random") or ev.get("random"))

def _extract_msg_req(ev:dict)->int:
    return _safe_int(ev.get("message_req") or ev.get("req"))

def _get_toggle_commands()->tuple[str,str]:
    cfg = _load_settings()
    return (
        cfg.get(SETTINGS_SECTION, "开启命令", fallback=DEFAULT_ENABLE_COMMAND).strip() or DEFAULT_ENABLE_COMMAND,
        cfg.get(SETTINGS_SECTION, "关闭命令", fallback=DEFAULT_DISABLE_COMMAND).strip() or DEFAULT_DISABLE_COMMAND,
    )

def _is_admin(user_qq:int)->bool:
    raw = _load_settings().get(SETTINGS_SECTION, "管理员（多个请用空格分割）", fallback=DEFAULT_ADMINS)
    return str(user_qq) in {x for x in raw.split() if x}

def _generate_code()->str:
    return str(random.randint(CAPTCHA_MIN, CAPTCHA_MAX))

def _create_text_image(code:str):
    Image, ImageDraw, _, ImageFont = _load_pillow()
    img = Image.new("RGBA", (GIF_WIDTH, GIF_HEIGHT), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    try: font = ImageFont.truetype("arial.ttf", 44)
    except Exception: font = ImageFont.load_default()
    box = draw.textbbox((0,0), code, font=font)
    x = (GIF_WIDTH - (box[2]-box[0])) // 2
    y = (GIF_HEIGHT - (box[3]-box[1])) // 2 - 4
    for i, ch in enumerate(code):
        color = ACCENT_COLORS[i % len(ACCENT_COLORS)]
        cbox = draw.textbbox((0,0), ch, font=font)
        draw.text((x, y + (-4 if i % 2 == 0 else 4)), ch, fill=color, font=font)
        x += (cbox[2]-cbox[0]) + 8
    return img

def generate_fragmented_captcha_gif(code:str)->tuple[bytes,dict]:
    Image, ImageDraw, ImageFilter, _ = _load_pillow()
    rng = random.Random()
    sharp = _create_text_image(code)
    soft = sharp.filter(ImageFilter.GaussianBlur(radius=0.7))
    ghost = Image.new("RGBA", (GIF_WIDTH, GIF_HEIGHT), (8,10,16,185))
    ghost.alpha_composite(soft)
    slice_h = max(6, (GIF_HEIGHT - GIF_PADDING * 2) // SLICE_COUNT)
    slices, top = [], GIF_PADDING
    for i in range(SLICE_COUNT):
        bottom = GIF_HEIGHT - GIF_PADDING if i == SLICE_COUNT - 1 else top + slice_h
        slices.append((GIF_PADDING, top, GIF_WIDTH - GIF_PADDING, bottom))
        top = bottom
    order = list(range(SLICE_COUNT)); fake = order[:]
    rng.shuffle(order); rng.shuffle(fake)
    frames = []
    for idx in range(GIF_FRAME_COUNT):
        frame = Image.new("RGBA", (GIF_WIDTH, GIF_HEIGHT), BACKGROUND_COLOR + ())
        draw = ImageDraw.Draw(frame)
        for x in range(0, GIF_WIDTH, 24): draw.line([(x,0),(x,GIF_HEIGHT)], fill=(34,42,62), width=1)
        for y in range(0, GIF_HEIGHT, 24): draw.line([(0,y),(GIF_WIDTH,y)], fill=(34,42,62), width=1)
        for _ in range(8):
            draw.line([(rng.randint(0,GIF_WIDTH-1), rng.randint(0,GIF_HEIGHT-1)), (rng.randint(0,GIF_WIDTH-1), rng.randint(0,GIF_HEIGHT-1))], fill=rng.choice(ACCENT_COLORS), width=1)
        frame.alpha_composite(ghost)
        active = {order[(idx + off) % SLICE_COUNT] for off in range(REAL_SLICE_COUNT_PER_FRAME)}
        sx = int((idx / max(1, GIF_FRAME_COUNT - 1)) * (GIF_WIDTH + 40)) - 20
        draw.rectangle((sx - 12, 0, sx + 12, GIF_HEIGHT), fill=(255,255,255,10))
        for i, box in enumerate(slices):
            ref = box if i in active else slices[fake[i]]
            src = sharp if i in active else soft
            dx = int(math.sin(idx * 0.35 + i) * (1 if i in active else 2))
            frag = src.crop(ref)
            frame.paste(frag, (box[0] + dx, box[1]), frag)
            if i not in active: draw.rectangle(box, fill=(0,0,0,24))
        draw.rounded_rectangle((10,10,GIF_WIDTH-10,GIF_HEIGHT-10), radius=14, outline=(72,84,120), width=2)
        draw.text((18,14), "HUMAN CHECK", fill=FOREGROUND_COLOR)
        frames.append(frame.filter(ImageFilter.SMOOTH_MORE).convert("P", palette=Image.Palette.ADAPTIVE))
    out = BytesIO()
    frames[0].save(out, format="GIF", save_all=True, append_images=frames[1:], duration=GIF_FRAME_DURATION_MS, loop=0, disposal=2, optimize=False)
    return out.getvalue(), {"code": code, "slice_count": SLICE_COUNT, "real_slice_count_per_frame": REAL_SLICE_COUNT_PER_FRAME}

def _send_captcha_message(this_qq:int, group_qq:int, user_qq:int, code:str, title:str):
    gif_bytes, params = generate_fragmented_captcha_gif(code)
    image_code = api.upload_group_image(this_qq, group_qq, False, gif_bytes)
    msg = "\n".join([f"{_message_mention(user_qq)} {title}", "请发送图片中看到的验证码完成验证。", image_code])
    api.send_group_message(this_qq, group_qq, msg, False)

def _open_settings_in_notepad():
    _load_settings()
    subprocess.Popen(["notepad.exe", str(_settings_path())])

def _handle_toggle_command(ev:dict, message:str)->bool:
    this_qq, group_qq, sender_qq = _extract_this_qq(ev), _extract_group_qq(ev), _extract_user_qq(ev)
    enable_cmd, disable_cmd = _get_toggle_commands()
    if message not in {enable_cmd, disable_cmd}: return False
    if not _is_admin(sender_qq):
        api.send_group_message(this_qq, group_qq, f"{_message_mention(sender_qq)} 你不是人机验证管理员。", False)
        return True
    _set_group_enabled(this_qq, group_qq, message == enable_cmd)
    api.send_group_message(this_qq, group_qq, f"[人机验证] 当前群验证已{'开启' if message == enable_cmd else '关闭'}。", False)
    return True

def _handle_test_command(ev:dict)->bool:
    if (ev.get("message") or "").strip() != TRIGGER_MESSAGE: return False
    _send_captcha_message(_extract_this_qq(ev), _extract_group_qq(ev), _extract_user_qq(ev), _generate_code(), "动态碎帧验证码测试")
    return True

def _handle_pending_member_message(ev:dict)->bool:
    this_qq, group_qq, sender_qq = _extract_this_qq(ev), _extract_group_qq(ev), _extract_user_qq(ev)
    pending = _get_pending_code(this_qq, group_qq, sender_qq)
    if not pending: return False
    content = (ev.get("message") or "").strip()
    if content == pending:
        _clear_pending_code(this_qq, group_qq, sender_qq)
        api.send_group_message(this_qq, group_qq, f"{_message_mention(sender_qq)} 人机验证通过，欢迎入群。", False)
        return True
    new_code = _generate_code()
    _set_pending_code(this_qq, group_qq, sender_qq, new_code)
    _log(f"ev={ev}")
    rnd, req = _extract_msg_random(ev), _extract_msg_req(ev)
    api.mute_group_member(this_qq, group_qq, sender_qq, 60)
    if rnd and req: _log(f"撤回消息 rnd={rnd} req={req} 结果{api.recall_group_message(this_qq, group_qq, rnd, req)}" );
    _send_captcha_message(this_qq, group_qq, sender_qq, new_code, "验证码错误，已为你刷新新的验证码")
    return True

def _handle_join_event(ev:dict)->bool:
    _log(f"检测到事件 event={ev}")
    event_type = _safe_int(ev.get("event_type"))
    if event_type not in JOIN_EVENT_TYPES: return False
    this_qq, group_qq, user_qq = _extract_this_qq(ev), _extract_group_qq(ev), _extract_user_qq(ev)
    if not this_qq or not group_qq or not user_qq or not _is_group_enabled(this_qq, group_qq): return False
    if _get_pending_code(this_qq, group_qq, user_qq):
        _log(f"跳过重复入群验证 event_type={event_type} group={group_qq} qq={user_qq}")
        return True
    code = _generate_code()
    _set_pending_code(this_qq, group_qq, user_qq, code)
    _send_captcha_message(this_qq, group_qq, user_qq, code, "欢迎入群，请先完成验证")
    api.mute_group_member(this_qq, group_qq, user_qq, 60)
    _log(f"检测到入群 event_type={event_type} group={group_qq} qq={user_qq} operate_qq={_safe_int(ev.get('operate_qq'))} sub_type={_safe_int(ev.get('event_sub_type'))}")
    return True

def apprun(ctx:dict)->dict:
    global api
    from xlz_sdk import Bridge
    api = Bridge(ctx["dll_path"])
    return {"app_name": PLUGIN_NAME, "author": PLUGIN_AUTHOR, "app_version": PLUGIN_VERSION, "description": PLUGIN_DESCRIPTION, "permissions": PLUGIN_PERMISSIONS}

def on_enable()->int:
    _load_settings(); _log("插件已启用"); return 0

def on_disable():
    _log("插件已禁用"); return 0

def on_uninstall():
    _log("插件已卸载"); return 0

def on_setting():
    try: _open_settings_in_notepad()
    except Exception as exc: _log(f"打开设置文件失败: {exc}")
    return 0

def on_group_message(ev:dict)->int:
    if api is None: return 0
    message = (ev.get("message") or "").strip()
    if not message: return 0
    if _handle_test_command(ev) or _handle_toggle_command(ev, message) or _handle_pending_member_message(ev): return 1
    return 0

def on_private_message(ev:dict)->int:
    return 0

def on_event_message(ev:dict)->int:
    return 1 if api is not None and _handle_join_event(ev) else 0
