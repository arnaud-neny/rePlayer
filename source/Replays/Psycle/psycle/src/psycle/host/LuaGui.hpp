// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2015 members of the psycle project http://psycle.sourceforge.net

#pragma once
// #include <psycle/host/detail/project.hpp>
#include "CanvasItems.hpp"
#include "LuaHelper.hpp"
#include <psycle/helpers/resampler.hpp>

namespace psycle {
namespace host {

namespace stock {  
namespace color {
enum type {
  UNDEFINED = -1,
  PVBACKGROUND = 1,
  PVROW,
  PVROW4BEAT,
  PVROWBEAT,
  PVSEPARATOR ,
  PVFONT,
  PVFONTSEL,
  PVFONTPLAY,
  PVCURSOR,
  PVSELECTION ,
  PVPLAYBAR,
  MVFONTBOTTOMCOLOR = 20,
  MVFONTHTOPCOLOR,
  MVFONTHBOTTOMCOLOR,
  MVFONTTITLECOLOR,
  MVTOPCOLOR,
  MVBOTTOMCOLOR,
  MVHTOPCOLOR,
  MVHBOTTOMCOLOR,
  MVTITLECOLOR,
};
}
}

class PsycleStock : public ui::Stock {
 public:
   virtual PsycleStock* Clone() const { return new PsycleStock(*this); }
   ui::MultiType value(int stock_key) const;
   std::string key_tostring(int stock_key) const;
};

struct LuaFileOpenBind {
  static const char* meta;
  static int open(lua_State *L);
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int show(lua_State *L);
  static int filename(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::filename); }
  static int setfilename(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::set_filename); }
  static int setfolder(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::set_folder); }
  static int isok(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::is_ok); }
};

struct LuaFileSaveBind {
  static const char* meta;
  static int open(lua_State *L);
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int show(lua_State *L);
  static int filename(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::filename); }
  static int setfilename(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::set_filename); }
  static int setfolder(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::set_folder); }
  static int isok(lua_State *L) { LUAEXPORTM(L, meta, &ui::FileDialog::is_ok); }
};

class LuaFileObserver : public ui::FileObserver, public LuaState {
 public:
   LuaFileObserver(lua_State* L) : LuaState(L), ui::FileObserver() {}
   
   virtual void OnCreateFile(const std::string& path);
   virtual void OnDeleteFile(const std::string& path);
   virtual void OnChangeFile(const std::string& path);
};

struct LuaFileObserverBind {
    static const char* meta;
    static int open(lua_State *L);    
    static int create(lua_State* L);    
    static int gc(lua_State* L);
    static int setdirectory(lua_State* L);
    static int directory(lua_State* L);
    static int startwatching(lua_State* L);
    static int stopwatching(lua_State* L);
  };

struct LuaPointBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L);
  static int gc(lua_State* L);  
  static int setxy(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::set_xy);  }
  static int setx(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::set_x); }
  static int x(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::x); }
  static int sety(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::set_y); }
  static int y(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::y); }
  static int offset(lua_State* L) { LUAEXPORTM(L, meta, &ui::Point::offset); }
  static int add(lua_State* L);
  static int sub(lua_State* L);
  static int mul(lua_State* L);
  static int div(lua_State* L);
};

struct LuaDimensionBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int set(lua_State* L) { LUAEXPORTM(L, meta, &ui::Dimension::set); }
  static int setwidth(lua_State* L) { LUAEXPORTM(L, meta, &ui::Dimension::set_width); }
  static int width(lua_State* L) { LUAEXPORTM(L, meta, &ui::Dimension::width); }
  static int setheight(lua_State* L) { LUAEXPORTM(L, meta, &ui::Dimension::set_height); }
  static int height(lua_State* L) { LUAEXPORTM(L, meta, &ui::Dimension::height); }
};

struct LuaUiRectBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int set(lua_State* L);
  static int setleft(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_left); }
  static int left(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::left); }
  static int settop(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_top); }
  static int top(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::top); }
  static int setright(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_right); }
  static int right(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::right); }
  static int setbottom(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_bottom); }
  static int bottom(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::bottom); }
  static int setwidth(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_width); }
  static int width(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::width); }
  static int setheight(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::set_height); }
  static int height(lua_State *L) { LUAEXPORTM(L, meta, &ui::Rect::height); } 
  static int dimension(lua_State* L) {
    using namespace ui;
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
    LuaHelper::requirenew<LuaDimensionBind>(L, "psycle.ui.dimension",
      new Dimension(rect->dimension()));
    return 1;
  } 
  static int intersect(lua_State* L) {
    using namespace ui;
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
    Point::Ptr point = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
    lua_pushboolean(L, rect->intersect(*point.get()));
    return 1;
  }
  static int offset(lua_State* L) {
    using namespace ui;
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
    Point::Ptr delta = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
    rect->offset(*delta.get());
    return LuaHelper::chaining(L);
  }
  static int move(lua_State* L) {
    using namespace ui;
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 1, LuaUiRectBind::meta);
    Point::Ptr pos = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
    rect->move(*pos.get());
    return LuaHelper::chaining(L);
  }
  static int settopleft(lua_State *L);
  static int topleft(lua_State *L);  
  static int setbottomright(lua_State *L);
  static int bottomright(lua_State *L);
};

struct LuaBoxSpaceBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int set(lua_State* L);
  static int left(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::left); }
  static int top(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::top); }
  static int right(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::right); }
  static int bottom(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::bottom); }  
};

struct LuaBorderRadiusBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L);
  static int gc(lua_State* L);
  // static int set(lua_State* L);
  // static int left(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::left); }
  // static int top(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::top); }
  // static int right(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::right); }
  // static int bottom(lua_State *L) { LUAEXPORTM(L, meta, &ui::BoxSpace::bottom); }  
};

struct LuaSystemMetrics {  
  static std::string meta;
  static int open(lua_State *L); 
  static int screendimension(lua_State *L);
};

class LuaCommand : public ui::Command, public LuaState {
 public:
  LuaCommand(lua_State* L) : ui::Command(), LuaState(L) {}

  virtual void Execute();
};

struct LuaCommandBind {  
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);
};

template <class T>
class LuaWindowBase : public T, public LuaState {
 public:    
  LuaWindowBase(lua_State* L) : LuaState(L), T() {}
  LuaWindowBase(lua_State* L, ui::WindowImp* imp) : LuaState(L), T(0) {}

  virtual std::string GetType() const;

  virtual void OnMouseDown(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "onmousedown", ev, *this)) {
      T::OnMouseDown(ev);
    }
  }
  virtual void OnDblclick(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "ondblclick", ev, *this)) {
      T::OnDblclick(ev);
    }
  }   
  virtual void OnMouseUp(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "onmouseup", ev, *this)) {
      T::OnMouseUp(ev);
    }
  }
  virtual void OnMouseMove(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "onmousemove", ev, *this)) {
      T::OnMouseMove(ev);
    }
  }
  virtual void OnWheel(ui::WheelEvent& ev) {
    if (!SendWheelEvent(L, "onwheel", ev, *this)) {
      T::OnWheel(ev);
    }
  }
  virtual void OnMouseEnter(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "onmouseenter", ev, *this)) {
      T::OnMouseEnter(ev);
    }
  }
  virtual void OnMouseOut(ui::MouseEvent& ev) {
    if (!SendMouseEvent(L, "onmouseout", ev, *this)) {
      T::OnMouseOut(ev);
    }
  }
  virtual void OnKeyDown(ui::KeyEvent& ev) {
    if (!SendKeyEvent(L, "onkeydown", ev, *this)) {
      T::OnKeyDown(ev);
    }
  }
  virtual void set_properties(const ui::Properties& properties);
  virtual void OnKeyUp(ui::KeyEvent& ev) {
    if (!SendKeyEvent(L, "onkeyup", ev, *this)) {
      T::OnKeyUp(ev);
    }
  }
  virtual void OnFocus(ui::Event& ev) {
    if (!SendEvent(L, "onfocus", ev, *this)) {
      T::OnFocus(ev);
    }
  }
  virtual void OnKillFocus(ui::Event& ev) {
    if (!SendEvent(L, "onkillfocus", ev, *this)) {
      T::OnFocus(ev);
    }
  }   
  virtual void OnSize(const ui::Dimension &dimension);   
  virtual ui::Dimension OnCalcAutoDimension() const; 
  static bool SendEvent(lua_State* L, const::std::string method,
                        ui::Event& ev, ui::Window& window);
  static bool SendKeyEvent(lua_State* L, const::std::string method,
                           ui::KeyEvent& ev, ui::Window& window);
  static bool SendMouseEvent(lua_State* L, const::std::string method,
                             ui::MouseEvent& ev, ui::Window& window);
  static bool SendWheelEvent(lua_State* L, const::std::string method,
                             ui::WheelEvent& ev, ui::Window& window);
};


class LuaRun : public LuaState {
 public:
  LuaRun(lua_State* L) : LuaState(L) {}
  void Run();
};

struct LuaRunBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);  
};

class LuaWindow : public LuaWindowBase<ui::Window> {
 public:  
  LuaWindow(lua_State* L) : LuaWindowBase<ui::Window>(L) {}

  virtual void Draw(ui::Graphics* g, ui::Region& draw_rgn); 
  virtual void OnSize(double cw, double ch);  
  virtual bool transparent() const;  
};

class Machine;
class ScopeMemory;

class LuaScopeWindow : public LuaWindow, public Timer {
  public:
   static std::string type() { return "scope"; }  
   LuaScopeWindow(lua_State* L);
   
   virtual void Draw(ui::Graphics* g, ui::Region& draw_rgn);
   virtual void DrawScope(ui::Graphics* g, double x, double y, Machine* mac);
   virtual void OnTimerViewRefresh();
   virtual void OnSize(const ui::Dimension& dimension);   

  protected:
   virtual void OnMouseDown(ui::MouseEvent& ev);

  private:   
    int GetY(float f, float mult);
	Machine* HitTest(const ui::Point& pos) const;
	int MachineCount() const;
	void CalcScopeSize(int mac_num, const ui::Dimension& dimension);
	double scope_width_, scope_height_, scope_mid_;
	double scale_x_;
	int mac_count_;
	int scope_mode;
	int scope_peak_rate;
	int scope_osc_freq;
	int scope_osc_rate;
	int scope_spec_samples;
	int scope_spec_rate;
	int scope_spec_mode;
	int scope_phase_rate;
	float *pSamplesL;
	float *pSamplesR;
	BOOL hold;
	BOOL clip_;
    std::vector<boost::shared_ptr<ScopeMemory> > scope_memories_;   
	helpers::dsp::cubic_resampler resampler;
};

typedef LuaWindowBase<ui::Group> LuaGroup;
typedef LuaWindowBase<ui::HeaderGroup> LuaHeaderGroup;
typedef LuaWindowBase<ui::Viewport> LuaViewport;
typedef LuaWindowBase<ui::RectangleBox> LuaRectangleBox;
typedef LuaWindowBase<ui::Line> LuaLine;
typedef LuaWindowBase<ui::Text> LuaText;

class LuaPic : public LuaWindowBase<ui::Pic> {
 public:  
  LuaPic(lua_State* L) : LuaWindowBase<ui::Pic>(L) {}  
  
  virtual void Draw(ui::Graphics* g, ui::Region& draw_rgn);
};

class LuaScrollBox : public LuaWindowBase<ui::ScrollBox> {
 public:  
  LuaScrollBox(lua_State* L) : LuaWindowBase<ui::ScrollBox>(L) {}
};

class LuaButton : public LuaWindowBase<ui::Button> {
 public:  
  LuaButton(lua_State* L) : LuaWindowBase<ui::Button>(L) {}
  
  virtual void OnClick();
};

class LuaRadioButton : public LuaWindowBase<ui::RadioButton> {
public:
  LuaRadioButton(lua_State* L) : LuaWindowBase<ui::RadioButton>(L) {}

  virtual void OnClick();
};

class LuaGroupBox : public LuaWindowBase<ui::GroupBox> {
public:
  LuaGroupBox(lua_State* L) : LuaWindowBase<ui::GroupBox>(L) {}

  virtual void OnClick();
};

class LuaEdit : public LuaWindowBase<ui::Edit> {
 public:  
  LuaEdit(lua_State* L) : LuaWindowBase<ui::Edit>(L) {}  
};

class LuaComboBox : public LuaWindowBase<ui::ComboBox> {
 public:  
  LuaComboBox(lua_State* L) : LuaWindowBase<ui::ComboBox>(L) {}

  virtual void OnSelect();
};

class LuaTreeView : public LuaWindowBase<ui::TreeView> {
 public:  
  LuaTreeView(lua_State* L) : LuaWindowBase<ui::TreeView>(L) {}
  
  virtual void OnChange(const ui::Node::Ptr& node);
  virtual void OnRightClick(const ui::Node::Ptr& node);
  virtual void OnEditing(const ui::Node::Ptr& node, const std::string& text);
  virtual void OnEdited(const ui::Node::Ptr& node, const std::string& text);
  virtual void OnContextPopup(ui::Event&, const ui::Point& mouse_point,
                              const ui::Node::Ptr& node);
};

class LuaListView : public LuaWindowBase<ui::ListView> {
 public:  
  LuaListView(lua_State* L) : LuaWindowBase<ui::ListView>(L) {}
  
  virtual void OnChange(const ui::Node::Ptr& node);
  virtual void OnRightClick(const ui::Node::Ptr& node);
  virtual void OnEditing(const ui::Node::Ptr& node, const std::string& text);
  virtual void OnEdited(const ui::Node::Ptr& node, const std::string& text);
};

struct LuaEventBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int preventdefault(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::PreventDefault);
  }
  static int isdefaultprevented(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::is_default_prevented);
  }
  static int stoppropagation(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::StopPropagation);
  }
  static int ispropagationstopped(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::is_propagation_stopped);
  }
};

struct LuaKeyEventBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int keycode(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::KeyEvent::keycode);
  }
  static int shiftkey(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::KeyEvent::shiftkey);
  }
  static int ctrlkey(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::KeyEvent::ctrlkey);
  }
  static int extendedkey(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::KeyEvent::extendedkey);
  }
  static int preventdefault(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::KeyEvent::PreventDefault);
  }
  static int isdefaultprevented(lua_State* L) {
   LUAEXPORTM(L, meta, &ui::Event::is_default_prevented);
  }
  static int stoppropagation(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::StopPropagation);
  }
  static int ispropagationstopped(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Event::is_propagation_stopped);
  }
};

struct LuaMouseEventBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);  
  static int clientpos(lua_State* L) { 
    using namespace ui;
    MouseEvent::Ptr ev = LuaHelper::check_sptr<MouseEvent>(L, 1, meta);
    LuaHelper::requirenew<LuaPointBind>(L, "psycle.ui.point",
                                        new Point(ev->client_pos()));
    return 1;
  }
  static int windowpos(lua_State* L) { 
    using namespace ui;
    MouseEvent::Ptr ev = LuaHelper::check_sptr<MouseEvent>(L, 1, meta);
    LuaHelper::requirenew<LuaPointBind>(L, "psycle.ui.point",
                                        new Point(ev->window_pos()));
    return 1;
  } 
  static int button(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::MouseEvent::button);
  }
  static int preventdefault(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::MouseEvent::PreventDefault);
  }
  static int isdefaultprevented(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::MouseEvent::is_default_prevented);
  }
  static int stoppropagation(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::MouseEvent::StopPropagation);
  }
  static int ispropagationstopped(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::MouseEvent::is_propagation_stopped);
  }
};

struct LuaWheelEventBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);  
  static int clientpos(lua_State* L) { 
    using namespace ui;
    WheelEvent::Ptr ev = LuaHelper::check_sptr<WheelEvent>(L, 1, meta);
    LuaHelper::requirenew<LuaPointBind>(L, "psycle.ui.point",
                                        new Point(ev->client_pos()));
    return 1;
  }
  static int wheeldelta(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::wheel_delta);
  }
  static int button(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::button);
  }
  static int shiftkey(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::shiftkey);
  }
  static int ctrlkey(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::ctrlkey);
  }
  static int preventdefault(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::PreventDefault);
  }
  static int isdefaultprevented(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::is_default_prevented);
  }
  static int stoppropagation(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::StopPropagation);
  }
  static int ispropagationstopped(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::WheelEvent::is_propagation_stopped);
  }
};

struct LuaImageBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int graphics(lua_State* L);
  static int dimension(lua_State* L);
  static int resize(lua_State* L);  
  static int rotate(lua_State* L);
  static int reset(lua_State *L);
  static int load(lua_State *L) { LUAEXPORTM(L, meta, &ui::Image::Load); }
  static int save(lua_State *L);
  static int cut(lua_State *L);
  static int settransparent(lua_State* L);
  static int setpixel(lua_State *L);
  static int getpixel(lua_State *L);
};

class LuaColorHelper {
 public:
  static int open(lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"encodeargb", encodeargb},
    {"decodeargb", decodeargb},     
    { NULL, NULL }
  };
  luaL_newlib(L, funcs);
  return 1;
  }

  static int encodeargb(lua_State* L) {
    int red = static_cast<int>(luaL_checkinteger(L, 1));
    int green = static_cast<int>(luaL_checkinteger(L, 2));
    int blue = static_cast<int>(luaL_checkinteger(L, 3));
    int alpha = static_cast<int>(luaL_checkinteger(L, 4));
    int argb = (blue << 0) | (green << 8) | (red << 16) | (alpha << 24);
    lua_pushinteger(L, argb);
    return 1;
  }

  static int decodeargb(lua_State* L) {
    int argb = static_cast<int>(luaL_checkinteger(L, 1));
    int alpha = (argb >> 24) & 0xff;
    int red = (argb >> 16) & 0xff;
    int green = (argb >> 8) & 0xff;
    int blue = argb & 0xff;
    lua_pushinteger(L, red);
    lua_pushinteger(L, green);
    lua_pushinteger(L, blue);
    lua_pushinteger(L, alpha);
    return 4;
  }
};

class LuaImagesBind {
 public:
  static std::string meta;
  static int open(lua_State *L) {
    static const luaL_Reg methods[] = {
      {"new", create},                  
      {"add", add},      
      {"size", size},
      {"at", at},         
      {NULL, NULL}
    };
    LuaHelper::open(L, meta, methods,  gc);
    return 1;
  }

  static int create(lua_State* L) {
    LuaHelper::new_shared_userdata(L, meta.c_str(), new ui::Images());
    lua_newtable(L);
    lua_setfield(L, -2, "_children");
    return 1;
  }

  static int gc(lua_State* L) {
    return LuaHelper::delete_shared_userdata<ui::Images>(L, meta);
  }    
  static int add(lua_State* L) {
    using namespace ui;
    Images::Ptr images = LuaHelper::check_sptr<Images>(L, 1, meta);
    Image::Ptr image = LuaHelper::check_sptr<Image>(L, 2, LuaImageBind::meta);
    images->Add(image);
    lua_getfield(L, -2, "_children");
    int len = lua_rawlen(L, -1);
    lua_pushvalue(L, -2);
    lua_rawseti(L, -2, len+1);
    return LuaHelper::chaining(L);
  }
    
  static int size(lua_State* L) { LUAEXPORTM(L, meta, &ui::Images::size); }
  static int at(lua_State *L) {
    using namespace ui;
    if (lua_isnumber(L, 2)) {
      Images::Ptr images = LuaHelper::check_sptr<Images>(L, 1, meta);
      int index = static_cast<int>(luaL_checkinteger(L, 2));
      if (index < 1 && index >= images->size()) {
        luaL_error(L, "index out of range");
      }            
      Image::Ptr tn = *(images->begin() + index - 1);
      if (tn.get()) {
        LuaHelper::find_weakuserdata(L, tn.get());
      } else {
        lua_pushnil(L);
      }      
      return 1;
    }
    return 0;
  }
};

struct LuaRegionBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int setrect(lua_State *L) { 
    using namespace ui;
    Region::Ptr rgn = LuaHelper::check_sptr<Region>(L, 1, meta); 
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    if (rect.get()) {
      rgn->SetRect(*rect.get());
    }
    return LuaHelper::chaining(L);
  }   
  static int bounds(lua_State *L) {
    using namespace ui;
    Region::Ptr rgn = LuaHelper::check_sptr<Region>(L, 1, meta);
    LuaHelper::requirenew<LuaUiRectBind>(L, LuaUiRectBind::meta, new ui::Rect(rgn->bounds()));   
    return 1;
  }
  static int Union(lua_State *L) {
    using namespace ui;
    Region::Ptr rgn = LuaHelper::check_sptr<Region>(L, 1, meta);
    Region::Ptr rgnsrc = LuaHelper::check_sptr<Region>(L, 2, meta);
    rgn->Union(*rgnsrc.get());
    return LuaHelper::chaining(L);
  }
  static int offset(lua_State *L) {
    LUAEXPORTML(L, meta, &ui::Region::Offset);
  }
  static int gc(lua_State* L);
};

struct LuaAreaBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);  
  static int boundrect(lua_State *L) {
    using namespace ui;
    Area::Ptr area = LuaHelper::check_sptr<Area>(L, 1, meta);
    LuaHelper::requirenew<LuaUiRectBind>(L, LuaUiRectBind::meta, new ui::Rect(area->bounds()));   
    return 1;
  }
  static int combine(lua_State *L) {
    LUAEXPORTML(L, meta, &ui::Area::Combine);
  }
  static int offset(lua_State *L) { LUAEXPORTML(L, meta, &ui::Area::Offset); }
  static int clear(lua_State* L) { LUAEXPORTML(L, meta, &ui::Area::Clear); }
  static int setrect(lua_State* L) {
    using namespace ui;
    Area::Ptr rgn = LuaHelper::check_sptr<Area>(L, 1, meta);
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    rgn->Combine(Area(*rect.get()), RGN_OR);
    return LuaHelper::chaining(L);
  }
  static int gc(lua_State* L);
};

struct LuaFontInfoBind {
  static std::string meta;
  static int open(lua_State *L);
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int setsize(lua_State* L) { LUAEXPORTM(L, meta, &ui::FontInfo::set_size); }
  static int size(lua_State* L) { LUAEXPORTM(L, meta, &ui::FontInfo::size); }
  static int setfamily(lua_State* L) { LUAEXPORTM(L, meta, &ui::FontInfo::set_family); }
  static int family(lua_State* L) { LUAEXPORTM(L, meta, &ui::FontInfo::family); }
  static int style(lua_State* L) {
    using namespace ui;
    FontInfo::Ptr font_info = LuaHelper::check_sptr<FontInfo>(L, 1, meta);
    lua_pushinteger(L, static_cast<int>(font_info->style()));
    return 1;
  }
  static int tostring(lua_State* L);
};

struct LuaFontBind {
  static const char* meta;
  static int open(lua_State *L);
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int setfontinfo(lua_State* L);
};

struct LuaFontsBind {
  static const char* meta;
  static int open(lua_State *L);
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int fontlist(lua_State* L);
  static int importfont(lua_State* L) {
    LUAEXPORTM(L, meta, &ui::Fonts::import_font);
  }
};

struct LuaGraphicsBind {
  static const char* meta;
  static int open(lua_State *L);
  static int create(lua_State *L);  
  static int gc(lua_State* L);
  static int dispose(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::Dispose); }
  static int translate(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Point::Ptr delta = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    g->Translate(*delta.get());
    return LuaHelper::chaining(L);    
  }
  static int retranslate(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);   
    g->Retranslate();
    return LuaHelper::chaining(L);    
  }
  static int cleartranslations(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);   
    g->ClearTranslations();
    return LuaHelper::chaining(L);    
  }
  static int setcolor(lua_State* L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    ARGB color = LuaHelper::check32bit(L, 2);
    g->set_color(color);
    return LuaHelper::chaining(L);    
  }
  static int setfill(lua_State* L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    ARGB color = LuaHelper::check32bit(L, 2);
    g->set_fill(color);
    return LuaHelper::chaining(L);    
  }
  static int setstroke(lua_State* L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    ARGB color = LuaHelper::check32bit(L, 2);
    g->set_stroke(color);
    return LuaHelper::chaining(L);    
  }
  static int color(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::color); }
  static int plot(lua_State *L);
  static int drawline(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);    
    Point::Ptr from = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    Point::Ptr to = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
    g->DrawLine(*from.get(), *to.get());
    return LuaHelper::chaining(L);
  }
  static int drawcurve(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);    
    Point::Ptr p1= LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    Point::Ptr control_p1 = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
    Point::Ptr control_p2 = LuaHelper::check_sptr<Point>(L, 4, LuaPointBind::meta);
    Point::Ptr p2 = LuaHelper::check_sptr<Point>(L, 5, LuaPointBind::meta);
    g->DrawCurve(*p1.get(), *control_p1.get(), *control_p2.get(), *p2.get());
    return LuaHelper::chaining(L);
  }
  static int drawrect(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    g->DrawRect(*rect.get());
    return LuaHelper::chaining(L);
  }
  static int drawroundrect(lua_State *L) {    
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    Dimension::Ptr arc_dimension =
        LuaHelper::check_sptr<Dimension>(L, 3, LuaDimensionBind::meta);
    g->DrawRoundRect(*pos.get(), *arc_dimension.get());
    return LuaHelper::chaining(L);
  }
  static int drawoval(lua_State* L) { 
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    g->DrawOval(*pos.get());
    return LuaHelper::chaining(L);
  }
  static int drawcircle(lua_State* L) { 
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Point::Ptr center = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    double radius = luaL_checknumber(L, 3);
    g->DrawCircle(*center.get(), radius);
    return LuaHelper::chaining(L);
  }
  static int fillcircle(lua_State* L) { 
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Point::Ptr center = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    double radius = luaL_checknumber(L, 3);
    g->FillCircle(*center.get(), radius);
    return LuaHelper::chaining(L);
  }
  static int drawellipse(lua_State* L) { 
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Point::Ptr center = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);    
    Point::Ptr radius = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);    
    g->DrawEllipse(*center.get(), *radius.get());
    return LuaHelper::chaining(L);
  }
  static int fillellipse(lua_State* L) { 
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Point::Ptr center = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    Point::Ptr radius = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);    
    g->FillEllipse(*center.get(), *radius.get());
    return LuaHelper::chaining(L);
  }
  static int fillrect(lua_State *L) {
    using namespace ui;
	int n = lua_gettop(L);
	if (n == 2) {
      Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
      Rect::Ptr pos = LuaHelper::test_sptr<Rect>(L, 2, LuaUiRectBind::meta);
	  if (pos) {
        g->FillRect(*pos.get());
	  } else {
	    Dimension::Ptr dimension = LuaHelper::check_sptr<Dimension>(L, 2, LuaDimensionBind::meta);
		g->FillRect(Rect(Point(), *dimension.get()));
	  }
	} else {	
	  return luaL_error(L, "Wrong number of arguments.");
	}
    return LuaHelper::chaining(L);
  }
  static int fillroundrect(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    Dimension::Ptr arc_dimension =
        LuaHelper::check_sptr<Dimension>(L, 3, LuaDimensionBind::meta);
    g->FillRoundRect(*pos.get(), *arc_dimension.get());
    return LuaHelper::chaining(L);  
  }
  static int filloval(lua_State* L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);
    Rect::Ptr pos = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    g->FillOval(*pos.get());
    return LuaHelper::chaining(L);
  }
  static int copyarea(lua_State* L) {
    LuaHelper::check_sptr<ui::Graphics>(L, 1, meta)->CopyArea(ui::Rect(
      ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)),
      ui::Dimension(luaL_checknumber(L, 4), luaL_checknumber(L, 5))),
      ui::Point(luaL_checknumber(L, 6), luaL_checknumber(L, 7)));
    return LuaHelper::chaining(L);
  }
  static int drawstring(lua_State* L);
  static int setfont(lua_State* L);
  static int font(lua_State* L);
  static int drawpolygon(lua_State* L);
  static int fillpolygon(lua_State* L);
  static int drawpolyline(lua_State* L);
  static int drawimage(lua_State* L);
  static int textdimension(lua_State* L);  
  static int beginpath(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::BeginPath); }
  static int endpath(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::EndPath); } 
  static int fillpath(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::FillPath); } 
  static int drawpath(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::DrawPath); }
  static int drawfillpath(lua_State* L) { LUAEXPORTM(L, meta, &ui::Graphics::DrawFillPath); }
  static int lineto(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g =  LuaHelper::check_sptr<Graphics>(L, 1, meta);        
    Point::Ptr to = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    g->LineTo(*to.get());
    return LuaHelper::chaining(L);
  }
  static int moveto(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);        
    Point::Ptr to = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    g->MoveTo(*to.get());
    return LuaHelper::chaining(L);
  }
  static int curveto(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);        
    Point::Ptr control_p1 = LuaHelper::check_sptr<Point>(L, 2, LuaPointBind::meta);
    Point::Ptr control_p2 = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
    Point::Ptr p = LuaHelper::check_sptr<Point>(L, 4, LuaPointBind::meta);
    g->CurveTo(*control_p1.get(), *control_p2.get(), *p.get());
    return LuaHelper::chaining(L);
  }
  static int arcto(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);        
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    Point::Ptr p1 = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
    Point::Ptr p2 = LuaHelper::check_sptr<Point>(L, 4, LuaPointBind::meta);
    g->ArcTo(*rect.get(), *p1.get(), *p2.get());
    return LuaHelper::chaining(L);
  }
  static int drawarc(lua_State *L) {
    using namespace ui;
    Graphics::Ptr g = LuaHelper::check_sptr<Graphics>(L, 1, meta);        
    Rect::Ptr rect = LuaHelper::check_sptr<Rect>(L, 2, LuaUiRectBind::meta);
    Point::Ptr p1 = LuaHelper::check_sptr<Point>(L, 3, LuaPointBind::meta);
    Point::Ptr p2 = LuaHelper::check_sptr<Point>(L, 4, LuaPointBind::meta);
    g->DrawArc(*rect.get(), *p1.get(), *p2.get());
    return LuaHelper::chaining(L);
  }
};

class LuaPopupMenu : public ui::PopupMenu, public LuaState {
 public:  
  LuaPopupMenu(lua_State* state) : LuaState(state) {}

  virtual void OnMenuItemClick(boost::shared_ptr<ui::Node> node);
};

class LuaScintilla : public LuaWindowBase<ui::Scintilla> {
 public:  
  LuaScintilla(lua_State* L) : LuaWindowBase<ui::Scintilla>(L) {}    

  virtual void OnMarginClick(int line_pos);
};

class LuaFrame : public LuaWindowBase<ui::Frame> {
 public:   
   typedef boost::shared_ptr<LuaFrame> Ptr;
   LuaFrame(lua_State* L) : LuaWindowBase<ui::Frame>(L) {}   
      
   virtual void OnClose();
   virtual void OnShow();
   virtual void OnContextPopup(ui::Event&, const ui::Point& mouse_point,
                               const ui::Node::Ptr& node);
};

class LuaPopupFrame : public LuaWindowBase<ui::PopupFrame> {
 public:   
   typedef boost::shared_ptr<LuaPopupFrame> Ptr;
   LuaPopupFrame(lua_State* L) : LuaWindowBase<ui::PopupFrame>(L) {}   
      
   virtual void OnClose();
   virtual void OnShow();
};

class LuaSplitter : public ui::Splitter, public LuaState {
 public:  
  LuaSplitter(lua_State* L) : LuaState(L) {}
  LuaSplitter(lua_State* L, const ui::Orientation::Type& orientation) 
    : ui::Splitter(orientation), LuaState(L) {
  }  
};

class LuaScrollBar : public ui::ScrollBar, public LuaState {
 public:  
  LuaScrollBar(lua_State* state) : LuaState(state) {}
  LuaScrollBar(lua_State* state, const ui::Orientation::Type& orientation) 
    : ui::ScrollBar(orientation), LuaState(state) {
  }
    
  virtual void OnScroll(int pos);   
};

class LuaOrnament : public ui::Ornament, public LuaState {
 public:
  LuaOrnament() : Ornament(), LuaState(0) {}
  LuaOrnament(lua_State* L) : Ornament(), LuaState(L) {}
  virtual ~LuaOrnament() {};
  virtual ui::Ornament* Clone();

  virtual bool transparent() const;
  virtual void Draw(ui::Window& window, ui::Graphics* g, ui::Region& draw_region);
  virtual std::auto_ptr<ui::Rect> padding() const;
  virtual ui::BoxSpace preferred_space() const;
};

struct LuaOrnamentBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);
};

struct OrnamentFactoryBind {
  static int open(lua_State *L);
  static std::string meta;
  static int create(lua_State *L); 
  static int createlineborder(lua_State* L);
  static int createwallpaper(lua_State* L);
  static int createfill(lua_State* L);
  static int createcirclefill(lua_State* L);
  static int createboundfill(lua_State* L);
};

struct LineBorderBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);
  static int setborderradius(lua_State* L);
  static int setborderstyle(lua_State* L);
};

struct WallpaperBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);  
};

struct FillBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);
  static int setcolor(lua_State* L) { LUAEXPORTM(L, FillBind::meta, &ui::Fill::set_color); } 
};

struct CircleFillBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);  
};

class LuaGameController : public ui::GameController,  public LuaState {
 public:
  LuaGameController() : ui::GameController(), LuaState(0) {}
  LuaGameController(lua_State* L) : ui::GameController(), LuaState(L) {}

  virtual void OnButtonDown(int button);
  virtual void OnButtonUp(int button);
  virtual void OnXAxis(int pos, int old_pos);
  virtual void OnYAxis(int pos, int old_pos);
  virtual void OnZAxis(int pos, int old_pos);
};

struct LuaGameControllerBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);  
  static int xposition(lua_State* L) { LUAEXPORTM(L, LuaGameControllerBind::meta, &ui::GameController::xposition); }
  static int yposition(lua_State* L) { LUAEXPORTM(L, LuaGameControllerBind::meta, &ui::GameController::yposition); }
  static int zposition(lua_State* L) { LUAEXPORTM(L, LuaGameControllerBind::meta, &ui::GameController::zposition); }
  static int buttons(lua_State* L);    
};

class LuaGameControllers : public ui::GameControllers<LuaGameController>, public LuaState {
  public:
   LuaGameControllers() : ui::GameControllers<LuaGameController>(), LuaState(0) {}
   LuaGameControllers(lua_State* L) : ui::GameControllers<LuaGameController>(), LuaState(L) {}
   
   virtual void OnConnect(LuaGameController& controller);
   virtual void OnDisconnect(LuaGameController& controller);  
};

struct LuaGameControllersBind {
  static std::string meta;
  static int open(lua_State *L);  
  static int create(lua_State *L); 
  static int gc(lua_State* L);  
  static int update(lua_State* L) { LUAEXPORTM(L, LuaGameControllersBind::meta, &LuaGameControllers::Update); }
  static int controllers(lua_State* L);
};

template <class T>
class LuaColorMixIn {
 public:  
  static int setmethods(lua_State* L) {     
     static const luaL_Reg methods[] = {
      {"setcolor", setcolor},
      {"color", color},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int setcolor(lua_State* L) {
    using namespace ui;
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, T::type());
    ARGB color = LuaHelper::check32bit(L, 2);    
    window->set_color(color);
    return LuaHelper::chaining(L);     
  }
  static int color(lua_State* L) { LUAEXPORT(L, &T::color); }
};

template <class T>
class LuaBackgroundColorMixIn {
 public:  
  static int setmethods(lua_State* L) {     
     static const luaL_Reg methods[] = {
      {"setbackgroundcolor", setbackgroundcolor},
      {"backgroundcolor", backgroundcolor},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int setbackgroundcolor(lua_State* L) {
    using namespace ui;
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, T::type());
    ARGB color = LuaHelper::check32bit(L, 2);    
    window->set_background_color(color);
    return LuaHelper::chaining(L);        
  }
  static int backgroundcolor(lua_State* L) { LUAEXPORT(L, &T::background_color); }
};

class LuaAligner : public ui::Aligner, public LuaState {
 public:
  LuaAligner() : LuaState(0) {}
  LuaAligner(lua_State* L) : LuaState(L) {}
  ~LuaAligner() {}

  virtual void CalcDimensions();
  virtual void SetPositions();
};

struct LuaAlignerBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int windows(lua_State* L);
  static int setdimension(lua_State* L);
  static int groupdimension(lua_State* L);
};

template<class T = LuaWindow>
class LuaWindowBind {
 public:
  static const std::string meta;  
  typedef LuaWindowBind B;
  static int open(lua_State *L) {
    using namespace ui;
    LuaHelper::openex(L, meta, setmethods, gc);   
    static const char* const e[] = {
      "DOTTED", "DASHED", "SOLID", "DOUBLE", "GROOVE", "RIDGE", "INSET", 
      "OUTSET", "NONE", "HIDDEN"
    };
    size_t size = sizeof(e)/sizeof(e[0]);    
    LuaHelper::buildenum(L, e, size); 
    return 1;
  }
  static int setmethods(lua_State* L) {
    static const luaL_Reg methods[] = {
      {"new", create},
      {"setposition", setposition},
//      {"scrollto", scrollto},
     // {"setsize", setsize},     
      {"position", position},
      {"absoluteposition", absoluteposition},
      {"desktopposition", desktopposition},
	  {"setdimension", setdimension},
      {"dimension", dimension},
      {"setmindimension", setmindimension},
      {"setautosize", setautosize},
      {"autosize", autosize},
      {"setdebugtext", setdebugtext},      
      {"setfocus", setfocus},
      {"hasfocus", hasfocus},
      {"show", show},
      {"hide", hide},
      {"visible", visible},
      {"updatearea", updatearea},
      {"enablepointerevents", enablepointerevents},
      {"disablepointerevents", disablepointerevents},
      {"parent", parent},
      {"root", root},
      {"bounds", bounds},      
      //{"intersect", intersect},
      {"fls", fls},      
      {"invalidate", invalidate},
      {"preventdraw", preventdraw},
      {"enabledraw", enabledraw},      
      {"flsprevented", flsprevented},
	  {"preventdrawbackground", preventdrawbackground},
      {"enabledrawbackground", enabledrawbackground},      
      {"drawbackgroundprevented", drawbackgroundprevented},     
      {"area", area},
      {"drawregion", drawregion},
      {"setclip", setclip},
      {"clip", clip},
      {"tostring", tostring},   
      {"addornament", addornament},
      {"removeornaments", removeornaments},
      {"ornaments", ornaments},
      {"setcursor", setcursor},
	  {"cursor", cursor},
      {"setclipchildren", setclipchildren},
      {"addstyle", addstyle},
      {"removestyle", removestyle},      
      {"setalign", setalign},
      {"setmargin", setmargin},
      {"margin", margin},
      {"setpadding", setpadding},
      {"padding", padding},
      {"align", align},
      {"capturemouse", setcapture},
      {"releasemouse", releasecapture},
      {"setaligner", setaligner},
      {"enable", enable},
      {"disable", disable},
      {"showcursor", showcursor},
      {"hidecursor", hidecursor},
      {"setcursorpos", setcursorposition},      
	  {"cursorposition", cursorposition},
      {"viewdoublebuffered", viewdoublebuffered},
      {"viewsinglebuffered", viewsinglebuffered},
      {"bringtotop", bringtotop},
      {"mapcapslocktoctrl", mapcapslocktoctrl},  
      {"enablecapslock", enablecapslock},
      {"addsettings", addsettings},
      {"addrule", addrule},
      {"link", link},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int create(lua_State *L);
  static boost::shared_ptr<ui::Group> testgroup(lua_State *L);
  static int gc(lua_State* L);
  static int draw(lua_State* L) { return 0; }
  static int invalidate(lua_State* L) { LUAEXPORT(L, &T::Invalidate); }
  static int setcursor(lua_State* L);
  static int cursor(lua_State* L);
  static int setalign(lua_State* L);
  static int align(lua_State* L);
  static int setmargin(lua_State* L);
  static int margin(lua_State* L);
  static int setpadding(lua_State* L);
  static int padding(lua_State* L);
  static int enable(lua_State *L) { LUAEXPORT(L, &T::Enable); }
  static int disable(lua_State *L) { LUAEXPORT(L, &T::Disable); }
  static int showcursor(lua_State* L) { LUAEXPORT(L, &T::ShowCursor); }
  static int hidecursor(lua_State* L) { LUAEXPORT(L, &T::HideCursor); }
  static int setcursorposition(lua_State* L);
  static int cursorposition(lua_State* L);
  static int viewdoublebuffered(lua_State* L) { LUAEXPORT(L, &T::ViewDoubleBuffered); }
  static int viewsinglebuffered(lua_State* L) { LUAEXPORT(L, &T::ViewSingleBuffered); }
  static int bringtotop(lua_State* L) { LUAEXPORT(L, &T::BringToTop); }
  static int hasfocus(lua_State* L) { LUAEXPORT(L, &T::has_focus); }
  static int mapcapslocktoctrl(lua_State* L) { LUAEXPORT(L, &T::MapCapslockToCtrl); }
  static int enablecapslock(lua_State* L) { LUAEXPORT(L, &T::EnableCapslock); }
  static int addstyle(lua_State *L) {
//C    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
//C   UINT style = (unsigned int) luaL_checknumber(L, 2);    
//C   window->add_style(style);
    return LuaHelper::chaining(L);
  }
  static int autosize(lua_State *L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);   
    lua_pushboolean(L, window->auto_size_width());
    lua_pushboolean(L, window->auto_size_height());    
    return 2;
  } 
  static int removestyle(lua_State *L) {
//C    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
//C    UINT style = (unsigned int) luaL_checknumber(L, 2);    
//C    window->remove_style(style);
    return LuaHelper::chaining(L);
  }
  static int setposition(lua_State *L) {        
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    boost::shared_ptr<ui::Rect> pos = LuaHelper::check_sptr<ui::Rect>(L, 2, LuaUiRectBind::meta);
    window->set_position(*pos);    
    return LuaHelper::chaining(L);
  }
  static int updatearea(lua_State *L) { LUAEXPORT(L, &T::needsupdate) }
  static int position(lua_State *L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
    LuaHelper::requirenew<LuaUiRectBind>(L, "psycle.ui.rect", new ui::Rect(window->position()));
    return 1;
  }  
  static int setautosize(lua_State *L) { LUAEXPORT(L, &T::set_auto_size) }
  static int absoluteposition(lua_State* L) {     
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
    LuaHelper::requirenew<LuaUiRectBind>(L, "psycle.ui.rect", new ui::Rect(window->absolute_position()));
    return 1;
  }
  static int desktopposition(lua_State* L) {    
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
    LuaHelper::requirenew<LuaUiRectBind>(L, "psycle.ui.rect", new ui::Rect(window->desktop_position()));
    return 1;    
  }
  static int setdimension(lua_State *L) {
    using namespace ui;        
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    boost::shared_ptr<Dimension> dimension = LuaHelper::check_sptr<Dimension>(L, 2, LuaDimensionBind::meta);
    window->set_position(Rect(Point(), *dimension));    
    return LuaHelper::chaining(L);
  }
  static int dimension(lua_State* L) {    
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
    LuaHelper::requirenew<LuaDimensionBind>(L, "psycle.ui.dimension", new ui::Dimension(window->dim()));
    return 1;    
  }
  static int setmindimension(lua_State *L) {        
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    boost::shared_ptr<ui::Dimension> dimension = LuaHelper::check_sptr<ui::Dimension>(L, 2, LuaDimensionBind::meta);
    window->set_min_dimension(*dimension);    
    return LuaHelper::chaining(L);
  }
  static int fls(lua_State *L);
  static int preventdraw(lua_State *L) { LUAEXPORT(L, &T::PreventFls) }
  static int enabledraw(lua_State *L) { LUAEXPORT(L, &T::EnableFls) }
  static int flsprevented(lua_State *L) { LUAEXPORT(L, &T::fls_prevented) }
  static int preventdrawbackground(lua_State *L) { LUAEXPORT(L, &T::PreventDrawBackground) }
  static int enabledrawbackground(lua_State *L) { LUAEXPORT(L, &T::EnableDrawBackground) }
  static int drawbackgroundprevented(lua_State *L) { LUAEXPORT(L, &T::draw_background_prevented) }    
  static int show(lua_State* L);
  static int hide(lua_State* L) { LUAEXPORT(L, &T::Hide) }
  static int enablepointerevents(lua_State* L) { LUAEXPORT(L, &T::EnablePointerEvents); }
  static int disablepointerevents(lua_State* L) { LUAEXPORT(L, &T::DisablePointerEvents); }
  static int setclipchildren(lua_State* L) { LUAEXPORT(L, &T::set_clip_children); }
  static int bounds(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    ui::Rect bounds = window->area().bounds();
    lua_pushnumber(L, bounds.left());
    lua_pushnumber(L, bounds.top());
    lua_pushnumber(L, bounds.width());
    lua_pushnumber(L, bounds.height());
    return 4;
  }  
  static int parent(lua_State *L);
  static int root(lua_State* L);
  static int area(lua_State *L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    ui::Area* rgn = window->area().Clone();
    ui::Area ** ud = (ui::Area **)lua_newuserdata(L, sizeof(ui::Area *));
    *ud = rgn;
    luaL_setmetatable(L, LuaAreaBind::meta);
    return 1;
  }
  static int drawregion(lua_State *L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    std::auto_ptr<ui::Region> rgn = window->draw_region();    
    if (rgn.get()) {
      ui::Region ** ud = (ui::Region **)lua_newuserdata(L, sizeof(ui::Region *));      
      *ud = rgn.get();     
      rgn.release();
      luaL_setmetatable(L, LuaRegionBind::meta);
    } else {
      lua_pushnil(L);
    }
    return 1;
  }
  static int setclip(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    window->SetClip(ui::Rect(ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)), 
                           ui::Point(luaL_checknumber(L, 3), luaL_checknumber(L, 4))));
    return LuaHelper::chaining(L);
  }
  static int clip(lua_State *L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    ui::Region ** ud = (ui::Region **)lua_newuserdata(L, sizeof(ui::Region *));      
    *ud = window->clip().Clone();          
    luaL_setmetatable(L, LuaRegionBind::meta);    
    return 1;
  }
  static int tostring(lua_State* L) {
    lua_pushstring(L, T::type().c_str());
    return 1;
  }
  static int setdebugtext(lua_State* L) { LUAEXPORT(L, &T::set_debug_text); }
  static int visible(lua_State* L) { LUAEXPORT(L, &T::visible); }
  static int addornament(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    boost::shared_ptr<ui::Ornament> ornament;
    if (!lua_isnil(L, 2)) {      
      ornament = LuaHelper::test_sptr<ui::LineBorder>(L, 2, LineBorderBind::meta);
      if (!ornament) {
        ornament = LuaHelper::test_sptr<ui::Wallpaper>(L, 2, WallpaperBind::meta);
        if (!ornament) {
          ornament = LuaHelper::test_sptr<ui::Fill>(L, 2, FillBind::meta);
		  if (!ornament) {
		    ornament = LuaHelper::check_sptr<LuaOrnament>(L, 2, LuaOrnamentBind::meta);
		  }
        }
      }            
    }    
    lua_getfield(L, 1, "_ornaments");
    if (lua_isnil(L, -1)) {
      lua_newtable(L);
      lua_setfield(L, 1, "_ornaments");
    }
    lua_getfield(L, 1, "_ornaments");
    int n = lua_rawlen(L, -1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, n + 1);    
    window->add_ornament(ornament);
    return LuaHelper::chaining(L);
  }
  static int removeornaments(lua_State* L) {    
    lua_pushnil(L);
    lua_setfield(L, 1, "_ornaments");
    LUAEXPORT(L, &T::RemoveOrnaments);
  }
  static int ornaments(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    if (window->ornaments().empty()) {
      lua_pushnil(L);
    } else {
      /*luaL_requiref(L, "psycle.ui.canvas.ornament", LuaOrnamentBind::open, true);
      int n = lua_gettop(L);                  
      ui::Ornament* ornament = window->ornament().lock()->Clone();
      LuaHelper::new_shared_userdata<>(L, LuaOrnamentBind::meta, window->ornament().lock(), n, true);     */
      lua_pushnumber(L, 1); // todo
    }
    return 1;
  }
  static int setaligner(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    boost::shared_ptr<LuaAligner> aligner = LuaHelper::test_sptr<LuaAligner>(L, 2, LuaAlignerBind::meta);
	if (aligner) {
      window->set_aligner(aligner);
	}
    return LuaHelper::chaining(L);
  }
  static int setcapture(lua_State* L) { LUAEXPORT(L, &T::SetCapture); }
  static int releasecapture(lua_State* L) { LUAEXPORT(L, &T::ReleaseCapture); }
  static int setfocus(lua_State *L) { LUAEXPORT(L, &T::SetFocus) }
  static int addsettings(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    return LuaHelper::chaining(L);
  }
  static int link(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int size = lua_rawlen(L, 2);
    for (int i = 1; i <= size; ++i) {
      lua_rawgeti(L, 2, i);
      processrule(L, window);
      lua_pop(L, 1);
    }
    window->RefreshRules();
    return LuaHelper::chaining(L);
  }
  static int addrule(lua_State* L) {
    boost::shared_ptr<T> window = LuaHelper::check_sptr<T>(L, 1, meta);    
    luaL_checktype(L, 2, LUA_TTABLE);    
    processrule(L, window);
    return LuaHelper::chaining(L);
  }
  static int processrule(lua_State* L, const ui::Window::Ptr& window) {
    ui::Properties properties;
    std::string selector = "";    
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      lua_pushvalue(L, -2);
      const char *key = lua_tostring(L, -1);      
      if (strcmp(key, "selector") == 0) {
        selector = std::string(lua_tostring(L, -2));
      } else
      if (strcmp(key, "properties") == 0) {
        lua_pushvalue(L, -2);
        luaL_checktype(L, -1, LUA_TTABLE);    
        lua_pushnil(L);
        ui::Property prop;
        while (lua_next(L, -2) != 0) {
          lua_pushvalue(L, -2);
          const char *key = lua_tostring(L, -1);
          const int idx = -2;
          switch (lua_type(L, idx)) {
            case LUA_TSTRING: {
              const char* value = lua_tostring(L, idx);
              if (strlen(value) > 0) {
                char first_char = value[0];
                if (first_char == '@') {
                  std::string stock(value + 1);
                  using namespace stock::color;
                  stock::color::type stock_id = UNDEFINED;
                  if (stock == "PVBACKGROUND") {
                    stock_id = PVBACKGROUND;
                  } else
                  if (stock == "PVROW") {
                    stock_id = PVROW;
                  } else
                  if (stock == "PVROW4BEAT") {
                    stock_id = PVROW4BEAT;
                  } else
                  if (stock == "PVROWBEAT") {
                    stock_id = PVROWBEAT;
                  } else                  
                  if (stock == "PVROWBEAT") {
                    stock_id = PVROWBEAT;
                  } else
                  if (stock == "PVSEPARATOR") {
                    stock_id = PVSEPARATOR;
                  } else
                  if (stock == "PVFONT") {
                    stock_id = PVFONT;
                  } else
				  if (stock == "PVFONTSEL") {
                    stock_id = PVFONTSEL;
                  } else
                  if (stock == "PVFONTPLAY") {
                    stock_id = PVFONTPLAY;
                  } else   
                  if (stock == "PVCURSOR") {
                    stock_id = PVCURSOR;
                  } else  
                  if (stock == "PVSELECTION") {
                    stock_id = PVSELECTION;
                  } else
                  if (stock == "PVPLAYBAR") {
                    stock_id = PVPLAYBAR;
                  } else
                  if (stock == "MVFONTBOTTOMCOLOR") {
                    stock_id = MVFONTBOTTOMCOLOR;
                  } else
                  if (stock == "MVFONTHTOPCOLOR") {
                    stock_id = MVFONTHTOPCOLOR;
                  } else
                  if (stock == "MVFONTHBOTTOMCOLOR") {
                    stock_id = MVFONTHBOTTOMCOLOR;
                  } else
                  if (stock == "MVFONTTITLECOLOR") {
                    stock_id = MVFONTTITLECOLOR;
                  } else
                  if (stock == "MVTOPCOLOR") {
                    stock_id = MVTOPCOLOR;
                  } else
                  if (stock == "MVBOTTOMCOLOR") {
                    stock_id = MVBOTTOMCOLOR;
                  } else
                  if (stock == "MVHTOPCOLOR") {
                    stock_id = MVHTOPCOLOR;
                  }  else
                  if (stock == "MVHBOTTOMCOLOR") {
                    stock_id = MVHBOTTOMCOLOR;
                  } else
                  if (stock == "MVTITLECOLOR") {
                    stock_id = MVTITLECOLOR;
                  }
                  if (stock_id != UNDEFINED) {
                    PsycleStock stock;
                    ui::Property prop(stock);
                    prop.set_stock_key(stock_id);
                    properties.set(key, prop);
                  }
                } else {
                  prop.set_value(std::string(value));
                  properties.set(key, prop);
                }
              }            
            }
            break;
            case LUA_TBOOLEAN:
              //prop.set_value(lua_toboolean(L, idx));
              //properties.set(key, prop);
            break;
            case LUA_TNUMBER:{              
              prop.set_value((ui::ARGB)lua_tonumber(L, idx));
              properties.set(key, prop);
            }
            break;        
            default:          
              luaL_error(L, "Wrong argument type");
          }
          lua_pop(L, 2);
        }
        lua_pop(L, 1);
      }            
      lua_pop(L, 2);
    }
    ui::Rule rule(selector, properties);
    window->add_rule(rule);
    return 0;   
  }
};

template <class T>
const std::string LuaWindowBind<T>::meta = T::type();

template <class T = LuaGroup>
class LuaGroupBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L);
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {   
      {"new", create},      
      {"windowcount", windowcount},
      {"windows", getwindows},
      {"remove", remove},
      {"removeall", removeall},
      {"add", add},
      {"insert", insert},
      {"setzorder", setzorder},
      {"zorder", zorder},
      {"windowindex", zorder},
      {"intersect", intersect},     
      {"updatealign", updatealign},
      {"flagnotaligned", flagnotaligned},
      {"at", at},
      {"empty", empty},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int create(lua_State *L);  
  static int windowcount(lua_State* L) { LUAEXPORT(L, &T::size); }
  static int getwindows(lua_State* L);
  static int remove(lua_State* L);
  static int removeall(lua_State* L);
  static int add(lua_State* L);  
  static int insert(lua_State* L);
  static int setzorder(lua_State* L);
  static int zorder(lua_State* L);
  static int intersect(lua_State* L);
  static int updatealign(lua_State* L) { LUAEXPORT(L, &T::UpdateAlign); }
  static int empty(lua_State* L) { LUAEXPORT(L, &T::empty); }
  static int flagnotaligned(lua_State* L) { LUAEXPORT(L, &T::FlagNotAligned); }
  static ui::Window::Ptr test(lua_State* L, int index); 
  static int at(lua_State* L);  
};

template <class T = LuaHeaderGroup>
class LuaHeaderGroupBind : public LuaGroupBind<T> {
 public:
  typedef LuaGroupBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {   
      {"settitle", settitle},            
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }  
  static int settitle(lua_State* L) { LUAEXPORT(L, &T::set_title); }  
};

template <class T = LuaScrollBox>
class LuaScrollBoxBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {      
      {"scrollby", scrollby},
      {"scrollto", scrollto},
      {"updatescrollrange", updatescrollrange},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }  
  static int scrollby(lua_State* L) { LUAEXPORT(L, &T::ScrollBy); }
  static int scrollto(lua_State* L) { 
    boost::shared_ptr<T> box = LuaHelper::check_sptr<T>(L, 1, B::meta);
    box->ScrollTo(ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
    return LuaHelper::chaining(L);
  }
  static int updatescrollrange(lua_State* L) { LUAEXPORT(L, &T::UpdateScrollRange); }
};

template <class T = LuaViewport>
class LuaViewportBind : public LuaGroupBind<T> {
 public:
  typedef LuaGroupBind<T> B;  
  static int open(lua_State *L); // { return openex<B>(L, meta, setmethods, gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {  
      {"new", create},      
      {"showscrollbar", showscrollbar},
      {"setscrollinfo", setscrollinfo},
      {"invalidateontimer", invalidateontimer},
      {"invalidatedirect", invalidatedirect},
      {"flush", flush},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }      
  static int create(lua_State* L);  
  static int showscrollbar(lua_State* L);
  static int setscrollinfo(lua_State* L);
  static int invalidateontimer(lua_State* L);
  static int invalidatedirect(lua_State* L);
  static int flush(lua_State* L) { LUAEXPORT(L, &T::Flush); }  
};

template<class T = LuaRectangleBox>
class LuaRectangleBoxBind : public LuaWindowBind<T>, public LuaColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { 
    return LuaHelper::openex(L, B::meta, setmethods, B::gc);
  }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaColorMixIn<T>::setmethods(L);    
    return 0;
  }  
};

template <class T = LuaLine>
class LuaLineBind : public LuaWindowBind<T>, public LuaColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
      B::setmethods(L);
      LuaColorMixIn<T>::setmethods(L);
      static const luaL_Reg methods[] = {      
      {"setpoints", setpoints},
      {"points", points},
      {"setpoint", setpoint},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }  
  static int setpoints(lua_State* L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    boost::shared_ptr<T> line = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::Points pts;
    size_t len = lua_rawlen(L, 2);
    for (size_t i = 1; i <= len; ++i) {
      lua_rawgeti(L, 2, i); // GetTable
      lua_rawgeti(L, -1, 1); // get px
      double x = luaL_checknumber(L, -1);
      lua_pop(L, 1);
      lua_rawgeti(L, -1, 2); // get py
      double y = luaL_checknumber(L, -1);
      lua_pop(L,2);
      pts.push_back(ui::Point(x, y));
    }
    line->SetPoints(pts);
    return LuaHelper::chaining(L);
  }
  static int setpoint(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 4, "self, idx, x, y");
    if (err!=0) return err;
    boost::shared_ptr<T> line = LuaHelper::check_sptr<T>(L, 1, B::meta);
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    double x = luaL_checknumber(L, 3);
    double y = luaL_checknumber(L, 4);
    ui::Point pt(x, y);
    line->SetPoint(idx-1,  pt);
    return LuaHelper::chaining(L);
  }
  static int points(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 1, "self");
    if (err!=0) return err;
    boost::shared_ptr<T> line = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::Points pts = line->points();
    lua_newtable(L);
    ui::Points::iterator it = pts.begin();
    int k = 1;
    for (; it != pts.end(); ++it) {
      lua_newtable(L);
      lua_pushnumber(L, (*it).x());
      lua_setfield(L, -2, "x");
      lua_pushnumber(L, (*it).y());
      lua_setfield(L, -2, "y");
      lua_rawseti(L, -2, k++);
    }
    return 1;
  }
};

template <class T>
class LuaTextMixIn {
 public:  
  static int setmethods(lua_State* L) {     
     static const luaL_Reg methods[] = {
      {"settext", settext},
      {"text", text},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int settext(lua_State* L) { LUAEXPORT(L, &T::set_text); }
  static int text(lua_State* L) { LUAEXPORT(L, &T::text); }
};

template <typename T>
class LuaFontMixIn {
 public:  
  static int setmethods(lua_State* L) {     
     static const luaL_Reg methods[] = {   
      {"setfont", setfont},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }    
  static int setfont(lua_State* L) {    
    using namespace ui;
    typename T::Ptr window = LuaHelper::check_sptr<T>(L, 1, T::type());    
    Font::Ptr font = LuaHelper::check_sptr<Font>(L, 2, LuaFontBind::meta);        
    window->set_font(*font.get());
    return LuaHelper::chaining(L);
  }  
};

template <class T = LuaText>
class LuaTextBind : public LuaWindowBind<T>, public LuaTextMixIn<T>, public LuaColorMixIn<T>, LuaFontMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) {
    using namespace ui;
    LuaHelper::openex(L, T::type(), setmethods, B::gc); 
    LuaHelper::setfield(L, "LEFTJUSTIFY", JustifyStyle::LEFTJUSTIFY);
    LuaHelper::setfield(L, "CENTERJUSTIFY", JustifyStyle::CENTERJUSTIFY);
    LuaHelper::setfield(L, "RIGHTJUSTIFY", JustifyStyle::RIGHTJUSTIFY);
    return 1;    
  }
  static int setmethods(lua_State* L) {
     B::setmethods(L);
     LuaTextMixIn<T>::setmethods(L);
     LuaColorMixIn<T>::setmethods(L);
     LuaFontMixIn<T>::setmethods(L);
     static const luaL_Reg methods[] = {         
      {"setverticalalignment", setverticalalignment},
      {"setjustify", setjustify},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }  
  static int setverticalalignment(lua_State* L) {    
    LuaHelper::check_sptr<LuaText>(L, 1, T::type())
      ->set_vertical_alignment(
           static_cast<ui::AlignStyle::Type>(luaL_checkinteger(L, 2)));
    return LuaHelper::chaining(L);
  }
  static int setjustify(lua_State* L) {    
    LuaHelper::check_sptr<LuaText>(L, 1, T::type())
      ->set_justify(
          static_cast<ui::JustifyStyle::Type>(luaL_checkinteger(L, 2)));
    return LuaHelper::chaining(L);
  }
};

template <class T = LuaPic>
class LuaPicBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { 
   return LuaHelper::openex(L, B::meta, setmethods, B::gc);
  }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {
      {"setsource", setsource},
      {"setimage", setimage},
      {"setzoom", setzoom},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int setsource(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 3, "self, x, y");
    if (err!=0) return err;
    boost::shared_ptr<T> pic = LuaHelper::check_sptr<T>(L, 1, B::meta);
    pic->SetSource(ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
    return LuaHelper::chaining(L);    
  }
  static int setzoom(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 3, "self, x, y");
    if (err!=0) return err;
    boost::shared_ptr<T> pic = LuaHelper::check_sptr<T>(L, 1, B::meta);
    pic->set_zoom(ui::Point(luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
    return LuaHelper::chaining(L);    
  }
  static int setimage(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 2, "self, image");
    if (err!=0) return err;
    boost::shared_ptr<T> pic = LuaHelper::check_sptr<T>(L, 1, B::meta);
    pic->SetImage(LuaHelper::check_sptr<ui::Image>(L, 2, LuaImageBind::meta).get());
    return LuaHelper::chaining(L);
  } 
};

template <class T = LuaButton>
class LuaButtonBind : public LuaWindowBind<T>, public LuaTextMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaTextMixIn<T>::setmethods(L);
    return 0;
  }
};

template <class T = LuaScopeWindow>
class LuaScopeBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);  
	static const luaL_Reg methods[] = {		
	    {"resize", resize},
		{NULL, NULL}
	};
	luaL_setfuncs(L, methods, 0);
    return 0;
  }  
  static int resize(lua_State* L) {
    boost::shared_ptr<LuaScopeWindow> scopes = LuaHelper::check_sptr<LuaScopeWindow>(L, 1, meta);	
	scopes->OnSize(scopes->parent()->dim());
    return LuaHelper::chaining(L);
  }
};

template <class T = LuaSplitter>
class LuaSplitterBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) {
  LuaHelper::openex(L, B::meta, setmethods, B::gc);
  LuaHelper::setfield(L, "HORZ", ui::Orientation::HORZ);
  LuaHelper::setfield(L, "VERT", ui::Orientation::VERT);
  return 1;
  }
  static int create(lua_State *L) {
  int n = lua_gettop(L);  // Number of arguments  
    boost::shared_ptr<LuaGroup> group;
    ui::Orientation::Type orientation = ui::Orientation::VERT;
    if (n>=2 && !lua_isnil(L, 2)) {
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
		if (n==3) {
		  orientation = (ui::Orientation::Type) luaL_checkinteger(L, 3);
		}
    }
    boost::shared_ptr<T> window = LuaHelper::new_shared_userdata(L, B::meta.c_str(), new T(L, orientation));    
    LuaHelper::register_weakuserdata(L, window.get());
    if (group) {    
      group->Add(window);
      LuaHelper::register_userdata(L, window.get());
    }  
    LuaHelper::new_lua_module(L, "psycle.signal");  
    lua_setfield(L, -2, "keydown");
    return 1;
  }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {
       {"new", create},   
       {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  } 
};

template <class T = LuaGroupBox>
class LuaGroupBoxBind : public LuaWindowBind<T>, public LuaTextMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaTextMixIn<T>::setmethods(L);
    return 0;
  } 
};

template <class T = LuaRadioButton>
class LuaRadioButtonBind : public LuaWindowBind<T>, public LuaTextMixIn<T> {
public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaTextMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {     
      {"check", check},
      {"uncheck", uncheck},
      {"checked", checked},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  } 
  static int check(lua_State *L) { LUAEXPORT(L, &T::Check); }
  static int uncheck(lua_State *L) { LUAEXPORT(L, &T::UnCheck); }
  static int checked(lua_State *L) { LUAEXPORT(L, &T::checked); }
};

template <class T = LuaComboBox>
class LuaComboBoxBind : public LuaWindowBind<T>, LuaTextMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaTextMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {
       {"setitems", setitems},
       {"items", items},
       {"setitemindex", setitemindex},
       {"itemindex", itemindex},
       {"additem", additem},       
       {"setitembytext", setitembytext},       
       {"clear", clear},
       {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int setitems(lua_State* L) {
    boost::shared_ptr<T> combo_box = LuaHelper::check_sptr<T>(L, 1, B::meta);
    std::vector<std::string> itemlist;
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = lua_rawlen(L, 2);
    for (int i = 1; i <= n; ++i) {
      lua_rawgeti(L, 2, i);
      const char* str = luaL_checkstring(L, -1);
      lua_pop(L, 1);
      itemlist.push_back(str);
    }
    combo_box->set_items(itemlist);
    return LuaHelper::chaining(L);
  }
  static int additem(lua_State* L) {
    boost::shared_ptr<T> combo_box = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    const char* str = luaL_checkstring(L, -1);    
    combo_box->add_item(str);
    return LuaHelper::chaining(L);
  }
  static int items(lua_State* L) {
    boost::shared_ptr<T> combo_box = LuaHelper::check_sptr<T>(L, 1, B::meta);
    lua_newtable(L);
    std::vector<std::string> itemlist = combo_box->items();
    std::vector<std::string>::iterator it = itemlist.begin();
    for (int i = 1; it != itemlist.end(); ++it, ++i) {
      lua_pushstring(L, (*it).c_str());
      lua_rawseti(L, -2, i);
    }    
    return 1;
  }
  static int itemindex(lua_State* L) {
    boost::shared_ptr<T> combo_box = LuaHelper::check_sptr<T>(L, 1, B::meta);
    lua_pushinteger(L, combo_box->item_index() + 1);
    return 1;
  };
  static int setitemindex(lua_State* L) {
    boost::shared_ptr<T> combo_box = LuaHelper::check_sptr<T>(L, 1, B::meta);
    int item_index = static_cast<int>(luaL_checkinteger(L, 2) - 1);
    combo_box->set_item_index(item_index);
    return LuaHelper::chaining(L);
  };
  static int settext(lua_State* L) { LUAEXPORT(L, &T::set_text); }
  static int text(lua_State* L) { LUAEXPORT(L, &T::text); }
  static int clear(lua_State* L) { LUAEXPORT(L, &T::Clear); }
  static int setitembytext(lua_State* L) { LUAEXPORT(L, &T::set_item_by_text); }
};

struct LuaFrameAlignerBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int gc(lua_State* L);
  static int sizetoscreen(lua_State* L) { LUAEXPORTM(L, meta, &ui::FrameAligner::SizeToScreen) };
};

struct LuaPopupMenuBind {
  static int open(lua_State *L);
  static const char* meta;
  static int create(lua_State *L);
  static int add(lua_State *L);  
  static int gc(lua_State* L);
  static int setrootnode(lua_State* L);  
  static int update(lua_State* L) { LUAEXPORTM(L, meta, &LuaPopupMenu::Update); }
  static int invalidate(lua_State* L) { LUAEXPORTM(L, meta, &LuaPopupMenu::Invalidate); }
  static int track(lua_State* L);
};

class LuaNodeBind : public LuaTextMixIn<ui::Node> {
 public:
  static std::string meta;
  static int open(lua_State *L) {    
    static const luaL_Reg methods[] = {
      {"new", create},
      {"setcommand", setcommand},     
      {"setimage", setimage},
      {"setimageindex", setimageindex},
      {"setselectedimageindex", setselectedimageindex},      
      {"add", add},
      {"insertafter", insertafter},
      {"insertfront", insertfront},
      {"size", size},
      {"empty", empty},
      {"at", at},
      {"remove", remove},
      {"clear", clear},
      {"parent", parent},
      {"level", level},
      {"setname", setname},
      {"name", name},
      {"select", select},
      {"deselect", deselect},
      {"selected", selected},
      {NULL, NULL}
    };    
    LuaHelper::open(L, meta, methods,  gc);
    LuaTextMixIn<ui::Node>::setmethods(L);
    return 1;
  }

  static int create(lua_State* L) {
    int n = lua_gettop(L);
    if (n==1) {
      LuaHelper::new_shared_userdata(L, meta.c_str(), new ui::Node());
    } else 
    if (n==2) {            
      ui::Node::Ptr parent_node = LuaHelper::check_sptr<ui::Node>(L, 2, meta);
      ui::Node::Ptr node = LuaHelper::new_shared_userdata(L, meta.c_str(), new ui::Node());
      parent_node->AddNode(node);
      lua_getfield(L, -2, "_children");
      int len = lua_rawlen(L, -1);
      lua_pushvalue(L, -2);
      lua_rawseti(L, -2, len+1);
      lua_pushvalue(L, 3);
    } else {
      return luaL_error(L, "Wrong Number Of Arguments.");
    }
    lua_newtable(L);
    lua_setfield(L, -2, "_children");
    return 1;
  }
  static int gc(lua_State* L) {
    return LuaHelper::delete_shared_userdata<ui::Node>(L, meta);
  }  
  static int setimage(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 2, "self, image");
    if (err!=0) return err;
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);
    node->set_image(LuaHelper::check_sptr<ui::Image>(L, 2, LuaImageBind::meta));
    return LuaHelper::chaining(L);
  }
  static int setcommand(lua_State* L) {
    int err = LuaHelper::check_argnum(L, 2, "self, command");
    if (err!=0) return err;
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);
    node->set_command(LuaHelper::check_sptr<ui::Command>(L, 2, LuaCommandBind::meta));
    return LuaHelper::chaining(L);
  }
  static int setimageindex(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::set_image_index); }
  static int setselectedimageindex(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::set_selected_image_index); }
  static int add(lua_State* L) {
    ui::Node::Ptr treenode = LuaHelper::check_sptr<ui::Node>(L, 1, meta);    
    ui::Node::Ptr treenode2 = LuaHelper::check_sptr<ui::Node>(L, 2, meta);
    treenode->AddNode(treenode2);
    lua_getfield(L, -2, "_children");
    int len = lua_rawlen(L, -1);
    lua_pushvalue(L, -2);
    lua_rawseti(L, -2, len+1);
    return LuaHelper::chaining(L);
  }
  static int insertfront(lua_State* L) {
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);    
    ui::Node::Ptr node1 = LuaHelper::check_sptr<ui::Node>(L, 2, meta);
    node->insert(node->begin(), node1);        
    lua_getfield(L, 1, "_children");
    int len = lua_rawlen(L, -1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, len+1);
    return LuaHelper::chaining(L);
  }
  static int insertafter(lua_State* L) {
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);    
    ui::Node::Ptr node1 = LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta);
    ui::Node::Ptr node2 = LuaHelper::check_sptr<ui::Node>(L, 3, LuaNodeBind::meta);

    for (ui::Node::iterator it = node->begin(); it != node->end(); ++it) {
      if ((*it) == node2) {
        ++it;
        node->insert(it, node1);
        break;
      }
    }    
    lua_getfield(L, 1, "_children");
    int len = lua_rawlen(L, -1);
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, len+1);
    return LuaHelper::chaining(L);
  }
  static int remove(lua_State *L) {
    if (lua_isnumber(L, 2)) {
      ui::Node::Ptr treenode = LuaHelper::check_sptr<ui::Node>(L, 1, meta);
      int index = static_cast<int>(luaL_checkinteger(L, 2));
      if (treenode->size() == 0 || (index < 1 && index > treenode->size())) {
        luaL_error(L, "index out of range");
      }
      treenode->erase(treenode->begin() + index - 1);
      lua_getfield(L, -2, "_children");
      lua_pushnil(L);
      lua_rawseti(L, -2, index);      
      lua_gc(L, LUA_GCCOLLECT, 0);
      return 1;
    } else {
      ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);
      ui::Node::Ptr node1 = LuaHelper::check_sptr<ui::Node>(L, 2, meta);

      lua_getfield(L, -2, "_children");
      int n = lua_rawlen(L, -1);
      for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, -1, i);
        ui::Node::Ptr node2 = LuaHelper::check_sptr<ui::Node>(L, lua_gettop(L), meta);
        if (node2 == node1) {          
          lua_pushnil(L);
          lua_rawseti(L, -2, i);          
          break;
        }
        lua_remove(L, -1);
      }
      for (ui::Node::iterator it = node->begin(); it != node->end(); ++it) {
        if ((*it) == node1) {          
          node->erase(it);
          break;
        }
      }
      lua_gc(L, LUA_GCCOLLECT, 0);
    }    
    return LuaHelper::chaining(L);
  }
  static int select(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::select); }
  static int deselect(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::deselect); }
  static int selected(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::selected); }
  static int clear(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::clear); }
  static int parent(lua_State *L) {
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 1, meta);
    LuaHelper::find_weakuserdata(L, node->parent());
    return 1;
  }
  static int size(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::size); }
  static int empty(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::empty); }
  static int at(lua_State *L) {
    if (lua_isnumber(L, 2)) {
      ui::Node::Ptr treenode = LuaHelper::check_sptr<ui::Node>(L, 1, meta);  
      int index = static_cast<int>(luaL_checkinteger(L, 2));
      if (treenode->size() == 0 || (index < 1 && index > treenode->size())) {
        luaL_error(L, "index out of range");
      }            
      ui::Node::Ptr tn = *(treenode->begin() + index - 1);
      if (tn.get()) {
        LuaHelper::find_weakuserdata(L, tn.get());
      } else {
        lua_pushnil(L);
      }      
      return 1;
    }
    return 0;
  }
  static int level(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::level); }
  static int setname(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::set_name); }
  static int name(lua_State* L) { LUAEXPORTM(L, meta, &ui::Node::name); }
};

template <class T = LuaTreeView>
class LuaTreeViewBind : public LuaWindowBind<T>, public LuaColorMixIn<T>, public LuaBackgroundColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaColorMixIn<T>::setmethods(L);
    LuaBackgroundColorMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {      
      {"setrootnode", setrootnode},
      {"addnode", addnode},      
//      {"clear", clear},      
      {"isediting", isediting},
      {"updatetree", updatetree},
      {"selectnode", selectnode},
      {"deselectnode", deselectnode},
      {"editnode", editnode},
      {"selected", selected},
      {"showlines", showlines},
      {"hidelines", hidelines},
      {"showbuttons", showbuttons},
      {"hidebuttons", hidebuttons},
      {"setimages", setimages},
      {"setpopupmenu", setpopupmenu},
      {"expandall", expandall},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  
  static int expandall(lua_State* L) { LUAEXPORT(L, &T::ExpandAll); }  
  static int showlines(lua_State* L) { LUAEXPORT(L, &T::ShowLines); }
  static int hidelines(lua_State* L) { LUAEXPORT(L, &T::HideLines); }
  static int showbuttons(lua_State* L) { LUAEXPORT(L, &T::ShowButtons); }
  static int hidebuttons(lua_State* L) { LUAEXPORT(L, &T::HideButtons); }  
  static int isediting(lua_State* L) { LUAEXPORT(L, &T::is_editing) }
  static int updatetree(lua_State* L) { LUAEXPORT(L, &T::UpdateTree); }
  static int selectnode(lua_State* L) { 
    boost::shared_ptr<T> tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    boost::shared_ptr<ui::Node> node = 
        boost::dynamic_pointer_cast<ui::Node>(LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta));
    tree_view->select_node(node);
    return LuaHelper::chaining(L);
  }
  static int deselectnode(lua_State* L) { 
    typename T::Ptr tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta);
    tree_view->deselect_node(node);
    return LuaHelper::chaining(L);
  }
  static int editnode(lua_State* L) { 
    boost::shared_ptr<T> tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    boost::shared_ptr<ui::Node> node = 
        boost::dynamic_pointer_cast<ui::Node>(LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta));
    tree_view->EditNode(node);
    return LuaHelper::chaining(L);
  }
  static int selected(lua_State* L) {
    boost::shared_ptr<T> tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    boost::shared_ptr<ui::Node> tn = tree_view->selected().lock();
    if (tn.get()) {
      LuaHelper::find_weakuserdata(L, tn.get());
    } else {
      lua_pushnil(L);
    }
    return 1;
  }
  static int setrootnode(lua_State* L) {
    typename T::Ptr tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta);
    tree_view->set_root_node(node);
    tree_view->UpdateTree();
    tree_view->FLS();
    return LuaHelper::chaining(L);
  }
  static int addnode(lua_State* L) {    
    return LuaHelper::chaining(L);
  }  
  static int setimages(lua_State* L) {    
    boost::shared_ptr<T> tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::Images::Ptr images = LuaHelper::check_sptr<ui::Images>(L, 2, LuaImagesBind::meta);
    tree_view->set_images(images);
    return LuaHelper::chaining(L);
  }

  static int setpopupmenu(lua_State* L) {    
    boost::shared_ptr<T> tree_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::PopupMenu::Ptr popup_menu = LuaHelper::check_sptr<ui::PopupMenu>(L, 2, LuaPopupMenuBind::meta);
    tree_view->set_popup_menu(popup_menu);
    return LuaHelper::chaining(L);
  }
  
  /*static int clear(lua_State* L) {
    using namespace ui::canvas;
    boost::shared_ptr<T> tree = LuaHelper::check_sptr<T>(L, 1, meta);
    using namespace ui::canvas;
    TreeItem::TreeItemList subitems = tree->SubChildren(); 
    TreeItem::TreeItemList::iterator it = subitems.begin();
    for ( ; it != subitems.end(); ++it) {
      TreeItem::Ptr subitem = *it;
      LuaHelper::unregister_userdata<>(L, subitem.get());
    }   
    tree->Clear();
    return LuaHelper::chaining(L);
  }*/
};

template <class T = LuaListView>
class LuaListViewBind : public LuaWindowBind<T>, public LuaColorMixIn<T>, public LuaBackgroundColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaColorMixIn<T>::setmethods(L);
    LuaBackgroundColorMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {      
      {"setrootnode", setrootnode},
      {"addcolumn", addcolumn},
//    {"clear", clear},      
      {"isediting", isediting},
      {"updatelist", updatelist},
      {"selectnode", selectnode},
      {"deselectnode", deselectnode},
      {"selectednodes", selectednodes},
      {"editnode", editnode},
      {"selected", selected},
      {"setimages", setimages},
      {"viewlist", viewlist},
      {"viewreport", viewreport},
      {"viewicon", viewicon},
      {"viewsmallicon", viewsmallicon},
      {"enablerowselect", enablerowselect},
      {"diablerowselect", disablerowselect},
      {"preventdraw", preventdraw},
      {"enabledraw", enabledraw},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }      
  static int isediting(lua_State* L) { LUAEXPORT(L, &T::is_editing) }
  static int updatelist(lua_State* L) { LUAEXPORT(L, &T::UpdateList); }
  static int viewlist(lua_State* L) { LUAEXPORT(L, &T::ViewList); }
  static int viewreport(lua_State* L) { LUAEXPORT(L, &T::ViewReport); }
  static int viewicon(lua_State* L) { LUAEXPORT(L, &T::ViewIcon); }
  static int viewsmallicon(lua_State* L) { LUAEXPORT(L, &T::ViewSmallIcon); }
  static int preventdraw(lua_State* L) { LUAEXPORT(L, &T::PreventDraw); }
  static int enabledraw(lua_State* L) { LUAEXPORT(L, &T::EnableDraw); }
  static int selectednodes(lua_State* L) { 
    typename T::Ptr list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    lua_newtable(L);
    std::vector<ui::Node::Ptr> nodes = list_view->selected_nodes();
    std::vector<ui::Node::Ptr>::iterator it = nodes.begin();
    for (int i = 1; it != nodes.end(); ++i, ++it) {
      LuaHelper::find_weakuserdata(L, (*it).get());
      lua_rawseti(L, -2, i);
    }      
    return 1;
  }
  static int selectnode(lua_State* L) { 
    typename T::Ptr list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta);
    list_view->select_node(node);
    return LuaHelper::chaining(L);
  }
  static int deselectnode(lua_State* L) { 
    typename T::Ptr list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    ui::Node::Ptr node = LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta);
    list_view->deselect_node(node);
    return LuaHelper::chaining(L);
  }
  static int editnode(lua_State* L) { 
    boost::shared_ptr<T> list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    boost::shared_ptr<ui::Node> node = 
      boost::dynamic_pointer_cast<ui::Node>(LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta));
    list_view->EditNode(node);
    return LuaHelper::chaining(L);
  }
  static int selected(lua_State* L) {
    boost::shared_ptr<T> list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    boost::shared_ptr<ui::Node> tn = list_view->selected().lock();
    if (tn.get()) {
      LuaHelper::find_weakuserdata(L, tn.get());
    } else {
      lua_pushnil(L);
    }
    return 1;
  }
  static int setrootnode(lua_State* L) {
    boost::shared_ptr<T> list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    boost::shared_ptr<ui::Node> node = 
      boost::dynamic_pointer_cast<ui::Node>(LuaHelper::check_sptr<ui::Node>(L, 2, LuaNodeBind::meta));
    list_view->set_root_node(node);
    list_view->UpdateList();
    return LuaHelper::chaining(L);
  }  
  static int addcolumn(lua_State* L) { 
    boost::shared_ptr<T> list_view = LuaHelper::check_sptr<T>(L, 1, B::meta);
    list_view->AddColumn(luaL_checkstring(L, 2), static_cast<int>(luaL_checkinteger(L, 3)));
    return LuaHelper::chaining(L);
  }  
  static int setimages(lua_State* L) {    
    boost::shared_ptr<T> list_view = LuaHelper::check_sptr<T>(L, 1,B::meta);
    ui::Images::Ptr images = LuaHelper::check_sptr<ui::Images>(L, 2, LuaImagesBind::meta);
    list_view->set_images(images);
    return LuaHelper::chaining(L);
  }
  static int enablerowselect(lua_State* L) { LUAEXPORT(L, &T::EnableRowSelect); }
  static int disablerowselect(lua_State* L) { LUAEXPORT(L, &T::DisableRowSelect); }
  
  /*static int clear(lua_State* L) {
    using namespace ui::canvas;
    boost::shared_ptr<T> tree = LuaHelper::check_sptr<T>(L, 1, meta);
    using namespace ui::canvas;
    TreeItem::TreeItemList subitems = tree->SubChildren(); 
    TreeItem::TreeItemList::iterator it = subitems.begin();
    for ( ; it != subitems.end(); ++it) {
      TreeItem::Ptr subitem = *it;
      LuaHelper::unregister_userdata<>(L, subitem.get());
    }   
    tree->Clear();
    return LuaHelper::chaining(L);
  }*/
};

template <class T = LuaFrame>
class LuaFrameBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {
      {"new", create},      
      {"settitle", settitle},
      {"title", title},
      {"setviewport", setviewport},
      {"view", view},
      {"showdecoration", showdecoration},
      {"hidedecoration", hidedecoration},
      {"setpopupmenu", setpopupmenu},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
 
  static int create(lua_State* L) {   
    boost::shared_ptr<T> item = LuaHelper::new_shared_userdata(L, B::meta.c_str(), new T(L));
    LuaHelper::register_weakuserdata(L, item.get());
    return 1;
  }  
  static int setviewport(lua_State* L) {
    LuaFrame::Ptr frame = LuaHelper::check_sptr<LuaFrame>(L, 1, B::meta);
    LuaViewport::Ptr viewport = LuaHelper::check_sptr<LuaViewport>(L, 2, LuaViewportBind<>::meta);
    if (viewport) {
      LuaHelper::register_userdata(L, viewport.get());
      frame->set_viewport(viewport);      
    }    
    return LuaHelper::chaining(L);
  }
  static int settitle(lua_State* L)  { LUAEXPORT(L, &T::set_title); }
  static int title(lua_State* L)  { LUAEXPORT(L, &T::title); }
  static int showdecoration(lua_State* L)  { LUAEXPORT(L, &T::ShowDecoration); }
  static int hidedecoration(lua_State* L)  { LUAEXPORT(L, &T::HideDecoration); }  
  static int view(lua_State* L) {   
    boost::shared_ptr<T> frame = LuaHelper::check_sptr<T>(L, 1, B::meta);
    LuaHelper::find_weakuserdata<>(L, frame->viewport().get());
    return 1;   
  }
  static int setpopupmenu(lua_State* L) {    
    boost::shared_ptr<T> frame = LuaHelper::check_sptr<T>(L, 1, B::meta);
    ui::PopupMenu::Ptr popup_menu = LuaHelper::check_sptr<ui::PopupMenu>(L, 2, LuaPopupMenuBind::meta);
    frame->set_popup_menu(popup_menu);
    return LuaHelper::chaining(L);
  }
};

typedef LuaFrameBind<LuaPopupFrame> LuaPopupFrameItemBind;

class LuaLexerBind {
 public:
  static std::string meta;
  static int open(lua_State *L) {
    static const luaL_Reg methods[] = {
      {"new", create},
      {"setkeywords", setkeywords},
      {"setcommentcolor", setcommentcolor},
      {"commentcolor", commentcolor},
      {"setcommentlinecolor", setcommentlinecolor},
      {"commentlinecolor", commentlinecolor},
      {"setcommentdoccolor", setcommentdoccolor},
      {"commentdoccolor", commentdoccolor},
      {"setnumbercolor", setnumbercolor},
      {"numbercolor", numbercolor},
      {"setwordcolor", setwordcolor},
      {"wordcolor", wordcolor},
      {"setstringcolor", setstringcolor},
      {"stringcolor", stringcolor},
      {"setoperatorcolor", setoperatorcolor},
      {"operatorcolor", operatorcolor},
      {"setcharactercodecolor", setcharactercodecolor},
      {"charactercodecolor", charactercodecolor},
      {"setpreprocessorcolor", setpreprocessorcolor},
      {"preprocessorcolor", preprocessorcolor},
      {"setidentifiercolor", setidentifiercolor},
      {"identifiercolor", identifiercolor},    
      {NULL, NULL}
    };
    LuaHelper::open(L, meta, methods,  gc);
    return 1;
  }
  static int create(lua_State* L) {
    LuaHelper::new_shared_userdata(L, meta, new ui::Lexer());
    return 1;
  }
  static int gc(lua_State* L) {
    return LuaHelper::delete_shared_userdata<ui::Lexer>(L, meta);
  }
  static int setkeywords(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_keywords); }  
  static int setcommentcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_comment_color); } 
  static int commentcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::comment_color); } 
  static int setcommentlinecolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_comment_line_color); } 
  static int commentlinecolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::comment_line_color); } 
  static int setcommentdoccolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_comment_doc_color); }   
  static int commentdoccolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::comment_doc_color); } 
  static int setnumbercolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_number_color); }
  static int numbercolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::number_color); }
  static int setwordcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_word_color); }
  static int wordcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::word_color); }
  static int setstringcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_string_color); }
  static int stringcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::string_color); }
  static int setoperatorcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_operator_color); }
  static int operatorcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::operator_color); }
  static int setcharactercodecolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_character_code_color); }
  static int charactercodecolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::character_code_color); }
  static int setpreprocessorcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_preprocessor_color); }
  static int preprocessorcolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::preprocessor_color); }
  static int setidentifiercolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::set_identifier_color); }
  static int identifiercolor(lua_State* L) { LUAEXPORTM(L, meta, &ui::Lexer::identifier_color); }
};

template <class T = LuaScintilla>
class LuaScintillaBind : public LuaWindowBind<T>, public LuaBackgroundColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaBackgroundColorMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {       
       {"f", f},       
       {"gotoline", gotoline},
       {"gotopos", gotopos},
       {"lineup", lineup},
       {"linedown", linedown},
       {"charleft", charleft},
       {"charright", charright},
       {"wordleft", wordleft},
       {"wordright", wordright},
       {"cut", cut},
       {"copy", copy},
       {"paste", paste},
       {"length", length},
       {"loadfile", loadfile},
       {"reload", reload},
       {"savefile", savefile},
       {"filename", filename},
       {"hasfile", hasfile},
       {"addtext", addtext},
       {"inserttext", inserttext},
       {"textrange", textrange},       
       {"deletetextrange", deletetextrange},
       {"clearall", clearall},
       {"findtext", findtext},
       {"addmarker", addmarker},
       {"deletemarker", deletemarker},
       {"definemarker", definemarker},
       {"selectionstart", selectionstart},
       {"selectionend", selectionend},
       {"setsel", setsel},
       {"hasselection", hasselection},
       {"replacesel", replacesel},
       {"setfindwholeword", setfindwholeword},
       {"setfindmatchcase", setfindmatchcase},
       {"setfindregexp", setfindregexp},
       {"setforegroundcolor", setforegroundcolor},
       {"foregroundcolor", foregroundcolor},          
       {"setlinenumberforegroundcolor", setlinenumberforegroundcolor},
       {"linenumberforegroundcolor", linenumberforegroundcolor},   
       {"setlinenumberbackgroundcolor", setlinenumberbackgroundcolor},
       {"linenumberbackgroundcolor", linenumberbackgroundcolor},       
       {"setfoldingbackgroundcolor", setfoldingbackgroundcolor},
       {"setfoldingmarkercolors", setfoldingmarkercolors},       
       {"setselforegroundcolor", setselforegroundcolor},
       {"setselbackgroundcolor", setselbackgroundcolor},
       {"setselalpha", setselalpha},
       {"setcaretcolor", setcaretcolor},
       {"caretcolor", caretcolor},
       {"setidentcolor", setidentcolor},
       {"styleclearall", styleclearall},
       {"setlexer", setlexer},
       {"setfontinfo", setfontinfo},
       {"line", line},
       {"currentpos", currentpos},
       {"column", column},
       {"overtype", overtype},
       {"modified", modified},
       {"showcaretline", showcaretline},
       {"hidecaretline", hidecaretline},
       {"setcaretlinebackgroundcolor", setcaretlinebackgroundcolor},
       {"settabwidth", settabwidth},
       {"tabwidth", tabwidth},
       {"undo", undo},
       {"redo", redo},
       {"hidelinenumbers", hidelinenumbers},
       {"hidebreakpoints", hidebreakpoints},
       {"hidehorscrollbar", hidehorscrollbar},
       {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  
  template <typename T1>
  static T1 win_param(lua_State* L, int idx) {  
    T1 result;  
    switch (lua_type(L, idx)) {
      case LUA_TSTRING:
        result = (T1)(lua_tostring(L, idx));
      break;
      case LUA_TBOOLEAN:
        result = (T1)(lua_toboolean(L, idx));
      break;
      case LUA_TNUMBER:
        result = (T1)(lua_tonumber(L, idx));
      break;
      default:
        result = 0;
        luaL_error(L, "Wrong argument type");
    }
    return result;
  }
    
  static int f(lua_State *L) {	  
    typename T::Ptr scintilla = LuaHelper::check_sptr<LuaScintilla>(L, 1, B::meta);    
#ifdef _WIN32
#else	  
    typedef unsigned int UINT_PTR;
    typedef UINT_PTR WPARAM;
    typedef UINT_PTR LPARAM;
#endif	  
    lua_pushinteger(L, scintilla->f(luaL_checkinteger(L, 2),
                                    (void*) win_param<WPARAM>(L, 3),
                                    (void*) win_param<LPARAM>(L, 4)));  
    return 1;
  }
  static int definemarker(lua_State *L) {
    boost::shared_ptr<T> item = LuaHelper::check_sptr<T>(L, 1, B::meta);
    int val1 = static_cast<int>(luaL_checkinteger(L, 2));
    int val2 = static_cast<int>(luaL_checkinteger(L, 3));
    int val3 = static_cast<int>(luaL_checkinteger(L, 4));
    int val4 = static_cast<int>(luaL_checkinteger(L, 5));
    item->define_marker(val1, val2, val3, val4);  
    return 0;
  }  
  static int showcaretline(lua_State *L) { LUAEXPORT(L, &T::ShowCaretLine); } 
  static int hidecaretline(lua_State *L) { LUAEXPORT(L, &T::HideCaretLine); }   
  static int hidelinenumbers(lua_State *L) { LUAEXPORT(L, &T::HideLineNumbers); }
  static int hidebreakpoints(lua_State *L) { LUAEXPORT(L, &T::HideBreakpoints); }  
  static int addmarker(lua_State *L) { LUAEXPORT(L, &T::add_marker); } 
  static int setcaretlinebackgroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_caret_line_background_color); } 
  static int settabwidth(lua_State *L) { LUAEXPORT(L, &T::set_tab_width); } 
  static int tabwidth(lua_State *L) { LUAEXPORT(L, &T::tab_width); }
  static int deletemarker(lua_State *L) { LUAEXPORT(L, &T::delete_marker); } 
  static int setsel(lua_State *L) { LUAEXPORT(L, &T::SetSel); } 
  static int selectionstart(lua_State *L) { LUAEXPORT(L, &T::selectionstart); } 
  static int selectionend(lua_State *L) { LUAEXPORT(L, &T::selectionend); } 
  static int hasselection(lua_State *L) { LUAEXPORT(L, &T::has_selection); } 
  static int replacesel(lua_State *L) { LUAEXPORT(L, &T::ReplaceSel); } 
  static int gotoline(lua_State *L) { LUAEXPORT(L, &T::GotoLine); }
  static int gotopos(lua_State *L) { LUAEXPORT(L, &T::GotoPos); }
  static int line(lua_State *L) { LUAEXPORT(L, &T::line); }
  static int currentpos(lua_State *L) { LUAEXPORT(L, &T::current_pos); }
  static int lineup(lua_State *L) { LUAEXPORT(L, &T::LineUp); }
  static int linedown(lua_State *L) { LUAEXPORT(L, &T::LineDown); }
  static int charleft(lua_State *L) { LUAEXPORT(L, &T::CharLeft); }
  static int charright(lua_State *L) { LUAEXPORT(L, &T::CharRight); }
  static int wordleft(lua_State *L) { LUAEXPORT(L, &T::WordLeft); }
  static int wordright(lua_State *L) { LUAEXPORT(L, &T::WordRight); }
  static int cut(lua_State *L) { LUAEXPORT(L, &T::Cut); }
  static int copy(lua_State *L) { LUAEXPORT(L, &T::Copy); }
  static int paste(lua_State *L) { LUAEXPORT(L, &T::Paste); }
  static int length(lua_State *L) { LUAEXPORT(L, &T::length); }  
  static int addtext(lua_State *L) { LUAEXPORT(L, &T::AddText); }
  static int inserttext(lua_State *L) { LUAEXPORT(L, &T::InsertText); }
  static int textrange(lua_State* L) {
    typename T::Ptr window = LuaHelper::check_sptr<T>(L, 1, B::meta); 
    double cpmin = luaL_checkinteger(L, 2);        
    double cpmax = luaL_checkinteger(L, 3);            
    lua_pushstring(L, window->text_range(cpmin, cpmax).c_str());
    return 1;
  }
  static int deletetextrange(lua_State *L) { LUAEXPORT(L, &T::delete_text_range); }
  static int findtext(lua_State *L) { LUAEXPORT(L, &T::FindText); }  
  static int clear(lua_State *L) { LUAEXPORT(L, &T::RemoveAll); }  
  static int loadfile(lua_State *L) { LUAEXPORT(L, &T::LoadFile); }
  static int reload(lua_State *L) { LUAEXPORT(L, &T::Reload); }
  static int savefile(lua_State *L) { LUAEXPORT(L, &T::SaveFile); }
  static int filename(lua_State *L) { LUAEXPORT(L, &T::filename); }  
  static int hasfile(lua_State *L) { LUAEXPORT(L, &T::has_file); }
  static int setfindmatchcase(lua_State *L) { LUAEXPORT(L, &T::set_find_match_case); }
  static int setfindwholeword(lua_State *L) { LUAEXPORT(L, &T::set_find_whole_word); }
  static int setfindregexp(lua_State *L) { LUAEXPORT(L, &T::set_find_regexp); }
  static int setforegroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_foreground_color); }
  static int foregroundcolor(lua_State *L) { LUAEXPORT(L, &T::foreground_color); }    
  static int setlinenumberforegroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_linenumber_foreground_color); }
  static int linenumberforegroundcolor(lua_State *L) { LUAEXPORT(L, &T::linenumber_foreground_color); }
  static int setlinenumberbackgroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_linenumber_background_color); }
  static int linenumberbackgroundcolor(lua_State *L) { LUAEXPORT(L, &T::linenumber_background_color); }  
  static int setfoldingbackgroundcolor(lua_State* L) { LUAEXPORT(L, &T::set_folding_background_color); }   
  static int setfoldingmarkercolors(lua_State* L) {      
    typename T::Ptr window = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    double fore = luaL_checknumber(L, 2);        
    double back = luaL_checknumber(L, 3);        
    window->set_folding_marker_colors(fore, back);
    return LuaHelper::chaining(L);
  }
  static int setselforegroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_sel_foreground_color); }
  //static int selforegroundcolor(lua_State *L) { LUAEXPORT(L, &T::sel_foreground_color); }
  static int setselbackgroundcolor(lua_State *L) { LUAEXPORT(L, &T::set_sel_background_color); }
  //static int selbackgroundcolor(lua_State *L) { LUAEXPORT(L, &T::sel_background_color); }
  static int setselalpha(lua_State *L) { LUAEXPORT(L, &T::set_sel_alpha); }
  static int setcaretcolor(lua_State *L) { LUAEXPORT(L, &T::set_caret_color); }
  static int caretcolor(lua_State *L) { LUAEXPORT(L, &T::caret_color); }
  static int setidentcolor(lua_State *L) { LUAEXPORT(L, &T::set_ident_color); }
  static int styleclearall(lua_State *L) { LUAEXPORT(L, &T::StyleClearAll); }
  static int hidehorscrollbar(lua_State *L) { LUAEXPORT(L, &T::HideHorScrollbar); }
  static int setlexer(lua_State *L) { 
    LuaHelper::bindud<T, ui::Lexer>(L, B::meta, LuaLexerBind::meta, &T::set_lexer); 
    return LuaHelper::chaining(L);
  }
  static int setfontinfo(lua_State *L) {
    using namespace ui;
    typename T::Ptr window = LuaHelper::check_sptr<T>(L, 1, B::meta);    
    FontInfo::Ptr font_info = LuaHelper::check_sptr<FontInfo>(L, 2, LuaFontInfoBind::meta);        
    window->set_font_info(*font_info.get());
    return LuaHelper::chaining(L);
  }  
  static int column(lua_State *L) { LUAEXPORT(L, &T::column); }
  static int overtype(lua_State *L) { LUAEXPORT(L, &T::over_type); }
  static int modified(lua_State *L) { LUAEXPORT(L, &T::modified); }
  static int clearall(lua_State *L) { LUAEXPORT(L, &T::ClearAll); }
  static int undo(lua_State *L) { LUAEXPORT(L, &T::Undo); } 
  static int redo(lua_State *L) { LUAEXPORT(L, &T::Redo); } 
};

template <class T = LuaEdit>
class LuaEditBind : public LuaWindowBind<T>, LuaTextMixIn<T>,  public LuaColorMixIn<T>, LuaFontMixIn<T>, public LuaBackgroundColorMixIn<T> {
 public:
  typedef LuaWindowBind<T> B;  
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    LuaTextMixIn<T>::setmethods(L);
    LuaFontMixIn<T>::setmethods(L);
    LuaColorMixIn<T>::setmethods(L);    
    LuaBackgroundColorMixIn<T>::setmethods(L);
    static const luaL_Reg methods[] = {
      {"setsel", setsel},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    return 0;
  }
  static int setsel(lua_State* L) { 
    typename T::Ptr edit = LuaHelper::check_sptr<T>(L, 1, T::type());
    int cp_min = luaL_checkinteger(L, 2) - 1;
    int cp_max = luaL_checkinteger(L, 3) - 1;
    edit->set_sel(cp_min, cp_max);
    return LuaHelper::chaining(L);
  }
};

template <class T = LuaScrollBar>
class LuaScrollBarBind : public LuaWindowBind<T> {
 public:
  typedef LuaWindowBind<T> B;
  static int open(lua_State *L) { return LuaHelper::openex(L, B::meta, setmethods, B::gc); }
  static int setmethods(lua_State* L) {
    B::setmethods(L);
    static const luaL_Reg methods[] = {
      {"new", create},
      {"setscrollposition", setscrollposition},
	  {"incscrollposition", incscrollposition},
	  {"decscrollposition", decscrollposition},
      {"scrollposition", scrollposition},
      {"setscrollrange", setscrollrange},
      {"scrollrange", scrollrange},
      {"systemsize", systemsize},
      {NULL, NULL}
    };
    luaL_setfuncs(L, methods, 0);
    const char* const e[] = {"HORZ", "VERT"};
    LuaHelper::buildenum(L, e, 2);
    return 0;
  }
  static int create(lua_State *L) {   
    int n = lua_gettop(L);  // Number of arguments  
    boost::shared_ptr<LuaGroup> group;
    if (n >= 2 && !lua_isnil(L, 2)) {
      group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaGroupBind<>::meta);
      if (!group) {
        group = LuaHelper::test_sptr<LuaGroup>(L, 2, LuaHeaderGroupBind<>::meta);
        if (!group) {
          group = LuaHelper::check_sptr<LuaGroup>(L, 2, LuaViewportBind<>::meta);
        }
      }
    }
    ui::Orientation::Type orientation = ui::Orientation::VERT;
    if (n == 3) {
      orientation = static_cast<ui::Orientation::Type>(luaL_checkinteger(L, 3));
    }
    boost::shared_ptr<T> item = LuaHelper::new_shared_userdata(L, B::meta, new T(L, orientation));
    if (group) {
      group->Add(item);
      LuaHelper::register_userdata(L, item.get());
    }
    LuaHelper::new_lua_module(L, "psycle.signal");
    lua_setfield(L, -2, "keydown");
    return 1;   
  }
  static int setscrollposition(lua_State* L) { LUAEXPORT(L, &T::set_scroll_position); }
  static int incscrollposition(lua_State* L) { LUAEXPORT(L, &T::inc_scroll_position); }
  static int decscrollposition(lua_State* L) { LUAEXPORT(L, &T::dec_scroll_position); }
  static int scrollposition(lua_State* L) { LUAEXPORT(L, &T::scroll_position); }
  static int setscrollrange(lua_State* L) { LUAEXPORT(L, &T::set_scroll_range); }
  static int scrollrange(lua_State* L) { LUAEXPORT(L, &T::scroll_range); }
  static int systemsize(lua_State* L) { LUAEXPORT(L, &T::system_size); }
};

template class LuaRectangleBoxBind<LuaRectangleBox>;
template class LuaLineBind<LuaLine>;
template class LuaTextBind<LuaText>;
template class LuaPicBind<LuaPic>;
template class LuaButtonBind<LuaButton>;
template class LuaRadioButtonBind<LuaRadioButton>;
template class LuaGroupBoxBind<LuaGroupBox>;
template class LuaEditBind<LuaEdit>;
template class LuaScopeBind<LuaScopeWindow>;
template class LuaScrollBarBind<LuaScrollBar>;
template class LuaScintillaBind<LuaScintilla>;
template class LuaComboBoxBind<LuaComboBox>;
template class LuaTreeViewBind<LuaTreeView>;
template class LuaListViewBind<LuaListView>;
template class LuaFrameBind<LuaFrame>;
template class LuaFrameBind<LuaPopupFrame>;
template class LuaScrollBoxBind<LuaScrollBox>;
template class LuaSplitterBind<LuaSplitter>;

static int lua_ui_requires(lua_State* L) {
  // ui binds  
  LuaHelper::require<LuaPointBind>(L, "psycle.ui.point");  
  LuaHelper::require<LuaDimensionBind>(L, "psycle.ui.dimension");
  LuaHelper::require<LuaUiRectBind>(L, "psycle.ui.rect");
  LuaHelper::require<LuaBoxSpaceBind>(L, "psycle.ui.boxspace");
  LuaHelper::require<LuaBorderRadiusBind>(L, "psycle.ui.borderradius");
  LuaHelper::require<LuaRegionBind>(L, "psycle.ui.region");
  LuaHelper::require<LuaAreaBind>(L, "psycle.ui.area");
  LuaHelper::require<LuaImageBind>(L, "psycle.ui.image");
  LuaHelper::require<LuaImagesBind>(L, "psycle.ui.images");
  LuaHelper::require<LuaGraphicsBind>(L, "psycle.ui.graphics");
  LuaHelper::require<LuaFontBind>(L, "psycle.ui.font");
  LuaHelper::require<LuaFontsBind>(L, "psycle.ui.fonts");
  LuaHelper::require<LuaFontInfoBind>(L, "psycle.ui.fontinfo");  
  LuaHelper::require<LuaColorHelper>(L, "psycle.ui.colorhelper");
  LuaHelper::require<LuaGameControllersBind>(L, "psycle.ui.gamecontrollers");
  LuaHelper::require<LuaGameControllerBind>(L, "psycle.ui.gamecontroller");
  // filedialog
  LuaHelper::require<LuaFileOpenBind>(L, "psycle.ui.fileopen");
  LuaHelper::require<LuaFileSaveBind>(L, "psycle.ui.filesave");
  // ui menu binds
  LuaHelper::require<LuaPopupMenuBind>(L, "psycle.ui.popupmenu");
  LuaHelper::require<LuaSystemMetrics>(L, "psycle.ui.systemmetrics");
  LuaHelper::require<LuaViewportBind<> >(L, "psycle.ui.viewport");
  LuaHelper::require<LuaFrameBind<> >(L, "psycle.ui.frame");
  LuaHelper::require<LuaPopupFrameItemBind >(L, "psycle.ui.popupframe");
  LuaHelper::require<LuaFrameAlignerBind>(L, "psycle.ui.framealigner");
  LuaHelper::require<LuaAlignerBind>(L, "psycle.ui.aligner");
  LuaHelper::require<LuaGroupBind<> >(L, "psycle.ui.group");
  LuaHelper::require<LuaHeaderGroupBind<> >(L, "psycle.ui.headergroup");
  LuaHelper::require<LuaScrollBoxBind<> >(L, "psycle.ui.scrollbox");
  LuaHelper::require<LuaWindowBind<> >(L, "psycle.ui.window");
  LuaHelper::require<LuaLineBind<> >(L, "psycle.ui.line");
  LuaHelper::require<LuaPicBind<> >(L, "psycle.ui.pic");  
  LuaHelper::require<LuaRectangleBoxBind<> >(L, "psycle.ui.rectanglebox");
  LuaHelper::require<LuaTextBind<> >(L, "psycle.ui.text");
  LuaHelper::require<LuaButtonBind<> >(L, "psycle.ui.button");
  LuaHelper::require<LuaSplitterBind<> >(L, "psycle.ui.splitter");
  LuaHelper::require<LuaTreeViewBind<> >(L, "psycle.ui.treeview");
  LuaHelper::require<LuaListViewBind<> >(L, "psycle.ui.listview");
  LuaHelper::require<LuaNodeBind>(L, "psycle.node");    
  LuaHelper::require<LuaRadioButtonBind<> >(L, "psycle.ui.radiobutton");
  LuaHelper::require<LuaGroupBoxBind<> >(L, "psycle.ui.groupbox");
  LuaHelper::require<LuaComboBoxBind<> >(L, "psycle.ui.combobox");
  LuaHelper::require<LuaEditBind<> >(L, "psycle.ui.edit");
  LuaHelper::require<LuaScopeBind<> >(L, "psycle.ui.scope");
  LuaHelper::require<LuaLexerBind>(L, "psycle.ui.lexer");
  LuaHelper::require<LuaScintillaBind<> >(L, "psycle.ui.scintilla");
  LuaHelper::require<LuaScrollBarBind<> >(L, "psycle.ui.scrollbar");
  LuaHelper::require<LuaEventBind>(L, "psycle.ui.event");
  LuaHelper::require<LuaKeyEventBind>(L, "psycle.ui.keyevent");
  LuaHelper::require<LuaMouseEventBind>(L, "psycle.ui.mouseevent");
  LuaHelper::require<LuaMouseEventBind>(L, "psycle.ui.wheelevent");
  LuaHelper::require<LuaOrnamentBind>(L, "psycle.ui.ornament");
  LuaHelper::require<OrnamentFactoryBind>(L, "psycle.ui.ornamentfactory");
  LuaHelper::require<LineBorderBind>(L, "psycle.ui.lineborder");
  LuaHelper::require<WallpaperBind>(L, "psycle.ui.wallpaper");
  LuaHelper::require<FillBind>(L, "psycle.ui.fill");  
  LuaHelper::require<CircleFillBind>(L, "psycle.ui.circlefill");
  return 0;
}

} // namespace host
} // namespace psycle