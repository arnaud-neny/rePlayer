// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file \brief meta header that includes universalis' essential features

#pragma once

#include "universalis/compiler/stringize.hpp"
#include "universalis/compiler/concat.hpp"
#include "universalis/compiler/token.hpp"
#include "universalis/os/eol.hpp"

#if 1//ndef DIVERSALIS__COMPILER__RESOURCE // rePlayer

	#include "universalis/compiler/constexpr.hpp"
	#include "universalis/compiler/noexcept.hpp"
	#include "universalis/compiler/restrict.hpp"
	#include "universalis/compiler/pragma.hpp"
	#include "universalis/compiler/attribute.hpp"
	#include "universalis/compiler/const_function.hpp"
	#include "universalis/compiler/pure_function.hpp"
	#include "universalis/compiler/align.hpp"
	#include "universalis/compiler/pack.hpp"
	#include "universalis/compiler/message.hpp"
	#include "universalis/compiler/deprecated.hpp"
	#include "universalis/compiler/virtual.hpp"
	#include "universalis/compiler/thread_local.hpp"
	#include "universalis/compiler/weak.hpp"
	#include "universalis/compiler/calling_convention.hpp"
	#include "universalis/compiler/asm.hpp"
	#include "universalis/compiler/dyn_link.hpp"
	#include "universalis/compiler/auto_link.hpp"
	#include "universalis/compiler/typenameof.hpp"
	#include "universalis/compiler/location.hpp"
	#include "universalis/compiler/exception.hpp"

	#include "universalis/stdlib/cstdint.hpp"
	#include "universalis/stdlib/exception.hpp"

	#include "universalis/os/loggers.hpp"
	#include "universalis/os/exception.hpp"

	#include "universalis/cpu/exception.hpp"

	// TODO remove these
	#include <boost/preprocessor/stringize.hpp>
	#include <boost/static_assert.hpp>
	#include <cassert>
	#include <ciso646>

// rePlayer begin
	// disable all the mfc crap
 	#include <atltypes.h>
	#include <atlstr.h>
    #define MFC_CTOR(a) template <typename...Args> a(Args&&...) {}
    #define MFC_FUNC0(a, b) template <typename...Args> a b(Args&&...) {}
    #define MFC_FUNC1(a, b, c) template <typename...Args> a b(Args&&...) { return c; }
    #define MFC_FUNC0_CONST(a, b) template <typename...Args> a b(Args&&...) const {}
    #define MFC_FUNC1_CONST(a, b, c) template <typename...Args> a b(Args&&...) const { return c; }

    struct CBitmap;
    struct CBrush;
    struct CFrameWnd;
    struct CFont;
    struct CGdiObject;
    struct CPen;

    using HDROP = void*;
	using HTREEITEM = void*;
    using POSITION = int;
    struct OPENFILENAME
    {
        DWORD lStructSize;
        HWND hwndOwner;
        HINSTANCE hInstance;
        LPCSTR lpstrFilter;
        LPSTR lpstrCustomFilter;
        DWORD nMaxCustFilter;
        DWORD nFilterIndex;
        LPSTR lpstrFile;
        DWORD nMaxFile;
        LPSTR lpstrFileTitle;
        DWORD nMaxFileTitle;
        LPCSTR lpstrInitialDir;
        LPCSTR lpstrTitle;
        DWORD Flags;
        WORD nFileOffset;
        WORD nFileExtension;
        LPCSTR lpstrDefExt;
        LPARAM lCustData;
        void* lpfnHook;
        LPCSTR lpTemplateName;
    };
    struct TCITEM
    {
        uint32_t mask;
        LPSTR pszText;
    };
    struct UDACCEL
    {
        uint32_t nSec;
        uint32_t nInc;
    };

    struct CCmdUI
    {
        MFC_FUNC0(void, Enable);
        MFC_FUNC0(void, SetCheck);
        MFC_FUNC0(void, SetText);
    };
    struct CDataExchange {};
    struct CDumpContext {};
    struct CObject
    {
        MFC_FUNC0_CONST(void, AssertValid)
        MFC_FUNC0_CONST(void, Dump)
    };
    struct CSingleLock
    {
        MFC_CTOR(CSingleLock);
        MFC_FUNC1(bool, Lock, true);
        MFC_FUNC0(void, Unlock);
    };
    struct CWinApp
    {
        MFC_FUNC0(void, DoWaitCursor);
        MFC_FUNC1(int, ExitInstance, 0);
        MFC_FUNC1(bool, IsIdleMessage, true);
        MFC_FUNC1(HICON, LoadIcon, nullptr);
        MFC_FUNC1(HCURSOR, LoadStandardCursor, nullptr);
        MFC_FUNC1(bool, OnIdle, true);
        MFC_FUNC1(bool, PreTranslateMessage, true);

        struct CWnd* m_pMainWnd;
        LPSTR m_lpCmdLine = "";
    };
    // CObject
    struct CDC : public CObject
    {
        MFC_FUNC0(void, Arc);
        MFC_FUNC0(void, Attach);
        MFC_FUNC1(bool, BitBlt, true);
        MFC_FUNC1(bool, CreateCompatibleDC, true);
        MFC_FUNC0(void, DeleteDC);
        MFC_FUNC0(void, Detach);
        MFC_FUNC0(void, Draw3dRect);
        MFC_FUNC0(void, ExtTextOut);
        MFC_FUNC0(void, FillRect);
        MFC_FUNC0(void, FillSolidRect);
        MFC_FUNC1(int, GetClipBox, 0);
        MFC_FUNC1(CFont*, GetCurrentFont, nullptr);
        MFC_FUNC1(int, GetDeviceCaps, 0);
        MFC_FUNC1(CSize, GetTextExtent, {});
        MFC_FUNC0(void, LineTo);
        MFC_FUNC0(void, MoveTo);
        MFC_FUNC0(void, Polygon);
        MFC_FUNC0(void, Polyline);
        MFC_FUNC0(void, Rectangle);
        MFC_FUNC0(void, ScrollDC);
        CBitmap* SelectObject(CBitmap*) { return nullptr; }
        CBrush* SelectObject(CBrush*) { return nullptr; }
        CFont* SelectObject(CFont*) { return nullptr; }
        CGdiObject* SelectObject(CGdiObject*) { return nullptr; }
        CPen* SelectObject(CPen*) { return nullptr; }
        MFC_FUNC1(CGdiObject*, SelectStockObject, nullptr);
        MFC_FUNC1(void*, SelectObject, nullptr)
        MFC_FUNC0(void, SetBkColor);
        MFC_FUNC0(void, SetBkMode);
        MFC_FUNC1(int, SetROP2, 0);
        MFC_FUNC0(void, SetStretchBltMode);
        MFC_FUNC0(void, SetTextColor);
        MFC_FUNC1(CPoint, SetWindowOrg, {});
        MFC_FUNC0(void, StretchBlt);
        MFC_FUNC0(void, TextOut);
    };
    struct CCmdTarget : public CObject {};
    struct CFileFind : public CObject
    {
        MFC_FUNC0(void, Close);
        MFC_FUNC1(int, FindFile, 0);
        MFC_FUNC1(int, FindNextFile, 0);
        MFC_FUNC1(CString, GetFileName, {});
        MFC_FUNC1(CString, GetFilePath, {});
        MFC_FUNC1(bool, GetLastWriteTime, true);
        MFC_FUNC1(bool, IsDirectory, false);
        MFC_FUNC1(bool, IsDots, false);
    };
    struct CGdiObject : public CObject
    {
        MFC_FUNC1(bool, Attach, true);
        MFC_FUNC0(void, DeleteObject);
        MFC_FUNC1(HGDIOBJ, Detach, nullptr);
        MFC_FUNC0(void, GetObject);
    };
    struct CImageList : public CObject
    {
        MFC_FUNC1(int, Add, 0);
        MFC_FUNC1(bool, Create, true);
        MFC_FUNC0(void, DeleteImageList);
        MFC_FUNC1(void*, GetSafeHandle, nullptr);
        MFC_FUNC1(COLORREF, SetBkColor, {});
        MFC_FUNC0(void, SetImageList);
    };
    struct CMenu : public CObject
    {
        MFC_FUNC0(void, AppendMenu);
        MFC_FUNC0(void, CheckMenuItem);
        MFC_FUNC1(bool, CreatePopupMenu, true);
        MFC_FUNC0(void, DeleteMenu);
        MFC_FUNC0(void, DestroyMenu);
        MFC_FUNC1(void*, Detach, nullptr);
        MFC_FUNC0(void, EnableMenuItem);
        MFC_FUNC1(bool, LoadMenu, true);
        MFC_FUNC1(uint32_t, GetMenuItemCount, 0);
        MFC_FUNC1(uint32_t, GetMenuItemID, 0);
        MFC_FUNC1(CString, GetMenuString, {});
        MFC_FUNC1(void*, GetSafeHmenu, nullptr);
        MFC_FUNC1(CMenu*, GetSubMenu, nullptr);
        MFC_FUNC0(void, ModifyMenu);
        MFC_FUNC0(void, TrackPopupMenu);
        MFC_FUNC0(void, RemoveMenu);
    };
    struct CSyncObject : public CObject
    {
        MFC_FUNC1(bool, Lock, true);
        MFC_FUNC0(void, Unlock);
    };
    // CCmdTarget
    struct CWnd : public CCmdTarget
    {
        virtual ~CWnd() {}
        MFC_FUNC0(void, Attach);
        MFC_FUNC0(void, CalcWindowRect);
        MFC_FUNC0(void, ClientToScreen);
        MFC_FUNC1(bool, Create, true);
        MFC_FUNC0(void, DestroyWindow);
        MFC_FUNC1(HWND, Detach, nullptr);
        MFC_FUNC0(void, DoDataExchange);
        MFC_FUNC1(int, DoModal, -1);
        MFC_FUNC0(void, DragAcceptFiles);
        MFC_FUNC0(void, DragFinish);
        MFC_FUNC1(int, DragQueryFile, 0);
        MFC_FUNC0(void, DrawMenuBar);
        MFC_FUNC0(void, EnableToolTips);
        MFC_FUNC0(void, EnableTrackingToolTips);
        MFC_FUNC0(void, EnableWindow);
        MFC_FUNC0(void, GetClientRect);
        MFC_FUNC1(int, GetDlgCtrlID, 0);
        MFC_FUNC1(CWnd*, GetDlgItem, nullptr);
        MFC_FUNC1(CWnd*, GetFocus, nullptr);
        MFC_FUNC1(HICON, GetIcon, nullptr);
        MFC_FUNC1(CWnd*, GetLastActivePopup, nullptr);
        MFC_FUNC1(CMenu*, GetMenu, nullptr);
        MFC_FUNC1(CWnd*, GetOwner, nullptr);
        MFC_FUNC1(CWnd*, GetParent, nullptr);
        MFC_FUNC1(CFrameWnd*, GetParentFrame, nullptr);
        MFC_FUNC1(HWND, GetSafeHwnd, nullptr);
        MFC_FUNC1(bool, GetUpdateRect, true);
        MFC_FUNC0(void, GetUpdateRgn);
        MFC_FUNC1(CWnd*, GetWindow, nullptr);
        MFC_FUNC1(bool, GetWindowPlacement, true);
        MFC_FUNC0(void, GetWindowRect);
        MFC_FUNC1_CONST(int, GetWindowText, 0);
        MFC_FUNC1(bool, HandleInitDialog, true);
        MFC_FUNC0(void, Invalidate);
        MFC_FUNC0(void, InvalidateRect);
        MFC_FUNC1(bool, IsIconic, false);
        MFC_FUNC1(bool, IsWindowVisible, true);
        MFC_FUNC1(bool, KillTimer, true);
        MFC_FUNC1(int, MessageBox, 0);
        MFC_FUNC1(bool, ModifyStyle, true);
        MFC_FUNC0(void, MoveWindow);
        MFC_FUNC0(void, OnActivate);
        MFC_FUNC0(void, OnBarStyleChange);
        MFC_FUNC0(void, OnClose);
        MFC_FUNC1(bool, OnCmdMsg, true);
        MFC_FUNC0(void, OnContextMenu);
        MFC_FUNC1(HBRUSH, OnCtlColor, nullptr);
        MFC_FUNC1(int, OnCreate, -1);
        MFC_FUNC0(void, OnDestroy);
        MFC_FUNC0(void, OnDestroyClipboard);
        MFC_FUNC1(bool, OnEraseBkgnd, true);
        MFC_FUNC0(void, OnHScroll);
        MFC_FUNC0(void, OnInitMenuPopup);
        MFC_FUNC0(void, OnKeyDown);
        MFC_FUNC0(void, OnKeyUp);
        MFC_FUNC0(void, OnLButtonDown);
        MFC_FUNC0(void, OnLButtonUp);
        MFC_FUNC0(void, OnLButtonDblClk);
        MFC_FUNC0(void, OnMButtonDown);
        MFC_FUNC1(bool, OnMouseWheel, true);
        MFC_FUNC0(void, OnRButtonDown);
        MFC_FUNC0(void, OnRButtonUp);
        MFC_FUNC0(void, OnMouseMove);
        MFC_FUNC0(void, OnSetFocus);
        MFC_FUNC1(int, OnSetMessageString, 0);
        MFC_FUNC0(void, OnShowWindow);
        MFC_FUNC0(void, OnSize);
        MFC_FUNC0(void, OnTimer);
        MFC_FUNC0(void, OnVScroll);
        MFC_FUNC1(bool, OnWndMsg, true);
        MFC_FUNC1(bool, OpenClipboard, true);
        MFC_FUNC0(void, PostMessage);
        MFC_FUNC0(void, PostNcDestroy);
        MFC_FUNC1(bool, PreCreateWindow, true);
        MFC_FUNC0(void, PreSubclassWindow);
        MFC_FUNC1(bool, PreTranslateMessage, true);
        MFC_FUNC0(void, ScreenToClient);
        MFC_FUNC1(bool, SetScrollInfo, true);
        MFC_FUNC1(int, SendMessage, 0);
        MFC_FUNC0(void, SetActiveWindow);
        MFC_FUNC0(void, SetCapture);
        MFC_FUNC0(void, SetFocus);
        MFC_FUNC0(void, SetFont);
        MFC_FUNC0(void, SetForegroundWindow);
        MFC_FUNC0(void, SetIcon);
        MFC_FUNC1(int, SetScrollPos, 0);
        MFC_FUNC1(uintptr_t, SetTimer, 0);
        MFC_FUNC1(bool, SetWindowPos, true);
        MFC_FUNC0(void, SetWindowText);
        MFC_FUNC0(void, ShowScrollBar);
        MFC_FUNC0(void, ShowWindow);
        MFC_FUNC1(bool, UpdateData, true);
        MFC_FUNC0(void, UpdateWindow);
        MFC_FUNC1(int, WindowProc, 0);

        HWND m_hWnd = nullptr;
        CWnd* m_pInPlaceOwner = nullptr;
    };
    // CGdiObject
	struct CBitmap : public CGdiObject
    {
        MFC_CTOR(CBitmap);
        MFC_FUNC1(int, GetBitmap, 0);
		MFC_FUNC0(void, LoadBitmap);
		MFC_FUNC0(void, CreateBitmap);
        MFC_FUNC0(void, CreateCompatibleBitmap);
		void* m_hObject = nullptr;
    };
    struct CBrush : public CGdiObject
    {
        MFC_CTOR(CBrush);
        MFC_FUNC1(bool, CreateSolidBrush, true);
    };
    struct CFont : public CGdiObject
    {
        MFC_FUNC1(bool, CreatePointFont, true);
        MFC_FUNC1(bool, CreatePointFontIndirect, true);
    };
    struct CPen : public CGdiObject
    {
        MFC_CTOR(CPen);
        MFC_FUNC1(bool, CreatePen, true);
    };
    struct CRgn : public CGdiObject
    {
        MFC_FUNC0(void, CreateRectRgn);
    };
	// CDC
	struct CClientDC : public CDC
	{
        MFC_CTOR(CClientDC);
	};
    struct CPaintDC : public CDC
    {
        MFC_CTOR(CPaintDC)
		struct  
		{
			RECT rcPaint = {};
		} m_ps;
    };
    // CSyncObject
    struct CEvent : public CSyncObject
    {
        MFC_CTOR(CEvent);
        MFC_FUNC0(void, SetEvent);
    };
    class CCriticalSection : public CSyncObject {};
    struct CSemaphore : public CSyncObject
    {
        MFC_CTOR(CSemaphore);
    };
    struct CMutex : public CSyncObject {};
    // CWinApp
	struct CWinAppEx : public CWinApp {};
    // CWnd
    struct CButton : public CWnd
    {
        MFC_FUNC0(void, SetCheck);
        MFC_FUNC1_CONST(bool, GetCheck, false);
    };
    struct CComboBox : public CWnd
    {
        MFC_FUNC1(int, AddString, 0);
        MFC_FUNC0(void, Clear);
        MFC_FUNC0(void, DeleteString);
        MFC_FUNC1(int, GetCount, 0);
        MFC_FUNC1_CONST(int, GetCurSel, 0);
        MFC_FUNC1(uint32_t, GetItemData, 0);
        MFC_FUNC0(void, LimitText);
        MFC_FUNC0(void, ResetContent);
        MFC_FUNC1(int, SelectString, 0);
        MFC_FUNC1(int, SetCurSel, 0);
        MFC_FUNC1(int, SetItemData, 0);
    };
    struct CControlBar : public CWnd
    {
        MFC_FUNC0(void, EnableDocking);
        MFC_FUNC1(uint32_t, GetBarStyle, 0);
        MFC_FUNC0(void, SetBarStyle);

        CControlBar* m_pDockBar = nullptr;
    };
    struct CDialog : public CWnd
	{
        MFC_CTOR(CDialog);
        MFC_FUNC0(void, EndDialog);
        MFC_FUNC0(void, MapDialogRect);
        MFC_FUNC0(void, OnCancel);
		MFC_FUNC0(void, OnInitDialog);
        MFC_FUNC0(void, OnOK);
	};
    struct CPropertyPage : public CDialog
    {
        MFC_CTOR(CPropertyPage);
        MFC_FUNC1(bool, OnSetActive, true);
    };
    struct CEdit : public CWnd
    {
        MFC_FUNC0(void, SetLimitText);
        MFC_FUNC1(bool, SetReadOnly, true);
        MFC_FUNC0(void, SetSel);
    };
    struct CFrameWnd : public CWnd
    {
        MFC_FUNC0(void, DockControlBar);
        MFC_FUNC0(void, EnableDocking);
        MFC_FUNC0(void, LoadBarState);
        MFC_FUNC1(bool, LoadFrame, true);
        MFC_FUNC0(void, RecalcLayout);
        MFC_FUNC0(void, SaveBarState);
        MFC_FUNC0(void, ShowControlBar);
    };
    struct CHotKeyCtrl : public CWnd
    {
        MFC_FUNC0(void, GetHotKey);
        MFC_FUNC0(void, SetHotKey);
        MFC_FUNC0(void, SetRules);
    };
    struct CListBox : public CWnd
    {
        MFC_FUNC1(int, AddString, 0);
        MFC_FUNC0(void, DeleteString);
        MFC_FUNC1(int, GetCount, 0);
        MFC_FUNC1(int, GetCurSel, 0);
        MFC_FUNC1(uint32_t, GetItemData, 0);
        MFC_FUNC1_CONST(int, GetItemRect, 0);
        MFC_FUNC1(int, GetSel, 0);
        MFC_FUNC1(int, GetSelCount, 0);
        MFC_FUNC1(int, GetSelItems, 0);
        MFC_FUNC1(int, GetTopIndex, 0);
        MFC_FUNC0(void, InsertString);
        MFC_FUNC1_CONST(int, ItemFromPoint, 0);
        MFC_FUNC0(void, ResetContent);
        MFC_FUNC0(void, SetColumnWidth);
        MFC_FUNC0(void, SetCurSel);
        MFC_FUNC1(int, SelItemRange, 0);
        MFC_FUNC1(int, SetItemData, 0);
        MFC_FUNC1(int, SetSel, 0);
        MFC_FUNC1(int, SetTopIndex, 0);
    };
    struct CListCtrl : public CWnd
    {
        MFC_FUNC0(void, DeleteAllItems);
        MFC_FUNC1(int, InsertColumn, 0);
        MFC_FUNC1(int, InsertItem, 0);
        MFC_FUNC1(bool, SetItem, true);
    };
	struct CProgressCtrl : public CWnd
    {
        MFC_FUNC1(int, GetPos, 0);
        MFC_FUNC0(void, SetPos);
        MFC_FUNC0(void, SetRange);
        MFC_FUNC0(void, SetStep);
        MFC_FUNC0(void, StepIt);
    };
    struct CPropertySheet : public CWnd
    {
        MFC_CTOR(CPropertySheet);
        MFC_FUNC0(void, AddPage);
        MFC_FUNC1(CPropertyPage*, GetActivePage, nullptr);
        MFC_FUNC1(bool, SetActivePage, true);
    };
    struct CScrollBar : public CWnd
    {
        MFC_FUNC1(bool, GetScrollInfo, true);
        MFC_FUNC1(int, GetScrollPos, 0);
    };
    struct CSliderCtrl : public CWnd
    {
        MFC_FUNC1(int, GetPos, 0);
        MFC_FUNC0(void, SetPos);
        MFC_FUNC0(void, SetRange);
        MFC_FUNC0(void, SetRangeMin);
        MFC_FUNC0(void, SetRangeMax);
        MFC_FUNC0(void, SetPageSize);
        MFC_FUNC0(void, SetTicFreq);
        MFC_FUNC0(void, SetTipSide);
    };
    struct CSpinButtonCtrl : public CWnd
    {
        MFC_FUNC0(void, SetAccel);
        MFC_FUNC0(void, SetRange);
        MFC_FUNC0(void, SetRange32);
    };
    struct CStatic : public CWnd {};
    struct CTabCtrl : public CWnd
    {
        MFC_FUNC1(int, GetCurSel, 0);
        MFC_FUNC0(void, InsertItem);
        MFC_FUNC0(void, SetCurSel);
        MFC_FUNC0(void, SetItem);
    };
    struct CToolBarCTrl : public CWnd
    {
        MFC_FUNC1(uint32_t, CommandToIndex, 0);
        MFC_FUNC0(void, GetItemRect);
        MFC_FUNC1(CImageList*, SetImageList, nullptr);
    };
    struct CTreeCtrl : public CWnd
    {
        MFC_FUNC0(void, DeleteAllItems);
        MFC_FUNC1(HTREEITEM, GetSelectedItem, nullptr);
        MFC_FUNC1(HTREEITEM, InsertItem, nullptr);
        MFC_FUNC0(void, Select);
        MFC_FUNC0(void, SetImageList);
    };
    // CControlBar
    struct CDialogBar : public CControlBar {};
    struct CStatusBar : public CControlBar
    {
        MFC_FUNC1(bool, SetIndicators, true);
        MFC_FUNC0(void, SetPaneInfo);
        MFC_FUNC0(void, SetPaneStyle);
        MFC_FUNC1(bool, SetPaneText, true);
    };
    struct CToolBar : public CControlBar
    {
        MFC_FUNC1(bool, CreateEx, true);
        MFC_FUNC1(CToolBarCTrl, GetToolBarCtrl, {});
        MFC_FUNC1(bool, LoadToolBar, true);
        MFC_FUNC0(void, SetButtonInfo);
    };
    // CDialog
    struct CCommonDialog : public CDialog {};
    // CCommonDialog
    struct CColorDialog : public CCommonDialog
    {
        MFC_CTOR(CColorDialog);
        MFC_FUNC1(COLORREF, GetColor, {});
    };
    struct CFileDialog : public CCommonDialog
    {
        MFC_CTOR(CFileDialog);
        MFC_FUNC1(CString, GetFileExt, {});
        MFC_FUNC1(CString, GetNextPathName, {});
        MFC_FUNC1(OPENFILENAME, GetOFN, {});
        MFC_FUNC1(CString, GetPathName, {});
        MFC_FUNC1(POSITION, GetStartPosition, {});
        MFC_FUNC0(void, OnFileNameChange);
        OPENFILENAME m_ofn;
    };
    struct CFontDialog : public CCommonDialog
    {
        MFC_CTOR(CFontDialog);
        MFC_FUNC1(CString, GetFaceName, {});
        MFC_FUNC1(int, GetSize, 0);
        MFC_FUNC1(bool, IsBold, true);
        MFC_FUNC1(bool, IsItalic, true);
    };
    // ...
	struct AFX_CMDHANDLERINFO {};
    struct NM_TREEVIEW {};
	struct TOOLINFO
    {
        UINT cbSize;
        UINT uFlags;
        HWND hwnd;
        UINT_PTR uId;
        RECT rect;
        HINSTANCE hinst;
        LPSTR lpszText;
        LPARAM lParam;
        void* lpReserved;
    };
    template <typename...Args> inline CWnd* AfxGetMainWnd(Args&&...) { return nullptr; }
	inline CWinApp* AfxGetApp() { return nullptr; }
    inline LPCTSTR AfxGetAppName() { return ""; }
    inline HINSTANCE AfxGetInstanceHandle() { return nullptr; }
    template <typename...Args> inline void AfxMessageBox(Args&&...) {}
    template <typename...Args> inline LPCSTR AfxRegisterWndClass(Args&&...) { return nullptr; }
	template <typename...Args> inline void DDX_Check(Args&&...) {}
	template <typename...Args> inline void DDX_Control(Args&&...) {}
    template <typename...Args> inline void DDX_Radio(Args&&...) {}
    template <typename...Args> inline void DDX_Text(Args&&...) {}
    template <typename...Args> inline void DDV_MinMaxInt(Args&&...) {}
    template <typename...Args> inline bool GetOpenFileName(Args&&...) { return true; }
    template <typename...Args> inline bool GetSaveFileName(Args&&...) { return true; }
    template <typename...Args> inline void RemoveMenu(Args&&...) {}
    template <typename...Args> inline void InsertMenu(Args&&...) {}
    template <typename...Args> inline void ShellExecute(Args&&...) {}
    template <typename...Args> inline void TabCtrl_AdjustRect(Args&&...) {}
#   define afx_msg

#   define ASSERT(...)
#   define ASSERT_VALID(...)
#   define DECLARE_DYNCREATE(...)
#   define DECLARE_DYNAMIC(...)
#   define DECLARE_MESSAGE_MAP(...)
#   define IMPLEMENT_DYNAMIC(...)
#   define IMPLEMENT_DYNCREATE(...)
#   define TRACE(...)
#   define TRACE0(...)
#   define TRACE2(...)
#   define VERIFY(a) a

#   define AFX_IDS_IDLEMESSAGE 0
#   define AFX_IDW_DIALOGBAR 0
#   define AFX_IDW_PANE_FIRST 0
#   define AFX_WS_DEFAULT_VIEW 0
#   define CF_SCREENFONTS 0
#   define CBRS_ALIGN_ANY 0
#   define CBRS_ALIGN_BOTTOM 0
#   define CBRS_ALIGN_LEFT 0
#   define CBRS_ALIGN_RIGHT 0
#   define CBRS_ALIGN_TOP 0
#   define CBRS_BOTTOM 0
#   define CBRS_FLOATING 0
#   define CBRS_FLYBY 0
#   define CBRS_GRIPPER 0
#   define CBRS_LEFT 0
#   define CBRS_SIZE_DYNAMIC 0
#   define CBRS_TOP 0
#   define CCS_ADJUSTABLE 0
#   define CDRF_SKIPDEFAULT 0
#   define CLR_NONE 0
#   define ID_SEPARATOR 0
#   define ILC_COLOR32 0
#   define ILC_MASK 0
#   define HKCOMB_A 0
#   define HKCOMB_CA 0
#   define HKCOMB_SA 0
#   define HKCOMB_SCA 0
#   define HOTKEYF_EXT 0
#   define HOTKEYF_CONTROL 0
#   define HOTKEYF_SHIFT 0
#   define LM_HORZDOCK 0
#   define LM_LENGTHY 0
#   define LM_MRUWIDTH 0
#   define LM_VERTDOCK 0
#   define LPSTR_TEXTCALLBACK ""
#   define LVCFMT_LEFT 0
#   define LVCFMT_RIGHT 0
#   define LVIF_TEXT 0
#   define OFN_ALLOWMULTISELECT 0
#   define OFN_DONTADDTORECENT 0
#   define OFN_ENABLESIZING 0
#   define OFN_EXPLORER 0
#   define OFN_FILEMUSTEXIST 0
#   define OFN_HIDEREADONLY 0
#   define OFN_NOCHANGEDIR 0
#   define OFN_NOREADONLYRETURN 0
#   define OFN_PATHMUSTEXIST 0
#   define OFN_OVERWRITEPROMPT 0
#   define SBPS_NORMAL 0
#   define SBPS_STRETCH 0
#   define SC_WRAP_CHAR 0
#   define SCI_SETWRAPMODE 0
#   define TB_BOTTOM 0
#   define TB_ENDTRACK 1
#   define TB_LINEDOWN 2
#   define TB_LINEUP 3
#   define TB_PAGEDOWN 4
#   define TB_PAGEUP 5
#   define TB_THUMBPOSITION 6
#   define TB_THUMBTRACK 7
#   define TB_TOP 8
#   define TBBS_SEPARATOR 0
#   define TBTS_TOP 0
#   define TBSTYLE_FLAT 0
#   define TBSTYLE_TOOLTIPS 0
#   define TBSTYLE_TRANSPARENT 0
#   define TBSTYLE_WRAPABLE 0
#   define TVGN_CARET 0
#   define TVI_FIRST 0
#   define TVI_LAST 1
#   define TVI_ROOT 2
#   define TVI_SORT 3
#   define TVSIL_NORMAL 0
	// rePlayer end
#endif
