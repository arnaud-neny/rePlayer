// ExListBox.cpp : implementation file
// Code originally from ran wainstein:
// http://www.codeproject.com/combobox/cexlistboc.asp

#include <psycle/host/detail/project.private.hpp>
#include "ExListBox.h"
#include "MainFrm.hpp"

#include "Song.hpp"

namespace psycle { namespace host {

/////////////////////////////////////////////////////////////////////////////
// CExListBox
CExListBox::CExListBox()
{
}

CExListBox::~CExListBox()
{
}

/*
BEGIN_MESSAGE_MAP(CExListBox, CListBox)
	ON_WM_CONTEXTMENU()
	ON_EN_CHANGE(IDC_SEQEDITBOX, OnChangePatternName)
	ON_EN_KILLFOCUS(IDC_SEQEDITBOX,OnKillFocusPatternName)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnToolTipText)
	ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnToolTipText)
END_MESSAGE_MAP()
*/
/////////////////////////////////////////////////////////////////////////////
// CExListBox message handlers

void CExListBox::PreSubclassWindow() 
{
	CListBox::PreSubclassWindow();
	EnableToolTips(TRUE);
}

void CExListBox::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_SEQUENCE_MENU));
	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, AfxGetMainWnd());

	menu.DestroyMenu();
}

void CExListBox::ShowEditBox(bool isName)
{
	this->isName = isName;
	int row = GetCurSel();

	HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
	CFont font;
	font.Attach( hFont );

	RECT cellrect;
	GetItemRect(row,&cellrect);

	myedit.DestroyWindow();
	myedit.Create(WS_CHILD|WS_BORDER|ES_AUTOHSCROLL,cellrect,this,IDC_SEQEDITBOX);
	myedit.SetFont(&font);
	if(isName) {
		myedit.SetLimitText(30);
		myedit.SetWindowText(Global::song().patternName[Global::song().playOrder[row]]);
	}
	else {
		myedit.SetLimitText(3);
		char bla[8];
		sprintf(bla, "%.2X", Global::song().playOrder[row]);
		myedit.SetWindowText(bla);
	}
	myedit.SetSel(0,-1);
	myedit.ShowWindow(SW_SHOWNORMAL);
	myedit.SetFocus();
}

void CExListBox::OnKillFocusPatternName()
{
	((CMainFrame*)GetParentFrame())->UpdateSequencer();
	myedit.DestroyWindow();
}

void CExListBox::OnChangePatternName()
{
	CString string;
	myedit.GetWindowText(string);
	std::stringstream ss;
	ss << string;
	if(isName) {
		strncpy(Global::song().patternName[Global::song().playOrder[GetCurSel()]],ss.str().c_str(),32);
	}
	else {
		int val = hexstring_to_integer(ss.str());
		if(val < MAX_PATTERNS) {
			Global::song().playOrder[GetCurSel()] = val;
		}
	}
}
INT_PTR CExListBox::OnToolHitTest(CPoint point, TOOLINFO * pTI) const
{
	int row;
	RECT cellrect;   // cellrect		- to hold the bounding rect
	BOOL outside = FALSE;
	row  = ItemFromPoint(point,outside);  //we call the ItemFromPoint function to determine the row,
	if ( outside ) 
		return -1;

	//set up the TOOLINFO structure. GetItemRect(row,&cellrect);
	GetItemRect(row,&cellrect);
	pTI->rect = cellrect;
	pTI->hwnd = m_hWnd;
	pTI->uId = (UINT)((row));   //The ‘uId’ is assigned a value according to the row value.
	pTI->lpszText = LPSTR_TEXTCALLBACK;
	return pTI->uId;
}


//Define OnToolTipText(). This is the handler for the TTN_NEEDTEXT notification from 
//support ansi and unicode 
BOOL CExListBox::OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
/*
	if (GetCount() == 0)
		return TRUE;
	// need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	CString strTipText = Global::song().patternName[Global::song().playOrder[pNMHDR->idFrom]];
//	UINT nID = pNMHDR->idFrom;
//	GetText( nID ,strTipText);

#ifndef _UNICODE
	if (pNMHDR->code == TTN_NEEDTEXTA)
		lstrcpyn(pTTTA->szText, strTipText, 80);
	else
		_mbstowcsz(pTTTW->szText, strTipText, 80);
#else
	if (pNMHDR->code == TTN_NEEDTEXTA)
		_wcstombsz(pTTTA->szText, strTipText, 80);
	else
		lstrcpyn(pTTTW->szText, strTipText, 80);
#endif
	*pResult = 0;
*/

	return TRUE;    
}

}}
