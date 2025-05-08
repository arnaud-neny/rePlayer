///\file
///\brief implementation file for psycle::host::MixerFrameView.
#include <psycle/host/detail/project.private.hpp>
#include "MixerFrameView.hpp"
#include "NativeGraphics.hpp"
#include "Song.hpp"
#include "internal_machines.hpp"
#include "FrameMachine.hpp"
#include "InputHandler.hpp"
#include "ExclusiveLock.hpp"

namespace psycle { namespace host {


	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////
/*
		BEGIN_MESSAGE_MAP(MixerFrameView, CWnd)
			ON_WM_CREATE()
			ON_WM_SETFOCUS()
			ON_WM_PAINT()
			ON_WM_LBUTTONDOWN()
			ON_WM_LBUTTONDBLCLK()
			ON_WM_MOUSEMOVE()
			ON_WM_LBUTTONUP()
			ON_WM_RBUTTONUP()
		END_MESSAGE_MAP()
*/


		MixerFrameView::MixerFrameView(CFrameMachine* frame,Machine* effect)
		:CNativeView(frame,effect)
		,backgroundBmp(0)
		,numSends(0)
		,numChans(0)
		,updateBuffer(false)
		,_swapstart(-1)
		,_swapend(-1)
		,isslider(false)
		,refreshheaders(false)
		{
			colwidth=uiSetting->dialwidth+32;
			SelectMachine(effect);
		}
		MixerFrameView::~MixerFrameView()
		{
			if ( backgroundBmp ) { backgroundBmp->DeleteObject(); delete backgroundBmp; }
		}

		void MixerFrameView::OnPaint() 
		{
			int infowidth(colwidth-uiSetting->dialwidth);
			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			int vuxoffset = (colwidth + uiSetting->coords.sMixerSlider.width - uiSetting->coords.sMixerVuOff.width)/2;
			int vuyoffset = uiSetting->coords.sMixerSlider.height - uiSetting->coords.sMixerVuOff.height-uiSetting->dialheight-1;
			int xoffset(realwidth), yoffset(0);
			char value[64];

			if (!_pMachine) return;
			CPaintDC dc(this); // device context for painting

			CRect rect;
			GetClientRect(&rect);

			if(UpdateSendsandChans()) 
			{
				GetViewSize(rect);
				parentFrame->ResizeWindow(&rect);
				updateBuffer = true;
			}

			CDC bufferDC;
			CBitmap bufferBmp;
			bufferDC.CreateCompatibleDC(&dc);
			bufferBmp.CreateCompatibleBitmap(&dc,rect.right-rect.left,rect.bottom-rect.top);
			CBitmap* oldbmp = bufferDC.SelectObject(&bufferBmp);
			CFont *oldfont=bufferDC.SelectObject(&uiSetting->font);


			CDC knobDC;
			CDC mixerBmpDC;
			knobDC.CreateCompatibleDC(&bufferDC);
			mixerBmpDC.CreateCompatibleDC(&bufferDC);
			CBitmap *knobbmp=knobDC.SelectObject(&uiSetting->dial);
			CBitmap *mixbmp=mixerBmpDC.SelectObject(&uiSetting->mixerSkin);

			if (updateBuffer || refreshheaders) 
			{
				if ( backgroundBmp ) { backgroundBmp->DeleteObject(); delete backgroundBmp; }
				CDC backgroundDC;
				backgroundDC.CreateCompatibleDC(&bufferDC);
				backgroundBmp = new CBitmap();
				backgroundBmp->CreateCompatibleBitmap(&bufferDC,rect.right-rect.left,rect.bottom-rect.top);
				CBitmap* oldbgnd = backgroundDC.SelectObject(backgroundBmp);
				CFont *oldbgndfont=backgroundDC.SelectObject(&uiSetting->font);

				backgroundDC.FillSolidRect(0,0,rect.right,rect.bottom,uiSetting->bottomColor);
				GenerateBackground(backgroundDC, mixerBmpDC);

				bufferDC.BitBlt(0,0,rect.right,rect.bottom,&backgroundDC,0,0,SRCCOPY);
				backgroundDC.SelectObject(oldbgnd);
				backgroundDC.SelectObject(oldbgndfont);
				backgroundDC.DeleteDC();

				updateBuffer=false;
			}
			else {
				CDC backgroundDC;
				backgroundDC.CreateCompatibleDC(&bufferDC);
				CBitmap* oldbgnd = backgroundDC.SelectObject(backgroundBmp);
				bufferDC.BitBlt(0,0,rect.right,rect.bottom,&backgroundDC,0,0,SRCCOPY);
				backgroundDC.SelectObject(oldbgnd);
				backgroundDC.DeleteDC();
			}


			// Column 1 Master volume
			yoffset=(mixer().numsends()+1)*realheight;
			mixer().GetParamValue(13,value);
			Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(13)/256.0f);
			InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
			yoffset+=realheight;
			mixer().GetParamValue(14,value);
			Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(14)/1024.0f);
			InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
			yoffset+=realheight;
			mixer().GetParamValue(15,value);
			Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(15)/256.0f);
			InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);

			mixer().GetParamValue(0,value);
			InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,value);
			yoffset+=realheight;
			GraphSlider::DrawKnob(bufferDC,mixerBmpDC,xoffset,yoffset,mixer().GetParamValue(0)/4096.0f);
			VuMeter::Draw(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset,mixer()._volumeDisplay/97.0f);

			// Columns 2 onwards, controls
			xoffset+=realwidth;
			for (int i(0); i<mixer().numinputs(); i++)
			{
				yoffset=0;
				if ( _swapend != -1 || refreshheaders)
				{
					std::string chantxt = mixer().GetAudioInputName(int(i+Mixer::chan1));
					if (mixer().ChannelValid(i))
					{
						if ( _swapend == i+chan1)
							InfoLabel::DrawHLight(bufferDC,xoffset,yoffset,colwidth,chantxt.c_str(),mixer().inWires[i].GetSrcMachine()._editName);
						else 
							InfoLabel::DrawHeader2(bufferDC,xoffset,yoffset,colwidth,chantxt.c_str(),mixer().inWires[i].GetSrcMachine()._editName);
					}
					else
					{
						if ( _swapend == i+chan1)
							InfoLabel::DrawHLight(bufferDC,xoffset,colwidth,yoffset,chantxt.c_str(),"");
						else
							InfoLabel::DrawHeader2(bufferDC,xoffset,colwidth,yoffset,chantxt.c_str(),"");
					}
				}
				if (mixer().ChannelValid(i))
				{
					yoffset+=realheight;
					for (int j=0; j<mixer().numsends(); j++)
					{
						int param =(i+1)*0x10+(j+1);
						Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(param)/256.0f);
						mixer().GetParamValue(param,value);
						InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
						yoffset+=realheight;
					}
					int param =(i+1)*0x10;
					Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(param)/256.0f);
					mixer().GetParamValue(param,value);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
					yoffset+=realheight;
					param+=14;
					Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(param)/1024.0f);
					mixer().GetParamValue(param,value);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
					yoffset+=realheight;
					param++;
					Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(param)/256.0f);
					mixer().GetParamValue(param,value);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);
					param= i+1;
					mixer().GetParamValue(param,value);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,value);
					yoffset+=realheight;
					GraphSlider::DrawKnob(bufferDC,mixerBmpDC,xoffset,yoffset,mixer().GetParamValue(param)/4096.0f);
					VuMeter::Draw(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset,mixer().VuChan(i));
					CheckedButton::Draw(bufferDC,mixerBmpDC,mixer().GetSoloState(i),xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"S");
					yoffset+=uiSetting->coords.sMixerCheckOff.height;
					CheckedButton::Draw(bufferDC, mixerBmpDC,
						mixer().Channel(i).Mute() || (mixer().IsSoloColumn() && !mixer().GetSoloState(i))
						,xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"M");
					yoffset+=uiSetting->coords.sMixerCheckOff.height;
					CheckedButton::Draw(bufferDC,mixerBmpDC,mixer().Channel(i).DryOnly(),xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"D");
					yoffset+=uiSetting->coords.sMixerCheckOff.height;
					CheckedButton::Draw(bufferDC,mixerBmpDC,mixer().Channel(i).WetOnly(),xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"W");
				}
				else {
					std::string chantxt = mixer().GetAudioInputName(int(i+Mixer::chan1));
					InfoLabel::DrawHLight(bufferDC,xoffset,0,colwidth,chantxt.c_str(),"");
				}
				xoffset+=realwidth;
			}
			for (int i(0); i<mixer().numreturns(); i++)
			{
				yoffset=0;
				if ( _swapend != -1  || refreshheaders)
				{
					std::string sendtxt = mixer().GetAudioInputName(int(i+Mixer::return1));
					if (mixer().ReturnValid(i)) {
						if(_swapend != i+return1) {
							InfoLabel::DrawHeader2(bufferDC,xoffset,yoffset,colwidth,sendtxt.c_str(),sendNames[i].c_str());
						} else {
							InfoLabel::DrawHLight(bufferDC,xoffset,yoffset,colwidth,sendtxt.c_str(),sendNames[i].c_str());
						}
					} else {
						InfoLabel::DrawHLight(bufferDC,xoffset,yoffset,colwidth,sendtxt.c_str(),"");
					}
				}

				if (mixer().ReturnValid(i))
				{
					yoffset=(2+i)*realheight;
					for (int j=i+1; j<mixer().numsends(); j++)
					{
						SwitchButton::Draw(bufferDC,mixerBmpDC, GetRouteState(i,j),xoffset,yoffset);
						yoffset+=realheight;
					}
					SwitchButton::Draw(bufferDC,mixerBmpDC, GetRouteState(i,13),xoffset,yoffset);

					int param =0xF1+i;
					yoffset=(mixer().numsends()+3)*realheight;
					mixer().GetParamValue(param,value);
					Knob::Draw(bufferDC,knobDC,xoffset,yoffset,mixer().GetParamValue(param)/256.0f);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,value);

					param = 0xE1+i;
					mixer().GetParamValue(param,value);
					InfoLabel::DrawValue(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,value);
					yoffset+=realheight;
					GraphSlider::DrawKnob(bufferDC,mixerBmpDC,xoffset,yoffset,mixer().GetParamValue(param)/4096.0f);
					VuMeter::Draw(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset,mixer().VuSend(i));
					CheckedButton::Draw(bufferDC,mixerBmpDC,mixer().GetSoloState(i+Mixer::return1),xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"S");
					yoffset+=uiSetting->coords.sMixerCheckOff.height;
					CheckedButton::Draw(bufferDC, mixerBmpDC,
						mixer().Return(i).Mute() ||	(mixer().IsSoloReturn() && !mixer().GetSoloState(i+Mixer::return1))
						,xoffset+uiSetting->coords.sMixerSlider.width,yoffset,"M");
				}
				else {
					std::string sendtxt = mixer().GetAudioInputName(int(i+Mixer::return1));
					InfoLabel::DrawHLight(bufferDC,xoffset,0,colwidth,sendtxt.c_str(),"");
				}
				xoffset+=realwidth;
			}

			dc.BitBlt(0,0,rect.right,rect.bottom,&bufferDC,0,0,SRCCOPY);

			mixerBmpDC.SelectObject(mixbmp);
			mixerBmpDC.DeleteDC();
			knobDC.SelectObject(knobbmp);
			knobDC.DeleteDC();

			bufferDC.SelectObject(oldbmp);
			bufferDC.SelectObject(oldfont);
			bufferDC.DeleteDC();

			if(istweak  && !allowmove) {
				//Reposition the cursor, so that it doesn't move out of the window.
				//This solves the problem where the cursor cannot move more because it reaches the top or bottom of the screen.
				CPoint point;
				point.x = sourcex;
				point.y = sourcepoint;
				tweakbase = helpers::math::round<int,float>(visualtweakvalue);
				ClientToScreen(&point);
				positioning=true;
				SetCursorPos(point.x,point.y);
			}
		}

		void MixerFrameView::OnLButtonDown(UINT nFlags, CPoint point) 
		{

			int xoffset(0),yoffset(0);
			const int col=GetColumn(point.x,xoffset);
			const int row=GetRow(xoffset,point.y,yoffset);
			
			istweak = false;
			isslider = false;

			if (col == collabels || (row == rowlabels && col == colmaster)) return;
			if (row == rowlabels)
			{
				// move/swap channels.
				_swapstart = _swapend = col;
			}
			else if (col == colmaster)
			{
				if ( row == mix)		{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == gain)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == pan)		{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == slider) { istweak=GraphSlider::LButtonDown(nFlags,xoffset,yoffset);isslider=istweak; }
			}
			else if ( col < chanmax )
			{
				int chan = col - chan1;
				if ( row < sendmax)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == mix)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == gain)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == pan)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == slider) { istweak=GraphSlider::LButtonDown(nFlags,xoffset,yoffset); isslider=istweak;}
				else if ( row == solo)
				{
					chan++;
					tweakpar=GetParamFromPos(col,row);
					int solo = _pMachine->GetParamValue(tweakpar);
					_pMachine->SetParameter(tweakpar,(solo==chan)?0:chan);
				}
				else if ( row == mute)
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					_pMachine->SetParameter(tweakpar,statebits==3?0:3);
				}
				else if ( row == dryonly)
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					_pMachine->SetParameter(tweakpar,statebits==1?0:1);
				}
				else if ( row == wetonly)
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					_pMachine->SetParameter(tweakpar,statebits==2?0:2);
				}
			}
			else
			{
				int ret = col - return1;
				if ( row < sendmax)
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					// XOR to the "row+1"th bit.
					_pMachine->SetParameter(tweakpar,(statebits & ~(1<<row)) | ((statebits&(1<<row)) ^ (1<<row)));
				}
				else if ( row == mix )
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					// XOR to the 14th bit.
					_pMachine->SetParameter(tweakpar,(statebits & ~(1<<13)) | ((statebits&(1<<13)) ^ (1<<13)));
				}

				else if ( row == pan)	{ istweak=Knob::LButtonDown(nFlags,xoffset,yoffset); }
				else if ( row == slider) { istweak=GraphSlider::LButtonDown(nFlags,xoffset,yoffset); isslider=istweak; }
				else if ( row == solo)
				{
					ret+=MAX_CONNECTIONS+1;
					tweakpar=GetParamFromPos(col,row);
					int solo = _pMachine->GetParamValue(tweakpar);
					_pMachine->SetParameter(tweakpar,(solo==ret)?0:ret);
				}
				else if ( row == mute)
				{
					tweakpar=GetParamFromPos(col,row);
					int statebits = _pMachine->GetParamValue(tweakpar);
					// XOR to the 1st bit.
					_pMachine->SetParameter(tweakpar,(statebits & ~0x1) | ((statebits&0x1) ^0x1));
				}
			}

			if (istweak)
			{
				SetCapture();
				sourcex=point.x;
				sourcepoint=point.y;
				tweakpar=GetParamFromPos(col,row); 
				_pMachine->GetParamRange(tweakpar,minval,maxval);
				tweakbase = _pMachine->GetParamValue(tweakpar);
				if ( row == slider)
				{
					float foffset = yoffset/ float(uiSetting->coords.sMixerSlider.height-uiSetting->coords.sMixerKnob.height);
					float fbase = tweakbase/ float(maxval-minval);
					float knobheight = (uiSetting->coords.sMixerKnob.height/float(uiSetting->coords.sMixerSlider.height-uiSetting->coords.sMixerKnob.height));
					float ypos(0);
					if ( fbase < 0.375) ypos = 1.0f;
					else if ( fbase < 0.999) ypos = (((tweakbase/float(maxval-minval)-0.375f)*1.6f)-1.0f)*-1.0f;

					if ( foffset <= ypos || foffset > ypos+knobheight) // if mouse not over the knob, move the knob first
					{
						foffset = foffset - (knobheight/2.0);
						double freak = (maxval-minval)*0.625; // *0.625 adjust the full range to the visual range
						double nv = (1.0f-foffset)*freak + 0.375*(maxval-minval); // 0.375 to compensate for the visual range.

						tweakbase = nv+0.5f;
						_pMachine->SetParameter(tweakpar,tweakbase);
						parentFrame->Automate(tweakpar,tweakbase,true);
					}
				}
				else if (_swapstart == -1) {
					while (ShowCursor(FALSE) >= 0);
				}
				allowmove =  isslider || _swapstart > -1;
				prevval = tweakbase;
				visualtweakvalue= tweakbase;
				PsycleGlobal::inputHandler().AddMacViewUndo();
			}
			CWnd::OnLButtonDown(nFlags, point);
		}

		void MixerFrameView::OnMouseMove(UINT nFlags, CPoint point) 
		{
			if (isslider)
			{
				if (( ultrafinetweak && !(nFlags & MK_SHIFT )) || //shift-key has been left.
					( !ultrafinetweak && (nFlags & MK_SHIFT))) //shift-key has just been pressed
				{
					tweakbase = _pMachine->GetParamValue(tweakpar);
					if (isslider) sourcepoint=point.y;
					ultrafinetweak=!ultrafinetweak;
				}
				else if (( finetweak && !(nFlags & MK_CONTROL )) || //control-key has been left.
					( !finetweak && (nFlags & MK_CONTROL))) //control-key has just been pressed
				{
					tweakbase = _pMachine->GetParamValue(tweakpar);
					if (isslider) sourcepoint=point.y;
					finetweak=!finetweak;
				}

				double freak = (maxval-minval)*0.625/(uiSetting->coords.sMixerSlider.height-uiSetting->coords.sMixerKnob.height); // *0.625 adjust the full range to the visual range
				if ( ultrafinetweak ) freak /= 10;
				if (finetweak) freak/=4;

				double nv = (double)(sourcepoint - point.y)*freak + (double)tweakbase; // +0.375 to compensate for the visual range.
				
				if (nv < minval) {
					nv = minval;
				}
				if (nv > maxval) {
					nv = maxval;
				}
				prevval=helpers::math::round<int,float>(nv);
				_pMachine->SetParameter(tweakpar,prevval);
				parentFrame->Automate(tweakpar,prevval,false);

				Invalidate(false);
				CWnd::OnMouseMove(nFlags,point);
			}
			else if (_swapstart > -1)
			{
				int xoffset(0);
				int col = GetColumn(point.x,xoffset);
				if ( _swapstart < chanmax && col >= return1) _swapend = -1;
				else if ( _swapstart >= return1 && col < chanmax) _swapend = -1;
				else _swapend = col;
				CWnd::OnMouseMove(nFlags,point);
			}
			else CNativeView::OnMouseMove(nFlags,point);
		}

		void MixerFrameView::OnLButtonUp(UINT nFlags, CPoint point) 
		{
			if ( _swapstart >= chan1 && _swapend >= chan1 && _swapstart != _swapend)
			{
				CExclusiveLock lock(&Global::song().semaphore, 2, true);
				if ( _swapstart < chanmax)
				{
					_swapstart -=chan1; _swapend -= chan1; 
					mixer().ExchangeChans(_swapstart,_swapend);
				}
				else 
				{
					_swapstart-=return1; _swapend -= return1;
					mixer().ExchangeReturns(_swapstart,_swapend);
				}
			}
			ReleaseCapture();
			if (istweak && !isslider && _swapstart == -1) while (ShowCursor(TRUE) < 0);
			refreshheaders=true;
			_swapstart = -1;
			_swapend = -1;
			isslider = false;
			istweak=false;
			Invalidate();
			CWnd::OnLButtonUp(nFlags, point);
		}

		int MixerFrameView::ConvertXYtoParam(int x, int y)
		{
			int realwidth = colwidth+1;
			int xoffset(0),yoffset(0);
			return GetParamFromPos(GetColumn(x,xoffset),GetRow(x%realwidth,y,yoffset));
		}

		bool MixerFrameView::GetViewSize(CRect& rect)
		{
			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			rect.left= rect.top = 0;
			rect.right = ncol * realwidth;
			rect.bottom = parspercol * realheight + (uiSetting->coords.sMixerSlider.height+1);
			return true;
		}

		void MixerFrameView::SelectMachine(Machine* pMachine)
		{
			_pMachine = pMachine;
			//This forces a reload of the values.
			numSends=-1;
			UpdateSendsandChans();
			updateBuffer=true;
		}

			
		bool MixerFrameView::UpdateSendsandChans()
		{
			//int sends(0),cols(0);
			for (int i=0; i<mixer().numreturns(); i++)
			{
				if (mixer().Return(i).IsValid()) {
					sendNames[i]=mixer().Return(i).GetWire().GetSrcMachine().GetEditName();
					//sends++;
				}
				else sendNames[i]="";
			}
			/*for (int i=0; i<mixer().numinputs(); i++)
			{
				if (mixer().ChannelValid(i)) cols++;
			}*/

			if ( numSends != mixer().numreturns()/*sends*/ || numChans != mixer().numinputs()/*cols*/)
			{
				//numSends= sends; numChans = cols;
				numSends = mixer().numreturns();
				numChans = mixer().numinputs();

				// +2 -> labels column, plus master column.
				ncol = mixer().numinputs()+mixer().numreturns()+2;
				 // + 5 -> labels row, pan, gain, and mix. Slider is aside
				parspercol = mixer().numsends()+4;
				return true;
			}
			return false;
		}

		void MixerFrameView::GenerateBackground(CDC &bufferDC, CDC& mixerBmpDC)
		{
			// Draw to buffer.
			// Column 0, Labels.
			int xoffset(0), yoffset(0);
			const COLORREF titleColor = uiSetting->titleColor;
			int infowidth(colwidth-uiSetting->dialwidth);
			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			int vuxoffset = (colwidth + uiSetting->coords.sMixerSlider.width - uiSetting->coords.sMixerVuOff.width)/2;
			int vuyoffset = uiSetting->coords.sMixerSlider.height - uiSetting->coords.sMixerVuOff.height-uiSetting->dialheight-1;
			for (int i=0; i <mixer().numsends();i++)
			{
				yoffset+=realheight;
				std::ostringstream sendtxt;
				sendtxt << "Send " << i+1;
				if ( mixer().SendValid(i))
				{
					InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,sendtxt.str().c_str(),sendNames[i].c_str());
				}
				else InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,sendtxt.str().c_str(),"");
				bufferDC.Draw3dRect(-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
			}
			yoffset+=realheight;
			InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,"Mix","");
			bufferDC.Draw3dRect(-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
			yoffset+=realheight;
			InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,"Gain","");
			bufferDC.Draw3dRect(-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
			yoffset+=realheight;
			InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,"Pan","");
			bufferDC.Draw3dRect(-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

			yoffset+=uiSetting->coords.sMixerSlider.height+1;
			InfoLabel::DrawHeader2(bufferDC,0,yoffset,colwidth,"Ch. Input","");
			yoffset+=realheight;
			bufferDC.Draw3dRect(-1,-1,realwidth+1,yoffset+1,titleColor,titleColor);

			// Column 1 master Volume.
			xoffset+=realwidth;
			yoffset=0;
			InfoLabel::DrawHeader2(bufferDC,xoffset,yoffset,colwidth,"Master Out","");
			bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

			yoffset+=(mixer().numsends()+1)*realheight;
			InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"D/W","");
			bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
			yoffset+=realheight;
			InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Gain","");
			bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
			yoffset+=realheight;
			InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Pan","");
			bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

			InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,"Level","");
			yoffset+=realheight;
			GraphSlider::Draw(bufferDC,mixerBmpDC,xoffset,yoffset);
			VuMeter::DrawBack(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset);
			yoffset+=uiSetting->coords.sMixerSlider.height+1;
			bufferDC.Draw3dRect(xoffset-1,-1,realwidth+1,yoffset+1,titleColor,titleColor);


			// Columns 2 onwards, controls
			xoffset+=realwidth;
			for (int i=0; i<mixer().numinputs(); i++)
			{
				yoffset=0;
				std::string chantxt = mixer().GetAudioInputName(int(i+Mixer::chan1));
				if (mixer().ChannelValid(i))
				{
					InfoLabel::DrawHeader2(bufferDC,xoffset,yoffset,colwidth,chantxt.c_str(),mixer().inWires[i].GetSrcMachine()._editName);
				}
				else InfoLabel::DrawHLight(bufferDC,xoffset,yoffset,colwidth,chantxt.c_str(),"");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
				yoffset+=realheight;
				for (int j=0; j<mixer().numsends(); j++)
				{
					InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Send","");
					bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
					yoffset+=realheight;
				}
				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Mix","");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
				yoffset+=realheight;
				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Gain","");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
				yoffset+=realheight;
				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Pan","");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,"Level","");
				yoffset+=realheight;
				GraphSlider::Draw(bufferDC,mixerBmpDC,xoffset,yoffset);
				VuMeter::DrawBack(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset);
				yoffset+=uiSetting->coords.sMixerSlider.height+1;
				bufferDC.Draw3dRect(xoffset-1,-1,realwidth+1,yoffset+1,titleColor,titleColor);
				xoffset+=realwidth;
			}
			for (int i=0; i<mixer().numreturns(); i++)
			{
				yoffset=0;
				std::string sendtxt = mixer().GetAudioInputName(int(i+Mixer::return1));
				if (mixer().ReturnValid(i))
				{
					InfoLabel::DrawHeader2(bufferDC,xoffset,yoffset,colwidth,sendtxt.c_str(),sendNames[i].c_str());
				}
				else InfoLabel::DrawHLight(bufferDC,xoffset,yoffset,colwidth,sendtxt.c_str(),"");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
				yoffset+=(2+i)*realheight;
				for (int j=i+1; j<mixer().numsends(); j++)
				{
					InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Route","");
					bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);
					yoffset+=realheight;
				}
				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Master","");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

				yoffset=(mixer().numsends()+3)*realheight;
				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset,infowidth,"Pan","");
				bufferDC.Draw3dRect(xoffset-1,yoffset-1,realwidth+1,realheight+1,titleColor,titleColor);

				InfoLabel::Draw(bufferDC,xoffset+uiSetting->dialwidth,yoffset+uiSetting->coords.sMixerSlider.height,infowidth,"Level","");
				yoffset+=realheight;
				GraphSlider::Draw(bufferDC,mixerBmpDC,xoffset,yoffset);
				VuMeter::DrawBack(bufferDC,mixerBmpDC,xoffset+vuxoffset,yoffset+vuyoffset);
				yoffset+=uiSetting->coords.sMixerSlider.height+1;
				bufferDC.Draw3dRect(xoffset-1,-1,realwidth+1,yoffset+1,titleColor,titleColor);
				xoffset+=realwidth;
			}
		}
		int MixerFrameView::GetColumn(int x, int &xoffset)
		{
			int realwidth = colwidth+1;

			int col=x/(realwidth);
			xoffset=x%(realwidth);
			if ( col < chan1+mixer().numinputs()) return col;
			else
			{
				col-=chan1+mixer().numinputs();
				return return1+col;
			}
		}
		int MixerFrameView::GetRow(int x,int y,int &yoffset)
		{
			int realheight = uiSetting->dialheight+1;

			int row = y/realheight;
			yoffset=y%realheight;
			if (row < send1+mixer().numsends()) return row;
			else
			{
				row-=send1+mixer().numsends();
				if (row == 0 ) return mix;
				else if (row == 1) return gain;
				else if (row == 2) return pan;
				else if (x < uiSetting->coords.sMixerSlider.width)
				{
					yoffset = y - (realheight*(mixer().numsends()+4 ));
					return slider;
				}
				else
				{
					int zeroy = (3+send1+mixer().numsends())*realheight;
					row = (y-zeroy)/uiSetting->coords.sMixerCheckOff.height;
					if ( row == 0 ) return solo;
					else if(row == 1) return mute;
					else if(row == 2) return dryonly;
					else if(row == 3) return wetonly;
					else {
						yoffset = y - (realheight*(mixer().numsends()+4 ));
						return slider;
					}
				}
			}
			//return -1;
		}
		int MixerFrameView::GetParamFromPos(int col,int row)
		{
			if ( col == collabels || row == rowlabels) return -1;
			else if ( col == colmaster)
			{
				if ( row == slider) return 0;
				else if (row == mix) return 13;
				else if (row == gain) return 14;
				else if (row == pan) return 15;
				return -1;
			}
			else if ( col < chanmax)
			{
				int chan = col - chan1;
				if (row < sendmax) return (chan+1)*0x10+(row-send1+1);
				else if ( row==mix) return (chan+1)*0x10;
				else if ( row==gain) return (chan+1)*0x10 +14;
				else if ( row==pan) return (chan+1)*0x10 +15;
				else if ( row==slider) return (chan+1);
				else if ( row==solo) return 13*0x10;
				else if ( row==mute) return (chan+1)*0x10 +13;
				else if ( row==dryonly) return (chan+1)*0x10 +13;
				else if ( row==wetonly) return (chan+1)*0x10 +13;
			}
			else 
			{
				int chan = col - return1;
				if (row < sendmax) return 13*0x10 +(chan+1);
				else if ( row==mix) return 13*0x10 +(chan+1); // mix is route to master.
				else if ( row==slider) return 14*0x10 +(chan+1);
				else if ( row==pan) return 15*0x10 +(chan+1);
				else if ( row==solo) return 13*0x10;
				else if ( row==mute) return 13*0x10 +(chan+1);
			}
			return -1;
		}

		bool MixerFrameView::GetRouteState(int ret,int send)
		{
			if (send < sendmax)
				return mixer().Return(ret).SendsTo(send);
			else if ( send == 13)
				return mixer().Return(ret).MasterSend();
			return false;
		}
	}   // namespace
}   // namespace
