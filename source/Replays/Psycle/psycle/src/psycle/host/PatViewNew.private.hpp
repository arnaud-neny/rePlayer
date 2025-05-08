///\file
///\brief pattern view graphic operations for psycle::host::CChildView, private header
#pragma once
#include <psycle/host/detail/project.hpp>
namespace psycle { namespace host {

		#define DRAW_DATA		1
		#define DRAW_HSCROLL	2
		#define DRAW_VSCROLL	4
		#define DRAW_TRHEADER	8
		#define DRAW_FULL_DATA	16

		void CChildView::PreparePatternRefresh(int drawMode)
		{
		#ifdef _DEBUG_PATVIEW
			TRACE("PreparePatternRefresh\n");
		#endif

			CRect rect;	
			updateMode=drawMode;					// this is ununsed for patterns
			const int snt = _pSong.SONGTRACKS;
			const int plines = _pSong.patternLines[_pSong.playOrder[editPosition]];
			if ( editcur.track >= snt ) // This should only happen when changing the song tracks.
			{							// Else, there is a problem.
				TRACE("editcur.track out of range in PreparePatternRefresh\n");
				editcur.track = snt-1;
			}
			if ( editcur.line >= plines ) // This should only happen when changing the pattern lines
			{							  // or changing to a pattern with less lines.
				TRACE("editcur.line out of range in PreparePatternRefresh\n");
				editcur.line = plines-1;
			}
			//////////////////////////////////////////////////////////////////////
			// Set the offsets and positions of data on the screen.

			// Track Offset
			if ( snt <= VISTRACKS)	
			{ 
				maxt = snt; 
				rntOff = 0; 
			}
			else
			{
				if (bScrollDetatch)
				{
					if ( drawMode == draw_modes::horizontal_scroll )
					{
						rntOff = ntOff;
						if ( rntOff >= snt-VISTRACKS ) 
							maxt = VISTRACKS;
						else 
							maxt = VISTRACKS+1;
					}
					else
					{
						if ( tOff+VISTRACKS > snt )
						{
							rntOff = snt-VISTRACKS;
							maxt=VISTRACKS;
						}
						else if (detatchpoint.track < tOff ) 
						{ 
							rntOff = detatchpoint.track; 
							maxt = VISTRACKS+1; 
						}
						else
						{
							if (detatchpoint.track >= tOff+VISTRACKS ) 
								rntOff =detatchpoint.track-VISTRACKS+1;
							else 
								rntOff = tOff;
						
							if ( rntOff >= snt-VISTRACKS ) 
								maxt = VISTRACKS;
							else 
								maxt = VISTRACKS+1;
						}
					}
				}
				else if (patView->_centerCursor)
				{
					if ( drawMode == draw_modes::horizontal_scroll ) 
						rntOff = ntOff;
					else 
						rntOff = editcur.track - (VISTRACKS/2);

					if (rntOff >= snt-VISTRACKS)	
					{	
						rntOff = snt-VISTRACKS;	
						maxt = VISTRACKS;	
					}
					else 
					{
						if ( rntOff < 0 ) 
						{ 
							rntOff = 0; 
						}
						maxt = VISTRACKS+1;
					}
				}
				else
				{
					if ( drawMode == draw_modes::horizontal_scroll )
					{
						rntOff = ntOff;
						if ( rntOff >= snt-VISTRACKS ) 
							maxt = VISTRACKS;
						else 
							maxt = VISTRACKS+1;
					}
					else
					{
						if ( tOff+VISTRACKS > snt )
						{
							rntOff = snt-VISTRACKS;
							maxt=VISTRACKS;
						}
						else if ( editcur.track < tOff ) 
						{ 
							rntOff = editcur.track; 
							maxt = VISTRACKS+1; 
						}
						else
						{
							if ( editcur.track >= tOff+VISTRACKS ) 
								rntOff =editcur.track-VISTRACKS+1;
							else 
								rntOff = tOff;
						
							if ( rntOff >= snt-VISTRACKS ) 
								maxt = VISTRACKS;
							else 
								maxt = VISTRACKS+1;
						}
					}
				}
			}
			// Line Offset
			if ( plines <=  VISLINES)	
			{ 
				maxl = plines; 
				rnlOff = 0; 
			}
			else 
			{
				if (bScrollDetatch)
				{
					if ( drawMode == draw_modes::vertical_scroll )
					{
						rnlOff = nlOff;
						if ( rnlOff >= plines-VISLINES ) 
							maxl = VISLINES;
						else 
							maxl = VISLINES+1;
					}
					else 
					{
						if ( lOff+VISLINES > plines )
						{
							rnlOff = plines-VISLINES;
							maxl=VISLINES;
						}
						else if ( detatchpoint.line < lOff+1 ) 
						{ 
							rnlOff = detatchpoint.line-1; 
							if (rnlOff < 0)
							{
								rnlOff = 0;
							}
							maxl = VISLINES+1; 
						}
						else 
						{
							if ( detatchpoint.line >= lOff+VISLINES ) 
								rnlOff =detatchpoint.line-VISLINES+1;
							else 
								rnlOff = lOff;

							if ( rnlOff >= plines-VISLINES ) 
								maxl = VISLINES;
							else 
								maxl = VISLINES+1;
						}
					}
				}
				else if (patView->_centerCursor)
				{
					if ( drawMode == draw_modes::vertical_scroll ) 
						rnlOff = nlOff;
					else 
						rnlOff = editcur.line - (VISLINES/2);

					if (rnlOff >= plines-VISLINES) 
					{ 
						rnlOff = plines-VISLINES; 
						maxl = VISLINES; 
					}
					else 
					{
						if ( rnlOff < 0 ) 
						{ 
							rnlOff = 0; 
						}
						maxl = VISLINES+1;
					}
				}
				else
				{
					if ( drawMode == draw_modes::vertical_scroll )
					{
						rnlOff = nlOff;
						if ( rnlOff >= plines-VISLINES ) 
							maxl = VISLINES;
						else 
							maxl = VISLINES+1;
					}
					else 
					{
						if ( lOff+VISLINES > plines )
						{
							rnlOff = plines-VISLINES;
							maxl=VISLINES;
						}
						else if ( editcur.line < lOff+1 ) 
						{ 
							rnlOff = editcur.line-1; 
							if (rnlOff < 0)
							{
								rnlOff = 0;
							}
							maxl = VISLINES+1; 
						}
						else 
						{
							if ( editcur.line >= lOff+VISLINES ) 
								rnlOff =editcur.line-VISLINES+1;
							else 
								rnlOff = lOff;

							if ( rnlOff >= plines-VISLINES ) 
								maxl = VISLINES;
							else 
								maxl = VISLINES+1;
						}
					}
				}
			}
			////////////////////////////////////////////////////////////////////
			// Determines if background Scroll is needed or not.

			if (drawMode != draw_modes::all && drawMode != draw_modes::pattern)
			{
				if ( rnlOff != lOff )
				{
					rect.top=YOFFSET;	
					rect.left=0;
					rect.bottom=CH;		
					rect.right=CW;
					updatePar |= DRAW_VSCROLL;
					InvalidateRect(rect,false);
				}
				if ( rntOff != tOff )
				{
					rect.top=0;		
					rect.left=XOFFSET;
					rect.bottom=CH;
					rect.right=CW;
					updatePar |= DRAW_HSCROLL;
					InvalidateRect(rect,false);
				}
			}
			
			switch (drawMode)
			{
			case draw_modes::all: 
				// header
				rect.top=0; 
				rect.left=0;
				rect.bottom=CH;	
				rect.right=CW;
				updatePar |= DRAW_TRHEADER | DRAW_FULL_DATA;
				InvalidateRect(rect,false);
				SetPatternScrollBars(snt,plines);
				break;
			case draw_modes::pattern: 
				// all data
				rect.top=0;		
				rect.left=0;
				rect.bottom=CH;
				rect.right=CW;
				//Drawing also the header, because it can have differen track names.
				updatePar |= DRAW_TRHEADER | DRAW_FULL_DATA;
				InvalidateRect(rect,false);
				SetPatternScrollBars(snt,plines);
				break;
			case draw_modes::playback: 
				{
					int pos = Global::player()._lineCounter;
					if (( pos-rnlOff >= 0 ) &&  ( pos-rnlOff <maxl ) &&
						(_pSong.playOrder[editPosition] == _pSong.playOrder[Global::player()._playPosition]))
					{
						if (pos != playpos)
						{
							newplaypos = pos;

							rect.top= YOFFSET+ ((pos-rnlOff)*ROWHEIGHT);
							rect.bottom=rect.top+ROWHEIGHT;	// left never changes and is set at ChildView init.
							rect.left = 0;
							rect.right=CW;
							NewPatternDraw(0, _pSong.SONGTRACKS, pos, pos);
							updatePar |= DRAW_DATA;
							InvalidateRect(rect,false);
							if ((playpos >= 0) && (playpos != newplaypos))
							{
								rect.top = YOFFSET+ ((playpos-rnlOff)*ROWHEIGHT);
								rect.bottom = rect.top+ROWHEIGHT;
								rect.left = 0;
								rect.right = CW;
								NewPatternDraw(0, _pSong.SONGTRACKS, playpos, playpos);
								updatePar |= DRAW_DATA;
								playpos =-1;
								InvalidateRect(rect,false);
							}
						}
					}
					else 
					{
						newplaypos=-1;
						if (playpos >= 0) 
						{
							rect.top = YOFFSET+ ((playpos-rnlOff)*ROWHEIGHT);
							rect.bottom = rect.top+ROWHEIGHT;
							rect.left = 0;
							rect.right = CW;
							NewPatternDraw(0, _pSong.SONGTRACKS, playpos, playpos);
							updatePar |= DRAW_DATA;
							playpos = -1;
							InvalidateRect(rect,false);
						}
					}
					// header
					if (patView->PatHeaderCoords.bHasPlaying) {
						rect.top=0; 
						rect.left=XOFFSET;
						rect.bottom=YOFFSET-1;	
						rect.right=XOFFSET+maxt*ROWWIDTH;
						updatePar |= DRAW_TRHEADER;
						InvalidateRect(rect,false);
					}
				}
				break;
			case draw_modes::playback_change: 
				if (_pSong.playOrder[editPosition] == _pSong.playOrder[Global::player()._playPosition])
				{
					newplaypos= Global::player()._lineCounter;
				}
				else 
				{
					newplaypos=-1;
				}
				playpos=-1;
				rect.top=YOFFSET;		
				rect.left=0;
				rect.bottom=CH;
				rect.right=CW;
				updatePar |= DRAW_FULL_DATA;
				InvalidateRect(rect,false);
				SetPatternScrollBars(snt,plines);
				break;
			case draw_modes::selection: 
				// could optimize to only draw the changes
				if (blockSelected)
				{
					if ((blockSel.end.track<rntOff) || (blockSel.end.line<rnlOff) ||
						(blockSel.start.track>=rntOff+VISTRACKS) ||
						(blockSel.start.line>=rnlOff+VISLINES))
					{
						newselpos.bottom = 0; // This marks as "don't show selection" (because out of range)
					}
					else 
					{
						newselpos.top=blockSel.start.line;
						newselpos.bottom=blockSel.end.line+1;
						newselpos.left=blockSel.start.track;
						newselpos.right=blockSel.end.track+1;

						if (selpos.bottom == 0)
						{
							rect.left=XOFFSET+(blockSel.start.track-rntOff)*ROWWIDTH;
							rect.top=YOFFSET+(blockSel.start.line-rnlOff)*ROWHEIGHT;
							rect.right=XOFFSET+(blockSel.end.track-rntOff+1)*ROWWIDTH;
							rect.bottom=YOFFSET+(blockSel.end.line-rnlOff+1)*ROWHEIGHT;
							
							NewPatternDraw(blockSel.start.track, blockSel.end.track, blockSel.start.line, blockSel.end.line);
							updatePar |= DRAW_DATA;
							InvalidateRect(rect,false);
						}
						else if (newselpos != selpos)
						{
							rect.left = (newselpos.left < selpos.left) ? newselpos.left : selpos.left;
							rect.right = (newselpos.right > selpos.right) ? newselpos.right : selpos.right;
							rect.top = (newselpos.top < selpos.top) ? newselpos.top : selpos.top;
							rect.bottom = (newselpos.bottom > selpos.bottom) ? newselpos.bottom : selpos.bottom;
						
							//optimizations when only one side changes.
							if (tOff != rntOff && lOff != rnlOff) {
								if (newselpos.right == selpos.right && newselpos.top == selpos.top && newselpos.bottom == selpos.bottom) {
									rect.right = (newselpos.left < selpos.left) ? selpos.left : newselpos.left;
								}
								else if (newselpos.left < selpos.left && newselpos.top == selpos.top && newselpos.bottom == selpos.bottom) {
									rect.left = (newselpos.right > selpos.right) ? selpos.right : newselpos.right;
								}
								else if (newselpos.left < selpos.left && newselpos.right == selpos.right && newselpos.bottom == selpos.bottom) {
									rect.bottom = (newselpos.top < selpos.top) ? selpos.top : newselpos.top;
								}
								else if (newselpos.left < selpos.left && newselpos.right == selpos.right && newselpos.top == selpos.top ) {
									rect.top = (newselpos.bottom > selpos.bottom)? selpos.bottom : newselpos.bottom;
								}
							}
							NewPatternDraw(rect.left, rect.right, rect.top, rect.bottom);
							updatePar |= DRAW_DATA;
							rect.left = XOFFSET+(rect.left-rntOff)*ROWWIDTH;
							rect.right = XOFFSET+(rect.right-rntOff)*ROWWIDTH;
							rect.top=YOFFSET+(rect.top-rnlOff)*ROWHEIGHT;
							rect.bottom=YOFFSET+(rect.bottom-rnlOff)*ROWHEIGHT;
							InvalidateRect(rect,false);
						}						
					}
				}
				else if ( selpos.bottom != 0)
				{
					rect.left=XOFFSET+(selpos.left-rntOff)*ROWWIDTH;
					rect.top=YOFFSET+(selpos.top-rnlOff)*ROWHEIGHT;
					rect.right=XOFFSET+(selpos.right-rntOff)*ROWWIDTH;
					rect.bottom=YOFFSET+(selpos.bottom-rnlOff)*ROWHEIGHT;
					
					NewPatternDraw(selpos.left, selpos.right, selpos.top, selpos.bottom);
					updatePar |= DRAW_DATA;
					newselpos.bottom=0;
					InvalidateRect(rect,false);
				}
				break;
			case draw_modes::data: 
				{
					SPatternDraw* pPD = &pPatternDraw[numPatternDraw-1];
					
					rect.left=XOFFSET+  ((pPD->drawTrackStart-rntOff)*ROWWIDTH);
					rect.right=XOFFSET+ ((pPD->drawTrackEnd-(rntOff-1))*ROWWIDTH);
					rect.top=YOFFSET+	((pPD->drawLineStart-rnlOff)*ROWHEIGHT);
					rect.bottom=YOFFSET+((pPD->drawLineEnd-(rnlOff-1))*ROWHEIGHT);
					updatePar |= DRAW_DATA;
					InvalidateRect(rect,false);
				}
				break;
			case draw_modes::track_header: 
				// header
				rect.top=0; 
				rect.left=XOFFSET;
				rect.bottom=YOFFSET-1;	
				rect.right=XOFFSET+maxt*ROWWIDTH;
				updatePar |= DRAW_TRHEADER;
				InvalidateRect(rect,false);
				break;
		//	case draw_modes::cursor: 
		//		break;
			case draw_modes::none: 
				break;
			}

			if ((editcur.col != editlast.col) || (editcur.track != editlast.track) || (editcur.line != editlast.line))
			{
				rect.left = XOFFSET+(editcur.track-rntOff)*ROWWIDTH;
				rect.right = rect.left+ROWWIDTH;
				rect.top = YOFFSET+(editcur.line-rnlOff)*ROWHEIGHT;
				rect.bottom = rect.top+ROWWIDTH;
				NewPatternDraw(editcur.track, editcur.track, editcur.line, editcur.line);
				updatePar |= DRAW_DATA;
				InvalidateRect(rect,false);
				if (editcur.line != editlast.line)
				{
					if (XOFFSET!=1)
					{
						rect.left = 0;
						rect.right = XOFFSET;
						InvalidateRect(rect,false);
					}
					rect.left = XOFFSET+(editlast.track-rntOff)*ROWWIDTH;
					rect.right = rect.left+ROWWIDTH;
					rect.top = YOFFSET+(editlast.line-rnlOff)*ROWHEIGHT;
					rect.bottom = rect.top+ROWWIDTH;
					NewPatternDraw(editlast.track, editlast.track, editlast.line, editlast.line);
					InvalidateRect(rect,false);
					if (XOFFSET!=1)
					{
						rect.left = 0;
						rect.right = XOFFSET;
						InvalidateRect(rect,false);
					}
				}
				else if (editcur.track != editlast.track)
				{
					rect.left = XOFFSET+(editlast.track-rntOff)*ROWWIDTH;
					rect.right = rect.left+ROWWIDTH;
					rect.top = YOFFSET+(editlast.line-rnlOff)*ROWHEIGHT;
					rect.bottom = rect.top+ROWWIDTH;
					NewPatternDraw(editlast.track, editlast.track, editlast.line, editlast.line);
					InvalidateRect(rect,false);
				}
			}

			// turn off play line if not playing
			if (playpos >= 0 && !Global::player()._playing) 
			{
				newplaypos=-1;
				rect.top = YOFFSET+ (playpos-rnlOff)*ROWHEIGHT;
				rect.bottom = rect.top+ROWHEIGHT;
				rect.left = 0;
				rect.right = XOFFSET+(maxt)*ROWWIDTH;
				NewPatternDraw(0, _pSong.SONGTRACKS, playpos, playpos);
				playpos =-1;
				updatePar |= DRAW_DATA;
				InvalidateRect(rect,false);
			}

			////////////////////////////////////////////////////////////////////
			// Checks for specific code to update.

			SetScrollPos(SB_HORZ,rntOff);
			SetScrollPos(SB_VERT,rnlOff);
			//\todo: This line has been commented out because it is what is causing the breaks in sound when
			//  using follow song and the pattern changes. Commenting it out should only mean that the refresh
			//	is delayed for the next idle time, and not forced now, inside the timer, (which has a lock).
//			UpdateWindow();
		}




		#define DF_NONE			0
		#define	DF_SELECTION	1
		#define DF_PLAYBAR		2
		#define DF_CURSOR		4
		#define DF_DRAWN		15

		void CChildView::DrawPatEditor(CDC *devc)
		{
			///////////////////////////////////////////////////////////
			// Prepare pattern for update 
			CRect rect;
			CFont* oldFont;

			int scrollT= tOff-rntOff;
			int scrollL= lOff-rnlOff;

			tOff = ntOff = rntOff; 
			lOff = nlOff = rnlOff;

			oldFont= devc->SelectObject(&patView->pattern_font);

			// 1 if there is a redraw header, we do that 
			/////////////////////////////////////////////////////////////
			// Update Mute/Solo Indicators
			if ((updatePar & DRAW_TRHEADER) || (abs(scrollT) > VISTRACKS) || (scrollT && scrollL))
			{
				rect.top = 0;
				rect.bottom = YOFFSET;
				if (XOFFSET!=1)
				{
					rect.left = 0;
					rect.right = 1;
					devc->FillSolidRect(&rect,pvc_separator[0]);
					rect.left++;
					rect.right = XOFFSET-1;
					devc->FillSolidRect(&rect, pvc_background[0]);
					devc->SetBkColor(patView->background);	// This affects TXT background
					devc->SetTextColor(patView->font);
					TXT(devc,"Line",1,1,XOFFSET-2,YOFFSET-2);
				}

				int xOffset = XOFFSET-1;
				int iFirstIni = tOff;
				int iLastIni = tOff+maxt;
				for (int i = iFirstIni; i < iLastIni; i++,xOffset += ROWWIDTH)
				{
					rect.left = xOffset;
					rect.right = xOffset+1;
					devc->FillSolidRect(&rect, pvc_separator[i+1]);
					rect.left++;
					rect.right+= ROWWIDTH-1;
					devc->FillSolidRect(&rect,pvc_background[i+1]);
				}

				DrawPatternHeader(devc,XOFFSET-1, iFirstIni, iLastIni);
			}

			// 2 if there is a redraw all, we do that then exit
			if ((updatePar & DRAW_FULL_DATA) || (abs(scrollT) > VISTRACKS) || (abs(scrollL) > VISLINES) || (scrollT && scrollL))
			{
		#ifdef _DEBUG_PATVIEW
				TRACE("DRAW_FULL_DATA\n");
		#endif
				// draw everything
				rect.top = YOFFSET;
				rect.bottom = CH;

				if (XOFFSET!=1)
				{
					rect.left = 0;
					rect.right = 1;
					devc->FillSolidRect(&rect,pvc_separator[0]);
					rect.left++;
					rect.right = XOFFSET-1;
					devc->FillSolidRect(&rect,pvc_background[0]);
				}
				int xOffset = XOFFSET-1;

				for (int i=tOff;i<tOff+maxt;i++)
				{
					rect.left = xOffset;
					rect.right = xOffset+1;
					devc->FillSolidRect(&rect,pvc_separator[i+1]);
					rect.left++;
					rect.right+= ROWWIDTH-1;
					devc->FillSolidRect(&rect,pvc_background[i+1]);

					xOffset += ROWWIDTH;
				}
				DrawPatternData(devc,0,VISTRACKS+1,0,VISLINES+1);
				// wipe todo list
				numPatternDraw = 0;
				// Fill Bottom Space with Background colour if needed
				if (maxl < VISLINES+1)
				{
		#ifdef _DEBUG_PATVIEW
					TRACE("DRAW_BOTTOM\n");
		#endif
					if (XOFFSET!=1)
					{
						rect.left=0; 
						rect.right=XOFFSET; 
						rect.top=YOFFSET+(maxl*ROWHEIGHT); 
						rect.bottom=CH;
						devc->FillSolidRect(&rect,pvc_separator[0]);
					}

					int xOffset = XOFFSET;

					rect.top=YOFFSET+(maxl*ROWHEIGHT); 
					rect.bottom=CH;
					for(int i=tOff;i<tOff+maxt;i++)
					{
						rect.left=xOffset; 
						rect.right=xOffset+ROWWIDTH; 
						devc->FillSolidRect(&rect,pvc_separator[i+1]);
						xOffset += ROWWIDTH;
					}
				}
				// Fill Right Space with Background colour if needed
				if (maxt < VISTRACKS+1)
				{
		#ifdef _DEBUG_PATVIEW
					TRACE("DRAW_RIGHT\n");
		#endif
					rect.top=0; 
					rect.bottom=CH;  
					rect.right=CW;
					rect.left=XOFFSET+(maxt*ROWWIDTH)-1;
					devc->FillSolidRect(&rect,patView->separator2);
				}
			}
			else
			{
				//There is no (scrollT && scrollL) case because it is don in the if above.
				// h scroll - remember to check the header when scrolling H so no double blits
				//			  add to draw list uncovered area
				if (scrollT)
				{
					CRgn rgn;
					int xOffsetIni,iFirstIni,iLastIni,drawStart,drawEnd;
					if (scrollT > 0) {	//Scrolling to the left
						xOffsetIni = XOFFSET-1;
						iFirstIni = tOff;
						iLastIni = tOff+scrollT;
						drawStart = 0;
						drawEnd = scrollT;
					}
					else {	//Scrolling to the right
						xOffsetIni = XOFFSET-1+((VISTRACKS+scrollT)*ROWWIDTH);
						iFirstIni = tOff+maxt+scrollT-1;
						iLastIni = tOff+maxt;
						drawStart = VISTRACKS+scrollT;
						drawEnd = VISTRACKS+1;
					}

					if (updatePar & DRAW_TRHEADER)
					{
						 // In this case, we have alredy drawn the whole header above so we only need toscroll and paint the text.
	#ifdef _DEBUG_PATVIEW
							TRACE("DRAW_HSCROLL+\n");
	#endif
						const RECT patR = {XOFFSET,YOFFSET , CW, CH};
						devc->ScrollDC(scrollT*ROWWIDTH,NULL,&patR,&patR,&rgn,&rect);

						rect.top = YOFFSET;
						rect.bottom = CH;
						int xOffset = xOffsetIni;
						for (int i = iFirstIni; i < iLastIni; i++,xOffset += ROWWIDTH)
						{
							rect.left = xOffset;
							rect.right = xOffset+1;
							devc->FillSolidRect(&rect, pvc_separator[i+1]);
							rect.left++;
							rect.right+= ROWWIDTH-1;
							devc->FillSolidRect(&rect,pvc_background[i+1]);
						}
						DrawPatternData(devc,drawStart, drawEnd, 0, VISLINES+1);
					}
					else
					{
#ifdef _DEBUG_PATVIEW
						TRACE("DRAW_HSCROLL+\n");
#endif
						// scroll header too
						const RECT trkR = {XOFFSET, 0, CW, CH};
						devc->ScrollDC(scrollT*ROWWIDTH,NULL,&trkR,&trkR,&rgn,&rect);

						rect.top = 0;
						rect.bottom = CH;
						int xOffset = xOffsetIni;
						for (int i = iFirstIni; i < iLastIni; i++, xOffset += ROWWIDTH)
						{
							rect.left = xOffset;
							rect.right = xOffset+1;
							devc->FillSolidRect(&rect, pvc_separator[i+1]);
							rect.left++;
							rect.right+= ROWWIDTH-1;
							devc->FillSolidRect(&rect,pvc_background[i+1]);
						}
						DrawPatternHeader(devc,xOffsetIni, iFirstIni, iLastIni);

						DrawPatternData(devc,drawStart, drawEnd, 0, VISLINES+1);
					}
					// Fill Bottom Space with Background colour if needed
					if (maxl < VISLINES+1)
					{
						int xOffset = XOFFSET;
						CRect rect;
						rect.top=YOFFSET+(maxl*ROWHEIGHT); 
						rect.bottom=CH;
						for(int i=tOff;i<tOff+maxt;i++)
						{
							rect.left=xOffset; 
							rect.right=xOffset+ROWWIDTH; 
							devc->FillSolidRect(&rect, pvc_separator[i+1]);
							xOffset += ROWWIDTH;
						}
					}
					// Fill Right Space with Background colour if needed
					if (maxt < VISTRACKS+1)
					{
	#ifdef _DEBUG_PATVIEW
						TRACE("DRAW_RIGHT\n");
	#endif
						CRect rect;
						rect.top=0; 
						rect.bottom=CH;  
						rect.right=CW;
						rect.left=XOFFSET+(maxt*ROWWIDTH)-1;
						devc->FillSolidRect(&rect,patView->separator2);
					}
				}

				// v scroll - 
				//			  add to draw list uncovered area
				else if (scrollL)
				{
					const RECT linR = {0, YOFFSET, CW, CH};

					CRgn rgn;
					devc->ScrollDC(0,scrollL*ROWHEIGHT,&linR,&linR,&rgn,&rect);
					// add visible part to 
					if (scrollL > 0)	//Scrolling to the top
					{	
	#ifdef _DEBUG_PATVIEW
						TRACE("DRAW_VSCROLL+\n");
	#endif
						//if(editcur.line!=0)
						DrawPatternData(devc, 0, VISTRACKS+1, 0,scrollL);
					}
					else 	//Scrolling to the bottom
					{	
	#ifdef _DEBUG_PATVIEW
						TRACE("DRAW_VSCROLL-\n");
	#endif
						DrawPatternData(devc, 0, VISTRACKS+1,VISLINES+scrollL,VISLINES+1);
					}
					// Fill Bottom Space with Background colour if needed
					if (maxl < VISLINES+1)
					{
	#ifdef _DEBUG_PATVIEW
						TRACE("DRAW_BOTTOM\n");
	#endif
						if (XOFFSET!=1)
						{
							CRect rect;
							rect.left=0; 
							rect.right=XOFFSET; 
							rect.top=YOFFSET+(maxl*ROWHEIGHT); 
							rect.bottom=CH;
							devc->FillSolidRect(&rect, pvc_separator[0]);
						}

						int xOffset = XOFFSET;

						CRect rect;
						rect.top=YOFFSET+(maxl*ROWHEIGHT); 
						rect.bottom=CH;
						for(int i=tOff;i<tOff+maxt;i++)
						{
							rect.left=xOffset; 
							rect.right=xOffset+ROWWIDTH; 
							devc->FillSolidRect(&rect, pvc_separator[i+1]);
							xOffset += ROWWIDTH;
						}
					}
				}
			}

			// then we draw any data that needs to be drawn
			// each time we draw data check for playbar or cursor, not fast but...
			// better idea is to have an array of flags, so never draw twice
			////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////
			// Draw Pattern data.
			if (updatePar & DRAW_DATA)
			{
	#ifdef _DEBUG_PATVIEW
				TRACE("DRAW_DATA\n");
	#endif
				////////////////////////////////////////////////
				// Draw Data Changed (draw_modes::dataChange)
				for (int i = 0; i < numPatternDraw; i++)
				{

					int ts = pPatternDraw[i].drawTrackStart-tOff;
					if ( ts < 0 ) 
						ts = 0;
					int te = pPatternDraw[i].drawTrackEnd -(tOff-1);
					if ( te > maxt ) 
						te = maxt;

					int ls = pPatternDraw[i].drawLineStart-lOff;
					if ( ls < 0 ) 
						ls = 0;
					int le = pPatternDraw[i].drawLineEnd-(lOff-1);
					if ( le > maxl ) 
						le = maxl;

					DrawPatternData(devc,ts,te,ls,le);
				}
				numPatternDraw = 0;
			}

			playpos=newplaypos;
			selpos=newselpos;
			editlast=editcur;

			devc->SelectObject(oldFont);

			updateMode = draw_modes::none;
			updatePar = 0;
		}

		void CChildView::DrawPatternHeader(CDC *devc,int xOffsetIni,int iFirstIni, int iLastIni)
		{
			char buffer[256];
			CDC memDC;
			CBitmap *oldbmp;
			memDC.CreateCompatibleDC(devc);
			oldbmp = memDC.SelectObject(&patView->patternheader);
			int xOffset = xOffsetIni;

			if (!patView->showTrackNames_ || PatHeaderCoords->bHasTextSkin) 
			{
				if (PatHeaderCoords->bHasTransparency)
				{
					for(int i=iFirstIni;i<iLastIni;i++, xOffset += ROWWIDTH)
					{
						const int trackx0 = i/10;
						const int track0x = i%10;
						const int realxOff = xOffset+1+HEADER_INDENT;

						TransparentBltSkin(devc, PatHeaderCoords->sBackground, &memDC,
							&patView->patternheadermask, 0, 0, realxOff, 1);

						if (_pSong._trackMuted[i]) {
							TransparentBltSkin(devc, PatHeaderCoords->sMuteOn, PatHeaderCoords->dMuteOn, &memDC,
								&patView->patternheadermask, 0, 0, realxOff, 1);
						}
						if (_pSong._trackArmed[i]) {
							TransparentBltSkin(devc, PatHeaderCoords->sRecordOn, PatHeaderCoords->dRecordOn, &memDC,
								&patView->patternheadermask, 0, 0, realxOff, 1);
						}
						if (_pSong._trackSoloed == i ) {
							TransparentBltSkin(devc, PatHeaderCoords->sSoloOn, PatHeaderCoords->dSoloOn, &memDC,
								&patView->patternheadermask, 0, 0, realxOff, 1);
						}
						if (Global::player().trackPlaying(i) && PatHeaderCoords->bHasPlaying) {
							TransparentBltSkin(devc, PatHeaderCoords->sPlayOn, PatHeaderCoords->dPlayOn, &memDC,
								&patView->patternheadermask, 0, 0, realxOff, 1);
						}
						if (PatHeaderCoords->sNumber0.width > 0 ) {
							TransparentBltSkin(devc, PatHeaderCoords->sNumber0, PatHeaderCoords->dDigitX0, &memDC,
								&patView->patternheadermask, trackx0*PatHeaderCoords->sNumber0.width, 0, realxOff, 1);

							TransparentBltSkin(devc, PatHeaderCoords->sNumber0, PatHeaderCoords->dDigit0X, &memDC,
								&patView->patternheadermask, track0x*PatHeaderCoords->sNumber0.width, 0, realxOff, 1);
						}
					}
				}
				else
				{
					for(int i=iFirstIni;i<iLastIni;i++,xOffset += ROWWIDTH)
					{
						const int trackx0 = i/10;
						const int track0x = i%10;
						const int realxOff = xOffset+1+HEADER_INDENT;

						BitBltSkin(devc, PatHeaderCoords->sBackground, &memDC,0,0,realxOff,1);
						if (_pSong._trackMuted[i]) {
							BitBltSkin(devc, PatHeaderCoords->sMuteOn, PatHeaderCoords->dMuteOn,&memDC, 0,0,realxOff,1);
						}
						if (_pSong._trackArmed[i]) {
							BitBltSkin(devc, PatHeaderCoords->sRecordOn, PatHeaderCoords->dRecordOn,&memDC,0,0,realxOff,1);
						}
						if (_pSong._trackSoloed == i ) {
							BitBltSkin(devc, PatHeaderCoords->sSoloOn, PatHeaderCoords->dSoloOn,&memDC,0,0,realxOff,1);
						}
						if (Global::player().trackPlaying(i) && PatHeaderCoords->bHasPlaying) {
							BitBltSkin(devc, PatHeaderCoords->sPlayOn, PatHeaderCoords->dPlayOn,&memDC,0,0,realxOff,1);
						}
						if (PatHeaderCoords->sNumber0.width > 0 ) {
							BitBltSkin(devc, PatHeaderCoords->sNumber0, PatHeaderCoords->dDigitX0, &memDC,
								trackx0*PatHeaderCoords->sNumber0.width,0,realxOff,1);
							BitBltSkin(devc, PatHeaderCoords->sNumber0, PatHeaderCoords->dDigit0X, &memDC,
								track0x*PatHeaderCoords->sNumber0.width,0,realxOff,1);
						}
					}
				}
			}
			xOffset = xOffsetIni;
			if (patView->showTrackNames_) {
				CFont * oldFont = NULL;
				if (PatHeaderCoords->bHasTextSkin) {
					oldFont= devc->SelectObject(&patView->pattern_tracknames_font);
					devc->SetTextColor(PatHeaderCoords->cTextFontColour);
					devc->SetBkMode(TRANSPARENT);
				}
				if (PatHeaderCoords->sNumber0.width > 0 ) {
					for(int i=iFirstIni;i<iLastIni;i++, xOffset += ROWWIDTH)
					{
						sprintf(buffer,"%s",_pSong._trackNames[_ps()][i].c_str());
						TXTTRANS(devc,buffer,xOffset+PatHeaderCoords->sTextCrop.x,PatHeaderCoords->sTextCrop.y,
							PatHeaderCoords->sTextCrop.width,PatHeaderCoords->sTextCrop.height);
					}
				}
				else {
					for(int i=iFirstIni;i<iLastIni;i++, xOffset += ROWWIDTH)
					{
						if (_pSong._trackMuted[i]) {
							sprintf(buffer,"%.2d_%s",i,_pSong._trackNames[_ps()][i].c_str());
						}
						else if (_pSong._trackSoloed == i ) {
							sprintf(buffer,"%.2d*%s",i,_pSong._trackNames[_ps()][i].c_str());
						}
						else {
							sprintf(buffer,"%.2d:%s",i,_pSong._trackNames[_ps()][i].c_str());
						}
						TXT(devc,buffer,xOffset+PatHeaderCoords->sTextCrop.x,PatHeaderCoords->sTextCrop.y,
							PatHeaderCoords->sTextCrop.width,PatHeaderCoords->sTextCrop.height);
					}
				}
				if (PatHeaderCoords->bHasTextSkin) {
					devc->SelectObject(oldFont);
					devc->SetBkMode(OPAQUE);
				}
			}
			xOffset = xOffsetIni;
			if (PatHeaderCoords->bHasTransparency)
			{
				for(int i=iFirstIni;i<iLastIni;i++, xOffset += ROWWIDTH)
				{
					const int realxOff = xOffset+1+HEADER_INDENT;
					if (trackingMuteTrack == i) {
						TransparentBltSkin(devc, PatHeaderCoords->sMuteOnTracking, PatHeaderCoords->dMuteOnTrack, &memDC,
							&patView->patternheadermask, 0, 0, realxOff, 1);
					}
					if (trackingRecordTrack == i) {
						TransparentBltSkin(devc, PatHeaderCoords->sRecordOnTracking, PatHeaderCoords->dRecordOnTrack, &memDC,
							&patView->patternheadermask, 0, 0, realxOff, 1);
					}
					if (trackingSoloTrack == i) {
						TransparentBltSkin(devc, PatHeaderCoords->sSoloOnTracking, PatHeaderCoords->dSoloOnTrack, &memDC,
							&patView->patternheadermask, 0, 0, realxOff, 1);
					}
				}
			}
			else
			{
				for(int i=iFirstIni;i<iLastIni;i++, xOffset += ROWWIDTH)
				{
					const int realxOff = xOffset+1+HEADER_INDENT;
					if (trackingMuteTrack==i) {
						BitBltSkin(devc, PatHeaderCoords->sMuteOnTracking, PatHeaderCoords->dMuteOnTrack,&memDC,0,0,realxOff,1);
					}
					if (trackingRecordTrack==i) {
						BitBltSkin(devc, PatHeaderCoords->sRecordOnTracking, PatHeaderCoords->dRecordOnTrack,&memDC,0,0,realxOff,1);
					}
					if (trackingSoloTrack==i) {
						BitBltSkin(devc, PatHeaderCoords->sSoloOnTracking, PatHeaderCoords->dSoloOnTrack,&memDC,0,0,realxOff,1);
					}
				}
			}

			memDC.SelectObject(oldbmp);
			memDC.DeleteDC();
		}

		// ADVISE! [lOff+lstart..lOff+lend] and [tOff+tstart..tOff+tend] HAVE TO be valid!
		void CChildView::DrawPatternData(CDC *devc,int tstart,int tend, int lstart, int lend)
		{

		#ifdef _DEBUG_PATVIEW
			TRACE("DrawPatternData\n");
		#endif

		//	if (lstart > VISLINES)
			if (lstart > maxl)
			{
				return;
			}
			else if (lstart < 0)
			{
				lstart = 0;
			}


			if (lend < 0)
			{
				return;
			}
			else if (lend > maxl)
		//	else if (lend > VISLINES+1)
			{
		//		lend = VISLINES+1;
				lend = maxl;
			}

		//	if (tstart > VISTRACKS)
			if (tstart > maxt)
			{
				return;
			}
			else if (tstart < 0)
			{
				tstart = 0;
			}

			if (tend < 0)
			{
				return;
			}
		//	else if (tend > VISTRACKS+1)
			else if (tend > maxt)
			{
		//		tend = VISTRACKS+1;
				tend = maxt;
			}

			int yOffset=lstart*ROWHEIGHT+YOFFSET;
			int linecount=lOff+ lstart;
			char tBuf[16];

			COLORREF* pBkg;
			int linesPerBar = (_pSong.LinesPerBeat()*patView->timesig);
			char* linecountformat;

			if (patView->_linenumbersHex) {
				linecountformat="%.2X";
			} else {
				linecountformat="%3i";
			}

			for (int i=lstart;i<lend;i++) // Lines
			{
				// break this up into several more general loops for speed
				if((linecount%_pSong.LinesPerBeat()) == 0)
				{
					if ((linecount%linesPerBar) == 0) 
						pBkg = pvc_row4beat;
					else 
						pBkg = pvc_rowbeat;
				}
				else
				{
					pBkg = pvc_row;
				}

				if ((XOFFSET!=1))// && (tstart == 0))
				{
					if ((linecount == editcur.line) && (patView->_linenumbersCursor))
					{
						devc->SetBkColor(pvc_cursor[0]);
						devc->SetTextColor(pvc_fontCur[0]);
					}
					else if (linecount == newplaypos)
					{
						devc->SetBkColor(pvc_playbar[0]);
						devc->SetTextColor(pvc_fontPlay[0]);
					}
					else 
					{
						devc->SetBkColor(pBkg[0]);
						devc->SetTextColor(pvc_font[0]);
					}
					sprintf(tBuf,linecountformat,linecount);
					TXTFLAT(devc,tBuf,1,yOffset,XOFFSET-2,ROWHEIGHT-1);	// Print Line Number.
				}

				unsigned char *patOffset = _ppattern() +
											(linecount*MULTIPLY) + 	(tstart+tOff)*EVENT_SIZE;

				int xOffset= XOFFSET+(tstart*ROWWIDTH);

				int trackcount = tstart+tOff;
				for (int t=tstart;t<tend;t++)
				{
					if (linecount == newplaypos)
					{
						devc->SetBkColor(pvc_playbar[trackcount]);
						devc->SetTextColor(pvc_fontPlay[trackcount]);
					}
					else if ((linecount >= newselpos.top) && 
						(linecount < newselpos.bottom) &&
						(trackcount >= newselpos.left) &&
						(trackcount < newselpos.right))
					{

						if (pBkg == pvc_rowbeat)
						{
							devc->SetBkColor(pvc_selectionbeat[trackcount]);
						}
						else if (pBkg == pvc_row4beat)
						{
							devc->SetBkColor(pvc_selection4beat[trackcount]);
						}
						else
						{
							devc->SetBkColor(pvc_selection[trackcount]);
						}
						devc->SetTextColor(pvc_fontSel[trackcount]);
					}
					else
					{
						devc->SetBkColor(pBkg[trackcount]);
						devc->SetTextColor(pvc_font[trackcount]);
					}
					OutNote(devc,xOffset+COLX[0],yOffset,*patOffset);
					patOffset++;
					OutData(devc,xOffset+COLX[1],yOffset,*patOffset,*(patOffset) == 255);
					patOffset++;
					OutData(devc,xOffset+COLX[3],yOffset,*patOffset,*(patOffset) == 255);
					patOffset++;
					bool trflag = *(patOffset) == 0 && *(patOffset+1) == 0 && 
						(*(patOffset-3) <= notecommands::release || *(patOffset-3) == 255 );
					OutData(devc,xOffset+COLX[5],yOffset,*patOffset++,trflag);
					OutData(devc,xOffset+COLX[7],yOffset,*patOffset++,trflag);

					// could optimize this check some, make separate loops
					if ((linecount == editcur.line) && (trackcount == editcur.track))
					{
						patOffset-=5;
						devc->SetBkColor(pvc_cursor[trackcount]);
						devc->SetTextColor(pvc_fontCur[trackcount]);
						switch (editcur.col)
						{
						case 0:
							OutNote(devc,xOffset+COLX[0],yOffset,*(patOffset));
							break;
						case 1:
							OutData4(devc,xOffset+COLX[1],yOffset,(*(patOffset+1))>>4,*(patOffset+1) == 255);
							break;
						case 2:
							OutData4(devc,xOffset+COLX[2],yOffset,(*(patOffset+1))&0x0F,*(patOffset+1) == 255);
							break;
						case 3:
							OutData4(devc,xOffset+COLX[3],yOffset,(*(patOffset+2))>>4,*(patOffset+2) == 255);
							break;
						case 4:
							OutData4(devc,xOffset+COLX[4],yOffset,(*(patOffset+2))&0x0F,*(patOffset+2) == 255);
							break;
						case 5:
							OutData4(devc,xOffset+COLX[5],yOffset,(*(patOffset+3))>>4,trflag);
							break;
						case 6:
							OutData4(devc,xOffset+COLX[6],yOffset,(*(patOffset+3))&0x0F,trflag);
							break;
						case 7:
							OutData4(devc,xOffset+COLX[7],yOffset,(*(patOffset+4))>>4,trflag);
							break;
						case 8:
							OutData4(devc,xOffset+COLX[8],yOffset,(*(patOffset+4))&0x0F,trflag);
							break;
						}
						patOffset+=5;
					}
					trackcount++;
					xOffset+=ROWWIDTH;
				}
				linecount++;
				yOffset+=ROWHEIGHT;
			}
		}

		void CChildView::NewPatternDraw(int drawTrackStart, int drawTrackEnd, int drawLineStart, int drawLineEnd)
		{
			if (viewMode == view_modes::pattern)
			{
				if (!(updatePar & DRAW_FULL_DATA))
				{
					// inserts pattern data to be drawn into the list
					if (numPatternDraw < MAX_DRAW_MESSAGES)
					{
						for (int i=0; i < numPatternDraw; i++)
						{
							if ((pPatternDraw[i].drawTrackStart <= drawTrackStart) &&
								(pPatternDraw[i].drawTrackEnd >= drawTrackEnd) &&
								(pPatternDraw[i].drawLineStart <= drawLineStart) &&
								(pPatternDraw[i].drawLineEnd >= drawLineEnd))
							{
								return;
							}
						}
						pPatternDraw[numPatternDraw].drawTrackStart = drawTrackStart;
						pPatternDraw[numPatternDraw].drawTrackEnd = drawTrackEnd;
						pPatternDraw[numPatternDraw].drawLineStart = drawLineStart;
						pPatternDraw[numPatternDraw].drawLineEnd = drawLineEnd;
						numPatternDraw++;
					}
					else if (numPatternDraw == MAX_DRAW_MESSAGES)
					{
						// this should never have to happen with a 32 message buffer, but just incase....
						numPatternDraw++;
						PreparePatternRefresh(draw_modes::all);
					}
				}
			}
			else
			{
				numPatternDraw=0;
			}
		}

		void CChildView::RecalculateColour(COLORREF* pDest, COLORREF source1, COLORREF source2)
		{
			// makes an array of colours between source1 and source2
			float p0 = float((source1>>16)&0xff);
			float p1 = float((source1>>8)&0xff);
			float p2 = float(source1&0xff);

			float d0 = float((source2>>16)&0xff);
			float d1 = float((source2>>8)&0xff);
			float d2 = float(source2&0xff);

			int len = _pSong.SONGTRACKS+1;

			float a0=(d0-p0)/(len);
			float a1=(d1-p1)/(len);
			float a2=(d2-p2)/(len);

			for (int i = 0; i < len; i++)
			{
				pDest[i] = (helpers::math::round<int,float>(p0*0x10000)&0xff0000)
							| (helpers::math::round<int,float>(p1*0x100)&0xff00)
							| (helpers::math::round<int,float>(p2)&0xff);
				p0+=a0;
				p1+=a1;
				p2+=a2;

				if (p0 < 0)
				{
					p0 = 0;
				}
				else if (p0 > 255)
				{
					p0 = 255;
				}

				if (p1 < 0)
				{
					p1 = 0;
				}
				else if (p1 > 255)
				{
					p1 = 255;
				}

				if (p2 < 0)
				{
					p2 = 2;
				}
				else if (p2 > 255)
				{
					p2 = 255;
				}
			}
		}

		COLORREF CChildView::ColourDiffAdd(COLORREF base, COLORREF adjust, COLORREF add)
		{
			int a0 = ((add>>16)&0x0ff)+((adjust>>16)&0x0ff)-((base>>16)&0x0ff);
			int a1 = ((add>>8 )&0x0ff)+((adjust>>8 )&0x0ff)-((base>>8 )&0x0ff);
			int a2 = ((add    )&0x0ff)+((adjust    )&0x0ff)-((base    )&0x0ff);

			if (a0 < 0)
			{
				a0 = 0;
			}
			else if (a0 > 255)
			{
				a0 = 255;
			}

			if (a1 < 0)
			{
				a1 = 0;
			}
			else if (a1 > 255)
			{
				a1 = 255;
			}

			if (a2 < 0)
			{
				a2 = 0;
			}
			else if (a2 > 255)
			{
				a2 = 255;
			}

			COLORREF pa = (a0<<16) | (a1<<8) | (a2);
			return pa;
		}

		void CChildView::RecalculateColourGrid()
		{
			RecalculateColour(pvc_background, patView->background, patView->background2);
			RecalculateColour(pvc_separator, patView->separator, patView->separator2);
			RecalculateColour(pvc_row4beat, patView->row4beat, patView->row4beat2);
			RecalculateColour(pvc_rowbeat, patView->rowbeat, patView->rowbeat2);
			RecalculateColour(pvc_row, patView->row, patView->row2);
			RecalculateColour(pvc_selection, patView->selection, patView->selection2);
			RecalculateColour(pvc_playbar, patView->playbar, patView->playbar2);
			RecalculateColour(pvc_cursor, patView->cursor, patView->cursor2);
			RecalculateColour(pvc_font, patView->font, patView->font2);
			RecalculateColour(pvc_fontPlay, patView->fontPlay, patView->fontPlay2);
			RecalculateColour(pvc_fontCur, patView->fontCur, patView->fontCur2);
			RecalculateColour(pvc_fontSel, patView->fontSel, patView->fontSel2);
			RecalculateColour(pvc_selectionbeat, ColourDiffAdd(patView->row, patView->rowbeat, patView->selection), ColourDiffAdd(patView->row2, patView->rowbeat2, patView->selection2));
			RecalculateColour(pvc_selection4beat, ColourDiffAdd(patView->row, patView->row4beat, patView->selection), ColourDiffAdd(patView->row2, patView->row4beat2, patView->selection2));
		}
		void CChildView::SetPatternScrollBars(int snt, int plines)
		{
			if ( snt > VISTRACKS )
			{	
				SCROLLINFO si;
				si.cbSize = sizeof(SCROLLINFO);
				si.fMask = SIF_PAGE | SIF_RANGE;
				si.nMin = 0;
				si.nMax = snt-VISTRACKS;
				si.nPage = 1;
				SetScrollInfo(SB_HORZ,&si);
				ShowScrollBar(SB_HORZ,TRUE);
			}
			else
			{	
				ShowScrollBar(SB_HORZ,FALSE); 
			}

			if ( plines > VISLINES )
			{	
				SCROLLINFO si;
				si.cbSize = sizeof(SCROLLINFO);
				si.fMask = SIF_PAGE | SIF_RANGE;
				si.nMin = 0;
				si.nMax = plines-VISLINES;
				si.nPage = 1;
				SetScrollInfo(SB_VERT,&si);
				ShowScrollBar(SB_VERT,TRUE);
			}
			else
			{	
				ShowScrollBar(SB_VERT,FALSE); 
			}
		}

		void CChildView::ShowTrackNames(bool show)
		{
			if(show) { PatHeaderCoords->SwitchToText(); }
			else { PatHeaderCoords->SwitchToClassic(); }
			if (!PatHeaderCoords->bHasTextSkin) {
				PatHeaderCoords->sTextCrop.x = COLX[0];
			}
			patView->showTrackNames_ = show;
			RecalcMetrics();
			Repaint();
		}
}}
