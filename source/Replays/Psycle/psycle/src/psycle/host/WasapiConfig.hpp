///\file
///\brief interface file for psycle::host::CASIOConfig.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		class WasapiDriver;

		class WasapiConfig : public CDialog
		{
		public:
			WasapiConfig(CWnd* pParent = 0);
		// Dialog Data
			enum { IDD = IDD_CONFIG_WASAPI };
			CComboBox	m_driverComboBox;
			CStatic	m_latency;
			CComboBox	m_sampleRateCombo;
			CComboBox	m_bitDepthCombo;
			CComboBox	m_bufferSizeCombo;
			WasapiDriver* wasapi;
			int	m_driverIndex;
			int m_shareMode;
			int m_sampleRate;
			int m_bitDepth;
			bool m_dither;
			int	m_bufferSize;
		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnOK();
		// Implementation
		protected:
			void RecalcLatency();
			void FillBufferBox();
			void SetProperties();
			DECLARE_MESSAGE_MAP()
			afx_msg void OnSelendokDriver();
			afx_msg void OnSelendokSamplerate();
			afx_msg void OnSelendokBits();
			afx_msg void OnSelendokBuffersize();
			afx_msg void OnShared();
			afx_msg void OnExclusive();
			afx_msg void OnDestroy();
		};

}}
