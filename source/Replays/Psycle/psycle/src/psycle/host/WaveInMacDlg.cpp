#include <psycle/host/detail/project.private.hpp>
#include "WaveInMacDlg.hpp"

#include "PsycleConfig.hpp"
#include "InputHandler.hpp"

#include "internal_machines.hpp"
#include "AudioDriver.hpp"
#include <psycle/helpers/dsp.hpp>

namespace psycle { namespace host {

CWaveInMacDlg::CWaveInMacDlg(CWnd* wndView, CWaveInMacDlg** windowVar, AudioRecorder& new_recorder)
: CDialog(CWaveInMacDlg::IDD, AfxGetMainWnd())
, mainView(wndView)
, windowVar_(windowVar)
, recorder(new_recorder)
{
	CDialog::Create(IDD, AfxGetMainWnd());
}

void CWaveInMacDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, m_listbox);
	DDX_Control(pDX, IDC_VOLLABEL, m_vollabel);
	DDX_Control(pDX, IDC_SLIDER1, m_volslider);
}

/*
BEGIN_MESSAGE_MAP(CWaveInMacDlg, CDialog)
	ON_WM_CLOSE()
	ON_WM_HSCROLL()
	ON_CBN_SELENDOK(IDC_COMBO1, OnCbnSelendokCombo1)
END_MESSAGE_MAP()
*/


BOOL CWaveInMacDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	FillCombobox();
	m_volslider.SetRange(0,1024);
	m_volslider.SetPos(recorder._gainvol*256);
	char label[30];
	sprintf(label,"%.01fdB", helpers::dsp::dB(recorder._gainvol));
	m_vollabel.SetWindowText(label);
	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
void CWaveInMacDlg::OnCancel()
{
	DestroyWindow();
}
void CWaveInMacDlg::OnClose()
{
	CDialog::OnClose();
	DestroyWindow();
}
void CWaveInMacDlg::PostNcDestroy()
{
	CDialog::PostNcDestroy();
	if(windowVar_!=NULL) *windowVar_ = NULL;
	delete this;
}

BOOL CWaveInMacDlg::PreTranslateMessage(MSG* pMsg) 
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
void CWaveInMacDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	//CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
	switch(nSBCode){
	case TB_BOTTOM: //fallthrough
	case TB_LINEDOWN: //fallthrough
	case TB_PAGEDOWN: //fallthrough
	case TB_TOP: //fallthrough
	case TB_LINEUP: //fallthrough
	case TB_PAGEUP: //fallthrough
		OnChangeSlider();
		break;
	case TB_THUMBPOSITION: //fallthrough
	case TB_THUMBTRACK:
		OnChangeSlider();
		break;
	}
	CDialog::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CWaveInMacDlg::FillCombobox()
{
	AudioDriver &mydriver = *Global::configuration()._pOutputDriver;
	std::vector<std::string> ports;
	mydriver.RefreshAvailablePorts();
	mydriver.GetCapturePorts(ports);
	for (unsigned int i =0; i < ports.size(); ++i)
	{
		m_listbox.AddString(ports[i].c_str());
	}
	if (ports.size()==0) m_listbox.AddString("No Inputs Available");
	m_listbox.SetCurSel(recorder._captureidx);
}

void CWaveInMacDlg::OnCbnSelendokCombo1()
{
	recorder.ChangePort(m_listbox.GetCurSel());
}

void CWaveInMacDlg::OnChangeSlider()
{
	char label[30];
	recorder._gainvol = value_mapper::map_256_1(m_volslider.GetPos());
	sprintf(label,"%.01fdB", helpers::dsp::dB(recorder._gainvol));
	m_vollabel.SetWindowText(label);
}

}   // namespace
}   // namespace

