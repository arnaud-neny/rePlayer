// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

// #include "stdafx.h"
#include "LuaGui.hpp"
#include "LuaHost.hpp"
#include "Player.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include "MfcUi.hpp"

namespace psycle {
namespace host {

ui::MultiType PsycleStock::value(int stock_key) const {
  using namespace ui;
  PsycleConfig::PatternView* pv_cfg = &PsycleGlobal::conf().patView();
  PsycleConfig::MachineParam* mv_cfg = &PsycleGlobal::conf().macParam();  
  ARGB result(0xFF000000);
  using namespace stock;
  using namespace color;  
  switch (stock_key) {
    case PVBACKGROUND: result = pv_cfg->background; break;
    case PVROW: result = pv_cfg->row; break;    
    case PVROW4BEAT: result = pv_cfg->row4beat; break;
    case PVROWBEAT: result = pv_cfg->rowbeat; break;
    case PVSEPARATOR: result = pv_cfg->separator; break;
    case PVFONT: result = pv_cfg->font; break;
	case PVFONTSEL: result = pv_cfg->fontSel; break;
	case PVFONTPLAY : result = pv_cfg->fontPlay; break;
    case PVCURSOR: result = pv_cfg->cursor; break;
    case PVSELECTION: result = pv_cfg->selection; break;
    case PVPLAYBAR: result = pv_cfg->playbar; break;    
    case MVFONTBOTTOMCOLOR : result = mv_cfg->fontBottomColor; break;
    case MVFONTHTOPCOLOR : result = mv_cfg->fonthTopColor; break;
    case MVFONTHBOTTOMCOLOR : result = mv_cfg->fonthBottomColor; break;
    case MVFONTTITLECOLOR : result = mv_cfg->fonttitleColor; break;
    case MVTOPCOLOR : result = mv_cfg->topColor; break;
    case MVBOTTOMCOLOR : result = mv_cfg->bottomColor; break;
    case MVHTOPCOLOR : result = mv_cfg->hTopColor; break;
    case MVHBOTTOMCOLOR : result = mv_cfg->hBottomColor; break;
    case MVTITLECOLOR : result = mv_cfg->titleColor; break;
    default:
    break;
  }
  return ToARGB(result);
}

std::string PsycleStock::key_tostring(int stock_key) const {  
  std::string result;
  using namespace stock;
  using namespace color;  
  switch (stock_key) {
    case PVBACKGROUND: result = "stock.color.PVBACKGROUND"; break;
    case PVROW: result = "stock.color.PVROW"; break;
    case PVROW4BEAT: result = "stock.color.PVROW4BEAT"; break;
    case PVROWBEAT: result = "stock.color.PVROWBEAT"; break;
    case PVSEPARATOR: result = "stock.color.PVSEPARATOR"; break;
    case PVFONT: result = "stock.color.PVFONT"; break;
	case PVFONTSEL: result = "stock.color.PVFONTSEL"; break;
	case PVFONTPLAY : result = "stock.color.PVFONTPLAY"; break;
    case PVCURSOR: result = "stock.color.PVCURSOR"; break;
    case PVSELECTION: result = "stock.color.PVSELECTION"; break;
    case PVPLAYBAR: result = "stock.color.PVPLAYBAR"; break;
    case MVFONTBOTTOMCOLOR : result = "stock.color.MVFONTBOTTOMCOLOR"; break;
    case MVFONTHTOPCOLOR : result = "stock.color.MVFONTHTOPCOLOR"; break;
    case MVFONTHBOTTOMCOLOR : result = "stock.color.MVFONTHBOTTOMCOLOR"; break;
    case MVFONTTITLECOLOR : result = "stock.color.MVFONTTITLECOLOR"; break;
    case MVTOPCOLOR : result = "stock.color.MVTOPCOLOR"; break;
    case MVBOTTOMCOLOR : result = "stock.color.MVBOTTOMCOLOR"; break;
    case MVHTOPCOLOR : result = "stock.color.MVHTOPCOLOR"; break;
    case MVHBOTTOMCOLOR : result = "stock.color.MVHBOTTOMCOLOR"; break;
    case MVTITLECOLOR : result = "stock.color.MVTITLECOLOR"; break;
    default:
    break;
  }
  return result;
}

LockIF* locker(lua_State *L) {
  return LuaGlobal::proxy(L);  
}

const char* LuaPopupMenuBind::meta = "psypopupmenumeta";

int LuaPopupMenuBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {"update", update},
    {"invalidate", invalidate},
    {"setrootnode", setrootnode},
    {"track", track},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaPopupMenuBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) {
    return err;
  }
  LuaHelper::new_shared_userdata<>(L, meta, new LuaPopupMenu(L));
  return 1;
}

int LuaPopupMenuBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaPopupMenu>(L, meta);  
}

int LuaPopupMenuBind::setrootnode(lua_State* L) {
  using namespace ui;
  PopupMenu::Ptr popup_menu = LuaHelper::check_sptr<LuaPopupMenu>(L, 1, meta);
  Node::Ptr node = LuaHelper::check_sptr<Node>(L, 2,
                                               LuaNodeBind::meta.c_str());
  popup_menu->set_root_node(node);  
  return LuaHelper::chaining(L);
}

int LuaPopupMenuBind::track(lua_State* L) {
  using namespace ui;
  PopupMenu::Ptr popup_menu = LuaHelper::check_sptr<LuaPopupMenu>(L, 1, meta);
  Point::Ptr point = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
  popup_menu->Track(*point.get());    
  return LuaHelper::chaining(L);
}

/////////////////////////////////////////////////////////////////////////////
// PsycleFileOpenBind
/////////////////////////////////////////////////////////////////////////////

const char* LuaFileOpenBind::meta = "psyfileopenmeta";

int LuaFileOpenBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"show", show},
    {"setfilename", setfilename},
    {"filename", filename},
    {"setfolder", setfolder},
    {"isok", isok},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods,  gc);
  return 1;
}

int LuaFileOpenBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) {
    return err;
  }
  LuaHelper::new_shared_userdata<>(L, meta, new ui::FileOpenDialog());
  return 1;
}

int LuaFileOpenBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::FileOpenDialog>(L, meta);
}

int LuaFileOpenBind::show(lua_State* L) {
/*
  boost::shared_ptr<ui::FileOpenDialog> dlg
      = LuaHelper::check_sptr<ui::FileOpenDialog>(L, 1, meta);
  dlg->Show();
  try {
    LuaImport import(L, dlg.get(), 0);  
    if (import.open(dlg->is_ok() ? "onok" : "oncancel")) {
      import << dlg->filename() << pcall(0);
    }
  } catch (std::exception& e) {
    return luaL_error(L, e.what());
  }
*/
  return LuaHelper::chaining(L);
}

/////////////////////////////////////////////////////////////////////////////
// PsycleFileSaveBind
/////////////////////////////////////////////////////////////////////////////

const char* LuaFileSaveBind::meta = "psyfilesavemeta";

int LuaFileSaveBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"show", show},
    {"setfilename", setfilename},
    {"filename", filename},
    {"setfolder", setfolder},
    {"isok", isok},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);
  return 1;
}

int LuaFileSaveBind::create(lua_State* L) {  
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) {
    return err;
  }  
  LuaHelper::new_shared_userdata<>(L, meta, new ui::FileSaveDialog());
  return 1;
}

int LuaFileSaveBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::FileSaveDialog>(L, meta);
}

int LuaFileSaveBind::show(lua_State* L) {
/*
  boost::shared_ptr<ui::FileSaveDialog> dlg
      = LuaHelper::check_sptr<ui::FileSaveDialog>(L, 1, meta);
  dlg->Show();
  try {
    LuaImport import(L, dlg.get(), 0);  
    if (import.open(dlg->is_ok() ? "onok" : "oncancel")) {
      import << dlg->filename() << pcall(0);
    }
  } catch (std::exception& e) {
    return luaL_error(L, e.what());
  }
*/
  return LuaHelper::chaining(L);
}
// LuaFileObserver + Bind
void LuaFileObserver::OnCreateFile(const std::string& path) { 
  /*struct {
    std::string path;
    lua_State* L;
    LuaFileObserver* that;
    void operator()() const {
      try {
        LuaImport in(L, that, locker(L));
        if (in.open("oncreatefile")) {
          lua_pushstring(L, path.c_str());
          in << pcall(0);      
        }
      } catch (std::exception& e) {
         ui::alert(e.what());   
      }
    }
   } f;   
  f.L = L;
  f.that = this;
  f.path = path;
  LuaGlobal::InvokeLater(L, f);  */
}

void LuaFileObserver::OnDeleteFile(const std::string& path) {
  /*struct {
    std::string path;
    lua_State* L;
    LuaFileObserver* that;
    void operator()() const {
      try {
        LuaImport in(L, that, locker(L));
        if (in.open("ondeletefile")) {
          lua_pushstring(L, path.c_str());
          in << pcall(0);      
        }
      } catch (std::exception& e) {
         ui::alert(e.what());   
      }
    }
   } f;   
  f.L = L;
  f.that = this;
  f.path = path;
  LuaGlobal::InvokeLater(L, f);*/
}

void LuaFileObserver::OnChangeFile(const std::string& path) {   
  /*struct {
    std::string path;
    lua_State* L;
    LuaFileObserver* that;
    void operator()() const {
      try {
        LuaImport in(L, that, locker(L));
        if (in.open("onchangefile")) {
          lua_pushstring(L, path.c_str());
          in << pcall(0);      
        }
      } catch (std::exception& e) {
         ui::alert(e.what());   
      }
    }
   } f;   
  f.L = L;
  f.that = this;
  f.path = path;
  LuaGlobal::InvokeLater(L, f);*/
}

const char* LuaFileObserverBind::meta = "psyfileobservermeta";

int LuaFileObserverBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {"setdirectory", setdirectory},
    {"directory", directory},
    {"startwatching", startwatching},
    {"stopwatching", stopwatching},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaFileObserverBind::create(lua_State* L) {  
  int n = lua_gettop(L);  // Number of arguments
  if (n != 1) {
    return luaL_error(L, "Got %d arguments expected 1 (self)", n);
  }  
  LuaHelper::new_shared_userdata<>(L, meta, new LuaFileObserver(L), 1, true);
  return 1;
}

int LuaFileObserverBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<LuaFileObserver>(L, meta);
  return 0;
}

int LuaFileObserverBind::setdirectory(lua_State* L) { 
  LUAEXPORTM(L, meta, &LuaFileObserver::SetDirectory);
}

int LuaFileObserverBind::directory(lua_State* L) { 
  LUAEXPORTM(L, meta, &LuaFileObserver::directory);
}

int LuaFileObserverBind::startwatching(lua_State* L) { 
  LUAEXPORTM(L, meta, &LuaFileObserver::StartWatching);
}

int LuaFileObserverBind::stopwatching(lua_State* L) { 
  LUAEXPORTM(L, meta, &LuaFileObserver::StopWatching);
}

// LuaSystemMetricsBind
int  LuaSystemMetrics::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"screendimension", screendimension},    
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  return 1;
}

int LuaSystemMetrics::screendimension(lua_State* L) {
  using namespace ui;
  LuaHelper::requirenew<LuaDimensionBind>(
      L, "psycle.ui.dimension", 
      new Dimension(Systems::instance().metrics().screen_dimension()));
  return 1;
}

void LuaCommand::Execute() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("execute")) {      
      in.pcall(0);
      in.close();      
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());    
  }
}

void LuaAligner::CalcDimensions() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("calcdimensions")) {      
      in.pcall(0);
      in.close();      
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());    
  }
}

void LuaAligner::SetPositions() {
	if (group() && group()->size() != 0) {		
		for (iterator i = begin(); i != end(); ++i) {
			(*i)->reset_align_dimension_changed();			
		}
	}

  try {
    LuaImport in(L, this, locker(L));
    if (in.open("setpositions")) {      
      in.pcall(0);
      in.close();      
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());    
  }
}

//  LuaAlignerBind
const char* LuaAlignerBind::meta = "psyalignermeta";

int LuaAlignerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
	{"windows", windows},
	{"setdimension", setdimension},
	{"groupdimension", groupdimension},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaAlignerBind::create(lua_State* L) {
  using namespace ui;  
  int n = lua_gettop(L);    
  if (n != 1) {
    luaL_error(L, "Aligner create. Wrong number of arguments.");
  }    
  LuaHelper::new_shared_userdata<>(L, meta, new LuaAligner(L));
  return 1;
}

int LuaAlignerBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaAligner>(L, meta);
}

int LuaAlignerBind::windows(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<LuaAligner> aligner = LuaHelper::check_sptr<LuaAligner>(L, 1, LuaAlignerBind::meta);
  lua_newtable(L); // create window table  
  ui::Window::iterator it = aligner->group()->begin();
  for (; it != aligner->group()->end(); ++it) {
    LuaHelper::find_userdata(L, (*it).get());
    lua_rawseti(L, 2, lua_rawlen(L, 2)+1); // windows[#windows+1] = newwindow
  }
  lua_pushvalue(L, 2);
  return 1;
}

int LuaAlignerBind::setdimension(lua_State* L) {
  boost::shared_ptr<LuaAligner> aligner = LuaHelper::check_sptr<LuaAligner>(L, 1, meta);
  boost::shared_ptr<ui::Dimension> dimension = LuaHelper::check_sptr<ui::Dimension>(L, 2, LuaDimensionBind::meta);
  aligner->set_dimension(*dimension);    
  return LuaHelper::chaining(L);
}

int LuaAlignerBind::groupdimension(lua_State* L) {
  boost::shared_ptr<LuaAligner> aligner = LuaHelper::check_sptr<LuaAligner>(L, 1, meta);
  LuaHelper::requirenew<LuaDimensionBind>(L, "psycle.ui.dimension",
      new ui::Dimension(aligner->group()->dim()));
  return 1;
}


//  LuaCenterShowStrategyBind
const char* LuaFrameAlignerBind::meta = "psyframealignermeta";

int LuaFrameAlignerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},        
    {"sizetoscreen", sizetoscreen},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaFrameAlignerBind::create(lua_State* L) {
  using namespace ui;  
  AlignStyle::Type alignment(AlignStyle::CENTER);
  int n = lua_gettop(L);  
  if (n == 2) {
    alignment = static_cast<AlignStyle::Type>(luaL_checkinteger(L, 2));
  } else 
  if (n != 1) {
    luaL_error(L, "FrameAligner create. Wrong number of arguments.");
  }    
  LuaHelper::new_shared_userdata<>(L, meta, new FrameAligner(alignment));
  return 1;
}

int LuaFrameAlignerBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::FrameAligner>(L, meta);
}

// LuaFrame + Bind
void LuaFrame::OnClose() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclose")) {      
      in.pcall(0);
      in.close();
      LuaHelper::collect_full_garbage(L);
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());    
  }
}

void LuaFrame::OnShow() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onshow")) {      
      in << pcall(0);      
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());   
  }  
}

void LuaFrame::OnContextPopup(ui::Event& ev, const ui::Point& mouse_point,
                              const ui::Node::Ptr& node) {
  using namespace ui;
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("oncontextpopup")) {      
      LuaHelper::requirenew<LuaEventBind>(L, "psycle.ui.canvas.event", &ev,
                                          true);
      LuaHelper::requirenew<LuaPointBind>(L, "psycle.ui.point",
                                          new Point(mouse_point));
      in.pcall(0);
    } 
  } catch (std::exception& e) {
    alert(e.what());   
  }
}

// LuaPopupFrame + Bind
void LuaPopupFrame::OnClose() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclose")) {      
      in.pcall(0);
      in.close();
      LuaHelper::collect_full_garbage(L);
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());   
  }  
}

void LuaPopupFrame::OnShow() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onshow")) {      
      in.pcall(0);      
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());   
  }  
}

// LuaViewportBind
template <class T>
int LuaViewportBind<T>::create(lua_State* L) { 
  using namespace ui; 
  Group::Ptr group = LuaWindowBind<>::testgroup(L);  
  Window::Ptr window = LuaHelper::new_shared_userdata(L, B::meta, new T(L));  
  window->set_aligner(boost::shared_ptr<Aligner>(new DefaultAligner()));  
  if (group) {    
    group->Add(window);
    LuaHelper::register_userdata(L, window.get());
  }
  return 1;
}

template <class T>
std::string LuaWindowBase<T>::GetType() const {
  std::string result;
  LuaImport in(L, (void*)(this), locker(L));
  try {
    if (in.open("typename")) {
      in << pcall(1) >> result;
    } else {
      result = T::GetType();
    }
  } catch(std::exception& e) {
    ui::alert(e.what());
  }
  return result;
}


template <class T>
bool LuaWindowBase<T>::SendEvent(lua_State* L,
                              const::std::string method,
                              ui::Event& ev, 
                              ui::Window& window) {
  LuaImport in(L, &window, locker(L));
  bool has_method = in.open(method);
  if (has_method) {
    ev.StopWorkParent();
    ui::Event* base_ev =  new ui::Event();    
    LuaHelper::requirenew<LuaEventBind>(L, "psycle.ui.canvas.event", base_ev);
    in.pcall(0);
    ev = *base_ev;    
  }
  LuaHelper::collect_full_garbage(L);
  return has_method;
}

template <class T>
bool LuaWindowBase<T>::SendKeyEvent(lua_State* L,
                                 const::std::string method,
                                 ui::KeyEvent& ev, 
                                 ui::Window& window) {
  LuaImport in(L, &window, locker(L));
  bool has_method = in.open(method);
  if (has_method) {
    ev.StopWorkParent();    
    LuaHelper::requirenew<LuaKeyEventBind>(L, "psycle.ui.canvas.keyevent", &ev, true);
    in.pcall(0);
    LuaHelper::collect_full_garbage(L);
  }  
  return has_method;
}

template <class T>
bool LuaWindowBase<T>::SendMouseEvent(lua_State* L,
                                   const::std::string method,
                                   ui::MouseEvent& ev, 
                                   ui::Window& window) {  
  LuaImport in(L, &window, locker(L));
  bool has_method = in.open(method);
  if (has_method) {
    ev.StopWorkParent();
    LuaHelper::requirenew<LuaMouseEventBind>(L, "psycle.ui.canvas.mouseevent", &ev, true);
    in.pcall(0);
    LuaHelper::collect_full_garbage(L);
    in.close();
  }   
  return has_method;
}

template <class T>
bool LuaWindowBase<T>::SendWheelEvent(lua_State* L,
                                      const::std::string method,
                                      ui::WheelEvent& ev, 
                                      ui::Window& window) {  
  LuaImport in(L, &window, locker(L));
  bool has_method = in.open(method);
  if (has_method) {
    ev.StopWorkParent();
    LuaHelper::requirenew<LuaWheelEventBind>(L, "psycle.ui.canvas.mouseevent", &ev, true);
    in.pcall(0);
    LuaHelper::collect_full_garbage(L);
    in.close();
  }   
  return has_method;
}

template <class T>
void LuaWindowBase<T>::OnSize(const ui::Dimension& dimension) {
  T::OnSize(dimension);
  if (dimension.width() == 1235) {
    int d(0);
  }
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onsize")) {
      LuaHelper::requirenew<LuaDimensionBind>(L, "psycle.ui.dimension", new ui::Dimension(dimension));
      in.pcall(0);
    }
  } catch(std::exception& e) {
    ui::alert(e.what());
  }
}

template <class T>
void LuaWindowBase<T>::set_properties(const ui::Properties& properties) {
  using namespace ui;  
  T::set_properties(properties);  
  LuaImport in(L, this, locker(L));
  try {
    if (in.open("setproperties")) {
      LuaHelper::new_lua_module(L, "psycle.orderedtable");      
      for (Properties::const_iterator it = properties.begin(); it != properties.end(); ++it) {    
        const Property& p = it->second;
        MultiType value = p.value();
        if (value.which() != 0) {
          LuaHelper::new_lua_module(L, "property");        
          if (value.which() == 1) {
            lua_pushnumber(L, boost::get<ARGB>(p.value()));
          } else 
          if (value.which() == 2) {
            lua_pushstring(L, p.string_value().c_str());            
          } else
          if (value.which() == 3) {
            lua_pushboolean(L, boost::get<bool>(p.value()));            
          } else {
            luaL_error(L, "Wrong Property type.");
          }
          lua_setfield(L, -2, "value_");
          if (p.stock_key() != -1) {
            lua_pushinteger(L, p.stock_key());
            lua_setfield(L, -2, "stock_key_");
          }
          lua_setfield(L, -2, it->first.c_str());
        }
      }
      in << pcall(0);
    }
  } catch(std::exception& e) {
    alert(e.what());
  }  
}

template<class T>
ui::Dimension LuaWindowBase<T>::OnCalcAutoDimension() const {
  using namespace ui;
  Dimension result; 
  LuaImport in(L, (void*) this, locker(L));
  try {
    if (in.open("oncalcautodimension")) {     
      in.pcall(1);
	  Dimension::Ptr dimension =
        LuaHelper::check_sptr<Dimension>(L, -1, LuaDimensionBind::meta);
      result = *dimension.get();
    } else {
      result = T::OnCalcAutoDimension();
    }
  } catch(std::exception& e) {
    ui::alert(e.what());
  }
  return result;
}

// LuaTreeView
void LuaTreeView::OnChange(const ui::Node::Ptr& node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onchange")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaTreeView::OnRightClick(const ui::Node::Ptr& node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onrightclick")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaTreeView::OnEditing(const ui::Node::Ptr& node, const std::string& text) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onediting")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in << text << pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaTreeView::OnEdited(const ui::Node::Ptr& node, const std::string& text) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onedited")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in << text << pcall(0);      
    }
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaTreeView::OnContextPopup(ui::Event& ev, const ui::Point& mouse_point, const ui::Node::Ptr& node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("oncontextpopup")) {      
      LuaHelper::requirenew<LuaEventBind>(L, "psycle.ui.canvas.event", &ev, true);
      LuaHelper::setfield(L, "x", mouse_point.x());
      LuaHelper::setfield(L, "y", mouse_point.y());
      if (node.get()) {
        LuaHelper::find_weakuserdata(L, node.get());
      } else {
        lua_pushnil(L);
      }
      lua_setfield(L, -2, "node");
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

// LuaListView
void LuaListView::OnChange(const ui::Node::Ptr& node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onchange")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaListView::OnRightClick(const ui::Node::Ptr& node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onrightclick")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaListView::OnEditing(const ui::Node::Ptr& node, const std::string& text) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onediting")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in << text << pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaListView::OnEdited(const ui::Node::Ptr& node, const std::string& text) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onedited")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in << text << pcall(0);      
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

// LuaNodeBind
std::string LuaNodeBind::meta = ui::Node::type();

// LuaPopupMenu
void LuaPopupMenu::OnMenuItemClick(boost::shared_ptr<ui::Node> node) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclick")) {
      LuaHelper::find_weakuserdata<>(L, node.get());
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

// LuaCanvas + Bind
template <class T>
int LuaViewportBind<T>::open(lua_State *L) {  
  LuaHelper::openex(L, B::meta, setmethods, B::gc);
  static const char* const e[] = {
    "AUTO", "MOVE", "NO_DROP", "COL_RESIZE", "ALL_SCROLL", "POINTER",
    "NOT_ALLOWED", "ROW_RESIZE", "CROSSHAIR", "PROGRESS", "E_RESIZE",
    "NE_RESIZE", "DEFAULT", "TEXT", "N_RESIZE", "NW_RESIZE", "HELP",
    "VERTICAL_TEXT", "S_RESIZE", "SE_RESIZE", "INHERIT", "WAIT",
    "W_RESIZE", "SW_RESIZE"
  };
  // set cursor enum
  lua_newtable(L);
  LuaHelper::buildenum(L, e, sizeof(e)/sizeof(e[0]));
  lua_setfield(L, -2, "CURSOR");
  return 1;
}

template <class T>
int LuaViewportBind<T>::showscrollbar(lua_State* L) {
  boost::shared_ptr<T> canvas = LuaHelper::check_sptr<T>(L, 1, B::meta);
//  canvas->show_scrollbar = true;
  return LuaHelper::chaining(L);
}

template <class T>
int LuaViewportBind<T>::setscrollinfo(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 2, "self, nposh, nposv");
  if (err!=0) return err;
  boost::shared_ptr<T> canvas = LuaHelper::check_sptr<T>(L, 1, B::meta);
  luaL_checktype(L, 2, LUA_TTABLE);
  lua_getfield(L, 2, "nposh");
  lua_getfield(L, 2, "nposv");
  // canvas->nposh = luaL_checknumber(L, -2);
  // canvas->nposv = luaL_checknumber(L, -1);
  // canvas->show_scrollbar = true;
  return LuaHelper::chaining(L);
}

template <class T>
int LuaViewportBind<T>::invalidateontimer(lua_State* L) {     
  boost::shared_ptr<T> canvas = LuaHelper::check_sptr<T>(L, 1, B::meta);
  canvas->SetSave(true);
  return LuaHelper::chaining(L);
}

template <class T>
int LuaViewportBind<T>::invalidatedirect(lua_State* L) {
  boost::shared_ptr<T> canvas = LuaHelper::check_sptr<T>(L, 1, B::meta);
  canvas->SetSave(false);
  return LuaHelper::chaining(L);
}
  
template class LuaViewportBind<LuaViewport>;

// LuaWindowBind
void LuaWindow::OnSize(double w, double h) {
  LuaImport in(L, this, locker(L));
  try {
    if (in.open("onsize")) {
      in << w << h << pcall(0);
    }
  } catch(std::exception& e) {
    ui::alert(e.what());
  }
}

bool LuaWindow::transparent() const {
  bool result(false);
  LuaImport in(L, const_cast<LuaWindow*>(this), locker(L));
  try {
    if (in.open("transparent")) {
      in << pcall(1) >> result;
    } else {
      result = LuaWindowBase<ui::Window>::transparent();
    }
  } catch(std::exception& e) {
    ui::alert(e.what());
  }
  return result;
}

template <class T>
boost::shared_ptr<ui::Group> LuaWindowBind<T>::testgroup(lua_State* L) {
  int n = lua_gettop(L);  // Number of arguments  
  boost::shared_ptr<LuaGroup> group;
  if (n==2 && !lua_isnil(L, 2)) {
    group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaGroupBind<>::meta);
    if (!group) {
      group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaViewportBind<>::meta);
      if (!group) {
        group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaHeaderGroupBind<>::meta);
        if (!group) {
          group = LuaHelper::check_sptr<LuaGroup>(L, 2, LuaScrollBoxBind<>::meta);
        }
      }
    }    
  }
  return group;
}
  
template <class T>
int LuaWindowBind<T>::create(lua_State* L) {  
  ui::Group::Ptr group = testgroup(L);    
  ui::Window::Ptr window = LuaHelper::new_shared_userdata(L, meta.c_str(), new T(L));
  if (group) {    
    group->Add(window);
    LuaHelper::register_userdata(L, window.get());
  }  
  return 1;
}

template <class T>
int LuaWindowBind<T>::gc(lua_State* L) {
  typedef boost::shared_ptr<T> SPtr;
  SPtr window = *(SPtr*) luaL_checkudata(L, 1, meta.c_str());
  if (!window->empty()) {
    ui::Window::Container subitems = window->SubItems();
    typename T::iterator it = subitems.begin();
    for (; it != subitems.end(); ++it) {      
      LuaHelper::unregister_userdata<>(L, (*it).get());          
    }
  }
  return LuaHelper::delete_shared_userdata<T>(L, meta.c_str());
}

template <class T>
int LuaWindowBind<T>::show(lua_State* L) { 
    int n = lua_gettop(L);
    if (n==1) {
      LUAEXPORT(L, &T::Show) 
    } else {
      boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
      boost::shared_ptr<ui::WindowShowStrategy> window_show_strategy 
          = LuaHelper::check_sptr<ui::WindowShowStrategy>(L, 2, LuaFrameAlignerBind::meta);
      window->Show(window_show_strategy);
    }
    return LuaHelper::chaining(L);
  }

template <class T>
int LuaWindowBind<T>::setalign(lua_State* L) {  
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  window->set_align(static_cast<ui::AlignStyle::Type>(luaL_checkinteger(L, 2)));
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::align(lua_State* L) {
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  lua_pushinteger(L, static_cast<int>(window->align()));
  return 1;
}

template <class T>
int LuaWindowBind<T>::setcursorposition(lua_State* L) {
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
  boost::shared_ptr<ui::Point> pt = LuaHelper::check_sptr<ui::Point>(L, 2, LuaPointBind::meta);
  window->SetCursorPosition(*pt.get());  
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::cursorposition(lua_State* L) {   
	boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
	LuaHelper::requirenew<LuaPointBind>(L, "psycle.ui.point", new ui::Point(window->CursorPosition()));
	return 1;    
}

template <class T>
int LuaWindowBind<T>::setpadding(lua_State* L) {
  using namespace ui;
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  BoxSpace::Ptr space = LuaHelper::check_sptr<BoxSpace>(L, 2, LuaBoxSpaceBind::meta);
  window->set_padding(*space);    
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::padding(lua_State* L) {
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
  LuaHelper::requirenew<LuaBoxSpaceBind>(L, "psycle.ui.boxspace", new ui::BoxSpace(window->padding()));
  return 1;
}

template <class T>
int LuaWindowBind<T>::setmargin(lua_State* L) {
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  boost::shared_ptr<ui::BoxSpace> space = LuaHelper::check_sptr<ui::BoxSpace>(L, 2, LuaBoxSpaceBind::meta);
  window->set_margin(*space);    
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::margin(lua_State* L) {
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
  LuaHelper::requirenew<LuaBoxSpaceBind>(L, "psycle.ui.boxspace", new ui::BoxSpace(window->margin()));
  return 1;
}

template <class T>
int LuaWindowBind<T>::setcursor(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "Wrong number of arguments.");
  if (err!=0) return err;
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  CursorStyle::Type style = (CursorStyle::Type) (int) luaL_checknumber(L, 2);
  window->SetCursor(style);
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::cursor(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "Wrong number of arguments.");
  if (err!=0) return err;
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  lua_pushinteger(L, static_cast<int>(window->cursor()));
  return 1;
}

template <class T>
int LuaWindowBind<T>::fls(lua_State* L) {
  using namespace ui;
  const int n=lua_gettop(L);
  if (n==1) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    window->FLS();        
  } else
  if (n==2) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    Region::Ptr rgn = LuaHelper::check_sptr<Region>(L, 2, LuaRegionBind::meta);
    if (rgn) {
      window->FLS(*rgn.get());
    }        
  } else
  if (n==5) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    double x = luaL_checknumber(L, 2);
    double y = luaL_checknumber(L, 3);
    double width = luaL_checknumber(L, 4);
    double height = luaL_checknumber(L, 5);
    Region rgn(Rect(Point(x, y), Dimension(width, height)));
    window->FLS(rgn);
  } else {
    luaL_error(L, "Wrong number of arguments.");
  }
  return LuaHelper::chaining(L);
}

template <class T>
int LuaWindowBind<T>::parent(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);  
  LuaHelper::find_weakuserdata<>(L, window->parent());
  return 1;
}

template <class T>
int LuaWindowBind<T>::root(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err != 0) {
    return err;
  }
  boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
  Window* root = window->root();
  LuaHelper::find_weakuserdata<>(L, root);
  return 1;
}

template class LuaWindowBind<LuaWindow>;

// LuaGroupBind

template <class T>
int LuaGroupBind<T>::open(lua_State *L) {
  return LuaHelper::openex(L, B::meta, setmethods, B::gc); 
}

template <class T>
int LuaGroupBind<T>::create(lua_State* L) {
  using namespace ui;
  int n = lua_gettop(L);  // Number of arguments  
  boost::shared_ptr<LuaGroup> group;
  if (n==2 && !lua_isnil(L, 2)) {
    group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaGroupBind<>::meta);
    if (!group) {     
      group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaHeaderGroupBind<>::meta);
      if (!group) {
        group = LuaHelper::check_sptr<LuaGroup>(L, 2, LuaViewportBind<>::meta);
      }     
    }    
  }
  boost::shared_ptr<T> window = LuaHelper::new_shared_userdata(L, B::meta.c_str(), new T(L));  
  window->set_aligner(boost::shared_ptr<Aligner>(new DefaultAligner()));  
  if (group) {    
    group->Add(window);
    LuaHelper::register_userdata(L, window.get());
  }  
  return 1;
}

template <class T>
ui::Window::Ptr LuaGroupBind<T>::test(lua_State* L, int index) {  
  using namespace ui;
  static const char* metas[] = {    
    LuaRectangleBoxBind<>::meta.c_str(),
    LuaViewportBind<>::meta.c_str(),
    LuaGroupBind<>::meta.c_str(),
    LuaHeaderGroupBind<>::meta.c_str(),
    LuaTextBind<>::meta.c_str(),
    LuaSplitterBind<>::meta.c_str(),
    LuaPicBind<>::meta.c_str(),
    LuaLineBind<>::meta.c_str(),
    LuaWindowBind<LuaWindow>::meta.c_str(),
    LuaButtonBind<LuaButton>::meta.c_str(),
    LuaGroupBoxBind<LuaGroupBox>::meta.c_str(),
    LuaRadioButtonBind<LuaRadioButton>::meta.c_str(),
    LuaEditBind<LuaEdit>::meta.c_str(),
    LuaScrollBarBind<LuaScrollBar>::meta.c_str(),
    LuaScintillaBind<LuaScintilla>::meta.c_str(),
    LuaComboBoxBind<LuaComboBox>::meta.c_str(),
    LuaTreeViewBind<LuaTreeView>::meta.c_str(),
    LuaListViewBind<LuaListView>::meta.c_str(),
    LuaScrollBoxBind<LuaScrollBox>::meta.c_str()
  };
  int size = sizeof(metas)/sizeof(metas[0]);
  Window::Ptr window;
  for (int i = 0; i < size; ++i) {
    window = LuaHelper::test_sptr<Window>(L, index, metas[i]);
    if (window) {
      break;
    }
  }  
  return window;
}

template <class T>
int LuaGroupBind<T>::removeall(lua_State* L) {  
  using namespace ui;
  Window::Ptr group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  std::vector<Window::Ptr> subitems = group->SubItems();
  typename T::iterator it = subitems.begin();
  for ( ; it != subitems.end(); ++it) {
    Window::Ptr subitem = *it;
    LuaHelper::unregister_userdata<>(L, subitem.get());
  }  
  group->RemoveAll();  
  return LuaHelper::chaining(L);
}

template <class T>
int LuaGroupBind<T>::remove(lua_State* L) {  
  using namespace ui;
  typename T::Ptr group = LuaHelper::check_sptr<T>(L, 1, B::meta);    
  Window::Ptr window = test(L, 2);
  if (window) {
    std::vector<Window::Ptr> subitems = window->SubItems();
    typename T::iterator it = subitems.begin();
    for ( ; it != subitems.end(); ++it) {
      Window::Ptr subitem = *it;
      LuaHelper::unregister_userdata<>(L, subitem.get());    
    }
    LuaHelper::unregister_userdata<>(L, window.get());
    group->Remove(window);      
  } else {
    luaL_error(L, "Argument is no canvas window.");
  }  
  return LuaHelper::chaining(L);
}

template <class T>
int LuaGroupBind<T>::add(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, window");
  if (err!=0) return err;
  typename T::Ptr group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  Window::Ptr window = test(L, 2);
  if (window) {
    try {     
      group->Add(window);
      LuaHelper::register_userdata(L, window.get());
    } catch(std::exception &e) {
      luaL_error(L, e.what());
    }
  } else {
    luaL_error(L, "Argument is no window.");
  }
  return LuaHelper::chaining(L);
}

template <class T>
int LuaGroupBind<T>::insert(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self, window, window");
  if (err != 0) {
    return err;
  }
  boost::shared_ptr<T> group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  Window::Ptr pos = test(L, 2);
  Window::Ptr window = test(L, 3);
  if (window) {
    try {
      Window::iterator  it = std::find(group->begin(), group->end(), pos);
      if (it != group->end() ) {
        group->Insert(it, window);        
      } else {
        group->Add(window);
      }      
      LuaHelper::register_userdata(L, window.get());
    } catch(std::exception &e) {
      luaL_error(L, e.what());
    }
  } else {
    luaL_error(L, "Argument is no window.");
  }
  return LuaHelper::chaining(L);
}

template <class T>
int LuaGroupBind<T>::intersect(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 3, "self, self, x1, y1 [,x2, y2]");
  if (err!=0) return err;
  // T* window = LuaHelper::check<T>(L, 1, meta);
  // double x = luaL_checknumber(L, 2);
  // double y = luaL_checknumber(L, 3);
  /*Window* res = window->intersect(x, y);
  if (res) {
    LuaHelper::find_userdata(L, res);
  } else {
    lua_pushnil(L);
  }*/
  return 1;
}

template <class T>
int LuaGroupBind<T>::getwindows(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;
  boost::shared_ptr<T> group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  lua_newtable(L); // create window table
  typename T::iterator it = group->begin();
  for (; it!=group->end(); ++it) {
    LuaHelper::find_userdata(L, (*it).get());
    lua_rawseti(L, 2, lua_rawlen(L, 2)+1); // windows[#windows+1] = newwindow
  }
  lua_pushvalue(L, 2);
  return 1;
}

template <class T>
int LuaGroupBind<T>::at(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, index");
  if (err!=0) {
    return err;
  }
  typename T::Ptr group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  int index = static_cast<int>(luaL_checkinteger(L, 2));
  try {
    Window::Ptr window = group->at(index + 1);
    LuaHelper::find_userdata(L, (window).get());
  } catch (std::exception& e) {
    luaL_error(L, e.what());
  }  
  return 1;
}

template <class T>
int LuaGroupBind<T>::setzorder(lua_State* L) {
/*  int err = LuaHelper::check_argnum(L, 3, "self, window, z");
  if (err!=0) return err;
  boost::shared_ptr<T> group = LuaHelper::check_sptr<T>(L, 1, meta);
  Window* window = test(L, 2);
  if (window) {
    int z  = luaL_checknumber(L, 3);
    group->set_zorder(window, z - 1); // transform from lua onebased to zerobased
  }*/
  return LuaHelper::chaining(L);
}

template <class T>
int LuaGroupBind<T>::zorder(lua_State* L) {  
  int err = LuaHelper::check_argnum(L, 2, "self, window");
  if (err != 0) {
    return err;
  }
  boost::shared_ptr<T> group = LuaHelper::check_sptr<T>(L, 1, B::meta);
  ui::Window::Ptr window = test(L, 2);
  if (window) {
    int z  = group->zorder(window);
    lua_pushnumber(L, z + 1); // transform from zerobased to lua onebased
  } else {
    return luaL_error(L, "Window not in group");
  }  
  return 1;
}

template class LuaGroupBind<LuaGroup>;
template class LuaHeaderGroupBind<LuaHeaderGroup>;

// LuaFontInfoBind
std::string LuaFontInfoBind::meta = "fontinfometa";

int LuaFontInfoBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {  
    {"new", create},    
    {"setfamily", setfamily},
    {"family", family},
    {"setsize", setsize},
    {"size", size},
    {"style", style},
    {"tostring", tostring},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods, gc);  
}

int LuaFontInfoBind::create(lua_State* L) {  
  using namespace ui;
  int n = lua_gettop(L);
  if (n==1) {
    LuaHelper::new_shared_userdata<>(L, meta,  new FontInfo());
  } else 
  if (n==2) {
    const char* family_name = luaL_checkstring(L, 2);
    LuaHelper::new_shared_userdata<>(L, meta,  new FontInfo(family_name));
  } else
  if (n==3) {
    const char* family_name = luaL_checkstring(L, 2);
    int size  = luaL_checkinteger(L, 3);    
    LuaHelper::new_shared_userdata<>(L, meta,  new FontInfo(family_name, size));
  } else 
  if (n==4) {
    const char* family_name = luaL_checkstring(L, 2);
    int size  = luaL_checkinteger(L, 3);    
    int style  = luaL_checkinteger(L, 4);    
    FontInfo* font_info = new FontInfo(family_name, size);
    font_info->set_style(static_cast<FontStyle::Type>(style));
    LuaHelper::new_shared_userdata<>(L, meta,  font_info);
  } else {
    luaL_error(L, "Wrong Number of Arguments.");
  }   
  return 1;  
}

int LuaFontInfoBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::FontInfo>(L, meta);  
  return 0;
}

int LuaFontInfoBind::tostring(lua_State* L) {  
  using namespace ui;
  FontInfo::Ptr font_info = LuaHelper::check_sptr<FontInfo>(L, 1, meta);
  lua_pushstring(L, font_info->tostring().c_str());
  return 1;
}

// LuaFontBind
const char* LuaFontBind::meta = "psyfontsmeta";

int LuaFontBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {  
    {"new", create},
    {"setfontinfo", setfontinfo},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);  
}

int LuaFontBind::create(lua_State* L) {
  ui::Font* font = ui::Systems::instance().CreateFont();  
  LuaHelper::new_shared_userdata<>(L, meta, font);
  return 1;  
}

int LuaFontBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::Font>(L, meta);  
  return 0;
}

int LuaFontBind::setfontinfo(lua_State* L) {
  using namespace ui;
  Font::Ptr font = LuaHelper::check_sptr<Font>(L, 1, meta);
  FontInfo::Ptr font_info = LuaHelper::check_sptr<FontInfo>(L, 2, LuaFontInfoBind::meta);    
  font->set_font_info(*font_info.get());    
  return LuaHelper::chaining(L);
}

// LuaFontsBind
const char* LuaFontsBind::meta = "psyfontsmeta";

int LuaFontsBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {  
    {"new", create},
    {"fontlist", fontlist},
    {"importfont", importfont},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);  
}

int LuaFontsBind::create(lua_State* L) {
  ui::Fonts* fonts = ui::Systems::instance().CreateFonts();  
  LuaHelper::new_shared_userdata<>(L, meta, fonts);
  return 1;  
}

int LuaFontsBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::Fonts>(L, meta);  
  return 0;
}

int LuaFontsBind::fontlist(lua_State* L) {
  using namespace ui;
  boost::shared_ptr<Fonts> fonts = LuaHelper::check_sptr<Fonts>(L, 1, meta);  
  lua_newtable(L);
  Fonts::iterator it = fonts->begin();
  for (int i = 0; it != fonts->end(); ++it, ++i) {    
    lua_pushstring(L, (*it).c_str());
    lua_rawseti(L, -2, i);
  }
  return 1;
}

// LuaGraphicsBind
const char* LuaGraphicsBind::meta = "psygraphicsmeta";

int LuaGraphicsBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {  
    {"new", create},
    {"translate", translate},
	{"retranslate", retranslate},
	{"cleartranslation", cleartranslations},
    {"setcolor", setcolor},
    {"setfill", setfill},
    {"setstroke", setstroke},
    {"color", color},
	{"plot", plot},
    {"drawline", drawline},
    {"drawcurve", drawcurve},
    {"drawarc", drawarc},    
    {"drawrect", drawrect},    
    {"drawoval", drawoval},
    {"drawcircle", drawcircle},
    {"drawellipse", drawellipse},
    {"drawroundrect", drawroundrect},
    {"fillrect", fillrect},
    {"fillroundrect", fillroundrect},
    {"filloval", filloval},
    {"fillcircle", fillcircle},
    {"fillellipse", fillellipse},
    {"copyarea", copyarea},
    {"drawstring", drawstring},
    {"setfont", setfont},
    {"font", font},
    {"textdimension", textdimension}, 
    {"drawpolygon", drawpolygon},
    {"fillpolygon", fillpolygon},
    {"drawpolyline", drawpolyline},
    {"drawimage", drawimage},   
    {"beginpath", beginpath},
    {"endpath", endpath},
    {"fillpath", fillpath},
    {"drawpath", drawpath},
    {"drawfillpath", drawfillpath},
    {"moveto", moveto},
    {"lineto", lineto},
    {"curveto", curveto},
    {"arcto", arcto},
	{"dispose", dispose},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);  
}

int LuaGraphicsBind::create(lua_State* L) {
  using namespace ui;
  Graphics* g(0);
  int n = lua_gettop(L);
  if (n==1) {
    g = Systems::instance().CreateGraphics(); 
  } else if (n==2) {
    Image::Ptr image = LuaHelper::check_sptr<Image>(L, 2, LuaImageBind::meta);
	g = Systems::instance().CreateGraphics(image);
  } else {
    luaL_error(L, "Wrong number of arguments. (self [, psycle.ui.image])");
  }
  LuaHelper::new_shared_userdata<>(L, meta, g);
  return 1;  
}

int LuaGraphicsBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::Graphics>(L, meta);  
  return 0;
}

int LuaGraphicsBind::drawstring(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "");
  if (err != 0) {
    return err;
  }  
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  const char* str = luaL_checkstring(L, 2);
  Point::Ptr pos = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
  g->DrawString(str, *pos.get());
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::setfont(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, font");
   if (err != 0) {
    return err;
  }
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  Font::Ptr font = LuaHelper::check_sptr<Font>(L, 2, LuaFontBind::meta);  
  g->SetFont(*font.get());
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::font(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err != 0) {
    return err;
  }
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);  
  LuaHelper::requirenew<LuaFontBind>(L, "psycle.ui.font", new Font(g->font()));
  return 1;
}

int LuaGraphicsBind::textdimension(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "");
  if (err != 0) {
    return err;
  }
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);  
  const char* text = luaL_checkstring(L, 2);  
  LuaHelper::requirenew<LuaDimensionBind>(L, "psycle.ui.dimension",
      new Dimension(g->text_dimension(text)));
  return 1;
}

int LuaGraphicsBind::drawpolygon(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self, xpoints, ypoints");
  if (err !=0 ) {
    return err;
  }
  boost::shared_ptr<Graphics> g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  luaL_checktype(L, 2, LUA_TTABLE);
  size_t n1 = lua_rawlen(L, 2);
  size_t n2 = lua_rawlen(L, 2);
  if (n1 != n2) {
    return luaL_error(L, "Length of x and y points have to be the same size.");
  }
  Points points;
  for (size_t i=1; i <= n1; ++i) {
    lua_rawgeti(L, 2, i);
    lua_rawgeti(L, 3, i);
    double x = luaL_checknumber(L, -2);
    double y = luaL_checknumber(L, -1);
    points.push_back(Point(x, y));
    lua_pop(L, 2);
  }
  g->DrawPolygon(points);
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::drawpolyline(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self, xpoints, ypoints");
  if (err!=0) {
    return err;
  }
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  luaL_checktype(L, 2, LUA_TTABLE);
  size_t n1 = lua_rawlen(L, 2);
  size_t n2 = lua_rawlen(L, 2);
  if (n1!=n2) return luaL_error(L, "Length of x and y points have to be the same size.");
  Points points;
  for (size_t i=1; i <= n1; ++i) {
    lua_rawgeti(L, 2, i);
    lua_rawgeti(L, 3, i);
    double x = luaL_checknumber(L, -2);
    double y = luaL_checknumber(L, -1);
    points.push_back(Point(x, y));
    lua_pop(L, 2);
  }
  g->DrawPolyline(points);
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::fillpolygon(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self, xpoints, ypoints");
  if (err!=0) {
    return err;
  }
  boost::shared_ptr<Graphics> g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  luaL_checktype(L, 2, LUA_TTABLE);
  size_t n1 = lua_rawlen(L, 2);
  size_t n2 = lua_rawlen(L, 2);
  if (n1 != n2) {
    return luaL_error(L, "Length of x and y points have to be the same size.");
  }
  Points points;
  for (size_t i=1; i <=n1; ++i) {
    lua_rawgeti(L, 2, i);
    lua_rawgeti(L, 3, i);
    double x = luaL_checknumber(L, -2);
    double y = luaL_checknumber(L, -1);
    points.push_back(Point(x, y));
    lua_pop(L, 2);
  }
  g->FillPolygon(points);
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::drawimage(lua_State* L) {
  using namespace ui;
  int n = lua_gettop(L);
  if (n!=3 && n!=4) {
    return luaL_error(L, "Wrong number of arguments.");
  }
  Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 2, LuaImageBind::meta);  
  if (n==3) {
    Point::Ptr pos = LuaHelper::test_sptr<Point>(L, 3, LuaPointBind::meta);  
    if (pos.get()) {
      g->DrawImage(img.get(), *pos.get());
    } else {
      Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 3, LuaUiRectBind::meta);
      g->DrawImage(img.get(), *pos.get());
    }
  } else
  if (n==4) {    
    Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 3, LuaUiRectBind::meta);
    Point::Ptr src = LuaHelper::check_sptr<Point>(L, 4, LuaPointBind::meta);      
    g->DrawImage(img.get(), *pos.get(), *src.get());
  }
  return LuaHelper::chaining(L);
}

int LuaGraphicsBind::plot(lua_State *L) {
    int n = lua_gettop(L);
	using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
	PSArray* y = *(PSArray **)luaL_checkudata(L, 2, LuaArrayBind::meta);		
	double zoom = 1;
	if (n==3) {
	  zoom = luaL_checknumber(L, 3);
	}
	int num = y->len();
	if (num > 0) {
		g->MoveTo(Point(0, y->get_val(0)));	
		int xl = 0;
		int step = 1;
		for (int x = 1; x < num; x = x + step, ++xl) {
		  g->LineTo(Point(xl * zoom, y->get_val(x)));
		}
	}
	return LuaHelper::chaining(L);
}

// LuaRun + Bind
void LuaRun::Run() {  
  LuaImport in(L, this, 0);  
  if (in.open("run")) {    
    in.pcall(0);
  }  
}

std::string LuaRunBind::meta = "psyrunbind";

int LuaRunBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaRunBind::create(lua_State* L) {  
  LuaHelper::new_shared_userdata(L, meta, new LuaRun(L));
  return 1;
}

int LuaRunBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaRun>(L, meta);
}

std::string LuaCommandBind::meta = "psycommandbind";

int LuaCommandBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaCommandBind::create(lua_State* L) {  
  LuaHelper::new_shared_userdata(L, meta, new LuaCommand(L));
  return 1;
}

int LuaCommandBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaCommand>(L, meta);
}
 
// LuaWindow + Bind
void LuaWindow::Draw(ui::Graphics* g, ui::Region& draw_rgn) {    
  LuaImport in(L, this, locker(L));  
  if (in.open("draw")) {        
    LuaHelper::requirenew<LuaGraphicsBind>(L, "psycle.ui.canvas.graphics", g, true);
    LuaHelper::requirenew<LuaRegionBind>(L, "psycle.ui.region", &draw_rgn, true);
    in.pcall(0);    
//    LuaHelper::collect_full_garbage(L);
  }
}

// LuaScopeWindow

LuaScopeWindow::LuaScopeWindow(lua_State* L) : 
		LuaWindow(L),
		scope_width_(88),
		scope_height_(50),
		scale_x_(10),
		scope_mid_(25), 		
		mac_count_(0),
		clip_(false),
		hold(false) {
	for(int c=0; c<MAX_MACHINES; ++c) {
		scope_memories_.push_back(boost::shared_ptr<ScopeMemory>());
	}  
	add_ornament(ui::Ornament::Ptr(ui::OrnamentFactory::Instance().CreateFill(0xFF232323)));
	ViewDoubleBuffered();
	scope_peak_rate = 20;
	scope_osc_freq = 14;
	scope_osc_rate = 20;
	scope_spec_rate = 20;
	scope_spec_mode = 2;
	scope_spec_samples = 2048;
	scope_phase_rate = 20;
	StartTimer();
}

void LuaScopeWindow::Draw(ui::Graphics* g, ui::Region& draw_rgn) {    
  using namespace ui;  
  g->set_color(0xFFFFFFFF);  
  Song& song = Global::song();
  double y = 0;
  double x = 0;   
  int count = MachineCount();
  if (count != mac_count_) {
    CalcScopeSize(count, position().dimension());
	mac_count_ = count;
  }   
  for(int c=0; c<MAX_MACHINES; ++c) {
    Machine* mac = song._pMachine[c];
	if (mac) {	  
	  if (!mac->HasScopeMemories()) {	    
		scope_memories_[c] = boost::shared_ptr<ScopeMemory>(new ScopeMemory());
        mac->AddScopeMemory(scope_memories_[c]);  
	  }	  
	  DrawScope(g, x, y, mac);	  	  
	  x += scope_width_;
	  if (x >= position().width()) {
	    x = 0;
		y += scope_height_;
	  } 
	 }
  }
  g->set_color(0xFF666666);
  g->DrawLine(Point(0, scope_height_),  Point(position().width(),scope_height_));
}

const COLORREF CLBARDC =   0x1010DC;
const COLORREF CLBARPEAK = 0xC0C0C0;
const COLORREF CLLEFT =    0xC06060;
const COLORREF CLRIGHT =   0x60C060;
const COLORREF CLBOTH =    0xC0C060;

void LuaScopeWindow::DrawScope(ui::Graphics* g, double x, double y, Machine* mac) {    
/*
	using namespace ui;  
	char buf[4];
	sprintf(buf, "%02X:", mac->_macIndex);	
	g->set_color(0xFFFFFFFF);	  
	g->DrawString(buf + std::string(mac->GetEditName()), Point(x + 4, y + 10));
	if (!mac->Mute()) {	    
	    g->Translate(Point(x, y - scope_height_/2));
		ScopeMemory& scope_memory = *scope_memories_[mac->_macIndex].get();
		pSamplesL = scope_memory.samples_left->data();
		pSamplesR = scope_memory.samples_right->data();	
	    ui::mfc::GraphicsImp* mfc_dc = (ui::mfc::GraphicsImp*) (g->imp());
	    CDC& bufDC = *mfc_dc->dev_dc();
		Machine& srcMachine = *mac;
		float invol = 1.0f;
		float mult = 1.0f / srcMachine.GetAudioRange();
		const int SCOPE_BUF_SIZE = scope_memory.scope_buf_size;
		// ok this is a little tricky - it chases the wrapping buffer, starting at the last sample 
		// buffered and working backwards - it does it this way to minimize chances of drawing 
		// erroneous data across the buffering point
		// scopeBufferIndex is the position where it will write new data. Pos is the buffer position on Hold.
		// keeping nOrig, since the buffer can change while we draw it.
		int nOrig  = (scope_memory.scopeBufferIndex == 0) ? scope_memory.scope_buf_size-scope_memory.pos 
														  : scope_memory.scopeBufferIndex-scope_memory.pos;
		if (nOrig < 0.f) { nOrig+=scope_memory.scope_buf_size; }
		// osc_freq range 5..100, freq range 12..5K
		float num_samples = (float) scope_width_;
		const int freq = (scope_osc_freq*scope_osc_freq)>>1;
		const float add = (float(Global::player().SampleRate())/float(freq))/num_samples;
		const float multleft= invol*mult *srcMachine._lVol;
		const float multright= invol*mult *srcMachine._rVol;

		int sizey = scope_height_;

		RECT rect = {0,0,1,0};
		float n=nOrig;
		for (int x = static_cast<int>(num_samples); x >= 0; x--, rect.left += 1, rect.right += 1) {		    
			n -= add;
			if (n < 0.f) { n+=SCOPE_BUF_SIZE; }							
			int aml = GetY(resampler.work_float(pSamplesL,n,SCOPE_BUF_SIZE, NULL,pSamplesL, pSamplesL+SCOPE_BUF_SIZE-1),multleft);
			int amr = GetY(resampler.work_float(pSamplesR,n,SCOPE_BUF_SIZE, NULL,pSamplesR, pSamplesR+SCOPE_BUF_SIZE-1),multright);
			int smaller, bigger, halfbottom, bottompeak;
			COLORREF colourtop, colourbottom;
			if (aml < amr) { smaller=aml; bigger=amr; colourtop=CLLEFT; colourbottom=CLRIGHT; }
			else {			smaller=amr; bigger=aml; colourtop=CLRIGHT; colourbottom=CLLEFT; }
			if (smaller < sizey ){
				rect.top=smaller;
				if (bigger < sizey) {
					rect.bottom=bigger;
					halfbottom=bottompeak=sizey;
				}
				else {
					rect.bottom=halfbottom=sizey;
					bottompeak=bigger;
				}
			}
			else {
				rect.top=rect.bottom=sizey;
				halfbottom=smaller;
				bottompeak=bigger;
			}
			if (x % 2 == 0) { // raster scope				
				bufDC.FillSolidRect(&rect,colourtop);
				rect.top = rect.bottom;
				rect.bottom = halfbottom;
				bufDC.FillSolidRect(&rect,CLBOTH);
				rect.top = rect.bottom;
				rect.bottom = bottompeak;
				bufDC.FillSolidRect(&rect,colourbottom);
			}
		}
		// red line if last frame was clipping
		if (clip_)
		{
			RECT rect = {0,32,256,34};
			bufDC.FillSolidRect(&rect,0x00202040);
			rect.top = sizey + 31;
			rect.bottom = rect.top+2;
			bufDC.FillSolidRect(&rect,0x00202040);
			rect.top = sizey-4;
			rect.bottom = rect.top+8;
			bufDC.FillSolidRect(&rect,0x00202050);
			rect.top = sizey-2;
			rect.bottom = rect.top+4;
			bufDC.FillSolidRect(&rect,0x00101080);
			clip_ = FALSE;
		}
		scope_memory.pos = 1;
		g->Translate(Point(-x, -y + scope_height_/2));
	} else {		
		ui::Dimension text_dimension = g->text_dimension("Muted");
		Point center = Point(x + (scope_width_ - text_dimension.width()) / 2,
							 y + (scope_height_ - text_dimension.height()) / 2);
		g->set_color(0xFFFFD24D);
		g->DrawString("Muted", center);
	}
	g->set_color(0xFF666666);
	g->DrawLine(Point(x + scope_width_, 0),  Point(x + scope_width_, y + scope_height_));
*/
}

int LuaScopeWindow::GetY(float f, float mult) {	
	f=scope_height_-(f*scope_height_*mult);
	if (f < 1.f)  {
		clip_ = TRUE;
		return 1;
	} else if (f > scope_height_*2) {
		clip_ = TRUE;
		return 2*scope_height_;
	}
	return int(f);
}

void LuaScopeWindow::OnSize(const ui::Dimension& dimension) {
  CalcScopeSize(MachineCount(), dimension);
}

void LuaScopeWindow::CalcScopeSize(int mac_num, const ui::Dimension& dimension) {
  if ((mac_num % 2) != 0) {
      ++mac_num;
  }
  scope_width_ = dimension.width() / mac_num * 2;
  scope_height_ = dimension.height() / 2;
  scope_mid_ = scope_height_ / 2;
  scale_x_ = 880 / scope_width_;
}

int LuaScopeWindow::MachineCount() const {
	int result(0);
	for (int i = 0; i < MAX_MACHINES; ++i) {
		if (Global::song()._pMachine[i] != 0) {
			++result;
		}
	}
	return result;
}

void LuaScopeWindow::OnTimerViewRefresh() {
  FLS();
}

void LuaScopeWindow::OnMouseDown(ui::MouseEvent& ev) {
  ui::Point pt = ev.client_pos() - absolute_position().top_left();
  Machine* machine = HitTest(pt);
  if (machine) {
	if (ev.button() == 1) {	  	  	  
	  machine->Mute(!machine->Mute());
	} else {
		Song& song = Global::song();
		bool mute(true);
		if (machine->Mute()) {		  
		  mute = !machine->Mute();		  
		}
		for(int c=0; c<MAX_MACHINES; ++c) {  
			if (c != 128 && machine->_macIndex != c) {
				Machine* mac = song._pMachine[c];     
				if (mac) {
					mac->Mute(mute);
				}
			}
		}
	}
  }
  HostExtensions::Instance().FlsMain();
}

Machine* LuaScopeWindow::HitTest(const ui::Point& pos) const {
  Song& song = Global::song();
  Machine* result(0);
  int offset = position().width() / scope_width_;
  int col = pos.x() / scope_width_;
  int row = pos.y() / scope_height_;  
  int scope_idx = offset*row + col;
  int n(0);
  for(int c=0; c<MAX_MACHINES; ++c) {	
    Machine* mac = song._pMachine[c];
	if (mac) {
	 if (n == scope_idx) {
	   result = mac;
	   break;
	 }
	 ++n;
	}
  }
  return result;
}

// LuaImageBind
const char* LuaImageBind::meta = "psyimagebind";

int LuaImageBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"reset", reset},
    {"load", load},
    {"cut", cut},
    {"save", save},
    {"settransparent", settransparent},
    {"dimension", dimension},
    {"resize", resize},
    {"rotate", rotate},
    {"graphics", graphics},
    {"setpixel", setpixel},
    {"getpixel", getpixel},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaImageBind::create(lua_State* L) {  
  LuaHelper::new_shared_userdata<>(L, meta, ui::Systems::instance().CreateImage());
  return 1;
}

int LuaImageBind::gc(lua_State* L) {  
  return LuaHelper::delete_shared_userdata<ui::Image>(L, meta);
}

int LuaImageBind::graphics(lua_State * L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err != 0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  std::auto_ptr<Graphics> g = img->graphics();  
  LuaHelper::requirenew<LuaGraphicsBind>(L, "psycle.ui.canvas.graphics", g.get());  
  g.release();
  return 1;
}

int LuaImageBind::setpixel(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 4, "self, color");
  if (err != 0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  ARGB argb = static_cast<ARGB>(luaL_checknumber(L, 4));
  img->SetPixel(Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)), argb);
  return LuaHelper::chaining(L);
}

int LuaImageBind::getpixel(lua_State * L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self");
  if (err != 0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  ARGB color = img->GetPixel(ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
  lua_pushnumber(L, static_cast<int>(color));
  return 1;
}

int LuaImageBind::save(lua_State * L) {
  using namespace ui;
  int n = lua_gettop(L);
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);  
  if (n == 1) {
    try {
      img->Save();
    }
    catch (std::exception& e) {
      std::stringstream str;
      str << "File Save Error: " << e.what();     
      luaL_error(L, str.str().c_str());
    }
  } else
  if (n == 2) {
    try {
      const char* filename = luaL_checkstring(L, 2);
      img->Save(filename);
    }
    catch (std::exception& e) {
      std::stringstream str;
      str << "File Save Error: " << e.what();
      luaL_error(L, str.str().c_str());     
    }
  }
  return LuaHelper::chaining(L);
}

int LuaImageBind::cut(lua_State * L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 5, "self, x, y, w, h");
  if (err!=0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);  
  img->Cut(Rect(Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)),
                Dimension(luaL_checknumber(L, 4), luaL_checknumber(L, 5))));
  return LuaHelper::chaining(L);  
}

int LuaImageBind::resize(lua_State * L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 3, "self, w, h");
  if (err != 0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  img->Resize(Dimension(luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
  return LuaHelper::chaining(L);
}

int LuaImageBind::rotate(lua_State * L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, radians");
  if (err != 0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  img->Rotate(static_cast<float>(luaL_checknumber(L, 2)));
  return LuaHelper::chaining(L);
}

int LuaImageBind::settransparent(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, color");
  if (err!=0) {
    return err;
  }
  Image::Ptr img = LuaHelper::check_sptr<Image>(L, 1, meta);
  ARGB argb = static_cast<ARGB>(luaL_checknumber(L, 2));
  img->SetTransparent(true, argb);
  return LuaHelper::chaining(L);
}

int LuaImageBind::reset(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self, dimension");
  if (err != 0) {
    return err;
  }
  Image::Ptr image = LuaHelper::check_sptr<Image>(L, 1, meta); 
  Dimension::Ptr dimension =
        LuaHelper::check_sptr<Dimension>(L, 2, LuaDimensionBind::meta); 
  image->Reset(*dimension.get());
  return LuaHelper::chaining(L);
}

int LuaImageBind::dimension(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err != 0) {
    return err;
  }
  Image::Ptr image = LuaHelper::check_sptr<Image>(L, 1, meta);
  LuaHelper::requirenew<LuaDimensionBind>(
      L, "psycle.ui.dimension", 
      new Dimension(image->dim()));
  return 1;
}

std::string LuaImagesBind::meta = "psyimagesbind";

// PointBind
std::string LuaPointBind::meta = "psypointmeta";

int LuaPointBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {    
    {"new", create},
    {"setxy", setxy},
    {"setx", setx},
    {"x", x},
    {"sety", sety},
    {"y", y},
	{"offset", offset},
    {"add", add},
	{"sub", sub},
	{"mul", mul},
	{"div", div},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaPointBind::create(lua_State* L) {
  using namespace ui;  
  int n = lua_gettop(L);
  if (n == 1) {
    LuaHelper::new_shared_userdata<>(L, meta, new Point());
  } else
  if (n == 2) {
    Point::Ptr other = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta); 
    LuaHelper::new_shared_userdata<>(L, meta, new Point(*other.get()));
  } else
  if (n == 3) {
    double x = luaL_checknumber(L, 2);
    double y = luaL_checknumber(L, 3);
    LuaHelper::new_shared_userdata<>(L, meta, new Point(x, y));
  } else {
    luaL_error(L, "Wrong number of arguments");
  }
  return 1;
}

int LuaPointBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Point>(L, meta);
}

int LuaPointBind::add(lua_State *L) {
	using namespace ui;
	Point::Ptr self = LuaHelper::check_sptr<Point>(L, 1, LuaPointBind::meta);
	Point::Ptr rhs = LuaHelper::test_sptr<Point>(L, 2, LuaPointBind::meta); 
	if (rhs) {
	  *self.get() += *rhs.get();
	} else {
	  double a = luaL_checknumber(L, 2);
	  *self.get() += Point(a, a);
	}
	return LuaHelper::chaining(L);
}

int LuaPointBind::sub(lua_State *L) {
	using namespace ui;
	Point::Ptr self = LuaHelper::check_sptr<Point>(L, 1, LuaPointBind::meta);
	Point::Ptr rhs = LuaHelper::test_sptr<Point>(L, 2, LuaPointBind::meta); 
	if (rhs) {
	  *self.get() -= *rhs.get();
	} else {
	  double a = luaL_checknumber(L, 2);
	  *self.get() -= Point(a, a);
	}
	return LuaHelper::chaining(L);
}

int LuaPointBind::mul(lua_State *L) {
	using namespace ui;
	Point::Ptr self = LuaHelper::check_sptr<Point>(L, 1, LuaPointBind::meta);
	Point::Ptr rhs = LuaHelper::test_sptr<Point>(L, 2, LuaPointBind::meta); 
	if (rhs) {
	  *self.get() *= *rhs.get();
	} else {
	  double a = luaL_checknumber(L, 2);
	  *self.get() *= Point(a, a);
	}
	return LuaHelper::chaining(L);
}

int LuaPointBind::div(lua_State *L) {
	using namespace ui;
	Point::Ptr self = LuaHelper::check_sptr<Point>(L, 1, LuaPointBind::meta);
	Point::Ptr rhs = LuaHelper::test_sptr<Point>(L, 2, LuaPointBind::meta); 
	if (rhs) {
	  if (rhs->x() == 0 || rhs->y() == 0) {
	    return luaL_error(L, "Math error. Division by zero."); 
	  }
	  *self.get() /= *rhs.get();
	} else {
	  double a = luaL_checknumber(L, 2);
	  if (a == 0) {
	    return luaL_error(L, "Math error. Division by zero."); 
	  }
	  *self.get() /= Point(a, a);
	}
	return LuaHelper::chaining(L);
}

// DimensionBind
std::string LuaDimensionBind::meta = "psydimensionmeta";

int LuaDimensionBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"set", set},
    {"setwidth", setwidth},
    {"width", width},
    {"setheight", setheight},
    {"height", height},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaDimensionBind::create(lua_State* L) {
  int n = lua_gettop(L);
  if (n == 1) {
    LuaHelper::new_shared_userdata<>(L, meta, new ui::Dimension());
  } else
  if (n == 3) {
    double width = luaL_checknumber(L, 2);
    double height = luaL_checknumber(L, 3);
    LuaHelper::new_shared_userdata<>(L, meta, new ui::Dimension(width, height));
  } else {
    luaL_error(L, "Wrong number of arguments");
  }
  return 1;
}

int LuaDimensionBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Dimension>(L, meta);
}

// RectBind
std::string LuaUiRectBind::meta = "psyuirectmeta";

int LuaUiRectBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
	{"setleft", setleft},
    {"left", left},
	{"settop", settop},
    {"top", top},
	{"setright", setright},
    {"right", right},
	{"setbottom", setbottom},
    {"bottom", bottom},
	{"setwidth", setwidth},
    {"width", width},
	{"setheight", setheight},
    {"height", height},
	{"dimension", dimension},
	{"settopleft", settopleft},
    {"topleft", topleft},
	{"setbottomright", setbottomright},
    {"bottomright", bottomright},
    {"intersect", intersect},
	{"offset", offset},
	{"move", move},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaUiRectBind::create(lua_State* L) {
  using namespace ui;
  int n = lua_gettop(L);
  if (n == 1) {
    LuaHelper::new_shared_userdata<>(L, meta, new Rect());
  } else
  if (n == 2) {
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
	LuaHelper::new_shared_userdata<>(L, meta, new Rect(*rect.get()));
  } else
  if (n == 3) {    
    Point::Ptr top_left = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
	Point::Ptr bottom_right = LuaHelper::test_sptr<Point>(L, 3, LuaPointBind::meta);
	if (bottom_right) {
	  LuaHelper::new_shared_userdata<>(L, meta, new Rect(*top_left.get(), *bottom_right.get()));
	} else {
      Dimension::Ptr dimension =
        LuaHelper::check_sptr<Dimension>(L, 3, LuaDimensionBind::meta);
      LuaHelper::new_shared_userdata<>(L, meta, new Rect(*top_left.get(), *dimension.get()));
	}   
  } else {
    luaL_error(L, "Wrong number of arguments");
  }  
  return 1;
}

int LuaUiRectBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Rect>(L, meta);
}

int LuaUiRectBind::set(lua_State* L) {
  using namespace ui;
  Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
  int n = lua_gettop(L);
  if (n==3) {    
    Point::Ptr top_left = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
	Point::Ptr bottom_right = LuaHelper::test_sptr<Point>(L, 3, LuaPointBind::meta);
	if (bottom_right) {
	  rect->set(*top_left.get(), *bottom_right.get());
	} else {
      Dimension::Ptr dimension =
        LuaHelper::check_sptr<Dimension>(L, 3, LuaDimensionBind::meta);
      rect->set(*top_left.get(), *dimension.get());
	}   
  } else {
    luaL_error(L, "Wrong number of arguments");
  }  
  return LuaHelper::chaining(L);
}

int LuaUiRectBind::settopleft(lua_State *L) {
	using namespace ui;
	Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
	Point::Ptr topleft = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
	rect->set_top_left(*topleft.get());
	return LuaHelper::chaining(L);
}

int LuaUiRectBind::topleft(lua_State *L) {
  using namespace ui;
  Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
  LuaHelper::requirenew<LuaPointBind>(L,
                                      "psycle.ui.point", 
                                       new Point(rect->top_left()));
  return 1;
}

int LuaUiRectBind::setbottomright(lua_State *L) {
	using namespace ui;
	Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
	Point::Ptr bottomright = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
	rect->set_bottom_right(*bottomright.get());
	return LuaHelper::chaining(L);
}

int LuaUiRectBind::bottomright(lua_State *L) {
  using namespace ui;
  Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
  LuaHelper::requirenew<LuaPointBind>(L,
                                      "psycle.ui.point", 
                                       new Point(rect->bottom_right()));
  return 1;
}

// BoxSpaceBind
std::string LuaBoxSpaceBind::meta = "psyuiboxspacemeta";

int LuaBoxSpaceBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"left", left},
    {"top", top},
    {"right", right},
    {"bottom", bottom},    
    {"set", set},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaBoxSpaceBind::create(lua_State* L) {
  int n = lua_gettop(L);
  if (n==1) {
    LuaHelper::new_shared_userdata<>(L, meta, new ui::BoxSpace());
  } else
  if (n==2) {
    double space = luaL_checknumber(L, 2);
    LuaHelper::new_shared_userdata<>(L, meta, new ui::BoxSpace(space));
  } else
  if (n==5) {    
    double top = luaL_checknumber(L, 2);
    double right = luaL_checknumber(L, 3);
    double bottom = luaL_checknumber(L, 4);
    double left = luaL_checknumber(L, 5);    
    LuaHelper::new_shared_userdata<>(L, meta, new ui::BoxSpace(top, right, bottom, left));
  } else {
    luaL_error(L, "Wrong number of arguments");
  }  
  return 1;
}

int LuaBoxSpaceBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::BoxSpace>(L, meta);
}

int LuaBoxSpaceBind::set(lua_State* L) {
  int n = lua_gettop(L);
  ui::BoxSpace::Ptr box_space = LuaHelper::check_sptr<ui::BoxSpace>(L, 1, meta);
  if (n == 2) {
    double space = luaL_checknumber(L, 2);
    box_space->set(space);    
  } else
  if (n == 5) {    
    double top = luaL_checknumber(L, 2);
    double right = luaL_checknumber(L, 3);
    double bottom = luaL_checknumber(L, 4);
    double left = luaL_checknumber(L, 5);    
    box_space->set(top, right, bottom, left);
  }
  return LuaHelper::chaining(L);
}

// LuaBorderRadiusBind
std::string LuaBorderRadiusBind::meta = "psyuiborderradiusemeta";

int LuaBorderRadiusBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaBorderRadiusBind::create(lua_State* L) {
  int n = lua_gettop(L);
  if (n==1) {
    LuaHelper::new_shared_userdata<>(L, meta, new ui::BorderRadius());
  } else
  if (n==2) {
    double radius = luaL_checknumber(L, 2);
    LuaHelper::new_shared_userdata<>(L, meta, new ui::BorderRadius(radius));
  } else {
    luaL_error(L, "Wrong number of arguments");
  }  
  return 1;
}

int LuaBorderRadiusBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::BorderRadius>(L, meta);
}

//LuaEventBind
const char* LuaEventBind::meta = "psyeventbind";

int LuaEventBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"preventdefault", preventdefault},
    {"isdefaultprevented", isdefaultprevented},
    {"stoppropagation", stoppropagation},
    {"ispropagationstopped", ispropagationstopped},    
    {NULL, NULL}
  };
  return LuaHelper::open(L, meta, methods,  gc);
}

int LuaEventBind::create(lua_State* L) {  
  LuaHelper::new_shared_userdata<>(L, meta, new ui::Event());
  return 1;
}

int LuaEventBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Event>(L, meta);
}

// LuaKeyEventBind
const char* LuaKeyEventBind::meta = "psykeyeventbind";

int LuaKeyEventBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"keycode", keycode},
    {"shiftkey", shiftkey},
    {"ctrlkey", ctrlkey},
	{"extendedkey", extendedkey},
    {"preventdefault", preventdefault},
    {"isdefaultprevented", isdefaultprevented},
    {"stoppropagation", stoppropagation},
    {"ispropagationstopped", ispropagationstopped},
    {NULL, NULL}
  };  
  LuaHelper::open(L, meta, methods,  gc);
  return 1;
}

int LuaKeyEventBind::create(lua_State* L) {
  int keycode = static_cast<int>(luaL_checkinteger(L, -1));  
  LuaHelper::new_shared_userdata<>(L, meta, new ui::KeyEvent(keycode, 0));
  return 1;
}

int LuaKeyEventBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::KeyEvent>(L, meta);
}

// LuaMouseEventBind
const char* LuaMouseEventBind::meta = "psymouseeventbind";

int LuaMouseEventBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"button", button},   
    {"clientpos", clientpos},
	{"windowpos", windowpos},
    {"preventdefault", preventdefault},
    {"isdefaultprevented", isdefaultprevented},
    {"stoppropagation", stoppropagation},
    {"ispropagationstopped", ispropagationstopped},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);  
  return 1;
}

int LuaMouseEventBind::create(lua_State* L) { 
  LuaHelper::new_shared_userdata<>(L, meta, new ui::MouseEvent());
  return 1;
}

int LuaMouseEventBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::MouseEvent>(L, meta);
}

// LuaWheelEventBind
const char* LuaWheelEventBind::meta = "psywheeleventbind";

int LuaWheelEventBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
	{"wheeldelta", wheeldelta},
    {"button", button},
	{"shiftkey", shiftkey}, 
	{"ctrlkey", ctrlkey},    
    {"clientpos", clientpos},
    {"preventdefault", preventdefault},
    {"isdefaultprevented", isdefaultprevented},
    {"stoppropagation", stoppropagation},
    {"ispropagationstopped", ispropagationstopped},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);  
  return 1;
}

int LuaWheelEventBind::create(lua_State* L) { 
  LuaHelper::new_shared_userdata<>(L, meta, new ui::WheelEvent());
  return 1;
}

int LuaWheelEventBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::WheelEvent>(L, meta);
}

ui::Ornament* LuaOrnament::Clone() { 
	return new LuaOrnament(state());
}

bool LuaOrnament::transparent() const {
	bool result(true);
	LuaImport in(state(), (void*)this, locker(state()));
	if (in.open("transparent")) {
		in << pcall(1) >> result;            
	}
	return result;
}

void LuaOrnament::Draw(ui::Window& window, ui::Graphics* g, ui::Region& draw_region) {
  LuaImport in(state(), this, locker(state()));  
  if (in.open("draw")) {
    LuaHelper::find_weakuserdata(state(), &window);      
    LuaHelper::requirenew<LuaGraphicsBind>(state(), "psycle.ui.canvas.graphics", g, true);
    LuaHelper::requirenew<LuaRegionBind>(state(), "psycle.ui.region", &draw_region, true);
    in.pcall(0);    
    LuaHelper::collect_full_garbage(state());
  }
}

std::auto_ptr<ui::Rect> LuaOrnament::padding() const {    
	return std::auto_ptr<ui::Rect>();
}

ui::BoxSpace LuaOrnament::preferred_space() const {
	return ui::BoxSpace();
}  

// OrnamentFactoryBind
std::string OrnamentFactoryBind::meta = "canvasembelissherfactory";

int OrnamentFactoryBind::open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"new", create},      
    {NULL, NULL}
  };

  static const luaL_Reg methods[] = {   
    {"createlineborder", createlineborder},
    {"createwallpaper", createwallpaper},
    {"createfill", createfill},
    {"createcirclefill", createcirclefill},
    {"createboundfill", createboundfill},
    {NULL, NULL}
  };
  luaL_newmetatable(L, meta.c_str());
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  luaL_setfuncs(L, methods, 0);    
  luaL_newlib(L, funcs); 
  return 1;
}

int OrnamentFactoryBind::create(lua_State* L) {  
  using namespace ui;
  OrnamentFactory** p = (OrnamentFactory**)lua_newuserdata(L, sizeof(OrnamentFactory*));
  luaL_setmetatable(L, meta.c_str());
  *p = &OrnamentFactory::Instance();  
  return 1;
}

int OrnamentFactoryBind::createlineborder(lua_State* L) { 
  using namespace ui; 
  luaL_checkudata(L, 1, meta.c_str());  
  int n = lua_gettop(L);
  if (n==2) {   
    LuaHelper::requirenew<LineBorderBind>(L,
                                          "psycle.ui.canvas.lineborder", 
                                          ui::OrnamentFactory::Instance()
                                            .CreateLineBorder(
                                              static_cast<ARGB>(luaL_checknumber(L, 2))));
  } else if (n == 3) {
    boost::shared_ptr<ui::BoxSpace> space = LuaHelper::check_sptr<ui::BoxSpace>(L, 3, LuaBoxSpaceBind::meta);
    LuaHelper::requirenew<LineBorderBind>(L,
                                          "psycle.ui.canvas.lineborder", 
                                          ui::OrnamentFactory::Instance().CreateLineBorder(
                                             static_cast<ARGB>(luaL_checknumber(L, 2)), *space.get()));
  } else {
    return luaL_error(L, "LineBorder, wrong number of arguments");
  }
  return 1;
}

int OrnamentFactoryBind::createwallpaper(lua_State* L) { 
  luaL_checkudata(L, 1, meta.c_str());
  LuaHelper::requirenew<WallpaperBind>(L,
                                       "psycle.ui.canvas.wallpaper", 
                                        ui::OrnamentFactory::Instance()
                                         .CreateWallpaper(
                                            LuaHelper::check_sptr<ui::Image>(
                                               L, 2, LuaImageBind::meta)));
  return 1;
}

int OrnamentFactoryBind::createfill(lua_State* L) {   
  using namespace ui;
  luaL_checkudata(L, 1, meta.c_str());
  LuaHelper::requirenew<FillBind>(L,
                                  "psycle.ui.canvas.fill",
                                     ui::OrnamentFactory::Instance()
                                       .CreateFill(static_cast<ARGB>(luaL_checknumber(L, 2))));
  return 1;
}

int OrnamentFactoryBind::createcirclefill(lua_State* L) {   
  using namespace ui;
  luaL_checkudata(L, 1, meta.c_str());
  LuaHelper::requirenew<FillBind>(L,
                                  "psycle.ui.canvas.circlefill",
                                     ui::OrnamentFactory::Instance()
                                       .CreateCircleFill(static_cast<ARGB>(luaL_checknumber(L, 2))));
  return 1;
}

int OrnamentFactoryBind::createboundfill(lua_State* L) {
  using namespace ui;
  luaL_checkudata(L, 1, meta.c_str());
  LuaHelper::requirenew<FillBind>(L,
                                  "psycle.ui.canvas.fill",
                                     ui::OrnamentFactory::Instance()
                                       .CreateBoundFill(
                                         static_cast<ARGB>(luaL_checknumber(L, 2))));
  return 1;  
}

std::string LuaOrnamentBind::meta = "ornamentbind";

int LuaOrnamentBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},  
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int LuaOrnamentBind::create(lua_State* L) {
  LuaHelper::new_shared_userdata<>(L, meta, new LuaOrnament(L));
  return 1;
}

int LuaOrnamentBind::gc(lua_State* L) {
  using namespace ui;
  return LuaHelper::delete_shared_userdata<LuaOrnament>(L, meta);  
}


std::string LineBorderBind::meta = "canvaslineborder";

int LineBorderBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {"setborderradius", setborderradius},
    {"setborderstyle", setborderstyle},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int LineBorderBind::create(lua_State* L) {
  using namespace ui;
  int n = lua_gettop(L);
  ARGB color(0xFFFFFFFF);
  if (n==2) {
    color = static_cast<ARGB>(luaL_checkinteger(L, 2));
  }  
  LineBorder* border = new LineBorder(color);
  LuaHelper::new_shared_userdata<>(L, meta, border);
  return 1;
}

int LineBorderBind::gc(lua_State* L) {
  using namespace ui;
  return LuaHelper::delete_shared_userdata<LineBorder>(L, meta);  
}

int LineBorderBind::setborderradius(lua_State* L) {
  using namespace ui;
  boost::shared_ptr<LineBorder> border = LuaHelper::check_sptr<LineBorder>(L, 1, meta);  
  boost::shared_ptr<ui::BorderRadius> radius = LuaHelper::check_sptr<ui::BorderRadius>(L, 2, LuaBorderRadiusBind::meta);  
  border->set_border_radius(*radius.get());  
  return LuaHelper::chaining(L);
} 

int LineBorderBind::setborderstyle(lua_State* L) {
  using namespace ui;
  boost::shared_ptr<LineBorder> border = LuaHelper::check_sptr<LineBorder>(L, 1, meta);  
  LineFormat left = static_cast<LineFormat>(luaL_checkinteger(L, 2));
  LineFormat top = static_cast<LineFormat>(luaL_checkinteger(L, 3));
  LineFormat right = static_cast<LineFormat>(luaL_checkinteger(L, 4));
  LineFormat bottom = static_cast<LineFormat>(luaL_checkinteger(L, 5));
  border->set_border_style(BorderStyle(left, top, right, bottom));  
  return LuaHelper::chaining(L);
}

std::string WallpaperBind::meta = "canvaswallpaper";

int WallpaperBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int WallpaperBind::create(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self");
  if (err!=0) return err;
  ui::Image::Ptr image = LuaHelper::check_sptr<ui::Image>(L, 2, LuaImageBind::meta);
  Wallpaper* wallpaper = new Wallpaper(image);
  LuaHelper::new_shared_userdata<>(L, meta, wallpaper);
  return 1;
}

int WallpaperBind::gc(lua_State* L) {
  using namespace ui;
  return LuaHelper::delete_shared_userdata<ui::Wallpaper>(L, meta);
}

std::string CircleFillBind::meta = "canvascirclefill";

int CircleFillBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int CircleFillBind::create(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self");
  if (err!=0) return err;
  ARGB color = static_cast<ARGB>(luaL_checknumber(L, 2));
  CircleFill* fill = new CircleFill(color);
  LuaHelper::new_shared_userdata<>(L, meta, fill);
  return 1;
}

int CircleFillBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Fill>(L, meta);
}

std::string FillBind::meta = "canvasfill";

int FillBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {"setcolor", setcolor},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int FillBind::create(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 2, "self");
  if (err!=0) return err;
  ARGB color = static_cast<ARGB>(luaL_checknumber(L, 2));
  Fill* fill = new Fill(color);
  LuaHelper::new_shared_userdata<>(L, meta, fill);
  return 1;
}

int FillBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<ui::Fill>(L, meta);
}

// LuaGameController + Bind
void LuaGameController::OnButtonDown(int button) {   
  LuaImport in(L, this, locker(L));
  if (in.open("onbuttondown")) {
    in << button << pcall(0);            
  }
}

void LuaGameController::OnButtonUp(int button) {   
  LuaImport in(L, this, locker(L));
  if (in.open("onbuttonup")) {
    in << button << pcall(0);            
  }
}

void LuaGameController::OnXAxis(int pos, int oldpos) {
  LuaImport in(L, this, locker(L));
  if (in.open("onxaxis")) {
    in << pos << oldpos << pcall(0);            
  }
}

void LuaGameController::OnYAxis(int pos, int oldpos) {
  LuaImport in(L, this, locker(L));
  if (in.open("onyaxis")) {
    in << pos << oldpos << pcall(0);            
  }
}

void LuaGameController::OnZAxis(int pos, int oldpos) {
  LuaImport in(L, this, locker(L));
  if (in.open("onzaxis")) {
    in << pos << oldpos << pcall(0);            
  }
}

std::string LuaGameControllerBind::meta = "gamecontroller";

int LuaGameControllerBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},    
    {"xposition", xposition},
    {"yposition", yposition},
    {"xzposition", zposition},
    {"buttons", buttons},  
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);   
  return 1;
}

int LuaGameControllerBind::create(lua_State* L) {
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;  
  LuaGameController* game_controller_info = new LuaGameController(L);
  LuaHelper::new_shared_userdata<>(L, meta, game_controller_info);
  return 1;
}

int LuaGameControllerBind::gc(lua_State* L) {
  return LuaHelper::delete_shared_userdata<LuaGameController>(L, meta);
}

int LuaGameControllerBind::buttons(lua_State* L) {
  boost::shared_ptr<LuaGameController> controller = LuaHelper::check_sptr<LuaGameController>(L, 1, meta);
  lua_pushinteger(L, controller->buttons().to_ulong());
  return 1;
}

void LuaGameControllers::OnConnect(LuaGameController& controller) {
	LuaImport in(L, this, locker(L));
	if (in.open("onconnect")) {
		lua_getfield(L, -1, "_controllers");
		int size = luaL_len(L, -1);
		LuaHelper::requirenew<LuaGameControllerBind>(L, "psycle.ui.gamecontroller",
                                                 &controller, true);
		lua_pushvalue(L, -1);
		controller.set_lua_state(L);
		lua_rawseti(L, -3, size + 1);
		lua_remove(L, -2);
		in << pcall(0);            
	}	
}

void LuaGameControllers::OnDisconnect(LuaGameController& controller) {
  LuaImport in(L, this, locker(L));
  if (in.open("ondisconnect")) {
	LuaHelper::find_weakuserdata(L, this);
    in << pcall(0);            
  }
  in.close();
  LuaHelper::find_weakuserdata(L, this);  
  if (lua_isnil(L, -1)) {
     assert(0);
     lua_pop(L, 1);
     throw std::runtime_error("no proxy found");
  }
  lua_getfield(L, -1, "_controllers");
  int tbl_idx = lua_gettop(L);
  int size = luaL_len(L, -1);
  if (size != 0) {
    int pos = 1;  
	bool remove(false);  
	for (int pos = 1; pos < size; pos++) {
	  lua_rawgeti(L, tbl_idx, pos);
	  if (!remove) {
		int n = lua_gettop(L);
		boost::shared_ptr<LuaGameController> controller_cache = LuaHelper::check_sptr<LuaGameController>(L, n, LuaGameControllerBind::meta);
		if (controller_cache.get() == &controller) {
			remove = true;
		}
	  }
	  if (remove) {
		lua_rawgeti(L, tbl_idx, pos+1);
		lua_rawseti(L, tbl_idx, pos);  /* t[pos] = t[pos+1] */
	  }
    }
	lua_pushnil(L);
    lua_rawseti(L, tbl_idx, pos);  /* t[pos] = nil */
  }    
}

// GameControllersBind
std::string LuaGameControllersBind::meta = "gamecontrollerbind";

int LuaGameControllersBind::open(lua_State *L) {
  static const luaL_Reg methods[] = {
    {"new", create},
    {"controllers", controllers},
    {"update", update},
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);
  return 1;
}

int LuaGameControllersBind::create(lua_State* L) {
  using namespace ui;
  int err = LuaHelper::check_argnum(L, 1, "self");
  if (err!=0) return err;    
  LuaGameControllers* controllers = new LuaGameControllers(L);  
  LuaHelper::new_shared_userdata<>(L, meta, controllers);
  lua_newtable(L);  
  LuaGameControllers::iterator it = controllers->begin();
  for (; it != controllers->end(); ++it) {
    (*it)->set_lua_state(L);
    LuaHelper::requirenew<LuaGameControllerBind>(L, "psycle.ui.gamecontroller",
                                                 (*it).get(), true);
    lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
  }
  lua_setfield(L, -2, "_controllers");
  return 1;
}

int LuaGameControllersBind::gc(lua_State* L) {
  using namespace ui;
  return LuaHelper::delete_shared_userdata<GameControllers<LuaGameController> >(L, meta);
}

int LuaGameControllersBind::controllers(lua_State* L) {
  using namespace ui;
  boost::shared_ptr<LuaGameController> controller = LuaHelper::check_sptr<LuaGameController>(L, 1, meta);
  lua_getfield(L, -1, "_controllers");
  return 1;
}

// LuaScintilla

void LuaScintilla::OnMarginClick(int line_pos) {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onmarginclick")) {
      in << line_pos << pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

std::string LuaLexerBind::meta = "psylexermeta";


void LuaButton::OnClick() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclick")) {
      in.pcall(0);
    } 
  } catch (std::exception& e) {
      ui::alert(e.what());   
  }
}

void LuaRadioButton::OnClick() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclick")) {
      in.pcall(0);
    }
  }
  catch (std::exception& e) {
    ui::alert(e.what());
  }
}

void LuaGroupBox::OnClick() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onclick")) {
      in.pcall(0);
    }
  }
  catch (std::exception& e) {
    ui::alert(e.what());
  }
}

void LuaComboBox::OnSelect() {
  try {
    LuaImport in(L, this, locker(L));
    if (in.open("onselect")) {      
      in.pcall(0);
    } 
  } catch (std::exception& e) {
    ui::alert(e.what());   
  }
}

void LuaScrollBar::OnScroll(int pos) {
  try {
	  LuaImport in(L, this, locker(L));
	  if (in.open("onscroll")) {
	    LuaHelper::find_weakuserdata(L, this);
        in.pcall(0);		
	  }  
  } catch (std::exception& e) {
    ui::alert(e.what());   
  }
}


// LuaRegion+Bind
const char* LuaRegionBind::meta = "psyregionbind";

int LuaRegionBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {
    {"new", create},
    {"bounds", bounds},
    {"setrect", setrect},   
    {"union", Union},
    {"offset", offset},    
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);
  /*LuaHelper::setfield(L, "OR", RGN_OR);
  LuaHelper::setfield(L, "AND", RGN_AND);
  LuaHelper::setfield(L, "XOR", RGN_XOR);
  LuaHelper::setfield(L, "DIFF", RGN_DIFF);
  LuaHelper::setfield(L, "COPY", RGN_COPY);*/
  return 1;
}

int LuaRegionBind::create(lua_State* L) {
  ui::Region* region(0);
  int n = lua_gettop(L);
  if (n == 1) {
    region = new ui::Region();
  } else
  if (n == 2) {
    boost::shared_ptr<ui::Region> src = LuaHelper::check_sptr<ui::Region>(L, 2, meta);
	region = new ui::Region(*(src.get()));
  } else {
    luaL_error(L, "Wrong number of arguments.");
  }
  LuaHelper::new_shared_userdata<>(L, meta, region);
  return 1;
}

int LuaRegionBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::Region>(L, meta); 
  return 0;
}

// LuaAreaBind
const char* LuaAreaBind::meta = "psyareabind";

int LuaAreaBind::open(lua_State *L) {  
  static const luaL_Reg methods[] = {
    {"new", create},
    {"boundrect", boundrect},
    {"setrect", setrect},
    {"clear", clear},
    {"combine", combine},
    {"offset", offset},    
    {NULL, NULL}
  };
  LuaHelper::open(L, meta, methods, gc);
  /*LuaHelper::setfield(L, "OR", RGN_OR);
  LuaHelper::setfield(L, "AND", RGN_AND);
  LuaHelper::setfield(L, "XOR", RGN_XOR);
  LuaHelper::setfield(L, "DIFF", RGN_DIFF);
  LuaHelper::setfield(L, "COPY", RGN_COPY);*/
  return 1;
}

int LuaAreaBind::create(lua_State* L) {
  LuaHelper::new_shared_userdata<>(L, meta, new ui::Area());
  return 1;
}

int LuaAreaBind::gc(lua_State* L) {
  LuaHelper::delete_shared_userdata<ui::Area>(L, meta);
  return 0;
}

void LuaPic::Draw(ui::Graphics* g, ui::Region& draw_rgn) {
  Pic::Draw(g, draw_rgn);
  LuaImport in(L, this, locker(L));
  if (in.open("draw")) {
  LuaHelper::requirenew<LuaGraphicsBind>(L, "psycle.ui.graphics", g, true);
  LuaHelper::requirenew<LuaRegionBind>(L, "psycle.ui.region", &draw_rgn, true);
  in.pcall(0);      
  } 
}


} // namespace host
} // namespace psycle