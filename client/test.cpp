#include <windows.h>
#include <stdio.h>
#include <iostream>

typedef void (*StopRun)();

typedef bool (*IsStoped)();

// 停止程序运行
StopRun stop = NULL;

// 是否成功停止
IsStoped bStop = NULL;

struct CONNECT_ADDRESS
{
	DWORD dwFlag;
	char  szServerIP[MAX_PATH];
	int   iPort;
}g_ConnectAddress={0x1234567,"",0};

int main()
{
	char path[_MAX_PATH], *p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, "ServerDll.dll");
	HMODULE hDll = LoadLibraryA(path);
	typedef void (*TestRun)(char* strHost,int nPort );
	TestRun run = hDll ? TestRun(GetProcAddress(hDll, "TestRun")) : NULL;
	stop = hDll ? StopRun(GetProcAddress(hDll, "StopRun")) : NULL;
	bStop = hDll ? IsStoped(GetProcAddress(hDll, "IsStoped")) : NULL;
	if (run)
	{
		char *ip = g_ConnectAddress.szServerIP;
		int &port = g_ConnectAddress.iPort;
		if (0 == strlen(ip))
		{
			strcpy(p+1, "remote.ini");
			GetPrivateProfileStringA("remote", "ip", "127.0.0.1", ip, _MAX_PATH, path);
			port = GetPrivateProfileIntA("remote", "port", 2356, path);
		}
		printf("[remote] %s:%d\n", ip, port);
		run(ip, port);
#ifdef _DEBUG
		while(1){ char ch[64]; std::cin>>ch; if (ch[0]=='q'){ break; } }
		if (stop) stop();
		while(bStop && !bStop()) Sleep(200);
#endif
	}
	return -1;
}
