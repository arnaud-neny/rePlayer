///\file
///\brief implementation file for psycle::host::CConfigDlg.

#include <psycle/host/detail/project.private.hpp>
#include "ConfigDlg.hpp"

namespace psycle { namespace host {

		IMPLEMENT_DYNAMIC(CConfigDlg, CPropertySheet)

		CConfigDlg::CConfigDlg(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
			: CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
		{
			AddControlPages();
		}

		CConfigDlg::CConfigDlg(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
			:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
		{
			AddControlPages();
		}

		CConfigDlg::~CConfigDlg()
		{
		}

		void CConfigDlg::AddControlPages() {
// 			m_psh.dwFlags |= PSH_NOAPPLYNOW; // Remove the Apply button

			AddPage(&_skinDlg);
			AddPage(&_keyDlg);
			AddPage(&_dirDlg);
			AddPage(&_outputDlg);
			AddPage(&_midiDlg);
		}
/*
		BEGIN_MESSAGE_MAP(CConfigDlg, CPropertySheet)
		END_MESSAGE_MAP()
*/


		INT_PTR CConfigDlg::DoModal() 
		{
			int retVal = CPropertySheet::DoModal();
			if (retVal == IDOK)
			{
				PsycleGlobal::conf().SavePsycleSettings();
			}
			return retVal;
		}

	}   // namespace
}   // namespace
