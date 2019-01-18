// RegisterManager.h: interface for the CRegisterManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGISTERMANAGER_H__2EFB2AB3_C6C9_454E_9BC7_AE35362C85FE__INCLUDED_)
#define AFX_REGISTERMANAGER_H__2EFB2AB3_C6C9_454E_9BC7_AE35362C85FE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"
#include "RegisterOperation.h"

class CRegisterManager : public CManager  
{
public:
	CRegisterManager(IOCPClient* ClientObject, int n);
	virtual ~CRegisterManager();
	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);
	VOID Find(char bToken, char *szPath);
};

#endif // !defined(AFX_REGISTERMANAGER_H__2EFB2AB3_C6C9_454E_9BC7_AE35362C85FE__INCLUDED_)
