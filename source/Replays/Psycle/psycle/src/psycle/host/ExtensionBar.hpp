#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "ExListBox.h"
#include "CanvasItems.hpp"

namespace psycle { namespace host {
		
	class LuaPlugin;

	class ExtensionWindow : public ui::Group {
		public:
			ExtensionWindow();
			~ExtensionWindow() {}

			void Push(LuaPlugin& plugin);
			void Pop();
			void DisplayTop();
			bool HasExtensions() const { return !extensions_.empty(); }
			void RemoveExtensions();
			void UpdateMenu(class MenuHandle& menu_handle);
			bool IsTopDisplay(LuaPlugin& plugin) const;
			
		protected:
			virtual void OnKeyDown(ui::KeyEvent& ev);
			virtual void OnKeyUp(ui::KeyEvent& ev);
			virtual void OnSize(const ui::Dimension&) { UpdateAlign(); }

		private:
			void RemoveExtension(LuaPlugin& plugin);
			typedef std::list<boost::weak_ptr<LuaPlugin> > Extensions;
			Extensions extensions_;
	};

	class LuaPlugin;

	class PsycleMeditation : public ui::Viewport {
		public:
			PsycleMeditation();
			~PsycleMeditation() {}

			void set_error_text(const std::string& text);
			void set_plugin(LuaPlugin& plugin);

		private:
		  ui::Text::Ptr AddText(const ui::Window::Ptr& parent,
			const std::string& text);						
		  ui::Scintilla::Ptr error_box_;		 
		  ui::Text::Ptr meditation_field_;
		  LuaPlugin* plugin_;
		  void OnMouseDown(ui::MouseEvent& ev);
	};
	
	class ExtensionBar : public CDialogBar
	{		
		DECLARE_DYNAMIC(ExtensionBar)
	public:
		ExtensionBar();
		virtual ~ExtensionBar();
				
		void Add(LuaPlugin& plugin);
		boost::shared_ptr<ExtensionWindow> m_luaWndView;

	protected:
		virtual void DoDataExchange(CDataExchange* pDX);
			
		DECLARE_MESSAGE_MAP()
		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg LRESULT OnInitDialog(WPARAM , LPARAM);													
		CSize CalcDynamicLayout(int length, DWORD dwMode);			

	private:
		CSize m_sizeFloating;
		bool docked(DWORD mode) const {
			return (mode & LM_VERTDOCK) || (mode & LM_HORZDOCK);
		}
	};
}}
