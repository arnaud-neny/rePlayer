///\file
///\brief implementation file for psycle::host::CNewVal.
#include <psycle/host/detail/project.private.hpp>
#include "NewVal.hpp"

namespace psycle { namespace host {

		CNewVal::CNewVal(int mindex,int pindex, int vval, int vmin, int vmax,char* title, CWnd* pParent)
			: CDialog(CNewVal::IDD, pParent)
			, macindex(mindex) , paramindex(pindex), m_Value(vval) , v_min(vmin) , v_max(vmax)
		{
			strcpy(dlgtitle,title);
		}

		void CNewVal::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_EDIT1, m_value);
			DDX_Control(pDX, IDC_TEXT, m_text);
		}

/*
		BEGIN_MESSAGE_MAP(CNewVal, CDialog)
			ON_EN_UPDATE(IDC_EDIT1, OnUpdateEdit1)
		END_MESSAGE_MAP()
*/

		BOOL CNewVal::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			SetWindowText(dlgtitle);
			char buf[32];
			sprintf(buf,"%d",m_Value);
			m_value.SetWindowText(buf);
			m_value.SetSel(-1,-1,false);
			return TRUE;
		}

		void CNewVal::OnUpdateEdit1() 
		{
			char buffer[256];
			m_value.GetWindowText(buffer,16);
			m_Value=atoi(buffer);

			if (m_Value < v_min)
			{
				m_Value = v_min;
				sprintf(buffer,"Below Range. Use this HEX value: twk %.2X %.2X %.4X",paramindex,macindex,m_Value-v_min);
			}
			else if(m_Value > v_max)
			{
				m_Value = v_max;
				sprintf(buffer,"Above Range. Use this HEX value: twk %.2X %.2X %.4X",paramindex,macindex,m_Value-v_min);
			}
			else
			{
				sprintf(buffer,"Use this HEX value: twk %.2X %.2X %.4X",paramindex,macindex,m_Value-v_min);
			}
			m_text.SetWindowText(buffer);
		}

	}   // namespace
}   // namespace
