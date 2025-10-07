#pragma once
#include <string>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <WinInet.h>
#include "logger.h"
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
#pragma comment(lib, "Iphlpapi.lib")

inline void splitIpPort(const std::string& input, std::string& ip, std::string& port) {
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

/**
 * IPConverter: IP �����࣬���ڻ�ȡ����IP����ȡIP��Ӧ�ĵ���λ�õ�.
 * Ŀǰ��ͨ�����ù����Ļ�����API��ɣ�����ò�ѯ��վ���ɷ�������Ҫ��������.
 */
class IPConverter
{
public:
	std::string IPtoAddress(const std::string& ip) { return "implement me"; }

	/**
	 * �жϸ����� IP ��ַ�Ƿ��Ǿ�������������IP
	 * @param ipAddress IP ��ַ�ַ������� "192.168.1.1"��
	 * @return ����Ǿ����� IP������ true; ���򷵻� false
	 */
	bool IsPrivateIP(const std::string& ipAddress) {
		// �� IP ��ַ�ַ���ת��Ϊ�����Ƹ�ʽ
		in_addr addr;
		if (inet_pton(AF_INET, ipAddress.c_str(), &addr) != 1) {
			Mprintf("Invalid IP address: %s\n", ipAddress.c_str());
			return false;
		}

		// �������� IP ��ַת��Ϊ�޷�������
		unsigned long ip = ntohl(addr.s_addr);

		// ��� IP ��ַ�Ƿ��ھ�������Χ��
		if ((ip >= 0x0A000000 && ip <= 0x0AFFFFFF) || // 10.0.0.0/8
			(ip >= 0xAC100000 && ip <= 0xAC1FFFFF) || // 172.16.0.0/12
			(ip >= 0xC0A80000 && ip <= 0xC0A8FFFF) || // 192.168.0.0/16
			(ip >= 0x7F000000 && ip <= 0x7FFFFFFF)) { // 127.0.0.0/8
			return true;
		}

		return false;
	}

	// ��ȡ��������λ��
	std::string GetLocalLocation() {
		return GetGeoLocation(getPublicIP());
	}

	// ��ȡ IP ��ַ����λ��(����[ipinfo.io])
	std::string GetGeoLocation(const std::string& IP) {
		if (IP.empty()) return "";

		std::string ip = IP;
		if (isLocalIP(ip)) {
			ip = getPublicIP();
			if (ip.empty())
				return "";
		}

		HINTERNET hInternet, hConnect;
		DWORD bytesRead;
		std::string readBuffer;

		// ��ʼ�� WinINet
		hInternet = InternetOpen("IP Geolocation", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (hInternet == NULL) {
			Mprintf("InternetOpen failed! %d\n", GetLastError());
			return "";
		}

		// ���� HTTP ������
		std::string url = "http://ipinfo.io/" + ip + "/json";
		hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
		if (hConnect == NULL) {
			Mprintf("InternetOpenUrlA failed! %d\n", GetLastError());
			InternetCloseHandle(hInternet);
			return "";
		}

		// ��ȡ���ص�����
		char buffer[4096];
		while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
			readBuffer.append(buffer, bytesRead);
		}

		// ���� JSON ��Ӧ
		Json::Value jsonData;
		Json::Reader jsonReader;
		std::string location;

		if (jsonReader.parse(readBuffer, jsonData)) {
			std::string country = jsonData["country"].asString();
			std::string city = jsonData["city"].asString();
			std::string loc = jsonData["loc"].asString();  // ��γ����Ϣ
			if (city.empty() && country.empty()) {
			}
			else if (city.empty()) {
				location = country;
			}
			else if (country.empty()) {
				location = city;
			}
			else {
				location = city + ", " + country;
			}
			if (location.empty() && IsPrivateIP(ip)) {
				location = "Local Area Network";
			}
		}
		else {
			Mprintf("Failed to parse JSON response: %s.\n", readBuffer.c_str());
		}

		// �رվ��
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);

		return location;
	}

	bool isLoopbackAddress(const std::string& ip) {
		return (ip == "127.0.0.1" || ip == "::1");
	}

	bool isLocalIP(const std::string& ip) {
		if (isLoopbackAddress(ip)) return true; // �ȼ��ػ���ַ

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

	// ��ȡ����IP, ��ȡʧ�ܷ��ؿ�
	std::string getPublicIP() {
		clock_t t = clock();

		// �����ѡ��ѯԴ
		static const std::vector<std::string> urls = {
			"https://checkip.amazonaws.com",  // ȫ������
			"https://api.ipify.org",          // �����߿���
			"https://ipinfo.io/ip",           // ���÷���
			"https://icanhazip.com",          // ��������
			"https://ifconfig.me/ip"          // ĩλ����
		};

		// �� WinINet �Ự
		HINTERNET hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		if (!hInternet) {
			Mprintf("InternetOpen failed. cost %d ms.\n", clock() - t);
			return "";
		}

		// ���ó�ʱ (����)
		DWORD timeout = 3000; // 3 ��
		InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
		InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
		InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

		std::string result;
		char buffer[2048];
		DWORD bytesRead = 0;

		// ��ѯ��ͬ IP ��ѯԴ
		for (const auto& url : urls) {
			HINTERNET hConnect = InternetOpenUrlA(
				hInternet, url.c_str(), NULL, 0,
				INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE,
				0
			);

			if (!hConnect) {
				continue; // ��ǰԴʧ�ܣ�������һ��
			}

			memset(buffer, 0, sizeof(buffer));
			if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
				result.assign(buffer, bytesRead);

				// ȥ�����з��Ϳո�
				while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
					result.pop_back();

				InternetCloseHandle(hConnect);
				break; // �ɹ���ȡ��ֹͣ����
			}

			InternetCloseHandle(hConnect);
		}

		InternetCloseHandle(hInternet);

		Mprintf("getPublicIP %s cost %d ms.\n", result.empty() ? "failed" : "succeed", clock() - t);

		return result;
	}
};
