import configparser, math, os, random, subprocess, sys
from io import BytesIO
from pathlib import Path

api = None
PLUGIN_NAME = "人机验证"
PLUGIN_VERSION = "1.0.0"
PLUGIN_AUTHOR = "MadeSpark"
PLUGIN_DESCRIPTION = "检测到有人入群时进行人机验证\n开源地址：https://github.com/MadeSpark/XiaoLiZiVM_PythonSDK"
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
SLICE_COUNT = 7
# 每一帧里真正显示正确位置的切片数量；越小越难识别
REAL_SLICE_COUNT_PER_FRAME = 2
# 验证码内容距离图片边缘的内边距
GIF_PADDING = 24
# GIF背景主色，格式为 RGB 三元组
BACKGROUND_COLOR = (18, 24, 38)
# 标题等前景文字颜色，格式为 RGB 三元组
FOREGROUND_COLOR = (242, 245, 250)
# 验证码字符与干扰线使用的高亮颜色列表，会循环取用
ACCENT_COLORS = [(87,214,255),(255,170,76),(134,239,172),(255,105,180),(196,181,253)]
# 验证码字体候选列表；会尽量随机使用至少 3 种不同字体
CAPTCHA_FONT_CANDIDATES = [
    "arial.ttf", "arialbd.ttf", "times.ttf", "timesbd.ttf",
    "simhei.ttf", "msyh.ttc", "simsun.ttc", "consola.ttf",
]
# 验证码字号范围
CAPTCHA_FONT_MAX_SIZE = 100
CAPTCHA_FONT_MIN_SIZE = 50
# 干扰数字字号范围
NOISE_DIGIT_MAX_SIZE = 25
NOISE_DIGIT_MIN_SIZE = 10
# 干扰数字数量
NOISE_DIGIT_COUNT = 180
# 单个字符最大旋转角度，左右旋转均不超过该值
CAPTCHA_ROTATE_MAX_DEGREES = 60
# 字符间距
CAPTCHA_CHAR_SPACING = 1
# 单字符绘制缓冲边距，避免旋转后被裁切
CAPTCHA_CHAR_PADDING = 16
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

    def _load_font(font_name:str, font_size:int):
        try:
            return ImageFont.truetype(font_name, font_size)
        except Exception:
            return None

    def _load_any_font(font_size:int):
        random_names = CAPTCHA_FONT_CANDIDATES[:]
        random.shuffle(random_names)
        for font_name in random_names:
            font = _load_font(font_name, font_size)
            if font is not None:
                return font_name, font
        return "default", ImageFont.load_default()

    def _pick_font_specs(base_font_size:int, needed_count:int):
        distinct_font_count = min(len(CAPTCHA_FONT_CANDIDATES), max(3, min(needed_count, len(CAPTCHA_FONT_CANDIDATES))))
        chosen_font_names = random.sample(CAPTCHA_FONT_CANDIDATES, distinct_font_count) if CAPTCHA_FONT_CANDIDATES else []
        specs = []
        for i in range(needed_count):
            font_name = chosen_font_names[i % len(chosen_font_names)] if chosen_font_names else ""
            size_jitter = random.randint(-6, 6)
            font_size = max(CAPTCHA_FONT_MIN_SIZE, min(CAPTCHA_FONT_MAX_SIZE, base_font_size + size_jitter))
            specs.append((font_name, font_size))
        random.shuffle(specs)
        return specs

    def _measure_layout(font_specs:list[tuple[str, int]]):
        probe = Image.new("RGBA", (1, 1), (0, 0, 0, 0))
        probe_draw = ImageDraw.Draw(probe)
        char_images = []
        total_width = 0
        max_height = 0
        for i, ch in enumerate(code):
            font_name, font_size = font_specs[i]
            font = _load_font(font_name, font_size) if font_name else None
            if font is None:
                _, font = _load_any_font(font_size)
            color = random.choice(ACCENT_COLORS)
            cbox = probe_draw.textbbox((0, 0), ch, font=font)
            char_w = max(1, cbox[2] - cbox[0])
            char_h = max(1, cbox[3] - cbox[1])
            pad = CAPTCHA_CHAR_PADDING
            char_layer = Image.new("RGBA", (char_w + pad * 2, char_h + pad * 2), (0, 0, 0, 0))
            char_draw = ImageDraw.Draw(char_layer)
            char_draw.text((pad - cbox[0], pad - cbox[1]), ch, fill=color, font=font)
            angle = random.uniform(-CAPTCHA_ROTATE_MAX_DEGREES, CAPTCHA_ROTATE_MAX_DEGREES)
            rotated = char_layer.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
            char_images.append(rotated)
            total_width += rotated.size[0]
            if i != len(code) - 1:
                total_width += CAPTCHA_CHAR_SPACING
            max_height = max(max_height, rotated.size[1])
        return char_images, total_width, max_height

    def _draw_noise_digits(img, protected_boxes:list[tuple[int, int, int, int]]):
        probe = ImageDraw.Draw(Image.new("RGBA", (1, 1), (0, 0, 0, 0)))
        grid_step = max(12, NOISE_DIGIT_MAX_SIZE + 8)
        for cell_top in range(0, img.size[1], grid_step):
            for cell_left in range(0, img.size[0], grid_step):
                digit = str(random.randint(0, 9))
                font_size = random.randint(NOISE_DIGIT_MIN_SIZE, NOISE_DIGIT_MAX_SIZE)
                _, font = _load_any_font(font_size)
                color = random.choice(ACCENT_COLORS)
                bbox = probe.textbbox((0, 0), digit, font=font)
                text_w = max(1, bbox[2] - bbox[0])
                text_h = max(1, bbox[3] - bbox[1])
                pad = 2
                layer = Image.new("RGBA", (text_w + pad * 2, text_h + pad * 2), (0, 0, 0, 0))
                layer_draw = ImageDraw.Draw(layer)
                layer_draw.text((pad - bbox[0], pad - bbox[1]), digit, fill=color, font=font)
                angle = random.uniform(-CAPTCHA_ROTATE_MAX_DEGREES, CAPTCHA_ROTATE_MAX_DEGREES)
                rotated = layer.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
                if rotated.size[0] > grid_step or rotated.size[1] > grid_step:
                    continue
                jitter_x = random.randint(0, max(0, grid_step - rotated.size[0]))
                jitter_y = random.randint(0, max(0, grid_step - rotated.size[1]))
                slot_left = cell_left + jitter_x
                slot_top = cell_top + jitter_y
                if slot_left + rotated.size[0] > img.size[0] or slot_top + rotated.size[1] > img.size[1]:
                    continue
                img.alpha_composite(rotated, (slot_left, slot_top))

    canvas_width = GIF_WIDTH
    base_font_size = CAPTCHA_FONT_MAX_SIZE
    char_images = []
    total_width = 0
    max_height = 0
    while base_font_size >= CAPTCHA_FONT_MIN_SIZE:
        font_specs = _pick_font_specs(base_font_size, len(code))
        char_images, total_width, max_height = _measure_layout(font_specs)
        if total_width <= canvas_width - GIF_PADDING * 2 and max_height <= GIF_HEIGHT - GIF_PADDING * 2:
            break
        base_font_size -= 2
    else:
        canvas_width = max(GIF_WIDTH, total_width + GIF_PADDING * 2)

    canvas_width = max(canvas_width, total_width + GIF_PADDING * 2)
    text_img = Image.new("RGBA", (canvas_width, GIF_HEIGHT), (0,0,0,0))
    noise_img = Image.new("RGBA", (canvas_width, GIF_HEIGHT), (0,0,0,0))
    x = max(GIF_PADDING, (canvas_width - total_width) // 2)
    y = max(GIF_PADDING // 2, (GIF_HEIGHT - max_height) // 2)
    protected_boxes = []
    cursor_x = x
    for char_img in char_images:
        offset_y = max(0, min(GIF_HEIGHT - char_img.size[1], y + random.randint(-6, 6)))
        text_img.alpha_composite(char_img, (cursor_x, offset_y))
        protected_boxes.append((cursor_x, offset_y, cursor_x + char_img.size[0], offset_y + char_img.size[1]))
        cursor_x += char_img.size[0] + CAPTCHA_CHAR_SPACING
    _draw_noise_digits(noise_img, [])
    return text_img, noise_img

def generate_fragmented_captcha_gif(code:str)->tuple[bytes,dict]:
    Image, ImageDraw, ImageFilter, _ = _load_pillow()
    rng = random.Random()
    sharp, noise = _create_text_image(code)
    canvas_width, canvas_height = sharp.size
    soft = sharp.filter(ImageFilter.GaussianBlur(radius=0.7))
    ghost = Image.new("RGBA", (canvas_width, canvas_height), (8,10,16,185))
    ghost.alpha_composite(soft)
    slice_h = max(6, (canvas_height - GIF_PADDING * 2) // SLICE_COUNT)
    slices, top = [], GIF_PADDING
    for i in range(SLICE_COUNT):
        bottom = canvas_height - GIF_PADDING if i == SLICE_COUNT - 1 else top + slice_h
        slices.append((GIF_PADDING, top, canvas_width - GIF_PADDING, bottom))
        top = bottom
    order = list(range(SLICE_COUNT)); fake = order[:]
    rng.shuffle(order); rng.shuffle(fake)
    frames = []
    for idx in range(GIF_FRAME_COUNT):
        frame = Image.new("RGBA", (canvas_width, canvas_height), BACKGROUND_COLOR + ())
        draw = ImageDraw.Draw(frame)
        for x in range(0, canvas_width, 24): draw.line([(x,0),(x,canvas_height)], fill=(34,42,62), width=1)
        for y in range(0, canvas_height, 24): draw.line([(0,y),(canvas_width,y)], fill=(34,42,62), width=1)
        frame.alpha_composite(noise)
        frame.alpha_composite(ghost)
        active = {order[(idx + off) % SLICE_COUNT] for off in range(REAL_SLICE_COUNT_PER_FRAME)}
        sx = int((idx / max(1, GIF_FRAME_COUNT - 1)) * (canvas_width + 40)) - 20
        draw.rectangle((sx - 12, 0, sx + 12, canvas_height), fill=(255,255,255,10))
        for i, box in enumerate(slices):
            ref = box if i in active else slices[fake[i]]
            src = sharp if i in active else soft
            dx = int(math.sin(idx * 0.35 + i) * (1 if i in active else 2))
            frag = src.crop(ref)
            frame.paste(frag, (box[0] + dx, box[1]), frag)
            if i not in active: draw.rectangle(box, fill=(0,0,0,24))
        for _ in range(8):
            draw.line([(rng.randint(0,canvas_width-1), rng.randint(0,canvas_height-1)), (rng.randint(0,canvas_width-1), rng.randint(0,canvas_height-1))], fill=rng.choice(ACCENT_COLORS), width=1)
        draw.rounded_rectangle((10,10,canvas_width-10,canvas_height-10), radius=14, outline=(72,84,120), width=2)
        draw.text((18,14), "HUMAN CHECK", fill=FOREGROUND_COLOR)
        frames.append(frame.filter(ImageFilter.SMOOTH_MORE).convert("P", palette=Image.Palette.ADAPTIVE))
    out = BytesIO()
    frames[0].save(out, format="GIF", save_all=True, append_images=frames[1:], duration=GIF_FRAME_DURATION_MS, loop=0, disposal=2, optimize=False)
    return out.getvalue(), {"code": code, "slice_count": SLICE_COUNT, "real_slice_count_per_frame": REAL_SLICE_COUNT_PER_FRAME, "canvas_width": canvas_width, "canvas_height": canvas_height}

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
    #api.mute_group_member(this_qq, group_qq, user_qq, 60)
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
