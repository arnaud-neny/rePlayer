///\file
///\brief interface file for psycle::host::CMacProp.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

		class CChildView;
		class Song;
		class Machine;

		/// machine properties window.
		class CMacProp : public CDialog
		{
		public:
			CMacProp(Song& song, CWnd* pParent = 0);
			Machine *pMachine;
			Song& thesong;
			CChildView *m_view;
			int thisMac;
			char txt[32];
			bool deleted;

			enum { IDD = IDD_MACPROP };
			CButton	m_soloCheck;
			CButton	m_bypassCheck;
			CButton	m_muteCheck;
			CButton	m_macbefore;
			CEdit	m_macname;
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnChangeMacName();
			afx_msg void OnDelete();
			afx_msg void OnMute();
			afx_msg void OnBypass();
			afx_msg void OnSolo();
			afx_msg void OnClone();
			afx_msg void OnAddNewBefore();
			afx_msg void OnAddNewAfter();
			afx_msg void OnBnClickedReplacemac();
		};

	}   // namespace host
}   // namespace psycle
