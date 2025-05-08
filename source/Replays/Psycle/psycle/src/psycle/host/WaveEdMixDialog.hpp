///\file
///\brief interface file for psycle::host::CWaveEdMixDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class CWaveEdMixDialog : public CDialog
		{
		// Construction
		public:
			CWaveEdMixDialog(CWnd* pParent = 0);
			enum { IDD = IDD_WAVED_MIX };
			CSliderCtrl		m_srcVol;
			CSliderCtrl		m_destVol;
			CButton			m_bFadeIn;
			CButton			m_bFadeOut;
			CEdit			m_fadeInTime;
			CEdit			m_fadeOutTime;
			CStatic			m_destVolText;
			CStatic			m_srcVolText;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
		public:
			float srcVol;
			float destVol;
			bool bFadeIn;
			bool bFadeOut;
			float fadeInTime;
			float fadeOutTime;
		protected:
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
		public:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnCustomDrawDestSlider(NMHDR *pNMHDR, LRESULT *pResult);
			afx_msg void OnCustomDrawSrcSlider(NMHDR *pNMHDR, LRESULT *pResult);
			afx_msg void OnBnClickedFadeoutcheck();
			afx_msg void OnBnClickedFadeincheck();
		};

}   // namespace
}   // namespace
