# -*- coding: utf-8 -*-
"""
事件/状态常量（由易语言 SDK 源码 cornsdk6.0.8.e 的常量表迁移而来）。
命名规则见 开发SDK/SDK字段命名规范.md；数值与框架内部定义严格一致，请勿改动。
本文件由 tools/gen_sdk_constants.py 生成，勿手工编辑。

用法：
    from xlz_constants import GroupEvent, MessageHandle
    if ev.get('event_type') == GroupEvent.MEMBER_JOINED: ...
    return MessageHandle.IGNORE
"""
from enum import IntEnum

__all__ = [
    "MessageHandle",
    "MessageType",
    "ChannelPushType",
    "GroupEvent",
    "ZoneEvent",
    "FrameworkEvent",
    "DiscussionGroupEvent",
    "FriendEvent",
    "LoginEvent",
    "VirtualMachineEvent",
    "SubChannelEvent",
    "ChannelEvent",
    "EnableResponse",
]


class MessageHandle(IntEnum):
    """消息处理"""
    INTERCEPT                          =   1 # 消息处理_拦截
    IGNORE                             =   0 # 消息处理_忽略


class MessageType(IntEnum):
    """消息类型"""
    TEMPORARY_SESSION                  = 141 # 消息类型_临时会话
    TEMPORARY_SESSION_GROUP            =   0 # 消息类型_临时会话_群临时
    TEMPORARY_SESSION_DISCUSS_GROUP    =   1 # 消息类型_临时会话_讨论组临时
    TEMPORARY_SESSION_OFFICIAL_ACCOUNT = 129 # 消息类型_临时会话_公众号
    TEMPORARY_SESSION_WEB_QQ           = 201 # 消息类型_临时会话_网页QQ咨询
    TEMPORARY_SESSION_FRIEND_REQUEST   = 134 # 消息类型_临时会话_好友申请验证消息
    FRIEND_NORMAL_MESSAGE              = 166 # 消息类型_好友通常消息
    DISCUSS_GROUP_MESSAGE              =  83 # 消息类型_讨论组消息


class ChannelPushType(IntEnum):
    """频道推送类型"""
    SUB_CHANNEL_MESSAGE                =   0 # 频道推送类型_子频道消息
    DIRECT_MESSAGE                     =   1 # 频道推送类型_私信消息
    EVENT_MESSAGE                      =   2 # 频道推送类型_事件消息


class GroupEvent(IntEnum):
    """群事件"""
    SELF_INVITED_TO_GROUP              =   1 # 群事件_我被邀请加入群
    MEMBER_JOINED                      =   2 # 群事件_某人加入了群
    MEMBER_APPLY_JOIN                  =   3 # 群事件_某人申请加群
    GROUP_DISMISSED                    =   4 # 群事件_群被解散
    MEMBER_LEFT                        =   5 # 群事件_某人退出了群
    MEMBER_KICKED                      =   6 # 群事件_某人被踢出群
    MEMBER_MUTED                       =   7 # 群事件_某人被禁言
    MEMBER_RECALLED                    =   8 # 群事件_某人撤回事件
    MEMBER_ADMIN_REVOKED               =   9 # 群事件_某人被取消管理
    MEMBER_ADMIN_GRANTED               =  10 # 群事件_某人被赋予管理
    ALL_MUTE_ENABLED                   =  11 # 群事件_开启全员禁言
    ALL_MUTE_DISABLED                  =  12 # 群事件_关闭全员禁言
    ANONYMOUS_CHAT_ENABLED             =  13 # 群事件_开启匿名聊天
    ANONYMOUS_CHAT_DISABLED            =  14 # 群事件_关闭匿名聊天
    HONEST_TALK_ENABLED                =  15 # 群事件_开启坦白说
    HONEST_TALK_DISABLED               =  16 # 群事件_关闭坦白说
    TEMPORARY_SESSION_ALLOWED          =  17 # 群事件_允许群临时会话
    TEMPORARY_SESSION_FORBIDDEN        =  18 # 群事件_禁止群临时会话
    NEW_GROUP_CHAT_ALLOWED             =  19 # 群事件_允许发起新的群聊
    NEW_GROUP_CHAT_FORBIDDEN           =  20 # 群事件_禁止发起新的群聊
    UPLOAD_GROUP_FILE_ALLOWED          =  21 # 群事件_允许上传群文件
    UPLOAD_GROUP_FILE_FORBIDDEN        =  22 # 群事件_禁止上传群文件
    UPLOAD_ALBUM_ALLOWED               =  23 # 群事件_允许上传相册
    UPLOAD_ALBUM_FORBIDDEN             =  24 # 群事件_禁止上传相册
    MEMBER_INVITED                     =  25 # 群事件_某人被邀请入群
    MEMBER_TITLE_SHOWN                 =  26 # 群事件_展示成员群头衔
    MEMBER_TITLE_HIDDEN                =  27 # 群事件_隐藏成员群头衔
    MEMBER_UNMUTED                     =  28 # 群事件_某人被解除禁言
    GROUP_NAME_CHANGED                 =  32 # 群事件_群名变更
    SYSTEM_NOTICE                      =  33 # 群事件_系统提示
    GROUP_AVATAR_CHANGED               =  34 # 群事件_群头像事件
    ENTRY_EFFECT                       =  35 # 群事件_入场特效
    GROUP_CARD_CHANGED                 =  36 # 群事件_修改群名片
    GROUP_TRANSFERRED                  =  37 # 群事件_群被转让
    ANONYMOUS_MUTED                    =  40 # 群事件_匿名被禁言
    ANONYMOUS_UNMUTED                  =  41 # 群事件_匿名被解除禁言
    JOIN_REQUEST_REJECTED              =  42 # 群事件_某人的加群申请被拒绝
    GROUP_BADGE_SHOWN                  =  43 # 群事件_展示群互动标识
    GROUP_BADGE_HIDDEN                 =  44 # 群事件_隐藏群互动标识
    MEMBER_LEVEL_SHOWN                 =  45 # 群事件_展示群成员等级
    MEMBER_LEVEL_HIDDEN                =  46 # 群事件_隐藏群成员等级


class ZoneEvent(IntEnum):
    """空间事件"""
    RELATED_TO_ME                      =  29 # 空间事件_与我相关


class FrameworkEvent(IntEnum):
    """框架事件"""
    LOGIN_SUCCESS                      =  31 # 框架事件_登录成功
    LOGIN_FAILED                       =  38 # 框架事件_登录失败
    RESTARTING_FOR_UPDATE              =  39 # 框架事件_即将重启更新自身


class DiscussionGroupEvent(IntEnum):
    """讨论组事件"""
    NAME_CHANGED                       = 300 # 讨论组事件_讨论组名变更
    MEMBER_RECALLED                    = 301 # 讨论组事件_某人撤回事件
    MEMBER_INVITED                     = 302 # 讨论组事件_某人被邀请入群
    MEMBER_LEFT                        = 303 # 讨论组事件_某人退出了群
    MEMBER_KICKED                      = 304 # 讨论组事件_某人被踢出群


class FriendEvent(IntEnum):
    """好友事件"""
    FRIEND_REMOVED                     = 100 # 好友事件_被好友删除
    SIGNATURE_CHANGED                  = 101 # 好友事件_签名变更
    NICKNAME_CHANGED                   = 102 # 好友事件_昵称改变
    MESSAGE_RECALLED                   = 103 # 好友事件_某人撤回事件
    NEW_FRIEND                         = 104 # 好友事件_有新好友
    FRIEND_REQUEST                     = 105 # 好友事件_好友请求
    FRIEND_REQUEST_ACCEPTED            = 106 # 好友事件_对方同意了您的好友请求
    FRIEND_REQUEST_REJECTED            = 107 # 好友事件_对方拒绝了您的好友请求
    PROFILE_CARD_LIKED                 = 108 # 好友事件_资料卡点赞
    SIGNATURE_LIKED                    = 109 # 好友事件_签名点赞
    SIGNATURE_REPLIED                  = 110 # 好友事件_签名回复
    PERSONAL_TAG_LIKED                 = 111 # 好友事件_个性标签点赞
    MOOD_REPLY                         = 112 # 好友事件_随心贴回复
    MOOD_ADDED                         = 113 # 好友事件_随心贴增添
    SYSTEM_NOTICE                      = 114 # 好友事件_系统提示
    MOOD_LIKED                         = 115 # 好友事件_随心贴点赞
    ANONYMOUS_QUESTION_ASKED           = 116 # 好友事件_匿名提问_被提问
    ANONYMOUS_QUESTION_LIKED           = 117 # 好友事件_匿名提问_被点赞
    ANONYMOUS_QUESTION_REPLIED         = 118 # 好友事件_匿名提问_被回复
    INPUT_STATUS                       = 119 # 好友事件_输入状态


class LoginEvent(IntEnum):
    """登录事件"""
    PC_ONLINE                          = 200 # 登录事件_电脑上线
    PC_OFFLINE                         = 201 # 登录事件_电脑下线
    MOBILE_ONLINE                      = 202 # 登录事件_移动设备上线
    MOBILE_OFFLINE                     = 203 # 登录事件_移动设备下线
    OTHER_APP_LOGIN_VERIFICATION       = 204 # 登录事件_其他应用登录验证请求


class VirtualMachineEvent(IntEnum):
    """虚拟机事件"""
    ONE_BOT_RAW_REQUEST                = 205 # 虚拟机事件_OneBot原始请求


class SubChannelEvent(IntEnum):
    """子频道事件"""
    SYSTEM_NOTICE                      =   0 # 子频道事件_系统提示
    MESSAGE_REACTION_CHANGED           =   1 # 子频道事件_消息贴表情变更
    MESSAGE_RECALLED                   =   2 # 子频道事件_消息被撤回
    LIVE_CHANNEL_STATUS_CHANGED        =   3 # 子频道事件_直播频道状态变更


class ChannelEvent(IntEnum):
    """频道事件"""
    USER_APPLY_JOIN_CHANNEL            =   4 # 频道事件_频道用户申请加入频道


class EnableResponse(IntEnum):
    """启用响应"""
    REJECT                             =   8 # 启用响应_拒绝
    DONE                               =   0 # 启用响应_完成
