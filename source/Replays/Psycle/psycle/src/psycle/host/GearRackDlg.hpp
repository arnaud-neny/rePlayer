///\file
///\brief interface file for psycle::host::CGearRackDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class CChildView;
		class CMainFrame;
		/// gear rack window.
		class CGearRackDlg : public CDialog
		{
		public:
			CGearRackDlg(CMainFrame* pParent, CChildView* pMain, CGearRackDlg** windowVar);
			void RedrawList();
		// Dialog Data
			enum { IDD = IDD_GEARRACK };
			CButton	m_props;
			CButton m_radio_wave;
			CButton	m_radio_ins;
			CButton	m_radio_gen;
			CButton	m_radio_efx;
			CButton	m_text;
			CListBox	m_list;

			CChildView* mainView;
			CMainFrame* mainFrame;
			CGearRackDlg** windowVar;
			static int DisplayMode;
		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual BOOL PreTranslateMessage(MSG* pMsg);
			virtual void PostNcDestroy();
			virtual void OnCancel();
		// Implementation
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnCreate();
			afx_msg void OnClose();
			afx_msg void OnDelete();
			afx_msg void OnDblclkGearlist();
			afx_msg void OnProperties();
			afx_msg void OnMasterProperties();
			afx_msg void OnParameters();
			afx_msg void OnSelchangeGearlist();
			afx_msg void OnMachineType();
			afx_msg void OnRadioEfx();
			afx_msg void OnRadioGen();
			afx_msg void OnRadioIns();
			afx_msg void OnRadioWaves();
			afx_msg void OnExchange();
			afx_msg void OnClonemachine();
		};

	}   // namespace
}   // namespace
