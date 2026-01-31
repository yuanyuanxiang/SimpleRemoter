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

// UTF-8 转 ANSI（当前系统代码页）
inline std::string Utf8ToAnsi(const std::string& utf8)
{
    if (utf8.empty()) return "";

    // UTF-8 -> UTF-16
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (wideLen <= 0) return utf8;

    std::wstring wideStr(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wideStr[0], wideLen);

    // UTF-16 -> ANSI
    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, NULL, 0, NULL, NULL);
    if (ansiLen <= 0) return utf8;

    std::string ansiStr(ansiLen, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &ansiStr[0], ansiLen, NULL, NULL);

    // 去掉末尾的 null 字符
    if (!ansiStr.empty() && ansiStr.back() == '\0') {
        ansiStr.pop_back();
    }

    return ansiStr;
}

// 检测字符串是否可能是 UTF-8 编码（包含多字节序列）
inline bool IsLikelyUtf8(const std::string& str)
{
    for (size_t i = 0; i < str.size(); i++) {
        unsigned char c = str[i];
        if (c >= 0x80) {
            // 2字节序列: 110xxxxx 10xxxxxx
            if ((c & 0xE0) == 0xC0 && i + 1 < str.size()) {
                if ((str[i+1] & 0xC0) == 0x80) return true;
            }
            // 3字节序列: 1110xxxx 10xxxxxx 10xxxxxx
            else if ((c & 0xF0) == 0xE0 && i + 2 < str.size()) {
                if ((str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80) return true;
            }
            // 4字节序列: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            else if ((c & 0xF8) == 0xF0 && i + 3 < str.size()) {
                if ((str[i+1] & 0xC0) == 0x80 && (str[i+2] & 0xC0) == 0x80 && (str[i+3] & 0xC0) == 0x80) return true;
            }
        }
    }
    return false;
}

// 安全转换：检测到 UTF-8 才转换，否则原样返回
inline std::string SafeUtf8ToAnsi(const std::string& str)
{
    if (str.empty()) return str;
    return IsLikelyUtf8(str) ? Utf8ToAnsi(str) : str;
}

inline void splitIpPort(const std::string& input, std::string& ip, std::string& port)
{
    size_t pos = input.find(':');
    if (pos != std::string::npos) {
        ip = input.substr(0, pos);
        port = input.substr(pos + 1);
    } else {
        ip = input;
        port = "";
    }
}

/**
 * IPConverter: IP 操作类，用于获取公网IP，获取IP对应的地理位置等.
 * 目前是通过调用公开的互联网API完成，假如该查询网站不可访问则需要重新适配.
 */
class IPConverter
{
public:
    virtual ~IPConverter() {}

    virtual std::string IPtoAddress(const std::string& ip)
    {
        return "implement me";
    }

    /**
     * 判断给定的 IP 地址是否是局域网（内网）IP
     * @param ipAddress IP 地址字符串（如 "192.168.1.1"）
     * @return 如果是局域网 IP，返回 true; 否则返回 false
     */
    bool IsPrivateIP(const std::string& ipAddress)
    {
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

    // 获取本机地理位置
    std::string GetLocalLocation()
    {
        return GetGeoLocation(getPublicIP());
    }

    // 获取 IP 地址地理位置(基于[ipinfo.io])
    virtual std::string GetGeoLocation(const std::string& IP)
    {
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
            std::string country = Utf8ToAnsi(jsonData["country"].asString());
            std::string city = Utf8ToAnsi(jsonData["city"].asString());
            std::string loc = jsonData["loc"].asString();  // 经纬度信息
            if (city.empty() && country.empty()) {
            } else if (city.empty()) {
                location = country;
            } else if (country.empty()) {
                location = city;
            } else {
                location = city + ", " + country;
            }
            if (location.empty() && IsPrivateIP(ip)) {
                location = "Local Area Network";
            }
        } else {
            Mprintf("Failed to parse JSON response: %s.\n", readBuffer.c_str());
        }

        // 关闭句柄
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        return location;
    }

    bool isLoopbackAddress(const std::string& ip)
    {
        return (ip == "127.0.0.1" || ip == "::1");
    }

    bool isLocalIP(const std::string& ip)
    {
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
    std::string getPublicIP()
    {
        clock_t t = clock();

        // 多个候选查询源
        static const std::vector<std::string> urls = {
            "https://checkip.amazonaws.com",  // 全球最稳
            "https://api.ipify.org",          // 主流高可用
            "https://ipinfo.io/ip",           // 备用方案
            "https://icanhazip.com",          // 轻量快速
            "https://ifconfig.me/ip"          // 末位兜底
        };

        // 打开 WinINet 会话
        HINTERNET hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            Mprintf("InternetOpen failed. cost %d ms.\n", clock() - t);
            return "";
        }

        // 设置超时 (毫秒)
        DWORD timeout = 3000; // 3 秒
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

        std::string result;
        char buffer[2048];
        DWORD bytesRead = 0;

        // 轮询不同 IP 查询源
        for (const auto& url : urls) {
            HINTERNET hConnect = InternetOpenUrlA(
                                     hInternet, url.c_str(), NULL, 0,
                                     INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE,
                                     0
                                 );

            if (!hConnect) {
                continue; // 当前源失败，尝试下一个
            }

            memset(buffer, 0, sizeof(buffer));
            if (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                result.assign(buffer, bytesRead);

                // 去除换行符和空格
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
                    result.pop_back();

                InternetCloseHandle(hConnect);
                break; // 成功获取，停止尝试
            }

            InternetCloseHandle(hConnect);
        }

        InternetCloseHandle(hInternet);

        Mprintf("getPublicIP %s cost %d ms.\n", result.empty() ? "failed" : "succeed", clock() - t);

        return result;
    }
};

IPConverter* LoadFileQQWry(const char* datPath);
