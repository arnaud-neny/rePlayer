///\file
///\brief implementation file for psycle::host::CVolumeDlg.
#include <psycle/host/detail/project.private.hpp>
#include "VolumeDlg.hpp"
#include <cmath>

namespace psycle { namespace host {
		CVolumeDlg::CVolumeDlg(CWnd* pParent) : CDialog(CVolumeDlg::IDD, pParent)
		{
			//{{AFX_DATA_INIT(CVolumeDlg)
			//}}AFX_DATA_INIT
			go = false;
		}

		void CVolumeDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CVolumeDlg)
			DDX_Control(pDX, IDC_EDIT_DB, m_db);
			DDX_Control(pDX, IDC_EDIT_PER, m_per);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CVolumeDlg, CDialog)
			//{{AFX_MSG_MAP(CVolumeDlg)
			ON_EN_CHANGE(IDC_EDIT_DB, OnChangeEditDb)
			ON_EN_CHANGE(IDC_EDIT_PER, OnChangeEditPer)
			//}}AFX_MSG_MAP
		END_MESSAGE_MAP()
*/

		BOOL CVolumeDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			DrawPer();
			DrawDb();
			if (edit_type)
			{
				m_per.SetFocus();
				m_per.SetSel(0,32);
			}
			else
			{
				m_db.SetFocus();
				m_db.SetSel(0,32);
			}
			go = true;
			return FALSE;
		}

		void CVolumeDlg::DrawDb() 
		{
			char bufdb[32];

			if (volume > 4.0f)
			{
				volume = 4.0f;
			}
			if (volume > 1.0f)
			{	
				sprintf(bufdb,"+%.1f",20.0f * log10(volume)); 
			}
			else if (volume == 1.0f)
			{	
				sprintf(bufdb,"0.0"); 
			}
			else if (volume > 0.0f)
			{	
				sprintf(bufdb,"%.1f",20.0f * log10(volume)); 
			}
			else 
			{				
				volume = 0.0f;
				sprintf(bufdb,"-999.9"); 
			}
			go = false;
			m_db.SetWindowText(bufdb);
			go = true;
		}

		void CVolumeDlg::DrawPer() 
		{
			char bufper[32];

			if (volume > 4.0f)
			{
				volume = 4.0f;
			}
			if (volume > 1.0f)
			{	
				sprintf(bufper,"%.2f%",volume*100); 
			}
			else if (volume == 1.0f)
			{	
				std::sprintf(bufper,"100.00%"); 
			}
			else if (volume > 0.0f)
			{	
				std::sprintf(bufper,"%.2f",volume*100); 
			}
			else 
			{
				volume = 0.0f;
				std::sprintf(bufper,"0.00"); 
			}
			go = false;
			m_per.SetWindowText(bufper);
			go = true;
		}

		void CVolumeDlg::OnChangeEditDb() 
		{
			// note: If this is a RICHEDIT control, the control will not
			// send this notification unless you override the CDialog::OnInitDialog()
			// function and call CRichEditCtrl().SetEventMask()
			// with the ENM_CHANGE flag ORed into the mask.
			if (go)
			{
				char buf[32];
				m_db.GetWindowText(buf,31);

				float val = float(atof(buf));
				volume = powf(10.0,val/20.0f);
				DrawPer();	
			}
		}

		void CVolumeDlg::OnChangeEditPer() 
		{
			// note: If this is a RICHEDIT control, the control will not
			// send this notification unless you override the CDialog::OnInitDialog()
			// function and call CRichEditCtrl().SetEventMask()
			// with the ENM_CHANGE flag ORed into the mask.
			if (go)
			{
				char buf[32];
				m_per.GetWindowText(buf,31);

				float val = float(atof(buf));

				volume = val/100.0f;
				DrawDb();
			}
		}
	}   // namespace
}   // namespace
