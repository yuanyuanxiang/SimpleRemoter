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


CString iniFile::GetStr(CString MainKey, CString SubKey, CString def)
{
	char buf[_MAX_PATH];
	::GetPrivateProfileString(MainKey, SubKey, def, buf, sizeof(buf), m_IniFilePath);
	return buf;
}


BOOL iniFile::SetStr(CString MainKey, CString SubKey, CString Data)
{
	return ::WritePrivateProfileString(MainKey, SubKey, Data, m_IniFilePath);
}

iniFile::~iniFile(void)
{
}
