// IOCPClient.h: interface for the IOCPClient class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_)
#define AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <WinSock2.h>
#include <Windows.h>
#include <MSTcpIP.h>
#include "Buffer.h"
#include "Manager.h"

#pragma comment(lib,"ws2_32.lib")

#define MAX_RECV_BUFFER  1024*32
#define MAX_SEND_BUFFER  1024*32
#define FLAG_LENGTH    5
#define HDR_LENGTH     13

enum { S_STOP = 0, S_RUN, S_END };

class IOCPClient  
{
public:
	IOCPClient(bool exit_while_disconnect = false);
	virtual ~IOCPClient();
	SOCKET   m_sClientSocket;

	BOOL	 m_bWorkThread;
	HANDLE   m_hWorkThread;
	
	BOOL ConnectServer(char* szServerIP, unsigned short uPort);
	static DWORD WINAPI WorkThreadProc(LPVOID lParam);

	VOID OnServerReceiving(char* szBuffer, ULONG ulReceivedLength);
	BOOL OnServerSending(const char* szBuffer, ULONG ulOriginalLength);
	BOOL SendWithSplit(const char* szBuffer, ULONG ulLength, ULONG ulSplitLength);

	BOOL IsRunning() const
	{
		return m_bIsRunning;
	}

	BOOL m_bIsRunning;
	BOOL m_bConnected;

	char    m_szPacketFlag[FLAG_LENGTH + 3];

	VOID setManagerCallBack(CManager* Manager);

	VOID Disconnect();
	VOID RunEventLoop(const BOOL &bCondition);
	bool IsConnected() const { return m_bConnected == TRUE; }

public:	
	CManager* m_Manager; 
	CRITICAL_SECTION m_cs;
	bool m_exit_while_disconnect;
};

#endif // !defined(AFX_IOCPCLIENT_H__C96F42A4_1868_48DF_842F_BF831653E8F9__INCLUDED_)
