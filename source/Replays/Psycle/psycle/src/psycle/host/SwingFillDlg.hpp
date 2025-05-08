///\file
///\brief interface file for psycle::host::CSwingFillDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		/// swing fill dialog window.
		class CSwingFillDlg : public CDialog
		{
		public:
			CSwingFillDlg();
			BOOL bGo;
			int tempo;
			int width;
			float variance;
			float phase;
			BOOL offset;
			enum { IDD = IDD_SWINGFILL };
			CEdit	m_Tempo;
			CEdit	m_Width;
			CEdit	m_Variance;
			CEdit	m_Phase;
			CButton m_Offset;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			DECLARE_MESSAGE_MAP()
		};

	}   // namespace
}   // namespace
