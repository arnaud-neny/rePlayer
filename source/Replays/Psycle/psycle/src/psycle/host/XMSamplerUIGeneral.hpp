#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {

/////////////////////////////////////////////////////////////////////////////
// XMSamplerUIGeneral dialog
class XMSampler;
class XMSamplerUIGeneral : public CPropertyPage
{
	DECLARE_DYNCREATE(XMSamplerUIGeneral)
// Construction
public:
	XMSamplerUIGeneral();
	virtual ~XMSamplerUIGeneral();

	XMSampler * const pMachine() { return _pMachine;};
	void pMachine(XMSampler * const p) { _pMachine = p;};

	enum { IDD = IDD_GEAR_SAMPULSE_GEN };
	CComboBox	m_interpol;
	CSliderCtrl	m_polyslider;
	CStatic m_polylabel;
	CEdit m_ECommandInfo;
	CButton m_bAmigaSlides;
	CButton m_ckFilter;
	CComboBox m_cbPanningMode;
protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	void SliderPolyphony(CSliderCtrl* slid);
private:
	XMSampler* _pMachine;
	bool m_bInitialize;
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedAmigaSlide();
	afx_msg void OnBnClickedFilter();
	afx_msg void OnCbnSelendokXminterpol();
	afx_msg void OnCbnSelendokXmpanningmode();
};

}}
