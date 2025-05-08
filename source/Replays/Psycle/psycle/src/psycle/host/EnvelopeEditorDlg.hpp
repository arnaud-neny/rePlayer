#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMInstrument.hpp"
#include "EnvelopeEditor.hpp"

namespace psycle { namespace host {

class XMSampler;

class CEnvelopeEditorDlg : public CDialog
{
public:
	CEnvelopeEditorDlg();
	virtual ~CEnvelopeEditorDlg();

	/// Dialog ID
	enum { IDD = IDD_INST_SAMPULSE_ENVDLG };

public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	void ChangeEnvelope(XMInstrument::Envelope& env);
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedEnvmillis();
	afx_msg void OnBnClickedEnvticks();
	afx_msg void OnBnClickedEnvadsr();
	afx_msg void OnBnClickedEnvfreeform();
	afx_msg void OnBnClickedEnvcheck();
	afx_msg void OnBnClickedCarrycheck();
	afx_msg void OnBnClickedSusBegin();
	afx_msg void OnBnClickedSusEnd();
	afx_msg void OnBnClickedLoopStart();
	afx_msg void OnBnClickedLoopEnd();
	afx_msg void OnEnvelopeChanged();
	afx_msg void OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult);
public:
	CEnvelopeEditor m_EnvelopeEditor;

	CButton m_EnvEnabled;
	CButton m_CarryEnabled;
private:
	void SliderBase(CSliderCtrl* slid);
	void SliderMod(CSliderCtrl* slid);
	void SliderAttack(CSliderCtrl* slid);
	void SliderDecay(CSliderCtrl* slid);
	void SliderSustain(CSliderCtrl* slid);
	void SliderRelease(CSliderCtrl* slid);
	void RefreshButtons();
	void SetADSRMode();
	void SetFreeformMode();

	CSliderCtrl m_SlADSRBase;
	CSliderCtrl m_SlADSRMod;
	CSliderCtrl m_SlADSRAttack;
	CSliderCtrl m_SlADSRDecay;
	CSliderCtrl m_SlADSRSustain;
	CSliderCtrl m_SlADSRRelease;
	float sliderMultiplier;
};

}}