#pragma once
// VerifyV2.h - V2 授权验证（仅验证，不含签名功能）
// 用于客户端离线验证 V2 签名
//
// ============================================================================
// 授权校验逻辑说明
// ============================================================================
//
// 授权分为 V1 和 V2 两种：
//   - V1: 需要在线连接授权服务器校验
//   - V2: 支持离线校验（使用 ECDSA P-256 签名）
//
// V2 离线校验流程：
//   1. 读取配置中的 SN (设备ID)、Password (授权码)、PwdHmac (签名)
//   2. 验证签名：使用内嵌公钥验证 PwdHmac 是否为 "SN|Password" 的有效签名
//   3. 检查有效期：解析 Password 中的结束日期，判断是否过期
//
// Password 格式: "YYYYMMDD-YYYYMMDD-NNNN-XXXXXXXX"
//                 开始日期  结束日期  数量  校验码
//
// 授权行为：
//   +---------------------------+-------------------------------------------+
//   | 情况                      | 行为                                      |
//   +---------------------------+-------------------------------------------+
//   | V2 + 签名有效 + 未过期    | SetAuthorized() → 离线OK，无需连接服务器  |
//   | V2 + 签名有效 + 已过期    | 需要连接服务器续期                        |
//   | V2 + 签名无效             | 需要连接服务器                            |
//   | V1                        | 需要连接服务器                            |
//   | 试用版                    | 保持连接，WAN检测+端口限制                |
//   | 未授权/连不上服务器       | 循环弹窗警告                              |
//   +---------------------------+-------------------------------------------+
//
// 服务器授权成功后：
//   - 试用版: 保持连接，执行 WAN 检测和端口限制
//   - 正式版: AuthKernelManager 退出
//
// ============================================================================

#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

// 包含公钥
#include "key.h"

// V2 签名长度 (ECDSA P-256: 64 bytes)
#define V2_SIGNATURE_SIZE 64
// V2 公钥长度 (ECDSA P-256: 64 bytes, X+Y coordinates)
#define V2_PUBKEY_SIZE 64

namespace VerifyV2
{
    // Base64 解码
    inline bool base64Decode(const std::string& encoded, BYTE* dataOut, size_t* lenOut)
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
        if (inLen == 0 || inLen % 4 != 0) return false;

        size_t outLen = inLen / 4 * 3;
        if (encoded[inLen - 1] == '=') outLen--;
        if (inLen >= 2 && encoded[inLen - 2] == '=') outLen--;

        size_t j = 0;
        for (size_t i = 0; i < inLen; i += 4) {
            BYTE a = DECODE_TABLE[(BYTE)encoded[i]];
            BYTE b = DECODE_TABLE[(BYTE)encoded[i + 1]];
            BYTE c = DECODE_TABLE[(BYTE)encoded[i + 2]];
            BYTE d = DECODE_TABLE[(BYTE)encoded[i + 3]];

            if (a == 255 || b == 255) return false;

            dataOut[j++] = (a << 2) | (b >> 4);
            if (j < outLen) dataOut[j++] = (b << 4) | (c >> 2);
            if (j < outLen) dataOut[j++] = (c << 6) | d;
        }

        *lenOut = outLen;
        return true;
    }

    // 计算 SHA256 哈希 (使用 BCrypt)
    inline bool computeSHA256(const BYTE* data, size_t len, BYTE* hashOut)
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

    // 使用公钥验证 ECDSA P-256 签名
    inline bool VerifySignature(const BYTE* publicKey, const BYTE* msg, int len, const BYTE* signature)
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

    // 验证口令签名 (V2)
    // deviceId: 设备ID (如 "XXXX-XXXX-XXXX-XXXX")
    // password: 口令字符串 (如 "20250301-20261231-0002-XXXXXXXX")
    // hmacField: HMAC 字段值 (应以 "v2:" 开头)
    // publicKey: 公钥 (64 bytes)，默认使用内嵌公钥
    // 返回: true 签名验证通过
    inline bool verifyPasswordV2(const std::string& deviceId, const std::string& password,
                                  const std::string& hmacField, const BYTE* publicKey = g_LicensePublicKey)
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
        return VerifySignature(publicKey, (const BYTE*)payload.c_str(), (int)payload.length(), signature);
    }

    // 获取设备ID (从配置读取或从硬件ID计算)
    // 供外部调用，简化验证流程
    inline std::string getDeviceIdFromHardwareId(const std::string& hardwareId)
    {
        // 与 pwd_gen.cpp 中的 getDeviceID 逻辑一致
        // SHA256(hardwareId) -> 取前16字节 -> 格式化为 XXXX-XXXX-XXXX-XXXX
        BYTE hash[32];
        if (!computeSHA256((const BYTE*)hardwareId.c_str(), hardwareId.length(), hash))
            return "";

        char deviceId[20];
        snprintf(deviceId, sizeof(deviceId), "%02X%02X-%02X%02X-%02X%02X-%02X%02X",
                 hash[0], hash[1], hash[2], hash[3],
                 hash[4], hash[5], hash[6], hash[7]);
        return deviceId;
    }

    // 检查 V2 密码是否过期
    // password 格式: "20250301-20261231-0002-XXXXXXXX" (startDate-endDate-...)
    // 返回: true 如果未过期（在有效期内）
    inline bool isPasswordValid(const std::string& password)
    {
        // 解析结束日期 (位置 9-16, 格式 YYYYMMDD)
        if (password.length() < 17 || password[8] != '-') {
            return false;
        }

        std::string endDateStr = password.substr(9, 8);
        if (endDateStr.length() != 8) {
            return false;
        }

        // 解析 YYYYMMDD
        int endYear = 0, endMonth = 0, endDay = 0;
        if (sscanf(endDateStr.c_str(), "%4d%2d%2d", &endYear, &endMonth, &endDay) != 3) {
            return false;
        }

        // 获取当前日期
        SYSTEMTIME st;
        GetLocalTime(&st);

        // 比较日期 (转换为 YYYYMMDD 整数比较)
        int endDate = endYear * 10000 + endMonth * 100 + endDay;
        int currentDate = st.wYear * 10000 + st.wMonth * 100 + st.wDay;

        return currentDate <= endDate;
    }

}

// 便捷宏：从配置验证 V2 授权并设置授权状态
// 需要包含 iniFile.h 和 LANChecker.h
// 注意：只有签名有效且未过期才会跳过超时检测，过期则需要连接服务器续期
#define VERIFY_V2_AND_SET_AUTHORIZED() \
    do { \
        config* _v2cfg = IsDebug ? new config : new iniFile; \
        std::string _pwdHmac = _v2cfg->GetStr("settings", "PwdHmac", ""); \
        if (_pwdHmac.length() >= 3 && _pwdHmac.substr(0, 3) == "v2:") { \
            std::string _deviceId = _v2cfg->GetStr("settings", "SN", ""); \
            std::string _password = _v2cfg->GetStr("settings", "Password", ""); \
            if (!_deviceId.empty() && !_password.empty() && \
                VerifyV2::verifyPasswordV2(_deviceId, _password, _pwdHmac) && \
                VerifyV2::isPasswordValid(_password)) { \
                AuthTimeoutChecker::SetAuthorized(); \
            } \
        } \
        delete _v2cfg; \
    } while(0)
