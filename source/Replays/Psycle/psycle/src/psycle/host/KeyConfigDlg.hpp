///\file
///\brief interface file for psycle::host::CKeyConfigDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"

namespace psycle { namespace host {
		/// key config window.
		class CKeyConfigDlg : public CPropertyPage
		{
			DECLARE_DYNCREATE(CKeyConfigDlg)
		public:
			CKeyConfigDlg();
			enum { IDD = IDD_CONFIG_KEY };
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnCancel();
			virtual void OnOK();
			
			void RefreshDialog();
			void FillCmdList();
			void SaveHotKey(long idx,WORD key,WORD mods);
			void FindKey(long idx,WORD &key,WORD &mods);

		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnSelchangeCmdlist();
			afx_msg void OnImportreg();
			afx_msg void OnExportreg();
			afx_msg void OnDefaults();
			afx_msg void OnBnClickedSpecialKeys();
			afx_msg void OnNone();
			afx_msg void OnUpdateNumLines();
			afx_msg void OnSelchangeStore();
		protected:
			CListBox	m_lstCmds;
			CHotKeyCtrl	m_hotkey0;
			CButton	m_cmdCtrlPlay;
			CButton	m_cmdNewHomeBehaviour;
			CButton	m_cmdFT2Del;
			CButton m_windowsblocks;
			CButton	m_cmdShiftArrows;
			CButton	m_wrap;
			CButton	m_cursordown;
			CButton	m_tweak_smooth;
			CButton	m_record_unarmed;			
			CButton m_navigation_ignores_step;
			CComboBox m_pageupsteps;

			CButton	m_autosave;
			CEdit	m_autosave_mins;
			CSpinButtonCtrl	m_autosave_spin;
			CButton	m_save_reminders;
			CButton	m_show_info;
			CButton m_allowinstances;
			CComboBox m_storeplaces;

			CEdit	m_numlines;
			CSpinButtonCtrl	m_spinlines;
			CStatic m_textlines;

			BOOL bInit;
			long m_prvIdx;

			std::map<int, std::pair<int,int>> keyMap;
			///\todo: Implement a copy of it, instead of a reference.
			PsycleConfig::InputHandler& handler;
		};
	}   // namespace
}   // namespace
