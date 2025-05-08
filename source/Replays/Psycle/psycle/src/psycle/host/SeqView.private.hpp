///\file
///\brief sequencer view graphic operations for psycle::host::CChildView, private header
#pragma once
#include <psycle/host/detail/project.hpp>
namespace psycle {
namespace host {

		#define SEQ_ROWHEIGHT  17
		#define SEQ_YOFFSET 17

		// Update seqTickOffs and seqGenOffs when changing seqSteps
		// "seqTickOffs -= (seqTickOffs%seqSteps)"
		// Update seqTick and seqGen ?


		void CChildView::DrawSeqEditor(CDC *devc)
		{
		/// the following vars go outside (childview)
			
		int seqTracksHoriz = 1;
		int seqSteps = 4;
		int seqGen = 0;  // Cursor 
		int seqTick = 0; // ^ ^ ^
		int seqGenOffs = 0;  // Scroll
		int seqTickOffs = 0; // ^ ^ ^
			
		int SEQ_XOFFSET = 96;   // These two are variables, since changing from 
		int SEQ_ROWWIDTH = 40;  // horizontal to vertical, makes them different.
		int seqNumRows=CH/SEQ_ROWHEIGHT; seqNumRows; // not used
		int seqNumCols=CW/SEQ_ROWWIDTH;

		int seqGenPos[MAX_MACHINES];

		////
			
		CRect rect;
		int curXoffset=SEQ_XOFFSET;
		int curYoffset=SEQ_YOFFSET;
			
		int i,j,k,m;
		float curcolf;
		int curcol;
		char cbuffer[128];
			
		CFont* oldFont;
			
			oldFont= devc->SelectObject(&patView->pattern_font);
			
		/////////////////////////// Draw Background
			rect.top=0;
			rect.left=0;
			rect.bottom=CH;
			rect.right=CW;
			devc->FillSolidRect(&rect,patView->background);

		/////////////////////////// Draw (Columns') Header

			rect.top=0;
			rect.bottom=SEQ_YOFFSET;
			rect.left=0;
			rect.right=CW;
			devc->FillSolidRect(&rect,patView->separator);

			devc->SetBkColor(patView->background);
			devc->SetTextColor(patView->font);
			
			if ( seqTracksHoriz ) 
			{
				TXT(devc,"Gear \\ Beat",0,0,SEQ_XOFFSET-1,SEQ_YOFFSET-1);

				curXoffset=SEQ_XOFFSET;
				curcol = seqTickOffs;
				for (i=0;i<seqNumCols;i++)
				{
					//rect.left= curXoffset;
					//rect.right= curXoffset+SEQ_ROWWIDTH;
					//devc->FillSolidRect(&rect,patView->separator);
					
					devc->SetBkColor(patView->rowbeat);	// This affects TXT background
					sprintf(cbuffer,"%d",curcol+1);
					TXT(devc,cbuffer,curXoffset,0,SEQ_ROWWIDTH-1,SEQ_ROWHEIGHT-1);
					
					curcol+=seqSteps;
					curXoffset+=SEQ_ROWWIDTH;
				}
			}
			else
			{
				TXT(devc,"Beat \\ Group ",0,0,SEQ_XOFFSET-1,SEQ_YOFFSET-1);
			}

		/////////////////////////// Draw Lines
			
			if ( seqTracksHoriz )
			{
				curYoffset=SEQ_YOFFSET;
				m=0;
				for(i=0; i<MAX_MACHINES; i++)
				{
					curXoffset=SEQ_XOFFSET;
					Machine *tmac = _pSong._pMachine[i];
					if (tmac)
					{
						seqGenPos[m++]=i;

						rect.top=curYoffset;
						rect.left=0;
						rect.bottom=curYoffset+SEQ_ROWHEIGHT;
						rect.right=CW;
						devc->FillSolidRect(&rect,patView->separator);
						
		//				rect.right= SEQ_XOFFSET;
		//				devc->FillSolidRect(&rect,patView->rowbeat);
						devc->SetBkColor(patView->rowbeat);
						devc->SetTextColor(patView->font);
						sprintf(cbuffer,"%.02X:%s",i,tmac->_editName);
						TXT(devc,cbuffer,0,curYoffset,SEQ_XOFFSET-1,SEQ_ROWHEIGHT-1);

					/////////////////////////////////////////// Add Draw patterns.(NEED TO REDO WITH MULTISEQUENCE!)
		//				rect.top=curYoffset;
		//				rect.bottom=curYoffset+SEQ_ROWHEIGHT;
						rect.bottom--;
						if (i == MASTER_INDEX)
						{

							curcol = seqTickOffs; // this will be used to know where to start drawing the pattern
							j=0;curcolf=0;
							while (curXoffset < CW && j < _pSong.playLength)
							{
								// Needs modification, since the patterns don't need to come one after the other in Multipattern sequence.

								int psize=(_pSong.patternLines[_pSong.playOrder[j]]*SEQ_ROWWIDTH)/(_pSong.LinesPerBeat()*seqSteps);
								rect.left=curXoffset;
								//rect.right=curXoffset+psize;
								//devc->FillSolidRect(rect,patView->row4beat);
								devc->SetBkColor(patView->row4beat);
								sprintf(cbuffer,"%.2d:%s",_pSong.playOrder[j],_pSong.patternName[_pSong.playOrder[j]]);
								TXT(devc,cbuffer,curXoffset,curYoffset,psize-1,SEQ_ROWHEIGHT-1);
								curXoffset+=psize;
								j++;
								curcolf+=(float)psize/SEQ_ROWWIDTH;
							}
							if ( curcolf < seqNumCols )
							{
								if ( SEQ_XOFFSET+(int)curcolf*SEQ_ROWWIDTH < curXoffset) // Remaining block to draw.
								{
									curcolf=(int)curcolf;
									rect.left=curXoffset;
									rect.right=	SEQ_XOFFSET+((curcolf+1)*SEQ_ROWWIDTH)-1;
									devc->FillSolidRect(rect,patView->row);
									curXoffset=rect.right+1;
									curcolf+=1;
								}
								while ( curcolf < seqNumCols )
								{
									rect.left=curXoffset;
									rect.right=curXoffset+SEQ_ROWWIDTH-1;
									devc->FillSolidRect(rect,patView->row);
									curXoffset+=SEQ_ROWWIDTH;
									curcolf+=1;
								}
							}
						}
						else
						{
							for (j=0;j<seqNumCols;j++)
							{
								rect.left=curXoffset;
								rect.right=curXoffset+SEQ_ROWWIDTH-1;
								devc->FillSolidRect(rect,patView->row);
								curXoffset+=SEQ_ROWWIDTH;
							}
						}
						curYoffset+=SEQ_ROWHEIGHT;
					}
				}
			}
			else
			{

			}
		/////////////////////////// Cursor

		//	CBrush brush1;
		//	CBrush *oldbrush;

			if ( seqTracksHoriz )
			{
				rect.top = SEQ_YOFFSET + (seqGen - seqGenOffs)*SEQ_ROWHEIGHT;
				rect.left = SEQ_XOFFSET + ((seqTick - seqTickOffs)/seqSteps)*SEQ_ROWWIDTH;
			}
			else
			{
				rect.top = SEQ_YOFFSET + ((seqTick - seqTickOffs)/seqSteps)*SEQ_ROWHEIGHT;
				rect.left = SEQ_XOFFSET + (seqGen - seqGenOffs)*SEQ_ROWWIDTH;
			}
			rect.bottom = rect.top+SEQ_ROWHEIGHT-1;
			rect.right = rect.left+SEQ_ROWWIDTH+1;

			if ( seqGenPos[seqGen]== MASTER_INDEX)
			{
				k=0;
				for (i=0;k<seqTick;i++)
				{
					k+=_pSong.patternLines[_pSong.playOrder[i]]/(seqSteps*_pSong.LinesPerBeat());
				}
				if ( k == seqTick ) sprintf(cbuffer,"%.2d",_pSong.playOrder[i]);
				else strcpy(cbuffer,"...");
			}
			else
			{
				strcpy(cbuffer,"...");
			}
			devc->SetBkColor(patView->cursor);
			devc->SetTextColor(patView->fontCur);
			
		//	devc->FillSolidRect(&rect,patView->separator);
			TXTFLAT(devc,cbuffer,rect.left,rect.top,SEQ_ROWWIDTH-1,SEQ_ROWHEIGHT-1);
			
			
		//	brush1.CreateSolidBrush(patView->cursor);
		//	oldbrush = (CBrush*)devc->SelectObject(&brush1);

		//	devc->FrameRect(rect,&brush1);
			
		//	devc->SelectObject(oldbrush);
		//	brush1.DeleteObject()
			
		////////////////////////// Timeline


		//
		// The way to get the playback position needs to be changed. This is for testing only.
		//
		//

			rect.top = curYoffset;
			rect.bottom = curYoffset+SEQ_ROWHEIGHT;
			rect.left=0;
			rect.right=SEQ_XOFFSET;
			devc->FillSolidRect(&rect,patView->separator);
		//	rect.top++;
		//	rect.bottom+= SEQ_ROWHEIGHT-1;
		//	devc->FillSolidRect(&rect,patView->row);
			devc->SetBkColor(patView->row4beat);	
			devc->SetTextColor(patView->font);
			TXT(devc,"Timeline",0,curYoffset,SEQ_XOFFSET-1,SEQ_ROWHEIGHT-1);

		//	rect.top++;
			rect.left=SEQ_XOFFSET;
			rect.right=CW;
			rect.bottom--;
			devc->FillSolidRect(&rect,patView->row4beat);

			curcol = seqTickOffs - (seqTickOffs%seqSteps);
			
			float curtime = (Global::player().sampleCount * Global::player().bpm) / (60.0f * Global::player().SampleRate());

			if ( (float)(curcol*seqSteps) <= curtime && Global::player()._playing )
			{
				if ( (float)(curcol+(seqNumCols*seqSteps)) <= curtime )
					rect.right=CW;
				else
					rect.right=SEQ_XOFFSET+1 + (int)(
					(curtime - (float)curcol) *  SEQ_ROWWIDTH /
					seqSteps
					);
				devc->FillSolidRect(&rect,patView->playbar);
			}



			devc->SelectObject(oldFont);
		}
}}
