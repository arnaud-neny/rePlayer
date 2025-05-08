///\file
///\brief interface file for psycle::host::CWaveOutDialog.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

	class WaveOut;
		/// mme config window.
		class CWaveOutDialog : public CDialog
		{
		public:
			CWaveOutDialog(CWnd * pParent = 0);
			enum { IDD = IDD_CONFIG_WAVEOUT };
			CComboBox	m_deviceList;
			CComboBox	m_sampleRateBox;
			CComboBox	m_bitDepthBox;
			CEdit	m_bufNumEdit;
			CSpinButtonCtrl	m_bufNumSpin;
			CEdit	m_bufSamplesEdit;
			CSpinButtonCtrl	m_bufSamplesSpin;
			CStatic	m_latency;
			WaveOut* waveout;
			int		m_device;
			int		m_sampleRate;
			int		m_bitDepth;
			bool	m_dither;
			int		m_bufNum;
			int		m_bufSamples;
			int static const numbuf_min = 3;
			int static const numbuf_max = 8;
			int static const numsamples_min = 128;
			int static const numsamples_max = 8193;


		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
		// Implementation
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnChangeBufnum();
			afx_msg void OnChangeBufsize();
			afx_msg void OnSelendokSamplerate();
			afx_msg void OnSelendokBitDepth();
		private:
			void RecalcLatency();
		};

	}   // namespace
}   // namespace
