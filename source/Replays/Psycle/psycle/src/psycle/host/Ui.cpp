// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net
// #include "stdafx.h"

#include "Ui.hpp"
#include "LockIF.hpp"
#ifdef __linux__ 
  #include "XUi.hpp"
#elif _WIN32
   #include "MfcUi.hpp"
#else
  // platform not supported
#endif
#include "CanvasItems.hpp"
#include <limits>
#include <stdio.h>

namespace psycle {
namespace host {
namespace ui {

const ui::Point ui::Point::zero_;
const ui::Rect ui::Rect::zero_;
int MenuContainer::id_counter = 0;
Font Font::dummy_font(0);
FontInfo FontInfo::dummy_font_info(0);

MenuContainer::MenuContainer() : imp_(ImpFactory::instance().CreateMenuContainerImp()) {
  imp_->set_menu_bar(this);
}

MenuContainer::MenuContainer(MenuContainerImp* imp) : imp_(imp) {
  imp_->set_menu_bar(this);
}

void MenuContainer::Update() {
  if (!root_node_.expired()) {
    assert(imp_.get());		
    imp_->DevUpdate(root_node_.lock());  
  }
}

void MenuContainer::Invalidate() {
  assert(imp_.get());
  imp_->DevInvalidate();
}

PopupMenu::PopupMenu() 
  : MenuContainer(ImpFactory::instance().CreatePopupMenuImp()) {
}

void PopupMenu::Track(const Point& pos) {
  assert(imp());
  imp()->DevTrack(pos);
}

Region::Region() : imp_(ImpFactory::instance().CreateRegionImp()) {
}

Region::Region(const Rect& rect) : imp_(ImpFactory::instance().CreateRegionImp()) {
  imp_->DevSetRect(rect);
}

Region::Region(RegionImp* imp) : imp_(imp) {
}

Region& Region::operator = (const Region& other) {
  imp_.reset(other.imp()->DevClone());
  return *this;
}

Region::Region(const Region& other) {
  imp_.reset(other.imp()->DevClone());
}

Region* Region::Clone() const {
  Region* region = 0;
  if (imp_.get()) {
    region = new Region(imp_->DevClone());
  }
  return region;
}

void Region::Offset(double dx, double dy) {
  if (imp_.get()) {
    imp_.get()->DevOffset(dx, dy);
  }
}

void Region::Union(const Region& other) {  
  if (imp_.get()) {
    imp_.get()->DevUnion(other);
  }  
}

ui::Rect Region::bounds() const {
  ui::Rect result;
  if (imp_.get()) {
    result = imp_.get()->DevBounds();
  }
  return result;
}

bool Region::Intersect(double x, double y) const {
  bool result = false;
  if (imp_.get()) {
    result = imp_.get()->DevIntersect(x, y);
  }
  return result;
}

bool Region::IntersectRect(const ui::Rect& rect) const {
  bool result = false;
  if (imp_.get()) {
    result = imp_.get()->DevIntersectRect(rect);
  }
  return result;
}

void Region::Clear() {  
  if (imp_.get()) {
    imp_.get()->DevClear();
  }  
}

void Region::SetRect(const ui::Rect& rect) {
  if (imp_.get()) {
    imp_.get()->DevSetRect(rect);
  }
}

std::auto_ptr<ui::Region> Rect::region() const {
  std::auto_ptr<ui::Region> result(ui::Systems::instance().CreateRegion());
  result->SetRect(*this);
  return result;  
}

Area::Area() : needs_update_(true) {}

Area::Area(const Rect& rect) : needs_update_(true) {
  Add(RectShape(rect));
}

Area::~Area() {}

Area* Area::Clone() const {
  Area* area = new Area();  
  area->rect_shapes_.insert(area->rect_shapes_.end(), rect_shapes_.begin(),
                            rect_shapes_.end());
  return area;
}

const Region& Area::region() const {     
  Update();  
  return region_cache_;
}

const ui::Rect& Area::bounds() const {    
  Update();  
  return bound_cache_;
}

void Area::Offset(double dx, double dy) {  
  rect_iterator i = rect_shapes_.begin();
  for (; i != rect_shapes_.end(); ++i) {
    i->offset(dx, dy);
  }
  needs_update_ = true;  
}

void Area::Clear() {  
  rect_shapes_.clear();
  region_cache_.Clear();
  bound_cache_.reset();
  needs_update_ = false;
}

bool Area::Intersect(double x, double y) const {  
  Update();
  return region_cache_.Intersect(x, y);
}

void Area::Add(const RectShape& rect) {  
  rect_shapes_.push_back(rect);
  needs_update_ = true;
}

int Area::Combine(const Area& other, int combinemode) {  
  // if (combinemode == RGN_OR) {    
  rect_shapes_.insert(rect_shapes_.end(), other.rect_shapes_.begin(),
       other.rect_shapes_.end());
  needs_update_ = true;
#ifdef __linux__ 
  return 1;
#elif defined _WIN32
  return COMPLEXREGION;
#else
    return 1;
#endif
  //}	
}

void Area::ComputeRegion() const {
  region_cache_.Clear();
  rect_const_iterator i = rect_shapes_.begin();
  for (; i != rect_shapes_.end(); ++i) {    
     region_cache_.Union(ui::Region((*i).bounds()));
  }
}

void Area::ComputeBounds() const {
  if (rect_shapes_.empty()) {
    bound_cache_.set(ui::Point(), ui::Point());
  } else {
    bound_cache_.set(ui::Point((std::numeric_limits<double>::max)(),
								(std::numeric_limits<double>::max)()),
                      ui::Point((std::numeric_limits<double>::min)(),
		                        (std::numeric_limits<double>::min)()));
    rect_const_iterator i = rect_shapes_.begin();
    for (; i != rect_shapes_.end(); ++i) {
      ui::Rect bounds = (*i).bounds();
      if (bounds.left() < bound_cache_.left()) {
        bound_cache_.set_left(bounds.left());
      }
      if (bounds.top() < bound_cache_.top()) {
        bound_cache_.set_top(bounds.top());
      }
      if (bounds.right() > bound_cache_.right()) {
        bound_cache_.set_right(bounds.right());
      }
      if (bounds.bottom() > bound_cache_.bottom()) {
        bound_cache_.set_bottom(bounds.bottom());
      }
    }
  }  
}

Commands::Commands() : invalid_(false), addcount(0) {}
void Commands::Clear() {
  locker_.lock();
  functors.clear();
  locker_.unlock();
}

void Commands::Invoke() {       
  locker_.lock();
  GlobalTimer::instance().KillTimer();
  std::list<boost::function<bool(void)> >::iterator it = functors.begin();
  while (it != functors.end()) { 
    if ((*it)()) {
      functors.erase(it++);
    } else {
      ++it;
    }
  }
  GlobalTimer::instance().StartTimer();
  locker_.unlock();
}

FontInfo::FontInfo() : imp_(ui::ImpFactory::instance().CreateFontInfoImp()) {
}

FontInfo::FontInfo(const std::string& family) :
    imp_(ui::ImpFactory::instance().CreateFontInfoImp()) {
  set_family(family);  
}

FontInfo::FontInfo(const std::string& family, int size) :
    imp_(ui::ImpFactory::instance().CreateFontInfoImp()) {
  set_family(family); 
  set_size(size); 
}

FontInfo::FontInfo(const std::string& family, int size, int weight, FontStyle::Type style) :
    imp_(ui::ImpFactory::instance().CreateFontInfoImp()) {
  set_family(family);
  set_size(size);
  set_weight(weight);
  set_style(style);
}

FontInfo::FontInfo(const FontInfo& other) {    
  assert(other.imp());
  imp_.reset(other.imp()->DevClone());  
}

FontInfo::FontInfo(FontInfoImp* imp) : imp_(imp) {
}

FontInfo& FontInfo::operator=(const FontInfo& other) {
  assert(other.imp());
  if (this != &other) {
    imp_.reset(other.imp()->DevClone());
  }
  return *this;
}

void FontInfo::set_style(FontStyle::Type style) {
  assert(imp());  
  imp()->dev_set_style(style);
}

FontStyle::Type FontInfo::style() const{
  assert(imp());
  return imp()->dev_style();
}

void FontInfo::set_size(int size) {
  assert(imp());  
  imp()->dev_set_size(size);
}

int FontInfo::size() const {
  assert(imp());
  return imp()->dev_size();
}

void FontInfo::set_weight(int weight) {
  assert(imp());  
  imp()->dev_set_weight(weight);
}

int FontInfo::weight() const {
  assert(imp());
  return imp()->dev_weight();
}

void FontInfo::set_family(const std::string& family) {
  assert(imp());  
  imp()->dev_set_family(family);
}

std::string FontInfo::family() const {
  assert(imp());
  return imp()->dev_family();
}

void FontInfo::set_stock_id(int id) {
  assert(imp());
  return imp()->dev_set_stock_id(id);
}

int FontInfo::stock_id() const {
  assert(imp());
  return imp()->dev_stock_id();
}

Font::Font()  : imp_(ui::ImpFactory::instance().CreateFontImp()) {
}

Font::Font(const ui::FontInfo& font_info)  {
  if (font_info.stock_id() == -1) {
    imp_.reset(ui::ImpFactory::instance().CreateFontImp());
    imp_->dev_set_font_info(font_info);
  } else {
    imp_.reset(ui::ImpFactory::instance().CreateFontImp(font_info.stock_id()));
  }  
}

Font::Font(const Font& other) {    
  assert(other.imp());
  imp_.reset(other.imp()->DevClone());  
}

Font::Font(FontImp* imp) : imp_(imp) {
}

Font& Font::operator= (const Font& other) {
  assert(other.imp());
  if (this != &other) {
    imp_.reset(other.imp()->DevClone());
  }
  return *this;
}

void Font::set_font_info(const FontInfo& info) {
  assert(imp());  
  imp()->dev_set_font_info(info);
}

FontInfo Font::font_info() const {
  assert(imp());
  return imp()->dev_font_info();
}

Fonts::Fonts() : imp_(ui::ImpFactory::instance().CreateFontsImp()) {
  font_list_ = imp_->dev_font_list();
}

void Fonts::import_font(const std::string& path) {
  assert(imp_.get());
  imp_->dev_import_font(path);
}

//Image
Image::Image() 
	: imp_(ui::ImpFactory::instance().CreateImageImp()), 
    has_file_(false) {
}

Image::Image(const Image& other)
    : imp_(other.imp()->DevClone()), has_file_(other.has_file_) {
}

Image::~Image() {}

Image& Image::operator=(const Image& other){
	if (this != &other) {
		imp_.reset(other.imp()->DevClone());
		has_file_ = other.has_file_;
	}
	return *this;
}

void Image::Reset(const ui::Dimension& dimension) {
  assert(imp_.get());
  imp_->DevReset(dimension);	
}

void Image::Load(const std::string& filename) {
  assert(imp_.get());
  imp_->DevLoad(filename);
  filename_ = filename;
}

void Image::Save(const std::string& filename) {
  assert(imp_.get());
  imp_->DevSave(filename);
	filename_ = filename;
}

void Image::Save() {
  assert(imp_.get());
  imp_->DevSave(filename_);
}

void Image::SetTransparent(bool on, ARGB color) {
  assert(imp_.get());
  imp_->DevSetTransparent(on, color);
}

ui::Dimension Image::dim() const {
  assert(imp_.get());
  return imp_->dev_dim();
}

std::auto_ptr<ui::Graphics> Image::graphics() {
#ifdef __linux__ 
  std::auto_ptr<ui::Graphics> paint_graphics;
  // not implemented yet;
  assert(0);
#elif _WIN32
  assert(imp_.get());		
  CDC* memDC = new CDC();
  memDC->CreateCompatibleDC(NULL);
  std::auto_ptr<ui::Graphics> paint_graphics(new ui::Graphics(memDC));
  paint_graphics->AttachImage(this);
#else
  // platform not supported
#endif	  	
  return paint_graphics;
}

void Image::Cut(const ui::Rect& bounds) {
	assert(imp_.get());
	return imp_->DevCut(bounds);
}

void Image::SetPixel(const ui::Point& pt, ARGB color) {
  assert(imp_.get());
  imp_->DevSetPixel(pt, color);
}

ARGB Image::GetPixel(const ui::Point & pt) const {
  assert(imp_.get());
  return imp_->DevGetPixel(pt);	
}

void Image::Resize(const ui::Dimension& dimension) {
	assert(imp_.get());
	imp_->DevResize(dimension);
}

void Image::Rotate(float radians) {
	assert(imp_.get());
	imp_->DevRotate(radians);
}

// Graphics
Graphics::Graphics() : imp_(ui::ImpFactory::instance().CreateGraphicsImp()) {
}

/*Graphics::Graphics(bool debug) : imp_(ui::ImpFactory::instance().CreateGraphicsImp(debug)) {
}*/

Graphics::Graphics(GraphicsImp* imp)  : imp_(imp) {
}

Graphics::Graphics(const Image::Ptr& image) : imp_(ui::ImpFactory::instance().CreateGraphicsImp(image)) {
  
}

#ifdef _WIN32
Graphics::Graphics(CDC* cr) 
	: imp_(ui::ImpFactory::instance().CreateGraphicsImp(cr)) {
}
#endif

void Graphics::CopyArea(const ui::Rect& rect, const ui::Point& delta) {
  assert(imp_.get());
  imp_->DevCopyArea(rect, delta);  
}

void Graphics::DrawArc(const ui::Rect& rect, const Point& start, const Point& end) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawArc(rect, start, end);  
  }
}

void Graphics::DrawLine(const ui::Point& p1, const ui::Point& p2) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevDrawLine(p1, p2);  
  }
}

void Graphics::DrawCurve(const Point& p1, const Point& control_p1, const Point& control_p2, const Point& p2) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevDrawCurve(p1, control_p1, control_p2, p2);  
  }
}

void Graphics::DrawRect(const ui::Rect& rect) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawRect(rect);  
  }
}

void Graphics::DrawRoundRect(const Rect& rect,
     const Dimension& arc_dimension) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawRoundRect(rect, arc_dimension);  
  }
}

void Graphics::DrawOval(const ui::Rect& rect) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawOval(rect); 
  }
}

void Graphics::DrawCircle(const Point& center, double radius) {
  if (is_color_opaque()) {
    assert(imp_.get());    
    imp_->DevDrawOval(Rect(center - Point(radius, radius), center + Point(radius, radius))); 
  }
}

void Graphics::FillCircle(const Point& center, double radius) {
  if (is_color_opaque()) {
    assert(imp_.get());    
    imp_->DevFillOval(Rect(center - Point(radius, radius), center + Point(radius, radius))); 
  }
}

void Graphics::DrawEllipse(const Point& center, const Point& radius) {
  if (is_color_opaque()) {
    assert(imp_.get());    
    imp_->DevDrawOval(Rect(center - radius, center + radius));
  }
}

void Graphics::FillEllipse(const Point& center, const Point& radius) {
  if (is_color_opaque()) {
    assert(imp_.get());    
    imp_->DevFillOval(Rect(center - radius, center + radius));
  }
}

void Graphics::DrawString(const std::string& str, const Point& position) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawString(str, position);
  }
}

void Graphics::FillRect(const Rect& position) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevFillRect(position);
  }
}

void Graphics::FillRoundRect(const Rect& position,
                             const Dimension& arc_dimension) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevFillRoundRect(position, arc_dimension);
  }
}

void Graphics::FillOval(const Rect& position) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevFillOval(position);
  }
}

void Graphics::FillRegion(const Region& rgn) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevFillRegion(rgn);
  }
}

void Graphics::set_color(ARGB color) {  
  assert(imp_.get());
  imp_->DevSetColor(color);  
}

void Graphics::set_fill(ARGB color) {  
  assert(imp_.get());
  imp_->DevSetFill(color);  
}

void Graphics::set_stroke(ARGB color) {  
  assert(imp_.get());
  imp_->DevSetStroke(color);  
}

ARGB Graphics::color() const {
  return imp_.get()  ? imp_->dev_color() : 0;
}

void Graphics::SetPenWidth(double width) {
	assert(imp_.get()); 
  imp_->DevSetPenWidth(width);
}

void Graphics::Translate(const Point& delta) {
  assert(imp_.get()); 
  imp_->DevTranslate(delta);
  translations_.push(delta);
}

void Graphics::Retranslate() {
  if (translations_.size() > 0) {
	  ui::Point& top = translations_.top();
	  assert(imp_.get()); 
	  imp_->DevTranslate(ui::Point(-top.x(), -top.y()));
	  translations_.pop();
  }
}

void Graphics::ClearTranslations() {
  while (translations_.size() > 0) {
    Retranslate();
  }
}

void Graphics::SetFont(const Font& font) {
  assert(imp_.get());
  imp_->DevSetFont(font);  
}

const Font& Graphics::font() const {
  assert(imp_.get());
  return imp_->dev_font();
}

Dimension Graphics::text_dimension(const std::string& text) const {
  assert(imp_.get());
  return imp_->dev_text_dimension(text);  
}

void Graphics::DrawPolygon(const Points& points) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawPolygon(points);  
  }
}

void Graphics::FillPolygon(const Points& points) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawPolygon(points);
  }
}

void Graphics::DrawPolyline(const Points& points) {
  if (is_color_opaque()) {
    assert(imp_.get());
    imp_->DevDrawPolyline(points);
  }
}

void Graphics::DrawImage(Image* img, const Point& top_left) {
  assert(imp_.get());
  imp_->DevDrawImage(img, top_left);  
}

void Graphics::DrawImage(Image* img, const Rect& position) {
  assert(imp_.get());
  imp_->DevDrawImage(img, position);  
}

void Graphics::set_debug_flag() {
  imp_->DevSetDebugFlag();
}

void Graphics::DrawImage(Image* img, const Rect& destination_position,
		                     const Point& src) {
  assert(imp_.get());
  imp_->DevDrawImage(img, destination_position, src);
}

void Graphics::DrawImage(Image* img, const Rect& destination_position,
		                     const Point& src, const Point& zoom_factor) {
  assert(imp_.get());
  imp_->DevDrawImage(img, destination_position, src, zoom_factor);
}

void Graphics::SetClip(const Rect& rect) {
  assert(imp_.get());
  imp_->DevSetClip(rect);  
}

void Graphics::SetClip(Region* rgn) {
  assert(imp_.get());
  imp_->DevSetClip(rgn);  
}

void Graphics::BeginPath() {
  assert(imp_.get());
  imp_->DevBeginPath();  
}

void Graphics::EndPath() {
  assert(imp_.get());
  imp_->DevEndPath();  
}

void Graphics::FillPath() {
  assert(imp_.get());
  imp_->DevFillPath();  
}

void Graphics::DrawPath() {
  assert(imp_.get());
  imp_->DevDrawPath();  
}

void Graphics::DrawFillPath() {
  assert(imp_.get());
  imp_->DevDrawFillPath();  
}

void Graphics::MoveTo(const ui::Point& p) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevMoveTo(p);  
  }
}

void Graphics::CurveTo(const Point& control_p1, const Point& control_p2, const Point& p) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevCurveTo(control_p1, control_p2, p);  
  }
}

void Graphics::ArcTo(const Rect& rect, const Point& start, const Point& end) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevArcTo(rect, start, end);  
  }
}

void Graphics::LineTo(const ui::Point& p) {
  if (is_color_opaque()) {
    assert(imp_.get());  
    imp_->DevLineTo(p);  
  }
}

void Graphics::Dispose() {
  assert(imp_.get());
  imp_->DevDispose();  
}

void Graphics::AttachImage(Image* image) {
	assert(imp_.get());
  return imp_->DevAttachImage(image);	
}

/*
CDC* Graphics::dc() {
  assert(imp_.get());
  return imp_->dev_dc();
}*/

void Graphics::SaveOrigin() {
  assert(imp_.get());
  imp_->DevSaveOrigin();
}

void Graphics::RestoreOrigin() {
  assert(imp_.get());
  imp_->DevRestoreOrigin();
}

InheritedProperties::InheritedProperties() {
  inherited_["color"] = true;
  inherited_["font_family"] = true;
  inherited_["font_size"] = true;
  inherited_["font_style"] = true;
  inherited_["font_weight"] = true;
}

bool InheritedProperties::is_inherited(const std::string& name) const {
  bool result(false);
  std::map<std::string, bool>::const_iterator it = inherited_.find(name);
  if (it != inherited_.end()) {
      return it->second;
  }
  return result;
}

//Rules
void Rules::ApplyTo(Window* window) {
#ifdef _WIN32
   // linux outcommented because gcc errors here
  struct {    
    RuleContainer* rules;
    bool operator()(Window& window) const {
      RuleContainer::iterator it = rules->find(window.GetType());
      if (it != rules->end()) {
        Rule& rule = it->second;       
        window.set_properties(rule.properties());        
      } else {
        RuleContainer::iterator it = rules->find("inherit");
        if (it != rules->end()) {
          window.set_properties(it->second.properties());
        }
      }    
      return false;
    }
  } recursive_apply;   
  Rules& rules = VirtualRules();
  recursive_apply.rules = &rules.rules_;
  struct { 
    bool operator()(Window& window) const { return false; }    
  } no_abort;  
  window->PreOrderTreeTraverse(recursive_apply, no_abort);
#endif  
}

void Rule::InheritFrom(const Rule& rule) {
  Properties::Container::const_iterator it = rule.properties().elements().begin();
  for ( ; it != rule.properties().elements().end(); ++it) {    
     Properties::Container::const_iterator result = properties_.elements().find(it->first);
     if (result == properties_.elements().end()) {
       properties_.set(it->first, it->second);
     }
  }
}

void Rule::merge(const Rule& rule) {
  Properties::Container::const_iterator it = rule.properties().elements().begin();
  for ( ; it != rule.properties().elements().end(); ++it) {
    if (InheritedProperties::instance().is_inherited(it->first)) {
      Properties::Container::const_iterator result = properties_.elements().find(it->first);
      if (result == properties_.elements().end()) {
        properties_.set(it->first, it->second);
      }
    }
  }
}

Rules& Rules::VirtualRules() {  
  if (virtual_rules_cache_.get() == 0) {
    Rules* result = new Rules();
    Window* window = owner_;
    std::list<Rule*> rules;
    while (window) {
      RuleContainer::iterator it = window->rules_.begin();
      for ( ; it != window->rules_.end(); ++it) {
        Rule& rule = it->second;
        rules.push_front(&rule);
      }    
      window = window->parent();
    }  
    Rule inherit("inherit");
    std::list<Rule*>::iterator it = rules.begin();
    for ( ; it != rules.end(); ++it) {
      Rule* rule = *it;
      if (rule->selector() == "group") {
        inherit.merge(*rule);
      }
      rule->merge(inherit);    
    }
    it = rules.begin();
    for ( ; it != rules.end(); ++it) {
      Rule* rule = *it;
      rule->merge(inherit);
      result->add(*rule);		
    }
    result->add(inherit);
    virtual_rules_cache_.reset(result);	  
  }  
  assert(virtual_rules_cache_.get());
  return *virtual_rules_cache_.get();
}

void Rules::InheritFrom(const Rule& parent_rule) {
  RuleContainer::iterator it = rules_.begin();
  for ( ; it != rules_.end(); ++it) {
    it->second.InheritFrom(parent_rule);
  }   
}

// Window
Window::Window() :   
    parent_(0),
    update_(true),
    area_(new Area()),
    auto_size_width_(true),
    auto_size_height_(true),
    visible_(true),
    align_(AlignStyle::ALNONE),
    pointer_events_(true),
		prevent_auto_dimension_(false),
		align_dimension_changed_(true) {
  set_imp(ui::ImpFactory::instance().CreateWindowImp());
}

Window::Window(WindowImp* imp) : 
    parent_(0),
    update_(true),
    area_(new Area()),
    fls_area_(new Area()),
    auto_size_width_(true),
    auto_size_height_(true),
    visible_(true),
    align_(AlignStyle::ALNONE),
    pointer_events_(true),
		prevent_auto_dimension_(false),
		align_dimension_changed_(true) {
  imp_.reset(imp);  
  if (imp) {
    imp->set_window(this);
  }
}

Window::~Window() {
  BeforeDestruction(*this);
  for (iterator it = begin(); it != end(); ++it) {
    (*it)->parent_ = 0;
  }

  if (ImpFactory::instance().DestroyWindowImp(imp_.get())) {              
    imp_.release();         
  }  
}

void Window::set_imp(WindowImp* imp) { 
  imp_.reset(imp);
  if (imp) {
    imp->set_window(this);
  }
}

Window* Window::root() {   
  if (is_root()) {
    return this;
  }
  if (parent()) {    
    Window* window = this;
    do {
     if (window->is_root()) {    
       return window;
     }
     window = window->parent();
   } while (window);            
  }
  return 0;
}

const Window* Window::root() const {  
  if (is_root()) {
    return this;
  }
  if (parent()) {    
    const Window* window = this;
    do {
     if (window->is_root()) {    
       return window;
     }
     window = window->parent();
   } while (window);            
  }
  return 0;
}

bool Window::IsInGroup(Window::WeakPtr group) const {  
  const Window* p = parent();
  while (p) {
    if (group.lock().get() == p) {
      return true;
    }
    p = p->parent();
  }
  return false;
}

void Window::FLSEX() {  
#ifdef _WIN32	
  ui::Window* root_window = root();
  if (root_window && visible() && imp()) {    
		ui::Rect fls_pos = imp()->dev_absolute_system_position();
    std::auto_ptr<Area> tmp(new Area(fls_pos));		    
    if (fls_area_.get()) {    
      fls_area_->Combine(*tmp, RGN_OR);
      root_window->Invalidate(fls_area_->region());  
    } else {
      root_window->Invalidate(tmp->region());  
    }  
    fls_area_ = tmp;  
  }
#endif	
}

void Window::FLS() {
  if (visible() && imp()) {                    
    Window* root_window = root();    
    if (!fls_prevented()) {
      ui::Rect pos = imp()->dev_absolute_system_position();        
      fls_area_.reset(new Area(pos));   
      if (root_window && root_window->IsSaving()) {
        root_window->Invalidate(fls_area_->region());
      } else {      
        ui::Window* non_transparent_window = FirstNonTransparentWindow();
        assert(non_transparent_window->imp());
        ui::Rect non_trans_pos = non_transparent_window->imp()->dev_absolute_system_position();
        Area redraw_rgn(ui::Rect(pos.top_left() - non_trans_pos.top_left(), pos.dimension()));        
        non_transparent_window->Invalidate(redraw_rgn.region());            
      }
    }
  }	
}

bool Window::IsInGroupVisible() const {
  bool res = visible();
  const Window* p = parent();
  while (p) {
    res = p->visible();
    if (!res) {
      break;   
    }
    p = p->parent();
  }
  return res;
}

void Window::add_ornament(boost::shared_ptr<Ornament> ornament) {  
  ornaments_.push_back(ornament);
	if (imp_.get()) {
		imp_->dev_set_border_space(sum_border_space());
	}
  FLSEX();
}

Window::Ornaments Window::ornaments() {
  return ornaments_;
}

void Window::RemoveOrnaments() {
	ornaments_.clear();
	if (imp_.get()) {
		imp_->dev_set_border_space(ui::BoxSpace());
	}
	FLSEX();
}

const Area& Window::area() const {	
	area_->Clear();
	area_->Add(ui::Rect(ui::Point::zero(), dim()));
  return *area_.get();
}  

void Window::set_position(const ui::Point& pos) {  
  set_position(ui::Rect(pos, dim()));
}

void Window::set_position(const ui::Rect& pos) {  
  ui::Rect new_pos = pos;
  bool has_auto_dimension = (!prevent_auto_dimension_) && ((auto_size_width() || auto_size_height()));
  if (imp_.get()) {
    if (has_auto_dimension) {
       ui::Dimension auto_dimension = OnCalcAutoDimension();
       if (auto_size_width()) {
	 new_pos.set_width(auto_dimension.width());
       }
       if (auto_size_height()) {
	 new_pos.set_height(auto_dimension.height());
       }
    }
   // bool size_changed = imp_->dev_position().dimension() != new_pos.dimension();
    imp_->dev_set_position(new_pos);
    //if (size_changed) {			
     // OnSize(new_pos.dimension());
    //}
    FLSEX();    
    align_dimension_changed();    		
    if (!prevent_auto_dimension_) {
     // WorkChildposition();
    }
  }
}

void Window::ScrollTo(int offsetx, int offsety) {
  if (imp()) {
    imp()->DevScrollTo(offsetx, offsety);
  }
}

ui::Rect Window::position() const { 
  return imp() ? imp_->dev_position() : Rect();
}

ui::Rect Window::absolute_position() const {
  return imp() ? Rect(imp_->dev_absolute_position().top_left(), dim()) : Rect();
}

ui::Rect Window::desktop_position() const {
  return imp() ? Rect(imp_->dev_desktop_position().top_left(), dim()) : Rect();
}

ui::Dimension Window::dim() const {  
  return (imp_.get()) ? imp_->dev_position().dimension() : ui::Dimension();
}

bool Window::check_position(const ui::Rect& pos) const {
  return (imp_.get()) ? imp_->dev_check_position(pos) : false;
}

void Window::UpdateAutoDimension() {
  ui::Dimension new_dimension = position().dimension();
  bool has_auto_dimension = auto_size_width() || auto_size_height();
  if (has_auto_dimension && imp_.get()) {    				
    ui::Dimension auto_dimension = OnCalcAutoDimension();
    if (auto_size_width()) {
       new_dimension.set_width(auto_dimension.width());
    }
    if (auto_size_height()) {
       new_dimension.set_height(auto_dimension.height());      
    }  
    if (overall_position() != overall_position(ui::Rect(position().top_left(), new_dimension))) {
      imp_->dev_set_position(ui::Rect(position().top_left(), new_dimension));
      OnSize(new_dimension); 	
      WorkChildPosition();	  
      FLSEX();    
    }
  }
}

void Window::set_margin(const BoxSpace& margin) {
  if (imp_.get()) {
    imp_->dev_set_margin(margin);
  }
}

const BoxSpace& Window::margin() const {	
   return imp() ? imp_->dev_margin() : BoxSpace::zero();
}

void Window::set_padding(const ui::BoxSpace& padding) {
  if (imp_.get()) {
    imp_->dev_set_padding(padding);
  }	
}

const BoxSpace& Window::padding() const {
   return imp() ? imp_->dev_padding() : BoxSpace::zero();
}

void Window::set_border_space(const ui::BoxSpace& border_space) {
  if (imp_.get()) {
    imp_->dev_set_border_space(border_space);
  }
}

const BoxSpace& Window::border_space() const {
  return imp() ? imp_->dev_border_space() : BoxSpace::zero();
}

bool Window::auto_size_width() const { 
  return auto_size_width_;
}

bool Window::auto_size_height() const {  
  return auto_size_height_;  
} 

void Window::needsupdate() {
  update_ = true;
  if (parent()) {
    parent()->needsupdate();
  }
}

ui::BoxSpace Window::sum_border_space() const {
  ui::BoxSpace result;	
  if (!ornaments_.empty()) {       
    for (Ornaments::const_iterator it = ornaments_.begin(); it != ornaments_.end(); ++it) {				
      if (*it) {
        result = result + (*it)->preferred_space();		
      }
    }
  }
  return result;
}

void Window::WorkChildPosition() {
  needsupdate();
  std::vector<Window*> windows;
  Window* p = parent();
  while (p) {
    windows.push_back(p);
    p = p->parent();
  }  
  std::vector<Window*>::reverse_iterator rev_it = windows.rbegin();
  for (; rev_it != windows.rend(); ++rev_it) {
    Window* window = *rev_it;
    ChildPosEvent ev(*this);
    window->OnChildPosition(ev);
    if (ev.is_propagation_stopped()) {
      break;
    }
  } 
}

void Window::SetFocus() {
  if (imp()) {
    imp()->DevSetFocus();      
  }
}

void Window::PreventFls() {
  if (root()) {
    root()->PreventFls();
  }
}

void Window::EnableFls() {
  if (root()) {
    root()->EnableFls();
  }
}

bool Window::fls_prevented() const { 
  return root() ? root()->fls_prevented() : false; 
}

void Window::PreventDrawBackground() {
  if (root()) {
    root()->PreventDrawBackground();
  }
}

void Window::EnableDrawBackground() {
  if (root()) {
    root()->EnableDrawBackground();
  }
}

bool Window::draw_background_prevented() const { 
  return root() ? root()->draw_background_prevented() : false; 
}

void Window::Show() {
  if (imp_.get()) {
    imp_->DevShow();
  }  
  visible_ = true;
  OnShow();  
}

void Window::BringToTop() {
  if (imp_.get()) {
    imp_->DevBringToTop();
  }
}

void Window::Show(const boost::shared_ptr<WindowShowStrategy>& aligner) {  
  aligner->set_position(*this);
  Show();  
}

void Window::Hide() {   
  if (imp_.get()) {
    imp_->DevHide();  
  }  
  visible_ = false;
  FLS();
}

bool Window::visible() const { 
 /* if (imp()) {
    bool dev_vis = imp_->dev_visible();
    if (visible_ != dev_vis) {
      int fordebugonly(0);
    }
  }*/
  return visible_;
}
  

bool Window::has_focus() const {
  return imp() ? imp_->dev_has_focus() : false;
}

void Window::Invalidate() { 
  if (imp_.get()) {    
    imp_->DevInvalidate();    
  }
}

void Window::Invalidate(const Region& rgn) {
  if (imp_.get()) {
    imp_->DevInvalidate(rgn);
  }
}

void Window::SetCapture() {
  if (imp_.get()) {
    imp_->DevSetCapture();
  }
}

void Window::ReleaseCapture() {
  if (imp_.get()) {
    imp_->DevReleaseCapture();
  }
}

void Window::ShowCursor() {
  if (imp_.get()) {
    imp_->DevShowCursor();
  }
}

void Window::HideCursor() {
  if (imp_.get()) {
    imp_->DevHideCursor();
  }
}

void Window::SetCursorPosition(const ui::Point& position) {
  if (imp_.get()) {
    imp_->DevSetCursorPosition(position);
  }
}

Point Window::CursorPosition() const {
  return imp() ? imp_->DevCursorPosition() : Point();
}

void Window::SetCursor(CursorStyle::Type style) {
  if (imp_.get()) {
    imp_->DevSetCursor(style);
  }
}

void Window::Enable() {
  if (imp()) {
    imp()->DevEnable();
  }
}

void Window::Disable() {
  if (imp()) {
    imp()->DevDisable();
  }
}

void Window::MapCapslockToCtrl() {
  if (imp()) {
    imp()->DevMapCapslockToCtrl();
  }
}

void Window::EnableCapslock() {
  if (imp()) {
    imp()->DevEnableCapslock();
  }
}

void Window::ViewDoubleBuffered() {
  if (imp_.get()) {
    imp_->DevViewDoubleBuffered();
  }
}

void Window::ViewSingleBuffered() {
  if (imp_.get()) {
    imp_->DevViewSingleBuffered();
  }
}

bool Window::is_double_buffered() const {
  return imp() ? imp_->dev_is_double_buffered() : false;
}

void Window::set_parent(Window* window) {
  if (imp_.get()) {   
    imp_->dev_set_parent(window);
  }
  parent_ = window;
}

void Window::set_clip_children() {
  if (imp_.get()) {
    imp_->dev_set_clip_children();
  }
}

void Window::add_style(UINT flag) {
  if (imp_.get()) {
    imp_->dev_add_style(flag);
  }
}

void Window::remove_style(UINT flag) {
  if (imp_.get()) {
    imp_->dev_remove_style(flag);
  }
}

void Window::DrawBackground(Graphics* g, Region& draw_region) {
  if (draw_region.bounds().height() > 0) {					
    for (Ornaments::iterator it = ornaments_.begin(); it != ornaments_.end(); ++it) {
      if (*it) {					
        (*it)->Draw(*this, g, draw_region);
      }
    }
  }  
}

bool Window::transparent() const {
  bool result = true;
  for (Ornaments::const_iterator it = ornaments_.begin(); it != ornaments_.end(); ++it) {
    if ((*it) && !((*it)->transparent())) {						  
      result = false;
      break;
    }
  }
  return result;
}

const Region& Window::clip() const {
  static std::auto_ptr<ui::Region> dummy(new Region());
  return *dummy.get();
}

template<class T, class T1>
void Window::PreOrderTreeTraverse(T& functor, T1& cond) {  
  if (!functor(*this)) {
    for (iterator i = begin(); i != end(); ++i) {
      Window::Ptr window = *i;      
      if (!cond(*window.get())) {
        window->PreOrderTreeTraverse(functor, cond);
      }
    }  
  }
}

template<class T, class T1>
void Window::PostOrderTreeTraverse(T& functor, T1& cond) {      
  for (iterator i = begin(); i != end(); ++i) {
    Window::Ptr window = *i;		
    if (!cond(*window.get())) {      
      window->PostOrderTreeTraverse(functor, cond);
    }
  }
  functor(*this);
}

// Aligner
Aligner::Aligner() : group_(0), aligned_(false) {}

bool Aligner::full_align_ = true;

bool Visible::operator()(Window& window) const {
  return window.visible();
}

/*
void CalcDim::operator()(Window& window) const { 
 if (window.aligner()) {
   window.aligner()->set_dimension(ui::Dimension());
 }
 if (window.aligner() && 
     window.aligner()->group())
     //!window.aligner()->group()->empty()) // &&
   // (window.aligner()->group()->auto_size_width() || 
   // window.aligner()->group()->auto_size_height()))
 {
   window.aligner()->CalcDimensions();       
 } 
};

bool AbortPos::operator()(Window& window) const { 
  bool result = true;
	if (window.aligner()) { 
		bool window_needs_align = (window.visible() && window.has_childs() && (!window.aligner()->aligned() ||
	  window.has_align_dimension_changed()));   
    result = !window_needs_align;        
    window.aligner()->set_aligned(window_needs_align);
	}
  return result;
}

bool SetUnaligned::operator()(Window& window) const {
  if (window.aligner()) {
    window.aligner()->set_aligned(false);    
  }
  return false;
}

bool SetPos::operator()(Window& window) const {  
  if (window.visible() &&
      window.aligner() && 
      window.aligner()->group() && 
      !window.aligner()->group()->empty() &&
      !window.aligner()->group()->area().bounds().empty()) {    
    window.aligner()->SetPositions();
  }  
  return false;
};*/

void CalcDim::operator()(Window& window) const {
 if (window.aligner()) {
   window.aligner()->CalcDimensions();    
 } 
};

bool AbortPos::operator()(Window& window) const { 
  bool result = true;
	if (window.aligner()) { 
		bool window_needs_align = (window.visible() && window.has_childs() && (!window.aligner()->aligned() ||
	  window.has_align_dimension_changed()));   
    result = !window_needs_align;        
    window.aligner()->set_aligned(window_needs_align);
	}
  return result;
}

bool SetUnaligned::operator()(Window& window) const {
  if (window.aligner()) {
    window.aligner()->set_aligned(false);
  }
  return false;
}

bool SetPos::operator()(Window& window) const {  
  if (window.visible() && window.has_childs()) {    
    window.aligner()->SetPositions();
  }  
  return false;
};

Window::Container Window::SubItems() {
  Window::Container allwindows;
  iterator it = begin();
  for (; it != end(); ++it) {
    Window::Ptr window = *it;
    allwindows.push_back(window);
    Window::Container subs = window->SubItems();
    iterator itsub = subs.begin();
    for (; itsub != subs.end(); ++itsub) {
      allwindows.push_back(*it);
    }
  }
  return allwindows;
}     

// Group
Group::Group() {  
  set_auto_size(false, false);
  rules_.set_owner(this);
}

Group::Group(WindowImp* imp) {
  set_imp(imp);
  set_auto_size(false, false);
  rules_.set_owner(this);
}  

void Group::Add(const Window::Ptr& window, bool apply_rules) {  
  if (window->parent()) {
    throw std::runtime_error("Window already child of a group.");
  }
  window->set_parent(this);
  windows_.push_back(window); 
  if (apply_rules) {
	rules_.ApplyTo(window.get());    
  }
  window->needsupdate();
}

void Group::Insert(iterator it, const Window::Ptr& window) {
  if (window->parent()) {
    throw std::runtime_error("Window already child of a group.");
  }        
  windows_.insert(it, window);
  window->set_parent(this);
  rules_.ApplyTo(window.get());
  window->needsupdate(); 
}

void Group::Remove(const Window::Ptr& window) {
  assert(window);
  iterator it = find(windows_.begin(), windows_.end(), window);
  if (it == windows_.end()) {
    throw std::runtime_error("Window is no child of the group");
  }  
  windows_.erase(it);  
  window->set_parent(0);  
}

ui::Dimension Group::OnCalcAutoDimension() const {	
	Rect result(Point((std::numeric_limits<double>::max)(),
	 						    (std::numeric_limits<double>::max)()),
                  Point((std::numeric_limits<double>::min)(),
                  (std::numeric_limits<double>::min)()));	      
  for (const_iterator i = begin(); i != end(); ++i) {		  
		 Rect window_pos = (*i)->position();
		 window_pos.increase(ui::BoxSpace((*i)->padding().top() + (*i)->border_space().top() + (*i)->margin().top(),
			 (*i)->padding().right() + (*i)->border_space().right() + (*i)->margin().right(),
			 (*i)->padding().bottom() + (*i)->border_space().bottom() + (*i)->margin().bottom(),
			 (*i)->padding().left() + (*i)->border_space().left() + (*i)->margin().left()));
		  if (window_pos.left() < result.left()) {
        result.set_left(window_pos.left());
      }
      if (window_pos.top() < result.top()) {
        result.set_top(window_pos.top());
      }
      if (window_pos.right() > result.right()) {
        result.set_right(window_pos.right());
      }
      if (window_pos.bottom() > result.bottom()) {
        result.set_bottom(window_pos.bottom());
      }
    }
	return result.dimension();
}

void Group::set_zorder(const Window::Ptr& window, int z) {
  assert(window);
  if (z<0 || z>= static_cast<int>(windows_.size())) return;
  iterator it = find(windows_.begin(), windows_.end(), window);
  assert(it != windows_.end());  
  windows_.erase(it);
  windows_.insert(begin()+z, window);  
}

int Group::zorder(const Window::Ptr& window) const {
	int result = -1;
  for (Window::Container::size_type k = 0; k < windows_.size(); ++k) {
    if (windows_[k] == window) {
      result = k;
      break;
    }
  }
  return result;
}

Window::Ptr Group::HitTest(double x, double y) {
  Window::Ptr result;
  Window::Container::const_reverse_iterator rev_it = windows_.rbegin();
  for (; rev_it != windows_.rend(); ++rev_it) {
    Window::Ptr window = *rev_it;
    window = window->visible() 
           ? window->HitTest(x-window->position().left(), y-window->position().top())
		       : Window::Ptr(); 
    if (window) {
      result = window;
      break;
    }
  }
  return result;
}

void Group::OnChildPosition(ChildPosEvent& ev) {
  if (auto_size_width() || auto_size_height()) {
      ev.window()->needsupdate();    		
		ui::Dimension new_dimension = OnCalcAutoDimension();
		if (!auto_size_width()) {
			new_dimension.set_width(position().width());
		}
		if (!auto_size_height()) {
			new_dimension.set_height(position().height());
		}
    imp()->dev_set_position(ui::Rect(position().top_left(), new_dimension));
  } else {
    // ev.StopPropagation();
  }
}

void Group::OnMessage(WindowMsg msg, int param) {
  for (iterator it = windows_.begin(); it != windows_.end(); ++it ) {
    (*it)->OnMessage(msg, param);
  }
}

void Group::set_aligner(const Aligner::Ptr& aligner) { 
  aligner_ = aligner;
  aligner_->set_group(*this);
}
  
Window::Container Aligner::dummy;

void Group::UpdateAlign() {
  if (aligner_) {   
    bool old_is_saving(false);
    bool old_fls_prevented(false);
    if (root()) {     
      old_fls_prevented = fls_prevented();      
      old_is_saving = root()->IsSaving();
      if (!old_fls_prevented) {        
        dynamic_cast<Viewport*>(root())->SetSave(true);
      }
    }         
    aligner_->Align();
    if (root()) {    
      if (!old_is_saving && !old_fls_prevented) {    
       dynamic_cast<Viewport*>(root())->Flush();
      }  
      dynamic_cast<Viewport*>(root())->SetSave(old_is_saving);    
    }
  }
}

void Group::FlagNotAligned() {
  static AbortNone abort_none;
  static SetUnaligned set_unaligned;
  PreOrderTreeTraverse(set_unaligned, abort_none);  
}

// ScrollBar
ScrollBar::ScrollBar() 
  : Window(ui::ImpFactory::instance().CreateScrollBarImp(Orientation::HORZ)) {
}

ScrollBar::ScrollBar(const ui::Orientation::Type& orientation) 
  : Window(ui::ImpFactory::instance().CreateScrollBarImp(orientation)) {  
}

ScrollBar::ScrollBar(ScrollBarImp* imp) : Window(imp) {
}

void ScrollBar::set_scroll_range(int minpos, int maxpos) {
  if (imp()) {
    imp()->dev_set_scroll_range(minpos, maxpos);
  }
}

void ScrollBar::scroll_range(int& minpos, int& maxpos) {
  if (imp()) {
    imp()->dev_scroll_range(minpos, maxpos);
  }
}

void ScrollBar::set_scroll_position(int pos) {
  if (imp()) {
    imp()->dev_set_scroll_position(pos);
  }
}

int ScrollBar::scroll_position() const {
  return imp() ? imp()->dev_scroll_position() : 0;  
}

void ScrollBar::system_size(int& width, int& height) const {
  if (imp()) {
    ui::Dimension dim = imp()->dev_system_size();
    width = static_cast<int>(dim.width());
    height = static_cast<int>(dim.height());
  }
}

void ScrollBarImp::OnDevScroll(int pos) {
  ((ui::ScrollBar*) window())->OnScroll(pos);
  //((ui::ScrollBar*) window())->scroll(*this);
}

// ScrollBox
ScrollBox::ScrollBox() {
  Init();  
}

void ScrollBox::Init() { 
	// ViewDoubleBuffered();
  pane_.reset(new ui::Group());
  client_background_.reset(ui::OrnamentFactory::Instance().CreateFill(0x292929));
	
  //pane_->set_ornament(client_background_);
  //pane_->ViewDoubleBuffered();
	pane_->set_auto_size(false, false);
	Group::Add(pane_);

  client_.reset(new ui::Group());
  //client_->ViewDoubleBuffered();
	//pane_->add_style(0x02000000 | WS_CLIPCHILDREN);
  //client_background_.reset(ui::OrnamentFactory::Instance().CreateFill(0x292929));
  //client_->set_ornament(client_background_);
	client_->set_auto_size(true, true);  
	//client_->add_style(0x02000000 | WS_CLIPCHILDREN);
	pane_->Add(client_);

  hscrollbar_.reset(ui::Systems::instance().CreateScrollBar(Orientation::HORZ));
  hscrollbar_->set_auto_size(false, false);
  hscrollbar_->scroll.connect(boost::bind(&ScrollBox::OnHScroll, this, _1));
  hscrollbar_->set_scroll_range(0, 100);
  hscrollbar_->set_scroll_position(0);
  Group::Add(hscrollbar_);
  vscrollbar_.reset(ui::Systems::instance().CreateScrollBar(Orientation::VERT));
  vscrollbar_->set_auto_size(false, false);
  vscrollbar_->scroll.connect(boost::bind(&ScrollBox::OnVScroll, this, _1));
  vscrollbar_->set_scroll_range(0, 100);
  vscrollbar_->set_scroll_position(0);
  Group::Add(vscrollbar_);
}

void ScrollBox::UpdateScrollRange() {
	if (client_) {
		client_->needsupdate();
 		ui::Dimension dimension = client_->area().bounds().dimension();
		ui::Dimension pane_dimension = pane_->area().bounds().dimension();
		dimension -= pane_dimension;
		hscrollbar_->set_scroll_range(0, static_cast<int>(dimension.width()));
		vscrollbar_->set_scroll_range(0, static_cast<int>(dimension.height()));
	}
}

void ScrollBox::OnSize(const ui::Dimension& dimension) {
  ui::Dimension scrollbar_size = ui::Systems::instance().metrics().scrollbar_size();
  hscrollbar_->set_position(
      ui::Rect(ui::Point(0, dimension.height() -  scrollbar_size.height()),
               ui::Dimension(dimension.width(), scrollbar_size.height())));
  vscrollbar_->set_position(
      ui::Rect(ui::Point(dimension.width() -  scrollbar_size.width(), 0), 
               ui::Dimension(scrollbar_size.width(), dimension.height() - scrollbar_size.height())));
  pane_->set_position(
       ui::Rect(ui::Point(0, 0), 
                ui::Dimension(dimension.width() - scrollbar_size.width() - 2,
                              dimension.height() - scrollbar_size.height() - 2)));
	UpdateScrollRange();
}

void ScrollBox::OnHScroll(ui::ScrollBar& bar) {
  if (!client_->empty()) {
    ui::Window::Ptr view = *client_->begin();
		int dx = static_cast<int>(bar.scroll_position() + view->position().left());
    ScrollBy(-dx, 0);
  }
}

void ScrollBox::OnVScroll(ui::ScrollBar& bar) {
  if (!client_->empty()) {
    ui::Window::Ptr view = *client_->begin();
    int dy = static_cast<int>(bar.scroll_position() + view->position().top());
    ScrollBy(0, -dy);
  }
}

void ScrollBox::ScrollBy(double dx, double dy) {
  if (!client_->empty()) {
    ui::Window::Ptr view = *client_->begin();
    ui::Rect new_pos = view->position();
    new_pos.set_left(new_pos.left() + dx);
    new_pos.set_top(new_pos.top() + dy);
    view->imp()->dev_set_position(new_pos);
    view->ScrollTo(static_cast<int>(-dx), static_cast<int>(-dy));
  }
}

void ScrollBox::ScrollTo(const ui::Point& top_left) {
	if (!client_->empty()) {
		ui::Window::Ptr view = *client_->begin();		
		view->set_position(top_left);
		hscrollbar_->set_scroll_position(static_cast<int>(top_left.x()));
		vscrollbar_->set_scroll_position(static_cast<int>(top_left.y()));
	}
}

void FrameAligner::set_position(Window& window) {  
  using namespace ui;  
  
  Dimension win_dim(
    (width_perc_ < 0)  
        ? window.dim().width()
        : (std::max)(
             window.min_dimension().width(),
             Systems::instance().metrics().screen_dimension().width() * width_perc_),
    (height_perc_ < 0)
        ? window.dim().height() 
        : (std::max)(
             window.min_dimension().height(),
             Systems::instance().metrics().screen_dimension().height() * height_perc_)                       
  );
  Point top_left;
  switch (alignment_) {    
    case AlignStyle::RIGHT:      
      top_left.set_xy(Systems::instance().metrics().screen_dimension().width() - win_dim.width(), (Systems::instance().metrics().screen_dimension().height() - win_dim.height()) /2);
    break;    
    case AlignStyle::CENTER:
      top_left = ((Systems::instance().metrics().screen_dimension() - win_dim) / 2.0).as_point();
    break;
    default:;    
  }
  window.set_position(ui::Rect(top_left, win_dim));
}

Frame::Frame() : Window(ui::ImpFactory::instance().CreateFrameImp()) {
  set_auto_size(false, false); 
}

Frame::Frame(FrameImp* imp) : Window(imp) {
  set_auto_size(false, false);
}

void Frame::set_title(const std::string& title) {
  assert(imp());
  imp()->dev_set_title(title);  
}

std::string Frame::title() const {
  assert(imp());
  return imp()->dev_title();
}

void Frame::set_viewport(const Window::Ptr& viewport) { 
  assert(imp());
  imp()->dev_set_viewport(viewport);  
}

Window::Ptr Frame::viewport() { 
  assert(imp());
  return imp()->dev_viewport();  
}

void Frame::SetMenuRootNode(const Node::Ptr& root_node) {
  assert(imp());
  imp()->DevSetMenuRootNode(root_node);
}

void Frame::ShowDecoration() {
  assert(imp());
  imp()->DevShowDecoration();
}

void Frame::HideDecoration() {
  assert(imp());
  imp()->DevHideDecoration();
}

void Frame::PreventResize() {
  assert(imp());
  imp()->DevPreventResize();
}

void Frame::AllowResize() {
  assert(imp());
  imp()->DevAllowResize();
}

void Frame::WorkOnContextPopup(ui::Event& ev, const ui::Point& mouse_point) {
  if (!popup_menu_.expired()) {
    ev.StopPropagation();
  }
  OnContextPopup(ev, mouse_point);
  if (!ev.is_default_prevented() && !popup_menu_.expired()) {
    popup_menu_.lock()->Track(mouse_point);
  }  
}

PopupFrame::PopupFrame() : Frame(ui::ImpFactory::instance().CreatePopupFrameImp()) {
  set_auto_size(false, false); 
}

PopupFrame::PopupFrame(FrameImp* imp) : Frame(imp) {
  set_auto_size(false, false);
}

recursive_node_iterator Node::recursive_begin() {
  Node::iterator b = begin();
  return recursive_node_iterator(b);
}

recursive_node_iterator Node::recursive_end() {
  Node::iterator e = end();
  return recursive_node_iterator(e);
}

void Node::erase(iterator it) {       
  boost::ptr_list<NodeImp>::iterator imp_it = imps.begin();
  for ( ; imp_it != imps.end(); ++imp_it) {
    imp_it->owner()->DevErase(*it);
  }
  children_.erase(it);
}

void Node::erase_imp(NodeOwnerImp* owner) {
  boost::ptr_list<NodeImp>::iterator it = imps.begin();
  while (it != imps.end()) {          
    if (it->owner() == owner) {      
      it = imps.erase(it);            
     } else {
      ++it;
    }
  }
}

void Node::erase_imps(NodeOwnerImp* owner) {
  struct {    
    NodeOwnerImp* that;    
    void operator()(Node::Ptr node, Node::Ptr prev_node) {
      node->erase_imp(that);
    }
  } imp_eraser;
  imp_eraser.that = owner;
  Node::Ptr nullnode;
  traverse(imp_eraser, nullnode);
}

void Node::clear() {
  for (Node::iterator it = children_.begin(); it != children_.end(); ++it) {
    boost::ptr_list<NodeImp>::iterator imp_it = (*it)->imps.begin();
    for ( ; imp_it != (*it)->imps.begin(); ++imp_it) {
      (*imp_it).owner()->DevErase(*it);
    }
  }  
  imps.clear();
  children_.clear();
}

NodeImp* Node::imp(NodeOwnerImp& owner) {
  NodeImp* result = 0;
  boost::ptr_list<NodeImp>::iterator it = imps.begin();  
  for ( ; it != imps.end(); ++it) {      
   if (it->owner() == &owner) {
     result = &(*it);
     break;
   }        
  }  
  return result;
}

void Node::AddNode(const Node::Ptr& node) {
  ui::Node::Ptr prev;
  if (children_.size() > 0) {
    prev = children_.back();
  }
  children_.push_back(node);
  node->set_parent(this);
  boost::ptr_list<NodeImp>::iterator imp_it = imps.begin();
  for ( ;imp_it != imps.end(); ++imp_it) {    
    imp_it->owner()->DevUpdate(node, prev);
  }
}

void Node::insert(iterator it, const Node::Ptr& node) { 
  Node::Ptr insert_after;
  if (it != begin() && it != end()) {    
    insert_after = *(--iterator(it));
  }      
  children_.insert(it, node);
  node->set_parent(this);
  boost::ptr_list<NodeImp>::iterator imp_it = imps.begin();
  for ( ; imp_it != imps.end(); ++imp_it) {
    imp_it->owner()->DevUpdate(node, insert_after);
  }
}

TreeView::TreeView() : Window(ui::ImpFactory::instance().CreateTreeViewImp()) {
  set_auto_size(false, false);
}

TreeView::TreeView(TreeViewImp* imp) : Window(imp) { 
  set_auto_size(false, false);
}

void TreeView::set_properties(const Properties& properties) {
  Properties::Container::const_iterator it = properties.elements().begin();
  for ( ; it != properties.elements().end(); ++it) {
    if (it->first == "backgroundcolor") {
      set_background_color(it->second.argb_value());
    } else
    if (it->first == "color") {
      set_color(it->second.argb_value());
    }    
  }
}

void TreeView::Clear() {
  if (!root_node_.expired()) {    
    root_node_.reset();
    if (imp()) {
      imp()->DevClear();
    }
  }
}

void TreeView::UpdateTree() {
  if (imp() && !root_node_.expired()) {
    imp()->DevClear();  
    imp()->DevUpdate(root_node_.lock());
  }  
}

Node::WeakPtr TreeView::selected() {
  return imp() ? imp()->dev_selected() : boost::weak_ptr<Node>();  
}

void TreeView::select_node(const Node::Ptr& node) {
  if (imp()) {
    imp()->dev_select_node(node);
  }
}

void TreeView::deselect_node(const Node::Ptr& node) {
  if (imp()) {
    imp()->dev_deselect_node(node);
  }
}

void TreeView::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
  }
}

ARGB TreeView::background_color() const {
  return imp() ? imp()->dev_background_color() : 0xFF00000;
}

void TreeView::set_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_color(color);
  }
}

ARGB TreeView::color() const {
  return imp() ? imp()->dev_color() : 0xFF00000;
}

void TreeView::EditNode(const Node::Ptr& node) {
  if (imp()) {
    imp()->DevEditNode(node);
  }
}

bool TreeView::is_editing() const {
  return imp() ? imp()->dev_is_editing() : false;  
}

void TreeView::ShowLines() {
  if (imp()) {
    imp()->DevShowLines();
  }
}

void TreeView::HideLines() {
  if (imp()) {
    imp()->DevHideLines();
  }
}

void TreeView::ShowButtons() {
  if (imp()) {
    imp()->DevShowButtons();
  }
}

void TreeView::HideButtons() {
  if (imp()) {
    imp()->DevHideButtons();
  }
}

void TreeView::ExpandAll() {
  if (imp()) {
    imp()->DevExpandAll();
  }
}

void TreeView::set_images(const Images::Ptr& images) { 
  images_ = images;
  if (imp()) {
    imp()->dev_set_images(images);
  }
}

// ListView
ListView::ListView() : Window(ui::ImpFactory::instance().CreateListViewImp()) {
  set_auto_size(false, false);
}

ListView::ListView(ListViewImp* imp) : Window(imp) { 
  set_auto_size(false, false);
}

void ListView::set_properties(const Properties& properties) {
  for (Properties::Container::const_iterator it = properties.elements().begin(); it != properties.elements().end(); ++it) {    
    if (it->first == "backgroundcolor") {
      set_background_color(boost::get<ARGB>(it->second.value()));
    } else
    if (it->first == "color") {
      set_color(boost::get<ARGB>(it->second.value()));
    }    
  }
}

void ListView::Clear() {
  if (!root_node_.expired()) {    
    root_node_.reset();
    if (imp()) {
      imp()->DevClear();
    }
  }
}

void ListView::UpdateList() {
  if (imp() && !root_node_.expired()) {
    PreventDraw();
    imp()->DevClear();  
    imp()->DevUpdate(root_node_.lock());
    EnableDraw();    
  }  
}
/* Alternate, if above fails
void ListView::UpdateList() {
	if (imp() && !root_node_.expired()) {
		ui::mfc::ListViewImp* mfc_imp = (ui::mfc::ListViewImp*)imp();
		mfc_imp->SetRedraw(false);
		imp()->DevClear();
		imp()->DevUpdate(root_node_.lock());
		mfc_imp->SetRedraw(true);
		mfc_imp->Invalidate();
	}
}*/

void ListView::EnableDraw() {
  if (imp()) {
    imp()->DevEnableDraw();
  }
}

void ListView::PreventDraw() {
  if (imp()) {
    imp()->DevPreventDraw();
  }
}

void ListView::AddColumn(const std::string& text, int width) {
  if (imp()) {
    imp()->DevAddColumn(text, width);
  }
}

boost::weak_ptr<Node> ListView::selected() {
  return imp() ? imp()->dev_selected() : boost::weak_ptr<Node>();  
}

std::vector<Node::Ptr> ListView::selected_nodes() {
  return imp() ? imp()->dev_selected_nodes() : std::vector<Node::Ptr>();  
}

int ListView::top_index() const {
  return imp() ? imp()->dev_top_index() : 0;  
}

void ListView::EnsureVisible(int index) {
  if (imp()) {
    imp()->DevEnsureVisible(index);
  }
}

void ListView::select_node(const Node::Ptr& node) {
  if (imp()) {
    imp()->dev_select_node(node);
  }
}

void ListView::deselect_node(const Node::Ptr& node) {
  if (imp()) {
    imp()->dev_deselect_node(node);
  }
}

void ListView::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
  }
}

ARGB ListView::background_color() const {
  return imp() ? imp()->dev_background_color() : 0xFF00000;
}

void ListView::set_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_color(color);
  }
}

ARGB ListView::color() const {
  return imp() ? imp()->dev_color() : 0xFF00000;
}

void ListView::EditNode(const Node::Ptr& node) {
  if (imp()) {
    imp()->DevEditNode(node);
  }
}

bool ListView::is_editing() const {
  return imp() ? imp()->dev_is_editing() : false;  
}

void ListView::set_images(const Images::Ptr& images) { 
  images_ = images;
  if (imp()) {
    imp()->dev_set_images(images);
  }
}

void ListView::ViewList() {
  if (imp()) {
    imp()->DevViewList();
  }
}

void ListView::ViewReport() {
  if (imp()) {
    imp()->DevViewReport();
  }
}

void ListView::ViewIcon() {
  if (imp()) {
    imp()->DevViewIcon();
  }
}

void ListView::ViewSmallIcon() {
  if (imp()) {
    imp()->DevViewSmallIcon();
  }
}

void ListView::EnableRowSelect() {
  if (imp()) {
    imp()->DevEnableRowSelect();
  }
}

void ListView::DisableRowSelect() {
  if (imp()) {
    imp()->DevDisableRowSelect();
  }
}

ComboBox::ComboBox() : Window(ui::ImpFactory::instance().CreateComboBoxImp()) {  
  set_auto_size(false, false);
}

ComboBox::ComboBox(ComboBoxImp* imp) : Window(imp) {
}

void ComboBox::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}

void ComboBox::add_item(const std::string& item) {
  if (imp()) {
    imp()->dev_add_item(item);
  }
}

void ComboBox::set_items(const std::vector<std::string>& itemlist) {
  if (imp()) {
    imp()->dev_set_items(itemlist);
  }
}

std::vector<std::string>  ComboBox::items() const {
  if (imp()) {
    return imp()->dev_items();
  } else {
    return std::vector<std::string>();
  }
}

void ComboBox::set_text(const std::string& text) {
  if (imp()) {
    imp()->dev_set_text(text);
  }
}

std::string ComboBox::text() const {
  if (imp()) {
    return imp()->dev_text();
  } else {
    return "";
  }
}

void ComboBox::Clear() {
  if (imp()) {
    imp()->dev_clear();
  }
}

void ComboBox::set_item_index(int index) {
  if (imp()) {
    imp()->dev_set_item_index(index);
  }
}

int ComboBox::item_index() const {
  if (imp()) {
    return imp() ? imp()->dev_item_index() : -1;
  }
  return 0;
}

Edit::Edit() : Window(ui::ImpFactory::instance().CreateEditImp()) { 
  set_auto_size(false, false);
}

Edit::Edit(InputType::Type type) : Window(0) {
  switch (type) {
    case InputType::NUMBER:
      set_imp(ui::ImpFactory::instance().CreateNumberEditImp());
    break;
    default:
      set_imp(ui::ImpFactory::instance().CreateEditImp());
  }
}

Edit::Edit(EditImp* imp) : Window(imp) {
}

void Edit::set_properties(const Properties& properties) {
  for (Properties::Container::const_iterator it = properties.elements().begin(); it != properties.elements().end(); ++it) {
    if (it->first == "font_family") {
      FontInfo fnt = font().font_info();
      fnt.set_family(it->second.string_value());
      set_font(fnt);
    } else
    if (it->first == "font_size") {
      FontInfo fnt = font().font_info();
      fnt.set_size(it->second.int_value());
      set_font(fnt);
    } else
    if (it->first == "font_style") {
      /*FontInfo fnt = font().font_info();
      fnt.set_size(it->second.INT_value());
      set_font(fnt);*/
    } else
    if (it->first == "font_weight") {
      FontInfo fnt = font().font_info();
      fnt.set_weight(it->second.int_value());
      set_font(fnt);
    } else
    if (it->first == "backgroundcolor") {
      set_background_color(boost::get<ARGB>(it->second.value()));
    } else
    if (it->first == "color") {
      set_color(boost::get<ARGB>(it->second.value()));
    }    
  }
}

void Edit::set_text(const std::string& text) {
  if (imp()) {
    imp()->dev_set_text(text);
  }
}

void Edit::set_input_type(InputType::Type input_type) {
  set_imp(ui::ImpFactory::instance().CreateNumberEditImp());
}

std::string Edit::text() const { 
  return imp() ? imp()->dev_text() : "";
}

void Edit::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
    FLS();
  }
}

ARGB Edit::background_color() const {
  return imp() ? imp()->dev_background_color() : 0xFF00000;
}

void Edit::set_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_color(color);
    FLS();
  }
}

ARGB Edit::color() const {
  return imp() ? imp()->dev_color() : 0xFF00000;
}

void Edit::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}

const Font& Edit::font() const {
  return imp() ? imp()->dev_font() : Font::dummy_font;  
}

void Edit::set_sel(int cpmin, int cpmax) {
  if (imp()) {
    imp()->dev_set_sel(cpmin, cpmax);
  }
}

Button::Button() : Window(ui::ImpFactory::instance().CreateButtonImp()) {
  set_auto_size(false, false);
}

Button::Button(const std::string& text) : Window(ui::ImpFactory::instance().CreateButtonImp()) {      
  set_auto_size(false, false);  
  imp()->dev_set_text(text);  
}

Button::Button(ButtonImp* imp) : Window(imp) {
  set_auto_size(false, false);
}

void Button::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}

void Button::Check() {
  if (imp()) {
    imp()->DevCheck();
  }
}

void Button::UnCheck() {
  if (imp()) {
    imp()->DevUnCheck();
  }
}


void Button::set_text(const std::string& text) {
  if (imp()) {
    imp()->dev_set_text(text);
  }
}

std::string Button::text() const {
  return imp() ? imp()->dev_text() : "";
}

CheckBox::CheckBox() : Button(ui::ImpFactory::instance().CreateCheckBoxImp()) {
  set_auto_size(false, false);
}

CheckBox::CheckBox(const std::string& text) : Button(ui::ImpFactory::instance().CreateCheckBoxImp()) {
  set_auto_size(false, false);  
  imp()->dev_set_text(text);  
}

CheckBox::CheckBox(CheckBoxImp* imp) : Button(imp) {
  set_auto_size(false, false);
}

bool CheckBox::checked() const {
  return imp() ? imp()->dev_checked() : false;
}

void CheckBox::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
  }
}

void CheckBox::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}

RadioButton::RadioButton() : Button(ui::ImpFactory::instance().CreateRadioButtonImp()) {
	set_auto_size(false, false);
}

RadioButton::RadioButton(const std::string& text) : Button(ui::ImpFactory::instance().CreateRadioButtonImp()) {
	set_auto_size(false, false);
	imp()->dev_set_text(text);
}

RadioButton::RadioButton(RadioButtonImp* imp) : Button(imp) {
	set_auto_size(false, false);
}

bool RadioButton::checked() const {
	return imp() ? imp()->dev_checked() : false;
}

void RadioButton::Check() {
	if (imp()) {
		imp()->DevCheck();
	}
}

void RadioButton::UnCheck() {
	if (imp()) {
		imp()->DevUnCheck();
	}
}

void RadioButton::set_background_color(ARGB color) {
	if (imp()) {
		imp()->dev_set_background_color(color);
	}
}

void RadioButton::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}

GroupBox::GroupBox() : Button(ui::ImpFactory::instance().CreateGroupBoxImp()) {
   set_auto_size(false, false);
}

GroupBox::GroupBox(const std::string& text) : Button(ui::ImpFactory::instance().CreateGroupBoxImp()) {
  set_auto_size(false, false);
  imp()->dev_set_text(text);
}

GroupBox::GroupBox(GroupBoxImp* imp) : Button(imp) {
  set_auto_size(false, false);
}

bool GroupBox::checked() const {
  return imp() ? imp()->dev_checked() : false;
}

void GroupBox::Check() {
  if (imp()) {
    imp()->DevCheck();
  }
}

void GroupBox::UnCheck() {
  if (imp()) {
    imp()->DevUnCheck();
  }
}

void GroupBox::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
  }
}

void GroupBox::set_font(const Font& font) {
  if (imp()) {
    imp()->dev_set_font(font);
  }  
}


std::string Scintilla::dummy_str_ = "";

Scintilla::Scintilla() : Window(ui::ImpFactory::instance().CreateScintillaImp()) { 
  set_auto_size(false, false);
}

Scintilla::Scintilla(ScintillaImp* imp) : Window(imp) {
}

int Scintilla::f(int sci_cmd, void* lparam, void* wparam) {
  if (imp()) {
    return imp()->dev_f(sci_cmd, lparam, wparam);
  }
  return 0;
}

void Scintilla::AddText(const std::string& text) {
  if (imp()) {
    imp()->DevAddText(text);
  }
}

void Scintilla::InsertText(const std::string& text, int pos) {
  if (imp()) {
    imp()->DevInsertText(text, pos);
  }
}

std::string Scintilla::text_range(int cpmin, int cpmax) const {
  if (imp()) {
    return imp()->dev_text_range(cpmin, cpmax);
  } else {
    return "";
  }
}

void Scintilla::delete_text_range(int pos, int length) {
  if (imp()) {
    imp()->dev_delete_text_range(pos, length);
  }
}


void Scintilla::FindText(const std::string& text, int cpmin, int cpmax, int& pos, int& cpselstart, int& cpselend) const {
  if (imp()) {
    imp()->DevFindText(text, cpmin, cpmax, pos, cpselstart, cpselend);
  }
}

void Scintilla::GotoLine(int line_pos) {
  if (imp()) {
    imp()->DevGotoLine(line_pos);
  }
}

void Scintilla::GotoPos(int char_pos) {
  if (imp()) {
    imp()->DevGotoPos(char_pos);
  }
}

void Scintilla::LineUp() {
  if (imp()) {
    imp()->DevLineUp();
  }
}

void Scintilla::LineDown() {
  if (imp()) {
    imp()->DevLineDown();
  }
} 

void Scintilla::CharLeft() {
  if (imp()) {
    imp()->DevCharLeft();
  }
}

void Scintilla::CharRight() {
  if (imp()) {
    imp()->DevCharRight();
  }
} 

void Scintilla::WordLeft() {
  if (imp()) {
    imp()->DevWordLeft();
  }
}

void Scintilla::WordRight() {
  if (imp()) {
    imp()->DevWordRight();
  }
}

void Scintilla::Cut() {
  if (imp()) {
    imp()->DevCut();
  }
}

void Scintilla::Copy() {
  if (imp()) {
    imp()->DevCopy();
  }
}

void Scintilla::Paste() {
  if (imp()) {
    imp()->DevPaste();
  }
}

int Scintilla::length() const  { 
  return imp() ? imp()->dev_length() : 0;
}

int Scintilla::selectionstart() const { 
  return imp() ? imp()->dev_selectionstart() : 0;
}

int Scintilla::selectionend() const {
  return imp() ? imp()->dev_selectionend() : 0;
}

void Scintilla::SetSel(int cpmin, int cpmax)  {
  if (imp()) {
    imp()->DevSetSel(cpmin, cpmax);
  }
}

void Scintilla::ReplaceSel(const std::string& text)  {
  if (imp()) {
    imp()->DevReplaceSel(text);
  }
}

bool Scintilla::has_selection() const {
  return imp() ? imp()->dev_has_selection() : false;
}

int Scintilla::column() const { 
  return imp() ? imp()->dev_column() : 0;
}

int Scintilla::line() const {
  return imp() ? imp()->dev_line() : 0; 
}

int Scintilla::current_pos() const {
  return imp() ? imp()->dev_current_pos() : 0; 
}

bool Scintilla::over_type() const {
  return imp() ? imp()->dev_over_type() : false;
}

bool Scintilla::modified() const {
  return imp() ? imp()->dev_modified() : false;
}

void Scintilla::set_find_match_case(bool on) {
  if (imp()) {
    imp()->dev_set_find_match_case(on);
  }
}

void Scintilla::set_find_whole_word(bool on) {
  if (imp()) {
    imp()->dev_set_find_whole_word(on);
  }
}

void Scintilla::set_find_regexp(bool on) {
  if (imp()) {
    imp()->dev_set_find_regexp(on);
  }
}

void Scintilla::LoadFile(const std::string& filename) {
  if (imp()) {
		imp()->DevClearAll();
    imp()->DevLoadFile(filename);
  }
}

void Scintilla::Reload() {
  if (imp()) {    
		imp()->DevReload();
  }
}

void Scintilla::SaveFile(const std::string& filename) {
  if (imp()) {
    imp()->DevSaveFile(filename);
  }
}

bool Scintilla::has_file() const {
  return imp() ? imp()->dev_has_file() : false;
}

void Scintilla::set_lexer(const Lexer& lexer) {
  if (imp()) {
    imp()->dev_set_lexer(lexer);
  }
}

void Scintilla::set_properties(const Properties& properties) {
  for (Properties::Container::const_iterator it = properties.elements().begin(); it != properties.elements().end(); ++it) {
    if (it->first == "font_family") {
      FontInfo fnt = font_info();
      fnt.set_family(it->second.string_value());
      set_font_info(fnt);
    } else
    if (it->first == "font_size") {
      FontInfo fnt = font_info();
      fnt.set_size(it->second.int_value());
      set_font_info(fnt);
    } else
    if (it->first == "font_style") {
      // FontInfo fnt = font().font_info();
      // fnt.set_size(it->second.INT_value());
      // set_font(fnt);
    } else
    if (it->first == "font_weight") {
      FontInfo fnt = font_info();
      fnt.set_weight(it->second.int_value());
      set_font_info(fnt);
    } else
    if (it->first == "backgroundcolor") {
      set_background_color(it->second.argb_value());
    } else
    if (it->first == "color") {
      set_foreground_color(it->second.argb_value());
    }    
  }
}

void Scintilla::set_foreground_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_foreground_color(color);
  }
}

ARGB Scintilla::foreground_color() const {
  return imp() ? imp()->dev_foreground_color() : 0;
}

void Scintilla::set_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_background_color(color);
  }
}

ARGB Scintilla::background_color() const {
  return imp() ? imp()->dev_background_color() : 0;
}

void Scintilla::set_linenumber_foreground_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_linenumber_foreground_color(color);
  }
}

ARGB Scintilla::linenumber_foreground_color() const {
  return imp() ? imp()->dev_linenumber_foreground_color() : 0;
}

void Scintilla::set_linenumber_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_linenumber_background_color(color);
  }
}

ARGB Scintilla::linenumber_background_color() const {
  return imp() ? imp()->dev_linenumber_background_color() : 0;
}

void Scintilla::set_folding_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_folding_background_color(color);
  }
}

void Scintilla::set_folding_marker_colors(ARGB fore, ARGB back) {
  if (imp()) {
    imp()->DevSetFoldingMarkerColors(fore, back);
  }
}

void Scintilla::set_sel_foreground_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_sel_foreground_color(color);
  }
}

  //ARGB sel_foreground_color() const { return ToARGB(ctrl().sel_foreground_color()); }  
void Scintilla::set_sel_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_sel_background_color(color);
  }
}

  //ARGB sel_background_color() const { return ToARGB(ctrl().sel_background_color()); }
void Scintilla::set_sel_alpha(int alpha) {
  if (imp()) {
    imp()->dev_set_sel_alpha(alpha);
  }
}

void Scintilla::set_ident_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_ident_color(color);
  }
}

void Scintilla::set_caret_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_caret_color(color);
  }
}

ARGB Scintilla::caret_color() const {
  return imp() ? imp()->dev_caret_color() : 0;
}

void Scintilla::StyleClearAll() {
  if (imp()) {
    imp()->DevStyleClearAll();
  }
}

int Scintilla::add_marker(int line, int id) {
  return imp() ? imp()->dev_add_marker(line, id) : 0;  
}

int Scintilla::delete_marker(int line, int id) {
  return imp() ? imp()->dev_delete_marker(line, id) : 0;  
}

void Scintilla::define_marker(int id, int symbol, ARGB foreground_color, ARGB background_color) {
  if (imp()) {
    imp()->dev_define_marker(id, symbol, foreground_color, background_color);
  }
}

void Scintilla::ShowCaretLine() {
  if (imp()) {
    imp()->DevShowCaretLine();
  }
}

void Scintilla::HideCaretLine() {
  if (imp()) {
    imp()->DevHideCaretLine();
  }
}

void Scintilla::HideLineNumbers() {
  if (imp()) {
    imp()->DevHideLineNumbers();
  }
}

void Scintilla::HideBreakpoints() {
  if (imp()) {
    imp()->DevHideBreakpoints();
  }
}

void Scintilla::HideHorScrollbar() {
  if (imp()) {
    imp()->DevHideHorScrollbar();
  }
}

void Scintilla::set_caret_line_background_color(ARGB color) {
  if (imp()) {
    imp()->dev_set_caret_line_background_color(color);
  }
}

void Scintilla::set_tab_width(int width_in_chars) {
  if (imp()) {
    imp()->dev_set_tab_width(width_in_chars);
  }
}

int Scintilla::tab_width() const {
  return imp() ? imp()->dev_tab_width() : 0;  
}

void Scintilla::ClearAll() {
	if (imp()) {
    imp()->DevClearAll();
  }
}

void Scintilla::Undo() {
  if (imp()) {
    imp()->dev_undo();
  }
}

void Scintilla::Redo() {
  if (imp()) {
    imp()->dev_redo();
  }
}

void Scintilla::PreventInput() {
  if (imp()) {
    imp()->dev_prevent_input();
  }
}

void Scintilla::EnableInput() {
  if (imp()) {
    imp()->dev_enable_input();
  }
}

const std::string& Scintilla::filename() const {
  return imp() ? imp()->dev_filename() : dummy_str_;
}

void Scintilla::set_font_info(const FontInfo& font_info) {
  if (imp()) {
    imp()->dev_set_font_info(font_info);
  }
}

const FontInfo& Scintilla::font_info() const {
  return imp() ? imp()->dev_font_info() : FontInfo::dummy_font_info;
}

GameController::GameController() : id_(-1), prevented_(false) {
  xpos_ = ypos_ = zpos_ = 0;
}

void GameController::AfterUpdate(const GameController& old_state) {
  try {
	  for (int b = 0; b < 32; ++b) {
		bool old_btn_set = old_state.buttons().test(b);
		bool current_btn_set = buttons().test(b);
		if (current_btn_set != old_btn_set) {
		  if (current_btn_set) {
			OnButtonDown(b);
		  } else {
			OnButtonUp(b);
		  }
		}
		if (xposition() != old_state.xposition()) {
		  OnXAxis(xposition(), old_state.xposition());
		}
		if (yposition() != old_state.yposition()) {
		  OnYAxis(yposition(), old_state.yposition());
		}
		if (zposition() != old_state.zposition()) {
		  OnZAxis(zposition(), old_state.zposition());
		}
	  }  
	} catch(std::exception& e) {
		throw std::runtime_error(e.what());
	}
}

FileObserver::FileObserver() : 
   imp_(ui::ImpFactory::instance().CreateFileObserverImp(this)) {
}

void FileObserver::StartWatching() {
  if (imp()) {
    imp()->DevStartWatching();
  }
}

void FileObserver::StopWatching() {
  if (imp()) {
    imp()->DevStopWatching();
  }
}

std::string FileObserver::directory() const {
  return imp() ? imp()->dev_directory() : "";
}

void FileObserver::SetDirectory(const std::string& path) {
  if (imp()) {
    imp()->DevSetDirectory(path);
  }
}

App::App() : imp_(ImpFactory::instance().CreateAppImp()) {
}

void App::Run() {
  if (imp_.get()) {
    imp_->DevRun();
  }
}

void App::Stop() {
  if (imp_.get()) {
    imp_->DevStop();
  }
}

// Ui Factory
Systems& Systems::instance() {
  static Systems instance_(new DefaultSystems());  
  return instance_;
}

void Systems::InitInstance(const std::string& config_path) {
  config_path_ = config_path;
  app_.reset(new App());
#ifdef _WIN32
  TerminalFrame::InitInstance();
#endif
}

void Systems::ExitInstance() {
#ifdef _WIN32
  TerminalFrame::ExitInstance();
#endif
}

SystemMetrics& Systems::metrics() {  
  #ifdef __linux__ 
    static x::SystemMetrics metrics_;
  #elif _WIN32
//      static ui::mfc::SystemMetrics metrics_;
    class s : public SystemMetrics
    {
    public:
        ui::Dimension screen_dimension() const override { return {}; }
        ui::Dimension scrollbar_size() const override { return {}; }
    } static metrics_;
#else
    printf("Platform not supported.\n");
    exit(42);
#endif	
  return metrics_;
}

void Systems::set_concret_factory(Systems& concrete_factory) {
  concrete_factory_.reset(&concrete_factory);
}

ui::Region* Systems::CreateRegion() { 
  assert(concrete_factory_.get());
  return concrete_factory_->CreateRegion(); 
}

ui::Graphics* Systems::CreateGraphics() { 
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGraphics(); 
}

#ifdef _WIN32_
ui::Graphics* Systems::CreateGraphics(CDC* dc) { 
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGraphics(dc); 
}
#endif

/*ui::Graphics* Systems::CreateGraphics(bool debug) { 
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGraphics(debug); 
}*/

ui::Graphics* Systems::CreateGraphics(const Image::Ptr& image) {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGraphics(image); 
}

ui::Image* Systems::CreateImage() { 
  assert(concrete_factory_.get());
  return concrete_factory_->CreateImage(); 
}

ui::Font* Systems::CreateFont() { 
  return new ui::Font();  
}

ui::Fonts* Systems::CreateFonts() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateFonts();
}

ui::Window* Systems::CreateWin() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateWin(); 
}

ui::Viewport* Systems::CreateViewport() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateViewport();
}

ui::Group* Systems::CreateGroup() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGroup();
}

ui::Scintilla* Systems::CreateScintilla() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateScintilla();
}

ui::ScrollBox* Systems::CreateScrollBox() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateScrollBox();
}

ui::RadioButton* Systems::CreateRadioButton() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateRadioButton();
}
ui::GroupBox* Systems::CreateGroupBox() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGroupBox();
}
ui::RectangleBox* Systems::CreateRectangleBox() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateRectangleBox();
}

ui::HeaderGroup* Systems::CreateHeaderGroup() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateHeaderGroup();
}

ui::Frame* Systems::CreateFrame() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateFrame(); 
}

ui::Frame* Systems::CreateMainFrame() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateMainFrame();
}

ui::PopupFrame* Systems::CreatePopupFrame() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreatePopupFrame();
}

ui::ComboBox* Systems::CreateComboBox() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateComboBox();
}

ui::Line* Systems::CreateLine() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateLine();
}

ui::Pic* Systems::CreatePic() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreatePic();
}

ui::Edit* Systems::CreateEdit() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateEdit();
}

ui::Text* Systems::CreateText() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateText();
}

ui::Button* Systems::CreateButton() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateButton();  
}

ui::CheckBox* Systems::CreateCheckBox() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateCheckBox();
}

ui::ScrollBar* Systems::CreateScrollBar(Orientation::Type orientation) {  
  assert(concrete_factory_.get());
  return concrete_factory_->CreateScrollBar(orientation);  
}

ui::TreeView* Systems::CreateTreeView() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateTreeView(); 
}

ui::ListView* Systems::CreateListView() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateListView(); 
}

ui::PopupMenu* Systems::CreatePopupMenu() {
  assert(concrete_factory_.get());
  return concrete_factory_->CreatePopupMenu(); 
}

// DefaultSystems

Region* DefaultSystems::CreateRegion() { 
  return new Region();
}

Graphics* DefaultSystems::CreateGraphics() {
  return new Graphics();
}

/*Graphics* DefaultSystems::CreateGraphics(bool debug) {
  return new Graphics(debug);
}*/

#ifdef _WIN32_
Graphics* DefaultSystems::CreateGraphics(CDC* dc) {
  return new Graphics(dc);
}
#endif

ui::Graphics* DefaultSystems::CreateGraphics(const Image::Ptr& image) {
  return new Graphics(image);
}

Image* DefaultSystems::CreateImage() {
  return new Image();
}

Font* DefaultSystems::CreateFont() {
 return new Font();
}

ui::Fonts* DefaultSystems::CreateFonts() {
  return new Fonts();
}

ui::Window* DefaultSystems::CreateWin() {
  return new Window();
}

ui::Frame* DefaultSystems::CreateFrame() {
  return new Frame();
}

ui::Frame* DefaultSystems::CreateMainFrame() {
  return new Frame();
}

ui::PopupFrame* DefaultSystems::CreatePopupFrame() {
  return new PopupFrame();
}

ui::ComboBox* DefaultSystems::CreateComboBox() {
  return new ComboBox();
}

ui::Viewport* DefaultSystems::CreateViewport() {
  return new Viewport();
}

ui::Group* DefaultSystems::CreateGroup() {
  return new Group();
}

ui::RadioButton* DefaultSystems::CreateRadioButton() {
  return new RadioButton();
}

ui::GroupBox* DefaultSystems::CreateGroupBox() {
  return new GroupBox();
}

ui::RectangleBox* DefaultSystems::CreateRectangleBox() {
  return new RectangleBox();
}

ui::HeaderGroup* DefaultSystems::CreateHeaderGroup() {
  return new HeaderGroup();
}

ui::Edit* DefaultSystems::CreateEdit() {
  return new Edit();
}

Line* DefaultSystems::CreateLine() {
  return new Line();
}

Text* DefaultSystems::CreateText() {
  return new Text();
}

Pic* DefaultSystems::CreatePic() {
  return new Pic();
}

ScrollBox* DefaultSystems::CreateScrollBox() {
  return new ScrollBox();
}

Scintilla* DefaultSystems::CreateScintilla() {
  return new Scintilla();
}

ui::Button* DefaultSystems::CreateButton() {
  return new Button();
}

ui::CheckBox* DefaultSystems::CreateCheckBox() {
  return new CheckBox();
}

ui::ScrollBar* DefaultSystems::CreateScrollBar(ui::Orientation::Type orientation) {
  return new ScrollBar(orientation);
}

ui::TreeView* DefaultSystems::CreateTreeView() {
  return new TreeView();
}

ui::ListView* DefaultSystems::CreateListView() {
  return new ListView();
}

ui::PopupMenu* DefaultSystems::CreatePopupMenu() {
  return new PopupMenu();
}

// WindowImp
void WindowImp::OnDevDraw(Graphics* g, Region& draw_region) {
  assert(window_);	
  try {
    window_->Draw(g, draw_region);    
  } catch(std::exception&) {
  }  
}

void WindowImp::OnDevMouseDown(MouseEvent& ev) {
  assert(window_);	
  window_->OnMouseDown(ev);
}

void WindowImp::OnDevMouseUp(MouseEvent& ev) {
  assert(window_);
  window_->OnMouseUp(ev);	
}

void WindowImp::OnDevMouseMove(MouseEvent& ev) {
  assert(window_);
  window_->OnMouseMove(ev);	
}

void WindowImp::OnDevMouseEnter(MouseEvent& ev) {
  assert(window_);
  window_->OnMouseEnter(ev);	
}

void WindowImp::OnDevMouseOut(MouseEvent& ev) {
  assert(window_);
  window_->OnMouseOut(ev);	
}

void WindowImp::OnDevWheel(WheelEvent& ev) {
  assert(window_);
  window_->OnWheel(ev);	
}

void WindowImp::OnDevSize(const ui::Dimension& dimension) {
  if (window_) {
    window_->needsupdate();
    window_->OnSize(dimension);
  }
}

void FrameImp::OnDevClose() {
  if (window()) {
    ((Frame*)window())->close(*((Frame*)window()));
    ((Frame*)window())->OnClose();
  }
}

void ButtonImp::OnDevClick() {
  if (window()) {
    ((Button*)window())->OnClick();
    ((Button*)window())->ExecuteAction();
    ((Button*)window())->click(*((Button*)window()));
  }
}

void EditImp::OnDevChange() {
  if (window()) {
    ((Edit*)window())->change(*((Edit*)window()));
  }
}

void CheckBoxImp::OnDevClick() {
  if (window()) {
    ((CheckBox*)window())->OnClick();
    ((CheckBox*)window())->ExecuteAction();
    ((CheckBox*)window())->click(*((CheckBox*)window()));
  }
}

// ImpFactory
ImpFactory& ImpFactory::instance() {  
  static ImpFactory instance_;
  if (!instance_.concrete_factory_.get()) {    
if (!instance_.concrete_factory_.get()) {    
  #ifdef __linux__ 
    instance_.concrete_factory_.reset(new ui::x::ImpFactory());
  #elif _WIN32
//    instance_.concrete_factory_.reset(new ui::mfc::ImpFactory());
  #else
    printf("Platform not supported.\n");    
    exit(42);
  #endif
  }
  }
  return instance_;
} 

void ImpFactory::set_concret_factory(ImpFactory& concrete_factory) {
  concrete_factory_.reset(&concrete_factory);
}

ui::WindowImp* ImpFactory::CreateWindowImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateWindowImp(); 
    return nullptr;
}

ui::FrameImp* ImpFactory::CreateFrameImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFrameImp(); 
    return nullptr;
}

ui::AppImp* ImpFactory::CreateAppImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateAppImp(); 
    return nullptr;
}

bool ImpFactory::DestroyWindowImp(ui::WindowImp* imp) {
//   assert(concrete_factory_.get());
//   return concrete_factory_->DestroyWindowImp(imp); 
    return nullptr;
}

ui::AlertImp* ImpFactory::CreateAlertImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateAlertImp(); 
    return nullptr;
}

ui::ConfirmImp* ImpFactory::CreateConfirmImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateConfirmImp(); 
    return nullptr;
}

ui::FileDialogImp* ImpFactory::CreateFileOpenDialogImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFileOpenDialogImp(); 
    return nullptr;
}

ui::FileDialogImp* ImpFactory::CreateFileSaveDialogImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFileSaveDialogImp(); 
    return nullptr;
}

ui::WindowImp* ImpFactory::CreateWindowCompositedImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateWindowCompositedImp(); 
    return nullptr;
}

ui::FrameImp* ImpFactory::CreateMainFrameImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateMainFrameImp(); 
    return nullptr;
}

ui::FrameImp* ImpFactory::CreatePopupFrameImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreatePopupFrameImp(); 
    return nullptr;
}

ui::ScrollBarImp* ImpFactory::CreateScrollBarImp(ui::Orientation::Type orientation) {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateScrollBarImp(orientation); 
    return nullptr;
}

ui::ComboBoxImp* ImpFactory::CreateComboBoxImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateComboBoxImp(); 
    return nullptr;
}

ui::EditImp* ImpFactory::CreateEditImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateEditImp(); 
    return nullptr;
}

ui::EditImp* ImpFactory::CreateNumberEditImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateNumberEditImp(); 
    return nullptr;
}

ui::TreeViewImp* ImpFactory::CreateTreeViewImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateTreeViewImp(); 
    return nullptr;
}

ui::ListViewImp* ImpFactory::CreateListViewImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateListViewImp(); 
    return nullptr;
}

ui::MenuContainerImp* ImpFactory::CreateMenuContainerImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateMenuContainerImp();
    return nullptr;
}

ui::MenuContainerImp* ImpFactory::CreatePopupMenuImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreatePopupMenuImp();
    return nullptr;
}

ui::MenuImp* ImpFactory::CreateMenuImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateMenuImp(); 
    return nullptr;
}

ui::ButtonImp* ImpFactory::CreateButtonImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateButtonImp(); 
    return nullptr;
}

ui::CheckBoxImp* ImpFactory::CreateCheckBoxImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateCheckBoxImp(); 
    return nullptr;
}

ui::RadioButtonImp* ImpFactory::CreateRadioButtonImp() {
// 	assert(concrete_factory_.get());
// 	return concrete_factory_->CreateRadioButtonImp();
    return nullptr;
}

ui::GroupBoxImp* ImpFactory::CreateGroupBoxImp() {
// 	assert(concrete_factory_.get());
// 	return concrete_factory_->CreateGroupBoxImp();
    return nullptr;
}

ui::RegionImp* ImpFactory::CreateRegionImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateRegionImp(); 
    return nullptr;
}

ui::FontInfoImp* ImpFactory::CreateFontInfoImp() {	
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFontInfoImp();  
    return nullptr;
}

ui::FontImp* ImpFactory::CreateFontImp() {  
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFontImp();  
    return nullptr;
}

ui::FontImp* ImpFactory::CreateFontImp(int stock) {  
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFontImp(stock);  
    return nullptr;
}

ui::FontsImp* ImpFactory::CreateFontsImp() {  
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFontsImp();  
    return nullptr;
}

ui::GraphicsImp* ImpFactory::CreateGraphicsImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateGraphicsImp(); 
    return nullptr;
}
/*ui::GraphicsImp* ImpFactory::CreateGraphicsImp(bool debug) {
  assert(concrete_factory_.get());
  return concrete_factory_->CreateGraphicsImp(debug); 
}*/
#ifdef _WIN32
ui::GraphicsImp* ImpFactory::CreateGraphicsImp(CDC* cr) {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateGraphicsImp(cr); 
    return nullptr;
}
#endif
ui::GraphicsImp* ImpFactory::CreateGraphicsImp(const Image::Ptr& image) {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateGraphicsImp(image);
    return nullptr;
}
ui::ImageImp* ImpFactory::CreateImageImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateImageImp(); 
    return nullptr;
}

ui::ScintillaImp* ImpFactory::CreateScintillaImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateScintillaImp(); 
    return nullptr;
}

ui::GameControllersImp* ImpFactory::CreateGameControllersImp() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateGameControllersImp(); 
    return nullptr;
}

ui::FileObserverImp* ImpFactory::CreateFileObserverImp(FileObserver* file_observer) {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateFileObserverImp(file_observer); 
    return nullptr;
}

LockIF* ImpFactory::CreateLocker() {
//   assert(concrete_factory_.get());
//   return concrete_factory_->CreateLocker();
    return nullptr;
}

AlertBox::AlertBox() : imp_(ImpFactory::instance().CreateAlertImp()) {
}

void AlertBox::OpenMessageBox(const std::string& text) {
  assert(imp_.get());
  imp_->DevAlert(text);
}

ConfirmBox::ConfirmBox() : imp_(ImpFactory::instance().CreateConfirmImp()) {
}

bool ConfirmBox::OpenMessageBox(const std::string& text) {
  assert(imp_.get());
  return imp_->DevConfirm(text);
}

FileOpenDialog::FileOpenDialog() {
  imp_.reset(ImpFactory::instance().CreateFileOpenDialogImp());
  assert(imp_.get());
}

FileSaveDialog::FileSaveDialog() {
  imp_.reset(ImpFactory::instance().CreateFileSaveDialogImp()); 
  assert(imp_.get());
}

void FileDialog::Show() {
   assert(imp_.get());
   imp_->DevShow();;
}

bool FileDialog::is_ok() const {
   assert(imp_.get());
   return imp_->dev_is_ok();
}

void FileDialog::set_folder(const std::string& folder) {
   assert(imp_.get());
   imp_->dev_set_folder(folder);
}

std::string FileDialog::folder() const {
  assert(imp_.get());
  return imp_->dev_folder();
}

void FileDialog::set_filename(const std::string& filename) {
  assert(imp_.get());	
  imp_->dev_set_filename(filename);
}

std::string FileDialog::filename() const {
  assert(imp_.get());
  return imp_->dev_filename();
}

} // namespace ui
} // namespace host
} // namespace psycle
