// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

#pragma once
#include "Canvas.hpp"
#include <algorithm> // for std::transform
#include <cctype> // for std::tolower

namespace psycle {
namespace host  {
namespace ui {

class RectangleBox : public Window {
 public:
  typedef boost::shared_ptr<RectangleBox> Ptr;
  typedef boost::shared_ptr<const RectangleBox> ConstPtr;
  typedef boost::weak_ptr<RectangleBox> WeakPtr;
  typedef boost::weak_ptr<const RectangleBox> ConstWeakPtr;
  static std::string type() { return "rectanglebox"; }  
  virtual std::string GetType() const { return "rectanglebox"; }

  RectangleBox() : Window(), fill_color_(0) { set_auto_size(false, false); }

  void set_color(ARGB color) { 
    fill_color_ = color;
    FLS();
  }
  ARGB color() const { return fill_color_; }
  void Draw(Graphics* g, Region& draw_region);

 private:
  ARGB fill_color_;
};

class Pic : public Window {
 public:
  Pic() : Window(), image_(0), zoom_factor_(1.0, 1.0) {
    transparent_ = pmdone = false;
  }
  
  static std::string type() { return "pic"; }

  virtual void Draw(Graphics* g, Region& draw_region);  
	virtual ui::Dimension OnCalcAutoDimension() const;
  void OnSize(const ui::Dimension& dimension) {
		view_dimension_ = dimension;    
    FLS();
  }
  void SetSource(const ui::Point& src) { 
		src_ = src;
    FLS(); 
  }
  void SetImage(Image* image);
	const ui::Dimension& view_dimension() const { return view_dimension_; }
	void set_zoom(const ui::Point& zoom_factor) { 
		zoom_factor_ = zoom_factor;		
		FLSEX();
	}

 private:  
	ui::Dimension view_dimension_;
	ui::Point src_;  
	ui::Point zoom_factor_;
  bool transparent_, pmdone;  
  Image* image_;
};

class Line : public Window {
 public:
  Line() : Window(), color_(0) { }    
  
  static std::string type() { return "line"; }

  virtual void Draw(Graphics* g, Region& draw_region);
  virtual Window::Ptr Intersect(double x, double y, Event* ev, bool &worked);  
  void SetPoints(const Points& pts) { pts_ = pts; FLS(); }
  void SetPoint(int idx, const Point& pt) { pts_[idx] = pt; FLS(); }
  const Points& points() const { return pts_; }
  const Point& PointAt(int index) const { return pts_.at(index); }
  void set_color(ARGB color) { 
    color_ = color;
    FLS();
  }
  ARGB color() const { return color_; }  
 private:
  Points pts_;
  ARGB color_;
};

class Text : public Window {
 public:
  typedef boost::shared_ptr<Text> Ptr;
  typedef boost::shared_ptr<const Text> ConstPtr;
  typedef boost::weak_ptr<Text> WeakPtr;
  typedef boost::weak_ptr<const Text> ConstWeakPtr;

  Text();
  Text(const std::string& text);

  static std::string type() { return "text"; }  
  virtual std::string GetType() const { return "text"; }

  virtual void set_properties(const Properties& properties);
  virtual void Draw(Graphics* cr, Region& draw_region);  
  void set_text(const std::string& text);
  const std::string& text() const { return text_; }
  void set_color(ARGB color) {    
    color_ = color;
    FLS();
  }
  ARGB color() const { return color_; }
  void set_font(const Font& font);
  const Font& font() const { return font_; }
  void set_vertical_alignment(AlignStyle::Type vertical_alignment) { 
    vertical_alignment_ = vertical_alignment;
		UpdateTextAlignment();
  }
  void set_justify(JustifyStyle::Type justify) { 
    justify_ = justify;
    UpdateTextAlignment();
  }
  virtual Dimension OnCalcAutoDimension() const { return text_dimension(); }
  virtual void OnSize(const Dimension& dimension) { UpdateTextAlignment(); }

 private:	
  void UpdateTextAlignment() { is_aligned_ = false; }
  const Point& text_alignment_position();	
  double justify_offset(const Dimension& text_dimension);
  double vertical_alignment_offset(const Dimension& text_dimension);	
  Dimension text_dimension() const;
  std::string text_;
  AlignStyle::Type vertical_alignment_;
  JustifyStyle::Type justify_;
  ARGB color_;
  Font font_;		
  bool is_aligned_;
  Point alignment_position_;
};

class Splitter : public Window {
 public:
	typedef boost::shared_ptr<Splitter> Ptr;
  typedef boost::shared_ptr<const Splitter> ConstPtr;
  typedef boost::weak_ptr<Splitter> WeakPtr;
  typedef boost::weak_ptr<const Splitter> ConstWeakPtr;

  static std::string type() { return "splitter"; }

  Splitter();
  Splitter(Orientation::Type orientation);

  void SetColor(ARGB color) {
    fill_color_ = color; 
    FLS();
  }
	void set_orientation(Orientation::Type orientation) {
		orientation_ = orientation; 
		if (orientation == Orientation::HORZ) {
			set_align(AlignStyle::BOTTOM);
			set_position(ui::Rect(Point(), Dimension(0, 5)));
		} else 
	  if (orientation == Orientation::VERT) {
			set_align(AlignStyle::LEFT);
		  set_position(Rect(Point(), Dimension(5, 0)));
	  }  		
	}
  void Draw(Graphics* g, Region& draw_region);

	virtual void OnMouseDown(MouseEvent& ev);
	virtual void OnMouseUp(MouseEvent& ev);
	virtual void OnMouseMove(MouseEvent& ev);
	virtual void OnMouseOut(MouseEvent& ev);
	virtual void OnMouseEnter(MouseEvent& ev);

 private:
	void StartDrag();
	void UpdateResizeWindow();
	ARGB fill_color_;
	bool do_split_;
	Orientation::Type orientation_;
	double drag_pos_;
	double parent_abs_pos_;
	ui::Window* item_;
	ui::Dimension restore_position_;
};

class TerminalView : public Scintilla, public psycle::host::Timer {
 public: 
  typedef boost::shared_ptr<TerminalView> Ptr;
  typedef boost::shared_ptr<const TerminalView> ConstPtr;
  typedef boost::weak_ptr<TerminalView> WeakPtr;
  typedef boost::weak_ptr<const TerminalView> ConstWeakPtr;

  TerminalView();
  void output(const std::string& text);
  void EnableAutoscroll() { autoscroll_prevented_ = false; }
  void PreventAutoscroll() { autoscroll_prevented_ = true; } 
  void EnableAutoClearText() { auto_clear_text_prevented_ = false; }
  void PreventAutoClearText() { auto_clear_text_prevented_ = true; } 
  void EnableLineLimit() { line_limit_prevented_ = false; }
  void PreventLineLimit() { line_limit_prevented_ = true; } 
  bool autoclear_text_prevented() const { return auto_clear_text_prevented_; }
  bool autoscroll_prevented() const { return autoscroll_prevented_; }
  bool line_limit_prevented() const { return line_limit_prevented_; }
  void set_line_limit(int line_limit) { line_limit_ = line_limit; }
  int line_limit() const { return line_limit_; }
  virtual void OnTimerViewRefresh() { invokelater.Invoke(); }
  virtual void OnKeyDown(KeyEvent& ev);

 private:
   Commands invokelater;   
   bool autoscroll_prevented_;
   int line_limit_;
   bool line_limit_prevented_;
   bool auto_clear_text_prevented_;
};

class TerminalFrame : public Frame {
  public:   
   TerminalFrame() { Init(); }
   void Init();
   void output(const std::string& text) {      
     Show();     
     terminal_view_->output(text);
   }
   virtual void OnShow() { terminal_view_->StartTimer(); }
   virtual void OnClose() { Hide(); }
   virtual void release_imp() {
     Frame::release_imp();
     terminal_view_->release_imp();
   }   
   static void InitInstance() { instance(); }
   static void ExitInstance() { terminal_frame_.reset(0); }
   static TerminalFrame& instance() {
     if (terminal_frame_.get() == 0) {
       terminal_frame_ = std::auto_ptr<TerminalFrame>(new TerminalFrame());
     }
     return *terminal_frame_;
   }

  private:
   void OnAutoscrollClick(CheckBox&);
   void OnLimitLinesClick(CheckBox&);
   void OnLineLimitChange(Edit&);
   void OnClearTextAtLineLimitClick(CheckBox&);   
   void AddOptionField(const ui::Window::Ptr& element, double width); 
   TerminalView::Ptr terminal_view_;
   static std::auto_ptr<TerminalFrame> terminal_frame_;
   Group::Ptr option_panel_;
   ui::Ornament::Ptr option_background_;
};

class HeaderGroup : public Group {
 public:
	static std::string type() { return "headergroup"; }  
  
	HeaderGroup();
	HeaderGroup(const std::string& title);

	virtual void Add(const Window::Ptr& item);
	virtual void RemoveAll();
	virtual void UpdateAlign();
	void set_title(const std::string& title) { header_text_->set_text(title); }
	const std::string& title() const { return header_text_->text(); }
	void FlsClient();

 private:
	void Init();
	Ornament::Ptr header_background_;
	Ornament::Ptr border_;
	Text::Ptr header_text_;
	Group::Ptr client_;
};

} // namespace ui
} // namespace host
} // namespace psycle
