////////////////////////////////////////////////////////////////
// MSDN Magazine -- June 2005
// If this code works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// Compiles with Visual Studio .NET 2003 (V7.1) on Windows XP. Tab size=3.
//
#include "stdafx.h"
#include "InputDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////
// Note: Make sure nBufLen is big enough to hold your entire dialog template!
//
CDlgTemplateBuilder::CDlgTemplateBuilder(UINT nBufLen)
{
	m_pBuffer   = new WORD[nBufLen];
	m_pNext     = m_pBuffer;
	m_pEndBuf   = m_pNext + nBufLen;
}

CDlgTemplateBuilder::~CDlgTemplateBuilder()
{
	delete [] m_pBuffer;
}

//////////////////
// Create template (DLGTEMPLATE)
//
DLGTEMPLATE* CDlgTemplateBuilder::Begin(DWORD dwStyle, const CRect& rc,
	LPCTSTR text, DWORD dwStyleEx)
{
	ASSERT(m_pBuffer==m_pNext);			 // call Begin first and only once!

	DLGTEMPLATE* hdr = (DLGTEMPLATE*)m_pBuffer;
	hdr->style = dwStyle;					 // copy style..
	hdr->dwExtendedStyle = dwStyleEx;	 // ..and extended, too
	hdr->cdit = 0;								 // number of items: zero

	// Set dialog rectangle.
	CRect rcDlg = rc;
	hdr->x  = (short)rcDlg.left;
	hdr->y  = (short)rcDlg.top;
	hdr->cx = (short)rcDlg.Width();
	hdr->cy = (short)rcDlg.Height();

	// Append trailing items: menu, class, caption. I only use caption.
	m_pNext = (WORD*)(hdr+1);
	*m_pNext++ = 0;							 // menu (none)
	*m_pNext++ = 0;							 // dialog class (use standard)
	m_pNext = AddText(m_pNext, text);	 // append dialog caption

	ASSERT(m_pNext < m_pEndBuf);
	return hdr;
}

//////////////////
// Add dialog item (control).
//
void CDlgTemplateBuilder::AddItemTemplate(WORD wType, DWORD dwStyle,
	const CRect& rc, WORD nID, DWORD dwStyleEx)
{
	ASSERT(m_pNext < m_pEndBuf);

	// initialize DLGITEMTEMPLATE 
	DLGITEMTEMPLATE& it = *((DLGITEMTEMPLATE*)AlignDWORD(m_pNext));
	it.style = dwStyle;
	it.dwExtendedStyle = dwStyleEx;

	CRect rcDlg = rc;
	it.x  = (short)rcDlg.left;
	it.y  = (short)rcDlg.top;
	it.cx = (short)rcDlg.Width();
	it.cy = (short)rcDlg.Height();
	it.id = nID;

	// add class (none)
	m_pNext = (WORD*)(&it+1);
	*m_pNext++ = 0xFFFF;						 // next WORD is atom
	*m_pNext++ = wType;						 // ..atom identifier
	ASSERT(m_pNext < m_pEndBuf);			 // check not out of range

	// increment control/item count
	DLGTEMPLATE* hdr = (DLGTEMPLATE*)m_pBuffer;
	hdr->cdit++;
}

//////////////////
// Add dialog item (control).
//
void CDlgTemplateBuilder::AddItem(WORD wType, DWORD dwStyle,
	const CRect& rc, LPCTSTR text, WORD nID, DWORD dwStyleEx)
{
	AddItemTemplate(wType, dwStyle, rc, nID, dwStyleEx);
	m_pNext = AddText(m_pNext, text);	 // append title
	*m_pNext++ = 0;							 // no creation data
	ASSERT(m_pNext < m_pEndBuf);
}

//////////////////
// Add dialog item (control).
//
void CDlgTemplateBuilder::AddItem(WORD wType, DWORD dwStyle,
	const CRect& rc, WORD wResID, WORD nID, DWORD dwStyleEx)
{
	AddItemTemplate(wType, dwStyle, rc, nID, dwStyleEx);
	*m_pNext++ = 0xFFFF;						 // next is resource id
	*m_pNext++ = wResID;						 // ..here it is
	*m_pNext++ = 0;							 // no extra stuff
	ASSERT(m_pNext < m_pEndBuf);
}

//////////////////
// Append text to buffer. Convert to Unicode if necessary.
// Return pointer to next character after terminating NULL.
//
WORD* CDlgTemplateBuilder::AddText(WORD* buf, LPCTSTR text)
{
	if (text) {
		USES_CONVERSION;
		wcscpy((WCHAR*)buf, T2W((LPTSTR)text));
		buf += wcslen((WCHAR*)buf)+1;
	} else {
		*buf++ = 0;
	}
	return buf;
}

//////////////////
// Create string dialog. If no icon specified, use IDI_QUESTION. Note that
// the order in which the controls are added is the TAB order.
//
BOOL CInputDialog::Init(LPCTSTR caption, LPCTSTR prompt, CWnd* pParent, WORD nIDIcon)
{
	const int CXDIALOG  = 200;					 // dialog width
	const int DLGMARGIN = 7;					 // margins all around
	const int CYSTATIC  = 8;					 // height of static text
	const int CYEDIT    = 12;					 // height of edit control
	const int CYSPACE   = 5;					 // vertical space between controls
	const int CXBUTTON  = 40;					 // button width...
	const int CYBUTTON  = 15;					 // ..and height

	CDlgTemplateBuilder& dtb = m_dtb;
	CRect rc(
		CPoint(0,0),
		CSize(CXDIALOG, CYSTATIC + CYEDIT + CYBUTTON + 2*DLGMARGIN + 2*CYSPACE));

	// create dialog header
	DLGTEMPLATE* pTempl = dtb.Begin(WS_POPUPWINDOW|DS_MODALFRAME|WS_DLGFRAME,rc,caption);

	// shrink main rect by margins
	rc.DeflateRect(CSize(DLGMARGIN,DLGMARGIN));

	// create icon if needed
	if (nIDIcon) {
		if (nIDIcon >= (WORD)IDI_APPLICATION) {
			// if using a system icon, I load it here and set it in OnInitDialog
			// because can't specify system icon in template, only icons from
			// application resource file.
			m_hIcon = ::LoadIcon(NULL, MAKEINTRESOURCE(nIDIcon));
			nIDIcon = 0;
		} else {
			m_hIcon = NULL;
		}

		// The size is calculated in pixels, but it seems to work OK--???
		CSize sz(GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON));
		CRect rcIcon(rc.TopLeft(), sz);
		dtb.AddItem(CDlgTemplateBuilder::STATIC, // add icon 
			WS_VISIBLE|WS_CHILD|SS_LEFT|SS_ICON, rc, nIDIcon, IDICON);
		rc.left += sz.cx;  // shrink main rect by width of icon
	}

	// add prompt
	rc.bottom = rc.top + CYSTATIC;					// height = height of static
	dtb.AddItem(CDlgTemplateBuilder::STATIC,		// add it
		WS_VISIBLE|WS_CHILD|SS_LEFT, rc, prompt);

	// add edit control
	rc += CPoint(0, rc.Height() + CYSPACE);		// move below static
	rc.bottom = rc.top + CYEDIT;						// height = height of edit control
	dtb.AddItem(CDlgTemplateBuilder::EDIT,			// add it ES_AUTOHSCROLL must be add
		WS_VISIBLE|WS_CHILD|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL, rc, m_str, IDEDIT);

	// add OK button
	rc += CPoint(0, rc.Height() + CYSPACE);		// move below edit control
	rc.bottom = rc.top + CYBUTTON;					// height = button height
	rc.left   = rc.right - CXBUTTON;					// width  = button width
	rc -= CPoint(CXBUTTON + DLGMARGIN,0);			// move left one button width
	dtb.AddItem(CDlgTemplateBuilder::BUTTON,		// add it
		WS_VISIBLE|WS_CHILD|WS_TABSTOP|BS_DEFPUSHBUTTON, rc, _T("&OK"), IDOK);

	// add Cancel button
	rc += CPoint(CXBUTTON + DLGMARGIN,0);			// move right again
	dtb.AddItem(CDlgTemplateBuilder::BUTTON,		// add Cancel button
		WS_VISIBLE|WS_CHILD|WS_TABSTOP, rc, _T("&Cancel"), IDCANCEL);

	return InitModalIndirect(pTempl, pParent);
}

//////////////////
// Initialize dialog: if I loaded a system icon, set it in static control.
//
BOOL CInputDialog::OnInitDialog()
{
	if (m_hIcon) {
		CStatic* pStatic = (CStatic*)GetDlgItem(IDICON);
		ASSERT(pStatic);
		pStatic->SetIcon(m_hIcon);
	}
	return CDialog::OnInitDialog();
}

/////////////////
// User pressed OK: check for empty string if required flag is set.
//
void CInputDialog::OnOK()
{
	UpdateData(TRUE);
	if (m_bRequired && m_str.IsEmpty()) {
		MessageBeep(0);
		return; // don't quit dialog!
	}
	CDialog::OnOK();
}
