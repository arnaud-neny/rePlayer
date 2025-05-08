#include <psycle/host/detail/project.private.hpp>
#include "InstrumentPanDlg.hpp"
#include "InstrumentEditorUI.hpp"
#include "PsycleConfig.hpp"
#include "XMSamplerUIInst.hpp"
#include <psycle/host/Song.hpp>

namespace psycle { namespace host {

CInstrumentPanDlg::CInstrumentPanDlg()
: CDialog(CInstrumentPanDlg::IDD)
{
}

CInstrumentPanDlg::~CInstrumentPanDlg()
{
}

void CInstrumentPanDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_VOLCUTOFFPAN, m_SlVolCutoffPan);
	DDX_Control(pDX, IDC_SWING1, m_SlSwing1Glide);

	DDX_Control(pDX, IDC_SLNOTEMODNOTE, m_SlNoteModNote);
	DDX_Control(pDX, IDC_NOTEMOD, m_SlNoteMod);
	
	DDX_Control(pDX, IDC_CUTOFFPAN, m_cutoffPan);

}
/*
BEGIN_MESSAGE_MAP(CInstrumentPanDlg, CDialog)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_CUTOFFPAN, OnBnEnablePan)
END_MESSAGE_MAP()
*/

BOOL CInstrumentPanDlg::PreTranslateMessage(MSG* pMsg)
{
	CWnd *tabCtl = GetParent();
	CWnd *UIInst = tabCtl->GetParent();
	Machine *tmac = Global::song().GetSampulseIfExists();
	InstrumentEditorUI* parent = dynamic_cast<InstrumentEditorUI*>(UIInst->GetParent());
	BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd(), tmac);
	if (res == FALSE ) return CDialog::PreTranslateMessage(pMsg);
	return res;
}

BOOL CInstrumentPanDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_SlVolCutoffPan.SetRange(0, 128);

	m_SlSwing1Glide.SetRangeMax(100);

	m_SlNoteModNote.SetRange(0, 119);

	m_SlNoteMod.SetRange(-32, 32);
	//Hack to fix "0 placed on leftmost on start".
	m_SlNoteMod.SetPos(-32);
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

void CInstrumentPanDlg::AssignPanningValues(XMInstrument& inst)
{
	m_instr = &inst;

	m_cutoffPan.SetCheck(inst.PanEnabled());
	m_SlVolCutoffPan.SetPos(value_mapper::map_1_128<int>(inst.Pan()));
	//FIXME: This is not showing the correct value. Should check if randompanning
	//is erroneous or is not the value to check.
	m_SlSwing1Glide.SetPos(inst.RandomPanning()*100.0f);
	m_SlNoteModNote.SetPos(inst.NoteModPanCenter());
	m_SlNoteMod.SetPos(inst.NoteModPanSep());
	SliderPan(&m_SlVolCutoffPan);
	SliderGlide(&m_SlSwing1Glide);
	SliderModNote(&m_SlNoteModNote);
	SliderMod(&m_SlNoteMod);

	m_EnvelopeEditorDlg.ChangeEnvelope(inst.PanEnvelope());

}
void CInstrumentPanDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
	int uId = the_slider->GetDlgCtrlID();

	switch(nSBCode){
	case TB_BOTTOM: //fallthrough
	case TB_LINEDOWN: //fallthrough
	case TB_PAGEDOWN: //fallthrough
	case TB_TOP: //fallthrough
	case TB_LINEUP: //fallthrough
	case TB_PAGEUP: //fallthrough
	case TB_THUMBPOSITION: //fallthrough
	case TB_THUMBTRACK:
		if (uId == IDC_VOLCUTOFFPAN) { SliderPan(the_slider); }
		if (uId == IDC_SWING1) { SliderGlide(the_slider); }
		if (uId == IDC_SLNOTEMODNOTE) { SliderModNote(the_slider); }
		if (uId == IDC_NOTEMOD) { SliderMod(the_slider); }
		break;
	case TB_ENDTRACK:
		break;
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}
void CInstrumentPanDlg::OnBnEnablePan()
{
	m_instr->PanEnabled(m_cutoffPan.GetCheck()!=0);
}
///TODO: Separate the label drawings from the control.
void CInstrumentPanDlg::SliderPan(CSliderCtrl* slid)
{
	char buffer[64];
	int nPos = slid->GetPos();
	m_instr->Pan(value_mapper::map_128_1(nPos));

	if ( nPos == 0) sprintf(buffer,"||%02d  ",nPos);
	else if ( nPos < 32) sprintf(buffer,"<<%02d  ",nPos);
	else if ( nPos < 64) sprintf(buffer," <%02d< ",nPos);
	else if ( nPos == 64) sprintf(buffer," |%02d| ",nPos);
	else if ( nPos <= 96) sprintf(buffer," >%02d> ",nPos);
	else if (nPos < 128) sprintf(buffer,"  %02d>>",nPos);
	else sprintf(buffer,"  %02d||",nPos);
	((CStatic*)GetDlgItem(IDC_LVOLCUTOFFPAN))->SetWindowText(buffer);
}
void CInstrumentPanDlg::SliderGlide(CSliderCtrl* slid)
{
	char tmp[64];
	m_instr->RandomPanning(m_SlSwing1Glide.GetPos()/100.0f);
	sprintf(tmp,"%d%%",m_SlSwing1Glide.GetPos());
	((CStatic*)GetDlgItem(IDC_LSWING))->SetWindowText(tmp);
}
void CInstrumentPanDlg::SliderModNote(CSliderCtrl* slid)
{
	char tmp[40], tmp2[40];
	m_instr->NoteModPanCenter(slid->GetPos());
	char notes[12][3]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
	int offset = (PsycleGlobal::conf().patView().showA440) ? -1 : 0;
	sprintf(tmp,"%s",notes[slid->GetPos()%12]);
	sprintf(tmp2,"%s%d",tmp,offset+(slid->GetPos()/12));
	((CStatic*)GetDlgItem(IDC_LNOTEMODNOTE))->SetWindowText(tmp2);
}
void CInstrumentPanDlg::SliderMod(CSliderCtrl* slid)
{
	char tmp[40];
	m_instr->NoteModPanSep(slid->GetPos());
	sprintf(tmp,"%.02f%%",(slid->GetPos()/2.56f));
	((CStatic*)GetDlgItem(IDC_LNOTEMOD))->SetWindowText(tmp);

	CWnd *tabCtl = GetParent();
	XMSamplerUIInst* UIInst = (XMSamplerUIInst*)tabCtl->GetParent();
	UIInst->UpdateTabNames();

}

}}