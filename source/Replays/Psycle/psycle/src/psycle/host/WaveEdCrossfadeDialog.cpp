///\file
///\brief implementation file for psycle::host::CWaveEdCrossfadeDialog.
#include <psycle/host/detail/project.private.hpp>
#include "WaveEdCrossfadeDialog.hpp"
#include <iomanip>

namespace psycle { namespace host {

		CWaveEdCrossfadeDialog::CWaveEdCrossfadeDialog(CWnd* pParent)
			: CDialog(CWaveEdCrossfadeDialog::IDD, pParent)
		{
			//{{AFX_DATA_INIT(CWaveEdCrossfadeDialog)
			//}}AFX_DATA_INIT
		}

		void CWaveEdCrossfadeDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CWaveEdCrossfadeDialog)
			DDX_Control(pDX, IDC_SRCSTARTVOL, m_srcStartVol);
			DDX_Control(pDX, IDC_SRCENDVOL, m_srcEndVol);
			DDX_Control(pDX, IDC_DESTSTARTVOL, m_destStartVol);
			DDX_Control(pDX, IDC_DESTENDVOL, m_destEndVol);
			DDX_Control(pDX, IDC_SRCSTARTVOL_TEXT, m_srcStartVolText);
			DDX_Control(pDX, IDC_SRCENDVOL_TEXT, m_srcEndVolText);
			DDX_Control(pDX, IDC_DESTSTARTVOL_TEXT, m_destStartVolText);
			DDX_Control(pDX, IDC_DESTENDVOL_TEXT, m_destEndVolText);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CWaveEdCrossfadeDialog, CDialog)
			//{{AFX_MSG_MAP(CWaveEdCrossfadeDialog)
			//}}AFX_MSG_MAP
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_DESTSTARTVOL, OnCustomDrawSliders)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_SRCSTARTVOL, OnCustomDrawSliders)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_DESTENDVOL, OnCustomDrawSliders)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_SRCENDVOL, OnCustomDrawSliders)

		END_MESSAGE_MAP()
*/

		BOOL CWaveEdCrossfadeDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_srcStartVol.SetRange(0, 2000);		// 0.0% to 200.0%
			m_destStartVol.SetRange(0, 2000);
			m_srcEndVol.SetRange(0, 2000);
			m_destEndVol.SetRange(0, 2000);
			m_srcStartVol.SetPos(2000);
			m_destStartVol.SetPos(1000);
			m_srcEndVol.SetPos(1000);
			m_destEndVol.SetPos(2000);
			return true;
		}

		void CWaveEdCrossfadeDialog::OnOK() 
		{
			srcStartVol		= (2000-m_srcStartVol.GetPos())	/ 1000.0f;
			srcEndVol		= (2000-m_srcEndVol.GetPos())	/ 1000.0f;
			destStartVol	= (2000-m_destStartVol.GetPos())/ 1000.0f;
			destEndVol		= (2000-m_destEndVol.GetPos())	/ 1000.0f;

			CDialog::OnOK();
		}

		void CWaveEdCrossfadeDialog::OnCancel() 
		{
			CDialog::OnCancel();
		}


		void CWaveEdCrossfadeDialog::OnCustomDrawSliders(NMHDR *pNMHDR, LRESULT *pResult)
		{
			float vol;
			std::ostringstream temp;
			temp.setf(std::ios::fixed);
			
			vol = (2000-m_srcStartVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db = 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_srcStartVolText.SetWindowText(temp.str().c_str());

			temp.str("");

			vol = (2000-m_srcEndVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db= 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_srcEndVolText.SetWindowText(temp.str().c_str());

			temp.str("");

			vol = (2000-m_destStartVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db = 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_destStartVolText.SetWindowText(temp.str().c_str());

			temp.str("");

			vol = (2000-m_destEndVol.GetPos())/10.0f;
			if(vol==0)
				temp<<"0.0%"<<std::endl<<"(-inf. dB)";
			else
			{
				float db= 20 * log10(vol/100.0f);
				temp<<std::setprecision(1)<<vol<<"%"<<std::endl<<"("<<db<<"dB)";
			}
			m_destEndVolText.SetWindowText(temp.str().c_str());

			*pResult = 0;
		}

	}   // namespace
}   // namespace
