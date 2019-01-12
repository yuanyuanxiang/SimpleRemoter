#include "StdAfx.h"
#include "iniFile.h"

iniFile::iniFile(void)
{
	ContructIniFile();
}

BOOL iniFile::ContructIniFile()
{
	char  szFilePath[MAX_PATH] = {0}, *p = szFilePath;
	::GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, "settings.ini");

	m_IniFilePath = szFilePath;

	return TRUE;
}

int iniFile::GetInt(CString MainKey,CString SubKey)
{
	return ::GetPrivateProfileInt(MainKey, SubKey,0,m_IniFilePath);
}

BOOL iniFile::SetInt(CString MainKey,CString SubKey,int Data)
{
	CString strData;
	strData.Format("%d", Data);
	return ::WritePrivateProfileString(MainKey, SubKey,strData,m_IniFilePath);
}

iniFile::~iniFile(void)
{
}
