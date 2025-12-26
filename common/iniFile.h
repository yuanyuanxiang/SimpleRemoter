#pragma once

#include "common/commands.h"

#define YAMA_PATH			"Software\\YAMA"
#define CLIENT_PATH			GetRegistryName()

#define NO_CURRENTKEY 1

#if NO_CURRENTKEY
#include <wtsapi32.h>
#include <sddl.h>
#pragma comment(lib, "wtsapi32.lib")

#ifndef SAFE_CLOSE_HANDLE
#define SAFE_CLOSE_HANDLE(h) do{if((h)!=NULL&&(h)!=INVALID_HANDLE_VALUE){CloseHandle(h);(h)=NULL;}}while(0)
#endif

inline std::string GetExeDir()
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *lastSlash = '\0';

    CharLowerA(path);
    return path;
}

inline std::string GetExeHashStr()
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    CharLowerA(path);

    ULONGLONG hash = 14695981039346656037ULL;
    for (const char* p = path; *p; p++)
    {
        hash ^= (unsigned char)*p;
        hash *= 1099511628211ULL;
    }

    char result[17];
    sprintf_s(result, "%016llX", hash);
    return result;
}

static inline std::string GetRegistryName() {
    static auto name = "Software\\" + GetExeHashStr();
    return name;
}

// 获取当前会话用户的注册表根键
// SYSTEM 进程无法使用 HKEY_CURRENT_USER，需要通过 HKEY_USERS\<SID> 访问
// 返回的 HKEY 需要调用者在使用完毕后调用 RegCloseKey 关闭
inline HKEY GetCurrentUserRegistryKey()
{
    HKEY hUserKey = NULL;
    // 获取当前进程的会话 ID
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

    // 获取该会话的用户令牌
    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        // 如果失败（可能不是服务进程），回退到 HKEY_CURRENT_USER
        return HKEY_CURRENT_USER;
    }

    // 获取令牌中的用户信息大小
    DWORD dwSize = 0;
    GetTokenInformation(hUserToken, TokenUser, NULL, 0, &dwSize);
    if (dwSize == 0) {
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    // 分配内存并获取用户信息
    TOKEN_USER* pTokenUser = (TOKEN_USER*)malloc(dwSize);
    if (!pTokenUser) {
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    if (!GetTokenInformation(hUserToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        free(pTokenUser);
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    // 将 SID 转换为字符串
    LPSTR szSid = NULL;
    if (!ConvertSidToStringSidA(pTokenUser->User.Sid, &szSid)) {
        free(pTokenUser);
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    // 打开 HKEY_USERS\<SID>
    if (RegOpenKeyExA(HKEY_USERS, szSid, 0, KEY_READ | KEY_WRITE, &hUserKey) != ERROR_SUCCESS) {
        // 尝试只读方式
        if (RegOpenKeyExA(HKEY_USERS, szSid, 0, KEY_READ, &hUserKey) != ERROR_SUCCESS) {
            hUserKey = NULL;
        }
    }

    LocalFree(szSid);
    free(pTokenUser);
    SAFE_CLOSE_HANDLE(hUserToken);

    return hUserKey ? hUserKey : HKEY_CURRENT_USER;
}

// 检查是否需要关闭注册表根键（非预定义键需要关闭）
inline void CloseUserRegistryKeyIfNeeded(HKEY hKey)
{
    if (hKey != HKEY_CURRENT_USER &&
        hKey != HKEY_LOCAL_MACHINE &&
        hKey != HKEY_USERS &&
        hKey != HKEY_CLASSES_ROOT &&
        hKey != NULL) {
        RegCloseKey(hKey);
    }
}

#else
#define GetCurrentUserRegistryKey() HKEY_CURRENT_USER
#define CloseUserRegistryKeyIfNeeded(hKey)
#endif


// 配置读取类: 文件配置.
class config
{
private:
    char m_IniFilePath[_MAX_PATH] = { 0 };

public:
    virtual ~config() {}

    config(const std::string& path="")
    {
        if (path.length() == 0) {
            ::GetModuleFileNameA(NULL, m_IniFilePath, sizeof(m_IniFilePath));
            GET_FILEPATH(m_IniFilePath, "settings.ini");
        } else {
            memcpy(m_IniFilePath, path.c_str(), path.length());
        }
    }

    virtual int GetInt(const std::string& MainKey, const std::string& SubKey, int nDef=0)
    {
        return ::GetPrivateProfileIntA(MainKey.c_str(), SubKey.c_str(), nDef, m_IniFilePath);
    }

    // 获取配置项中的第一个整数
    virtual int Get1Int(const std::string& MainKey, const std::string& SubKey, char ch=';', int nDef=0)
    {
        std::string s = GetStr(MainKey, SubKey, "");
        s = StringToVector(s, ch)[0];
        return s.empty() ? nDef : atoi(s.c_str());
    }

    virtual bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data)
    {
        std::string strData = std::to_string(Data);
        return ::WritePrivateProfileStringA(MainKey.c_str(), SubKey.c_str(), strData.c_str(), m_IniFilePath);
    }

    virtual std::string GetStr(const std::string& MainKey, const std::string& SubKey, const std::string& def = "")
    {
        char buf[_MAX_PATH] = { 0 };
        ::GetPrivateProfileStringA(MainKey.c_str(), SubKey.c_str(), def.c_str(), buf, sizeof(buf), m_IniFilePath);
        return std::string(buf);
    }

    virtual bool SetStr(const std::string& MainKey, const std::string& SubKey, const std::string& Data)
    {
        return ::WritePrivateProfileStringA(MainKey.c_str(), SubKey.c_str(), Data.c_str(), m_IniFilePath);
    }
};

// 配置读取类: 注册表配置.
class iniFile : public config
{
private:
    HKEY m_hRootKey;
    std::string m_SubKeyPath;

public:
    ~iniFile()
    {
        CloseUserRegistryKeyIfNeeded(m_hRootKey);
    }

    iniFile(const std::string& path = YAMA_PATH)
    {
        m_hRootKey = GetCurrentUserRegistryKey();
        m_SubKeyPath = path;
        if (path != YAMA_PATH) {
            static std::string workSpace = GetExeDir();
            SetStr("settings", "work_space", workSpace);
        }
    }

    // 写入整数，实际写为字符串
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetStr(MainKey, SubKey, std::to_string(Data));
    }

    // 写入字符串
    bool SetStr(const std::string& MainKey, const std::string& SubKey, const std::string& Data) override
    {
        std::string fullPath = m_SubKeyPath + "\\" + MainKey;
        HKEY hKey;
        if (RegCreateKeyExA(m_hRootKey, fullPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
            return false;

        bool bRet = (RegSetValueExA(hKey, SubKey.c_str(), 0, REG_SZ,
                                    reinterpret_cast<const BYTE*>(Data.c_str()),
                                    static_cast<DWORD>(Data.size() + 1)) == ERROR_SUCCESS);
        RegCloseKey(hKey);
        return bRet;
    }

    // 读取字符串
    std::string GetStr(const std::string& MainKey, const std::string& SubKey, const std::string& def = "") override
    {
        std::string fullPath = m_SubKeyPath + "\\" + MainKey;
        HKEY hKey;
        char buffer[512] = { 0 };
        DWORD dwSize = sizeof(buffer);
        std::string result = def;

        if (RegOpenKeyExA(m_hRootKey, fullPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD dwType = REG_SZ;
            if (RegQueryValueExA(hKey, SubKey.c_str(), NULL, &dwType, reinterpret_cast<LPBYTE>(buffer), &dwSize) == ERROR_SUCCESS &&
                dwType == REG_SZ) {
                result = buffer;
            }
            RegCloseKey(hKey);
        }
        return result;
    }

    // 读取整数，先从字符串中转换
    int GetInt(const std::string& MainKey, const std::string& SubKey, int defVal = 0) override
    {
        std::string val = GetStr(MainKey, SubKey);
        if (val.empty())
            return defVal;

        try {
            return std::stoi(val);
        } catch (...) {
            return defVal;
        }
    }
};

class binFile : public config
{
private:
    HKEY m_hRootKey;
    std::string m_SubKeyPath;

public:
    ~binFile()
    {
        CloseUserRegistryKeyIfNeeded(m_hRootKey);
    }

    binFile(const std::string& path = CLIENT_PATH)
    {
        m_hRootKey = GetCurrentUserRegistryKey();
        m_SubKeyPath = path;
        if (path != YAMA_PATH) {
            static std::string workSpace = GetExeDir();
            SetStr("settings", "work_space", workSpace);
        }
    }

    // 写入整数（写为二进制）
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(&Data), sizeof(int));
    }

    // 写入字符串（以二进制方式）
    bool SetStr(const std::string& MainKey, const std::string& SubKey, const std::string& Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(Data.data()), static_cast<DWORD>(Data.size()));
    }

    // 读取字符串（从二进制数据转换）
    std::string GetStr(const std::string& MainKey, const std::string& SubKey, const std::string& def = "") override
    {
        std::vector<BYTE> buffer;
        if (!GetBinary(MainKey, SubKey, buffer))
            return def;

        return std::string(buffer.begin(), buffer.end());
    }

    // 读取整数（从二进制解析）
    int GetInt(const std::string& MainKey, const std::string& SubKey, int defVal = 0) override
    {
        std::vector<BYTE> buffer;
        if (!GetBinary(MainKey, SubKey, buffer) || buffer.size() < sizeof(int))
            return defVal;

        int value = 0;
        memcpy(&value, buffer.data(), sizeof(int));
        return value;
    }

private:
    bool SetBinary(const std::string& MainKey, const std::string& SubKey, const BYTE* data, DWORD size)
    {
        std::string fullPath = m_SubKeyPath + "\\" + MainKey;
        HKEY hKey;
        if (RegCreateKeyExA(m_hRootKey, fullPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
            return false;

        bool bRet = (RegSetValueExA(hKey, SubKey.c_str(), 0, REG_BINARY, data, size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
        return bRet;
    }

    bool GetBinary(const std::string& MainKey, const std::string& SubKey, std::vector<BYTE>& outData)
    {
        std::string fullPath = m_SubKeyPath + "\\" + MainKey;
        HKEY hKey;
        if (RegOpenKeyExA(m_hRootKey, fullPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            return false;

        DWORD dwType = 0;
        DWORD dwSize = 0;
        if (RegQueryValueExA(hKey, SubKey.c_str(), NULL, &dwType, NULL, &dwSize) != ERROR_SUCCESS || dwType != REG_BINARY) {
            RegCloseKey(hKey);
            return false;
        }

        outData.resize(dwSize);
        bool bRet = (RegQueryValueExA(hKey, SubKey.c_str(), NULL, NULL, outData.data(), &dwSize) == ERROR_SUCCESS);
        RegCloseKey(hKey);
        return bRet;
    }
};
