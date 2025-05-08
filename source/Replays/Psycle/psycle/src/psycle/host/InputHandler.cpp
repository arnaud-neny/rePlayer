///\file
///\brief implementation file for psycle::host::InputHandler.
#include <psycle/host/detail/project.private.hpp>
#include "InputHandler.hpp"

#include "MainFrm.hpp"
#include "ChildView.hpp"

#include "Machine.hpp"
#include "Player.hpp"

// For SAMPLER_CMD_VOLUME
#include "Sampler.hpp"
// For Alert
#include "Ui.hpp"
// For Tweak
#include "LuaInternals.hpp"

namespace psycle
{
	namespace host
	{
		SPatternUndo::SPatternUndo()
			:pData(NULL)
			,dataSize(0)
		{}

		SPatternUndo::SPatternUndo(const SPatternUndo& other)
		{
			Copy(other);
		}
		SPatternUndo& SPatternUndo::operator=(const SPatternUndo &other)
		{
			if(pData != NULL) delete pData;
			Copy(other);
			return *this;
		}
		void SPatternUndo::Copy(const SPatternUndo &other)
		{
			if(other.dataSize > 0) {
				pData = new unsigned char[other.dataSize];
				memmove(pData,other.pData,other.dataSize);
				dataSize = other.dataSize;
			}
			else {
				pData = NULL;
				dataSize = 0;
			}
			type = other.type;
			pattern = other.pattern;
			x = other.x;
			y = other.y;
			tracks = other.tracks;
			lines = other.lines;
			edittrack = other.edittrack;
			editline = other.editline;
			editcol = other.editcol;
			seqpos = other.seqpos;
			counter = other.counter;
		}
		SPatternUndo::~SPatternUndo()
		{
			if(pData) delete pData;
		}

		InputHandler::InputHandler()
			:UndoCounter(0)
			,UndoSaved(0)
			,UndoMacCounter(0)
			,UndoMacSaved(0)
			,param_translator_dummy(new ParamTranslator())
		{
			bDoingSelection = false;

			// set up multi-channel playback
			for(UINT i=0;i<MAX_TRACKS;i++) {
				notetrack[i]=notecommands::release;
				instrtrack[i]=255;
				mactrack[i]=255;
			}
			outtrack=0;
		}

		InputHandler::~InputHandler()
		{
			KillRedo();
			KillUndo();
		}


		// KeyToCmd
		// IN: key + modifiers from OnKeyDown
		// OUT: command mapped to key
		CmdDef InputHandler::KeyToCmd(UINT nChar, UINT nFlags, bool has_shift, bool has_ctrl)
		{			
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();
			PsycleConfig& config = PsycleGlobal::conf();
			bDoingSelection=false;
			// invalid?	
			if(nChar>255)
			{
				CmdDef cmdNull;
				return cmdNull;
			}
			TRACE("Key nChar : %u pressed. Flags %u\n",nChar,nFlags);

			// special: right control mapped to PLAY
			if(settings.bCtrlPlay && GetKeyState(VK_RCONTROL)<0)
			{
				CmdDef cmdPlay(cdefPlaySong);
				return cmdPlay;
			}
			else
			{
				if (settings.bShiftArrowsDoSelect && GetKeyState(VK_SHIFT)<0 
					&& !(Global::player()._playing && config._followSong))
				{
					switch (nChar)
					{
					case VK_UP:
						{
							CmdDef cmdSel(cdefNavUp);
							bDoingSelection=true; 
							return cmdSel;
						}
						break;
					case VK_LEFT:
						{
							CmdDef cmdSel(cdefNavLeft);
							bDoingSelection=true;
							return cmdSel;
						}
						break;
					case VK_DOWN:
						{
							CmdDef cmdSel(cdefNavDn);
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					case VK_RIGHT:
						{
							CmdDef cmdSel(cdefNavRight); 
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					case VK_HOME:
						{
							CmdDef cmdSel(cdefNavTop); 
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					case VK_END:
						{
							CmdDef cmdSel(cdefNavBottom); 
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					case VK_PRIOR:
						{
							CmdDef cmdSel(cdefNavPageUp); 
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					case VK_NEXT:
						{
							CmdDef cmdSel(cdefNavPageDn); 
							bDoingSelection=true; 
							return cmdSel; 
						}
						break;
					}
				}
				nFlags= nFlags & ~MK_LBUTTON;
				// This comparison is to allow the "Shift+Note" (chord mode) to work.
				std::pair<int, int> pair(GetModifierIdx(nFlags, has_shift, has_ctrl), nChar);
				CmdDef& thisCmd = settings.keyMap[pair];

				if ( thisCmd.GetType() == CT_Note ) {
					return thisCmd;
				} else if ( thisCmd.GetID() == cdefNull) {
					std::pair<int, int> pair2(GetModifierIdx(nFlags, has_shift, has_ctrl) & ~MOD_S, nChar);
					thisCmd = settings.keyMap[pair2];
					return thisCmd;
				} else {
					return thisCmd;
				}
			}
		}	

		// StringToCmd
		// IN: command name (string)
		// OUT: command
		CmdDef InputHandler::StringToCmd(LPCTSTR str)
		{
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();
			std::map<CmdSet,std::pair<int,int>>::const_iterator it;
			for(it = settings.setMap.begin(); it != settings.setMap.end(); ++it)
			{
				CmdDef ret(it->first);
				if(!strcmp(ret.GetName(),str))
					return ret;
			}
			CmdDef ret(cdefNull);
			return ret;
		}

		// CmdToKey
		// IN: command def
		// OUT: key/mod command is defined for, 0/0 if not defined
		void InputHandler::CmdToKey(CmdSet cmd,WORD & key,WORD &mods)
		{
			key = 0;
			mods = 0;
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();
			std::map<CmdSet,std::pair<int,int>>::const_iterator it;
			it = settings.setMap.find(cmd);
			if (it != settings.setMap.end())
			{
				key = it->second.second;

				if(it->second.first & MOD_S)
					mods|=HOTKEYF_SHIFT;
				if(it->second.first & MOD_C)
					mods|=HOTKEYF_CONTROL;				
				if(it->second.first & MOD_E)
					mods|=HOTKEYF_EXT;
			}
		}


		// operations

		// perform command
		///\todo move to a callback system... this is disgustingly messy
		void InputHandler::PerformCmd(CmdDef &cmd, BOOL brepeat)
		{
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();
			PsycleConfig::PatternView& patSettings = PsycleGlobal::conf().patView();
			PsycleConfig& config = PsycleGlobal::conf();
			switch(cmd.GetID())
			{
			case cdefNull:
				break;

			case cdefPatternCut:
				pChildView->patCut();
				break;

			case cdefPatternCopy:
				pChildView->patCopy();
				break;

			case cdefPatternPaste:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->patPaste();
				break;

			case cdefPatternMixPaste:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->patMixPaste();
				break;

			case cdefPatternDelete:
				pChildView->patDelete();
				break;

			case cdefPatternTrackMute:
				pChildView->patTrackMute();
				break;

			case cdefPatternTrackSolo:
				pChildView->patTrackSolo();
				break;

			case cdefPatternTrackRecord:
				pChildView->patTrackRecord();
				break;

			case cdefFollowSong:	
				pMainFrame->ToggleFollowSong();
				break;

			case cdefKeyStopAny:
				pChildView->EnterNoteoffAny();
				break;

			case cdefColumnNext:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->AdvanceTrack(1,settings._wrapAround);
				break;

			case cdefColumnPrev:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->PrevTrack(1,settings._wrapAround);
				break;

			case cdefNavLeft:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( !bDoingSelection )
				{
					pChildView->PrevCol(settings._wrapAround);
					if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
				}
				else
				{
					if ( !pChildView->blockSelected )
					{
						pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
					}
					pChildView->PrevTrack(1,settings._wrapAround);
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}

				bDoingSelection = false;
				break;
			case cdefNavRight:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( !bDoingSelection )
				{
					pChildView->NextCol(settings._wrapAround);
					if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
				}
				else
				{
					if ( !pChildView->blockSelected)
					{
						pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
					}
					pChildView->AdvanceTrack(1,settings._wrapAround);
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}

				bDoingSelection = false;
				break;
			case cdefNavUp:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( bDoingSelection && !pChildView->blockSelected)
				{
					pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				if (pChildView->patStep == 0)
					pChildView->PrevLine(1,settings._wrapAround);
				else
					//if added by sampler. New option.
					if (!settings._NavigationIgnoresStep)
						pChildView->PrevLine(pChildView->patStep,settings._wrapAround);//before
					else
						pChildView->PrevLine(1,settings._wrapAround);//new option
				if ( bDoingSelection )
				{
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
				bDoingSelection = false;
				break;
			case cdefNavDn:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( bDoingSelection && !pChildView->blockSelected)
				{
					pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				if (pChildView->patStep == 0)
					pChildView->AdvanceLine(1,settings._wrapAround);
				else
					//if added by sampler. New option.
					if (!settings._NavigationIgnoresStep)
						pChildView->AdvanceLine(pChildView->patStep,settings._wrapAround); //before
					else
						pChildView->AdvanceLine(1,settings._wrapAround);//new option
				if ( bDoingSelection )
				{
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
				bDoingSelection = false;
				break;
			case cdefNavPageUp:
				{
					int stepsize(0);
					if ( settings._pageUpSteps == 0) stepsize = Global::song().LinesPerBeat();
					else if ( settings._pageUpSteps == 1)stepsize = Global::song().LinesPerBeat()*patSettings.timesig;
					else stepsize = settings._pageUpSteps;

					//if added by sampler to move backward 16 lines when playing
					if (Global::player()._playing && config._followSong)
					{
						if (Global::player()._playBlock )
						{
							if (Global::player()._lineCounter >= stepsize) Global::player()._lineCounter -= stepsize;
							else
							{
								Global::player()._lineCounter = 0;
								Global::player().ExecuteLine();
							}
						}
						else
						{
							if (Global::player()._lineCounter >= stepsize) Global::player()._lineCounter -= stepsize;
							else
							{
								if (Global::player()._playPosition > 0)
								{
									Global::player()._playPosition -= 1;
									Global::player()._lineCounter = Global::song().patternLines[Global::player()._playPosition] - stepsize;												
								}
								else
								{
									Global::player()._lineCounter = 0;
									Global::player().ExecuteLine();
								}
							}
						}
					}
					//end of if added by sampler
					else
					{
						pChildView->bScrollDetatch=false;
						pChildView->ChordModeOffs = 0;

						if ( bDoingSelection && !pChildView->blockSelected)
						{
							pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
						}
						pChildView->PrevLine(stepsize,false);
						if ( bDoingSelection )
						{
							pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
						}
						else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
					}
				}
				break;

			case cdefNavPageDn:
				{
					int stepsize(0);
					if ( settings._pageUpSteps == 0) stepsize = Global::song().LinesPerBeat();
					else if ( settings._pageUpSteps == 1)stepsize = Global::song().LinesPerBeat()*patSettings.timesig;
					else stepsize = settings._pageUpSteps;

					//if added by sampler
					if (Global::player()._playing && config._followSong)
					{
						Global::player()._lineCounter += stepsize;
					}
					//end of if added by sampler
					else
					{
						pChildView->bScrollDetatch=false;
						pChildView->ChordModeOffs = 0;
						if ( bDoingSelection && !pChildView->blockSelected)
						{
							pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
						}
						pChildView->AdvanceLine(stepsize,false);
						if ( bDoingSelection )
						{
							pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
						}
						else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();
					}
				}
				break;
			
			case cdefNavTop:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( bDoingSelection && !pChildView->blockSelected)
				{
					pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				if(settings.bFT2HomeBehaviour)
				{
					pChildView->editcur.line=0;
				}
				else
				{
					if (pChildView->editcur.col != 0) 
						pChildView->editcur.col = 0;
					else 
						if ( pChildView->editcur.track != 0 ) 
							pChildView->editcur.track = 0;
						else 
							pChildView->editcur.line = 0;
				}
				if ( bDoingSelection )
				{
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();

				pChildView->Repaint(draw_modes::cursor);
				break;
			
			case cdefNavBottom:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if ( bDoingSelection && !pChildView->blockSelected)
				{
					pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				if(settings.bFT2HomeBehaviour)
				{
					pChildView->editcur.line=Global::song().patternLines[Global::song().playOrder[pChildView->editPosition]]-1;
				}
				else
				{		
					if (pChildView->editcur.col != 8) 
						pChildView->editcur.col = 8;
					else if ( pChildView->editcur.track != Global::song().SONGTRACKS-1 ) 
						pChildView->editcur.track = Global::song().SONGTRACKS-1;
					else 
						pChildView->editcur.line = Global::song().patternLines[Global::song().playOrder[pChildView->editPosition]]-1;
				}
				if ( bDoingSelection )
				{
					pChildView->ChangeBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				}
				else if ( settings.bShiftArrowsDoSelect && settings._windowsBlocks) pChildView->BlockUnmark();


				pChildView->Repaint(draw_modes::cursor);
				break;
			
			case cdefRowInsert:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->InsertCurr();
				break;

			case cdefRowDelete:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if (pChildView->blockSelected && settings._windowsBlocks)
				{
					pChildView->DeleteBlock();
				}
				else
				{
					pChildView->DeleteCurr();
				}
				break;

			case cdefRowClear:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				if (pChildView->blockSelected && settings._windowsBlocks)
				{
					pChildView->DeleteBlock();
				}
				else
				{
					pChildView->ClearCurr();		
				}
				break;

			case cdefBlockStart:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				break;

			case cdefBlockEnd:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->EndBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
				break;

			case cdefSelectAll:
				{
					const int nl = Global::song().patternLines[Global::song().playOrder[pChildView->editPosition]];
					pChildView->StartBlock(0,0,0);
					pChildView->EndBlock(Global::song().SONGTRACKS-1,nl-1,8);
				}
				break;
				
			case cdefSelectCol:
				{
					const int nl = Global::song().patternLines[Global::song().playOrder[pChildView->editPosition]];
					pChildView->StartBlock(pChildView->editcur.track,0,0);
					pChildView->EndBlock(pChildView->editcur.track,nl-1,8);
				}
				break;

			case cdefSelectBar:
			//selects 1 bar, 2 bars, 4 bars... up to number of lines in pattern
				{
					const int nl = Global::song().patternLines[Global::song().playOrder[pChildView->editPosition]];			
								
					pChildView->bScrollDetatch=false;
					pChildView->ChordModeOffs = 0;
					
					if (pChildView->blockSelectBarState == 1) 
					{
						pChildView->StartBlock(pChildView->editcur.track,pChildView->editcur.line,pChildView->editcur.col);
					}

					int blockLength = (patSettings.timesig * pChildView->blockSelectBarState * Global::song().LinesPerBeat())-1;

					if ((pChildView->editcur.line + blockLength) >= nl-1)
					{
						pChildView->EndBlock(pChildView->editcur.track,nl-1,8);	
						pChildView->blockSelectBarState = 1;
					}
					else
					{
						pChildView->EndBlock(pChildView->editcur.track,pChildView->editcur.line + blockLength,8);
						pChildView->blockSelectBarState *= 2;
					}	
					
				}
				break;

			case cdefEditQuantizeDec:
				pMainFrame->EditQuantizeChange(-1);
				break;

			case cdefEditQuantizeInc:
				pMainFrame->EditQuantizeChange(1);
				break;

			case cdefTransposeChannelInc:
				pChildView->patTranspose(1);
				break;
			case cdefTransposeChannelDec:
				pChildView->patTranspose(-1);
				break;
			case cdefTransposeChannelInc12:
				pChildView->patTranspose(12);
				break;
			case cdefTransposeChannelDec12:
				pChildView->patTranspose(-12);
				break;

			case cdefTransposeBlockInc:
				pChildView->BlockTranspose(1);
				break;
			case cdefTransposeBlockDec:
				pChildView->BlockTranspose(-1);
				break;
			case cdefTransposeBlockInc12:
				pChildView->BlockTranspose(12);
				break;
			case cdefTransposeBlockDec12:
				pChildView->BlockTranspose(-12);
				break;


			case cdefBlockUnMark:
				pChildView->BlockUnmark();
				break;

			case cdefBlockDouble:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->DoubleLength();
				break;

			case cdefBlockHalve:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->HalveLength();
				break;

			case cdefBlockCut:
				pChildView->CopyBlock(true);
				break;

			case cdefBlockCopy:
				pChildView->CopyBlock(false);
				break;

			case cdefBlockPaste:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->PasteBlock(pChildView->editcur.track,pChildView->editcur.line,false);
				break;

			case cdefBlockMix:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->PasteBlock(pChildView->editcur.track,pChildView->editcur.line,true);
				break;

			case cdefBlockDelete:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->DeleteBlock();
				break;

			case cdefBlockInterpolate:
				pChildView->BlockParamInterpolate();
				break;

			case cdefBlockSetMachine:
				pChildView->BlockGenChange(Global::song().seqBus);
				break;

			case cdefBlockSetInstr:
				pChildView->BlockInsChange(Global::song().auxcolSelected);
				break;

			case cdefOctaveUp:
				pMainFrame->ShiftOctave(1);
				ANOTIFY(Actions::octaveup);
				break;

			case cdefOctaveDn:
				pMainFrame->ShiftOctave(-1);
				ANOTIFY(Actions::octavedown);
				break;

			case cdefPlaySong:
				PlaySong();
				break;

			case cdefPlayFromPos:
				PlayFromCur();
				break;

			case cdefPlayStart:
				pChildView->OnBarplayFromStart();
				break;

			case cdefPlayRowTrack:
				pChildView->PlayCurrentNote();
				pChildView->AdvanceLine(1,settings._wrapAround);
				break;

			case cdefPlayRowPattern:
				pChildView->PlayCurrentRow();
				pChildView->AdvanceLine(1,settings._wrapAround);
				break;

			case cdefPlayBlock:
				pChildView->OnButtonplayseqblock();
				break;

			case cdefEditToggle:
				pChildView->bEditMode = !pChildView->bEditMode;
				pChildView->ChordModeOffs = 0;
				
				if(settings.bCtrlPlay) Stop();
				
		//		pChildView->Repaint(draw_modes::patternHeader);
				break;

			case cdefPlayStop:
				Stop();
				break;
			
			case cdefSelectMachine:
				pChildView->SelectMachineUnderCursor();
				break;
			case cdefMachineInc:
				pMainFrame->OnBIncgen();
				break;

			case cdefMachineDec:
				pMainFrame->OnBDecgen();
				break;

			case cdefInstrInc:
				pMainFrame->OnBIncAux();
				break;

			case cdefInstrDec:
				pMainFrame->OnBDecAux();
				break;

			case cdefInfoPattern:
				if ( pChildView->viewMode == view_modes::pattern )
				{
					pChildView->ShowPatternDlg();
				}
				break;

			case cdefInfoMachine:
				{
					int inst=-1;
					Machine* pMac = Global::song().GetMachineOfBus(Global::song().seqBus, inst);
					if (pMac)
					{
						CPoint point;
						point.x = pMac->_x;
						point.y = pMac->_y;
						pMainFrame->ShowMachineGui(pMac->_macIndex, point);//, Global::song().seqBus);
					}
				}
				break;

			case cdefEditMachine:
				pChildView->OnMachineview();
				break;

			case cdefEditPattern:
				pChildView->OnPatternView();
				pChildView->ChordModeOffs = 0;
				break;

			case cdefEditInstr:
				pMainFrame->ShowInstrumentEditor();
				break;

			case cdefAddMachine:
				pChildView->OnNewmachine();
				break;

			case cdefMaxPattern:
				pChildView->OnFullScreen();
				break;

			case cdefPatternInc:
				pChildView->ChordModeOffs = 0;
				pChildView->IncCurPattern();
				ANOTIFY(Actions::seqmodified);
				break;

			case cdefPatternDec:
				pChildView->ChordModeOffs = 0;
				pChildView->DecCurPattern();
				ANOTIFY(Actions::seqmodified);
				break;

			case cdefSongPosInc:
				pChildView->ChordModeOffs = 0;
				pChildView->IncPosition(brepeat?true:false);
				pMainFrame->StatusBarIdle(); 
				break;

			case cdefSongPosDec:
				pChildView->ChordModeOffs = 0;
				pChildView->DecPosition();
				pMainFrame->StatusBarIdle();
				break;

			case cdefUndo:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->OnEditUndo();
				break;

			case cdefRedo:
				pChildView->bScrollDetatch=false;
				pChildView->ChordModeOffs = 0;
				pChildView->OnEditRedo();
				break;
			}
		}

		void InputHandler::Stop()
		{
			pChildView->OnBarstop();
		}

		void InputHandler::PlaySong() 
		{
			pChildView->OnBarplay();
		}

		void InputHandler::PlayFromCur() 
		{
			Global::player().Start(pChildView->editPosition,pChildView->editcur.line);
			pMainFrame->StatusBarIdle();
		}


		int InputHandler::GetTrackToPlay(int note, int macNo, int instNo, bool noteoff)
		{
			scoped_lock lock(mutex_);
			const Song &song = Global::song();
			const PsycleConfig& config = PsycleGlobal::conf();

			int targettrack=-1;
			if (noteoff)
			{
				int i;
				for (i = 0; i < song.SONGTRACKS; i++)
				{
					if (notetrack[i] == note && instrtrack[i] == instNo && mactrack[i] == macNo) {
						break;
					}
				}
				targettrack = (i < song.SONGTRACKS) ? i : -1;
			}
			else if(config.inputHandler().bMultiKey)
			{
				int i = outtrack+1;
				while ( notetrack[i] != notecommands::release && i < song.SONGTRACKS) i++;
				if (i >= song.SONGTRACKS)
				{
					i = 0;
					while ( notetrack[i] != notecommands::release && i <= outtrack) i++;
					if (i >= song.SONGTRACKS) i = -1;
				}
				outtrack = i;
				targettrack = outtrack;
			}
			else
			{
				targettrack = pChildView->editcur.track;
			}
			return targettrack;
		}

		int InputHandler::GetTrackAndLineToEdit(int note, int macNo, int instNo, bool noteoff, bool bchord, int& outline)
		{
			scoped_lock lock(mutex_);
			const Song &song = Global::song();
			const PsycleConfig& config = PsycleGlobal::conf();

			int targettrack=-1;
			int targetline;
			// If there is at least one track selected for recording, select the proper track
			if (Global::player()._playing && config._followSong)
			{
				if (song._trackArmedCount)
				{
					if (noteoff)
					{
						int i;
						for (i = 0; i < song.SONGTRACKS; i++)
						{
							if (notetrack[i] == note && instrtrack[i] == instNo && mactrack[i] == macNo) {
								break;
							}
						}
						if (i < song.SONGTRACKS) {
							targettrack = i;
						}
					}
					else {
						int next=-1;
						int i;
						for (i = pChildView->editcur.track+1; i < song.SONGTRACKS; i++)
						{
							if (song._trackArmed[i]) {
								if (next == -1) next = i;
								if (notetrack[i] == notecommands::release) {
									break;
								}
							}
						}
						if (i >= song.SONGTRACKS)
						{
							for (i = 0; i <= pChildView->editcur.track; i++)
							{
								if (song._trackArmed[i]) {
									if (next == -1) next = i;
									if (notetrack[i] == notecommands::release) {
										break;
									}
								}
							}
							if (i >= pChildView->editcur.track) i = next;
						}
						targettrack = i;
					}
				}
				targetline = Global::player()._lineCounter;
			}
			else if (bchord && note <= notecommands::release)
			{
				if (pChildView->ChordModeOffs == 0)
				{
					pChildView->ChordModeLine = pChildView->editcur.line;
					pChildView->ChordModeTrack = pChildView->editcur.track;
				}
				targettrack = (pChildView->ChordModeTrack+pChildView->ChordModeOffs)%song.SONGTRACKS;
				targetline = pChildView->ChordModeLine;
				pChildView->ChordModeOffs++;
			}
			else
			{
				if (pChildView->ChordModeOffs) // this should never happen because the shift check should catch it... but..
				{					// ok pooplog, now it REALLY shouldn't happen (now that the shift check works)
					pChildView->editcur.line = pChildView->ChordModeLine;
					pChildView->editcur.track = pChildView->ChordModeTrack;
					pChildView->ChordModeOffs = 0;
					pChildView->AdvanceLine(pChildView->patStep,config.inputHandler()._wrapAround,false);
				}
				targettrack = pChildView->editcur.track;
				targetline = pChildView->editcur.line;
			}
			if (targettrack != -1) {
				pChildView->editcur.track = targettrack;
				pChildView->editcur.line = targetline;
			}
			outline=targetline;
			return targettrack;
		}

		void InputHandler::PlayNote(PatternEntry* pEntry, int trackIn)
		{
			int track = (trackIn == -1) ? pChildView->editcur.track : trackIn;
			int inst=-1;
			Machine* pMachine = Global::song().GetMachineOfBus(pEntry->_mach,inst);
			if (pMachine && !pMachine->_mute)	
			{
				//if not a virtual instrument, or a tweak/mcm command, send it directly
				if (inst == -1 || pEntry->_note > notecommands::release) {
					pMachine->Tick(track, pEntry);
					if (pEntry->_note <= notecommands::release ) {
						notetrack[track]=pEntry->_note;
						instrtrack[track]=pEntry->_inst;
						mactrack[track]=pEntry->_mach;
					}
				}
				else {
					//If aux column is used, send it.
					if (pEntry->_inst != 255 ) {
						PatternEntry entry;
						entry._note = notecommands::midicc;
						entry._inst = track;
						entry._cmd = (pMachine->_type == MACH_XMSAMPLER) ? XMSampler::CMD::SENDTOVOLUME : SAMPLER_CMD_VOLUME;
						entry._parameter = pEntry->_inst;
						pMachine->Tick(track, &entry);
					}
					//finally send the note with the correct instrument
					PatternEntry entry(*pEntry);
					entry._inst = inst;
					pMachine->Tick(track, &entry);
					if (entry._note <= notecommands::release ) {
						notetrack[track]=entry._note;
						instrtrack[track]=entry._inst;
						mactrack[track]=entry._mach;
					}
				}
			}
		}

		void InputHandler::StopNote(int note, int instr/*=255*/, bool bTranspose/*=true*/,Machine* pMachine/*=NULL*/)
		{
			assert(note>=0 && note < notecommands::invalid);
			PatternEntry entry = BuildNote(note,instr,0,bTranspose,pMachine);
			// octave offset
			if ( note <= notecommands::b9 && bTranspose) {
				note += Global::song().currentOctave*12;
				if (note > notecommands::b9) { note = notecommands::b9; }
			}

			int track = GetTrackToPlay(note,entry._mach,entry._inst,true);
			PlayNote(&entry,track);
		}

		// velocity range 0 -> 127
		void InputHandler::PlayNote(int note,int instr/*=255*/, int velocity/*=127*/,bool bTranspose/*=true*/,Machine* pMachine/*=NULL*/)
		{
			assert(note>=0 && note < notecommands::invalid);
			int noteoffsetted;

			// stop any (stuck) notes with the same value
			int mactmp = (pMachine == NULL) ? Global::song().seqBus : pMachine->_macIndex;
			// octave offset
			if ( note <= notecommands::b9 && bTranspose) {
				noteoffsetted = note + Global::song().currentOctave*12;
				if (noteoffsetted > notecommands::b9) { noteoffsetted = notecommands::b9; }
			}
			else {
				noteoffsetted = note;
			}
			int tracktmp = GetTrackToPlay(noteoffsetted,mactmp,instr,true);
			if (tracktmp != -1) {
				PatternEntry entry = BuildNote(noteoffsetted,instr,0,false,pMachine);
				PlayNote(&entry,tracktmp);
			}

			PatternEntry entry = BuildNote(noteoffsetted,instr,velocity,false,pMachine);
			int track = GetTrackToPlay(entry._note,entry._mach,entry._inst,false);
			if (track == -1) track = pChildView->editcur.track;

			// this checks to see if a note is playing on that track already
			if (notetrack[track] < notecommands::release)
			{
				if (mactmp != mactrack[track] ) {
					int dummy=-1;
					Machine* mac2 = Global::song().GetMachineOfBus(mactrack[track],dummy);
					StopNote(notetrack[track], instrtrack[track], false, mac2);
				}
				else {
					StopNote(notetrack[track], instrtrack[track], false, pMachine);
				}
			}
			PlayNote(&entry,track);
		}


		PatternEntry InputHandler::BuildNote(int note, int instr/*=255*/, int velocity/*=127*/, bool bTranspose/*=true*/, Machine* mac/*=NULL*/, bool forcevolume/*=false*/)
		{
			const PsycleConfig::Midi &midiconf = PsycleGlobal::conf().midi();
			const Song &song = Global::song();
			PatternEntry entry;

			if (note < 0 || note >= notecommands::invalid ) return entry;

			int macIdx;
			if (mac) macIdx = mac->_macIndex;
			else macIdx = song.seqBus;

			int instNo;
			if (instr < 255) instNo = instr;
			else instNo = song.auxcolSelected;
			
			// octave offset
			if ( note <= notecommands::b9 && bTranspose) {
				note += song.currentOctave*12;
				if (note > notecommands::b9) { note = notecommands::b9; }
			}

			entry._note = (velocity==0) ? notecommands::release : note;
			entry._mach = macIdx;

			if (note > notecommands::release) {
				 // this should only happen when entering manually a twk/tws or mcm command in the pattern.
				entry._inst = instNo;
			}
			else
			{
				int alternateins=-1;
				bool sampulse=false;
				Machine *tmac = (mac != NULL) ? mac : song.GetMachineOfBus(macIdx, alternateins);
				if (tmac)
				{
					//Add if the machine needs aux column, but do not do it on virtual instruments
					if (tmac->NeedsAuxColumn() && alternateins == -1) {
						entry._inst = instNo;
					}
					// if the current machine is a sampler, check if current sample is locked to a machine.
					// if so, switch entry._mach to that machine number
					if (tmac->_type == MACH_SAMPLER && song._pInstrument[instNo]->_LOCKINST == true)
					{
						entry._mach = song._pInstrument[instNo]->sampler_to_use;
					}
					sampulse= (tmac->_type == MACH_XMSAMPLER);
				}

				if ( velocity < 127 && entry._note < notecommands::release && (forcevolume || PsycleGlobal::conf().inputHandler()._RecordTweaks))
				{
					if (midiconf.raw())
					{
						if (alternateins != -1) {
							if (sampulse) {
								entry._inst = velocity / 2;
							}
							else {
								entry._inst = velocity * 2;
							}
						}
						else {
							entry._cmd = 0x0C;
							entry._parameter = velocity * 2;
						}
					}
					else if (midiconf.velocity().record())
					{
						int par = midiconf.velocity().from() + (midiconf.velocity().to() - midiconf.velocity().from()) * velocity / 127;
						if (par > 255) { par = 255; }
						else if (par < 0) { par = 0; }

						entry._cmd = midiconf.velocity().command();
						entry._parameter = par;
					}
				}
			}
			return entry;
		}

		PatternEntry InputHandler::BuildTweak(int machine, int tweakidx, int value, bool slide, const ParamTranslator& translator)
		{
			if (value < 0) value = 0;
			if (value > 0xffff) value = 0xffff;
			PatternEntry newentry;
			newentry._cmd = (value>>8)&0xFF;
			newentry._parameter = value&0xFF;
			try {
				newentry._inst = translator.virtual_index(tweakidx);
			} catch(std::exception&) {				
				newentry._inst = tweakidx;						
			}
			newentry._mach = machine;
			newentry._note = (slide)?notecommands::tweakslide : notecommands::tweak;
			return newentry;
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////
		// MidiPatternNote
		//
		// DESCRIPTION	  : Called by the MIDI input interface to insert pattern notes
		// PARAMETERS     : int outnote - note to insert/stop . int velocity - velocity of the note, or zero if noteoff
		// RETURNS		  : <void>
		// 
		void InputHandler::MidiPatternNote(int outnote , int macidx, int channel, int velocity)
		{
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();
			PsycleConfig& config = PsycleGlobal::conf();
			Machine* mac = NULL;
			if(macidx >=0 && macidx <MAX_BUSES)
			{
				mac = Global::song()._pMachine[macidx];
			}
			int track=0;
			PatternEntry entry;
			if(pChildView->viewMode == view_modes::pattern && pChildView->bEditMode)
			{ 
				bool recording = Global::player()._playing && config._followSong;
				// add note
				if(velocity > 0 || (recording && settings._RecordNoteoff))
				{
					entry = BuildNote(outnote, channel,velocity,false, mac);
					int line=0;
					track = GetTrackAndLineToEdit(outnote,
						entry._mach, entry._inst, velocity==0, false, line);
					bool write = (!recording || track != -1 || PsycleGlobal::conf().inputHandler()._RecordUnarmed);
					if (track == -1) track = pChildView->editcur.track;
					if (write) pChildView->EnterData(&entry, track, line, velocity > 0, !recording);
				}
				else {
					entry = BuildNote(outnote, channel,velocity,false, mac, true);
					track= GetTrackToPlay(outnote,entry._mach, entry._inst, velocity==0);
				}
			}
			else {
				entry = BuildNote(outnote, channel,velocity,false, mac, true);
				track= GetTrackToPlay(outnote,entry._mach, entry._inst, velocity==0);
			}
			PlayNote(&entry,track);
		}

		void InputHandler::MidiPatternTweak(int busMachine, int tweakidx, int value, bool slide) {
			PatternEntry entry = BuildTweak(busMachine, tweakidx, value, slide, *(param_translator_dummy.get()));
			bool recording = Global::player()._playing && PsycleGlobal::conf()._followSong;
			int line=0;
			int track = GetTrackAndLineToEdit(entry._note, entry._mach, entry._inst, false, false, line);
			bool write = (!recording || track != -1 || PsycleGlobal::conf().inputHandler()._RecordUnarmed);
			if (track == -1) track = pChildView->editcur.track;
			if (write) pChildView->EnterData(&entry,  track, line, true, false);
			PlayNote(&entry, track);
		}

		void InputHandler::MidiPatternCommand(int busMachine, int command, int value){
			PatternEntry entry(notecommands::empty,255,busMachine,command&0xFF,value&0xFF);
			bool recording = Global::player()._playing && PsycleGlobal::conf()._followSong;
			int line=0;
			int track = GetTrackAndLineToEdit(entry._note, entry._mach, entry._inst, false, false, line);
			bool write = (!recording || track != -1 || PsycleGlobal::conf().inputHandler()._RecordUnarmed);
			if (track == -1) track = pChildView->editcur.track;
			if (write) pChildView->EnterData(&entry,  track, line, false, false);
			PlayNote(&entry, track);
		}

		void InputHandler::MidiPatternMidiCommand(int busMachine, int controlCode, int value){
			PatternEntry entry(notecommands::midicc,controlCode,busMachine,(value&0xFF00)>>8,value&0xFF);
			bool recording = Global::player()._playing && PsycleGlobal::conf()._followSong;
			int line=0;
			int track = GetTrackAndLineToEdit(entry._note, entry._mach, entry._inst, false, false, line);
			bool write = (!recording || track != -1 || PsycleGlobal::conf().inputHandler()._RecordUnarmed);
			if (track == -1) track = pChildView->editcur.track;
			if (write) pChildView->EnterData(&entry,  track, line, true, false);
			PlayNote(&entry, track);
		}

		void InputHandler::Automate(int macIdx, int param, int value, bool undo, const ParamTranslator& param_translator)
		{
			PsycleConfig::InputHandler& settings = PsycleGlobal::conf().inputHandler();

			if(undo || true) {
				AddMacViewUndo();
			}

			if(settings._RecordTweaks)
			{
				PatternEntry entry = BuildTweak(macIdx, param, value, settings._RecordMouseTweaksSmooth, param_translator);
				bool recording = Global::player()._playing && PsycleGlobal::conf()._followSong;
				int line=0;
				int track = GetTrackAndLineToEdit(entry._note, entry._mach, entry._inst, false, false, line);
				bool write = (!recording || track != -1 || PsycleGlobal::conf().inputHandler()._RecordUnarmed);
				if (track == -1) track = pChildView->editcur.track;
				if (write) pChildView->EnterData(&entry,  track, line, true, false);
				std::vector<boost::weak_ptr<Tweak> >::iterator it = tweak_listeners_.begin();
				for (; it != tweak_listeners_.end(); ++it) {
				  if (!(*it).expired()) {
					(*it).lock()->OnAutomate(macIdx, param, value, undo, param_translator, track, line);
				  } 
				}
			}			
		}
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		// undo/redo code
		////////////////////////////////////////////////////////////////////////////////////////////////////////

		void InputHandler::AddMacViewUndo()
		{
			// i have not written the undo code yet for machine and instruments
			// however, for now it at least tracks changes for save/new/open/close warnings
			UndoMacCounter++;
			pChildView->SetTitleBarText();
		}

		void InputHandler::AddUndo(int pattern, int x, int y, int tracks, int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo, int counter)
		{
			Song& song = Global::song();
			SPatternUndo pNew;

			// fill data
			pNew.dataSize = tracks*lines*EVENT_SIZE;
			unsigned char* pData = new unsigned char[pNew.dataSize];
			pNew.pData = pData;
			pNew.pattern = pattern;
			pNew.x = x;
			pNew.y = y;
			if (tracks+x > song.SONGTRACKS)
			{
				tracks = song.SONGTRACKS-x;
			}
			pNew.tracks = tracks;
						
			const int nl = song.patternLines[pattern];
			
			if (lines+y > nl)
			{
				lines = nl-y;
			}
			pNew.lines = lines;
			pNew.type = UNDO_PATTERN;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;

			for (int t=x;t<x+tracks;t++)
			{
				for (int l=y;l<y+lines;l++)
				{
					unsigned char *offset_source=song._ptrackline(pattern,t,l);
					
					memcpy(pData,offset_source,EVENT_SIZE);
					pData+=EVENT_SIZE;
				}
			}
			if (bWipeRedo)
			{
				KillRedo();
				UndoCounter++;
				pNew.counter = UndoCounter;
			}
			else
			{
				pNew.counter = counter;
			}
			pUndoList.push_back(pNew);
			if(pUndoList.size() > 100) {
				pUndoList.pop_front();
			}
			pChildView->SetTitleBarText();
		}

		void InputHandler::AddRedo(int pattern, int x, int y, int tracks, int lines, int edittrack, int editline, int editcol, int seqpos, int counter)
		{
			Song& song = Global::song();
			SPatternUndo pNew;
			pNew.dataSize = tracks*lines*EVENT_SIZE;
			unsigned char* pData = new unsigned char[pNew.dataSize];
			pNew.pData = pData;
			pNew.pattern = pattern;
			pNew.x = x;
			pNew.y = y;
			if (tracks+x > song.SONGTRACKS)
			{
				tracks = song.SONGTRACKS-x;
			}
			pNew.tracks = tracks;
			const int nl = song.patternLines[pattern];
			if (lines+y > nl)
			{
				lines = nl-y;
			}
			pNew.tracks = tracks;
			pNew.lines = lines;
			pNew.type = UNDO_PATTERN;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;
			pNew.counter = counter;

			for (int t=x;t<x+tracks;t++)
			{
				for (int l=y;l<y+lines;l++)
				{
					unsigned char *offset_source=song._ptrackline(pattern,t,l);
					
					memcpy(pData,offset_source,EVENT_SIZE);
					pData+=EVENT_SIZE;
				}
			}
			pRedoList.push_back(pNew);
			if(pRedoList.size() > 100) {
				pRedoList.pop_front();
			}
		}

		void InputHandler::AddUndoLength(int pattern, int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo, int counter)
		{
			SPatternUndo pNew;
			pNew.pattern = pattern;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = lines;
			pNew.type = UNDO_LENGTH;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;

			if (bWipeRedo)
			{
				KillRedo();
				UndoCounter++;
				pNew.counter = UndoCounter;
			}
			else
			{
				pNew.counter = counter;
			}
			pUndoList.push_back(pNew);
			if(pUndoList.size() > 100) {
				pUndoList.pop_front();
			}

			pChildView->SetTitleBarText();
		}

		void InputHandler::AddRedoLength(int pattern, int lines, int edittrack, int editline, int editcol, int seqpos, int counter)
		{
			SPatternUndo pNew;
			pNew.pattern = pattern;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = lines;
			pNew.type = UNDO_LENGTH;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;
			pNew.counter = counter;
			pRedoList.push_back(pNew);
			if(pRedoList.size() > 100) {
				pRedoList.pop_front();
			}
		}

		void InputHandler::AddUndoSequence(int lines, int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo, int counter)
		{
			SPatternUndo pNew;
			pNew.dataSize = MAX_SONG_POSITIONS;
			pNew.pData = new unsigned char[pNew.dataSize];
			memcpy(pNew.pData, Global::song().playOrder, MAX_SONG_POSITIONS*sizeof(char));
			pNew.pattern = 0;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = lines;
			pNew.type = UNDO_SEQUENCE;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;

			if (bWipeRedo)
			{
				KillRedo();
				UndoCounter++;
				pNew.counter = UndoCounter;
			}
			else
			{
				pNew.counter = counter;
			}
			pUndoList.push_back(pNew);
			if(pUndoList.size() > 100) {
				pUndoList.pop_front();
			}

			pChildView->SetTitleBarText();
		}

		void InputHandler::AddUndoSong(int edittrack, int editline, int editcol, int seqpos, BOOL bWipeRedo, int counter)
		{
			Song& song = Global::song();
			SPatternUndo pNew;
			// fill data
			// count used patterns
			unsigned short count = 0;
			for (unsigned short i = 0; i < MAX_PATTERNS; i++)
			{
				if (song.ppPatternData[i])
				{
					count++;
				}
			}
			pNew.dataSize = MAX_SONG_POSITIONS*sizeof(char)+sizeof(count)+count*sizeof(short)*MULTIPLY2;
			pNew.pData = new unsigned char[pNew.dataSize];
			unsigned char *pWrite=pNew.pData;
			memcpy(pWrite, song.playOrder, MAX_SONG_POSITIONS*sizeof(char));
			pWrite+=MAX_SONG_POSITIONS*sizeof(char);

			memcpy(pWrite, &count, sizeof(count));
			pWrite+=sizeof(count);

			for (unsigned short i = 0; i < MAX_PATTERNS; i++)
			{
				if (song.ppPatternData[i])
				{
					memcpy(pWrite, &i, sizeof(i));
					pWrite+=sizeof(i);
					memcpy(pWrite, song.ppPatternData[i], MULTIPLY2);
					pWrite+=MULTIPLY2;
				}
			}

			pNew.pattern = 0;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = song.playLength;
			pNew.type = UNDO_SONG;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;

			if (bWipeRedo)
			{
				KillRedo();
				UndoCounter++;
				pNew.counter = UndoCounter;
			}
			else
			{
				pNew.counter = counter;
			}
			pUndoList.push_back(pNew);
			if(pUndoList.size() > 100) {
				pUndoList.pop_front();
			}
			pChildView->SetTitleBarText();
		}

		void InputHandler::AddRedoSong(int edittrack, int editline, int editcol, int seqpos, int counter)
		{
			Song& song = Global::song();
			SPatternUndo pNew;
			// fill data
			// count used patterns
			unsigned char count = 0;
			for (unsigned short i = 0; i < MAX_PATTERNS; i++)
			{
				if (song.ppPatternData[i])
				{
					count++;
				}
			}
			pNew.dataSize = MAX_SONG_POSITIONS*sizeof(char)+sizeof(count)+count*sizeof(short)*MULTIPLY2;
			pNew.pData = new unsigned char[pNew.dataSize];
			unsigned char *pWrite=pNew.pData;
			memcpy(pWrite, song.playOrder, MAX_SONG_POSITIONS*sizeof(char));
			pWrite+=MAX_SONG_POSITIONS*sizeof(char);

			memcpy(pWrite, &count, sizeof(count));
			pWrite+=sizeof(count);

			for (unsigned short i = 0; i < MAX_PATTERNS; i++)
			{
				if (song.ppPatternData[i])
				{
					memcpy(pWrite, &i, sizeof(i));
					pWrite+=sizeof(i);
					memcpy(pWrite, song.ppPatternData[i], MULTIPLY2);
					pWrite+=MULTIPLY2;
				}
			}

			pNew.pattern = 0;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = song.playLength;
			pNew.type = UNDO_SONG;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;
			pNew.counter = counter;
			pRedoList.push_back(pNew);
			if(pRedoList.size() > 100) {
				pRedoList.pop_front();
			}
		}

		void InputHandler::AddRedoSequence(int lines, int edittrack, int editline, int editcol, int seqpos, int counter)
		{
			Song& song = Global::song();
			SPatternUndo pNew;
			// fill data
			pNew.dataSize = MAX_SONG_POSITIONS;
			pNew.pData = new unsigned char[pNew.dataSize];
			memcpy(pNew.pData, song.playOrder, MAX_SONG_POSITIONS*sizeof(char));
			pNew.pattern = 0;
			pNew.x = 0;
			pNew.y = 0;
			pNew.tracks = 0;
			pNew.lines = lines;
			pNew.type = UNDO_SEQUENCE;
			pNew.edittrack = edittrack;
			pNew.editline = editline;
			pNew.editcol = editcol;
			pNew.seqpos = seqpos;
			pNew.counter = counter;
			pRedoList.push_back(pNew);
			if(pRedoList.size() > 100) {
				pRedoList.pop_front();
			}
		}

		void InputHandler::KillRedo()
		{
			pRedoList.clear();
		}

		void InputHandler::KillUndo()
		{
			pUndoList.clear();
			UndoCounter = 0;
			UndoSaved = 0;

			UndoMacCounter=0;
			UndoMacSaved=0;
		}

		bool InputHandler::IsModified()
		{
			if(pUndoList.empty() && UndoSaved != 0)
			{
				return true;
			}
			if(!pUndoList.empty() && pUndoList.back().counter != UndoSaved)
			{
				return true;
			}
			if (UndoMacSaved != UndoMacCounter)
			{
				return true;
			}
			return false;
		}
		void InputHandler::SafePoint()
		{
			if(!pUndoList.empty())
			{
				UndoSaved = pUndoList.back().counter;
			}
			else
			{
				UndoSaved = 0;
			}
			UndoMacSaved = UndoMacCounter;
			pChildView->SetTitleBarText();
		}
		bool InputHandler::HasRedo(int viewMode)
		{
			if(pRedoList.empty()) {
				return false;
			}			
			else {
				SPatternUndo& redo = pRedoList.back();
				switch (redo.type)
				{
				case UNDO_SEQUENCE:
					return true;
					break;
				default:
					if(viewMode == view_modes::pattern)// && bEditMode)
					{
						return true;
					}
					else
					{
						return false;
					}
					break;
				}
			}
		}
		bool InputHandler::HasUndo(int viewMode)
		{
			if(pUndoList.empty()) {
				return false;
			}			
			else {
				SPatternUndo& undo = pUndoList.back();
				switch (undo.type)
				{
				case UNDO_SEQUENCE:
					return true;
					break;
				default:
					if(viewMode == view_modes::pattern)// && bEditMode)
					{
						return true;
					}
					else
					{
						return false;
					}
					break;
				}
			}
		}

		void ActionHandler::Notify(ActionType action) {
			ActionListenerList::iterator it = listeners_.begin();
			for (; it!=listeners_.end(); ++it) {
				try {
					(*it)->OnNotify(*this, action);
				} catch (std::exception& e) {
					ui::alert(e.what());
				}
			}
		}

		std::string ActionHandler::ActionAsString(ActionType action) const {
			std::string result;
			switch (action) {		
				case Actions::tpb: result = "ticksperbeatchanged"; break;
				case Actions::bpm: result = "bpm"; break;
				case Actions::trknum: result = "trknum"; break;
				case Actions::play: result = "play"; break;
				case Actions::playstart: result = "playstart"; break;
				case Actions::playseq: result = "playseq"; break;
				case Actions::stop: result = "stop"; break;
				case Actions::rec: result = "rec"; break;
				case Actions::seqsel: result = "seqsel"; break;
				case Actions::seqmodified: result = "seqmodified"; break;
				case Actions::seqfollowsong: result = "seqfollowsong"; break;
				case Actions::patkeydown: result = "patkeydown"; break;
				case Actions::patkeyup: result = "patkeyup"; break;
				case Actions::songload: result = "songload"; break;
				case Actions::songloaded: result = "songloaded"; break;
				case Actions::songnew: result = "songnew"; break;
				case Actions::tracknumchanged: result = "tracknumchanged"; break;
				case Actions::octaveup: result = "octaveup"; break;
				case Actions::octavedown: result = "octavedown"; break;
				case Actions::undopattern: result = "undopattern"; break;
				case Actions::patternlength: result = "patternlength"; break;
				default: result = "unknown"; break;
			}
			return result;
		}

	}
}
