#pragma once

#include <iostream>
#include <ctime>
#include <map>
#include <chrono>
#include <cmath>
#include <WinSock2.h>
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")

// NTP服务器列表（按客户端优先级：中国50%+ > 港澳台20% > 日新5% > 其他）
static const char* NTP_SERVERS[] = {
    // 中国大陆 (50%+)
    "ntp.aliyun.com",           // 阿里云，国内最快
    "ntp1.tencent.com",         // 腾讯云
    "ntp.tuna.tsinghua.edu.cn", // 清华大学 TUNA
    "cn.pool.ntp.org",          // 中国 NTP 池
    // 亚太区域 (香港20% + 日本/新加坡5%)
    "time.asia.apple.com",      // Apple 亚太节点
    "time.google.com",          // Google 亚太有节点
    "hk.pool.ntp.org",          // 香港 NTP 池
    "jp.pool.ntp.org",          // 日本 NTP 池
    "sg.pool.ntp.org",          // 新加坡 NTP 池
    // 全球兜底 (~20%其他地区)
    "pool.ntp.org",             // 全球 NTP 池
    "time.cloudflare.com",      // Cloudflare 全球 Anycast
};
static const int NTP_SERVER_COUNT = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);
static const int NTP_PORT = 123;
static const uint64_t NTP_EPOCH_OFFSET = 2208988800ULL;

// 检测程序是否处于试用期
class DateVerify
{
private:
    bool m_hasVerified = false;
    bool m_lastTimeTampered = true;  // 上次检测结果：true=被篡改
    time_t m_lastVerifyLocalTime = 0;
    time_t m_lastNetworkTime = 0;
    static const int VERIFY_INTERVAL = 6 * 3600;  // 6小时

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
        for (int i = 0; i < NTP_SERVER_COUNT && result == 0; i++) {
            result = getTimeFromServer(NTP_SERVERS[i]);
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

public:

    // 检测本地时间是否被篡改（用于授权验证）
    // toleranceDays: 允许的时间偏差天数，超过此值视为篡改
    // 返回值: true=时间被篡改或无法验证, false=时间正常
    bool isTimeTampered(int toleranceDays = 1)
    {
        time_t currentLocalTime = time(nullptr);

        // 检查是否可以使用缓存
        if (m_hasVerified) {
            time_t localElapsed = currentLocalTime - m_lastVerifyLocalTime;

            // 本地时间在合理范围内前进，使用缓存推算
            if (localElapsed >= 0 && localElapsed < VERIFY_INTERVAL) {
                time_t estimatedNetworkTime = m_lastNetworkTime + localElapsed;
                double diffDays = difftime(estimatedNetworkTime, currentLocalTime) / 86400.0;
                if (fabs(diffDays) <= toleranceDays) {
                    return false;  // 未篡改
                }
            }
        }

        // 执行网络验证
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) {
            // 网络不可用：如果之前验证通过且本地时间没异常，暂时信任
            if (m_hasVerified && !m_lastTimeTampered) {
                time_t localElapsed = currentLocalTime - m_lastVerifyLocalTime;
                // 允许5分钟回拨和24小时内的前进
                if (localElapsed >= -300 && localElapsed < 24 * 3600) {
                    return false;
                }
            }
            return false;  // 无法验证，视为篡改
        }

        // 更新缓存
        m_hasVerified = true;
        m_lastVerifyLocalTime = currentLocalTime;
        m_lastNetworkTime = networkTime;

        double diffDays = difftime(networkTime, currentLocalTime) / 86400.0;
        m_lastTimeTampered = fabs(diffDays) > toleranceDays;

        return m_lastTimeTampered;
    }

    // 获取网络时间与本地时间的偏差（秒）
    // 返回值: 正数=本地时间落后, 负数=本地时间超前, 0=无法获取网络时间
    int getTimeOffset()
    {
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) return 0;
        return static_cast<int>(difftime(networkTime, time(nullptr)));
    }

    bool isTrial(int trialDays = 7)
    {
        if (isTimeTampered())
            return false;

        tm tmCompile = parseCompileDate(__DATE__), tmCurrent = getCurrentDate();

        // 计算天数差
        int daysDiff = daysBetweenDates(tmCompile, tmCurrent);

        return daysDiff <= trialDays;
    }

    // 保留旧函数名兼容性（已弃用）
    bool isTrail(int trailDays = 7) { return isTrial(trailDays); }
};
