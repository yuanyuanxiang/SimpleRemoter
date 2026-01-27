// CRcEditDlg.cpp: 实现文件
//

#include "stdafx.h"
#include "CRcEditDlg.h"
#include "afxdialogex.h"
#include "Resource.h"


// CRcEditDlg 对话框

IMPLEMENT_DYNAMIC(CRcEditDlg, CDialogEx)

CRcEditDlg::CRcEditDlg(CWnd* pParent /*=nullptr*/)
    : CDialogLangEx(IDD_DIALOG_RCEDIT, pParent)
    , m_sExePath(_T(""))
    , m_sIcoPath(_T(""))
{

}

CRcEditDlg::~CRcEditDlg()
{
}

void CRcEditDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_EXE_FILE, m_EditExe);
    DDX_Control(pDX, IDC_EDIT_ICO_FILE, m_EditIco);
    DDX_Text(pDX, IDC_EDIT_EXE_FILE, m_sExePath);
    DDV_MaxChars(pDX, m_sExePath, 256);
    DDX_Text(pDX, IDC_EDIT_ICO_FILE, m_sIcoPath);
    DDV_MaxChars(pDX, m_sIcoPath, 256);
}


BEGIN_MESSAGE_MAP(CRcEditDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_SELECT_EXE, &CRcEditDlg::OnBnClickedBtnSelectExe)
    ON_BN_CLICKED(IDC_BTN_SELECT_ICO, &CRcEditDlg::OnBnClickedBtnSelectIco)
END_MESSAGE_MAP()


// CRcEditDlg 消息处理程序


BOOL CRcEditDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // TODO:  在此添加额外的初始化

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}


void CRcEditDlg::OnOK()
{
    if (m_sExePath.IsEmpty()) {
        MessageBoxL("请选择目标应用程序!", "提示", MB_ICONINFORMATION);
        return;
    }
    if (m_sIcoPath.IsEmpty()) {
        MessageBoxL("请选择[*.ico]图标文件!", "提示", MB_ICONINFORMATION);
        return;
    }
    std::string ReleaseEXE(int resID, const char* name);
    int run_cmd(std::string cmdLine);

    std::string rcedit = ReleaseEXE(IDR_BIN_RCEDIT, "rcedit.exe");
    if (rcedit.empty()) {
        MessageBoxL("解压程序失败，无法替换图标!", "提示", MB_ICONINFORMATION);
        return;
    }
    std::string exe = m_sExePath.GetString();
    std::string icon = m_sIcoPath.GetString();
    std::string cmdLine = "\"" + rcedit + "\" " + "\"" + exe + "\" --set-icon \"" + icon + "\"";
    int result = run_cmd(cmdLine);
    if (result) {
        MessageBoxL(CString("替换图标失败，错误代码: ") + std::to_string(result).c_str(),
                   "提示", MB_ICONINFORMATION);
        return;
    }

    __super::OnOK();
}


void CRcEditDlg::OnBnClickedBtnSelectExe()
{
    CFileDialog fileDlg(TRUE, _T("exe"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                        _T("EXE Files (*.exe)|*.exe|All Files (*.*)|*.*||"), AfxGetMainWnd());
    int ret = 0;
    try {
        ret = fileDlg.DoModal();
    } catch (...) {
        MessageBoxL("文件对话框未成功打开! 请稍后再试。", "提示", MB_ICONINFORMATION);
        return;
    }
    if (ret == IDOK) {
        m_sExePath = fileDlg.GetPathName();
        m_EditExe.SetWindowTextA(m_sExePath);
    }
}


void CRcEditDlg::OnBnClickedBtnSelectIco()
{
    CFileDialog fileDlg(TRUE, _T("ico"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                        _T("ICO Files (*.ico)|*.ico|All Files (*.*)|*.*||"), AfxGetMainWnd());
    int ret = 0;
    try {
        ret = fileDlg.DoModal();
    } catch (...) {
        MessageBoxL("文件对话框未成功打开! 请稍后再试。", "提示", MB_ICONINFORMATION);
        return;
    }
    if (ret == IDOK) {
        m_sIcoPath = fileDlg.GetPathName();
        m_EditIco.SetWindowTextA(m_sIcoPath);
    }
}
