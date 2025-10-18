#pragma once

#include <iostream>
#include <ctime>
#include <WinSock2.h>
#include <Windows.h>
#pragma comment(lib, "ws2_32.lib")

// �й���½�Ż���NTP�������б�
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

// �������Ƿ���������
class DateVerify
{
private:
    // ��ʼ��Winsock
    bool initWinsock()
    {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }

    // ��ָ��NTP��������ȡʱ��
    time_t getTimeFromServer(const char* server)
    {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return 0;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(NTP_PORT);

        // ����������
        hostent* host = gethostbyname(server);
        if (!host) {
            closesocket(sock);
            return 0;
        }
        serverAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr_list[0]);

        // ���ó�ʱ
        DWORD timeout = 2000; // 2�볬ʱ
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        // ׼��NTP�����
        char ntpPacket[48] = { 0 };
        ntpPacket[0] = 0x1B; // LI=0, VN=3, Mode=3

        // ��������
        if (sendto(sock, ntpPacket, sizeof(ntpPacket), 0,
                   (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            return 0;
        }

        // ������Ӧ
        if (recv(sock, ntpPacket, sizeof(ntpPacket), 0) <= 0) {
            closesocket(sock);
            return 0;
        }

        closesocket(sock);

        // ����NTPʱ��
        uint32_t ntpTime = ntohl(*((uint32_t*)(ntpPacket + 40)));
        return ntpTime - NTP_EPOCH_OFFSET;
    }

    // ��ȡ����ʱ�䣨���Զ����������
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

    // ���·���дת��Ϊ����(1-12)
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

    // ����__DATE__�ַ���Ϊtm�ṹ
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

    // ������������֮���������
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

    // ��ȡ��ǰ����
    tm getCurrentDate()
    {
        std::time_t now = std::time(nullptr);
        tm tmNow = *std::localtime(&now);
        tmNow.tm_hour = 0;
        tmNow.tm_min = 0;
        tmNow.tm_sec = 0;
        return tmNow;
    }

    // ��֤���������Ƿ��޸�
    bool isLocalDateModified()
    {
        time_t networkTime = getNetworkTimeInChina();
        if (networkTime == 0) {
            return true; // �޷���֤
        }

        time_t localTime = time(nullptr);
        double diffDays = difftime(networkTime, localTime) / 86400.0;

        // �����1��������������ӳٺ�ʱ�������أ�
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

        // ����������
        int daysDiff = daysBetweenDates(tmCompile, tmCurrent);

        return daysDiff <= trailDays;
    }
};
