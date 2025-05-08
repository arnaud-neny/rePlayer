/** @file 
 *  @brief MachineBar dialog
 *  $Date: 2010-08-15 18:18:35 +0200 (dg., 15 ag 2010) $
 *  $Revision: 9831 $
 */
#include <psycle/host/detail/project.private.hpp>
#include "MachineBar.hpp"
#include "ChildView.hpp"
#include "InputHandler.hpp"
#include "MainFrm.hpp"
#include "Song.hpp"
#include "Machine.hpp"
#include "Plugin.hpp"
#include "XMSongLoader.hpp"
#include "ITModule2.h"
#include "WavFileDlg.hpp"
#include "GearRackDlg.hpp"
#include "WaveEdFrame.hpp"
#include <psycle/helpers/riffwave.hpp>
#include <psycle/helpers/filetypedetector.hpp>
#include <psycle/helpers/pathname_validate.hpp>

namespace psycle{ namespace host{
	extern CPsycleApp theApp;
IMPLEMENT_DYNAMIC(MachineBar, CDialogBar)

	int MachineBar::wavFilterSelected=0;
	int MachineBar::insFilterSelected=0;


	MachineBar::MachineBar()
	{
	}

	MachineBar::~MachineBar()
	{
	}

	void MachineBar::DoDataExchange(CDataExchange* pDX)
	{
		CDialogBar::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_COMBOSTEP, m_stepcombo);
		DDX_Control(pDX, IDC_BAR_COMBOGEN, m_gencombo);
		DDX_Control(pDX, IDC_AUXSELECT, m_auxcombo);
		DDX_Control(pDX, IDC_BAR_COMBOINS, m_inscombo);
	}

	//Message Maps are defined in CMainFrame, since this isn't a window, but a DialogBar.
/*
	BEGIN_MESSAGE_MAP(MachineBar, CDialogBar)
		ON_MESSAGE(WM_INITDIALOG, OnInitDialog )
	END_MESSAGE_MAP()
*/

	void MachineBar::InitializeValues(CMainFrame* frame, CChildView* view, Song& song)
	{
		m_pParentMain = frame;
		m_pWndView = view;
		m_pSong = &song;
	}


	// MachineBar message handlers
	LRESULT MachineBar::OnInitDialog ( WPARAM wParam, LPARAM lParam)
	{
		BOOL bRet = HandleInitDialog(wParam, lParam);

		if (!UpdateData(FALSE))
		{
		   TRACE0("Warning: UpdateData failed during dialog init.\n");
		}

		macComboInitialized = false;

		((CButton*)GetDlgItem(IDC_B_DECGEN))->SetIcon(PsycleGlobal::conf().iconless);
		((CButton*)GetDlgItem(IDC_B_INCGEN))->SetIcon(PsycleGlobal::conf().iconmore);
		((CButton*)GetDlgItem(IDC_B_DECWAV))->SetIcon(PsycleGlobal::conf().iconless);
		((CButton*)GetDlgItem(IDC_B_INCWAV))->SetIcon(PsycleGlobal::conf().iconmore);

		m_stepcombo.SetCurSel(m_pWndView->patStep);

		m_auxcombo.SetCurSel(AUX_PARAMS);

		return bRet;
	}


	void MachineBar::OnSelchangeCombostep()
	{
		int sel=m_stepcombo.GetCurSel();
		m_pWndView->patStep=sel;
		m_pWndView->SetFocus();
	}

	void MachineBar::OnCloseupCombostep()
	{
		m_pWndView->SetFocus();
	}

	void MachineBar::EditQuantizeChange(int diff) // User Called (Hotkey)
	{
		const int total = m_stepcombo.GetCount();
		const int nextsel = (total + m_stepcombo.GetCurSel() + diff) % total;
		m_stepcombo.SetCurSel(nextsel);
		m_pWndView->patStep=nextsel;
	}

	void MachineBar::OnBDecgen() // called by Button and Hotkey.
	{
		const int val = m_gencombo.GetCurSel();
		if ( val > 0 ) {
			m_gencombo.SetCurSel(m_gencombo.GetCurSel()-1);
			while( m_gencombo.GetCurSel() > 0 && m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535)
			{
				m_gencombo.SetCurSel(m_gencombo.GetCurSel()-1);
			}
		}
		if ( val == 0 || m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535) {
			m_gencombo.SetCurSel(m_gencombo.GetCount()-1);
			while( m_gencombo.GetCurSel() > 0 && m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535)
			{
				m_gencombo.SetCurSel(m_gencombo.GetCurSel()-1);
			}
		}
		if ( m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535 )
		{
			m_gencombo.SetCurSel(val);
		}
		OnSelchangeBarCombogen();
		((CButton*)GetDlgItem(IDC_B_DECGEN))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	void MachineBar::OnBIncgen() // called by Button and Hotkey.
	{
		const int val = m_gencombo.GetCurSel();
		if ( val < m_gencombo.GetCount()-1 ) {
			m_gencombo.SetCurSel(m_gencombo.GetCurSel()+1);
			while( m_gencombo.GetCurSel() < m_gencombo.GetCount()-1 && m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535)
			{
				m_gencombo.SetCurSel(m_gencombo.GetCurSel()+1);
			}
		}
		if (val == m_gencombo.GetCount()-1 || m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535) {
			m_gencombo.SetCurSel(0);
			while( m_gencombo.GetCurSel() < m_gencombo.GetCount()-1 && m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535)
			{
				m_gencombo.SetCurSel(m_gencombo.GetCurSel()+1);
			}
		}
		if ( m_gencombo.GetItemData(m_gencombo.GetCurSel()) == 65535 )
		{
			m_gencombo.SetCurSel(val);
		}
		OnSelchangeBarCombogen();
		((CButton*)GetDlgItem(IDC_B_INCGEN))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	void MachineBar::UpdateComboGen(bool updatelist)
	{
		bool filled=false;
		bool found=false;
		int selected = -1;
		int line = -1;
		char buffer[64];
		
		if (m_pSong == NULL) 
		{
			return; // why should this happen?
		}
		
		macComboInitialized = false;
		if (updatelist) 
		{
			m_gencombo.ResetContent();
		}
		
		for (int b=0; b<MAX_BUSES; b++) // Check Generators
		{
			if( m_pSong->_pMachine[b])
			{
				if (updatelist)
				{	
					sprintf(buffer,"%.2X: %s",b,m_pSong->_pMachine[b]->_editName);
					m_gencombo.AddString(buffer);
					m_gencombo.SetItemData(m_gencombo.GetCount()-1,b);
				}
				if (!found) 
				{
					selected++;
				}
				if (m_pSong->seqBus == b) 
				{
					found = true;
				}
				filled = true;
			}
		}
		if ( updatelist) 
		{
			m_gencombo.AddString("----------------------------------------------------");
			m_gencombo.SetItemData(m_gencombo.GetCount()-1,65535);
		}
		if (!found) 
		{
			selected++;
			line = selected;
		}
		
		for (int b=MAX_BUSES; b<MAX_BUSES*2; b++) // Write Effects Names.
		{
			if(m_pSong->_pMachine[b])
			{
				if (updatelist)
				{	
					sprintf(buffer,"%.2X: %s",b,m_pSong->_pMachine[b]->_editName);
					m_gencombo.AddString(buffer);
					m_gencombo.SetItemData(m_gencombo.GetCount()-1,b);
				}
				if (!found) 
				{
					selected++;
				}
				if (m_pSong->seqBus == b) 
				{
					found = true;
				}
				filled = true;
			}
		}
		if ( updatelist) 
		{
			m_gencombo.AddString("----------------------------------------------------");
			m_gencombo.SetItemData(m_gencombo.GetCount()-1,65535);
		}
		if (!found) 
		{
			selected++;
			line = selected;
		}

		for (int b=MAX_MACHINES; b<MAX_VIRTUALINSTS; b++) // Write Virtual Instruments names.
		{
			Song::macinstpair inspair = m_pSong->VirtualInstrument(b);
			if(inspair.first != -1)
			{
				int mac = inspair.first;
				int inst = inspair.second;
				if (updatelist)
				{
					if (m_pSong->_pMachine[mac]) {
						sprintf(buffer,"%.2X: %s", b, m_pSong->GetVirtualMachineName(m_pSong->_pMachine[mac],inst).c_str());
					}
					else {
						sprintf(buffer,"%.2X: ", b);
					}
					m_gencombo.AddString(buffer);
					m_gencombo.SetItemData(m_gencombo.GetCount()-1,b);
				}
				if (!found) 
				{
					selected++;
				}
				if (m_pSong->seqBus == b) 
				{
					found = true;
				}
				filled = true;
			}
		}
		
		if (!filled)
		{
			m_gencombo.ResetContent();
			m_gencombo.AddString("No Machines Loaded");
			m_gencombo.SetItemData(m_gencombo.GetCount()-1,65535);
			selected = 0;
		}
		else if (!found) 
		{
			selected=line;
		}
		
		m_gencombo.SetCurSel(selected);

		// Select the appropiate Option in Aux Combobox.
		if (found) // If found (which also means, if it exists)
		{
			if ( m_pSong->seqBus < MAX_BUSES && m_pSong->_pMachine[m_pSong->seqBus]->NeedsAuxColumn())
			{
				// Generator that uses aux column.
				m_auxcombo.SetCurSel(AUX_INSTRUMENT);
				if (m_pSong->_pMachine[m_pSong->seqBus]->_type == MACH_XMSAMPLER) {
					m_pSong->auxcolSelected = m_pSong->instSelected;
				}
				else if (m_pSong->_pMachine[m_pSong->seqBus]->_type == MACH_SAMPLER) {
					m_pSong->auxcolSelected = m_pSong->waveSelected;
				}
				else {
					m_pSong->auxcolSelected = m_pSong->_pMachine[m_pSong->seqBus]->AuxColumnIndex();
				}
			}
			else if (m_pSong->seqBus >= MAX_MACHINES) {
				//Virtual generator
				m_auxcombo.SetCurSel(AUX_PARAMS);
			}
			else 
			{	//the rest.
				m_auxcombo.SetCurSel(AUX_PARAMS);
				if (m_pSong->_pMachine[m_pSong->seqBus]->translate_param(m_pSong->paramSelected) < m_pSong->_pMachine[m_pSong->seqBus]->GetNumParams())
				{
					m_pSong->auxcolSelected = m_pSong->paramSelected;
				}
				else {
					m_pSong->auxcolSelected = m_pSong->_pMachine[m_pSong->seqBus]->GetNumParams();
				}
			}
		}
		else
		{
			m_auxcombo.SetCurSel(AUX_INSTRUMENT); // WAVES
			m_pSong->auxcolSelected = m_pSong->waveSelected;
		}
		UpdateComboIns();
		macComboInitialized = true;
	}

	void MachineBar::OnSelchangeBarCombogen() 
	{
		if(macComboInitialized)
		{
			int nsb = GetNumFromCombo(&m_gencombo);

			if(m_pSong->seqBus!=nsb)
			{
				m_pSong->seqBus=nsb;
				UpdateComboGen(false);
			}
			
			m_pParentMain->RedrawGearRackList();
			//Added by J.Redfern, repaints main view after changing selection in combo
			m_pWndView->Repaint();

		}
	}

	void MachineBar::OnCloseupBarCombogen()
	{
		m_pWndView->SetFocus();
	}


	void MachineBar::ChangeGen(int i)	// Used to set an specific seqBus (used in "CChildView::SelectMachineUnderCursor")
	{
		if(i>=0 && i <MAX_VIRTUALINSTS && i != MASTER_INDEX)
		{
			m_pSong->seqBus=i;
			UpdateComboGen(false);
		}
	}

	void MachineBar::OnGearRack() 
	{
		if (m_pParentMain->pGearRackDialog == NULL)
		{
			m_pParentMain->pGearRackDialog = new CGearRackDlg(m_pParentMain,m_pWndView, &m_pParentMain->pGearRackDialog);
			m_pParentMain->pGearRackDialog->ShowWindow(SW_SHOW);
		}
		else {

			m_pParentMain->pGearRackDialog->SetActiveWindow();
		}
		((CButton*)GetDlgItem(IDC_GEAR_RACK))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
	}

	void MachineBar::OnCloseupAuxselect() 
	{
		m_pWndView->SetFocus();
	}

	void MachineBar::OnSelchangeAuxselect() 
	{
		if ( m_auxcombo.GetCurSel() == AUX_INSTRUMENT )	// WAVES
		{
			if ( m_pSong->seqBus < MAX_BUSES && m_pSong->_pMachine[m_pSong->seqBus] != NULL
				&& m_pSong->_pMachine[m_pSong->seqBus]->_type == MACH_XMSAMPLER) {
					m_pSong->auxcolSelected = m_pSong->instSelected;
			}
			else {
				m_pSong->auxcolSelected = m_pSong->waveSelected;
			}
		}
		UpdateComboIns();
	}
	void MachineBar::OnBDecAux() 
	{
		const int val = m_inscombo.GetCurSel();
		if ( val > 0 ) {
			m_inscombo.SetCurSel(val-1);
			int nsb = GetNumFromCombo(&m_inscombo);
			ChangeAux(nsb);
		}
		((CButton*)GetDlgItem(IDC_B_DECWAV))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	void MachineBar::OnBIncAux() 
	{
		bool changed= false;
		int val = m_inscombo.GetCurSel();
		if ( val < m_inscombo.GetCount() ) {
			val = m_inscombo.GetCurSel()+1;
			changed=true;
		}
		else if (m_pSong->seqBus < MAX_MACHINES) {
			//Automatically increase the samples/instruments vector when using increase button. Good for loading samples from toolbar.
			int target = m_pSong->auxcolSelected+1;
			Machine *tmac = m_pSong->_pMachine[m_pSong->seqBus];
			if (tmac) {
				if (tmac->_type == MACH_XMSAMPLER) {
					if (Global::song().xminstruments.size() <= target && target < MAX_INSTRUMENTS) {
						XMInstrument inst;
						inst.Init();
						Global::song().xminstruments.SetInst(inst,target);
						UpdateComboIns(true);
						val = m_inscombo.GetCurSel()+1;
						changed=true;
					}
				}
				else if (tmac->_type == MACH_SAMPLER) {
					if (Global::song().samples.size() <= target && target < MAX_INSTRUMENTS) {
						XMInstrument::WaveData<> wave;
						wave.Init();
						Global::song().samples.SetSample(wave,target);
						UpdateComboIns(true);
						val = m_inscombo.GetCurSel()+1;
						changed=true;
					}
				}
			}
		}
		if (changed) {
			m_inscombo.SetCurSel(val);
			int nsb = GetNumFromCombo(&m_inscombo);
			ChangeAux(nsb);
		}
		((CButton*)GetDlgItem(IDC_B_INCWAV))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	void MachineBar::UpdateComboIns(bool updatelist)
	{
		bool found=false;
		int selected = -1;

		macComboInitialized = false;

		if (updatelist) 
		{
			m_inscombo.ResetContent();
		}

		if ( m_pSong->seqBus >= MAX_MACHINES ) { // virtual generators
			if (updatelist) 
			{
				m_inscombo.AddString("No parameters");
				m_inscombo.SetItemData(m_inscombo.GetCount()-1,65535);
			}
		}
		else if ( m_auxcombo.GetCurSel() == AUX_PARAMS)	// Params
		{
			int nmac = m_pSong->seqBus;
			Machine *tmac = m_pSong->_pMachine[nmac];
			if (tmac) 
			{
				int i=0;
				for (i=0;i<256;i++)
				{
					int param = tmac->translate_param(i);
					if ( param < tmac->GetNumParams()) {
						if (updatelist) 
						{
							char buffer[64],buffer2[64];
							std::memset(buffer2,0,64);
							tmac->GetParamName(param,buffer2);
							bool label(false);
							if(tmac->_type == MACH_PLUGIN)
							{
								if(!(static_cast<Plugin*>(tmac)->GetInfo()->Parameters[param]->Flags & psycle::plugin_interface::MPF_STATE))
									label = true;
							}
							if(label) {
								// just a label
								sprintf(buffer, "------ %s ------", buffer2);
							}
							else if (i != param) {
								sprintf(buffer, "%.2X:(%.2X)  %s", i, param, buffer2);
							}
							else {
								sprintf(buffer, "%.2X:  %s", i, buffer2);
							}
							m_inscombo.AddString(buffer);
							m_inscombo.SetItemData(m_inscombo.GetCount()-1,i);
						}
						if (!found) 
						{
							selected++;
						}
						if (m_pSong->auxcolSelected == i) 
						{
							found = true;
						}
					}
				}
			}
			else
			{
				if (updatelist) 
				{
					m_inscombo.AddString("No Machine");
					m_inscombo.SetItemData(m_inscombo.GetCount()-1,65535);
				}
			}
		}
		else
		{
			char buffer[64];
			Machine *tmac = m_pSong->_pMachine[m_pSong->seqBus];
			if (tmac && tmac->NeedsAuxColumn()) 
			{
				int listlen= tmac->NumAuxColumnIndexes();
				if (updatelist) 
				{
					for (int i(0); i<listlen; i++)
					{
						sprintf(buffer, "%.2X: %s", i, tmac->AuxColumnName(i));
						m_inscombo.AddString(buffer);
						m_inscombo.SetItemData(m_inscombo.GetCount()-1,i);
					}
				}
				if (m_pSong->auxcolSelected < listlen) {
					selected=m_pSong->auxcolSelected;
					found=true;
				}
			}
			else
			{
				m_inscombo.AddString("No Machine");
				m_inscombo.SetItemData(m_inscombo.GetCount()-1,65535);
			}
		}
		if (!found) {
			m_pSong->auxcolSelected = 0;
			selected=0;
		}

		m_inscombo.SetCurSel(selected);
		macComboInitialized = true;
	}

	void MachineBar::OnSelchangeBarComboins() 
	{
		if(macComboInitialized)
		{
			int nsb = GetNumFromCombo(&m_inscombo);
			if (m_pSong->auxcolSelected != nsb) {
				ChangeAux(nsb);
			}
		}
	}

	void MachineBar::OnCloseupBarComboins()
	{
		m_pWndView->SetFocus();
	}
	void MachineBar::ChangeWave(int i)
	{
		if ( m_pSong->waveSelected == i) return;
		if (i<0 || i >= MAX_INSTRUMENTS) return;
		m_pSong->waveSelected=i;
		m_pParentMain->UpdateInstrumentEditor();
		m_pParentMain->WaveEditorBackUpdate();
		m_pParentMain->RedrawGearRackList();
		if ( m_auxcombo.GetCurSel() == AUX_INSTRUMENT ) {
			int dummy = -1;
			Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, dummy);
			if (dummy == -1 && (tmac == NULL || tmac->_type == MACH_SAMPLER)) {
				m_pSong->auxcolSelected = i;
				m_inscombo.SetCurSel(m_pSong->auxcolSelected);
			}
		}
	}
	void MachineBar::ChangeIns(int i)
	{
		if ( m_pSong->instSelected == i) return;
		if (i<0 || i >= MAX_INSTRUMENTS) return;
		m_pSong->instSelected=i;
		m_pParentMain->UpdateInstrumentEditor();
		m_pParentMain->RedrawGearRackList();
		if ( m_auxcombo.GetCurSel() == AUX_INSTRUMENT ) {
			int dummy = -1;
			Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, dummy);
			if (dummy == -1 && tmac != NULL && tmac->_type == MACH_XMSAMPLER) {
				m_pSong->auxcolSelected = i;
				m_inscombo.SetCurSel(m_pSong->auxcolSelected);
			}
		}
	}
	void MachineBar::ChangeAux(int i)	// User Called (Hotkey, button or list change)
	{
		if (i<0) return;
		if (m_auxcombo.GetCurSel() == AUX_PARAMS) {
			int dummy=-1;
			Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, dummy);
			if (tmac && tmac->translate_param(i) >= tmac->GetNumParams()) return;
		}
		else if (i >= m_inscombo.GetCount()) return;

		if ( m_auxcombo.GetCurSel() == AUX_PARAMS ) {
			m_pSong->paramSelected=i;
			m_pSong->auxcolSelected=i;
			m_inscombo.SetCurSel(GetNumFromComboInv(&m_inscombo));
		} else {
			int dummy=-1;
			Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, dummy);
			if (dummy != -1) {
				//virtual generators should have AUX_PARAMS selected, so this would be an exceptional case
			}
			else if (tmac) {
				if (tmac->_type == MACH_XMSAMPLER) {
					m_pSong->instSelected=i;
				}
				else if (tmac->_type == MACH_SAMPLER) {
					m_pSong->waveSelected=i;
				}
				else {
					tmac->AuxColumnIndex(i);
				}
			}
			else { m_pSong->waveSelected=i; }
			m_pParentMain->UpdateInstrumentEditor();
			m_pParentMain->WaveEditorBackUpdate();
			m_pParentMain->RedrawGearRackList();
			m_pSong->auxcolSelected=i;
			m_inscombo.SetCurSel(m_pSong->auxcolSelected);
		}
	}

	void MachineBar::OnLoadwave() 
	{
		bool update=false;
		int aux=-1;
		Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, aux);
		bool found=false;
		if (!tmac || (tmac->_type != MACH_SAMPLER && tmac->_type != MACH_XMSAMPLER)) {
			for(int i=0;i<MAX_MACHINES;i++) {
				if (m_pSong->_pMachine[i] && (m_pSong->_pMachine[i]->_type == MACH_SAMPLER ||
						m_pSong->_pMachine[i]->_type == MACH_XMSAMPLER)	) {
					m_pSong->seqBus = i;
					tmac = m_pSong->_pMachine[m_pSong->seqBus];
					m_pParentMain->UpdateComboGen();
					m_pWndView->Repaint();
					found=true;
					break;
				}
			}
		}
		else {
			found = true;
		}
		if(!found) {
			int i = m_pSong->GetFreeMachine();
			if (i != -1) {
				m_pSong->CreateMachine(MACH_SAMPLER,16,16,NULL, i);
				m_pSong->seqBus = i;
				m_pParentMain->UpdateComboGen();
				m_pWndView->Repaint();
				tmac = m_pSong->_pMachine[m_pSong->seqBus];
			}
		}
		
		if (tmac && tmac->_type == MACH_XMSAMPLER) {
			int si = m_pSong->instSelected;
			if (m_pSong->xminstruments.IsEnabled(si)) {
				if (MessageBox("An instrument already exists in this slot. If you continue, it will be ovewritten INCLUDING the samples it has associated. Continue?"
				,"Sample Loading",MB_YESNO|MB_ICONWARNING) == IDNO)  return;
			}
			update=LoadInstrument(si, false);
		}
		else {
			int si = m_pSong->waveSelected;
			if (m_pSong->samples.IsEnabled(si)) {
				if (MessageBox("A sample already exists in this slot. If you continue, it will be ovewritten. Continue?"
					,"Sample Loading",MB_YESNO|MB_ICONWARNING) == IDNO)  return;
			}
			update=LoadInstrument(si, true);
		}
		if (update){
			UpdateComboIns();
			m_pParentMain->m_wndStatusBar.SetWindowText("New wave loaded");
			m_pParentMain->WaveEditorBackUpdate();
			m_pParentMain->UpdateInstrumentEditor(true);
			m_pParentMain->RedrawGearRackList();
		}
		((CButton*)GetDlgItem(IDC_LOADWAVE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
	}

	void MachineBar::OnSavewave()
	{
		int aux=-1;
		Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, aux);
		if (tmac && tmac->_type == MACH_XMSAMPLER) {
			if (m_pSong->xminstruments.IsEnabled(m_pSong->instSelected)) {
				SaveInstrument(m_pSong->instSelected);
			}
			else MessageBox("Nothing to save...\nSelect nonempty instrument first.", "Error", MB_ICONERROR);
		}
		else 
		{
			if (m_pSong->samples.IsEnabled(m_pSong->waveSelected)) {
				SaveWave(m_pSong->waveSelected);
			}
			else MessageBox("Nothing to save...\nSelect nonempty wave first.", "Error", MB_ICONERROR);
		}
	}

	void MachineBar::OnEditwave() 
	{
		bool found=false;
		for(int i=0;i<MAX_MACHINES;i++) {
			if (m_pSong->_pMachine[i] && (m_pSong->_pMachine[i]->_type == MACH_SAMPLER ||
					m_pSong->_pMachine[i]->_type == MACH_XMSAMPLER)	) {
				found=true;
				break;
			}
		}
		if(!found) {
			MessageBox(_T("Warning: To use samples, it is required to have a Sampler or a Sampulse Internal machine"),
				_T("Instrument editor"),MB_ICONWARNING);
		}
		int aux=-1;
		Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, aux);
		if (aux != -1 && tmac->_type == MACH_XMSAMPLER) {
			m_pSong->instSelected = aux;
		}
		else if (aux != -1 && tmac->_type == MACH_SAMPLER) {
			m_pSong->waveSelected = aux;
		}
		m_pParentMain->ShowInstrumentEditor();
		((CButton*)GetDlgItem(IDC_EDITWAVE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
	}

	void MachineBar::OnWavebut() 
	{
		int aux=-1;
		Machine *tmac = m_pSong->GetMachineOfBus(m_pSong->seqBus, aux);
		if (aux != -1 && tmac->_type == MACH_XMSAMPLER) {
			m_pSong->instSelected = aux;
		}
		else if (aux != -1 && tmac->_type == MACH_SAMPLER) {
			m_pSong->waveSelected = aux;
		}
		m_pParentMain->m_pWndWed->ShowWindow(SW_SHOWNORMAL);
		m_pParentMain->m_pWndWed->SetActiveWindow();
		((CButton*)GetDlgItem(IDC_WAVEBUT))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
	}

	bool MachineBar::LoadWave(int waveIdx)
	{
		char extrapath[_MAX_PATH*16];
		extrapath[0]='\0';
		static const char szFilter[] = "All known samples|*.wav;*.aif;*.aiff;*.s3i;*.its;*.8svx;*.16sv;*.svx;*.iff|Wav (PCM) Files (*.wav)|*.wav|Apple AIFF (PCM) Files (*.aif)|*.aif;*.aiff|ST3 Samples (*.s3i)|*.s3i|IT2 Samples (*.its)|*.its|Amiga IFF/SVX Samples (*.svx)|*.8svx;*.16sv;*.svx;*.iff|All files|*.*||";
		static uint32_t szDetect[] = {helpers::FormatDetector::WAVE_ID,helpers::FormatDetector::AIFF_ID,helpers::FormatDetector::SCRS_ID,helpers::FormatDetector::IMPS_ID,helpers::FormatDetector::SVX8_ID};
		CWavFileDlg dlg(true,"wav", NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST| OFN_DONTADDTORECENT | OFN_ALLOWMULTISELECT, szFilter);
		dlg.m_pSong = m_pSong;
		std::string tmpstr = PsycleGlobal::conf().GetCurrentInstrumentDir();
		dlg.m_ofn.lpstrInitialDir = tmpstr.c_str();
		dlg.m_ofn.lCustData = reinterpret_cast<LPARAM>(szDetect);
		dlg.m_ofn.nFilterIndex = wavFilterSelected;
		dlg.m_ofn.lpstrFile = extrapath;
		dlg.m_ofn.nMaxFile = _MAX_PATH*16;
		bool update=false;
		if (dlg.DoModal() == IDOK)
		{
			wavFilterSelected = dlg.m_ofn.nFilterIndex;
			PsycleGlobal::inputHandler().AddMacViewUndo();

			CExclusiveLock lock(&m_pSong->semaphore, 2, true);
			uint32_t selCode;
			
			int numFiles=0;
			POSITION pos =dlg.GetStartPosition();
			while( pos )
			{
				CString csFileName = dlg.GetNextPathName(pos);

				if (dlg.GetOFN().nFilterIndex <= 1 || dlg.GetOFN().nFilterIndex >= (sizeof(szDetect)/4)) {
					helpers::FormatDetector detect;
					selCode = detect.AutoDetect(csFileName.GetString());
				}
				else selCode = szDetect[dlg.GetOFN().nFilterIndex-2];

				try {
					if (pos > 0) { waveIdx = -1;}
					LoaderHelper loadhelp(m_pSong,false,-1,waveIdx);
					if (selCode == helpers::FormatDetector::WAVE_ID) {
						int realsample = m_pSong->WavAlloc(loadhelp,csFileName.GetString());
						if (realsample > -1) {
							update=true;
						} else {
							MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
						}
					}
					else if (selCode == helpers::FormatDetector::SCRS_ID) {
						ITModule2 itsong;
						itsong.Open(csFileName.GetString());
						update = itsong.LoadS3MFileS3I(loadhelp);
						itsong.Close();
					}
					else if (selCode == helpers::FormatDetector::IMPS_ID) {
						ITModule2 itsong;
						itsong.Open(csFileName.GetString());
						XMInstrument::WaveData<> wave;
						update = itsong.LoadITSample(wave);
						if (update) {
							 if (waveIdx > 0) {
								m_pSong->samples.SetSample(wave,waveIdx);
							}
							else {
								m_pSong->samples.AddSample(wave);
							}
						}
						itsong.Close();
					}
					else if (selCode == helpers::FormatDetector::AIFF_ID) {
						int realsample = m_pSong->AIffAlloc(loadhelp, csFileName.GetString());
						if (realsample > -1) {
							update=true;
						} else {
							MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
						}
					}
					else if (selCode == helpers::FormatDetector::SVX8_ID) {
						int realsample = m_pSong->IffAlloc(loadhelp,false, csFileName.GetString());
						if (realsample > -1) {
							update=true;
						} else {
							MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
						}
					}
				}
				catch(const std::runtime_error & e) {
					std::ostringstream os;
					os <<"Could not finish the operation: " << e.what();
					MessageBox(os.str().c_str(),"Load Error",MB_ICONERROR);
				}
				numFiles++;
			}
			if (numFiles == 1) {
				CString str = dlg.m_ofn.lpstrFile;
				int index = str.ReverseFind('\\');
				if (index != -1)
				{
					PsycleGlobal::conf().SetCurrentInstrumentDir(static_cast<char const *>(str.Left(index)));
				}
			}
			else {
				PsycleGlobal::conf().SetCurrentInstrumentDir(static_cast<char const *>(dlg.m_ofn.lpstrFile));
			}
		}
		{
			CExclusiveLock lock(&m_pSong->semaphore, 2, true);
			// Stopping wavepreview if not stopped.
			m_pSong->wavprev.Stop();
		}
		return update;
	}

	void MachineBar::SaveWave(int waveIdx)
	{
		helpers::RiffWave output2;

		static const char szFilter[] = "Wav Files (*.wav)|*.wav|All Files (*.*)|*.*||";

		const XMInstrument::WaveData<> & wave = m_pSong->samples[waveIdx];
		std::string nametemp = wave.WaveName();
		pathname_validate::validate(nametemp);
		CFileDialog dlg(FALSE, "wav", nametemp.c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_DONTADDTORECENT, szFilter);
		std::string tmpstr = PsycleGlobal::conf().GetCurrentInstrumentDir();
		dlg.m_ofn.lpstrInitialDir = tmpstr.c_str();
		if (dlg.DoModal() == IDOK)
		{
			output2.Create(dlg.GetPathName().GetString(),true);
			output2.addFormat(wave.WaveSampleRate(), 16, (wave.IsWaveStereo()) ? 2 : 1, false );
			if (wave.IsWaveStereo()) {
				int16_t* samps[] = {wave.pWaveDataL(), wave.pWaveDataR()};
				output2.writeFromDeInterleavedSamples(reinterpret_cast<void**>(samps), wave.WaveLength());
			} else {
				output2.writeFromInterleavedSamples(wave.pWaveDataL(), wave.WaveLength());
			}
			output2.UpdateCurrentChunkSize();
			RiffWaveSmplChunk smpl;
			smpl.manufacturer = smpl.product = smpl.smpteFormat = smpl.smpteOffset = smpl.extraDataSize = 0;
			smpl.samplePeriod = 1000000 / wave.WaveSampleRate();
			smpl.midiNote =  60 - wave.WaveTune();
			smpl.midiPitchFr = static_cast<int>(wave.WaveFineTune() * 2.56) << 24;
			smpl.numLoops = 0;
			if (wave.WaveLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) smpl.numLoops++;
			if (wave.WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) smpl.numLoops++;
			int idx = 0;
			smpl.loops = new RiffWaveSmplLoopChunk[smpl.numLoops];
			if (wave.WaveLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
				smpl.loops[idx].cueId = 0;
				smpl.loops[idx].type = (wave.WaveLoopType() == XMInstrument::WaveData<>::LoopType::NORMAL) ? 0 : 1;
				smpl.loops[idx].playCount = 0;
				smpl.loops[idx].start = wave.WaveLoopStart();
				smpl.loops[idx].end = wave.WaveLoopEnd();
				smpl.loops[idx].fraction = 0;
				idx++;
			}
			if (wave.WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
				smpl.loops[idx].cueId = 1;
				smpl.loops[idx].type = (wave.WaveSusLoopType() == XMInstrument::WaveData<>::LoopType::NORMAL) ? 0 : 1;
				smpl.loops[idx].playCount = 1;
				smpl.loops[idx].start = wave.WaveSusLoopStart();
				smpl.loops[idx].end = wave.WaveSusLoopEnd();
				smpl.loops[idx].fraction = 0;
			}
			output2.addSmplChunk(smpl);

			RiffWaveInstChunk inst;
			inst.midiNote = 60 - wave.WaveTune();
			inst.midiCents = wave.WaveFineTune();
			inst.gaindB = static_cast<int>(helpers::dsp::dB(wave.WaveGlobVolume())); // wavevolume too?
			inst.lowNote = inst.lowVelocity = 1;
			inst.highNote = inst.highVelocity = 127;
			output2.addInstChunk(inst);
			output2.Close();
		}
	}

	bool MachineBar::LoadInstrument(int instIdxInput, bool indexIsSample)
	{
		static const char szFilter[] = "All Instruments and samples|*.psins;*.xi;*.iti;*.wav;*.aif;*.aiff;*.its;*.s3i;*.8svx;*.16sv;*.svx;*.iff|Psycle Instrument (*.psins)|*.psins|XM Instruments (*.xi)|*.xi|IT Instruments (*.iti)|*.iti|Wav (PCM) Files (*.wav)|*.wav|Apple AIFF (PCM) Files (*.aif)|*.aif;*.aiff|ST3 Samples (*.s3i)|*.s3i|IT2 Samples (*.its)|*.its|Amiga IFF/SVX Samples (*.svx)|*.8svx;*.16sv;*.svx;*.iff|All files (*.*)|*.*||";
		static uint32_t szDetect[] = {helpers::FormatDetector::PSYI_ID,helpers::FormatDetector::XI_ID,helpers::FormatDetector::IMPI_ID,helpers::FormatDetector::WAVE_ID,helpers::FormatDetector::AIFF_ID,helpers::FormatDetector::SCRS_ID,helpers::FormatDetector::IMPS_ID,helpers::FormatDetector::SVX8_ID};
		CWavFileDlg dlg(true,"psins", NULL, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST| OFN_DONTADDTORECENT, szFilter);
		dlg.m_pSong = m_pSong;
		std::string tmpstr = PsycleGlobal::conf().GetCurrentInstrumentDir();
		dlg.m_ofn.lpstrInitialDir = tmpstr.c_str();
		dlg.m_ofn.lCustData = reinterpret_cast<LPARAM>(szDetect);
		dlg.m_ofn.nFilterIndex = insFilterSelected;
		bool update=false;
		if (dlg.DoModal() == IDOK)
		{
			insFilterSelected = dlg.m_ofn.nFilterIndex;
			PsycleGlobal::inputHandler().AddMacViewUndo();

			CExclusiveLock lock(&m_pSong->semaphore, 2, true);
			uint32_t selCode;

			if (dlg.GetOFN().nFilterIndex <= 1 || dlg.GetOFN().nFilterIndex >= (sizeof(szDetect)/4)) {
				helpers::FormatDetector detect;
				selCode = detect.AutoDetect(dlg.GetPathName().GetString());
			}
			else selCode = szDetect[dlg.GetOFN().nFilterIndex-2];

			try {
				int smplIdx = (indexIsSample) ? instIdxInput : -1;
				int instIdx = (indexIsSample) ? -1 : instIdxInput;
				LoaderHelper loadhelp(m_pSong,false,instIdx,smplIdx);

				if (instIdx > -1 && m_pSong->xminstruments.IsEnabled(instIdx)) {
					XMInstrument & inst = m_pSong->xminstruments.get(instIdx);
					std::set<int> sampNums =  inst.GetWavesUsed();
					for (std::set<int>::iterator it = sampNums.begin(); it != sampNums.end();++it) {
						m_pSong->samples.RemoveAt(*it);
					}
					inst.Init();
					Global::song().DeleteVirtualOfInstrument(instIdx,true);
				}

				if (selCode == helpers::FormatDetector::PSYI_ID) {
					update = m_pSong->LoadPsyInstrument(loadhelp, dlg.GetPathName().GetString());
					if (!update) {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
				}
				else if (selCode == helpers::FormatDetector::XI_ID) {
					XMSongLoader xmsong;
					xmsong.Open(dlg.GetPathName().GetString());
					int realinst = xmsong.LoadInstrumentFromFile(loadhelp);
					xmsong.Close();
					if (realinst == -1) {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
					else {
						std::set<int> list = m_pSong->xminstruments[realinst].GetWavesUsed();
						for (std::set<int>::iterator ite =list.begin(); ite != list.end(); ++ite) {
							m_pSong->_pInstrument[*ite]->CopyXMInstrument(m_pSong->xminstruments[realinst],m_pSong->tickstomillis(1));
						}
					}
					update=true;
				}
				else if (selCode == helpers::FormatDetector::IMPI_ID) {
					ITModule2 itsong;
					itsong.Open(dlg.GetPathName().GetString());
					int realinst = itsong.LoadInstrumentFromFile(loadhelp);
					itsong.Close();
					if (realinst == -1) {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
					else {
						std::set<int> list = m_pSong->xminstruments[realinst].GetWavesUsed();
						for (std::set<int>::iterator ite =list.begin(); ite != list.end(); ++ite) {
							m_pSong->_pInstrument[*ite]->CopyXMInstrument(m_pSong->xminstruments[realinst],m_pSong->tickstomillis(1));
						}
					}
					update=true;
				}
				else if (selCode == helpers::FormatDetector::WAVE_ID) {
					int realsample = m_pSong->WavAlloc(loadhelp,dlg.GetPathName().GetString());
					if (realsample > -1) {
						const XMInstrument::WaveData<>& wave = m_pSong->samples[realsample];
						SetupDefaultInstrument(loadhelp, wave, realsample);
						update=true;
					} else {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
				}
				else if (selCode == helpers::FormatDetector::SCRS_ID) {
					ITModule2 itsong;
					itsong.Open(dlg.GetPathName().GetString());
					XMInstrument::WaveData<> wave;
					int realsample = itsong.LoadS3MFileS3I(loadhelp);
					itsong.Close();
					if (realsample > -1) {
						SetupDefaultInstrument(loadhelp, wave, realsample);
						update=true;
					} else {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
				}
				else if (selCode == helpers::FormatDetector::IMPS_ID) {
					ITModule2 itsong;
					itsong.Open(dlg.GetPathName().GetString());
					int realsample = -1; 
					XMInstrument::WaveData<>& wave = loadhelp.GetNextSample(realsample);
					update = itsong.LoadITSample(wave);
					itsong.Close();

					SetupDefaultInstrument(loadhelp, wave, realsample);
					update=true;
				}
				else if (selCode == helpers::FormatDetector::AIFF_ID) {
					int realsample = m_pSong->AIffAlloc(loadhelp, dlg.GetPathName().GetString());
					if (realsample > -1) {
						const XMInstrument::WaveData<>& wave = m_pSong->samples[realsample];
						SetupDefaultInstrument(loadhelp, wave, realsample);
						update=true;
					} else {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
				}
				else if (selCode == helpers::FormatDetector::SVX8_ID) {
					int realsample = m_pSong->IffAlloc(loadhelp,true, dlg.GetPathName().GetString());
					if (realsample > -1) {
						update=true;
					} else {
						MessageBox("Could not load the file, unrecognized format","Load Error",MB_ICONERROR);
					}
				}
			}
			catch(const std::runtime_error & e) {
				std::ostringstream os;
				os <<"Could not finish the operation: " << e.what();
				MessageBox(os.str().c_str(),"Load Error",MB_ICONERROR);
			}

			CString str = dlg.m_ofn.lpstrFile;
			int index = str.ReverseFind('\\');
			if (index != -1) {
				PsycleGlobal::conf().SetCurrentInstrumentDir(static_cast<char const *>(str.Left(index)));
			}
		}
		{
			CExclusiveLock lock(&m_pSong->semaphore, 2, true);
			// Stopping wavepreview if not stopped.
			m_pSong->wavprev.Stop();
		}
		return update;
	}
	void MachineBar::SaveInstrument(int instIdx)
	{
		static const char szFilter[] = "Psycle Instrument (*.psins)|*.psins||";//TODO: XM Instruments (*.xi)|*.xi|IT Instruments (*.iti)|*.iti||";

		const XMInstrument instr = m_pSong->xminstruments[instIdx];
		std::string nametemp = instr.Name();
		pathname_validate::validate(nametemp);
		CFileDialog dlg(FALSE, "psins", nametemp.c_str(), OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_DONTADDTORECENT, szFilter);
		std::string tmpstr = PsycleGlobal::conf().GetCurrentInstrumentDir();
		dlg.m_ofn.lpstrInitialDir = tmpstr.c_str();
		if (dlg.DoModal() == IDOK)
		{
			CString CurrExt=dlg.GetFileExt();
			CurrExt.MakeLower();
			if ( CurrExt == "psins" ) {
				m_pSong->SavePsyInstrument(dlg.GetPathName().GetString(),instIdx);
			}
			else if (CurrExt == "xi") {
			}
			else if (CurrExt == "iti") {
			}
		}
	}

	int MachineBar::SetupDefaultInstrument(LoaderHelper& loadhelp, const XMInstrument::WaveData<>& wave, int waveIdx)
	{
		int outinst=-1;
		XMInstrument instr = loadhelp.GetNextInstrument(outinst);
		instr.Name(wave.WaveName());
		instr.SetDefaultNoteMap(waveIdx);
		instr.ValidateEnabled();
		Instrument & smpins = *m_pSong->_pInstrument[waveIdx];
		smpins.CopyXMInstrument(instr,m_pSong->tickstomillis(1));
		return outinst;
	}

	int MachineBar::GetNumFromCombo(CComboBox *cb)
	{
		int result;
		result = cb->GetItemData(cb->GetCurSel());
		if (result == 65535) result = 0; //Since we have to assign a value, we assing zero.
		/* Old method that parsed the string. hexstring_to_integer took care of removing invalid characters (--- entries)
		CString str;
		cb->GetWindowText(str);
		std::stringstream ss;
		ss << str.Left(2);
		helpers::hexstring_to_integer(ss.str(), result);
		*/
		return result;
	}
	int MachineBar::GetNumFromComboInv(CComboBox *cb)
	{
		int i=0;
		for (;i< cb->GetCount(); i++) {
			if (cb->GetItemData(i) == m_pSong->auxcolSelected) break;
		}
		return i;
	}
}}