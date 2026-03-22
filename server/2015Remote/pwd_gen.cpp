
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
#include <algorithm>

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
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

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

std::string genHMACbyHash(const std::string& pwdHash, const std::string& superHash)
{
    std::string key = superHash;
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

// ============================================================================
// ECDSA P-256 非对称签名实现
// ============================================================================

// Base64 编码表
static const char* BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const BYTE* data, size_t len)
{
    std::string result;
    result.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        BYTE b0 = data[i];
        BYTE b1 = (i + 1 < len) ? data[i + 1] : 0;
        BYTE b2 = (i + 2 < len) ? data[i + 2] : 0;

        result += BASE64_CHARS[b0 >> 2];
        result += BASE64_CHARS[((b0 & 0x03) << 4) | (b1 >> 4)];
        result += (i + 1 < len) ? BASE64_CHARS[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        result += (i + 2 < len) ? BASE64_CHARS[b2 & 0x3F] : '=';
    }
    return result;
}

bool base64Decode(const std::string& encoded, BYTE* dataOut, size_t* lenOut)
{
    static const BYTE DECODE_TABLE[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
         52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
        255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
         41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
    };

    size_t inLen = encoded.length();
    if (inLen % 4 != 0) return false;

    size_t outLen = inLen / 4 * 3;
    if (encoded[inLen - 1] == '=') outLen--;
    if (encoded[inLen - 2] == '=') outLen--;

    size_t j = 0;
    for (size_t i = 0; i < inLen; i += 4) {
        BYTE a = DECODE_TABLE[(BYTE)encoded[i]];
        BYTE b = DECODE_TABLE[(BYTE)encoded[i + 1]];
        BYTE c = DECODE_TABLE[(BYTE)encoded[i + 2]];
        BYTE d = DECODE_TABLE[(BYTE)encoded[i + 3]];

        if (a == 255 || b == 255) return false;

        dataOut[j++] = (a << 2) | (b >> 4);
        if (encoded[i + 2] != '=') dataOut[j++] = (b << 4) | (c >> 2);
        if (encoded[i + 3] != '=') dataOut[j++] = (c << 6) | d;
    }

    *lenOut = j;
    return true;
}

std::string formatPublicKeyAsCode(const BYTE* publicKey)
{
    std::ostringstream ss;
    ss << "const BYTE g_LicensePublicKey[64] = {\n    ";
    for (int i = 0; i < V2_PUBKEY_SIZE; i++) {
        ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)publicKey[i];
        if (i < V2_PUBKEY_SIZE - 1) {
            ss << ", ";
            if ((i + 1) % 8 == 0) ss << "\n    ";
        }
    }
    ss << "\n};";
    return ss.str();
}

// 计算 SHA256 哈希 (使用 BCrypt)
static bool computeSHA256(const BYTE* data, size_t len, BYTE* hashOut)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashObjectSize = 0, dataLen = 0;
    PBYTE hashObject = nullptr;
    bool success = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return false;

    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectSize, sizeof(DWORD), &dataLen, 0) != 0)
        goto cleanup;

    hashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hashObjectSize);
    if (!hashObject) goto cleanup;

    if (BCryptCreateHash(hAlg, &hHash, hashObject, hashObjectSize, nullptr, 0, 0) != 0)
        goto cleanup;

    if (BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0) != 0)
        goto cleanup;

    if (BCryptFinishHash(hHash, hashOut, 32, 0) != 0)
        goto cleanup;

    success = true;

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    if (hashObject) HeapFree(GetProcessHeap(), 0, hashObject);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return success;
}

bool GenerateKeyPairV2(const char* privateKeyFile, BYTE* publicKeyOut)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool success = false;

    // 打开 ECDSA P-256 算法
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) {
        Mprintf("Failed to open ECDSA algorithm provider\n");
        return false;
    }

    // 生成密钥对
    if (BCryptGenerateKeyPair(hAlg, &hKey, 256, 0) != 0) {
        Mprintf("Failed to generate key pair\n");
        goto cleanup;
    }

    if (BCryptFinalizeKeyPair(hKey, 0) != 0) {
        Mprintf("Failed to finalize key pair\n");
        goto cleanup;
    }

    // 导出私钥
    {
        ULONG blobSize = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &blobSize, 0);

        std::vector<BYTE> privateBlob(blobSize);
        if (BCryptExportKey(hKey, nullptr, BCRYPT_ECCPRIVATE_BLOB, privateBlob.data(), blobSize, &blobSize, 0) != 0) {
            Mprintf("Failed to export private key\n");
            goto cleanup;
        }

        // 保存私钥到文件
        HANDLE hFile = CreateFileA(privateKeyFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            Mprintf("Failed to create private key file\n");
            goto cleanup;
        }
        DWORD written;
        WriteFile(hFile, privateBlob.data(), blobSize, &written, nullptr);
        CloseHandle(hFile);
    }

    // 导出公钥
    {
        ULONG blobSize = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0, &blobSize, 0);

        std::vector<BYTE> publicBlob(blobSize);
        if (BCryptExportKey(hKey, nullptr, BCRYPT_ECCPUBLIC_BLOB, publicBlob.data(), blobSize, &blobSize, 0) != 0) {
            Mprintf("Failed to export public key\n");
            goto cleanup;
        }

        // BCRYPT_ECCKEY_BLOB 结构: magic(4) + cbKey(4) + X(32) + Y(32)
        // 我们只需要 X+Y 坐标 (64 bytes)
        BCRYPT_ECCKEY_BLOB* header = (BCRYPT_ECCKEY_BLOB*)publicBlob.data();
        if (header->cbKey != 32) {
            Mprintf("Unexpected key size\n");
            goto cleanup;
        }
        memcpy(publicKeyOut, publicBlob.data() + sizeof(BCRYPT_ECCKEY_BLOB), V2_PUBKEY_SIZE);
    }

    success = true;

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return success;
}

bool SignMessageV2(const char* privateKeyFile, const BYTE* msg, int len, BYTE* signatureOut)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<BYTE> privateBlob;
    BYTE hash[32];
    bool success = false;

    // 读取私钥文件
    {
        HANDLE hFile = CreateFileA(privateKeyFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            Mprintf("Failed to open private key file\n");
            return false;
        }
        DWORD fileSize = GetFileSize(hFile, nullptr);
        privateBlob.resize(fileSize);
        DWORD bytesRead;
        ReadFile(hFile, privateBlob.data(), fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);
    }

    // 计算消息的 SHA256 哈希
    if (!computeSHA256(msg, len, hash)) {
        Mprintf("Failed to compute hash\n");
        return false;
    }

    // 打开 ECDSA 算法
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
        return false;

    // 导入私钥
    if (BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPRIVATE_BLOB, &hKey,
                            privateBlob.data(), (ULONG)privateBlob.size(), 0) != 0) {
        Mprintf("Failed to import private key\n");
        goto cleanup;
    }

    // 签名
    {
        ULONG sigLen = 0;
        if (BCryptSignHash(hKey, nullptr, hash, 32, nullptr, 0, &sigLen, 0) != 0)
            goto cleanup;

        if (sigLen != V2_SIGNATURE_SIZE) {
            Mprintf("Unexpected signature size: %lu\n", sigLen);
            goto cleanup;
        }

        if (BCryptSignHash(hKey, nullptr, hash, 32, signatureOut, sigLen, &sigLen, 0) != 0) {
            Mprintf("Failed to sign\n");
            goto cleanup;
        }
    }

    success = true;

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return success;
}

bool VerifyMessageV2(const BYTE* publicKey, const BYTE* msg, int len, const BYTE* signature)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    BYTE hash[32];
    bool success = false;

    // 计算消息的 SHA256 哈希
    if (!computeSHA256(msg, len, hash))
        return false;

    // 构建公钥 blob
    std::vector<BYTE> publicBlob(sizeof(BCRYPT_ECCKEY_BLOB) + V2_PUBKEY_SIZE);
    BCRYPT_ECCKEY_BLOB* header = (BCRYPT_ECCKEY_BLOB*)publicBlob.data();
    header->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    header->cbKey = 32;
    memcpy(publicBlob.data() + sizeof(BCRYPT_ECCKEY_BLOB), publicKey, V2_PUBKEY_SIZE);

    // 打开 ECDSA 算法
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
        return false;

    // 导入公钥
    if (BCryptImportKeyPair(hAlg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &hKey,
                            publicBlob.data(), (ULONG)publicBlob.size(), 0) != 0)
        goto cleanup;

    // 验证签名
    success = (BCryptVerifySignature(hKey, nullptr, hash, 32,
                                     (PUCHAR)signature, V2_SIGNATURE_SIZE, 0) == 0);

cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return success;
}

std::string signPasswordV2(const std::string& deviceId, const std::string& password, const char* privateKeyFile)
{
    // 构建待签名数据: "deviceId|password"
    std::string payload = deviceId + "|" + password;

    // 签名
    BYTE signature[V2_SIGNATURE_SIZE];
    if (!SignMessageV2(privateKeyFile, (const BYTE*)payload.c_str(), (int)payload.length(), signature)) {
        return "";
    }

    // 返回: "v2:" + Base64签名
    return "v2:" + base64Encode(signature, V2_SIGNATURE_SIZE);
}

bool verifyPasswordV2(const std::string& deviceId, const std::string& password,
                      const std::string& hmacField, const BYTE* publicKey)
{
    // 检查 v2: 前缀
    if (hmacField.length() < 3 || hmacField.substr(0, 3) != "v2:") {
        return false;
    }

    // 提取 Base64 签名
    std::string sigBase64 = hmacField.substr(3);

    // 解码签名
    BYTE signature[V2_SIGNATURE_SIZE];
    size_t sigLen = 0;
    if (!base64Decode(sigBase64, signature, &sigLen) || sigLen != V2_SIGNATURE_SIZE) {
        return false;
    }

    // 构建待验证数据: "deviceId|password"
    std::string payload = deviceId + "|" + password;

    // 验证签名
    return VerifyMessageV2(publicKey, (const BYTE*)payload.c_str(), (int)payload.length(), signature);
}

// ============================================================================
// Authorization 签名 (多层授权)
// ============================================================================

std::string computeSnHashPrefix(const std::string& deviceID)
{
    // snHashPrefix = SHA256(deviceID).substr(0, 8)
    // 用于在 Authorization 中标识第一层，实现不同第一层之间的隔离
    // 使用 deviceID 而非 pwdHash，以便在生成授权时直接计算（无需等待网络验证）
    std::string hash = hashSHA256(deviceID);
    return hash.substr(0, 8);
}

std::string signAuthorizationV2(const std::string& license, const std::string& snHashPrefix, const char* privateKeyFile)
{
    // payload 包含 license + snHashPrefix
    // license 格式: "20260317|20270317|0256" (startDate|endDate|hostNum)
    // snHashPrefix: 第一层 SN/deviceID 的前8字符哈希
    // 完整 payload: "20260317|20270317|0256|a1b2c3d4"
    std::string payload = license + "|" + snHashPrefix;

    BYTE signature[V2_SIGNATURE_SIZE];
    if (!SignMessageV2(privateKeyFile, (const BYTE*)payload.c_str(), (int)payload.length(), signature)) {
        Mprintf("signAuthorizationV2: SignMessageV2 failed\n");
        return "";
    }

    // 返回: "v2:" + Base64(signature)
    return "v2:" + base64Encode(signature, V2_SIGNATURE_SIZE);
}
