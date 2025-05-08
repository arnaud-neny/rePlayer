///\file
///\brief implementation file for psycle::host::CProgressDialog.
#include <psycle/host/detail/project.private.hpp>
#include "ProgressDialog.hpp"

namespace psycle { namespace host {

		CProgressDialog::CProgressDialog(CWnd* pParent, bool create) : CDialog(CProgressDialog::IDD, pParent)
		{
			if(create) {
				CDialog::Create(IDD, pParent);
			}
		}

		void CProgressDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_PROGRESS1, m_Progress);
		}

/*
		BEGIN_MESSAGE_MAP(CProgressDialog, CDialog)
			ON_WM_CLOSE()
		END_MESSAGE_MAP()
*/

		BOOL CProgressDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_Progress.SetPos(0);
			m_Progress.SetRange(0,16384);
			AfxGetApp()->DoWaitCursor(1);
			return TRUE;
		}
		void CProgressDialog::OnCancel()
		{
			OnClose();
		}

		void CProgressDialog::OnClose()
		{
			CDialog::OnClose();
			AfxGetApp()->DoWaitCursor(-1);
			DestroyWindow();
		}

	}   // namespace
}   // namespace
