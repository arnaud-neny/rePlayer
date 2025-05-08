#include <psycle/host/detail/project.private.hpp>
#include "EnvelopeEditorDlg.hpp"
#include <psycle/host/Player.hpp>
#include <psycle/host/Song.hpp>
#include "InstrumentEditorUI.hpp"
#include "XMSamplerUIInst.hpp"

namespace psycle { namespace host {

CEnvelopeEditorDlg::CEnvelopeEditorDlg()
: CDialog(CEnvelopeEditorDlg::IDD)
, sliderMultiplier(1)
{
}

CEnvelopeEditorDlg::~CEnvelopeEditorDlg()
{
}

void CEnvelopeEditorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INS_ENVELOPE, m_EnvelopeEditor);
	DDX_Control(pDX, IDC_ENVCHECK, m_EnvEnabled);
	DDX_Control(pDX, IDC_ENV_CARRY, m_CarryEnabled);
	DDX_Control(pDX, IDC_ADSRBASE, m_SlADSRBase);
	DDX_Control(pDX, IDC_ADSRMOD, m_SlADSRMod);
	DDX_Control(pDX, IDC_ADSRATT, m_SlADSRAttack);
	DDX_Control(pDX, IDC_ADSRDEC, m_SlADSRDecay);
	DDX_Control(pDX, IDC_ADSRSUS, m_SlADSRSustain);
	DDX_Control(pDX, IDC_ADSRREL, m_SlADSRRelease);
}
/*
BEGIN_MESSAGE_MAP(CEnvelopeEditorDlg, CDialog)
	ON_BN_CLICKED(IDC_ENVCHECK, OnBnClickedEnvcheck)
	ON_BN_CLICKED(IDC_ENV_CARRY, OnBnClickedCarrycheck)
	ON_BN_CLICKED(IDC_ENVMILLIS, OnBnClickedEnvmillis)
	ON_BN_CLICKED(IDC_ENVTICKS, OnBnClickedEnvticks)
	ON_BN_CLICKED(IDC_ENVADSR, OnBnClickedEnvadsr)
	ON_BN_CLICKED(IDC_ENVFREEFORM, OnBnClickedEnvfreeform)
	ON_BN_CLICKED(IDC_ENV_SUSBEGIN, OnBnClickedSusBegin)
	ON_BN_CLICKED(IDC_ENV_SUSEND, OnBnClickedSusEnd)
	ON_BN_CLICKED(IDC_ENV_LOOPSTART, OnBnClickedLoopStart)
	ON_BN_CLICKED(IDC_ENV_LOOPEND, OnBnClickedLoopEnd)
	ON_NOTIFY_RANGE(NM_CUSTOMDRAW, IDC_ADSRBASE, IDC_ADSRREL, OnCustomdrawSliderm)
	ON_MESSAGE_VOID(PSYC_ENVELOPE_CHANGED, OnEnvelopeChanged)
	ON_WM_HSCROLL()
END_MESSAGE_MAP()
*/

BOOL CEnvelopeEditorDlg::PreTranslateMessage(MSG* pMsg) 
{
	CWnd *insDataDlg = GetParent();
	CWnd *tabCtl = insDataDlg->GetParent();
	CWnd *UIInst = tabCtl->GetParent();
	Machine *tmac = Global::song().GetSampulseIfExists();
	InstrumentEditorUI* parent = dynamic_cast<InstrumentEditorUI*>(UIInst->GetParent());
	BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd(), tmac);
	if (res == FALSE ) return CDialog::PreTranslateMessage(pMsg);
	return res;
}

BOOL CEnvelopeEditorDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	m_SlADSRBase.SetRangeMax(100);
	m_SlADSRMod.SetRangeMax(100);
	m_SlADSRAttack.SetRangeMax(250);
	m_SlADSRDecay.SetRangeMax(250);
	m_SlADSRSustain.SetRangeMax(100);
	m_SlADSRRelease.SetRangeMax(500);
	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
void CEnvelopeEditorDlg::ChangeEnvelope(XMInstrument::Envelope &env) {

	m_EnvEnabled.SetCheck(env.IsEnabled());
	m_CarryEnabled.SetCheck(env.IsCarry());
	((CButton*)GetDlgItem(IDC_ENVMILLIS))->SetCheck(env.Mode() == XMInstrument::Envelope::Mode::MILIS);
	((CButton*)GetDlgItem(IDC_ENVTICKS))->SetCheck(env.Mode() == XMInstrument::Envelope::Mode::TICK);
	m_EnvelopeEditor.Initialize(env,Global::song().TicksPerBeat());
	if (env.Mode() == XMInstrument::Envelope::Mode::MILIS) {
		m_SlADSRAttack.SetRangeMax(5000);
		m_SlADSRDecay.SetRangeMax(5000);
		m_SlADSRRelease.SetRangeMax(10000);
		sliderMultiplier = 1;
	}
	else {
		m_SlADSRAttack.SetRangeMax(250);
		m_SlADSRDecay.SetRangeMax(250);
		m_SlADSRRelease.SetRangeMax(500);
		sliderMultiplier = 2500.f / Global::player().bpm;
	}
	if (env.IsAdsr()) {	SetADSRMode(); }
	else { SetFreeformMode(); }
}

void CEnvelopeEditorDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
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
		if (uId == IDC_ADSRBASE) { SliderBase(the_slider); }
		if (uId == IDC_ADSRMOD) { SliderMod(the_slider); }
		if (uId == IDC_ADSRATT) { SliderAttack(the_slider); }
		if (uId == IDC_ADSRDEC) { SliderDecay(the_slider); }
		if (uId == IDC_ADSRSUS) { SliderSustain(the_slider); }
		if (uId == IDC_ADSRREL) { SliderRelease(the_slider); }
		break;
	case TB_ENDTRACK:
		break;
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CEnvelopeEditorDlg::RefreshButtons()
{
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	const int point = m_EnvelopeEditor.editPoint();
	((CButton*)GetDlgItem(IDC_ENV_SUSBEGIN))->SetCheck(point==env.SustainBegin());
	((CButton*)GetDlgItem(IDC_ENV_SUSEND))->SetCheck(point==env.SustainEnd());
	((CButton*)GetDlgItem(IDC_ENV_LOOPSTART))->SetCheck(point==env.LoopStart());
	((CButton*)GetDlgItem(IDC_ENV_LOOPEND))->SetCheck(point==env.LoopEnd());
}


void CEnvelopeEditorDlg::OnBnClickedEnvcheck()
{
	m_EnvelopeEditor.envelope().IsEnabled(m_EnvEnabled.GetCheck()!=0);
	CWnd *insDataDlg = GetParent();
	CWnd *tabCtl = insDataDlg->GetParent();
	XMSamplerUIInst* UIInst = (XMSamplerUIInst*)tabCtl->GetParent();
	UIInst->UpdateTabNames();
}
void CEnvelopeEditorDlg::OnBnClickedCarrycheck()
{
	m_EnvelopeEditor.envelope().IsCarry(m_CarryEnabled.GetCheck()!=0);
}

void CEnvelopeEditorDlg::OnBnClickedEnvmillis()
{
	m_EnvelopeEditor.TimeFormulaMillis();
	m_EnvelopeEditor.Invalidate();
	if (m_EnvelopeEditor.envelope().Mode() == XMInstrument::Envelope::Mode::MILIS) {
		m_SlADSRAttack.SetRangeMax(5000);
		m_SlADSRDecay.SetRangeMax(5000);
		m_SlADSRRelease.SetRangeMax(10000);
		sliderMultiplier = 1;
	}
	else {
		m_SlADSRAttack.SetRangeMax(250);
		m_SlADSRDecay.SetRangeMax(250);
		m_SlADSRRelease.SetRangeMax(500);
		sliderMultiplier = 2500.f / Global::player().bpm;
	}
}
void CEnvelopeEditorDlg::OnBnClickedEnvticks()
{
	m_EnvelopeEditor.TimeFormulaTicks(Global::song().TicksPerBeat());
	m_EnvelopeEditor.Invalidate();
	if (m_EnvelopeEditor.envelope().Mode() == XMInstrument::Envelope::Mode::MILIS) {
		m_SlADSRAttack.SetRangeMax(5000);
		m_SlADSRDecay.SetRangeMax(5000);
		m_SlADSRRelease.SetRangeMax(10000);
		sliderMultiplier = 1;
	}
	else {
		m_SlADSRAttack.SetRangeMax(250);
		m_SlADSRDecay.SetRangeMax(250);
		m_SlADSRRelease.SetRangeMax(500);
		sliderMultiplier = 2500.f / Global::player().bpm;
	}
}

void CEnvelopeEditorDlg::OnBnClickedEnvadsr()
{
	m_EnvelopeEditor.ConvertToADSR(true);
	SetADSRMode();
}
void CEnvelopeEditorDlg::SetADSRMode()
{
	((CButton*)GetDlgItem(IDC_ENVFREEFORM))->SetCheck(FALSE);
	((CButton*)GetDlgItem(IDC_ENVADSR))->SetCheck(TRUE);
	m_SlADSRBase.EnableWindow(TRUE);
	m_SlADSRMod.EnableWindow(TRUE);
	m_SlADSRAttack.EnableWindow(TRUE);
	m_SlADSRDecay.EnableWindow(TRUE);
	m_SlADSRSustain.EnableWindow(TRUE);
	m_SlADSRRelease.EnableWindow(TRUE);
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	XMInstrument::Envelope::ValueType min=env.GetValue(0);
	XMInstrument::Envelope::ValueType max=env.GetValue(1);
	float diff = max-min;
	m_SlADSRBase.SetPos(min*100);
	if(diff > 0.f) {
		m_SlADSRMod.SetPos(diff*100.f);
	}
	else {m_SlADSRMod.SetPos(100);}
	m_SlADSRAttack.SetPos(m_EnvelopeEditor.AttackTime());
	m_SlADSRDecay.SetPos(m_EnvelopeEditor.DecayTime());

	m_SlADSRSustain.SetPos((m_EnvelopeEditor.SustainValue()-min)*100/diff);
	m_SlADSRRelease.SetPos(m_EnvelopeEditor.ReleaseTime());

	m_EnvelopeEditor.Invalidate();
}
		
void CEnvelopeEditorDlg::OnBnClickedEnvfreeform()
{
	m_EnvelopeEditor.envelope().SetAdsr(false);
	SetFreeformMode();
}
void CEnvelopeEditorDlg::SetFreeformMode() {
	((CButton*)GetDlgItem(IDC_ENVFREEFORM))->SetCheck(TRUE);
	((CButton*)GetDlgItem(IDC_ENVADSR))->SetCheck(FALSE);
	m_SlADSRBase.EnableWindow(FALSE);
	m_SlADSRMod.EnableWindow(FALSE);
	m_SlADSRAttack.EnableWindow(FALSE);
	m_SlADSRDecay.EnableWindow(FALSE);
	m_SlADSRSustain.EnableWindow(FALSE);
	m_SlADSRRelease.EnableWindow(FALSE);
}

void CEnvelopeEditorDlg::SliderBase(CSliderCtrl* slid)
{
	float diff = std::min(100-m_SlADSRBase.GetPos(),m_SlADSRMod.GetPos()) * 0.01f;
	float base = m_SlADSRBase.GetPos()*0.01f;
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	env.SetValue(0, base);
	env.SetValue(1, base+diff);
	env.SetValue(2, base+((float)m_SlADSRSustain.GetPos()*0.01f*diff));
	env.SetValue(3, base);
	m_EnvelopeEditor.Invalidate();
}
void CEnvelopeEditorDlg::SliderMod(CSliderCtrl* slid)
{
	float diff = std::min(100-m_SlADSRBase.GetPos(),m_SlADSRMod.GetPos()) * 0.01f;
	float base = m_SlADSRBase.GetPos()*0.01f;
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	env.SetValue(1, base+diff);
	env.SetValue(2, base+((float)m_SlADSRSustain.GetPos()*0.01f*diff));
	m_EnvelopeEditor.Invalidate();
}
void CEnvelopeEditorDlg::SliderAttack(CSliderCtrl* slid)
{
	m_EnvelopeEditor.AttackTime(m_SlADSRAttack.GetPos());
	m_EnvelopeEditor.Invalidate();
}
void CEnvelopeEditorDlg::SliderDecay(CSliderCtrl* slid)
{
	m_EnvelopeEditor.DecayTime(m_SlADSRDecay.GetPos());
	m_EnvelopeEditor.Invalidate();
}
void CEnvelopeEditorDlg::SliderSustain(CSliderCtrl* slid)
{
	float diff = std::min(100-m_SlADSRBase.GetPos(),m_SlADSRMod.GetPos()) * 0.01f;
	float base = m_SlADSRBase.GetPos()*0.01f;
	m_EnvelopeEditor.SustainValue(base+((float)m_SlADSRSustain.GetPos()*0.01f*diff));
	m_EnvelopeEditor.Invalidate();
}
void CEnvelopeEditorDlg::SliderRelease(CSliderCtrl* slid)
{
	m_EnvelopeEditor.ReleaseTime(m_SlADSRRelease.GetPos());
	m_EnvelopeEditor.Invalidate();
}

void CEnvelopeEditorDlg::OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult) 
{
/*
	NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	if (nmcd.dwDrawStage == CDDS_POSTPAINT)
	{
		char tmp[64];
		int label = 0;
		CSliderCtrl* slider = reinterpret_cast<CSliderCtrl*>(GetDlgItem(pNMHDR->idFrom));
		if (pNMHDR->idFrom == IDC_ADSRBASE) {
			sprintf(tmp,"%d%%",slider->GetPos());
			label = IDC_LADSRBASE;
		}
		else if(pNMHDR->idFrom == IDC_ADSRMOD) {
			sprintf(tmp,"%d%%",slider->GetPos());
			label = IDC_LADSRMOD;
		}
		else if(pNMHDR->idFrom == IDC_ADSRSUS) {
			sprintf(tmp,"%d%%",slider->GetPos());
			label = IDC_LADSRSUS;
		}
		else if (pNMHDR->idFrom == IDC_ADSRATT) {
			sprintf(tmp,"%.0fms",slider->GetPos()*sliderMultiplier);
			label = IDC_LADSRATT;
		}
		else if (pNMHDR->idFrom == IDC_ADSRDEC) {
			sprintf(tmp,"%.0fms",slider->GetPos()*sliderMultiplier);
			label = IDC_LADSRDEC;
		}
		else if (pNMHDR->idFrom == IDC_ADSRREL) {
			sprintf(tmp,"%.0fms",slider->GetPos()*sliderMultiplier);
			label = IDC_LADSRREL;
		}
		if (label != 0) {
			((CStatic*)GetDlgItem(label))->SetWindowText(tmp);
		}
		*pResult = CDRF_DODEFAULT;
	}
	else if (nmcd.dwDrawStage == CDDS_PREPAINT ){
		*pResult = CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
	}
	else {
		*pResult = CDRF_DODEFAULT;
	}
*/
}
void CEnvelopeEditorDlg::OnBnClickedSusBegin()
{
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	const int point = m_EnvelopeEditor.editPoint();
	if(point < env.NumOfPoints()) {
		if (env.SustainBegin() == point) {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_REMOVESUSTAIN);
		}
		else {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_SETSUSTAINBEGIN);
		}
		RefreshButtons();
	}
}
void CEnvelopeEditorDlg::OnBnClickedSusEnd()
{
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	const int point = m_EnvelopeEditor.editPoint();
	if(point < env.NumOfPoints()) {
		if (env.SustainEnd() == point) {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_REMOVESUSTAIN);
		}
		else {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_SETSUSTAINEND);
		}
		RefreshButtons();
	}
}
void CEnvelopeEditorDlg::OnBnClickedLoopStart()
{
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	const int point = m_EnvelopeEditor.editPoint();
	if(point < env.NumOfPoints()) {
		if (env.LoopStart() == point) {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_REMOVELOOP);
		}
		else {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_SETLOOPSTART);
		}
		RefreshButtons();
	}
}
void CEnvelopeEditorDlg::OnBnClickedLoopEnd()
{
	XMInstrument::Envelope& env = m_EnvelopeEditor.envelope();
	const int point = m_EnvelopeEditor.editPoint();
	if(point < env.NumOfPoints()) {
		if (env.LoopEnd() == point) {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_REMOVELOOP);
		}
		else {
			m_EnvelopeEditor.SendMessage(WM_COMMAND, ID__ENV_SETLOOPEND);
		}
		RefreshButtons();
	}
}

void CEnvelopeEditorDlg::OnEnvelopeChanged()
{
	RefreshButtons();
	if (m_EnvelopeEditor.envelope().IsAdsr()) {

		m_SlADSRAttack.SetPos(m_EnvelopeEditor.AttackTime());
		m_SlADSRDecay.SetPos(m_EnvelopeEditor.DecayTime());
		m_SlADSRRelease.SetPos(m_EnvelopeEditor.ReleaseTime());

		float diff = std::min(100-m_SlADSRBase.GetPos(),m_SlADSRMod.GetPos()) * 0.01f;
		float base = m_SlADSRBase.GetPos()*0.01f;
		m_SlADSRSustain.SetPos((m_EnvelopeEditor.SustainValue()-base)*100/diff);
	}
}

}}