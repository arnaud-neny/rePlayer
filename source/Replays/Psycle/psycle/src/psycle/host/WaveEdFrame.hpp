///\file
///\brief interface file for psycle::host::CWaveEdFrame.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "WaveEdChildView.hpp"

namespace psycle { namespace host {

		class CMainFrame;

		/// wave editor frame window.
		class CWaveEdFrame : public CFrameWnd
		{
			DECLARE_DYNAMIC(CWaveEdFrame)
		public:
			CWaveEdFrame(Song& _sng, CMainFrame* pframe);
			virtual ~CWaveEdFrame() throw();
		protected: 
			CWaveEdFrame();

		public:
		//	SetWave(signed short *pleft,signed short *pright,int numsamples, bool stereo);
			void GenerateView();
			void FillWaveCombobox();
			void Notify(void);
			Song* _pSong;
			CMainFrame *mainFrame;
		public:
			virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
		protected:
			virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
		private:
			void AdjustStatusBar(int ins);
			void PlayFrom(unsigned long startpos);
			CStatusBar statusbar;
			CToolBar ToolBar;
			CComboBox comboWav;

			CWaveEdChildView wavview;

			int wsInstrument;
			bool bPlaying;
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnUpdateStatusBar(CCmdUI *pCmdUI);
			afx_msg void OnUpdateSelection(CCmdUI *pCmdUI);
			afx_msg void OnUpdatePlayButtons(CCmdUI *pCmdUI);
			afx_msg void OnUpdateStopButton(CCmdUI *pCmdUI);
			afx_msg void OnUpdateReleaseButton(CCmdUI *pCmdUI);
			afx_msg void OnUpdateFFandRWButtons(CCmdUI* pCmdUI);
			afx_msg void OnUpdateWaveMinus(CCmdUI* pCmdUI);
			afx_msg void OnUpdateWavePlus(CCmdUI* pCmdUI);
			afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
			afx_msg void OnCloseupCmbWave();
			afx_msg void OnWaveMinus();
			afx_msg void OnWavePlus();
			afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
			afx_msg void OnDestroy();
			afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
			afx_msg void OnEditCut();
			afx_msg void OnPlay();
			afx_msg void OnRelease();
			afx_msg void OnPlayFromStart();
			afx_msg void OnStop();
			afx_msg void OnFastForward();
			afx_msg void OnRewind();
			afx_msg void OnFileLoad();
			afx_msg void OnFileSave();
		};

	}   // namespace
}   // namespace
