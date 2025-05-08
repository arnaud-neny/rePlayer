///\file
///\interface psycle::host::CDSoundConfig
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
namespace psycle { namespace host {
		
	class DirectSound;

		/// direct sound config window.
		class CDSoundConfig : public CDialog
		{
			public:
				CDSoundConfig(CWnd * pParent = 0);
				int const static IDD = IDD_CONFIG_DSOUND;
				CComboBox       m_deviceComboBox;
				CComboBox       m_sampleRateCombo;
				CComboBox       m_bitDepthCombo;
				CEdit           m_numBuffersEdit;
				CSpinButtonCtrl m_numBuffersSpin;
				CEdit           m_bufferSizeEdit;
				CSpinButtonCtrl m_bufferSizeSpin;
				CStatic         m_latency;
				DirectSound*	directsound;
				int               device_idx;
				int               sample_rate;
				int               bit_depth;
				int               buffer_count;
				int  const static buffer_count_min = 2;
				int  const static buffer_count_max = 8;
				int               buffer_samples;
				int  const static buffer_size_min = 128;
				int  const static buffer_size_max = 8192;
				bool              dither;

			protected:
				virtual void DoDataExchange(CDataExchange* pDX);
				virtual BOOL OnInitDialog();
				virtual void OnOK();
				void RecalcLatency();
			private:
				DECLARE_MESSAGE_MAP()
				afx_msg void OnSelchangeDevice();
				afx_msg void OnSelendokSamplerate();
				afx_msg void OnSelendokBitDepth();
				afx_msg void OnChangeBufnumEdit();
				afx_msg void OnChangeBufsizeEdit();
				afx_msg void OnDestroy();
		};
	}   // namespace
}   // namespace

