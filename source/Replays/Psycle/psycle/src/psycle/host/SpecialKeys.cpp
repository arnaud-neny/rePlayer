
#include <psycle/host/detail/project.private.hpp>
#include "SpecialKeys.hpp"
#include "CmdDef.hpp"

namespace psycle { namespace host {

CSpecialKeys::CSpecialKeys(CWnd* pParent /* = 0 */) : CDialog(CSpecialKeys::IDD, pParent)
,key(0)
,mod(0)
{
}

void CSpecialKeys::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

/*
BEGIN_MESSAGE_MAP(CSpecialKeys, CDialog)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()
*/

BOOL CSpecialKeys::OnInitDialog() 
{
	CDialog::OnInitDialog();
	/// mod&0x08 is the correct value for extended. In CmdDef, it is defined as 1<<2 instead.

	if (key == VK_SPACE) ((CButton *)GetDlgItem(IDC_SPKEYS_SPACE))->SetCheck(TRUE);
	else if (key == VK_TAB) ((CButton *)GetDlgItem(IDC_SPKEYS_TAB))->SetCheck(TRUE);
	else if (key == VK_BACK) ((CButton *)GetDlgItem(IDC_SPKEYS_BACKSPACE))->SetCheck(TRUE);
	else if (key == VK_DELETE && (mod&HOTKEYF_EXT)) ((CButton *)GetDlgItem(IDC_SPKEYS_DELETE))->SetCheck(TRUE);
	else if (key == VK_RETURN) ((CButton *)GetDlgItem(IDC_SPKEYS_RETURN))->SetCheck(TRUE);
	else if (key == VK_RETURN && (mod&HOTKEYF_EXT)) ((CButton *)GetDlgItem(IDC_SPKEYS_INTRO))->SetCheck(TRUE);
	else if (key == VK_PRIOR && (mod&HOTKEYF_EXT)) ((CButton *)GetDlgItem(IDC_SPKEYS_PREVPAG))->SetCheck(TRUE);
	else if (key == VK_NEXT && (mod&HOTKEYF_EXT)) ((CButton *)GetDlgItem(IDC_SPKEYS_NEXTPAG))->SetCheck(TRUE);

	if (mod&MOD_S) ((CButton *)GetDlgItem(IDC_SPKEYS_SHIFT))->SetCheck(TRUE);
	if (mod&MOD_C) ((CButton *)GetDlgItem(IDC_SPKEYS_CONTROL))->SetCheck(TRUE);

	return TRUE;
}

void CSpecialKeys::OnBnClickedOk()
{
	key=0;mod=0;
	if (((CButton *)GetDlgItem(IDC_SPKEYS_SPACE))->GetCheck()) key = VK_SPACE;
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_TAB))->GetCheck()) key = VK_TAB;
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_BACKSPACE))->GetCheck()) key = VK_BACK;
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_DELETE))->GetCheck())
	{	key = VK_DELETE;	mod=HOTKEYF_EXT;	}
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_RETURN))->GetCheck()) key = VK_RETURN;
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_INTRO))->GetCheck())
	{	key = VK_RETURN; mod=HOTKEYF_EXT;	}
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_PREVPAG))->GetCheck())
	{ key = VK_PRIOR; mod=HOTKEYF_EXT; }
	else if (((CButton *)GetDlgItem(IDC_SPKEYS_NEXTPAG))->GetCheck())
	{ key = VK_NEXT; mod=HOTKEYF_EXT; }

	if ( ((CButton *)GetDlgItem(IDC_SPKEYS_SHIFT))->GetCheck() ) mod+=MOD_S;
	if ( ((CButton *)GetDlgItem(IDC_SPKEYS_CONTROL))->GetCheck() ) mod+=MOD_C;

	OnOK();
}


}   // namespace
}   // namespace
