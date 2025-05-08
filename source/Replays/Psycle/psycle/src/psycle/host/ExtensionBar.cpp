/** @file 
 *  @brief SequenceBar dialog
 *  $Date: 2010-08-15 18:18:35 +0200 (dg., 15 ag 2010) $
 *  $Revision: 9831 $
 */
#include <psycle/host/detail/project.private.hpp>
#include "ExtensionBar.hpp"

#include "PsycleConfig.hpp"
#include "MainFrm.hpp"
#include "ChildView.hpp"
#include "CanvasItems.hpp"
#include "LuaPlugin.hpp"
#include "MfcUi.hpp"

namespace psycle {
namespace host {

	PsycleMeditation::PsycleMeditation() : plugin_(0)
	{
		using namespace ui;		
		InvalidateDirect();
		set_aligner(Aligner::Ptr(new DefaultAligner()));				
		add_ornament(Ornament::Ptr(OrnamentFactory::Instance().CreateFill(0xFF222222)));				
		ui::Group::Ptr top(new Group());
		top->set_auto_size(false, true);
		//top->set_position(Rect(Point(), Dimension(500, 200)));
		top->set_align(AlignStyle::TOP);		
		top->set_aligner(Aligner::Ptr(new DefaultAligner()));
		top->set_margin(ui::BoxSpace(10));
		top->add_ornament(Ornament::Ptr(OrnamentFactory::Instance().CreateLineBorder(0xFFFF3322, 3)));
		Add(top);		
		AddText(top, "Software Failure.   Press left mouse button to continue.");				
		meditation_field_ = AddText(top, "");
		meditation_field_->set_margin(ui::BoxSpace(5, 0, 5, 0));				
		error_box_.reset(new Scintilla());
	    error_box_->set_align(AlignStyle::CLIENT);
		error_box_->set_background_color(0xFF232323);
		error_box_->set_foreground_color(0xFFFFBF00);	
		error_box_->StyleClearAll();
		error_box_->set_linenumber_foreground_color(0xFF939393);
		error_box_->set_linenumber_background_color(0xFF232323);  
		error_box_->PreventInput();
		error_box_->f(SCI_SETWRAPMODE, (void*) SC_WRAP_CHAR, 0);
		Add(error_box_);		
	}

	ui::Text::Ptr PsycleMeditation::AddText(const ui::Window::Ptr& parent, const std::string& text)
	{
		using namespace ui;
		Text::Ptr text_field = Text::Ptr(new Text(text));
		text_field->set_auto_size(false, true);
		text_field->set_position(Rect(Point(), Dimension(500, 20)));
		text_field->set_color(0xFFFF3322);
		text_field->set_align(AlignStyle::TOP);
		text_field->set_justify(ui::JustifyStyle::CENTERJUSTIFY);
		text_field->set_vertical_alignment(ui::AlignStyle::CENTER);
		text_field->set_margin(ui::BoxSpace(5, 0, 0, 0));
		parent->Add(text_field);
		return text_field;
	}

	void PsycleMeditation::set_plugin(LuaPlugin& plugin)
	{
		plugin_ = &plugin;
		meditation_field_->set_text("Psycle Meditation #" + std::string(plugin.GetEditName()) + "." + std::string(plugin.GetDllName()));
	}

	void PsycleMeditation::set_error_text(const std::string& text) {		
		error_box_->EnableInput();
		error_box_->ClearAll();
		error_box_->AddText(text);
		error_box_->PreventInput();
	}

	void PsycleMeditation::OnMouseDown(ui::MouseEvent& ev) {
	  if (plugin_) {
	    plugin_->DoReload();
	  }
	}

	ExtensionWindow::ExtensionWindow()
	{
		using namespace ui;
		set_aligner(Aligner::Ptr(new DefaultAligner()));
	}

	void ExtensionWindow::Push(LuaPlugin& plugin)
	{									 					 		 
		if (!plugin.viewport().expired()) {
			if (HasExtensions()) {
				RemoveAll();
				RemoveExtension(plugin);
			} else {
				Show();				
			}
			if (!HasExtensions() || extensions_.back().lock().get() != &plugin) {
				extensions_.push_back(plugin.shared_from_this());
			}
		}			
	}
					
	void ExtensionWindow::Pop()
	{
		if (HasExtensions()) {			
			RemoveAll();
			extensions_.pop_back();
		}
		if (extensions_.empty()) {
			Hide();
		}
	}
		
	void ExtensionWindow::DisplayTop()
	{
		if (HasExtensions()) {
			assert(!extensions_.back().expired());	
			LuaPlugin::Ptr extension(extensions_.back().lock());
			ui::Viewport::Ptr top(extension->has_error()
							      ? extension->proxy().psycle_meditation().lock()
							      : extension->viewport().lock());
			top->set_align(ui::AlignStyle::CLIENT);
			Add(top, false);
			top->PreventFls();			
			UpdateAlign();
			top->EnableFls();
			if (extension->has_error()) {
			  top->FLS();
			}            
			top->SetFocus();
		}
	}	

	bool ExtensionWindow::IsTopDisplay(LuaPlugin& plugin) const
	{		
		return HasExtensions() && extensions_.back().lock().get() == &plugin;
	}
	
	void ExtensionWindow::RemoveExtension(LuaPlugin& plugin)
	{
		Extensions::iterator it = extensions_.begin();
		for (; it != extensions_.end(); ++it) {
			if (!(*it).expired() && (*it).lock().get() == &plugin) {
				extensions_.erase(it);
				break;
			}
		}
	}
		
	void ExtensionWindow::RemoveExtensions()
	{						
		if (HasExtensions()) {			
			RemoveAll();
			extensions_.clear();
			Hide();			
		}
	}
		
	void ExtensionWindow::UpdateMenu(MenuHandle& menu_handle)
	{
		menu_handle.clear();
		if (HasExtensions()) {
			menu_handle.set_menu(extensions_.back().lock()->proxy().menu_root_node());
		}
		::AfxGetMainWnd()->DrawMenuBar();
	}
	
	void ExtensionWindow::OnKeyDown(ui::KeyEvent& ev)
	{
		CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
		CChildView* view = &main->m_wndView;
		UINT nFlags(0);		
		if (ev.extendedkey()) {
	      nFlags |= 0x100;
	    }
		view->KeyDown(ev.keycode(), 0, nFlags);
	}						

	void ExtensionWindow::OnKeyUp(ui::KeyEvent& ev)
	{
		CMainFrame* main = (CMainFrame*) AfxGetMainWnd();
		CChildView* view = &main->m_wndView;
		UINT nFlags(0);		
		if (ev.extendedkey()) {
	      nFlags |= 0x100;
	    }
		view->KeyUp(ev.keycode(), 0, nFlags);
	}

	extern CPsycleApp theApp;

	IMPLEMENT_DYNAMIC(ExtensionBar, CDialogBar)

	ExtensionBar::ExtensionBar()
	{
		m_sizeFloating.SetSize(500, 200);
	}

	ExtensionBar::~ExtensionBar()
	{
	}

	int ExtensionBar::OnCreate(LPCREATESTRUCT lpCreateStruct) {
		if (CDialogBar::OnCreate(lpCreateStruct) == -1) {
			return -1;
		}
		m_luaWndView.reset(new ExtensionWindow());
// 		ui::mfc::WindowImp* mfc_imp = (ui::mfc::WindowImp*) m_luaWndView->imp();
// 		mfc_imp->SetParent(this);		
		return 0;
	}

	void ExtensionBar::DoDataExchange(CDataExchange* pDX)
	{
		CDialogBar::DoDataExchange(pDX);
	}

	//Message Maps are defined in CMainFrame, since this isn't a window, but a DialogBar.
/*
	BEGIN_MESSAGE_MAP(ExtensionBar, CDialogBar)
		ON_WM_CREATE()
		ON_MESSAGE(WM_INITDIALOG, OnInitDialog )
	END_MESSAGE_MAP()
*/

	// SequenceBar message handlers
	LRESULT ExtensionBar::OnInitDialog ( WPARAM wParam, LPARAM lParam)
	{
		BOOL bRet = HandleInitDialog(wParam, lParam);

		if (!UpdateData(FALSE))
		{
		   TRACE0("Warning: UpdateData failed during dialog init.\n");
		}

		return bRet;
	}

	CSize ExtensionBar::CalcDynamicLayout(int length, DWORD dwMode) {
		using namespace ui;
	    if (docked(dwMode)) {
			CRect rc;
			m_pDockBar->GetParent()->GetClientRect(&rc);
			m_luaWndView->set_position(Rect(Point(10, 0),
				Dimension(rc.Width(), m_luaWndView->min_dimension().height())));
			return CSize(rc.Width(), m_luaWndView->min_dimension().height());
		}
		if (dwMode & LM_MRUWIDTH) {
			m_luaWndView->set_position(Rect(Point(), Dimension(m_sizeFloating.cx, m_sizeFloating.cy)));			
			return m_sizeFloating;
		} 
		if (dwMode & LM_LENGTHY) {
			m_sizeFloating.cy = length;
			m_luaWndView->set_position(Rect(Point(), Dimension(m_sizeFloating.cx, m_sizeFloating.cy)));			
			return CSize(m_sizeFloating.cx, m_sizeFloating.cy);
		}
		else {
			m_sizeFloating.cx = length;
			m_luaWndView->set_position(Rect(Point(), Dimension(m_sizeFloating.cx, m_sizeFloating.cy)));			
			return CSize(m_sizeFloating.cx, m_sizeFloating.cy);
		}		
	}
	
	void ExtensionBar::Add(LuaPlugin& plugin)
	{
		if (!m_luaWndView->IsTopDisplay(plugin)) {
			m_luaWndView->set_min_dimension(plugin.viewport().lock()->min_dimension());									
			m_luaWndView->Push(plugin);				
			m_luaWndView->DisplayTop();
			((CMainFrame*)::AfxGetMainWnd())->RecalcLayout();
		}		
	}	

}}
