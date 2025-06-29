// SystemDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "SystemDlg.h"
#include "afxdialogex.h"


// CSystemDlg �Ի���

typedef struct ItemData
{
	DWORD ID;
	CString Data[3];
	CString GetData(int index)const {
		return Data[index];
	}
}ItemData;

IMPLEMENT_DYNAMIC(CSystemDlg, CDialog)

	CSystemDlg::CSystemDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: DialogBase(CSystemDlg::IDD, pParent, IOCPServer, ContextObject, IDI_SERVICE)
{
	m_bHow= m_ContextObject->InDeCompressedBuffer.GetBYTE(0);
}

CSystemDlg::~CSystemDlg()
{
}

void CSystemDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_SYSTEM, m_ControlList);
}


BEGIN_MESSAGE_MAP(CSystemDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_SIZE()
	ON_NOTIFY(NM_RCLICK, IDC_LIST_SYSTEM, &CSystemDlg::OnNMRClickListSystem)
	ON_NOTIFY(HDN_ITEMCLICK, 0, &CSystemDlg::OnHdnItemclickList)
	ON_COMMAND(ID_PLIST_KILL, &CSystemDlg::OnPlistKill)
	ON_COMMAND(ID_PLIST_REFRESH, &CSystemDlg::OnPlistRefresh)
	ON_COMMAND(ID_WLIST_REFRESH, &CSystemDlg::OnWlistRefresh)
	ON_COMMAND(ID_WLIST_CLOSE, &CSystemDlg::OnWlistClose)
	ON_COMMAND(ID_WLIST_HIDE, &CSystemDlg::OnWlistHide)
	ON_COMMAND(ID_WLIST_RECOVER, &CSystemDlg::OnWlistRecover)
	ON_COMMAND(ID_WLIST_MAX, &CSystemDlg::OnWlistMax)
	ON_COMMAND(ID_WLIST_MIN, &CSystemDlg::OnWlistMin)
END_MESSAGE_MAP()


// CSystemDlg ��Ϣ�������


BOOL CSystemDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);
	CString str;
	m_bHow==TOKEN_PSLIST 
		? str.Format("%s - ���̹���", m_IPAddress)
		:str.Format("%s - ���ڹ���", m_IPAddress);
	SetWindowText(str);//���öԻ������

	m_ControlList.SetExtendedStyle(LVS_EX_FLATSB | LVS_EX_FULLROWSELECT); 
	if (m_bHow==TOKEN_PSLIST)      //���̹����ʼ���б�
	{
		m_ControlList.InsertColumn(0, "ӳ������", LVCFMT_LEFT, 180);
		m_ControlList.InsertColumn(1, "PID", LVCFMT_LEFT, 70);
		m_ControlList.InsertColumn(2, "����·��", LVCFMT_LEFT, 320);
		ShowProcessList();   //���ڵ�һ������������Ϣ��������Ž��̵��������԰�������ʾ���б���\0\0
	}else if (m_bHow==TOKEN_WSLIST)//���ڹ����ʼ���б�
	{
		//��ʼ�� ���ڹ�����б�
		m_ControlList.InsertColumn(0, "���", LVCFMT_LEFT, 80);
		m_ControlList.InsertColumn(1, "��������", LVCFMT_LEFT, 420);
		m_ControlList.InsertColumn(2, "����״̬", LVCFMT_LEFT, 200);
		ShowWindowsList();
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// �쳣: OCX ����ҳӦ���� FALSE
}

void CSystemDlg::ShowWindowsList(void)
{
	Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
	char *szBuffer = tmp.c_str();
	DWORD	dwOffset = 0;
	char	*szTitle = NULL;
	bool isDel=false;

	DeleteAllItems();
	CString	str;
	int i ;
	for ( i = 0; dwOffset <m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1; ++i)
	{
		LPDWORD	lpPID = LPDWORD(szBuffer + dwOffset);   //���ھ��
		szTitle = (char *)szBuffer + dwOffset + sizeof(DWORD);   //���ڱ���    
		str.Format("%5u", *lpPID);
		m_ControlList.InsertItem(i, str);
		m_ControlList.SetItemText(i, 1, szTitle);
		m_ControlList.SetItemText(i, 2, "��ʾ"); //(d) ������״̬��ʾΪ "��ʾ"
		// ItemData Ϊ���ھ��
		auto data = new ItemData{ *lpPID, {str, szTitle,"��ʾ"} };
		m_ControlList.SetItemData(i, (DWORD_PTR)data);  //(d)
		dwOffset += sizeof(DWORD) + lstrlen(szTitle) + 1;
	}
	str.Format("��������    ���ڸ�����%d��", i);   //�޸�CtrlList 
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT;
	lvc.pszText = str.GetBuffer(0);
	lvc.cchTextMax = str.GetLength();
	m_ControlList.SetColumn(1, &lvc);
}

void CSystemDlg::ShowProcessList(void)
{
	Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(1);
	char	*szBuffer = tmp.c_str(); //xiaoxi[][][][][]
	char	*szExeFile;
	char	*szProcessFullPath;
	DWORD	dwOffset = 0;
	CString str;
	DeleteAllItems();       
	//������������ÿһ���ַ��������������ݽṹ�� Id+������+0+������+0
	int i;
	for (i = 0; dwOffset < m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1; ++i)
	{
		LPDWORD	PID = LPDWORD(szBuffer + dwOffset);        //����õ�����ID
		szExeFile = szBuffer + dwOffset + sizeof(DWORD);      //����������ID֮�����
		szProcessFullPath = szExeFile + lstrlen(szExeFile) + 1;  //���������ǽ�����֮�����
		//�������ݽṹ�Ĺ���������

		m_ControlList.InsertItem(i, szExeFile);       //���õ������ݼ��뵽�б���
		str.Format("%5u", *PID);
		m_ControlList.SetItemText(i, 1, str);
		m_ControlList.SetItemText(i, 2, szProcessFullPath);
		// ItemData Ϊ����ID
		auto data = new ItemData{ *PID, {szExeFile, str, szProcessFullPath} };
		m_ControlList.SetItemData(i, DWORD_PTR(data));

		dwOffset += sizeof(DWORD) + lstrlen(szExeFile) + lstrlen(szProcessFullPath) + 2;   //����������ݽṹ ������һ��ѭ��
	}

	str.Format("������� / %d", i); 
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT;
	lvc.pszText = str.GetBuffer(0);
	lvc.cchTextMax = str.GetLength();
	m_ControlList.SetColumn(2, &lvc); //���б�����ʾ�ж��ٸ�����
}


void CSystemDlg::OnClose()
{
	CancelIO();
	// �ȴ����ݴ������
	if (IsProcessing()) {
		ShowWindow(SW_HIDE);
		return;
	}

	DeleteAllItems();
	DialogBase::OnClose();
}

// �ͷ���Դ�Ժ������
void  CSystemDlg::DeleteAllItems() {
	for (int i = 0; i < m_ControlList.GetItemCount(); i++)
	{
		auto data = (ItemData*)m_ControlList.GetItemData(i);
		if (NULL != data) {
			delete data;
		}
	}
	m_ControlList.DeleteAllItems();
}

int CALLBACK CSystemDlg::CompareFunction(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
	auto* pSortInfo = reinterpret_cast<std::pair<int, bool>*>(lParamSort);
	int nColumn = pSortInfo->first;
	bool bAscending = pSortInfo->second;

	// ��ȡ��ֵ
	ItemData* context1 = (ItemData*)lParam1;
	ItemData* context2 = (ItemData*)lParam2;
	CString s1 = context1->GetData(nColumn);
	CString s2 = context2->GetData(nColumn);

	int result = s1.Compare(s2);
	return bAscending ? result : -result;
}

void CSystemDlg::SortByColumn(int nColumn) {
	static int m_nSortColumn = 0;
	static bool m_bSortAscending = false;
	if (nColumn == m_nSortColumn) {
		// ����������ͬһ�У��л�����˳��
		m_bSortAscending = !m_bSortAscending;
	}
	else {
		// �����л������в�����Ϊ����
		m_nSortColumn = nColumn;
		m_bSortAscending = true;
	}

	// ����������Ϣ
	std::pair<int, bool> sortInfo(m_nSortColumn, m_bSortAscending);
	m_ControlList.SortItems(CompareFunction, reinterpret_cast<LPARAM>(&sortInfo));
}

void CSystemDlg::OnHdnItemclickList(NMHDR* pNMHDR, LRESULT* pResult) {
	LPNMHEADER pNMHeader = reinterpret_cast<LPNMHEADER>(pNMHDR);
	int nColumn = pNMHeader->iItem; // ��ȡ�����������
	SortByColumn(nColumn);          // ����������
	*pResult = 0;
}

void CSystemDlg::OnNMRClickListSystem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CMenu	Menu;
	if (m_bHow==TOKEN_PSLIST)      //���̹����ʼ���б�
	{
		Menu.LoadMenu(IDR_PROCESS_LIST);
	}else if (m_bHow==TOKEN_WSLIST)
	{
		Menu.LoadMenu(IDR_WINDOW_LIST);
	}
	CMenu*	SubMenu = Menu.GetSubMenu(0);
	CPoint	Point;
	GetCursorPos(&Point);
	SubMenu->TrackPopupMenu(TPM_LEFTALIGN, Point.x, Point.y, this);

	*pResult = 0;
}

void CSystemDlg::OnPlistKill()
{
	CListCtrl	*ListCtrl = NULL;
	if (m_ControlList.IsWindowVisible())
		ListCtrl = &m_ControlList;
	else
		return;

	//[KILL][ID][ID][iD][ID]
	//���仺����
	LPBYTE szBuffer = (LPBYTE)LocalAlloc(LPTR, 1 + (ListCtrl->GetSelectedCount() * 4));//1.exe  4  ID   Handle
	//����������̵�����ͷ
	szBuffer[0] = COMMAND_KILLPROCESS; 
	//��ʾ������Ϣ
	char *szTips = "����: ��ֹ���̻ᵼ�²�ϣ�������Ľ����\n"
		"�������ݶ�ʧ��ϵͳ���ȶ����ڱ���ֹǰ��\n"
		"���̽�û�л��ᱣ����״̬�����ݡ�";
	CString str;
	if (ListCtrl->GetSelectedCount() > 1)
	{
		str.Format("%sȷʵ\n����ֹ��%d�������?", szTips, ListCtrl->GetSelectedCount());	
	}
	else
	{
		str.Format("%sȷʵ\n����ֹ���������?", szTips);
	}
	if (::MessageBox(m_hWnd, str, "���̽�������", MB_YESNO | MB_ICONQUESTION) == IDNO) {
		LocalFree(szBuffer);
		return;
	}

	DWORD	dwOffset = 1;
	POSITION Pos = ListCtrl->GetFirstSelectedItemPosition(); 
	//�õ�Ҫ�����ĸ�����
	while(Pos) 
	{
		int	nItem = ListCtrl->GetNextSelectedItem(Pos);
		auto data = (ItemData*)ListCtrl->GetItemData(nItem);
		DWORD dwProcessID = data->ID;
		memcpy(szBuffer + dwOffset, &dwProcessID, sizeof(DWORD));  //sdkfj101112
		dwOffset += sizeof(DWORD);
	}
	//�������ݵ����ض��ڱ��ض��в���COMMAND_KILLPROCESS�������ͷ
	m_ContextObject->Send2Client(szBuffer, LocalSize(szBuffer));
	LocalFree(szBuffer);

	Sleep(100);

	OnPlistRefresh();
}


VOID CSystemDlg::OnPlistRefresh()
{
	if (m_ControlList.IsWindowVisible())
	{
		DeleteAllItems();
		GetProcessList();
		ShowProcessList();
	}
}


VOID CSystemDlg::GetProcessList(void)
{
	BYTE bToken = COMMAND_PSLIST;
	m_ContextObject->Send2Client(&bToken, 1);
}


void CSystemDlg::OnWlistRefresh()
{
	GetWindowsList();
}


void CSystemDlg::GetWindowsList(void)
{
	BYTE bToken = COMMAND_WSLIST;
	m_ContextObject->Send2Client(&bToken, 1);
}


void CSystemDlg::OnReceiveComplete(void)
{
	switch (m_ContextObject->InDeCompressedBuffer.GetBYTE(0))
	{
	case TOKEN_PSLIST:
		{
			ShowProcessList();

			break;
		}
	case TOKEN_WSLIST:
		{
			ShowWindowsList();
			break;
		}

	default:
		// ���䷢���쳣����
		break;
	}
}


void CSystemDlg::OnWlistClose()
{
	BYTE lpMsgBuf[20];
	CListCtrl	*pListCtrl = NULL;
	pListCtrl = &m_ControlList;

	int	nItem = pListCtrl->GetSelectionMark();
	if (nItem>=0)
	{

		ZeroMemory(lpMsgBuf,20);
		lpMsgBuf[0]=CMD_WINDOW_CLOSE;           //ע������������ǵ�����ͷ
		auto data = (ItemData*)pListCtrl->GetItemData(nItem);
		DWORD hwnd = data->ID; //�õ����ڵľ��һͬ����  4   djfkdfj  dkfjf  4
		memcpy(lpMsgBuf+1,&hwnd,sizeof(DWORD));   //1 4
		m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));			

	}
}


void CSystemDlg::OnWlistHide()
{
	BYTE lpMsgBuf[20];
	CListCtrl	*pListCtrl = NULL;
	pListCtrl = &m_ControlList;

	int	nItem = pListCtrl->GetSelectionMark();
	if (nItem>=0)
	{
		ZeroMemory(lpMsgBuf,20);
		lpMsgBuf[0]=CMD_WINDOW_TEST;             //���ڴ�������ͷ
		auto data = (ItemData*)pListCtrl->GetItemData(nItem); 
		DWORD hwnd = data->ID;  //�õ����ڵľ��һͬ����
		pListCtrl->SetItemText(nItem,2,"����");      //ע����ʱ���б��е���ʾ״̬Ϊ"����"
		//������ɾ���б���Ŀʱ�Ͳ�ɾ�������� ���ɾ������ھ���ᶪʧ ����ԶҲ������ʾ��
		memcpy(lpMsgBuf+1,&hwnd,sizeof(DWORD));      //�õ����ڵľ��һͬ����
		DWORD dHow=SW_HIDE;                          //���ڴ������ 0
		memcpy(lpMsgBuf+1+sizeof(hwnd),&dHow,sizeof(DWORD));
		m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));	
	}
}


void CSystemDlg::OnWlistRecover()
{
	BYTE lpMsgBuf[20];
	CListCtrl	*pListCtrl = NULL;
	pListCtrl = &m_ControlList;

	int	nItem = pListCtrl->GetSelectionMark();
	if (nItem>=0)
	{
		ZeroMemory(lpMsgBuf,20);
		lpMsgBuf[0]= CMD_WINDOW_TEST;
		auto data = (ItemData*)pListCtrl->GetItemData(nItem);
		DWORD hwnd = data->ID;
		pListCtrl->SetItemText(nItem,2,"��ʾ");
		memcpy(lpMsgBuf+1,&hwnd,sizeof(DWORD));
		DWORD dHow=SW_NORMAL;
		memcpy(lpMsgBuf+1+sizeof(hwnd),&dHow,sizeof(DWORD));
		m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));	
	}
}


void CSystemDlg::OnWlistMax()
{
	BYTE lpMsgBuf[20];
	CListCtrl	*pListCtrl = NULL;
	pListCtrl = &m_ControlList;

	int	nItem = pListCtrl->GetSelectionMark();
	if (nItem>=0)
	{
		ZeroMemory(lpMsgBuf,20);
		lpMsgBuf[0]= CMD_WINDOW_TEST;
		auto data = (ItemData*)pListCtrl->GetItemData(nItem);
		DWORD hwnd = data->ID;
		pListCtrl->SetItemText(nItem,2,"��ʾ");
		memcpy(lpMsgBuf+1,&hwnd,sizeof(DWORD));
		DWORD dHow=SW_MAXIMIZE;
		memcpy(lpMsgBuf+1+sizeof(hwnd),&dHow,sizeof(DWORD));
		m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));	
	}
}


void CSystemDlg::OnWlistMin()
{
	BYTE lpMsgBuf[20];
	CListCtrl	*pListCtrl = NULL;
	pListCtrl = &m_ControlList;

	int	nItem = pListCtrl->GetSelectionMark();
	if (nItem>=0)
	{
		ZeroMemory(lpMsgBuf,20);
		lpMsgBuf[0]= CMD_WINDOW_TEST;
		auto data = (ItemData*)pListCtrl->GetItemData(nItem);
		DWORD hwnd = data->ID;
		pListCtrl->SetItemText(nItem,2,"��ʾ");
		memcpy(lpMsgBuf+1,&hwnd,sizeof(DWORD));
		DWORD dHow=SW_MINIMIZE;
		memcpy(lpMsgBuf+1+sizeof(hwnd),&dHow,sizeof(DWORD));
		m_ContextObject->Send2Client(lpMsgBuf, sizeof(lpMsgBuf));	
	}
}

void CSystemDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (!m_ControlList.GetSafeHwnd()) return; // ȷ���ؼ��Ѵ���

	// ������λ�úʹ�С
	CRect rc;
	m_ControlList.GetWindowRect(&rc);
	ScreenToClient(&rc);

	// �������ÿؼ���С
	m_ControlList.MoveWindow(0, 0, cx, cy, TRUE);
}
