#include "stdafx.h"
#include "IOCPUDPServer.h"
#include <thread>
#include <iostream>
#include "IOCPServer.h"

IOCPUDPServer::IOCPUDPServer() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

IOCPUDPServer::~IOCPUDPServer() {
	WSACleanup();
}

UINT IOCPUDPServer::StartServer(pfnNotifyProc NotifyProc, pfnOfflineProc OffProc, USHORT uPort) {
	if (m_running) return 1;
	m_port = uPort;
	m_notify = NotifyProc;
	m_offline = OffProc;

	m_socket = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == INVALID_SOCKET) return 2;

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(uPort);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return 3;

	m_hIOCP = CreateIoCompletionPort((HANDLE)m_socket, NULL, 0, 0);
	if (!m_hIOCP) return 4;

	m_running = true;

	// 启动工作线程
	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)+[](LPVOID param) -> DWORD {
		((IOCPUDPServer*)param)->WorkerThread();
		return 0;
		}, this, 0, NULL);

	// 提交多个初始接收
	for (int i = 0; i < 4; ++i)
		PostRecv();

	return 0; // 成功
}

void IOCPUDPServer::PostRecv() {
	if (!m_running) return;

	IO_CONTEXT* ioCtx = AddCount();
	if (ioCtx == nullptr) {
		Mprintf("IOCPUDPServer max connection number reached.\n");
		return;
	}

	CONTEXT_UDP* ctx = ioCtx->pContext;
	ctx->wsaInBuf.buf = ctx->szBuffer;
	ctx->wsaInBuf.len = sizeof(ctx->szBuffer);

	DWORD flags = 0;
	int err = WSARecvFrom(
		m_socket,
		&ctx->wsaInBuf,
		1,
		NULL,
		&flags,
		(sockaddr*)&ctx->clientAddr,
		&ctx->addrLen,
		&ioCtx->ol,
		NULL
	);

	if (err == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		DWORD err = WSAGetLastError();
		Mprintf("[IOCP] PostRecv error: %d\n", err);
		delete ioCtx;
		DelCount();
	}
}

void IOCPUDPServer::WorkerThread() {
	while (m_running) {
		DWORD bytes = 0;
		ULONG_PTR key = 0;
		LPOVERLAPPED pOverlapped = nullptr;

		BOOL ok = GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOverlapped, INFINITE);
		if (!ok) {
			DWORD err = WSAGetLastError();
			Mprintf("[IOCP] PostRecv error: %d\n", err);
			if (pOverlapped) {
				IO_CONTEXT* ioCtx = CONTAINING_RECORD(pOverlapped, IO_CONTEXT, ol);
				delete ioCtx;
				DelCount();
			}
			continue;
		}
		if (!pOverlapped) continue;

		IO_CONTEXT* ioCtx = CONTAINING_RECORD(pOverlapped, IO_CONTEXT, ol);
		CONTEXT_UDP* ctx = ioCtx->pContext;
		BOOL ret = ParseReceivedData(ctx, bytes, m_notify);
		if (999 != ret)
			ctx->Destroy();

		// 释放
		ioCtx->pContext = NULL;
		delete ioCtx;
		DelCount();

		PostRecv(); // 继续提交
	}
	CloseHandle(m_hThread);
	m_hThread = NULL;
}

VOID IOCPUDPServer::Send2Client(CONTEXT_OBJECT* ContextObject, PBYTE szBuffer, ULONG ulOriginalLength) {
	ContextObject->OutCompressedBuffer.ClearBuffer();
	if (!WriteContextData(ContextObject, szBuffer, ulOriginalLength)) 
		return;
	WSABUF buf = { 
		ContextObject->OutCompressedBuffer.GetBufferLength(), 
		(CHAR*)ContextObject->OutCompressedBuffer.GetBuffer(),
	};
	if (buf.len > 1200) {
		Mprintf("UDP large packet may lost: %d bytes\n", buf.len);
	}
	DWORD sent = 0;
	CONTEXT_UDP* ctx = (CONTEXT_UDP*)ContextObject;
	int err = WSASendTo(
		ContextObject->sClientSocket,
		&buf,
		1,
		&sent,
		0,
		(sockaddr*)&ctx->clientAddr,
		sizeof(sockaddr_in),
		NULL,
		NULL
	);
	if (err == SOCKET_ERROR) {
		DWORD err = WSAGetLastError();
		Mprintf("[IOCP] Send2Client error: %d\n", err);
	}
}

VOID IOCPUDPServer::Destroy() {
	if (m_socket != INVALID_SOCKET) {
		CancelIoEx((HANDLE)m_socket, NULL); // 取消所有IO请求
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	while (GetCount())
		Sleep(200);

	m_running = false;
	PostQueuedCompletionStatus(m_hIOCP, 0, 0, NULL); // 用于唤醒线程退出

	if (m_hThread) {
		WaitForSingleObject(m_hThread, INFINITE);
		while (m_hThread)
			Sleep(200);
	}

	if (m_hIOCP) {
		CloseHandle(m_hIOCP);
		m_hIOCP = NULL;
	}
}
