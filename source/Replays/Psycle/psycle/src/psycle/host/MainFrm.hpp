///\file
///\brief interface file for psycle::host::CMainFrame.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "ChildView.hpp"
#include "ExtensionBar.hpp"
#include "SequenceBar.hpp"
#include "SongBar.hpp"
#include "MachineBar.hpp"
#include "InfoDlg.hpp"
#include "InstrumentEditorUI.hpp"
#include "MidiMonitorDlg.hpp"


namespace universalis { namespace os {
	class terminal;
}}

namespace psycle { namespace host {

    namespace ui { class View; }

		class Song;
		class CWaveEdFrame;
		class CGearRackDlg;
		class CFrameMachine;

		enum
		{
			AUX_PARAMS=0,
			AUX_INSTRUMENT
		};

		/// main frame window.
		class CMainFrame : public CFrameWnd
		{
			friend class InputHandler;
		public:
			CMainFrame();
			virtual ~CMainFrame();      
		protected: 
			DECLARE_DYNAMIC(CMainFrame)

		public:
		// Overrides
			virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
			virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
      virtual BOOL PreTranslateMessage(MSG* pMsg);
			#if !defined NDEBUG
				virtual void AssertValid() const;
				virtual void Dump(CDumpContext& dc) const;
			#endif
		// Implementation
		public:
			void CenterWindowOnPoint(CWnd* pWnd, POINT point);
			void CheckForAutosave();
			void CloseMacGui(int mac,bool closewiredialogs=true);
			void CloseAllMacGuis();
			void ShowMachineGui(int tmac, CPoint point);
			void UpdateEnvInfo();
			void ShowPerformanceDlg();
			void ShowMidiMonitorDlg();
			void ShowInstrumentEditor();
			void ShowWaveEditor();
			void UpdateInstrumentEditor(bool force=false);
			void StatusBarText(std::string txt);
			void PsybarsUpdate();
			BOOL StatusBarIdleText();
			void StatusBarIdle();
			void RedrawGearRackList();
			void WaveEditorBackUpdate();

		private:
			void SaveRecent();
		public:  // control bar embedded members
			void SetAppSongBpm(int x);
			void SetAppSongTpb(int x);
			void ShiftOctave(int x);
			void UpdateMasterValue(int newvalue);
			void UpdateVumeters();

			void ChangeAux(int i);
			void ChangeGen(int i);
			//Only use ChangeWave/ChangeIns when you specifically want to change instSelected/waveSelected. Else use ChangeAux
			void ChangeIns(int i);
			void ChangeWave(int i);
			void UpdateComboIns(bool updatelist=true);
			void UpdateComboGen(bool updatelist=true);
			void EditQuantizeChange(int diff);
			bool LoadWave(int idx);
			void SaveWave(int idx);
			bool LoadInst(int idx);
			void SaveInst(int idx);

			bool ToggleFollowSong();
			void UpdatePlayOrder(bool mode);
			void UpdateSequencer(int bottom = -1);
      
			
			CChildView  m_wndView;      
			//CRebar      m_rebar;
			CToolBar    m_wndToolBar;
			CBitmap     m_tbBm;
			CBitmap     m_tbBm2;
			CImageList  m_tbImagelist;
			SongBar		m_songBar;
			MachineBar	m_machineBar;
			SequenceBar  m_seqBar;
			ExtensionBar  m_extensionBar;
			CStatusBar  m_wndStatusBar;
			std::string		szStatusIdle;
      int defmainmenuitemcount;
			
			CInfoDlg	m_wndInfo;
			CMidiMonitorDlg	m_midiMonitorDlg;
			CGearRackDlg* pGearRackDialog;
			InstrumentEditorUI* m_wndInst;
			CWaveEdFrame*	m_pWndWed;
			universalis::os::terminal * terminal;
            
			// Attributes
		public:
			Song* _pSong;
			CFrameMachine	*m_pWndMac[MAX_MACHINES];



		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg LRESULT OnSetMessageString (WPARAM wParam, LPARAM lParam);
			afx_msg LRESULT OnNewPlotter(WPARAM wParam, LPARAM lParam);
			afx_msg LRESULT OnStemPlotter(WPARAM wParam, LPARAM lParam);
			afx_msg LRESULT OnGCPlotter(WPARAM wParam, LPARAM lParam);
			afx_msg LRESULT OnTerminalMessage(WPARAM wParam, LPARAM lParam);
			afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
			afx_msg void OnSetFocus(CWnd *pOldWnd);
			afx_msg void OnClose();
			afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
			afx_msg void OnDestroy();
			afx_msg void OnDropFiles(HDROP hDropInfo);

//SongBar start
			afx_msg void OnSelchangeTrackcombo();
			afx_msg void OnCloseupTrackcombo();
			afx_msg void OnBpmAddOne();
			afx_msg void OnBpmDecOne();
			afx_msg void OnDecTPB();
			afx_msg void OnIncTPB();
			afx_msg void OnClickBPM();
			afx_msg void OnClickTPB();
			afx_msg void OnCloseupCombooctave();
			afx_msg void OnSelchangeCombooctave();
			afx_msg void OnClipbut();
			afx_msg BOOL OnToolTipNotify( UINT unId, NMHDR *pstNMHDR, LRESULT *pstResult );
//SongBar end
//Machinebar start
			afx_msg void OnSelchangeCombostep();
			afx_msg void OnCloseupCombostep();
			afx_msg void OnSelchangeBarCombogen();
			afx_msg void OnCloseupBarCombogen();
			afx_msg void OnCloseupAuxselect();
			afx_msg void OnSelchangeAuxselect();
			afx_msg void OnSelchangeBarComboins();
			afx_msg void OnCloseupBarComboins();
			afx_msg void OnBDecgen();
			afx_msg void OnBIncgen();
			afx_msg void OnGearRack();
			afx_msg void OnBDecAux();
			afx_msg void OnBIncAux();
			afx_msg void OnLoadwave();
			afx_msg void OnSavewave();
			afx_msg void OnEditwave();
			afx_msg void OnWavebut();
//Machinebar end
//sequencebar start
			afx_msg void OnSelchangeSeqlist();
			afx_msg void OnDblclkSeqlist();
			afx_msg void OnIncshort();
			afx_msg void OnDecshort();
			afx_msg void OnSeqnew();
			afx_msg void OnSeqduplicate();
			afx_msg void OnSeqins();
			afx_msg void OnSeqdelete();
			afx_msg void OnSeqrename();
			afx_msg void OnSeqchange();
			afx_msg void OnSeqcut();
			afx_msg void OnSeqcopy();
			afx_msg void OnSeqpaste();
			afx_msg void OnSeqpasteBelow();
			afx_msg void OnSeqsort();
			afx_msg void OnSeqclear();
			afx_msg void OnUpdatepaste(CCmdUI* pCmdUI);
			afx_msg void OnUpdatepasteBelow(CCmdUI* pCmdUI);
			afx_msg void OnFollow();
			afx_msg void OnRecordNoteoff();
			afx_msg void OnRecordTweaks();
			afx_msg void OnShowpattername();
			afx_msg void OnMultichannelAudition();
			afx_msg void OnNotestoeffects();
			afx_msg void OnMovecursorpaste();
//sequencebar end
//Statusbar start
			afx_msg void OnUpdateIndicatorSeqPos(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorPattern(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorLine(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorTime(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorEdit(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorFollow(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorNoteoff(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorTweaks(CCmdUI *pCmdUI);
			afx_msg void OnUpdateIndicatorOctave(CCmdUI *pCmdUI);
//Statusbar end
//Menu start
			afx_msg void OnViewSongbar();
			afx_msg void OnViewMachinebar();
			afx_msg void OnViewSequencerbar();
			afx_msg void OnUpdateViewSongbar(CCmdUI* pCmdUI);
			afx_msg void OnUpdateViewSequencerbar(CCmdUI* pCmdUI);
			afx_msg void OnUpdateViewMachinebar(CCmdUI* pCmdUI);
			afx_msg void OnPsyhelp();
//Menu end      
      afx_msg LRESULT OnReloadLua(WPARAM, LPARAM);
            
};

 
}}
