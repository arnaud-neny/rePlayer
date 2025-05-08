///\file
///\brief implementation file for psycle::host::CFrameMachine.
#include <psycle/host/detail/project.private.hpp>
#include "FrameMachine.hpp"
#include "InputHandler.hpp"
#include "ChildView.hpp"
#include "Machine.hpp"
#include "NativeView.hpp"
#include "MixerFrameView.hpp"
#include "Plugin.hpp"
#include "vsthost24.hpp"
#include "ladspamachine.h"
#include "PresetsDlg.hpp"
#include "ParamList.hpp"
#include "ParamMap.hpp"
#include "LuaPlugin.hpp"
#include "Canvas.hpp"
#include "MfcUi.hpp"

#include <boost/filesystem/operations.hpp>

namespace psycle { namespace host {
		int const ID_TIMER_PARAM_REFRESH = 2104;
		extern CPsycleApp theApp;
    using namespace ui;

        
		//////////////////////////////////////////////////////////////////////////

		void CMyToolBar::OnBarStyleChange(DWORD dwOldStyle, DWORD dwNewStyle)
		{
			// Call base class implementation.
			CToolBar::OnBarStyleChange(dwOldStyle, dwNewStyle);

			// Use exclusive-or to detect changes in style bits.
			DWORD changed = dwOldStyle ^ dwNewStyle;

			if (changed & CBRS_FLOATING) {
				if (dwNewStyle & CBRS_FLOATING) {
					((CFrameMachine*)GetOwner())->ResizeWindow(NULL);
				}
				else {
					((CFrameMachine*)GetOwner())->ResizeWindow(NULL);
				}
			}
#if 0
			if (changed & CBRS_ORIENT_ANY) {
				if (dwNewStyle & CBRS_ORIENT_HORZ) {
					// ToolBar now horizontal
				}
				else if (dwNewStyle & CBRS_ORIENT_VERT) {
					// ToolBar now vertical            
				}
			}
#endif
		}

/*
		IMPLEMENT_DYNAMIC(CFrameMachine, CFrameWnd)

		BEGIN_MESSAGE_MAP(CFrameMachine, CFrameWnd)
			ON_WM_CREATE()
			ON_WM_CLOSE()
			ON_WM_DESTROY()
			ON_WM_TIMER()
			ON_WM_SETFOCUS()
			ON_WM_KEYDOWN()
			ON_WM_KEYUP()
			ON_WM_SIZING()
			ON_WM_INITMENUPOPUP()
			ON_COMMAND(ID_PROGRAMS_OPENPRESET, OnProgramsOpenpreset)
			ON_COMMAND(ID_PROGRAMS_SAVEPRESET, OnProgramsSavepreset)
			ON_COMMAND_RANGE(ID_SELECTBANK_0, ID_SELECTBANK_0+99, OnSetBank)
			ON_COMMAND_RANGE(ID_SELECTPROGRAM_0, ID_SELECTPROGRAM_0+199, OnSetProgram)			
			ON_COMMAND(ID_PROGRAMS_RANDOMIZEPROGRAM, OnProgramsRandomizeprogram)
			ON_COMMAND(ID_PROGRAMS_RESETDEFAULT, OnParametersResetparameters)
			ON_COMMAND(ID_OPERATIONS_ENABLED, OnOperationsEnabled)
			ON_UPDATE_COMMAND_UI(ID_OPERATIONS_ENABLED, OnUpdateOperationsEnabled)
			ON_COMMAND(ID_VIEWS_PARAMETERLIST, OnViewsParameterlist)
			ON_UPDATE_COMMAND_UI(ID_VIEWS_PARAMETERLIST, OnUpdateViewsParameterlist)
			ON_COMMAND(ID_VIEWS_PARAMETERMAP, OnViewsParameterMap)
			ON_UPDATE_COMMAND_UI(ID_VIEWS_PARAMETERMAP, OnUpdateViewsParameterMap)
			ON_COMMAND(ID_VIEWS_BANKMANAGER, OnViewsBankmanager)
			ON_COMMAND(ID_VIEWS_SHOWTOOLBAR, OnViewsShowtoolbar)
			ON_UPDATE_COMMAND_UI(ID_VIEWS_SHOWTOOLBAR, OnUpdateViewsShowtoolbar)
			ON_COMMAND(ID_MACHINE_COMMAND, OnParametersCommand)
			ON_UPDATE_COMMAND_UI(ID_MACHINE_COMMAND, OnUpdateParametersCommand)
			ON_COMMAND(ID_PARAMETERS_VIEWS_RELOAD, OnMachineReloadScript)
			ON_UPDATE_COMMAND_UI(ID_PARAMETERS_VIEWS_RELOAD, OnUpdateMachineReloadScript)
			ON_COMMAND(ID_ABOUT_ABOUTMAC, OnMachineAboutthismachine)
			ON_LBN_SELCHANGE(ID_COMBO_PRG, OnSelchangeProgram)
			ON_CBN_CLOSEUP(ID_COMBO_PRG, OnCloseupProgram)
			ON_COMMAND(ID_PROGRAMLESS, OnProgramLess)
			ON_UPDATE_COMMAND_UI(ID_PROGRAMLESS, OnUpdateProgramLess)
			ON_COMMAND(ID_PROGRAMMORE, OnProgramMore)
			ON_UPDATE_COMMAND_UI(ID_PROGRAMMORE, OnUpdateProgramMore)
		END_MESSAGE_MAP()
*/

		CFrameMachine::CFrameMachine(Machine* pMachine, CChildView* wndView_, CFrameMachine** windowVar_)
		: _machine(pMachine), wndView(wndView_), windowVar(windowVar_), pView(NULL) , pParamGui(0), pParamMapGui(0), refreshcounter(0), lastprogram(0),lastnumprogrs(0)
    //, barmenu(0)
    //, custom_menubar(0)
		{
			//Use OnCreate.
		}

		CFrameMachine::~CFrameMachine()
		{
			//Use OnDestroy
		}

		int CFrameMachine::OnCreate(LPCREATESTRUCT lpCreateStruct) 
		{
			if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
			{
				return -1;
			}
			pView = CreateView();
			if(!pView)
			{
				TRACE0("Failed to create view window\n");
				return -1;
			}
			if (_machine->_type == MACH_PLUGIN)
			{
				((Plugin*)_machine)->GetCallback()->hWnd = m_hWnd;

				GetMenu()->GetSubMenu(1)->ModifyMenu(5, MF_BYPOSITION | MF_STRING, ID_MACHINE_COMMAND, 
					((Plugin*)_machine)->GetInfo()->Command);
			}      			

			if (!toolBar.CreateEx(this, TBSTYLE_FLAT|/*TBSTYLE_LIST*|*/TBSTYLE_TRANSPARENT|TBSTYLE_TOOLTIPS|TBSTYLE_WRAPABLE) ||
				!toolBar.LoadToolBar(IDR_FRAMEMACHINE))
			{
				TRACE0("Failed to create toolbar\n");
				return -1;      // fail to create
			}
#if 0
			//nice, but will make the toolbar too big.
			toolBar.SetButtonText(0,"blabla");
			CRect temp;
			toolBar.GetItemRect(0,&temp);
			toolBar.GetToolBarCtrl().SetButtonSize(CSize(temp.Width(),
				temp.Height()));
#endif

			toolBar.SetBarStyle(toolBar.GetBarStyle() | CBRS_FLYBY | CBRS_GRIPPER);
			toolBar.SetWindowText("Params Toolbar");
			toolBar.EnableDocking(CBRS_ALIGN_TOP);
			CRect rect;
			int nIndex = toolBar.GetToolBarCtrl().CommandToIndex(ID_COMBO_PRG);
			toolBar.SetButtonInfo(nIndex, ID_COMBO_PRG, TBBS_SEPARATOR, 160);
			toolBar.GetToolBarCtrl().GetItemRect(nIndex, &rect);
			rect.top = 1;
			rect.bottom = rect.top + 400; //drop height
			if(!comboProgram.Create( WS_CHILD |  CBS_DROPDOWNLIST | WS_VISIBLE | CBS_AUTOHSCROLL 
				 | WS_VSCROLL, rect, &toolBar, ID_COMBO_PRG))
			{
				TRACE0("Failed to create combobox\n");
				return -1;      // fail to create
			}
			HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
			CFont font;
			font.Attach( hFont );
			comboProgram.SetFont(&font);

			LocatePresets();
			FillProgramCombobox();

			EnableDocking(CBRS_ALIGN_ANY);
			DockControlBar(&toolBar);
			LoadBarState(_T("VstParamToolbar"));

			// Sets Icon
			HICON tIcon;
			tIcon=theApp.LoadIcon(IDR_FRAMEMACHINE);
			SetIcon(tIcon, true);
			SetIcon(tIcon, false);
      if (machine()._type == MACH_LUA) {
			  SetTimer(ID_TIMER_PARAM_REFRESH,1,0);
      } else {
        SetTimer(ID_TIMER_PARAM_REFRESH,30,0);
      }
			return 0;
		}
    
		BOOL CFrameMachine::PreCreateWindow(CREATESTRUCT& cs)
		{
			if( !CFrameWnd::PreCreateWindow(cs) )
				return FALSE;
			
			cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
			return TRUE;
		}

		void CFrameMachine::OnClose() 
		{      
			KillTimer(ID_TIMER_PARAM_REFRESH);
			CFrameWnd::OnClose();
		}

		void CFrameMachine::OnDestroy()
		{    
			HICON _icon = GetIcon(false);
			DestroyIcon(_icon);
			comboProgram.DestroyWindow();
			if (pView != NULL) { 
			  pView->Close(_machine);
			  pView->DestroyWindow();
			  delete pView; 
			}
			if (pParamGui) pParamGui->SendMessage(WM_CLOSE);
      if (pParamMapGui) { 
        delete pParamMapGui;
      }
			if ( _machine->_type == MACH_PLUGIN)
			{
				((Plugin*)_machine)->GetCallback()->hWnd = NULL;
			}
			SaveBarState(_T("VstParamToolbar"));
		}

		void CFrameMachine::PostNcDestroy() 
		{
			if(windowVar!= NULL) *windowVar = NULL;
			CFrameWnd::PostNcDestroy();
		}

		void CFrameMachine::OnTimer(UINT_PTR nIDEvent) 
		{            
			if ( nIDEvent == ID_TIMER_PARAM_REFRESH )
			{        
        pView->WindowIdle();
				if (--refreshcounter <= 0) {
					int tmp = _machine->GetCurrentProgram();
					if (lastprogram != tmp) {
						if(lastnumprogrs != _machine->GetNumPrograms()) {
							lastnumprogrs = _machine->GetNumPrograms();
							FillProgramCombobox();
						}
						else {
							ChangeProgram(tmp);	
						}						
					}
					refreshcounter=30;
				}
			}
			CFrameWnd::OnTimer(nIDEvent);
		}

		void CFrameMachine::OnSetFocus(CWnd* pOldWnd) 
		{
			CFrameWnd::OnSetFocus(pOldWnd);
			pView->WindowIdle();
		}

		void CFrameMachine::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
		{
      /*if (_machine->_type == MACH_LUA) {
        LuaPlugin* lp = (LuaPlugin*) _machine;
        canvas::Canvas* user_view = lp->GetCanvas();
        if (user_view !=0 && lp->GetGuiType() == 1) { 
          UINT flags = 0;
          if (GetKeyState(VK_SHIFT) & 0x8000) {
            flags |= MK_SHIFT;
          }
          if (GetKeyState(VK_CONTROL) & 0x8000) {
            flags |= MK_CONTROL;
          }
          if (nFlags == 13) {
            flags |= MK_ALT;
          }
          canvas::Event ev(0,
                         canvas::Event::KEY_DOWN, 
                         0,
                         0,
                         nChar,
                         flags);
          LuaPlugin* lp = (LuaPlugin*) _machine;
          if (lp->OnEvent(&ev)) {
            return;
          }
        }
      }
      */
			// ignore repeats: nFlags&0x4000
			const BOOL bRepeat = nFlags&0x4000;
			CmdDef cmd(PsycleGlobal::inputHandler().KeyToCmd(nChar,nFlags));
			if(cmd.IsValid())
			{
				switch(cmd.GetType())
				{
				case CT_Note:
					if (!bRepeat)
					{
						///\todo: change the option: "notesToEffects" to mean "notesToWindowOwner".
						const int outnote = cmd.GetNote();
						if ( _machine->_mode == MACHMODE_GENERATOR || PsycleGlobal::conf().inputHandler()._notesToEffects)
							PsycleGlobal::inputHandler().PlayNote(outnote,255,127,true,_machine);
						else
							PsycleGlobal::inputHandler().PlayNote(outnote);
					}
					break;

				case CT_Immediate:
				case CT_Editor:
					PsycleGlobal::inputHandler().PerformCmd(cmd,bRepeat);
					break;
				}
			}
			else if(nChar == VK_ESCAPE) {
				PostMessage(WM_CLOSE);
			}
			this->SetFocus();

			CFrameWnd::OnKeyDown(nChar, nRepCnt, nFlags);	
		}

		void CFrameMachine::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
		{      
     /* if (_machine->_type == MACH_LUA) {
        LuaPlugin* lp = (LuaPlugin*) _machine;
        canvas::Canvas* user_view = lp->GetCanvas();
        if (user_view !=0 && lp->GetGuiType() == 1) { 
          UINT flags = 0;
          if (GetKeyState(VK_SHIFT) & 0x8000) {
            flags |= MK_SHIFT;
          }
          if (GetKeyState(VK_CONTROL) & 0x8000) {
            flags |= MK_CONTROL;
          }
          if (nFlags == 13) {
            flags |= MK_ALT;
          }
          canvas::Event ev(0,
                         canvas::Event::KEY_UP, 
                         0,
                         0,
                         nChar,
                         flags);
          LuaPlugin* lp = (LuaPlugin*) _machine;
          if (lp->OnEvent(&ev)) {
            return;
          }        
        }		    
      }*/

			CmdDef cmd(PsycleGlobal::inputHandler().KeyToCmd(nChar,nFlags));
			const int outnote = cmd.GetNote();
			if(outnote>=0)
			{
				if ( _machine->_mode == MACHMODE_GENERATOR ||PsycleGlobal::conf().inputHandler()._notesToEffects)
				{
					PsycleGlobal::inputHandler().StopNote(outnote,255,true,_machine);
				}
				else PsycleGlobal::inputHandler().StopNote(outnote);
			}

			CFrameWnd::OnKeyUp(nChar, nRepCnt, nFlags);
		}

		void CFrameMachine::OnSizing(UINT fwSide, LPRECT pRect)
		{
			pView->WindowIdle();
		}


		//////////////////////////////////////////////////////////////////////////
		// OnInitMenuPopup : called when a popup menu is initialized            //
		//////////////////////////////////////////////////////////////////////////

		void CFrameMachine::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) 
		{
			// if Effect Edit menu popping up
			if ((pPopupMenu->GetMenuItemCount() > 0) &&
				(pPopupMenu->GetMenuItemID(0) == ID_PROGRAMS_OPENPRESET))
			{
				FillBankPopup(pPopupMenu);
				FillProgramPopup(pPopupMenu);
			}

			CFrameWnd::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
		}

		void CFrameMachine::OnProgramsOpenpreset()
		{      
      if (_machine->_type == MACH_LUA && ((LuaPlugin*)_machine)->prsmode()==MachinePresetType::CHUNK) {
        char tmp[2048];			
			  CFileDialog dlg(TRUE,
				  "fxb",
				  NULL,
				  OFN_HIDEREADONLY | OFN_FILEMUSTEXIST|OFN_DONTADDTORECENT,
				  "Effect Bank Files (.fxb)|*.fxb|Effect Program Files (.fxp)|*.fxp|All Files|*.*||");
			  dlg.m_ofn.lpstrInitialDir = tmp;

			  if (dlg.DoModal() != IDOK)
				  return;

        LuaPlugin* plug = (LuaPlugin*) this->_machine;
			  if ( dlg.GetFileExt() == "fxb" )
			  {				
				  if (!plug->LoadBank((LPCSTR)(dlg.GetPathName()))) {        				
					  MessageBox("Error Loading file", NULL, MB_ICONERROR);
          }
			  }
			  else if ( dlg.GetFileExt() == "fxp" )
			  {
				  /*CFxProgram p(dlg.GetPathName());
				  if ( p.Initialized() ) vstmachine().LoadProgram(p);
				  else
					  MessageBox("Error Loading file", NULL, MB_ICONERROR);*/

			  }
			  FillProgramCombobox();
      } else {
        MessageBox("Please, use Views->Bank manager with Native Plugins to import presets.","Parameters window");
      }
		}
		void CFrameMachine::OnProgramsSavepreset()
		{			
      if (_machine->_type == MACH_LUA && ((LuaPlugin*)_machine)->prsmode()==MachinePresetType::CHUNK) {
        char tmp[2048];			
			  CFileDialog dlg(FALSE,
				  "fxb",
				  NULL,
				  OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN |
				  OFN_OVERWRITEPROMPT,
				  "Effect Bank Files (.fxb)|*.fxb|Effect Program Files (.fxp)|*.fxp|All Files|*.*||");
			  dlg.m_ofn.lpstrInitialDir = tmp;

			  if (dlg.DoModal() == IDOK)
			  {
          LuaPlugin* plug = (LuaPlugin*) this->_machine;
				  if (dlg.GetFileExt() == "fxb")
            plug->SaveBank((LPCSTR)dlg.GetPathName());
				  else if ( dlg.GetFileExt() == "fxp") {
					  // SaveProgram((LPCSTR)dlg.GetPathName());
          }
			  }
      } else {
        MessageBox("Please, use Views->Bank manager with Native Plugins to export presets.","Parameters window");
      }
		}
		void CFrameMachine::OnSetBank(UINT nID)
		{
			int bank = nID - ID_SELECTBANK_0;
			if (bank < machine().GetNumBanks()) {
				machine().SetCurrentBank(bank);
				isInternal=false;
				isUser=false;
			}
			else 
			{
				bank -= machine().GetNumBanks();
				if(internalPresets.size() > 0) {
					bank--;
					if (bank == -1) isInternal=true;
				}
				if(userPresets.size() > 0) {
					bank--;
					if (bank == -1) isUser=true;
				}
			}

			FillProgramCombobox();
		}
		void CFrameMachine::OnSetProgram(UINT nID)
		{
			ChangeProgram(nID - ID_SELECTPROGRAM_0);
		}


		void CFrameMachine::OnProgramsRandomizeprogram()
		{
			int numParameters = machine().GetNumParams();
			for(int c(0); c < numParameters ; ++c)
			{
				int minran,maxran;
				_machine->GetParamRange(c,minran,maxran);

				int dif = (maxran-minran);

				float randsem = value_mapper::map_32768_1(rand());

				float roffset = randsem*(float)dif;

				PsycleGlobal::inputHandler().AddMacViewUndo();
				machine().SetParameter(c,minran+int(roffset));
			}
			Invalidate(false);
		}

		void CFrameMachine::OnParametersResetparameters() 
		{
			if ( _machine->_type == MACH_PLUGIN)
			{
				int numpars = _machine->GetNumParams();
				for (int c=0; c<numpars; c++)
				{
					int dv = ((Plugin*)_machine)->GetInfo()->Parameters[c]->DefValue;
					PsycleGlobal::inputHandler().AddMacViewUndo();
					_machine->SetParameter(c,dv);
				}
			}
			else if (_machine->_type == MACH_LADSPA)
			{
				((LADSPAMachine*)_machine)->SetDefaultsForControls();
			}
			Invalidate(false);
		}

		void CFrameMachine::OnOperationsEnabled()
		{
			if (machine()._mode == MACHMODE_GENERATOR)
			{
				machine()._mute = !machine()._mute;
			}
			else if (machine()._mute) 
			{
				machine()._mute = false;
				machine().Bypass(false);
			}
			else {
				machine().Bypass(!machine().Bypass());
			}
			wndView->Repaint();
		}

		void CFrameMachine::OnUpdateOperationsEnabled(CCmdUI *pCmdUI)
		{
			if (machine()._mode == MACHMODE_GENERATOR)
			{
				pCmdUI->SetCheck(!machine()._mute);
			}
			else
			{
				pCmdUI->SetCheck(!(machine()._mute || machine().Bypass()));
			}
		}

		void CFrameMachine::OnViewsBankmanager()
		{
			CPresetsDlg dlg(this);
			dlg._pMachine=_machine;
			dlg.DoModal();
			int tempSelected=userSelected;
			bool tmpInternal=isInternal;
			bool tmpUser=isUser;
			LocatePresets();
			userSelected=tempSelected;
			isInternal = tmpInternal;
			isUser = tmpUser;
			FillProgramCombobox();
		}

		void CFrameMachine::OnViewsParameterlist()
		{
			CRect rc;
			GetWindowRect(&rc);
			if (!pParamGui)
			{
				pParamGui= new CParamList(machine(), this, &pParamGui);
				pParamGui->SetWindowPos(0,rc.right+1,rc.top,0,0,SWP_NOSIZE | SWP_NOZORDER);
				pParamGui->ShowWindow(SW_SHOWNORMAL); 
			}
			else
			{
				pParamGui->SendMessage(WM_CLOSE);
			}
		}

    void CFrameMachine::OnViewsParameterMap()
	{      
      CRect rc;
		GetWindowRect(&rc);
		if (!pParamMapGui)
		{
			pParamMapGui = new ParamMap(&machine(),&pParamMapGui);
        pParamMapGui->close.connect(boost::bind(&CFrameMachine::OnParamMapClose, this, _1));
			pParamMapGui->set_position(ui::Rect(ui::Point(rc.right+1, rc.top), ui::Dimension(500, 500)));
			pParamMapGui->Show();
		}
		else
		{
			delete pParamMapGui;
        pParamMapGui = 0;
		}
    }

		void CFrameMachine::OnUpdateViewsParameterlist(CCmdUI *pCmdUI)
		{
			pCmdUI->SetCheck(pParamGui!= NULL);
			pCmdUI->Enable(machine().GetNumParams() > 0);
		}

		void CFrameMachine::OnUpdateViewsParameterMap(CCmdUI *pCmdUI)
		{
			pCmdUI->SetCheck(pParamMapGui!= NULL);
			pCmdUI->Enable(machine().GetNumParams() > 0);
		}

		void CFrameMachine::OnViewsShowtoolbar()
		{
			PsycleGlobal::conf().macParam().toolbarOnMachineParams = !PsycleGlobal::conf().macParam().toolbarOnMachineParams;

			if (PsycleGlobal::conf().macParam().toolbarOnMachineParams) ShowControlBar(&toolBar,TRUE,FALSE);
			else ShowControlBar(&toolBar,FALSE,FALSE);
			ResizeWindow(0);
		}

		void CFrameMachine::OnUpdateViewsShowtoolbar(CCmdUI *pCmdUI)
		{
			pCmdUI->SetCheck(PsycleGlobal::conf().macParam().toolbarOnMachineParams);
		}

		void CFrameMachine::OnParametersCommand() 
		{
			if ( _machine->_type == MACH_PLUGIN)
			{
				try
				{
					((Plugin*)_machine)->proxy().Command();
				}
				catch(const std::exception &)
				{
					// o_O`
				}
			}
			if ( _machine->_type == MACH_LUA) {
				try
				{
					((LuaPlugin*)_machine)->help();
				}
				catch(const std::exception &)
				{
					// o_O`
				}
			}
		}

		void CFrameMachine::OnUpdateParametersCommand(CCmdUI *pCmdUI)
		{						
		   pCmdUI->Enable(_machine->_type == MACH_PLUGIN or _machine->_type == MACH_LUA);			
		}

    void CFrameMachine::OnReload() {
      pView->OnReload(_machine);
/*      boost::weak_ptr<ui::MenuContainer> menu_bar = dynamic_cast<LuaPlugin*>(_machine)->proxy().menu_bar();
      if (!menu_bar.expired()) {
        ui::mfc::MenuContainerImp* menubar_imp = (ui::mfc::MenuContainerImp*) menu_bar.lock()->imp();
        menubar_imp->set_menu_window(this, menu_bar.lock()->root_node().lock());
        menu_bar.lock()->Invalidate();
      }*/       
      ResizeWindow(0);
      pView->Invalidate(false);
    }

		void CFrameMachine::OnMachineReloadScript()
		{
			if (_machine->_type == MACH_LUA)
			{                
				_machine->reload();        
			}
		}
		void CFrameMachine::OnUpdateMachineReloadScript(CCmdUI *pCmdUI)
		{
			pCmdUI->Enable(_machine->_type == MACH_LUA);
		}

		void CFrameMachine::OnMachineAboutthismachine() 
		{			
			if ( _machine->_type == MACH_LADSPA) {
				MessageBox(CString("About ") + CString(machine().GetName()));

			} else
			if ( _machine->_type == MACH_LUA) {
				PluginInfo info = ((LuaPlugin*)_machine)->info();
				MessageBox(CString("Authors: ") + CString(info.vendor.c_str()),
					 CString("About ") + CString(info.name.c_str()));

			} else
			if ( _machine->_type == MACH_PLUGIN)
			{
				MessageBox(CString("Authors: ") + CString(((Plugin*)_machine)->GetInfo()->Author),
						CString("About ") + CString(machine().GetName()));
			}
			else if ( _machine->_type == MACH_VST || _machine->_type == MACH_VSTFX)
			{
				///\todo: made an informative dialog like in seib's vsthost.
				MessageBox(CString("Vst Plugin by " )+ CString(static_cast<vst::Plugin*>(_machine)->GetVendorName()),
					CString("About") + CString(machine().GetName()));
			}
		}
		void CFrameMachine::OnSelchangeProgram() 
		{
			ChangeProgram(comboProgram.GetCurSel());
			SetFocus();
		}
		void CFrameMachine::OnCloseupProgram()
		{
			SetFocus();
		}

		void CFrameMachine::OnProgramLess()
		{
			int i = 0;
			if (isInternal) {
				i = userSelected;
			} else if(isUser){
				i = userSelected;
			}
			else {
				i = lastprogram;
			}
			ChangeProgram(i-1);
			UpdateWindow();
		}
		void CFrameMachine::OnUpdateProgramLess(CCmdUI *pCmdUI)
		{
			int i = 0;
			if (isInternal) {
				i = userSelected;
			} else if(isUser){
				i = userSelected;
			}
			else {
				i = lastprogram;
			}
			if ( i == 0)
			{
				pCmdUI->Enable(false);
			}
			else pCmdUI->Enable(true);
		}
		void CFrameMachine::OnProgramMore()
		{
			int i = 0;
			if (isInternal) {
				i = userSelected;
			} else if(isUser){
				i = userSelected;
			}
			else {
				i = lastprogram;
			}
			ChangeProgram(i+1);
			UpdateWindow();
		}
		void CFrameMachine::OnUpdateProgramMore(CCmdUI *pCmdUI)
		{
			int i = 0;
			int nump = 0;
			if (isInternal) {
				i = userSelected;
				nump = internalPresets.size();
			} else if(isUser){
				i = userSelected;
				nump = userPresets.size();
			}
			else {
				i = lastprogram;
				nump = lastnumprogrs;
			}

			if ( i+1 == nump || nump==0)
			{
				pCmdUI->Enable(false);
			}
			else pCmdUI->Enable(true);
		}

		//////////////////////////////////////////////////////////////////////////

		CBaseParamView* CFrameMachine::CreateView()
		{
			CBaseParamView* gui;
			if(machine()._type == MACH_MIXER) {
				gui = new MixerFrameView(this,&machine());
			} else 
      if(machine()._type == MACH_LUA) {
        PsycleUiParamView* psyclegui = new PsycleUiParamView(this, &machine());
        gui = psyclegui;
        LuaPlugin* lp = (LuaPlugin*) _machine;
        ui::Node::Ptr menu_root_node;
        if (!lp->proxy().menu_root_node().expired()) {
           menu_root_node = lp->proxy().menu_root_node().lock();
        }
        psyclegui->set_menu(menu_root_node);
        ui::Viewport::WeakPtr viewport = lp->viewport();
        if (!viewport.expired()) {
          gui->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
				    CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL);            
          psyclegui->set_viewport(viewport.lock()); 
          return gui;
        } else {
          gui = new CNativeView(this,&machine());
        }
      }
			else {
				gui = new CNativeView(this,&machine());
			}
			gui->Create(NULL, NULL, AFX_WS_DEFAULT_VIEW,
				CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL);
			return gui;
		}

		void CFrameMachine::PostOpenWnd()
		{
			pView->Open();
			ResizeWindow(0);
		}

		void CFrameMachine::GetWindowSize(CRect &rcFrame, CRect &rcClient, CRect *pRect)
		{
			if ( !pRect )
			{
				if (!pView->GetViewSize(rcClient))
				{
					rcClient.top = 0; rcClient.left = 0;
					rcClient.right = 400; rcClient.bottom = 300;
				}
				rcFrame = rcClient;
			}
			else 
			{
				rcFrame.left = pRect->left;
				rcFrame.top = pRect->top;
				rcFrame.right = pRect->right;
				rcFrame.bottom = pRect->bottom;
				rcClient.top = 0; rcClient.left = 0;
				rcClient.right = pRect->right - pRect->left; rcClient.bottom = pRect->bottom - pRect->top;
			}

			if (_machine->_type == MACH_LUA) {
			  LuaPlugin* plugin = dynamic_cast<LuaPlugin*>(_machine);
        assert(plugin);
			  ui::Viewport* user_view = plugin->viewport().lock().get();
			  if (user_view != 0 && plugin->ui_type() == MachineUiType::CUSTOMWND) {
			    user_view->OnSize(ui::Dimension(rcClient.right, rcClient.bottom));
			  }
			}  

			//Add frame border/caption size.
			CalcWindowRect(rcFrame);

			//SM_CYMENU The height of a single-line menu bar, in pixels.
			rcFrame.bottom += ::GetSystemMetrics(SM_CYMENU);
			if ( PsycleGlobal::conf().macParam().toolbarOnMachineParams && !(toolBar.GetBarStyle() & CBRS_FLOATING))
			{
				//SM_CYBORDER The height of a window border, in pixels. This is equivalent to the SM_CYEDGE value for windows with the 3-D look.
				CRect tbRect;
				toolBar.GetWindowRect(&tbRect);
				int heiTool = tbRect.bottom - tbRect.top - (2 * ::GetSystemMetrics(SM_CYBORDER) );
				rcClient.top+=heiTool;
				rcFrame.bottom += heiTool;
			}

		}
		void CFrameMachine::ResizeWindow(CRect *pRect)
		{
			CRect rcEffFrame,rcEffClient,rcTemp,tbRect;
			GetWindowSize(rcEffFrame, rcEffClient, pRect);
			SetWindowPos(NULL,0,0,rcEffFrame.right-rcEffFrame.left,rcEffFrame.bottom-rcEffFrame.top,SWP_NOZORDER | SWP_NOMOVE);
			pView->SetWindowPos(NULL,rcEffClient.left, rcEffClient.top, rcEffClient.right,rcEffClient.bottom,SWP_NOZORDER);         
			pView->WindowIdle();
		}


		//////////////////////////////////////////////////////////////////////////

		void CFrameMachine::LocatePresets()
		{
			int dataSizeStruct = 0;
			if( machine()._type == MACH_PLUGIN)
			{
				dataSizeStruct = ((Plugin *)_machine)->proxy().GetDataSize();
			}
			CString buffer;
			buffer = machine().GetDllName();
			buffer = buffer.Left(buffer.GetLength()-4);
			buffer += ".prs";
			boost::filesystem::path inpath(buffer.GetString());
			if(boost::filesystem::exists(inpath))
			{
				PresetIO::LoadPresets(buffer,machine().GetNumParams(),dataSizeStruct,internalPresets,false);
			}

			buffer = PsycleGlobal::conf().GetAbsolutePresetsDir().c_str();
			buffer = buffer + "\\" + buffer.Mid(buffer.ReverseFind('\\'));
			boost::filesystem::path inpath2(buffer.GetString());
			if(boost::filesystem::exists(inpath2))
			{
				PresetIO::LoadPresets(buffer,machine().GetNumParams(),dataSizeStruct,userPresets,false);
			}

			userSelected=0;
			if(internalPresets.size() > 0) {
				isInternal = true; isUser = false;
			}
			else if(userPresets.size() > 0) {
				isInternal = false; isUser = true;
			}
			else {
				isInternal = isUser = false;
			}
		}

		void CFrameMachine::FillBankPopup(CMenu* pPopupMenu)
		{
			CMenu* popBnk=0;
			popBnk = pPopupMenu->GetSubMenu(3);
			if (!popBnk)
				return;

			DeleteBankMenu(popBnk);
			int i = 0;
			for (i = 0; i < machine().GetNumBanks() && i < 980 ; i += 16)
			{
				CMenu popup;
				popup.CreatePopupMenu();
				for (int j = i; (j < i + 16) && (j < machine().GetNumBanks()); j++)
				{
					char s1[38];
					char s2[32];
					_machine->GetIndexBankName(j, s2);
					std::sprintf(s1,"%d: %s",j,s2);
					popup.AppendMenu(MF_STRING, ID_SELECTBANK_0 + j, s1);
				}
				char szSub[256] = "";;
				std::sprintf(szSub,"Banks %d-%d",i,i+15);
				popBnk->AppendMenu(MF_POPUP | MF_STRING,
					(UINT_PTR)popup.Detach(),
					szSub);
			}
			i = machine().GetNumBanks();
			if(internalPresets.size() > 0|| 
				( userPresets.size() == 0 && i == 0)) {
				popBnk->AppendMenu(MF_STRING, ID_SELECTBANK_0 + i, "Provided Presets");
				i++;
			}
			if(userPresets.size() > 0)
			{
				popBnk->AppendMenu(MF_STRING, ID_SELECTBANK_0 + i, "User Presets");
				i++;
			}
			int selected;
			if(isInternal || 
				(internalPresets.size() == 0 && isUser)) {
				selected = machine().GetNumBanks();
			}
			else if (isUser) {
				selected = machine().GetNumBanks()+1;
			}
			else {
				selected = machine().GetCurrentBank();
			}
			popBnk->CheckMenuItem(ID_SELECTBANK_0 + selected, MF_CHECKED | MF_BYCOMMAND);
		}

		bool CFrameMachine::DeleteBankMenu(CMenu* popBnk)
		{
			CMenu* secMenu=0;
			while (popBnk->GetMenuItemCount() > 0)
			{
				if ((secMenu=popBnk->GetSubMenu(0)))
				{
					while (secMenu->GetMenuItemCount() > 0) 
					{
						secMenu->DeleteMenu(0,MF_BYPOSITION);
					}
				}
				popBnk->DeleteMenu(0,MF_BYPOSITION);
			}
			return true;
		}
		void CFrameMachine::FillProgramPopup(CMenu* pPopupMenu)
		{
			CMenu* popPrg=0;
			popPrg = pPopupMenu->GetSubMenu(4);
			if (!popPrg)
				return;

			DeleteProgramMenu(popPrg);
			if(isInternal) 
			{
				FillPopupFromPresets(popPrg, internalPresets);
			}
			else if(isUser)
			{
				FillPopupFromPresets(popPrg, userPresets);
			}
			else if (machine().GetTotalPrograms() > 0)
			{
				for (int i = 0; i < machine().GetNumPrograms() && i < 980 ; i += 16)
				{
					CMenu popup;
					popup.CreatePopupMenu();
					for (int j = i; (j < i + 16) && (j < machine().GetNumPrograms()); j++)
					{
						char s1[38];
						char s2[32];
						_machine->GetIndexProgramName(machine().GetCurrentBank(), j, s2);
						std::sprintf(s1,"%d: %s",j,s2);
						popup.AppendMenu(MF_STRING, ID_SELECTPROGRAM_0 + j, s1);
					}
					char szSub[256] = "";;
					std::sprintf(szSub,"Programs %d-%d",i,i+15);
					popPrg->AppendMenu(MF_POPUP | MF_STRING,
						(UINT_PTR)popup.Detach(),
						szSub);
				}
			}
			int selected;
			if(isInternal) {
				selected = userSelected;
			}
			else if (isUser) {
				selected = userSelected;
			}
			else {
				selected = machine().GetCurrentProgram();
			}
			popPrg->CheckMenuItem(ID_SELECTPROGRAM_0 + selected, MF_CHECKED | MF_BYCOMMAND);
		}

		void CFrameMachine::FillPopupFromPresets(CMenu* popPrg, std::list<CPreset> const & presets )
		{
			std::list<CPreset>::const_iterator preset = presets.begin();
			for (int i = 0; i <presets.size() && i < 980 ; i += 16)
			{
				CMenu popup;
				popup.CreatePopupMenu();
				for(int j = i; (j < i + 16) && (preset != presets.end()); j++, preset++)
				{
					char s1[38];
					char s2[32];
					preset->GetName(s2);
					std::sprintf(s1,"%d: %s",j,s2);
					popup.AppendMenu(MF_STRING, ID_SELECTPROGRAM_0 + j, s1);
				}
				char szSub[256] = "";;
				std::sprintf(szSub,"Programs %d-%d",i,i+15);
				popPrg->AppendMenu(MF_POPUP | MF_STRING,
					(UINT_PTR)popup.Detach(),
					szSub);
			}
		}

		bool CFrameMachine::DeleteProgramMenu(CMenu* popPrg)
		{
			CMenu* secMenu=0;
			while (popPrg->GetMenuItemCount() > 0)
			{
				if ((secMenu=popPrg->GetSubMenu(0)))
				{
					while (secMenu->GetMenuItemCount() > 0)
					{
						secMenu->DeleteMenu(0,MF_BYPOSITION);
					}
				}
				popPrg->DeleteMenu(0,MF_BYPOSITION);
			}
			return true;
		}
		void CFrameMachine::FillProgramCombobox()
		{
			comboProgram.ResetContent();
			int nump = 0;
			if(isInternal)  {
				FillComboboxFromPresets(&comboProgram, internalPresets);
				nump = internalPresets.size();
			} else if (isUser) {
				FillComboboxFromPresets(&comboProgram, userPresets);
				nump = userPresets.size();
			} else if (_machine->GetNumPrograms() > 0) {
				nump =  _machine->GetNumPrograms();
				for(int i(0) ; i < nump; ++i)
				{
					char s1[38];
					char s2[32];
					_machine->GetIndexProgramName(0, i, s2);
					std::sprintf(s1,"%d: %s",i,s2);
					comboProgram.AddString(s1);
				}
			}
			int i=0;
			if (isInternal) {
				i = userSelected;
			} else if(isUser){
				i = userSelected;
			}
			else {
				i = _machine->GetCurrentProgram();
			}

			if ( i > nump || i < 0) {  i = 0; }
			comboProgram.SetCurSel(i);
			lastprogram = i;
			lastnumprogrs = nump;
			if (pParamGui){
				pParamGui->InitializePrograms();
			}
		}

		void CFrameMachine::FillComboboxFromPresets(CComboBox* combo, std::list<CPreset> const & presets )
		{
			std::list<CPreset>::const_iterator preset = presets.begin();
			for(int i = 0; preset != presets.end(); i++, preset++)
			{
				char s1[38];
				char s2[32];
				preset->GetName(s2);
				std::sprintf(s1,"%d: %s",i,s2);
				combo->AddString(s1);
			}
		}

		void CFrameMachine::ChangeProgram(int numProgram)
		{
			if(isInternal){
				userSelected=numProgram;
				std::list<CPreset>::iterator preset = internalPresets.begin();
				for(int i=0; preset != internalPresets.end(); i++, preset++)
				{
					if(i == userSelected) {
						_machine->Tweak(*preset);
						break;
					}
				}
			}
			else if (isUser) {
				userSelected=numProgram;
				std::list<CPreset>::iterator preset = userPresets.begin();
				for(int i=0; preset != userPresets.end(); i++, preset++)
				{
					if(i == userSelected) {
						_machine->Tweak(*preset);
						break;
					}
				}
			}
			else {
				_machine->SetCurrentProgram(numProgram);
			}
			comboProgram.SetCurSel(numProgram);
			lastprogram = numProgram;
			if (pParamGui){
				pParamGui->SelectProgram(numProgram);
			}
		}
		void CFrameMachine::Automate(int param, int value, bool undo, int min)
		{
			PsycleGlobal::inputHandler().Automate(machine()._macIndex, param, value-min, undo, machine().param_translator());
			if (pParamGui) {
				pParamGui->UpdateNew(param, value);
			}
			if (pParamMapGui) {
				pParamMapGui->UpdateNew(param, value);
			}
	}
    void CFrameMachine::OnParamMapClose(ui::Frame&) {
      pParamMapGui = 0;
    }

	}   // namespace
}   // namespace
