///\file
///\brief implementation file for psycle::host::WasapiConfig.

#include <psycle/host/detail/project.private.hpp>
#include "WasapiConfig.hpp"
#include "WasapiDriver.hpp"

namespace psycle { namespace host {

		WasapiConfig::WasapiConfig(CWnd* pParent) : CDialog(WasapiConfig::IDD, pParent)
		{
			m_bufferSize = 1024;
			m_driverIndex = -1;
			m_shareMode = 0;
		}

		void WasapiConfig::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_WASAPI_DEVICE, m_driverComboBox);
			DDX_CBIndex(pDX, IDC_WASAPI_DEVICE, m_driverIndex);
			DDX_Radio(pDX, IDC_ACCESS_EXCLUSIVE, m_shareMode);
			DDX_Control(pDX, IDC_WASAPI_SAMPLERATE_COMBO, m_sampleRateCombo);
			DDX_Control(pDX, IDC_WASAPI_BITS, m_bitDepthCombo);
			DDX_Control(pDX, IDC_WASAPI_BUFFERSIZE_COMBO, m_bufferSizeCombo);
			DDX_Control(pDX, IDC_WASAPI_LATENCY, m_latency);
		}

		BEGIN_MESSAGE_MAP(WasapiConfig, CDialog)
			ON_CBN_SELCHANGE(IDC_WASAPI_DEVICE, OnSelendokDriver)
			ON_CBN_SELENDOK(IDC_WASAPI_SAMPLERATE_COMBO, OnSelendokSamplerate)
			ON_CBN_SELENDOK(IDC_WASAPI_BITS, OnSelendokBits)
			ON_CBN_SELENDOK(IDC_WASAPI_BUFFERSIZE_COMBO, OnSelendokBuffersize)
			ON_BN_CLICKED(IDC_ACCESS_SHARED, OnShared)
			ON_BN_CLICKED(IDC_ACCESS_EXCLUSIVE, OnExclusive)
			ON_WM_DESTROY()
		END_MESSAGE_MAP()

		BOOL WasapiConfig::OnInitDialog() 
		{
			CString str;
			CDialog::OnInitDialog();
			// displays the DirectSound device in the combo box
			{
				std::vector<std::string> ports;
				wasapi->RefreshAvailablePorts();
				wasapi->GetPlaybackPorts(ports);
				for(int i=0 ; i < ports.size() ; ++i)
				{
					m_driverComboBox.AddString(ports[i].c_str());
				}
				m_driverComboBox.SetCurSel(m_driverIndex);
			}
			// Check boxes
			// recalc the buffers combo
			SetProperties();
			FillBufferBox();

			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}

		void WasapiConfig::OnOK() 
		{
			if (m_driverComboBox.GetCount() > 0)
			{

				CString str;
				m_sampleRateCombo.GetWindowText(str);
				m_sampleRate = atoi(str);

				m_bufferSizeCombo.GetWindowText(str);
				m_bufferSize = atoi(str);

				CDialog::OnOK();
				return;
			}
			CDialog::OnCancel();
		}

		void WasapiConfig::OnSelendokDriver() 
		{
			CString str;
			m_bufferSizeCombo.GetWindowText(str);

			m_driverIndex = m_driverComboBox.GetCurSel();
			SetProperties();
			FillBufferBox();
		}

		void WasapiConfig::OnSelendokSamplerate() 
		{
			if (!IsWindow(m_sampleRateCombo.GetSafeHwnd()))
			{
				return;
			}
			CString str;
			m_sampleRateCombo.GetWindowText(str);
			m_sampleRate = atoi(str);

			RecalcLatency();
		}
		void WasapiConfig::OnSelendokBits()
		{
			if (!IsWindow(m_bitDepthCombo.GetSafeHwnd()))
			{
				return;
			}
			CString str;
			int pos = m_bitDepthCombo.GetCurSel();
			switch(pos) {
			case 0: m_bitDepth = 16; m_dither=false; break;
			case 1: m_bitDepth = 16; m_dither=true; break;
			case 2: m_bitDepth = 24; m_dither=false; break;
			case 3:  m_bitDepth = 32; m_dither=false; break;
			}
			RecalcLatency();
		}

		void WasapiConfig::OnSelendokBuffersize() 
		{
			if (!IsWindow(m_bufferSizeCombo.GetSafeHwnd()))
			{
				return;
			}
			CString str;
			m_bufferSizeCombo.GetWindowText(str);
			m_bufferSize = atoi(str);
			RecalcLatency();
		}
		void WasapiConfig::OnShared()
		{
			m_shareMode = 1;
			SetProperties();
			FillBufferBox();
		}
		void WasapiConfig::OnExclusive()
		{
			m_shareMode = 0;
			SetProperties();
			FillBufferBox();
		}

		void WasapiConfig::OnDestroy() 
		{
			CDialog::OnDestroy();
		}

		void WasapiConfig::RecalcLatency()
		{
			CString str;
			//Multiplied by two becauase we have two buffers
			int lat = (m_bufferSize * 2 * 1000) / m_sampleRate;
			str.Format("Latency: %dms", lat);
			m_latency.SetWindowText(str);
		}

		void WasapiConfig::SetProperties()
		{
			CString srate;
			int bits;
			bool dither;

			if(m_shareMode) {
				m_bufferSizeCombo.EnableWindow(FALSE);
				m_sampleRateCombo.EnableWindow(FALSE);
				m_bitDepthCombo.EnableWindow(FALSE);
				srate.Format("%d", wasapi->_playEnums[m_driverIndex].MixFormat.Format.nSamplesPerSec);
				bits = wasapi->_playEnums[m_driverIndex].MixFormat.Samples.wValidBitsPerSample;
				dither=true;
			}
			else {
				m_bufferSizeCombo.EnableWindow(TRUE);
				m_sampleRateCombo.EnableWindow(TRUE);
				m_bitDepthCombo.EnableWindow(TRUE);
				srate.Format("%d", m_sampleRate);
				bits = m_bitDepth;
				dither=m_dither;
				if(bits==32) {bits=m_bitDepth=24;}
			}
			int i = m_sampleRateCombo.SelectString(-1, srate);
			if (i == CB_ERR)
			{
				i = m_sampleRateCombo.SelectString(-1, "44100");
			}
			// bit depth
			{
				switch(bits) {
				case 16: if (m_dither) m_bitDepthCombo.SetCurSel(1);
						 else  m_bitDepthCombo.SetCurSel(0);
						 break;
				case 24: m_bitDepthCombo.SetCurSel(2); break;
				case 32: m_bitDepthCombo.SetCurSel(3); break;
				}
			}
		}

		void WasapiConfig::FillBufferBox()
		{
			char buf[8];
			int prefindex = 0;
			int g = 64;
			m_bufferSizeCombo.ResetContent();

			if(m_shareMode) 
			{
				int samples = portaudio::MakeFramesFromHns(wasapi->_playEnums[m_driverIndex].DefaultDevicePeriod,m_sampleRate);
				sprintf(buf,"%d",samples);
				m_bufferSizeCombo.AddString(buf);
			}
			else {
				int samples = portaudio::MakeFramesFromHns(wasapi->_playEnums[m_driverIndex].MinimumDevicePeriod,m_sampleRate);
				samples = portaudio::AlignFramesPerBuffer(samples,m_sampleRate,m_bitDepth==24?32:m_bitDepth);
				int samples2 = portaudio::MakeFramesFromHns(wasapi->_playEnums[m_driverIndex].DefaultDevicePeriod,m_sampleRate);
				samples2 = portaudio::AlignFramesPerBuffer(samples2,m_sampleRate,m_bitDepth==24?32:m_bitDepth);
				samples2 += g;
				if(samples2 < 2048) samples2= 2048;

				sprintf(buf,"%d",samples);
				m_bufferSizeCombo.AddString(buf);

				for (int i = ((samples/g)+1)*g; i <= samples2; i += g)
				{
					if (i <= m_bufferSize)
					{
						prefindex++;
					}
					sprintf(buf,"%d",i);
					m_bufferSizeCombo.AddString(buf);
				}
				if (prefindex >= m_bufferSizeCombo.GetCount())
				{
					prefindex=m_bufferSizeCombo.GetCount()-1;
				}
			}
			m_bufferSizeCombo.SetCurSel(prefindex);
			CString str;
			m_bufferSizeCombo.GetWindowText(str);
			m_bufferSize = atoi(str);
			RecalcLatency();
		}

	}   // namespace
}   // namespace
