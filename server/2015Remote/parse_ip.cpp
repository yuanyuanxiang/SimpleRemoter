#include "stdafx.h"
#include "parse_ip.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Iphlpapi.lib")

/**
 * 判断给定的 IP 地址是否是局域网（内网）IP
 * @param ipAddress IP 地址字符串（如 "192.168.1.1"）
 * @return 如果是局域网 IP，返回 true；否则返回 false
 */
bool IsPrivateIP(const std::string& ipAddress) {
	// 将 IP 地址字符串转换为二进制格式
	in_addr addr;
	if (inet_pton(AF_INET, ipAddress.c_str(), &addr) != 1) {
		Mprintf("Invalid IP address: %s\n", ipAddress.c_str());
		return false;
	}

	// 将二进制 IP 地址转换为无符号整数
	unsigned long ip = ntohl(addr.s_addr);

	// 检查 IP 地址是否在局域网范围内
	if ((ip >= 0x0A000000 && ip <= 0x0AFFFFFF) || // 10.0.0.0/8
		(ip >= 0xAC100000 && ip <= 0xAC1FFFFF) || // 172.16.0.0/12
		(ip >= 0xC0A80000 && ip <= 0xC0A8FFFF) || // 192.168.0.0/16
		(ip >= 0x7F000000 && ip <= 0x7FFFFFFF)) { // 127.0.0.0/8
		return true;
	}

	return false;
}

// 获取 IP 地址地理位置(基于[ipinfo.io])
std::string GetGeoLocation(const std::string& IP) {
	std::string ip = IP;
	if (isLocalIP(ip)) {
		ip = getPublicIP();
		if (ip.empty())
			return "";
	}
	
	HINTERNET hInternet, hConnect;
	DWORD bytesRead;
	std::string readBuffer;

	// 初始化 WinINet
	hInternet = InternetOpen("IP Geolocation", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet == NULL) {
		Mprintf("InternetOpen failed! %d\n", GetLastError());
		return "";
	}

	// 创建 HTTP 请求句柄
	std::string url = "http://ipinfo.io/" + ip + "/json";
	hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
	if (hConnect == NULL) {
		Mprintf("InternetOpenUrlA failed! %d\n", GetLastError());
		InternetCloseHandle(hInternet);
		return "";
	}

	// 读取返回的内容
	char buffer[4096];
	while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		readBuffer.append(buffer, bytesRead);
	}

	// 解析 JSON 响应
	Json::Value jsonData;
	Json::Reader jsonReader;
	std::string location;

	if (jsonReader.parse(readBuffer, jsonData)) {
		std::string country = jsonData["country"].asString();
		std::string city = jsonData["city"].asString();
		std::string loc = jsonData["loc"].asString();  // 经纬度信息
		if (city.empty() && country.empty()) {
		}else if (city.empty()) {
			location = country;
		}else if (country.empty()) {
			location = city;
		}else {
			location = city + ", " + country;
		}
		if (location.empty() && IsPrivateIP(ip)) {
			location = "Local Area Network";
		}
	}
	else {
		Mprintf("Failed to parse JSON response: %s.\n", readBuffer.c_str());
	}

	// 关闭句柄
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return location;
}

bool isLoopbackAddress(const std::string& ip) {
	return (ip == "127.0.0.1" || ip == "::1");
}

bool isLocalIP(const std::string& ip) {
	if (isLoopbackAddress(ip)) return true; // 先检查回环地址

	ULONG outBufLen = 15000;
	IP_ADAPTER_ADDRESSES* pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
	if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(pAddresses);
		pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
	}

	if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen) == NO_ERROR) {
		for (IP_ADAPTER_ADDRESSES* pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
			for (IP_ADAPTER_UNICAST_ADDRESS* pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
				char addressBuffer[INET6_ADDRSTRLEN] = { 0 };
				getnameinfo(pUnicast->Address.lpSockaddr, pUnicast->Address.iSockaddrLength, addressBuffer, sizeof(addressBuffer), nullptr, 0, NI_NUMERICHOST);

				if (ip == addressBuffer) {
					free(pAddresses);
					return true;
				}
			}
		}
	}

	free(pAddresses);
	return false;
}

// 获取公网IP, 获取失败返回空
std::string getPublicIP() {
	HINTERNET hInternet, hConnect;
	DWORD bytesRead;
	char buffer[1024] = { 0 };

	hInternet = InternetOpen("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (!hInternet) return "";

	hConnect = InternetOpenUrl(hInternet, "https://api64.ipify.org", NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
	if (!hConnect) {
		InternetCloseHandle(hInternet);
		return "";
	}

	InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead);
	InternetCloseHandle(hConnect);
	InternetCloseHandle(hInternet);

	return std::string(buffer);
}

void splitIpPort(const std::string& input, std::string& ip, std::string& port) {
	size_t pos = input.find(':');
	if (pos != std::string::npos) {
		ip = input.substr(0, pos);
		port = input.substr(pos + 1);
	}
	else {
		ip = input;
		port = "";
	}
}
