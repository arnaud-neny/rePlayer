///\file
///\brief implementation file for psycle::host::CGearRackDlg.
#include <psycle/host/detail/project.private.hpp>
#include "GearRackDlg.hpp"

#include "WaveEdFrame.hpp"
#include "MainFrm.hpp"
#include "ChildView.hpp"
#include "InputHandler.hpp"
#include "Song.hpp"
#include "Machine.hpp"

namespace psycle { namespace host {

		int CGearRackDlg::DisplayMode = 0;

		CGearRackDlg::CGearRackDlg(CMainFrame* pParent, CChildView* pMain, CGearRackDlg** windowVar_)
			: CDialog(CGearRackDlg::IDD, AfxGetMainWnd())
			, mainFrame(pParent)
			, mainView(pMain)
			, windowVar(windowVar_)
		{
			CDialog::Create(IDD, AfxGetMainWnd());
		}

		void CGearRackDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_PROPERTIES, m_props);
			DDX_Control(pDX, IDC_RADIO_WAVES, m_radio_wave);
			DDX_Control(pDX, IDC_RADIO_INS, m_radio_ins);
			DDX_Control(pDX, IDC_RADIO_GEN, m_radio_gen);
			DDX_Control(pDX, IDC_RADIO_EFX, m_radio_efx);
			DDX_Control(pDX, ID_TEXT, m_text);
			DDX_Control(pDX, IDC_GEARLIST, m_list);
		}

/*
		BEGIN_MESSAGE_MAP(CGearRackDlg, CDialog)
			ON_WM_CLOSE()
			ON_BN_CLICKED(IDC_CREATE, OnCreate)
			ON_BN_CLICKED(IDC_DELETE, OnDelete)
			ON_LBN_DBLCLK(IDC_GEARLIST, OnDblclkGearlist)
			ON_BN_CLICKED(IDC_PROPERTIES, OnProperties)
			ON_BN_CLICKED(IDC_SHOW_MASTER, OnMasterProperties)
			ON_BN_CLICKED(IDC_PARAMETERS, OnParameters)
			ON_LBN_SELCHANGE(IDC_GEARLIST, OnSelchangeGearlist)
			ON_BN_CLICKED(IDC_RADIO_EFX, OnRadioEfx)
			ON_BN_CLICKED(IDC_RADIO_GEN, OnRadioGen)
			ON_BN_CLICKED(IDC_RADIO_INS, OnRadioIns)
			ON_BN_CLICKED(IDC_RADIO_WAVES, OnRadioWaves)
			ON_BN_CLICKED(IDC_EXCHANGE, OnExchange)
			ON_BN_CLICKED(IDC_CLONEMACHINE, OnClonemachine)
		END_MESSAGE_MAP()
*/


		BOOL CGearRackDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			// fill our list box and select the currently selected machine
			RedrawList();
			return TRUE;
			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
		}
		void CGearRackDlg::OnCancel() {
			DestroyWindow();
		}
		void CGearRackDlg::OnClose()
		{
			AfxGetMainWnd()->SetFocus();
			CDialog::OnClose();
			DestroyWindow();
		}
		void CGearRackDlg::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			if(windowVar != NULL) *windowVar = NULL;
			delete this;
		}

		BOOL CGearRackDlg::PreTranslateMessage(MSG* pMsg) 
		{
			if ((pMsg->message == WM_KEYDOWN) || (pMsg->message == WM_KEYUP)) {
				CmdDef def = PsycleGlobal::inputHandler().KeyToCmd(pMsg->wParam,0);
				if(def.GetType() == CT_Note) {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			return CDialog::PreTranslateMessage(pMsg);
		}
		void CGearRackDlg::RedrawList() 
		{
			char buffer[64];
			m_list.ShowWindow(SW_HIDE);
			m_list.ResetContent();

			int selected=0;
			int b;

			CComboBox *cc=(CComboBox *)mainFrame->m_machineBar.GetDlgItem(IDC_AUXSELECT);
			switch (DisplayMode)
			{
			case 0:
				m_text.SetWindowText("Machines: Generators");
				m_props.SetWindowText("Properties");
				m_radio_gen.SetCheck(1);
				m_radio_efx.SetCheck(0);
				m_radio_ins.SetCheck(0);
				m_radio_wave.SetCheck(0);

				selected = Global::song().seqBus;
				if (selected >= MAX_BUSES)
				{
					selected = 0;
				}
				for (b=0; b<MAX_BUSES; b++) // Check Generators
				{
					sprintf(buffer,"%.2X: %s",b,Global::song()._pMachine[b] ? Global::song()._pMachine[b]->_editName :"empty");
					m_list.AddString(buffer);
				}
				break;
			case 1:
				m_text.SetWindowText("Machines: Effects");
				m_props.SetWindowText("Properties");
				m_radio_gen.SetCheck(0);
				m_radio_efx.SetCheck(1);
				m_radio_ins.SetCheck(0);
				m_radio_wave.SetCheck(0);

				selected = Global::song().seqBus;
				if (selected < MAX_BUSES || selected >= MAX_BUSES*2)
				{
					selected = 0;
				}
				else
				{
					selected -= MAX_BUSES;
				}
				for (b=MAX_BUSES; b<MAX_BUSES*2; b++) // Write Effects Names.
				{
					sprintf(buffer,"%.2X: %s",b,Global::song()._pMachine[b] ? Global::song()._pMachine[b]->_editName : "empty");
					m_list.AddString(buffer);
				}
				break;
			case 2:
				m_text.SetWindowText("Sampulse instruments");
				m_props.SetWindowText("Wave Editor");
				m_radio_gen.SetCheck(0);
				m_radio_efx.SetCheck(0);
				m_radio_ins.SetCheck(1);
				m_radio_wave.SetCheck(0);

				for (int b=0;b<XMInstrument::MAX_INSTRUMENT;b++)
				{
					sprintf(buffer, "%.2X: %s", b, Global::song().xminstruments.IsEnabled(b) ? Global::song().xminstruments[b].Name().c_str(): "empty");
					m_list.AddString(buffer);
				}
				if (cc->GetCurSel() != AUX_INSTRUMENT)
				{
					cc->SetCurSel(AUX_INSTRUMENT);
					mainFrame->UpdateComboIns(true);
				}
				selected = Global::song().instSelected;
				break;
			case 3:
				m_text.SetWindowText("Sampled sounds");
				m_props.SetWindowText("Wave Editor");
				m_radio_gen.SetCheck(0);
				m_radio_efx.SetCheck(0);
				m_radio_ins.SetCheck(0);
				m_radio_wave.SetCheck(1);

				for (int b=0;b<PREV_WAV_INS;b++)
				{
					sprintf(buffer, "%.2X: %s", b, Global::song().samples.IsEnabled(b) ? Global::song().samples[b].WaveName().c_str(): "empty");
					m_list.AddString(buffer);
				}
				if (cc->GetCurSel() != AUX_INSTRUMENT)
				{
					cc->SetCurSel(AUX_INSTRUMENT);
					mainFrame->UpdateComboIns(true);
				}
				selected = Global::song().waveSelected;
				break;
			}
			m_list.ShowWindow(SW_SHOW);
			m_list.SelItemRange(false,0,m_list.GetCount()-1);
			m_list.SetSel(selected,true);
		}

		void CGearRackDlg::OnSelchangeGearlist() 
		{
			int tmac = m_list.GetCurSel();
			switch (DisplayMode)
			{
			case 1:
				tmac += MAX_BUSES;
				//fallthrough
			case 0:
				mainFrame->ChangeGen(tmac);
				break;
			case 2:
				mainFrame->ChangeIns(tmac);
				break;
			case 3:
				mainFrame->ChangeWave(tmac);
				break;
			}
		}

		void CGearRackDlg::OnCreate() 
		{
			int tmac = m_list.GetCurSel();
			CComboBox *cc=(CComboBox *)mainFrame->m_machineBar.GetDlgItem(IDC_AUXSELECT);
			switch (DisplayMode)
			{
			case 1:
				tmac += MAX_BUSES;
				//fallthrough
			case 0:
				{
					mainView->NewMachine(-1,-1,tmac);

					mainFrame->UpdateEnvInfo();
					mainFrame->UpdateComboGen();
					if (mainView->viewMode==view_modes::machine)
					{
						mainView->Repaint();
					}
				}
				break;
			case 2:
				{
				cc->SetCurSel(AUX_INSTRUMENT);
				mainFrame->LoadInst(tmac);
				//LoadInst does a call to refresh the editors, which includes our editor.
				//Our RedrawList() will be called.
				break;
			}
			case 3:
				cc->SetCurSel(AUX_INSTRUMENT);
				mainFrame->LoadWave(tmac);
				//LoadWave does a call to refresh the editors, which includes our editor.
				//Our RedrawList() will be called.
				break;
			}
			RedrawList();
		}

		void CGearRackDlg::OnDelete() 
		{
			int tmac = m_list.GetCurSel();
			PsycleGlobal::inputHandler().AddMacViewUndo();
			switch (DisplayMode)
			{
			case 1:
				tmac+=MAX_BUSES;
				//fallthrough
			case 0:
				if (MessageBox("Are you sure?","Delete Machine", MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
				{
					if (Global::song()._pMachine[tmac])
					{
						mainFrame->CloseMacGui(tmac);
						{
							CExclusiveLock lock(&Global::song().semaphore, 2, true);
							Global::song().DeleteMachineRewiring(tmac);
						}
						mainFrame->UpdateEnvInfo();
						mainFrame->UpdateComboGen();
						if (mainView->viewMode==view_modes::machine)
						{
							mainView->Repaint();
						}
					}
				}
				break;
			case 2:
				if (Global::song().xminstruments.Exists(Global::song().instSelected)
					&& MessageBox("Are you sure?","Delete Samples", MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
				{
					XMInstrument & inst = Global::song().xminstruments.get(Global::song().instSelected);
					std::set<int> sampNums =  inst.GetWavesUsed();
					if (sampNums.size() > 0) {
						int result = MessageBox(_T("This instrument uses one or more samples. Do you want to ALSO delete the samples?"),
							_T("Deleting Instrument"),MB_YESNOCANCEL | MB_ICONQUESTION);
						if (result == IDYES) {
							CExclusiveLock lock(&Global::song().semaphore, 2, true);
							for (std::set<int>::iterator it = sampNums.begin(); it != sampNums.end();++it) {
								Global::song().samples.RemoveAt(*it);
							}
						}
						else if (result == IDCANCEL) {
							return;
						}
					}
					inst.Init();
					Global::song().DeleteVirtualOfInstrument(Global::song().instSelected,true);
					mainFrame->UpdateComboIns(true);
					mainFrame->UpdateInstrumentEditor();
				}
				break;
			case 3:
				if (MessageBox("Are you sure?","Delete Samples", MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
				{
					{
						CExclusiveLock lock(&Global::song().semaphore, 2, true);
						Global::song().DeleteInstrument(Global::song().waveSelected);
					}
					mainFrame->UpdateComboIns(true);
					mainFrame->UpdateInstrumentEditor();
				}
				break;
			}
			RedrawList();
		}

		void CGearRackDlg::OnDblclkGearlist() 
		{
			OnCreate();	
		}


		void CGearRackDlg::OnProperties() 
		{
			int tmac = m_list.GetCurSel();
			switch (DisplayMode)
			{
			case 0:
				if (Global::song()._pMachine[tmac])
				{
					mainView->DoMacPropDialog(tmac);
					mainFrame->UpdateEnvInfo();
					mainFrame->UpdateComboGen();
					if (mainView->viewMode==view_modes::machine)
					{
						mainView->Repaint();
					}
				}
				break;
			case 1:
				if (Global::song()._pMachine[tmac+MAX_BUSES])
				{
					mainView->DoMacPropDialog(tmac+MAX_BUSES);
					mainFrame->UpdateEnvInfo();
					mainFrame->UpdateComboGen();
					if (mainView->viewMode==view_modes::machine)
					{
						mainView->Repaint();
					}
				}
				break;
			case 2:
				//fallthrough
			case 3:
				mainFrame->ShowWaveEditor();
				break;
			}
			RedrawList();
		}

		void CGearRackDlg::OnParameters() 
		{
			POINT point;
			GetCursorPos(&point);
			int tmac = m_list.GetCurSel();
			switch (DisplayMode)
			{
			case 0:
				if (Global::song()._pMachine[tmac])
				{
					mainFrame->ShowMachineGui(tmac,point);
				}
				break;
			case 1:
				if (Global::song()._pMachine[tmac+MAX_BUSES])
				{
					mainFrame->ShowMachineGui(tmac+MAX_BUSES,point);
				}
				break;
			case 2:
				//fallthrough
			case 3:
				mainFrame->ShowInstrumentEditor();
				break;
			}
			RedrawList();
		}
		void CGearRackDlg::OnMasterProperties()
		{
			POINT point;
			GetCursorPos(&point);
			mainFrame->ShowMachineGui(MASTER_INDEX,point);
		}

		void CGearRackDlg::OnRadioGen() 
		{
			DisplayMode = 0;
			RedrawList();
		}
		void CGearRackDlg::OnRadioEfx() 
		{
			DisplayMode = 1;
			RedrawList();
		}
		void CGearRackDlg::OnRadioIns() 
		{
			DisplayMode = 2;
			RedrawList();
		}
		void CGearRackDlg::OnRadioWaves() 
		{
			DisplayMode = 3;
			RedrawList();
		}

		void CGearRackDlg::OnExchange() 
		{
			if ( m_list.GetSelCount() != 2 )
			{
				MessageBox("This option requires that you select two entries","Gear Rack Dialog");
				return;
			}

			int sel[2]={0,0},j=0;
			const int maxitems=m_list.GetCount();
			for (int c=0;c<maxitems;c++) 
			{
				if ( m_list.GetSel(c) != 0) sel[j++]=c;
			}

			switch (DisplayMode) // should be necessary to rename opened parameter windows.
			{
			case 1:
				sel[0]+=MAX_BUSES;
				sel[1]+=MAX_BUSES;
				//fallthrough
			case 0:
				PsycleGlobal::inputHandler().AddMacViewUndo();
				Global::song().ExchangeMachines(sel[0],sel[1]);
				mainFrame->UpdateComboGen(true);
				if (mainView->viewMode==view_modes::machine)
				{
					mainView->Repaint();
				}
				break;
			case 2:
				//TODO: finish
				MessageBox("Unfinished");
				break;
			case 3:
				PsycleGlobal::inputHandler().AddMacViewUndo();
				Global::song().ExchangeInstruments(sel[0],sel[1]);
				
				mainFrame->UpdateComboIns(true);
				break;
			}
			
			RedrawList();
		}

		void CGearRackDlg::OnClonemachine() 
		{
			int tmac1 = m_list.GetCurSel();
			int tmac2 = -1;

			if ( m_list.GetSelCount() == 2 )
			{
				int sel[2]={0,0},j=0;
				const int maxitems=m_list.GetCount();
				for (int c=0;c<maxitems;c++) 
				{
					if ( m_list.GetSel(c) != 0) sel[j++]=c;
				}

				tmac1 = sel[0];
				tmac2 = sel[1];
			}
			else if ( m_list.GetSelCount() != 1 )
			{
				MessageBox("Select 1 active slot (and optionally 1 empty destination slot)","Gear Rack Dialog");
				return;
			}

			// now lets do the actual work...
			switch (DisplayMode) // should be necessary to rename opened parameter windows.
			{
			case 0:
				if (tmac2 < 0)
				{
					tmac2 = Global::song().GetFreeBus();
				}
				if (tmac2 >= 0)
				{
					if (!Global::song().CloneMac(tmac1,tmac2))
					{
						MessageBox("Select 1 active slot (and optionally 1 empty destination slot)","Gear Rack Dialog");
						return;
					}
				}
				mainFrame->UpdateComboGen(true);
				if (mainView->viewMode==view_modes::machine)
				{
					mainView->Repaint();
				}
				break;
			case 1:
				tmac1+=MAX_BUSES;
				if (tmac2 >= 0)
				{
					tmac2+=MAX_BUSES;
				}
				else
				{
					tmac2 = Global::song().GetFreeFxBus();
				}
				if (tmac2 >= 0)
				{
					if (!Global::song().CloneMac(tmac1,tmac2))
					{
						MessageBox("Select 1 active slot (and optionally 1 empty destination slot)","Gear Rack Dialog");
						return;
					}
				}
				mainFrame->UpdateComboGen(true);
				if (mainView->viewMode==view_modes::machine)
				{
					mainView->Repaint();
				}
				break;
			case 2:
				//TODO: finish
				MessageBox("Unfinished");
				break;
			case 3:
				if (tmac2 < 0)
				{
					for (int i = 0; i < MAX_INSTRUMENTS; i++)
					{
						if (Global::song().samples.IsEnabled(i) == false)
						{
							tmac2 = i;
							break;
						}
					}
				}
				if (tmac2 >=0)
				{
					if (!Global::song().CloneIns(tmac1,tmac2))
					{
						MessageBox("Select 1 active slot (and optionally 1 empty destination slot)","Gear Rack Dialog");
						return;
					}
				}
				
				mainFrame->UpdateComboIns(true);
				break;
			}
			
			RedrawList();
		}

	}   // namespace
}   // namespace
