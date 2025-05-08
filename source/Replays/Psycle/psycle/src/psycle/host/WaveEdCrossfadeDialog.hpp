///\file
///\brief interface file for psycle::host::CWaveEdCrossfadeDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class CWaveEdCrossfadeDialog : public CDialog
		{
		// Construction
		public:
			CWaveEdCrossfadeDialog(CWnd* pParent = 0);
			enum { IDD = IDD_WAVED_CROSSFADE };
			CSliderCtrl		m_srcStartVol;
			CSliderCtrl		m_srcEndVol;
			CSliderCtrl		m_destStartVol;
			CSliderCtrl		m_destEndVol;
			CStatic			m_srcStartVolText;
			CStatic			m_srcEndVolText;
			CStatic			m_destStartVolText;
			CStatic			m_destEndVolText;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
		public:
			float srcStartVol;
			float srcEndVol;
			float destStartVol;
			float destEndVol;
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnCustomDrawSliders(NMHDR *pNMHDR, LRESULT *pResult);
		};

	}   // namespace
}   // namespace
