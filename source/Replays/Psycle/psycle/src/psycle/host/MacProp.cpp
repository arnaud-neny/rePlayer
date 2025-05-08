///\file
///\brief implementation file for psycle::host::CMacProp.
#include <psycle/host/detail/project.private.hpp>
#include "MacProp.hpp"

#include "MainFrm.hpp"
#include "InputHandler.hpp"
#include "Machine.hpp"

namespace psycle { namespace host {

		extern CPsycleApp theApp;

		CMacProp::CMacProp(Song& song, CWnd* pParent)
			: CDialog(CMacProp::IDD, pParent)
			, thesong(song)
		{
			m_view=NULL;
		}

		void CMacProp::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_SOLO, m_soloCheck);
			DDX_Control(pDX, IDC_BYPASS, m_bypassCheck);
			DDX_Control(pDX, IDC_MUTE, m_muteCheck);
			DDX_Control(pDX, IDC_MAC_NAME, m_macname);
			DDX_Control(pDX, IDC_NEWMACBEFORE, m_macbefore);
		}

/*
		BEGIN_MESSAGE_MAP(CMacProp, CDialog)
			ON_EN_CHANGE(IDC_MAC_NAME, OnChangeMacName)
			ON_BN_CLICKED(IDC_DELETE, OnDelete)
			ON_BN_CLICKED(IDC_MUTE, OnMute)
			ON_BN_CLICKED(IDC_BYPASS, OnBypass)
			ON_BN_CLICKED(IDC_SOLO, OnSolo)
			ON_BN_CLICKED(IDC_CLONE, OnClone)
			ON_BN_CLICKED(IDC_NEWMACBEFORE, OnAddNewBefore)
			ON_BN_CLICKED(IDC_NEWMACAFTER, OnAddNewAfter)
			ON_BN_CLICKED(IDC_REPLACEMAC, OnBnClickedReplacemac)
		END_MESSAGE_MAP()
*/

		BOOL CMacProp::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			deleted=false;

			m_macname.SetLimitText(31);
			char buffer[64];
			sprintf(buffer,"%.2X : %s Properties",thisMac,pMachine->_editName);
			SetWindowText(buffer);

			m_macname.SetWindowText(pMachine->_editName);

			m_muteCheck.SetCheck(pMachine->_mute);
			m_soloCheck.SetCheck(thesong.machineSoloed == thisMac);
			m_bypassCheck.SetCheck(pMachine->Bypass());
			if (pMachine->_mode == MACHMODE_GENERATOR ) 
			{
				m_bypassCheck.ShowWindow(SW_HIDE);
				m_macbefore.EnableWindow(FALSE);
			}
			else if(pMachine->_type == MACH_MIXER)
			{
				m_soloCheck.ShowWindow(SW_HIDE);
				m_macbefore.EnableWindow(FALSE);
			}
			else
			{
				m_soloCheck.ShowWindow(SW_HIDE);
			}
			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}

		void CMacProp::OnChangeMacName() 
		{
			m_macname.GetWindowText(txt, 32);
		}

		void CMacProp::OnDelete() 
		{
			// Delete MACHINE!
			if (MessageBox("Are you sure?","Delete Machine", MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();

				deleted = true;
				PostMessage (WM_CLOSE);
			}
		}

		void CMacProp::OnMute() 
		{
			pMachine->_mute = (m_muteCheck.GetCheck() == 1);
			pMachine->_volumeCounter=0.0f;
			pMachine->_volumeDisplay = 0;
			if ( m_view != NULL )
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				m_view->updatePar=thisMac;
				m_view->Repaint(draw_modes::machine);
			}
		}
		void CMacProp::OnBypass() 
		{
			pMachine->Bypass(m_bypassCheck.GetCheck() == 1);
			if ( m_view != NULL )
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				m_view->updatePar=thisMac;
				m_view->Repaint(draw_modes::machine);
			}
		}

		void CMacProp::OnSolo() 
		{
			PsycleGlobal::inputHandler().AddMacViewUndo();
			if (m_soloCheck.GetCheck() == 1)
			{
				thesong.SoloMachine(thisMac);
				if ( m_muteCheck.GetCheck() ) m_muteCheck.SetCheck(0);
			}
			else
			{
				thesong.SoloMachine(-1);
				if ( m_muteCheck.GetCheck() ) m_muteCheck.SetCheck(0);
			}
			if ( m_view != NULL )
			{
				m_view->Repaint(draw_modes::all_machines);
			}
		}

		void CMacProp::OnClone() 
		{
			int src = pMachine->_macIndex;
			int dst = -1;

			if ((src < MAX_BUSES) && (src >=0))
			{
				dst = thesong.GetFreeBus();
			}
			else if ((src < MAX_BUSES*2) && (src >= MAX_BUSES))
			{
				dst = thesong.GetFreeFxBus();
			}
			if (dst >= 0)
			{
				if (!thesong.CloneMac(src,dst))
				{
					MessageBox("Cloning failed. Is your song directory correctly configured?","Cloning failed");
				}
				if ( m_view != NULL )
				{
					((CMainFrame *)theApp.m_pMainWnd)->UpdateComboGen(true);
					if (m_view->viewMode==view_modes::machine)
					{
						m_view->Repaint();
					}
				}
			}
			PostMessage (WM_CLOSE);
		}
		void CMacProp::OnAddNewBefore()
		{
			int newMacidx = thesong.GetFreeFxBus();
			m_view->NewMachine(pMachine->_x-16,pMachine->_y-16,newMacidx);

			if( newMacidx != -1 && thesong._pMachine[newMacidx]) {
				Machine* newMac = thesong._pMachine[newMacidx];
				CExclusiveLock lock(&thesong.semaphore, 2, true);
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					if(pMachine->inWires[i].Enabled()) {
						Machine& srcMac = pMachine->inWires[i].GetSrcMachine();
						int wiresrc = srcMac.FindOutputWire(pMachine->_macIndex);
						thesong.ChangeWireDestMacNonBlocking(&srcMac,newMac,wiresrc,newMac->GetFreeInputWire(0));
					}
				}
				thesong.InsertConnectionNonBlocking(newMac, pMachine);
				CMainFrame* pParentMain = ((CMainFrame *)theApp.m_pMainWnd);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (m_view->viewMode==view_modes::machine)
				{
					m_view->Repaint();
				}
				PostMessage (WM_CLOSE);
			}
		}
		void CMacProp::OnAddNewAfter()
		{
			int newMacidx = thesong.GetFreeFxBus();
			m_view->NewMachine(pMachine->_x+16,pMachine->_y+16,newMacidx);

			if( newMacidx != -1 && thesong._pMachine[newMacidx]) {
				Machine* newMac = thesong._pMachine[newMacidx];
				CExclusiveLock lock(&thesong.semaphore, 2, true);
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					Wire* wire = pMachine->outWires[i];
					if(wire && wire->Enabled()) {
						Machine& dstMac = wire->GetDstMachine();
						int wiredst = wire->GetDstWireIndex();
						thesong.ChangeWireSourceMacNonBlocking(newMac,&dstMac,newMac->GetFreeOutputWire(0),wiredst);
					}
				}
				thesong.InsertConnectionNonBlocking(pMachine, newMac);
				CMainFrame* pParentMain = ((CMainFrame *)theApp.m_pMainWnd);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (m_view->viewMode==view_modes::machine)
				{
					m_view->Repaint();
				}
				PostMessage (WM_CLOSE);
			}
		}

		void CMacProp::OnBnClickedReplacemac()
		{
			int index = pMachine->_macIndex;
			m_view->NewMachine(pMachine->_x,pMachine->_y,index);

			Machine* newMac = thesong._pMachine[index];
			if(newMac) {
				CMainFrame* pParentMain = ((CMainFrame *)theApp.m_pMainWnd);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (m_view->viewMode==view_modes::machine)
				{
					m_view->Repaint();
				}
				PostMessage (WM_CLOSE);
			}
		}

	}   // namespace
}   // namespace
