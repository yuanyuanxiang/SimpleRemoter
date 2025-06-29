#pragma once
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include "Server.h"


class IOCPUDPServer : public Server {
public:
	IOCPUDPServer();
	~IOCPUDPServer();

	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) override;
	VOID Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) override;
	VOID Destroy() override;

private:
	void WorkerThread();
	void PostRecv();
	void AddCount(int n=1){
		m_locker.lock();
		m_count += n;
		m_locker.unlock();
	}
	int GetCount() {
		m_locker.lock();
		int n = m_count;
		m_locker.unlock();
		return n;
	}
private:
	int m_count = 0;
	CLocker m_locker;
	SOCKET m_socket = INVALID_SOCKET;
	HANDLE m_hIOCP = NULL;
	HANDLE m_hThread = NULL;
	bool m_running = false;

	pfnNotifyProc m_notify = nullptr;
	pfnOfflineProc m_offline = nullptr;

	struct IO_CONTEXT {
		OVERLAPPED ol;
		CONTEXT_OBJECT* pContext;
	};
};
