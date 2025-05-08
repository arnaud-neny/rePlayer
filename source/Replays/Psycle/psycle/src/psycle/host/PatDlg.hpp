///\file
///\brief interface file for psycle::host::CPatDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { 
	namespace host {

		/// pattern window.
		class CPatDlg : public CDialog
		{
		public:
			CPatDlg(Song& song, CWnd* pParent = 0);
			enum { IDD = IDD_PATDLG };
			CEdit	m_patname;
			char patName[32];
			CEdit	m_numlines;
			int patLines;
			CSpinButtonCtrl	m_spinlines;
			CStatic m_text;
			CButton	m_adaptsizeCheck;
			BOOL	m_adaptsize;
			int		m_shownames;
			int		m_independentnames;

			CEdit   m_trackedit;
			CListBox m_tracklist;
			CButton m_copybutton;
			CComboBox m_patternlist;

			int patIdx;
			Song& m_song;
			int prevsel;
			std::string tracknames[MAX_TRACKS];
			BOOL bInit;
		protected:
			void FillTrackList();
			void FillPatternCombo();
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnAdaptContent();
			afx_msg void OnChangeNumLines();
			afx_msg void OnChangeTrackEdit();
			afx_msg void OnSelchangeTracklist();
			afx_msg void OnHeaderHeader();
			afx_msg void OnHeaderNames();
			afx_msg void OnNotShareNames();
			afx_msg void OnShareNames();
			afx_msg void OnCopyNames();
		};
}}
