#pragma once

#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <string>
#include "jsoncpp/json.h"

#ifndef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "jsoncpp/jsoncppd.lib")
#else
#pragma comment(lib, "jsoncpp/jsoncpp.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "jsoncpp/jsoncpp_x64d.lib")
#else
#pragma comment(lib, "jsoncpp/jsoncpp_x64.lib")
#endif
#endif

#pragma comment(lib, "wininet.lib")

// 获取 IP 地址地理位置
std::string GetGeoLocation(const std::string& ip);

bool isLocalIP(const std::string& ip);

std::string getPublicIP();

bool IsPrivateIP(const std::string& ipAddress);
