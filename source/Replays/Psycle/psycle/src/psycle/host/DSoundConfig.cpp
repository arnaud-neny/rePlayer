///\file
///\implementation psycle::host::CDSoundConfig.

#include <psycle/host/detail/project.private.hpp>
#include "DSoundConfig.hpp"
#include "DirectSound.hpp"
namespace psycle { namespace host {

		CDSoundConfig::CDSoundConfig(CWnd* pParent)
		:
			CDialog(CDSoundConfig::IDD, pParent),
			device_idx(0),
			sample_rate(44100),
			buffer_count(4),
			buffer_samples(512),
			bit_depth(16),
			dither(false)
		{
		}

		void CDSoundConfig::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_DSOUND_DEVICE, m_deviceComboBox);
			DDX_CBIndex(pDX, IDC_DSOUND_DEVICE, device_idx);
			DDX_Control(pDX, IDC_DSOUND_SAMPLERATE_COMBO, m_sampleRateCombo);
			DDX_Control(pDX, IDC_DSOUND_BITDEPTH, m_bitDepthCombo);
			DDX_Control(pDX, IDC_DSOUND_BUFNUM_EDIT, m_numBuffersEdit);
			DDX_Control(pDX, IDC_DSOUND_BUFNUM_SPIN, m_numBuffersSpin);
			DDX_Text(pDX, IDC_DSOUND_BUFNUM_EDIT, buffer_count);
			DDV_MinMaxInt(pDX, buffer_count, buffer_count_min, buffer_count_max);
			DDX_Control(pDX, IDC_DSOUND_BUFSIZE_EDIT, m_bufferSizeEdit);
			DDX_Control(pDX, IDC_DSOUND_BUFSIZE_SPIN, m_bufferSizeSpin);
			DDX_Text(pDX, IDC_DSOUND_BUFSIZE_EDIT, buffer_samples);
			DDV_MinMaxInt(pDX, buffer_samples, buffer_size_min, buffer_size_max);
			DDX_Control(pDX, IDC_DSOUND_LATENCY, m_latency);
		}

		BEGIN_MESSAGE_MAP(CDSoundConfig, CDialog)
			ON_CBN_SELCHANGE(IDC_DSOUND_DEVICE, OnSelchangeDevice)
			ON_CBN_SELENDOK(IDC_DSOUND_SAMPLERATE_COMBO, OnSelendokSamplerate)
			ON_CBN_SELENDOK(IDC_DSOUND_BITDEPTH, OnSelendokBitDepth)
			ON_EN_CHANGE(IDC_DSOUND_BUFNUM_EDIT, OnChangeBufnumEdit)
			ON_EN_CHANGE(IDC_DSOUND_BUFSIZE_EDIT, OnChangeBufsizeEdit)
			ON_WM_DESTROY()
		END_MESSAGE_MAP()

		BOOL CDSoundConfig::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			// displays the DirectSound device in the combo box
			{
				std::vector<std::string> ports;
				directsound->RefreshAvailablePorts();
				directsound->GetPlaybackPorts(ports);
				for(int i=0 ; i < ports.size() ; ++i)
				{
					m_deviceComboBox.AddString(ports[i].c_str());
				}
				m_deviceComboBox.SetCurSel(device_idx);
			}
			// bit depth
			{
				switch(bit_depth) {
				case 16: if (dither) m_bitDepthCombo.SetCurSel(1);
						 else  m_bitDepthCombo.SetCurSel(0);
						 break;
				case 24: m_bitDepthCombo.SetCurSel(2); break;
				case 32: m_bitDepthCombo.SetCurSel(3); break;
				}
			}
			// displays the sample rate
			{
				CString str;
				str.Format("%d", sample_rate);
				int const i(m_sampleRateCombo.SelectString(-1, str));
				if(i == CB_ERR) m_sampleRateCombo.SelectString(-1, "44100");
			}
			// displays the buffer count
			{
				CString str;
				str.Format("%d", buffer_count);
				m_numBuffersEdit.SetWindowText(str);
				m_numBuffersSpin.SetRange(buffer_count_min, buffer_count_max);
			}
			// displays the buffer size
			{
				CString str;
				str.Format("%d", buffer_samples);
				m_bufferSizeEdit.SetWindowText(str);
				m_bufferSizeSpin.SetRange32(buffer_size_min, buffer_size_max);
				// sets the spin button increments
				{
					UDACCEL accel;
					accel.nSec = 0;
					accel.nInc = 128;
					m_bufferSizeSpin.SetAccel(1, &accel);
				}
			}

			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}

		void CDSoundConfig::OnOK() 
		{
			if(!m_deviceComboBox.GetCount())
			{
				// no DirectSound driver ...
				MessageBox("no DirectSound driver", "Warning", MB_ICONWARNING);
				// we cancel instead.
				CDialog::OnCancel();
				return;
			}
			CDialog::OnOK();
		}

		void CDSoundConfig::RecalcLatency()
		{
			// computes the latency
			float latency_time_seconds;
			latency_time_seconds = float(buffer_samples * buffer_count) / sample_rate;
			// displays the latency in the gui
			{
				std::ostringstream s;
				s << "Latency: "
					//<< std::setprecision(0)
					<< static_cast<int>(latency_time_seconds * 1e3) << "ms";
				m_latency.SetWindowText(s.str().c_str());
			}
		}

		void CDSoundConfig::OnSelchangeDevice() 
		{
			device_idx = m_deviceComboBox.GetCurSel();
		}

		void CDSoundConfig::OnChangeBufsizeEdit() 
		{
			if(!IsWindow(m_bufferSizeEdit.GetSafeHwnd())) return;
			// read the buffer size from the gui
			CString s;
			m_bufferSizeEdit.GetWindowText(s);
			buffer_samples = std::atoi(s);
			RecalcLatency();
		}

		void CDSoundConfig::OnChangeBufnumEdit() 
		{
			if(!IsWindow(m_numBuffersEdit.GetSafeHwnd())) return;
			// read the buffer count from the gui
			CString s;
			m_numBuffersEdit.GetWindowText(s);
			buffer_count = std::atoi(s);
			RecalcLatency();
		}

		void CDSoundConfig::OnSelendokSamplerate() 
		{
			if(!IsWindow(m_sampleRateCombo.GetSafeHwnd())) return;
			// read the sample rate from the gui
			CString s;
			m_sampleRateCombo.GetWindowText(s);
			sample_rate = std::atoi(s);
			RecalcLatency();
		}
		void CDSoundConfig::OnSelendokBitDepth()
		{
			if(!IsWindow(m_bitDepthCombo.GetSafeHwnd())) return;

			int pos = m_bitDepthCombo.GetCurSel();
			switch(pos) {
			case 0: bit_depth = 16; dither=false; break;
			case 1: bit_depth = 16; dither=true; break;
			case 2: bit_depth = 24; dither=false; break;
			case 3:  bit_depth = 32; dither=false; break;
			}
		}


		void CDSoundConfig::OnDestroy() 
		{
			CDialog::OnDestroy();
		}

	}   // namespace
}   // namespace
