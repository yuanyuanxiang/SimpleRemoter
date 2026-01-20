// CUpdateDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "afxdialogex.h"
#include "CUpdateDlg.h"
#include "resource.h"

// CUpdateDlg 对话框

IMPLEMENT_DYNAMIC(CUpdateDlg, CDialogEx)

CUpdateDlg::CUpdateDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_UPDATE, pParent)
	, m_nSelected(0)
{

}

CUpdateDlg::~CUpdateDlg()
{
}

void CUpdateDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_UPDATE_SELECT, m_ComboUpdateSelect);
	DDX_CBIndex(pDX, IDC_COMBO_UPDATE_SELECT, m_nSelected);
}


BEGIN_MESSAGE_MAP(CUpdateDlg, CDialogEx)
END_MESSAGE_MAP()


// CUpdateDlg 消息处理程序

BOOL CUpdateDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO:  在此添加额外的初始化
	m_ComboUpdateSelect.InsertString(0, _T("TestRun"));
	m_ComboUpdateSelect.InsertString(1, _T("Ghost"));
	m_ComboUpdateSelect.SetCurSel(1);

	return TRUE;
}
