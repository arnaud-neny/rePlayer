///\file
///\brief interface file for psycle::host::CWaveEdChildView.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "WaveEdAmplifyDialog.hpp"
#include <psycle/host/WaveEdMixDialog.hpp>
#include <psycle/host/WaveEdInsertSilenceDialog.hpp>
#include <psycle/host/WaveEdCrossfadeDialog.hpp>
#include <psycle/host/ScrollableDlgBar.hpp>
#include <psycle/helpers/resampler.hpp>
#include <deque>
#include <utility>
namespace psycle { namespace host {

		class CMainFrame;
		class Song;
		int const ID_TIMER_WAVED_PLAYING = 31411;

		/// wave editor window.
		class CWaveEdChildView : public CWnd
		{
		public:
			CWaveEdChildView();
			void SetSong(Song& );
			void SetMainFrame(CMainFrame*);
			virtual ~CWaveEdChildView();
			CMainFrame* GetMainFrame() {return mainFrame; };
			void GenerateAndShow();
			void SetViewData(int ins);
			void SetSpecificZoom(int factor);

			unsigned long GetWaveLength();
			unsigned long GetSelectionLength();			//returns length of current selected block
			unsigned long GetCursorPos();				//returns cursor's position
			float GetVolume();						//returns the volume

			void SetCursorPos(unsigned long newpos);	//sets cursor's position
			bool IsStereo();
			void StartTimer();
			void StopTimer();

		protected:
			virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
		protected:
			// Generated message map functions
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnPaint();
			afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
			afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
			afx_msg void OnLButtonDblClk( UINT nFlags, CPoint point );
			afx_msg void OnMouseMove(UINT nFlags, CPoint point);
			afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
			afx_msg void OnSelectionZoomIn();
			afx_msg void OnSelectionZoomSel();
			afx_msg void OnSelectionZoomOut();
			afx_msg void OnSelectionFadeIn();
			afx_msg void OnSelectionFadeOut();	
			afx_msg void OnSelectionNormalize();
			afx_msg void OnSelectionRemoveDC();
			afx_msg void OnSelectionAmplify();
			afx_msg void OnSelectionReverse();
			afx_msg void OnSelectionShowall();
			afx_msg void OnSelectionInsertSilence();
			afx_msg void OnUpdateSelectionAmplify(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionReverse(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionFadein(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionFadeout(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionNormalize(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionRemovedc(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSeleccionZoomIn(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionZoomSel(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSeleccionZoomOut(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionShowall(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionInsertSilence(CCmdUI* pCmdUI);
			afx_msg void OnEditCopy();
			afx_msg void OnEditCut();
			afx_msg void OnEditCrop();
			afx_msg void OnEditPaste();
			afx_msg void OnEditDelete();
			afx_msg void OnConvertMono();
			afx_msg void OnEditSelectAll();
			afx_msg void OnEditSnapToZero();
			afx_msg void OnPasteOverwrite();
			afx_msg void OnPasteMix();
			afx_msg void OnPasteCrossfade();
			afx_msg void OnUpdateEditCopy(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditCrop(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditDelete(CCmdUI* pCmdUI);
			afx_msg void OnUpdateConvertMono(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditSelectAll(CCmdUI* pCmdUI);
			afx_msg void OnUpdateEditSnapToZero(CCmdUI* pCmdUI);
			afx_msg void OnUpdatePasteOverwrite(CCmdUI* pCmdUI);
			afx_msg void OnUpdatePasteCrossfade(CCmdUI* pCmdUI);
			afx_msg void OnUpdatePasteMix(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSetLoopStart(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSetLoopEnd(CCmdUI* pCmdUI);
			afx_msg void OnPopupSelectionToLoop();
			afx_msg void OnPopupSelectionSustain();
			afx_msg void OnPopupUnsetLoop();
			afx_msg void OnPopupUnsetSustain();
			afx_msg void OnUpdateSelectionToLoop(CCmdUI* pCmdUI);
			afx_msg void OnUpdateSelectionSustain(CCmdUI* pCmdUI);
			afx_msg void OnUpdateUnsetLoop(CCmdUI* pCmdUI);
			afx_msg void OnUpdateUnsetSustain(CCmdUI* pCmdUI);
			afx_msg void OnPopupZoomIn();
			afx_msg void OnPopupZoomOut();
			afx_msg void OnViewSampleHold();
			afx_msg void OnViewSpline();
			afx_msg void OnViewSinc();
			afx_msg void OnUpdateViewSampleHold(CCmdUI* pCmdUI);
			afx_msg void OnUpdateViewSpline(CCmdUI* pCmdUI);
			afx_msg void OnUpdateViewSinc(CCmdUI* pCmdUI);

			afx_msg void OnDestroyClipboard();
			afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
			afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
			afx_msg void OnSize(UINT nType, int cx, int cy);
			afx_msg void OnCustomdrawVolSlider(NMHDR* pNMHDR, LRESULT* pResult);

			afx_msg void OnTimer(UINT_PTR nIDEvent);
			afx_msg void OnDestroy();
			afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
		private:
			//refreshes position/thumb size of scroll bars and hsliders
			void ResetScrollBars(bool bNewLength=false);
			//locates the nearest zero crossing to a given sample index
			unsigned long FindNearestZero(unsigned long startpos);
			//refreshes wave display data
			void RefreshDisplayData(bool bRefreshHeader=false);

			short CalcBufferPeak(short* buffer, int length) const;
			
			//References and child
			Song* _pSong;
			CMainFrame* mainFrame;
			CScrollableDlgBar zoombar;

			//Clipboard
			char*	pClipboardData;
			HGLOBAL hClipboardData, hPasteData;
			// Wave data
			bool wdWave;				//whether we have a wave to display
			int wsInstrument;
			signed short* wdLeft;
			signed short* wdRight;
			bool wdStereo;
			unsigned long wdLength;
			unsigned long wdLoopS;
			unsigned long wdLoopE;
			bool wdLoop;
			unsigned long wdSusLoopS;
			unsigned long wdSusLoopE;
			bool wdSusLoop;

			// Painting pens
			CPen cpen_lo;
			CPen cpen_me;
			CPen cpen_hi;
			CPen cpen_sus;
			CPen cpen_white;

			HCURSOR hResizeLR;		//left/right arrows cursor for sizing selection
			HCURSOR hIBeam;			//i beam cursor

			float const static zoomBase;	//base of the logarithmic scale used for zooming with zoom slider
			//wave display data
			std::deque<std::pair<short, short> > lDisplay;
			std::deque<std::pair<short, short> > rDisplay;
			std::deque<std::pair<short, short> > lHeadDisplay;
			std::deque<std::pair<short, short> > rHeadDisplay;

			// Display data
			unsigned long diStart;		//first sample in current window
			unsigned long diLength;		//number of samples in window
			unsigned long blStart;		//first sample of selection
			unsigned long blLength;		//number of samples selected
			bool blSelection;			//whether data is selected currently
			unsigned long cursorPos;	//location of the cursor
			bool cursorBlink;			//switched on timer messages.. cursor is visible when true

			bool bSnapToZero;
			bool bDragLoopStart, bDragLoopEnd;	//indicates that the user is dragging the loop start/end
			bool bDragSusLoopStart, bDragSusLoopEnd;	//indicates that the user is dragging the loop start/end
			unsigned long SelStart;		//the end of the selection -not- being moved 
			bool contextmenu;

			helpers::dsp::cubic_resampler resampler;
			void* resampler_data;
			CSemaphore mutable semaphore;

			//used for finding invalid rect when resizing selection/moving loop points
			int prevHeadX, prevBodyX;
			int prevBodyLoopS, prevHeadLoopS;
			int prevBodyLoopE, prevHeadLoopE;

			int rbX, rbY;				//mouse pos on rbuttonup- used for some location-sensitive context menu commands
		};
	}   // namespace
}   // namespace
