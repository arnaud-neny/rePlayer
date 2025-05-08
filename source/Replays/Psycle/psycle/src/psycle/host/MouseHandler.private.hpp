///\file
///\brief pointer handler for psycle::host::CChildView, private header
#pragma once
#include <psycle/host/detail/project.hpp>
namespace psycle { namespace host {

    using namespace ui;

		void CChildView::OnRButtonDown( UINT nFlags, CPoint point )
		{	
			//Right mouse button behaviour (OnRButtonDown() and OnRButtonUp()) extended by sampler.
			SetCapture();
			
			if(viewMode == view_modes::machine) // User is in machine view mode
			{
				smac = -1; 		smacmode = smac_modes::move;
				wiresource = -1; wiredest = -1;
				wiremove = -1;
				if (nFlags & MK_CONTROL) // Control+Rightclick. Action: Move the wire origin.
				{
					int tmp;
					Wire* w = GetWire(point, tmp, wiremove);  // wiremove = wire connection index in destination machine
					if ( w != NULL ) // we are in a wire, let's enable origin-wire move.
					{
						Machine & macDest = w->GetDstMachine();
						wiredest = macDest._macIndex; // wiredest = destination machine *index*

						switch (macDest._mode) // Assing wireDX and wireDY for the next draw.
						{
						//case MACHMODE_GENERATOR: //A wire can't end in a generator								
						case MACHMODE_FX:
							wireDX = macDest._x+(MachineCoords->sEffect.width/2);
							wireDY = macDest._y+(MachineCoords->sEffect.height/2);
							wireSX = point.x;
							wireSY = point.y;
							break;

						case MACHMODE_MASTER:
							wireDX = macDest._x+(MachineCoords->sMaster.width/2);
							wireDY = macDest._y+(MachineCoords->sMaster.height/2);
							wireSX = point.x;
							wireSY = point.y;
							break;
						}
					}
				}
				else if (nFlags == MK_RBUTTON) // Right click alone. Action: Create a new wire or move wire destination.
				{
					wiresource = GetMachine(point); //See if we have clicked over a machine.
					if ( wiresource == -1 ) // not a machine. Let's see if it is a wire
					{
						int tmp;
						Wire* w = GetWire(point, wiremove, tmp);
						if (w != NULL) {
							wiresource = w->GetSrcMachine()._macIndex;
						}
					}
					if (wiresource != -1) // found a machine (either clicked, or via a wire), enable wire creation/move
					{
						Machine& mac = *_pSong._pMachine[wiresource];
						switch (mac._mode)
						{
						case MACHMODE_GENERATOR:
							wireSX = mac._x+(MachineCoords->sGenerator.width/2);
							wireSY = mac._y+(MachineCoords->sGenerator.height/2);
							wireDX = point.x;
							wireDY = point.y;
							break;
						case MACHMODE_FX:
							wireSX = mac._x+(MachineCoords->sEffect.width/2);
							wireSY = mac._y+(MachineCoords->sEffect.height/2);
							wireDX = point.x;
							wireDY = point.y;
							break;
						//case MACHMODE_MASTER: // A wire can't be sourced in master
						default:
							wiresource=-1;			//don't pretend we're drawing a wire if we're not..
							break;
						}
					}
				} // End nFlags & MK_RBUTTON
			}
			CWnd::OnRButtonDown(nFlags,point);
		}
		
		void CChildView::OnRButtonUp( UINT nFlags, CPoint point )
		{
			ReleaseCapture();
			allowcontextmenu=true;

			if (viewMode == view_modes::machine)
			{
				int propMac = GetMachine(point);

				if (propMac != -1) // Is the mouse pointer over a machine?
				{
					if (wiresource == propMac) // Is the mouse over the same machine than when we did OnRButtonDown?
					{
						//DoMacPropDialog(propMac);
					}
					else if (wiresource != -1) // Did we RButtonDown over a wire (moving dest) or a machine?
					{
						Machine *tmac = _pSong._pMachine[wiresource];
						Machine *dmac = _pSong._pMachine[propMac];
						PsycleGlobal::inputHandler().AddMacViewUndo();
						if (wiremove != -1) //were we moving a wire?
						{
							int w(-1);
							///\todo: hardcoded for the Mixer machine. This needs to be extended with multi-io.
							if ( tmac->_mode== MACHMODE_FX && dmac->GetInputSlotTypes() > 1 )
							{
								if (MessageBox("Should I connect this to a send/return input?","Mixer Connection",MB_YESNO) == IDYES )
								{
									w = dmac->GetFreeInputWire(1);
								}
								else { w = dmac->GetFreeInputWire(0); }
							}
							else { w = dmac->GetFreeInputWire(0); }
							if (!_pSong.ChangeWireDestMacBlocking(tmac,dmac,wiremove,w))
							{
								MessageBox("Wire move could not be completed!","Error!", MB_ICONERROR);
							}
							allowcontextmenu=false;
						}
						else //Create a new connection
						{
							int dsttype=0;
							///\todo: for multi-io.
							//if ( tmac->GetOutputSlotTypes() > 1 ) ask user and get index
							///\todo: hardcoded for the Mixer machine. This needs to be extended with multi-io.
							if ( tmac->_mode== MACHMODE_FX && dmac->GetInputSlotTypes() > 1 )
							{
								if (MessageBox("Should I connect this to a send/return input?","Mixer Connection",MB_YESNO) == IDYES )
								{
									dsttype=1;
								}
							}
							if (_pSong.InsertConnectionBlocking(tmac, dmac,0,dsttype)== -1)
							{
								if (tmac->connectedOutputs() >= 12 || dmac->connectedInputs() >= 12) {
									MessageBox("Couldn't connect the selected machines!\nIf needing more than 12 connections use multiple dummy or mixer machines","Error!", MB_ICONERROR);
								}
								else {
									MessageBox("Couldn't connect the selected machines!","Error!", MB_ICONERROR);
								}
							}
							allowcontextmenu=false;
						}
					}
					else if ( wiremove != -1) //were we moving a wire (source)?
					{
						Machine *tmac = _pSong._pMachine[propMac];
						Machine *dmac = _pSong._pMachine[wiredest];
						int srctype=0;
						///\todo: for multi-io.
						//if ( tmac->GetOutputSlotTypes() > 1 ) ask user and get index
						if (!_pSong.ChangeWireSourceMacBlocking(tmac,dmac,tmac->GetFreeOutputWire(srctype),wiremove))
						{
							MessageBox("Wire move could not be completed!","Error!", MB_ICONERROR);
						}
						allowcontextmenu=false;
					}
				}
				else
				{
					int tmp1,tmp2;
					Wire* w = GetWire(point,tmp1,tmp2);
					if ( w != NULL )	// Are we over a wire?
					{
						allowcontextmenu=false;
						Machine& tmac = w->GetSrcMachine();
						Machine& dmac = w->GetDstMachine();
						int free=-1;
						for (int i = 0; i < MAX_WIRE_DIALOGS; i++)
						{
							if (WireDialog[i])
							{
								if ((WireDialog[i]->srcMachine._macIndex == tmac._macIndex) &&
									(WireDialog[i]->dstMachine._macIndex == dmac._macIndex))  // If this is true, the dialog is already open
								{
									wiresource = -1;
									wiredest = -1;
									wiremove = -1;
									CWnd::OnRButtonUp(nFlags,point);
									return;
								}
							}
							else free = i;
							
						}
						if (free != -1) //If there is any dialog slot free
						{
					   		CWireDlg* wdlg;
							wdlg = WireDialog[free] = new CWireDlg(this, &WireDialog[free], free,
								*w);
							pParentMain->CenterWindowOnPoint(wdlg, point);
							wdlg->ShowWindow(SW_SHOW);
						}
						else
						{
							MessageBox("Cannot show the wire dialog. Too many of them opened!","Error!", MB_ICONERROR);
						}
					}			
				}
			}
			wiresource = -1;
			wiredest = -1;
			wiremove = -1;
			Repaint();
			CWnd::OnRButtonUp(nFlags,point);
		}
		void CChildView::OnContextMenu(CWnd* pWnd, CPoint point) 
		{
			if (viewMode == view_modes::machine && allowcontextmenu)
			{
				popupmacidx=-1;
				CPoint mypoint;
				if(point.x < 0 || point.y < 0){ // Context menu button pressed
					int inst=-1;
					Machine * pmac = _pSong.GetMachineOfBus(_pSong.seqBus, inst);
					if(pmac) {
						mypoint.x = pmac->_x;
						mypoint.y = pmac->_y;
						ClientToScreen(&mypoint);
						popupmacidx = pmac->_macIndex;
					}
				}
				else {  // Right click mouse button
					mypoint = point;
					ScreenToClient(&mypoint);
					popupmacidx = GetMachine(mypoint);
					mypoint = point;
				}
				if (popupmacidx != -1 ) {
					trackingpoint = point;
					CMenu menu;
					VERIFY(menu.LoadMenu(IDR_POPUP_MACHINE));
					CMenu* pPopup = menu.GetSubMenu(0);
					ASSERT(pPopup != NULL);
					FillConnectToPopup(pPopup->GetSubMenu(4));
					FillConnectionsPopup(pPopup->GetSubMenu(5));
					pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, mypoint.x, mypoint.y, GetOwner());
					menu.DestroyMenu();
				}
			}
			else if (viewMode == view_modes::pattern)
			{
				CPoint mypoint = point;
				if(mypoint.x < 0 || mypoint.y < 0){ // Context menu button pressed
					mypoint.x = XOFFSET+editcur.track*ROWWIDTH;
					mypoint.y = YOFFSET+editcur.line*ROWHEIGHT;
					ClientToScreen(&mypoint);
				}
				else {  // Right click mouse button
				}
				trackingpoint= point;
				CMenu menu;
				VERIFY(menu.LoadMenu(IDR_POPUPMENU));
				CMenu* pPopup = menu.GetSubMenu(0);
				ASSERT(pPopup != NULL);
				pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, mypoint.x, mypoint.y, GetOwner());
				menu.DestroyMenu();
			}
			CWnd::OnContextMenu(pWnd,point);
		}


		void CChildView::OnLButtonDown( UINT nFlags, CPoint point )
		{
			SetCapture();

			if(viewMode == view_modes::machine)
			{
				smac = -1;		smacmode = smac_modes::move;
				wiresource = -1;wiredest = -1;
				wiremove = -1;

				if ( nFlags & MK_CONTROL)
				{
					smac=GetMachine(point);
					if ( smac != -1 ) // Clicked on a machine. Let's select it.
					{
						switch (_pSong._pMachine[smac]->_mode)
						{
						case MACHMODE_GENERATOR:
						case MACHMODE_FX:
							mcd_x = point.x - _pSong._pMachine[smac]->_x;
							mcd_y = point.y - _pSong._pMachine[smac]->_y;
							_pSong.seqBus = smac;
							pParentMain->UpdateComboGen();
							Repaint();	
							break;
						}
					} 
					else // not a machine
					{	
						int tmp;
						Wire* w = GetWire(point,tmp, wiremove);
						if ( w != NULL ) // we are in a wire, let's enable origin-wire move.
						{
							Machine &mac = w->GetDstMachine();
							wiredest = mac._macIndex; // wiredest = destination machine *index*

							switch (mac._mode) // Assing wireDX and wireDY for the next draw.
							{
/*							case MACHMODE_GENERATOR: //A wire can't end in a generator
								wireDX = mac._x+(MachineCoords->sGenerator.width/2);
								wireDY = mac._y+(MachineCoords->sGenerator.height/2);
								break;
*/								
							case MACHMODE_FX:
								wireDX = mac._x+(MachineCoords->sEffect.width/2);
								wireDY = mac._y+(MachineCoords->sEffect.height/2);
								wireSX = point.x;
								wireSY = point.y;
								break;

							case MACHMODE_MASTER:
								wireDX = mac._x+(MachineCoords->sMaster.width/2);
								wireDY = mac._y+(MachineCoords->sMaster.height/2);
								wireSX = point.x;
								wireSY = point.y;
								break;
							}		
//							OnMouseMove(nFlags,point);
						}
						wiresource=-1;
					}
				}
				else if (nFlags & MK_SHIFT)
				{
					wiresource = GetMachine(point);
					if (wiresource == -1)
					{
						int tmp;
						Wire* w = GetWire(point, wiremove, tmp);
						if (w != NULL) {
							wiresource = w->GetSrcMachine()._macIndex;
						}
					}
					if (wiresource != -1) // found a machine (either clicked, or via a wire), enable origin-wire creation/move
					{
						Machine& mac = *_pSong._pMachine[wiresource];
						switch (mac._mode)
						{
						case MACHMODE_GENERATOR:
							wireSX = mac._x+(MachineCoords->sGenerator.width/2);
							wireSY = mac._y+(MachineCoords->sGenerator.height/2);
							wireDX = point.x;
							wireDY = point.y;
							break;
						case MACHMODE_FX:
							wireSX = mac._x+(MachineCoords->sEffect.width/2);
							wireSY = mac._y+(MachineCoords->sEffect.height/2);
							wireDX = point.x;
							wireDY = point.y;
							break;
/*						case MACHMODE_MASTER: // A wire can't be sourced in Master.
							wireSX = mac._x+(MachineCoords->sMaster.width/2);
							wireSY = mac._y+(MachineCoords->sMaster.height/2);
							break;
*/							
						}
//						OnMouseMove(nFlags,point);
					}
				}// Shift
				else if (nFlags & MK_LBUTTON)
				{
					smac=GetMachine(point);
					if ( smac != -1 ) // found a machine, let's do something on it.
					{
						int panning;
						Machine* tmac=Global::song()._pMachine[smac];
						mcd_x = point.x - tmac->_x;
						mcd_y = point.y - tmac->_y;

						SSkinDest tmpsrc;
						switch (tmac->_mode)
						{
						case MACHMODE_GENERATOR:
							
							// Since this is a generator, select it.
							_pSong.seqBus = smac;
							pParentMain->UpdateComboGen();
							
							panning = tmac->GetPan()*MachineCoords->dGeneratorPan.width;
							panning /= 128;
							tmpsrc.x=MachineCoords->dGeneratorPan.x; tmpsrc.y=MachineCoords->dGeneratorPan.y;
							if (InRect(mcd_x,mcd_y,tmpsrc,MachineCoords->sGeneratorPan,panning)) //changing panning
							{
								smacmode = smac_modes::panning;
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dGeneratorMute,MachineCoords->sGeneratorMute)) //Mute 
							{
								tmac->_mute = !tmac->_mute;
								if (tmac->_mute)
								{
									tmac->_volumeCounter=0.0f;
									tmac->_volumeDisplay=0;
									if (_pSong.machineSoloed == smac )
									{
										_pSong.machineSoloed = -1;
									}
									
								}
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dGeneratorSolo,MachineCoords->sGeneratorSolo)) //Solo 
							{
								if (_pSong.machineSoloed == smac )
								{
									_pSong.machineSoloed = -1;
									for ( int i=0;i<MAX_MACHINES;i++ )
									{	if ( _pSong._pMachine[i] )
										{	if (( _pSong._pMachine[i]->_mode == MACHMODE_GENERATOR ))
											{
												_pSong._pMachine[i]->_mute = false;
											}
										}
									}
								}
								else 
								{
									for ( int i=0;i<MAX_MACHINES;i++ )
									{
										if ( _pSong._pMachine[i] )
										{
											if (( _pSong._pMachine[i]->_mode == MACHMODE_GENERATOR ) && (i != smac))
											{
												_pSong._pMachine[i]->_mute = true;
												_pSong._pMachine[i]->_volumeCounter=0.0f;
												_pSong._pMachine[i]->_volumeDisplay=0;
											}
										}
									}
									tmac->_mute = false;
									_pSong.machineSoloed = smac;
								}
							}
							Repaint();
							break;

						case MACHMODE_FX:
							panning = tmac->GetPan()*MachineCoords->dEffectPan.width;
							panning /= 128;
							tmpsrc.x=MachineCoords->dEffectPan.x; tmpsrc.y=MachineCoords->dEffectPan.y;
							if (InRect(mcd_x,mcd_y,tmpsrc,MachineCoords->sEffectPan,panning)) //changing panning
							{
								smacmode = smac_modes::panning;
								OnMouseMove(nFlags,point);
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dEffectMute,MachineCoords->sEffectMute)) //Mute 
							{
								tmac->_mute = !tmac->_mute;
								if (tmac->_mute)
								{
									tmac->_volumeCounter=0.0f;	tmac->_volumeDisplay=0;
								}
								updatePar = smac;
								Repaint(draw_modes::machine);
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dEffectBypass,MachineCoords->sEffectMute)) //Solo 
							{
								tmac->Bypass(!tmac->Bypass());
								if (tmac->Bypass())
								{
									tmac->_volumeCounter=0.0f;	tmac->_volumeDisplay=0;
								}
								updatePar = smac;
								Repaint(draw_modes::machine);
							}
							break;

						case MACHMODE_MASTER:
							break;
						}
					}
				}// No Shift
			}	

			else if ( viewMode==view_modes::pattern)
			{			
				int ttm = tOff + (point.x-XOFFSET)/ROWWIDTH;
				if ( ttm >= _pSong.SONGTRACKS ) ttm = _pSong.SONGTRACKS-1;
				else if ( ttm < 0 ) ttm = 0;
				
				if (point.y >= 0 && point.y < YOFFSET ) // Mouse is in Track Header.
				{	
					int pointpos= ((point.x-XOFFSET)%ROWWIDTH) - HEADER_INDENT;
					if (point.x < XOFFSET) // Mouse is in the "Line" column
					{
						ShowTrackNames(!PsycleGlobal::conf().patView().showTrackNames_);
					}
					if (InRect(pointpos,point.y,PatHeaderCoords->dRecordOnTrack,PatHeaderCoords->sRecordOnTracking))
					{
						_pSong.ToggleTrackArmed(ttm);
					}
					else if (InRect(pointpos,point.y,PatHeaderCoords->dMuteOnTrack,PatHeaderCoords->sMuteOnTracking))
					{
						_pSong.ToggleTrackMuted(ttm);
					}
					else if (InRect(pointpos,point.y,PatHeaderCoords->dSoloOnTrack,PatHeaderCoords->sSoloOnTracking))
					{
						_pSong.ToggleTrackSoloed(ttm);
					}
					oldm.track = -1;
					Repaint(draw_modes::track_header);
				}
				else if ( point.y >= YOFFSET )
				{
					oldm.track=ttm;

					int plines = _pSong.patternLines[_ps()];
					oldm.line = lOff + (point.y-YOFFSET)/ROWHEIGHT;
					if ( oldm.line >= plines ) { oldm.line = plines - 1; }
					else if ( oldm.line < 0 ) oldm.line = 0;

					oldm.col=_xtoCol((point.x-XOFFSET)%ROWWIDTH);

					if (blockSelected
						&& oldm.track >=blockSel.start.track && oldm.track <= blockSel.end.track
						&& oldm.line >=blockSel.start.line && oldm.line <= blockSel.end.line && PsycleGlobal::conf().inputHandler()._windowsBlocks)
					{
						blockswitch=true;
						blockLastOrigin = blockSel;
						editcur = oldm;
					}
					else blockStart = true;
					if (nFlags & MK_SHIFT)
					{
						editcur = oldm;
						Repaint(draw_modes::cursor);
					}
				}
			}//<-- End LBUTTONPRESING/VIEWMODE if statement
			CWnd::OnLButtonDown(nFlags,point);
		}

		void CChildView::OnLButtonUp( UINT nFlags, CPoint point )
		{
			ReleaseCapture();
			
			if (viewMode == view_modes::machine )
			{
				int propMac = GetMachine(point);
				if ( propMac != -1)
				{
					if (wiremove >= 0) // are we moving a wire?
					{
						PsycleGlobal::inputHandler().AddMacViewUndo();
						if (wiredest == -1)
						{
							Machine *tmac = _pSong._pMachine[wiresource];
							Machine *dmac = _pSong._pMachine[propMac];
							int w(-1);
							///\todo: hardcoded for the Mixer machine. This needs to be extended with multi-io.
							if ( tmac->_mode== MACHMODE_FX && dmac->GetInputSlotTypes() > 1 )
							{
								if (MessageBox("Should I connect this to a send/return input?","Mixer Connection",MB_YESNO) == IDYES )
								{
									w = dmac->GetFreeInputWire(1);
								}
								else { w = dmac->GetFreeInputWire(0); }
							}
							else { w = dmac->GetFreeInputWire(0); }
							if (!_pSong.ChangeWireDestMacBlocking(tmac,dmac,wiremove,w))
							{
								MessageBox("Wire move could not be completed!","Error!", MB_ICONERROR);
							}
						}
						else
						{
							Machine *tmac = _pSong._pMachine[propMac];
							Machine *dmac = _pSong._pMachine[wiredest];
							int srctype=0;
							///\todo: for multi-io.
							//if ( tmac->GetOutputSlotTypes() > 1 ) ask user and get index
							if (!_pSong.ChangeWireSourceMacBlocking(tmac,dmac,tmac->GetFreeOutputWire(srctype),wiremove))
							{
								MessageBox("Wire move could not be completed!","Error!", MB_ICONERROR);
							}
						}
					}
					else if ((wiresource != -1) && (propMac != wiresource)) // Are we creating a connection?
					{
						PsycleGlobal::inputHandler().AddMacViewUndo();
						Machine *tmac = _pSong._pMachine[wiresource];
						Machine *dmac = _pSong._pMachine[propMac];
						int dsttype=0;
						///\todo: for multi-io.
						//if ( tmac->GetOutputSlotTypes() > 1 ) ask user and get index
						///\todo: hardcoded for the Mixer machine. This needs to be extended with multi-io.
						if ( tmac->_mode== MACHMODE_FX && dmac->GetInputSlotTypes() > 1 )
						{
							if (MessageBox("Should I connect this to a send/return input?","Mixer Connection",MB_YESNO) == IDYES )
							{
								dsttype=1;
							}
						}
						if (_pSong.InsertConnectionBlocking(tmac, dmac,0,dsttype)== -1)
						{
							if (tmac->connectedOutputs() >= 12 || dmac->connectedInputs() >= 12) {
								MessageBox("Couldn't connect the selected machines!\nIf needing more than 12 connections use multiple dummy or mixer machines","Error!", MB_ICONERROR);
							}
							else {
								MessageBox("Couldn't connect the selected machines!","Error!", MB_ICONERROR);
							}
						}
					}
					else if ( smacmode == smac_modes::move && smac != -1 ) // Are we moving a machine?
					{
						SSkinSource ssrc;
						PsycleGlobal::inputHandler().AddMacViewUndo();

						switch(_pSong._pMachine[smac]->_mode)
						{
							case MACHMODE_GENERATOR:
								ssrc = MachineCoords->sGenerator;break;
							case MACHMODE_FX:
								ssrc = MachineCoords->sEffect;break;
							case MACHMODE_MASTER:
								ssrc = MachineCoords->sMaster;break;
							default:
								assert(false);ssrc=SSkinSource();break;
						}
						if (point.x-mcd_x < 0 ) _pSong._pMachine[smac]->_x = 0;
						else if	(point.x-mcd_x+ssrc.width > CW) 
						{ 
							_pSong._pMachine[smac]->_x = CW-ssrc.width; 
						}

						if (point.y-mcd_y < 0 ) _pSong._pMachine[smac]->_y = 0; 
						else if (point.y-mcd_y+ssrc.height > CH) 
						{ 
						_pSong._pMachine[smac]->_y = CH-ssrc.height; 
						}
						Repaint(); 
					}
				}
	
				smac = -1;		smacmode = smac_modes::move;
				wiresource = -1;wiredest = -1;
				wiremove = -1;
				Repaint();

			}
			else if (viewMode == view_modes::pattern)
			{
				
				if ( (blockStart) &&
					( point.y > YOFFSET && point.y < YOFFSET+(maxl*ROWHEIGHT)) &&
					(point.x > XOFFSET && point.x < XOFFSET+(maxt*ROWWIDTH)))
				{
					editcur.track = tOff + char((point.x-XOFFSET)/ROWWIDTH);
		//			if ( editcur.track >= _pSong.SONGTRACKS ) editcur.track = _pSong.SONGTRACKS-1;
		//			else if ( editcur.track < 0 ) editcur.track = 0;

		//			int plines = _pSong.patternLines[_ps()];
					editcur.line = lOff + (point.y-YOFFSET)/ROWHEIGHT;
		//			if ( editcur.line >= plines ) {  editcur.line = plines - 1; }
		//			else if ( editcur.line < 0 ) editcur.line = 0;

					editcur.col = _xtoCol((point.x-XOFFSET)%ROWWIDTH);
					Repaint(draw_modes::cursor);
					pParentMain->StatusBarIdle();
					if (!(nFlags & MK_SHIFT) && PsycleGlobal::conf().inputHandler()._windowsBlocks)
					{
						blockSelected=false;
						blockSel.end.line=0;
						blockSel.end.track=0;
						ChordModeOffs = 0;
						bScrollDetatch=false;
						Repaint(draw_modes::selection);
					}
				}
				else if (blockswitch)
				{
					if (blockSel.start.track != blockLastOrigin.start.track ||
						blockSel.start.line != blockLastOrigin.start.line)
					{
						CSelection dest = blockSel;
						blockSel = blockLastOrigin;
						if ( nFlags & MK_CONTROL ) 
						{
							CopyBlock(false);
							PasteBlock(dest.start.track,dest.start.line,false);
						}
						else SwitchBlock(dest.start.track,dest.start.line);
						blockSel = dest;
					}
					else blockSelected=false; 
					blockswitch=false;
					Repaint(draw_modes::selection);
				}
			}//<-- End LBUTTONPRESING/VIEWMODE switch statement
			CWnd::OnLButtonUp(nFlags,point);
		}


		void CChildView::OnMouseMove( UINT nFlags, CPoint point )
		{
			if (viewMode == view_modes::machine)
			{
				if (smac > -1 && (nFlags & MK_LBUTTON))
				{
					if (_pSong._pMachine[smac])
					{
						if (smacmode == smac_modes::move)
						{
							_pSong._pMachine[smac]->_x = point.x-mcd_x;
							_pSong._pMachine[smac]->_y = point.y-mcd_y;

							char buf[128];
							sprintf(buf, "%s (%d,%d)", _pSong._pMachine[smac]->_editName, _pSong._pMachine[smac]->_x, _pSong._pMachine[smac]->_y);
							pParentMain->StatusBarText(buf);
							Repaint();
						}
						else if ((smacmode == smac_modes::panning) && (_pSong._pMachine[smac]->_mode != MACHMODE_MASTER))
						{
							int newpan = 64;
							switch(_pSong._pMachine[smac]->_mode)
							{
							case MACHMODE_GENERATOR:
								newpan = (point.x - _pSong._pMachine[smac]->_x - MachineCoords->dGeneratorPan.x - (MachineCoords->sGeneratorPan.width/2))*128;
								if (MachineCoords->dGeneratorPan.width)
								{
									newpan /= MachineCoords->dGeneratorPan.width;
								}
								break;
							case MACHMODE_FX:
								newpan = (point.x - _pSong._pMachine[smac]->_x - MachineCoords->dEffectPan.x - (MachineCoords->sEffectPan.width/2))*128;
								if (MachineCoords->dEffectPan.width)
								{
									newpan /= MachineCoords->dEffectPan.width;
								}
								break;
							}

							if (_pSong._pMachine[smac]->GetPan() != newpan)
							{
								PsycleGlobal::inputHandler().AddMacViewUndo();

								_pSong._pMachine[smac]->SetPan(newpan);
								newpan= _pSong._pMachine[smac]->GetPan();
								
								char buf[128];
								if (newpan != 64)
								{
									sprintf(buf, "%s Pan: %.0f%% Left / %.0f%% Right", _pSong._pMachine[smac]->_editName, 100.0f - ((float)newpan*0.78125f), (float)newpan*0.78125f);
								}
								else
								{
									sprintf(buf, "%s Pan: Center", _pSong._pMachine[smac]->_editName);
								}
								pParentMain->StatusBarText(buf);
								updatePar = smac;
								Repaint(draw_modes::machine);
							}
						}
					}
				}
				if (((nFlags == (MK_SHIFT | MK_LBUTTON)) || (nFlags == MK_RBUTTON)) && (wiresource != -1))
				{
					wireDX = point.x;
					wireDY = point.y;
					Repaint();
				}
				else if (((nFlags == (MK_CONTROL | MK_LBUTTON)) || (nFlags == (MK_CONTROL | MK_RBUTTON))) && (wiredest != -1))
				{
					wireSX = point.x;
					wireSY = point.y;
					Repaint();
				}
			}

			else if (viewMode == view_modes::pattern)
			{
				if ((nFlags & MK_LBUTTON) && oldm.track != -1)
				{
					draw_modes::draw_mode paintmode = draw_modes::all;
					if(scrollDelay <=0) {
						ntOff = tOff;
						nlOff = lOff;
					}

					int ttm = tOff + (point.x-XOFFSET)/ROWWIDTH;
					if ( point.x < XOFFSET ) ttm--; // 1/2 = 0 , -1/2 = 0 too!
					int ccm;
					if ( ttm < tOff ) // Exceeded from left
					{
						ccm=0;
						if ( ttm < 0 ) { ttm = 0; } // Out of Range
						// and Scroll
						if(scrollDelay <=0)
						{
							ntOff = ttm;
							if (ntOff != tOff) paintmode=draw_modes::horizontal_scroll;
						}
					}
					else if ( ttm - tOff >= VISTRACKS ) // Exceeded from right
					{
						ccm=8;
						if ( ttm >= _pSong.SONGTRACKS ) // Out of Range
						{	
							ttm = _pSong.SONGTRACKS-1;
							if ( tOff != ttm-VISTRACKS ) 
							{ 
								ntOff = ttm-VISTRACKS+1; 
								paintmode=draw_modes::horizontal_scroll; 
							}
						}
						else if(scrollDelay <=0)	//scroll
						{	
							ntOff = ttm-VISTRACKS+1;
							if ( ntOff != tOff ) 
								paintmode=draw_modes::horizontal_scroll;
						}
					}
					else // Not exceeded
					{
						ccm=_xtoCol((point.x-XOFFSET)%ROWWIDTH);
					}

					int plines = _pSong.patternLines[_ps()];
					int llm = lOff + (point.y-YOFFSET)/ROWHEIGHT;
					if ( point.y < YOFFSET ) llm--; // 1/2 = 0 , -1/2 = 0 too!

					if ( llm < lOff ) // Exceeded from top
					{
						
						if ( llm < 0 ) // Out of range
						{	
							llm = 0;
							if ( lOff != 0 ) 
							{ 
								nlOff = 0; 
								paintmode=draw_modes::vertical_scroll; 
							}
						}
						else if(scrollDelay <=0) //scroll
						{	
							nlOff = llm;
							if ( nlOff != lOff ) 
								paintmode=draw_modes::vertical_scroll;
						}
					}
					else if ( llm - lOff >= VISLINES ) // Exceeded from bottom
					{
						if ( llm >= plines ) //Out of Range
						{	
							llm = plines-1;
							if ( lOff != llm-VISLINES) 
							{ 
								nlOff = llm-VISLINES+1; 
								paintmode=draw_modes::vertical_scroll; 
							}
						}
						else if(scrollDelay <=0) //scroll
						{	
							nlOff = llm-VISLINES+1;
							if ( nlOff != lOff ) 
								paintmode=draw_modes::vertical_scroll;
						}
					}
					
					else if ( llm >= plines ) { llm = plines-1; } //Out of Range

					if ((ttm != oldm.track ) || (llm != oldm.line) || (ccm != oldm.col))
					{
						if (blockStart) 
						{
							blockStart = false;
							blockSelected=false;
							blockSel.end.line=0;
							blockSel.end.track=0;
							StartBlock(oldm.track,oldm.line,oldm.col);
						}
						else if ( blockswitch ) 
						{
							blockSelectBarState = 1;

							int tstart = (blockLastOrigin.start.track+(ttm-editcur.track) >= 0)?(ttm-editcur.track):-blockLastOrigin.start.track;
							int lstart = (blockLastOrigin.start.line+(llm-editcur.line) >= 0)?(llm-editcur.line):-blockLastOrigin.start.line;
							if (blockLastOrigin.end.track+(ttm-editcur.track) >= _pSong.SONGTRACKS) tstart = _pSong.SONGTRACKS-blockLastOrigin.end.track-1;
							if (blockLastOrigin.end.line+(llm-editcur.line) >= plines) lstart = plines - blockLastOrigin.end.line-1;

							blockSel.start.track=blockLastOrigin.start.track+(tstart);
							blockSel.start.line=blockLastOrigin.start.line+(lstart);
							iniSelec = blockSel.start;
							int tend = blockLastOrigin.end.track+(tstart);
							int lend = blockLastOrigin.end.line+(lstart);
							ChangeBlock(tend,lend,ccm);
						}
						else ChangeBlock(ttm,llm,ccm);
						oldm.track=ttm;
						oldm.line=llm;
						oldm.col=ccm;
						paintmode=draw_modes::selection;
					}

					if(scrollDelay <=0) {
						bScrollDetatch=true;
						detatchpoint.track = ttm;
						detatchpoint.line = llm;
						detatchpoint.col = ccm;
						scrollDelay=6;
					}
					scrollDelay--;
					if (nFlags & MK_SHIFT)
					{
						editcur = detatchpoint;
						if (!paintmode)
						{
							paintmode=draw_modes::cursor;
						}
					}

					if (paintmode)
					{
						Repaint(paintmode);
					}
				}
				else if (nFlags == MK_MBUTTON)
				{
					// scrolling
					if (abs(point.y - MBStart.y) > ROWHEIGHT)
					{
						int nlines = _pSong.patternLines[_ps()];
						int delta = (point.y - MBStart.y)/ROWHEIGHT;
						int nPos = lOff - delta;
						if (nPos > lOff )
						{
							if (nPos < 0)
								nPos = 0;
							else if (nPos > nlines-VISLINES)
								nlOff = nlines-VISLINES;
							else
								nlOff=nPos;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						else if (nPos < lOff )
						{
							if (nPos < 0)
								nlOff = 0;
							else if (nPos > nlines-VISLINES)
								nlOff = nlines-VISLINES;
							else
								nlOff=nPos;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						MBStart.y += delta*ROWHEIGHT;
					}
					// switching tracks
					if (abs(point.x - MBStart.x) > (ROWWIDTH))
					{
						int delta = (point.x - MBStart.x)/(ROWWIDTH);
						int nPos = tOff - delta;
						if (nPos > tOff)
						{
							if (nPos < 0)
								ntOff= 0;
							else if (nPos>_pSong.SONGTRACKS-VISTRACKS)
								ntOff=_pSong.SONGTRACKS-VISTRACKS;
							else
								ntOff=nPos;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::horizontal_scroll);
						}
						else if (nPos < tOff)
						{
							if (nPos < 0)
								ntOff= 0;
							else if (nPos>_pSong.SONGTRACKS-VISTRACKS)
								ntOff=_pSong.SONGTRACKS-VISTRACKS;
							else
								ntOff=nPos;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::horizontal_scroll);
						}
						MBStart.x += delta*ROWWIDTH;
					}
				}
				else if (point.y >= 0 && point.y < YOFFSET ) // Mouse is in Track Header.
				{	
					int pointpos = ((point.x-XOFFSET)%ROWWIDTH) - HEADER_INDENT;
					int trackpos = ((point.x-XOFFSET)/ROWWIDTH);
					if (InRect(pointpos,point.y,PatHeaderCoords->dMuteOnTrack,PatHeaderCoords->sMuteOnTracking)){
						trackingMuteTrack=ntOff+trackpos;
						trackingRecordTrack=trackingSoloTrack=-1;
						Repaint(draw_modes::track_header);
					}
					else if (InRect(pointpos,point.y,PatHeaderCoords->dRecordOnTrack,PatHeaderCoords->sRecordOnTracking)){
						trackingRecordTrack=ntOff+trackpos;
						trackingMuteTrack=trackingSoloTrack=-1;
						Repaint(draw_modes::track_header);
					}
					else if (InRect(pointpos,point.y,PatHeaderCoords->dSoloOnTrack,PatHeaderCoords->sSoloOnTracking)){
						trackingSoloTrack=ntOff+trackpos;
						trackingMuteTrack=trackingRecordTrack=-1;
						Repaint(draw_modes::track_header);
					}
					else if ((trackingMuteTrack&trackingRecordTrack&trackingSoloTrack) != -1) {
						trackingMuteTrack=trackingRecordTrack=trackingSoloTrack=-1;
						Repaint(draw_modes::track_header);
					}
				}
				else if ((trackingMuteTrack&trackingRecordTrack&trackingSoloTrack) != -1) {
					trackingMuteTrack=trackingRecordTrack=trackingSoloTrack=-1;
					Repaint(draw_modes::track_header);
				}
			}//<-- End LBUTTONPRESING/VIEWMODE switch statement
			CWnd::OnMouseMove(nFlags,point);
		}


		void CChildView::OnLButtonDblClk( UINT nFlags, CPoint point )
		{
			int tmac=-1;
			
			switch (viewMode)
			{
				case view_modes::machine: // User is in machine view mode
				
					tmac = GetMachine(point);

					if(tmac>-1)
					{
						Machine *pMac =  _pSong._pMachine[tmac];
						SSkinDest tmpsrc;
						switch (pMac->_mode)
						{
						case MACHMODE_GENERATOR:
							tmpsrc.x=MachineCoords->dGeneratorPan.x; tmpsrc.y=MachineCoords->dGeneratorPan.y;
							if (InRect(mcd_x,mcd_y,tmpsrc,MachineCoords->sGeneratorPan)) //changing panning
							{
								smac=tmac;
								smacmode = smac_modes::panning;
								OnMouseMove(nFlags,point);
								return;
							}
							else if (InRect(mcd_x,mcd_y, MachineCoords->dGeneratorMute,MachineCoords->sGeneratorMute)) //Mute 
							{
								pMac->_mute = !pMac->_mute;
								if (pMac->_mute)
								{
									pMac->_volumeCounter=0.0f;
									pMac->_volumeDisplay=0;
									if (_pSong.machineSoloed == tmac )
									{
										_pSong.machineSoloed = -1;
									}
								}
								updatePar = tmac;
								Repaint(draw_modes::machine);
								return;
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dGeneratorSolo,MachineCoords->sGeneratorSolo)) //Solo 
							{
								if (_pSong.machineSoloed == tmac )
								{
									_pSong.machineSoloed = -1;
									for ( int i=0;i<MAX_MACHINES;i++ )
									{
										if ( _pSong._pMachine[i] )
										{
											if ( _pSong._pMachine[i]->_mode == MACHMODE_GENERATOR )
											{
												_pSong._pMachine[i]->_mute = false;
											}
										}
									}
								}
								else 
								{
									for ( int i=0;i<MAX_MACHINES;i++ )
									{
										if ( _pSong._pMachine[i] ) 
										{
											if (( _pSong._pMachine[i]->_mode == MACHMODE_GENERATOR ) && (i != tmac))
											{
												_pSong._pMachine[i]->_mute = true;
												_pSong._pMachine[i]->_volumeCounter=0.0f;
												_pSong._pMachine[i]->_volumeDisplay=0;
											}
										}
									}
									pMac->_mute = false;
									_pSong.machineSoloed = tmac;
								}
								updatePar = tmac;
								Repaint(draw_modes::machine);
								return;
							}
							break;
						case MACHMODE_FX:
							tmpsrc.x=MachineCoords->dEffectPan.x; tmpsrc.y=MachineCoords->dEffectPan.y;
							if (InRect(mcd_x,mcd_y,tmpsrc,MachineCoords->sEffectPan)) //changing panning
							{
								smac=tmac;
								smacmode = smac_modes::panning;
								OnMouseMove(nFlags,point);
								return;
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dEffectMute,MachineCoords->sEffectMute)) //Mute 
							{
								pMac->_mute = !pMac->_mute;
								if (pMac->_mute)
								{
									pMac->_volumeCounter=0.0f;
									pMac->_volumeDisplay=0;
								}
								updatePar = tmac;
								Repaint(draw_modes::machine);
								return;
							}
							else if (InRect(mcd_x,mcd_y,MachineCoords->dEffectBypass,MachineCoords->sEffectBypass)) //Bypass
							{
								pMac->Bypass(!pMac->Bypass());
								if (pMac->Bypass())
								{
									pMac->_volumeCounter=0.0f;
									pMac->_volumeDisplay=0;
								}
								updatePar = tmac;
								Repaint(draw_modes::machine);
								return;
							}
							break;

						case MACHMODE_MASTER:
							break;
						}            
            pParentMain->ShowMachineGui(tmac, point);                        
            Repaint();
					}
					else
					{
						int tmp, tmp2;
						Wire* w = GetWire(point,tmp,tmp2);
						if ( w != NULL )
						{
							Machine &tmac = w->GetSrcMachine();
							wiresource = tmac._macIndex;
							Machine &dmac = w->GetDstMachine();
							int free=-1;
							for (int i = 0; i < MAX_WIRE_DIALOGS; i++)
							{
								if (WireDialog[i])
								{
									if ((WireDialog[i]->srcMachine._macIndex == tmac._macIndex) &&
										(WireDialog[i]->dstMachine._macIndex == dmac._macIndex))  // If this is true, the dialog is already open
									{
										wiresource = -1;
										return;
									}
								}
								else free = i;
							}
							if (free != -1) //If there is any dialog slot open
							{
								CWireDlg* wdlg;
								wdlg = WireDialog[free] = new CWireDlg(this, &WireDialog[free], 
										free, *w);
								pParentMain->CenterWindowOnPoint(wdlg, point);
								wdlg->ShowWindow(SW_SHOW);
								wiresource = -1;
								return;
							}
							else
							{
								MessageBox("Cannot show the wire dialog. Too many of them opened!","Error!", MB_ICONERROR);
							}
						}
						// if no connection then Show new machine dialog
						else NewMachine(point.x,point.y);
						wiresource = -1;
		//				Repaint();
					}
				
					break;

					////////////////////////////////////////////////////////////////

				case view_modes::pattern: // User is in pattern view mode
					if (( point.y >= YOFFSET ) && (point.x >= XOFFSET))
					{
						const int ttm = tOff + (point.x-XOFFSET)/ROWWIDTH;
						const int nl = _pSong.patternLines[_pSong.playOrder[editPosition]];

						StartBlock(ttm,0,0);
						EndBlock(ttm,nl-1,8);
						blockStart = false;
					}

					break;


			} // <-- End switch(viewMode)
			CWnd::OnLButtonDblClk(nFlags,point);
		}



		BOOL CChildView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
		{
			if ( viewMode == view_modes::pattern )
			{
				int nlines = _pSong.patternLines[_ps()];
				int nPos = lOff - (zDelta/30);
				if (nPos > lOff )
				{
					if (nPos < 0)
						nPos = 0;
					else if (nPos > nlines-VISLINES)
						nlOff = nlines-VISLINES;
					else
						nlOff=nPos;
					bScrollDetatch=true;
					detatchpoint.track = ntOff+1;
					detatchpoint.line = nlOff+1;
					Repaint(draw_modes::vertical_scroll);
				}
				else if (nPos < lOff )
				{
					if (nPos < 0)
						nlOff = 0;
					else if (nPos > nlines-VISLINES)
						nlOff = nlines-VISLINES;
					else
						nlOff=nPos;
					bScrollDetatch=true;
					detatchpoint.track = ntOff+1;
					detatchpoint.line = nlOff+1;
					Repaint(draw_modes::vertical_scroll);
				}
			}
			return CWnd ::OnMouseWheel(nFlags, zDelta, pt);
		}

		void CChildView::OnMButtonDown( UINT nFlags, CPoint point )
		{
			MBStart.x = point.x;
			MBStart.y = point.y;
			CWnd ::OnMButtonDown(nFlags, point);
		}

		void CChildView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
		{      
			if ( viewMode == view_modes::pattern )
			{       
				switch(nSBCode)
				{
					case SB_LINEDOWN:
						if ( lOff<_pSong.patternLines[_ps()]-VISLINES)
						{
							nlOff=lOff+1;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						break;
					case SB_LINEUP:
						if ( lOff>0 )
						{
							nlOff=lOff-1;
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						break;
					case SB_PAGEDOWN:
						if ( lOff<_pSong.patternLines[_ps()]-VISLINES)
						{
							const int nl = _pSong.patternLines[_ps()]-VISLINES;
							nlOff=lOff+16;
							if (nlOff > nl)
							{
								nlOff = nl;
							}
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						break;
					case SB_PAGEUP:
						if ( lOff>0)
						{
							nlOff=lOff-16;
							if (nlOff < 0)
							{
								nlOff = 0;
							}
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						break;
					case SB_THUMBPOSITION:
					case SB_THUMBTRACK:
						if (nlOff!=(int)nPos)
						{
							const int nl = _pSong.patternLines[_ps()]-VISLINES;
							nlOff=(int)nPos;
							if (nlOff > nl)
							{
								nlOff = nl;
							}
							else if (nlOff < 0)
							{
								nlOff = 0;
							}
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::vertical_scroll);
						}
						break;
					default: 
						break;
				}
			}
			CWnd ::OnVScroll(nSBCode, nPos, pScrollBar);
		}


		void CChildView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
		{     
			if ( viewMode == view_modes::pattern )
			{
				switch(nSBCode)
				{
					case SB_LINERIGHT:
					case SB_PAGERIGHT:
						if ( tOff<_pSong.SONGTRACKS-VISTRACKS)
						{
							ntOff=tOff+1;
//	Disabled, since people find it as a bug, not as a feature.
//  Reenabled, because else, when the cursor jumps to next line, it gets redrawn 
//   and the scrollbar position reseted.
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::horizontal_scroll);
						}
						break;
					case SB_LINELEFT:
					case SB_PAGELEFT:
						if ( tOff>0 )
						{
							ntOff=tOff-1;
//	Disabled, since people find it as a bug, not as a feature.
//  Reenabled, because else, when the cursor jumps to next line, it gets redrawn 
//   and the scrollbar position reseted.
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::horizontal_scroll);
						}
						else PrevTrack(1,false);
						break;
					case SB_THUMBPOSITION:
					case SB_THUMBTRACK:
						if (ntOff!=(int)nPos)
						{
							const int nt = _pSong.SONGTRACKS;
							ntOff=(int)nPos;
							if (ntOff >= nt)
							{
								ntOff = nt-1;
							}
							else if (ntOff < 0)
							{
								ntOff = 0;
							}
//	Disabled, since people find it as a bug, not as a feature.
//  Reenabled, because else, when the cursor jumps to next line, it gets redrawn 
//   and the scrollbar position reseted.
							bScrollDetatch=true;
							detatchpoint.track = ntOff+1;
							detatchpoint.line = nlOff+1;
							Repaint(draw_modes::horizontal_scroll);
						}
						break;
					default: 
						break;
				}
			}

			CWnd ::OnHScroll(nSBCode, nPos, pScrollBar);
		}


		void CChildView::OnPopMacOpenParams()
		{
			pParentMain->ShowMachineGui(popupmacidx, trackingpoint);
		}
		void CChildView::OnPopMacOpenProperties()
		{
			DoMacPropDialog(popupmacidx);
		}
		void CChildView::OnPopMacOpenBankManager()
		{
			CPresetsDlg dlg(this);
			dlg._pMachine=_pSong._pMachine[popupmacidx];
			dlg.DoModal();
		}
		void CChildView::OnPopMacConnecTo(UINT nID)
		{
			Machine *tmac = _pSong._pMachine[popupmacidx];
			Machine *dmac = _pSong._pMachine[MAX_BUSES+nID-ID_CONNECTTO_MACHINE0];
			int dsttype=0;
			///\todo: for multi-io.
			//if ( tmac->GetOutputSlotTypes() > 1 ) ask user and get index
			///\todo: hardcoded for the Mixer machine. This needs to be extended with multi-io.
			if ( tmac->_mode== MACHMODE_FX && dmac->GetInputSlotTypes() > 1 )
			{
				if (MessageBox("Should I connect this to a send/return input?","Mixer Connection",MB_YESNO) == IDYES )
				{
					dsttype=1;
				}
			}
			if (_pSong.InsertConnectionBlocking(tmac, dmac,0,dsttype)== -1)
			{
				if (tmac->connectedOutputs() >= 12 || dmac->connectedInputs() >= 12) {
					MessageBox("Couldn't connect the selected machines!\nIf needing more than 12 connections use multiple dummy or mixer machines","Error!", MB_ICONERROR);
				}
				else {
					MessageBox("Couldn't connect the selected machines!","Error!", MB_ICONERROR);
				}
			}
			Repaint();
		}
		void CChildView::OnPopMacShowWire(UINT nID)
		{
			int wireIdx = nID-ID_CONNECTIONS_CONNECTION0;
			Machine &tmac = *_pSong._pMachine[popupmacidx];
			Wire* w = (wireIdx < MAX_CONNECTIONS) ? &tmac.inWires[wireIdx] : tmac.outWires[wireIdx-MAX_CONNECTIONS];
			Machine &dmac = w->GetDstMachine();
			int free=-1;
			for (int i = 0; i < MAX_WIRE_DIALOGS; i++)
			{
				if (WireDialog[i])
				{
					if ((WireDialog[i]->srcMachine._macIndex == tmac._macIndex) &&
						(WireDialog[i]->dstMachine._macIndex == dmac._macIndex))  // If this is true, the dialog is already open
					{
						WireDialog[i]->SetFocus();
						return;
					}
				}
				else free = i;
			}
			if (free != -1) //If there is any dialog slot open
			{
				CWireDlg* wdlg = WireDialog[free] = new CWireDlg(this, &WireDialog[free], free, *w);
				pParentMain->CenterWindowOnPoint(wdlg, trackingpoint);
				wdlg->ShowWindow(SW_SHOW);
				wiresource = -1;
				return;
			}
			else
			{
				MessageBox("Cannot show the wire dialog. Too many of them opened!","Error!", MB_ICONERROR);
			}
		}
		void CChildView::OnPopMacReplaceMac()
		{
			Machine* pMachine = _pSong._pMachine[popupmacidx];
			NewMachine(pMachine->_x,pMachine->_y,popupmacidx);

			Machine* newMac = _pSong._pMachine[popupmacidx];
			if(newMac) {
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				Repaint();
			}
		}
		void CChildView::OnPopMacCloneMac()
		{
			int src = popupmacidx;
			int dst = -1;

			if ((src < MAX_BUSES) && (src >=0)) {
				dst = _pSong.GetFreeBus();
			}
			else if ((src < MAX_BUSES*2) && (src >= MAX_BUSES)) {
				dst = _pSong.GetFreeFxBus();
			}
			if (dst >= 0)
			{
				if (!_pSong.CloneMac(src,dst))
				{
					MessageBox("Cloning failed. Is your song directory correctly configured?","Cloning failed");
				}
				pParentMain->UpdateComboGen(true);
				Repaint();
			}
		}
		void CChildView::OnPopMacInsertBefore()
		{
			int newMacidx = _pSong.GetFreeFxBus();
			Machine* pMachine = _pSong._pMachine[popupmacidx];
			NewMachine(pMachine->_x-16,pMachine->_y-16,newMacidx);

			Machine* newMac = _pSong._pMachine[newMacidx];
			if(newMac) {
				CExclusiveLock lock(&_pSong.semaphore, 2, true);
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					if(pMachine->inWires[i].Enabled()) {
						Machine& srcMac = pMachine->inWires[i].GetSrcMachine();
						int wiresrc = srcMac.FindOutputWire(pMachine->_macIndex);
						_pSong.ChangeWireDestMacNonBlocking(&srcMac,newMac,wiresrc,newMac->GetFreeInputWire(0));
					}
				}
				_pSong.InsertConnectionNonBlocking(newMac, pMachine);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				Repaint();
			}
		}
		void CChildView::OnPopMacInsertAfter()
		{
			int newMacidx = _pSong.GetFreeFxBus();
			Machine* pMachine = _pSong._pMachine[popupmacidx];
			NewMachine(pMachine->_x+16,pMachine->_y+16,newMacidx);

			Machine* newMac = _pSong._pMachine[newMacidx];
			if(newMac) {
				CExclusiveLock lock(&_pSong.semaphore, 2, true);
				for(int i = 0; i < MAX_CONNECTIONS; i++) {
					Wire* wire = pMachine->outWires[i];
					if(wire && wire->Enabled()) {
						Machine& dstMac = wire->GetDstMachine();
						int wiredst = wire->GetDstWireIndex();
						_pSong.ChangeWireSourceMacNonBlocking(newMac,&dstMac,newMac->GetFreeOutputWire(0),wiredst);
					}
				}
				_pSong.InsertConnectionNonBlocking(pMachine, newMac);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				Repaint();
			}
		}
		void CChildView::OnPopMacDeleteMachine()
		{
			if (MessageBox("Are you sure?","Delete Machine", MB_YESNO|MB_ICONEXCLAMATION) == IDYES)
			{
				pParentMain->CloseMacGui(popupmacidx);
				{
					CExclusiveLock lock(&_pSong.semaphore, 2, true);
					_pSong.DeleteMachineRewiring(popupmacidx);
				}

				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (pParentMain->pGearRackDialog)
				{
					pParentMain->RedrawGearRackList();
				}
			}
		}
		void CChildView::OnPopMacMute()
		{
			Machine *pmac = _pSong._pMachine[popupmacidx];
			pmac->_mute = (!pmac->_mute);
			pmac->_volumeCounter=0.0f;
			pmac->_volumeDisplay = 0;
			updatePar=popupmacidx;
			Repaint(draw_modes::machine);
		}
		void CChildView::OnPopMacSolo()
		{
			if (_pSong.machineSoloed == popupmacidx) {
				_pSong.SoloMachine(-1);
			}
			else {
				_pSong.SoloMachine(popupmacidx);
			}
			Repaint(draw_modes::all_machines);
		}
		void CChildView::OnPopMacBypass()
		{
			Machine *pmac = _pSong._pMachine[popupmacidx];
			pmac->Bypass(!pmac->Bypass());
			updatePar=popupmacidx;
			Repaint(draw_modes::machine);
		}
		void CChildView::OnUpdateMacOpenProps(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_MASTER );
		}
		void CChildView::OnUpdateMacReplace(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_MASTER );
		}
		void CChildView::OnUpdateMacClone(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_MASTER );
		}
		void CChildView::OnUpdateMacInsertBefore(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_GENERATOR );
		}
		void CChildView::OnUpdateMacInsertAfter(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_MASTER );
		}
		void CChildView::OnUpdateMacDelete(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode != MACHMODE_MASTER );
		}
		void CChildView::OnUpdateMacMute(CCmdUI* pCmdUI)
		{
			pCmdUI->SetCheck(_pSong._pMachine[popupmacidx]->_mute);
		}
		void CChildView::OnUpdateMacSolo(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode == MACHMODE_GENERATOR );
			pCmdUI->SetCheck(_pSong.machineSoloed == popupmacidx);
		}
		void CChildView::OnUpdateMacBypass(CCmdUI* pCmdUI)
		{
			pCmdUI->Enable(_pSong._pMachine[popupmacidx]->_mode == MACHMODE_FX );
			pCmdUI->SetCheck(_pSong._pMachine[popupmacidx]->Bypass());
		}


		void CChildView::FillConnectToPopup(CMenu* pPopup)
		{
			DeleteSubmenu(pPopup);
			if (_pSong._pMachine[popupmacidx]->_type == MACH_MASTER)
			{
				return;
			}

			char s1[64];
			for (int j=MAX_BUSES; j < 2*MAX_BUSES; ++j)
			{
				if (j != popupmacidx && _pSong._pMachine[j] != NULL
						&& _pSong._pMachine[popupmacidx]->FindOutputWire(_pSong._pMachine[j]->_macIndex) == -1
						&& _pSong._pMachine[j]->FindOutputWire(popupmacidx) == -1) {
					std::sprintf(s1,"%02X: %s",_pSong._pMachine[j]->_macIndex,_pSong._pMachine[j]->GetEditName());
					pPopup->AppendMenu(MF_STRING, ID_CONNECTTO_MACHINE0 + j - MAX_BUSES, s1);
				}
			}
			if ( _pSong._pMachine[popupmacidx]->FindOutputWire(MASTER_INDEX) == -1) {
				std::sprintf(s1,"%02X: %s", MASTER_INDEX,_pSong._pMachine[MASTER_INDEX]->GetName());
				pPopup->AppendMenu(MF_STRING, ID_CONNECTTO_MACHINE64, s1);
			}
		}

		void CChildView::FillConnectionsPopup(CMenu* pPopup)
		{
			DeleteSubmenu(pPopup);
			char s1[64];
			int j=0;
			std::vector<Wire> & wiresin = _pSong._pMachine[popupmacidx]->inWires;
			for (std::vector<Wire>::const_iterator ite = wiresin.begin(); ite != wiresin.end(); ++ite)
			{
				if (ite->Enabled()) {
					std::sprintf(s1,"%02X: %s",ite->GetSrcMachine()._macIndex,ite->GetSrcMachine().GetEditName());
					pPopup->AppendMenu(MF_STRING, ID_CONNECTIONS_CONNECTION0 + j, s1);
				}
				j++;
			}
			j=12;
			pPopup->AppendMenu(MF_SEPARATOR, ID_CONNECTIONS_CONNECTION0 + j, s1);
			std::vector<Wire*> & wiresout = _pSong._pMachine[popupmacidx]->outWires;
			for (std::vector<Wire*>::const_iterator ite = wiresout.begin(); ite != wiresout.end(); ++ite)
			{
				if (*ite != NULL && (*ite)->Enabled()) {
					std::sprintf(s1,"%02X: %s",(*ite)->GetDstMachine()._macIndex,(*ite)->GetDstMachine().GetEditName());
					pPopup->AppendMenu(MF_STRING, ID_CONNECTIONS_CONNECTION0 + j, s1);
				}
				j++;
			}
		}

		bool CChildView::DeleteSubmenu(CMenu* popMenu)
		{
			CMenu* secMenu=0;
			while (popMenu->GetMenuItemCount() > 0)
			{
				if ((secMenu=popMenu->GetSubMenu(0)))
				{
					DeleteSubmenu(secMenu);
				}
				popMenu->DeleteMenu(0,MF_BYPOSITION);
			}
			return true;
		}
 

}}
