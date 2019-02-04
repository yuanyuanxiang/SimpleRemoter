#pragma once

class iniFile
{
public:
	BOOL ContructIniFile();
	int GetInt(CString MainKey,CString SubKey);
	BOOL SetInt(CString MainKey,CString SubKey,int Data);
	CString GetStr(CString MainKey,CString SubKey, CString def);
	BOOL SetStr(CString MainKey,CString SubKey,CString Data);
	CString  m_IniFilePath;
	iniFile(void);
	~iniFile(void);
};
