#pragma once

#include <cstdint>

// 事件/状态常量（由易语言 SDK 源码 cornsdk6.0.8.e 的常量表迁移而来）
// 命名规则见 ../SDK字段命名规范.md；C++ 统一放在 namespace xlz 下。
// 数值与框架内部定义严格一致，请勿改动；行尾注释为易语言常量原名。
// 本文件由 tools/gen_sdk_constants.py 生成，勿手工编辑。

namespace xlz
{
	// 消息处理
	enum class MessageHandle : int32_t
	{
		Intercept                        =   1, // 消息处理_拦截
		Ignore                           =   0, // 消息处理_忽略
	};

	// 消息类型
	enum class MessageType : int32_t
	{
		TemporarySession                 = 141, // 消息类型_临时会话
		TemporarySessionGroup            =   0, // 消息类型_临时会话_群临时
		TemporarySessionDiscussGroup     =   1, // 消息类型_临时会话_讨论组临时
		TemporarySessionOfficialAccount  = 129, // 消息类型_临时会话_公众号
		TemporarySessionWebQQ            = 201, // 消息类型_临时会话_网页QQ咨询
		TemporarySessionFriendRequest    = 134, // 消息类型_临时会话_好友申请验证消息
		FriendNormalMessage              = 166, // 消息类型_好友通常消息
		DiscussGroupMessage              =  83, // 消息类型_讨论组消息
	};

	// 频道推送类型
	enum class ChannelPushType : int32_t
	{
		SubChannelMessage                =   0, // 频道推送类型_子频道消息
		DirectMessage                    =   1, // 频道推送类型_私信消息
		EventMessage                     =   2, // 频道推送类型_事件消息
	};

	// 群事件
	enum class GroupEvent : int32_t
	{
		SelfInvitedToGroup               =   1, // 群事件_我被邀请加入群
		MemberJoined                     =   2, // 群事件_某人加入了群
		MemberApplyJoin                  =   3, // 群事件_某人申请加群
		GroupDismissed                   =   4, // 群事件_群被解散
		MemberLeft                       =   5, // 群事件_某人退出了群
		MemberKicked                     =   6, // 群事件_某人被踢出群
		MemberMuted                      =   7, // 群事件_某人被禁言
		MemberRecalled                   =   8, // 群事件_某人撤回事件
		MemberAdminRevoked               =   9, // 群事件_某人被取消管理
		MemberAdminGranted               =  10, // 群事件_某人被赋予管理
		AllMuteEnabled                   =  11, // 群事件_开启全员禁言
		AllMuteDisabled                  =  12, // 群事件_关闭全员禁言
		AnonymousChatEnabled             =  13, // 群事件_开启匿名聊天
		AnonymousChatDisabled            =  14, // 群事件_关闭匿名聊天
		HonestTalkEnabled                =  15, // 群事件_开启坦白说
		HonestTalkDisabled               =  16, // 群事件_关闭坦白说
		TemporarySessionAllowed          =  17, // 群事件_允许群临时会话
		TemporarySessionForbidden        =  18, // 群事件_禁止群临时会话
		NewGroupChatAllowed              =  19, // 群事件_允许发起新的群聊
		NewGroupChatForbidden            =  20, // 群事件_禁止发起新的群聊
		UploadGroupFileAllowed           =  21, // 群事件_允许上传群文件
		UploadGroupFileForbidden         =  22, // 群事件_禁止上传群文件
		UploadAlbumAllowed               =  23, // 群事件_允许上传相册
		UploadAlbumForbidden             =  24, // 群事件_禁止上传相册
		MemberInvited                    =  25, // 群事件_某人被邀请入群
		MemberTitleShown                 =  26, // 群事件_展示成员群头衔
		MemberTitleHidden                =  27, // 群事件_隐藏成员群头衔
		MemberUnmuted                    =  28, // 群事件_某人被解除禁言
		GroupNameChanged                 =  32, // 群事件_群名变更
		SystemNotice                     =  33, // 群事件_系统提示
		GroupAvatarChanged               =  34, // 群事件_群头像事件
		EntryEffect                      =  35, // 群事件_入场特效
		GroupCardChanged                 =  36, // 群事件_修改群名片
		GroupTransferred                 =  37, // 群事件_群被转让
		AnonymousMuted                   =  40, // 群事件_匿名被禁言
		AnonymousUnmuted                 =  41, // 群事件_匿名被解除禁言
		JoinRequestRejected              =  42, // 群事件_某人的加群申请被拒绝
		GroupBadgeShown                  =  43, // 群事件_展示群互动标识
		GroupBadgeHidden                 =  44, // 群事件_隐藏群互动标识
		MemberLevelShown                 =  45, // 群事件_展示群成员等级
		MemberLevelHidden                =  46, // 群事件_隐藏群成员等级
	};

	// 空间事件
	enum class ZoneEvent : int32_t
	{
		RelatedToMe                      =  29, // 空间事件_与我相关
	};

	// 框架事件
	enum class FrameworkEvent : int32_t
	{
		LoginSuccess                     =  31, // 框架事件_登录成功
		LoginFailed                      =  38, // 框架事件_登录失败
		RestartingForUpdate              =  39, // 框架事件_即将重启更新自身
	};

	// 讨论组事件
	enum class DiscussionGroupEvent : int32_t
	{
		NameChanged                      = 300, // 讨论组事件_讨论组名变更
		MemberRecalled                   = 301, // 讨论组事件_某人撤回事件
		MemberInvited                    = 302, // 讨论组事件_某人被邀请入群
		MemberLeft                       = 303, // 讨论组事件_某人退出了群
		MemberKicked                     = 304, // 讨论组事件_某人被踢出群
	};

	// 好友事件
	enum class FriendEvent : int32_t
	{
		FriendRemoved                    = 100, // 好友事件_被好友删除
		SignatureChanged                 = 101, // 好友事件_签名变更
		NicknameChanged                  = 102, // 好友事件_昵称改变
		MessageRecalled                  = 103, // 好友事件_某人撤回事件
		NewFriend                        = 104, // 好友事件_有新好友
		FriendRequest                    = 105, // 好友事件_好友请求
		FriendRequestAccepted            = 106, // 好友事件_对方同意了您的好友请求
		FriendRequestRejected            = 107, // 好友事件_对方拒绝了您的好友请求
		ProfileCardLiked                 = 108, // 好友事件_资料卡点赞
		SignatureLiked                   = 109, // 好友事件_签名点赞
		SignatureReplied                 = 110, // 好友事件_签名回复
		PersonalTagLiked                 = 111, // 好友事件_个性标签点赞
		MoodReply                        = 112, // 好友事件_随心贴回复
		MoodAdded                        = 113, // 好友事件_随心贴增添
		SystemNotice                     = 114, // 好友事件_系统提示
		MoodLiked                        = 115, // 好友事件_随心贴点赞
		AnonymousQuestionAsked           = 116, // 好友事件_匿名提问_被提问
		AnonymousQuestionLiked           = 117, // 好友事件_匿名提问_被点赞
		AnonymousQuestionReplied         = 118, // 好友事件_匿名提问_被回复
		InputStatus                      = 119, // 好友事件_输入状态
	};

	// 登录事件
	enum class LoginEvent : int32_t
	{
		PcOnline                         = 200, // 登录事件_电脑上线
		PcOffline                        = 201, // 登录事件_电脑下线
		MobileOnline                     = 202, // 登录事件_移动设备上线
		MobileOffline                    = 203, // 登录事件_移动设备下线
		OtherAppLoginVerification        = 204, // 登录事件_其他应用登录验证请求
	};

	// 虚拟机事件
	enum class VirtualMachineEvent : int32_t
	{
		OneBotRawRequest                 = 205, // 虚拟机事件_OneBot原始请求
	};

	// 子频道事件
	enum class SubChannelEvent : int32_t
	{
		SystemNotice                     =   0, // 子频道事件_系统提示
		MessageReactionChanged           =   1, // 子频道事件_消息贴表情变更
		MessageRecalled                  =   2, // 子频道事件_消息被撤回
		LiveChannelStatusChanged         =   3, // 子频道事件_直播频道状态变更
	};

	// 频道事件
	enum class ChannelEvent : int32_t
	{
		UserApplyJoinChannel             =   4, // 频道事件_频道用户申请加入频道
	};

	// 启用响应
	enum class EnableResponse : int32_t
	{
		Reject                           =   8, // 启用响应_拒绝
		Done                             =   0, // 启用响应_完成
	};
}
