///\file
///\brief implementation file for psycle::host::CWaveEdFrame.
#include <psycle/host/detail/project.private.hpp>
#include "WaveEdFrame.hpp"

#include "Configuration.hpp"
#include "AudioDriver.hpp"
#include "MainFrm.hpp"

#include "Song.hpp"

namespace psycle { namespace host {
		extern CPsycleApp theApp;
		IMPLEMENT_DYNAMIC(CWaveEdFrame, CFrameWnd)

/*
		BEGIN_MESSAGE_MAP(CWaveEdFrame, CFrameWnd)
			ON_WM_CLOSE()
			ON_UPDATE_COMMAND_UI ( ID_INDICATOR_SIZE, OnUpdateStatusBar )
			ON_WM_CREATE()
			ON_WM_SHOWWINDOW()
			ON_UPDATE_COMMAND_UI ( ID_INDICATOR_MODE, OnUpdateStatusBar )
			ON_UPDATE_COMMAND_UI ( ID_INDICATOR_SEL,  OnUpdateSelection )
			ON_UPDATE_COMMAND_UI ( ID_WAVED_PLAYFROMSTART, OnUpdatePlayButtons )
			ON_UPDATE_COMMAND_UI ( ID_WAVED_PLAY,	OnUpdatePlayButtons		)
			ON_UPDATE_COMMAND_UI ( ID_WAVED_STOP,	OnUpdateStopButton		)
			ON_UPDATE_COMMAND_UI ( ID_WAVED_RELEASE,OnUpdateReleaseButton		)
			ON_UPDATE_COMMAND_UI ( ID_WAVED_FASTFORWARD, OnUpdateFFandRWButtons )
			ON_UPDATE_COMMAND_UI ( ID_WAVED_REWIND, OnUpdateFFandRWButtons )
			ON_UPDATE_COMMAND_UI ( ID_WAVEED_WAVEMIN, OnUpdateWaveMinus )
			ON_UPDATE_COMMAND_UI ( ID_WAVEED_WAVEPLUS, OnUpdateWavePlus )
			ON_UPDATE_COMMAND_UI ( ID_WAVEED_SAVE,  OnUpdateFileSave )
			ON_COMMAND ( ID_WAVEED_LOAD, OnFileLoad )
			ON_COMMAND ( ID_WAVEED_SAVE, OnFileSave )
			ON_COMMAND ( ID_WAVEED_WAVEMIN, OnWaveMinus )
			ON_COMMAND ( ID_WAVEED_WAVEPLUS, OnWavePlus )
			ON_COMMAND ( ID_WAVED_PLAY, OnPlay )
			ON_COMMAND ( ID_WAVED_PLAYFROMSTART, OnPlayFromStart )
			ON_COMMAND ( ID_WAVED_STOP, OnStop )
			ON_COMMAND ( ID_WAVED_RELEASE, OnRelease)
			ON_COMMAND ( ID_WAVED_FASTFORWARD, OnFastForward )
			ON_COMMAND ( ID_WAVED_REWIND, OnRewind )
			ON_CBN_CLOSEUP(ID_WAVEEDIT_COMBO_WAV, OnCloseupCmbWave)
			ON_WM_DESTROY()
		END_MESSAGE_MAP()
*/

		static UINT indicators[] =
		{
			ID_SEPARATOR,           // status line indicator
			ID_INDICATOR_SEL,
			ID_INDICATOR_SIZE,
			ID_INDICATOR_MODE
		};

		CWaveEdFrame::CWaveEdFrame(): wsInstrument(0)
		{
		}
		CWaveEdFrame::CWaveEdFrame(Song& _sng, CMainFrame* pframe): mainFrame(pframe), wsInstrument(0)
		{
			this->_pSong = &_sng;
			wavview.SetSong(_sng);
			wavview.SetMainFrame(pframe);
		}

		CWaveEdFrame::~CWaveEdFrame() throw()
		{
		}

		BOOL CWaveEdFrame::PreCreateWindow(CREATESTRUCT& cs) 
		{
			if( !CFrameWnd::PreCreateWindow(cs) )
			{
				return false;
			}

			cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
			cs.lpszClass = AfxRegisterWndClass(0);

			return true;	
		}

		BOOL CWaveEdFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
		{
			if (wavview.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
			{
				this->AdjustStatusBar(wsInstrument);
				return true;	
			}
			return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
		}

		int CWaveEdFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
		{
			if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
			{
				return -1;
			}
			if (!wavview.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
				CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
			{
				TRACE0("Failed to create view window\n");
				return -1;
			}

			if( !ToolBar.CreateEx(this, TBSTYLE_FLAT| TBSTYLE_TRANSPARENT) ||
				!ToolBar.LoadToolBar(IDR_WAVEDFRAME))
			{
				TRACE0("Failed to create toolbar\n");
				return -1;      // fail to create
			}
			CRect rect;
			int nIndex = ToolBar.GetToolBarCtrl().CommandToIndex(ID_WAVEEDIT_COMBO_WAV);
			ToolBar.SetButtonInfo(nIndex, ID_WAVEEDIT_COMBO_WAV, TBBS_SEPARATOR, 160);
			ToolBar.GetToolBarCtrl().GetItemRect(nIndex, &rect);
			rect.top = 1;
			rect.bottom = rect.top + 400; //drop height
			if(!comboWav.Create( WS_CHILD |  CBS_DROPDOWNLIST | WS_VISIBLE | CBS_AUTOHSCROLL 
				 | WS_VSCROLL, rect, &ToolBar, ID_WAVEEDIT_COMBO_WAV))
			{
				TRACE0("Failed to create combobox\n");
				return -1;      // fail to create
			}
			HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
			CFont font;
			font.Attach( hFont );
			comboWav.SetFont(&font);

			// Status bar
			if (!statusbar.Create(this) ||
				!statusbar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT)))
			{
				TRACE0("Failed to create status bar\n");
				return -1;      // fail to create
			}
			statusbar.SetPaneStyle(0, /*SBPS_NORMAL*/ SBPS_STRETCH);
			statusbar.SetPaneInfo(1, ID_INDICATOR_SEL, SBPS_NORMAL, 180);
			statusbar.SetPaneInfo(2, ID_INDICATOR_SIZE, SBPS_NORMAL, 180);
			statusbar.SetPaneInfo(3, ID_INDICATOR_MODE, SBPS_NORMAL, 70);

			
			ToolBar.SetWindowText("Psycle Wave Editor tool bar");
			ToolBar.EnableDocking(CBRS_ALIGN_ANY);
			EnableDocking(CBRS_ALIGN_ANY);
			DockControlBar(&ToolBar);
			LoadBarState(_T("WaveEdToolbar"));
			// Sets Icon
			HICON tIcon;
			tIcon=theApp.LoadIcon(IDR_WAVEFRAME);
			SetIcon(tIcon, true);
			SetIcon(tIcon, false);

			bPlaying=false;
			SetWindowText("Psycle wave editor");
			return 0;
		}

		void CWaveEdFrame::OnClose() 
		{
			AfxGetMainWnd()->SetFocus();
			ShowWindow(SW_HIDE);
			OnStop();
		}
		void CWaveEdFrame::OnDestroy()
		{
			SaveBarState(_T("WaveEdToolbar"));
			OnStop();
			HICON _icon = GetIcon(false);
			DestroyIcon(_icon);
			CFrameWnd::OnDestroy();
		}

		void CWaveEdFrame::GenerateView() 
		{	
			if (_pSong->samples.size() == 0) {
				XMInstrument::WaveData<> tmpIns;
				_pSong->samples.SetSample(tmpIns,0);
			}
			FillWaveCombobox();
			this->wavview.GenerateAndShow(); 
		}
		void CWaveEdFrame::FillWaveCombobox() 
		{
			comboWav.ResetContent();
			char buffer[64];
			for (int i(0); i<_pSong->samples.size(); i++)
			{
				if (_pSong->samples.Exists(i)) {
					sprintf(buffer, "%.2X: %s", i, _pSong->samples[i].WaveName().c_str());
				}
				else {
					sprintf(buffer, "%02X:", i);
				}
				comboWav.AddString(buffer);
			}
			comboWav.SetCurSel(_pSong->waveSelected);
		}
		void CWaveEdFrame::OnUpdateStatusBar(CCmdUI *pCmdUI)  
		{     
			pCmdUI->Enable();  
		}

		void CWaveEdFrame::OnUpdatePlayButtons(CCmdUI *pCmdUI)
		{
			pCmdUI->Enable(!_pSong->wavprev.IsEnabled());
		}

		void CWaveEdFrame::OnUpdateStopButton(CCmdUI *pCmdUI)
		{
			pCmdUI->Enable(_pSong->wavprev.IsEnabled());
		}

		void CWaveEdFrame::OnUpdateReleaseButton(CCmdUI *pCmdUI)
		{
			pCmdUI->Enable(_pSong->wavprev.IsEnabled() && _pSong->wavprev.IsLooping());
		}

		void CWaveEdFrame::OnUpdateSelection(CCmdUI *pCmdUI)
		{
			pCmdUI->Enable();

			char buff[48];
			int sl=wavview.GetSelectionLength();
			if(sl==0 || _pSong->_pInstrument[wsInstrument]==NULL)
				sprintf(buff, "No Data in Selection.");
			else
			{
				float slInSecs = sl / float(Global::configuration()._pOutputDriver->GetSamplesPerSec());
				sprintf(buff, "Selection: %u (%0.3f secs.)", sl, slInSecs);
			}
			statusbar.SetPaneText(1, buff, true);
		}

		void CWaveEdFrame::AdjustStatusBar(int ins)
		{
			char buff[48];
			int	wl=0;
			if (_pSong->samples.IsEnabled(ins)) { wl = _pSong->samples[ins].WaveLength();}
			float wlInSecs = wl / float(Global::configuration()._pOutputDriver->GetSamplesPerSec());
			sprintf(buff, "Size: %u (%0.3f secs.)", wl, wlInSecs);
			statusbar.SetPaneText(2, buff, true);

			if (wl)
			{
				if (_pSong->samples[ins].IsWaveStereo()) statusbar.SetPaneText(3, "Mode: Stereo", true);
				else statusbar.SetPaneText(3, "Mode: Mono", true);
			}
			else statusbar.SetPaneText(3, "Mode: Empty", true);
		}

		void CWaveEdFrame::OnShowWindow(BOOL bShow, UINT nStatus) 
		{
			CFrameWnd::OnShowWindow(bShow, nStatus);
			if(bShow) {
				Notify();
				UpdateWindow();
				wavview.StartTimer();
			}
			else 
			{
				wavview.StopTimer();
			}
		}

		void CWaveEdFrame::Notify(void)
		{
			FillWaveCombobox();
			wsInstrument = _pSong->waveSelected;
			wavview.SetViewData(wsInstrument);
			AdjustStatusBar(wsInstrument);
		}
		void CWaveEdFrame::OnUpdateFileSave(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong->samples.IsEnabled(wsInstrument));
		}
		void CWaveEdFrame::OnFileLoad()
		{
			mainFrame->LoadWave(wsInstrument);
			//LoadWave does a call to refresh the editors, which includes our editor.
			//Our Notify() will be called.
		}
		void CWaveEdFrame::OnFileSave()
		{
			mainFrame->SaveWave(wsInstrument);
		}


		void CWaveEdFrame::OnPlay() {PlayFrom(wavview.GetCursorPos());}
		void CWaveEdFrame::OnPlayFromStart() {PlayFrom(0);}
		void CWaveEdFrame::OnRelease()
		{
			_pSong->wavprev.Release();
		}
		void CWaveEdFrame::OnStop()
		{
			_pSong->wavprev.Stop();
		}

		void CWaveEdFrame::PlayFrom(unsigned long startPos)
		{
			uint32_t wl=0;
			if (_pSong->samples.IsEnabled(wsInstrument)) { wl = _pSong->samples[wsInstrument].WaveLength();}
			if( startPos >= wl )
				return;

			OnStop();
			_pSong->wavprev.SetVolume(wavview.GetVolume());
			_pSong->wavprev.Play(notecommands::middleC, startPos);
			wavview.SetTimer(ID_TIMER_WAVED_PLAYING, 33, 0);
		}
		void CWaveEdFrame::OnUpdateFFandRWButtons(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(true);
		}

		void CWaveEdFrame::OnFastForward()
		{
			uint32_t curpos = wavview.GetCursorPos();
			if (_pSong->samples.IsEnabled(wsInstrument)) {
				const XMInstrument::WaveData<> & wave = _pSong->samples[wsInstrument];
				uint32_t targetpos = wave.WaveLength()-1;
				if (wave.WaveLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
					if (wave.WaveLoopStart() > curpos) {
						targetpos = std::min(targetpos, wave.WaveLoopStart());
					}
					else if (wave.WaveLoopEnd() > curpos) {
						targetpos = std::min(targetpos, wave.WaveLoopEnd());
					}
				}
				if (wave.WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
					if (wave.WaveSusLoopStart() > curpos) {
						targetpos = std::min(targetpos, wave.WaveSusLoopStart());
					}
					else if (wave.WaveSusLoopEnd() > curpos) {
						targetpos = std::min(targetpos, wave.WaveSusLoopEnd());
					}
				}
				curpos = targetpos;
			}
			wavview.SetCursorPos( curpos );
		}
		void CWaveEdFrame::OnRewind()
		{
			uint32_t curpos = wavview.GetCursorPos();
			if (_pSong->samples.IsEnabled(wsInstrument)) {
				const XMInstrument::WaveData<> & wave = _pSong->samples[wsInstrument];
				uint32_t targetpos = 0;
				if (wave.WaveLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
					if (wave.WaveLoopEnd() < curpos) {
						targetpos = std::max(targetpos, wave.WaveLoopEnd());
					}
					else if (wave.WaveLoopStart() < curpos) {
						targetpos = std::max(targetpos, wave.WaveLoopStart());
					}
				}
				if (wave.WaveSusLoopType() != XMInstrument::WaveData<>::LoopType::DO_NOT) {
					if (wave.WaveSusLoopEnd() < curpos) {
						targetpos = std::max(targetpos, wave.WaveSusLoopEnd());
					}
					else if (wave.WaveSusLoopStart() < curpos) {
						targetpos = std::max(targetpos, wave.WaveSusLoopStart());
					}
				}
				curpos = targetpos;
			}
			wavview.SetCursorPos( curpos );
		}

		void CWaveEdFrame::OnUpdateWaveMinus(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong->waveSelected>0);
		}
		void CWaveEdFrame::OnUpdateWavePlus(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong->waveSelected<MAX_INSTRUMENTS);
		}
		void CWaveEdFrame::OnCloseupCmbWave()
		{
			mainFrame->ChangeWave(comboWav.GetCurSel());
			wsInstrument = _pSong->waveSelected;
			mainFrame->UpdateComboIns(false);
		}
		void CWaveEdFrame::OnWaveMinus()
		{
			mainFrame->ChangeWave(_pSong->waveSelected-1);
			wsInstrument = _pSong->waveSelected;
			mainFrame->UpdateComboIns(false);
		}
		void CWaveEdFrame::OnWavePlus()
		{
			bool update=false;
			wsInstrument = _pSong->waveSelected+1;
			if (_pSong->samples.size()<=wsInstrument) {
				XMInstrument::WaveData<> tmpIns;
				_pSong->samples.SetSample(tmpIns,wsInstrument);
				update=true;
				FillWaveCombobox();
			}
			else {
				comboWav.SetCurSel(wsInstrument);
			}
			mainFrame->ChangeWave(wsInstrument);
			mainFrame->UpdateComboIns(update);
		}

	}   // namespace
}   // namespace
