// ExListBox.h : header file
// Code originally from ran wainstein:
// http://www.codeproject.com/combobox/cexlistboc.asp

#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

/////////////////////////////////////////////////////////////////////////////
// CExListBox window
class CExListBox : public CListBox
{
// Construction
public:
	CExListBox();
	virtual ~CExListBox();
	void ShowEditBox(bool isName);

public:
	CEdit myedit;
	bool isName;
protected:
	virtual void PreSubclassWindow();
	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO * pTI) const;

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillFocusPatternName();
	afx_msg void OnChangePatternName();
	afx_msg BOOL OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
};

}}
