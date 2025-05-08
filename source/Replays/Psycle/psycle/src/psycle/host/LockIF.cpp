// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

//#include "stdafx.h"
#include <algorithm>
#include "LockIF.hpp"
#include "Ui.hpp"
#include "MainFrm.hpp"
#undef _WIN32


namespace psycle {
namespace host {  

Lock::Lock() : imp_(ui::ImpFactory::instance().CreateLocker()) {
}

void Lock::lock() const {
//   assert(imp_.get());
//   imp_->lock();
}

void Lock::unlock() const {
//   assert(imp_.get());
//   imp_->unlock();
}

Timer::Timer() : is_running_(false) { GlobalTimer::instance().AddListener(this); }
Timer::~Timer() { GlobalTimer::instance().RemoveListener(this); }

const int GlobalTimer::refresh_rate = 30;

void GlobalTimer::AddListener(Timer* listener) {                
  listeners_.push_back(listener);
}
    
void GlobalTimer::RemoveListener(Timer* listener) {  
  if (!listeners_.empty()) {
    TimerList::iterator i = std::find(listeners_.begin(), listeners_.end(), listener);
    if (i != listeners_.end()) {
      if (i!=it) {
        listeners_.erase(i);
      } else {
        listeners_.erase(it++);
        removed_ = true;
      }          
    }
  }
}
    
void GlobalTimer::OnViewRefresh() {          
  if (started_) {
    it = listeners_.begin();
    while (it != listeners_.end()) {
      Timer* timer = *it;
      if (timer->is_running()) {
        timer->OnTimerViewRefresh();      
      }    
      if (!started_) {
        break;
      }
      if (!removed_) {          
        ++it;
      } else {
        removed_ = false;
      }
    }
  }
}   

void GlobalTimer::KillTimer() {
#ifdef _WIN32	
  CMainFrame* fr = (CMainFrame*) ::AfxGetMainWnd();
  CChildView* cv = &fr->m_wndView;    
  MSG msg; 
  HWND hWnd = cv->GetSafeHwnd();
  started_ = false;
  bool result = cv->KillTimer(39);
  while (result && 
    PeekMessage(&msg, hWnd, WM_TIMER, WM_TIMER, PM_NOREMOVE) && 
       (msg.wParam == 39))  {
    PeekMessage(&msg, hWnd, WM_TIMER, WM_TIMER, PM_REMOVE); 
  }
#endif	
}

void GlobalTimer::StartTimer() {
#ifdef _WIN32	
  CMainFrame* fr = (CMainFrame*) ::AfxGetMainWnd();
  CChildView* cv = &fr->m_wndView;
  started_ = true;
  cv->SetTimer(39, 30, NULL);
#endif	
}

}  // namespace
}  // namespace