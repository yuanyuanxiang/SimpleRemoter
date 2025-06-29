
// 2015Remote.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号
#include "common/iniFile.h"
#include "IOCPServer.h"

// CMy2015RemoteApp:
// 有关此类的实现，请参阅 2015Remote.cpp
//

class CMy2015RemoteApp : public CWinApp
{
public:
	CMy2015RemoteApp();
	config  *m_iniFile;
	HANDLE m_Mutex;
	Server* m_iocpServer;

	CImageList m_pImageList_Large;  //系统大图标
	CImageList m_pImageList_Small;	//系统小图标

// 重写
public:
	virtual BOOL InitInstance();

// 实现

	DECLARE_MESSAGE_MAP()
	virtual int ExitInstance();
};

extern CMy2015RemoteApp theApp;

CMy2015RemoteApp* GetThisApp();

config& GetThisCfg();

#define THIS_APP GetThisApp()

#define THIS_CFG GetThisCfg()
