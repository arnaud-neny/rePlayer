///\file
///\brief implementation file for psycle::host::CParamList.
#include <psycle/host/detail/project.private.hpp>
#include "ParamList.hpp"

#include "FrameMachine.hpp"
#include "InputHandler.hpp"
#include "Machine.hpp"
/*
	MSDN:
 	When you implement a modeless dialog box, always override the OnCancel member
    function and call DestroyWindow from within it. Don't call the base class
	CDialog::OnCancel, because it calls EndDialog, which will make the dialog box
	invisible but will not destroy it. You should also override PostNcDestroy for
	modeless dialog boxes in order to delete this, since modeless dialog boxes are
	usually allocated with new. Modal dialog boxes are usually constructed on the
	frame and do not need PostNcDestroy cleanup.

	JosepMa:
	I added also listening to the ON_WM_CLOSE message and doing the same in the OnClose().
	Basically, OnCancel() works on Dialogs, when the users presses Cancel or Escape,
	and OnClose() happens when the users clicks on the close button or does alt+f4.
 */

namespace psycle { namespace host {

/*****************************************************************************/
/* Create : creates the dialog                                               */
/*****************************************************************************/

		CParamList::CParamList(Machine& effect, CFrameMachine* parent, CParamList** windowVar_)
			: CDialog(CParamList::IDD, AfxGetMainWnd())
			, machine(effect)
			, parentFrame(parent)
			, windowVar(windowVar_)
			, _quantizedvalue(0)
		{
			CDialog::Create(IDD, AfxGetMainWnd());
		}
		CParamList::~CParamList()
		{
		}

		void CParamList::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_CMBPROGRAM, m_program);
			DDX_Control(pDX, IDC_SLIDERPARAM, m_slider);
			DDX_Control(pDX, IDC_STATUSPARAM, m_text);
			DDX_Control(pDX, IDC_LISTPARAM, m_parlist);
		}

/*
		BEGIN_MESSAGE_MAP(CParamList, CDialog)
			ON_WM_CLOSE()
			ON_WM_VSCROLL()
			ON_LBN_SELCHANGE(IDC_LISTPARAM, OnSelchangeList)
			ON_CBN_SELCHANGE(IDC_CMBPROGRAM, OnSelchangeProgram)
			ON_NOTIFY(UDN_DELTAPOS, IDC_SPINPARAM, OnDeltaposSpin)
			ON_WM_CREATE()
		END_MESSAGE_MAP()
*/

		/////////////////////////////////////////////////////////////////////////////
		// CParamList diagnostics

	#if !defined  NDEBUG
		void CParamList::AssertValid() const
		{
			CDialog::AssertValid();
		}

		void CParamList::Dump(CDumpContext& dc) const
		{
			CDialog::Dump(dc);
		}
	#endif //!NDEBUG

		BOOL CParamList::PreTranslateMessage(MSG* pMsg) {
			if ((pMsg->message == WM_KEYDOWN) || (pMsg->message == WM_KEYUP)) {
				CmdDef def = PsycleGlobal::inputHandler().KeyToCmd(pMsg->wParam,0);
				if(def.GetType() == CT_Note) {
					parentFrame->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			return CDialog::PreTranslateMessage(pMsg);
		}
		BOOL CParamList::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			Init();
			return TRUE;
		}
		void CParamList::OnCancel() {
			DestroyWindow();
		}
		void CParamList::OnClose()
		{
			CDialog::OnClose();
			DestroyWindow();
		}
		void CParamList::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			if(windowVar!= NULL) *windowVar = NULL;
			delete this;
		}
		void CParamList::Init() 
		{
			UpdateParList();
			InitializePrograms();
			UpdateOne();
		}
		
		void CParamList::InitializePrograms()
		{
			m_program.ResetContent();
			int nump = machine.GetNumPrograms();
			for(int i(0) ; i < nump; ++i)
			{
				char s1[38];
				char s2[32];
				machine.GetIndexProgramName(0, i, s2);
				std::sprintf(s1,"%d: %s",i,s2);
				m_program.AddString(s1);
			}
			int i = machine.GetCurrentProgram();
			m_program.SetCurSel(i);
		}
		void CParamList::SelectProgram(long index) {
			m_program.SetCurSel(index);
			UpdateOne();
		}
		void CParamList::UpdateParList()
		{
			const int nPar= m_parlist.GetCurSel();
			m_parlist.ResetContent();

			const long int params = machine.GetNumParams();
			for(int i(0) ; i < params; ++i)
			{
				char str[80], buf[64];
				std::memset(str, 0, 64);
				machine.GetParamName(i, buf);
				std::sprintf(str, "%.3X: %s", i, buf);
				m_parlist.AddString(str);
			}
			if ( nPar != -1 )
				m_parlist.SetCurSel(nPar);
			else 
				m_parlist.SetCurSel(0);
		}

		void CParamList::UpdateText(int value)
		{
			char str[80],str2[18];
			machine.GetParamValue(m_parlist.GetCurSel(),str);
			std::sprintf(str2,"\t[Hex: %.4X]",value);
			std::strcat(str,str2);
			m_text.SetWindowText(str);
		}

		void CParamList::UpdateOne()
		{
			int min_v, max_v;
			min_v = max_v = 0;
			machine.GetParamRange(m_parlist.GetCurSel(),min_v,max_v);
			int const amp_v = max_v - min_v;
			int quantizedvalue = machine.GetParamValue(m_parlist.GetCurSel()) - min_v;
			UpdateText(quantizedvalue);
			m_slider.SetRange(0, amp_v);
			m_slider.SetPos(amp_v - quantizedvalue);
		}

		void CParamList::UpdateNew(int par,int value)
		{
			int min_v, max_v;
			min_v = max_v = 0;
			machine.GetParamRange(par,min_v,max_v);
			if (par != m_parlist.GetCurSel() )
			{
				int const amp_v = max_v - min_v;
				m_slider.SetRange(0, amp_v);
				m_parlist.SetCurSel(par);
			}
			int quantizedvalue = value - min_v;
			UpdateText(quantizedvalue);
			m_slider.SetPos(max_v - value);
		}
		void CParamList::OnSelchangeList() 
		{
			UpdateOne();
		}

		void CParamList::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
		{
			int min_v, max_v;
			min_v = max_v = 0;
			machine.GetParamRange(m_parlist.GetCurSel(),min_v,max_v);
			const int nVal = max_v - m_slider.GetPos();

			if(nVal != _quantizedvalue)
			{
				_quantizedvalue = nVal;
				machine.SetParameter(m_parlist.GetCurSel(), nVal);
				UpdateText(nVal);
				parentFrame->Automate(m_parlist.GetCurSel(), nVal, false, min_v);
			}
		}


		void CParamList::OnSelchangeProgram() 
		{
			int const se=m_program.GetCurSel();
			machine.SetCurrentProgram(se);
			UpdateOne();
			parentFrame->ChangeProgram(se);
			//todo: update parent.
		}

		void CParamList::OnDeltaposSpin(NMHDR* pNMHDR, LRESULT* pResult) 
		{
/*
			NM_UPDOWN* pNMUpDown = (NM_UPDOWN*)pNMHDR;
			const int se(m_program.GetCurSel() + pNMUpDown->iDelta);
			if(se >= 0 && se < machine.GetNumPrograms())
			{
				m_program.SetCurSel(se);
				machine.SetCurrentProgram(se);
				UpdateOne();
				parentFrame->ChangeProgram(se);
			}
			*pResult = 0;
*/
		}

	}   // namespace
}   // namespace
