///\file
///\brief interface file for psycle::host::CGearTracker.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle {
namespace host {

		class Sampler;

		/// gear tracker window.
		class CGearTracker : public CDialog
		{
		public:
			CGearTracker(CGearTracker** windowVar, Sampler& machineref);
		// Dialog Data
			enum { IDD = IDD_GEAR_SAMPLER };
			CComboBox	m_interpol;
			CSliderCtrl	m_polyslider;
			CStatic	m_polylabel;
			CButton m_defaultC4;
			CButton m_linearSlide;
			Sampler& machine;
			CGearTracker** windowVar_;
		// Overrides
		protected:
			virtual BOOL OnInitDialog();
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual void PostNcDestroy();
			virtual void OnCancel();
		// Implementation
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
			afx_msg void OnCustomdrawNumVoices(NMHDR* pNMHDR, LRESULT* pResult);
			afx_msg void OnSelchangeCombo1();
			afx_msg void OnDefaultC4();
			afx_msg void OnLinearSlide();
		};

	}   // namespace
}   // namespace
