#pragma once

#include "common/commands.h"

#define YAMA_PATH			"Software\\YAMA"
#define CLIENT_PATH			"Software\\ServerD11"

// ���ö�ȡ��: �ļ�����.
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

    // ��ȡ�������еĵ�һ������
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

// ���ö�ȡ��: ע�������.
class iniFile : public config
{
private:
    HKEY m_hRootKey;
    std::string m_SubKeyPath;

public:
    ~iniFile() {}

    iniFile(const std::string& path = YAMA_PATH)
    {
        m_hRootKey = HKEY_CURRENT_USER;
        m_SubKeyPath = path;
    }

    // д��������ʵ��дΪ�ַ���
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetStr(MainKey, SubKey, std::to_string(Data));
    }

    // д���ַ���
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

    // ��ȡ�ַ���
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

    // ��ȡ�������ȴ��ַ�����ת��
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
    ~binFile() {}

    binFile(const std::string& path = CLIENT_PATH)
    {
        m_hRootKey = HKEY_CURRENT_USER;
        m_SubKeyPath = path;
    }

    // д��������дΪ�����ƣ�
    bool SetInt(const std::string& MainKey, const std::string& SubKey, int Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(&Data), sizeof(int));
    }

    // д���ַ������Զ����Ʒ�ʽ��
    bool SetStr(const std::string& MainKey, const std::string& SubKey, const std::string& Data) override
    {
        return SetBinary(MainKey, SubKey, reinterpret_cast<const BYTE*>(Data.data()), static_cast<DWORD>(Data.size()));
    }

    // ��ȡ�ַ������Ӷ���������ת����
    std::string GetStr(const std::string& MainKey, const std::string& SubKey, const std::string& def = "") override
    {
        std::vector<BYTE> buffer;
        if (!GetBinary(MainKey, SubKey, buffer))
            return def;

        return std::string(buffer.begin(), buffer.end());
    }

    // ��ȡ�������Ӷ����ƽ�����
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
