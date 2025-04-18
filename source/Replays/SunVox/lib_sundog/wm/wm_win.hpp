/*
    wm_win.h - platform-dependent module : Windows
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include "win_res.h" //(IDI_ICON1) Must be in your project

#ifndef WM_TOUCH
    #define WM_TOUCH 0x0240
#endif
#ifndef WM_MOUSEWHEEL
    #define WM_MOUSEWHEEL 0x020A
#endif

const char* className = "SunDogEngine";
volatile bool windowClass_registered = false;

#ifdef OPENGL
    #include "wm_opengl.hpp"
#endif

void SetupPixelFormat( HDC hDC );
LRESULT APIENTRY WinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static int CreateWin( const char* name, HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow, window_manager* wm );
#ifdef DIRECTDRAW
    #include "wm_win_ddraw.hpp"
#endif

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;
    
    wm->mods = 0;
#ifdef FRAMEBUFFER
    wm->fb = 0;
#endif
    
    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

    xsize = sconfig_get_int_value( APP_CFG_WIN_XSIZE, xsize, 0 );
    ysize = sconfig_get_int_value( APP_CFG_WIN_YSIZE, ysize, 0 );
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;

#ifdef GDI
    wm->gdi_bitmap_info[ 0 ] = 888;
#endif

    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    win_angle_apply( wm );

    if( wm->screen_ppi == 0 ) wm->screen_ppi = 110;
    win_zoom_init( wm );
    win_zoom_apply( wm );

    if( CreateWin( name, wm->sd->hCurrentInst, wm->sd->hPreviousInst, wm->sd->lpszCmdLine, wm->sd->nCmdShow, wm ) ) //create main window
	return 1; //Error

    if( sconfig_get_int_value( APP_CFG_NOCURSOR, -1, 0 ) != -1 )
    {
        //Hide mouse pointer:
        ShowCursor( 0 );
    }

    //Create framebuffer:
#ifdef FRAMEBUFFER
    #ifdef DIRECTDRAW
	wm->fb = 0;
    #else
	wm->fb = (COLORPTR)smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
	wm->fb_xpitch = 1;
	wm->fb_ypitch = wm->screen_xsize;
    #endif
#endif

    return retval;
}

void device_end( window_manager* wm )
{
#ifdef OPENGL
    gl_deinit( wm );
    wglMakeCurrent( wm->hdc, 0 );
    wglDeleteContext( wm->hGLRC );
#endif

#ifdef DIRECTX
    dd_close( wm );
#endif

#ifdef FRAMEBUFFER
#ifndef DIRECTDRAW
    smem_free( wm->fb );
#endif
#endif

    if( !( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) )
    {
	WINDOWPLACEMENT p;
	smem_clear_struct( p );
	p.length = sizeof(WINDOWPLACEMENT);
	if( GetWindowPlacement( wm->hwnd, &p ) )
	{
	    if( p.showCmd == SW_SHOWMAXIMIZED )
		wm->flags |= WIN_INIT_FLAG_MAXIMIZED;
	    else
		wm->flags &= ~WIN_INIT_FLAG_MAXIMIZED;
	}
    }

    DestroyWindow( wm->hwnd );
}

void device_event_handler( window_manager* wm )
{
    MSG msg;

    bool pause = true;

    while( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) 
    {
	if( GetMessage( &msg, NULL, 0, 0 ) == 0 ) return; //Exit
	TranslateMessage( &msg );
	DispatchMessage( &msg );
	pause = false;
    }

    if( wm->events_count )
	pause = false;

    //if( pause )
	//stime_sleep( wm->frame_len );

    stime_ticks_t cur_t = stime_ticks();
    if( cur_t - wm->prev_frame_time >= wm->ticks_per_frame )
    {
	wm->frame_event_request = 1;
	wm->prev_frame_time = cur_t;
    }
    else
    {
	if( pause )
	{
	    int pause_ms = ( wm->ticks_per_frame - ( cur_t - wm->prev_frame_time ) ) * 1000 / stime_ticks_per_second();
	    stime_sleep( pause_ms );
	}
    }
}

#ifdef OPENGL
void SetupPixelFormat( HDC hDC, window_manager* wm )
{
    if( hDC == 0 ) return;

    unsigned int flags = 
	( 0
        | PFD_SUPPORT_OPENGL
        | PFD_DRAW_TO_WINDOW
	| PFD_DOUBLEBUFFER
	);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  /* size */
        1,                              /* version */
	flags,
        PFD_TYPE_RGBA,                  /* color type */
        16,                             /* prefered color depth */
        0, 0, 0, 0, 0, 0,               /* color bits (ignored) */
        0,                              /* no alpha buffer */
        0,                              /* alpha bits (ignored) */
        0,                              /* no accumulation buffer */
        0, 0, 0, 0,                     /* accum bits (ignored) */
        16,                             /* depth buffer */
        0,                              /* no stencil buffer */
        0,                              /* no auxiliary buffers */
        PFD_MAIN_PLANE,                 /* main layer */
        0,                              /* reserved */
        0, 0, 0,                        /* no layer, visible, damage masks */
    };
    int pixelFormat;

    pixelFormat = ChoosePixelFormat( hDC, &pfd );
    if( pixelFormat == 0 ) 
    {
        MessageBox( WindowFromDC( hDC ), "ChoosePixelFormat failed.", "Error", MB_ICONERROR | MB_OK );
        exit( 1 );
    }

    if( SetPixelFormat( hDC, pixelFormat, &pfd ) != TRUE ) 
    {
        MessageBox( WindowFromDC( hDC ), "SetPixelFormat failed.", "Error", MB_ICONERROR | MB_OK );
        exit( 1 );
    }
}
#endif

uint16_t win_key_to_sundog_key( uint16_t vk, uint16_t par )
{
    switch( vk )
    {
	case VK_BACK: return KEY_BACKSPACE; break;
	case VK_TAB: return KEY_TAB; break;
	case VK_RETURN: return KEY_ENTER; break;
	case VK_ESCAPE: return KEY_ESCAPE; break;

	case VK_F1: return KEY_F1; break;
	case VK_F2: return KEY_F2; break;
	case VK_F3: return KEY_F3; break;
	case VK_F4: return KEY_F4; break;
	case VK_F5: return KEY_F5; break;
	case VK_F6: return KEY_F6; break;
	case VK_F7: return KEY_F7; break;
	case VK_F8: return KEY_F8; break;
	case VK_F9: return KEY_F9; break;
	case VK_F10: return KEY_F10; break;
	case VK_F11: return KEY_F11; break;
	case VK_F12: return KEY_F12; break;
	case VK_UP: return KEY_UP; break;
	case VK_DOWN: return KEY_DOWN; break;
	case VK_LEFT: return KEY_LEFT; break;
	case VK_RIGHT: return KEY_RIGHT; break;
	case VK_INSERT: return KEY_INSERT; break;
	case VK_DELETE: return KEY_DELETE; break;
	case VK_HOME: return KEY_HOME; break;
	case VK_END: return KEY_END; break;
	case 33: return KEY_PAGEUP; break;
	case 34: return KEY_PAGEDOWN; break;
	case VK_CAPITAL: return KEY_CAPS; break;
	case VK_SHIFT: return KEY_SHIFT; break;
	case VK_CONTROL: return KEY_CTRL; break;
	case VK_MENU: return KEY_ALT; break;

	case 189: return '-'; break;
	case 187: return '='; break;
	case 219: return '['; break;
	case 221: return ']'; break;
	case 186: return ';'; break;
	case 222: return 0x27; break; // '
	case 188: return ','; break;
	case 190: return '.'; break;
	case 191: return '/'; break;
	case 220: return 0x5C; break; // |
	case 192: return '`'; break;

	case VK_NUMPAD0: return '0'; break;
	case VK_NUMPAD1: return '1'; break;
	case VK_NUMPAD2: return '2'; break;
	case VK_NUMPAD3: return '3'; break;
	case VK_NUMPAD4: return '4'; break;
	case VK_NUMPAD5: return '5'; break;
	case VK_NUMPAD6: return '6'; break;
	case VK_NUMPAD7: return '7'; break;
	case VK_NUMPAD8: return '8'; break;
	case VK_NUMPAD9: return '9'; break;
	case VK_MULTIPLY: return '*'; break;
	case VK_ADD: return '+'; break;
	case VK_SEPARATOR: return '/'; break;
	case VK_SUBTRACT: return '-'; break;
	case VK_DECIMAL: return '.'; break;
	case VK_DIVIDE: return '/'; break;
    }

    if( vk == 0x20 ) return vk;
    if( vk >= 0x30 && vk <= 0x39 ) return vk; //Numbers
    if( vk >= 0x41 && vk <= 0x5A ) return vk + 0x20; //Letters (lowercase)

    return KEY_UNKNOWN + vk;
}

#ifdef MULTITOUCH
LRESULT OnTouch( HWND hWnd, WPARAM wParam, LPARAM lParam, window_manager* wm )
{
    BOOL bHandled = FALSE;
    UINT cInputs = LOWORD( wParam );
    PTOUCHINPUT pInputs = (PTOUCHINPUT)smem_new( sizeof( TOUCHINPUT ) * cInputs );
    if( pInputs )
    {
	if( GetTouchInputInfo( (HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT)) )
	{
    	    for( UINT i = 0; i < cInputs; i++ )
    	    {
        	TOUCHINPUT* ti = &pInputs[ i ]; //x, y, dwID
        	int x = ti->x / 100;
        	int y = ti->y / 100;
		POINT point;
		point.x = x;
		point.y = y;
		ScreenToClient( hWnd, &point );
		x = point.x;
		y = point.y;
        	if( ti->dwFlags & TOUCHEVENTF_DOWN )
        	{
        	    collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, 1024, ti->dwID, wm );
        	    if( ti->dwFlags & TOUCHEVENTF_PRIMARY )
        		wm->ignore_mouse_evts = true;
        	}
        	if( ti->dwFlags & TOUCHEVENTF_MOVE )
        	{
        	    uint evt_flags = wm->mods;
        	    //if( i < cInputs - 1 )
        		//evt_flags |= EVT_FLAG_DONTDRAW;
        	    collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY, evt_flags, x, y, 1024, ti->dwID, wm );
        	    if( ti->dwFlags & TOUCHEVENTF_PRIMARY )
        		wm->ignore_mouse_evts = true;
        	}
        	if( ti->dwFlags & TOUCHEVENTF_UP )
        	{
        	    collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, 1024, ti->dwID, wm );
        	    if( ti->dwFlags & TOUCHEVENTF_PRIMARY )
        	    {
        		if( wm->ignore_mouse_evts )
        		{
        		    wm->ignore_mouse_evts = false;
        		    wm->ignore_mouse_evts_before = stime_ms() + 1000;
        		}
        	    }
        	}
            }
            send_touch_events( wm );
            bHandled = TRUE;
        }
        else
        {
    	    /* handle the error here */
        }
        smem_free( pInputs );
    }
    else
    {
        /* handle the error here, probably out of memory */
    }
    if( bHandled )
    {
        // if you handled the message, close the touch input handle and return
        CloseTouchInputHandle( (HTOUCHINPUT)lParam );
        return 0;
    }
    else
    {
        // if you didn't handle the message, let DefWindowProc handle it
        return DefWindowProc( hWnd, WM_TOUCH, wParam, lParam );
    }
}
#endif

#define GET_XY \
    x = (int16_t)( lParam & 0xFFFF );\
    y = (int16_t)( lParam >> 16 );

bool mouse_evt_ignore( window_manager* wm )
{
    if( wm->ignore_mouse_evts ) return true;
    if( wm->ignore_mouse_evts_before )
    {
	if( (unsigned)( wm->ignore_mouse_evts_before - stime_ms() ) < 1000 * 10 )
	    return true;
	else
	{
	    wm->ignore_mouse_evts_before = 0;
	    wm->ignore_mouse_evts = false;
	}
    }
    return false;
}

LRESULT APIENTRY
WinProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam )
{
    int rv = 0;
    int x, y, d;
    POINT point;

    window_manager* wm = (window_manager*)GetWindowLongPtr( hWnd, GWLP_USERDATA );

    switch( message )
    {
	case WM_CREATE:
	    break;

	case WM_CLOSE:
	    if( wm )
	    {
		send_event( 0, EVT_QUIT, wm );
	    }
	    break;

	case WM_SIZE:
	    if( wm && LOWORD(lParam) != 0 && HIWORD(lParam) != 0 )
	    {
		int xsize = (int)LOWORD( lParam );
		int ysize = (int)HIWORD( lParam );
		if( xsize <= 64 && ysize <= 64 ) break; //Wine fix
		wm->real_window_width = xsize;
		wm->real_window_height = ysize;
		wm->screen_xsize = xsize;
		wm->screen_ysize = ysize;
#if defined(GDI) && !defined(OPENGL)
		//The scan lines for SetDIBitsToDevice() must be aligned on a DWORD!
		if( COLORBITS == 16 )
		{
		    wm->screen_xsize &= ~1;
		}
		if( COLORBITS == 8 )
		{
		    wm->screen_xsize &= ~3;
		}
#endif
		win_zoom_apply( wm );
		win_angle_apply( wm );
		send_event( wm->root_win, EVT_SCREENRESIZE, wm );
#ifdef OPENGL
		if( wm->hGLRC )
            	    gl_resize( wm );
#endif
	    }
	    break;

	case WM_PAINT:
	    if( wm )
	    {
		PAINTSTRUCT gdi_ps;
		BeginPaint( hWnd, &gdi_ps );
		EndPaint( hWnd, &gdi_ps );
		send_event( wm->root_win, EVT_DRAW, wm );
		return 0;
	    }
	    break;

	case WM_MOUSEWHEEL:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		point.x = x;
		point.y = y;
		ScreenToClient( hWnd, &point );
		x = point.x;
		y = point.y;
		convert_real_window_xy( x, y, wm );
		int key = 0;
		d = (unsigned int)wParam >> 16;
		int16_t d2 = d;
		if( d2 < 0 ) key = MOUSE_BUTTON_SCROLLDOWN;
		if( d2 > 0 ) key = MOUSE_BUTTON_SCROLLUP;
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, key, 0, 1024, wm );
	    }
	    break;
	case WM_LBUTTONDOWN:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_LEFT, 0, 1024, wm );
		SetCapture( hWnd );
	    }
	    break;
	case WM_LBUTTONUP:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_LEFT, 0, 1024, wm );
		ReleaseCapture();
	    }
	    break;
	case WM_MBUTTONDOWN:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, wm );
		SetCapture( hWnd );
	    }
	    break;
	case WM_MBUTTONUP:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, wm );
		ReleaseCapture();
	    }
	    break;
	case WM_RBUTTONDOWN:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, wm );
	    }
	    break;
	case WM_RBUTTONUP:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, wm );
	    }
	    break;
	case WM_MOUSEMOVE:
	    if( wm )
	    {
		if( mouse_evt_ignore( wm ) ) break;
		GET_XY;
		convert_real_window_xy( x, y, wm );
		int key = 0;
		if( wParam & MK_LBUTTON ) key |= MOUSE_BUTTON_LEFT;
		if( wParam & MK_MBUTTON ) key |= MOUSE_BUTTON_MIDDLE;
		if( wParam & MK_RBUTTON ) key |= MOUSE_BUTTON_RIGHT;
		send_event( 0, EVT_MOUSEMOVE, wm->mods, x, y, key, 0, 1024, wm );
	    }
	    break;

#ifdef MULTITOUCH
	case WM_TOUCH:
	    if( wm )
	    {
		OnTouch( hWnd, wParam, lParam, wm );
	    }
	    break;
#endif

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	    if( wm )
	    {
		if( wParam == VK_SHIFT )
		    wm->mods |= EVT_FLAG_SHIFT;
		if( wParam == VK_CONTROL )
		    wm->mods |= EVT_FLAG_CTRL;
		if( wParam == VK_MENU )
		    wm->mods |= EVT_FLAG_ALT;
		int key = win_key_to_sundog_key( wParam, lParam );
		if( message == WM_SYSKEYDOWN && key != KEY_F10 ) 
		{
		    rv = 1;
		    break;
		}
		if( key ) 
		{
		    send_event( 0, EVT_BUTTONDOWN, wm->mods, 0, 0, key, ( lParam >> 16 ) & 511, 1024, wm );
		}
	    }
	    break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
	    if( wm )
	    {
		if( wParam == VK_SHIFT )
		    wm->mods &= ~EVT_FLAG_SHIFT;
		if( wParam == VK_CONTROL )
		    wm->mods &= ~EVT_FLAG_CTRL;
		if( wParam == VK_MENU )
		    wm->mods &= ~EVT_FLAG_ALT;
		int key = win_key_to_sundog_key( wParam, lParam );
		if( message == WM_SYSKEYUP && key != KEY_F10 ) 
		{
		    rv = 1;
		    break;
		}
		if( key ) 
		{
		    send_event( 0, EVT_BUTTONUP, wm->mods, 0, 0, key, ( lParam >> 16 ) & 511, 1024, wm );
		}
	    }
	    break;

	case WM_KILLFOCUS:
	    if( wm )
	    {
		wm->mods = 0;
	    }
	    break;

	default:
	    rv = 1;
	    break;
    }

    if( rv == 1 )
    {
	rv = DefWindowProc( hWnd, message, wParam, lParam );
    }

    return rv;
}

static int CreateWin( const char* name, HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow, window_manager* wm )
{
    //Create temp strings:
    uint16_t* windowName_utf16 = (uint16_t*)smem_new( 256 * sizeof( uint16_t ) );
    char* windowName_ansi = (char*)smem_new( 256 );
    utf8_to_utf16( windowName_utf16, 256, name );
    wcstombs( windowName_ansi, (const wchar_t*)windowName_utf16, 256 );
    
    //Register window class:
    if( windowClass_registered == false )
    {
	windowClass_registered = true;
	WNDCLASS wc;
#ifdef DIRECTDRAW
	wc.style = CS_BYTEALIGNCLIENT;
#else
	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
#endif
	wc.lpfnWndProc = WinProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hCurrentInst;
	wc.hIcon = LoadIcon( hCurrentInst, (LPCTSTR)IDI_ICON1 );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = className;
	if( RegisterClass( &wc ) == 0 )
	    windowClass_registered = false;
    }

    //Create window:
    RECT Rect;
    Rect.top = 0;
    Rect.bottom = wm->real_window_height;
    Rect.left = 0;
    Rect.right = wm->real_window_width;
    uint WS = 0;
    uint WS_EX = 0;
    if( wm->flags & WIN_INIT_FLAG_SCALABLE )
	WS = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    else
	WS = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    if( wm->flags & WIN_INIT_FLAG_NOBORDER )
	WS = WS_POPUP;
#ifdef DIRECTDRAW
    WS = WS_POPUP;
    WS_EX = WS_EX_TOPMOST;
#else
    if( wm->flags & WIN_INIT_FLAG_FULLSCREEN )
    {
	WS_EX = WS_EX_APPWINDOW;
        WS = WS_POPUP;
    	Rect.right = GetSystemMetrics( SM_CXSCREEN );
    	Rect.bottom = GetSystemMetrics( SM_CYSCREEN );
    	wm->screen_xsize = Rect.right;
    	wm->screen_ysize = Rect.bottom;
    	wm->real_window_width = wm->screen_xsize;
    	wm->real_window_height = wm->screen_ysize;
	win_zoom_apply( wm );
	win_angle_apply( wm );
    }
    else
    {
	if( wm->flags & WIN_INIT_FLAG_MAXIMIZED ) nCmdShow = SW_SHOWMAXIMIZED;
    }
#endif
    AdjustWindowRectEx( &Rect, WS, 0, WS_EX );
    wm->hwnd = CreateWindowEx(
	WS_EX,
        className, windowName_ansi,
        WS,
        ( GetSystemMetrics( SM_CXSCREEN ) - ( Rect.right - Rect.left ) ) / 2, 
        ( GetSystemMetrics( SM_CYSCREEN ) - ( Rect.bottom - Rect.top ) ) / 2, 
        Rect.right - Rect.left, Rect.bottom - Rect.top,
        NULL, NULL, hCurrentInst, NULL
    );

    //Remove temp strings:
    smem_free( windowName_utf16 ); windowName_utf16 = NULL;
    smem_free( windowName_ansi ); windowName_ansi = NULL;

    //Init WM pointer:
    wm->hdc = GetDC( wm->hwnd );
    SetWindowLongPtr( wm->hwnd, GWLP_USERDATA, (LONG_PTR)wm );
    
#ifdef MULTITOUCH
    //Init touch device:
    int v = GetSystemMetrics( SM_DIGITIZER );
    if( v & NID_READY )
    {
	RegisterTouchWindow( wm->hwnd, 0 );
    }
#endif
    
    //Display window:
    ShowWindow( wm->hwnd, nCmdShow );
    UpdateWindow( wm->hwnd );

#ifdef OPENGL
    SetupPixelFormat( wm->hdc, wm );
    wm->hGLRC = wglCreateContext( wm->hdc );
    wglMakeCurrent( wm->hdc, wm->hGLRC );
    if( gl_init( wm ) )
	return 1; //error
    gl_resize( wm );
#endif
#ifdef DIRECTDRAW
    if( dd_init( fullscreen, wm ) ) 
	return 1; //error
#endif

    return 0;
}

void device_screen_unlock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter == 1 )
    {
#ifdef DIRECTDRAW
	if( g_primary_locked )
	{
	    lpDDSPrimary->Unlock( g_sd.lpSurface );
	    g_primary_locked = 0;
	    wm->fb = 0;
	}
#endif //DIRECTDRAW
	gl_unlock( wm );
    }

    if( wm->screen_lock_counter > 0 )
    {
	wm->screen_lock_counter--;
    }

    if( wm->screen_lock_counter > 0 ) 
	wm->screen_is_active = true;
    else
	wm->screen_is_active = false;
}

void device_screen_lock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter == 0 )
    {
#ifdef DIRECTDRAW
	if( g_primary_locked == 0 )
	{
	    if( lpDDSPrimary )
	    {
		lpDDSPrimary->Lock( 0, &g_sd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, 0 );
		wm->fb_ypitch = g_sd.lPitch / COLORLEN;
		wm->fb = (COLORPTR)g_sd.lpSurface;
		g_primary_locked = 1;
	    }
	}
#endif //DIRECTDRAW
	gl_lock( wm );
    }
    wm->screen_lock_counter++;
    
    if( wm->screen_lock_counter > 0 ) 
	wm->screen_is_active = true;
    else
	wm->screen_is_active = false;
}

void device_change_name( const char* name, window_manager* wm )
{
    if( !name ) return;
    int len = strlen( name ) + 1;
    uint16_t* name16 = (uint16_t*)smem_new( len * sizeof( uint16_t ) );
    utf8_to_utf16( name16, len, name );
    SetWindowTextW( wm->hwnd, (LPCWSTR)name16 );
    smem_free( name16 );
}

#ifndef OPENGL

void device_vsync( bool vsync, window_manager* wm )
{
}

void device_redraw_framebuffer( window_manager* wm ) 
{
    //stime_sleep(100); //simulate slow computer
}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    if( !wm->hdc ) return;
    HPEN pen;
    pen = CreatePen( PS_SOLID, 1, RGB( red( color ), green( color ), blue( color ) ) );
    SelectObject( wm->hdc, pen );
    MoveToEx( wm->hdc, x1, y1, 0 );
    LineTo( wm->hdc, x2, y2 );
    SetPixel( wm->hdc, x2, y2, RGB( red( color ), green( color ), blue( color ) ) );
    DeleteObject( pen );
}

void device_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    if( !wm->hdc ) return;
    if( xsize == 1 && ysize == 1 )
    {
	SetPixel( wm->hdc, x, y, RGB( red( color ), green( color ), blue( color ) ) );
    }
    else
    {
	HPEN pen = CreatePen( PS_SOLID, 1, RGB( red( color ), green( color ), blue( color ) ) );
	HBRUSH brush = CreateSolidBrush( RGB( red( color ), green( color ), blue( color ) ) );
	SelectObject( wm->hdc, pen );
	SelectObject( wm->hdc, brush );
	Rectangle( wm->hdc, x, y, x + xsize, y + ysize );
	DeleteObject( brush );
	DeleteObject( pen );
    }
}

void device_draw_image( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm )
{
    if( !wm->hdc ) return;
    BITMAPINFO* bi = (BITMAPINFO*)wm->gdi_bitmap_info;
    if( wm->gdi_bitmap_info[ 0 ] == 888 )
    {
	memset( bi, 0, sizeof( wm->gdi_bitmap_info ) );
	//Set 256 colors palette:
	int a;
#ifdef GRAYSCALE
	for( a = 0; a < 256; a++ ) 
	{ 
    	    bi->bmiColors[ a ].rgbRed = a; 
    	    bi->bmiColors[ a ].rgbGreen = a; 
    	    bi->bmiColors[ a ].rgbBlue = a; 
	}
#else
	for( a = 0; a < 256; a++ ) 
	{ 
    	    bi->bmiColors[ a ].rgbRed = (a<<5)&224; 
	    if( bi->bmiColors[ a ].rgbRed ) 
		bi->bmiColors[ a ].rgbRed |= 0x1F; 
	    bi->bmiColors[ a ].rgbReserved = 0;
	}
	for( a = 0; a < 256; a++ )
	{
	    bi->bmiColors[ a ].rgbGreen = (a<<2)&224; 
	    if( bi->bmiColors[ a ].rgbGreen ) 
		bi->bmiColors[ a ].rgbGreen |= 0x1F; 
	}
	for( a = 0; a < 256; a++ ) 
	{ 
	    bi->bmiColors[ a ].rgbBlue = (a&192);
	    if( bi->bmiColors[ a ].rgbBlue ) 
		bi->bmiColors[ a ].rgbBlue |= 0x3F; 
	}
#endif
    }
    int src_xs = img->xsize;
    int src_ys = img->ysize;
    COLORPTR data = (COLORPTR)img->data;
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = src_xs;
    bi->bmiHeader.biHeight = -src_ys;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = COLORBITS;
    bi->bmiHeader.biCompression = BI_RGB; //RGB 555
    SetDIBitsToDevice(  
	wm->hdc,
	dest_x, // Destination top left hand corner X Position
	dest_y, // Destination top left hand corner Y Position
	dest_xs, // Destinations Width
	dest_ys, // Desitnations height
	src_x, // Source low left hand corner's X Position
	src_ys - ( src_y + dest_ys ), // Source low left hand corner's Y Position
	0,
	src_ys,
	data, // Source's data
	(BITMAPINFO*)wm->gdi_bitmap_info, // Bitmap Info
	DIB_RGB_COLORS );
}

#endif //!OPENGL
