///\file
///\brief interface file for psycle::host::CWireDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include <psycle/helpers/fft.hpp>
#include <psycle/helpers/resampler.hpp>

namespace psycle {
	namespace host {

		class Machine;
		class Wire;
		class CChildView;

		const int SCOPE_SPEC_BANDS = 256;
		const int SCOPE_BARS_WIDTH = 256/SCOPE_SPEC_BANDS;
		const int SCOPE_BUF_SIZE_LOG = 13;
		const int SCOPE_BUF_SIZE = 1 << SCOPE_BUF_SIZE_LOG;
		const COLORREF CLBARDC =   0x1010DC;
		const COLORREF CLBARPEAK = 0xC0C0C0;
		const COLORREF CLLEFT =    0xC06060;
		const COLORREF CLRIGHT =   0x60C060;
		const COLORREF CLBOTH =    0xC0C060;

		/// wire monitor window.
		class CWireDlg : public CDialog
		{
		public:
			CWireDlg(CChildView* mainView, CWireDlg** windowVar, int wireDlgIdx,
				Wire& wire);
			virtual ~CWireDlg();
		
		protected:
			virtual BOOL PreTranslateMessage(MSG* pMsg);
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual	void OnCancel();
			virtual void PostNcDestroy();

		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnTimer(UINT_PTR nIDEvent);
			afx_msg void OnDelete();
			afx_msg void OnMode();
			afx_msg void OnHold();
			afx_msg void OnVolumeDb();
			afx_msg void OnVolumePer();
			afx_msg void OnAddEffectHere();
			afx_msg void OnChannelMap();
			afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
			afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

			void InitSpectrum();
			void FillLinearFromCircularBuffer(float inBuffer[], float outBuffer[], float vol);
			void SetMode();
			void OnChangeSliderMode(UINT nPos);
			void OnChangeSliderRate(UINT nPos);
			void OnChangeSliderVol(UINT nPos);
			void UpdateVolPerDb();
			inline int GetY(float f, float multi);

		public:
			Machine& srcMachine;
			Machine& dstMachine;
			Wire& wire;
		protected:
			CWireDlg** windowVar;
			CChildView* mainView;
			UINT wireDlgIdx;
		// Dialog Data
			enum { IDD = IDD_WIREDIALOG };
			CSliderCtrl	m_volslider;
			CSliderCtrl	m_sliderMode;
			CSliderCtrl	m_sliderRate;
			CStatic	m_volabel_per;
			CStatic	m_volabel_db;
			CButton m_mode;

			CBitmap* bufBM;
			CBitmap* clearBM;
			CPen linepenL;
			CPen linepenR;
			CPen linepenbL;
			CPen linepenbR;
			CRect rc;
			CFont font;

		protected:
			float invol;
			float mult;
			float *pSamplesL;
			float *pSamplesR;

			int scope_mode;
			int scope_peak_rate;
			int scope_osc_freq;
			int scope_osc_rate;
			int scope_spec_samples;
			int scope_spec_rate;
			int scope_spec_mode;
			int scope_phase_rate;

			//memories for oscillator.
			BOOL hold;
			BOOL clip;
			int pos;
			//memories for vu-meter
			float peakL,peakR;
			int peakLifeL,peakLifeR;
			//Memories for phase
			float o_mvc, o_mvpc, o_mvl, o_mvdl, o_mvpl, o_mvdpl, o_mvr, o_mvdr, o_mvpr, o_mvdpr;
			//Memories and precalculated values for spectrum
			int bar_heights[SCOPE_SPEC_BANDS];
			float sth[SCOPE_BUF_SIZE][SCOPE_SPEC_BANDS];
			float cth[SCOPE_BUF_SIZE][SCOPE_SPEC_BANDS];

			helpers::dsp::FFTClass fftSpec;
			helpers::dsp::cubic_resampler resampler;
			int FFTMethod;
		};

	}   // namespace
}   // namespace
