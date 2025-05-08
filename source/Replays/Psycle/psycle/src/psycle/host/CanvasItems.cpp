// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

// #include "stdafx.h"

#include "CanvasItems.hpp"
#include "Ui.hpp"
#include <stdio.h>
#undef _WIN32
#ifdef _WIN32
#include "Scintilla.h"
#endif

namespace psycle {
namespace host  {
namespace ui {
  
void RectangleBox::Draw(Graphics* g, Region& draw_region) {  
  g->set_color(fill_color_);
  g->FillRect(ui::Rect(ui::Point(), dim()));   
}

void Line::Draw(Graphics* g, Region& draw_region) {  
  g->set_color(color());
  ui::Point mp;  
  for (Points::iterator it = pts_.begin(); it != pts_.end(); ++it) {
    Point& pt = (*it);
    if (it != pts_.begin()) {
      g->DrawLine(mp, pt);
    }
    mp = pt;    
  }
}

Window::Ptr Line::Intersect(double x, double y, Event* ev, bool &worked) {
  /*double distance_ = 5;
  Point  p1 = PointAt(0);
  Point  p2 = PointAt(1);
  double  ankathede    = p1.first - p2.first;
  double  gegenkathede = p1.second - p2.second;
  double  hypetenuse   = sqrt(ankathede*ankathede + gegenkathede*gegenkathede);
  if (hypetenuse == 0) return 0;
  double cos = ankathede    / hypetenuse;
  double sin = gegenkathede / hypetenuse;
  int dx = static_cast<int> (-sin*distance_);
  int dy = static_cast<int> (-cos*distance_);
  std::vector<CPoint> pts;
  CPoint p;
  p.x = p1.first + dx; p.y = p1.second - dy;
  pts.push_back(p);
  p.x = p2.first + dx; p.y = p2.second - dy;
  pts.push_back(p);
  p.x = p2.first - dx; p.y = p2.second + dy;
  pts.push_back(p);
  p.x = p1.first - dx; p.y = p1.second + dy;
  pts.push_back(p);
  CRgn rgn;
  rgn.CreatePolygonRgn(&pts[0],pts.size(), WINDING);
  Item::Ptr item = rgn.PtInRegion(x-this->x(),y-this->y()) ? this : 0;
  rgn.DeleteObject();*/
  return Window::Ptr();
}

/*bool Line::OnUpdateArea(const ui::Dimension& preferred_dimension) {
  double dist = 5;  
  double zoom = 1.0; // parent() ? parent()->zoom() : 1.0;
  ui::Rect bounds = area().bounds();
  area_->Clear();
  area_->Add(RectShape(ui::Rect(ui::Point((bounds.left()-dist)*zoom, 
                                          (bounds.top()-dist)*zoom),
                                ui::Point((bounds.left() + bounds.width()+2*dist+1)*zoom, 
                                          (bounds.top() + bounds.height()+2*dist+1)*zoom))));
  return true;
}*/


// Text
Text::Text() : Window(), 
    vertical_alignment_(AlignStyle::TOP),
    justify_(JustifyStyle::LEFTJUSTIFY),
    color_(0xFFFFFFFF),    
    is_aligned_(false) {  
}

Text::Text(const std::string& text) : 
  vertical_alignment_(AlignStyle::TOP),
  justify_(JustifyStyle::LEFTJUSTIFY),
  color_(0xFFFFFFFF),
  text_(text),  
  is_aligned_(false) { 
}

void Text::set_properties(const Properties& properties) {
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
    if (it->first == "color") {
      set_color(boost::get<ARGB>(it->second.value()));
    }    
  }
}

void Text::set_text(const std::string& text) {
  if (text_ != text) {
    text_ = text;
    UpdateTextAlignment();   
    UpdateAutoDimension();  
    FLSEX();
  }
}

void Text::set_font(const Font& font) {  
  font_ = font;
  UpdateTextAlignment();
  UpdateAutoDimension();
  FLSEX();
}

void Text::Draw(Graphics* g, Region& draw_region) {  	   
   g->SetFont(font_);  
   g->set_color(color_);
   g->DrawString(text_, text_alignment_position());  
}

Dimension Text::text_dimension() const {
  Graphics g;		
  g.SetFont(font_); 
  return g.text_dimension(text_);
}

const Point& Text::text_alignment_position() {
  if (!is_aligned_) { 
    ui::Dimension text_dim = text_dimension();
    alignment_position_.set_xy(justify_offset(text_dim),
                                               vertical_alignment_offset(text_dim));
    is_aligned_ = true;
  }	
  return alignment_position_;
}

double Text::justify_offset(const Dimension& text_dimension) {
  double result(0);
  switch (justify_) {	  
    case JustifyStyle::CENTERJUSTIFY:
      result = (dim().width() - text_dimension.width())/2;
    break;
    case JustifyStyle::RIGHTJUSTIFY:
      result = dim().width() - text_dimension.width();
    break;
    default:
    break;
  }
  return result;
}

double Text::vertical_alignment_offset(const Dimension& text_dimension) {
  double result(0);
  switch (vertical_alignment_) {	  
    case AlignStyle::CENTER:        
      result = (dim().height() - text_dimension.height())/2;
    break;
    case AlignStyle::BOTTOM:
      result = dim().height() - text_dimension.height();
    break;
    default:      
    break;
  }
  return result;
}

// Pic
void Pic::Draw(Graphics* g, Region& draw_region) {
	if (image_) {
		if (zoom_factor_ == Point(1.0, 1.0)) {
			g->DrawImage(image_, ui::Rect(ui::Point::zero(), view_dimension_), src_);
		} else {
		  g->DrawImage(image_, ui::Rect(ui::Point::zero(), view_dimension_), src_, zoom_factor_);
		}
	}
}

void Pic::SetImage(Image* image) {  
  image_ = image;
  view_dimension_ = image_ ? image_->dim() : Dimension::zero();
  FLSEX();
}

Dimension Pic::OnCalcAutoDimension() const {
	return view_dimension_;
}

Splitter::Splitter() :  
	Window(), 
	fill_color_(0x404040),
	do_split_(false),
	drag_pos_(-1),
	parent_abs_pos_(0),
	item_(0) {
  set_auto_size(false, false);
  set_orientation(Orientation::HORZ);
  set_align(AlignStyle::BOTTOM);
  set_position(Rect(Point(), Dimension(0, 5)));
}

Splitter::Splitter(Orientation::Type orientation) :
   Window(), 
	 fill_color_(0x404040),
	 do_split_(false),
	 drag_pos_(-1),
	 parent_abs_pos_(0),
	 item_(0) {
  set_auto_size(false, false);
  set_orientation(orientation);
  if (orientation == Orientation::HORZ) {
	  set_align(AlignStyle::BOTTOM);
	  set_position(Rect(Point(), Dimension(0, 5)));
  } else {
	  set_align(AlignStyle::LEFT);
	  set_position(Rect(Point(), Dimension(5, 0)));
  }
}

void Splitter::Draw(Graphics* g, Region& draw_region) { 
  g->set_color(fill_color_);
  g->FillRect(ui::Rect(Point(), dim()));   
}

void Splitter::OnMouseDown(MouseEvent& ev) {
  UpdateResizeWindow();  
  if (ev.button() == 1)	{
	 StartDrag();
  } else if (item_ && ev.button() == 2) {
	if (orientation_ == Orientation::VERT) {   
		if (align() == AlignStyle::LEFT) {
			if (restore_position_.width() == 0) {
				restore_position_ = item_->position().dimension();
				item_->set_position(Rect(item_->position().top_left(),
									Dimension(0, item_->position().height())));      
			} else {				
				item_->set_position(Rect(item_->position().top_left(),
									Dimension(restore_position_.width(),
											  item_->position().height())));
				restore_position_.set(0, 0);
			}
		}
	} else      
	if (orientation_ == Orientation::HORZ) {
		if (restore_position_.height() == 0) {
			restore_position_ = item_->position().dimension();    
			if (align() == AlignStyle::BOTTOM) {
				item_->set_position(Rect(item_->position().top_left(),
									Dimension(item_->position().width(), 0)));
			}
		} else {
			if (align() == AlignStyle::BOTTOM) {
				item_->set_position(Rect(item_->position().top_left(),
									Dimension(item_->position().width(),
									restore_position_.height())));
			}
			restore_position_.set(0, 0);
		}
	}  
	((Group*)parent())->FlagNotAligned();
	parent()->UpdateAlign();   
  }
}

void Splitter::StartDrag() {	
	if (item_) {		
		parent_abs_pos_ = (orientation_ == Orientation::HORZ) 
		? parent()->absolute_position().top()
		: parent()->absolute_position().left();
		do_split_ = true;		
		BringToTop();
		SetCapture();	
	}
}

void Splitter::UpdateResizeWindow() {
	Window* last = item_ = 0;
	drag_pos_ = -1;
	for (iterator it = parent()->begin(); it!=parent()->end(); ++it) {
		if ((*it).get() == this) {
			item_ = last;
			break;
		}
		last = (*it).get();
	}
}

void Splitter::OnMouseUp(MouseEvent& ev) {
	do_split_ = false;
	ReleaseCapture();
  if (orientation_ == Orientation::VERT) {   
    if (align() == AlignStyle::LEFT) {
      item_->set_position(Rect(item_->position().top_left(),
                               Dimension(position().top_left().x() -
                               item_->position().top_left().x(),
                               item_->position().height())));      
    }
  } else      
  if (orientation_ == Orientation::HORZ) {    
    if (align() == AlignStyle::BOTTOM) {
		  item_->set_position(Rect(item_->position().top_left(),
                               Dimension(item_->position().width(),
                               item_->position().bottom_right().y() -
                               position().top_left().y() - 
                               position().height())));
    }
  }  
  ((Group*)parent())->FlagNotAligned();
  parent()->UpdateAlign();    
}

void Splitter::OnMouseMove(MouseEvent& ev) {
	if (do_split_) {
		if (orientation_ == Orientation::HORZ) {
      if (drag_pos_ != ev.client_pos().y()) {        			  
         drag_pos_ = (std::max)(0.0, ev.client_pos().y());
         set_position(Point(position().top_left().x(),
                            drag_pos_ - parent_abs_pos_));
      }
		} else {
			if (orientation_ == Orientation::VERT) {
				if (drag_pos_ != ev.client_pos().x()) {
          drag_pos_ = (std::max)(0.0, ev.client_pos().x());
          set_position(Point(drag_pos_ - parent_abs_pos_,
                             position().top_left().y())); 
				}
			}
		}
	} else {
		if (orientation_ == Orientation::HORZ) {
			SetCursor(CursorStyle::ROW_RESIZE);
		} else
		if (orientation_ == Orientation::VERT) {
			SetCursor(CursorStyle::COL_RESIZE);
		}
	}
}

void Splitter::OnMouseOut(MouseEvent& ev) {
	if (!do_split_) {
		SetCursor(CursorStyle::DEFAULT);
		fill_color_ = 0x404040;
		FLS();
	}
}

void Splitter::OnMouseEnter(MouseEvent& ev) {
  fill_color_ = 0xFF444444;
  FLS();
}

TerminalView::TerminalView() 
     : Scintilla(),
       Timer(),
       autoscroll_prevented_(false),
       auto_clear_text_prevented_(false),
       line_limit_(4096),
       line_limit_prevented_(false) {
  set_background_color(0xFF232323);
  set_foreground_color(0xFFFFBF00);      
  StyleClearAll();
  set_linenumber_foreground_color(0xFF939393);
  set_linenumber_background_color(0xFF232323);     
#ifdef _WIN32	       
  f(SCI_SETWRAPMODE, (void*) SC_WRAP_CHAR, 0);
#endif	       
  PreventInput();
  StartTimer();  
}

void TerminalView::output(const std::string& text) {
#ifdef _WIN32	
  struct {
    std::string text;
    TerminalView* that;
    bool operator()() const {
      that->EnableInput();
      if (!that->line_limit_prevented()) {
        int line_count = that->f(SCI_GETLINECOUNT, 0, 0);
        int line_overflow = line_count - that->line_limit();
        if (line_overflow >= 0) {
          if (!that->autoclear_text_prevented()) {
            int char_pos = that->f(SCI_POSITIONFROMLINE, (LPARAM*)line_overflow, 0);
            that->f(SCI_DELETERANGE, 0, (WPARAM*)char_pos);
            that->AddText(text);
          }
        } else {
          that->AddText(text);
        }
      } else {     
        that->AddText(text);
      }
      that->PreventInput();
      if (!that->autoscroll_prevented()) {
        that->GotoPos(that->length());
      }
      return true;
    }
   } f;
  f.that = this;
  f.text = text;
  invokelater.Add(f);
#endif  
}

void TerminalView::OnKeyDown(KeyEvent& ev) {
  if (ev.keycode() == KeyCodes::VKDELETE) {
    EnableInput();
    ClearAll();
    PreventInput();
  }
}  

std::auto_ptr<TerminalFrame> TerminalFrame::terminal_frame_(0);

void TerminalFrame::Init() {
  set_title("Psycle Terminal");
  Viewport::Ptr view_port = Viewport::Ptr(new Viewport());
  view_port->set_aligner(Aligner::Ptr(new DefaultAligner()));
  set_viewport(view_port);
  view_port->SetSave(false);
  terminal_view_ = boost::shared_ptr<TerminalView>(new TerminalView());
  terminal_view_->set_auto_size(false, false);
  terminal_view_->set_align(AlignStyle::CLIENT);
  view_port->Add(terminal_view_);      
  option_panel_.reset(new Group());
  option_panel_->set_aligner(Aligner::Ptr(new DefaultAligner()));
  option_panel_->set_auto_size(false, true);
  option_panel_->set_align(AlignStyle::BOTTOM);  
  option_background_.reset(OrnamentFactory::Instance().CreateFill(0xFFCACACA));
  option_panel_->add_ornament(option_background_);  
  view_port->Add(option_panel_);  
  CheckBox::Ptr autoscroll_checkbox(new CheckBox());  
  autoscroll_checkbox->set_text("Autoscroll");  
  autoscroll_checkbox->click.connect(boost::bind(&TerminalFrame::OnAutoscrollClick, this, _1));
  autoscroll_checkbox->Check();
  AddOptionField(autoscroll_checkbox, 140);  
  CheckBox::Ptr limit_lines_checkbox(new CheckBox());
  limit_lines_checkbox->set_text("Limit Lines To");
  limit_lines_checkbox->Check();
  limit_lines_checkbox->click.connect(boost::bind(&TerminalFrame::OnLimitLinesClick, this, _1));  
  AddOptionField(limit_lines_checkbox, 100);
  Edit::Ptr line_limit_input(new Edit(InputType::NUMBER));  
  line_limit_input->set_text("4096");  
  line_limit_input->change.connect(boost::bind(&TerminalFrame::OnLineLimitChange, this, _1));  
  AddOptionField(line_limit_input, 60);
  CheckBox::Ptr clear_text_checkbox(new CheckBox());
  clear_text_checkbox->set_text("Clear first lines at line limit");
  clear_text_checkbox->click.connect(boost::bind(&TerminalFrame::OnClearTextAtLineLimitClick, this, _1));
  clear_text_checkbox->Check();
  AddOptionField(clear_text_checkbox, 150);
  set_position(Rect(Point(), Dimension(500, 400)));
}

void TerminalFrame::AddOptionField(const Window::Ptr& element, double width) { 
  element->set_auto_size(false, false);
  element->set_align(AlignStyle::LEFT);  
  element->set_position(Rect(Point(), Dimension(width, 20)));    
  option_panel_->Add(element);
}

void TerminalFrame::OnAutoscrollClick(CheckBox& autoscroll_checkbox) {
  if (autoscroll_checkbox.checked()) {
    terminal_view_->EnableAutoscroll();
  } else {
    terminal_view_->PreventAutoscroll();
  }
}

void TerminalFrame::OnLimitLinesClick(CheckBox& limit_lines_checkbox) {
  if (limit_lines_checkbox.checked()) {
    terminal_view_->EnableLineLimit();
  } else {
    terminal_view_->PreventLineLimit();
  }
}

typedef enum {
    STR2INT_SUCCESS,
    STR2INT_OVERFLOW,
    STR2INT_UNDERFLOW,
    STR2INT_INCONVERTIBLE
} str2int_errno;

str2int_errno str2int(int *out, const char *s, int base) {
#ifdef _WIN32	
    char *end;
    if (s[0] == '\0' || isspace((unsigned char) s[0]))
        return STR2INT_INCONVERTIBLE;
    errno = 0;
    long l = strtol(s, &end, base);
    // Both checks are needed because INT_MAX == LONG_MAX is possible. 
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return STR2INT_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return STR2INT_UNDERFLOW;
    if (*end != '\0')
        return STR2INT_INCONVERTIBLE;
    *out = l;
#endif	
    return STR2INT_SUCCESS;
}

void TerminalFrame::OnLineLimitChange(Edit& edit) {
  int val;
#ifdef _WIN32
  if (str2int(&val, edit.text().c_str(), 10) == STR2INT_SUCCESS) {
    terminal_view_->set_line_limit(val);
  }  
#endif	
}

void TerminalFrame::OnClearTextAtLineLimitClick(CheckBox& clear_text_checkbox) {
  if (clear_text_checkbox.checked()) {
    terminal_view_->EnableAutoClearText();
  } else {
    terminal_view_->PreventAutoClearText();
  }
}

HeaderGroup::HeaderGroup() {		
   Init();
}

HeaderGroup::HeaderGroup(const std::string& title) {
	Init();
	header_text_->set_text(title);
}

void HeaderGroup::Init() {
	header_background_.reset(OrnamentFactory::Instance().CreateFill(0xFF6D799C));
	LineBorder* border = new LineBorder(0xFF444444);
	border->set_border_radius(BorderRadius(5));
	border_.reset(border);
	add_ornament(border_);
	set_aligner(Aligner::Ptr(new DefaultAligner()));
	set_auto_size(false, false);
	Group::Ptr header(new Group());
	header->set_align(AlignStyle::TOP);
	header->set_auto_size(false, true);
	header->set_aligner(Aligner::Ptr(new DefaultAligner()));
	header->add_ornament(header_background_);
	header->set_margin(BoxSpace(5));
	Group::Add(header);
	header_text_.reset(new Text());
	header_text_->set_text("Header");
	header->Add(header_text_);	
	header_text_->set_font(Font(FontInfo("Tahoma", 12, 500, FontStyle::ITALIC)));
	header_text_->set_color(0xFFFFFFFF);
	header_text_->set_align(AlignStyle::LEFT);
	header_text_->set_auto_size(true, true);
	header_text_->set_margin(BoxSpace(0, 5, 0, 0));
	client_.reset(new Group());
	Group::Add(client_);
	client_->set_align(AlignStyle::CLIENT);
	client_->set_auto_size(false, false);
	client_->set_aligner(Aligner::Ptr(new DefaultAligner()));
	client_->set_margin(BoxSpace(0, 5, 5, 5));
}

void HeaderGroup::Add(const ui::Window::Ptr& item) {		
	client_->Add(item);
}

void HeaderGroup::RemoveAll() {		
	client_->RemoveAll();
}

void HeaderGroup::UpdateAlign() {		
	client_->UpdateAlign();
}

void HeaderGroup::FlsClient() {
	client_->Invalidate();
}

} // namespace ui
} // namespace host
} // namespace psycle

