// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "AudioDriver.hpp"
#include <mmreg.h>
namespace psycle {
	namespace host {

		class CChildView;
		class CSelection;

		/// save wave dialog window.
		class CSaveWavDlg : public CDialog
		{
		// Construction
		public:
			/// mfc compliant constructor
			CSaveWavDlg(CChildView* pChildView, CSelection* pBlockSel, CWnd* pParent = 0);

			void SaveTick(void);
			void SaveEnd(void);
			void SaveToClipboard();
			int kill_thread;
			int threadopen;
			enum { IDD = IDD_SAVEWAVDLG };
			CButton	m_browse;
			CButton	m_cancel;
			CButton	m_savewave;
			CButton	m_savewires;
			CButton	m_savegens;
			CButton	m_savetracks;
			CButton m_dither;
			CEdit	m_rangestart;
			CEdit	m_rangeend;
			CEdit	m_linestart;
			CEdit	m_lineend;
			CProgressCtrl	m_progress;
			CEdit	m_patnumber;
			CEdit	m_patnumber2;
			CEdit	m_filename;
			CStatic m_text;
			int		m_recmode;
			int		m_outputtype;
			CComboBox	m_rate;
			CComboBox	m_bits;
			CComboBox	m_channelmode;
			CComboBox	m_pdf;
			CComboBox	m_noiseshaping;
		protected:
			HANDLE thread_handle;

			CChildView* pChildView;
			CSelection* pBlockSel;
			int lastpostick;
			int lastlinetick;
			int tickcont;

			static int rate;
			static int bits;
			static channel_mode channelmode;
			static int ditherpdf;
			static int noiseshape;

			struct pdf
			{
				enum pdfs
				{
					triangular=0,
					rectangular,
					gaussian
				};
			};
#pragma pack(push, 1)
			struct fullheader
			{
				uint32_t	head;
				uint32_t	size;
				uint32_t	head2;
				uint32_t	fmthead;
				uint32_t	fmtsize;
				WAVEFORMATEX	fmtcontent;
				uint32_t datahead;
				uint32_t datasize;
			} clipboardwavheader;
#pragma pack(pop)

			std::vector<char*> clipboardmem;

			int current;

			std::string rootname;

			static BOOL savetracks;
			static BOOL savewires;
			static BOOL savegens;

			bool _Muted[MAX_BUSES];

			bool autostop;
			bool playblock;
			bool loopsong;
			bool sel[MAX_SONG_POSITIONS];
			bool saving;
			bool dither;

			void SaveWav(std::string file, int bits, int rate, channel_mode channelmode, bool isFloat);
			
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnCancel();
			DECLARE_MESSAGE_MAP()						
			afx_msg void OnFilebrowse();
			afx_msg void OnSelAllSong();
			afx_msg void OnSelRange();
			afx_msg void OnSelPattern();
			afx_msg void OnSavewave();
			afx_msg void OnSelchangeComboBits();
			afx_msg void OnSelchangeComboChannels();
			afx_msg void OnSelchangeComboRate();
			afx_msg void OnSelchangeComboPdf();
			afx_msg void OnSelchangeComboNoiseShaping();
			afx_msg void OnSavetracksseparated();
			afx_msg void OnSavewiresseparated();
			afx_msg void OnSavegensseparated();
			afx_msg void OnToggleDither();
			afx_msg void OnRecblock();
			afx_msg void OnOutputfile();
			afx_msg void OnOutputclipboard();
			afx_msg void OnOutputsample();
		};
	}
}
