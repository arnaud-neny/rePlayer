///\file
///\brief interface file for psycle::host::CSongpDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
namespace psycle {
	namespace host {

		class Song; // forward declaration

		/// song dialog window.
		class CSongpDlg : public CDialog
		{
		public:
			/// mfc compliant constructor.
			CSongpDlg(Song& song);
			Song& _pSong;
		// Dialog Data
			enum { IDD = IDD_SONGPROP };
			CEdit	m_songcomments;
			CEdit	m_songcredits;
			CEdit	m_songtitle;
			CEdit	m_tempo;
			CEdit	m_linespb;
			CEdit	m_tickspb;
			CEdit	m_extraTicks;
			CEdit	m_realTempo;
			CEdit	m_realTpb;
			CSpinButtonCtrl m_tempoSpin;
			CSpinButtonCtrl m_linespbSpin;
			CSpinButtonCtrl m_tickspbSpin;
			CSpinButtonCtrl m_extraTicksSpin;
			bool	readonlystate;
			bool initialized;
		public:
			void SetReadOnly();
			int RealBPM();
			int RealTPB();
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnOk();
			afx_msg void OnEnchangeTempo();
			afx_msg void OnEnchangeLinesPB();
			afx_msg void OnEnchangeTicksPB();
			afx_msg void OnEnchangeExtraTicks();
		};

}}
