///\file
///\brief interface file for psycle::host::CKeyConfigDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

/// key config window.
class CSpecialKeys : public CDialog
{
public:
	CSpecialKeys(CWnd* pParent = NULL);   // standard constructor
	enum { IDD = IDD_SPECIALKEYS };

	WORD key;
	WORD mod;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedOk();
};

}}
