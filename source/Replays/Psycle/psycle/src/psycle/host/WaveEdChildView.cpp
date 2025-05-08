///\file
///\brief implementation file for psycle::host::CWaveEdChildView.
#include <psycle/host/detail/project.private.hpp>
#include <psycle/helpers/riff.hpp>
#include "WaveEdChildView.hpp"

#include "PsycleConfig.hpp"
#include "AudioDriver.hpp"

#include "MainFrm.hpp"
#include "XMInstrument.hpp"
#include "InputHandler.hpp"
#include "WaveEdFrame.hpp"
#include "Zap.hpp"
#include <mmreg.h>
#include <math.h>

namespace psycle { namespace host {

		using namespace helpers;
		int const ID_TIMER_WAVED_REFRESH = 31415;
	
		float const CWaveEdChildView::zoomBase = 1.06f;

		CWaveEdChildView::CWaveEdChildView()
		{
			resampler.quality(helpers::dsp::resampler::quality::spline);
			resampler_data =resampler.GetResamplerData();
			resampler.UpdateSpeed(resampler_data,1.0);
			contextmenu=false;
		}

		CWaveEdChildView::~CWaveEdChildView()
		{
		}


/*
		BEGIN_MESSAGE_MAP(CWaveEdChildView, CWnd)
			ON_WM_PAINT()
			ON_WM_RBUTTONDOWN()
			ON_WM_LBUTTONDOWN()
			ON_WM_LBUTTONDBLCLK()
			ON_WM_MOUSEMOVE()
			ON_WM_LBUTTONUP()
			ON_COMMAND(ID_SELECCION_ZOOMIN, OnSelectionZoomIn)
			ON_COMMAND(ID_SELECCION_ZOOMOUT, OnSelectionZoomOut)
			ON_COMMAND(ID_SELECTION_ZOOMSEL, OnSelectionZoomSel)
			ON_COMMAND(ID_POPUP_ZOOMSEL, OnSelectionZoomSel)
			ON_COMMAND(ID_SELECTION_FADEIN, OnSelectionFadeIn)
			ON_COMMAND(ID_SELECTION_FADEOUT, OnSelectionFadeOut)
			ON_COMMAND(ID_SELECTION_NORMALIZE, OnSelectionNormalize)
			ON_COMMAND(ID_SELECTION_REMOVEDC, OnSelectionRemoveDC)
			ON_COMMAND(ID_SELECTION_AMPLIFY, OnSelectionAmplify)
			ON_COMMAND(ID_SELECTION_REVERSE, OnSelectionReverse)
			ON_COMMAND(ID_SELECTION_SHOWALL, OnSelectionShowall)
			ON_COMMAND(ID_SELECTION_INSERTSILENCE, OnSelectionInsertSilence)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_AMPLIFY, OnUpdateSelectionAmplify)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_REVERSE, OnUpdateSelectionReverse)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_FADEIN, OnUpdateSelectionFadein)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_FADEOUT, OnUpdateSelectionFadeout)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_NORMALIZE, OnUpdateSelectionNormalize)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_REMOVEDC, OnUpdateSelectionRemovedc)
			ON_UPDATE_COMMAND_UI(ID_SELECCION_ZOOMIN, OnUpdateSeleccionZoomIn)
			ON_UPDATE_COMMAND_UI(ID_POPUP_ZOOMIN, OnUpdateSeleccionZoomIn)
			ON_UPDATE_COMMAND_UI(ID_SELECCION_ZOOMOUT, OnUpdateSeleccionZoomOut)
			ON_UPDATE_COMMAND_UI(ID_POPUP_ZOOMOUT, OnUpdateSeleccionZoomOut)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_SHOWALL, OnUpdateSelectionShowall)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_ZOOMSEL, OnUpdateSelectionZoomSel)
			ON_UPDATE_COMMAND_UI(ID_POPUP_ZOOMSEL, OnUpdateSelectionZoomSel)
			ON_UPDATE_COMMAND_UI(ID_SELECTION_INSERTSILENCE, OnUpdateSelectionInsertSilence)
			ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
			ON_COMMAND(ID_POPUP_COPY, OnEditCopy)
			ON_COMMAND(ID_EDIT_CUT, OnEditCut)
			ON_COMMAND(ID_POPUP_CUT, OnEditCut)
			ON_COMMAND(ID_EDIT_CROP, OnEditCrop)
			ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
			ON_COMMAND(ID_POPUP_PASTE, OnEditPaste)
			ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
			ON_COMMAND(ID_POPUP_DELETE, OnEditDelete)
			ON_COMMAND(ID_CONVERT_MONO, OnConvertMono)
			ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
			ON_COMMAND(ID_EDIT_SNAPTOZERO, OnEditSnapToZero)
			ON_COMMAND(ID_PASTE_OVERWRITE, OnPasteOverwrite)
			ON_COMMAND(ID_PASTE_MIX, OnPasteMix)
			ON_COMMAND(ID_PASTE_CROSSFADE, OnPasteCrossfade)
			ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
			ON_UPDATE_COMMAND_UI(ID_POPUP_COPY, OnUpdateEditCopy)
			ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
			ON_UPDATE_COMMAND_UI(ID_POPUP_CUT, OnUpdateEditCut)
			ON_UPDATE_COMMAND_UI(ID_EDIT_CROP, OnUpdateEditCrop)
			ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
			ON_UPDATE_COMMAND_UI(ID_POPUP_PASTE, OnUpdateEditPaste)
			ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
			ON_UPDATE_COMMAND_UI(ID_POPUP_DELETE, OnUpdateEditDelete)
			ON_UPDATE_COMMAND_UI(ID_CONVERT_MONO, OnUpdateConvertMono)
			ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
			ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
			ON_UPDATE_COMMAND_UI(ID_EDIT_SNAPTOZERO, OnUpdateEditSnapToZero)
			ON_UPDATE_COMMAND_UI(ID_PASTE_OVERWRITE, OnUpdatePasteOverwrite)
			ON_UPDATE_COMMAND_UI(ID_PASTE_CROSSFADE, OnUpdatePasteCrossfade)
			ON_UPDATE_COMMAND_UI(ID_PASTE_MIX, OnUpdatePasteMix)
			ON_UPDATE_COMMAND_UI(ID_POPUP_SETLOOPSTART, OnUpdateSetLoopStart)
			ON_UPDATE_COMMAND_UI(ID_POPUP_SETLOOPEND, OnUpdateSetLoopEnd)
			ON_UPDATE_COMMAND_UI(ID_POPUP_SELECTIONTOLOOP, OnUpdateSelectionToLoop)
			ON_UPDATE_COMMAND_UI(ID__SETSUSTANTOSELECTION, OnUpdateSelectionSustain)
			ON_UPDATE_COMMAND_UI(ID__UNSETLOOP, OnUpdateUnsetLoop)
			ON_UPDATE_COMMAND_UI(ID__UNSETSUSTAINLOOP, OnUpdateUnsetSustain)
			ON_COMMAND(ID_POPUP_SELECTIONTOLOOP, OnPopupSelectionToLoop)
			ON_COMMAND(ID__SETSUSTANTOSELECTION, OnPopupSelectionSustain)
			ON_COMMAND(ID__UNSETLOOP, OnPopupUnsetLoop)
			ON_COMMAND(ID__UNSETSUSTAINLOOP, OnPopupUnsetSustain)
			ON_COMMAND(ID_POPUP_ZOOMIN, OnPopupZoomIn)
			ON_COMMAND(ID_POPUP_ZOOMOUT, OnPopupZoomOut)
			ON_COMMAND(ID_VISUALREPRESENTATION_SAMPLEHOLD, OnViewSampleHold)
			ON_COMMAND(ID_VISUALREPRESENTATION_SPLINE, OnViewSpline)
			ON_COMMAND(ID_VISUALREPRESENTATION_SINC, OnViewSinc)
			ON_UPDATE_COMMAND_UI(ID_VISUALREPRESENTATION_SAMPLEHOLD, OnUpdateViewSampleHold)
			ON_UPDATE_COMMAND_UI(ID_VISUALREPRESENTATION_SPLINE, OnUpdateViewSpline)
			ON_UPDATE_COMMAND_UI(ID_VISUALREPRESENTATION_SINC, OnUpdateViewSinc)

			ON_WM_DESTROYCLIPBOARD()
			ON_WM_CREATE()
			ON_WM_DESTROY()
			ON_WM_HSCROLL()
			ON_WM_SIZE()
			ON_BN_CLICKED(IDC_ZOOMIN, OnSelectionZoomIn)
			ON_BN_CLICKED(IDC_ZOOMOUT, OnSelectionZoomOut)
			ON_WM_TIMER()
			ON_WM_CONTEXTMENU()
			ON_NOTIFY(NM_CUSTOMDRAW, IDC_VOLSLIDER, OnCustomdrawVolSlider)

		END_MESSAGE_MAP()
*/

		/////////////////////////////////////////////////////////////////////////////
		// CWaveEdChildView drawing

		void CWaveEdChildView::OnPaint(void)
		{
			CPaintDC dc(this);
			CPaintDC *pDC = &dc;

			int wrHeight = 0;
			int wrHeadHeight=0;
			long c;

			int cyHScroll = GetSystemMetrics(SM_CYHSCROLL);
			CSingleLock lock(&semaphore, TRUE);
			if(wdWave)
			{
				CRect invalidRect;
				pDC->GetClipBox(&invalidRect);
				CRect rect;
				GetClientRect(&rect);

				CBitmap* bmpBuffer = new CBitmap;
				CBitmap* oldbmp;
				bmpBuffer->CreateCompatibleBitmap(pDC, invalidRect.Width(), invalidRect.Height());
				CDC memDC;
				
				memDC.CreateCompatibleDC(pDC);
				oldbmp = memDC.SelectObject(bmpBuffer);
				memDC.SetWindowOrg(invalidRect.left, invalidRect.top);
				
				int const nHeadHeight=helpers::math::round<int,float>(rect.Height()*0.1f);
				int const nWidth=rect.Width();
				int const nHeight=rect.Height()-cyHScroll-nHeadHeight;

				int const my =		helpers::math::round<int,float>(nHeight*0.5f);
				int const myHead =	helpers::math::round<int,float>(nHeadHeight*0.5f);

				if(wdStereo)
				{
					wrHeight =		helpers::math::round<int,float>(my*0.5f);
					wrHeadHeight =	helpers::math::round<int,float>(myHead*0.5f);
				}
				else 
				{
					wrHeight=my;
					wrHeadHeight=myHead;
				}

				//ratios used to convert from sample indices to pixels
				float dispRatio = nWidth/(float)diLength;
				float headDispRatio = nWidth/(float)wdLength;

				// Draw preliminary stuff
				CPen *oldpen= memDC.SelectObject(&cpen_me);
				
				// Left channel 0 amplitude line
				memDC.MoveTo(0,wrHeight+nHeadHeight);
				memDC.LineTo(nWidth,wrHeight+nHeadHeight);
				// Left Header 0 amplitude line
				memDC.MoveTo(0,wrHeadHeight);
				memDC.LineTo(nWidth,wrHeadHeight);
				
				int const wrHeight_R = my + wrHeight;
				int const wrHeadHeight_R = myHead + wrHeadHeight;
				
				memDC.SelectObject(&cpen_white);
				// Header/Body divider
				memDC.MoveTo(0, nHeadHeight);
				memDC.LineTo(nWidth, nHeadHeight);
				if(wdStereo)
				{
					// Stereo channels separator line
					memDC.SelectObject(&cpen_lo);
					memDC.MoveTo(0,my+nHeadHeight);
					memDC.LineTo(nWidth,my+nHeadHeight);
					// Stereo channels Header Separator
					memDC.MoveTo(0, myHead);
					memDC.LineTo(nWidth,myHead);
					
					// Right channel 0 amplitude line
					memDC.SelectObject(&cpen_me);
					memDC.MoveTo(0,wrHeight_R+nHeadHeight);
					memDC.LineTo(nWidth,wrHeight_R+nHeadHeight);
					// Right Header 0 amplitude line
					memDC.MoveTo(0,wrHeadHeight_R);
					memDC.LineTo(nWidth, wrHeadHeight_R);

				}
				
				// Draw samples in channels (Fideloop's)

				memDC.SelectObject(&cpen_hi);

				for(c = 0; c < nWidth; c++)
				{
					memDC.MoveTo(c,wrHeight - lDisplay.at(c).first + nHeadHeight);
					memDC.LineTo(c,wrHeight - lDisplay.at(c).second + nHeadHeight);
					memDC.MoveTo(c, wrHeadHeight-lHeadDisplay.at(c).first);
					memDC.LineTo(c, wrHeadHeight-lHeadDisplay.at(c).second );
				}

				if(wdStereo)
				{
					//draw right channel wave data
					for(c = 0; c < nWidth; c++)
					{
						memDC.MoveTo(c,wrHeight_R - rDisplay.at(c).first + nHeadHeight);
						memDC.LineTo(c,wrHeight_R - rDisplay.at(c).second + nHeadHeight);
						memDC.MoveTo(c, wrHeadHeight_R-rHeadDisplay.at(c).first);
						memDC.LineTo(c, wrHeadHeight_R-rHeadDisplay.at(c).second );
					}
				}

				//draw loop points
				if ( wdLoop )
				{
					HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
					HGDIOBJ oldfont = memDC.SelectObject(hFont);
					memDC.SelectObject(&cpen_lo);
					if ( wdLoopS >= diStart && wdLoopS < diStart+diLength)
					{
						int ls = helpers::math::round<int,float>((wdLoopS-diStart)*dispRatio);
						memDC.MoveTo(ls,nHeadHeight);
						memDC.LineTo(ls,nHeight+nHeadHeight);
						CSize textsize = memDC.GetTextExtent("Start");
						memDC.TextOut( ls - textsize.cx/2, nHeadHeight, "Start" );
					}
					if ( wdLoopE >= diStart && wdLoopE < diStart+diLength)
					{
						int le = helpers::math::round<int,float>((wdLoopE-diStart)*dispRatio);
						memDC.MoveTo(le,nHeadHeight);
						memDC.LineTo(le,nHeight+nHeadHeight);
						CSize textsize = memDC.GetTextExtent("End");
						memDC.TextOut( le - textsize.cx/2, nHeight+nHeadHeight-textsize.cy, "End" );
					}

					//draw loop points in header
					int ls = helpers::math::round<int,float>(wdLoopS * headDispRatio);
					int le = helpers::math::round<int,float>(wdLoopE * headDispRatio);

					memDC.MoveTo(ls, 0);
					memDC.LineTo(ls, nHeadHeight);

					memDC.MoveTo(le, 0);
					memDC.LineTo(le, nHeadHeight);
					memDC.SelectObject(oldfont);
				}
				//draw sustain loop points
				if ( wdSusLoop )
				{
					HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
					HGDIOBJ oldfont = memDC.SelectObject(hFont);
					memDC.SelectObject(&cpen_sus);
					if ( wdSusLoopS >= diStart && wdSusLoopS < diStart+diLength)
					{
						int ls = helpers::math::round<int,float>((wdSusLoopS-diStart)*dispRatio);
						memDC.SetBkColor(0x00000000);
						memDC.MoveTo(ls,nHeadHeight);
						memDC.LineTo(ls,nHeight+nHeadHeight);
						CSize textsize = memDC.GetTextExtent("Start");
						memDC.SetBkColor(0x00FFFFFF);
						memDC.TextOut( ls - textsize.cx/2, nHeadHeight, "Start" );
					}
					if ( wdSusLoopE >= diStart && wdSusLoopE < diStart+diLength)
					{
						int le = helpers::math::round<int,float>((wdSusLoopE-diStart)*dispRatio);
						memDC.SetBkColor(0x00000000);
						memDC.MoveTo(le,nHeadHeight);
						memDC.LineTo(le,nHeight+nHeadHeight);
						CSize textsize = memDC.GetTextExtent("End");
						memDC.SetBkColor(0x00FFFFFF);
						memDC.TextOut( le - textsize.cx/2, nHeight+nHeadHeight-textsize.cy, "End" );
					}

					//draw loop points in header
					int ls = helpers::math::round<int,float>(wdSusLoopS * headDispRatio);
					int le = helpers::math::round<int,float>(wdSusLoopE * headDispRatio);

					memDC.MoveTo(ls, 0);
					memDC.LineTo(ls, nHeadHeight);

					memDC.MoveTo(le, 0);
					memDC.LineTo(le, nHeadHeight);
					memDC.SelectObject(oldfont);
				}

				//draw screen size on header
				memDC.SelectObject(&cpen_white);
				int screenx  = helpers::math::round<int,float>( diStart           * headDispRatio);
				int screenx2 = helpers::math::round<int,float>((diStart+diLength) * headDispRatio);
				memDC.MoveTo(screenx, 0);
				memDC.LineTo(screenx, nHeadHeight-1);
				memDC.LineTo(screenx2,nHeadHeight-1);
				memDC.LineTo(screenx2,0);
				memDC.LineTo(screenx, 0);

				memDC.SelectObject(oldpen);

				memDC.SetROP2(R2_NOT);

				if(cursorBlink	&&	cursorPos >= diStart	&&	cursorPos <= diStart+diLength)
				{
					int cursorX = helpers::math::round<int,float>((cursorPos-diStart)*dispRatio);
					memDC.MoveTo(cursorX, nHeadHeight);
					memDC.LineTo(cursorX, nHeadHeight+nHeight);
				}
				if(_pSong->wavprev.IsEnabled()) {
					int cursorX = helpers::math::round<int,float>((_pSong->wavprev.GetPosition()-diStart)*dispRatio);
					memDC.MoveTo(cursorX, nHeadHeight);
					memDC.LineTo(cursorX, nHeadHeight+nHeight);
				}
				unsigned long selx, selx2;
				selx =blStart;
				selx2=blStart+blLength;

				int HeadSelX = helpers::math::round<int,float>(selx * headDispRatio);
				int HeadSelX2= helpers::math::round<int,float>(selx2* headDispRatio);
				memDC.Rectangle(HeadSelX,0,		HeadSelX2,nHeadHeight);

				if(selx<diStart) selx=diStart;
				if(selx2>diStart+diLength) selx2=diStart+diLength;
				//if the selected block is entirely off the screen, the above statements will flip the order
				if(selx<selx2)					//if not, it will just clip the drawing
				{
					selx = helpers::math::round<int,float>((selx -diStart)*dispRatio) ;
					selx2= helpers::math::round<int,float>((selx2-diStart)*dispRatio) ;
					memDC.Rectangle(selx,nHeadHeight,selx2,nHeight+nHeadHeight);
				}

				pDC->BitBlt(invalidRect.left, invalidRect.top, invalidRect.Width(), invalidRect.Height(),
							&memDC, invalidRect.left, invalidRect.top, SRCCOPY);
				memDC.SelectObject(oldbmp);
				memDC.DeleteDC();
				bmpBuffer->DeleteObject();
				delete bmpBuffer;

			}
			else
			{
				HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
				HGDIOBJ oldfont = pDC->SelectObject(hFont);
				pDC->TextOut(4,4,"No Wave Data");
				pDC->SelectObject(oldfont);

			}
			
			// Do not call CWnd::OnPaint() for painting messages
		}




		/////////////////////////////////////////////////////////////////////////////
		// CWaveEdChildView message handlers


		BOOL CWaveEdChildView::PreCreateWindow(CREATESTRUCT& cs) 
		{
			if (!CWnd::PreCreateWindow(cs)) return false;

			cs.dwExStyle |= WS_EX_CLIENTEDGE;
			cs.style &= ~WS_BORDER;
			cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
			::LoadCursor(NULL, IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH), NULL);

			return true;
			
		//	return CWnd::PreCreateWindow(cs);
		}

		int CWaveEdChildView::OnCreate(LPCREATESTRUCT lpCreateStruct)
		{
			if (CWnd::OnCreate(lpCreateStruct) == -1)
				return -1;

			cpen_lo.CreatePen(PS_SOLID,0,0xFF0000);
			cpen_sus.CreatePen(PS_DOT,0,0xFF0000);
			cpen_me.CreatePen(PS_SOLID,0,0xCCCCCC);
			cpen_hi.CreatePen(PS_SOLID,0,0x00DD77);
			cpen_white.CreatePen(PS_SOLID,0,0xEEEEEE);

			zoombar.Create(this, IDD_WAVED_ZOOMBAR, CBRS_BOTTOM | WS_CHILD, AFX_IDW_DIALOGBAR);
			//set volume slider
			CSliderCtrl* volSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_VOLSLIDER));
			volSlider->SetRange(0, 100);
			volSlider->SetPos(100);

			hResizeLR = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
			hIBeam = AfxGetApp()->LoadStandardCursor(IDC_IBEAM);

			bSnapToZero=false;
			bDragLoopStart = bDragLoopEnd = false;
			bDragSusLoopStart = bDragSusLoopEnd = false;
			SelStart=0;
			cursorPos=0;
			wdWave=false;
			wsInstrument=-1;
			prevHeadLoopS = prevBodyLoopS = prevHeadLoopE = prevBodyLoopE = 0;
			prevBodyX = prevHeadX = 0;

			return 0;
		}

		void CWaveEdChildView::OnDestroy()
		{
			StopTimer();
			zoombar.DestroyWindow();
			cpen_lo.DeleteObject();
			cpen_sus.DeleteObject();
			cpen_me.DeleteObject();
			cpen_hi.DeleteObject();
			cpen_white.DeleteObject();
			CWnd::OnDestroy();
		}


		//todo: use some sort of multiplier to prevent scrolling from being limited for waves with more samples than can be expressed
		//		in a signed 32-bit int.
		void CWaveEdChildView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
		{
			CRect client;
			GetClientRect(&client);
			int delta;
			int nWidth;

			CScrollBar* hScroll = static_cast<CScrollBar*>(zoombar.GetDlgItem(IDC_HSCROLL));
			CSliderCtrl* zoomSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_ZOOMSLIDE));
			CSliderCtrl* volSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_VOLSLIDER));
			
			if(pScrollBar == hScroll)
			{
				switch (nSBCode)
				{
				case SB_LEFT:
					diStart=0;
					break;
				case SB_RIGHT:
					diStart=wdLength-1;
					break;
				case SB_ENDSCROLL:
					break;
				case SB_THUMBTRACK:
				case SB_THUMBPOSITION:
					SCROLLINFO si;
					hScroll->GetScrollInfo(&si, SIF_TRACKPOS|SIF_POS);	//this is necessary because the nPos arg to OnHScroll is
					diStart = si.nTrackPos;								//transparently restricted to 16 bits
					break;
				case SB_LINELEFT:
					nWidth=client.Width();					// n samps / 20 pix  =  diLength samps / nWidth pix
					delta = 20 * diLength/nWidth;			//	n = 20 * diLength/nWidth
					if(delta<1) delta=1;			//in case we're zoomed in reeeally far
					diStart -= delta;
					break;
				case SB_LINERIGHT:
					nWidth=client.Width();
					delta = 20 * diLength/nWidth;
					if(delta<1) delta=1;
					diStart += delta;
					break;
				case SB_PAGELEFT:
					diStart -= diLength;
					break;
				case SB_PAGERIGHT:
					diStart+= diLength;
					break;
				}

				hScroll->SetScrollPos(diStart);
				diStart = hScroll->GetScrollPos();		//petzold's let-windows-do-the-boundary-checking method

				RefreshDisplayData();
				client.bottom -= GetSystemMetrics(SM_CYHSCROLL);
				InvalidateRect(&client, false);
			}
			else if((CSliderCtrl*)pScrollBar == zoomSlider)
			{
				int newzoom = zoomSlider->GetPos();
				SetSpecificZoom(newzoom);
				this->SetFocus();
			}
			else if((CSliderCtrl*)pScrollBar == volSlider)
			{
				volSlider->Invalidate(false);
				this->SetFocus();
			}
			else
				MessageBox("Unexpected scroll bar");
			
			CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
		}

		void CWaveEdChildView::OnSize(UINT nType, int cx, int cy)
		{
			CWnd::OnSize(nType, cx, cy);

			int cyZoombar = GetSystemMetrics(SM_CYHSCROLL);

			int clientcy = cy - cyZoombar;

			RefreshDisplayData(true);

			zoombar.MoveWindow(0, clientcy, cx, cyZoombar);

			CButton* cb;
			cb=(CButton *)zoombar.GetDlgItem(IDC_ZOOMIN);
			cb->SetWindowPos(NULL, cx-20, 0, 15, cyZoombar, SWP_NOZORDER);
			cb=(CButton *)zoombar.GetDlgItem(IDC_ZOOMOUT);
			cb->SetWindowPos(NULL, cx-115, 0, 15, cyZoombar, SWP_NOZORDER);

			CSliderCtrl* cs;
			cs = (CSliderCtrl*)zoombar.GetDlgItem(IDC_ZOOMSLIDE);
			cs->SetWindowPos(NULL, cx-100, 0, 80, cyZoombar, SWP_NOZORDER);

			cs = (CSliderCtrl*)zoombar.GetDlgItem(IDC_VOLSLIDER);
			cs->SetWindowPos(NULL, 0, 0, 75, cyZoombar, SWP_NOZORDER);

			CScrollBar *csb;
			csb = (CScrollBar*)zoombar.GetDlgItem(IDC_HSCROLL);
			
			csb->SetWindowPos(NULL, 75, 0, cx-190, cyZoombar, SWP_NOZORDER);
			
		}
		void CWaveEdChildView::StartTimer()
		{
			UINT refreshTime = GetCaretBlinkTime();
			if(refreshTime> 0 && refreshTime != INFINITE)
			{
				SetTimer(ID_TIMER_WAVED_REFRESH, refreshTime, 0);
			}
			else {	
				SetTimer(ID_TIMER_WAVED_REFRESH, 750, 0);
			}
		}
		void CWaveEdChildView::StopTimer()
		{
			KillTimer(ID_TIMER_WAVED_REFRESH);
			KillTimer(ID_TIMER_WAVED_PLAYING);
		}

		void CWaveEdChildView::OnTimer(UINT_PTR nIDEvent)
		{
			if(nIDEvent==ID_TIMER_WAVED_REFRESH)
			{
				cursorBlink = !cursorBlink;
				CRect rect;
				GetClientRect(&rect);
				rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
				InvalidateRect(&rect, false);
			}
			if(nIDEvent==ID_TIMER_WAVED_PLAYING)
			{
				CRect rect;
				GetClientRect(&rect);
				rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
				InvalidateRect(&rect, false);
				if (!_pSong->wavprev.IsEnabled()) {
					KillTimer(ID_TIMER_WAVED_PLAYING);
				}
			}
			CWnd::OnTimer(nIDEvent);
		}

		void CWaveEdChildView::OnContextMenu(CWnd* pWnd, CPoint point)
		{
			if (contextmenu) {
				CMenu menu;
				menu.LoadMenu(IDR_WAVED_POPUP);
				CMenu *popup;
				popup=menu.GetSubMenu(0);
				assert(popup);
				popup->TrackPopupMenu(TPM_LEFTALIGN|TPM_RIGHTBUTTON, point.x, point.y, GetOwner());
				menu.DestroyMenu();

				CRect rect;
				GetWindowRect(&rect);
				rbX = point.x-rect.left;
				rbY = point.y-rect.top;
			}
		}

		
		void CWaveEdChildView::OnCustomdrawVolSlider(NMHDR* pNMHDR, LRESULT* pResult)
		{
/*
			NMCUSTOMDRAW nmcd = *(LPNMCUSTOMDRAW)pNMHDR;

			if ( nmcd.dwDrawStage == CDDS_PREPAINT )
			{
				CSliderCtrl* volSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_VOLSLIDER));
				float vol = volSlider->GetPos()/100.f;
				CDC* pDC = CDC::FromHandle( nmcd.hdc );
				CDC memDC;
				memDC.CreateCompatibleDC(pDC);
				CRect rc;
				zoombar.GetWindowRect(&rc);
				rc.bottom -=rc.top;
				rc.top=0;
				rc.left=0;
				rc.right=74;

				CBitmap *bmpBuf = new CBitmap;
				CBitmap *oldBmp;
				bmpBuf->CreateCompatibleBitmap(pDC, rc.Width(), rc.Height());
				oldBmp = memDC.SelectObject(bmpBuf);

				CBrush blueBrush;
				CBrush *oldBrush;
				blueBrush.CreateSolidBrush( RGB(0,128,200) );

				oldBrush = (CBrush*)memDC.SelectStockObject(BLACK_BRUSH);
				memDC.Rectangle(&rc);

				memDC.SelectStockObject(DKGRAY_BRUSH);
				memDC.Rectangle(rc.left+7, rc.top+2, rc.right-7, rc.bottom-2);

				memDC.SelectObject( &blueBrush );
				memDC.Rectangle(	rc.left+7,
								rc.top+2,
								rc.left+7+helpers::math::round<int,float>( vol*(rc.right-rc.left-14) ),
								rc.bottom-2);
				
				memDC.SelectStockObject(BLACK_BRUSH);
				CPoint points[3];
				points[0].x = rc.left+5;			points[0].y = rc.bottom-6;
				points[1].x = rc.right-2;		points[1].y = rc.top+1;
				points[2].x = rc.left+5;			points[2].y = rc.top+1;
				memDC.Polygon(points, 3);

				pDC->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &memDC, rc.left, rc.top, SRCCOPY);

				pDC->Detach();
				memDC.SelectObject(oldBrush);
				memDC.SelectObject(oldBmp);
				memDC.DeleteDC();
				bmpBuf->DeleteObject();
				delete bmpBuf;
				*pResult = CDRF_SKIPDEFAULT;
			}
*/
		}


		void CWaveEdChildView::GenerateAndShow()
		{
			blSelection=false;
			UpdateWindow();
		}

		void  CWaveEdChildView::SetViewData(int ins)
		{
			int wl=0;
			if (_pSong->samples.IsEnabled(ins)) {wl = _pSong->samples[ins].WaveLength();}

			wsInstrument=ins;	// Do not put inside of "if(wl)". Pasting needs this.

			if(wl)
			{
				wdWave=true;
				const XMInstrument::WaveData<>& wave = _pSong->samples[wsInstrument];
					
				wdLength=wl;
				wdLeft=wave.pWaveDataL();
				wdRight=wave.pWaveDataR();
				wdStereo=wave.IsWaveStereo();
				wdLoop=wave.WaveLoopType()!=XMInstrument::WaveData<>::LoopType::DO_NOT;
				wdLoopS=wave.WaveLoopStart();
				wdLoopE=wave.WaveLoopEnd();
				wdSusLoop=wave.WaveSusLoopType()!=XMInstrument::WaveData<>::LoopType::DO_NOT;
				wdSusLoopS=wave.WaveSusLoopStart();
				wdSusLoopE=wave.WaveSusLoopEnd();

				diStart=0;
				diLength=wl;
				blStart=0;
				blLength=0;

				ResetScrollBars(true);
				RefreshDisplayData(true);
				Invalidate();
			}
			else
			{
				wdWave=false;

				ResetScrollBars(true);
				RefreshDisplayData(true);
				SetWindowText("Wave Editor [No Data]");
				Invalidate(true);
			}

			_pSong->SetWavPreview(wsInstrument);
			blSelection=false;
		}



		//////////////////////////////////////////////////////////////////////////
		//////		Zoom Functions



		void CWaveEdChildView::OnSelectionZoomIn()
		{
			if(wdWave && wdLength>8)
			{
				if(diLength>=12)
					diLength = diLength*2/3;
				else
					diLength=8;

				if(cursorPos<diLength/2)
					diStart=0;
				else if(cursorPos+diLength/2>wdLength)
					diStart=wdLength-diLength;
				else
					diStart=cursorPos-diLength/2;
			}

			ResetScrollBars();
			RefreshDisplayData();
			Invalidate(false);

		}

		void CWaveEdChildView::OnPopupZoomIn()
		{
			CRect rect;
			GetClientRect(&rect);
			int nWidth = rect.Width();
			unsigned long newCenter = diStart + rbX*diLength/nWidth;
			if(wdWave && wdLength>8)
			{
				if(diLength>=12)
					diLength = diLength*2/3;
				else diLength=8;

				if(newCenter<diLength/2)
					diStart=0;
				else if(newCenter+diLength/2>wdLength)
					diStart=wdLength-diLength;
				else
					diStart=newCenter-diLength/2;
			}

			ResetScrollBars();
			RefreshDisplayData();
			Invalidate(false);
		}

		void CWaveEdChildView::OnSelectionZoomOut()
		{
			if(wdWave && diLength<wdLength)
			{
				diLength=diLength*3/2;

				if(diLength>wdLength) diLength=wdLength;

				if(cursorPos<diLength/2)	//cursorPos is unsigned, so we can't just set it and check if it's < 0
					diStart=0;
				else if(cursorPos+diLength/2>wdLength)
					diStart=wdLength-diLength;
				else
					diStart = cursorPos-diLength/2;

				ResetScrollBars();
				RefreshDisplayData();
				Invalidate(false);
			}
		}

		void CWaveEdChildView::OnPopupZoomOut()
		{
			CRect rect;
			GetClientRect(&rect);
			int nWidth = rect.Width();
			unsigned long newCenter = diStart + rbX*diLength/nWidth;
			if(wdWave && diLength<wdLength)
			{
				diLength=diLength*3/2;

				if(diLength>wdLength) diLength=wdLength;

				if(newCenter<diLength/2)
					diStart=0;
				else if(newCenter+diLength/2>wdLength)
					diStart=wdLength-diLength;
				else
					diStart=newCenter-diLength/2;
			}

			ResetScrollBars();
			RefreshDisplayData();
			Invalidate(false);
		}



		void CWaveEdChildView::OnSelectionZoomSel()
		{
			if(blSelection && wdWave)
			{
				diStart = blStart;
				diLength = blLength;
				if(diLength<8)
				{
					unsigned long diCenter = diStart+diLength/2;
					diLength=8;
					if(diCenter<diLength/2)
						diStart=0;
					else
						diStart = diCenter-diLength/2;
				}
				if(diLength+diStart>=wdLength) //???
					diLength=wdLength-diStart;

				ResetScrollBars();
				RefreshDisplayData();
				Invalidate(false);
			}
		}

		void CWaveEdChildView::SetSpecificZoom(int factor)
		{
			float ratio = 1 / (float)pow(zoomBase, factor);
			int newLength=wdLength*ratio;
			if(newLength>=8)
			{
				diLength=wdLength*ratio;

				if(diLength>wdLength) diLength=wdLength;

				if(cursorPos<diLength/2)
					diStart=0;
				else if(cursorPos+diLength/2 > wdLength)
					diStart=wdLength-diLength;
				else
					diStart=cursorPos-diLength/2;

				ResetScrollBars();
				RefreshDisplayData();
				Invalidate(false);
			}
		}

		void CWaveEdChildView::OnSelectionShowall() 
		{
			diStart = 0;
			diLength = wdLength;

			ResetScrollBars(false);
			RefreshDisplayData();
			Invalidate(true);
		}


		//////////////////////////////////////////////////////////////////////////
		//////		Mouse event handlers


		void CWaveEdChildView::OnRButtonDown(UINT nFlags, CPoint point) 
		{
			if(wdWave)
			{
				int const x=point.x;

				if ( nFlags & MK_CONTROL )
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					_pSong->StopInstrument(wsInstrument);

					CRect rect;
					GetClientRect(&rect);
					wdLoopE = diStart+((x*diLength)/rect.Width());
					wave.WaveLoopEnd(wdLoopE);
					if (wave.WaveLoopStart()> wdLoopE )
					{
						wave.WaveLoopStart(wdLoopE);
					}
					wdLoopS = wave.WaveLoopStart();
					if (!wdLoop) 
					{
						wdLoop=true;
						wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
					}
					mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
					rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
					InvalidateRect(&rect, false);
					contextmenu=false;
				}
				else if ( nFlags & MK_SHIFT )
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					_pSong->StopInstrument(wsInstrument);

					CRect rect;
					GetClientRect(&rect);
					wdSusLoopE = diStart+((x*diLength)/rect.Width());
					wave.WaveSusLoopEnd(wdSusLoopE);
					if (wave.WaveSusLoopStart()> wdSusLoopE )
					{
						wave.WaveSusLoopStart(wdSusLoopE);
					}
					wdSusLoopS = wave.WaveSusLoopStart();
					if (!wdSusLoop) 
					{
						wdSusLoop=true;
						wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
					}
					mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
					rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
					InvalidateRect(&rect, false);
					contextmenu=false;
				}
				else
				{
					contextmenu=true;
				}

			}
			CWnd::OnRButtonDown(nFlags, point);
		}


		void CWaveEdChildView::OnLButtonDown(UINT nFlags, CPoint point) 
		{
			SetCapture();
			if(wdWave)
			{
				int const x=point.x;
				int const y=point.y;

				if ( nFlags & MK_CONTROL )
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					_pSong->StopInstrument(wsInstrument);


					CRect rect;
					GetClientRect(&rect);
					wdLoopS = diStart+((x*diLength)/rect.Width());
					wave.WaveLoopStart(wdLoopS);
					if (wave.WaveLoopEnd() < wdLoopS )
					{
						wave.WaveLoopEnd(wdLoopS);
					}
					wdLoopE = wave.WaveLoopEnd();

					if (!wdLoop) 
					{
						wdLoop=true;
						wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
					}
					mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
					rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
					InvalidateRect(&rect, false);
				}
				else if ( nFlags & MK_SHIFT )
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					_pSong->StopInstrument(wsInstrument);


					CRect rect;
					GetClientRect(&rect);
					wdSusLoopS = diStart+((x*diLength)/rect.Width());
					wave.WaveSusLoopStart(wdSusLoopS);
					if (wave.WaveSusLoopEnd() < wdSusLoopS )
					{
						wave.WaveSusLoopEnd(wdSusLoopS);
					}
					wdSusLoopE = wave.WaveSusLoopEnd();

					if (!wdSusLoop) 
					{
						wdSusLoop=true;
						wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
					}
					mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
					rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
					InvalidateRect(&rect, false);
				}
				else if ( nFlags == MK_LBUTTON )
				{
					CRect rect;
					GetClientRect(&rect);
					int const nWidth=rect.Width();
					int const nHeadHeight = rect.Height()/10;


					if(y>nHeadHeight && diLength!=0)		//we're clicking on the main body
					{
						float dispRatio = nWidth/float(diLength);
						if		( blSelection	&&
								abs(x - helpers::math::round<int,float>((blStart-diStart)			* dispRatio )) < 10 )	//mouse down on block start
						{
							SelStart = blStart+blLength;				//set SelStart to the end we're -not- moving
							cursorPos=blStart;
						}
						else if ( blSelection	&&
								abs(x - helpers::math::round<int,float>((blStart+blLength-diStart)	* dispRatio )) < 10 )	//mouse down on block end
						{
							SelStart=blStart;							//set SelStart to the end we're -not- moving
							cursorPos=blStart+blLength;
						}
						else if ( wdLoop		&&
								abs(x - helpers::math::round<int,float>((wdLoopS-diStart)			* dispRatio )) < 10 )	//mouse down on loop start
						{
							bDragLoopStart=true;
						}
						else if ( wdLoop		&&
								abs(x - helpers::math::round<int,float>((wdLoopE-diStart)			* dispRatio )) < 10 )	//mouse down on loop end
						{
							bDragLoopEnd=true;
						}
						else if ( wdSusLoop		&&
								abs(x - helpers::math::round<int,float>((wdSusLoopS-diStart)			* dispRatio )) < 10 )	//mouse down on loop start
						{
							bDragSusLoopStart=true;
						}
						else if ( wdSusLoop		&&
								abs(x - helpers::math::round<int,float>((wdSusLoopE-diStart)			* dispRatio )) < 10 )	//mouse down on loop end
						{
							bDragSusLoopEnd=true;
						}
						else
						{
							blSelection=false;
							
							blStart=diStart+int(x*diLength/nWidth);
							blLength=0;
							SelStart = blStart;
							cursorPos = blStart;

						}
					}
					else					//we're clicking on the header
					{
						float headDispRatio = nWidth/float(wdLength);
						if		( blSelection		&&
								abs( x - helpers::math::round<int,float>( blStart				* headDispRatio ) ) < 10 )	//mouse down on block start
						{
							SelStart = blStart+blLength;
						}
						else if ( blSelection		&&
								abs( x - helpers::math::round<int,float>((blStart+blLength)	* headDispRatio ) ) < 10 )	//mouse down on block end
						{
							SelStart = blStart;
						}
						else
						{
							blSelection=false;
							
							blStart = helpers::math::round<int,float>(double((x*wdLength)/nWidth));
							blLength=0;
							SelStart = blStart;

						}
					}
					::SetCursor(hResizeLR);
					
					rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
					InvalidateRect(&rect, false);
				}
			}
			
			CWnd::OnLButtonDown(nFlags, point);
		}

		void CWaveEdChildView::OnLButtonDblClk( UINT nFlags, CPoint point )
		{
			OnEditSelectAll();
		}

		void CWaveEdChildView::OnMouseMove(UINT nFlags, CPoint point) //Fideloop's
		{
			int x=point.x;
			int y=point.y;
			CRect rect;
			GetClientRect(&rect);
			int const nWidth=rect.Width();
			int const nHeadHeight = rect.Height()/10;
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);

			if(nFlags == MK_LBUTTON && wdWave)
			{
				CRect invHead;
				CRect invBody;
				if(y>nHeadHeight)		//mouse is over body
				{
					float diRatio = (float) diLength/nWidth;
					unsigned long newpos =  (x*diRatio+diStart > 0? x*diRatio+diStart: 0);
					int headX = helpers::math::round<int,float>((diStart+x*diRatio)*nWidth/float(wdLength));
					if(bDragLoopStart)
					{
						if(newpos > wdLoopE)		wdLoopS = wdLoopE;
						else						wdLoopS = newpos;

						//set invalid rects
						float sampWidth = nWidth/(float)diLength+20;
						if(x<prevBodyLoopS)	invBody.SetRect(x-sampWidth, nHeadHeight,				prevBodyLoopS+sampWidth,	rect.Height());
						else				invBody.SetRect(prevBodyLoopS-sampWidth, nHeadHeight,	x+sampWidth,	rect.Height());
						if(headX<prevHeadLoopS)	invHead.SetRect(headX-20, 0,				prevHeadLoopS+20,	nHeadHeight);
						else					invHead.SetRect(prevHeadLoopS-20,0,			headX+20, nHeadHeight);
						prevBodyLoopS=x;	prevHeadLoopS=headX;
					}
					else if(bDragLoopEnd)
					{
						if(newpos >= wdLength)		wdLoopE = wdLength-1;
						else if(newpos >= wdLoopS)	wdLoopE = newpos;
						else						wdLoopE = wdLoopS;

						//set invalid rects
						float sampWidth = nWidth/(float)diLength + 20;
						if(x<prevBodyLoopE)	invBody.SetRect(x-sampWidth, nHeadHeight,				prevBodyLoopE+sampWidth,	rect.Height());
						else				invBody.SetRect(prevBodyLoopE-sampWidth, nHeadHeight,	x+sampWidth,	rect.Height());
						if(headX<prevHeadLoopE)	invHead.SetRect(headX-20, 0,				prevHeadLoopE+20,	nHeadHeight);
						else					invHead.SetRect(prevHeadLoopE-20,0,			headX+20, nHeadHeight);
						prevBodyLoopE=x;	prevHeadLoopE=headX;
					}
					else if(bDragSusLoopStart)
					{
						if(newpos > wdSusLoopE)		wdSusLoopS = wdSusLoopE;
						else						wdSusLoopS = newpos;

						//set invalid rects
						float sampWidth = nWidth/(float)diLength+20;
						if(x<prevBodyLoopS)	invBody.SetRect(x-sampWidth, nHeadHeight,				prevBodyLoopS+sampWidth,	rect.Height());
						else				invBody.SetRect(prevBodyLoopS-sampWidth, nHeadHeight,	x+sampWidth,	rect.Height());
						if(headX<prevHeadLoopS)	invHead.SetRect(headX-20, 0,				prevHeadLoopS+20,	nHeadHeight);
						else					invHead.SetRect(prevHeadLoopS-20,0,			headX+20, nHeadHeight);
						prevBodyLoopS=x;	prevHeadLoopS=headX;
					}
					else if(bDragSusLoopEnd)
					{
						if(newpos >= wdLength)		wdSusLoopE = wdLength-1;
						else if(newpos >= wdSusLoopS)	wdSusLoopE = newpos;
						else						wdSusLoopE = wdSusLoopS;

						//set invalid rects
						float sampWidth = nWidth/(float)diLength + 20;
						if(x<prevBodyLoopE)	invBody.SetRect(x-sampWidth, nHeadHeight,				prevBodyLoopE+sampWidth,	rect.Height());
						else				invBody.SetRect(prevBodyLoopE-sampWidth, nHeadHeight,	x+sampWidth,	rect.Height());
						if(headX<prevHeadLoopE)	invHead.SetRect(headX-20, 0,				prevHeadLoopE+20,	nHeadHeight);
						else					invHead.SetRect(prevHeadLoopE-20,0,			headX+20, nHeadHeight);
						prevBodyLoopE=x;	prevHeadLoopE=headX;
					}
					else
					{
						if (newpos >= SelStart)
						{
							if (newpos > wdLength)	{ newpos = wdLength; }
							blStart = SelStart;
							blLength = newpos - blStart;
							cursorPos=blStart+blLength;
						}
						else
						{
							if (newpos < 0) { newpos = 0; }
							blStart = newpos;
							blLength = SelStart - blStart;
							cursorPos=blStart;
						}
						//set invalid rects
						int sampWidth = nWidth/(float)diLength+1;
						if(x<prevBodyX)			invBody.SetRect(x-sampWidth, nHeadHeight,			prevBodyX+sampWidth,	rect.Height());	
						else					invBody.SetRect(prevBodyX-sampWidth, nHeadHeight,	x+sampWidth,			rect.Height());	
						if(headX<prevHeadX)		invHead.SetRect(headX-1, 0,					prevHeadX+1,	nHeadHeight);
						else					invHead.SetRect(prevHeadX-1, 0,				headX+1,		nHeadHeight);
						prevHeadX=headX;
						prevBodyX=x;
					}
				}
				else					//mouse is over header
				{
					float diRatio = (float) wdLength/nWidth;
					unsigned long newpos = (x * diRatio > 0? x*diRatio: 0);
					if (newpos >= SelStart)
					{
						if (newpos >= wdLength)	{ newpos = wdLength-1;	}
						blStart = SelStart;
						blLength = newpos - blStart;
					}
					else
					{
						blStart = newpos;
						blLength = SelStart-blStart;
					}
					//set invalid rects
					int bodyX = helpers::math::round<int,float>(double( (x*wdLength - diStart*nWidth)/diLength ));
					if(bodyX<0 || bodyX>nWidth)
						invBody.SetRectEmpty();
					else
						if(bodyX<prevBodyX)	invBody.SetRect(bodyX-1,		nHeadHeight,	prevBodyX+1,	rect.Height());
						else				invBody.SetRect(prevBodyX-1,	nHeadHeight,	bodyX+1,		rect.Height());

					if(x<prevHeadX)			invHead.SetRect(x-1, 0,					prevHeadX+1,	nHeadHeight);
					else					invHead.SetRect(prevHeadX-1, 0,			x+1,			nHeadHeight);
					prevBodyX=bodyX;
					prevHeadX=x;
				}
				blSelection=true;
				CRect invalid;
				invalid.UnionRect(&invBody, &invHead);
				InvalidateRect(&invalid, false);
			}
			else 
			{

				if(y>nHeadHeight && diLength!=0)		//mouse is over body
				{
					float dispRatio = nWidth/(float)diLength;
					if	(		blSelection		&&
							(	abs ( x - helpers::math::round<int,float>((  blStart-diStart )			* dispRatio ))  < 10		||
								abs ( x - helpers::math::round<int,float>((  blStart+blLength-diStart)	* dispRatio ))  < 10	)	||
							(	wdLoop &&
							(	abs ( x - helpers::math::round<int,float>((  wdLoopS-diStart )			* dispRatio ))  < 10		||
								abs ( x - helpers::math::round<int,float>((  wdLoopE-diStart )			* dispRatio ))  < 10) )		||
							(	wdSusLoop &&
							(	abs ( x - helpers::math::round<int,float>((  wdSusLoopS-diStart )			* dispRatio ))  < 10		||
								abs ( x - helpers::math::round<int,float>((  wdSusLoopE-diStart )			* dispRatio ))  < 10) )
						)
						::SetCursor(hResizeLR);
					else
						::SetCursor(hIBeam);
				}
				else if (wdLength!=0)					//mouse is over header
				{
					
					float dispRatio = nWidth/(float)wdLength;
					if (		blSelection		&&
							(	abs ( x - helpers::math::round<int,float>(   blStart			* dispRatio ))	< 10 ||
								abs ( x - helpers::math::round<int,float>((  blStart+blLength)	* dispRatio ))	< 10 )
						)
						::SetCursor(hResizeLR);
					else
						::SetCursor(hIBeam);
				}
			}
			
			
			CWnd::OnMouseMove(nFlags, point);
		}

		void CWaveEdChildView::OnLButtonUp(UINT nFlags, CPoint point) 
		{
			if(blLength==0)
				blSelection=false;
			if(bSnapToZero)
			{
				if(blSelection)
				{
					long delta = blStart - FindNearestZero(blStart);
					blStart-=delta;
					blLength+=delta;
					blLength = FindNearestZero(blStart+blLength) - blStart;
				}
				cursorPos = FindNearestZero(cursorPos);
			}
			ReleaseCapture();
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			_pSong->StopInstrument(wsInstrument);
			if(wdWave) {
				XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
				wave.WaveLoopStart(wdLoopS);
				wave.WaveLoopEnd(wdLoopE);
				wave.WaveSusLoopStart(wdSusLoopS);
				wave.WaveSusLoopEnd(wdSusLoopE);
				mainFrame->UpdateInstrumentEditor();
				if(_pSong->wavprev.IsEnabled()) {
					_pSong->wavprev.Play(notecommands::middleC,_pSong->wavprev.GetPosition());
				}
			}
			bDragLoopEnd = bDragLoopStart = false;
			bDragSusLoopEnd = bDragSusLoopStart = false;
			CRect rect;
			GetClientRect(&rect);
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
			CWnd::OnLButtonUp(nFlags, point);
		}



		//////////////////////////////////////////////////////////////////////////
		//////		Audio processing functions


		void CWaveEdChildView::OnSelectionFadeIn()
		{
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long length = (blSelection? blLength: wdLength);
			if(wdWave)
			{
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				PsycleGlobal::inputHandler().AddMacViewUndo();
				XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
				wave.Fade(startPoint, startPoint+length, 0.f, 1.f);

				RefreshDisplayData(true);
				Invalidate(true);
			}
		}


		void CWaveEdChildView::OnSelectionFadeOut()
		{
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long length = (blSelection? blLength: wdLength);
			if(wdWave)
			{
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				PsycleGlobal::inputHandler().AddMacViewUndo();
				XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
				wave.Fade(startPoint, startPoint+length, 1.f, 0.f);

				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnSelectionNormalize() // (Fideloop's)
		{
			signed short maxL = 0;
			double ratio = 0;
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long length = (blSelection? blLength: wdLength);

			if (wdWave)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();

				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				maxL = CalcBufferPeak(&wdLeft[startPoint], length);
				if (wdStereo) {
					short maxR = CalcBufferPeak(&wdRight[startPoint], length);
					if (maxL < maxR) maxL = maxR;
				}
				
				if (maxL) ratio = (double) 32767 / maxL;
				
				if (ratio != 1)
				{
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					wave.Amplify(startPoint, startPoint+length, ratio);
				}

				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnSelectionRemoveDC() // (Fideloop's)
		{
			double meanL = 0, meanR = 0;
			unsigned long c = 0;
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long length = (blSelection? blLength: wdLength);
			signed short buf;

			if (wdWave)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();

				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				for (c=startPoint; c<startPoint+length; c++)
				{
					meanL = meanL + ( (double) *(wdLeft+c) / length);

					if (wdStereo) meanR = (double) meanR + ((double) *(wdRight+c) / length);
				}

				for (c=startPoint; c<startPoint+length; c++)
				{
					buf = *(wdLeft+c);
					if (meanL > 0)
					{
						if ((double)(buf - meanL) < (-32768))	*(wdLeft+c) = -32768;
						else	*(wdLeft+c) = (short)(buf - meanL);
					}
					else if (meanL < 0)
					{
						if ((double)(buf - meanL) > 32767) *(wdLeft+c) = 32767;
						else *(wdLeft + c) = (short)(buf - meanL);
					}
				}
			
				if (wdStereo)
				{
					for (c=startPoint; c<startPoint+length; c++)
					{
						buf = *(wdRight+c);
						if (meanR > 0)
						{
							if ((double)(buf - meanR) < (-32768))	*(wdRight + c) = -32768;
							else	*(wdRight+c) = (short)(buf - meanR);
						}
						else if (meanR < 0)
						{
							if ((double)(buf - meanR) > 32767) *(wdRight+c) = 32767;
							else *(wdRight + c) = (short)(buf - meanR);
						}
					}
				}
				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnSelectionAmplify()
		{
			double ratio =1;
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long length = (blSelection? blLength: wdLength);
			int pos = 0;

			if (wdWave)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				CWaveEdAmplifyDialog AmpDialog(GetOwner());
				pos = AmpDialog.DoModal();
				if (pos != AMP_DIALOG_CANCEL)
				{
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					ratio = pow(10.0, (double) pos / (double) 2000.0);

					wave.Amplify(startPoint, startPoint+length, ratio);

					RefreshDisplayData(true);
					Invalidate(true);
				}
			}
		}

		void CWaveEdChildView::OnSelectionReverse() 
		{
			short buf = 0;
			int c, halved = 0;
			unsigned long startPoint = (blSelection? blStart: 0);
			unsigned long endpoint = (blSelection? blLength-1: wdLength-1);

			if (wdWave)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();

				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				//flip all samples, except if length is odd.
				halved = (int) floor((endpoint+1)/2.0f);

				for (c = 0; c < halved; c++)
				{
					buf = wdLeft[startPoint+endpoint - c];
					wdLeft[startPoint+endpoint - c] = wdLeft[startPoint + c];
					wdLeft[startPoint + c] = buf;

					if (wdStereo)
					{
						buf = wdRight[startPoint+endpoint - c];
						wdRight[startPoint+endpoint - c] = wdRight[startPoint + c];
						wdRight[startPoint + c] = buf;
					}

				}
				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnSelectionInsertSilence()
		{
			CWaveEdInsertSilenceDialog silenceDlg(GetOwner());
			if(silenceDlg.DoModal()==IDOK)
			{
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				unsigned long timeInSamps = Global::configuration()._pOutputDriver->GetSamplesPerSec() * silenceDlg.timeInSecs;
				if(!wdWave)
				{
					XMInstrument::WaveData<>& wave = _pSong->WavAlloc(wsInstrument, false, timeInSamps, "New Waveform");
					std::memset(wave.pWaveDataL(), 0, timeInSamps*sizeof(short) );
					wdLeft = wave.pWaveDataL();
					wdLength=timeInSamps;
					wdStereo=false;
					wdWave=true;
					ResetScrollBars(true);
					OnSelectionShowall();
				}
				else
				{
					unsigned long insertPos;
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);

					switch(silenceDlg.insertPos)
					{
					case CWaveEdInsertSilenceDialog::at_start:
						insertPos = 0;
						break;
					case CWaveEdInsertSilenceDialog::at_end:
						insertPos = wdLength-1;
						break;
					case CWaveEdInsertSilenceDialog::at_cursor:
						insertPos = cursorPos;
						break;
					default:
						MessageBox("Invalid option");
						return;
					}
					XMInstrument::WaveData<> wavewithS;
					wavewithS.AllocWaveData(timeInSamps, wave.IsWaveStereo());
					std::memset(wavewithS.pWaveDataL(), 0, timeInSamps*sizeof(short));					//insert silence
					wave.InsertAt(insertPos, wavewithS);
					wdLeft = wave.pWaveDataL();
					wdRight = wave.pWaveDataR();
					wdLength = wave.WaveLength();
					wavewithS.DeleteWaveData();

					if(wdLoop)		//update loop points if necessary
					{
						if(insertPos<wdLoopS)
						{
							wdLoopS += timeInSamps;
							wave.WaveLoopStart(wdLoopS);
						}
						if(insertPos<wdLoopE)
						{
							wdLoopE += timeInSamps;
							wave.WaveLoopEnd(wdLoopE);
						}
					}
					if(wdSusLoop)		//update loop points if necessary
					{
						if(insertPos<wdSusLoopS)
						{
							wdSusLoopS += timeInSamps;
							wave.WaveSusLoopStart(wdSusLoopS);
						}
						if(insertPos<wdSusLoopE)
						{
							wdSusLoopE += timeInSamps;
							wave.WaveSusLoopEnd(wdSusLoopE);
						}
					}
				}

				mainFrame->UpdateInstrumentEditor();
				mainFrame->WaveEditorBackUpdate();
				ResetScrollBars(true);
				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnConvertMono() 
		{
			if (wdWave && wdStereo)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
				wave.ConvertToMono();
				wdStereo = false;
				RefreshDisplayData(true);
				Invalidate(true);
			}
			mainFrame->UpdateInstrumentEditor();
			mainFrame->WaveEditorBackUpdate();
		}



		//////////////////////////////////////////////////////////////////////////
		//////		Menu Update Handlers

		void CWaveEdChildView::OnUpdateSelectionAmplify(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);
		}

		void CWaveEdChildView::OnUpdateSelectionReverse(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);
			
		}

		void CWaveEdChildView::OnUpdateSelectionFadein(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);
			
		}

		void CWaveEdChildView::OnUpdateSelectionFadeout(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);	
		}

		void CWaveEdChildView::OnUpdateSelectionNormalize(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);	
		}

		void CWaveEdChildView::OnUpdateSelectionRemovedc(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);	
		}

		void CWaveEdChildView::OnUpdateSeleccionZoomIn(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && (diLength > 16));
		}

		void CWaveEdChildView::OnUpdateSeleccionZoomOut(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && (diLength < wdLength) );
		}

		void CWaveEdChildView::OnUpdateSelectionZoomSel(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && blSelection);
		}

		void CWaveEdChildView::OnUpdateSelectionShowall(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && (diLength < wdLength) );		
		}

		void CWaveEdChildView::OnUpdateEditCopy(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && blSelection);
		}

		void CWaveEdChildView::OnUpdateEditCut(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && blSelection);
		}

		void CWaveEdChildView::OnUpdateEditCrop(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && blSelection);
		}

		void CWaveEdChildView::OnUpdateEditPaste(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable( IsClipboardFormatAvailable(CF_WAVE) );	
		}

		void CWaveEdChildView::OnUpdateEditDelete(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && blSelection);	
		}

		void CWaveEdChildView::OnUpdateConvertMono(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave && wdStereo);
		}

		void CWaveEdChildView::OnUpdateEditUndo(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(false);
		}

		void CWaveEdChildView::OnUpdateEditSelectAll(CCmdUI* pCmdUI) 
		{
			pCmdUI->Enable(wdWave);
		}

		void CWaveEdChildView::OnUpdateEditSnapToZero(CCmdUI* pCmdUI)
		{
			pCmdUI->SetCheck((bSnapToZero? 1: 0));
			pCmdUI->Enable();
		}

		void CWaveEdChildView::OnUpdatePasteOverwrite(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && IsClipboardFormatAvailable(CF_WAVE) );
		}

		void CWaveEdChildView::OnUpdatePasteMix(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && IsClipboardFormatAvailable(CF_WAVE) );
		}

		void CWaveEdChildView::OnUpdatePasteCrossfade(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && IsClipboardFormatAvailable(CF_WAVE) );
		}

		void CWaveEdChildView::OnUpdateSelectionInsertSilence(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable();
		}

		void CWaveEdChildView::OnUpdateSetLoopStart(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable( wdWave );
		}
		void CWaveEdChildView::OnUpdateSetLoopEnd(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable( wdWave );
		}

		void CWaveEdChildView::OnViewSampleHold()
		{
			CSingleLock lock(&semaphore, TRUE);
			resampler.DisposeResamplerData(resampler_data);
			resampler.quality(helpers::dsp::resampler::quality::zero_order);
			resampler_data = resampler.GetResamplerData();
			resampler.UpdateSpeed(resampler_data,1.0);
			RefreshDisplayData();
			Invalidate(true);
		}
		void CWaveEdChildView::OnViewSpline()
		{
			CSingleLock lock(&semaphore, TRUE);
			resampler.DisposeResamplerData(resampler_data);
			resampler.quality(helpers::dsp::resampler::quality::spline);
			resampler_data = resampler.GetResamplerData();
			resampler.UpdateSpeed(resampler_data,1.0);
			RefreshDisplayData();
			Invalidate(true);
		}
		void CWaveEdChildView::OnViewSinc()
		{
			CSingleLock lock(&semaphore, TRUE);
			resampler.DisposeResamplerData(resampler_data);
			resampler.quality(helpers::dsp::resampler::quality::sinc);
			resampler_data = resampler.GetResamplerData();
			resampler.UpdateSpeed(resampler_data,1.0);
			RefreshDisplayData();
			Invalidate(true);
		}
		void CWaveEdChildView::OnUpdateViewSampleHold(CCmdUI* pCmdUI)
		{
			pCmdUI->SetCheck(resampler.quality() == helpers::dsp::resampler::quality::zero_order);
		}
		void CWaveEdChildView::OnUpdateViewSpline(CCmdUI* pCmdUI)
		{
			pCmdUI->SetCheck(resampler.quality() == helpers::dsp::resampler::quality::spline);
		}
		void CWaveEdChildView::OnUpdateViewSinc(CCmdUI* pCmdUI)
		{
			pCmdUI->SetCheck(resampler.quality() == helpers::dsp::resampler::quality::sinc);
		}


		//////////////////////////////////////////////////////////////////////////
		//////		Clipboard Functions

		void CWaveEdChildView::OnEditDelete()
		{
			if (wdWave && blSelection)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				{
					CExclusiveLock lock(&_pSong->semaphore, 2, true);
					_pSong->StopInstrument(wsInstrument);

					unsigned long length = blLength;

					long datalen = (wdLength - length);
					if (datalen)
					{
						XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
						wave.DeleteAt(blStart, length);
						wdLeft = wave.pWaveDataL();
						wdRight = wave.pWaveDataR();
						wdLength = wave.WaveLength();
						//	adjust loop points if necessary
						if(wdLoop)
						{
							if(blStart+length<wdLoopS)
							{
								wdLoopS -= length;
								wave.WaveLoopStart(wdLoopS);
							}
							if(blStart+length<wdLoopE)
							{
								wdLoopE -= length;
								wave.WaveLoopEnd(wdLoopE);
							}
						}
						if(wdSusLoop)
						{
							if(blStart+length<wdSusLoopS)
							{
								wdSusLoopS -= length;
								wave.WaveSusLoopStart(wdSusLoopS);
							}
							if(blStart+length<wdSusLoopE)
							{
								wdSusLoopE -= length;
								wave.WaveSusLoopEnd(wdSusLoopE);
							}
						}

					}
					else
					{
						_pSong->samples.RemoveAt(wsInstrument);
						wdLength = 0;
						wdWave   = false;
					}
				
					//Validate display
					if ( (diStart + diLength) > wdLength )
					{
						long newlen = wdLength - diLength;

						if ( newlen < 0 )
							this->OnSelectionShowall();
						else
							diStart = (unsigned)newlen;
					}
					
					blSelection = false;
					blLength  = 0;
					blStart   = 0;
				}
				mainFrame->UpdateInstrumentEditor();
				mainFrame->WaveEditorBackUpdate();
				ResetScrollBars(true);
				RefreshDisplayData(true);
				Invalidate(true);
			}
		}

		void CWaveEdChildView::OnEditCopy() 
		{
			unsigned long c = 0;
			unsigned long length = blLength;

///Needs to be packed, else the compiler aligns it.
#pragma pack(push, 1) 
			struct fullheader
			{
				uint32_t	head;
				uint32_t	size;
				uint32_t	head2;
				uint32_t	fmthead;
				uint32_t	fmtsize;
				WAVEFORMATEX	fmtcontent;
				uint32_t datahead;
				uint32_t datasize;
			} wavheader;
#pragma pack(pop)

			OpenClipboard();
			EmptyClipboard();
			hClipboardData = GlobalAlloc(GMEM_MOVEABLE, ( wdStereo ? length*4 + sizeof(fullheader) : length*2 + sizeof(fullheader)));
			const XMInstrument::WaveData<>& wave = _pSong->samples[wsInstrument];
			
			wavheader.head = FourCC("RIFF");
			wavheader.size = wdStereo ? (length*4 + sizeof(fullheader) - 8) : (length*2 + sizeof(fullheader) - 8);
			wavheader.head2= FourCC("WAVE");
			wavheader.fmthead = FourCC("fmt ");
			wavheader.fmtsize = sizeof(WAVEFORMATEX);
			wavheader.fmtcontent.wFormatTag = WAVE_FORMAT_PCM;
			wavheader.fmtcontent.nChannels = wdStereo ? 2 : 1;
			wavheader.fmtcontent.nSamplesPerSec = wave.WaveSampleRate();
			wavheader.fmtcontent.wBitsPerSample = 16;
			wavheader.fmtcontent.nAvgBytesPerSec = wavheader.fmtcontent.wBitsPerSample/8*wavheader.fmtcontent.nChannels*wavheader.fmtcontent.nSamplesPerSec;
			wavheader.fmtcontent.nBlockAlign = wdStereo ? 4 : 2 ;
			wavheader.fmtcontent.cbSize = 0;
			wavheader.datahead = FourCC("data");
			wavheader.datasize = wdStereo ? length*4 : length*2;

			pClipboardData = (char*) GlobalLock(hClipboardData);
			
			CopyMemory(pClipboardData, &wavheader, sizeof(fullheader) );
			if (wdStereo)
			{
				pClipboardData += sizeof(fullheader);
				for (c = 0; c < length*2; c += 2)
				{
					*((signed short*)pClipboardData + c) = *(wdLeft + blStart + (long)(c*0.5));
					*((signed short*)pClipboardData + c + 1) = *(wdRight + blStart + (long)(c*0.5));
				}
			}
			else
			{
				CopyMemory(pClipboardData + sizeof(fullheader), (wdLeft + blStart), length*2);
			}

			GlobalUnlock(hClipboardData);
			SetClipboardData(CF_WAVE, hClipboardData);
			CloseClipboard();
			RefreshDisplayData(true);
			Invalidate(true);
		}

		void CWaveEdChildView::OnEditCut() 
		{
			OnEditCopy();
			OnEditDelete();
		}

		void CWaveEdChildView::OnEditCrop()
		{
			unsigned long blStartTemp = blStart;
			
			blStart += blLength;
			blLength = (wdLength - blStart);
			///\todo : fix the blLengths. There need to be some +1 and -1 throughout the source.
			if (blLength > 2 ) OnEditDelete();
			
			blSelection = true;
			blStart = 0;
			blLength = blStartTemp;
			if (blLength > 2 ) OnEditDelete();
		}

		void CWaveEdChildView::OnEditPaste() 
		{
			unsigned long c = 0;

			PsycleGlobal::inputHandler().AddMacViewUndo();

			char *pData;
			uint32_t lFmt, lData;
			
			WAVEFORMATEX* pFmt;
			short* pPasteData;

			OpenClipboard();
			hPasteData = GetClipboardData(CF_WAVE);
			pPasteData = (short*)GlobalLock(hPasteData);

			if ((*(uint32_t*)pPasteData != FourCC("RIFF")) && (*((uint32_t*)pPasteData + 2)!=FourCC("WAVE"))) {
				GlobalUnlock(hPasteData);
				CloseClipboard();
				return;
			}
			lFmt= *(uint32_t*)((char*)pPasteData + 16);
			pFmt = reinterpret_cast<WAVEFORMATEX*>((char*)pPasteData + 20); //"RIFF"+ len. +"WAVE" + "fmt " + len. = 20 bytes.

			lData = *(uint32_t*)((char*)pPasteData + 20 + lFmt + 4);
			pData = (char*)pPasteData + 20 + lFmt + 8;

			unsigned long lDataSamps = (unsigned long)(lData/pFmt->nBlockAlign);	//data length in bytes divided by number of bytes per sample
			{
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				_pSong->StopInstrument(wsInstrument);

				if (!wdWave)
				{
					//\todo: support different bitdepths
					if (pFmt->wBitsPerSample == 16)
					{
						XMInstrument::WaveData<>& wave = _pSong->WavAlloc(wsInstrument, (pFmt->nChannels==2) ? true : false, lDataSamps, "Clipboard");
						wdLength = lDataSamps;
						wdLeft  = wave.pWaveDataL();
						if (pFmt->nChannels == 1)
						{
							memcpy(wave.pWaveDataL(), pData, lData);
							wdStereo = false;
						}
						else if (pFmt->nChannels == 2)
						{
							for (c = 0; c < lDataSamps; ++c)
							{
								*(wave.pWaveDataL() + c) = *(signed short*)(pData + c*pFmt->nBlockAlign);
								*(wave.pWaveDataR() + c) = *(signed short*)(pData + c*pFmt->nBlockAlign + (int)(pFmt->nBlockAlign/2));
							}
							wdRight = wave.pWaveDataR();
							wdStereo = true;
						}
						wdWave = true;
						ResetScrollBars(true);
						OnSelectionShowall();
						wave.WaveSampleRate(pFmt->nSamplesPerSec);
					}
					else {
						MessageBox(_T("Only 16bit mono or stereo data supported"),_T("Paste Wav"), MB_ICONERROR);
					}
				}
				else
				{
					XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
					if (pFmt->wBitsPerSample == 16 && (pFmt->nChannels==1 || pFmt->nChannels==2))
					{
						if ( (pFmt->nChannels == 1 && wdStereo == false) ||	
								(pFmt->nChannels == 2 && wdStereo == true) ) {
							XMInstrument::WaveData<> waveToPaste;
							waveToPaste.AllocWaveData(lDataSamps, wdStereo);
							//prepare left channel
							for (c = 0; c < lDataSamps; c++) {
								waveToPaste.pWaveDataL()[c] = *(short*)(pData + c*pFmt->nBlockAlign);
							}

							if(wdStereo)	//if stereo, prepare right channel
							{
								int align = (int)(pFmt->nBlockAlign/2);
								for (c = 0; c < lDataSamps; c++)
									waveToPaste.pWaveDataR()[c] = *(short*)(pData + c*pFmt->nBlockAlign + align);
							}

							//Insert
							wave.InsertAt(cursorPos,waveToPaste);
							wdLeft = wave.pWaveDataL();
							wdRight = wave.pWaveDataR();

							//update length
							wdLength = wave.WaveLength();

							//	adjust loop points if necessary
							if(wdLoop)
							{
								if(cursorPos<wdLoopS)
								{
									wdLoopS += lDataSamps;
									wave.WaveLoopStart(wdLoopS);
								}
								if(cursorPos<wdLoopE)
								{
									wdLoopE += lDataSamps;
									wave.WaveLoopEnd(wdLoopE);
								}
							}
							if(wdSusLoop)
							{
								if(cursorPos<wdSusLoopS)
								{
									wdSusLoopS += lDataSamps;
									wave.WaveSusLoopStart(wdSusLoopS);
								}
								if(cursorPos<wdSusLoopE)
								{
									wdSusLoopE += lDataSamps;
									wave.WaveSusLoopEnd(wdSusLoopE);
								}
							}
						}
						else {
							//todo: deal with this better.. i.e. dialog box offering to convert clipboard data
							MessageBox(_T("There is a difference in the number of channels. Please, convert it first"), _T("Paste wav"), MB_ICONERROR);
						}
					}
					else {
						MessageBox(_T("Only 16bit mono or stereo data supported"),_T("Paste Wav"), MB_ICONERROR);
					}
				}
			}
			GlobalUnlock(hPasteData);
			CloseClipboard();
			//OnSelectionShowall();

			ResetScrollBars(true);
			RefreshDisplayData(true);

			mainFrame->UpdateInstrumentEditor();
			mainFrame->WaveEditorBackUpdate();
			Invalidate(true);
		}

		void CWaveEdChildView::OnPasteOverwrite()
		{
			unsigned long startPoint;

			PsycleGlobal::inputHandler().AddMacViewUndo();

			char *pData;
			uint32_t lFmt, lData;
			
			WAVEFORMATEX* pFmt;
			short* pPasteData;

			OpenClipboard();
			hPasteData = GetClipboardData(CF_WAVE);
			pPasteData = (short*)GlobalLock(hPasteData);

			if ((*(uint32_t*)pPasteData != FourCC("RIFF")) && (*((uint32_t*)pPasteData + 2)!=FourCC("WAVE"))) {
				GlobalUnlock(hPasteData);
				CloseClipboard();
				return;
			}
			lFmt= *(uint32_t*)((char*)pPasteData + 16);
			pFmt = reinterpret_cast<WAVEFORMATEX*>((char*)pPasteData + 20); //"RIFF"+ len. +"WAVE" + "fmt " + len. = 20 bytes.

			lData = *(uint32_t*)((char*)pPasteData + 20 + lFmt + 4);
			pData = (char*)pPasteData + 20 + lFmt + 8;

			unsigned long lDataSamps = (int)(lData/pFmt->nBlockAlign);	//data length in bytes divided by number of bytes per sample
			{
				CExclusiveLock lock(&_pSong->semaphore, 2, true);
				_pSong->StopInstrument(wsInstrument);
				if (pFmt->wBitsPerSample == 16 && (pFmt->nChannels==1 || pFmt->nChannels==2))
				{
					if ( (pFmt->nChannels == 1 && wdStereo == false) ||
							(pFmt->nChannels == 2 && wdStereo == true) ) {
					
						unsigned long c;

						if(blSelection)	//overwrite selected block
						{
							//if clipboard data is longer than selection, truncate it
							if(lDataSamps>blLength)
							{
								lData=blLength*pFmt->nBlockAlign; 
								lDataSamps=blLength;
							}
							startPoint=blStart;
						}
						else		//overwrite at cursor
						{
							//truncate to current wave size	(should we be extending in this case??)
							if(lDataSamps>(wdLength-cursorPos))
							{
								lData=(wdLength-cursorPos)*pFmt->nBlockAlign;
								lDataSamps=wdLength-cursorPos;
							}
							startPoint=cursorPos;
						}

						XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
						//do left channel
						for (c = 0; c < lDataSamps; c++)
							wave.pWaveDataL()[startPoint + c] = *(short*)(pData + c*pFmt->nBlockAlign);
						
						if(wdStereo)	//do right channel if stereo
						{
							int align = (int)(pFmt->nBlockAlign/2);
							for (c = 0; c < lDataSamps; c++)
								wave.pWaveDataR()[startPoint + c] = *(short*)(pData + c*pFmt->nBlockAlign + align);
						}
						wave.WaveSampleRate(pFmt->nSamplesPerSec);
					}
					else {
						//todo: deal with this better.. i.e. dialog box offering to convert clipboard data
						MessageBox(_T("There is a difference in the number of channels. Please, convert it first"), _T("Paste wav"), MB_ICONERROR);
					}
				}
				else {
					MessageBox(_T("Only 16bit mono or stereo data supported"),_T("Paste Wav"), MB_ICONERROR);
				}
			}
			GlobalUnlock(hPasteData);
			CloseClipboard();
			//OnSelectionShowall();

			ResetScrollBars();
			RefreshDisplayData(true);

			mainFrame->UpdateInstrumentEditor();
			mainFrame->WaveEditorBackUpdate();
			Invalidate(true);

		}

		void CWaveEdChildView::OnPasteMix()
		{
			CWaveEdMixDialog MixDlg(GetOwner());
			if(MixDlg.DoModal() != IDCANCEL)
			{
				char *pData;
				WAVEFORMATEX* pFmt;
				unsigned long lDataSamps;
				{
					uint32_t lFmt, lData;
					short* pPasteData;

					OpenClipboard();
					hPasteData = GetClipboardData(CF_WAVE);
					pPasteData = (short*)GlobalLock(hPasteData);

					if ((*(uint32_t*)pPasteData != FourCC("RIFF")) && (*((uint32_t*)pPasteData + 2)!=FourCC("WAVE"))) {
						GlobalUnlock(hPasteData);
						CloseClipboard();
						return;
					}
					lFmt= *(uint32_t*)((char*)pPasteData + 16);
					pFmt = reinterpret_cast<WAVEFORMATEX*>((char*)pPasteData + 20); //"RIFF"+ len. +"WAVE" + "fmt " + len. = 20 bytes.

					lData = *(uint32_t*)((char*)pPasteData + 20 + lFmt + 4);
					pData = (char*)pPasteData + 20 + lFmt + 8;

					lDataSamps = (unsigned long)(lData/pFmt->nBlockAlign);	//data length in bytes divided by number of bytes per sample
				}

				if (pFmt->wBitsPerSample == 16 && (pFmt->nChannels==1 || pFmt->nChannels==2))
				{
					if ((pFmt->nChannels == 1 && wdStereo == false) 	
							|| (pFmt->nChannels == 2 && wdStereo == true))
					{
						unsigned long startPoint;
						unsigned long fadeInSamps(0), fadeOutSamps(0);
						unsigned long destFadeIn(0);	
						XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);

						CExclusiveLock lock(&_pSong->semaphore, 2, true);
						PsycleGlobal::inputHandler().AddMacViewUndo();
						_pSong->StopInstrument(wsInstrument);

						if(MixDlg.bFadeIn)
							fadeInSamps = Global::configuration()._pOutputDriver->GetSamplesPerSec() * MixDlg.fadeInTime;
						if(MixDlg.bFadeOut)
							fadeOutSamps = Global::configuration()._pOutputDriver->GetSamplesPerSec() * MixDlg.fadeOutTime;

						if(blSelection)	//overwrite selected block
						{
							//if clipboard data is longer than selection, truncate it
							lDataSamps = std::min(lDataSamps, blLength);
							startPoint=blStart;
						}
						else {		//overwrite at cursor
							startPoint=cursorPos;
						}

						unsigned long newLength = std::max(startPoint+lDataSamps,wdLength);

						fadeInSamps = std::min(fadeInSamps,lDataSamps);
						fadeOutSamps = std::min(fadeOutSamps,lDataSamps);
						destFadeIn = std::min(fadeInSamps,wdLength-startPoint);

						XMInstrument::WaveData<> waveClip;
						waveClip.AllocWaveData(newLength,wdStereo);
						waveClip.Silence(0,newLength);
						if (wdStereo) {
							int align = int(pFmt->nBlockAlign*0.5);
							for (unsigned long c = 0; c < lDataSamps; c++) {
								waveClip.pWaveDataL()[startPoint+c]= *(short*)(pData + c*pFmt->nBlockAlign);	//copy clipboard data to pTmp
								waveClip.pWaveDataR()[startPoint+c]= *(short*)(pData + c*pFmt->nBlockAlign + align);
							}
						}
						else {
							for (unsigned long c = 0; c < lDataSamps; c++) {
								waveClip.pWaveDataL()[startPoint+c] = *(short*)(pData + c*pFmt->nBlockAlign);	//copy clipboard data to pTmp
							}
						}
						waveClip.Fade(startPoint, fadeInSamps, 0, MixDlg.srcVol); //do fade in on clipboard data
						wave.Fade(startPoint, destFadeIn, 1.0f, MixDlg.destVol); //do fade in on wave data
						waveClip.Amplify(startPoint+fadeInSamps, lDataSamps-fadeInSamps-fadeOutSamps, MixDlg.srcVol); //amplify non-faded part of clipboard data

						if(startPoint+lDataSamps < wdLength)
						{
							wave.Amplify(startPoint+destFadeIn, lDataSamps-destFadeIn-fadeOutSamps, MixDlg.destVol); //amplify wave data
							wave.Fade(startPoint+lDataSamps-fadeOutSamps, fadeOutSamps, MixDlg.destVol, 1.0f);	//fade out wave data
							wave.Fade(startPoint+lDataSamps-fadeOutSamps, fadeOutSamps, MixDlg.srcVol, 0);		//fade out clipboard data
						}
						else	//ignore fade out in this case, it doesn't make sense here
							wave.Amplify(startPoint+destFadeIn, wdLength-startPoint-destFadeIn, MixDlg.destVol);	//amplify wave data

						wave.Mix(waveClip);		//mix into pTmp
						wdLeft =wave.pWaveDataL();
						wdRight=wave.pWaveDataR();
						wdLength = wave.WaveLength();
					}
					else {
						//todo: deal with this better.. i.e. dialog box offering to convert clipboard data
						MessageBox(_T("There is a difference in the number of channels. Please, convert it first"), _T("Paste wav"), MB_ICONERROR);
					}
				}
				else {
					MessageBox(_T("Only 16bit mono or stereo data supported"),_T("Paste Wav"), MB_ICONERROR);
				}

				GlobalUnlock(hPasteData);
				CloseClipboard();
				//OnSelectionShowall();

				ResetScrollBars(true);
				RefreshDisplayData(true);

				mainFrame->UpdateInstrumentEditor();
				mainFrame->WaveEditorBackUpdate();
				Invalidate(true);
			}

		}



		void CWaveEdChildView::OnPasteCrossfade()
		{
			CWaveEdCrossfadeDialog XFadeDlg(GetOwner());
			if(XFadeDlg.DoModal() != IDCANCEL)
			{
				char *pData;
				WAVEFORMATEX* pFmt;
				unsigned long lDataSamps;
				{
					uint32_t lFmt, lData;
					short* pPasteData;

					OpenClipboard();
					hPasteData = GetClipboardData(CF_WAVE);
					pPasteData = (short*)GlobalLock(hPasteData);

					if ((*(uint32_t*)pPasteData != FourCC("RIFF")) && (*((uint32_t*)pPasteData + 2)!=FourCC("WAVE"))) {
						GlobalUnlock(hPasteData);
						CloseClipboard();
						return;
					}
					lFmt= *(uint32_t*)((char*)pPasteData + 16);
					pFmt = reinterpret_cast<WAVEFORMATEX*>((char*)pPasteData + 20); //"RIFF"+ len. +"WAVE" + "fmt " + len. = 20 bytes.

					lData = *(uint32_t*)((char*)pPasteData + 20 + lFmt + 4);
					pData = (char*)pPasteData + 20 + lFmt + 8;

					lDataSamps = (unsigned long)(lData/pFmt->nBlockAlign);	//data length in bytes divided by number of bytes per sample
				}

				if (pFmt->wBitsPerSample == 16 && (pFmt->nChannels==1 || pFmt->nChannels==2))
				{
					if ((pFmt->nChannels == 1 && wdStereo == false) 	
							|| (pFmt->nChannels == 2 && wdStereo == true))
					{
						unsigned long startPoint, endPointPlus1;
						XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);

						CExclusiveLock lock(&_pSong->semaphore, 2, true);
						PsycleGlobal::inputHandler().AddMacViewUndo();
						_pSong->StopInstrument(wsInstrument);

						if(blSelection)	//overwrite selected block
						{
							startPoint=blStart;
							endPointPlus1 = startPoint+std::min(lDataSamps, blLength);
							//selection determines length of the crossfade
							//if selection is longer, fade length is length of clipboard data
						}
						else		//overwrite at cursor
						{
							startPoint=cursorPos;
							endPointPlus1 = std::min(startPoint+lDataSamps,wdLength-startPoint);
							//if clipboard data fits in existing wave, its length is fade length
							//if not, the end of the existing wave marks the end of the fade
						}

						unsigned long newLength = std::max(startPoint+lDataSamps, wdLength);

						XMInstrument::WaveData<> waveClip;
						waveClip.AllocWaveData(newLength,wdStereo);
						waveClip.Silence(0,newLength);
						if (wdStereo) {
							int align = int(pFmt->nBlockAlign*0.5);
							for (unsigned long c = 0; c < lDataSamps; c++) {
								waveClip.pWaveDataL()[startPoint+c]= *(short*)(pData + c*pFmt->nBlockAlign);	//copy clipboard data to pTmp
								waveClip.pWaveDataR()[startPoint+c]= *(short*)(pData + c*pFmt->nBlockAlign + align);
							}
						}
						else {
							for (unsigned long c = 0; c < lDataSamps; c++) {
								waveClip.pWaveDataL()[startPoint+c] = *(short*)(pData + c*pFmt->nBlockAlign);	//copy clipboard data to pTmp
							}
						}

						waveClip.Fade(startPoint, endPointPlus1, XFadeDlg.srcStartVol, XFadeDlg.srcEndVol);			//fade clipboard data
						wave.Fade(startPoint, endPointPlus1, XFadeDlg.destStartVol, XFadeDlg.destEndVol);		//fade wave data
						wave.Mix(waveClip);															//mix clipboard with wave
						wdLeft = wave.pWaveDataL();
						wdRight = wave.pWaveDataR();
						wdLength = wave.WaveLength();
					}
					else {
						//todo: deal with this better.. i.e. dialog box offering to convert clipboard data
						MessageBox(_T("There is a difference in the number of channels. Please, convert it first"), _T("Paste wav"), MB_ICONERROR);
					}
				}
				else {
					MessageBox(_T("Only 16bit mono or stereo data supported"),_T("Paste Wav"), MB_ICONERROR);
				}



				GlobalUnlock(hPasteData);
				CloseClipboard();

				ResetScrollBars(true);
				RefreshDisplayData(true);

				mainFrame->UpdateInstrumentEditor();
				mainFrame->WaveEditorBackUpdate();
				Invalidate(true);
			}

		}



		void CWaveEdChildView::OnEditSelectAll() 
		{
			diStart = 0;
			blStart = 0;
			diLength = wdLength;
			blLength = wdLength;
			blSelection = true;

			ResetScrollBars();
			RefreshDisplayData();

			Invalidate(true);
		}

		void CWaveEdChildView::OnDestroyClipboard() 
		{
			CWnd::OnDestroyClipboard();
			GlobalFree(hClipboardData);
		}

		void CWaveEdChildView::OnEditSnapToZero()
		{
			bSnapToZero=!bSnapToZero;
		}

/*
		void CWaveEdChildView::OnPopupSetLoopStart()
		{
			PsycleGlobal::inputHandler().AddMacViewUndo();
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
			_pSong->StopInstrument(wsInstrument);
			CRect rect;
			GetClientRect(&rect);
			int nWidth = rect.Width();
			wdLoopS = diStart + rbX * diLength/nWidth;
			wave.WaveLoopStart(wdLoopS);
			if (wave.WaveLoopEnd()< wdLoopS )
			{
				wave.WaveLoopEnd(wdLoopS);
			}
			wdLoopE = wave.WaveLoopEnd();
			if (!wdLoop) 
			{
				wdLoop=true;
				wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
			}
			mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Play(notecommands::middleC,_pSong->wavprev.GetPosition());
			}
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
		void CWaveEdChildView::OnPopupSetLoopEnd()
		{
			PsycleGlobal::inputHandler().AddMacViewUndo();
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
			_pSong->StopInstrument(wsInstrument);
			CRect rect;
			GetClientRect(&rect);
			int nWidth = rect.Width();
			wdLoopE = diStart + rbX * diLength/nWidth;
			wave.WaveLoopEnd(wdLoopE);
			if (wave.WaveLoopStart()> wdLoopE )
			{
				wave.WaveLoopStart(wdLoopE);
			}
			wdLoopS = wave.WaveLoopStart();
			if (!wdLoop) 
			{
				wdLoop=true;
				wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
			}
			mainFrame->UpdateInstrumentEditor();// This causes an update of the Instrument Editor.
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Play(notecommands::middleC,_pSong->wavprev.GetPosition());
			}
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
*/

		void CWaveEdChildView::OnPopupSelectionToLoop()
		{
			if(!blSelection) return;
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
			_pSong->StopInstrument(wsInstrument);

			wdLoopS = blStart;
			wdLoopE = blStart+blLength;
			wave.WaveLoopStart(wdLoopS);
			wave.WaveLoopEnd(wdLoopE);
			if (!wdLoop) 
			{
				wdLoop=true;
				wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
			}

			mainFrame->UpdateInstrumentEditor();
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Play(notecommands::middleC,_pSong->wavprev.GetPosition());
			}

			CRect rect;
			GetClientRect(&rect);
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
		void CWaveEdChildView::OnPopupSelectionSustain()
		{
			if(!blSelection) return;
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);
			_pSong->StopInstrument(wsInstrument);

			wdSusLoopS = blStart;
			wdSusLoopE = blStart+blLength;
			wave.WaveSusLoopStart(wdSusLoopS);
			wave.WaveSusLoopEnd(wdSusLoopE);
			if (!wdSusLoop) 
			{
				wdSusLoop=true;
				wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
			}

			mainFrame->UpdateInstrumentEditor();
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Play(notecommands::middleC,_pSong->wavprev.GetPosition());
			}

			CRect rect;
			GetClientRect(&rect);
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
		void CWaveEdChildView::OnPopupUnsetLoop()
		{
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);

			wdLoop=false;
			wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);

			mainFrame->UpdateInstrumentEditor();
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Stop();
			}

			CRect rect;
			GetClientRect(&rect);
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
		void CWaveEdChildView::OnPopupUnsetSustain()
		{
			CExclusiveLock lock(&_pSong->semaphore, 2, true);
			XMInstrument::WaveData<>& wave = _pSong->samples.get(wsInstrument);

			wdSusLoop=false;
			wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);

			mainFrame->UpdateInstrumentEditor();
			if(_pSong->wavprev.IsEnabled()) {
				_pSong->wavprev.Stop();
			}

			CRect rect;
			GetClientRect(&rect);
			rect.bottom -= GetSystemMetrics(SM_CYHSCROLL);
			InvalidateRect(&rect, false);
		}
		void CWaveEdChildView::OnUpdateSelectionToLoop(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && blSelection);	
		}
		void CWaveEdChildView::OnUpdateSelectionSustain(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && blSelection);	
		}
		void CWaveEdChildView::OnUpdateUnsetLoop(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && wdLoop);	
		}
		void CWaveEdChildView::OnUpdateUnsetSustain(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(wdWave && wdSusLoop);	
		}



		void CWaveEdChildView::SetSong(Song& _sng)
		{
			_pSong = &_sng;
		}
		void CWaveEdChildView::SetMainFrame(CMainFrame* parent)
		{
			mainFrame = parent;
		}
		unsigned long CWaveEdChildView::GetWaveLength()
		{
			if(wdWave)
				return wdLength;
			else
				return 0;
		}
		bool CWaveEdChildView::IsStereo()
		{
			return wdStereo;
		}
		unsigned long CWaveEdChildView::GetSelectionLength()
		{
			if(wdWave && blSelection)
				return blLength;
			else
				return 0;
		}
		float CWaveEdChildView::GetVolume() 
		{
			CSliderCtrl* volSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_VOLSLIDER));
			return volSlider->GetPos()/100.f;
		}
		unsigned long CWaveEdChildView::GetCursorPos()
		{
			if(wdWave)
				return cursorPos;
			else
				return 0;
		}
		void CWaveEdChildView::SetCursorPos(unsigned long newpos)
		{
			if(newpos<0 || newpos>=wdLength)
				return;

			cursorPos = newpos;
			if(cursorPos < diLength) diStart=0;
			else if(cursorPos>wdLength-diLength) diStart=wdLength-diLength;
			else diStart = cursorPos-(diLength/2);

			ResetScrollBars();
			RefreshDisplayData();
			Invalidate(false);
		}

		void CWaveEdChildView::ResetScrollBars(bool bNewLength)
		{
			CScrollBar* hScroll = static_cast<CScrollBar*>(zoombar.GetDlgItem(IDC_HSCROLL));
			CSliderCtrl* zoomSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_ZOOMSLIDE));
			CSliderCtrl* volSlider = static_cast<CSliderCtrl*>(zoombar.GetDlgItem(IDC_VOLSLIDER));

			if(wdWave)
			{
				//set horizontal scroll bar
				SCROLLINFO si;
				si.cbSize=sizeof si;
				si.nPage=diLength;
				si.nPos=diStart;
				if(bNewLength)
				{
					si.fMask=SIF_PAGE | SIF_POS | SIF_RANGE;
					si.nMin=0; si.nMax=wdLength-1;
				}
				else
					si.fMask=SIF_PAGE | SIF_POS;

				hScroll->SetScrollInfo(&si);

				//set zoom slider
				if(bNewLength)
				{
					//here we're looking for the highest n where wdLength/(b^n)>8 , where b is zoomBase and n is the max value for the zoom slider.
					//the value of the slider determines a wdLength/diLength ratio of b^n.. so this way, diLength is limited to a minimum of 8 samples.
					//another alternative to consider would be to use a fixed range, and change the zoomBase based on the wavelength to match..

					float maxzoom = log10(float(wdLength/8.0f))/log10(zoomBase);	// wdLength/(b^n)>=8    <==>   n <= log<b>(wdLength/8)
					// log<10>(x)/log<10>(b) == log<b>(x)
					int slidermax = helpers::math::round<int,float>(floor(maxzoom));
					if(slidermax<0) slidermax=0;			//possible for waves with less than 8 samples (!)
					zoomSlider->SetRange(0, slidermax);
				}
				if(diLength!=0)
				{

					//this is the same concept, except this is to give us some idea of where to draw the slider based on the existing zoom
					//so, instead of wdLength/8 (the max zoom), we're doing wdLength/diLength (the current zoom)
					float zoomfactor = log10(wdLength/(float)diLength)/log10(zoomBase);
					int newpos = helpers::math::round<int,float>(zoomfactor+0.5f);
					if(newpos<0)	newpos=0;		//i'm not sure how this would happen, but just in case
					zoomSlider->SetPos(newpos);
				}
				volSlider->SetPos(_pSong->wavprev.GetVolume()*100);
			}
			else
			{
				//disabled scrollbar
				SCROLLINFO si;
				si.cbSize=sizeof si;
				si.nPage=0;
				si.nPos=0;
				si.fMask=SIF_PAGE | SIF_POS | SIF_RANGE;
				si.nMin=0; si.nMax=0;
				hScroll->SetScrollInfo(&si);

				//disabled zoombar
				zoomSlider->SetRange(0, 0);
				zoomSlider->SetPos(0);
				volSlider->SetPos(100);
			}

			//set volume slider
			volSlider->Invalidate(false);
		}


		////////////////////////////////////////////
		////		FindNearestZero()
		////	searches for the zero crossing nearest to a given sample index.
		////  returns the sample index of the nearest zero, or, in the event that the nearest zero crossing never actually hits zero,
		////	it will return the index of the sample that comes the closest to zero.  if the index is out of range, the last sample
		////	index is returned.  the first and last sample of the wave are considered zero.
		unsigned long CWaveEdChildView::FindNearestZero(unsigned long startpos)
		{
			if(startpos>=wdLength) return wdLength-1;
			
			float sign;
			bool bLCLZ=false, bLCRZ=false, bRCLZ=false, bRCRZ = false;		//right/left chan, right/left zero
			unsigned long LCLZPos(0), LCRZPos(0), RCLZPos(0), RCRZPos(0);
			unsigned long ltRange, rtRange;	//these refer to the left and right side of the startpos, not the left/right channel
			ltRange=startpos;
			rtRange=wdLength-startpos;

			// do left channel
			if(wdLeft[startpos]<0) sign=-1;
			else if(wdLeft[startpos]>0) sign=1;
			else return startpos;		//easy enough..

			//left chan, left side
			for(unsigned long i=1; i<=ltRange; ++i)		//start with i=1-- since we're looking for a sign change from startpos, i=0 will never give us what we want
			{
				if( wdLeft[startpos-i] * sign <= 0 )		//if this product is negative, sign has switched.
				{
					LCLZPos=startpos-i;
					if(abs(wdLeft[LCLZPos+1]) < abs(wdLeft[LCLZPos]))				//check if the last one was closer to zero..
						LCLZPos++;													//and if so, set to that sample.
					bLCLZ=true;
					if(rtRange>i)rtRange=i;		//limit further searches
					ltRange=i;
					break;
				}
			}
			
			//left chan, right side
			for(unsigned long i=1; i<rtRange; ++i)
			{
				if( wdLeft[startpos+i] * sign <= 0 )
				{
					LCRZPos = startpos+i;
					if(abs(wdLeft[LCRZPos-1]) < abs(wdLeft[LCRZPos]))
						LCRZPos--;
					bLCRZ=true;
					break;
				}
			}

			if(wdStereo)
			{
				// do right channel
				if(wdRight[startpos]<0) sign=-1;
				else if(wdRight[startpos]>0) sign=1;
				else return startpos;		//easy enough..

				//right chan, left side
				for(unsigned long i=1; i<=ltRange; ++i)
				{
					if( wdRight[startpos-i] * sign <= 0 )
					{
						RCLZPos=startpos-i;
						if(abs(wdRight[RCLZPos+1]) < abs(wdRight[RCLZPos]))
							RCLZPos++;
						bRCLZ=true;
						break;
					}
				}
				
				//right chan, right side
				for(unsigned long i=1; i<rtRange; ++i)
				{
					if( wdRight[startpos+i] * sign <= 0 )
					{
						RCRZPos = startpos+i;
						if(abs(wdRight[RCRZPos-1]) < abs(wdRight[RCRZPos]))
							RCRZPos--;
						bRCRZ=true;
						break;
					}
				}

			}

			//find the closest
			unsigned long ltNearest=0, rtNearest=wdLength-1;

			if(wdStereo)
			{
				if(bLCLZ || bRCLZ)		//there's a zero on the left side
				{
					if(!bRCLZ)				//only in the left channel?
						ltNearest=LCLZPos;	//then that one's closest..
					else if(!bLCLZ)			//..and vice versa
						ltNearest=RCLZPos;
					else					//zeros in both chans?
						ltNearest = ( LCLZPos>RCLZPos? LCLZPos: RCLZPos );	//both should be lower than startpos, so the highest is the closest
				}

				if(bLCRZ || bLCRZ)
				{
					if(!bRCRZ)
						rtNearest=LCRZPos;
					else if(!bLCRZ)
						rtNearest=RCRZPos;
					else
						rtNearest = (LCRZPos<RCRZPos? LCRZPos: RCRZPos );
				}
			}
			else		//mono sample
			{
				if(bLCLZ)
					ltNearest = LCLZPos;
				if(bLCRZ)
					rtNearest = LCRZPos;
			}

			if(startpos-ltNearest < rtNearest-startpos)
				return ltNearest;
			else
				return rtNearest;
		}

		void CWaveEdChildView::RefreshDisplayData(bool bRefreshHeader /*=false */)
		{
			if(wdWave)
			{
				CRect rect;
				GetClientRect(&rect);
				int const cyHScroll=GetSystemMetrics(SM_CYHSCROLL);
				int const nWidth = rect.Width();
				if(nWidth==0)return;
				int const nHeadHeight = rect.Height()/10;
				int const nHeight= rect.Height()-cyHScroll-nHeadHeight;
				int const my=nHeight/2;
				int const myHead=nHeadHeight/2;
				int wrHeight;
				int wrHeadHeight;

				if(wdStereo)
				{	wrHeight=my/2;	wrHeadHeight=myHead/2;	}
				else 
				{	wrHeight=my;	wrHeadHeight=myHead;	}

				float OffsetStep = (float) diLength / nWidth;
				float HeaderStep = (float) wdLength / nWidth;
				int yLow, yHi;

				lDisplay.resize(nWidth);
				if ( OffsetStep >1.f)
				{
					long offsetEnd=diStart;
					for(int c(0); c < nWidth; c++)
					{
						int yLow=0, yHi=0;
						long d = offsetEnd;
						offsetEnd = diStart + (long)floorf((c+1) * OffsetStep);
						for (; d < offsetEnd; d+=1)
						{
							int value = *(wdLeft+d);
							if (yLow > value) yLow = value;
							if (yHi <  value) yHi  = value;
						}
						lDisplay[c].first  = (wrHeight * yLow) >> 15;
						lDisplay[c].second = (wrHeight * yHi ) >> 15;
					}
					if(wdStereo)
					{
						rDisplay.resize(nWidth);
						offsetEnd=diStart;
						for(int c(0); c < nWidth; c++)
						{
							int yLow=0, yHi=0;
							long d = offsetEnd;
							offsetEnd = diStart + (long)floorf((c+1) * OffsetStep);
							for (; d < offsetEnd; d+=1)
							{
								int value = *(wdRight+d);
								if (yLow > value) yLow = value;
								if (yHi <  value) yHi  = value;
							}
							rDisplay[c].first  = (wrHeight * yLow) >> 15;
							rDisplay[c].second = (wrHeight * yHi ) >> 15;
						}
					}
				}
				else {
					ULARGE_INTEGER offsetStepRes; offsetStepRes.QuadPart = (double)OffsetStep* 4294967296.0;
					ULARGE_INTEGER posin; posin.QuadPart = diStart * 4294967296.0;
					for(int c = 0; c < nWidth; c++)
					{
						yLow=resampler.work(wdLeft+posin.HighPart,posin.HighPart,posin.LowPart,wdLength,resampler_data);
						posin.QuadPart += offsetStepRes.QuadPart;

						lDisplay[c].first  = (wrHeight * yLow) >> 15;
						lDisplay[c].second = 0;
					}
					if(wdStereo)
					{
						rDisplay.resize(nWidth);
						posin.QuadPart = diStart * 4294967296.0;
						for(int c(0); c < nWidth; c++)
						{
							yLow=resampler.work(wdRight+posin.HighPart,posin.HighPart,posin.LowPart,wdLength,resampler_data);
							posin.QuadPart += offsetStepRes.QuadPart;

							rDisplay[c].first  = (wrHeight * yLow) >> 15;
							rDisplay[c].second = 0;
						}
					}
				}
				if(bRefreshHeader)
				{
					// left channel of header
					lHeadDisplay.resize(nWidth);
					if (HeaderStep> 1.f) {
						long offsetEnd=0;
						for(int c(0); c < nWidth; c++)
						{
							yLow=0;yHi=0;
							long d = offsetEnd;
							offsetEnd = (long)floorf((c+1) * HeaderStep);
							for (; d < offsetEnd; d+=1)
							{
								int value = *(wdLeft+d);
								if (yLow > value) yLow = value;
								if (yHi <  value) yHi  = value;
							}
							// todo: very low-volume samples tend to disappear.. we should round up instead of down
							lHeadDisplay[c].first = (wrHeadHeight * yLow) >> 15;
							lHeadDisplay[c].second= (wrHeadHeight * yHi ) >> 15;
						}
						if(wdStereo)
						{
							// right channel of header
							rHeadDisplay.resize(nWidth);
							offsetEnd=0;
							for(int c(0); c < nWidth; c++)
							{
								yLow=0;yHi=0;
								long d = offsetEnd;
								offsetEnd = (long)floorf((c+1) * HeaderStep);
								for (; d < offsetEnd; d+=1)
								{
									int value = *(wdRight+d);
									if (yLow > value) yLow = value;
									if (yHi <  value) yHi  = value;
								}
								// todo: very low-volume samples tend to disappear. we should round up instead of down
								rHeadDisplay[c].first = (wrHeadHeight * yLow) >> 15;
								rHeadDisplay[c].second= (wrHeadHeight * yHi ) >> 15;
							}
						}
					}
					else {
						for(int c(0); c < nWidth; c++)
						{
							long d = floorf(c * HeaderStep);
							int value = *(wdLeft+d);
							// todo: very low-volume samples tend to disappear. we should round up instead of down
							lHeadDisplay[c].first = (wrHeadHeight * value) >> 15;
							lHeadDisplay[c].second= 0;
						}
						if(wdStereo)
						{
							rHeadDisplay.resize(nWidth);
							for(int c(0); c < nWidth; c++)
							{
								long d = floorf(c * HeaderStep);
								int value = *(wdRight+d);
								// todo: very low-volume samples tend to disappear. we should round up instead of down
								rHeadDisplay[c].first = (wrHeadHeight * value) >> 15;
								rHeadDisplay[c].second= 0;
							}
						}
					}
				}
			}
			else
			{
				lDisplay.clear();
				rDisplay.clear();
				lHeadDisplay.clear();
				rHeadDisplay.clear();
			}
		}

		short CWaveEdChildView::CalcBufferPeak(short* buffer, int length) const {
			short  max = 0;
			for (int c = 0; c < length ; c++) {
				short const absBuf = std::abs(buffer[c]);
				if (max < absBuf) max = absBuf;
			}
			return max;
		}

}}
