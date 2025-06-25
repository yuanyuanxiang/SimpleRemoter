#pragma once

#include "common/commands.h"

#define YAMA_PATH			"Software\\YAMA"
#define CLIENT_PATH			"Software\\ServerD11"

class config
{
private:
	char m_IniFilePath[_MAX_PATH];

public:
	virtual ~config() {}

	config()
	{
		::GetModuleFileNameA(NULL, m_IniFilePath, sizeof(m_IniFilePath));
		GET_FILEPATH(m_IniFilePath, "settings.ini");
	}

	virtual int GetInt(const std::string& MainKey, const std::string& SubKey, int nDef=0)
	{
		return ::GetPrivateProfileIntA(MainKey.c_str(), SubKey.c_str(), nDef, m_IniFilePath);
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

		if (RegOpenKeyExA(m_hRootKey, fullPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD dwType = REG_SZ;
			if (RegQueryValueExA(hKey, SubKey.c_str(), NULL, &dwType, reinterpret_cast<LPBYTE>(buffer), &dwSize) == ERROR_SUCCESS &&
				dwType == REG_SZ)
			{
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
		}
		catch (...) {
			return defVal;
		}
	}
};
