///\file
///\brief implementation file for psycle::host::CWaveOutDialog.

#include <psycle/host/detail/project.private.hpp>
#include "WaveOutDialog.hpp"
#include "WaveOut.hpp"

namespace psycle { namespace host {
		CWaveOutDialog::CWaveOutDialog(CWnd* pParent)
			: CDialog(CWaveOutDialog::IDD, pParent)
			, m_device(-1)
			, m_sampleRate(44100)
			, m_bitDepth(16)
			, m_dither(false)
			, m_bufNum(6)
			, m_bufSamples(1024)
			, waveout(NULL)
		{
		}

		void CWaveOutDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_CONFIG_DEVICE, m_deviceList);
			DDX_CBIndex(pDX, IDC_CONFIG_DEVICE, m_device);
			DDX_Control(pDX, IDC_CONFIG_SAMPLERATE, m_sampleRateBox);
			DDX_Control(pDX, IDC_WAVEOUT_BITDEPTH, m_bitDepthBox);
			DDX_Control(pDX, IDC_CONFIG_BUFNUM, m_bufNumEdit);
			DDX_Control(pDX, IDC_CONFIG_BUFNUM_SPIN, m_bufNumSpin);
			DDX_Text(pDX, IDC_CONFIG_BUFNUM, m_bufNum);
			DDX_Control(pDX, IDC_CONFIG_BUFSIZE, m_bufSamplesEdit);
			DDX_Control(pDX, IDC_CONFIG_BUFSIZE_SPIN, m_bufSamplesSpin);
			DDX_Text(pDX, IDC_CONFIG_BUFSIZE, m_bufSamples);
			DDX_Control(pDX, IDC_CONFIG_LATENCY, m_latency);
		}

		BEGIN_MESSAGE_MAP(CWaveOutDialog, CDialog)
			ON_CBN_SELENDOK(IDC_CONFIG_SAMPLERATE, OnSelendokSamplerate)
			ON_CBN_SELENDOK(IDC_WAVEOUT_BITDEPTH, OnSelendokBitDepth)
			ON_EN_CHANGE(IDC_CONFIG_BUFNUM, OnChangeBufnum)
			ON_EN_CHANGE(IDC_CONFIG_BUFSIZE, OnChangeBufsize)
		END_MESSAGE_MAP()

		BOOL CWaveOutDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			std::vector<std::string> enums;
			waveout->RefreshAvailablePorts();
			waveout->GetPlaybackPorts(enums);
			// device list
			{
				for (int i = 0; i < enums.size(); i++)
				{
					m_deviceList.AddString(enums[i].c_str());
				}
					
				if (m_device >= enums.size())
					m_device = 0;

				m_deviceList.SetCurSel(m_device);
			}
			// samplerate
			{
				CString str;
				str.Format("%d", m_sampleRate);
				
				int i = m_sampleRateBox.SelectString(-1, str);
				if (i == CB_ERR)
					i = m_sampleRateBox.SelectString(-1, "44100");
			}
			// bit depth
			{
				switch(m_bitDepth) {
				case 16: if (m_dither) m_bitDepthBox.SetCurSel(1);
						 else  m_bitDepthBox.SetCurSel(0);
						 break;
				case 24: m_bitDepthBox.SetCurSel(2); break;
				case 32: m_bitDepthBox.SetCurSel(3); break;
				}
			}
			// buffers
			{
				m_bufNumSpin.SetRange(numbuf_min, numbuf_max);
				m_bufSamplesSpin.SetRange(numsamples_min, numsamples_max);
				UDACCEL acc;
				acc.nSec = 0;
				acc.nInc = 128;
				m_bufSamplesSpin.SetAccel(1, &acc);
			}
			RecalcLatency();
			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}

		void CWaveOutDialog::OnOK() 
		{
			CDialog::OnOK();
		}

		void CWaveOutDialog::RecalcLatency()
		{
			CString str;
			int lat = (m_bufSamples*m_bufNum* 1000) / m_sampleRate;

			str.Format("Latency: %dms", lat);
			m_latency.SetWindowText(str);
		}

		void CWaveOutDialog::OnChangeBufnum() 
		{
			if (!IsWindow(m_bufNumEdit.GetSafeHwnd()))
				return;

			CString str;
			m_bufNumEdit.GetWindowText(str);
			m_bufNum = atoi(str);

			if(m_bufNum < numbuf_min) { m_bufNum = numbuf_min; str.Format("%d", m_bufNum); m_bufNumEdit.SetWindowText(str); }
			else if(m_bufNum > numbuf_max) { m_bufNum = numbuf_max; str.Format("%d", m_bufNum); m_bufNumEdit.SetWindowText(str);}
			
			RecalcLatency();
		}

		void CWaveOutDialog::OnChangeBufsize() 
		{
			if (!IsWindow(m_bufSamplesEdit.GetSafeHwnd()))
				return;

			CString str;
			m_bufSamplesEdit.GetWindowText(str);
			m_bufSamples = atoi(str);
			if(m_bufSamples < numsamples_min) { m_bufSamples = numsamples_min; str.Format("%d", m_bufSamples); m_bufSamplesEdit.SetWindowText(str);}
			else if(m_bufSamples > numsamples_max) { m_bufSamples = numsamples_max; str.Format("%d", m_bufSamples); m_bufSamplesEdit.SetWindowText(str);}

			RecalcLatency();
		}

		void CWaveOutDialog::OnSelendokSamplerate() 
		{
			if (!IsWindow(m_sampleRateBox.GetSafeHwnd()))
				return;
			
			CString str;
			m_sampleRateBox.GetWindowText(str);
			m_sampleRate = atoi(str);
			RecalcLatency();
		}

		void CWaveOutDialog::OnSelendokBitDepth()
		{
			if (!IsWindow(m_bitDepthBox.GetSafeHwnd()))
				return;
			
			int pos = m_bitDepthBox.GetCurSel();
			switch(pos) {
			case 0: m_bitDepth = 16; m_dither=false; break;
			case 1: m_bitDepth = 16; m_dither=true; break;
			case 2: m_bitDepth = 24; m_dither=false; break;
			case 3:  m_bitDepth = 32; m_dither=false; break;
			}
		}

	}   // namespace
}   // namespace

