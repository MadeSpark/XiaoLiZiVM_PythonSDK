import ctypes


class Ptr:
    """包装一个整数值，告诉 _pack 用 p: 格式传递（指针）"""
    def __init__(self, addr: int):
        self.addr = addr

    def __repr__(self):
        return f"Ptr({self.addr})"


def _is_ptr(a) -> bool:
    """跨模块安全的 Ptr 检测，用类名而不是 isinstance"""
    return type(a).__name__ == 'Ptr' and hasattr(a, 'addr')


class Bridge:
    """小栗子 Python SDK 桥接层（与 C++ API 同名/同语义）"""

    def __init__(self, dll_path: str):
        self._dll = ctypes.WinDLL(dll_path)

        # 基础桥接函数
        self._dll.XLZ_Bridge_OutputLog.argtypes = [ctypes.c_char_p]
        self._dll.XLZ_Bridge_OutputLog.restype = None

        self._dll.XLZ_Bridge_SendGroupMessage.argtypes = [
            ctypes.c_longlong,
            ctypes.c_longlong,
            ctypes.c_char_p,
            ctypes.c_int,
        ]
        self._dll.XLZ_Bridge_SendGroupMessage.restype = ctypes.c_char_p

        self._dll.XLZ_Bridge_CallApiReturnUtf8.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self._dll.XLZ_Bridge_CallApiReturnUtf8.restype = ctypes.c_char_p

        self._dll.XLZ_Bridge_CallApiReturnU32.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self._dll.XLZ_Bridge_CallApiReturnU32.restype = ctypes.c_uint32

        self._dll.XLZ_Bridge_CallApiVoid.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        self._dll.XLZ_Bridge_CallApiVoid.restype = None

        self._dll.XLZ_Bridge_ResetTempStrings.argtypes = []
        self._dll.XLZ_Bridge_ResetTempStrings.restype = None

        self._dll.XLZ_Bridge_UploadGroupImage.argtypes = [
            ctypes.c_longlong,
            ctypes.c_longlong,
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.c_int,
        ]
        self._dll.XLZ_Bridge_UploadGroupImage.restype = ctypes.c_char_p

    def _enc(self, s: str) -> bytes:
        return (s or "").encode("utf-8")

    def _pack(self, *args) -> str:
        """
        参数编码协议（与 C++ BuildPackedArgs 对齐）
        u32:1 | u64:2 | i32:-1 | i64:-2 | b:1 | s:utf8文本 | p:123456
        """
        segs = []
        for a in args:
            if isinstance(a, bool):
                segs.append(f"b:{1 if a else 0}")
            elif _is_ptr(a):
                segs.append(f"p:{a.addr}")
            elif isinstance(a, int):
                segs.append(f"i64:{a}")
            elif isinstance(a, str):
                segs.append(f"s:{a}")
            elif a is None:
                segs.append("p:0")
            else:
                raise TypeError(f"unsupported arg type: {type(a)}")
        return "|".join(segs)

    def _call_utf8(self, api_name_cn: str, *args) -> str:
        self._dll.XLZ_Bridge_ResetTempStrings()
        p = self._dll.XLZ_Bridge_CallApiReturnUtf8(self._enc(api_name_cn), self._enc(self._pack(*args)))
        return p.decode("utf-8") if p else ""

    def _call_u32(self, api_name_cn: str, *args) -> int:
        self._dll.XLZ_Bridge_ResetTempStrings()
        return int(self._dll.XLZ_Bridge_CallApiReturnU32(self._enc(api_name_cn), self._enc(self._pack(*args))))

    def _call_void(self, api_name_cn: str, *args) -> None:
        self._dll.XLZ_Bridge_ResetTempStrings()
        self._dll.XLZ_Bridge_CallApiVoid(self._enc(api_name_cn), self._enc(self._pack(*args)))

    # ===== 与 C++ wrappers 对齐：API全集（中文注释） =====

    # 输出日志, 逻辑型, 公开
    def output_log(self, message_utf8: str, text_color: int = 0, background_color: int = 16777215):
        self._dll.XLZ_Bridge_OutputLog(self._enc(message_utf8))

    # 发送好友消息, 文本型, 公开
    def send_private_message(self, this_qq: int, friend_qq: int, message_utf8: str, message_random: int = 0, message_req: int = 0) -> str:
        return self._call_utf8("发送好友消息", this_qq, friend_qq, message_utf8, message_random, message_req)

    # 发送群消息, 文本型, 公开
    def send_group_message(self, this_qq: int, group_qq: int, message_utf8: str, anonymous: bool = False) -> str:
        p = self._dll.XLZ_Bridge_SendGroupMessage(this_qq, group_qq, self._enc(message_utf8), 1 if anonymous else 0)
        return p.decode("utf-8") if p else ""

    # 取框架QQ, 文本型, 公开
    def get_framework_qq(self) -> str:
        return self._call_utf8("取框架QQ")

    # 取群列表, 整数型, 公开
    def get_group_list(self, this_qq: int, blocks_ptr: int = 0) -> int:
        return self._call_u32("取群列表", this_qq, blocks_ptr)

    # 取群成员列表, 整数型, 公开
    def get_group_member_list(self, this_qq: int, group_qq: int, blocks_ptr: int = 0) -> int:
        return self._call_u32("取群成员列表", this_qq, group_qq, blocks_ptr)

    # 发送群临时消息, 文本型, 公开
    def send_group_temporary_message(self, this_qq: int, group_id: int, other_qq: int, content_utf8: str, out_random_ptr: int = 0, out_req_ptr: int = 0) -> str:
        return self._call_utf8("发送群临时消息", this_qq, group_id, other_qq, content_utf8, out_random_ptr, out_req_ptr)

    # 发送群json消息, 文本型, 公开
    def send_group_json_message(self, this_qq: int, group_qq: int, json_utf8: str, anonymous: bool = False) -> str:
        return self._call_utf8("发送群json消息", this_qq, group_qq, json_utf8, anonymous)

    # 上传好友图片, 文本型, 公开
    def upload_friend_image(self, this_qq: int, friend_qq: int, is_flash: bool, pic_bytes_ptr: int, pic_size: int) -> str:
        return self._call_utf8("上传好友图片", this_qq, friend_qq, is_flash, Ptr(pic_bytes_ptr), pic_size)

    # 上传群图片, 文本型, 公开
    def upload_group_image(self, this_qq: int, group_qq: int, is_flash: bool, pic_bytes: bytes) -> str:
        """直接传入 bytes，C++ 层负责持有数据，避免 GC 问题"""
        c_buf = (ctypes.c_char * len(pic_bytes)).from_buffer_copy(pic_bytes)
        p = self._dll.XLZ_Bridge_UploadGroupImage(
            this_qq, group_qq, 1 if is_flash else 0,
            ctypes.cast(c_buf, ctypes.c_void_p),
            len(pic_bytes),
        )
        return p.decode("utf-8") if p else ""

    # 上传好友语音, 文本型, 公开
    def upload_friend_audio(self, this_qq: int, friend_qq: int, audio_type: int, audio_text_utf8: str, audio_bytes_ptr: int, audio_size: int) -> str:
        return self._call_utf8("上传好友语音", this_qq, friend_qq, audio_type, audio_text_utf8, Ptr(audio_bytes_ptr), audio_size)

    # 上传群语音, 文本型, 公开
    def upload_group_audio(self, this_qq: int, group_qq: int, audio_type: int, audio_text_utf8: str, audio_bytes_ptr: int, audio_size: int) -> str:
        return self._call_utf8("上传群语音", this_qq, group_qq, audio_type, audio_text_utf8, Ptr(audio_bytes_ptr), audio_size)

    # 上传群文件, 文本型, 公开
    def upload_group_file(self, this_qq: int, group_qq: int, file_path_utf8: str, folder_utf8: str = "") -> str:
        return self._call_utf8("上传群文件", this_qq, group_qq, file_path_utf8, folder_utf8)

    # 取管理层列表, 文本型, 公开
    def get_administrator_list(self, this_qq: int, group_qq: int) -> str:
        return self._call_utf8("取管理层列表", this_qq, group_qq)

    # 取群名片, 文本型, 公开
    def get_group_card(self, this_qq: int, group_qq: int, member_qq: int) -> str:
        return self._call_utf8("取群名片", this_qq, group_qq, member_qq)

    # 设置群名片, 文本型, 公开
    def set_group_card(self, this_qq: int, group_qq: int, member_qq: int, new_card_utf8: str) -> str:
        return self._call_utf8("设置群名片", this_qq, group_qq, member_qq, new_card_utf8)

    # 取昵称_从缓存, 文本型, 公开
    def get_nickname_from_cache(self, other_qq_text_utf8: str) -> str:
        return self._call_utf8("取昵称_从缓存", other_qq_text_utf8)

    # 强制取昵称, 文本型, 公开
    def get_nickname_force(self, this_qq: int, other_qq_text_utf8: str) -> str:
        return self._call_utf8("强制取昵称", this_qq, other_qq_text_utf8)

    # 取好友文件下载地址, 文本型, 公开
    def get_friend_file_download_url(self, this_qq: int, file_id_utf8: str, file_name_utf8: str) -> str:
        return self._call_utf8("取好友文件下载地址", this_qq, file_id_utf8, file_name_utf8)

    # 取图片下载地址, 文本型, 公开
    def get_image_download_url(self, image_code_utf8: str, this_qq: int, group_qq: int) -> str:
        return self._call_utf8("取图片下载地址", image_code_utf8, this_qq, group_qq)

    # 撤回消息_群聊, 逻辑型, 公开
    def recall_group_message(self, this_qq: int, group_qq: int, message_random: int, message_req: int) -> int:
        return self._call_u32("撤回消息_群聊", this_qq, group_qq, message_random, message_req)

    # 禁言群成员, 逻辑型, 公开
    def mute_group_member(self, this_qq: int, group_qq: int, member_qq: int, duration_seconds: int) -> int:
        return self._call_u32("禁言群成员", this_qq, group_qq, member_qq, duration_seconds)

    # 删除群成员, 逻辑型, 公开
    def remove_group_member(self, this_qq: int, group_qq: int, member_qq: int, refuse_next_join: bool) -> int:
        return self._call_u32("删除群成员", this_qq, group_qq, member_qq, refuse_next_join)

    # 处理好友验证事件, 公开
    def handle_friend_verification_event(self, this_qq: int, trigger_qq: int, message_seq: int, operate_type: int) -> None:
        self._call_void("处理好友验证事件", this_qq, trigger_qq, message_seq, operate_type)

    # 处理群验证事件, 公开
    def handle_group_verification_event(self, this_qq: int, source_group_qq: int, trigger_qq: int, message_seq: int, operate_type: int, event_type: int, refuse_reason_utf8: str = "") -> None:
        self._call_void("处理群验证事件", this_qq, source_group_qq, trigger_qq, message_seq, operate_type, event_type, refuse_reason_utf8)

    # 全员禁言, 逻辑型, 公开
    def mute_all(self, this_qq: int, group_qq: int, enabled: bool) -> int:
        return self._call_u32("全员禁言", this_qq, group_qq, enabled)

    # QQ点赞, 文本型, 公开
    def qq_like(self, this_qq: int, other_qq: int) -> str:
        return self._call_utf8("QQ点赞", this_qq, other_qq)

    # 群聊打卡, 文本型, 公开
    def group_check_in(self, this_qq: int, group_qq: int) -> str:
        return self._call_utf8("群聊打卡", this_qq, group_qq)

    # 分享音乐, 逻辑型, 公开
    def share_music(self, this_qq: int, target: int, music_name_utf8: str, artist_name_utf8: str, jump_url_utf8: str, cover_url_utf8: str, file_url_utf8: str = "", app_type: int = 0, share_type: int = 0) -> int:
        return self._call_u32("分享音乐", this_qq, target, music_name_utf8, artist_name_utf8, jump_url_utf8, cover_url_utf8, file_url_utf8, app_type, share_type)

    # 提取图片文字, 逻辑型, 公开
    def extract_text_from_image(self, this_qq: int, image_url_utf8: str, out_text_ptr: int = 0) -> int:
        return self._call_u32("提取图片文字", this_qq, image_url_utf8, out_text_ptr)

    # 调用指定OneBot接口, 文本型, 公开
    def call_onebot_interface(self, this_qq: int, send_data_utf8: str, no_wait: bool = False) -> str:
        return self._call_utf8("调用指定OneBot接口", this_qq, send_data_utf8, no_wait)

    # 取群成员信息, 文本型, 公开
    def get_group_member_info(self, this_qq: int, group_qq: int, other_qq: int, out_data_ptr: int = 0) -> str:
        return self._call_utf8("取群成员信息", this_qq, group_qq, other_qq, out_data_ptr)

    # 取插件数据目录, 文本型, 公开
    def get_plugin_data_directory(self) -> str:
        return self._call_utf8("取插件数据目录")

    # 重载自身, 公开
    def reload_itself(self, new_dll_path_utf8: str = "empty") -> None:
        self._call_void("重载自身", new_dll_path_utf8)

    # 取插件自身版本号, 文本型, 公开
    def get_plugin_self_version(self) -> str:
        return self._call_utf8("取插件自身版本号")

    # 取框架主窗口句柄, 整数型, 公开
    def get_framework_main_window_handle(self) -> int:
        return self._call_u32("取框架主窗口句柄")

    # 取QQ头像, 文本型, 公开
    def get_qq_avatar(self, other_qq: int, hd_original: bool = False) -> str:
        return self._call_utf8("取QQ头像", other_qq, hd_original)

    # 取插件文件名, 文本型, 公开
    def get_plugin_file_name(self) -> str:
        return self._call_utf8("取插件文件名")

    # 取框架版本, 文本型, 公开
    def get_framework_version(self) -> str:
        return self._call_utf8("取框架版本")

    # 创建网络文件路径, 文本型, 公开
    def create_network_file_path(self, this_qq: int, file_path_utf8: str) -> str:
        return self._call_utf8("创建网络文件路径", this_qq, file_path_utf8)

    # 取当前OneBot客户端类型, 文本型, 公开
    def get_current_onebot_client_type(self, this_qq: int) -> str:
        return self._call_utf8("取当前OneBot客户端类型", this_qq)
