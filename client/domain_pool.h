#pragma once
#include <string>
#include <vector>
#include <common/commands.h>

std::string GetIPAddress(const char* hostName);

class DomainPool
{
private:
    char Address[100]; // 此长度和CONNECT_ADDRESS定义匹配
    std::vector<std::string> IPList;
public:
    DomainPool()
    {
        memset(Address, 0, sizeof(Address));
    }
    DomainPool(const char* addr)
    {
        strcpy_s(Address, addr ? addr : "");
        IPList = StringToVector(Address, ';');
        for (int i = 0; i < IPList.size(); i++) {
            IPList[i] = GetIPAddress(IPList[i].c_str());
        }
    }
    std::string SelectIP() const
    {
        auto n = rand() % IPList.size();
        return IPList[n];
    }
    std::vector<std::string> GetIPList() const
    {
        return IPList;
    }
};
