#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMInstrument.hpp"
#include "EnvelopeEditorDlg.hpp"

namespace psycle { namespace host {

class XMSampler;

class CInstrumentPanDlg : public CDialog
{
public:
	CInstrumentPanDlg();
	virtual ~CInstrumentPanDlg();

	/// Dialog ID
	enum { IDD = IDD_INST_SAMPULSE_INSTPAN };

public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	void AssignPanningValues(XMInstrument& inst);
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnEnablePan();

private:
	void SliderPan(CSliderCtrl* slid);
	void SliderGlide(CSliderCtrl* slid);
	void SliderModNote(CSliderCtrl* slid);
	void SliderMod(CSliderCtrl* slid);

	CEnvelopeEditorDlg m_EnvelopeEditorDlg;

	CSliderCtrl m_SlVolCutoffPan;
	CButton m_cutoffPan;
	CSliderCtrl m_SlSwing1Glide;
	CSliderCtrl m_SlNoteModNote;
	CSliderCtrl m_SlNoteMod;
	
	XMInstrument *m_instr;

};

}}