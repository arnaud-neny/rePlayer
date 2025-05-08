// This source is free software ; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation ; either version 2, or (at your option) any later 
// version.
// copyright 2015-2016 members of the psycle project 
// http://psycle.sourceforge.net

#pragma once
// #include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "LockIF.hpp"
#define BOOST_SIGNALS_NO_DEPRECATION_WARNING
#include <boost/signal.hpp>
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/ptr_container/ptr_list.hpp>
#include <boost/unordered_map.hpp>
#pragma warning(push)
#pragma warning(disable: 4345)
#include <boost/variant.hpp>
#pragma warning(pop)
// #include <boost/locale.hpp>
#include <bitset>
#include <stack>

#undef GetGValue
#define GetGValue(rgb) (LOBYTE((rgb) >> 8))
#define ToCOLORREF(argb) RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, (argb >> 0) & 0xFF)
#define GetAlpha(argb) (argb>>24) & 0xFF
#define RGBToARGB(r, g, b) (((ARGB) (b) << 0) | ((ARGB) (g) << 8) |((ARGB) (r) << 16) | ((ARGB) (0xFF) << 24))
#define ToARGB(rgb) RGBToARGB(GetRValue(rgb), GetGValue(rgb), GetBValue(rgb))

// #define ANSITOUTF8(str) boost::locale::conv::to_utf<char>(str, std::locale())
#define ANSITOUTF8(str) str

#ifdef __linux__ 
   typedef void* MSG;
   typedef uint32_t UINT;
   typedef unsigned long COLORREF;
#endif

namespace psycle {
namespace host {

class LockIF;

namespace ui {

typedef uint32_t ARGB;
typedef uint32_t RGB;
typedef uint32_t RGBA;

/*extern class {
 public:
  template<typename T>
  operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(); }
  template<typename T>
  operator boost::weak_ptr<T>() { return boost::weak_ptr<T>(); }
} nullpointer;*/

struct Color {
  Color() : argb_(0xFF000000) {}  
  Color(ARGB argb) : argb_(argb) {}
  
  ARGB argb() const { return argb_; }

 private:   
   ARGB argb_;
};

struct Point {
  typedef boost::shared_ptr<Point> Ptr;
  typedef boost::shared_ptr<const Point> ConstPtr;
  typedef boost::weak_ptr<Point> WeakPtr;
    
  Point() : x_(0), y_(0) {}
  Point(double x, double y) : x_(x), y_(y) {}

  inline static const Point& zero() { return zero_; }
  inline bool operator==(const Point& rhs) const { 
    return x_ == rhs.x()  && y_ == rhs.y();
  }
  inline bool operator!=(const Point& rhs) const { return !(*this == rhs); }
  inline Point operator+(const Point& rhs) const { 
    return Point(x_ + rhs.x(), y_ + rhs.y());
  }
  inline Point operator-(const Point& rhs) const {
    return Point(x_ - rhs.x(), y_ - rhs.y());
  }
  Point& operator+=(const Point& rhs) {
    x_ += rhs.x();
    y_ += rhs.y();
    return *this;
  }
  Point& operator-=(const Point& rhs) {
    x_ -= rhs.x();
    y_ -= rhs.y();
    return *this;
  }
  Point& operator*=(const Point& rhs) {
    x_ *= rhs.x();
    y_ *= rhs.y();
    return *this;
  }
  Point& operator/=(const Point& rhs) {
    x_ /= rhs.x();
    y_ /= rhs.y();
    return *this;
  }
  void set_xy(double x, double y) { 
    x_ = x; 
    y_ = y;    
  }
  void set_x(double x) { x_ = x; }  
  double x() const { return x_; }
  void set_y(double y) { y_ = y; }
  double y() const { return y_; }
  void offset(double dx, double dy) {
	x_ += dx;
	y_ += dy;
  }
  void reset() { x_ = y_ = 0; }

 private:
  double x_, y_;
  static const Point zero_;
};

typedef std::vector<Point> Points;

struct Dimension {
  typedef boost::shared_ptr<Dimension> Ptr;
  typedef boost::shared_ptr<const Dimension> ConstPtr;
  typedef boost::weak_ptr<Dimension> WeakPtr;

  Dimension() : width_(0), height_(0) {}
  Dimension(double val) : width_(val), height_(val) {}
  Dimension(double width, double height) : width_(width), height_(height) {}

  inline static const Dimension& zero() {
	static Dimension* zero_dimension = new Dimension();
    return *zero_dimension;
  }
  inline bool operator==(const Dimension& rhs) const { 
    return width_ == rhs.width()  && height_ == rhs.height();
  }
  inline bool operator!=(const Dimension& rhs) const { 
    return !(*this == rhs);
  }
  inline Dimension operator+(const Dimension& rhs) const { 
    return Dimension(width_ + rhs.width(), height_ + rhs.height());
  }
  inline Dimension operator-(const Dimension& rhs) const {
    return Dimension(width_ - rhs.width(), height_ - rhs.height());
  }
  inline Dimension operator/(const Dimension& rhs) const {
    return Dimension(width_/rhs.width(), height_/rhs.height());
  }
  inline Dimension operator*(const Dimension& rhs) const {
    return Dimension(width_*rhs.width(), height_*rhs.height());
  }
  inline Dimension operator*(const Point& rhs) const {
	  return Dimension(width_*rhs.x(), height_*rhs.y());
  }
  Dimension& operator+=(const Dimension& rhs) {
    width_ += rhs.width();
    height_ += rhs.height();
    return *this;
  }
  Dimension& operator-=(const Dimension& rhs) {
    width_ -= rhs.width();
    height_ -= rhs.height();
    return *this;
  }
  Dimension& operator*=(const Dimension& rhs) {
    width_ *= rhs.width();
    height_ *= rhs.height();
    return *this;
  }
  Dimension& operator/=(const Dimension& rhs) {
    width_ /= rhs.width();
    height_ /= rhs.height();
    return *this;
  } 
  void set(double width, double height) { 
    width_ = width;
    height_ = height;
  }
  void set_width(double width) { width_ = width; }
  void set_height(double height) { height_ = height; }
  double width() const { return width_; }
  double height() const { return height_; }
  Point as_point() const { return Point(width_, height_); }
  void reset() { width_ = height_ = 0; }
 
 private:
  double width_, height_;
};

class Region;

struct BoxSpace {
  typedef boost::shared_ptr<BoxSpace> Ptr;
  typedef boost::shared_ptr<const BoxSpace> ConstPtr;
  typedef boost::weak_ptr<BoxSpace> WeakPtr;

  BoxSpace() {}
  BoxSpace(double space) : 
      top_left_(space, space),
	  bottom_right_(space, space) {
  }
  BoxSpace(double top, double right, double bottom, double left) :
      top_left_(left, top),
      bottom_right_(right, bottom) {	
  }

  inline static const BoxSpace& zero() {
    static BoxSpace* zero_space = new BoxSpace();
    return *zero_space;
  }
  inline BoxSpace operator+(const BoxSpace& rhs) const { 
    return BoxSpace(top_left_.y() + rhs.top_left_.y(),
			        bottom_right_.x() + rhs.bottom_right_.x(),
			        bottom_right_.y() + rhs.bottom_right_.y(),
			        top_left_.x() + rhs.top_left_.x());
  }
  inline bool operator==(const BoxSpace& rhs) const { 
    return top_left_ == rhs.top_left_ && bottom_right_ == rhs.bottom_right_;
  }
  inline bool operator!=(const BoxSpace& rhs) const { return !(*this == rhs); }
  inline bool empty() const { 
    return top_left_ == Point::zero() && bottom_right_ == Point::zero();
  }
  void set(double space) { 
    top_left_.set_xy(space, space);
    bottom_right_.set_xy(space, space);
  }
  void set(double top, double right, double bottom, double left) {
    top_left_.set_xy(left, top);
    bottom_right_.set_xy(right, bottom);
  }
  void set_top(double top) { top_left_.set_y(top); }
  double top() const { return top_left_.y(); }
  void set_right(double right) { bottom_right_.set_x(right); }
  double right() const { return bottom_right_.x(); }
  void set_bottom(double bottom) { bottom_right_.set_y(bottom); }
  double bottom() const { return bottom_right_.y(); }
  void set_left(double left) { top_left_.set_x(left); }
  double left() const { return top_left_.x(); }
  double width() const { return top_left_.x() + bottom_right_.x(); }
  double height() const { return top_left_.y() + bottom_right_.y(); }
	
 private:
  Point top_left_, bottom_right_;
};

struct Rect {
  typedef boost::shared_ptr<Rect> Ptr;
  typedef boost::shared_ptr<const Rect> ConstPtr;
  typedef boost::weak_ptr<Rect> WeakPtr;
  
  Rect() {}  
  Rect(const Point& top_left, const Point& bottom_right) :
     top_left_(top_left),
     bottom_right_(bottom_right) {
  }
  Rect(const Point& top_left, const Dimension& dim) : 
     top_left_(top_left),
     bottom_right_(top_left.x() + dim.width(), top_left.y() + dim.height()) {
  }

  inline static const Rect& zero() { return zero_; }
  inline bool operator==(const Rect& rhs) const { 
    return left() == rhs.left() && 
           top() == rhs.top() && 
           right() == rhs.right() && 
           bottom() == rhs.bottom();
  }
  inline bool operator!=(const Rect& rhs) const { return !(*this == rhs); }
  inline void reset() {
    top_left_.reset();
    bottom_right_.reset();
  } 
  inline void set(const Point& top_left, const Point& bottom_right) {
    top_left_ = top_left;
    bottom_right_ = bottom_right;
  }
  inline void set(const Point& top_left, const Dimension& dim) {
    top_left_ = top_left;
    bottom_right_.set_xy(top_left.x() + dim.width(),
		                 top_left.y() + dim.height());
  }  
  inline void set_left(double left) { top_left_.set_x(left); }
  inline void set_top(double top) { top_left_.set_y(top); }
  inline void set_right(double right) { bottom_right_.set_x(right); }
  inline void set_bottom(double bottom) { bottom_right_.set_y(bottom); }
  inline void set_width(double width) {
    bottom_right_.set_x(top_left_.x() + width);
  }
  inline void set_height(double height) {
    bottom_right_.set_y(top_left_.y() + height);
  }
  inline double left() const { return top_left_.x(); }
  inline double top() const { return top_left_.y(); }
  inline double right() const { return bottom_right_.x(); }
  inline double bottom() const { return bottom_right_.y(); }
  inline double width() const { return bottom_right_.x() - top_left_.x(); }
  inline double height() const { return bottom_right_.y() - top_left_.y(); }
  inline void set_top_left(const Point& top_left) { top_left_ = top_left; }
  inline const Point& top_left() const { return top_left_; }
  inline const Point top_right() const { return Point(right(), top()); }
  inline void set_bottom_right(const Point& bottom_right) { bottom_right_ = bottom_right; }
  inline const Point& bottom_right() const { return bottom_right_; }
  inline const Point bottom_left() const { return Point(left(), bottom()); }  
  inline void set_dimension(const Dimension& dimension) {
    set_width(dimension.width());
    set_height(dimension.height());
  }
  inline Dimension dimension() const {
    return Dimension(width(), height());
  }
  inline bool empty() const { 
    return top_left_.x() == 0 && top_left_.y() == 0 && 
           bottom_right_.x() == 0 && bottom_right_.y() == 0;
  }
  void increase(const BoxSpace& space) {
    top_left_.set_x(top_left_.x() - space.left());
    top_left_.set_y(top_left_.y() - space.top());
    bottom_right_.set_x(bottom_right_.x() + space.right());
    bottom_right_.set_y(bottom_right_.y() + space.bottom());
  }
  void decrease(const BoxSpace& space) {
    top_left_.set_x(top_left_.x() + space.left());
    top_left_.set_y(top_left_.y() + space.top());
    bottom_right_.set_x(bottom_right_.x() - space.right());
    bottom_right_.set_y(bottom_right_.y() - space.bottom());
  }  
  inline void offset(double dx, double dy) {
    top_left_.set_x(top_left_.x() + dx);
    top_left_.set_y(top_left_.y() + dy);
    bottom_right_.set_x(bottom_right_.x() + dx);
    bottom_right_.set_y(bottom_right_.y() + dy);    
  }
  inline void offset(const Point& delta) { offset(delta.x(), delta.y()); }
  inline void move(const Point& to) { set(to, dimension()); }
  inline bool intersect(const Point& point) {    
    if (point.x() < left()) return false;
    if (point.y() < top()) return false;
    if (point.x() >= right()) return false;
    if (point.y() >= bottom()) return false;
    return true;
  }
 
  std::auto_ptr<Region> region() const;
  std::string str() const {
    std::stringstream str;
    str << top_left_.x() << " " << top_left_.y() << " " << bottom_right_.x()
		<< " " << bottom_right_.y();
    return str.str();
  }
  
 private:
  Point top_left_, bottom_right_;  
  static const Rect zero_;
};

class SystemMetrics {
 public:    
  SystemMetrics() {}  
  virtual ~SystemMetrics() = 0;

  virtual ui::Dimension screen_dimension() const = 0;
  virtual ui::Dimension scrollbar_size() const = 0;
};

 inline SystemMetrics::~SystemMetrics() {}

class RegionImp;

class Region {
 public:    
  typedef boost::shared_ptr<Region> Ptr;
  typedef boost::shared_ptr<const Region> ConstPtr;
  typedef boost::weak_ptr<Region> WeakPtr;

  Region();
  Region(ui::RegionImp* imp);
  Region(const ui::Rect& rect);
  Region& operator = (const Region& other);
  Region(const Region& other);
  virtual ~Region() {}
  
  ui::RegionImp* imp() { return imp_.get(); }
  ui::RegionImp* imp() const { return imp_.get(); }

  virtual Region* Clone() const;
  virtual void Offset(double dx, double dy);  
  virtual void Union(const Region& other);
  virtual ui::Rect bounds() const;
  virtual bool Intersect(double x, double y) const;
  virtual bool IntersectRect(const ui::Rect& rect) const;
  virtual void Clear();
  virtual void SetRect(const ui::Rect& rect);  
  
 private:  
  std::auto_ptr<RegionImp> imp_;
};

struct Shape {
  typedef boost::shared_ptr<Shape> Ptr;
  typedef boost::shared_ptr<const Shape> ConstPtr;
  typedef boost::weak_ptr<Shape> WeakPtr;

  Shape() {}
  virtual ~Shape() {}

  virtual void offset(double dx, double dy) = 0;
  virtual Rect bounds() const = 0;
};

struct RectShape : public Shape {
  RectShape() {}
  RectShape(const Rect& rect) : rect_(rect) {}
  virtual ~RectShape() {}
  virtual void offset(double dx, double dy) { rect_.offset(dx, dy); }
  void SetRect(const Rect& rect) { rect_ = rect; }
  virtual Rect bounds() const{ return rect_; }

 private:
  Rect rect_;
};

struct Area {
  typedef boost::shared_ptr<Area> Ptr;
  typedef boost::shared_ptr<const Area> ConstPtr;
  typedef boost::weak_ptr<Area> WeakPtr;

  Area();
  Area(const Rect& rect);
  Area* Clone() const;
  ~Area();
  
  void Add(const RectShape& area);
  void Clear();
  void Offset(double dx, double dy);
  int Combine(const Area& other, int combinemode);
  bool Intersect(double x, double y) const;
  const Rect& bounds() const;
  const Region& region() const;
  int size() const { return rect_shapes_.size(); }

 private:
  void Update() const {
    if (needs_update_) {
      ComputeBounds();
      ComputeRegion();
      needs_update_ = false;
    }
  }
  void ComputeBounds() const;
  void ComputeRegion() const;
  typedef std::vector<RectShape>::iterator rect_iterator;
  typedef std::vector<RectShape>::const_iterator rect_const_iterator;
  std::vector<RectShape> rect_shapes_; 
  volatile mutable bool needs_update_;   
  mutable Rect bound_cache_;
  mutable Region region_cache_;   
};

namespace Orientation {
enum Type {HORZ = 0, VERT = 1};
}

namespace  CursorStyle {
enum Type {
  AUTO, MOVE, NO_DROP, COL_RESIZE, ALL_SCROLL, POINTER, NOT_ALLOWED,
  ROW_RESIZE, CROSSHAIR, PROGRESS, E_RESIZE, NE_RESIZE, DEFAULT, TEXT,
  N_RESIZE, NW_RESIZE, HELP, VERTICAL_TEXT, S_RESIZE, SE_RESIZE, INHERIT,
  WAIT, W_RESIZE, SW_RESIZE
};
}

class Event {
 public:  
  Event() : work_parent_(false), default_prevented_(false), stop_propagation_(false) {}
  virtual ~Event() {}

  void WorkParent() { work_parent_ = true; }
  void StopWorkParent() { work_parent_ = false; }
  bool is_work_parent() const { return work_parent_; }
  void PreventDefault() { default_prevented_ = true; }
  bool is_default_prevented() const { return default_prevented_; }  
  void StopPropagation() { stop_propagation_ = true; }
  bool is_propagation_stopped() const { return stop_propagation_; }
  void set_relative_offset(const Point& offset) { relative_offset_ = offset; }
  const Point& relative_offset() const { return relative_offset_; }
  
 private:    
  bool work_parent_;
  bool default_prevented_;
  bool stop_propagation_;
  Point relative_offset_;
};

class Window;

class MouseEvent : public Event {
 public:
  typedef boost::shared_ptr<MouseEvent> Ptr;
  typedef boost::shared_ptr<const MouseEvent> ConstPtr;
  typedef boost::weak_ptr<MouseEvent> WeakPtr;
  typedef boost::weak_ptr<const MouseEvent> ConstWeakPtr;

  MouseEvent() : button_(0), shift_(0) {}
  MouseEvent(const Point& client_pos, int button, unsigned int shift) : 
       client_pos_(client_pos),        
       button_(button), 
       shift_(shift) {
  }
  MouseEvent(Window* sender,
                      const Point& client_pos,
                      int button, unsigned int shift) : 
       sender_(sender),
       client_pos_(client_pos),        
       button_(button), 
       shift_(shift) {
  }
  const Point& client_pos() const { return client_pos_; }  
  Point window_pos() const { return client_pos_ - relative_offset(); }
  int button() const { return button_; }
  int shift() const { return shift_; }
  Window* sender() const { return sender_;  }  

 private:  
  Window* sender_;
  Point client_pos_;
  
  int button_, shift_;
};

class WheelEvent : public Event {
 public:
  typedef boost::shared_ptr<WheelEvent> Ptr;
  typedef boost::shared_ptr<const WheelEvent> ConstPtr;
  typedef boost::weak_ptr<WheelEvent> WeakPtr;
  typedef boost::weak_ptr<const WheelEvent> ConstWeakPtr;

  WheelEvent() : button_(0), shift_(0) {}
  WheelEvent(const Point& client_pos, int wheel_delta, int button, bool shift, bool ctrl) : 
       client_pos_(client_pos),
	   wheel_delta_(wheel_delta),        
       button_(button), 
       shift_(shift),
	   ctrl_(ctrl) {
  }
  WheelEvent(Window* sender,
             const Point& client_pos,
			 int wheel_delta,
             int button,
			 bool shift,
			 bool ctrl) : 
       sender_(sender),
       client_pos_(client_pos),
	   wheel_delta_(wheel_delta),      
       button_(button), 
       shift_(shift) {
  }
  const Point& client_pos() const { return client_pos_; }
  int wheel_delta() const { return wheel_delta_; }  
  int button() const { return button_; }
  bool shiftkey() const { return shift_; }
  bool ctrlkey() const { return ctrl_; }
  Window* sender() const { return sender_;  }

 private:  
  Window* sender_;
  Point client_pos_;
  int wheel_delta_, button_;
  bool shift_, ctrl_;
};

namespace KeyCodes {
enum Type { 
  CK_SHIFT = 4,
  CK_CONTROL = 8,
  VKDELETE = 46,
  CK_EXTENDED = 256
};  
}

class KeyEvent : public Event {
 public:  
  KeyEvent(int keycode, int flags) : keycode_(keycode), flags_(flags) {}

  int keycode() const { return keycode_; }
  int flags() const { return flags_; }
  bool shiftkey() const { return (KeyCodes::CK_SHIFT & flags_) == KeyCodes::CK_SHIFT; }
  bool ctrlkey() const { return (KeyCodes::CK_CONTROL & flags_) == KeyCodes::CK_CONTROL; }    
  bool extendedkey() const { return (KeyCodes::CK_EXTENDED & flags_) == KeyCodes::CK_EXTENDED; }

 private:
   int keycode_, flags_;
};

namespace FontStyle {
enum Type {
  NORMAL = 1,
  ITALIC = 2
};
}

class FontInfoImp;

struct FontInfo {  
  typedef boost::shared_ptr<FontInfo> Ptr;
  typedef boost::shared_ptr<const FontInfo> ConstPtr;
  typedef boost::weak_ptr<FontInfo> WeakPtr;
  typedef boost::weak_ptr<const FontInfo> ConstWeakPtr;

  FontInfo();
  FontInfo(const std::string& family);
  FontInfo(const std::string& family, int size);
  FontInfo(const std::string& family, int size, int weight, FontStyle::Type style);
  FontInfo(const FontInfo& font_info);  
  FontInfo(FontInfoImp* imp);
  FontInfo& operator= (const FontInfo& other);
  virtual ~FontInfo() {}

  FontInfoImp* imp() { return imp_.get(); }
  FontInfoImp* imp() const { return imp_.get(); } 
  
  void set_style(FontStyle::Type style);
  FontStyle::Type style() const;
  void set_size(int size);
  int size() const;
  void set_weight(int weight);
  int weight() const;
  void set_family(const std::string& family);
  std::string family() const;
  void set_stock_id(int id);
  int stock_id() const;
  std::string tostring() const {
    std::stringstream str;    
    str << "\"" << family() << "\", " << size() << ", " << static_cast<int>(style());
    return str.str();    
  }
  static FontInfo dummy_font_info;

 private:
   std::auto_ptr<FontInfoImp> imp_;    
};

class FontImp;

class Font {
 public:
  typedef boost::shared_ptr<Font> Ptr;
  typedef boost::shared_ptr<const Font> ConstPtr;
  typedef boost::weak_ptr<Font> WeakPtr;
  typedef boost::weak_ptr<const Font> ConstWeakPtr;

  Font();  
  Font(const FontInfo& font_info);  
  Font(const Font& other);
  Font& operator= (const Font& other);
  Font(FontImp* imp);
  virtual ~Font() {}
  
  ui::FontImp* imp() { return imp_.get(); }
  ui::FontImp* imp() const { return imp_.get(); }

  virtual void set_font_info(const FontInfo& info);
  virtual FontInfo font_info() const;
  static Font dummy_font;

 private:  
  std::auto_ptr<FontImp> imp_;
};

class FontImp {
 public:
  FontImp() {}
  virtual ~FontImp() = 0;
  virtual FontImp* DevClone() const = 0;

  virtual void dev_set_font_info(const FontInfo& info) = 0;
  virtual FontInfo dev_font_info() const = 0;
};

inline FontImp::~FontImp() {}

class FontInfoImp {
 public:
  FontInfoImp() {}
  virtual ~FontInfoImp() = 0;
  
  virtual FontInfoImp* DevClone() const = 0;  
  virtual void dev_set_style(FontStyle::Type style) = 0;
  virtual FontStyle::Type dev_style() const = 0;
  virtual void dev_set_size(int size) = 0;
  virtual int dev_size() const = 0;
  virtual void dev_set_weight(int weight) = 0;
  virtual int dev_weight() const = 0;
  virtual void dev_set_family(const std::string& family) = 0;
  virtual std::string dev_family() const = 0;
  virtual void dev_set_stock_id(int id) = 0;
  virtual int dev_stock_id() const = 0;    
};

inline FontInfoImp::~FontInfoImp() {}

class FontsImp;

class Fonts {
 public:
  typedef std::vector<std::string> Container;
  typedef std::vector<std::string>::iterator iterator;

  Fonts();
  virtual ~Fonts() {}

  virtual iterator begin() { return font_list_.begin(); }
  virtual iterator end() { return font_list_.end(); }
  virtual bool empty() const { return font_list_.empty(); }
  virtual int size() const { return font_list_.size(); }
      
  virtual void import_font(const std::string& path);

 private:
  std::auto_ptr<FontsImp> imp_;
  std::vector<std::string> font_list_;
};

class FontsImp {
 public:
  virtual ~FontsImp() {}
  virtual Fonts::Container dev_font_list() const = 0;
  virtual void dev_import_font(const std::string& path)  = 0;
};

class RegionImp {
 public:
  RegionImp() {}
  virtual ~RegionImp() = 0;
  virtual RegionImp* DevClone() const = 0;
  virtual void DevOffset(double dx, double dy) = 0;  
  virtual void DevUnion(const Region& other) = 0;
  virtual ui::Rect DevBounds() const = 0;
  virtual bool DevIntersect(double x, double y) const = 0;
  virtual bool DevIntersectRect(const ui::Rect& rect) const = 0;
  virtual void DevClear() = 0;
  virtual void DevSetRect(const ui::Rect& rect) = 0;  
};

inline RegionImp::~RegionImp() {}

class ImageImp;
class Graphics;

class Image {
 public:
  typedef boost::shared_ptr<Image> Ptr;
  typedef boost::weak_ptr<Image> WeakPtr;

  Image();
  Image(const Image& other);
  Image& operator=(const Image& other);
  virtual ~Image();

  ImageImp* imp() { return imp_.get(); }
  const ImageImp* imp() const { return imp_.get(); }

  virtual void Reset(const Dimension& dimension);
  virtual void Load(const std::string& filename);
  virtual void Save(const std::string& filename);
  virtual void Save();
  virtual void SetTransparent(bool on, ARGB color);
  virtual Dimension dim() const;  
  std::auto_ptr<Graphics> graphics();
  void Cut(const Rect& bounds);
  void SetPixel(const Point& pt, ARGB color);
  ARGB GetPixel(const Point& pt) const;
  virtual void Resize(const Dimension& dimension);
  virtual void Rotate(float radians);

 private:
  std::auto_ptr<ImageImp> imp_;	 
  std::string filename_;
  bool has_file_;
};

class ImageImp {
 public:  
  ImageImp() {}
  virtual ~ImageImp() = 0;

  virtual ImageImp* DevClone() const = 0;
  virtual void DevLoad(const std::string& filename) = 0;
  virtual void DevSave(const std::string& filename) = 0;
  virtual void DevSetTransparent(bool on, ARGB color) = 0;
  virtual void DevReset(const Dimension& dimension) = 0;	
  virtual Dimension dev_dim() const = 0;  
  virtual Graphics* dev_graphics() = 0;
  virtual void DevCut(const Rect& bounds) = 0;
  virtual void DevSetPixel(const Point& pt, ARGB color) = 0;
  virtual ARGB DevGetPixel(const Point& pt) const = 0;
  virtual void DevResize(const Dimension& dimension) = 0;
  virtual void DevRotate(float radians) = 0;
};

inline ImageImp::~ImageImp() { }

class Images {
 public:  
  typedef boost::shared_ptr<Images> Ptr;
  typedef boost::shared_ptr<const Images> ConstPtr;
  typedef boost::weak_ptr<Images> WeakPtr;
  typedef boost::weak_ptr<const Images> ConstWeakPtr;
  typedef std::vector<ui::Image::Ptr> Container;
  typedef Images::Container::iterator iterator;

  virtual iterator begin() { return images_.begin(); }
  virtual iterator end() { return images_.end(); }
  virtual bool empty() const { return images_.empty(); }
  virtual int size() const { return images_.size(); }
  
  Images() {}
  virtual ~Images() {}
    
  void Add(const Image::Ptr& image) { images_.push_back(image); }
  void Insert(iterator it, const Image::Ptr& item) { 
    images_.insert(it, item);
  }
  void Remove(iterator it) { images_.erase(it); }
  void RemoveAll() { images_.clear(); }

  private:
   Images::Container images_;
};

class GraphicsImp;

class Graphics {
friend class Group;
 public:
  typedef boost::shared_ptr<Graphics> Ptr;
  typedef boost::shared_ptr<const Graphics> ConstPtr;
  typedef boost::weak_ptr<Graphics> WeakPtr;
  typedef boost::weak_ptr<const Graphics> ConstWeakPtr;
  Graphics(); 
  //Graphics(bool); 
  Graphics(GraphicsImp* imp);
#ifdef _WIN32
  Graphics(CDC* cr);
#endif
  Graphics(const boost::shared_ptr<Image>& image);

  virtual ~Graphics() {}
  
  GraphicsImp* imp() { return imp_.get(); };
  GraphicsImp* imp() const { return imp_.get(); };

  virtual void CopyArea(const Rect& rect, const Point& delta);
  virtual void DrawArc(const Rect& rect, const Point& start, const Point& end);
  virtual void DrawLine(const Point& p1, const Point& p2);
  virtual void DrawCurve(const Point& p1, const Point& control_p1, const Point& control_p2, const Point& p2);
  virtual void DrawRect(const Rect& rect);
  virtual void DrawRoundRect(const Rect& rect, const Dimension& arc_dimension);
  virtual void DrawOval(const Rect& rect);
  virtual void DrawCircle(const Point& center, double radius);
  virtual void DrawEllipse(const Point& center, const Point& radius);
  virtual void DrawString(const std::string& str, const Point& position);
  virtual void FillRect(const Rect& rect);
  virtual void FillRoundRect(const Rect& rect, const Dimension& arc_dimension);
  virtual void FillOval(const Rect& rect);
  virtual void FillCircle(const Point& center, double radius);
  virtual void FillEllipse(const Point& center, const Point& radius);
  virtual void FillRegion(const Region& rgn);
  virtual void set_stroke(ARGB color);
  virtual void set_fill(ARGB color);
  virtual void set_color(ARGB color);
  virtual ARGB color() const;
  virtual void SetPenWidth(double width);
  virtual void Translate(const Point& delta);
  virtual void Retranslate();
  virtual void ClearTranslations();
  virtual void SetFont(const Font& font);
  virtual const Font& font() const;
  virtual Dimension text_dimension(const std::string& text) const;
  virtual void DrawPolygon(const Points& points);
  virtual void FillPolygon(const Points& points);
  virtual void DrawPolyline(const Points& points);
  virtual void DrawImage(Image* img, const Point& top_left);
  virtual void DrawImage(Image* img, const Rect& position);
  virtual void DrawImage(Image* img, const Rect& destination_position,
		                     const Point& src);
  virtual void DrawImage(Image* img, const Rect& destination_position,
		                     const Point& src, const Point& zoom_faktor);
  virtual void SetClip(const Rect& rect);
  virtual void SetClip(Region* rgn);
  // virtual CRgn& clip();
  virtual void Dispose();
	void AttachImage(Image* image);
  // virtual CDC* dc();  // just for testing right now
  void set_debug_flag();

  virtual void BeginPath();
  virtual void EndPath();
  virtual void FillPath();  
  virtual void DrawPath();
  virtual void DrawFillPath();
  virtual void MoveTo(const Point& p);
  virtual void LineTo(const Point& p);
  virtual void CurveTo(const Point& control_p1, const Point& control_p2, const Point& p);
  virtual void ArcTo(const Rect& rect, const Point& start, const Point& end);

 private:
   inline bool is_color_opaque() const { 
     int alpha = GetAlpha(color());
     bool result = (alpha != 0);
     return result;
   }
  std::auto_ptr<GraphicsImp> imp_;
  virtual void SaveOrigin();
  virtual void RestoreOrigin();

  std::stack<Point> translations_;
};

enum WindowMsg { ONWND, SHOW, HIDE, FOCUS };

namespace AlignStyle {
enum Type { ALNONE = 1, TOP, LEFT, RIGHT, BOTTOM, CLIENT, CENTER };
}

namespace JustifyStyle {
enum Type { LEFTJUSTIFY, RIGHTJUSTIFY, CENTERJUSTIFY };
}

class WindowImp;
class Ornament;
class Aligner;

class Commands {
 public:
   Commands();
   ~Commands() {}
   
   template<typename T>
   void Add(T& f) {     
     locker_.lock();
     functors.push_back(f);     
     locker_.unlock();
   }
   void Clear();
   void Invoke();      

   std::list<boost::function<bool(void)> > functors;  
   Lock locker_;
   bool invalid_;
   int addcount;
};

class AlertImp;

class AlertBox {
 public:	 
  AlertBox();
  void OpenMessageBox(const std::string& text);

 private:
  std::auto_ptr<AlertImp> imp_;
};

inline void alert(const std::string& text) {
  AlertBox alert_box;
  alert_box.OpenMessageBox(text);
}

class AlertImp {
 public:
  AlertImp() {}
	  
  virtual ~AlertImp() {}
  virtual void DevAlert(const std::string& text) = 0;
};

class ConfirmImp;

class ConfirmBox {
 public:	 
  ConfirmBox();
  bool OpenMessageBox(const std::string& text);

 private:
  std::auto_ptr<ConfirmImp> imp_;
};

inline bool confirm(const std::string& text) {
  ConfirmBox confirm_box;
  return confirm_box.OpenMessageBox(text);
}

class ConfirmImp {
 public:
  ConfirmImp() {}
  virtual ~ConfirmImp() {}
	  
  virtual bool DevConfirm(const std::string& text) = 0;
};

class FileDialogImp {
 public:	
  virtual ~FileDialogImp() {}
	  
  virtual void DevShow() = 0;
  virtual void dev_set_folder(const std::string& folder) = 0;
  virtual std::string dev_folder() const = 0;
  virtual void dev_set_filename(const std::string& filename) = 0;
  virtual std::string dev_filename() const = 0;
  virtual bool dev_is_ok() const = 0; 
};

class FileDialog {
 public:	
  FileDialogImp* imp() { return imp_.get(); }
  FileDialogImp* imp() const { return imp_.get(); }
  virtual ~FileDialog() {}
	  
  void Show();
  void set_folder(const std::string& folder);
  std::string folder() const;
  void set_filename(const std::string& filename);
  std::string filename() const;        
  bool is_ok() const;

 protected:
  std::auto_ptr<FileDialogImp> imp_;
};

class FileOpenDialog : public ui::FileDialog {
 public:	
  FileOpenDialog();
};

class FileSaveDialog : public ui::FileDialog {
 public:	
  FileSaveDialog();
};

class Window;

class WindowShowStrategy {
  public:
   WindowShowStrategy() {};
   virtual ~WindowShowStrategy() = 0;
   virtual void set_position(Window& window) = 0;
};

inline WindowShowStrategy::~WindowShowStrategy() {}

class ChildPosEvent;
class Window;

typedef boost::variant<boost::blank, ARGB, std::string, bool, FontInfo> MultiType;

class Stock {
 public:

  virtual ~Stock() {}
  virtual Stock* Clone() const { return new Stock(*this); }
  virtual MultiType value(int stock_key) const { 
    return MultiType(ARGB(0xFF000000));
  }
  virtual std::string key_tostring(int stock_key) const { return ""; }
};

class Property {
 public:     
  Property() : stock_(0), stock_key_(-1), inherit_(false) {}
  Property(MultiType value) : stock_(0), stock_key_(-1), value_(value), inherit_(false) {}
  Property(const Stock& stock) : stock_(stock.Clone()), stock_key_(-1), inherit_(false) {}
  Property(const Property& other) : 
       stock_((other.stock_) ? other.stock_->Clone() : 0),
       value_(other.value_), 
       stock_key_(other.stock_key_),
       inherit_(false) {       
  }
  Property& operator=(Property other) {    
    swap(*this, other);
    return *this;
  }    
  // Property(Property&& other) : Property() {
  //  swap(*this, other);
  // }  
  ~Property() { delete stock_; }
  friend void swap(Property& first, Property& second) {        
    using std::swap;        
    swap(first.value_, second.value_);
    swap(first.stock_key_, second.stock_key_);
    swap(first.stock_, second.stock_);
    swap(first.inherit_, second.inherit_);
  }

  MultiType value() const { 
    MultiType result;
    if (stock_ && stock_key_ != -1) {
      result = MultiType(stock_->value(stock_key_));
    } else {
      result = value_;
    }
    return result;
  }
  void set_value(const MultiType& value) { value_ = value; }
  void set_stock_key(int stock_key) { stock_key_ = stock_key; }
  int stock_key() const { return stock_key_; }
  
  ARGB argb_value() const { return boost::get<ARGB>(value()); }
  int int_value() const { return (int) boost::get<ARGB>(value()); }
  std::string string_value() const { return boost::get<std::string>(value()); }
  void EnableInherit() { inherit_ = true; }
  bool inherit() const { return inherit_; }
  bool is_blank() const { return value_.which() == 0; }

 private:
  Stock* stock_;
  MultiType value_;  
  int stock_key_;
  bool inherit_;
};

#ifdef BOOST_MSCV
#pragma warning(pop)
#endif

class Properties {
 public:
	typedef std::map<std::string, Property> Container;
	typedef Container::iterator iterator;
    typedef Container::const_iterator const_iterator;

	iterator begin() { return elements_.begin(); }
	iterator end() { return elements_.end(); }	
	const_iterator begin() const { return elements_.begin(); }
	const_iterator end() const { return elements_.end(); }	
	bool empty() const { return elements_.empty(); }
	int size() const { return elements_.size(); }

  void set(const std::string& key, const Property& prop) {
    elements_[key] = prop;
  }
 // private:
  const Container& elements() const { return elements_; }
  private:
   Container elements_;
};

class Command {
 public:
  typedef boost::shared_ptr<Command> Ptr;
  typedef boost::weak_ptr<Command> WeakPtr;  

  virtual ~Command() {}
  virtual void Execute() = 0; 
};

class InheritedProperties { 
 public:
  static InheritedProperties& instance() {
    static InheritedProperties the_instance;
    return the_instance;
  }
  
  bool is_inherited(const std::string& name) const;
 
 private:
  InheritedProperties();
  std::map<std::string, bool> inherited_;
};

class Rule {
 public:
  Rule() {}
  Rule(const std::string& selector) : selector_(selector) {}
  Rule(const std::string& selector, const Properties& properties) : 
      selector_(selector),
      properties_(properties) {
  }  
  ~Rule() {}

  const std::string& selector() const { return selector_; }
  Properties& properties() { return properties_; }
  const Properties& properties() const { return properties_; }
  void InheritFrom(const Rule& rule);
  void merge(const Rule& rule);

 private:
  std::string selector_;
  Properties properties_;  
};

class Rules {
  public:
   typedef std::map<std::string, Rule> RuleContainer;
   typedef RuleContainer::iterator iterator;
   typedef RuleContainer::const_iterator const_iterator;

   Rules() : owner_(0) {}
   void set_owner(Window* owner) { owner_ = owner; }

   iterator begin() { return rules_.begin(); }
   iterator end() { return rules_.end(); }	
   const_iterator begin() const { return rules_.begin(); }
   const_iterator end() const { return rules_.end(); }	
   bool empty() const { return rules_.empty(); }
   int size() const { return rules_.size(); }
   
   void add(Rule rule) { 
	 rules_[rule.selector()] = rule; 
     NeedsUpdate();
   }
   void ApplyTo(Window* window);
   void InheritFrom(const Rule& rule);   
   Rules& VirtualRules();
  // private:
   RuleContainer rules_;
   class Window* owner_;
   void NeedsUpdate() const { virtual_rules_cache_.reset(); }

  private:   
   mutable std::auto_ptr<Rules> virtual_rules_cache_;
};

class Window : public boost::enable_shared_from_this<Window> {
   friend class WindowImp;
   friend class Rules;
 public:
  typedef boost::shared_ptr<Window> Ptr;
  typedef boost::shared_ptr<const Window> ConstPtr;
  typedef boost::weak_ptr<Window> WeakPtr;
  typedef boost::weak_ptr<const Window> ConstWeakPtr;
  typedef std::vector<Window::Ptr> Container;
  typedef Window::Container::iterator iterator;
  typedef Window::Container::const_iterator const_iterator;
  typedef std::vector<boost::shared_ptr<Ornament> > Ornaments;

  Window();
  Window(WindowImp* imp);
  virtual ~Window();

  void set_imp(WindowImp* imp);
  virtual void release_imp() { imp_.release(); }
  WindowImp* imp() { return imp_.get(); };
  WindowImp* imp() const { return imp_.get(); };
  
  void set_command(const Command::Ptr& command) { command_ = command; }
  void ExecuteAction() { 
    if (!command_.expired()) {
      command_.lock()->Execute();
    }
  }

  void set_debug_text(const std::string& text) { debug_text_ = text; }
  const std::string& debug_text() const { return debug_text_; }

  static std::string type() { return "window"; }
  virtual std::string GetType() const { return "window"; }

  virtual iterator begin() { return dummy_list_.begin(); }
  virtual iterator end() { return dummy_list_.end(); }	
  virtual const_iterator begin() const { return dummy_list_.begin(); }
  virtual const_iterator end() const { return dummy_list_.end(); }	
  virtual bool empty() const { return true; }
  virtual int size() const { return 0; }
  bool has_childs() const { return size() != 0; }
  virtual Window* root();
  virtual const Window* root() const;
  virtual bool is_root() const { return 0; }
  virtual bool IsInGroup(Window::WeakPtr group) const;
  virtual bool IsInGroupVisible() const;
  virtual void set_parent(Window* parent);
  virtual Window* parent() { return parent_; }
  virtual Window* parent() const { return parent_; }
  template <class T, class T1>
  void PreOrderTreeTraverse(T& functor, T1& cond);
  template <class T, class T1>
  void PostOrderTreeTraverse(T& functor, T1& cond);
  Window::Container SubItems();
  virtual void Add(const Window::Ptr& window, bool apply_rules = true) {}
  virtual void Insert(iterator it, const Window::Ptr& window) {}
  virtual void Remove(const Window::Ptr& window) {}
  virtual void RemoveAll() {}
  virtual void set_position(const Point& pos);
  virtual void set_position(const Rect& pos);
  virtual void ScrollTo(int offsetx, int offsety);
  virtual Rect position() const;
  virtual Rect absolute_position() const;
  virtual Rect desktop_position() const;
  virtual bool check_position(const ui::Rect& pos) const;
  virtual Dimension dim() const;
  virtual Dimension min_dimension() const { return min_dimension_; }
  virtual void set_min_dimension(const Dimension& dimension) {
     min_dimension_ = dimension;
  }
  virtual ui::Dimension OnCalcAutoDimension() const { 
    return ui::Dimension(100, 20);
  }
  void UpdateAutoDimension();
  void PreventAutoDimension() { prevent_auto_dimension_ = true; }
  void RestoreAutoDimension() { prevent_auto_dimension_ = false; }
  void set_margin(const BoxSpace& margin);
  const BoxSpace& margin() const;
  void set_padding(const BoxSpace& padding);
  const BoxSpace& padding() const;
  void set_border_space(const BoxSpace& border_space);
  const BoxSpace& border_space() const;
  virtual void set_aligner(const boost::shared_ptr<Aligner>& aligner) {}
  virtual boost::shared_ptr<Aligner> aligner() const { return boost::shared_ptr<Aligner>(); }
  void set_align(AlignStyle::Type align) { align_ = align; }
  AlignStyle::Type align() const { return align_; }		
  virtual void UpdateAlign() {}
  virtual void Show();
  virtual void Show(const boost::shared_ptr<WindowShowStrategy>& aligner);
  virtual void BringToTop();
  virtual void Hide();
  virtual bool visible() const;
  virtual void FLS(); // invalidate new region
  virtual void FLSEX(); //  invalidate combine new & old region
  virtual void FLS(const Region& rgn) { Invalidate(rgn); }
  virtual void Invalidate();
  virtual void Invalidate(const Region& rgn);
  virtual void PreventFls();
  virtual void EnableFls();
  virtual bool fls_prevented() const;
  virtual void PreventDrawBackground();
  virtual void EnableDrawBackground();
  virtual bool draw_background_prevented() const;
  virtual bool IsSaving() const { return false; }
  virtual void SetCapture();
  virtual void ReleaseCapture();
  virtual void ShowCursor();
  virtual void HideCursor();
  virtual void SetCursorPosition(const Point& position);
  virtual Point CursorPosition() const;
  virtual void SetCursor(CursorStyle::Type style);
  CursorStyle::Type cursor() const { return CursorStyle::DEFAULT; }
  virtual void needsupdate();
  virtual const Area& area() const;
  virtual std::auto_ptr<Region> draw_region() { return std::auto_ptr<Region>(); }	
  virtual Window::Ptr HitTest(double x, double y) {
    return area().Intersect(x, y) ? shared_from_this() : Window::Ptr();
  }
  virtual void set_auto_size(bool auto_size_width, bool auto_size_height) {
    auto_size_width_ = auto_size_width;
    auto_size_height_ = auto_size_height;
  }
  virtual bool auto_size_width() const;
  virtual bool auto_size_height() const;
  bool auto_size() const { return auto_size_width() && auto_size_height(); }
  virtual void SetClip(const Rect& rect) {}
  virtual bool has_clip() const { return false; }
  const Region& clip() const;
  virtual void RemoveClip() {}
  virtual void Draw(Graphics* g, Region& draw_region) {}
  virtual void DrawBackground(Graphics* g, Region& draw_region);
  virtual void add_ornament(boost::shared_ptr<Ornament> ornament);
  virtual void RemoveOrnaments();
  virtual Ornaments ornaments();
  void set_clip_children();	
  void add_style(UINT flag);
  void remove_style(UINT flag);
  virtual void OnMessage(WindowMsg msg, int param = 0) {}
  virtual void OnSize(const Dimension& dimension) {}
  virtual void OnChildPosition(ChildPosEvent& ev) {}
  virtual void EnablePointerEvents() { pointer_events_ = true; }
  virtual void DisablePointerEvents() { pointer_events_ = false; }
  virtual bool pointerevents() const { return pointer_events_; }
  virtual void OnMouseDown(MouseEvent& ev) { 
     ev.WorkParent();
     MouseDown(ev);
  }
  virtual void OnMouseUp(MouseEvent& ev) { ev.WorkParent(); }
  virtual void OnDblclick(MouseEvent& ev) { ev.WorkParent(); }
  virtual void OnMouseMove(MouseEvent& ev) {
    ev.WorkParent();
    MouseMove(ev);
  }
  virtual void OnWheel(WheelEvent& ev) {
    ev.WorkParent();
    Wheel(ev);
  }
  virtual void OnMouseEnter(MouseEvent& ev) {
    ev.WorkParent();
    MouseEnter(ev);
  }
  virtual void OnMouseOut(MouseEvent& ev) {
    ev.WorkParent();
    MouseOut(ev);
  }
  virtual void OnShow() {}
  boost::signal<void(Window&)> BeforeDestruction;
  boost::signal<void(MouseEvent&)> MouseDown;
  boost::signal<void(MouseEvent&)> MouseUp;
  boost::signal<void(MouseEvent&)> DblClick;
  boost::signal<void(MouseEvent&)> MouseMove;
  boost::signal<void(MouseEvent&)> MouseEnter;
  boost::signal<void(MouseEvent&)> MouseOut;
  boost::signal<void(WheelEvent&)> Wheel;
  virtual void OnKeyDown(KeyEvent& ev) { ev.WorkParent(); }
  virtual void OnKeyUp(KeyEvent& ev) { ev.WorkParent(); }
  boost::signal<void(KeyEvent&)> KeyDown;
  boost::signal<void(KeyEvent&)> KeyUp;	
  virtual void SetFocus();
  virtual void OnFocus(Event& ev) { ev.WorkParent(); }
  virtual void OnKillFocus(Event& ev) { ev.WorkParent(); }
  virtual bool has_focus() const;
  boost::signal<void()> Focus;
  boost::signal<void()> KillFocus;	
  bool has_align_dimension_changed() const { return align_dimension_changed_; }
  void reset_align_dimension_changed() { align_dimension_changed_ = false; }	
  void align_dimension_changed() { align_dimension_changed_ = true; }  
  virtual void Enable();
  virtual void Disable();
  void ViewDoubleBuffered();
  void ViewSingleBuffered();
  void MapCapslockToCtrl();
  void EnableCapslock();
  bool is_double_buffered() const;
  virtual void set_properties(const Properties& properties) {}
	ui::Dimension non_content_dimension() const {
	  return ui::Dimension(padding().width() + border_space().width() +
                         margin().width(),
                         padding().height() + border_space().height() +
                         margin().height());
  }
  ui::Rect overall_position() const { return overall_position(position()); }
  ui::Rect overall_position(const ui::Rect& pos) const {			
     ui::Rect result(pos.top_left(),
			 ui::Dimension(pos.width() + padding().width() + border_space().width(), 
										pos.height() + padding().height() + border_space().height()));
     result.offset(margin().left(), margin().top());
     if (parent()) {
       result.offset(parent()->border_space().left() + parent()->padding().left(),
                           parent()->border_space().top() + parent()->padding().top());
     }
     return result;		
  }
  Window* FirstNonTransparentWindow() {
    Window* result = this;
    while (!result->is_root() && result->parent() && result->transparent()) {            
      result = result->parent();       
    }
    return result;
  } 
  virtual bool transparent() const;
  virtual void add_rule(const Rule& rule) {
    rules_.add(rule);
    for (iterator i = begin(); i != end(); ++i) {
      Window::Ptr window = *i;
      window->rules_.InheritFrom(rule);
      window->add_rule(rule);      
    }
  }  
  Rules& rules() { return rules_; } 
  void RefreshRules() { rules_.ApplyTo(this); }

 protected:  
  virtual void WorkMouseDown(MouseEvent& ev) { OnMouseDown(ev); }
  virtual void WorkMouseUp(MouseEvent& ev) { OnMouseUp(ev); }
  virtual void WorkMouseMove(MouseEvent& ev) { OnMouseMove(ev); }
  virtual void WorkMouseLeave(MouseEvent& ev) { OnMouseOut(ev); }
  virtual void WorkDblClick(MouseEvent& ev) { OnDblclick(ev); }
  virtual void WorkKeyUp(KeyEvent& ev) { OnKeyUp(ev); }
  virtual bool WorkKeyDown(KeyEvent& ev) { OnKeyDown(ev); return true; }  
  virtual void WorkFocus(Event& ev) { OnFocus(ev); }
  void WorkChildPosition();

  mutable bool update_;
  mutable std::auto_ptr<Area> area_;  
  mutable std::auto_ptr<Area> fls_area_;

 // private    
  Rules rules_;
 private:
  ui::BoxSpace sum_border_space() const;
  std::auto_ptr<WindowImp> imp_;
  Window* parent_;  
  Container dummy_list_;  
  std::string debug_text_;  
  bool auto_size_width_, auto_size_height_, prevent_auto_dimension_;
  Ornaments ornaments_;
  bool visible_, pointer_events_;  
  AlignStyle::Type align_;		
  bool align_dimension_changed_;
  ui::Dimension min_dimension_;
 Command::WeakPtr command_;
};

class ChildPosEvent : public Event {
 public:  
  ChildPosEvent(ui::Window& window) : window_(&window) {}

  Window* window() { return window_; }

 private:
  Window* window_;
};

class Group : public Window {
 public:  
  static std::string type() { return "group"; }  
  virtual std::string GetType() const { return "group"; }

  typedef boost::shared_ptr<Group> Ptr;
  typedef boost::shared_ptr<const Group> ConstPtr;
  typedef boost::weak_ptr<Group> WeakPtr;
  typedef boost::weak_ptr<const Group> ConstWeakPtr;  

  Group();  
  Group(WindowImp* imp);
  
  // structure  
  virtual Window::iterator begin() { return windows_.begin(); }
  virtual Window::iterator end() { return windows_.end(); }
  virtual Window::const_iterator begin() const { return windows_.begin(); }
  virtual Window::const_iterator end() const { return windows_.end(); }
  virtual bool empty() const { return windows_.empty(); }
  virtual int size() const { return windows_.size(); }
  virtual Window::Ptr at(int index) {
    Window::Ptr result;
    if (index >= 0 && index < windows_.size()) {
      result = *(windows_.begin() + index);
    } else {
      throw std::runtime_error("Index Out Of Bounds Error.");
    }
    return result;
  }  
  void Add(const Window::Ptr& window, bool apply_rules = true);
  void Insert(iterator it, const Window::Ptr& window);
  void Remove(const Window::Ptr& window);
  void RemoveAll() {        
    for (iterator it = windows_.begin(); it != windows_.end(); ++it) {
      (*it)->set_parent(0);
    }
    windows_.clear();
    FLS();
  }    
  // appearence
  void set_aligner(const boost::shared_ptr<Aligner>& aligner); 
  virtual Window::Ptr HitTest(double x, double y);
  void RaiseToTop(const Window::Ptr& window) {  Remove(window); Add(window); }
  void set_zorder(const Window::Ptr& window, int z);
  int zorder(const Window::Ptr& window) const;    
	virtual Dimension OnCalcAutoDimension() const;
  virtual void OnMessage(WindowMsg msg, int param = 0);    
  virtual void UpdateAlign();
  void FlagNotAligned();
  boost::shared_ptr<Aligner> aligner() const { return aligner_; }  
  virtual void OnChildPosition(ChildPosEvent& ev);
  
 protected:  
  Window::Container windows_;

 private:
  void Init();  
  boost::shared_ptr<Aligner> aligner_;
};

struct CalcDim { void operator()(Window& window) const; };
struct SetPos { bool operator()(Window& window) const; };
struct Visible { bool operator()(Window& window) const; };
struct AbortNone { bool operator()(Window& window) const { return false; }};
struct AbortDefault { bool operator()(Window& window) const { return !window.visible(); } };
struct AbortPos { bool operator()(Window& window) const; };
struct SetUnaligned { bool operator()(Window& window) const; };

class Aligner { 
 public:
  typedef boost::shared_ptr<Aligner> Ptr;
  typedef boost::weak_ptr<const Aligner> ConstPtr;
  typedef boost::weak_ptr<Aligner> WeakPtr;  
  
  Aligner();
  virtual ~Aligner() = 0;

  void Align() {
    static CalcDim calc_dim;
    static AbortPos pos_abort;
    static SetPos set_position;    
    static AbortDefault abort;        
    if (group_) {
      group_->PostOrderTreeTraverse(calc_dim, abort);
      group_->PreOrderTreeTraverse(set_position, pos_abort);
    }
  }
  virtual void CalcDimensions() = 0;
  virtual void SetPositions() = 0;
  virtual void set_group(Group& group) { group_ = &group; }  
  Group* group() { return group_; }
  const Group* group() const { return group_; }
  void set_dimension(const ui::Dimension& dim) { dim_ = dim; }
  const Dimension& dim() const { return dim_; }
  const Rect& position() const { return pos_; }   
  void set_aligned(bool aligned) { aligned_ = aligned; } 
  bool aligned() const { return aligned_; }
  bool full_align() const { return true; }
  void CachePosition(const Rect& pos) { pos_ = pos; }
     	
 protected:  
  typedef Window::Container::iterator iterator;
  iterator begin() { 
    return group_ ? group_->begin() : dummy.begin();
  }  
  iterator end() { 
    return group_ ? group_->end() : dummy.end();   
  }
  bool empty() const { 
    return group_ ? group_->empty() : false; 
  }
  int size() const { 
    return group_ ? group_->size() : 0;
  }  
       
  Rect pos_;
   
 private: 
  Group* group_;
  static Window::Container dummy;
  static bool full_align_;
  Dimension dim_;
  bool aligned_;
};

inline Aligner::~Aligner() {};

class FrameImp;

class FrameAligner : public WindowShowStrategy {
  public:   
   typedef boost::shared_ptr<FrameAligner> Ptr;
   typedef boost::shared_ptr<const FrameAligner> ConstPtr;
   typedef boost::weak_ptr<FrameAligner> WeakPtr;
   typedef boost::weak_ptr<const FrameAligner> ConstWeakPtr;

   FrameAligner() :
       width_perc_(-1),
       height_perc_(-1),
       alignment_(AlignStyle::CENTER) {
   }
   FrameAligner(AlignStyle::Type alignment) : 
       width_perc_(-1),
       height_perc_(-1),
       alignment_(alignment) {
   }
   virtual ~FrameAligner() {}
   virtual void set_position(Window& window);
   void SizeToScreen(double width_perc, double height_perc) {
     width_perc_ = width_perc;
     height_perc_ = height_perc;
   }

  private:    
    double width_perc_, height_perc_;
    AlignStyle::Type alignment_;
};

class WindowRightToScreen {
  public:   
   WindowRightToScreen() : width_perc_(-1), height_perc_(-1) {}   
   virtual ~WindowRightToScreen() {}
   virtual void set_position(Window& window);
   void SizeToScreen(double width_perc, double height_perc) {
     width_perc_ = width_perc;
     height_perc_ = height_perc;
   }

  private:    
    double width_perc_, height_perc_;
};

class PopupMenu;

class Frame : public Window {
 friend FrameImp;
 public:    
  typedef boost::shared_ptr<Frame> Ptr;
  typedef boost::shared_ptr<const Frame> ConstPtr;
  typedef boost::weak_ptr<Frame> WeakPtr;
  typedef boost::weak_ptr<const Frame> ConstWeakPtr;

  static std::string type() { return "frame"; }

  Frame();
  Frame(FrameImp* imp);  

  virtual void PreTranslateMessage(MSG* pMsg) {}

  FrameImp* imp() { return (FrameImp*) Window::imp(); };
  FrameImp* imp() const { return (FrameImp*) Window::imp(); };

  virtual void set_viewport(const Window::Ptr& viewport);
  ui::Window::Ptr viewport();
  virtual void set_title(const std::string& title);
  virtual std::string title() const;
  void set_popup_menu(const boost::shared_ptr<PopupMenu>& popup_menu) { 
    popup_menu_ = popup_menu;
  }
  boost::weak_ptr<PopupMenu> popup_menu() { return popup_menu_; }
  void SetMenuRootNode(const boost::shared_ptr<class Node>& root_node);
  virtual void ShowDecoration();
  virtual void HideDecoration();
  virtual void PreventResize();
  virtual void AllowResize();

  virtual void OnClose() {}
  virtual void OnShow() {}  
  virtual void OnContextPopup(Event&, const Point& mouse_point) {}
  virtual void WorkOnContextPopup(Event& ev, const Point& mouse_point);
  
  boost::signal<void (Frame&)> close;
	  
 private:  
  boost::weak_ptr<PopupMenu> popup_menu_;

};

class PopupFrameImp;

class PopupFrame : public Frame {
 public:
   PopupFrame();
   PopupFrame(FrameImp* imp);

   FrameImp* imp() { return (FrameImp*) Window::imp(); };
   FrameImp* imp() const { return (FrameImp*) Window::imp(); };

   static std::string type() { return "popupframe"; }  
};

class Ornament {
 public:
  typedef boost::shared_ptr<Ornament> Ptr;
  typedef boost::weak_ptr<Ornament> WeakPtr;

  Ornament() {}
  virtual ~Ornament() = 0;
  virtual Ornament* Clone() = 0;

  virtual bool transparent() const { return true; }
  virtual void Draw(Window& window, Graphics* g, Region& draw_region) = 0;
  virtual std::auto_ptr<Rect> padding() const { return std::auto_ptr<Rect>(); }
  virtual BoxSpace preferred_space() const { return BoxSpace(); }  
};

inline Ornament::~Ornament() {}

// Controls

class TreeView;

class TreeViewImp;
class ListViewImp;
class Node;
class NodeOwnerImp;

class NodeImp { 
 public: 
  NodeImp() : owner_(0) {}
  virtual ~NodeImp() {}

  virtual int position() const { return -1; }
  virtual void set_text(const std::string& text) {}

  void set_node(const boost::shared_ptr<Node>& node) { node_ = node; } 
  boost::weak_ptr<Node> node() { return node_; }
  void set_owner(NodeOwnerImp* owner) { owner_ = owner; }
  NodeOwnerImp* owner() { return owner_; }
  const NodeOwnerImp* owner() const { return owner_; }
  	
 private:
  NodeOwnerImp* owner_;	
  boost::weak_ptr<Node> node_;
};

class recursive_node_iterator;

class Node : public boost::enable_shared_from_this<Node> {
 public:
  static std::string type() { return "node"; }   
  typedef boost::shared_ptr<Node> Ptr;
  typedef boost::weak_ptr<Node> WeakPtr;
  typedef std::vector<Node::Ptr> Container;
  typedef Container::iterator iterator;
   
  Node() : image_index_(0), selected_image_index_(0), data_(0), parent_(0), selected_(false) {}
  Node(const std::string& text) : 
      text_(text),
      image_index_(0),
      selected_image_index_(0),
      data_(0),
      parent_(0) {
  }
  virtual ~Node() {}
	  
  void set_command(const Command::Ptr& command) { command_ = command; }
  Command::WeakPtr command() { return command_; }  
  void ExecuteAction() { 
    if (!command_.expired()) {
      command_.lock()->Execute();
    }
  }      
  virtual void set_text(const std::string& text) { 
    text_ = text;
    changed(*this);
  }  
  virtual const std::string& text() const { return text_; }
  virtual void set_name(const std::string& name) { name_ = name; }  
  virtual const std::string& name() const { return name_; }
  virtual void set_image(const Image::WeakPtr& image) { image_ = image; }
  virtual Image::WeakPtr image() { return image_; }

  virtual void set_image_index(int index) { image_index_ = index; }
  virtual int image_index() const { return image_index_; }
  virtual void set_selected_image_index(int index) { selected_image_index_ = index; }
  virtual int selected_image_index() const { return selected_image_index_; }
  virtual void select() { 
    selected_ = true;
    changed(*this);
  }
  virtual void deselect() { 
    selected_ = false;
    changed(*this);
  }
  virtual bool selected() const { 
    return selected_;
  }      
  iterator begin() { return children_.begin(); }
  iterator end() { return children_.end(); }
  bool empty() const { return children_.empty(); }
  int size() const { return children_.size(); }
  Ptr back() const { return children_.back(); }

  recursive_node_iterator recursive_begin();
  recursive_node_iterator recursive_end();
    
  int level() const { return parent() ? parent()->level() + 1 : 0; }    
  void AddImp(NodeImp* imp) { 
    imps.push_back(imp);    
  }
  void erase_imps(NodeOwnerImp* owner);
  void erase_imp(NodeOwnerImp* owner);

  void AddNode(const Node::Ptr& node);
  void insert(iterator it, const Node::Ptr& node);
  void erase(iterator it);
  void clear();

  template<class T>
  void traverse(T& func, Node::Ptr prev_node = Node::Ptr()) {
    func(this->shared_from_this(), prev_node);
    ui::Node::Ptr prev = Node::Ptr();
    for (iterator it = begin(); it != end(); ++it) {        
      (*it)->traverse(func, prev);
      prev = *it;
    }
  }
  void set_parent(Node* parent) { parent_ = parent; }
  Node* parent() const { return parent_; }
  boost::signal<void (Node&)> changed;
	boost::signal<void (Window&)> dimension_changed;
  boost::ptr_list<NodeImp> imps;
  NodeImp* imp(NodeOwnerImp& imp);
	void set_data(void* data) { data_ = data;  }
	void* data() { return data_; }
  const void* data() const { return data_; }

 private:
  std::string text_, name_;
  bool selected_; 
  Image::WeakPtr image_;
  int image_index_, selected_image_index_;
  Container children_;
  Node* parent_;
  void* data_;
  Command::WeakPtr command_;
};

class recursive_node_iterator {
 friend class Node;
 public:
  recursive_node_iterator(Node::iterator& it) { i.push(it); }

  bool operator==(const recursive_node_iterator& rhs) const {
    return i.size() == rhs.i.size() && i.top() == rhs.i.top();
  }
  inline bool operator!=(const recursive_node_iterator& rhs) const { 
    return !(*this == rhs);
  }
  Node::Ptr& operator*() { return *(i.top()); }
  recursive_node_iterator& operator++() {    
    Node* parent = (*i.top())->parent();
    if (i.top() != parent->end()) {
      if ((*i.top())->size() > 0) {        
        i.push((*i.top())->begin());        
      } else {
        ++i.top();        
        while (i.size() > 1 && i.top() == parent->end()) {
          i.pop();
          parent = (*i.top())->parent();
          ++i.top();                    
        }
      }
    } else 
    if (i.size() > 0) {
      i.pop();      
    }
    return *this;
  }
  recursive_node_iterator operator++(int) {
    recursive_node_iterator clone(*this);
    ++(*this);
    return clone;
  }
  int level() const { return i.size(); }

 private:  
  std::stack<Node::iterator> i;  
};

class NodeOwnerImp {
 public:
  virtual ~NodeOwnerImp() {}
  
  virtual void DevUpdate(const Node::Ptr&, const Node::Ptr& prev_node = Node::Ptr()) = 0;
  virtual void DevErase(const Node::Ptr& node)  = 0;
};

class MenuContainer;

class MenuContainerImp : public NodeOwnerImp {
 public:
  MenuContainerImp() : bar_(0) {}
  virtual ~MenuContainerImp() {}
  
  void set_menu_bar(MenuContainer* bar) { bar_ = bar; }
  MenuContainer* menu_bar() { return bar_; }  
  virtual void DevTrack(const Point& pos) = 0;
  virtual void DevInvalidate() = 0;
   
 private:
  MenuContainer* bar_;
};

class PopupMenu;

class MenuImp : public NodeImp {
  public:
    MenuImp() {}
    virtual ~MenuImp() {}

    virtual void dev_set_text(const std::string& text) = 0;
    virtual void dev_set_position(int pos)  = 0;
    virtual int dev_position() const = 0;
};

class MenuContainer {
 public:
  MenuContainer();
  MenuContainer(MenuContainerImp* imp);
  virtual ~MenuContainer() {}

  virtual MenuContainerImp* imp() { return imp_.get(); }
  virtual MenuContainerImp* imp() const { return imp_.get(); }

  void clear() { set_root_node(ui::Node::Ptr()); }
  virtual void Update();
  virtual void Invalidate();    
  void set_root_node(const Node::Ptr& root_node) {
		if (!root_node_.expired()) {
      root_node_.lock()->erase_imps(imp());
    }
    root_node_ = root_node;  
  }
  Node::WeakPtr root_node() { return root_node_; }
  virtual void OnMenuItemClick(boost::shared_ptr<Node> node) {}
	boost::signal<void (MenuContainer&, const Node::Ptr& node)> menu_item_click;

  static int id_counter;

 private:
  std::auto_ptr<ui::MenuContainerImp> imp_;
  Node::WeakPtr root_node_;
};

class PopupMenu : public MenuContainer {
 public:
  typedef boost::shared_ptr<PopupMenu> Ptr;
  typedef boost::shared_ptr<const PopupMenu> ConstPtr;
  typedef boost::weak_ptr<PopupMenu> WeakPtr;

  PopupMenu();

  virtual void Track(const Point& pos); 
};      

class TreeView : public Window {
 public:
  typedef boost::shared_ptr<TreeView> Ptr;
  typedef boost::shared_ptr<const TreeView> ConstPtr;
  typedef boost::weak_ptr<TreeView> WeakPtr;
  typedef boost::weak_ptr<const TreeView> ConstWeakPtr;
  
  static std::string type() { return "treeview"; }
  virtual std::string GetType() const { return "treeview"; }  

  TreeView();
  TreeView(TreeViewImp* imp);

  TreeViewImp* imp() { return (TreeViewImp*) Window::imp(); };
  TreeViewImp* imp() const { return (TreeViewImp*) Window::imp(); };      

  virtual void set_properties(const Properties& properties);
  void UpdateTree();
  void set_root_node(const Node::Ptr& root_node) { root_node_ = root_node; }
  boost::weak_ptr<Node> root_node() { return root_node_; }      
  void set_images(const Images::Ptr& images);
  boost::weak_ptr<Images> images() { return images_; }
  void set_popup_menu(const PopupMenu::Ptr& popup_menu) { 
    popup_menu_ = popup_menu;
  }
  void Clear();      
  virtual void set_background_color(ARGB color);
  virtual ARGB background_color() const;
  virtual void set_color(ARGB color);
  virtual ARGB color() const;
  virtual void EditNode(const Node::Ptr& node);
  bool is_editing() const;

  void ShowLines();
  void HideLines();
  void ShowButtons();
  void HideButtons();
	void ExpandAll();

  virtual void select_node(const Node::Ptr& node);
  virtual void deselect_node(const Node::Ptr& node);
  virtual Node::WeakPtr selected();

  virtual void OnChange(const Node::Ptr& node) {}
  virtual void OnRightClick(const Node::Ptr& node) {}
  virtual void OnEditing(const Node::Ptr& node, const std::string& text) {}
  virtual void OnEdited(const Node::Ptr& node, const std::string& text) {}  
  virtual void OnContextPopup(Event&, const Point& mouse_point, const Node::Ptr& node) {}
  virtual void WorkOnContextPopup(Event& ev, const Point& mouse_point, const Node::Ptr& node) {
    OnContextPopup(ev, mouse_point, node);
		context_popup(*this, ev, mouse_point, node);
    if (!ev.is_default_prevented() && !popup_menu_.expired()) {
      popup_menu_.lock()->Track(mouse_point);
    }
  }
	boost::signal<void (TreeView&, const Node::Ptr&, const std::string&)> edited;
	boost::signal<void (TreeView&, Event&, const Point&, const Node::Ptr&)> context_popup;
	boost::signal<void (TreeView&, const Node::Ptr& node)> change;
 private:     
  Node::WeakPtr root_node_;
  Images::WeakPtr images_;
  PopupMenu::WeakPtr popup_menu_;
};

class ListView : public Window {
 public:  
  static std::string type() { return "listview"; }
  virtual std::string GetType() const { return "listview"; }

  typedef boost::shared_ptr<ListView> Ptr;
  typedef boost::shared_ptr<const ListView> ConstPtr;
  typedef boost::weak_ptr<ListView> WeakPtr;
  typedef boost::weak_ptr<const ListView> ConstWeakPtr;

  ListView();
  ListView(ListViewImp* imp);

  ListViewImp* imp() { return (ListViewImp*) Window::imp(); };
  ListViewImp* imp() const { return (ListViewImp*) Window::imp(); };      

  virtual void set_properties(const Properties& properties);
  void UpdateList();
  void set_root_node(const Node::Ptr& root_node) { root_node_ = root_node; }
  void AddColumn(const std::string& text, int width);
  Node::WeakPtr root_node() { return root_node_; }      
  void set_images(const Images::Ptr& images);
  Images::WeakPtr images() { return images_; }
  void Clear();      
  virtual void set_background_color(ARGB color);
  virtual ARGB background_color() const;
  virtual void set_color(ARGB color);
  virtual ARGB color() const;
  virtual void EditNode(const Node::Ptr& node);
  bool is_editing() const;

  void ViewList();
  void ViewReport();  
  void ViewIcon();
  void ViewSmallIcon();

  void EnableRowSelect();
  void DisableRowSelect();

  void EnableDraw();
  void PreventDraw();
  
  virtual void select_node(const Node::Ptr& node);
  virtual void deselect_node(const Node::Ptr& node);
  virtual Node::WeakPtr selected();
  virtual std::vector<Node::Ptr> selected_nodes();
  int top_index() const;
  void EnsureVisible(int index);

  virtual void OnChange(const Node::Ptr& node) {}
  virtual void OnRightClick(const Node::Ptr& node) {}
  virtual void OnEditing(const Node::Ptr& node, const std::string& text) {}
  virtual void OnEdited(const Node::Ptr& node, const std::string& text) {}

  boost::signal<void (ListView&, const Node::Ptr& node)> change;

 private:     
  Node::WeakPtr root_node_;
  Images::WeakPtr images_;
};

class ScrollBarImp;

class ScrollBar : public Window {
 public:
  typedef boost::shared_ptr<ScrollBar> Ptr;
  typedef boost::shared_ptr<const ScrollBar> ConstPtr;
  typedef boost::weak_ptr<ScrollBar> WeakPtr;
  typedef boost::weak_ptr<const ScrollBar> ConstWeakPtr;

  static std::string type() { return "scrollbar"; }

  ScrollBar();
  ScrollBar(const Orientation::Type& orientation);
  ScrollBar(ScrollBarImp* imp);

  ScrollBarImp* imp() { return (ScrollBarImp*) Window::imp(); };
  ScrollBarImp* imp() const { return (ScrollBarImp*) Window::imp(); };

  virtual void OnScroll(int pos) { scroll(*this); }
  boost::signal<void (ScrollBar&)> scroll;

  void set_scroll_range(int minpos, int maxpos);
  void scroll_range(int& minpos, int& maxpos); 
  void set_scroll_position(int pos);
  int scroll_position() const;
  void inc_scroll_position() { set_scroll_position(scroll_position() + 1); }
  void dec_scroll_position() { set_scroll_position(scroll_position() - 1); }
  void system_size(int& width, int& height) const;  
};

class ScrollBox : public Group {
 public:   
   ScrollBox();
   static std::string type() { return "scrollbox"; }

   virtual void ScrollTo(const Point& top_left);
   virtual void ScrollBy(double dx, double dy);
   virtual void OnSize(const Dimension& dimension);
   virtual void Add(const Window::Ptr& window) { client_->Add(window); }
	 void UpdateScrollRange();

  private:
    void Init();
    void OnHScroll(ScrollBar&);
    void OnVScroll(ScrollBar&);		
    ScrollBar::Ptr hscrollbar_, vscrollbar_;
		Window::Ptr pane_;
    Window::Ptr client_;
    Ornament::Ptr client_background_;
};

class ComboBoxImp;

class ComboBox : public Window {
 public:
  static std::string type() { return "combobox"; }
  
  typedef boost::shared_ptr<ComboBox> Ptr;
  typedef boost::shared_ptr<const ComboBox> ConstPtr;
  typedef boost::weak_ptr<ComboBox> WeakPtr;
  typedef boost::weak_ptr<const ComboBox> ConstWeakPtr;
  typedef std::vector<std::string> Items;

  ComboBox();
  ComboBox(ComboBoxImp* imp);

  ComboBoxImp* imp() { return (ComboBoxImp*) Window::imp(); };
  ComboBoxImp* imp() const { return (ComboBoxImp*) Window::imp(); };

  virtual void add_item(const std::string& item);
  virtual void set_items(const std::vector<std::string>& itemlist);
  void set_item_by_text(const std::string& text) { 
    Items tmp = items();
    Items::iterator it = tmp.begin();
    for (int i = 0; it != tmp.end(); ++it, ++i) {
      if (*it == text) {
        set_item_index(i);
        break;
      }
    }
  }
  virtual Items items() const;
  void set_item_index(int index);
  int item_index() const;
  void set_text(const std::string& text);
  std::string text() const;
  virtual void OnSelect() {}
  void set_font(const Font& font);
  void Clear();
  boost::signal<void (ComboBox&)> select;
};

class EditImp;

namespace InputType {
enum Type { TEXT = 1, NUMBER = 2 };
}

class Edit : public Window {
 public:
  typedef boost::shared_ptr<Edit> Ptr;
  typedef boost::shared_ptr<const Edit> ConstPtr;
  typedef boost::weak_ptr<Edit> WeakPtr;
  typedef boost::weak_ptr<const Edit> ConstWeakPtr;

  static std::string type() { return "edit"; }
  virtual std::string GetType() const { return "edit"; }

  Edit();
  Edit(InputType::Type type);
  Edit(EditImp* imp);

  EditImp* imp() { return (EditImp*) Window::imp(); };
  EditImp* imp() const { return (EditImp*) Window::imp(); };

  virtual void set_properties(const Properties& properties);
  virtual void set_text(const std::string& text);
  virtual std::string text() const;
  virtual void set_background_color(ARGB color);
  virtual ARGB background_color() const;
  virtual void set_color(ARGB color);
  virtual ARGB color() const;
  virtual void set_font(const Font& font);
  virtual const Font& font() const;
  virtual void set_sel(int cp_min, int cp_max);
  void set_input_type(InputType::Type input_type);

  boost::signal<void (Edit&)> change;
};

class ButtonImp;

class Button : public Window {
 public:
  static std::string type() { return "button"; }

  typedef boost::shared_ptr<Button> Ptr;
  typedef boost::shared_ptr<const Button> ConstPtr;
  typedef boost::weak_ptr<Button> WeakPtr;
  typedef boost::weak_ptr<const Button> ConstWeakPtr;

  Button();
  Button(const std::string& text);
  Button(ButtonImp* imp);
  
  ButtonImp* imp() { return (ButtonImp*) Window::imp(); };
  ButtonImp* imp() const { return (ButtonImp*) Window::imp(); };
  virtual void set_text(const std::string& text);
  virtual std::string text() const;

  virtual void OnClick() {}  
	virtual void Check();
	virtual void UnCheck();
  
  void set_font(const Font& font);

  boost::signal<void (Button&)> click;
};

class CheckBoxImp;

class CheckBox : public Button {
 public:
  static std::string type() { return "checkbox"; }
  
  typedef boost::shared_ptr<CheckBox> Ptr;
  typedef boost::shared_ptr<const CheckBox> ConstPtr;
  typedef boost::weak_ptr<CheckBox> WeakPtr;
  typedef boost::weak_ptr<const CheckBox> ConstWeakPtr;

  CheckBox();
  CheckBox(const std::string& text);
  CheckBox(CheckBoxImp* imp);

  CheckBoxImp* imp() { return (CheckBoxImp*) Window::imp(); };
  CheckBoxImp* imp() const { return (CheckBoxImp*) Window::imp(); };

  virtual void set_background_color(ARGB color);
  virtual bool checked() const;
  
  void set_font(const Font& font);
 
  boost::signal<void (CheckBox&)> click;
};

class RadioButtonImp;

class RadioButton : public Button {
public:
	static std::string type() { return "radiobutton"; }  

	typedef boost::shared_ptr<RadioButton> Ptr;
	typedef boost::shared_ptr<const RadioButton> ConstPtr;
	typedef boost::weak_ptr<RadioButton> WeakPtr;
	typedef boost::weak_ptr<const RadioButton> ConstWeakPtr;

	RadioButton();
	RadioButton(const std::string& text);
	RadioButton(RadioButtonImp* imp);

	RadioButtonImp* imp() { return (RadioButtonImp*)Window::imp(); };
	RadioButtonImp* imp() const { return (RadioButtonImp*)Window::imp(); };

	virtual void set_background_color(ARGB color);
	virtual bool checked() const;

	void Check();
	void UnCheck();
  void set_font(const Font& font);

	boost::signal<void(RadioButton&)> click;
};

class GroupBoxImp;

class GroupBox : public Button {
public:
	static std::string type() { return "groupbox"; }  

	typedef boost::shared_ptr<GroupBox> Ptr;
	typedef boost::shared_ptr<const GroupBox> ConstPtr;
	typedef boost::weak_ptr<GroupBox> WeakPtr;
	typedef boost::weak_ptr<const GroupBox> ConstWeakPtr;

	GroupBox();
	GroupBox(const std::string& text);
	GroupBox(GroupBoxImp* imp);

	GroupBoxImp* imp() { return (GroupBoxImp*)Window::imp(); };
	GroupBoxImp* imp() const { return (GroupBoxImp*)Window::imp(); };

	virtual void set_background_color(ARGB color);
	virtual bool checked() const;

	void Check();
	void UnCheck();
  void set_font(const Font& font);

	boost::signal<void(GroupBox&)> click;
};

struct Lexer {
  Lexer() : 
      comment_color_(0),
      comment_line_color_(0),
      comment_doc_color_(0),
      identifier_color_(0),
      number_color_(0),
      string_color_(0),
      word_color_(0),       
      operator_color_(0),
      character_code_color_(0),
      preprocessor_color_(0) {    
  }

  void set_keywords(const std::string& keywords) { keywords_ = keywords; }
  const std::string& keywords() const { return keywords_; }
  void set_comment_color(ARGB color) { comment_color_ = color; }
  ARGB comment_color() const { return comment_color_; }
  void set_comment_line_color(ARGB color) { comment_line_color_ = color; }
  ARGB comment_line_color() const { return comment_line_color_; }
  void set_comment_doc_color(ARGB color) { comment_doc_color_ = color; }
  ARGB comment_doc_color() const { return comment_doc_color_; }
  void set_identifier_color(ARGB color) { identifier_color_ = color; }
  ARGB identifier_color() const { return  identifier_color_; }
  void set_number_color(ARGB color) { number_color_ = color; }
  ARGB number_color() const { return number_color_; }
  void set_word_color(ARGB color) { word_color_ = color; }
  ARGB word_color() const { return word_color_; }
  void set_string_color(ARGB color) { string_color_ = color; }
  ARGB string_color() const { return string_color_; }
  void set_operator_color(ARGB color) { operator_color_ = color; }
  ARGB operator_color() const { return operator_color_; }
  void set_character_code_color(ARGB color) { character_code_color_ = color; }
  ARGB character_code_color() const { return  character_code_color_; }
  void set_preprocessor_color(ARGB color) { preprocessor_color_ = color; }
  ARGB preprocessor_color() const { return  preprocessor_color_; }  
  
private:
  std::string keywords_;
  ARGB comment_color_, comment_line_color_, comment_doc_color_,
       identifier_color_, number_color_, string_color_, word_color_,       
       operator_color_, character_code_color_, preprocessor_color_;
};

class ScintillaImp;

class Scintilla : public Window {
 public:
  static std::string type() { return "scintilla"; }  
  virtual std::string GetType() const { return "scintilla"; }

  typedef boost::shared_ptr<Scintilla> Ptr;
  typedef boost::shared_ptr<const Scintilla> ConstPtr;
  typedef boost::weak_ptr<Scintilla> WeakPtr;
  typedef boost::weak_ptr<const Scintilla> ConstWeakPtr;

  Scintilla();
  Scintilla(ScintillaImp* imp);

  ScintillaImp* imp() { return (ScintillaImp*) Window::imp(); };
  ScintillaImp* imp() const { return (ScintillaImp*) Window::imp(); };  

  int f(int sci_cmd, void* lparam, void* wparam);
  void AddText(const std::string& text);
  void InsertText(const std::string& text, int pos);
  std::string text_range(int cpmin, int cpmax) const;
  void delete_text_range(int pos, int length);
  void FindText(const std::string& text,
                int caret_pos_min,
                int caret_pos_max,
                int& pos,
                int& caret_pos_selection_start,
                int& caret_pos_selection_end) const;
  void GotoLine(int line_pos);
  void GotoPos(int char_pos);
  void LineUp();
  void LineDown();
  void CharLeft();
  void CharRight();
  void WordLeft();
  void WordRight();
  void Cut();
  void Copy();
  void Paste();
  int length() const;
  int selectionstart() const;
  int selectionend() const;
  void SetSel(int cpmin, int cpmax);
  bool has_selection() const;
  void ReplaceSel(const std::string& text);
  void set_find_match_case(bool on);
  void set_find_whole_word(bool on);
  void set_find_regexp(bool on);
  void LoadFile(const std::string& filename);
  void Reload();
  void SaveFile(const std::string& filename);
  bool has_file() const;
  void set_lexer(const Lexer& lexer);
  void set_foreground_color(ARGB color);
  ARGB foreground_color() const;
  void set_background_color(ARGB color);
  ARGB background_color() const;
  void HideLineNumbers();
  void HideBreakpoints();
  void set_linenumber_foreground_color(ARGB color);
  ARGB linenumber_foreground_color() const;
  void set_linenumber_background_color(ARGB color);
  ARGB linenumber_background_color() const;  
  void set_folding_background_color(ARGB color);
  void set_folding_marker_colors(ARGB fore, ARGB back);
  void set_sel_foreground_color(ARGB color);
  //ARGB sel_foreground_color() const { return ToARGB(ctrl().sel_foreground_color()); }  
  void set_sel_background_color(ARGB color);
  //ARGB sel_background_color() const { return ToARGB(ctrl().sel_background_color()); }
  void set_sel_alpha(int alpha);
  void set_ident_color(ARGB color);
  void set_caret_color(ARGB color);
  ARGB caret_color() const;
  void StyleClearAll();
  const std::string& filename() const;
  bool is_modified() const;  
  void set_font_info(const FontInfo& font_info);
  const FontInfo& font_info() const;
  int column() const;    
  int line() const;
  int current_pos() const;
  bool over_type() const;
  bool modified() const;
  int add_marker(int line, int id);
  int delete_marker(int line, int id);
  void define_marker(int id, int symbol, ARGB foreground_color, ARGB background_color);
  void ShowCaretLine();
  void HideCaretLine();
  void HideHorScrollbar();
  void set_caret_line_background_color(ARGB color);
  void set_tab_width(int width_in_chars);
  int tab_width() const;
  void ClearAll();
  void Undo();
  void Redo();
  void EnableInput();
  void PreventInput();
  virtual bool transparent() const { return true; }  
  virtual void OnFirstModified() {}
  virtual void OnMarginClick(int line_pos) {}  
  virtual void set_properties(const Properties& properties);

 private:
  static std::string dummy_str_;
};

class GameController  { 
 public:
  GameController();

  void set_id(int id) { id_ = id; }
  int id() const { return id_; }
  void Prevent() { prevented_ = true; }
  void Enable() { prevented_ = false; }  
  bool prevented() const { return prevented_; }
  void set(int xpos, int ypos, int zpos, std::bitset<32> buttons) {
    xpos_ = xpos;
    ypos_ = ypos;
    zpos_ = zpos;
    buttons_ = buttons;
  }
  int xposition() const { return xpos_; }
  int yposition() const  { return ypos_; }
  int zposition() const { return zpos_; }  
  const std::bitset<32> buttons() const { return buttons_; }
  
  virtual void OnButtonDown(int button) {}
  virtual void OnButtonUp(int button) {}
  virtual void OnXAxis(int pos, int old_pos) {}
  virtual void OnYAxis(int pos, int old_pos) {}
  virtual void OnZAxis(int pos, int old_pos) {}
    
  void AfterUpdate(const GameController& old_state);

 private:  
  int id_;  
  int xpos_, ypos_, zpos_;
  bool prevented_;  
  std::bitset<32> buttons_;
};

class MenuContainer;
class Viewport;
class RadioGroup;
class RectangleBox;
class HeaderGroup;
class Line;
class Pic;
class Text;

// App
class AppImp {
  public:
   AppImp() {}
   virtual ~AppImp() {}
	   
   virtual void DevRun() = 0;
   virtual void DevStop() = 0;	   
};		
	
class App {
  public:
   App();
    
   AppImp* imp() { return imp_.get(); }
   AppImp* imp() const { return imp_.get(); }
   
   void Run();
   void Stop();
   void set_title(const std::string& title) { title_ = title; }
   const std::string& title() const { return title_; }
   
   boost::signal<void(MouseEvent&)> MouseDown;
  
  private:
   std::auto_ptr<AppImp> imp_;
   std::string title_;
};

// Ui Factory
class Systems {
 public:  
  typedef boost::shared_ptr<Systems> Ptr;
  typedef boost::shared_ptr<const Systems> ConstPtr;
  typedef boost::weak_ptr<Systems> WeakPtr;

  Systems() {}    
  Systems(Systems* concrete_factory) : concrete_factory_(concrete_factory) {}
  virtual ~Systems() {}
  static Systems& instance();  

  void set_concret_factory(Systems& concrete_factory);
   
  virtual Region* CreateRegion();
  virtual Graphics* CreateGraphics();
  // virtual Graphics* CreateGraphics(bool debug);
#ifdef _WIN32_
  virtual Graphics* CreateGraphics(CDC* dc);
#endif
  virtual Graphics* CreateGraphics(const Image::Ptr& image);
  virtual Image* CreateImage();
  virtual Font* CreateFont();  
  virtual Fonts* CreateFonts();
  virtual Window* CreateWin();
  virtual Frame* CreateFrame();
  virtual Frame* CreateMainFrame();
  virtual PopupFrame* CreatePopupFrame();
  virtual ComboBox* CreateComboBox();
  virtual Viewport* CreateViewport();
  virtual Group* CreateGroup();  
  virtual RadioButton* CreateRadioButton();
  virtual GroupBox* CreateGroupBox();
  virtual RectangleBox* CreateRectangleBox();
  virtual HeaderGroup* CreateHeaderGroup();
  virtual Edit* CreateEdit();  
  virtual Line* CreateLine();
  virtual Text* CreateText();
  virtual Pic* CreatePic();
  virtual ScrollBox* CreateScrollBox();
  virtual Scintilla* CreateScintilla();
  virtual Button* CreateButton();
  virtual CheckBox* CreateCheckBox();
  virtual ScrollBar* CreateScrollBar(Orientation::Type orientation = Orientation::VERT);
  virtual TreeView* CreateTreeView();
  virtual ListView* CreateListView();
  virtual PopupMenu* CreatePopupMenu(); 

  App* app() { return app_.get(); }
  SystemMetrics& metrics();
  void InitInstance(const std::string& config_path);
  void ExitInstance();
  std::string config_path() const { return config_path_; }
    	   
 private:     
  std::auto_ptr<Systems> concrete_factory_;
 std::auto_ptr<App> app_;
  std::string config_path_;  
};

class DefaultSystems : public Systems {
 public:  
  virtual Region* CreateRegion();
  virtual Graphics* CreateGraphics();
  //virtual Graphics* CreateGraphics(bool debug);
#ifdef _WIN32_
  virtual Graphics* CreateGraphics(CDC* dc);
#endif
  virtual Graphics* CreateGraphics(const Image::Ptr& image);
  virtual Image* CreateImage();
  virtual Font* CreateFont();  
  virtual Fonts* CreateFonts();
  virtual Window* CreateWin();
  virtual Frame* CreateFrame();
  virtual Frame* CreateMainFrame();
  virtual PopupFrame* CreatePopupFrame();
  virtual ComboBox* CreateComboBox();
  virtual Viewport* CreateViewport();
  virtual Group* CreateGroup();  
  virtual RadioButton* CreateRadioButton();
  virtual GroupBox* CreateGroupBox();
  virtual RectangleBox* CreateRectangleBox();
  virtual HeaderGroup* CreateHeaderGroup();
  virtual Edit* CreateEdit();  
  virtual Line* CreateLine();
  virtual Text* CreateText();
  virtual Pic* CreatePic();
  virtual ScrollBox* CreateScrollBox();
  virtual Scintilla* CreateScintilla();
  virtual Button* CreateButton();
  virtual CheckBox* CreateCheckBox();
  virtual ScrollBar* CreateScrollBar(Orientation::Type orientation = Orientation::VERT);
  virtual TreeView* CreateTreeView();
  virtual ListView* CreateListView();
  virtual PopupMenu* CreatePopupMenu();
};

// Imp Interfaces
class GraphicsImp {
 public:
  GraphicsImp() {}
  GraphicsImp(const boost::shared_ptr<Image>& image) {}
  virtual ~GraphicsImp() {}

  virtual void DevSetDebugFlag()  {} //= 0;
  virtual void DevCopyArea(const Rect& rect, const Point& delta)  {} //= 0;
  virtual void DevDrawArc(const Rect& rect, const Point& start, const Point& end)  {} //= 0;
  virtual void DevDrawLine(const Point& p1, const Point& p2)  {} //= 0;
  virtual void DevDrawCurve(const Point& p1, const Point& control_p1, const Point& control_p2, const Point& p2)  {} //= 0;
  virtual void DevDrawRect(const Rect& rect)  {} //= 0;
  virtual void DevDrawRoundRect(const Rect& position,
                                const Dimension& arc_diminesion)  {} //= 0;
  virtual void DevDrawOval(const Rect& position)  {} //= 0;
  virtual void DevDrawString(const std::string& str,
                             const Point& position)  = 0;
  virtual void DevFillRect(const Rect& rect)  {} //= 0;
  virtual void DevFillRoundRect(const Rect& position,
                                const Dimension& arc_dimension)  {} //= 0;
  virtual void DevFillOval(const Rect& position)  {} //= 0;
  virtual void DevFillRegion(const Region& rgn)  {} //= 0;
  virtual void DevSetStroke(ARGB color)  {} //= 0;
  virtual void DevSetFill(ARGB color)  {} //= 0;
  virtual void DevSetColor(ARGB color)  = 0;
  virtual ARGB dev_color() const = 0;
  virtual void DevTranslate(const Point& delta)  {} //= 0;  
  virtual void DevSetFont(const Font& font)  {} //= 0;	
  virtual const Font& dev_font() const = 0;
  virtual Dimension dev_text_dimension(const std::string& text) const  { return Dimension(100, 20); } //= 0;
	virtual void DevSetPenWidth(double width)  {} //= 0;
  virtual void DevDrawPolygon(const Points& points)  {} //= 0;
  virtual void DevFillPolygon(const Points& points)  {} //= 0;
  virtual void DevDrawPolyline(const Points& points)  {} //= 0;	
  virtual void DevDrawImage(Image* image, const Point& top_left) {
    if (image) {
      DevDrawImage(image, Rect(top_left, image->dim()), Point());
    }
  }
  virtual void DevDrawImage(Image* image, const Rect& position) {
    if (image) {
      DevDrawImage(image, position, Point());
    }
  }
  virtual void DevDrawImage(Image* img, const Rect& destination_position,
		                        const Point& src)  {} //= 0;
  virtual void DevDrawImage(Image* img,
		                        const Rect& destination_position,
		                        const Point& src,
		                        const Point& zoom_factor)  {} //= 0;
  virtual void DevSetClip(const Rect& rect)  {} //= 0;
  virtual void DevSetClip(Region* rgn)  {} //= 0;
  // virtual CRgn& dev_clip() = 0;
  virtual void DevDispose()  {} //= 0;
  // virtual CDC* dev_dc() = 0;
  virtual void DevSaveOrigin()  {} //= 0;
  virtual void DevRestoreOrigin()  {} //= 0;
  virtual void DevAttachImage(Image* image)  {} //= 0;
  virtual void DevBeginPath()  {} //= 0;
  virtual void DevEndPath()  {} //= 0;
  virtual void DevFillPath()  {} //= 0;
  virtual void DevDrawPath()  {} //= 0;
  virtual void DevDrawFillPath()  {} //= 0;
  virtual void DevLineTo(const Point& point)  {} //= 0;
  virtual void DevMoveTo(const Point& point)  {} //= 0;
  virtual void DevCurveTo(const Point& control_p1, const Point& control_p2, const Point& p)  {} //= 0;
  virtual void DevArcTo(const Rect& rect, const Point& start, const Point& end)  {} //= 0;
};

class WindowImp {
 public:
  WindowImp() : window_(0) {}
  WindowImp(Window* window) : window_(window) {}
  virtual ~WindowImp() {}

  void set_window(Window* window) { window_ = window; }
  Window* window() { return window_; }
  Window* window() const { return window_; }
  virtual ui::Graphics* DevGC() { return 0; }
  virtual void DevSwapBuffer() {};
  virtual void dev_set_position(const Rect& pos)  = 0;  
  virtual Rect dev_position() const = 0;
  virtual Rect dev_absolute_position() const { return Rect(); } //= 0;
  virtual Rect dev_absolute_system_position() const  { return Rect(); } //= 0;  
  virtual Rect dev_desktop_position() const  { return Rect(); } //= 0;
  virtual Dimension dev_dim() const { return Dimension(); } //= 0;
  virtual bool dev_check_position(const Rect& pos) const  { return false; } //= 0;
  virtual void dev_set_margin(const BoxSpace& margin)  {} //= 0;
  virtual const BoxSpace& dev_margin() const = 0;
  virtual void dev_set_padding(const BoxSpace& padding) = 0;
  virtual const BoxSpace& dev_padding() const = 0;
  virtual void dev_set_border_space(const BoxSpace& border_space)  {} //= 0;
  virtual const BoxSpace& dev_border_space() const = 0;
  virtual void DevScrollTo(int offsetx, int offsety)  {} //= 0;
  virtual void DevShow() = 0;
  virtual void DevHide() = 0;
  virtual bool dev_visible() const  { return false; } //= 0;
  virtual void DevBringToTop()  {} //= 0;
  virtual void DevInvalidate()  {} //= 0;
  virtual void DevInvalidate(const Region& rgn)  {} //= 0;  
  virtual void DevSetCapture() = 0;
  virtual void DevReleaseCapture() = 0;
  virtual void DevShowCursor() = 0;
  virtual void DevHideCursor() = 0;
  virtual void DevSetCursorPosition(const Point& position) = 0;
  virtual Point DevCursorPosition() const = 0;
  virtual void DevSetCursor(CursorStyle::Type style) {}  
  virtual void dev_set_parent(Window* window) {} 
  virtual void dev_set_clip_children() {}  
  virtual void DevSetFocus() {} // = 0;
  virtual bool dev_has_focus() const { return false; } // = 0;
  virtual void OnDevDraw(Graphics* g, Region& draw_region);  
  virtual void OnDevSize(const Dimension& dimension);  
  virtual void dev_add_style(UINT flag) {}
  virtual void dev_remove_style(UINT flag) {}
  virtual void DevEnable()  {} //= 0;
  virtual void DevDisable()  {} //= 0;
  virtual void DevViewDoubleBuffered()  {} //= 0;
  virtual void DevViewSingleBuffered()  {} //= 0;
  virtual bool dev_is_double_buffered() const { return false; }
  virtual void DevMapCapslockToCtrl()  {} //= 0;
  virtual void DevEnableCapslock()  {} //= 0;
  virtual void OnDevMouseDown(MouseEvent& ev);
  virtual void OnDevMouseUp(MouseEvent& ev);
  virtual void OnDevMouseMove(MouseEvent& ev);  
  virtual void OnDevMouseEnter(MouseEvent& ev);
  virtual void OnDevMouseOut(MouseEvent& ev);
  virtual void OnDevWheel(WheelEvent& ev);
  
 private:
  Window* window_;
};

class FrameImp : public WindowImp {
 public:  
  FrameImp() {}
  FrameImp(Window* window) : WindowImp(window) {}

  virtual void dev_set_title(const std::string& title) = 0;
  virtual std::string dev_title() const { return ""; } //= 0;
  virtual void dev_set_viewport(const Window::Ptr& viewport)  = 0;
  virtual Window::Ptr& dev_viewport() = 0;
  virtual void DevShowDecoration()  {} //= 0;
  virtual void DevHideDecoration()  {} //= 0;
  virtual void DevPreventResize()  {} //= 0;
  virtual void DevAllowResize()  {} //= 0;
  virtual void DevSetMenuRootNode(const Node::Ptr& root_node) {}

  virtual void OnDevClose();
  bool DevIsValid() const;
};

class TreeViewImp : public WindowImp, public NodeOwnerImp {
 public:  
  TreeViewImp() : WindowImp() {}
  TreeViewImp(Window* window) : WindowImp(window) {}
      
  virtual void dev_set_background_color(ARGB color) = 0;
  virtual ARGB dev_background_color() const = 0;  
  virtual void dev_set_color(ARGB color) = 0;
  virtual ARGB dev_color() const = 0;
  virtual void DevClear() = 0;  
  virtual void dev_select_node(const Node::Ptr& node) = 0;
  virtual void dev_deselect_node(const Node::Ptr& node) = 0;
  virtual Node::WeakPtr dev_selected() = 0;
  virtual void DevEditNode(const Node::Ptr& node) = 0;
  virtual bool dev_is_editing() const = 0;
  virtual void DevShowLines() = 0;
  virtual void DevHideLines() = 0;
  virtual void DevShowButtons() = 0;
  virtual void DevHideButtons() = 0;
  virtual void DevExpandAll() = 0;
  virtual void dev_set_images(const Images::Ptr& images)  {} //= 0;
};

class ListViewImp : public WindowImp, public NodeOwnerImp {
 public:  
  ListViewImp() : WindowImp() {}
  ListViewImp(Window* window) : WindowImp(window) {}
      
  virtual void dev_set_background_color(ARGB color) = 0;
  virtual ARGB dev_background_color() const = 0;  
  virtual void dev_set_color(ARGB color) = 0;
  virtual ARGB dev_color() const  = 0;
  virtual void DevClear() = 0;  
  virtual void dev_select_node(const Node::Ptr& node) = 0;
  virtual void dev_deselect_node(const Node::Ptr& node) = 0;
  virtual std::vector<Node::Ptr> dev_selected_nodes() = 0;
  virtual Node::WeakPtr dev_selected() = 0;
  virtual void DevEditNode(const Node::Ptr& node) = 0;
  virtual bool dev_is_editing() const = 0;
  virtual void DevAddColumn(const std::string& text, int width) = 0;
  virtual void DevViewList() = 0;
  virtual void DevViewReport() = 0;
  virtual void DevViewIcon() = 0;
  virtual void DevViewSmallIcon() = 0;
  virtual void DevEnableRowSelect() = 0;  
  virtual void DevDisableRowSelect() = 0;
  virtual void dev_set_images(const Images::Ptr& images) = 0;
  virtual int dev_top_index() const = 0;
  virtual void DevEnsureVisible(int index) = 0;
  virtual void DevEnableDraw() = 0;
  virtual void DevPreventDraw() = 0;
};

class ScrollBarImp : public WindowImp {
 public:
  ScrollBarImp() : WindowImp() {}
  ScrollBarImp(Window* window) : WindowImp(window) {}

  virtual void OnScroll(int pos) {}  
  virtual void dev_set_scroll_range(int minpos, int maxpos) = 0;
  virtual void dev_scroll_range(int& minpos, int& maxpos) = 0;   
  virtual void dev_set_scroll_position(int pos) = 0;
  virtual int dev_scroll_position() const = 0;
  virtual Dimension dev_system_size() const = 0;

  virtual void OnDevScroll(int pos);
};

class ComboBoxImp : public WindowImp {
 public:
  ComboBoxImp() : WindowImp() {}
  ComboBoxImp(Window* window) : WindowImp(window) {}

  virtual void dev_add_item(const std::string& item)  {} //= 0;
  virtual std::vector<std::string> dev_items() const = 0;
  virtual void dev_set_items(const std::vector<std::string>& itemlist) {} // = 0;
  virtual void dev_set_item_index(int index)  {} //= 0;
  virtual int dev_item_index() const { return 0; } // = 0;
  virtual void dev_set_text(const std::string& text) {} // = 0;
  virtual std::string dev_text() const { return ""; } // = 0;
  virtual void dev_set_font(const Font& font) {} // = 0;
  virtual const Font& dev_font() const = 0;
  virtual void dev_clear() {} // = 0;
};

class ButtonImp : public WindowImp {
 public:
  ButtonImp() : WindowImp() {}
  ButtonImp(Window* window) : WindowImp(window) {}

  virtual void dev_set_text(const std::string& text) = 0;
  virtual std::string dev_text() const = 0;
  
  virtual void DevCheck()  = 0;
  virtual void DevUnCheck() = 0;
  virtual bool dev_checked() const = 0;
  virtual void dev_set_font(const Font& font) = 0;
  virtual const Font& dev_font() const = 0;
  virtual void OnDevClick();  
};

class CheckBoxImp : public ui::ButtonImp {
 public:
  CheckBoxImp() : ButtonImp() {}  
  CheckBoxImp(Window* window) : ButtonImp(window) {}

  virtual void dev_set_background_color(ARGB color)  {} //= 0;
  virtual void OnDevClick();
};

class RadioButtonImp : public ui::ButtonImp {
 public:
  RadioButtonImp() : ButtonImp() {}
  RadioButtonImp(Window* window) : ButtonImp(window) {}

  virtual void dev_set_background_color(ARGB color) {} // = 0;
  virtual bool dev_checked() const { return false; } // = 0;
  virtual void DevCheck()  {} //= 0;
  virtual void DevUnCheck() {} // = 0;
  virtual void dev_set_font(const Font& font) {} // = 0;
  virtual const Font& dev_font() const = 0;
};

class GroupBoxImp : public ui::ButtonImp {
  public:
   GroupBoxImp() : ButtonImp() {}
   GroupBoxImp(Window* window) : ButtonImp(window) {}

   virtual void dev_set_background_color(ARGB color) {} // = 0;
   virtual bool dev_checked() const { return false; } // = 0;
   virtual void DevCheck() {} // = 0;
   virtual void DevUnCheck() {} // = 0;
};

class EditImp : public WindowImp {
 public:
  EditImp() : WindowImp() {}
  EditImp(Window* window) : WindowImp(window) {}

  virtual void dev_set_text(const std::string& text) {} //;
  virtual std::string dev_text() const { return ""; } //= 0;
  virtual void dev_set_background_color(ARGB color) {} // = 0;
  virtual ARGB dev_background_color() const { return 0; } // = 0;  
  virtual void dev_set_color(ARGB color) {} // = 0;
  virtual ARGB dev_color() const { return 0; } // = 0;
  virtual void dev_set_font(const Font& font) {} // = 0;
  virtual const Font& dev_font() const = 0;
  virtual void dev_set_sel(int cpmin, int cpmax) {} // = 0;
  
  virtual void OnDevChange();
};

class ScintillaImp : public WindowImp {
 public: 
  ScintillaImp() : WindowImp() {}
  ScintillaImp(Window* window) : WindowImp(window) {}
    
  virtual int dev_f(int sci_cmd, void* lparam, void* wparam) { return 0; }
  virtual void DevAddText(const std::string& text) {}
  virtual void DevInsertText(const std::string& text, int pos) = 0;
  virtual std::string dev_text_range(int cpmin, int cpmax) const = 0;
  virtual void dev_delete_text_range(int pos, int length) = 0;
  virtual void DevClearAll() = 0;
  virtual void DevFindText(const std::string& text, int cpmin, int cpmax, int& pos, int& cpselstart, int& cpselend) const {}
  virtual void DevGotoLine(int line_pos) = 0;
  virtual void DevGotoPos(int char_pos) = 0;
  virtual void DevLineUp() = 0;
  virtual void DevLineDown() = 0;
  virtual void DevCharLeft() = 0;
  virtual void DevCharRight() = 0;
  virtual void DevWordLeft() = 0;
  virtual void DevWordRight() = 0;
  virtual void DevCopy() = 0;
  virtual void DevCut() = 0;
  virtual void DevPaste() = 0;
  virtual int dev_length() const { return 0; }
  virtual int dev_selectionstart() const = 0;
  virtual int dev_selectionend() const = 0;
  virtual void DevSetSel(int cpmin, int cpmax) {}
  virtual bool dev_has_selection() const = 0;
  virtual void DevReplaceSel(const std::string& text) = 0; 
  virtual void dev_set_find_match_case(bool on) {}
  virtual void dev_set_find_whole_word(bool on) {}
  virtual void dev_set_find_regexp(bool on) {}
  virtual void DevLoadFile(const std::string& filename) {}
  virtual void DevReload() {}
  virtual void DevSaveFile(const std::string& filename) {}
  virtual bool dev_has_file() const { return false; }
  virtual void dev_set_lexer(const Lexer& lexer) {}
  virtual void dev_set_foreground_color(ARGB color) {}
  virtual ARGB dev_foreground_color() const { return 0; }
  virtual void dev_set_background_color(ARGB color) {}
  virtual ARGB dev_background_color() const { return 0; }
  virtual void dev_set_linenumber_foreground_color(ARGB color) {}
  virtual ARGB dev_linenumber_foreground_color() const { return 0; }
  virtual void dev_set_linenumber_background_color(ARGB color) {}
  virtual ARGB dev_linenumber_background_color() const { return 0; }  
  virtual void dev_set_folding_background_color(ARGB color) {}
  virtual void DevSetFoldingMarkerColors(ARGB fore, ARGB back) {}
  virtual void dev_set_sel_foreground_color(ARGB color) {}
  //ARGB sel_foreground_color() const { return ToARGB(ctrl().sel_foreground_color()); }  
  virtual void dev_set_sel_background_color(ARGB color) {}
  //ARGB sel_background_color() const { return ToARGB(ctrl().sel_background_color()); }
  virtual void dev_set_sel_alpha(int alpha) {}
  virtual void dev_set_ident_color(ARGB color) {}
  virtual void dev_set_caret_color(ARGB color) {}
  virtual ARGB dev_caret_color() const { return 0; }
  virtual void DevStyleClearAll() {}
  virtual const std::string& dev_filename() const = 0;  
  virtual void dev_set_font_info(const FontInfo& font_info) = 0;
  virtual const FontInfo& dev_font_info() const = 0;
  virtual int dev_column() const = 0;    
  virtual int dev_line() const = 0;
  virtual int dev_current_pos() const = 0;
  virtual bool dev_over_type() const = 0;
  virtual bool dev_modified() const = 0;
  virtual int dev_add_marker(int line, int id) = 0;
  virtual int dev_delete_marker(int line, int id) = 0;
  virtual void dev_define_marker(int id, int symbol, ARGB foreground_color, ARGB background_color) = 0;
  virtual void DevShowCaretLine() = 0;
  virtual void DevHideCaretLine() = 0;
  virtual void DevHideLineNumbers() = 0;
  virtual void DevHideBreakpoints() = 0;
  virtual void DevHideHorScrollbar() = 0;
  virtual void dev_set_caret_line_background_color(ARGB color)  = 0;
  virtual void dev_set_tab_width(int width_in_chars) = 0;
  virtual int dev_tab_width() const = 0;
  virtual void dev_undo() = 0;
  virtual void dev_redo() = 0;
  virtual void dev_enable_input() = 0;
  virtual void dev_prevent_input() = 0;
};

enum FileAccess {  
  CREATE = 1,
};  
  
class FileObserverImp;

class FileObserver {
 public:
  FileObserver();
  virtual ~FileObserver() {}

  FileObserverImp* imp() { return imp_.get(); }
  FileObserverImp* imp() const { return imp_.get(); }

  virtual void StartWatching();
  virtual void StopWatching();
  virtual void SetDirectory(const std::string& path);
  virtual std::string directory() const;

  virtual void OnCreateFile(const std::string& path) {}
  virtual void OnDeleteFile(const std::string& path) {}
  virtual void OnChangeFile(const std::string& path) {}

 private:      
  std::auto_ptr<FileObserverImp> imp_;
};

class FileObserverImp {
 public:
  FileObserverImp() : file_observer_(0) {}
  FileObserverImp(FileObserver* file_observer) : file_observer_(file_observer) {}
  virtual ~FileObserverImp() {}
 
  virtual void DevStartWatching() = 0;
  virtual void DevStopWatching() = 0;
  virtual void DevSetDirectory(const std::string& path) = 0;
  virtual std::string dev_directory() const = 0;
  
 private:
   FileObserver* file_observer_;
};

class GameControllersImp {
 public:
   GameControllersImp() {}
   virtual ~GameControllersImp() = 0;

   virtual int dev_size() const = 0;
   virtual void DevScanPluggedControllers(std::vector<int>& plugged_controller_ids) = 0;   
   virtual void DevUpdateController(GameController& controller) = 0;
};

class MenuContainerImp;
class PopupMenuImp;
class MenuItemImp;

class ImpFactory {
 public:  
  ImpFactory() {}
  ImpFactory(ImpFactory const&) {}             
  ImpFactory& operator=(ImpFactory const&) {}
  virtual ~ImpFactory() {}
	  
  static ImpFactory& instance();

  void set_concret_factory(ImpFactory& concrete_factory);
	  
  virtual AppImp* CreateAppImp();	  
  virtual ui::AlertImp* CreateAlertImp();
  virtual ui::FileDialogImp* CreateFileOpenDialogImp();
  virtual ui::FileDialogImp* CreateFileSaveDialogImp();  
  virtual ui::ConfirmImp* CreateConfirmImp();
  virtual ui::WindowImp* CreateWindowImp();
  virtual bool DestroyWindowImp(ui::WindowImp* imp);
  virtual ui::WindowImp* CreateWindowCompositedImp();
  virtual ui::FrameImp* CreateFrameImp() ;
  virtual ui::FrameImp* CreateMainFrameImp();
  virtual ui::FrameImp* CreatePopupFrameImp();
  virtual ui::ScrollBarImp* CreateScrollBarImp(Orientation::Type orientation);
  virtual ui::ComboBoxImp* CreateComboBoxImp();
  virtual ui::EditImp* CreateEditImp();
  virtual ui::EditImp* CreateNumberEditImp();
  virtual ui::TreeViewImp* CreateTreeViewImp();
  virtual ui::ListViewImp* CreateListViewImp();
  virtual ui::MenuContainerImp* CreateMenuContainerImp();
  virtual ui::MenuContainerImp* CreatePopupMenuImp();
  virtual ui::MenuImp* CreateMenuImp();
  virtual ui::ButtonImp* CreateButtonImp();
  virtual ui::CheckBoxImp* CreateCheckBoxImp();
  virtual ui::RadioButtonImp* CreateRadioButtonImp();
  virtual ui::GroupBoxImp* CreateGroupBoxImp();
  virtual ui::ScintillaImp* CreateScintillaImp();
  virtual ui::RegionImp* CreateRegionImp();
  virtual ui::FontsImp* CreateFontsImp();
  virtual ui::FontInfoImp* CreateFontInfoImp();
  virtual ui::FontImp* CreateFontImp();
  virtual ui::FontImp* CreateFontImp(int stockid);
  // virtual ui::GraphicsImp* CreateGraphicsImp(bool debug);
  virtual ui::GraphicsImp* CreateGraphicsImp();
  virtual ui::GraphicsImp* CreateGraphicsImp(const boost::shared_ptr<Image>& image);
#ifdef _WIN32
  virtual ui::GraphicsImp* CreateGraphicsImp(CDC* cr);
#endif
  virtual ui::ImageImp* CreateImageImp();
  virtual ui::GameControllersImp* CreateGameControllersImp();
  virtual ui::FileObserverImp* CreateFileObserverImp(FileObserver* file_observer);  
  virtual LockIF* CreateLocker();

 private:
  std::auto_ptr<ImpFactory> concrete_factory_;
};

inline GameControllersImp::~GameControllersImp() {}

template <class T>
class GameControllers : public psycle::host::Timer {
 public:
   GameControllers() : prevented_(false) {
     imp_.reset(ImpFactory::instance().CreateGameControllersImp());
     ScanPluggedControllers();
     StartTimer();
   }
   virtual ~GameControllers() {}

   typedef typename  std::vector<boost::shared_ptr<T> > Container;
   typedef typename  std::vector<boost::shared_ptr<T> >::iterator iterator;

   virtual iterator begin() { return plugged_controllers_.begin(); }
   virtual iterator end() { return plugged_controllers_.end(); }
   virtual bool empty() const { return plugged_controllers_.empty(); }
   virtual int size() const { return plugged_controllers_.size(); }
   
   void set_imp(GameControllersImp* imp) { imp_.reset(imp); }
   void release_imp() { imp_.release(); }
   GameControllersImp* imp() { return imp_.get(); };
   GameControllersImp* imp() const { return imp_.get(); };

   void ScanPluggedControllers() {
     std::vector<int> controller_ids;
     imp_->DevScanPluggedControllers(controller_ids);
     std::vector<int>::iterator it = controller_ids.begin();
     for ( ; it != controller_ids.end(); ++it) {       
       boost::shared_ptr<T> game_controller_info(new T());
       game_controller_info->set_id(*it);
       plugged_controllers_.push_back(game_controller_info);
     }
   }
	void Update() {
		if (!prevented()) try {
			if (size() < imp_->dev_size()) {
				std::vector<int> controller_ids;
				imp_->DevScanPluggedControllers(controller_ids);
				std::vector<int>::iterator id_it = controller_ids.begin();
				for (; id_it != controller_ids.end(); ++id_it) {
					int id = *id_it;
					bool found(false);
					for (iterator it = plugged_controllers_.begin(); it != plugged_controllers_.end(); ++it) {
						if ((*it)->id() == id) {
							found = true;
							break;
						}
					}
					if (!found) {				
						boost::shared_ptr<T> game_controller_info(new T());
						game_controller_info->set_id(id);
						plugged_controllers_.push_back(game_controller_info);					
						OnConnect(*game_controller_info.get());
					}
				}			
			}
			iterator it = plugged_controllers_.begin();
			while (it != plugged_controllers_.end()) {
				boost::shared_ptr<T> controller = *it;
				GameController old_state = *controller;
				imp_->DevUpdateController(*controller.get());
				if (controller->id() == -1) {
					OnDisconnect(*controller.get());
					it = plugged_controllers_.erase(it);
				} else {
					controller->AfterUpdate(old_state);
					++it;
				}
			}      
		} catch(std::exception&) {
			Prevent();
		}
	}
	
	virtual void OnTimerViewRefresh() { Update(); }
	void Prevent() { prevented_ = true; }
	void Enable() { prevented_ = false; }  
	bool prevented() const { return prevented_; }

  protected:
   virtual void OnConnect(T& controller) {}
   virtual void OnDisconnect(T& controller) {}

 private:
   std::auto_ptr<GameControllersImp> imp_;
   Container plugged_controllers_;
   bool prevented_;
};

template class GameControllers<GameController>;

} // namespace ui
} // namespace host
} // namespace psycle