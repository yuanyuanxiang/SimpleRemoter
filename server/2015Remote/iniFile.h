#pragma once

class iniFile
{
public:
	BOOL iniFile::ContructIniFile();
	int iniFile::GetInt(CString MainKey,CString SubKey);
	BOOL iniFile::SetInt(CString MainKey,CString SubKey,int Data);
	CString  m_IniFilePath;
	iniFile(void);
	~iniFile(void);
};
