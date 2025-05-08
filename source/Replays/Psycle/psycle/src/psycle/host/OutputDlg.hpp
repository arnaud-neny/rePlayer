///\file
///\interface file for psycle::host::COutputDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		class AudioDriver;

		/// audio device output config window.
		class COutputDlg : public CPropertyPage
		{
			DECLARE_DYNCREATE(COutputDlg)
		public:
			COutputDlg();
			enum { IDD = IDD_CONFIG_OUTPUT };
			CComboBox       m_driverComboBox;
			CComboBox       m_midiDriverComboBox;
			CButton         m_inmediate;
			CButton         m_sequenced;
			CComboBox       m_midiSyncComboBox;
			CEdit           m_midiHeadroomEdit;
			CSpinButtonCtrl m_midiHeadroomSpin;
			CEdit           m_numberThreadsEdit;
		public:
			int m_driverIndex;
			int m_numberThreads;
			int m_midiDriverIndex;
			int m_syncDriverIndex;
			int m_midiHeadroom;
		private:
			int m_oldDriverIndex;
			int m_oldMidiDriverIndex;
			int m_oldSyncDriverIndex;
			int const static MIN_HEADROOM = 0;
			int const static MAX_HEADROOM = 9999;
		protected:
			void EnableClockOptions();
			void DisableClockOptions();
			void PopulateListbox( CComboBox * listbox, int numDevs, bool issync );
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnConfig();
			afx_msg void OnSelChangeOutput();
			afx_msg void OnSelChangeMidi();
			afx_msg void OnSelChangeSync();
			afx_msg void OnEnableInmediate();
			afx_msg void OnEnableSequenced();
		};

	}   // namespace
}   // namespace
