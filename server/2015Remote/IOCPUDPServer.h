#pragma once
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include "Server.h"


class IOCPUDPServer : public Server {
	struct IO_CONTEXT {
		OVERLAPPED ol = {};
		CONTEXT_UDP* pContext = nullptr;
		IO_CONTEXT() : ol({}), pContext(new CONTEXT_UDP) {
		}
		~IO_CONTEXT() {
			SAFE_DELETE(pContext);
		}
	};
public:
	IOCPUDPServer();
	~IOCPUDPServer();

	int GetPort() const override {
		return m_port;
	}
	UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) override;
	VOID Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) override;
	VOID Destroy() override;

private:
	void WorkerThread();
	void PostRecv();
	IO_CONTEXT* AddCount(){
		m_locker.lock();
		IO_CONTEXT* ioCtx = new IO_CONTEXT();
		ioCtx->pContext->InitMember(m_socket, this);
		m_count++;
		m_locker.unlock();
		return ioCtx;
	}
	void DelCount() {
		m_locker.lock();
		m_count--;
		m_locker.unlock();
	}
	int GetCount() {
		m_locker.lock();
		int n = m_count;
		m_locker.unlock();
		return n;
	}
private:
	int m_port = 6543;
	int m_count = 0;
	CLocker m_locker;
	SOCKET m_socket = INVALID_SOCKET;
	HANDLE m_hIOCP = NULL;
	HANDLE m_hThread = NULL;
	bool m_running = false;

	pfnNotifyProc m_notify = nullptr;
	pfnOfflineProc m_offline = nullptr;
};
