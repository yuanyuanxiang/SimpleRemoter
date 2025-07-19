#pragma once

#include "Server.h"


class IOCPKCPServer : public Server {
public:
	IOCPKCPServer(){}
	virtual ~IOCPKCPServer(){}

	virtual int GetPort() const override { return m_port; }
	virtual UINT StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) override;
	virtual void Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) override;
	virtual void Destroy() override;
	virtual void Disconnect(CONTEXT_OBJECT* ctx) override;

private:
	SOCKET m_socket = INVALID_SOCKET;
	HANDLE m_hIOCP = NULL;
	HANDLE m_hThread = NULL;
	bool m_running = false;
	USHORT m_port = 0;

	pfnNotifyProc m_notify = nullptr;
	pfnOfflineProc m_offline = nullptr;

	std::mutex m_contextsMutex;
	std::unordered_map<std::string, CONTEXT_OBJECT*> m_clients; // key: "IP:port"

	std::thread m_kcpUpdateThread;

	std::string MakeClientKey(const sockaddr_in& addr);

	CONTEXT_OBJECT* FindOrCreateClient(const sockaddr_in& addr, SOCKET sClientSocket);

	void WorkerThread();

	void KCPUpdateLoop();

	static IUINT32 iclock();
};
