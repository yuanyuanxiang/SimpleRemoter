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

// 鑾峰彇褰撳墠浼氳瘽鐢ㄦ埛鐨勬敞鍐岃〃鏍归敭
// SYSTEM 杩涚▼鏃犳硶浣跨敤 HKEY_CURRENT_USER锛岄渶瑕侀€氳繃 HKEY_USERS\<SID> 璁块棶
// 杩斿洖鐨?HKEY 闇€瑕佽皟鐢ㄨ€呭湪浣跨敤瀹屾瘯鍚庤皟鐢?RegCloseKey 鍏抽棴
inline HKEY GetCurrentUserRegistryKey()
{
    HKEY hUserKey = NULL;
    // 鑾峰彇褰撳墠杩涚▼鐨勪細璇?ID
    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

    // 鑾峰彇璇ヤ細璇濈殑鐢ㄦ埛浠ょ墝
    HANDLE hUserToken = NULL;
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        // 濡傛灉澶辫触锛堝彲鑳戒笉鏄湇鍔¤繘绋嬶級锛屽洖閫€鍒?HKEY_CURRENT_USER
        return HKEY_CURRENT_USER;
    }

    // 鑾峰彇浠ょ墝涓殑鐢ㄦ埛淇℃伅澶у皬
    DWORD dwSize = 0;
    GetTokenInformation(hUserToken, TokenUser, NULL, 0, &dwSize);
    if (dwSize == 0) {
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    // 鍒嗛厤鍐呭瓨骞惰幏鍙栫敤鎴蜂俊鎭?
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

    // 灏?SID 杞崲涓哄瓧绗︿覆
    LPSTR szSid = NULL;
    if (!ConvertSidToStringSidA(pTokenUser->User.Sid, &szSid)) {
        free(pTokenUser);
        SAFE_CLOSE_HANDLE(hUserToken);
        return HKEY_CURRENT_USER;
    }

    // 鎵撳紑 HKEY_USERS\<SID>
    if (RegOpenKeyExA(HKEY_USERS, szSid, 0, KEY_READ | KEY_WRITE, &hUserKey) != ERROR_SUCCESS) {
        // 灏濊瘯鍙鏂瑰紡
        if (RegOpenKeyExA(HKEY_USERS, szSid, 0, KEY_READ, &hUserKey) != ERROR_SUCCESS) {
            hUserKey = NULL;
        }
    }

    LocalFree(szSid);
    free(pTokenUser);
    SAFE_CLOSE_HANDLE(hUserToken);

    return hUserKey ? hUserKey : HKEY_CURRENT_USER;
}

// 妫€鏌ユ槸鍚﹂渶瑕佸叧闂敞鍐岃〃鏍归敭锛堥潪棰勫畾涔夐敭闇€瑕佸叧闂級
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


// 閰嶇疆璇诲彇绫? 鏂囦欢閰嶇疆.
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

    // 鑾峰彇閰嶇疆椤逛腑鐨勭涓€涓暣鏁?
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

// 閰嶇疆璇诲彇绫? 娉ㄥ唽琛ㄩ厤缃?
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

    // 鍐欏叆鏁存暟锛屽疄闄呭啓涓哄瓧绗︿覆
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetStr(MainKey, SubKey, std::to_string(Data));
    }

    // 鍐欏叆瀛楃涓?
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

    // 璇诲彇瀛楃涓?
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

    // 璇诲彇鏁存暟锛屽厛浠庡瓧绗︿覆涓浆鎹?
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

    // 鍐欏叆鏁存暟锛堝啓涓轰簩杩涘埗锛?
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(&Data), sizeof(int));
    }

    // 鍐欏叆瀛楃涓诧紙浠ヤ簩杩涘埗鏂瑰紡锛?
    bool SetStr(const std::string& MainKey, const std::string& SubKey, const std::string& Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(Data.data()), static_cast<DWORD>(Data.size()));
    }

    // 璇诲彇瀛楃涓诧紙浠庝簩杩涘埗鏁版嵁杞崲锛?
    std::string GetStr(const std::string& MainKey, const std::string& SubKey, const std::string& def = "") override
    {
        std::vector<BYTE> buffer;
        if (!GetBinary(MainKey, SubKey, buffer))
            return def;

        return std::string(buffer.begin(), buffer.end());
    }

    // 璇诲彇鏁存暟锛堜粠浜岃繘鍒惰В鏋愶級
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
