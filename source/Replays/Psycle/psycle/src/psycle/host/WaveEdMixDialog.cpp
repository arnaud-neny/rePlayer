///\file
///\brief implementation file for psycle::host::CWaveEdMixDialog.
#include <psycle/host/detail/project.private.hpp>
#include "WaveEdMixDialog.hpp"

#include <iomanip>
namespace psycle { namespace host {

		CWaveEdMixDialog::CWaveEdMixDialog(CWnd* pParent)
			: CDialog(CWaveEdMixDialog::IDD, pParent)
		{
			//{{AFX_DATA_INIT(CWaveEdMixDialog)
			//}}AFX_DATA_INIT
		}

		void CWaveEdMixDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CWaveEdMixDialog)
			DDX_Control(pDX, IDC_SRCVOL, m_srcVol);
			DDX_Control(pDX, IDC_DESTVOL, m_destVol);
			DDX_Control(pDX, IDC_FADEINCHECK, m_bFadeIn);
			DDX_Control(pDX, IDC_FADEOUTCHECK, m_bFadeOut);
			DDX_Control(pDX, IDC_FADEINTIME, m_fadeInTime);
			DDX_Control(pDX, IDC_FADEOUTTIME, m_fadeOutTime);
			DDX_Control(pDX, IDC_DESTVOL_TEXT, m_destVolText);
			DDX_Control(pDX, IDC_SRCVOL_TEXT, m_srcVolText);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CWaveEdMixDialog, CDialog)
			//{{AFX_MSG_MAP(CWaveEdMixDialog)
			//}}AFX_MSG_MAP
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_DESTVOL, OnCustomDrawDestSlider)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_SRCVOL, OnCustomDrawSrcSlider)
			ON_BN_CLICKED(IDC_FADEOUTCHECK, OnBnClickedFadeoutcheck)
			ON_BN_CLICKED(IDC_FADEINCHECK, OnBnClickedFadeincheck)
		END_MESSAGE_MAP()
*/

		BOOL CWaveEdMixDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_srcVol.SetRange(0, 2000);		// 0.0% to 200.0%
			m_destVol.SetRange(0, 2000);
			m_srcVol.SetPos(1000);
			m_destVol.SetPos(1000);
			m_bFadeIn.SetCheck(0);
			m_bFadeOut.SetCheck(0);
			bFadeIn=false;
			bFadeOut=false;
			std::ostringstream temp;
			temp<<"100%";
			m_srcVol.SetWindowText(temp.str().c_str());
			m_destVol.SetWindowText(temp.str().c_str());
			temp.str("");
			temp<<"0.000";
			m_fadeInTime.SetWindowText(temp.str().c_str());
			m_fadeOutTime.SetWindowText(temp.str().c_str());
			srcVol=destVol=fadeInTime=fadeOutTime=0;
			return true;
		}

		void CWaveEdMixDialog::OnOK() 
		{
			char temp[16];
			srcVol	= (2000-m_srcVol.GetPos())/1000.0f;
			destVol	= (2000-m_destVol.GetPos())/1000.0f;
			if(m_bFadeIn.GetCheck())
			{
				m_fadeInTime.GetWindowText(temp, 16);
				bFadeIn = (fadeInTime=atof(temp));
			}
			if(m_bFadeOut.GetCheck())
			{
				m_fadeOutTime.GetWindowText(temp, 16);
				bFadeOut = (fadeOutTime=atof(temp));
			}
			CDialog::OnOK();
		}

		void CWaveEdMixDialog::OnCancel() 
		{
			CDialog::OnCancel();
		}


		void CWaveEdMixDialog::OnCustomDrawDestSlider(NMHDR *pNMHDR, LRESULT *pResult)
		{
			//LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

			std::ostringstream temp;
			temp.setf(std::ios::fixed);
			float vol = (2000-m_destVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db = 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_destVolText.SetWindowText(temp.str().c_str());
	
			*pResult = 0;
		}

		void CWaveEdMixDialog::OnCustomDrawSrcSlider(NMHDR *pNMHDR, LRESULT *pResult)
		{
			//LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);

			std::ostringstream temp;
			temp.setf(std::ios::fixed);
			float vol = (2000-m_srcVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db= 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_srcVolText.SetWindowText(temp.str().c_str());

			*pResult = 0;
		}

		void CWaveEdMixDialog::OnBnClickedFadeoutcheck()
		{
			m_fadeOutTime.EnableWindow(m_bFadeOut.GetCheck()==BST_CHECKED);
		}

		void CWaveEdMixDialog::OnBnClickedFadeincheck()
		{
			m_fadeInTime.EnableWindow(m_bFadeIn.GetCheck()==BST_CHECKED);
		}

	}   // namespace
}   // namespace
