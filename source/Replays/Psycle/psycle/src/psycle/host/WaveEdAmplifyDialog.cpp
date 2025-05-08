///\file
///\brief implementation file for psycle::host::CWaveEdAmplifyDialog.

#include <psycle/host/detail/project.private.hpp>
#include "WaveEdAmplifyDialog.hpp"

namespace psycle { namespace host {

		CWaveEdAmplifyDialog::CWaveEdAmplifyDialog(CWnd* pParent)
			: CDialog(CWaveEdAmplifyDialog::IDD, pParent)
		{
		}

		void CWaveEdAmplifyDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_EDIT2, m_dbedit);
			DDX_Control(pDX, IDC_SLIDER3, m_slider);
		}

/*
		BEGIN_MESSAGE_MAP(CWaveEdAmplifyDialog, CDialog)
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_SLIDER3, OnCustomdrawSlider)
		END_MESSAGE_MAP()
*/

		BOOL CWaveEdAmplifyDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_slider.SetRange(-9600, 4800);
			//Hack to fix "0 placed on leftmost on start".
			m_slider.SetPos(-9600);
			m_slider.SetPos(0);
			return TRUE;
		}

		void CWaveEdAmplifyDialog::OnCustomdrawSlider(NMHDR* pNMHDR, LRESULT* pResult) 
		{
			float db = float(m_slider.GetPos())*0.01f;
			std::ostringstream temp;
			temp.setf(std::ios::fixed);
// 			temp<<std::setprecision(2)<<db;
			m_dbedit.SetWindowText(temp.str().c_str());
			*pResult = 0;
		}

		void CWaveEdAmplifyDialog::OnOK() 
		{
			char db_t[10];
			int db_i = 0;		
			m_dbedit.GetWindowText(db_t,10);
			db_i = (int)(100*atof(db_t));
			if (db_i) EndDialog( db_i );
			else {
				MessageBox("Warning: The amplification is zero, or the value isn't correct. Check it.");
			}
			//	CDialog::OnOK();
		}

		void CWaveEdAmplifyDialog::OnCancel() 
		{
			EndDialog( AMP_DIALOG_CANCEL );
			//CDialog::OnCancel();
		}

	}   // namespace
}   // namespace
