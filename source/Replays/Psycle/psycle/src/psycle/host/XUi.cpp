// #ifdef __linux__
#include "XUi.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xatom.h>


namespace psycle {
namespace host {
namespace ui {
namespace x {
  

Display* display() {
    Display* dpy(0);
    App* app = Systems::instance().app();
    if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     dpy = app_imp->dpy();
    }
    return dpy;
} 

Visual* visual() {
    Visual* vis(0);
    App* app = Systems::instance().app();
    if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     vis = app_imp->visual();
    }
    return vis;
}

AppImp* app_imp() {    
    AppImp* app_imp(0);
    App* app = Systems::instance().app();
    if (app) {
      app_imp = dynamic_cast<ui::x::AppImp*>(app->imp()); 
    }
    return app_imp;
} 

AppImp::AppImp() : abort_(false) {
    InitDisplay();
    InitXdbe();
    InitColormap();	
    InitAtoms();    
}

AppImp::~AppImp() {   
  XCloseDisplay(dpy_);
}

void AppImp::InitDisplay() {
   dpy_ = XOpenDisplay(NIL);     
   if (dpy_ == 0) {
     printf("Cannot open X Display. Abort App.\n");
     exit(EXIT_FAILURE);
   }
}

void AppImp::InitXdbe() {
  d_vis = NULL;
  int major, minor;
  if (XdbeQueryExtension(dpy_, &major, &minor)) {
    printf("Xdbe (%d.%d) supported, using double buffering\n", major, minor);
    int numScreens = 1;
    Drawable screens[] = { DefaultRootWindow(dpy_) };
    XdbeScreenVisualInfo *info = XdbeGetVisualInfo(dpy_, screens, &numScreens);
    if (!info || numScreens < 1 || info->count < 1) {
      fprintf(stderr, "No visuals support Xdbe\n");
      exit(EXIT_FAILURE);
    }  
    XVisualInfo xvisinfo_templ;
    xvisinfo_templ.visualid = info->visinfo[0].visual; // We know there's at least one
    // As far as I know, screens are densely packed, so we can assume that if at least 1 exists, it's screen 0.
    xvisinfo_templ.screen = 0;
    xvisinfo_templ.depth = info->visinfo[0].depth;
    XFree(info);
    int matches;
    XVisualInfo *xvisinfo_match =
        XGetVisualInfo(dpy_, VisualIDMask|VisualScreenMask|VisualDepthMask, &xvisinfo_templ, &matches);
    if (!xvisinfo_match || matches < 1) {
        fprintf(stderr, "Couldn't match a Visual with double buffering\n");
        exit(EXIT_FAILURE);
    }
    d_vis = xvisinfo_match->visual;
    XFree(xvisinfo_match);
  } else {
     fprintf(stderr, "No Xdbe support\n");
     exit(EXIT_FAILURE);
  }    
}

void AppImp::InitColormap() {
   default_colormap_ = DefaultColormap(dpy_, DefaultScreen(dpy_));  
}

void AppImp::InitAtoms() {
  wm_delete_window = XInternAtom(dpy_, "WM_DELETE_WINDOW", True);
}
  
void AppImp::DevRun() {   
   XEvent e;  	
   x11_fd = ConnectionNumber(dpy_);
   abort_ = false;
   while(!abort_) {        
      FD_ZERO(&in_fds);
      FD_SET(x11_fd, &in_fds);        
      tv.tv_usec = 0;
      tv.tv_sec = 1;        		
      int num_ready_fds = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);
      if (num_ready_fds > 0) {
          // printf("Event Received!\n");
      } else
      if (num_ready_fds == 0) {
          // Handle timer here
          printf("Timer Fired!\n");
      } else {
          printf("An error occured!\n");
      }      
      while(XPending(dpy_) && !abort_) {
        if (ProcessNextEvent(e)) {
	   abort_ = true;
	}		
      }
   }
}

bool AppImp::ProcessNextEvent(XEvent& e) {  
  bool result(false);
  Display* dpy(display());  
  XNextEvent(dpy, &e);  
  switch (e.type) {
    case ClientMessage:            
       if (e.xclient.data.l[0] == wm_delete_window) { 
         result = true; 
       }
    break;
    case Expose: {      
      ui::WindowImp* window_imp = find(e.xany.window);
      if (window_imp) {                
        ui::Graphics* g = window_imp->DevGC();
        ui::Rect rc(ui::Point(e.xexpose.x, e.xexpose.y), 
             ui::Dimension(e.xexpose.width, e.xexpose.height));
        Region draw_rgn(rc);    
        assert(window_imp->window());
        window_imp->window()->DrawBackground(g, draw_rgn);    
        // g.Translate(Point(padding_.left() + border_space_.left(),
        // padding_.top() + border_space_.top()));
        window_imp->OnDevDraw(g, draw_rgn);
        window_imp->DevSwapBuffer();	      
     }       
   }
   break;
   case KeyPress:
     result = true;
   break;
   case ButtonPress: {
     ui::WindowImp* window_imp = find(e.xany.window);
     if (window_imp && window_imp->window()) {
       MouseEvent ev(window_imp->window(), Point(), 1, 0);       
       window_imp->OnDevMouseDown(ev);	     
       Systems::instance().app()->MouseDown(ev);
     }     
   }
   break;
   case ButtonRelease: {
     ui::WindowImp* window_imp = find(e.xany.window);
     if (window_imp && window_imp->window()) {
       MouseEvent ev(window_imp->window(), Point(), 1, 0);
       window_imp->OnDevMouseUp(ev);
     }
   }
   case MotionNotify: {     
     ui::WindowImp* window_imp = find(e.xany.window);
     if (window_imp && window_imp->window()) {
       MouseEvent ev(window_imp->window(), Point(), 1, 0);
       window_imp->OnDevMouseMove(ev);
     }
   }
   break;
   case EnterNotify: {
      ui::WindowImp* window_imp = find(e.xany.window);
      if (window_imp && window_imp->window()) {
        MouseEvent ev(window_imp->window(), Point(), 1, 0);
        window_imp->OnDevMouseEnter(ev);
      }
    }
   break;
   case LeaveNotify: {
     ui::WindowImp* window_imp = find(e.xany.window);
     if (window_imp && window_imp->window()) {
       MouseEvent ev(window_imp->window(), Point(), 1, 0);
       window_imp->OnDevMouseOut(ev);
     }
   }
   break;
   case ConfigureNotify: {
      ui::WindowImp* window_imp = find(e.xany.window);
      if (window_imp && window_imp->window()) {          
        window_imp->OnDevSize(
        ui::Dimension(e.xconfigure.width, e.xconfigure.height));
      }
    }     
    break;    
    default: ;
  }
  return result;
}

ui::WindowImp* AppImp::find(::Window w) {    
   std::map< ::Window, ui::WindowImp*>::iterator it = windows_.find(w);
   return it != windows_.end() ? it->second : 0;    
}


template<typename I>
WindowTemplateImp<I>::~WindowTemplateImp() {
   App* app = Systems::instance().app();
   if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     app_imp->UnregisterWindow(x_window_);
   }
   Display* dpy(display());
   XEvent ev;
   while (XPending(dpy)) { XNextEvent(dpy, &ev); }
   XDestroyWindow(display(), x_window_);
   XFlush(dpy);
}
  
::Window DummyWindow::dummy_wnd_(0);  
  
::Window DummyWindow::dummy() {
  if (!dummy_wnd_) {   
    Display* dpy(display());
    int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
    int whiteColor = WhitePixel(dpy, DefaultScreen(dpy)); 
     dummy_wnd_ = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 
             200, 100, 0, blackColor, blackColor);     
   }
   return dummy_wnd_;
}

GraphicsImp::GraphicsImp(): free_gc_(false) {
   Display* dpy(display());
   gc_ = DefaultGC(dpy, DefaultScreen(dpy));
   w_ = NIL;  
   x::FontImp* imp = dynamic_cast<x::FontImp*>(font_.imp());
   assert(imp);
   assert(imp->xfont());  
   XSetFont(dpy, gc_, imp->xfont()->fid);
}

GraphicsImp::GraphicsImp(::Window w) : free_gc_(true) {   
   Display* dpy(display());
   int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));       
   x::FontImp* imp = dynamic_cast<x::FontImp*>(font_.imp());
   assert(imp);   
   XGCValues gr_values;
   gr_values.font = imp->xfont()->fid;
   gr_values.foreground = whiteColor;    
   gc_ = XCreateGC(dpy, w, GCFont+GCForeground, &gr_values);   
   w_ = w;
}

void GraphicsImp::Init() {
}

void GraphicsImp::DevDispose() {
  if (free_gc_) {    
    XFreeGC(display(), gc_);
  }
}

void GraphicsImp::DevSetColor(ARGB color) {      
  //if (argb_color_ != color) {
  argb_color_ = color;
  AppImp* app_i = app_imp();
  assert(app_i);
  Display* dpy(app_i->dpy());
  Colormap screen_colormap = app_i->default_colormap();
  int red = (color >>16) & 0xff;
  int green = (color >> 8) & 0xff;  
  int blue = color & 0xff;
  XColor x_color;
  x_color.red = 0xFF * red;
  x_color.green = 0xFF * green;
  x_color.blue = 0xFF * blue;
  Status rc = XAllocColor(dpy, screen_colormap, &x_color);
  XSetForeground(dpy, gc_, x_color.pixel);
  XFlush(display());  
  //}
}

Dimension GraphicsImp::dev_text_dimension(const std::string& text) const {    
  XGCValues val;
  XGetGCValues(display(), gc_, GCFont, &val);    
  FontImp* font_imp = dynamic_cast<x::FontImp*>(font_.imp());
  assert(font_imp);
  assert(font_imp->xfont());        
  int w = XTextWidth(font_imp->xfont(), text.c_str(), text.length());
  int h = font_imp->xfont()->ascent + font_imp->xfont()->descent;    
  return Dimension(w, h);
}

void GraphicsImp::DevDrawString(const std::string& str, const Point& position) {
      // check_pen_update();    
  FontImp* font_imp = dynamic_cast<x::FontImp*>(font_.imp()); 
  assert(font_imp);
  assert(font_imp->xfont());  
  XDrawString(display(), w_, gc_, position.x(), position.y() + font_imp->xfont()->ascent, str.c_str(), str.length());
      //cr_->TextOut(static_cast<int>(position.x()), static_cast<int>(position.y()),
      //Charset::utf8_to_win(str).c_str());
}

void GraphicsImp::DevFillRect(const Rect& rect) {   
   XFillRectangle(display(), w_, gc_, rect.left(), rect.top(), rect.width(), rect.height());   
   XFlush(display());
}

void GraphicsImp::DevFillRegion(const ui::Region& rgn) {  
  Display* dpy(display());
  GC save_gc = XCreateGC(dpy, w_, 0, NULL);
  XCopyGC(dpy, gc_, 0xFFFF, save_gc); 
  ui::Region tmp(rgn);
  x::RegionImp* region_imp = dynamic_cast<x::RegionImp*>(tmp.imp());
  assert(region_imp); 
  XSetRegion(display(), gc_, region_imp->xrgn());  
  ui::Rect rc = region_imp->DevBounds();
  XFillRectangle(display(), w_, gc_,  
        rc.left(),
        rc.top(),
        rc.width(),
        rc.height()); 
  XCopyGC(dpy, save_gc, 0xFFFF, gc_);
  XFreeGC(display(), save_gc);
    /*1. Copy the region
    2. Intersect with visual rectangle
    3. Intersect with clipping rectangle
    4. Offset to physical rectangle
    5. set to clipping gc
    6. fill everything
    7. toss extra region
    8. restore clipping rectangle*/  
}

RegionImp::RegionImp() { 
  rgn_ = XCreateRegion();  
  XRectangle rect;
  memset(&rect, 0, sizeof(XRectangle)); 
  XUnionRectWithRegion(&rect, rgn_, rgn_);
}
  
RegionImp::RegionImp(const ::Region& rgn) {
  rgn_ = XCreateRegion();     
  XUnionRegion(rgn, rgn_, rgn_);
}

RegionImp::~RegionImp() {
   XDestroyRegion(rgn_);
}

RegionImp* RegionImp::DevClone() const {
  return new RegionImp(rgn_);  
}
  
void RegionImp::DevOffset(double dx, double dy) {  
   XOffsetRegion(rgn_, dx, dy);
}

 void RegionImp::DevUnion(const Region& other) {
   x::RegionImp* imp = dynamic_cast<x::RegionImp*>(other.imp());
   assert(imp);
   XUnionRegion(rgn_,  imp->xrgn(), rgn_);
}

ui::Rect RegionImp::DevBounds() const { 
  XRectangle rc;  
  XClipBox(rgn_, &rc);
  return ui::Rect(ui::Point(rc.x, rc.y), ui::Dimension(rc.width, rc.height));
}

bool RegionImp::DevIntersect(double x, double y) const {
  return XPointInRegion(rgn_, static_cast<int>(x), static_cast<int>(y)) != 0;      
}

bool RegionImp::DevIntersectRect(const ui::Rect& rect) const {    
  return XRectInRegion(rgn_, rect.left(), rect.top(), rect.width(), rect.height()) != RectangleOut;
}
  
void RegionImp::DevSetRect(const ui::Rect& rect) {
  XDestroyRegion(rgn_); 
  rgn_ = XCreateRegion();  
  XRectangle rc;
  rc.x = rect.left();
  rc.y = rect.top();
  rc.width = rect.width();
  rc.height = rect.height();  
  XUnionRectWithRegion(&rc, rgn_, rgn_);
}

void RegionImp::DevClear() {
  DevSetRect(ui::Rect());  
}
  


template<typename I>
ui::Graphics* WindowTemplateImp<I>::DevGC() {
  if (!gc_.get()) {	
     gc_.reset(new ui::Graphics(new ui::x::GraphicsImp(x_window_)));    
  }
  return gc_.get();
}

template<typename I>
void WindowTemplateImp<I>::dev_set_parent(Window*  parent) {    
  Display* dpy(display());
  if (parent && parent->imp()) {        
    ::Window parent_win = dynamic_cast<WindowImp*>(parent->imp())->x_window();      
    Rect pos = dev_position();
    XReparentWindow(dpy, x_window_, parent_win, pos.left(), pos.top());
    XMapWindow(dpy, x_window_); 
  } else {
    XReparentWindow(dpy, x_window_, DummyWindow::dummy(), 0, 0);    
    XMapWindow(dpy, x_window_); 
  }
}

template<typename I>
void WindowTemplateImp<I>::DevShow() {
  Display* dpy(display());
  XMapWindow(dpy, x_window_);    
  XFlush(dpy);
}

template<typename I>
void WindowTemplateImp<I>::DevHide() {
  Display* dpy(display());
  XUnmapWindow(dpy, x_window_);    
  XFlush(dpy);
}

template<typename I>
void WindowTemplateImp<I>::DevInvalidate()  {  
  Display* dpy(display());  
  XEvent e;  
  std::memset(&e, 0, sizeof(e));  
  e.type = Expose;
  e.xexpose.window = x_window_;	
  XWindowAttributes size;
  assert(x_window_);	
  XGetWindowAttributes(dpy, x_window_, &size);  
  e.xexpose.x = 0;
  e.xexpose.y = 0;
  e.xexpose.width = size.width;
  e.xexpose.height = size.height;    
  XSendEvent(dpy, x_window_, False, ExposureMask, &e);  
  XFlush(dpy);  
}

template<typename I>
void WindowTemplateImp<I>::dev_set_position(const ui::Rect& pos) {
  ui::Point top_left = pos.top_left();
  top_left.offset(margin_.left(), margin_.top());
  /*if (window() && window()->parent()) {
    top_left.offset(window()->parent()->border_space().left() +
                      window()->parent()->padding().left(),
                    window()->parent()->border_space().top() +
                      window()->parent()->padding().top());
  }*/
  Display* dpy(display());  
  int w = static_cast<int>(pos.width() + padding_.width() + border_space_.width());
  int h = static_cast<int>(pos.height() + padding_.height() + border_space_.height());
  w = (std::max)(1, w);
  h = (std::max)(1, h);              
  XMoveResizeWindow(dpy,
                            x_window_,
                                  static_cast<int>(top_left.x()),
                      static_cast<int>(top_left.y()),
                      w, h);
  XFlush(display());  
}

template<typename I>
ui::Rect WindowTemplateImp<I>::dev_position() const {
   Display* dpy(display());       
   XWindowAttributes xwa;   
   XGetWindowAttributes( dpy, x_window_, &xwa);
   return ui::Rect(ui::Point(xwa.x, xwa.y),
       ui::Dimension(xwa.width, xwa.height)); 
}

template<typename I>
ui::Rect WindowTemplateImp<I>::dev_desktop_position() const {
   Display* dpy(display());    
   ::Window child;
   XWindowAttributes xwa;
   XGetWindowAttributes( dpy, x_window_, &xwa );   
   ::Window root = DefaultRootWindow(display());  
   XTranslateCoordinates(dpy, x_window_, root,
     0, 0, &(xwa.x), &(xwa.y), &child);   
   return ui::Rect(ui::Point(xwa.x, xwa.y),
                            ui::Dimension(xwa.width, xwa.height));
}

template<typename I>
Dimension WindowTemplateImp<I>::dev_dim() const {
  XWindowAttributes size;
  XGetWindowAttributes(display(), x_window_, &size);
  return Dimension(size.width, size.height);    
}

template<typename I>
void WindowTemplateImp<I>::DevSetCapture() {
  Display* dpy(display());
    ::XGrabPointer(dpy, x_window_, True,
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        FocusChangeMask |
        EnterWindowMask |
        LeaveWindowMask,
        GrabModeAsync,
        GrabModeAsync,
        RootWindow(dpy, DefaultScreen(dpy)),
        None,
        CurrentTime);       
}

template<typename I>
void WindowTemplateImp<I>::DevReleaseCapture()  {
    ::XUngrabPointer(display(), CurrentTime);      
}

WindowImp::WindowImp(::Window parent) {     
	App* app = Systems::instance().app();  
  if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
   Display* dpy(display());
   int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
   int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));  
   x_window_ = XCreateSimpleWindow(dpy, parent, 0, 0,
       100, 20, 0, blackColor, blackColor);   	
   unsigned long xAttrMask = CWBackPixel;
   XSetWindowAttributes xAttr;   
   xAttr.colormap = app_imp->default_colormap() ;
   xAttr.border_pixel = 0;
   xAttr.background_pixel = 0;
   x_window_ = XCreateWindow(dpy, parent,
        0, 0, 100, 20, 0,
       CopyFromParent, InputOutput, CopyFromParent,
   CWColormap | CWBorderPixel | CWBackPixel, &xAttr);
   d_backBuf = XdbeAllocateBackBufferName(dpy, x_window_, XdbeBackground);	
   XSelectInput(dpy, x_window_, StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask);        
     app_imp->RegisterWindow(x_window_, this);
   }
}

ui::Graphics* WindowImp::DevGC() {
  if (!gc_.get()) {	
     gc_.reset(new ui::Graphics(new ui::x::GraphicsImp(d_backBuf)));    
  }
  return gc_.get();
}

void WindowImp::DevSwapBuffer() {
  Display* dpy(display());
  XdbeSwapInfo swapInfo;
  swapInfo.swap_window = x_window_;
  swapInfo.swap_action = XdbeCopied;
  // XdbeSwapBuffers returns true on success, we return 0 on success.
  if (!XdbeSwapBuffers(dpy, &swapInfo, 1))
   return;    
  XFlush(dpy);
}

ButtonImp::ButtonImp(::Window parent) : checked_(false), text_("Button") {     
   Display* dpy(display());
   int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
   int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));  
   x_window_ = XCreateSimpleWindow(dpy, parent, 0, 0, 
       200, 200, 0, blackColor, blackColor);	
   XSelectInput(dpy, x_window_, StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask);
   App* app = Systems::instance().app();
   if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     app_imp->RegisterWindow(x_window_, this);
   }
}

void ButtonImp::OnDevDraw(Graphics* g, Region& draw_region) {
   if (!checked_) { 
     g->set_color(0xFFFFFFFF);
   } else {
     g->set_color(0xFFAAAAAA);
   }   
   assert(window());
   g->FillRect(ui::Rect(ui::Point(), window()->position().dimension()));
   g->set_color(0xFF222222);
   g->SetFont(font_);
   g->DrawString(text_.c_str(), ui::Point());
}

void ButtonImp::OnDevMouseDown(MouseEvent& ev) {
  checked_ = true;  
  ui::Graphics g(new ui::x::GraphicsImp(x_window_));
  ui::Region draw_rgn(0);
  OnDevDraw(&g, draw_rgn);      
}

void ButtonImp::OnDevMouseUp(MouseEvent& ev) {
  checked_ = false; 
  ui::Graphics g(new ui::x::GraphicsImp(x_window_));
  ui::Region draw_rgn(0);
  OnDevDraw(&g, draw_rgn);
  OnDevClick();  
}

FrameImp::FrameImp() {
   App* app = Systems::instance().app();
   if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     Display* dpy(display());
     int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
     int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));  
     XSetWindowAttributes xAttr;   
     xAttr.colormap = app_imp->default_colormap() ;
     xAttr.border_pixel = 0;
     xAttr.background_pixel = 0;   
     x_window_ = XCreateWindow(dpy, DefaultRootWindow(dpy),
          0, 0, 200, 100, 0,
          CopyFromParent, InputOutput, CopyFromParent,
          CWColormap | CWBorderPixel | CWBackPixel, &xAttr);
     XSelectInput(dpy, x_window_, StructureNotifyMask | ExposureMask | KeyPressMask | ConfigureNotify);     
     app_imp->RegisterWindow(x_window_, this);
     XSetWMProtocols(dpy, x_window_, &app_imp->wm_delete_window, 1);   
   }      
}

void FrameImp::DevShow() {
    Display* dpy(display());    
    XMapWindow(dpy, x_window_);    
    // Wait for the MapNotify event
    for(;;) {
      XEvent e;
      XNextEvent(dpy, &e);
      if (e.type == MapNotify)
      break;
    }    
    XFlush(dpy);
}

void FrameImp::dev_set_viewport(const ui::Window::Ptr& viewport) {
    /*if (viewport_) {
      CWnd* wnd = dynamic_cast<CWnd*>(viewport_->imp());
      ::Window wnd = dynamic_cast<WindowImp*>(viewport_->imp())->x_window();      
      
      if (wnd) {
        wnd->SetParent(DummyWindow::dummy());
      }
    }*/
    if (viewport) {
      ::Window wnd = dynamic_cast<x::WindowImp*>(viewport->imp())->x_window();
      if (wnd) {        
        XReparentWindow(display(), wnd, x_window_, 0, 0);
      }
    }    
}

void FrameImp::OnDevSize(const ui::Dimension& dimension) {    
  if (window()) {
    Frame* frame = dynamic_cast<Frame*>(window());
    assert(frame);
    if (frame->viewport()) {      
      ::Window wnd = dynamic_cast<x::WindowImp*>(
          frame->viewport()->imp())->x_window();
      if (wnd) {        
        XResizeWindow(display(), wnd, dimension.width(), dimension.height());
      }
    }    
  }
}

void FrameImp::dev_set_title(const std::string& title)  {
     XStoreName(display(), x_window_, title.c_str());
  }

PopupFrameImp::PopupFrameImp() {
   App* app = Systems::instance().app();
   if (app) {
     AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
     Display* dpy(display());
     int blackColor = BlackPixel(dpy, DefaultScreen(dpy));
     int whiteColor = WhitePixel(dpy, DefaultScreen(dpy));  		
     XSetWindowAttributes xAttr;   
     xAttr.colormap = app_imp->default_colormap() ;
     xAttr.border_pixel = 0;
     xAttr.background_pixel = 0;
     xAttr.override_redirect = True;  
     x_window_ = XCreateWindow(dpy, DefaultRootWindow(dpy),
        0, 0, 200, 100, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel, &xAttr);		   
     Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
     Atom value = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
     XChangeProperty(dpy, x_window_, type, 4, 32, PropModeReplace,
         reinterpret_cast<unsigned char*>(&value), 1);
      XSelectInput(dpy, x_window_,
         StructureNotifyMask | ExposureMask | KeyPressMask | ConfigureNotify | PointerMotionMask);   
     XMoveResizeWindow(dpy, x_window_, 0, 0, 300, 300);
     app_imp->RegisterWindow(x_window_, this);
   }
}

ui::WindowImp* ImpFactory::CreateWindowImp() {     
  ui::WindowImp* imp = WindowImp::Make(0, DummyWindow::dummy());    
  return imp;
}

void AlertImp::Init() {
   frame_.reset(new Frame());   
   frame_->set_title(Systems::instance().app()->title());
   canvas_.reset(new Canvas());
   background_.reset(OrnamentFactory::Instance().CreateFill(0xFFCACACA));
   canvas_->add_ornament(background_);
   text_.reset(new Text());
   text_->set_auto_size(false, false);
   text_->set_align(AlignStyle::ALTOP);	
   canvas_->Add(text_);   
   Group::Ptr button_group(new Group());
   button_group->set_align(AlignStyle::ALBOTTOM);
   button_group->set_auto_size(false, true);
   canvas_->Add(button_group);	
   ok_button_.reset(new Button());   
   ok_button_->set_align(AlignStyle::ALRIGHT);
   ok_button_->click.connect(boost::bind(&AlertImp::OnOkButtonClick, this, _1));
   button_group->Add(ok_button_);
   frame_->set_viewport(canvas_);
   frame_->set_position(ui::Rect(ui::Point(), ui::Dimension(150, 150)));   
}

void AlertImp::DevAlert(const std::string& text) {   
   text_->set_text(text);
   frame_->Show();   
   WaitForOk();
}

void AlertImp::OnOkButtonClick(Button&) {
   abort_ = true;
}

void AlertImp::WaitForOk() {
  App* app = Systems::instance().app();	
  assert(app);
  AppImp* app_imp = dynamic_cast<ui::x::AppImp*>(app->imp());
  assert(app_imp);        
  Display* dpy(app_imp->dpy());	  
  x11_fd = ConnectionNumber(dpy);
  XEvent e;    
  abort_ = false;	  
  while(!abort_) {        
    FD_ZERO(&in_fds);
    FD_SET(x11_fd, &in_fds);        
    tv.tv_usec = 0;
    tv.tv_sec = 1;        		
    int num_ready_fds = select(x11_fd + 1, &in_fds, NULL, NULL, &tv);
    if (num_ready_fds > 0) {
        // printf("Event Received!\n");
    } else
    if (num_ready_fds == 0) {
       // Handle timer here
       printf("Timer Fired!\n");
    } else {
       printf("An error occured!\n");
    }      
    while(XPending(dpy) && !abort_) {
      if (app_imp->ProcessNextEvent(e)) {
        abort_ = true;	
      }
    }       
  }
}

template class WindowTemplateImp<ui::WindowImp>;
template class WindowTemplateImp<ui::ButtonImp>;

} // namespace x
} // namespace ui
} // namespace host
} // namespace psycle
//#endif
