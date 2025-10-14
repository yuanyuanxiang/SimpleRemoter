#include "stdafx.h"
#include "CDrawingBoard.h"
#include "afxdialogex.h"
#include "Resource.h"

IMPLEMENT_DYNAMIC(CDrawingBoard, CDialog)

CDrawingBoard::CDrawingBoard(CWnd* pParent, Server* IOCPServer, CONTEXT_OBJECT* ContextObject)
    : DialogBase(IDD_DRAWING_BOARD, pParent, IOCPServer, ContextObject, IDI_ICON_DRAWING),
      m_bDrawing(false)
{
    m_bTopMost = true;
    m_bTransport = true;
    m_bMoving = false;
    m_bSizing = false;
    m_pInputEdit = nullptr;
}

CDrawingBoard::~CDrawingBoard()
{
    if (m_pInputEdit != nullptr) {
        m_pInputEdit->DestroyWindow();
        delete m_pInputEdit;
        m_pInputEdit = nullptr;
    }
    if (m_font.GetSafeHandle())
        m_font.DeleteObject();
    if (m_pen.GetSafeHandle())
        m_pen.DeleteObject();
}

void CDrawingBoard::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDrawingBoard, CDialog)
    ON_WM_CLOSE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_WINDOWPOSCHANGED()
    ON_WM_RBUTTONDOWN()
    ON_COMMAND(ID_DRAWING_TOPMOST, &CDrawingBoard::OnDrawingTopmost)
    ON_COMMAND(ID_DRAWING_TRANSPORT, &CDrawingBoard::OnDrawingTransport)
    ON_COMMAND(ID_DRAWING_MOVE, &CDrawingBoard::OnDrawingMove)
    ON_COMMAND(ID_DRAWING_SIZE, &CDrawingBoard::OnDrawingSize)
    ON_COMMAND(IDM_CLEAR_DRAWING, &CDrawingBoard::OnDrawingClear)
    ON_COMMAND(ID_DRAWING_TEXT, &CDrawingBoard::OnDrawingText)
END_MESSAGE_MAP()

void CDrawingBoard::OnReceiveComplete()
{
    // 接收时处理逻辑（暂空）
}

void CDrawingBoard::OnClose()
{
    CancelIO();
    if (IsProcessing()) {
        ShowWindow(SW_HIDE);
        return;
    }
    DialogBase::OnClose();
}

void CDrawingBoard::OnPaint()
{
    CPaintDC dc(this);

    CPen* pOldPen = dc.SelectObject(&m_pen);

    for (const auto& path : m_paths) {
        if (path.size() < 2) continue;

        dc.MoveTo(path[0]);
        for (size_t i = 1; i < path.size(); ++i)
            dc.LineTo(path[i]);
    }

    if (m_bDrawing && m_currentPath.size() >= 2) {
        dc.MoveTo(m_currentPath[0]);
        for (size_t i = 1; i < m_currentPath.size(); ++i)
            dc.LineTo(m_currentPath[i]);
    }

    CFont* pOldFont = dc.SelectObject(&m_font);
    dc.SetTextColor(RGB(0, 0, 0));
    dc.SetBkMode(TRANSPARENT);

    for (const auto& entry : m_Texts)
        dc.TextOut(entry.first.x, entry.first.y, entry.second);

    dc.SelectObject(pOldFont);

    dc.SelectObject(pOldPen);
}

void CDrawingBoard::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bDrawing = true;
    m_currentPath.clear();
    m_currentPath.push_back(point);

    SetCapture();
}

void CDrawingBoard::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDrawing) {
        m_currentPath.push_back(point);
        Invalidate(FALSE);

        // 发送当前点
        BYTE pkg[1 + sizeof(POINT)] = { CMD_DRAW_POINT };
        memcpy(pkg + 1, &point, sizeof(POINT));
        m_ContextObject->Send2Client((BYTE*)pkg, sizeof(pkg));
    }
}

void CDrawingBoard::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDrawing) {
        m_bDrawing = false;
        m_currentPath.push_back(point);
        ReleaseCapture();

        m_paths.push_back(m_currentPath);
        Invalidate();

        // 发送结束命令，表示当前路径完成
        BYTE endCmd = CMD_DRAW_END;
        m_ContextObject->Send2Client(&endCmd, 1);
    }
}

void CDrawingBoard::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
    CDialog::OnWindowPosChanged(lpwndpos);
    if (!m_bMoving) return;

    CRect rect;
    GetWindowRect(&rect);  // 获取当前窗口屏幕位置
    BYTE pkg[1 + sizeof(CRect)] = { CMD_MOVEWINDOW };
    if (!m_bSizing) {
        rect.right = rect.left;
        rect.bottom = rect.top;
    }
    memcpy(pkg + 1, &rect, sizeof(CRect));
    m_ContextObject->Send2Client((BYTE*)pkg, sizeof(pkg));
}

void CDrawingBoard::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (!m_bSizing) return;

    // 发送新的窗口尺寸到客户端
    int sizeData[2] = { cx, cy };
    BYTE pkg[sizeof(sizeData) + 1] = { CMD_SET_SIZE };
    memcpy(pkg + 1, &sizeData, sizeof(sizeData));
    m_ContextObject->Send2Client((PBYTE)pkg, sizeof(pkg));
}

void CDrawingBoard::OnDrawingTopmost()
{
    m_bTopMost = !m_bTopMost;
    BYTE cmd[2] = { CMD_TOPMOST, m_bTopMost };
    m_ContextObject->Send2Client((PBYTE)cmd, sizeof(cmd));
    HMENU hMenu = ::GetMenu(this->GetSafeHwnd());
    int n = m_bTopMost ? MF_CHECKED : MF_UNCHECKED;
    ::CheckMenuItem(hMenu, ID_DRAWING_TOPMOST, MF_BYCOMMAND | n);
}


void CDrawingBoard::OnDrawingTransport()
{
    m_bTransport = !m_bTransport;
    BYTE cmd[2] = { CMD_TRANSPORT, m_bTransport ? 150 : 255 };
    m_ContextObject->Send2Client((PBYTE)cmd, sizeof(cmd));
    HMENU hMenu = ::GetMenu(this->GetSafeHwnd());
    int n = m_bTransport ? MF_CHECKED : MF_UNCHECKED;
    ::CheckMenuItem(hMenu, ID_DRAWING_TRANSPORT, MF_BYCOMMAND | n);
}


void CDrawingBoard::OnDrawingMove()
{
    m_bMoving = !m_bMoving;
    HMENU hMenu = ::GetMenu(this->GetSafeHwnd());
    int cmd = m_bMoving ? MF_CHECKED : MF_UNCHECKED;
    ::CheckMenuItem(hMenu, ID_DRAWING_MOVE, MF_BYCOMMAND | cmd);
}


void CDrawingBoard::OnDrawingSize()
{
    m_bSizing = !m_bSizing;
    HMENU hMenu = ::GetMenu(this->GetSafeHwnd());
    int cmd = m_bSizing ? MF_CHECKED : MF_UNCHECKED;
    ::CheckMenuItem(hMenu, ID_DRAWING_SIZE, MF_BYCOMMAND | cmd);
}


BOOL CDrawingBoard::OnInitDialog()
{
    DialogBase::OnInitDialog();

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    CString str;
    str.Format("%s - 画板演示", m_IPAddress);
    SetWindowText(str);

    m_font.CreateFont(
        20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Arial"));

    m_pen.CreatePen(PS_SOLID, 2, RGB(0, 0, 0));

    HMENU hMenu = ::GetMenu(this->GetSafeHwnd());
    ::CheckMenuItem(hMenu, ID_DRAWING_TOPMOST, MF_BYCOMMAND | MF_CHECKED);
    ::CheckMenuItem(hMenu, ID_DRAWING_TRANSPORT, MF_BYCOMMAND | MF_CHECKED);

    return TRUE;
}


void CDrawingBoard::OnDrawingClear()
{
    m_paths.clear();
    m_currentPath.clear();
    m_Texts.clear();
    BYTE cmd[2] = { CMD_DRAW_CLEAR, 0 };
    m_ContextObject->Send2Client((PBYTE)cmd, sizeof(cmd));
    if (m_hWnd && IsWindow(m_hWnd))
        ::InvalidateRect(m_hWnd, NULL, TRUE);  // 重绘整个窗口，清除痕迹
}

void CDrawingBoard::OnRButtonDown(UINT nFlags, CPoint point)
{
    m_RightClickPos = point;			// 记录鼠标点
    ClientToScreen(&m_RightClickPos);	// 变为屏幕坐标

    CMenu menu;
    menu.LoadMenu(IDR_MENU_POPUP);
    CMenu* pSubMenu = menu.GetSubMenu(0);

    if (pSubMenu)
        pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, m_RightClickPos.x, m_RightClickPos.y, this);

    CDialog::OnRButtonDown(nFlags, point);
}


void CDrawingBoard::OnDrawingText()
{
    if (m_pInputEdit != nullptr) {
        m_pInputEdit->DestroyWindow();
        delete m_pInputEdit;
        m_pInputEdit = nullptr;
    }

    CPoint ptClient = m_RightClickPos;
    ScreenToClient(&ptClient);

    m_pInputEdit = new CEdit();
    m_pInputEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                         CRect(ptClient.x, ptClient.y, ptClient.x + 200, ptClient.y + 25),
                         this, 1234); // 控件 ID 可自定
    m_pInputEdit->SetFocus();
}

BOOL CDrawingBoard::PreTranslateMessage(MSG* pMsg)
{
    if (m_pInputEdit && pMsg->hwnd == m_pInputEdit->m_hWnd) {
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
            CString strText;
            m_pInputEdit->GetWindowText(strText);

            // TODO: 将文字位置和内容加入列表并触发绘图
            if (!strText.IsEmpty()) {
                CPoint ptClient = m_RightClickPos;
                ScreenToClient(&ptClient);
                m_Texts.push_back(std::make_pair(ptClient, strText));
                SendTextsToRemote(ptClient, strText);
            }

            m_pInputEdit->DestroyWindow();
            delete m_pInputEdit;
            m_pInputEdit = nullptr;

            return TRUE; // 吞掉回车
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CDrawingBoard::SendTextsToRemote(const CPoint& pt, const CString& text)
{
    if (text.IsEmpty()) return;

    // 1. 转换为 UTF-8
    // CString 是 TCHAR 类型，若项目是多字节字符集，则需先转为宽字符
#ifdef _UNICODE
    LPCWSTR lpWideStr = text;
#else
    // 从 ANSI 转为宽字符
    int wideLen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    std::wstring wideStr(wideLen, 0);
    MultiByteToWideChar(CP_ACP, 0, text, -1, &wideStr[0], wideLen);
    LPCWSTR lpWideStr = wideStr.c_str();
#endif

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, lpWideStr, -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 1) return;  // 空或失败

    std::vector<char> utf8Text(utf8Len - 1); // 去掉末尾的 \0
    WideCharToMultiByte(CP_UTF8, 0, lpWideStr, -1, utf8Text.data(), utf8Len - 1, NULL, NULL);

    // 2. 构造发送 buffer
    std::vector<char> buffer;
    buffer.push_back(CMD_DRAW_TEXT); // 自定义命令码

    buffer.insert(buffer.end(), (char*)&pt, (char*)&pt + sizeof(CPoint));
    buffer.insert(buffer.end(), utf8Text.begin(), utf8Text.end());

    // 3. 发送
    m_ContextObject->Send2Client((LPBYTE)buffer.data(), (ULONG)buffer.size());
}
