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

// 是否为本机IP
bool isLocalIP(const std::string& ip);

// 获取本机公网IP, 获取失败返回空
std::string getPublicIP();

// 判断给定的 IP 地址是否是局域网（内网）IP
bool IsPrivateIP(const std::string& ipAddress);

void splitIpPort(const std::string& input, std::string& ip, std::string& port);
