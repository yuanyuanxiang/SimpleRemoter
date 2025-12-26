
#ifdef _WINDOWS
#include "stdafx.h"
#else
#include <windows.h>
#define Mprintf
#endif

#ifndef SAFE_CLOSE_HANDLE
#define SAFE_CLOSE_HANDLE(h) do{if((h)!=NULL&&(h)!=INVALID_HANDLE_VALUE){CloseHandle(h);(h)=NULL;}}while(0)
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

// 执行系统命令，获取硬件信息
std::string execCommand(const char* cmd)
{
    // 设置管道，用于捕获命令的输出
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // 创建用于接收输出的管道
    HANDLE hStdOutRead, hStdOutWrite;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0)) {
        Mprintf("CreatePipe failed with error: %d\n", GetLastError());
        return "ERROR";
    }

    // 设置启动信息
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    // 设置窗口隐藏
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdOutWrite;  // 将标准输出重定向到管道

    // 创建进程
    if (!CreateProcess(
            NULL,          // 应用程序名称
            (LPSTR)cmd,    // 命令行
            NULL,          // 进程安全属性
            NULL,          // 线程安全属性
            TRUE,          // 是否继承句柄
            0,             // 创建标志
            NULL,          // 环境变量
            NULL,          // 当前目录
            &si,           // 启动信息
            &pi            // 进程信息
        )) {
        Mprintf("CreateProcess failed with error: %d\n", GetLastError());
        return "ERROR";
    }

    // 关闭写入端句柄
    SAFE_CLOSE_HANDLE(hStdOutWrite);

    // 读取命令输出
    char buffer[128];
    std::string result = "";
    DWORD bytesRead;
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    // 关闭读取端句柄
    SAFE_CLOSE_HANDLE(hStdOutRead);

    // 等待进程完成
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 关闭进程和线程句柄
    SAFE_CLOSE_HANDLE(pi.hProcess);
    SAFE_CLOSE_HANDLE(pi.hThread);

    // 去除换行符和空格
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(remove(result.begin(), result.end(), '\r'), result.end());

    // 返回命令的输出结果
    return result.empty() ? "ERROR" : result;
}

std::string getHardwareID_PS()
{
    // Get-WmiObject 在 PowerShell 2.0+ 都可用 (>=Win7)
    const char* psScript =
        "(Get-WmiObject Win32_Processor).ProcessorId + '|' + "
        "(Get-WmiObject Win32_BaseBoard).SerialNumber + '|' + "
        "(Get-WmiObject Win32_DiskDrive | Select -First 1).SerialNumber";

    std::string cmd = "powershell -NoProfile -Command \"";
    cmd += psScript;
    cmd += "\"";

    std::string combinedID = execCommand(cmd.c_str());
    if ((combinedID.empty() || combinedID.find("ERROR") != std::string::npos)) {
        return "";
    }
    return combinedID;
}

// 获取硬件 ID（CPU + 主板 + 硬盘）
std::string getHardwareID()
{
    // wmic在新系统可能被移除了
    std::string cpuID = execCommand("wmic cpu get processorid");
    std::string boardID = execCommand("wmic baseboard get serialnumber");
    std::string diskID = execCommand("wmic diskdrive get serialnumber");
    std::string combinedID = cpuID + "|" + boardID + "|" + diskID;
    if (combinedID.find("ERROR") != std::string::npos) {
        // 失败再使用 PowerShell 方法
        std::string psID = getHardwareID_PS();
        if (!psID.empty()) {
            Mprintf("Get hardware info with PowerShell: %s\n", psID.c_str());
            return psID;
        }
        Mprintf("Get hardware info FAILED!!! \n");
        Sleep(1234);
        TerminateProcess(GetCurrentProcess(), 0);
    }
    return combinedID;
}

// 使用 SHA-256 计算哈希
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

// 生成 16 字符的唯一设备 ID
std::string getFixedLengthID(const std::string& hash)
{
    return hash.substr(0, 4) + "-" + hash.substr(4, 4) + "-" + hash.substr(8, 4) + "-" + hash.substr(12, 4);
}

std::string deriveKey(const std::string& password, const std::string& hardwareID)
{
    return hashSHA256(password + " + " + hardwareID);
}

std::string getDeviceID(const std::string &hardwareId)
{
    std::string hashedID = hashSHA256(hardwareId);
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
