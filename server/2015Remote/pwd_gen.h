#pragma once

#include <string>


// �����ɷ���˹��ܽ��м���

std::string getHardwareID();

std::string hashSHA256(const std::string& data);

std::string genHMAC(const std::string& pwdHash, const std::string& superPass);

std::string getFixedLengthID(const std::string& hash);

std::string deriveKey(const std::string& password, const std::string& hardwareID);

std::string getDeviceID();
