// ServicesManager.h: interface for the CServicesManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SERVICESMANAGER_H__02181EAA_CF77_42DD_8752_D809885D5F08__INCLUDED_)
#define AFX_SERVICESMANAGER_H__02181EAA_CF77_42DD_8752_D809885D5F08__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Manager.h"

class CServicesManager : public CManager  
{
public:
	CServicesManager(IOCPClient* ClientObject, int n);
	virtual ~CServicesManager();
	VOID SendServicesList();
	LPBYTE GetServicesList();
	VOID  OnReceive(PBYTE szBuffer, ULONG ulLength);
	void ServicesConfig(PBYTE szBuffer, ULONG ulLength);
	SC_HANDLE m_hscManager;
};

#endif // !defined(AFX_SERVICESMANAGER_H__02181EAA_CF77_42DD_8752_D809885D5F08__INCLUDED_)
