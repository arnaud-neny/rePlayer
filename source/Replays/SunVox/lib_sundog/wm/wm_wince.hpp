/*
    wm_wince.h - platform-dependent module : Windows CE
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include <wingdi.h>
#include <aygshell.h>
#include "win_res.h" //(IDI_ICON1) Must be defined in your project

#define GETRAWFRAMEBUFFER   0x00020001
#define FORMAT_565 1
#define FORMAT_555 2
#define FORMAT_OTHER 3
typedef struct _RawFrameBufferInfo
{
   WORD wFormat;
   WORD wBPP;
   VOID* pFramePointer;
   int  cxStride;
   int  cyStride;
   int  cxPixels;
   int  cyPixels;
} RawFrameBufferInfo;

WCHAR* className = L"SunDogEngine";
const char* windowName = "SunDogEngine_window";
volatile bool windowClass_registered = false;

HANDLE systemIdleTimerThread = 0;

void SetupPixelFormat( HDC hDC );
LRESULT APIENTRY WinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static int CreateWin( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow, window_manager* wm );

#include "gx_loader.h"
tGXOpenDisplay GXOpenDisplay = 0;
tGXCloseDisplay GXCloseDisplay = 0;
tGXBeginDraw GXBeginDraw = 0;
tGXEndDraw GXEndDraw = 0;
tGXOpenInput GXOpenInput = 0;
tGXCloseInput GXCloseInput = 0;
tGXGetDisplayProperties GXGetDisplayProperties = 0;
tGXGetDefaultKeys GXGetDefaultKeys = 0;
tGXSuspend GXSuspend = 0;
tGXResume GXResume = 0;
tGXSetViewport GXSetViewport = 0;
GXDisplayProperties gx_dp;

typedef void (*tSystemIdleTimerReset)( void );
tSystemIdleTimerReset SD_SystemIdleTimerReset = 0;

DWORD WINAPI SystemIdleTimerProc( LPVOID lpParameter )
{
    while( 1 )
    {
	stime_sleep( 1000 * 10 );
	if( SD_SystemIdleTimerReset )
	    SD_SystemIdleTimerReset();
    }
    ExitThread( 0 );
    return 0;
}

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;
    FARPROC proc;

    systemIdleTimerThread = 0;
    wm->mods = 0;
    wm->wince_zoom_buffer = 0;
    wm->fb = 0;
    GXOpenDisplay = 0;
    GXCloseDisplay = 0;
    GXBeginDraw = 0;
    GXEndDraw = 0;
    GXOpenInput = 0;
    GXCloseInput = 0;
    GXGetDisplayProperties = 0;
    GXGetDefaultKeys = 0;
    GXSuspend = 0;
    GXResume = 0;
    GXSetViewport = 0;
    SD_SystemIdleTimerReset = 0;

    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

    wm->flags |= WIN_INIT_FLAG_FULLSCREEN;

    if( name ) windowName = name;

    wm->gdi_bitmap_info[ 0 ] = 888;

    //Get OS version:
    OSVERSIONINFO ver;
    GetVersionEx( &ver );
    wm->os_version = ver.dwMajorVersion;

    char* vd_str = sconfig_get_str_value( APP_CFG_VIDEODRIVER, "gapi", 0 );
    wm->vd = VIDEODRIVER_GAPI;
    if( vd_str )
    {
	slog( "Videodriver: %s\n", vd_str );
	if( smem_strcmp( vd_str, "raw" ) == 0 )
	    wm->vd = VIDEODRIVER_RAW;
	if( smem_strcmp( vd_str, "gdi" ) == 0 )
	    wm->vd = VIDEODRIVER_GDI;
    }
    else
    {
	slog( "No videodriver selected. Default: gapi\n" );
    }
    
    //Get real screen resolution:
    xsize = GetSystemMetrics( SM_CXSCREEN );
    ysize = GetSystemMetrics( SM_CYSCREEN );
    if( xsize > 320 && ysize > 320 )
	wm->hires = 1;
    else
	wm->hires = 0;

    if( ysize > xsize ) wm->screen_angle = ( wm->screen_angle + 1 ) & 3;

    //Get user defined resolution:
    xsize = sconfig_get_int_value( APP_CFG_WIN_XSIZE, xsize, 0 );
    ysize = sconfig_get_int_value( APP_CFG_WIN_YSIZE, ysize, 0 );

    //Save it:
    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;

    if( wm->vd == VIDEODRIVER_GAPI )
    {
	HMODULE gapiLibrary;
	FARPROC proc;
	gapiLibrary = LoadLibrary( TEXT( "gx.dll" ) );
	if( gapiLibrary == 0 ) 
	{
	    MessageBox( wm->hwnd, L"GX not found", L"Error", MB_OK );
	    slog( "ERROR: GX.dll not found\n" );
	    return 1;
	}
	IMPORT( gapiLibrary, proc, tGXOpenDisplay, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z", GXOpenDisplay );
	IMPORT( gapiLibrary, proc, tGXGetDisplayProperties, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ", GXGetDisplayProperties );
	IMPORT( gapiLibrary, proc, tGXOpenInput,"?GXOpenInput@@YAHXZ", GXOpenInput );
	IMPORT( gapiLibrary, proc, tGXCloseDisplay, "?GXCloseDisplay@@YAHXZ", GXCloseDisplay );
	IMPORT( gapiLibrary, proc, tGXBeginDraw, "?GXBeginDraw@@YAPAXXZ", GXBeginDraw );
	IMPORT( gapiLibrary, proc, tGXEndDraw, "?GXEndDraw@@YAHXZ", GXEndDraw );
	IMPORT( gapiLibrary, proc, tGXCloseInput,"?GXCloseInput@@YAHXZ", GXCloseInput );
	IMPORT( gapiLibrary, proc, tGXSetViewport,"?GXSetViewport@@YAHKKKK@Z", GXSetViewport );
	IMPORT( gapiLibrary, proc, tGXSuspend, "?GXSuspend@@YAHXZ", GXSuspend );
	IMPORT( gapiLibrary, proc, tGXResume, "?GXResume@@YAHXZ", GXResume );
	if( GXOpenDisplay == 0 ) retval = 1;
	if( GXGetDisplayProperties == 0 ) retval = 2;
	if( GXOpenInput == 0 ) retval = 3;
	if( GXCloseDisplay == 0 ) retval = 4;
	if( GXBeginDraw == 0 ) retval = 5;
	if( GXEndDraw == 0 ) retval = 6;
	if( GXCloseInput == 0 ) retval = 7;
	if( GXSuspend == 0 ) retval = 8;
	if( GXResume == 0 ) retval = 9;
	if( retval )
	{
	    MessageBox( wm->hwnd, L"Some GX functions not found", L"Error", MB_OK );
	    slog( "ERROR: some GX functions not found (%d)\n", retval );
	    return retval;
	}
    }

    CreateWin( wm->sd->hCurrentInst, wm->sd->hPreviousInst, (char*)wm->sd->lpszCmdLine, wm->sd->nCmdShow, wm ); //create main window

    if( wm->vd == VIDEODRIVER_GAPI )
    {
	wm->gx_suspended = 0;
	if( GXOpenDisplay( wm->hwnd, GX_FULLSCREEN ) == 0 )
	{
	    MessageBox( wm->hwnd, L"Can't open GAPI display", L"Error", MB_OK );
	    slog( "ERROR: Can't open GAPI display\n" );
	    return 1;
	}
	else
        {
	    gx_dp = GXGetDisplayProperties();
	    slog( "GAPI Width: %d\n", gx_dp.cxWidth );
	    slog( "GAPI Height: %d\n", gx_dp.cyHeight );
	    {
		//Screen resolution must be less or equal to GAPI resolution:
		if( wm->screen_xsize >= gx_dp.cxWidth )
		    wm->screen_xsize = gx_dp.cxWidth;
		if( wm->screen_ysize >= gx_dp.cyHeight )
		    wm->screen_ysize = gx_dp.cyHeight;
	    }
	    slog( "GAPI cbxPitch: %d\n", gx_dp.cbxPitch );
	    slog( "GAPI cbyPitch: %d\n", gx_dp.cbyPitch );
	    slog( "GAPI cBPP: %d\n", gx_dp.cBPP );
	    if( gx_dp.ffFormat & kfLandscape ) slog( "kfLandscape\n" );
	    if( gx_dp.ffFormat & kfPalette ) slog( "kfPalette\n" );
	    if( gx_dp.ffFormat & kfDirect ) slog( "kfDirect\n" );
	    if( gx_dp.ffFormat & kfDirect555 ) slog( "kfDirect555\n" );
	    if( gx_dp.ffFormat & kfDirect565 ) slog( "kfDirect565\n" );
	    if( gx_dp.ffFormat & kfDirect888 ) slog( "kfDirect888\n" );
	    if( gx_dp.ffFormat & kfDirect444 ) slog( "kfDirect444\n" );
	    if( gx_dp.ffFormat & kfDirectInverted ) slog( "kfDirectInverted\n" );
	    wm->fb_xpitch = gx_dp.cbxPitch;
	    wm->fb_ypitch = gx_dp.cbyPitch;
	    wm->fb_xpitch /= COLORLEN;
	    wm->fb_ypitch /= COLORLEN;
    	    GXSetViewport( 0, gx_dp.cyHeight, 0, 0 );
	    GXOpenInput();
	}
    }
    if( wm->vd == VIDEODRIVER_RAW )
    {
	RawFrameBufferInfo* rfb = (RawFrameBufferInfo*)wm->rfb;
	HDC hdc = GetDC( NULL );
	int rv = ExtEscape( hdc, GETRAWFRAMEBUFFER, 0, NULL, sizeof( RawFrameBufferInfo ), (char *)rfb );
	ReleaseDC( NULL, hdc );
	if( rv < 0 )
	{
	    MessageBox( wm->hwnd, L"Can't open raw framebuffer", L"Error", MB_OK );
	    slog( "ERROR: Can't open raw framebuffer\n" );
	    return rv;
	}
	slog( "RFB wFormat:\n" );
	if( rfb->wFormat == FORMAT_565 ) slog( "FORMAT_565\n" );
	else if( rfb->wFormat == FORMAT_555 ) slog( "FORMAT_555\n" );
	else 
	{
	    slog( "%d\n", rfb->wFormat );
	    MessageBox( wm->hwnd, L"Can't open raw framebuffer", L"Error", MB_OK );
	    return 1;
	}
	slog( "RFB wBPP: %d\n", rfb->wBPP );
	slog( "RFB cxStride: %d\n", rfb->cxStride );
	slog( "RFB cyStride: %d\n", rfb->cyStride );
	slog( "RFB cxPixels: %d\n", rfb->cxPixels );
	slog( "RFB cyPixels: %d\n", rfb->cyPixels );
	wm->fb_xpitch = rfb->cxStride;
	wm->fb_ypitch = rfb->cyStride;
	wm->fb_xpitch /= COLORLEN;
	wm->fb_ypitch /= COLORLEN;
	wm->raw_fb_xpitch = wm->fb_xpitch;
	wm->raw_fb_ypitch = wm->fb_ypitch;
    }
    if( wm->vd == VIDEODRIVER_GDI )
    {
	//Create virtual framebuffer:
	if( wm->hires )
	{
	    wm->screen_xsize /= 2;
	    wm->screen_ysize /= 2;
	}
	wm->fb = (COLORPTR)smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
	wm->fb_xpitch = 1;
	wm->fb_ypitch = wm->screen_xsize;
    }
    
    wm->real_window_width = wm->screen_xsize;
    wm->real_window_height = wm->screen_ysize;
    
    if( wm->screen_ppi == 0 )
    {
	if( wm->screen_xsize <= 320 || wm->screen_ysize <= 320 )
	    wm->screen_ppi = 100;
	else
	    wm->screen_ppi = 200;
    }
    if( wm->screen_scale == 0 )
    {
	wm->screen_scale = 160.0F / 256.0F; //HACK for SunVox
    }
    
    win_zoom_init( wm );
    
    if( wm->vd != VIDEODRIVER_RAW ) wm->screen_zoom = 1;
    if( wm->screen_zoom > 1 )
    {
	win_zoom_apply( wm );
	wm->wince_zoom_buffer = smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
	wm->fb_xpitch = 1;
	wm->fb_ypitch = wm->screen_xsize;
    }

    if( wm->screen_angle == 1 )
    {
	//Rotate the screen by 90:
	wm->fb_offset = wm->fb_ypitch * ( wm->screen_ysize - 1 );
	int ttt = wm->fb_ypitch;
	wm->fb_ypitch = wm->fb_xpitch;
	wm->fb_xpitch = -ttt;
	ttt = wm->screen_xsize; wm->screen_xsize = wm->screen_ysize; wm->screen_ysize = ttt;
    }
    if( wm->screen_angle == 2 )
    {
	//Rotate the screen by 180:
	wm->fb_offset = wm->fb_ypitch * ( wm->screen_ysize - 1 ) + wm->fb_xpitch * ( wm->screen_xsize - 1 );
	wm->fb_ypitch = -wm->fb_ypitch;
	wm->fb_xpitch = -wm->fb_xpitch;
    }
    if( wm->screen_angle == 3 )
    {
	//Rotate the screen by 270:
	wm->fb_offset = wm->fb_xpitch * ( wm->screen_xsize - 1 );
	int ttt = wm->fb_ypitch;
	wm->fb_ypitch = -wm->fb_xpitch;
	wm->fb_xpitch = ttt;
	ttt = wm->screen_xsize; wm->screen_xsize = wm->screen_ysize; wm->screen_ysize = ttt;
    }
    
    HMODULE coreLibrary;
    coreLibrary = LoadLibrary( TEXT( "coredll.dll" ) );
    if( coreLibrary == 0 ) 
    {
	slog( "ERROR: coredll.dll not found\n" );
    }
    else
    {
	IMPORT( coreLibrary, proc, tSystemIdleTimerReset, "SystemIdleTimerReset", SD_SystemIdleTimerReset );
	if( SD_SystemIdleTimerReset == 0 )
	    slog( "SystemIdleTimerReset() not found\n" );
    }
    systemIdleTimerThread = (HANDLE)CreateThread( NULL, 0, SystemIdleTimerProc, NULL, 0, NULL );
    
    return retval;
}

void device_end( window_manager* wm )
{
    if( systemIdleTimerThread )
	CloseHandle( systemIdleTimerThread );

    if( wm->vd == VIDEODRIVER_GDI && wm->fb )
    {
	smem_free( wm->fb );
	wm->fb = 0;
    }
    
    if( wm->vd == VIDEODRIVER_GAPI && wm->fb )
    {
        if( wm->gx_suspended == 0 )
        {
            GXEndDraw();
            wm->fb = 0;
        }
    }

    SHFullScreen( wm->hwnd, SHFS_SHOWTASKBAR | SHFS_SHOWSTARTICON | SHFS_SHOWSIPBUTTON );

    DestroyWindow( wm->hwnd );
    stime_sleep( 400 );
    
    smem_free( wm->wince_zoom_buffer );
}

void device_event_handler( window_manager* wm )
{
    MSG msg;
    
    if( wm->vd == VIDEODRIVER_GAPI && wm->fb )
    {
	if( wm->screen_lock_counter == 0 && wm->gx_suspended == 0 )
	{
	    GXEndDraw();
	    wm->fb = 0;
	}
    }
    
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

void SetupPixelFormat( HDC hDC )
{
    if( hDC == 0 ) return;
}

uint16_t win_key_to_sundog_key( uint16_t vk, window_manager* wm )
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
	case VK_UP: 
	    switch( wm->screen_angle )
	    {
		case 0: return KEY_UP; break;
		case 1: return KEY_RIGHT; break;
		case 2: return KEY_DOWN; break;
		case 3: return KEY_LEFT; break;
	    }
	    break;
	case VK_DOWN: 
	    switch( wm->screen_angle )
	    {
		case 0: return KEY_DOWN; break;
		case 1: return KEY_LEFT; break;
		case 2: return KEY_UP; break;
		case 3: return KEY_RIGHT; break;
	    }
	    break;
	case VK_LEFT: 
	    switch( wm->screen_angle )
	    {
		case 0: return KEY_LEFT; break;
		case 1: return KEY_UP; break;
		case 2: return KEY_RIGHT; break;
		case 3: return KEY_DOWN; break;
	    }
	    break;
	case VK_RIGHT: 
	    switch( wm->screen_angle )
	    {
		case 0: return KEY_RIGHT; break;
		case 1: return KEY_DOWN; break;
		case 2: return KEY_LEFT; break;
		case 3: return KEY_UP; break;
	    }
	    break;
	case VK_INSERT: return KEY_INSERT; break;
	case VK_DELETE: return KEY_DELETE; break;
	case VK_HOME: return KEY_HOME; break;
	case VK_END: return KEY_END; break;
	case 33: return KEY_PAGEUP; break;
	case 34: return KEY_PAGEDOWN; break;
	case VK_CAPITAL: return KEY_CAPS; break;
	case VK_SHIFT: return KEY_SHIFT; break;
	case VK_CONTROL: return KEY_CTRL; break;

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
	    
	case 195: return KEY_ESCAPE; break; //OK
    }
    
    if( vk == 0x20 ) return vk;
    if( vk >= 0x30 && vk <= 0x39 ) return vk; //Numbers
    if( vk >= 0x41 && vk <= 0x5A ) return vk; //Letters (capital)
    if( vk >= 0x61 && vk <= 0x7A ) return vk; //Letters (small)

    return KEY_UNKNOWN + vk;
}

#define GET_XY \
    x = lParam & 0xFFFF;\
    y = lParam>>16;

#define CONVERT_XY \
    /*Real coordinates -> window_manager coordinates*/\
    if( wm->vd == VIDEODRIVER_GAPI || wm->vd == VIDEODRIVER_GDI ) \
    { \
	if( wm->hires ) \
	{ \
	    x /= 2; \
	    y /= 2; \
	} \
    } \
    convert_real_window_xy( x, y, wm );

LRESULT APIENTRY
WinProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    int x, y, d;
    short d2;
    int key;
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

	case WM_DESTROY:
	    if( wm )
	    {
		if( wm->vd == VIDEODRIVER_GAPI )
		{
		    GXCloseInput();
		    GXCloseDisplay();
		}
	    }
	    PostQuitMessage( 0 );
	    break;

	case WM_SIZE:
	    if( wm )
	    {
#ifndef FRAMEBUFFER
		if( LOWORD(lParam) != 0 && HIWORD(lParam) != 0 )
		{
		    wm->screen_xsize = (int) LOWORD(lParam);
		    wm->screen_ysize = (int) HIWORD(lParam);
		    win_zoom_apply( wm );
		    win_angle_apply( wm );
		    send_event( wm->root_win, EVT_SCREENRESIZE, wm );
		}
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

	case WM_DEVICECHANGE:
	    if( wm && wParam == 0x8000 ) //DBT_DEVICEARRIVAL
	    {
		//It may be device power ON:
		send_event( wm->root_win, EVT_DRAW, wm );
	    }
	    break;

	case WM_KILLFOCUS:
	    if( wm )
	    {	
		if( wm->vd == VIDEODRIVER_GAPI )
		{
	    	    if( wm->screen_lock_counter > 0 )
			GXEndDraw();
		    GXSuspend();
		}
    		wm->gx_suspended = 1;
		send_event( wm->root_win, EVT_SCREENUNFOCUS, wm );
	    }
	    break;
	case WM_SETFOCUS:
	    if( wm )
	    {
		if( wm->gx_suspended )
		{
		    if( wm->vd == VIDEODRIVER_GAPI )
		    {
			if( wm->screen_lock_counter > 0 )
			{
		    	    wm->fb = (COLORPTR)GXBeginDraw();
			}
			GXResume();
		    }
    		    wm->gx_suspended = 0;
		}
		//Redraw all:
		send_event( wm->root_win, EVT_SCREENFOCUS, wm );
		send_event( wm->root_win, EVT_DRAW, wm );
	    }
	    //Hide taskbar and another system windows:
	    SHFullScreen( hWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON | SHFS_HIDESIPBUTTON );
	    break;

	case 0x020A: //WM_MOUSEWHEEL
	    if( wm )
	    {
		GET_XY;
		point.x = x;
		point.y = y;
		ScreenToClient( hWnd, &point );
		x = point.x;
		y = point.y;
		CONVERT_XY;
		key = 0;
		d = (unsigned int)wParam >> 16;
		d2 = (short)d;
		if( d2 < 0 ) key = MOUSE_BUTTON_SCROLLDOWN;
		if( d2 > 0 ) key = MOUSE_BUTTON_SCROLLUP;
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, key, 0, 1024, wm );
	    }
	    break;
	case WM_LBUTTONDOWN:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_LEFT, 0, 1024, wm );
	    }
	    break;
	case WM_LBUTTONUP:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_LEFT, 0, 1024, wm );
	    }
	    break;
	case WM_MBUTTONDOWN:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, wm );
	    }
	    break;
	case WM_MBUTTONUP:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_MIDDLE, 0, 1024, wm );
	    }
	    break;
	case WM_RBUTTONDOWN:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, wm );
	    }
	    break;
	case WM_RBUTTONUP:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, MOUSE_BUTTON_RIGHT, 0, 1024, wm );
	    }
	    break;
	case WM_MOUSEMOVE:
	    if( wm )
	    {
		GET_XY;
		CONVERT_XY;
		key = 0;
        	if( wParam & MK_LBUTTON ) key |= MOUSE_BUTTON_LEFT;
        	if( wParam & MK_MBUTTON ) key |= MOUSE_BUTTON_MIDDLE;
        	if( wParam & MK_RBUTTON ) key |= MOUSE_BUTTON_RIGHT;
        	send_event( 0, EVT_MOUSEMOVE, wm->mods, x, y, key, 0, 1024, wm );
    	    }
	    break;
	case WM_KEYDOWN:
	    if( wm )
	    {
		if( wParam == VK_SHIFT )
		    wm->mods |= EVT_FLAG_SHIFT;
		if( wParam == VK_CONTROL )
		    wm->mods |= EVT_FLAG_CTRL;
		key = win_key_to_sundog_key( wParam, wm );
		if( key ) 
		{
		    send_event( 0, EVT_BUTTONDOWN, wm->mods, 0, 0, key, ( lParam >> 16 ) & 511, 1024, wm );
		}
	    }
	    break;
	case WM_KEYUP:
	    if( wm )
	    {
		if( wParam == VK_SHIFT )
		    wm->mods &= ~EVT_FLAG_SHIFT;
		if( wParam == VK_CONTROL )
		    wm->mods &= ~EVT_FLAG_CTRL;
		key = win_key_to_sundog_key( wParam, wm );
		if( key ) 
		{
		    send_event( 0, EVT_BUTTONUP, wm->mods, 0, 0, key, ( lParam >> 16 ) & 511, 1024, wm );
		}
	    }
	    break;
	    
	default:
	    return DefWindowProc( hWnd, message, wParam, lParam );
	    break;
    }

    return 0;
}

static int CreateWin( HINSTANCE hCurrentInst, HINSTANCE hPreviousInst, LPSTR lpszCmdLine, int nCmdShow, window_manager* wm )
{
    uint16_t windowName_utf16[ 128 ];
    utf8_to_utf16( windowName_utf16, 128, windowName );

    //Register window class:
    if( windowClass_registered == false )
    {
        windowClass_registered = true;    
        WNDCLASS wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WinProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hCurrentInst;
        wc.hIcon = LoadIcon( hCurrentInst, MAKEINTRESOURCE( IDI_ICON1 ) );
        wc.hCursor = LoadCursor( NULL, IDC_ARROW );
        wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
        wc.lpszMenuName = NULL;
        wc.lpszClassName = className;
        if( RegisterClass( &wc ) == 0 )
            windowClass_registered = false;
    }

    //Create window:
    RECT Rect;
#ifndef FRAMEBUFFER
    Rect.top = 0;
    Rect.bottom = wm->screen_ysize;
    Rect.left = 0;
    Rect.right = wm->screen_xsize;
    AdjustWindowRectEx( &Rect, WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, 0, 0 );
    if( wm->flags & WIN_INIT_FLAG_SCALABLE )
    {
	wm->hwnd = CreateWindow(
	    className, windowName_utf16,
	    WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
	    ( GetSystemMetrics(SM_CXSCREEN) - (Rect.right - Rect.left) ) / 2, 
	    ( GetSystemMetrics(SM_CYSCREEN) - (Rect.bottom - Rect.top) ) / 2, 
	    Rect.right - Rect.left, Rect.bottom - Rect.top,
	    NULL, NULL, hCurrentInst, NULL
	);
    }
    else
    {
	wm->hwnd = CreateWindow(
	    className, windowName_utf16,
	    WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
	    ( GetSystemMetrics(SM_CXSCREEN) - (Rect.right - Rect.left) ) / 2, 
	    ( GetSystemMetrics(SM_CYSCREEN) - (Rect.bottom - Rect.top) ) / 2, 
	    Rect.right - Rect.left, Rect.bottom - Rect.top,
	    NULL, NULL, hCurrentInst, NULL
	);
    }
#else
    wm->hwnd = CreateWindow(
	className, (const WCHAR*)windowName_utf16,
	WS_VISIBLE,
	0, 0,
	GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ),
	NULL, NULL, hCurrentInst, NULL
    );
#endif //FRAMEBUFFER

    //Init WM pointer:
    wm->hdc = GetDC( wm->hwnd );
    SetWindowLongPtr( wm->hwnd, GWLP_USERDATA, (LONG_PTR)wm );

    //Hide taskbar and another system windows:
    SHFullScreen( wm->hwnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON | SHFS_HIDESIPBUTTON );

    //Display the window:
    ShowWindow( wm->hwnd, nCmdShow );
    UpdateWindow( wm->hwnd );
    SetActiveWindow( wm->hwnd );
    SetForegroundWindow( wm->hwnd );

    return 0;
}

#define VK_APP_LAUNCH1       0xC1
#define VK_APP_LAUNCH2       0xC2
#define VK_APP_LAUNCH3       0xC3
#define VK_APP_LAUNCH4       0xC4
#define VK_APP_LAUNCH5       0xC5
#define VK_APP_LAUNCH6       0xC6
#define VK_APP_LAUNCH7       0xC7
#define VK_APP_LAUNCH8       0xC8
#define VK_APP_LAUNCH9       0xC9
#define VK_APP_LAUNCH10      0xCA
#define VK_APP_LAUNCH11      0xCB
#define VK_APP_LAUNCH12      0xCC
#define VK_APP_LAUNCH13      0xCD
#define VK_APP_LAUNCH14      0xCE
#define VK_APP_LAUNCH15      0xCF

void device_screen_unlock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter > 0 )
    {
	wm->screen_lock_counter--;
    }

    if( wm->gx_suspended == 0 && wm->screen_lock_counter > 0 )
	wm->screen_is_active = true;
    else
	wm->screen_is_active = false;
}

void device_screen_lock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter == 0 && wm->gx_suspended == 0 )
    {
	if( wm->vd == VIDEODRIVER_GAPI )
	{
	    if( wm->fb == 0 )
		wm->fb = (COLORPTR)GXBeginDraw();
	}
	else if( wm->vd == VIDEODRIVER_RAW )
	{
	    if( wm->screen_zoom == 1 )
	    {
		RawFrameBufferInfo* rfb = (RawFrameBufferInfo*)wm->rfb;
		wm->fb = (COLORPTR)rfb->pFramePointer;
	    }
	    else 
	    {
		wm->fb = (COLORPTR)wm->wince_zoom_buffer;
	    }
	}
    }
    wm->screen_lock_counter++;
    
    if( wm->gx_suspended == 0 && wm->screen_lock_counter > 0 )
	wm->screen_is_active = true;
    else
	wm->screen_is_active = false;
}

void device_vsync( bool vsync, window_manager* wm )
{
}

void device_redraw_framebuffer( window_manager* wm ) 
{
    if( wm->screen_changed == 0 ) return;
    
    if( wm->flags & WIN_INIT_FLAG_FRAMEBUFFER )
    {
	if( wm->fb == 0 ) return;
	if( wm->vd == VIDEODRIVER_GDI )
	{
    	    wm->screen_changed = 0;
    	    int src_xsize = wm->screen_xsize;
    	    int src_ysize = wm->screen_ysize;
    	    if( wm->screen_angle == 1 || wm->screen_angle == 3 )
    	    {
        	int ttt = src_xsize;
        	src_xsize = src_ysize;
        	src_ysize = ttt;
    	    }
    	    BITMAPINFO bi;
    	    memset( &bi, 0, sizeof( BITMAPINFO ) );
    	    bi.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    	    bi.bmiHeader.biWidth = src_xsize;
    	    bi.bmiHeader.biHeight = -src_ysize;
    	    bi.bmiHeader.biPlanes = 1;
    	    bi.bmiHeader.biBitCount = COLORBITS;
    	    bi.bmiHeader.biCompression = BI_RGB;
    	    StretchDIBits(
        	wm->hdc,
        	0, // Destination top left hand corner X Position
        	0, // Destination top left hand corner Y Position
        	GetSystemMetrics( SM_CXSCREEN ), // Destinations Width
        	GetSystemMetrics( SM_CYSCREEN ), // Desitnations height
        	0, // Source low left hand corner's X Position
        	0, // Source low left hand corner's Y Position
        	src_xsize,
        	src_ysize,
        	wm->fb, // Source's data
        	&bi, // Bitmap Info
        	DIB_RGB_COLORS,
        	SRCCOPY );
	}
	if( wm->vd == VIDEODRIVER_RAW && wm->screen_zoom == 2 )
	{
    	    RawFrameBufferInfo* rfb = (RawFrameBufferInfo*)wm->rfb;
    	    COLORPTR s = (COLORPTR)rfb->pFramePointer;
    	    uint fb_ypitch = wm->raw_fb_ypitch;
    	    uint fb_xpitch = wm->raw_fb_xpitch;
    	    COLORPTR src = (COLORPTR)wm->wince_zoom_buffer;
    	    for( int y = 0; y < wm->real_window_height / 2; y++ )
    	    {
        	COLORPTR s2 = s;
        	for( int x = 0; x < wm->real_window_width / 2; x++ )
        	{
            	    COLOR c = *src;
            	    s2[ 0 ] = c;
            	    s2[ 1 ] = c;
            	    s2[ fb_ypitch ] = c;
            	    s2[ fb_ypitch + 1 ] = c;
            	    s2 += fb_xpitch * 2;
            	    src++;
        	}
        	s += fb_ypitch * 2;
    	    }
	}
	wm->screen_changed = 0;
    }
    else
    {
	//No framebuffer:
	wm->screen_changed = 0;
    }
}

void device_change_name( const char* name, window_manager* wm )
{
}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    if( wm->hdc == 0 ) return;
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
    if( wm->hdc == 0 ) return;
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
    if( wm->hdc == 0 ) return;
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
    bi->bmiHeader.biCompression = BI_RGB;
    int new_src_y;
    if( wm->os_version > 4 )
    {
	new_src_y = src_ys - ( src_y + dest_ys );
    }
    else
    {
	new_src_y = src_y;
    }
    SetDIBitsToDevice(  
	wm->hdc,
	dest_x, // Destination top left hand corner X Position
	dest_y,	// Destination top left hand corner Y Position
	dest_xs, // Destinations Width
	dest_ys, // Desitnations height
	src_x, // Source low left hand corner's X Position
	new_src_y, // Source low left hand corner's Y Position
	0,
	src_ys,
	data, // Source's data
	(BITMAPINFO*)wm->gdi_bitmap_info, // Bitmap Info
	DIB_RGB_COLORS );
}
