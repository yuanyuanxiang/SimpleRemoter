
// 2015Remote.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������
#include "common/iniFile.h"
#include "IOCPServer.h"

// CMy2015RemoteApp:
// �йش����ʵ�֣������ 2015Remote.cpp
//

class CMy2015RemoteApp : public CWinApp
{
public:
	CMy2015RemoteApp();
	config  *m_iniFile;
	HANDLE m_Mutex;
	Server* m_iocpServer;

	CImageList m_pImageList_Large;  //ϵͳ��ͼ��
	CImageList m_pImageList_Small;	//ϵͳСͼ��

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
	virtual int ExitInstance();
};

extern CMy2015RemoteApp theApp;

CMy2015RemoteApp* GetThisApp();

config& GetThisCfg();

#define THIS_APP GetThisApp()

#define THIS_CFG GetThisCfg()
