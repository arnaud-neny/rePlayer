///\file
///\brief implementation file for psycle::host::CSwingFillDlg.
#include <psycle/host/detail/project.private.hpp>
#include "SwingFillDlg.hpp"

namespace psycle { namespace host {

		CSwingFillDlg::CSwingFillDlg() : CDialog(CSwingFillDlg::IDD)
		{
			//{{AFX_DATA_INIT(CSwingFillDlg)
				// NOTE: the ClassWizard will add member initialization here
			//}}AFX_DATA_INIT
		}


		void CSwingFillDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CSwingFillDlg)
			DDX_Control(pDX, IDC_CENTER_TEMPO, m_Tempo);
			DDX_Control(pDX, IDC_WIDTH, m_Width);
			DDX_Control(pDX, IDC_VARIANCE, m_Variance);
			DDX_Control(pDX, IDC_PHASE, m_Phase);
			DDX_Control(pDX, IDC_OFFSET, m_Offset);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CSwingFillDlg, CDialog)
			//{{AFX_MSG_MAP(CSwingFillDlg)
				// NOTE: the ClassWizard will add message map macros here
			//}}AFX_MSG_MAP
		END_MESSAGE_MAP()
*/

		BOOL CSwingFillDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			bGo = FALSE;
			
			char buf[32];
			sprintf(buf,"%d",tempo);
			m_Tempo.SetWindowText(buf);
			m_Tempo.SetSel(-1,-1,false);

			sprintf(buf,"%d",width);
			m_Width.SetWindowText(buf);
			m_Width.SetSel(-1,-1,false);

			sprintf(buf,"%.2f",variance);
			m_Variance.SetWindowText(buf);
			m_Variance.SetSel(-1,-1,false);
			
			sprintf(buf,"%.2f",phase);
			m_Phase.SetWindowText(buf);
			m_Phase.SetSel(-1,-1,false);

			m_Offset.SetCheck(offset?1:0);

			return TRUE;
		}

		void CSwingFillDlg::OnOK() 
		{
//\todo: Redo the SwingFillDlg so that it does this:
/*
* Swing depth : will determine how much time from the second line is given to the first line. negative values would do the opposite (I am unsure of the option range right now).

Optional options:

* Swing LFO: an LFO over the swing depth. This means that the swing depth would be variable over time.

* Swing LFO speed: the speed of this LFO. determines how fast it would do a cycle.


Several notes:

This procedure does not mess with BPM nor with LPB so it shouldn't cause any problem to the other plugins. Just remember that delays will not swing.

The beat is always at the same place if not using the LFO with special values
 */

			bGo = TRUE;
			char buf[32];
			m_Tempo.GetWindowText(buf,32);
			tempo=atoi(buf);
			if (tempo < 33)
				tempo = 33;
			else if (tempo > 999)
				tempo = 999;

			m_Width.GetWindowText(buf,32);
			width=atoi(buf);
			if (width < 1)
				width = 1;

			m_Variance.GetWindowText(buf,32);
			variance=float(atof(buf));

			m_Phase.GetWindowText(buf,32);
			phase=float(atof(buf));

			offset = m_Offset.GetCheck()?true:false;

			CDialog::OnOK();
		}
	}   // namespace
}   // namespace
