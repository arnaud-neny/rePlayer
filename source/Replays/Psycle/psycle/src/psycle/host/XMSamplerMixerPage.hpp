#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMSampler.hpp"

namespace psycle { namespace host {

// Cuadro de diálogo de XMSamplerMixerPage

class XMSamplerMixerPage : public CPropertyPage
{
	DECLARE_DYNAMIC(XMSamplerMixerPage)

public:
	XMSamplerMixerPage();
	virtual ~XMSamplerMixerPage();

// Datos del cuadro de diálogo
	enum { IDD = IDD_GEAR_SAMPULSE_MIXER };

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // Compatibilidad con DDX o DDV

public:
	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnSetActive(void);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSelEndOkFilter1();
	afx_msg void OnSelEndOkFilter2();
	afx_msg void OnSelEndOkFilter3();
	afx_msg void OnSelEndOkFilter4();
	afx_msg void OnSelEndOkFilter5();
	afx_msg void OnSelEndOkFilter6();
	afx_msg void OnSelEndOkFilter7();
	afx_msg void OnSelEndOkFilter8();
	afx_msg void CloseupFilter();
	afx_msg void DropDownFilter();
	afx_msg void OnBnClickedChSurr1();
	afx_msg void OnBnClickedChSurr2();
	afx_msg void OnBnClickedChSurr3();
	afx_msg void OnBnClickedChSurr4();
	afx_msg void OnBnClickedChSurr5();
	afx_msg void OnBnClickedChSurr6();
	afx_msg void OnBnClickedChSurr7();
	afx_msg void OnBnClickedChSurr8();
	afx_msg void OnBnClickedChMute1();
	afx_msg void OnBnClickedChMute2();
	afx_msg void OnBnClickedChMute3();
	afx_msg void OnBnClickedChMute4();
	afx_msg void OnBnClickedChMute5();
	afx_msg void OnBnClickedChMute6();
	afx_msg void OnBnClickedChMute7();
	afx_msg void OnBnClickedChMute8();
private:
	void SliderVolume(CSliderCtrl* slid, int offset);
	void SliderPanning(CSliderCtrl* slid, int offset);
	void SliderCutoff(CSliderCtrl* slid, int offset);
	void SliderRessonance(CSliderCtrl* slid, int offset);
	void ClickSurround(int offset);
	void ClickMute(int offset);
	void SelEndOkFilter(int offset);
	XMSampler::Channel& GetChannel(int channel);

	static const int dlgName[8];
	static const int dlgVol[8];
	static const int dlgSurr[8];
	static const int dlgPan[8];
	static const int dlgFil[8];
	static const int dlgRes[8];
	static const int dlgCut[8];
	static const int dlgMute[8];

	static int const volumeRange;
	static int const panningRange;
	static int const resRange;
	static int const cutoffRange;

	CSliderCtrl m_slChannels;
	CSliderCtrl m_slMaster;
	CProgressCtrl m_vu;
	CStatic	m_voicesPl;
	CButton m_bShowChan;
	CButton m_bShowPlay;
	CButton m_bShowPlayAll;

	Machine** arrayMacs;
	XMSampler *sampler;
	// Indicates the index of the first channel shown (controlled by IDC_SL_CHANNELS)
	int m_ChannelOffset;
	bool m_UpdatingGraphics;
	bool m_comboOpen;
public:
	// Refreshes the values of all the controls of the dialog, except IDC_SL_CHANNELS
	void UpdateAllChannels(void);
	// Refreshes the values of the controls of a specific channel.
	void UpdateChannel(int index);
	// Refreshes the values of the controls of the master channel, including vu meter.
	void UpdateMaster(void);

	void pMachine(XMSampler* mac) { sampler = mac; }
	void pArrayMacs(Machine* macs[]) { arrayMacs = macs; }
private:
};

}}
