#include "IOCPUDPClient.h"


BOOL IOCPUDPClient::ConnectServer(const char* szServerIP, unsigned short uPort) {
	if (szServerIP != NULL && uPort != 0) {
		SetServerAddress(szServerIP, uPort);
	}
	m_sCurIP = m_Domain.SelectIP();
	unsigned short port = m_nHostPort;

	// ���� UDP socket
	m_sClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_sClientSocket == INVALID_SOCKET) {
		Mprintf("Failed to create UDP socket\n");
		return FALSE;
	}

	// ��ʼ����������ַ�ṹ
	memset(&m_ServerAddr, 0, sizeof(m_ServerAddr));
	m_ServerAddr.sin_family = AF_INET;
	m_ServerAddr.sin_port = htons(port);

#ifdef _WIN32
	m_ServerAddr.sin_addr.S_un.S_addr = inet_addr(m_sCurIP.c_str());
#else
	if (inet_pton(AF_INET, m_sCurIP.c_str(), &m_ServerAddr.sin_addr) <= 0) {
		Mprintf("Invalid address or address not supported\n");
		closesocket(m_sClientSocket);
		m_sClientSocket = INVALID_SOCKET;
		return FALSE;
	}
#endif

	// UDP������ connect()��Ҳ������ TCP keep-alive ���ѡ��

	// ���������̣߳������Ҫ��
	if (m_hWorkThread == NULL) {
#ifdef _WIN32
		m_hWorkThread = (HANDLE)CreateThread(NULL, 0, WorkThreadProc, (LPVOID)this, 0, NULL);
		m_bWorkThread = m_hWorkThread ? S_RUN : S_STOP;
#else
		pthread_t id = 0;
		m_hWorkThread = (HANDLE)pthread_create(&id, nullptr, (void* (*)(void*))IOCPClient::WorkThreadProc, this);
#endif
	}

	Mprintf("UDP client socket created and ready to send.\n");
	m_bConnected = TRUE;
	return TRUE;
}

int IOCPUDPClient::ReceiveData(char* buffer, int bufSize, int flags) {
	sockaddr_in fromAddr;
	int fromLen = sizeof(fromAddr);
	return recvfrom(m_sClientSocket, buffer, bufSize - 1, flags, (sockaddr*)&fromAddr, &fromLen);
}

int IOCPUDPClient::SendTo(const char* buf, int len, int flags) {
	if (len > 1200) {
		Mprintf("UDP large packet may lost: %d bytes\n", len);
	}
	return ::sendto(m_sClientSocket, buf, len, flags, (sockaddr*)&m_ServerAddr, sizeof(m_ServerAddr));
}
