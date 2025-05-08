///\file
///\brief implementation file for psycle::host::CChildView.
#include <psycle/host/detail/project.private.hpp>
#include "ChildView.hpp"

#include "Configuration.hpp"
#include "MainFrm.hpp"
#include "InputHandler.hpp"
#include "MidiInput.hpp"

#include "ConfigDlg.hpp"
#include "GreetDialog.hpp"
#include "SaveWavDlg.hpp"
#include "SongpDlg.hpp"
#include "MasterDlg.hpp"
#include "XMSamplerUI.hpp"
#include "WireDlg.hpp"
#include "MacProp.hpp"
#include "PresetsDlg.hpp" // For bank Manager popup option
#include "NewMachine.hpp"
#include "TransformPatternDlg.hpp"
#include "PatDlg.hpp"
#include "SwingFillDlg.hpp"
#include "InterpolateCurveDlg.hpp"
#include "ProgressDialog.hpp"
#include "Canvas.hpp"
#include "MfcUi.hpp"

#include "ITModule2.h"
#include "XMSongLoader.hpp"
#include "XMSongExport.hpp"
#include "Player.hpp"
#include "VstHost24.hpp" //included because of the usage of a call in the Timer function. It should be standarized to the Machine class.
#include "LuaPlugin.hpp"
#include "CanvasItems.hpp"
#include "ExtensionBar.hpp"

#include <cmath> // SwingFill


const int VIEWINSERTSTARTPOS = 8;

namespace psycle { namespace host {

		extern CPsycleApp theApp;

		int const ID_TIMER_VIEW_REFRESH =39;
		int const ID_TIMER_AUTOSAVE = 159;
		int const windows_menu_pos = 6;

		CMainFrame		*pParentMain;    

		TCHAR* CChildView::hex_tab[16] = {
			_T("0"), _T("1"), _T("2"), _T("3"), _T("4"), _T("5"), _T("6"), _T("7"),
			_T("8"), _T("9"), _T("A"), _T("B"), _T("C"), _T("D"), _T("E"), _T("F")
		};

		MenuHandle::MenuHandle()
			: menu_container_(new ui::MenuContainer())
			, menu_pos_(VIEWINSERTSTARTPOS)	  
		{
		}

		MenuHandle::~MenuHandle()
		{
		  ::DeleteObject(windows_menu_);
		}

		void MenuHandle::set_menu(const ui::Node::WeakPtr& menu_root_node)
		{
			using namespace ui;
			menu_container_->clear();
			extension_menu_.reset();
			if (!menu_root_node.expired()) {
				extension_menu_.reset(new Node());        
			    extension_menu_->AddNode(menu_root_node.lock());              
				menu_container_->set_root_node(extension_menu_);
// 				mfc::MenuContainerImp* menucontainer_imp = dynamic_cast<mfc::MenuContainerImp*>(menu_container_->imp());
// 				assert(menucontainer_imp);
// 				menucontainer_imp->set_menu_window(::AfxGetMainWnd(), extension_menu_);
			}
		}

		void MenuHandle::add_view_menu_item(Link& link) {     
			CMenu* view_menu = ::AfxGetMainWnd()->GetMenu()->GetSubMenu(3);
			if (view_menu) {
				int win32_menu_id = ID_DYNAMIC_MENUS_START + ui::MenuContainer::id_counter++;
// 				view_menu->InsertMenu(
// 					menu_pos_++, MF_STRING | MF_BYPOSITION, win32_menu_id,
// 					ui::mfc::Charset::utf8_to_win(link.label()).c_str());  
				menuItemIdMap_[win32_menu_id] = link;
			}
		}

		void MenuHandle::add_help_menu_item(Link& link) {      
			CMenu* main_menu = ::AfxGetMainWnd()->GetMenu();
			CMenu* help_menu = FindSubMenu(main_menu, "Help");  
			if (help_menu) {    
				int win32_menu_id = ID_DYNAMIC_MENUS_START + ui::MenuContainer::id_counter++;
// 				help_menu->AppendMenu(MF_STRING, win32_menu_id,
// 					ui::mfc::Charset::utf8_to_win(link.label()).c_str());  
				menuItemIdMap_[win32_menu_id] = link;
			}
		}

		void MenuHandle::replace_help_menu_item(Link& link, int pos) { 
			CMenu* main_menu = ::AfxGetMainWnd()->GetMenu();
			CMenu* help_menu = FindSubMenu(main_menu, "Help");
			if (help_menu) {    
				int win32_menu_id = ID_DYNAMIC_MENUS_START + ui::MenuContainer::id_counter++;    
				help_menu->RemoveMenu(pos, MF_BYPOSITION);
// 				help_menu->InsertMenu(pos, MF_BYPOSITION, win32_menu_id, ui::mfc::Charset::utf8_to_win(link.label()).c_str());    
				menuItemIdMap_[win32_menu_id] = link;
			}
		}

		void MenuHandle::add_windows_menu_item(Link& link) {
			const int id = ID_DYNAMIC_MENUS_START + ui::MenuContainer::id_counter++;
			std::string label;
// 			::AppendMenu(windows_menu_,
// 				MF_STRING | MF_BYPOSITION,
// 				id,
// 				ui::mfc::Charset::utf8_to_win(menu_label(link)).c_str());
			menuItemIdMap_[id] = link;
		}

		void MenuHandle::remove_windows_menu_item(LuaPlugin* plugin) {
			MenuMap::iterator it = menuItemIdMap_.begin();
			for ( ; it != menuItemIdMap_.end(); ++it) {
				if (!it->second.plugin.expired()) {
					LuaPlugin* plug = it->second.plugin.lock().get();
					if (plug == plugin) {
						::RemoveMenu(windows_menu_, it->first, MF_BYCOMMAND);
					}
				}
			}
		}

	void MenuHandle::change_windows_menu_item_text(LuaPlugin* plugin) {
		MenuMap::iterator it = menuItemIdMap_.begin();
		for ( ; it != menuItemIdMap_.end(); ++it) {
			if (!it->second.plugin.expired()) {
			LuaPlugin* plug = it->second.plugin.lock().get();
			if (plug == plugin) {    
// 				std::string label = ui::mfc::Charset::utf8_to_win(
// 					menu_label(it->second)).c_str();    
// 				::ModifyMenu(windows_menu_,
// 							it->first,
// 							MF_BYCOMMAND | MF_STRING,
// 							it->first,
// 							label.c_str()
// 							);
				}
			}
		}
	  }

		void MenuHandle::restore_view_menu()
		{
			CMenu* view_menu = ::AfxGetMainWnd()->GetMenu()->GetSubMenu(3);	
			if (view_menu) {
				int count = menu_pos_ - VIEWINSERTSTARTPOS;
				while (count--) {
					RemoveMenu(view_menu->GetSafeHmenu(), VIEWINSERTSTARTPOS, MF_BYPOSITION);    
				}
				menu_pos_ = VIEWINSERTSTARTPOS;
			}
		}

		void MenuHandle::InitWindowsMenu() {     
			CMenu* main_menu = ::AfxGetMainWnd()->GetMenu();  
			windows_menu_ = ::CreateMenu();    
			InsertMenu(main_menu->GetSafeHmenu(), windows_menu_pos, MF_POPUP | MF_ENABLED, (UINT_PTR)windows_menu_, _T("Windows"));         
		}

		void MenuHandle::clear()
		{
			menu_container_->clear();
		}

		CMenu* MenuHandle::FindSubMenu(CMenu* parent, const std::string& text) {   
			CMenu* main_menu = ::AfxGetMainWnd()->GetMenu();
			CMenu* result(0);
			for (int i=0; i < main_menu->GetMenuItemCount(); ++i) {
				CString str;
				main_menu->GetMenuString(i, str, MF_BYPOSITION);
				std::string tmp(str.GetString());
				if (tmp.find(text) != std::string::npos) {
					result = main_menu->GetSubMenu(i);
					break;
				}
			}
			return result;
		}

		std::string MenuHandle::menu_label(const Link& link) const {
			std::string result;
			if (!link.plugin.expired()) {  
				std::string filename_noext;
				filename_noext = boost::filesystem::path(link.dll_name()).stem().string();
				std::transform(filename_noext.begin(), filename_noext.end(), filename_noext.begin(), ::tolower);
				result = filename_noext + " - " + link.plugin.lock()->title();
			} else {
				result = link.label();
			}
			return result;
		}

		void MenuHandle::HandleInput(UINT nID) {
// 			ui::mfc::MenuContainerImp* mbimp =  ui::mfc::MenuContainerImp::MenuContainerImpById(nID);
// 			if (mbimp != 0) {
// 				mbimp->WorkMenuItemEvent(nID);
// 				return;
// 			}
			MenuHandle::MenuMap::iterator it = menuItemIdMap_.find(nID);
			int viewport = CHILDVIEWPORT;
			if (it != menuItemIdMap_.end()) {        
				Link link = it->second;
				LuaPlugin::Ptr plug;
				bool expired = link.plugin.expired();
				if (expired) {
					try {
						link.plugin = plug = HostExtensions::Instance().Execute(link);			
					} catch(std::exception& e) {
						ui::alert(e.what());
						return;
					}
					if (link.user_interface() == SDI) {        
						menuItemIdMap_[nID] = link;
					}
					viewport = link.viewport();
				} else {					
					if (link.plugin.lock()->proxy().has_frame()) {			 
						return;			  
					}
					if (link.viewport() == TOOLBARVIEWPORT) {		     
						CDialogBar* bar = &((CMainFrame*)::AfxGetMainWnd())->m_extensionBar;
						if (bar->IsWindowVisible()) {
							((CMainFrame*)::AfxGetMainWnd())->ShowControlBar(bar, FALSE,FALSE);
						} else {
							((CMainFrame*)::AfxGetMainWnd())->ShowControlBar(bar, TRUE,FALSE);
						}
						viewport = TOOLBARVIEWPORT; 
					} else {
					  viewport = CHILDVIEWPORT;
					}
					plug = link.plugin.lock();
					ui::Viewport::WeakPtr user_view = plug->viewport();
					    
				}        
				if (!plug || plug->crashed()) return;        
					ui::Viewport::WeakPtr user_view = plug->viewport();        
					if (!user_view.expired()) {            
						HostExtensions::Instance().set_active_lua(plug.get());
						try {                                 
							plug->ViewPortChanged(*plug.get(), viewport);
							if (expired && viewport == FRAMEVIEWPORT) {						
								plug->proxy().OpenInFrame();
							}
							plug->OnActivated(viewport);
						} catch (std::exception&) {               
							// AfxMessageBox(e.what());
						}
						::AfxGetMainWnd()->Invalidate(false);
					} else {  
						try {
							plug->OnExecute();
						} catch (std::exception& e) {
							ui::alert(e.what());
						// LuaGlobal::onexception(plug->proxy().state());        
						}
					} 
					::AfxGetMainWnd()->DrawMenuBar();
				}    
		}   
		
		void MenuHandle::ExecuteLink(Link& link) {       
			MenuHandle::MenuMap::iterator it = menuItemIdMap_.begin();
			for (; it != menuItemIdMap_.end(); ++it) {
				if (it->second.dll_name() == link.dll_name()) {
					boost::shared_ptr<LuaPlugin> plug;
					int viewport = CHILDVIEWPORT;
					std::string script_path = PsycleGlobal::configuration().GetAbsoluteLuaDir();
					try {
						link.plugin = plug = HostExtensions::Instance().Execute(link);			
					} catch(std::exception& e) {
					ui::alert(e.what());
					return;
					}
					if (link.user_interface() == SDI) {        
						menuItemIdMap_[it->first] = link;
					}
					viewport = link.viewport();
					if (plug->crashed()) return;        
					ui::Viewport::WeakPtr user_view = plug->viewport();        
					if (!user_view.expired()) {            
						HostExtensions::Instance().set_active_lua(plug.get());
						try {                                 
							plug->ViewPortChanged(*plug.get(), viewport);
							plug->OnActivated(viewport);
						} catch (std::exception&) {               
						// AfxMessageBox(e.what());
						}
						::AfxGetMainWnd()->Invalidate(false);
					} else {  
					try {
						plug->OnExecute();
					} catch (std::exception& e) {
						ui::alert(e.what());
						// LuaGlobal::onexception(plug->proxy().state());        
					}
					} 
					::AfxGetMainWnd()->DrawMenuBar();
					break;
				}
			}
		}
		
		CChildView::CChildView()
			:pParentFrame(0)
			,hRecentMenu(0)
			,MasterMachineDialog(NULL)
			,SamplerMachineDialog(NULL)
			,XMSamplerMachineDialog(NULL)
			,WaveInMachineDialog(NULL)
			,blockSelected(false)
			,blockStart(false)
			,blockswitch(false)
			,blockSelectBarState(1)
			,bScrollDetatch(false)
			,bEditMode(true)
			,patStep(1)
			,editPosition(0)
			,prevEditPosition(0)
			,ChordModeOffs(0)
			,updateMode(0)
			,updatePar(0)			
			,_outputActive(false)
			,CW(300)
			,CH(200)
			,maxView(false)
			,textLeftEdge(2)
			,bmpDC(NULL)
			,playpos(-1)
			,newplaypos(-1) 
			,numPatternDraw(0)
			,smac(-1)
			,smacmode(smac_modes::move)
			,wiresource(-1)
			,wiredest(-1)
			,wiremove(-1)
			,wireSX(0)
			,wireSY(0)
			,wireDX(0)
			,wireDY(0)
			,allowcontextmenu(true)
			,popupmacidx(-1)
			,maxt(1)
			,maxl(1)
			,tOff(0)
			,lOff(0)
			,ntOff(0)
			,nlOff(0)
			,scrollDelay(0)
			,rntOff(0)
			,rnlOff(0)
			,trackingMuteTrack(-1)
			,trackingRecordTrack(-1)
			,trackingSoloTrack(-1)
			,isBlockCopied(false)
			,blockNTracks(0)
			,blockNLines(0)
			,mcd_x(0)
			,mcd_y(0)
			,patBufferLines(0)
			,patBufferCopy(false)
			,_pSong(Global::song())					
			,oldViewMode(view_modes::machine)			
		{
			HostExtensions::Instance().InitInstance(this);						 
			for(int c(0) ; c < MAX_WIRE_DIALOGS ; ++c)
			{
				WireDialog[c] = NULL;
			}
			for (int c=0; c<256; c++)	{ FLATSIZES[c]=8; }
			selpos.bottom=0;
			newselpos.bottom=0;
			szBlankParam[0]='\0';
			note_tab_selected=NULL;
			MBStart.x=0;
			MBStart.y=0;      

			patView = &PsycleGlobal::conf().patView();
			macView = &PsycleGlobal::conf().macView();
			PatHeaderCoords = &patView->PatHeaderCoords;
			MachineCoords = &macView->MachineCoords;

			PsycleGlobal::inputHandler().SetChildView(this);

			// Creates a new song object. The application Song.
			_pSong.New();						
		}

		CChildView::~CChildView()
		{
			PsycleGlobal::inputHandler().SetChildView(NULL);      
			if ( bmpDC != NULL )
			{
				char buf[128];
				sprintf(buf,"CChildView::~CChildView(). Deleted bmpDC (was 0x%p)\n",(void*)bmpDC);
				TRACE(buf);
				bmpDC->DeleteObject();
				delete bmpDC; bmpDC = 0;
			}
		}

/*
		BEGIN_MESSAGE_MAP(CChildView,CWnd)      
			ON_WM_TIMER()
			ON_WM_PAINT()
			ON_WM_CREATE()
			ON_WM_DESTROY()
			ON_WM_SIZE()
			ON_WM_CONTEXTMENU()
			ON_WM_HSCROLL()
			ON_WM_VSCROLL()
			ON_WM_KEYDOWN()
			ON_WM_KEYUP()
			ON_WM_LBUTTONDOWN()
			ON_WM_RBUTTONDOWN()
			ON_WM_LBUTTONUP()
			ON_WM_RBUTTONUP()
			ON_WM_LBUTTONDBLCLK()
			ON_WM_MBUTTONDOWN()
			ON_WM_MOUSEMOVE()
			ON_WM_MOUSEWHEEL()      
//Main menu and toolbar (A few entries are in MainFrm)
			ON_COMMAND(ID_FILE_NEW, OnFileNew)
			ON_COMMAND(ID_FILE_LOADSONG, OnFileLoadsong)
			ON_COMMAND(ID_FILE_SAVE, OnFileSave)
			ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
			ON_COMMAND(ID_IMPORT_PATTERNS, OnFileImportPatterns)
			ON_COMMAND(ID_IMPORT_INSTRUMENTS, OnFileImportInstruments)
			ON_COMMAND(ID_IMPORT_MACHINES, OnFileImportMachines)
			ON_COMMAND(ID_EXPORT_PATTERNS, OnFileExportPatterns)
			ON_COMMAND(ID_EXPORT_INSTRUMENTS, OnFileExportInstruments)
			ON_COMMAND(ID_EXPORT_MODULE, OnFileExportModule)
			ON_COMMAND(ID_FILE_SAVEAUDIO, OnFileSaveaudio)
			ON_COMMAND(ID_FILE_SONGPROPERTIES, OnFileSongproperties)
			ON_COMMAND(ID_FILE_REVERT, OnFileRevert)
			ON_COMMAND(ID_FILE_RECENT_01, OnFileRecent_01)
			ON_COMMAND(ID_FILE_RECENT_02, OnFileRecent_02)
			ON_COMMAND(ID_FILE_RECENT_03, OnFileRecent_03)
			ON_COMMAND(ID_FILE_RECENT_04, OnFileRecent_04)
			ON_COMMAND(ID_RECORDB, OnRecordWav)
			ON_UPDATE_COMMAND_UI(ID_RECORDB, OnUpdateRecordWav)
			ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
			ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
			ON_COMMAND(ID_EDIT_SEARCHANDREPLACE, OnEditSearchReplace)
			ON_COMMAND(ID_EDIT_CUT, patCut)
			ON_COMMAND(ID_EDIT_COPY, patCopy)
			ON_COMMAND(ID_EDIT_PASTE, patPaste)
			ON_COMMAND(ID_EDIT_MIXPASTE, patMixPaste)
			ON_COMMAND(ID_EDIT_DELETE, patDelete)
			ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateUndo)
			ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateRedo)
			ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdatePatternCutCopy)
			ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdatePatternCutCopy)
			ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdatePatternPaste)
			ON_UPDATE_COMMAND_UI(ID_EDIT_MIXPASTE, OnUpdatePatternPaste)
			ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdatePatternCutCopy)
			ON_COMMAND(ID_BARREC, OnBarrec)
			ON_COMMAND(ID_BARPLAYFROMSTART, OnBarplayFromStart)
			ON_COMMAND(ID_BARPLAY, OnBarplay)
			ON_COMMAND(ID_BUTTONPLAYSEQBLOCK, OnButtonplayseqblock)
			ON_COMMAND(ID_BARSTOP, OnBarstop)
			ON_COMMAND(ID_AUTOSTOP, OnAutostop)
			ON_COMMAND(ID_CONFIGURATION_LOOPPLAYBACK, OnConfigurationLoopplayback)
			ON_UPDATE_COMMAND_UI(ID_BARREC, OnUpdateBarrec)
			ON_UPDATE_COMMAND_UI(ID_BARPLAYFROMSTART, OnUpdateBarplayFromStart)
			ON_UPDATE_COMMAND_UI(ID_BARPLAY, OnUpdateBarplay)
			ON_UPDATE_COMMAND_UI(ID_BUTTONPLAYSEQBLOCK, OnUpdateButtonplayseqblock)
			ON_UPDATE_COMMAND_UI(ID_AUTOSTOP, OnUpdateAutostop)
			ON_UPDATE_COMMAND_UI(ID_CONFIGURATION_LOOPPLAYBACK, OnUpdateConfigurationLoopplayback)
			ON_COMMAND(ID_MACHINEVIEW, OnMachineview)
			ON_COMMAND(ID_PATTERNVIEW, OnPatternView)	
			ON_COMMAND(IDC_FULL_SCREEN, OnFullScreen)
			ON_UPDATE_COMMAND_UI(IDC_FULL_SCREEN, OnUpdateFullScreen)
			ON_COMMAND(ID_VIEW_INSTRUMENTEDITOR, OnViewInstrumenteditor)
			//Show Gear Rack is the command IDC_GEAR_RACK of the machine bar (in mainfrm)
			ON_COMMAND(ID_NEWMACHINE, OnNewmachine)
			//Show Wave editor is the command IDC_WAVEBUT of the machine bar (in mainfrm)
			ON_UPDATE_COMMAND_UI(ID_MACHINEVIEW, OnUpdateMachineview)
			ON_UPDATE_COMMAND_UI(ID_PATTERNVIEW, OnUpdatePatternView)
			ON_COMMAND(ID_CONFIGURATION_SETTINGS, OnConfigurationSettings)
			ON_COMMAND(ID_CONFIGURATION_ENABLEAUDIO, OnEnableAudio)
			ON_UPDATE_COMMAND_UI(ID_CONFIGURATION_ENABLEAUDIO, OnUpdateEnableAudio)
			ON_COMMAND(ID_CONFIGURATION_REGENERATEPLUGINCACHE, OnRegenerateCache)
			ON_COMMAND(ID_CPUPERFORMANCE, OnHelpPsycleenviromentinfo)
			ON_COMMAND(ID_MIDI_MONITOR, OnMidiMonitorDlg)
			ON_COMMAND(ID_HELP_README, OnHelpReadme)
			ON_COMMAND(ID_HELP_KEYBTXT, OnHelpKeybtxt)
			ON_COMMAND(ID_HELP_TWEAKING, OnHelpTweaking)
			ON_COMMAND(ID_HELP_WHATSNEW, OnHelpWhatsnew)
			ON_COMMAND(ID_HELP_SALUDOS, OnHelpSaludos)
			ON_COMMAND(ID_HELP_LUASCRIPT, OnHelpLuaScript)
//Pattern Popup
			ON_COMMAND(ID_POP_CUT, OnPopCut)
			ON_COMMAND(ID_POP_COPY, OnPopCopy)
			ON_COMMAND(ID_POP_PASTE, OnPopPaste)
			ON_COMMAND(ID_POP_MIXPASTE, OnPopMixpaste)
			ON_COMMAND(ID_POP_DELETE, OnPopDelete)
			ON_COMMAND(ID_POP_ADDNEWTRACK, OnPopAddNewTrack)
			ON_COMMAND(ID_POP_INTERPOLATE, OnPopInterpolate)
			ON_COMMAND(ID_POP_INTERPOLATE_CURVE, OnPopInterpolateCurve)
			ON_COMMAND(ID_POP_CHANGEGENERATOR, OnPopChangegenerator)
			ON_COMMAND(ID_POP_CHANGEINSTRUMENT, OnPopChangeinstrument)
			ON_COMMAND(ID_POP_TRANSPOSE1, OnPopTranspose1)
			ON_COMMAND(ID_POP_TRANSPOSE12, OnPopTranspose12)
			ON_COMMAND(ID_POP_TRANSPOSE_1, OnPopTranspose_1)
			ON_COMMAND(ID_POP_TRANSPOSE_12, OnPopTranspose_12)
			ON_COMMAND(ID_POP_BLOCK_SWINGFILL, OnPopBlockSwingfill)
			ON_COMMAND(ID_POP_TRACK_SWINGFILL, OnPopTrackSwingfill)
			ON_COMMAND(ID_POP_TRANSFORMPATTERN, OnPopTransformpattern)
			ON_COMMAND(ID_POP_PATTENPROPERTIES, OnPopPattenproperties)
			ON_UPDATE_COMMAND_UI(ID_POP_CUT, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_COPY, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_MIXPASTE, OnUpdatePaste)
			ON_UPDATE_COMMAND_UI(ID_POP_PASTE, OnUpdatePaste)
			ON_UPDATE_COMMAND_UI(ID_POP_DELETE, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_INTERPOLATE, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_INTERPOLATE_CURVE, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_CHANGEGENERATOR, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_CHANGEINSTRUMENT, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_TRANSPOSE1, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_TRANSPOSE12, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_TRANSPOSE_1, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_TRANSPOSE_12, OnUpdateCutCopy)
			ON_UPDATE_COMMAND_UI(ID_POP_BLOCK_SWINGFILL, OnUpdateCutCopy)
/// Machine popup
			ON_COMMAND(ID_POPUPMAC_OPENPARAMETERS, OnPopMacOpenParams)
			ON_COMMAND(ID_POPUPMAC_OPENPROPERTIES, OnPopMacOpenProperties)
			ON_COMMAND(ID_POPUPMAC_OPENBANKMANAGER, OnPopMacOpenBankManager)
			ON_COMMAND_RANGE(ID_CONNECTTO_MACHINE0, ID_CONNECTTO_MACHINE64, OnPopMacConnecTo)
			ON_COMMAND_RANGE(ID_CONNECTIONS_CONNECTION0, ID_CONNECTIONS_CONNECTION24, OnPopMacShowWire)
			ON_COMMAND_RANGE(ID_DYNAMIC_MENUS_START, ID_DYNAMIC_MENUS_END, OnDynamicMenuItems)
			ON_COMMAND(ID_POPUPMAC_REPLACEMACHINE, OnPopMacReplaceMac)
			ON_COMMAND(ID_POPUPMAC_CLONEMACHINE, OnPopMacCloneMac)
			ON_COMMAND(ID_POPUPMAC_INSERTEFFECTBEFORE, OnPopMacInsertBefore)
			ON_COMMAND(ID_POPUPMAC_INSERTEFFECTAFTER, OnPopMacInsertAfter)
			ON_COMMAND(ID_POPUPMAC_DELETEMACHINE, OnPopMacDeleteMachine)
			ON_COMMAND(ID_POPUPMAC_MUTE, OnPopMacMute)
			ON_COMMAND(ID_POPUPMAC_SOLO, OnPopMacSolo)
			ON_COMMAND(ID_POPUPMAC_BYPASS, OnPopMacBypass)

			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_OPENPROPERTIES, OnUpdateMacOpenProps)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_REPLACEMACHINE, OnUpdateMacReplace)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_CLONEMACHINE, OnUpdateMacClone)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_INSERTEFFECTBEFORE, OnUpdateMacInsertBefore)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_INSERTEFFECTAFTER, OnUpdateMacInsertAfter)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_DELETEMACHINE, OnUpdateMacDelete)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_MUTE, OnUpdateMacMute)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_SOLO, OnUpdateMacSolo)
			ON_UPDATE_COMMAND_UI(ID_POPUPMAC_BYPASS, OnUpdateMacBypass)

		END_MESSAGE_MAP()
*/

		BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
		{
			if (!CWnd::PreCreateWindow(cs))
				return FALSE;
			
			cs.dwExStyle |= WS_EX_CLIENTEDGE;
			cs.style &= ~WS_BORDER;
			cs.lpszClass = AfxRegisterWndClass
				(
					CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
					//::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
					::LoadCursor(NULL, IDC_ARROW),
					(HBRUSH)GetStockObject( HOLLOW_BRUSH ),
					NULL
				);

			return TRUE;
		}

		int CChildView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
		  if (CWnd::OnCreate(lpCreateStruct) == -1)
				return -1;
			m_luaWndView.reset(new ExtensionWindow());
			m_luaWndView->Hide();
// 			ui::mfc::WindowImp* mfc_imp = (ui::mfc::WindowImp*) m_luaWndView->imp();
// 			mfc_imp->SetParent(this);
			return 0;
		}

		/// This function gives to the pParentMain the pointer to a CMainFrame
		/// object. Call this function from the CMainFrm side object to
		/// allow CChildView call functions of the CMainFrame parent object
		/// Call this function after creating both the CChildView object and
		/// the CMainFrame object
		void CChildView::ValidateParent()
		{
			pParentMain=(CMainFrame *)pParentFrame;
		}

		/// Timer initialization
		void CChildView::InitTimer()
		{      
			KillTimer(ID_TIMER_VIEW_REFRESH);
			KillTimer(ID_TIMER_AUTOSAVE);
			if (!SetTimer(ID_TIMER_VIEW_REFRESH, GlobalTimer::refresh_rate, NULL)) // GUI update. 
			{
				AfxMessageBox(IDS_COULDNT_INITIALIZE_TIMER, MB_ICONERROR);
			}

			if ( PsycleGlobal::conf().autosaveSong )
			{
				if (!SetTimer(ID_TIMER_AUTOSAVE,PsycleGlobal::conf().autosaveSongTime*60000,NULL)) // Autosave Song
				{
					AfxMessageBox(IDS_COULDNT_INITIALIZE_TIMER, MB_ICONERROR);
				}
			}
		}

		/// Timer handler


		void CChildView::OnTimer( UINT_PTR nIDEvent )
		{      
			if (nIDEvent == ID_TIMER_VIEW_REFRESH)
			{            
				CSingleLock lock(&_pSong.semaphore, FALSE);
				if (!lock.Lock(50)) return;
				GlobalTimer::instance().OnViewRefresh();
				Master* master = ((Master*)_pSong._pMachine[MASTER_INDEX]);
				if (master)
				{
					pParentMain->UpdateVumeters();
					pParentMain->UpdateMasterValue(master->_outDry);
					if ( MasterMachineDialog ) {
						MasterMachineDialog->UpdateUI();
						for (int i=0;i<MAX_CONNECTIONS; i++)
						{
							if ( master->inWires[i].Enabled())
							{
								strcpy(MasterMachineDialog->macname[i], master->inWires[i].GetSrcMachine()._editName);
							}
							else {
								strcpy(MasterMachineDialog->macname[i],"");
							}
						}
					}
				}
				if (viewMode == view_modes::machine)
				{
						Repaint(draw_modes::playback);
				} 

				for(int c=0; c<MAX_MACHINES; c++)
				{
					if (_pSong._pMachine[c])
					{
						if ( _pSong._pMachine[c]->_type == MACH_VST ||
							_pSong._pMachine[c]->_type == MACH_VSTFX )
						{
							//I don't know if this has to be done in a synchronized thread
							//(like this one) neither if it can take a moderate amount of time.
							static_cast<vst::Plugin*>(_pSong._pMachine[c])->Idle();
						}
					}
				}

				if (XMSamplerMachineDialog != NULL ) XMSamplerMachineDialog->UpdateUI();
				if (Global::player()._playing)
				{
					if (Global::player()._lineChanged)
					{
						Global::player()._lineChanged = false;
						pParentMain->SetAppSongBpm(0);
						pParentMain->SetAppSongTpb(0);

						if (PsycleGlobal::conf()._followSong)
						{
							CListBox* pSeqList = (CListBox*)pParentMain->m_seqBar.GetDlgItem(IDC_SEQLIST);
							editcur.line=Global::player()._lineCounter;
							if (editPosition != Global::player()._playPosition)
							//if (pSeqList->GetCurSel() != Global::player()._playPosition)
							{
								pSeqList->SelItemRange(false,0,pSeqList->GetCount());
								pSeqList->SetSel(Global::player()._playPosition,true);
								int top = Global::player()._playPosition - 0xC;
								if (top < 0) top = 0;
								pSeqList->SetTopIndex(top);
								editPosition=Global::player()._playPosition;
								if ( viewMode == view_modes::pattern ) 
								{ 
									Repaint(draw_modes::pattern);//draw_modes::playback_change);  // Until this mode is coded there is no point in calling it since it just makes patterns not refresh correctly currently
									Repaint(draw_modes::playback);
								}
							}
							else if( viewMode == view_modes::pattern ) Repaint(draw_modes::playback);
						}
						else if ( viewMode == view_modes::pattern ) Repaint(draw_modes::playback);
						if ( viewMode == view_modes::sequence ) Repaint(draw_modes::playback);
					}
				}
			}
			if (nIDEvent == ID_TIMER_AUTOSAVE && !Global::player()._recording)
			{
				std::string filepath(PsycleGlobal::conf().GetAbsoluteSongDir());
				filepath += "\\autosave.psy";
				OldPsyFile file;
				if(!file.Create(filepath, true)) return;
				CProgressDialog progress(NULL,false);
				_pSong.Save(&file,progress, true);
				if (!file.Close())
				{
					std::ostringstream s;
					s << "Error writing to file '" << file.szName << "'" << std::endl;
					MessageBox(s.str().c_str(),"File Error!!!",0);
				}
			}
		}

		void CChildView::OnEnableAudio()
		{
			AudioDriver* pOut = PsycleGlobal::conf()._pOutputDriver;
			if (pOut->Enabled()) {
				pOut->Enable(false);
				_outputActive = false;
			}
			else {
				_outputActive = pOut->Enable(true);
			}
		}
		void CChildView::OnUpdateEnableAudio(CCmdUI* pCmdUI) 
		{
			AudioDriver* pOut = PsycleGlobal::conf()._pOutputDriver;
			pCmdUI->SetCheck(pOut->Enabled());
		}
		void CChildView::EnableSound()
		{
			if (_outputActive)
			{
				AudioDriver* pOut = PsycleGlobal::conf()._pOutputDriver;

				_outputActive = false;
				if (!pOut->Enabled())
				{
					_outputActive = pOut->Enable(true);
				}
				else {
					_outputActive = true;
				}
			}
			// set midi input mode to real-time or step
			if(viewMode == view_modes::machine && PsycleGlobal::conf().midi()._midiMachineViewSeqMode)
				PsycleGlobal::midi().m_midiMode = MODE_REALTIME;
			else
				PsycleGlobal::midi().m_midiMode = MODE_STEP;
		}
		void CChildView::OnRegenerateCache()
		{
			Global::machineload().ReScan();
		}

		/// Put exit destroying code here...
		void CChildView::OnDestroy()
		{
			HostExtensions::Instance().ExitInstance();
			GlobalTimer::instance().Clear();
			if (PsycleGlobal::conf()._pOutputDriver->Initialized())
			{
				PsycleGlobal::conf()._pOutputDriver->Reset();
			}
			pParentMain->KillTimer(ID_TIMER_VIEW_REFRESH);
			pParentMain->KillTimer(ID_TIMER_AUTOSAVE);			

			//This prevents some rare cases with LUA plugins and the LuaWaveOscBind::oscs being deleted before Song instance.
			Global::song().New();
		}

		void CChildView::OnPaint() 
		{
			CRgn rgn;
			rgn.CreateRectRgn(0, 0, 0, 0);
			GetUpdateRgn(&rgn, FALSE);

			if (!GetUpdateRect(NULL) ) return; // If no area to update, exit.
			CPaintDC dc(this);

			if ( bmpDC == NULL ) // buffer creation
			{
				CRect rc;
				GetClientRect(&rc);
				bmpDC = new CBitmap;
				bmpDC->CreateCompatibleBitmap(&dc,rc.right-rc.left,rc.bottom-rc.top);
				char buf[128];
				sprintf(buf,"CChildView::OnPaint(). Initialized bmpDC to 0x%p\n",(void*)bmpDC);
				TRACE(buf);
			}
			CDC bufDC;			
			bufDC.CreateCompatibleDC(&dc);
			CBitmap* oldbmp;
			oldbmp = bufDC.SelectObject(bmpDC);
			if (viewMode==view_modes::machine)	// Machine view paint handler
			{
				switch (updateMode)
				{
				case draw_modes::all:
					DrawMachineEditor(&bufDC);
					break;
				case draw_modes::machine:
					//ClearMachineSpace(Global::song()._pMachines[updatePar], updatePar, &bufDC);
					DrawMachine(updatePar, &bufDC);
					DrawMachineVumeters(updatePar, &bufDC);
					updateMode=draw_modes::all;
					break;
				case draw_modes::playback:
					///\todo need to refresh also mute/solo/bypass and panning.
					//warning: doing DrawMachine can cause problems if transparent
					//graphics or if machine text is drawn outside of the machine.
					DrawAllMachineVumeters(&bufDC);
					break;
				case draw_modes::all_machines:
					for (int i=0;i<MAX_MACHINES;i++)
					{
						if (_pSong._pMachine[i])
						{
							DrawMachine(i, &bufDC);
						}
					}
					DrawAllMachineVumeters(&bufDC);
					break;
				}
			}
			else if (viewMode == view_modes::pattern)	// Pattern view paint handler
			{
				DrawPatEditor(&bufDC);
			}
			else if ( viewMode == view_modes::sequence)
			{
				DrawSeqEditor(&bufDC);
			}      

			CRect rc;
			GetClientRect(&rc);
			dc.BitBlt(0,0,rc.right-rc.left,rc.bottom-rc.top,&bufDC,0,0,SRCCOPY);
			bufDC.SelectObject(oldbmp);
			bufDC.DeleteDC();
			rgn.DeleteObject();
		}

		void CChildView::Repaint(draw_modes::draw_mode drawMode)
		{
			switch(viewMode)
			{
			case view_modes::machine:
				{
					if ( drawMode <= draw_modes::machine )
					{
						updateMode = drawMode;
						Invalidate(false);
					}
					else if ( drawMode == draw_modes::playback && macView->draw_vus)
					{
						updateMode = drawMode;
						PrepareDrawAllMacVus();
					}
				}
				break;
			case view_modes::pattern:
				{
					if (drawMode >= draw_modes::pattern || drawMode == draw_modes::all )	
					{
						PreparePatternRefresh(drawMode);
					}
				}
				break;
			case view_modes::sequence:
				{
					Invalidate(false);
				}
				break;
			}			
		}

		void CChildView::OnSize(UINT nType, int cx, int cy) 
		{
			CWnd ::OnSize(nType, cx, cy);      

			CW = cx;
			CH = cy;
			
			if ( bmpDC != NULL ) // remove old buffer to force recreating it with new size
			{
				TRACE("CChildView::OnResize(). Deleted bmpDC\n");
				bmpDC->DeleteObject();
				delete bmpDC; bmpDC = 0;
			}
			if (viewMode == view_modes::pattern)
			{
				RecalcMetrics();
			}						
			if (m_luaWndView)
			{				
				m_luaWndView->set_position(ui::Rect(ui::Point(), ui::Dimension(cx, cy)));
			}  
			Repaint();   			
		}

		/// "Save Song" Function
		void CChildView::OnFileExportModule() 
		{
			OPENFILENAME ofn; // common dialog box structure
			std::string name = Global::song().fileName;
			std::size_t dotpos = name.find_last_of(".");
			if ( dotpos == std::string::npos ) dotpos = name.length();
			std::string ifile = name.substr(0,dotpos)  + ".xm";
			
			char szFile[_MAX_PATH];

			szFile[_MAX_PATH-1]=0;
			strncpy(szFile,ifile.c_str(),_MAX_PATH-1);
			
			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "FastTracker 2 Song (*.xm)\0*.xm\0All (*.*)\0*.*\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			std::string tmpstr = PsycleGlobal::conf().GetCurrentSongDir();
			ofn.lpstrInitialDir = tmpstr.c_str();
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
			
			// Display the Open dialog box. 
			if (GetSaveFileName(&ofn) == TRUE)
			{
				CString str = ofn.lpstrFile;

				CString str2 = str.Right(3);
				if ( str2.CompareNoCase(".xm") != 0 ) str.Insert(str.GetLength(),".xm");

				int index = str.ReverseFind('\\');
				if (index != -1)
				{
					PsycleGlobal::conf().SetCurrentSongDir(static_cast<char const *>(str.Left(index)));
				}
				std::stringstream ss;
				ss << str;
				XMSongExport file;
				if (!file.Create(ss.str(), true))
				{
					MessageBox("Error creating file!", "Error!", MB_OK);
					return;
				}
				file.exportsong(Global::song());
				file.Close();
			}
		}

		/// "Save Song" Function
		void CChildView::OnFileSave() 
		{
			if ( Global::song()._saved )
			{
				if (MessageBox("Proceed with Saving?","Song Save",MB_YESNO) == IDYES)
				{
					std::string filepath = PsycleGlobal::conf().GetCurrentSongDir();
					filepath += '\\';
					filepath += Global::song().fileName;
					
					OldPsyFile file;
					CProgressDialog progress;
					if (!file.Create((char*)filepath.c_str(), true))
					{
						MessageBox("Error creating file!", "Error!", MB_OK);
						return;
					}
					progress.SetWindowText("Saving...");
					progress.ShowWindow(SW_SHOW);
					if (!_pSong.Save(&file, progress))
					{
						MessageBox("Error saving file!", "Error!", MB_OK);
					}
					else 
					{
						PsycleGlobal::inputHandler().SafePoint();
					}
					progress.SendMessage(WM_CLOSE);
					if (!file.Close())
					{
						std::ostringstream s;
						s << "Error writing to file '" << file.szName << "'" << std::endl;
						MessageBox(s.str().c_str(),"File Error!!!",0);
					}
				}
			}
			else 
			{
				OnFileSaveAs();
			}
		}

		//////////////////////////////////////////////////////////////////////
		// "Save Song As" Function

		void CChildView::OnFileSaveAs() 
		{
			OPENFILENAME ofn; // common dialog box structure
			std::string ifile = Global::song().fileName;
			
			char szFile[_MAX_PATH];

			szFile[_MAX_PATH-1]=0;
			strncpy(szFile,ifile.c_str(),_MAX_PATH-1);
			
			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "Songs (*.psy)\0*.psy\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			std::string tmpstr = PsycleGlobal::conf().GetCurrentSongDir();
			ofn.lpstrInitialDir = tmpstr.c_str();
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
			
			// Display the Open dialog box. 
			if (GetSaveFileName(&ofn) == TRUE)
			{
				CString str = ofn.lpstrFile;
				OldPsyFile file;
				CProgressDialog progress;
				CString str2 = str.Right(4);
				if ( str2.CompareNoCase(".psy") != 0 ) str.Insert(str.GetLength(),".psy");
				int index = str.ReverseFind('\\');
				if (index != -1)
				{
					PsycleGlobal::conf().SetCurrentSongDir(static_cast<char const *>(str.Left(index)));
					Global::song().fileName = str.Mid(index+1);
				}
				else
				{
					Global::song().fileName = str;
				}
				
				if (!file.Create(Global::song().fileName, true))
				{
					MessageBox("Error creating file!", "Error!", MB_OK);
					return;
				}

				progress.SetWindowText("Saving...");
				progress.ShowWindow(SW_SHOW);
				if (!_pSong.Save(&file,progress))
				{
					MessageBox("Error saving file!", "Error!", MB_OK);
				}
				else 
				{
					std::stringstream ss;
					ss << str;

					AppendToRecent(ss.str());
					
					PsycleGlobal::inputHandler().SafePoint();
				}
				progress.SendMessage(WM_CLOSE);
				if (!file.Close())
				{
					std::ostringstream s;
					s << "Error writing to file '" << file.szName << "'" << std::endl;
					MessageBox(s.str().c_str(),"File Error!!!",0);
				}
			}
		}

		#include <cderr.h>

		void CChildView::OnFileLoadsong()
		{
			OPENFILENAME ofn; // common dialog box structure
			char szFile[_MAX_PATH]; // buffer for file name
			
			szFile[0]='\0';
			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "All Songs (*.psy *.xm *.it *.s3m *.mod)" "\0*.psy;*.xm;*.it;*.s3m;*.mod\0"
				"Songs (*.psy)"				        "\0*.psy\0"
				"FastTracker II Songs (*.xm)"       "\0*.xm\0"
				"Impulse Tracker Songs (*.it)"      "\0*.it\0"
				"Scream Tracker Songs (*.s3m)"      "\0*.s3m\0"
				"Original Mod Format Songs (*.mod)" "\0*.mod\0";;
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			std::string tmpstr = PsycleGlobal::conf().GetCurrentSongDir();
			ofn.lpstrInitialDir = tmpstr.c_str();
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
			
			// Display the Open dialog box. 
			if(::GetOpenFileName(&ofn)==TRUE)
			{				
				FileLoadsongNamed(szFile);				
			}
			else
			{
// 				DWORD comDlgErr = CommDlgExtendedError();
// 				switch(comDlgErr)
// 				{
// 				case CDERR_DIALOGFAILURE:
// 					MessageBox("CDERR_DIALOGFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_FINDRESFAILURE:
// 					MessageBox("CDERR_FINDRESFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_INITIALIZATION:
// 					MessageBox("CDERR_INITIALIZATION", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_LOADRESFAILURE:
// 					MessageBox("CDERR_LOADRESFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_LOADSTRFAILURE:
// 					MessageBox("CDERR_LOADSTRFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_LOCKRESFAILURE:
// 					MessageBox("CDERR_LOCKRESFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_MEMALLOCFAILURE:
// 					MessageBox("CDERR_MEMALLOCFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_MEMLOCKFAILURE:
// 					MessageBox("CDERR_MEMLOCKFAILURE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_NOHINSTANCE:
// 					MessageBox("CDERR_NOHINSTANCE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_NOHOOK:
// 					MessageBox("CDERR_NOHOOK", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_NOTEMPLATE:
// 					MessageBox("CDERR_NOTEMPLATE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_REGISTERMSGFAIL:
// 					MessageBox("CDERR_REGISTERMSGFAIL", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				case CDERR_STRUCTSIZE:
// 					MessageBox("CDERR_STRUCTSIZE", "exception", MB_OK | MB_ICONERROR);
// 					break;
// 				}
			}
			pParentMain->StatusBarIdle();
		}

		void CChildView::OnFileNew() 
		{
			if (CheckUnsavedSong("New Song"))
			{
				PsycleGlobal::inputHandler().KillUndo();
				PsycleGlobal::inputHandler().KillRedo();
				pParentMain->CloseAllMacGuis();
				Global::player().Stop();

				_pSong.New();
				_pSong._pMachine[MASTER_INDEX]->_x = (CW - PsycleGlobal::conf().macView().MachineCoords.sMaster.width) / 2;
				_pSong._pMachine[MASTER_INDEX]->_y = (CH - PsycleGlobal::conf().macView().MachineCoords.sMaster.height) / 2;

				Global::player().SetBPM(Global::song().BeatsPerMin(),Global::song().LinesPerBeat(),Global::song().ExtraTicksPerLine());
				editPosition=0;
				pParentMain->PsybarsUpdate(); // Updates all values of the bars
				pParentMain->WaveEditorBackUpdate();
				pParentMain->UpdateInstrumentEditor();
				pParentMain->RedrawGearRackList();
				pParentMain->UpdateSequencer();
				pParentMain->UpdatePlayOrder(false); // should be done always after updatesequencer
				//pParentMain->UpdateComboIns(); PsybarsUpdate calls UpdateComboGen that always call updatecomboins
				RecalculateColourGrid();
				Repaint();
				SetTitleBarText();
				ANOTIFY(Actions::songnew); 
			}
			pParentMain->StatusBarIdle();
		}

		void CChildView::OnFileImportPatterns()
		{
			OPENFILENAME ofn; // common dialog box structure
			char szFile[16*_MAX_PATH]; // buffer for file name
			
			szFile[0]='\0';
			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "Psycle Pattern (*.psb)\0*.psb\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			std::string tmpstr = PsycleGlobal::conf().GetCurrentSongDir();
			ofn.lpstrInitialDir = tmpstr.c_str();
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_DONTADDTORECENT;
			
			// Display the Open dialog box. 
			if(::GetOpenFileName(&ofn)==TRUE)
			{
				if (ofn.nFileOffset > 0 && ofn.lpstrFile[ofn.nFileOffset-1]== NULL) {
					std::string dir = ofn.lpstrFile;
					int i= ofn.nFileOffset;
					while ( i < 16*_MAX_PATH && ofn.lpstrFile[i]!= NULL) {
						std::string file = (ofn.lpstrFile+i);
						ImportPatternBlock(dir+'\\'+file, true);
						i+=file.length()+1;
					}
				}
				else {
					ImportPatternBlock(szFile);
				}
			}
			pParentMain->StatusBarIdle();
		}
		void CChildView::OnFileImportInstruments()
		{
			MessageBox("This option will allow to import instruments from other .psy files or module files. Currently unimplemented");
		}
		void CChildView::OnFileImportMachines()
		{
			MessageBox("This option will allow to import machines from other .psy files. Currently unimplemented");
		}
		void CChildView::OnFileExportPatterns()
		{
			OPENFILENAME ofn; // common dialog box structure
			std::ostringstream asdf;
			std::size_t dotpos = _pSong.fileName.find_last_of(".");
			if ( dotpos == std::string::npos ) dotpos = _pSong.fileName.length();
			asdf << _pSong.fileName.substr(0,dotpos) <<"-" << std::hex << static_cast<uint32_t>(_pSong.playOrder[editPosition]);
			char szFile[_MAX_PATH];

			szFile[_MAX_PATH-1]=0;
			strncpy(szFile,asdf.str().c_str(),_MAX_PATH-1);
			
			// Initialize OPENFILENAME
			ZeroMemory(&ofn, sizeof(OPENFILENAME));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = GetParent()->m_hWnd;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrFilter = "Psycle Pattern (*.psb)\0*.psb\0";
			ofn.lpstrDefExt = "psb";
			ofn.nFilterIndex = 1;
			ofn.lpstrFileTitle = NULL;
			ofn.nMaxFileTitle = 0;
			std::string tmpstr = PsycleGlobal::conf().GetCurrentSongDir();
			ofn.lpstrInitialDir = tmpstr.c_str();
			ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
			
			// Display the Open dialog box. 
			if (GetSaveFileName(&ofn) == TRUE)
			{
				ExportPatternBlock(ofn.lpstrFile);
			}
		}
		void CChildView::OnFileExportInstruments()
		{
		}
		void CChildView::OnFileSaveaudio() 
		{
			OnBarstop();
			KillTimer(ID_TIMER_VIEW_REFRESH);
			KillTimer(ID_TIMER_AUTOSAVE);
			OnTimer(ID_TIMER_AUTOSAVE); // Autosave
			CSaveWavDlg dlg(this, &blockSel);
			dlg.DoModal();
			InitTimer();
		}

		///\todo that method does not take machine changes into account  
		//  <JosepMa> is this still the case? or what does "machine changes" mean?
		BOOL CChildView::CheckUnsavedSong(std::string szTitle)
		{
			if (PsycleGlobal::inputHandler().IsModified()
				&& PsycleGlobal::conf().bFileSaveReminders)
			{
				std::string filepath = PsycleGlobal::conf().GetCurrentSongDir();
				filepath += '\\';
				filepath += Global::song().fileName;
				OldPsyFile file;
				CProgressDialog progress;
				std::ostringstream szText;
				szText << "Save changes to \"" << Global::song().fileName << "\"?";
				int result = MessageBox(szText.str().c_str(),szTitle.c_str(),MB_YESNOCANCEL | MB_ICONEXCLAMATION);
				switch (result)
				{
				case IDYES:
					progress.SetWindowText("Saving...");
					progress.ShowWindow(SW_SHOW);
					if (!file.Create((char*)filepath.c_str(), true))
					{
						std::ostringstream szText;
						szText << "Error writing to \"" << filepath << "\"!!!";
						MessageBox(szText.str().c_str(),szTitle.c_str(),MB_ICONEXCLAMATION);
						return FALSE;
					}
					_pSong.Save(&file,progress);
					progress.SendMessage(WM_CLOSE);
					if (!file.Close())
					{
						std::ostringstream s;
						s << "Error writing to file '" << file.szName << "'" << std::endl;
						MessageBox(s.str().c_str(),"File Error!!!",0);
					}
					return TRUE;
					break;
				case IDNO:
					return TRUE;
					break;
				case IDCANCEL:
					return FALSE;
					break;
				}
			}
			return TRUE;
		}

		void CChildView::OnFileRevert()
		{
			if (MessageBox("Warning! You will lose all changes since song was last saved! Proceed?","Revert to Saved",MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
			{
				if (Global::song()._saved)
				{
					std::ostringstream fullpath;
					fullpath << PsycleGlobal::conf().GetCurrentSongDir()
						<< '\\' << Global::song().fileName;
					FileLoadsongNamed(fullpath.str());
				}
			}
			pParentMain->StatusBarIdle();
		}
        
		/// Tool bar buttons and View Commands
		void CChildView::OnMachineview() 
		{			        						
			if (viewMode != view_modes::machine)
			{        
				m_luaWndView->RemoveExtensions();
				m_luaWndView->UpdateMenu(extension_menu_handle_);
				viewMode = view_modes::machine;
				ShowScrollBar(SB_BOTH,FALSE);

				// set midi input mode to real-time or Step
				if(PsycleGlobal::conf().midi()._midiMachineViewSeqMode) {
					PsycleGlobal::midi().m_midiMode = MODE_REALTIME;
					PsycleGlobal::midi().ReSync();
				}
				else
					PsycleGlobal::midi().m_midiMode = MODE_STEP;

				Repaint();
				pParentMain->StatusBarIdle();
			}
			SetFocus();
		}

		void CChildView::OnUpdateMachineview(CCmdUI* pCmdUI) 
		{
			if (viewMode==view_modes::machine)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);
		}

		void CChildView::OnPatternView() 
		{								
			if (viewMode != view_modes::pattern)
			{     
				m_luaWndView->RemoveExtensions();
				m_luaWndView->UpdateMenu(extension_menu_handle_);	   
				RecalcMetrics();

				viewMode = view_modes::pattern;
				//ShowScrollBar(SB_BOTH,FALSE);
				
				// set midi input mode to step insert
				PsycleGlobal::midi().m_midiMode = MODE_STEP;
				
				GetParent()->SetActiveWindow();

				if (PsycleGlobal::conf()._followSong &&
					editPosition  != Global::player()._playPosition &&
					Global::player()._playing)
				{
					editPosition=Global::player()._playPosition;
				}
				Repaint();
				pParentMain->StatusBarIdle();
			}
			SetFocus();
		}

		void CChildView::OnUpdatePatternView(CCmdUI* pCmdUI) 
		{
			if(viewMode == view_modes::pattern)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);
		}

		void CChildView::OnShowPatternSeq() 
		{
			/*
			if (viewMode != view_modes::sequence)
			{
				viewMode = view_modes::sequence;
				ShowScrollBar(SB_BOTH,FALSE);
				
				// set midi input mode to step insert
				PsycleGlobal::midi().m_midiMode = MODE_STEP;
				
				GetParent()->SetActiveWindow();
				Repaint();
				pParentMain->StatusBarIdle();
			}	
			*/
			SetFocus();
		}

		void CChildView::OnUpdatePatternSeq(CCmdUI* pCmdUI) 
		{
			if (viewMode==view_modes::sequence)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);	
		}

		void CChildView::OnBarplay() 
		{
			if (PsycleGlobal::conf()._followSong)
			{
				bScrollDetatch=false;
			}
			prevEditPosition=editPosition;
			Global::player().Start(editPosition,0);
			pParentMain->StatusBarIdle();
			ANOTIFY(Actions::play);
		}

		void CChildView::OnBarplayFromStart() 
		{
			if (PsycleGlobal::conf()._followSong)
			{
				bScrollDetatch=false;
			}
			prevEditPosition=editPosition;
			Global::player().Start(0,0);
			pParentMain->StatusBarIdle();
			ANOTIFY(Actions::playstart);
		}

		void CChildView::OnUpdateBarplay(CCmdUI* pCmdUI) 
		{
			if (Global::player()._playing)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);
		}

		void CChildView::OnUpdateBarplayFromStart(CCmdUI* pCmdUI) 
		{
			pCmdUI->SetCheck(0);
		}

		void CChildView::OnBarrec() 
		{
			if (PsycleGlobal::conf()._followSong && bEditMode)
			{
				bEditMode = FALSE;
			}
			else
			{
				PsycleGlobal::conf()._followSong = TRUE;
				bEditMode = TRUE;
				CButton*cb=(CButton*)pParentMain->m_seqBar.GetDlgItem(IDC_FOLLOW);
				cb->SetCheck(1);
			}
			pParentMain->StatusBarIdle();
			ANOTIFY(Actions::rec);
		}

		void CChildView::OnUpdateBarrec(CCmdUI* pCmdUI) 
		{
			if (PsycleGlobal::conf()._followSong && bEditMode)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);
		}

		void CChildView::OnButtonplayseqblock() 
		{
			if (PsycleGlobal::conf()._followSong)
			{
				bScrollDetatch=false;
			}

			prevEditPosition=editPosition;
			int i=0;
			while ( Global::song().playOrderSel[i] == false ) i++;
			
			if(!Global::player()._playing)
				Global::player().Start(i,0);

			Global::player()._playBlock=!Global::player()._playBlock;

			pParentMain->StatusBarIdle();
			if ( viewMode == view_modes::pattern ) Repaint(draw_modes::pattern);
			ANOTIFY(Actions::playseq);
		}

		void CChildView::OnUpdateButtonplayseqblock(CCmdUI* pCmdUI) 
		{
			if ( Global::player()._playBlock == true ) pCmdUI->SetCheck(TRUE);
			else pCmdUI->SetCheck(FALSE);
		}

		void CChildView::OnBarstop()
		{
			bool pl = Global::player()._playing;
			bool blk = Global::player()._playBlock;
			Global::player().Stop();
			Repaint(draw_modes::playback);
			pParentMain->SetAppSongBpm(0);
			pParentMain->SetAppSongTpb(0);

			if (pl)
			{
				if ( PsycleGlobal::conf()._followSong && blk)
				{
					editPosition=prevEditPosition;
					pParentMain->UpdatePlayOrder(false); // <- This restores the selected block
					Repaint(draw_modes::pattern);
				}
				else
				{
					memset(Global::song().playOrderSel,0,MAX_SONG_POSITIONS*sizeof(bool));
					Global::song().playOrderSel[editPosition] = true;
					Repaint(draw_modes::cursor); 
				}
			}
      ANOTIFY(Actions::stop);
		}

		void CChildView::OnRecordWav() 
		{
			if (!Global::player()._recording)
			{
				static const char szFilter[] = "Wav Files (*.wav)|*.wav|All Files (*.*)|*.*||";
				
				CFileDialog dlg(false,"wav",NULL,OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_DONTADDTORECENT,szFilter);
				if ( dlg.DoModal() == IDOK ) 
				{
					std::stringstream ss;
					ss << dlg.GetPathName();
					Global::player().StartRecording(ss.str());
				}
				//If autoStopMachine is activated, deactivate it while recording
				if ( PsycleGlobal::conf().UsesAutoStopMachines() ) 
				{
					OnAutostop();
				}
			}
			else
			{
				Global::player().StopRecording();
			}
		}

		void CChildView::OnUpdateRecordWav(CCmdUI* pCmdUI) 
		{
			if (Global::player()._recording)
			{
				pCmdUI->SetCheck(1);
			}
			else
			{
				pCmdUI->SetCheck(0);
			}
		}

		void CChildView::OnAutostop() 
		{
			if ( PsycleGlobal::conf().UsesAutoStopMachines() )
			{
				PsycleGlobal::conf().UseAutoStopMachines(false);
				for (int c=0; c<MAX_MACHINES; c++)
				{
					if (Global::song()._pMachine[c])
					{
						Global::song()._pMachine[c]->Standby(false);
					}
				}
			}
			else PsycleGlobal::conf().UseAutoStopMachines(true);
		}

		void CChildView::OnUpdateAutostop(CCmdUI* pCmdUI) 
		{
			if (PsycleGlobal::conf().UsesAutoStopMachines() ) pCmdUI->SetCheck(TRUE);
			else pCmdUI->SetCheck(FALSE);
		}

		void CChildView::OnFileSongproperties() 
		{	
			CSongpDlg dlg(Global::song());
			dlg.DoModal();
			pParentMain->PsybarsUpdate();
			pParentMain->UpdatePlayOrder(false);
			pParentMain->StatusBarIdle();
			//Repaint();
		}

		void CChildView::OnViewInstrumenteditor()
		{
			pParentMain->ShowInstrumentEditor();
		}


		/// Show the CPU Performance dialog
		void CChildView::OnHelpPsycleenviromentinfo() 
		{
			pParentMain->ShowPerformanceDlg();
		}

		/// Show the MIDI monitor dialog
		void CChildView::OnMidiMonitorDlg() 
		{
			pParentMain->ShowMidiMonitorDlg();
		}

		void CChildView::ShowTransformPatternDlg(void)
		{
			CTransformPatternDlg dlg(_pSong, *this);
			//\todo: implement as a modeless dialog
			dlg.DoModal();
		}

		void CChildView::ShowPatternDlg(void)
		{
			CPatDlg dlg(_pSong);
			int patNum = _pSong.playOrder[editPosition];
			int nlines = _pSong.patternLines[patNum];
			char name[32];
			strcpy(name,_pSong.patternName[patNum]);

			dlg.patLines= nlines;
			strcpy(dlg.patName,name);
			dlg.patIdx = patNum;
			dlg.m_shownames = PsycleGlobal::conf().patView().showTrackNames_?1:0;
			dlg.m_independentnames = _pSong.shareTrackNames?0:1;

			if (dlg.DoModal() == IDOK)
			{
				_pSong.shareTrackNames = (dlg.m_independentnames == 0);
				for(int i(0); i< _pSong.SONGTRACKS; i++) {
					_pSong.ChangeTrackName(patNum,i,dlg.tracknames[i]);
				}
				if (PsycleGlobal::conf().patView().showTrackNames_ != (dlg.m_shownames != 0)) {
					ShowTrackNames((dlg.m_shownames != 0));
				}

				if ( nlines != dlg.patLines )
				{
					PsycleGlobal::inputHandler().AddUndo(patNum,0,0,MAX_TRACKS,nlines,editcur.track,editcur.line,editcur.col,editPosition);
					PsycleGlobal::inputHandler().AddUndoLength(patNum,nlines,editcur.track,editcur.line,editcur.col,editPosition);
					_pSong.AllocNewPattern(patNum,dlg.patName,dlg.patLines,dlg.m_adaptsize?true:false);
					if ( strcmp(name,dlg.patName) != 0 )
					{
						strcpy(_pSong.patternName[patNum],dlg.patName);
						pParentMain->UpdateSequencer();
						pParentMain->StatusBarIdle();
					}
					Repaint();
					ANOTIFY(Actions::patternlength);
				}
				else if ( strcmp(name,dlg.patName) != 0 )
				{
					strcpy(_pSong.patternName[patNum],dlg.patName);
					pParentMain->UpdateSequencer();
					pParentMain->StatusBarIdle();
				}
			}
		}

		void CChildView::OnNewmachine() 
		{
			NewMachine();
		}
		
		void CChildView::CreateNewMachine(int x, int y, int mac, int selectedMode, int Outputmachine, const std::string& psOutputDll, int shellIdx, Machine* insert)
		{      
			int fb,xs,ys;
			if (mac < 0)
			{
				PsycleGlobal::inputHandler().AddMacViewUndo();
				if (selectedMode == modegen) 
				{
					fb = Global::song().GetFreeBus();
					xs = MachineCoords->sGenerator.width;
					ys = MachineCoords->sGenerator.height;
				}
				else 
				{
					fb = Global::song().GetFreeFxBus();
					xs = MachineCoords->sEffect.width;
					ys = MachineCoords->sEffect.height;
				}
				x-=xs/2;
				y-=ys/2;
			}
			else
			{
				if (mac >= MAX_BUSES && selectedMode != modegen)
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					fb = mac;
					xs = MachineCoords->sEffect.width;
					ys = MachineCoords->sEffect.height;
				}
				else if (mac < MAX_BUSES && selectedMode == modegen)
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					fb = mac;
					xs = MachineCoords->sGenerator.width;
					ys = MachineCoords->sGenerator.height;
				}
				else
				{
					MessageBox("Wrong Class of Machine!");
					return;
				}
			}

			if ( fb == -1)
			{
				MessageBox("Machine Creation Failed","Error!",MB_OK);
			}
			else
			{
				// Get info of old machine and close any open gui.
				if (Global::song()._pMachine[fb])
				{
					x = Global::song()._pMachine[fb]->_x;
					y = Global::song()._pMachine[fb]->_y;
					pParentMain->CloseMacGui(fb);
				}
				else if ((x < 0) || (y < 0))
				{
					 // random position
					bool bCovered = TRUE;
					while (bCovered)
					{
						x = (rand())%(CW-xs);
						y = (rand())%(CH-ys);
						bCovered = FALSE;
						for (int i=0; i < MAX_MACHINES; i++)
						{
							if (Global::song()._pMachine[i])
							{
								if ((abs(Global::song()._pMachine[i]->_x - x) < 32) &&
									(abs(Global::song()._pMachine[i]->_y - y) < 32))
								{
									bCovered = TRUE;
									i = MAX_MACHINES;
								}
							}
						}
					}
				}

				bool created=false;
				if (insert) {            
					CExclusiveLock lock(&Global::song().semaphore, 2, true);
					Global::song()._pMachine[fb] = insert;
					insert->_x = x;
					insert->_y = y;
					insert->_macIndex = fb;
					created = true;            
				}
				else if (Global::song()._pMachine[fb])
				{
					created = Global::song().ReplaceMachine(Global::song()._pMachine[fb],(MachineType)Outputmachine, x, y, psOutputDll.c_str(),fb,shellIdx);
				}
				else 
				{
					created = Global::song().CreateMachine((MachineType)Outputmachine, x, y, psOutputDll.c_str(),fb,shellIdx);
				}
				if (created)
				{
					ParamTranslator param;
					char name[255];
					strcpy(name,_pSong._pMachine[fb]->GetDllName());
					if (strcmp(name,"") == 0) { strcpy(name,_pSong._pMachine[fb]->GetName()); }
					PresetIO::LoadDefaultMap(PsycleGlobal::conf().GetAbsolutePresetsDir() + "/" + name + ".map",param);
					_pSong._pMachine[fb]->set_virtual_param_map(param);
				
					if (selectedMode == modegen)
					{
						Global::song().seqBus = fb;
					}

					// make sure that no 2 machines have the same name, because that is irritating

					int number = 1;
					char buf[sizeof(_pSong._pMachine[fb]->_editName)+4];
					strcpy (buf,_pSong._pMachine[fb]->_editName);

					for (int i = 0; i < MAX_MACHINES-1; i++)
					{
						if (i!=fb && _pSong._pMachine[i] && strcmp(_pSong._pMachine[i]->_editName,buf)==0)
						{
							number++;
							sprintf(buf,"%s %d",_pSong._pMachine[fb]->_editName,number);
							i = -1;
						}
					}

					buf[sizeof(_pSong._pMachine[fb]->_editName)-1] = 0;
					strncpy(_pSong._pMachine[fb]->_editName,buf,sizeof(_pSong._pMachine[fb]->_editName)-1);

					pParentMain->UpdateComboGen();
					Repaint(draw_modes::all);
        
					//Repaint(draw_modes::all_machines); // Seems that this doesn't always work (multiple calls to Repaint?)
				}
				else MessageBox("Machine Creation Failed","Error!",MB_OK);
			}
		}

		/// Show new machine dialog
		void CChildView::NewMachine(int x, int y, int mac) 
		{
			CNewMachine dlg(this);
			if(mac >= 0)
			{
				if (mac < MAX_BUSES)
				{
					dlg.selectedMode = modegen;
				}
				else
				{
					dlg.selectedMode = modefx;
				}
			}
			if ((dlg.DoModal() == IDOK) && (dlg.Outputmachine >= 0))
			{
				CreateNewMachine(x, y, mac, dlg.selectedMode, dlg.Outputmachine, dlg.psOutputDll, dlg.shellIdx);
			}
			//Repaint();
			pParentMain->RedrawGearRackList();
		}
		void CChildView::OnFullScreen()
		{
			if (maxView) 
			{
				maxView = false;
				pParentMain->ShowControlBar(&pParentMain->m_seqBar,TRUE,FALSE);
				pParentMain->ShowControlBar(&pParentMain->m_songBar,TRUE,FALSE);
				pParentMain->ShowControlBar(&pParentMain->m_wndToolBar,TRUE,FALSE);
			} 
			else
			{			
				maxView = true;
				pParentMain->ShowControlBar(&pParentMain->m_seqBar,FALSE,FALSE);
				pParentMain->ShowControlBar(&pParentMain->m_songBar,FALSE,FALSE);
				pParentMain->ShowControlBar(&pParentMain->m_wndToolBar,FALSE,FALSE);
				pParentMain->ShowWindow(SW_MAXIMIZE);
			}
		}
		void CChildView::OnUpdateFullScreen(CCmdUI* pCmdUI) 
		{
			pCmdUI->SetCheck(maxView?TRUE:FALSE);
		}

		void CChildView::OnConfigurationSettings() 
		{
			CConfigDlg dlg("Psycle Settings");
			if (dlg.DoModal() == IDOK)
			{
				PsycleGlobal::conf().RefreshSettings();
				RecalculateColourGrid();
				RecalcMetrics();
				InitTimer();
			}
			Repaint();
		}

		void CChildView::OnHelpSaludos() 
		{
			CGreetDialog dlg;
			dlg.DoModal();
		}

		void CChildView::ShowSwingFillDlg(bool bTrackMode)
		{
			int st = Global::song().BeatsPerMin();
			static int sw = 2;
			static float sv = 13.0f;
			static float sp = -90.0f;
			static BOOL of = true;
			CSwingFillDlg dlg;
			dlg.tempo = st;
			dlg.width = sw;
			dlg.variance = sv;
			dlg.phase = sp;
			dlg.offset = true;
		
			dlg.DoModal();
			if (dlg.bGo)
			{
				st = dlg.tempo;
				sw = dlg.width;
				sv = dlg.variance;
				sp = dlg.phase;
				of = dlg.offset;
				float var = (sv/100.0f);
				const float twopi = 2.0f*helpers::math::pi_f;
				// time to do our fill
				// first some math
				// our range has to go from spd+var to spd-var and back in width+1 lines
				float step = twopi/(sw);
				float index = sp*twopi/360;

				int l;
				int x;
				int y;
				int ny;
				if (bTrackMode)
				{
					x = editcur.track;
					y = 0;
					ny = _pSong.patternLines[_ps()];
				}
				else
				{
					x = blockSel.start.track;
					y = blockSel.start.line;
					ny = 1+blockSel.end.line-blockSel.start.line;
				}

				// remember we are at each speed for the length of time it takes to do one tick
				// this approximately calculates the offset
				float dcoffs = 0;
				if (of)
				{
					float swing=0;
					for (l=0;l<sw;l++)
					{
						float val = ((sinf(index)*var*st)+st);
						swing += (val/st)*(val/st);
						index+=step;
					}
					dcoffs = ((swing-sw)*st)/sw;
				}

				// now fill the pattern
				unsigned char *base = _ppattern();
				if (base)
				{
					PsycleGlobal::inputHandler().AddUndo(_ps(),x,y,1,ny,editcur.track,editcur.line,editcur.col,editPosition);
					for (l=y;l<y+ny;l++)
					{
						int const displace=x*EVENT_SIZE+l*MULTIPLY;
						
						unsigned char *offset=base+displace;
						
						PatternEntry *entry = reinterpret_cast<PatternEntry*>(offset);
						entry->_cmd = 0xff;
						int val = helpers::math::round<int,float>(((sinf(index)*var*st)+st)+dcoffs);//-0x20; // ***** proposed change to ffxx command to allow more useable range since the tempo bar only uses this range anyway...
						if (val < 1)
						{
							val = 1;
						}
						else if (val > 255)
						{
							val = 255;
						}
						entry->_parameter = unsigned char (val);
						index+=step;
					}
					NewPatternDraw(x,x,y,y+ny);	
					Repaint(draw_modes::data);
				}
			}
		}

		//// Right Click Popup Menu
		void CChildView::OnPopCut() { CopyBlock(true); }

		void CChildView::OnUpdateCutCopy(CCmdUI* pCmdUI) 
		{
			if (blockSelected && (viewMode == view_modes::pattern)) pCmdUI->Enable(TRUE);
			else pCmdUI->Enable(FALSE);
		}

		void CChildView::OnPopCopy() { CopyBlock(false); }

		void CChildView::OnPopPaste() { PasteBlock(editcur.track,editcur.line,false); }
		void CChildView::OnUpdatePaste(CCmdUI* pCmdUI) 
		{
			if (isBlockCopied && (viewMode == view_modes::pattern)) pCmdUI->Enable(TRUE);
			else  pCmdUI->Enable(FALSE);
		}

		void CChildView::OnPopMixpaste() { PasteBlock(editcur.track,editcur.line,true); }

		void CChildView::OnPopDelete() { DeleteBlock(); }
		
		void CChildView::OnPopAddNewTrack() 
		{
			int pattern = _pSong.playOrder[editPosition];
			if (MessageBox("Do you want to add the track to all patterns?","Add pattern track",MB_YESNO) == IDYES )
			{
				pattern = -1;
			}

			_pSong.AddNewTrack(pattern, editcur.track);
			pParentMain->PsybarsUpdate();
			RecalculateColourGrid();
			Repaint();
		}


		void CChildView::OnPopInterpolate() { BlockParamInterpolate(); }

		void CChildView::OnPopInterpolateCurve()
		{
			CInterpolateCurve dlg(blockSel.start.line,blockSel.end.line,_pSong.LinesPerBeat());
			
			int* valuearray = new int[blockSel.end.line-blockSel.start.line+1];
			int ps=_pSong.playOrder[editPosition];
			unsigned char notecommand = notecommands::empty;
			unsigned char targetmac = 255;
			unsigned char targettwk = 255;
			for (int i=0; i<=blockSel.end.line-blockSel.start.line; i++)
			{
				unsigned char *offset_target=_ptrackline(ps,blockSel.start.track,i+blockSel.start.line);
				if (*offset_target <= notecommands::release || *offset_target == notecommands::empty)
				{
					if ( *(offset_target+3) == 0 && *(offset_target+4) == 0 ) valuearray[i]=-1;
					else {
						targettwk = *(offset_target+3);
						valuearray[i]= *(offset_target+3)*0x100 + *(offset_target+4);
					}
				}
				else {
					notecommand = *offset_target;
					targetmac = *(offset_target+2);
					targettwk = *(offset_target+1);
					valuearray[i] = *(offset_target+3)*0x100 + *(offset_target+4);
				}
			}
			if ( notecommand == notecommands::tweak ) {
				int min=0, max=0xFFFF;
				if(targetmac < MAX_MACHINES && _pSong._pMachine[targetmac] != NULL) {
					Machine* pmac=_pSong._pMachine[targetmac];
					pmac->GetParamRange(pmac->translate_param(targettwk),min,max);
				}
				//If the parameter uses negative number, the values are shifted up.
				max-=min;
				max&=0xFFFF;
				min=0;
				dlg.AssignInitialValues(valuearray,0,min,max);
			}
			else if ( notecommand == notecommands::tweakslide ) {
				int min=0, max=0xFFFF;
				if(targetmac < MAX_MACHINES && _pSong._pMachine[targetmac] != NULL) {
					Machine* pmac=_pSong._pMachine[targetmac];
					pmac->GetParamRange(pmac->translate_param(targettwk),min,max);
				}
				//If the parameter uses negative number, the values are shifted up.
				max-=min;
				max&=0xFFFF;
				min=0;
				dlg.AssignInitialValues(valuearray,1,min,max);
			}
			else if ( notecommand == notecommands::midicc ) dlg.AssignInitialValues(valuearray,2,0,0x7F);
			else {
				dlg.AssignInitialValues(valuearray,-1, targettwk*0x100,targettwk*0x100 +0xFF);
			}

			if (dlg.DoModal() == IDOK )
			{
				int twktype(255);
				if ( dlg.kftwk == 0 ) twktype = notecommands::tweak;
				else if ( dlg.kftwk == 1 ) twktype = notecommands::tweakslide;
				else if ( dlg.kftwk == 2 ) twktype = notecommands::midicc;
				BlockParamInterpolate(dlg.kfresult,twktype);
			}
			delete[] valuearray;
		}


		void CChildView::OnPopChangegenerator() { BlockGenChange(_pSong.seqBus); }

		void CChildView::OnPopChangeinstrument() { BlockInsChange(_pSong.auxcolSelected); }

		void CChildView::OnPopTranspose1() { BlockTranspose(1); }

		void CChildView::OnPopTranspose12() { BlockTranspose(12); }

		void CChildView::OnPopTranspose_1() { BlockTranspose(-1); }

		void CChildView::OnPopTranspose_12() { BlockTranspose(-12); }

		void CChildView::OnPopTransformpattern() 
		{
			ShowTransformPatternDlg();			
		}

		void CChildView::OnPopPattenproperties() 
		{
			ShowPatternDlg();
		}

		/// fill block
		void CChildView::OnPopBlockSwingfill()
		{
			ShowSwingFillDlg(FALSE);
		}

		/// fill track
		void CChildView::OnPopTrackSwingfill()
		{
			ShowSwingFillDlg(TRUE);
		}

		void CChildView::OnEditSearchReplace()
		{
			ShowTransformPatternDlg();			
		}
		void CChildView::OnUpdateUndo(CCmdUI* pCmdUI)
		{
			if(PsycleGlobal::inputHandler().HasUndo(viewMode))
			{
				pCmdUI->Enable(TRUE);
			}
			else
			{
				pCmdUI->Enable(FALSE);
			}
		}

		void CChildView::OnUpdateRedo(CCmdUI* pCmdUI)
		{
			if(PsycleGlobal::inputHandler().HasRedo(viewMode))
			{
				pCmdUI->Enable(TRUE);
			}
			else
			{
				pCmdUI->Enable(FALSE);
			}
		}

		void CChildView::OnUpdatePatternCutCopy(CCmdUI* pCmdUI) 
		{
			if(viewMode == view_modes::pattern) pCmdUI->Enable(TRUE);
			else pCmdUI->Enable(FALSE);
		}

		void CChildView::OnUpdatePatternPaste(CCmdUI* pCmdUI) 
		{
			if(patBufferCopy&&(viewMode == view_modes::pattern)) pCmdUI->Enable(TRUE);
			else pCmdUI->Enable(FALSE);
		}

		void CChildView::AppendToRecent(std::string const& fName)
		{
			int iCount;
			char* nameBuff;
			UINT nameSize;
			UINT ids[] =
				{
					ID_FILE_RECENT_01,
					ID_FILE_RECENT_02,
					ID_FILE_RECENT_03,
					ID_FILE_RECENT_04
				};
			// Remove initial empty element, if present.
			if(GetMenuItemID(hRecentMenu, 0) == ID_FILE_RECENT_NONE)
			{
				DeleteMenu(hRecentMenu, 0, MF_BYPOSITION);
			}
			// Check for duplicates and eventually remove.
			for(iCount = GetMenuItemCount(hRecentMenu)-1; iCount >=0; iCount--)
			{
				nameSize = GetMenuString(hRecentMenu, iCount, 0, 0, MF_BYPOSITION) + 1;
				nameBuff = new char[nameSize];
				GetMenuString(hRecentMenu, iCount, nameBuff, nameSize, MF_BYPOSITION);
				if ( !strcmp(nameBuff, fName.c_str()) )
				{
					DeleteMenu(hRecentMenu, iCount, MF_BYPOSITION);
				}
				delete[] nameBuff;
			}
			// Ensure menu size doesn't exceed 4 positions.
			if (GetMenuItemCount(hRecentMenu) == 4)
			{
				DeleteMenu(hRecentMenu, 4-1, MF_BYPOSITION);
			}
			::MENUITEMINFO hNewItemInfo, hTempItemInfo;
			hNewItemInfo.cbSize		= sizeof(MENUITEMINFO);
			hNewItemInfo.fMask		= MIIM_ID | MIIM_TYPE;
			hNewItemInfo.fType		= MFT_STRING;
			hNewItemInfo.wID		= ids[0];
			hNewItemInfo.cch		= (UINT)fName.length();
			hNewItemInfo.dwTypeData = (LPSTR)fName.c_str();
			InsertMenuItem(hRecentMenu, 0, TRUE, &hNewItemInfo);
			// Update identifiers.
			for(iCount = 1;iCount < GetMenuItemCount(hRecentMenu);iCount++)
			{
				hTempItemInfo.cbSize	= sizeof(MENUITEMINFO);
				hTempItemInfo.fMask		= MIIM_ID;
				hTempItemInfo.wID		= ids[iCount];
				SetMenuItemInfo(hRecentMenu, iCount, true, &hTempItemInfo);
			}
			PsycleGlobal::conf().AddRecentFile(fName);
		}
		void CChildView::RestoreRecent()
		{
			const std::vector<std::string> recent = PsycleGlobal::conf().GetRecentFiles();
			UINT ids[] =
				{
					ID_FILE_RECENT_01,
					ID_FILE_RECENT_02,
					ID_FILE_RECENT_03,
					ID_FILE_RECENT_04
				};
			for(int iCount = GetMenuItemCount(hRecentMenu)-1; iCount>=0;iCount--)
			{
				DeleteMenu(hRecentMenu, iCount, MF_BYPOSITION);
			}
			for(int iCount = static_cast<int>(recent.size())-1; iCount>= 0;iCount--)
			{
				::MENUITEMINFO hNewItemInfo;
				hNewItemInfo.cbSize		= sizeof(MENUITEMINFO);
				hNewItemInfo.fMask		= MIIM_ID | MIIM_TYPE;
				hNewItemInfo.fType		= MFT_STRING;
				hNewItemInfo.wID		= ids[iCount];
				hNewItemInfo.cch		= (UINT)recent[iCount].length();
				hNewItemInfo.dwTypeData = (LPSTR)recent[iCount].c_str();
				InsertMenuItem(hRecentMenu, 0, TRUE, &hNewItemInfo);
			}
		}

		void CChildView::OnFileRecent_01() { CallOpenRecent(0); }
		void CChildView::OnFileRecent_02() { CallOpenRecent(1); }
		void CChildView::OnFileRecent_03() { CallOpenRecent(2); }
		void CChildView::OnFileRecent_04() { CallOpenRecent(3); }

		void CChildView::ImportPatternBlock(const std::string& fName, bool newpattern/*=false*/)
		{
			if (newpattern && _pSong.playLength<(MAX_SONG_POSITIONS-1))
			{
				FILE* hFile=fopen(fName.c_str(),"rb");
				PsycleGlobal::inputHandler().AddUndoSequence(_pSong.playLength,editcur.track,editcur.line,editcur.col,editPosition);
				++_pSong.playLength;

				editPosition++;
				int const pop=editPosition;
				for(int c=(_pSong.playLength-1);c>=pop;c--)
				{
					_pSong.playOrder[c]=_pSong.playOrder[c-1];
				}
				_pSong.playOrder[editPosition]=_pSong.GetBlankPatternUnused();
				
				if ( _pSong.playOrder[editPosition]>= MAX_PATTERNS )
				{
					_pSong.playOrder[editPosition]=MAX_PATTERNS-1;
				}

				_pSong.AllocNewPattern(_pSong.playOrder[editPosition],"",
					Global::configuration().GetDefaultPatLines(),FALSE);

				LoadBlock(hFile,_pSong.playOrder[editPosition]);
				fclose(hFile);

			}
			else {
				FILE* hFile=fopen(fName.c_str(),"rb");
				LoadBlock(hFile);
				fclose(hFile);
			}
			pParentMain->UpdatePlayOrder(true);
			pParentMain->UpdateSequencer(editPosition);

			Repaint(draw_modes::pattern);

		}

		void CChildView::ExportPatternBlock(const std::string& name) 
		{
			FILE* hFile=fopen(name.c_str(),"wb");
			SaveBlock(hFile);
			fflush(hFile);
			fclose(hFile);
		}
		void CChildView::FileLoadsongNamed(const std::string& fName)
		{
		    ANOTIFY(Actions::songload);
			PsycleGlobal::inputHandler().KillUndo();
			PsycleGlobal::inputHandler().KillRedo();
			pParentMain->CloseAllMacGuis();
			Global::player().Stop();
			
			OldPsyFile* file;
			const char* szExtension = strrchr(fName.c_str(), '.')+1;
			if (szExtension == NULL) {
				MessageBox("Could not Open file. Check that it uses a supported extension/format.", "Loading Error", MB_OK);
				return;
			}
			else {
				if (!_strnicmp(szExtension, "psy",3)) {
					file = new OldPsyFile();
				}
				else if (!_strnicmp(szExtension, "xm",2)) {
					file = new XMSongLoader();
				}
				else if (!_strnicmp(szExtension, "it",2)) {
					file = new ITModule2();
				}
				else if (!_strnicmp(szExtension, "s3m",3)) {
					file = new ITModule2();
				}
				else if (!_strnicmp(szExtension, "mod",3)) {
					file = new MODSongLoader();
				}
				else {
					file = new MODSongLoader();
					if (!file->Open(fName.c_str())) {
						MessageBox("Could not Open file. Check that the location is correct.", "Loading Error", MB_OK);
						delete file;
						return;
					}
					bool valid = dynamic_cast<MODSongLoader*>(file)->IsValid();
					file->Close();
					if (!valid) {
						delete file;
						MessageBox("Could not Open file. Check that it uses a supported extension/format.", "Loading Error", MB_OK);
						return;
					}
				}
			}
			if (!file->Open(fName.c_str())) {
				MessageBox("Could not Open file. Check that the location is correct.", "Loading Error", MB_OK);
				delete file;
				return;
			}
			CProgressDialog progress;
			progress.ShowWindow(SW_SHOW);
			_pSong.New();
			_pSong._pMachine[MASTER_INDEX]->_x = (CW - PsycleGlobal::conf().macView().MachineCoords.sMaster.width) / 2;
			_pSong._pMachine[MASTER_INDEX]->_y = (CH - PsycleGlobal::conf().macView().MachineCoords.sMaster.height) / 2;
			if(!file->Load(_pSong,progress))
			{
				file->Close();
				progress.SendMessage(WM_CLOSE);
				std::ostringstream s;
				s << "Error reading from file '" << file->szName << "'" << std::endl;
				MessageBox(s.str().c_str(), "File Error!!!", 0);
				_pSong.New();
				_pSong._pMachine[MASTER_INDEX]->_x = (CW - PsycleGlobal::conf().macView().MachineCoords.sMaster.width) / 2;
				_pSong._pMachine[MASTER_INDEX]->_y = (CH - PsycleGlobal::conf().macView().MachineCoords.sMaster.height) / 2;
			}
			else {
				file->Close();
				progress.SendMessage(WM_CLOSE);
				AppendToRecent(fName);
				std::string::size_type index = fName.rfind('\\');
				if (index != std::string::npos) {
					PsycleGlobal::conf().SetCurrentSongDir(fName.substr(0,index));
					_pSong.fileName = fName.substr(index+1);
				}
				else {
					_pSong.fileName = fName;
				}
				if (_pSong.fileName.rfind(".psy") == std::string::npos) {
					_pSong.fileName += ".psy";
				}
			}

			editPosition = 0;
			Global::player().SetBPM(Global::song().BeatsPerMin(), Global::song().LinesPerBeat(),Global::song().ExtraTicksPerLine());
			EnforceAllMachinesOnView();
			pParentMain->PsybarsUpdate();
			pParentMain->WaveEditorBackUpdate();
			pParentMain->UpdateInstrumentEditor();
			pParentMain->RedrawGearRackList();
			pParentMain->UpdateSequencer();
			pParentMain->UpdatePlayOrder(false);
			//pParentMain->UpdateComboIns(); PsyBarsUpdate calls UpdateComboGen that also calls UpdatecomboIns
			RecalculateColourGrid();
			Repaint();
			SetTitleBarText();
			if (PsycleGlobal::conf().bShowSongInfoOnLoad)
			{
				CSongpDlg dlg(Global::song());
				dlg.SetReadOnly();
				dlg.DoModal();
			}
			delete file;
			ANOTIFY(Actions::songloaded);
		}


		void CChildView::CallOpenRecent(int pos)
		{
			UINT nameSize;
			nameSize = GetMenuString(hRecentMenu, pos, 0, 0, MF_BYPOSITION) + 1;
			char* nameBuff = new char[nameSize];
			GetMenuString(hRecentMenu, pos, nameBuff, nameSize, MF_BYPOSITION);
			FileLoadsongNamed(nameBuff);
			delete[] nameBuff; nameBuff = 0;
		}

		void CChildView::SetTitleBarText()
		{
			std::string titlename = "[";
			titlename+=Global::song().fileName;
			if(PsycleGlobal::inputHandler().IsModified())
			{
				titlename+=" *";
			}
			// don't know how to access to the IDR_MAINFRAME String Title.
			titlename += "] Psycle Modular Music Creation Studio (" PSYCLE__VERSION ")";
			pParentMain->SetWindowText(titlename.c_str());
		}

		void CChildView::OnHelpKeybtxt() 
		{
			char path[MAX_PATH];
			sprintf(path,"%sdocs\\keys.txt",PsycleGlobal::conf().appPath().c_str());
			ShellExecute(pParentMain->m_hWnd,"open",path,NULL,"",SW_SHOW);
		}

		void CChildView::OnHelpReadme() 
		{
			char path[MAX_PATH];
			sprintf(path,"%sdocs\\readme.txt",PsycleGlobal::conf().appPath().c_str());
			ShellExecute(pParentMain->m_hWnd,"open",path,NULL,"",SW_SHOW);
		}

		void CChildView::OnHelpTweaking() 
		{
			char path[MAX_PATH];
			sprintf(path,"%sdocs\\tweaking.txt",PsycleGlobal::conf().appPath().c_str());
			ShellExecute(pParentMain->m_hWnd,"open",path,NULL,"",SW_SHOW);
		}

		void CChildView::OnHelpWhatsnew() 
		{
			char path[MAX_PATH];
			sprintf(path,"%sdocs\\whatsnew.txt",PsycleGlobal::conf().appPath().c_str());
			ShellExecute(pParentMain->m_hWnd,"open",path,NULL,"",SW_SHOW);
		}
		void CChildView::OnHelpLuaScript()
		{
			char path[MAX_PATH];
			sprintf(path,"%sdocs\\LuaScriptingManual.pdf",PsycleGlobal::conf().appPath().c_str());
			ShellExecute(pParentMain->m_hWnd,"open",path,NULL,"",SW_SHOW);
		}


		void CChildView::EnforceAllMachinesOnView()
		{
			SMachineCoords mcoords = PsycleGlobal::conf().macView().MachineCoords;
			for(int i(0);i<MAX_MACHINES;i++)
			{
				if(_pSong._pMachine[i])
				{
					int x = _pSong._pMachine[i]->_x;
					int y = _pSong._pMachine[i]->_y;
					switch (_pSong._pMachine[i]->_mode)
					{
					case MACHMODE_GENERATOR:
						if ( x > CW-mcoords.sGenerator.width ) 
						{
							x = CW-mcoords.sGenerator.width;
						}
						if ( y > CH-mcoords.sGenerator.height ) 
						{
							y = CH-mcoords.sGenerator.height;
						}
						break;
					case MACHMODE_FX:
						if ( x > CW-mcoords.sEffect.width )
						{
							x = CW-mcoords.sEffect.width;
						}
						if ( y > CH-mcoords.sEffect.height ) 
						{
							y = CH-mcoords.sEffect.height;
						}
						break;

					case MACHMODE_MASTER:
						if ( x > CW-mcoords.sMaster.width ) 
						{
							x = CW-mcoords.sMaster.width;
						}
						if ( y > CH-mcoords.sMaster.height )
						{
							y = CH-mcoords.sMaster.height;
						}
						break;
					}
					_pSong._pMachine[i]->_x = x;
					_pSong._pMachine[i]->_y = y;
				}
			}
		}

		void CChildView::RecalcMetrics()
		{
			if (patView->draw_empty_data)
			{
				strcpy(szBlankParam,".");
				patView->notes_tab_a220[notecommands::empty]="---";
				patView->notes_tab_a440[notecommands::empty]="---";
			}
			else
			{
				strcpy(szBlankParam," ");
				patView->notes_tab_a220[notecommands::empty]="   ";
				patView->notes_tab_a440[notecommands::empty]="   ";
			}
			note_tab_selected = patView->showA440 ? patView->notes_tab_a440 : patView->notes_tab_a220;

			TEXTHEIGHT = patView->font_y;
			ROWHEIGHT = TEXTHEIGHT+1;
			TEXTWIDTH = patView->font_x;
			for (int c=0; c<256; c++)	
			{ 
				FLATSIZES[c]=patView->font_x; 
			}
			COLX[0] = 0;
			COLX[1] = (TEXTWIDTH*3)+2;
			COLX[2] = COLX[1]+TEXTWIDTH;
			COLX[3] = COLX[2]+TEXTWIDTH+1;
			COLX[4] = COLX[3]+TEXTWIDTH;
			COLX[5] = COLX[4]+TEXTWIDTH+1;
			COLX[6] = COLX[5]+TEXTWIDTH;
			COLX[7] = COLX[6]+TEXTWIDTH;
			COLX[8] = COLX[7]+TEXTWIDTH;
			COLX[9] = COLX[8]+TEXTWIDTH+1;
			ROWWIDTH = COLX[9];
			int HEADER_HEIGHT = PatHeaderCoords->sBackground.height+2;
			int HEADER_ROWWIDTH  = PatHeaderCoords->sBackground.width+1;

			if (ROWWIDTH < HEADER_ROWWIDTH)
			{
				int temp = (HEADER_ROWWIDTH-ROWWIDTH)/2;
				ROWWIDTH = HEADER_ROWWIDTH;
				for (int i = 0; i < 10; i++)
				{
					COLX[i] += temp;
				}
			}
			HEADER_INDENT = (ROWWIDTH - HEADER_ROWWIDTH)/2;
			if (patView->_linenumbers)
			{
				XOFFSET = (4*TEXTWIDTH);
				YOFFSET = TEXTHEIGHT+2;
				if (YOFFSET < HEADER_HEIGHT) {
					YOFFSET = HEADER_HEIGHT;
				}
			}
			else
			{
				XOFFSET = 1;
				YOFFSET = HEADER_HEIGHT;
			}
			VISTRACKS = (CW-XOFFSET)/ROWWIDTH;
			VISLINES = (CH-YOFFSET)/ROWHEIGHT;
			if (VISLINES < 1) 
			{ 
				VISLINES = 1; 
			}
			if (VISTRACKS < 1) 
			{ 
				VISTRACKS = 1; 
			}
			triangle_size_tall = macView->triangle_size+((23*macView->wirewidth)/16);

			triangle_size_center = triangle_size_tall/2;
			triangle_size_wide = triangle_size_tall/2;
			triangle_size_indent = triangle_size_tall/6;
		}


		void CChildView::TransparentBlt
			(
				CDC* pDC,
				int xDest,  int yDest,
				int wWidth,  int wHeight,
				CDC* pSkinDC,
				CBitmap* bmpMask,
				int xSource, // = 0
				int ySource // = 0
			)
		{
			// We are going to paint the two DDB's in sequence to the destination.
			// 1st the monochrome bitmap will be blitted using an AND operation to
			// cut a hole in the destination. The color image will then be ORed
			// with the destination, filling it into the hole, but leaving the
			// surrounding area untouched.
			CDC hdcMem;
			hdcMem.CreateCompatibleDC(pDC);
			CBitmap* hbmT = hdcMem.SelectObject(bmpMask);
			pDC->SetTextColor(RGB(0,0,0));
			pDC->SetBkColor(RGB(255,255,255));
			if (!pDC->BitBlt(xDest, yDest, wWidth, wHeight, &hdcMem, xSource, ySource, 
				SRCAND))
			{
				TRACE("Transparent Blit failure SRCAND\n");
			}
			// Also note the use of SRCPAINT rather than SRCCOPY.
			if (!pDC->BitBlt(xDest, yDest, wWidth, wHeight, pSkinDC, xSource, ySource,
				SRCPAINT))
			{
				TRACE("Transparent Blit failure SRCPAINT\n");
			}
			// Now, clean up.
			hdcMem.SelectObject(hbmT);
			hdcMem.DeleteDC();
		}

		void CChildView::patTrackMute()
		{
			if (viewMode == view_modes::pattern)
			{
				_pSong._trackMuted[editcur.track] = !_pSong._trackMuted[editcur.track];
				Repaint(draw_modes::track_header);
			}
		}

		void CChildView::patTrackSolo()
		{
			if (viewMode == view_modes::pattern)
			{
				if (_pSong._trackSoloed == editcur.track)
				{
					for (int i = 0; i < MAX_TRACKS; i++)
					{
						_pSong._trackMuted[i] = FALSE;
					}
					_pSong._trackSoloed = -1;
				}
				else
				{
					for (int i = 0; i < MAX_TRACKS; i++)
					{
						_pSong._trackMuted[i] = TRUE;
					}
					_pSong._trackMuted[editcur.track] = FALSE;
					_pSong._trackSoloed = editcur.track;
				}
				Repaint(draw_modes::track_header);
			}
		}

		void CChildView::patTrackRecord()
		{
			if (viewMode == view_modes::pattern)
			{
				_pSong.ToggleTrackArmed(editcur.track);
				Repaint(draw_modes::track_header);
			}
		}

		void CChildView::DoMacPropDialog(int propMac)
		{
			if((propMac < 0 ) || (propMac >= MAX_MACHINES-1)) return;
			CMacProp dlg(_pSong);
			dlg.m_view=this;
			dlg.pMachine = _pSong._pMachine[propMac];
			dlg.thisMac = propMac;
			if(dlg.DoModal() == IDOK)
			{
				strncpy(dlg.pMachine->_editName, dlg.txt,sizeof(dlg.pMachine->_editName)-1);
				pParentMain->StatusBarText(dlg.txt);
				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (pParentMain->pGearRackDialog)
				{
					pParentMain->RedrawGearRackList();
				}
			}
	 		if(dlg.deleted)
			{
				pParentMain->CloseMacGui(propMac);
				{
					CExclusiveLock lock(&Global::song().semaphore, 2, true);
					Global::song().DeleteMachineRewiring(propMac);
				}

				pParentMain->UpdateEnvInfo();
				pParentMain->UpdateComboGen();
				if (pParentMain->pGearRackDialog)
				{
					pParentMain->RedrawGearRackList();
				}
			}
		}

		void CChildView::OnConfigurationLoopplayback() 
		{
			Global::player()._loopSong = !Global::player()._loopSong;
		}

		void CChildView::OnUpdateConfigurationLoopplayback(CCmdUI* pCmdUI) 
		{
			if (Global::player()._loopSong)
				pCmdUI->SetCheck(1);
			else
				pCmdUI->SetCheck(0);	
		}
    
    void CChildView::InitWindowMenu() {     
      extension_menu_handle_.InitWindowsMenu();        
    }

    void CChildView::LoadHostExtensions() {      
      HostExtensions::Instance().StartScript();
	  if (!HostExtensions::Instance().HasToolBarExtension()) {	   
		CMainFrame* pParentMain =(CMainFrame *)pParentFrame;	
		assert(pParentMain);
		pParentMain->ShowControlBar(&pParentMain->m_extensionBar, FALSE, FALSE);
	  }
    }

    void CChildView::OnAddViewMenu(Link& link) {
	  extension_menu_handle_.add_view_menu_item(link);     
    }

    void CChildView::OnRestoreViewMenu() { 
	  extension_menu_handle_.restore_view_menu();
    }

    void CChildView::OnAddHelpMenu(Link& link) {
	  extension_menu_handle_.add_help_menu_item(link);      
    }

    void CChildView::OnReplaceHelpMenu(Link& link, int pos) {
	  extension_menu_handle_.replace_help_menu_item(link, pos);     
    }
    
    void CChildView::OnAddWindowsMenu(class Link& link) {
	  extension_menu_handle_.add_windows_menu_item(link);      
    }   
    
    void CChildView::OnRemoveWindowsMenu(LuaPlugin* plugin) {
		extension_menu_handle_.remove_windows_menu_item(plugin);     
    }           

    void CChildView::OnChangeWindowsMenuText(LuaPlugin* plugin)
	{
		extension_menu_handle_.change_windows_menu_item_text(plugin);
    }

	void CChildView::ShowExtensionInToolbar(LuaPlugin& plugin)
	{
		((CMainFrame*)::AfxGetMainWnd())->m_extensionBar.Add(plugin);
	}

    void CChildView::OnHostViewportChange(LuaPlugin& plugin, int viewport) {           
		switch (viewport) {			
			case CHILDVIEWPORT:
				UpdateViewMode();				
				ShowScrollBar(SB_BOTH,FALSE);									
				m_luaWndView->Push(plugin);				
				m_luaWndView->DisplayTop();								
				m_luaWndView->UpdateMenu(extension_menu_handle_);
			break;
			case FRAMEVIEWPORT:
				if (m_luaWndView->IsTopDisplay(plugin)) {
					m_luaWndView->Pop();					
					m_luaWndView->DisplayTop();
					if (!plugin.has_error()) {
					  m_luaWndView->UpdateMenu(extension_menu_handle_);				
					  RestoreViewMode();
					}
				}								
			break;
		};	
    }

	void CChildView::UpdateViewMode()
	{
		if (viewMode != view_modes::extension) {
			oldViewMode = viewMode;
			viewMode = view_modes::extension;
		}		
	}

	void CChildView::RestoreViewMode()
	{
		if (!m_luaWndView->HasExtensions()) {				
			viewMode = oldViewMode;
			Repaint();			
		}		
	}	

	void CChildView::OnExecuteLink(Link& link) {
		extension_menu_handle_.ExecuteLink(link);
	}

    void CChildView::OnDynamicMenuItems(UINT nID) {
		extension_menu_handle_.HandleInput(nID);
    }                         
    
}}

// graphics operations, private headers included only by this translation unit
#include "MachineView.private.hpp"
#include "PatViewNew.private.hpp"
#include "SeqView.private.hpp"

// User/Mouse Responses, private headers included only by this translation unit
#include "KeybHandler.private.hpp"
#include "MouseHandler.private.hpp"
