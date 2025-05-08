#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "InstrumentEditor.hpp"
#include "XMSamplerUISample.hpp"
#include "XMSamplerUIInst.hpp"

namespace psycle { namespace host {

/////////////////////////////////////////////////////////////////////////////
// InstrumentEditorUI dialog
class Machine;

class InstrumentEditorUI : public CPropertySheet
{
	DECLARE_DYNAMIC(InstrumentEditorUI)

	public:
		InstrumentEditorUI(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
		InstrumentEditorUI(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
		virtual ~InstrumentEditorUI();

		enum { IDD = IDD_INSTRUMENTUI };

	private:
		HACCEL  m_hAccelTable;
		InstrumentEditorUI** windowVar_;
		CInstrumentEditor m_InstrBasic;
		XMSamplerUIInst m_InstrSampulse;
		XMSamplerUISample m_SampleBank;
		bool init;

	public:
		virtual void PostNcDestroy();
		void Init(InstrumentEditorUI** windowVar);
		void ShowSampler();
		void ShowSampulse();
		void UpdateUI(bool force);
		BOOL PreTranslateChildMessage(MSG* pMsg, HWND focusWin, Machine* mac);

	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg void OnClose();
		afx_msg void OnShowSampler();
		afx_msg void OnShowSampulse();
		afx_msg void OnShowWaveBank();
		afx_msg void OnShowGen();
		afx_msg void OnShowVol();
		afx_msg void OnShowPan();
		afx_msg void OnShowFilter();
};

}}
