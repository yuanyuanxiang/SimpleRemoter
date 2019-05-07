#include <windows.h>
#include <stdio.h>
#include <iostream>

typedef void (*StopRun)();

typedef bool (*IsStoped)();

// 停止程序运行
StopRun stop = NULL;

// 是否成功停止
IsStoped bStop = NULL;

// 是否退出被控端
IsStoped bExit = NULL;

BOOL status = 0;

struct CONNECT_ADDRESS
{
	DWORD dwFlag;
	char  szServerIP[MAX_PATH];
	int   iPort;
}g_ConnectAddress={0x1234567,"",0};

//提升权限
void DebugPrivilege()
{
	HANDLE hToken = NULL;
	//打开当前进程的访问令牌
	int hRet = OpenProcessToken(GetCurrentProcess(),TOKEN_ALL_ACCESS,&hToken);

	if( hRet)
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		//取得描述权限的LUID
		LookupPrivilegeValue(NULL,SE_DEBUG_NAME,&tp.Privileges[0].Luid);
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		//调整访问令牌的权限
		AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(tp),NULL,NULL);

		CloseHandle(hToken);
	}
}

/** 
* @brief 设置本身开机自启动
* @param[in] *sPath 注册表的路径
* @param[in] *sNmae 注册表项名称
* @return 返回注册结果
* @details Win7 64位机器上测试结果表明，注册项在：\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
* @note 首次运行需要以管理员权限运行，才能向注册表写入开机启动项
*/
BOOL SetSelfStart(const char *sPath, const char *sNmae)
{
	DebugPrivilege();

	// 写入的注册表路径
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// 在注册表中写入启动信息
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// 判断是否成功
	if(lRet != ERROR_SUCCESS)
		return FALSE;

	lRet = RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1);

	// 关闭注册表
	RegCloseKey(hKey);

	// 判断是否成功
	return lRet == ERROR_SUCCESS;
}

BOOL CALLBACK callback(DWORD CtrlType)
{
	if (CtrlType == CTRL_CLOSE_EVENT)
	{
		status = 1;
		if(stop) stop();
		while(1==status)
			Sleep(20);
	}
	return TRUE;
}

int main(int argc, const char *argv[])
{
	if(!SetSelfStart(argv[0], "a_ghost"))
	{
		std::cout<<"设置开机自启动失败，请用管理员权限运行.\n";
	}
	status = 0;
	SetConsoleCtrlHandler(&callback, TRUE);
	char path[_MAX_PATH], *p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, "ServerDll.dll");
	HMODULE hDll = LoadLibraryA(path);
	typedef void (*TestRun)(char* strHost,int nPort);
	TestRun run = hDll ? TestRun(GetProcAddress(hDll, "TestRun")) : NULL;
	stop = hDll ? StopRun(GetProcAddress(hDll, "StopRun")) : NULL;
	bStop = hDll ? IsStoped(GetProcAddress(hDll, "IsStoped")) : NULL;
	bExit = hDll ? IsStoped(GetProcAddress(hDll, "IsExit")) : NULL;
	if (run)
	{
		char *ip = g_ConnectAddress.szServerIP;
		int &port = g_ConnectAddress.iPort;
		if (0 == strlen(ip))
		{
			strcpy(p+1, "settings.ini");
			GetPrivateProfileStringA("settings", "localIp", "yuanyuanxiang.oicp.net", ip, _MAX_PATH, path);
			port = GetPrivateProfileIntA("settings", "ghost", 19141, path);
		}
		printf("[server] %s:%d\n", ip, port);
		do 
		{
			run(ip, port);
			while(bStop && !bStop() && 0 == status)
				Sleep(20);
		} while (bExit && !bExit() && 0 == status);

		while(bStop && !bStop() && 1 == status)
			Sleep(20);
	}
	else {
		printf("加载动态链接库\"ServerDll.dll\"失败.\n");
		Sleep(3000);
	}
	status = 0;
	return -1;
}
