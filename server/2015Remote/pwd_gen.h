#pragma once

#include <string>

// 对生成服务端功能进行加密

std::string getHardwareID();

std::string hashSHA256(const std::string& data);

std::string genHMAC(const std::string& pwdHash, const std::string& superPass);

std::string getFixedLengthID(const std::string& hash);

std::string deriveKey(const std::string& password, const std::string& hardwareID);

std::string getDeviceID(const std::string &hardwareId);

// Use HMAC to sign a message.
uint64_t SignMessage(const std::string& pwd, BYTE* msg, int len);

bool VerifyMessage(const std::string& pwd, BYTE* msg, int len, uint64_t signature);
