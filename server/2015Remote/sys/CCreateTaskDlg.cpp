// CCreateTaskDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "CCreateTaskDlg.h"

// CCreateTaskDlg 对话框

IMPLEMENT_DYNAMIC(CCreateTaskDlg, CDialog)

CCreateTaskDlg::CCreateTaskDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_CREATETASK, pParent)
    , m_TaskPath(_T("\\"))
    , m_TaskNames(_T("bhyy"))
    , m_ExePath(_T("C:\\windows\\system32\\cmd.exe"))
    , m_Author(_T("Microsoft Corporation"))
    , m_Description(_T("此任务用于在需要时启动 Windows 更新服务以执行计划的操作(如扫描)"))
{
}

CCreateTaskDlg::~CCreateTaskDlg()
{
}

void CCreateTaskDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT_PATH, m_TaskPath);
    DDX_Control(pDX, IDC_EDIT_NAME, m_TaskName);
    DDX_Text(pDX, IDC_EDIT_NAME, m_TaskNames);
    DDX_Text(pDX, IDC_EDIT_EXEPATH, m_ExePath);
    DDX_Text(pDX, IDC_EDIT_MAKER, m_Author);
    DDX_Text(pDX, IDC_EDIT_TEXT, m_Description);
}


BEGIN_MESSAGE_MAP(CCreateTaskDlg, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_CREAT, &CCreateTaskDlg::OnBnClickedButtonCREAT)
END_MESSAGE_MAP()


// CCreateTaskDlg 消息处理程序


void CCreateTaskDlg::OnBnClickedButtonCREAT()
{
    UpdateData(TRUE);
    // TODO: 在此添加控件通知处理程序代码
    CDialog::OnOK();
}
