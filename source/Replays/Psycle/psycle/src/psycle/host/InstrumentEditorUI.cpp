#include <psycle/host/detail/project.private.hpp>
#include "InstrumentEditorUI.hpp"
#include "MainFrm.hpp"
#include "InputHandler.hpp"
#include <psycle/host/Song.hpp>

/////////////////////////////////////////////////////////////////////////////
// InstrumentEditorUI dialog

namespace psycle { namespace host {


IMPLEMENT_DYNAMIC(InstrumentEditorUI, CPropertySheet)

InstrumentEditorUI::InstrumentEditorUI(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
: CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
, init(false)
, windowVar_(NULL)
{
	m_hAccelTable = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_INSTRUMENTEDIT));
}

InstrumentEditorUI::InstrumentEditorUI(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
, init(false)
, windowVar_(NULL)
{
	m_hAccelTable = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_INSTRUMENTEDIT));
}
InstrumentEditorUI::~InstrumentEditorUI()
{
}

/*
BEGIN_MESSAGE_MAP(InstrumentEditorUI, CPropertySheet)
	ON_COMMAND(ID_ACCEL_SAMPLER, OnShowSampler)
	ON_COMMAND(ID_ACCEL_SAMPULSE, OnShowSampulse)
	ON_COMMAND(ID_ACCEL_WAVE, OnShowWaveBank)
	ON_COMMAND(ID_ACCEL_GEN, OnShowGen)
	ON_COMMAND(ID_ACCEL_VOL, OnShowVol)
	ON_COMMAND(ID_ACCEL_PAN, OnShowPan)
	ON_COMMAND(ID_ACCEL_FILTER, OnShowFilter)
	ON_WM_CLOSE()
END_MESSAGE_MAP()
*/

void InstrumentEditorUI::OnClose()
{
	CPropertySheet::OnClose();
	DestroyWindow();
}
void InstrumentEditorUI::PostNcDestroy()
{
	CPropertySheet::PostNcDestroy();
	if(windowVar_!= NULL) *windowVar_ = NULL;
	delete this;
}
BOOL InstrumentEditorUI::PreTranslateChildMessage(MSG* pMsg, HWND focusWin, Machine* mac)
{
	if (m_hAccelTable) {
      if (::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg)) {
         return(TRUE);
      }
    }
	if (pMsg->message == WM_KEYDOWN)
	{
		/*	DWORD dwID = GetDlgCtrlID(focusWin);
		if (dwID == IDD_SOMETHING)*/
		TCHAR out[256];
		GetClassName(focusWin,out,256);
		bool editbox=(strncmp(out,"Edit",5) == 0);
		if (pMsg->wParam == VK_ESCAPE) {
			PostMessage (WM_CLOSE);
			return TRUE;
		} else if (pMsg->wParam == VK_UP ||pMsg->wParam == VK_DOWN ||pMsg->wParam == VK_LEFT 
			|| pMsg->wParam == VK_RIGHT ||pMsg->wParam == VK_TAB || pMsg->wParam == VK_NEXT 
			||pMsg->wParam == VK_PRIOR || pMsg->wParam == VK_HOME|| pMsg->wParam == VK_END) {
			//default action.
			return FALSE;
		} else if (!editbox) {
			// get command
			CmdDef cmd = PsycleGlobal::inputHandler().KeyToCmd(pMsg->wParam,pMsg->lParam>>16);
			if(cmd.IsValid()) {
				const BOOL bRepeat = (pMsg->lParam>>16)&0x4000;
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				switch(cmd.GetType())
				{
				case CT_Note:
					{
						if (!bRepeat) {
							PsycleGlobal::inputHandler().PlayNote(cmd.GetNote(),255,127,true,mac);
						}
						return TRUE;
					}
					break;
				case CT_Editor:
					if (win->m_wndView.viewMode == view_modes::pattern)
					{
						PsycleGlobal::inputHandler().PerformCmd(cmd,bRepeat);
						return TRUE;
					}
					break;
				case CT_Immediate:
					{
						PsycleGlobal::inputHandler().PerformCmd(cmd,bRepeat);
						return TRUE;
					}
					break;
				default: break;
				}
			}
		}
	}
	else if (pMsg->message == WM_KEYUP)
	{
		TCHAR out[256];
		GetClassName(focusWin,out,256);
		bool editbox=(strncmp(out,"Edit",5) == 0);
		if (pMsg->wParam == VK_ESCAPE) {
			return TRUE;
		} else if (pMsg->wParam == VK_UP ||pMsg->wParam == VK_DOWN ||pMsg->wParam == VK_LEFT 
			|| pMsg->wParam == VK_RIGHT ||pMsg->wParam == VK_TAB || pMsg->wParam == VK_NEXT 
			||pMsg->wParam == VK_PRIOR || pMsg->wParam == VK_HOME|| pMsg->wParam == VK_END) {
			//default action.
			return FALSE;
		} else if (!editbox) {
			// get command
			CmdDef cmd = PsycleGlobal::inputHandler().KeyToCmd(pMsg->wParam,pMsg->lParam>>16);
			if(cmd.IsValid()) {
				const BOOL bRepeat = (pMsg->lParam>>16)&0x4000;
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				switch(cmd.GetType())
				{
				case CT_Note:
					{
						PsycleGlobal::inputHandler().StopNote(cmd.GetNote(),255,true,mac);
						return TRUE;
					}
					break;
				case CT_Editor:
					if (win->m_wndView.viewMode == view_modes::pattern)
					{
						PsycleGlobal::inputHandler().PerformCmd(cmd,bRepeat);
						return TRUE;
					}
					break;
				case CT_Immediate:
					{
						CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
						win->m_wndView.SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
						return TRUE;
					}
					break;
				default: break;
				}
			}
		}
	}

	return FALSE;
}


void InstrumentEditorUI::Init(InstrumentEditorUI** windowVar) 
{
	windowVar_ = windowVar;
	AddPage(&m_InstrBasic);
	AddPage(&m_InstrSampulse);
	AddPage(&m_SampleBank);
	init = true;
}
void InstrumentEditorUI::UpdateUI(bool force)
{
	if ( !init ) return;
	m_InstrBasic.WaveUpdate();
	m_InstrSampulse.WaveUpdate(force);
	m_SampleBank.WaveUpdate(force);
}

void InstrumentEditorUI::ShowSampler() {
	SetActivePage(0);
}
void InstrumentEditorUI::ShowSampulse() {
	SetActivePage(1);
}

void InstrumentEditorUI::OnShowSampler() {
	SetActivePage(0);
}
void InstrumentEditorUI::OnShowSampulse() {
	SetActivePage(1);
}
void InstrumentEditorUI::OnShowWaveBank() {
	SetActivePage(2);
}
void InstrumentEditorUI::OnShowGen() {
	SetActivePage(1);
	m_InstrSampulse.SetActivePage(0);
}
void InstrumentEditorUI::OnShowVol() {
	SetActivePage(1);
	m_InstrSampulse.SetActivePage(1);
}
void InstrumentEditorUI::OnShowPan() {
	SetActivePage(1);
	m_InstrSampulse.SetActivePage(2);
}
void InstrumentEditorUI::OnShowFilter() {
	SetActivePage(1);
	m_InstrSampulse.SetActivePage(3);
}


}}
