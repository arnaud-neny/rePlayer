///\file
///\brief interface file for psycle::host::CWaveEdAmplifyDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		#define AMP_DIALOG_CANCEL -10000

		/// wave amplification dialog window.
		class CWaveEdAmplifyDialog : public CDialog
		{
		// Construction
		public:
			CWaveEdAmplifyDialog(CWnd* pParent = 0);
			enum { IDD = IDD_WAVED_AMPLIFY };
			CEdit	m_dbedit;
			CEdit	m_edit;
			CSliderCtrl	m_slider;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnCustomdrawSlider(NMHDR* pNMHDR, LRESULT* pResult);
		};

	}   // namespace
}   // namespace
