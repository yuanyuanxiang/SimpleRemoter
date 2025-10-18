
#ifdef _WINDOWS
#include "stdafx.h"
#else
#include <windows.h>
#define Mprintf
#endif

#include "pwd_gen.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <wincrypt.h>
#include <iostream>
#include "common/commands.h"

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "bcrypt.lib")

// ִ��ϵͳ�����ȡӲ����Ϣ
std::string execCommand(const char* cmd)
{
    // ���ùܵ������ڲ�����������
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // �������ڽ�������Ĺܵ�
    HANDLE hStdOutRead, hStdOutWrite;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        Mprintf("CreatePipe failed with error: %d\n", GetLastError());
        return "ERROR";
    }

    // ����������Ϣ
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // ���ô�������
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWrite;  // ����׼����ض��򵽹ܵ�

    // ��������
    if (!CreateProcess(
            NULL,          // Ӧ�ó�������
            (LPSTR)cmd,    // ������
            NULL,          // ���̰�ȫ����
            NULL,          // �̰߳�ȫ����
            TRUE,          // �Ƿ�̳о��
            0,             // ������־
            NULL,          // ��������
            NULL,          // ��ǰĿ¼
            &si,           // ������Ϣ
            &pi            // ������Ϣ
        )) {
        Mprintf("CreateProcess failed with error: %d\n", GetLastError());
        return "ERROR";
    }

    // �ر�д��˾��
    CloseHandle(hStdOutWrite);

    // ��ȡ�������
    char buffer[128];
    std::string result = "";
    DWORD bytesRead;
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    // �رն�ȡ�˾��
    CloseHandle(hStdOutRead);

    // �ȴ��������
    WaitForSingleObject(pi.hProcess, INFINITE);

    // �رս��̺��߳̾��
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // ȥ�����з��Ϳո�
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(remove(result.begin(), result.end(), '\r'), result.end());

    // ���������������
    return result;
}

// ��ȡӲ�� ID��CPU + ���� + Ӳ�̣�
std::string getHardwareID()
{
    std::string cpuID = execCommand("wmic cpu get processorid");
    std::string boardID = execCommand("wmic baseboard get serialnumber");
    std::string diskID = execCommand("wmic diskdrive get serialnumber");

    std::string combinedID = cpuID + "|" + boardID + "|" + diskID;
    return combinedID;
}

// ʹ�� SHA-256 �����ϣ
std::string hashSHA256(const std::string& data)
{
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
    BYTE hash[32];
    DWORD hashLen = 32;
    std::ostringstream result;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {

        CryptHashData(hHash, (BYTE*)data.c_str(), data.length(), 0);
        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

        for (DWORD i = 0; i < hashLen; i++) {
            result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
    }
    return result.str();
}

std::string genHMAC(const std::string& pwdHash, const std::string& superPass)
{
    std::string key = hashSHA256(superPass);
    std::vector<std::string> list({ "g","h","o","s","t" });
    for (int i = 0; i < list.size(); ++i)
        key = hashSHA256(key + " - " + list.at(i));
    return hashSHA256(pwdHash + " - " + key).substr(0, 16);
}

// ���� 16 �ַ���Ψһ�豸 ID
std::string getFixedLengthID(const std::string& hash)
{
    return hash.substr(0, 4) + "-" + hash.substr(4, 4) + "-" + hash.substr(8, 4) + "-" + hash.substr(12, 4);
}

std::string deriveKey(const std::string& password, const std::string& hardwareID)
{
    return hashSHA256(password + " + " + hardwareID);
}

std::string getDeviceID()
{
    std::string hardwareID = getHardwareID();
    std::string hashedID = hashSHA256(hardwareID);
    std::string deviceID = getFixedLengthID(hashedID);
    return deviceID;
}

uint64_t SignMessage(const std::string& pwd, BYTE* msg, int len)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    BYTE hash[32]; // SHA256 = 32 bytes
    DWORD hashObjectSize = 0;
    DWORD dataLen = 0;
    PBYTE hashObject = nullptr;
    uint64_t result = 0;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return 0;

    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(DWORD), &dataLen, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    hashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hashObjectSize);
    if (!hashObject) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    if (BCryptCreateHash(hAlg, &hHash, hashObject, hashObjectSize,
                         (PUCHAR)pwd.data(), static_cast<ULONG>(pwd.size()), 0) != 0) {
        HeapFree(GetProcessHeap(), 0, hashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    if (BCryptHashData(hHash, msg, len, 0) != 0) {
        BCryptDestroyHash(hHash);
        HeapFree(GetProcessHeap(), 0, hashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    if (BCryptFinishHash(hHash, hash, sizeof(hash), 0) != 0) {
        BCryptDestroyHash(hHash);
        HeapFree(GetProcessHeap(), 0, hashObject);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return 0;
    }

    memcpy(&result, hash, sizeof(result));

    BCryptDestroyHash(hHash);
    HeapFree(GetProcessHeap(), 0, hashObject);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return result;
}

bool VerifyMessage(const std::string& pwd, BYTE* msg, int len, uint64_t signature)
{
    uint64_t computed = SignMessage(pwd, msg, len);
    return computed == signature;
}
