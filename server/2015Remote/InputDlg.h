////////////////////////////////////////////////////////////////
// PixieLib(TM) Copyright 1997-2005 Paul DiLascia
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// Compiles with Visual Studio.NET 7.1 or greater. Set tabsize=3.
//

//////////////////
// Helper class to build a dialog template in memory. Only supports what's
// needed for CStringDialog.
//
class CDlgTemplateBuilder {
protected:
	WORD* m_pBuffer;							 // internal buffer holds dialog template
	WORD* m_pNext;								 // next WORD to copy stuff
	WORD* m_pEndBuf;							 // end of buffer

	// align ptr to nearest DWORD
	WORD* AlignDWORD(WORD* ptr) {
		ptr++;									 // round up to nearest DWORD
		LPARAM lp = (LPARAM)ptr;			 // convert to long
		lp &= 0xFFFFFFFC;						 // make sure on DWORD boundary
		return (WORD*)lp;
	}

	void AddItemTemplate(WORD wType, DWORD dwStyle, const CRect& rc,
		WORD nID, DWORD dwStyleEx);

public:
	// Windows predefined atom names
	enum { BUTTON=0x0080, EDIT, STATIC, LISTBOX, SCROLLBAR, COMBOBOX };

	CDlgTemplateBuilder(UINT nBufLen=1024);
	~CDlgTemplateBuilder();

	DLGTEMPLATE* GetTemplate() { return (DLGTEMPLATE*)m_pBuffer; }

	// functions to build the template
	DLGTEMPLATE* Begin(DWORD dwStyle, const CRect& rc, LPCTSTR caption, DWORD dwStyleEx=0);
	WORD* AddText(WORD* buf, LPCTSTR text);
	void AddItem(WORD wType, DWORD dwStyle, const CRect& rc,
		LPCTSTR text, WORD nID=-1, DWORD dwStyleEx=0);
	void AddItem(WORD wType, DWORD dwStyle, const CRect& rc,
		WORD nResID, WORD nID=-1, DWORD dwStyleEx=0);
};

//////////////////
// Class to implement a simple string input dialog. Kind of like MessageBox
// but it accepts a single string input from user. You provide the prompt. To
// use:
//
//		CStringDialog dlg;						// string dialog
//		dlg.m_bRequired = m_bRequired;		// if string is required
//		dlg.Init(_T("Title"), _T("Enter a string:"), this, IDI_QUESTION);
//		dlg.DoModal();								// run dialog
//		CString result = dlg.m_str;			// whatever the user typed
//
class CInputDialog : public CDialog {
public:
	CString	m_str;							 // the string returned [in,out]
	BOOL		m_bRequired;					 // string required?
	HICON		m_hIcon;							 // icon if not supplied

	CInputDialog() { }
	~CInputDialog() { }
	
	// Call this to create the template with given caption and prompt.
	BOOL Init(LPCTSTR caption, LPCTSTR prompt, CWnd* pParent=NULL,
		WORD nIDIcon=(WORD)IDI_QUESTION);
	
protected:
	CDlgTemplateBuilder m_dtb;				 // place to build/hold the dialog template
	enum { IDICON=1, IDEDIT };				 // control IDs
	
	// MFC virtual overrides
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void DoDataExchange(CDataExchange* pDX)
	{
		DDX_Text(pDX, IDEDIT, m_str);
	}
};

