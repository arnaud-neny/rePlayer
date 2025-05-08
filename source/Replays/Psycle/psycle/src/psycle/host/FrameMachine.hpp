///\file
///\brief interface file for psycle::host::CFrameMachine.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PresetIO.hpp"

namespace psycle {
namespace host {
	class CChildView;
	class Machine;
	class CBaseParamView;
	class CParamList;    
    class ParamMap;
    namespace ui {
      class Frame;
    }

		class CMyToolBar : public CToolBar {
		public:
			virtual void OnBarStyleChange(DWORD dwOldStyle, DWORD dwNewStyle);
		};

		/// machine window.
		class CFrameMachine : public CFrameWnd
		{
			DECLARE_DYNAMIC(CFrameMachine)
		public:
			CFrameMachine(Machine* pMachine, CChildView* wndView_, CFrameMachine** windowVar_);
			virtual ~CFrameMachine();
		protected:
			CFrameMachine(); // protected constructor used by dynamic creation
			virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
			virtual void PostNcDestroy();

			friend class CParamList;
			friend class CNativeView;
			friend class MixerFrameView;
		public:
			virtual void ResizeWindow(CRect* pRect);
			virtual void GetWindowSize(CRect &rcFrame, CRect &rcClient, CRect *pRect = NULL);
			virtual void PostOpenWnd();
      void OnReload();
		protected:
			virtual CBaseParamView* CreateView();

		// Attributes
		public:
			inline Machine& machine(){ return *_machine; }			
		protected:
			void Automate(int param, int value, bool undo, int min=0);
			void ChangeProgram(int program);
			void LocatePresets();
			void FillBankPopup(CMenu* pPopupMenu);
			bool DeleteBankMenu(CMenu* popPrg);
			void FillProgramPopup(CMenu* pPopupMenu);
			bool DeleteProgramMenu(CMenu* popPrg);
			void FillPopupFromPresets(CMenu* popPrg, std::list<CPreset> const & presets );
			void FillProgramCombobox();
			void FillComboboxFromPresets(CComboBox* combo, std::list<CPreset> const & presets );
			Machine* _machine;
			CChildView *wndView;
			CFrameMachine** windowVar;

			CMyToolBar toolBar;
			CComboBox comboProgram;
			CBaseParamView* pView;
			CParamList* pParamGui;
      ParamMap* pParamMapGui;
			
			std::list<CPreset> internalPresets;
			std::list<CPreset> userPresets;
			bool isInternal;
			bool isUser;
			int userSelected;
			int refreshcounter;
			int lastprogram;
			int lastnumprogrs;

		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);      
			afx_msg void OnClose();
			afx_msg void OnDestroy();
			afx_msg void OnTimer(UINT_PTR nIDEvent);
			afx_msg void OnSetFocus(CWnd* pOldWnd);
			afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
			afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
			afx_msg void OnSizing(UINT fwSide, LPRECT pRect);
			afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
			afx_msg void OnSetBank(UINT nID);
			afx_msg void OnSetProgram(UINT nID);
			afx_msg void OnProgramsOpenpreset();
			afx_msg void OnProgramsSavepreset();
			afx_msg void OnProgramsRandomizeprogram();
			afx_msg void OnParametersResetparameters();
			afx_msg void OnOperationsEnabled();
			afx_msg void OnUpdateOperationsEnabled(CCmdUI *pCmdUI);
			afx_msg void OnViewsBankmanager();
			afx_msg void OnViewsParameterlist();
			afx_msg void OnUpdateViewsParameterlist(CCmdUI *pCmdUI);
      afx_msg void OnViewsParameterMap();
	  afx_msg void OnUpdateViewsParameterMap(CCmdUI *pCmdUI);
			afx_msg void OnViewsShowtoolbar();
			afx_msg void OnUpdateViewsShowtoolbar(CCmdUI *pCmdUI);
			afx_msg void OnParametersCommand();
			afx_msg void OnUpdateParametersCommand(CCmdUI *pCmdUI);
			afx_msg void OnMachineReloadScript();
			afx_msg void OnUpdateMachineReloadScript(CCmdUI *pCmdUI);
			afx_msg void OnMachineAboutthismachine();
			afx_msg void OnSelchangeProgram();
			afx_msg void OnCloseupProgram();
			afx_msg void OnProgramLess();
			afx_msg void OnUpdateProgramLess(CCmdUI *pCmdUI);
			afx_msg void OnProgramMore();
			afx_msg void OnUpdateProgramMore(CCmdUI *pCmdUI);   
      void OnParamMapClose(ui::Frame&);
		};

	}   // namespace
}   // namespace

