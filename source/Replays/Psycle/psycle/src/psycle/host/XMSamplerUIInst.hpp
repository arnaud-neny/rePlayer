#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMInstrument.hpp"
#include "InstrumentGenDlg.hpp"
#include "InstrumentAmpDlg.hpp"
#include "InstrumentPanDlg.hpp"
#include "InstrumentFilDlg.hpp"

namespace psycle { namespace host {

class XMSampler;

class XMSamplerUIInst : public CPropertyPage
{
public:
	DECLARE_DYNAMIC(XMSamplerUIInst)

public:
	XMSamplerUIInst();
	virtual ~XMSamplerUIInst();

	/// Dialog ID
	enum { IDD = IDD_INST_SAMPULSE_INSTRUMENT };

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // Compatibilidad con DDX o DDV
	virtual BOOL OnInitDialog();


protected:
	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnSetActive(void);

	afx_msg void OnLbnSelchangeInstrumentlist();
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
//	afx_msg void OnTcnSelchangingTab1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedLoadins();
	afx_msg void OnBnClickedSaveins();
	afx_msg void OnBnClickedDupeins();
	afx_msg void OnBnClickedDeleteins();

public:
	void WaveUpdate(bool force);
	void SetInstrumentData(const int instno);
	void FillInstrumentList(int instNo=-1);
	void SetActivePage(int index);
	void UpdateTabNames();
	void UpdateInstrSamples();
	std::set<int> GetSharedSamplesOfInstrument(int instidx, std::set<int> sampNums);
private:
	bool m_bInitialized;
	int		m_iCurrentSelected;

protected:
	CListBox m_InstrumentList;
	CListBox m_InstListSamples;

	CTabCtrl m_tabMain;
	CInstrumentGenDlg m_genTab;
	CInstrumentAmpDlg m_ampTab;
	CInstrumentPanDlg m_panTab;
	CInstrumentFilDlg m_filTab;
	char genTabName[24];
	char ampTabName[24];
	char panTabName[24];
	char filTabName[24];
};

}}
