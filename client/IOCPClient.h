// IOCPClient.h: interface for the IOCPClient class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_)
#define AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef _WIN32
#include "stdafx.h"
#include <WinSock2.h>
#include <MSTcpIP.h>
#pragma comment(lib,"ws2_32.lib")
#endif

#include "Buffer.h"
#include "common/commands.h"
#include "zstd/zstd.h"
#include "domain_pool.h"

#define MAX_RECV_BUFFER  1024*32
#define MAX_SEND_BUFFER  1024*32
#define FLAG_LENGTH    5
#define HDR_LENGTH     13

enum { S_STOP = 0, S_RUN, S_END };

typedef int (*DataProcessCB)(void* userData, PBYTE szBuffer, ULONG ulLength);

class IOCPManager {
public:
	virtual ~IOCPManager() {}
	virtual BOOL IsAlive() const { return TRUE; }
	virtual BOOL IsReady() const { return TRUE; }
	virtual VOID OnReceive(PBYTE szBuffer, ULONG ulLength) { }

	static int DataProcess(void* user, PBYTE szBuffer, ULONG ulLength) {
		IOCPManager* m_Manager = (IOCPManager*)user;
		if (nullptr == m_Manager) {
			Mprintf("IOCPManager DataProcess on NULL ptr: %d\n", unsigned(szBuffer[0]));
			return FALSE;
		}
		// �ȴ�����׼���������ܴ�������, 1���㹻��
		int i = 0;
		for (; i < 1000 && !m_Manager->IsReady(); ++i)
			Sleep(1);
		if (!m_Manager->IsReady()) {
			Mprintf("IOCPManager DataProcess is NOT ready: %d\n", unsigned(szBuffer[0]));
			return FALSE;
		}
		if (i) {
			Mprintf("IOCPManager DataProcess wait for %dms: %d\n", i, unsigned(szBuffer[0]));
		}
		m_Manager->OnReceive(szBuffer, ulLength);
		return TRUE;
	}
};

class IOCPClient  
{
public:
	IOCPClient(State& bExit, bool exit_while_disconnect = false);
	virtual ~IOCPClient();
	SOCKET   m_sClientSocket;
	CBuffer	 m_CompressedBuffer;
	BOOL	 m_bWorkThread;
	HANDLE   m_hWorkThread;
#if USING_CTX
	ZSTD_CCtx* m_Cctx; // ѹ��������
	ZSTD_DCtx* m_Dctx; // ��ѹ������
#endif
	int SendLoginInfo(const LOGIN_INFOR& logInfo) {
		LOGIN_INFOR tmp = logInfo;
		int iRet = Send2Server((char*)&tmp, sizeof(LOGIN_INFOR));

		return iRet;
	}
	BOOL ConnectServer(const char* szServerIP, unsigned short uPort);
	static DWORD WINAPI WorkThreadProc(LPVOID lParam);
	BOOL Send2Server(const char* szBuffer, ULONG ulOriginalLength) {
		return OnServerSending(szBuffer, ulOriginalLength);
	}
	VOID OnServerReceiving(char* szBuffer, ULONG ulReceivedLength);
	BOOL OnServerSending(const char* szBuffer, ULONG ulOriginalLength);
	BOOL SendWithSplit(const char* szBuffer, ULONG ulLength, ULONG ulSplitLength);

	void SetServerAddress(const char* szServerIP, unsigned short uPort) {
		m_Domain = szServerIP ? szServerIP : "127.0.0.1";
		m_nHostPort = uPort;
	}

	BOOL IsRunning() const
	{
		return m_bIsRunning;
	}

	void SetExit() {
		m_bIsRunning = FALSE;
	}

	BOOL m_bIsRunning;
	BOOL m_bConnected;

	char    m_szPacketFlag[FLAG_LENGTH + 3];

	VOID setManagerCallBack(void* Manager, DataProcessCB dataProcess);

	VOID Disconnect();
	VOID RunEventLoop(const BOOL &bCondition);
	bool IsConnected() const { return m_bConnected == TRUE; }
	BOOL Reconnect(void* manager) {
		Disconnect();
		if (manager) m_Manager = manager;
		return ConnectServer(NULL, 0);
	}
public:	
	State& g_bExit;					// ȫ��״̬��
	void* m_Manager;				// �û�����
	DataProcessCB m_DataProcess;	// �����û�����
	DomainPool m_Domain;
	std::string m_sCurIP;
	int m_nHostPort;
	bool m_exit_while_disconnect;
};

#endif // !defined(AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_)
