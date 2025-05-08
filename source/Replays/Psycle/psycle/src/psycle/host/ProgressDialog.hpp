///\file
///\brief interface file for psycle::host::CProgressDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		/// progress meter window.
		class CProgressDialog : public CDialog
		{
		public:
			CProgressDialog(CWnd* pParent = 0, bool create=true);
		// Dialog Data
			enum { IDD = IDD_PROGRESS_DIALOG };
			CProgressCtrl	m_Progress;
		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual	void OnCancel();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
		};

	}   // namespace host
}   // namespace psycle
