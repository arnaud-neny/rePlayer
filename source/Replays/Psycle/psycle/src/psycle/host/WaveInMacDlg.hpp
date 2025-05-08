#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle {
namespace host {

/// gear rack window.

class AudioRecorder;

class CWaveInMacDlg : public CDialog
{
public:
	CWaveInMacDlg(CWnd* wndView,CWaveInMacDlg** windowVar, AudioRecorder& new_recorder);
public:
	AudioRecorder& recorder;
protected:
	void RedrawList();
	void FillCombobox();
	void OnChangeSlider();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnCancel();
	virtual void PostNcDestroy();

	enum { IDD = IDD_GEAR_WAVEIN };
	CStatic m_vollabel;
	CSliderCtrl m_volslider;
	CComboBox m_listbox;
	CWnd* mainView;
	CWaveInMacDlg** windowVar_;

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnClose();
	afx_msg void OnCbnSelendokCombo1();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};

}   // namespace
}   // namespace
