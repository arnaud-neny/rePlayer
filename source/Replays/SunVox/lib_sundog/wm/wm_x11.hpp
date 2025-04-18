/*
    wm_x11.h - platform-dependent module : X11 (X Window System) + OpenGL
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include <sched.h>	//sched_yield()

#ifdef OPENGL
    #ifdef OPENGLES
	#ifdef OS_RASPBERRY_PI
	    #include "bcm_host.h"
	#endif
    #else
        static int g_gl_snglBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, None };
	static int g_gl_dblBuf[] = { GLX_RGBA, GLX_DEPTH_SIZE, 16, GLX_DOUBLEBUFFER, None };
    #endif
    #include "wm_opengl.hpp"
#endif

const char* g_msg_id_x11_paste = "X11 Paste Buffer";

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;

    wm->main_loop_thread = pthread_self();
    XInitThreads();

    //Open a connection to the X server:
    const char* dname;
    if( ( dname = getenv( "DISPLAY" ) ) == NULL )
	dname = ":0";
    wm->dpy = XOpenDisplay( dname );
    if( wm->dpy == NULL )
    {
    	slog( "could not open display %s\n", dname );
	return 1;
    }
    int xscr = XDefaultScreen( wm->dpy );

    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

    xsize = sconfig_get_int_value( APP_CFG_WIN_XSIZE, xsize, 0 );
    ysize = sconfig_get_int_value( APP_CFG_WIN_YSIZE, ysize, 0 );
#if defined(OPENGL) || defined(OPENGLES)
#ifdef OS_RASPBERRY_PI
    wm->flags |= WIN_INIT_FLAG_FULLSCREEN;
#endif
#endif
    if( wm->flags & WIN_INIT_FLAG_FULLSCREEN )
    {
	int w = DisplayWidth( wm->dpy, xscr );
	int h = DisplayHeight( wm->dpy, xscr );
	if( w > 1 && h > 1 )
	{
	    //FIXME: we should not use this size for the window on a multi-monitor setup?
	    xsize = w;
	    ysize = h;
	    slog( "X screen size: %d x %d\n", w, h );
	}
    }
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;
    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    win_angle_apply( wm );

    if( wm->screen_ppi == 0 )
    {
	wm->screen_ppi = 110;
	int w = DisplayWidth( wm->dpy, xscr );
	int wmm = DisplayWidthMM( wm->dpy, xscr );
	if( w && wmm )
	{
	    int ppi = (float)w / ( wmm / 25.4F );
	    if( ppi > wm->screen_ppi ) wm->screen_ppi = ppi;
	}
    }
    win_zoom_init( wm );
    win_zoom_apply( wm );

    XSetWindowAttributes swa;
    smem_clear_struct( swa );
    swa.event_mask = 
	( 0
	| ExposureMask 
	| ButtonPressMask 
	| ButtonReleaseMask 
	| PointerMotionMask 
	| KeyPressMask 
	| KeyReleaseMask 
	| FocusChangeMask 
	| StructureNotifyMask
	);

    wm->xatom_wmstate = XInternAtom( wm->dpy, "_NET_WM_STATE", 0 ),
    wm->xatom_wmstate_fullscreen = XInternAtom( wm->dpy, "_NET_WM_STATE_FULLSCREEN", 0 );
    wm->xatom_wmstate_maximized_v = XInternAtom( wm->dpy, "_NET_WM_STATE_MAXIMIZED_VERT", 0 );
    wm->xatom_wmstate_maximized_h = XInternAtom( wm->dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", 0 );
    wm->xatom_delete_window = XInternAtom( wm->dpy, "WM_DELETE_WINDOW", 0 );

#ifndef OPENGL
    //Simple X11 init (not GLX) :
    wm->win_visual = XDefaultVisual( wm->dpy, xscr ); if( !wm->win_visual ) slog( "XDefaultVisual error\n" );
    wm->win_depth = XDefaultDepth( wm->dpy, xscr ); if( !wm->win_depth ) slog( "XDefaultDepth error\n" );
    slog( "depth = %d\n", wm->win_depth );
    wm->cmap = XDefaultColormap( wm->dpy, xscr );
    swa.colormap = wm->cmap;
    wm->win = XCreateWindow( wm->dpy, XDefaultRootWindow( wm->dpy ), 0, 0, xsize, ysize, 0, CopyFromParent, InputOutput, wm->win_visual, CWBorderPixel | CWColormap | CWEventMask, &swa );
    XSetStandardProperties( wm->dpy, wm->win, name, name, None, wm->sd->argv, wm->sd->argc, NULL );
    XMapWindow( wm->dpy, wm->win ); //Request the X window to be displayed on the screen
    wm->win_gc = XDefaultGC( wm->dpy, xscr ); if( !wm->win_gc ) slog( "XDefaultGC error\n" );
#endif //!OPENGL

#ifdef OPENGL
#ifdef OPENGLES
    //OpenGLES:

#ifdef OS_RASPBERRY_PI
    bcm_host_init();
#endif

    wm->win = XCreateWindow( wm->dpy, XDefaultRootWindow( wm->dpy ), 0, 0, xsize, ysize, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa );

    XSetWindowAttributes xattr;
    smem_clear_struct( xattr );
    int one = 1;

    xattr.override_redirect = 0;
    XChangeWindowAttributes( wm->dpy, wm->win, CWOverrideRedirect, &xattr );

#ifdef OS_MAEMO
    XChangeProperty(
	wm->dpy, wm->win,
	wm->xatom_wmstate,
	XA_ATOM, 32, PropModeReplace,
	(uint8_t*)&wm->xatom_fullscreen, 1 );

    XChangeProperty(
	wm->dpy, wm->win,
	XInternAtom( wm->dpy, "_HILDON_NON_COMPOSITED_WINDOW", 1 ),
	XA_INTEGER, 32, PropModeReplace,
	(uint8_t*)&one, 1 );
#endif

    XMapWindow( wm->dpy, wm->win ); //Make the window visible on the screen
    XStoreName( wm->dpy, wm->win, name ); //Give the window a name

#ifdef OS_MAEMO
    //Get identifiers for the provided atom name strings:
    XEvent xev;
    memset( &xev, 0, sizeof( xev ) );
    xev.type                 = ClientMessage;
    xev.xclient.window       = wm->win;
    xev.xclient.message_type = wm->xatom_wmstate;
    xev.xclient.format       = 32;
    xev.xclient.data.l[ 0 ]  = 1;
    xev.xclient.data.l[ 1 ]  = wm->xatom_wmstate_fullscreen;
    XSendEvent(
	wm->dpy,
	DefaultRootWindow( wm->dpy ),
	0,
	SubstructureNotifyMask,
	&xev );
#endif

    //Init OpenGLES:

#ifdef OS_RASPBERRY_PI
    wm->egl_display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
#else
    wm->egl_display = eglGetDisplay( (EGLNativeDisplayType)wm->dpy );
#endif
    if( wm->egl_display == EGL_NO_DISPLAY ) 
    {
	slog( "Got no EGL display.\n" );
	return 1;
    }

    if( !eglInitialize( wm->egl_display, NULL, NULL ) ) 
    {
	slog( "Unable to initialize EGL\n" );
	return 1;
    }

    //Some attributes to set up our egl-interface:
    EGLint attr[] = 
    {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_DEPTH_SIZE, 16,
	EGL_NONE
    };
    if( wm->screen_buffer_preserved ) attr[ 3 ] |= EGL_SWAP_BEHAVIOR_PRESERVED_BIT;

    EGLConfig ecfg;
    EGLint num_config;
    if( !eglChooseConfig( wm->egl_display, attr, &ecfg, 1, &num_config ) ) 
    {
	slog( "Failed to choose config (eglError: %d)\n", eglGetError() );
	return 1;
    }

    if( num_config != 1 ) 
    {
	slog( "Didn't get exactly one config, but %d\n", num_config );
	return 1;
    }

#ifdef OS_RASPBERRY_PI
    static EGL_DISPMANX_WINDOW_T nativewindow;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;
    uint32_t screen_width;
    uint32_t screen_height;
    EGLint gles2_attrib[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    wm->egl_context = eglCreateContext( wm->egl_display, ecfg, EGL_NO_CONTEXT, gles2_attrib );
    if( wm->egl_context == EGL_NO_CONTEXT ) 
    {
	slog( "Unable to create EGL context (eglError: %d)\n", eglGetError() );
	return 1;
    }
    if( graphics_get_display_size( 0 /* LCD */, &screen_width, &screen_height ) )
    {
	slog( "graphics_get_display_size() error\n" );
	return 1;
    }
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screen_width;
    dst_rect.height = screen_height;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screen_width << 16;
    src_rect.height = screen_height << 16;
    dispman_display = vc_dispmanx_display_open( 0 /* LCD */ );
    dispman_update = vc_dispmanx_update_start( 0 );
    dispman_element = vc_dispmanx_element_add( dispman_update, dispman_display,
	0 /*layer*/, &dst_rect, 0 /*src*/,
	&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0 /*clamp*/, (DISPMANX_TRANSFORM_T)0 /*transform*/ );
    nativewindow.element = dispman_element;
    nativewindow.width = screen_width;
    nativewindow.height = screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );
    wm->egl_surface = eglCreateWindowSurface( wm->egl_display, ecfg, &nativewindow, NULL );
    if( wm->egl_surface == EGL_NO_SURFACE ) 
    {
	slog( "Unable to create EGL surface (eglError: %d)\n", eglGetError() );
	return 1;
    }
#else
    wm->egl_surface = eglCreateWindowSurface( wm->egl_display, ecfg, wm->win, NULL );
    if( wm->egl_surface == EGL_NO_SURFACE ) 
    {
	slog( "Unable to create EGL surface (eglError: %d)\n", eglGetError() );
	return 1;
    }
    //egl-contexts collect all state descriptions needed required for operation:
    eglBindAPI( EGL_OPENGL_ES_API );
    EGLint ctxattr[] =
    {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
    };
    wm->egl_context = eglCreateContext( wm->egl_display, ecfg, EGL_NO_CONTEXT, ctxattr );
    if( wm->egl_context == EGL_NO_CONTEXT ) 
    {
	slog( "Unable to create EGL context (eglError: %d)\n", eglGetError() );
	return 1;
    }
#endif

    //Associate the egl-context with the egl-surface:
    eglMakeCurrent( wm->egl_display, wm->egl_surface, wm->egl_surface, wm->egl_context );

    if( wm->screen_buffer_preserved )
    {
        if( eglSurfaceAttrib( wm->egl_display, wm->egl_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED ) != EGL_TRUE )
            slog( "eglSurfaceAttrib error %d", eglGetError() );
        EGLint sb = EGL_BUFFER_DESTROYED;
        if( eglQuerySurface( wm->egl_display, wm->egl_surface, EGL_SWAP_BEHAVIOR, &sb ) != EGL_TRUE )
            slog( "eglQuerySurface error %d", eglGetError() );
        if( sb == EGL_BUFFER_PRESERVED )
        {
            wm->screen_buffer_preserved = 1;
            slog( "EGL_BUFFER_PRESERVED" );
        }
        else
        {
            wm->screen_buffer_preserved = 0;
            slog( "EGL_BUFFER_DESTROYED" );
        }
    }

#else
    //OpenGL:

    //Make sure OpenGL's GLX extension supported:
    int dummy;
    if( !glXQueryExtension( wm->dpy, &dummy, &dummy ) ) 
    {
    	slog( "X server has no OpenGL GLX extension\n" );
	return 1;
    }

    //Find an appropriate visual:
    //Find an OpenGL-capable RGB visual with depth buffer:
    wm->win_visual = glXChooseVisual( wm->dpy, xscr, g_gl_dblBuf );
    if( wm->win_visual == 0 ) 
    {
    	wm->win_visual = glXChooseVisual( wm->dpy, xscr, g_gl_snglBuf );
	if( wm->win_visual == NULL ) 
	{
	    slog( "No RGB visual with depth buffer.\n" );
	}
    }

    //Create an OpenGL rendering context:
    wm->gl_context = glXCreateContext( wm->dpy, wm->win_visual, /* no sharing of display lists */ None, /* direct rendering if possible */ GL_TRUE );
    if( wm->gl_context == NULL ) 
    {
	slog( "Could not create rendering context.\n" );
	return 1;
    }

    //Create an X window with the selected visual:
    //Create an X colormap since probably not using default visual:
    wm->cmap = XCreateColormap( wm->dpy, RootWindow( wm->dpy, wm->win_visual->screen ), wm->win_visual->visual, AllocNone );
    swa.colormap = wm->cmap;
    wm->win = XCreateWindow( wm->dpy, RootWindow( wm->dpy, wm->win_visual->screen ), 0, 0, xsize, ysize, 0, wm->win_visual->depth, InputOutput, wm->win_visual->visual, CWBorderPixel | CWColormap | CWEventMask, &swa );

    XSetStandardProperties( wm->dpy, wm->win, name, name, None, wm->sd->argv, wm->sd->argc, NULL );
    XMapWindow( wm->dpy, wm->win ); //Request the X window to be displayed on the screen

    //Bind the rendering context to the window:
    if( glXMakeCurrent( wm->dpy, wm->win, wm->gl_context ) == 0 )
	slog( "glXMakeCurrent() ERROR %d\n", glGetError() );

#endif
#endif //OPENGL

    XClassHint* hint = XAllocClassHint();
    char* app_name = smem_strdup( g_app_name_short );
    make_string_lowercase( app_name, strlen( app_name ) + 1, app_name );
    hint->res_name = app_name; //application name
    hint->res_class = app_name; //application class
    XSetClassHint( wm->dpy, wm->win, hint );
    XFree( hint );
    smem_free( app_name );
    //Type "xprop WM_CLASS" in a terminal, the cursor will change, then click on the window of the app and xprop will return the wm class of the app...

    if( wm->flags & WIN_INIT_FLAG_FULLSCREEN )
    {
	//BUG: this code works unpredictably on Gnome+Wayland:
	XEvent e = { 0 };
	e.xclient.type = ClientMessage;
	e.xclient.message_type = wm->xatom_wmstate;
	e.xclient.display = wm->dpy;
	e.xclient.window = wm->win;
	e.xclient.format = 32;
	e.xclient.data.l[ 0 ] = 1;
	e.xclient.data.l[ 1 ] = wm->xatom_wmstate_fullscreen;
	XSendEvent( wm->dpy, DefaultRootWindow( wm->dpy ), false, SubstructureNotifyMask | SubstructureRedirectMask, &e );
    }
    else if( wm->flags & WIN_INIT_FLAG_MAXIMIZED )
    {
	XEvent e = { 0 };
	e.xclient.type = ClientMessage;
	e.xclient.message_type = wm->xatom_wmstate;
	e.xclient.display = wm->dpy;
	e.xclient.window = wm->win;
	e.xclient.format = 32;
	e.xclient.data.l[ 0 ] = 1;
	e.xclient.data.l[ 1 ] = wm->xatom_wmstate_maximized_v;
	e.xclient.data.l[ 2 ] = wm->xatom_wmstate_maximized_h;
	XSendEvent( wm->dpy, DefaultRootWindow( wm->dpy ), false, SubstructureNotifyMask | SubstructureRedirectMask, &e );
    }

    //Register interest in the delete window message:
    XSetWMProtocols( wm->dpy, wm->win, &wm->xatom_delete_window, 1 );

    if( sconfig_get_int_value( APP_CFG_NOCURSOR, -1, 0 ) != -1 )
    {
	//Hide mouse pointer:
	Cursor cursor;
	Pixmap blank;
	XColor dummy;
	static char data[] = { 0 };
	blank = XCreateBitmapFromData( wm->dpy, wm->win, data, 1, 1 );
	cursor = XCreatePixmapCursor( wm->dpy, blank, blank, &dummy, &dummy, 0, 0 );
	XDefineCursor( wm->dpy, wm->win, cursor );
	XFreePixmap( wm->dpy, blank );
    }

    XkbSetDetectableAutoRepeat( wm->dpy, 1, &wm->auto_repeat );

    //XGrabKeyboard( wm->dpy, wm->win, 1, GrabModeAsync, GrabModeAsync, CurrentTime );
    //XGrabKey( wm->dpy, 0, ControlMask, wm->win, 1, 1, GrabModeSync );

#ifdef MULTITOUCH
    if( XQueryExtension( wm->dpy, INAME, &wm->xi_opcode, &wm->xi_event, &wm->xi_error ) ) 
    {
	wm->xi = true;
	XIGrabModifiers modifiers;
	XIEventMask mask;
	mask.deviceid = XIAllMasterDevices;
	mask.mask_len = XIMaskLen( XI_LASTEVENT );
	mask.mask = (uint8_t*)calloc( mask.mask_len, 1 );
	XISetMask( mask.mask, XI_TouchBegin );
        XISetMask( mask.mask, XI_TouchUpdate );
	XISetMask( mask.mask, XI_TouchEnd );
	XISelectEvents( wm->dpy, wm->win, &mask, 1 );
    }
#endif

#ifdef OPENGL
    if( gl_init( wm ) ) return 1;
    gl_resize( wm );
#endif

#ifdef FRAMEBUFFER
    //Create framebuffer:
    wm->fb = (COLORPTR)smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
    wm->fb_xpitch = 1;
    wm->fb_ypitch = wm->screen_xsize;
#endif

    return retval;
}

void device_end( window_manager* wm )
{
    int temp;

    Atom prop_type;
    int prop_format;
    unsigned long prop_nitems = 0; //actual number of 8-bit, 16-bit, or 32-bit items stored in the prop_return data.
    unsigned long after_return; //number of bytes remaining to be read in the property if a partial read was performed.
    unsigned char* prop_data = nullptr;
    XGetWindowProperty( wm->dpy, wm->win, wm->xatom_wmstate, 0, 32768, false, XA_ATOM, &prop_type, &prop_format, &prop_nitems, &after_return, &prop_data );
    Atom* atoms = (Atom*)prop_data;
    wm->flags &= ~WIN_INIT_FLAG_MAXIMIZED;
    for( unsigned long i = 0; i < prop_nitems; i++ )
    {
	if( atoms[ i ] == wm->xatom_wmstate_maximized_v )
	    wm->flags |= WIN_INIT_FLAG_MAXIMIZED;
    }
    XFree( prop_data );

    XkbSetDetectableAutoRepeat( wm->dpy, wm->auto_repeat, &temp );
#ifdef OPENGL
    gl_deinit( wm );
#ifdef OPENGLES
    eglMakeCurrent( wm->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroyContext( wm->egl_display, wm->egl_context );
    eglDestroySurface( wm->egl_display, wm->egl_surface );
    eglTerminate( wm->egl_display );
#ifdef OS_RASPBERRY_PI
    bcm_host_deinit();
#endif
#endif
#else
    if( wm->last_ximg )
    {
	XDestroyImage( wm->last_ximg );
	wm->last_ximg = 0;
    }
    if( wm->temp_bitmap )
    {
	wm->temp_bitmap = 0;
    }
#endif
    XDestroyWindow( wm->dpy, wm->win );
    XCloseDisplay( wm->dpy );

#ifdef FRAMEBUFFER
    smem_free( wm->fb );
#endif
}

void device_event_handler( window_manager* wm )
{
    XEvent evt;
    int handled = 0;
    int pend = XPending( wm->dpy );
    for( int e = 0; e < pend; e++ )
    {
	handled++;
	XNextEvent( wm->dpy, &evt );
        switch( evt.type ) 
	{
	    case FocusIn:
		//XGrabKeyboard( wm->dpy, wm->win, 1, GrabModeAsync, GrabModeAsync, CurrentTime ); 
		break;

	    case FocusOut:
		//XUngrabKeyboard( wm->dpy, CurrentTime );
		if( wm->mods & EVT_FLAG_SHIFT ) send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_SHIFT, 0, 1024, wm );
		if( wm->mods & EVT_FLAG_CTRL ) send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_CTRL, 0, 1024, wm );
		if( wm->mods & EVT_FLAG_ALT ) send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_ALT, 0, 1024, wm );
		if( wm->mods & EVT_FLAG_CMD ) send_event( 0, EVT_BUTTONUP, 0, 0, 0, KEY_CMD, 0, 1024, wm );
		wm->mods = 0;
		break;

	    case MapNotify:
	    case Expose:
		send_event( wm->root_win, EVT_DRAW, wm );
		break;

	    case DestroyNotify:
		send_event( 0, EVT_QUIT, wm );
		break;

	    case ClientMessage:
		if( evt.xclient.data.l[ 0 ] == (long)wm->xatom_delete_window )
		{
		    send_event( 0, EVT_QUIT, wm );
		}
		break;

	    case ConfigureNotify:
#ifndef OPENGL
		if( !( wm->flags & WIN_INIT_FLAG_SCALABLE ) )
		{
		    XSizeHints size_hints.flags = PMinSize;
		    int xsize = wm->screen_xsize * wm->screen_zoom;
		    int ysize = wm->screen_ysize * wm->screen_zoom;
    	    	    if( wm->screen_angle & 1 )
		    {
			int temp = xsize;
			xsize = ysize;
			ysize = temp;
		    }
		    size_hints.min_width = xsize;
		    size_hints.min_height = ysize;
		    size_hints.max_width = xsize;
		    size_hints.max_height = ysize;
		    XSetNormalHints( wm->dpy, wm->win, &size_hints );
		}
#endif
		if( wm->flags & WIN_INIT_FLAG_SCALABLE )
		{
		    wm->real_window_width = evt.xconfigure.width;
	    	    wm->real_window_height = evt.xconfigure.height;
		    wm->screen_xsize = wm->real_window_width;
    	    	    wm->screen_ysize = wm->real_window_height;
    	    	    win_zoom_apply( wm );
    	    	    win_angle_apply( wm );
		    send_event( wm->root_win, EVT_SCREENRESIZE, wm );
		}
#ifdef OPENGL
		gl_resize( wm );
#endif
		break;

	    case MotionNotify:
	    {
		if( evt.xmotion.window != wm->win ) break;
		int x = evt.xmotion.x;
		int y = evt.xmotion.y;
		convert_real_window_xy( x, y, wm );
		int key = 0;
		if( evt.xmotion.state & Button1Mask )
                    key |= MOUSE_BUTTON_LEFT;
                if( evt.xmotion.state & Button2Mask )
                    key |= MOUSE_BUTTON_MIDDLE;
                if( evt.xmotion.state & Button3Mask )
                    key |= MOUSE_BUTTON_RIGHT;
		send_event( 0, EVT_MOUSEMOVE, wm->mods, x, y, key, 0, 1024, wm );
		break;
	    }

	    case ButtonPress:
	    case ButtonRelease:
	    {
		if( evt.xmotion.window != wm->win ) break;
		int x = evt.xbutton.x;
		int y = evt.xbutton.y;
		convert_real_window_xy( x, y, wm );
		/* Get pressed button */
		int key = 0;
	    	if( evt.xbutton.button == 1 )
		    key = MOUSE_BUTTON_LEFT;
		else if( evt.xbutton.button == 2 )
		    key = MOUSE_BUTTON_MIDDLE;
		else if( evt.xbutton.button == 3 )
		    key = MOUSE_BUTTON_RIGHT;
		else if( ( evt.xbutton.button == 4 || evt.xbutton.button == 5 ) && evt.type == ButtonPress )
		{
		    if( evt.xbutton.button == 4 ) 
			key = MOUSE_BUTTON_SCROLLUP;
		    else
			key = MOUSE_BUTTON_SCROLLDOWN;
		}
		if( key )
		{
		    if( evt.type == ButtonPress )
			send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, key, 0, 1024, wm );
		    else
			send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, key, 0, 1024, wm );
		}
		break;
	    }

#ifdef MULTITOUCH
	    case GenericEvent:
	    {
	        XGenericEventCookie* cookie = &evt.xcookie;
	        if( XGetEventData( wm->dpy, cookie ) )
	        {
	    	    if( cookie->type == GenericEvent && cookie->extension == wm->xi_opcode )
		    {
		        XIDeviceEvent* xevt = (XIDeviceEvent*)cookie->data;
		        int x = xevt->event_x;
			int y = xevt->event_y;
			switch( xevt->evtype )
			{
    			    case XI_TouchBegin:
    			        collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, 1024, xevt->detail, wm );
    			        break;
			    case XI_TouchUpdate: 
    			        collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, 1024, xevt->detail, wm );
				break;
			    case XI_TouchEnd:
    			        collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, 1024, xevt->detail, wm );
			        break;
			    default: break;
			}
			send_touch_events( wm );
		    }
		    XFreeEventData( wm->dpy, cookie );
		}
		break;
	    }
#endif

	    case KeyPress:
	    case KeyRelease:
	    {
		//evt.xkey.keycode - physical (or logical) key code (8...255)
		//KeySym - encoding of a symbol on the cap of a key; NOT UNICODE!
		KeySym sym = XkbKeycodeToKeysym( wm->dpy, evt.xkey.keycode, 0, 0 ); //event.xkey.state & ShiftMask ? 1 : 0 );
		//KeySym sym = XkbKeycodeToKeysym( wm->dpy, evt.xkey.keycode, GROUP, SHIFT ); //GROUP: 0 - default; 1 - alternative (RU for example);
		//KeySym can be converted to UTF32 using xkb_keysym_to_utf32() (xkbcommon/xkbcommon.h) or Xutf8LookupString()
		//current group can be changed by XK_Mode_switch or with the following code:
		//	int group = XkbGroupForCoreState( evt.xkey.state ); //example: 0 - EN; 1 - RU;
		//	sym = XkbKeycodeToKeysym( wm->dpy, evt.xkey.keycode, group, 0 );
		if( sym >= XK_KP_Space && sym <= XK_KP_9 )
		{
		    //Keypad:
		    if( evt.xkey.state & Mod2Mask )
		    {
			//Numlock:
			KeySym sym2 = XkbKeycodeToKeysym( wm->dpy, evt.xkey.keycode, 0, 1 );
			if( sym2 != NoSymbol ) sym = sym2;
		    }
		}
		int key = 0;
		if( sym == NoSymbol || sym == 0 ) break;
		switch( sym )
		{
		    case XK_Num_Lock: break;

		    case XK_KP_Left: key = KEY_LEFT; break;
		    case XK_KP_Right: key = KEY_RIGHT; break;
		    case XK_KP_Up: key = KEY_UP; break;
		    case XK_KP_Down: key = KEY_DOWN; break;
		    case XK_KP_Home: key = KEY_HOME; break;
		    case XK_KP_End: key = KEY_END; break;
		    case XK_KP_Page_Up: key = KEY_PAGEUP; break;
		    case XK_KP_Page_Down: key = KEY_PAGEDOWN; break;
		    case XK_KP_Delete: key = KEY_DELETE; break;
		    case XK_KP_Insert: key = KEY_INSERT; break;
		    case XK_KP_Enter: key = KEY_ENTER; break;
		    case XK_KP_0: key = '0'; break;
		    case XK_KP_1: key = '1'; break;
		    case XK_KP_2: key = '2'; break;
		    case XK_KP_3: key = '3'; break;
		    case XK_KP_4: key = '4'; break;
		    case XK_KP_5: key = '5'; break;
		    case XK_KP_6: key = '6'; break;
		    case XK_KP_7: key = '7'; break;
		    case XK_KP_8: key = '8'; break;
		    case XK_KP_9: key = '9'; break;

		    case XK_F1: key = KEY_F1; break;
		    case XK_F2: key = KEY_F2; break;
		    case XK_F3: key = KEY_F3; break;
		    case XK_F4: key = KEY_F4; break;
		    case XK_F5: key = KEY_F5; break;
		    case XK_F6: key = KEY_F6; break;
		    case XK_F7: key = KEY_F7; break;
		    case XK_F8: key = KEY_F8; break;
		    case XK_F9: key = KEY_F9; break;
		    case XK_F10: key = KEY_F10; break;
		    case XK_F11: key = KEY_F11; break;
		    case XK_F12: key = KEY_F12; break;
		    case XK_BackSpace: key = KEY_BACKSPACE; break;
		    case XK_Tab: key = KEY_TAB; break;
		    case XK_Return: key = KEY_ENTER; break;
		    case XK_Escape: key = KEY_ESCAPE; break;
		    case XK_Left: key = KEY_LEFT; break;
		    case XK_Right: key = KEY_RIGHT; break;
		    case XK_Up: key = KEY_UP; break;
		    case XK_Down: key = KEY_DOWN; break;
		    case XK_Home: key = KEY_HOME; break;
		    case XK_End: key = KEY_END; break;
		    case XK_Page_Up: key = KEY_PAGEUP; break;
		    case XK_Page_Down: key = KEY_PAGEDOWN; break;
		    case XK_Delete: key = KEY_DELETE; break;
		    case XK_Insert: key = KEY_INSERT; break;
		    case XK_Caps_Lock: key = KEY_CAPS; break;
		    case XK_Shift_L: 
		    case XK_Shift_R:
			key = KEY_SHIFT;
			if( evt.type == KeyPress )
                            wm->mods |= EVT_FLAG_SHIFT;
                        else
                            wm->mods &= ~EVT_FLAG_SHIFT;
			break;
		    case XK_Control_L: 
		    case XK_Control_R:
			key = KEY_CTRL;
			if( evt.type == KeyPress )
                            wm->mods |= EVT_FLAG_CTRL;
                        else
                            wm->mods &= ~EVT_FLAG_CTRL;
			break;
		    case XK_Alt_L: 
		    case XK_Alt_R:
			key = KEY_ALT;
			if( evt.type == KeyPress )
                            wm->mods |= EVT_FLAG_ALT;
                        else
                            wm->mods &= ~EVT_FLAG_ALT;
			break;
		    case XK_Mode_switch:
			if( evt.type == KeyPress )
                            wm->mods |= EVT_FLAG_MODE;
                        else
                            wm->mods &= ~EVT_FLAG_MODE;
			break;
		    default:
			if( sym >= 0x20 && sym <= 0x7E ) 
			    key = sym;
			else
			    key = KEY_UNKNOWN + sym;
			break;
		}
		if( key )
		{
		    if( evt.type == KeyPress )
		    {
			send_event( 0, EVT_BUTTONDOWN, wm->mods, 0, 0, key, evt.xkey.keycode, 1024, wm );
		    }
		    else
		    {
			send_event( 0, EVT_BUTTONUP, wm->mods, 0, 0, key, evt.xkey.keycode, 1024, wm );
		    }
		}
		break;
	    }

	    default: break;
	}
    }

    /*if( handled == 0 )
    { //No more X11 events
        if( wm->events_count == 0 ) //And no WM events
	    stime_sleep( wm->frame_len );
    }*/

    stime_ticks_t cur_t = stime_ticks();
    if( cur_t - wm->prev_frame_time >= wm->ticks_per_frame )
    {
	wm->frame_event_request = 1;
	wm->prev_frame_time = cur_t;
    }
    else
    {
	if( handled == 0 )
	{ //No more X11 events
	    if( wm->events_count == 0 ) //And no WM events
	    {
        	int pause_ms = ( wm->ticks_per_frame - ( cur_t - wm->prev_frame_time ) ) * 1000 / stime_ticks_per_second();
        	stime_sleep( pause_ms );
    	    }
        }
    }
}

void device_screen_lock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter == 0 )
    {
        gl_lock( wm );
    }
    wm->screen_lock_counter++;

    if( wm->screen_lock_counter > 0 )
        wm->screen_is_active = true;
    else
        wm->screen_is_active = false;
}

void device_screen_unlock( WINDOWPTR win, window_manager* wm )
{
    if( wm->screen_lock_counter == 1 )
    {
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

void device_change_name( const char* name, window_manager* wm )
{
    if( !name ) return;
    const char* atom_names[] = { "_NET_WM_NAME", "UTF8_STRING" };
    Atom atoms[ 2 ];
    XInternAtoms( wm->dpy, (char**)atom_names, 2, false, atoms );
    //XStoreName( wm->dpy, wm->win, name ); //can't work with UTF-8
    XChangeProperty( wm->dpy, wm->win, atoms[ 0 ], atoms[ 1 ], 8, PropModeReplace, (const unsigned char*)name, strlen( name ) );
}

#ifndef OPENGL

uint g_local_colors[ 32 * 32 * 32 ] = { 0x19283746 };

static void device_find_color( XColor* col, COLOR color, window_manager* wm )
{
    if( g_local_colors[ 0 ] == 0x19283746 )
    {
	for( uint lc = 0; lc < 32 * 32 * 32; lc++ ) g_local_colors[ lc ] = 0xFFFFFFFF;
    }
    int r = red( color );
    int g = green( color );
    int b = blue( color );
    int p = ( r >> 3 ) | ( ( g >> 3 ) << 5 ) | ( ( b >> 3 ) << 10 );
    if( g_local_colors[ p ] == 0xFFFFFFFF )
    {
	col->red = r << 8;
	col->green = g << 8;
	col->blue = b << 8;
	XAllocColor( wm->dpy, wm->cmap, col );
	g_local_colors[ p ] = col->pixel;
    }
    else 
    {
	col->pixel = g_local_colors[ p ];
    }
}

void device_vsync( bool vsync, window_manager* wm )
{
}

void device_redraw_framebuffer( window_manager* wm ) 
{
}

void device_draw_image( 
    int dest_x, int dest_y, 
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm )
{
    int src_xs = img->xsize;
    int src_ys = img->ysize;
    COLORPTR data = (COLORPTR)img->data;

    if( wm->temp_bitmap == 0 )
    {
	wm->temp_bitmap_size = 64 * 1024;
	wm->temp_bitmap = (uint8_t*)malloc( wm->temp_bitmap_size );
    }
    
    XImage* ximg;
    if( wm->last_ximg )
    {
	ximg = wm->last_ximg;
	ximg->width = src_xs;
	ximg->height = src_ys;
	ximg->bytes_per_line = ( ximg->bits_per_pixel / 8 ) * src_xs;
    }
    else
    {
	ximg = XCreateImage( wm->dpy, wm->win_visual, wm->win_depth, ZPixmap, 0, (char*)wm->temp_bitmap, src_xs, src_ys, 8, 0 );
	wm->last_ximg = ximg;
    }
    
    size_t new_temp_bitmap_size = ( ximg->bits_per_pixel / 8 ) * src_xs * src_ys;
    if( new_temp_bitmap_size > wm->temp_bitmap_size )
    {
	new_temp_bitmap_size += 32 * 1024;
	wm->temp_bitmap_size = new_temp_bitmap_size;
	wm->temp_bitmap = (uint8_t*)realloc( wm->temp_bitmap, wm->temp_bitmap_size );
	ximg->data = (char*)wm->temp_bitmap;
    }
    
    if( ximg->bits_per_pixel == COLORLEN )
    {
	smem_copy( wm->temp_bitmap, data, src_xs * src_ys * COLORLEN );
    }
    else
    {
	int pixel_len = ximg->bits_per_pixel / 8;
	uint8_t* bitmap = wm->temp_bitmap + ( src_y * src_xs + src_x ) * pixel_len;
	int a = src_y * src_xs + src_x;
	for( int y = 0; y < dest_ys; y++ )
	{
	    for( int x = 0; x < dest_xs; x++ )
	    {
		COLOR c = data[ a ];
		int r = red( c );
		int g = green( c );
		int b = blue( c );
		int res;
		switch( pixel_len )
		{
		    case 1:
			*(bitmap++) = (uint8_t)( (r>>5) + ((g>>5)<<3) + (b>>6)<<6 ); 
			break;
		    case 2:
			res = (b>>3) + ((g>>2)<<5) + ((r>>3)<<11);
			*(bitmap++) = (uint8_t)(res & 255); *(bitmap++) = (uint8_t)(res >> 8); 
			break;
		    case 3:
			*(bitmap++) = (uint8_t)b; *(bitmap++) = (uint8_t)g; *(bitmap++) = (uint8_t)r; 
			break;
		    case 4:
			*(bitmap++) = (uint8_t)b; *(bitmap++) = (uint8_t)g; *(bitmap++) = (uint8_t)r; *(bitmap++) = 0;
			break;
		}
		a++;
	    }
	    int ystep = src_xs - dest_xs;
	    bitmap += ystep * pixel_len;
	    a += ystep;
	}
    }
    switch( XPutImage( wm->dpy, wm->win, wm->win_gc, ximg, src_x, src_y, dest_x, dest_y, dest_xs, dest_ys ) )
    {
	case BadDrawable: slog( "BadDrawable\n" ); break;
	case BadGC: slog( "BadGC\n" ); break;
	case BadMatch: slog( "BadMatch\n" ); break;
	case BadValue: slog( "BadValue\n" ); break;
    }
}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm )
{
    XColor col;
    device_find_color( &col, color, wm );
    XSetForeground( wm->dpy, wm->win_gc, col.pixel );
    XDrawLine( wm->dpy, wm->win, wm->win_gc, x1, y1, x2, y2 );
}

void device_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
    XColor col;
    device_find_color( &col, color, wm );
    XSetForeground( wm->dpy, wm->win_gc, col.pixel );
    XFillRectangle( wm->dpy, wm->win, wm->win_gc, x, y, xsize, ysize );
}

#endif //!OPENGL
