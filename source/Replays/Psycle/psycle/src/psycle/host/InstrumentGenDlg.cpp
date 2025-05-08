#include <psycle/host/detail/project.private.hpp>
#include "InstrumentGenDlg.hpp"
#include "InstrumentEditorUI.hpp"
#include "XMSamplerUIInst.hpp"
#include "MainFrm.hpp"
#include "InstrNoteMap.hpp"
#include "PsycleConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/XMInstrument.hpp>

namespace psycle { namespace host {

	extern CPsycleApp theApp;

CInstrumentGenDlg::CInstrumentGenDlg()
: CDialog(CInstrumentGenDlg::IDD)
, m_bInitialized(false)
, m_instIdx(0)
{
}

CInstrumentGenDlg::~CInstrumentGenDlg()
{
}

void CInstrumentGenDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_VIRTINST, m_virtual);
	DDX_Control(pDX, IDC_VIRTINSCOMBO, m_virtinstcombo);
	DDX_Control(pDX, IDC_VIRTMACCOMBO, m_virtmaccombo);

	DDX_Control(pDX, IDC_INS_NAME, m_InstrumentName);
	DDX_Control(pDX, IDC_INS_NNACOMBO, m_NNA);
	DDX_Control(pDX, IDC_INS_DCTCOMBO, m_DCT);
	DDX_Control(pDX, IDC_INS_DCACOMBO, m_DCA);

	DDX_Control(pDX, IDC_INS_NOTEMAP, m_SampleAssign);
	DDX_Control(pDX, IDC_INS_NOTESCROLL, m_scBar);
}

/*
BEGIN_MESSAGE_MAP(CInstrumentGenDlg, CDialog)
	ON_WM_VSCROLL()
	ON_EN_CHANGE(IDC_INS_NAME, OnEnChangeInsName)
	ON_BN_CLICKED(IDC_VIRTINST,OnCheckVirtual)
	ON_CBN_SELENDOK(IDC_VIRTINSCOMBO, OnCbnSelendokVirtInstcombo)
	ON_CBN_SELENDOK(IDC_VIRTMACCOMBO, OnCbnSelendokVirtMaccombo)
	ON_CBN_SELENDOK(IDC_INS_NNACOMBO, OnCbnSelendokInsNnacombo)
	ON_CBN_SELENDOK(IDC_INS_DCTCOMBO, OnCbnSelendokInsDctcombo)
	ON_CBN_SELENDOK(IDC_INS_DCACOMBO, OnCbnSelendokInsDcacombo)
	ON_BN_CLICKED(IDC_SETDEFAULT,OnBtnSetDefaults)
	ON_BN_CLICKED(IDC_EDITMAPPING,OnBtnEditMapping)
	ON_BN_CLICKED(IDC_SET_ALL_SAMPLE,OnBtnPopupSetSample)
	ON_COMMAND_RANGE(IDC_INSTR_SETSAMPLES_0, IDC_INSTR_SETSAMPLES_255, OnSetSample)
END_MESSAGE_MAP()
*/

BOOL CInstrumentGenDlg::PreTranslateMessage(MSG* pMsg)
{
	CWnd *tabCtl = GetParent();
	CWnd *UIInst = tabCtl->GetParent();
	Machine *tmac = Global::song().GetSampulseIfExists();
	InstrumentEditorUI* parent = dynamic_cast<InstrumentEditorUI*>(UIInst->GetParent());
	BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd(), tmac);
	if (res == FALSE ) return CDialog::PreTranslateMessage(pMsg);
	return res;
}

BOOL CInstrumentGenDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_InstrumentName.SetLimitText(31);

	for (int i=MAX_MACHINES;i<MAX_VIRTUALINSTS;i++) {
		std::ostringstream os;
		os << std::hex << std::uppercase << i;
		int idx = m_virtinstcombo.AddString(os.str().c_str());
		m_virtinstcombo.SetItemData(idx, i);
	}

	m_NNA.AddString(_T("Note Cut"));
	m_NNA.AddString(_T("Note Continue"));
	m_NNA.AddString(_T("Note Off"));
	m_NNA.AddString(_T("Note Fadeout"));

	m_DCT.AddString(_T("Disabled"));
	m_DCT.AddString(_T("Note"));
	m_DCT.AddString(_T("Sample"));
	m_DCT.AddString(_T("Instrument"));

	m_DCA.AddString(_T("Note Cut"));
	m_DCA.AddString(_T("Note Continue"));
	m_DCA.AddString(_T("Note Off"));
	m_DCA.AddString(_T("Note Fadeout"));

	SCROLLINFO info;
	m_scBar.GetScrollInfo(&info, SIF_PAGE|SIF_RANGE);
	info.fMask = SIF_RANGE|SIF_POS;
	info.nMin = 0;
	info.nMax  = 8;
	info.nPos = m_SampleAssign.Octave();
	m_scBar.SetScrollInfo(&info, false);

	m_EnvelopeEditorDlg.Create(CEnvelopeEditorDlg::IDD,this);
	CRect rect, rect2;
	((CStatic*)GetDlgItem(IDC_GROUP_ENV))->GetWindowRect(rect);
	this->GetWindowRect(rect2);
	rect.OffsetRect(-rect2.left,-rect2.top);
	m_EnvelopeEditorDlg.m_EnvelopeEditor.negative(true);
	m_EnvelopeEditorDlg.SetWindowPos(this,rect.left+12,rect.top+12,0,0,SWP_NOZORDER|SWP_NOREDRAW|SWP_NOSIZE);
	m_EnvelopeEditorDlg.ShowWindow(SW_SHOW);

	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
void CInstrumentGenDlg::AssignGeneralValues(XMInstrument& inst, int instno)
{
	m_bInitialized=false;
	m_instr = &inst;
	m_instIdx = instno;
	m_InstrumentName.SetWindowText(inst.Name().c_str());
	SetNewNoteAction(inst.NNA(),inst.DCT(),inst.DCA());
	m_SampleAssign.Initialize(inst);

	Song &song = Global::song();
	m_virtmaccombo.ResetContent();
	for (int i=0;i<MAX_BUSES;i++) {
		if (song._pMachine[i] != NULL && song._pMachine[i]->_type == MACH_XMSAMPLER) {
			std::ostringstream os;
			os << std::hex << std::uppercase << i << std::nouppercase << ": "<< song._pMachine[i]->_editName;
			int idx = m_virtmaccombo.AddString(os.str().c_str());
			m_virtmaccombo.SetItemData(idx, i);
		}
	}
	UpdateVirtInstOptions();

	m_EnvelopeEditorDlg.ChangeEnvelope(inst.PitchEnvelope());
	m_bInitialized=true;
}

void CInstrumentGenDlg::SetNewNoteAction(const int nna,const int dct,const int dca)
{
	m_NNA.SetCurSel(nna);
	m_DCT.SetCurSel(dct);
	m_DCA.EnableWindow(m_DCT.GetCurSel() != 0);
	m_DCA.SetCurSel(dca);
}

void CInstrumentGenDlg::OnEnChangeInsName()
{
	if(m_bInitialized)
	{
		TCHAR _buf[256];
		m_InstrumentName.GetWindowText(_buf,sizeof(_buf));
		m_instr->Name(_buf);
		m_instr->ValidateEnabled();
		XMSamplerUIInst* win = dynamic_cast<XMSamplerUIInst*>(GetParent()->GetParent());
		win->FillInstrumentList(-2);
		CMainFrame* winMain = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
		winMain->UpdateComboIns(true);
		winMain->UpdateComboGen(true); //for virtual instruments
	}
}

void CInstrumentGenDlg::OnCheckVirtual()
{
	bool virtselected = static_cast<bool>(m_virtual.GetCheck());
	m_virtinstcombo.EnableWindow(virtselected);
	m_virtmaccombo.EnableWindow(virtselected);

	if (!virtselected) {
		int virtualIndex = Global::song().VirtualInstrumentInverted(m_instIdx,true);
		m_virtinstcombo.SetCurSel(-1);
		m_virtmaccombo.SetCurSel(-1);
		Global::song().DeleteVirtualInstrument(virtualIndex);
	}
	((CMainFrame *)theApp.m_pMainWnd)->UpdateComboGen(true);
}
void CInstrumentGenDlg::OnCbnSelendokVirtInstcombo()
{
	bool virtselected = static_cast<bool>(m_virtual.GetCheck());
	DWORD_PTR instIdx = m_virtinstcombo.GetItemData(m_virtinstcombo.GetCurSel());
	if (virtselected && instIdx != CB_ERR) {
		Song::macinstpair pairinst = Global::song().VirtualInstrument(instIdx);
		if (pairinst.first != -1 ) {
			int result = MessageBox("Warning! This virtual instrument is already used. If you confirm, the previous association will be lost","Set virtual instrument",MB_YESNO | MB_ICONEXCLAMATION);
			if ( result == IDNO) {
				m_virtinstcombo.SetCurSel(-1);
				return;
			}
		}
		DWORD_PTR macIdx = m_virtmaccombo.GetItemData(m_virtmaccombo.GetCurSel());
		if ( macIdx != CB_ERR ) {
			Global::song().SetVirtualInstrument(instIdx,macIdx,m_instIdx);
		}
		((CMainFrame *)theApp.m_pMainWnd)->UpdateComboGen(true);
	}
}
void CInstrumentGenDlg::OnCbnSelendokVirtMaccombo()
{
	bool virtselected = static_cast<bool>(m_virtual.GetCheck());
	DWORD_PTR macIdx = m_virtmaccombo.GetItemData(m_virtmaccombo.GetCurSel());
	if (virtselected && macIdx != CB_ERR) {
		DWORD_PTR instIdx = m_virtinstcombo.GetItemData(m_virtinstcombo.GetCurSel());
		if (instIdx != CB_ERR ) {
			Global::song().SetVirtualInstrument(instIdx,macIdx,m_instIdx);
		}
		((CMainFrame *)theApp.m_pMainWnd)->UpdateComboGen(true);
	}
}


void CInstrumentGenDlg::OnCbnSelendokInsNnacombo()
{
	m_instr->NNA((XMInstrument::NewNoteAction::Type)m_NNA.GetCurSel());
}
	
void CInstrumentGenDlg::OnCbnSelendokInsDctcombo()
{
	m_instr->DCT((XMInstrument::DupeCheck::Type)m_DCT.GetCurSel());
	m_DCA.EnableWindow(m_DCT.GetCurSel() != 0);
}

void CInstrumentGenDlg::OnCbnSelendokInsDcacombo()
{
	m_instr->DCA((XMInstrument::NewNoteAction::Type)m_DCA.GetCurSel());
}

void CInstrumentGenDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	int m_Octave = 8-m_SampleAssign.Octave();
	switch(nSBCode)
	{
		case SB_TOP:
			m_Octave=0;
			break;
		case SB_BOTTOM:
			m_Octave=8;
			break;
		case SB_LINEDOWN:
		case SB_PAGEDOWN:
			if ( m_Octave < 8) { m_Octave++; }
			break;
		case SB_LINEUP:
		case SB_PAGEUP:
			if ( m_Octave>0 ) { m_Octave--; }
			break;
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_Octave=std::max(0,std::min((int)nPos,8));
			break;
		default: 
			break;
	}
	if (m_Octave != m_SampleAssign.Octave()) {
		m_scBar.SetScrollPos(m_Octave);
		m_SampleAssign.Octave(8-m_Octave);
		m_SampleAssign.Invalidate();
	}
	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}



void CInstrumentGenDlg::OnBtnSetDefaults()
{
	m_instr->SetDefaultNoteMap(m_instIdx);
	m_SampleAssign.Invalidate();
	ValidateEnabled();
}
void CInstrumentGenDlg::OnBtnPopupSetSample()
{
	RECT tmp;
	((CButton*)GetDlgItem(IDC_SET_ALL_SAMPLE))->GetWindowRect(&tmp);
//	ClientToScreen(&tmp);

	CMenu menu;
	menu.CreatePopupMenu();

	SampleList & list = Global::song().samples;
	for (int i = 0; i <list.lastused() && i < 256 ; i += 16)
	{
		CMenu popup;
		popup.CreatePopupMenu();
		for(int j = i; (j < i + 16) && j < list.lastused(); j++)
		{
			char s1[64]={'\0'};
			std::sprintf(s1,"%02X%s: %s",j,list.IsEnabled(j)?"*":" "
				,list.Exists(j)?list[j].WaveName().c_str():"");
			popup.AppendMenu(MF_STRING, IDC_INSTR_SETSAMPLES_0 + j, s1);
		}
		char szSub[256] = "";;
		std::sprintf(szSub,"Samples %2X-%2X",i,i+15);
		menu.AppendMenu(MF_POPUP | MF_STRING,
			(UINT_PTR)popup.Detach(),
			szSub);
	}
	menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, tmp.left, tmp.top, this);
	menu.DestroyMenu();

}

void CInstrumentGenDlg::OnSetSample(UINT nID)
{
	int sample = nID - IDC_INSTR_SETSAMPLES_0;
	for(int i=0;i< XMInstrument::NOTE_MAP_SIZE; i++) {
		XMInstrument::NotePair pair = m_instr->NoteToSample(i);
		pair.second = sample;
		m_instr->NoteToSample(i, pair);
	}
	m_SampleAssign.Invalidate();
	ValidateEnabled();
}


void CInstrumentGenDlg::OnBtnEditMapping()
{
	CInstrNoteMap map;
	map.m_instr = m_instr;
	map.m_instIdx = m_instIdx;
	if (map.DoModal() == IDOK) {
		ValidateEnabled();
		m_SampleAssign.Invalidate();
	}
}


void CInstrumentGenDlg::ValidateEnabled() {
	bool prev = m_instr->IsEnabled();
	m_instr->ValidateEnabled();

	XMSamplerUIInst* win = dynamic_cast<XMSamplerUIInst*>(GetParent()->GetParent());
	win->UpdateInstrSamples();

	if (prev == m_instr->IsEnabled()) return;

	win->FillInstrumentList(-2);
	CMainFrame* winMain = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
	winMain->UpdateComboIns(true);
}


void CInstrumentGenDlg::UpdateVirtInstOptions()
{
	int virtualIndex = Global::song().VirtualInstrumentInverted(m_instIdx,true);
	if (virtualIndex != -1) {
		m_virtual.SetCheck(BST_CHECKED);
		m_virtinstcombo.EnableWindow(true);
		m_virtmaccombo.EnableWindow(true);

		Song::macinstpair pair = Global::song().VirtualInstrument(virtualIndex);
		SelectByData(m_virtinstcombo,virtualIndex);
		SelectByData(m_virtmaccombo,pair.first);
	}
	else {
		m_virtual.SetCheck(BST_UNCHECKED);
		m_virtinstcombo.EnableWindow(false);
		m_virtmaccombo.EnableWindow(false);
		m_virtinstcombo.SetCurSel(-1);
		m_virtmaccombo.SetCurSel(-1);
	}
}

void CInstrumentGenDlg::SelectByData(CComboBox& combo,DWORD_PTR data) 
{
	for (int i=0; i < combo.GetCount(); i++) {
		if (combo.GetItemData(i) == data) {
			combo.SetCurSel(i);
			break;
		}
	}
}

//void CInstrumentGenDlg
}}