#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "WaveScopeCtrl.hpp"
#include <psycle/host/XMInstrument.hpp>

namespace psycle { namespace host {

class XMSampler;

class XMSamplerUISample : public CPropertyPage
{
public:
	DECLARE_DYNAMIC(XMSamplerUISample)

public:
	XMSamplerUISample();
	virtual ~XMSamplerUISample();

	// Datos del cuadro de diálogo
	enum { IDD = IDD_INST_SAMPLEBANK };

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // Compatibilidad con DDX o DDV
	virtual BOOL OnInitDialog();


public:
	void WaveUpdate(bool force);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnSetActive(void);
	afx_msg void OnLbnSelchangeSamplelist();
	afx_msg void OnCbnSelendokVibratotype();
	afx_msg void OnCbnSelendokLoop();
	afx_msg void OnCbnSelendokSustainloop();
	afx_msg void OnEnChangeLoopstart();
	afx_msg void OnEnChangeLoopend();
	afx_msg void OnEnChangeSustainstart();
	afx_msg void OnEnChangeSustainend();
	afx_msg void OnEnChangeWavename();
	afx_msg void OnEnChangeSamplerate();
	afx_msg void OnDeltaposSpinsamplerate(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedOpenwaveeditor();
	afx_msg void OnBnClickedLoad();
	afx_msg void OnBnClickedSave();
	afx_msg void OnBnClickedDupe();
	afx_msg void OnBnClickedDelete();
	afx_msg void OnBnClickedPanenabled();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult);
protected:

	void SliderDefvolume(CSliderCtrl* slid);
	void SliderGlobvolume(CSliderCtrl* slid);
	void SliderPan(CSliderCtrl* slid);
	void SliderVibratoAttack(CSliderCtrl* slid);
	void SliderVibratospeed(CSliderCtrl* slid);
	void SliderVibratodepth(CSliderCtrl* slid);
	void SliderSamplenote(CSliderCtrl* slid);
	void SliderFinetune(CSliderCtrl* slid);
	void FillPanDescription(int val);
	void SetSample(int sample);
	void RefreshSampleList(int sample=-1);
	void RefreshSampleData();
	void UpdateSampleInstrs();
	void DrawScope(void);

	void pWave(XMInstrument::WaveData<> *const p){m_pWave = p;};
	XMInstrument::WaveData<>& rWave(){return *m_pWave;};

	XMInstrument::WaveData<> *m_pWave;
	bool m_Init;

protected:
	CListBox m_SampleList;
	CListBox m_SampleInstList;
	CWaveScopeCtrl m_WaveScope;
};

}}
