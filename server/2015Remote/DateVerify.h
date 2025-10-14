#pragma once

#include <iostream>
#include <ctime>
#include <WinSock2.h>
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")

// 中国大陆优化的NTP服务器列表
const char* CN_NTP_SERVERS[] = {
    "ntp.aliyun.com",
    "time1.aliyun.com",
    "ntp1.tencent.com",
    "time.edu.cn",
    "ntp.tuna.tsinghua.edu.cn",
    "cn.ntp.org.cn",
};
const int CN_NTP_COUNT = sizeof(CN_NTP_SERVERS) / sizeof(CN_NTP_SERVERS[0]);
const int NTP_PORT = 123;
const uint64_t NTP_EPOCH_OFFSET = 2208988800ULL;

// 检测程序是否处于试用期
class DateVerify
{
private:
    // 初始化Winsock
    bool initWinsock()
    {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }

    // 从指定NTP服务器获取时间
    time_t getTimeFromServer(const char* server)
    {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return 0;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(NTP_PORT);

        // 解析主机名
        hostent* host = gethostbyname(server);
        if (!host) {
            closesocket(sock);
            return 0;
        }
        serverAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr_list[0]);

        // 设置超时
        DWORD timeout = 2000; // 2秒超时
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        // 准备NTP请求包
        char ntpPacket[48] = { 0 };
        ntpPacket[0] = 0x1B; // LI=0, VN=3, Mode=3

        // 发送请求
        if (sendto(sock, ntpPacket, sizeof(ntpPacket), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return 0;
        }

        // 接收响应
        if (recv(sock, ntpPacket, sizeof(ntpPacket), 0) <= 0) {
            closesocket(sock);
            return 0;
        }

        closesocket(sock);

        // 解析NTP时间
        uint32_t ntpTime = ntohl(*((uint32_t*)(ntpPacket + 40)));
        return ntpTime - NTP_EPOCH_OFFSET;
    }

    // 获取网络时间（尝试多个服务器）
    time_t getNetworkTimeInChina()
    {
        if (!initWinsock()) return 0;

        time_t result = 0;
        for (int i = 0; i < CN_NTP_COUNT && result == 0; i++) {
            result = getTimeFromServer(CN_NTP_SERVERS[i]);
            if (result != 0) {
                break;
            }
        }

        WSACleanup();
        return result;
    }

    // 将月份缩写转换为数字(1-12)
    int monthAbbrevToNumber(const std::string& month)
    {
        static const std::map<std::string, int> months = {
            {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
            {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
            {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
        };
        auto it = months.find(month);
        return (it != months.end()) ? it->second : 0;
    }

    // 解析__DATE__字符串为tm结构
    tm parseCompileDate(const char* compileDate)
    {
        tm tmCompile = { 0 };
        std::string monthStr(compileDate, 3);
        std::string dayStr(compileDate + 4, 2);
        std::string yearStr(compileDate + 7, 4);

        tmCompile.tm_year = std::stoi(yearStr) - 1900;
        tmCompile.tm_mon = monthAbbrevToNumber(monthStr) - 1;
        tmCompile.tm_mday = std::stoi(dayStr);

        return tmCompile;
    }

    // 计算两个日期之间的天数差
    int daysBetweenDates(const tm& date1, const tm& date2)
    {
        auto timeToTimePoint = [](const tm& tmTime) {
            std::time_t tt = mktime(const_cast<tm*>(&tmTime));
            return std::chrono::system_clock::from_time_t(tt);
        };

        auto tp1 = timeToTimePoint(date1);
        auto tp2 = timeToTimePoint(date2);

        auto duration = tp1 > tp2 ? tp1 - tp2 : tp2 - tp1;
        return std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
    }

    // 获取当前日期
    tm getCurrentDate()
    {
        std::time_t now = std::time(nullptr);
        tm tmNow = *std::localtime(&now);
        tmNow.tm_hour = 0;
        tmNow.tm_min = 0;
        tmNow.tm_sec = 0;
        return tmNow;
    }

    // 验证本地日期是否被修改
    bool isLocalDateModified()
    {
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) {
            return true; // 无法验证
        }

        time_t localTime = time(nullptr);
        double diffDays = difftime(networkTime, localTime) / 86400.0;

        // 允许±1天的误差（考虑网络延迟和时区等因素）
        if (fabs(diffDays) > 1.0) {
            return true;
        }

        return false;
    }

public:

    bool isTrail(int trailDays=7)
    {
        if (isLocalDateModified())
            return false;

        tm tmCompile = parseCompileDate(__DATE__), tmCurrent = getCurrentDate();

        // 计算天数差
        int daysDiff = daysBetweenDates(tmCompile, tmCurrent);

        return daysDiff <= trailDays;
    }
};
