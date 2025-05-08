///\file
///\brief interface file for psycle::host::CBaseParamView.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
namespace psycle { namespace host {

class CFrameMachine;

class CBaseParamView : public CWnd {
	public:
		CBaseParamView(CFrameMachine* frame) : parentFrame(frame) {};
		virtual void Open() {};
		virtual void Close(class Machine* mac) {};
		virtual bool GetViewSize(CRect& rect) { return false; }
		virtual void WindowIdle() {      
      Invalidate(false);
    }
    virtual void OnReload(class Machine* mac) {}
	protected:
		virtual void* WindowPtr() { return GetSafeHwnd(); }
		CFrameMachine* parentFrame;
};

}}
