#pragma once

#include "Common.h"
#include "IOCPClient.h"
#include <IOSTREAM>
#include "LoginServer.h"
#include "KernelManager.h"
#include <iosfwd>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <shellapi.h>
#include <corecrt_io.h>
#include "domain_pool.h"

BOOL IsProcessExit();

typedef BOOL(*IsRunning)(void* thisApp);

BOOL IsSharedRunning(void* thisApp);

BOOL IsClientAppRunning(void* thisApp);

// �ͻ����ࣺ��ȫ�ֱ��������һ��.
typedef struct ClientApp
{
	State			g_bExit;			// Ӧ�ó���״̬��1-���ض��˳� 2-���ض��˳� 3-����������
	BOOL			g_bThreadExit;		// �����߳�״̬
	HINSTANCE		g_hInstance;		// ���̾��
	CONNECT_ADDRESS* g_Connection;		// ������Ϣ
	HANDLE			g_hEvent;			// ȫ���¼�
	BOOL			m_bShared;			// �Ƿ����
	IsRunning		m_bIsRunning;		// ����״̬
	unsigned		m_ID;				// Ψһ��ʶ
	static int		m_nCount;			// ������
	static CLock	m_Locker;
	ClientApp(CONNECT_ADDRESS*conn, IsRunning run, BOOL shared=FALSE) {
		memset(this, 0, sizeof(ClientApp));
		g_Connection = new CONNECT_ADDRESS(*conn);
		m_bIsRunning = run;
		m_bShared = shared;
		g_bThreadExit = TRUE;
	}
	std::vector<std::string> GetSharedMasterList() {
		DomainPool pool = g_Connection->ServerIP();
		auto list = pool.GetIPList();
		return list;
	}
	~ClientApp() {
		SAFE_DELETE(g_Connection);
	}
	ClientApp* SetID(unsigned id) {
		m_ID = id;
		return this;
	}
	static void AddCount(int n=1) {
		m_Locker.Lock();
		m_nCount+=n;
		m_Locker.Unlock();
	}
	static int GetCount() {
		m_Locker.Lock();
		int n = m_nCount;
		m_Locker.Unlock();
		return n;
	}
	static void Wait() {
		while (GetCount())
			Sleep(50);
	}
	bool IsThreadRun() {
		m_Locker.Lock();
		BOOL n = g_bThreadExit;
		m_Locker.Unlock();
		return FALSE == n;
	}
	void SetThreadRun(BOOL run = TRUE) {
		m_Locker.Lock();
		g_bThreadExit = !run;
		m_Locker.Unlock();
	}
	void SetProcessState(State state = S_CLIENT_NORMAL) {
		m_Locker.Lock();
		g_bExit = state;
		m_Locker.Unlock();
	}
}ClientApp;

ClientApp* NewClientStartArg(const char* remoteAddr, IsRunning run = IsClientAppRunning, BOOL shared=FALSE);

// ���������̣߳�����Ϊ��ClientApp
DWORD WINAPI StartClient(LPVOID lParam);

// ���������̣߳�����Ϊ��ClientApp
DWORD WINAPI StartClientApp(LPVOID param);
