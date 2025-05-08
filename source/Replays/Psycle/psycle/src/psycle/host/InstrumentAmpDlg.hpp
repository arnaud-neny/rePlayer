#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMInstrument.hpp"
#include "EnvelopeEditorDlg.hpp"

namespace psycle { namespace host {

class XMSampler;

class CInstrumentAmpDlg : public CDialog
{
public:
	CInstrumentAmpDlg();
	virtual ~CInstrumentAmpDlg();

	/// Dialog ID
	enum { IDD = IDD_INST_SAMPULSE_INSTAMP };

public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	void AssignAmplitudeValues(XMInstrument& inst);
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

private:
	void SliderVolume(CSliderCtrl* slid);
	void SliderGlideVol(CSliderCtrl* slid);
	void SliderFadeout(CSliderCtrl* slid);
	void SliderModNote(CSliderCtrl* slid);
	void SliderMod(CSliderCtrl* slid);

	CEnvelopeEditorDlg m_EnvelopeEditorDlg;

	CSliderCtrl m_SlVolCutoffPan;
	CSliderCtrl m_SlSwing1Glide;
	CSliderCtrl m_SlFadeoutRes;
	CSliderCtrl m_SlNoteModNote;
	CSliderCtrl m_SlNoteMod;

	XMInstrument *m_instr;

};

}}