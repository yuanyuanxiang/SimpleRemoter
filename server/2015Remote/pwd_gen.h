#pragma once

#include <string>

// 对生成服务端功能进行加密

std::string getHardwareID();

std::string hashSHA256(const std::string& data);

std::string genHMAC(const std::string& pwdHash, const std::string& superPass);

std::string genHMACbyHash(const std::string& pwdHash, const std::string& superHash);

std::string getFixedLengthID(const std::string& hash);

std::string deriveKey(const std::string& password, const std::string& hardwareID);

std::string getDeviceID(const std::string &hardwareId);

// Use HMAC to sign a message.
uint64_t SignMessage(const std::string& pwd, BYTE* msg, int len);

bool VerifyMessage(const std::string& pwd, BYTE* msg, int len, uint64_t signature);

// ============================================================================
// ECDSA P-256 非对称签名 (v2 授权)
// ============================================================================

// V2 签名长度 (ECDSA P-256: 64 bytes)
#define V2_SIGNATURE_SIZE 64
// V2 公钥长度 (ECDSA P-256: 64 bytes, X+Y coordinates)
#define V2_PUBKEY_SIZE 64
// V2 私钥长度 (ECDSA P-256: 32 bytes)
#define V2_PRIVKEY_SIZE 32

// 生成 V2 密钥对，保存到文件
// privateKeyFile: 私钥保存路径
// publicKeyOut: 输出公钥 (64 bytes)，用于内嵌到程序
// 返回: true 成功
bool GenerateKeyPairV2(const char* privateKeyFile, BYTE* publicKeyOut);

// 使用私钥签名消息 (类似 SignMessage)
// privateKeyFile: 私钥文件路径
// msg: 待签名数据
// len: 数据长度
// signatureOut: 输出签名 (64 bytes)
// 返回: true 成功
bool SignMessageV2(const char* privateKeyFile, const BYTE* msg, int len, BYTE* signatureOut);

// 使用公钥验证签名 (类似 VerifyMessage)
// publicKey: 公钥数据 (64 bytes)
// msg: 原始数据
// len: 数据长度
// signature: 签名数据 (64 bytes)
// 返回: true 验证通过
bool VerifyMessageV2(const BYTE* publicKey, const BYTE* msg, int len, const BYTE* signature);

// 签名口令 (V2)
// deviceId: 设备ID (如 "XXXX-XXXX-XXXX-XXXX")
// password: 口令字符串 (如 "20250301-20261231-0002-XXXXXXXX")
// privateKeyFile: 私钥文件路径
// 返回: HMAC 字段值 (如 "v2:BASE64_SIGNATURE")，失败返回空字符串
std::string signPasswordV2(const std::string& deviceId, const std::string& password, const char* privateKeyFile);

// 验证口令签名 (V2)
// deviceId: 设备ID
// password: 口令字符串
// hmacField: HMAC 字段值 (应以 "v2:" 开头)
// publicKey: 内嵌公钥 (64 bytes)
// 返回: true 签名验证通过
bool verifyPasswordV2(const std::string& deviceId, const std::string& password,
                      const std::string& hmacField, const BYTE* publicKey);

// Base64 编解码
std::string base64Encode(const BYTE* data, size_t len);
bool base64Decode(const std::string& encoded, BYTE* dataOut, size_t* lenOut);

// 格式化公钥为 C 数组代码
std::string formatPublicKeyAsCode(const BYTE* publicKey);

// ============================================================================
// Authorization 签名 (多层授权)
// ============================================================================

// 签名 Authorization（不绑定 deviceID，绑定 snHashPrefix，可在同一第一层的下级间共享）
// license: 授权信息 (如 "20260317|20270317|0256")
// snHashPrefix: 第一层 SN/deviceID 的前8字符哈希，用于隔离不同第一层
// privateKeyFile: 私钥文件路径
// 返回: 签名字段值 (如 "v2:BASE64_SIGNATURE")，失败返回空字符串
std::string signAuthorizationV2(const std::string& license, const std::string& snHashPrefix, const char* privateKeyFile);

// 计算 snHashPrefix（用于 Authorization 隔离）
// deviceID: 第一层的设备ID（如 "XXXX-XXXX-XXXX-XXXX"）
// 返回: SHA256(deviceID).substr(0, 8)
std::string computeSnHashPrefix(const std::string& deviceID);
