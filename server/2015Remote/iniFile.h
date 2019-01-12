#pragma once

class iniFile
{
public:
	BOOL ContructIniFile();
	int GetInt(CString MainKey,CString SubKey);
	BOOL SetInt(CString MainKey,CString SubKey,int Data);
	CString  m_IniFilePath;
	iniFile(void);
	~iniFile(void);
};
