/*****************************************************************************/
/* EffectWnd.h : Interface for CEffectWnd class. (Base plugin window)		 */
/*****************************************************************************/

/*****************************************************************************/
/* Work Derived from the LGPL host "vsthost (1.16m)".						 */
/* (http://www.hermannseib.com/english/vsthost.htm)"						 */
/* vsthost has the following lincense:										 *

Copyright (C) 2006  Hermann Seib

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#pragma once
#include "CVSTHost.Seib.hpp"

namespace seib {
	namespace vst{

typedef struct {
	UINT vkWin;
	unsigned char vstVirt;
} VkeysT;

class CEffectGui
{
public:
	CEffectGui(CEffect* effect): pEffect(effect){};
	void Open() { 
		pEffect->EditOpen(WindowPtr());
	}
	/// calculates the effect window's size
	bool GetViewSize(ERect &rcClient, ERect *pRect);
	void WindowIdle() { pEffect->EditIdle(); }
protected:
	virtual void* WindowPtr() = 0;
	CEffect* pEffect;
};

class CEffectWnd
{
protected:
	CEffectWnd(){};
public:
	CEffectWnd(CEffect* effect);
	virtual ~CEffectWnd();

// Operations
public:
    CEffect& GetEffect() { return *pEffect; }
	void SetTitleText(const char* lpszText = NULL) { sTitle = (lpszText) ? lpszText : ""; UpdateTitle(); }

protected:
	void ConvertToVstKeyCode(UINT nChar, UINT nRepCnt, UINT nFlags, VstKeyCode &keyCode);
	/// saves bank to file
	bool SaveBank(std::string const & file);
	bool SaveProgram(std::string const & file);

	///\name overridables
	///\{
public:
	virtual void CloseEditorWnd() = 0;
	virtual void ResizeWindow(int width, int height) = 0;
	virtual void RefreshUI() = 0;
	virtual bool BeginAutomating(VstInt32 index) { return false; }
	virtual bool SetParameterAutomated(VstInt32 index, float value) { return false; }
	virtual bool EndAutomating(VstInt32 index) { return false; }
	virtual bool OpenFileSelector (VstFileSelect *ptr) { return false; }
	virtual bool CloseFileSelector (VstFileSelect *ptr) { return false; }
	virtual void* OpenSecondaryWnd(VstWindow& window) { return 0; }
	virtual bool CloseSecondaryWnd(VstWindow& window) { return false; }

protected:
	virtual void UpdateTitle() = 0;
	///\}

protected:
	CEffect* pEffect;
	std::string sTitle;
	static VkeysT VKeys[];
};
	}
}
