/*
    wm_unix_sdl.h - platform-dependent module : SDL
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#ifdef SDL12
    #include "SDL/SDL.h"
    #include "SDL/SDL_syswm.h"
#else
    #include "SDL2/SDL.h"
    #include "SDL2/SDL_syswm.h"
#endif

#ifdef OS_MAEMO
    extern "C" int osso_display_blanking_pause( void* osso );
    extern "C" void* osso_initialize( const char* application, const char* version, bool activation, void* context );
    extern "C" void osso_deinitialize( void* osso );
#endif

struct sdl_data
{
#ifdef SDL12
    SDL_Surface*        sdl_screen;
#else
    SDL_Window*         sdl_window;
    SDL_Renderer*       sdl_renderer;
    SDL_Texture*        sdl_screen;
    uint32_t 		sdl_pixel_format;
    bool		sdl_multitouch_xy_normalized;
#endif
#ifdef OS_MAEMO
    sthread             sdl_screen_control_thread;
    volatile bool       sdl_screen_control_thread_exit_request;
#endif
    void*               sdl_zoom_buffer;
    stime_ticks_t		sdl_prev_frame_time;
    uint                sdl_wm_flags;
    const char*    	sdl_wm_name;
    int                 sdl_wm_xsize; //requested size
    int                 sdl_wm_ysize;
};

#ifdef OS_MAEMO
static void* screen_control_thread( void* arg )
{
    window_manager* wm = (window_manager*)arg;
    int c = 0;
    void* ossocontext = osso_initialize( "sunvox", "1", 0, NULL );
    sdl_data* dd = (sdl_data*)wm->dd;
    while( !dd->sdl_screen_control_thread_exit_request )
    {
	if( c == 0 )
	    osso_display_blanking_pause( ossocontext );
	stime_sleep( 1000 );
	c++;
	if( c >= 30 ) c = 0;
    }
    osso_deinitialize( ossocontext );
    return NULL;
}
#endif

static int sdl_init( window_manager* wm )
{
    int rv = -1;
    uint32_t win_flags = 0;
    uint32_t render_flags = 0;
    sdl_data* dd = (sdl_data*)wm->dd;

    while( 1 )
    {
	if( g_sdl_initialized == false )
	{
	    g_sdl_initialized = true;
	    SDL_Init( 0 );
	}
#ifdef SDL12
	//SDL 1.2:
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_SCALABLE ) win_flags |= SDL_RESIZABLE;
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_NOBORDER ) win_flags |= SDL_NOFRAME;
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_FULLSCREEN ) win_flags = SDL_FULLSCREEN;
        if( SDL_InitSubSystem( SDL_INIT_VIDEO ) < 0 ) 
	{
	    slog( "Can't initialize SDL video subsystem: %s\n", SDL_GetError() );
	    rv = -1;
    	    break;
        }
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_FULLSCREEN )
        {
    	    //Get list of available video-modes:
	    SDL_Rect** modes;
	    modes = SDL_ListModes( NULL, SDL_FULLSCREEN | SDL_HWSURFACE );
	    if( modes != (SDL_Rect **)-1 )
	    {
		slog( "Available Video Modes:\n" );
		for( int i = 0; modes[ i ]; ++i )
		    printf( "  %d x %d\n", modes[ i ]->w, modes[ i ]->h );
	    }
	}
#ifdef OS_MAEMO
	dd->sdl_screen = SDL_SetVideoMode( 0, 0, COLORBITS, SDL_HWSURFACE | win_flags );
#else
	dd->sdl_screen = SDL_SetVideoMode( dd->sdl_wm_xsize, dd->sdl_wm_ysize, COLORBITS, SDL_HWSURFACE | win_flags );
#endif
	if( !dd->sdl_screen )
	{
	    slog( "SDL. Can't set videomode: %s\n", SDL_GetError() );
	    rv = -2;
	    break;
	}
	SDL_WM_SetCaption( dd->sdl_wm_name, dd->sdl_wm_name );
	SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
#else
	//SDL 2:
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_SCALABLE ) win_flags |= SDL_WINDOW_RESIZABLE;
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_NOBORDER ) win_flags |= SDL_WINDOW_BORDERLESS;
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_FULLSCREEN ) 
	{
	    if( sconfig_get_int_value( "change_video_mode", -1, 0 ) != -1 )
	    {
		win_flags = SDL_WINDOW_FULLSCREEN;
    		slog( "SDL: video mode will be changed...\n" );
    	    }
	    else
		win_flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	if( dd->sdl_wm_flags & WIN_INIT_FLAG_MAXIMIZED ) win_flags |= SDL_WINDOW_MAXIMIZED;
	if( SDL_InitSubSystem( SDL_INIT_VIDEO | SDL_INIT_EVENTS ) < 0 ) 
	{
	    slog( "Can't initialize SDL video subsystem: %s\n", SDL_GetError() );
	    rv = -1;
	    break;
        }
        {
	    SDL_version ver;
            SDL_GetVersion( &ver );
    	    if( ( ver.major << 16 ) + ( ver.minor << 8 ) + ver.patch >= 0x20007 ) 
    		dd->sdl_multitouch_xy_normalized = true;
    	    else
    		dd->sdl_multitouch_xy_normalized = false;
    	}
    	/*if( dd->sdl_wm_xsize == 0 )
    	{
    	    SDL_DisplayMode dm; dm.w = 0;
	    SDL_GetCurrentDisplayMode( 0, &dm );
	    if( dm.w )
	    {
		if( dm.w > 1920 ) dm.w = 1920;
		if( dm.h > 1080 ) dm.h = 1080;
		dd->sdl_wm_xsize = dm.w - 100;
		dd->sdl_wm_ysize = dm.h - 100;
	    }
	    else
	    {
		dm.w = 800;
		dm.h = 600;
	    }
    	}*/
	dd->sdl_window = SDL_CreateWindow( dd->sdl_wm_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dd->sdl_wm_xsize, dd->sdl_wm_ysize, win_flags );
        if( !dd->sdl_window )
	{
	    slog( "SDL: can't create a window: %s\n", SDL_GetError() );
	    rv = -2;
	    break;
        }
	SDL_GetWindowSize( dd->sdl_window, &dd->sdl_wm_xsize, &dd->sdl_wm_ysize );
        if( sconfig_get_int_value( "softrender", -1, 0 ) != -1 )
        {
    	    render_flags |= SDL_RENDERER_SOFTWARE;
    	    render_flags &= ~SDL_RENDERER_ACCELERATED;
    	    slog( "SDL: softrender enabled\n" );
        }
	dd->sdl_renderer = SDL_CreateRenderer( dd->sdl_window, -1, render_flags );
	if( !dd->sdl_renderer )
	{
	    slog( "SDL: can't create a renderer: %s\n", SDL_GetError() );
	    rv = -3;
	    break;
	}
#endif

	if( sconfig_get_int_value( APP_CFG_NOCURSOR, -1, 0 ) != -1 ) SDL_ShowCursor( 0 );

	wm->real_window_width = dd->sdl_wm_xsize;
	wm->real_window_height = dd->sdl_wm_ysize;
        wm->screen_xsize = dd->sdl_wm_xsize;
	wm->screen_ysize = dd->sdl_wm_ysize;
        win_angle_apply( wm );
        if( wm->screen_ppi == 0 )
        {
    	    wm->screen_ppi = 110;
#ifndef SDL12
            float ddpi = 0;
            float hdpi = 0;
            float vdpi = 0;
    	    if( SDL_GetDisplayDPI( 0, &ddpi, &hdpi, &vdpi ) == 0 )
    	    {
    		if( ddpi > wm->screen_ppi ) wm->screen_ppi = ddpi;
    	    }
#endif
    	}
	win_zoom_init( wm );

#ifdef SDL12
        //SDL 1.2:
	if( wm->screen_zoom > 1 )
        {
    	    win_zoom_apply( wm );
    	    dd->sdl_zoom_buffer = smem_new( wm->screen_xsize * wm->screen_ysize * COLORLEN );
	}
#else
        //SDL 2:
	//SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "nearest" );
	if( wm->screen_zoom > 1 )
	{
	    win_zoom_apply( wm );
	}
	//SDL_RenderSetLogicalSize( dd->sdl_renderer, wm->screen_xsize, wm->screen_ysize );
#ifdef COLOR32BITS
        dd->sdl_pixel_format = SDL_PIXELFORMAT_ARGB8888;
#endif
#ifdef COLOR16BITS
	dd->sdl_pixel_format = SDL_PIXELFORMAT_RGB565;
#ifdef RGB555
        dd->sdl_pixel_format = SDL_PIXELFORMAT_RGB555;
#endif
#endif
#ifdef COLOR8BITS
	dd->sdl_pixel_format = SDL_PIXELFORMAT_RGB332;
#endif
	wm->fb_xsize = wm->screen_xsize;
	wm->fb_ysize = wm->screen_ysize;
	if( wm->screen_angle & 1 ) { int t = wm->fb_xsize; wm->fb_xsize = wm->fb_ysize; wm->fb_ysize = t; }
        dd->sdl_screen = SDL_CreateTexture( dd->sdl_renderer, dd->sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, wm->fb_xsize, wm->fb_ysize );
	if( !dd->sdl_screen )
        {
	    slog( "SDL: can't create a texture: %s\n", SDL_GetError() );
	    rv = -4;
	    break;
        }
        SDL_SetTextureBlendMode( dd->sdl_screen, SDL_BLENDMODE_NONE );
	wm->fb = (COLORPTR)smem_new( wm->fb_xsize * wm->fb_ysize * COLORLEN );
#endif

	rv = 0;
	break;
    }

    return rv;
}

static int sdl_deinit( window_manager* wm )
{
    int rv = -1;
    sdl_data* dd = (sdl_data*)wm->dd;

    while( 1 )
    {
	slog( "SDL: video deinit...\n" );
#ifdef SDL12
        //SDL 1.2:
	smem_free( dd->sdl_zoom_buffer );
	dd->sdl_zoom_buffer = NULL;
#else
        //SDL 2:
        if( dd->sdl_window )
        {
    	    if( !( wm->flags & WIN_INIT_FLAG_FULLSCREEN ) )
    	    {
		uint32_t win_flags = SDL_GetWindowFlags( dd->sdl_window );
		if( win_flags & SDL_WINDOW_MAXIMIZED )
		    wm->flags |= WIN_INIT_FLAG_MAXIMIZED;
		else
		    wm->flags &= ~WIN_INIT_FLAG_MAXIMIZED;
	    }
	}
	smem_free( wm->fb );
        if( dd->sdl_screen ) SDL_DestroyTexture( dd->sdl_screen );
	if( dd->sdl_renderer ) SDL_DestroyRenderer( dd->sdl_renderer );
        if( dd->sdl_window ) SDL_DestroyWindow( dd->sdl_window );
        wm->fb = NULL;
        dd->sdl_screen = NULL;
        dd->sdl_renderer = NULL;
        dd->sdl_window = NULL;
#endif
	rv = 0;
	break;
    }

    return rv;
}

static int sdl_handle_event( window_manager* wm )
{
    sdl_data* dd = (sdl_data*)wm->dd;
    SDL_Event event;
    int key = 0;
    int hide_mod = 0;

    int evt_count = 0;
    while( SDL_PollEvent( &event ) )
    {
	evt_count++;
	switch( event.type ) 
	{
	    case SDL_MOUSEBUTTONDOWN:
	    case SDL_MOUSEBUTTONUP:
#ifndef SDL12
#ifdef MULTITOUCH
		if( event.button.which == SDL_TOUCH_MOUSEID )
		{
		    //Events that were generated by a touch input device, and not a real mouse. 
		    //Ignore such events, because we can handle SDL_TouchFingerEvent.
		    break;
		}
#endif
#endif
		key = 0;
		switch( event.button.button )
		{
		    case SDL_BUTTON_LEFT: key = MOUSE_BUTTON_LEFT; break;
		    case SDL_BUTTON_MIDDLE: key = MOUSE_BUTTON_MIDDLE; break;
		    case SDL_BUTTON_RIGHT: key = MOUSE_BUTTON_RIGHT; break;
#ifdef SDL12
		    case SDL_BUTTON_WHEELUP: if( event.type == SDL_MOUSEBUTTONDOWN ) key = MOUSE_BUTTON_SCROLLUP; break;
		    case SDL_BUTTON_WHEELDOWN: if( event.type == SDL_MOUSEBUTTONDOWN ) key = MOUSE_BUTTON_SCROLLDOWN; break;
#endif
		}
		if( key )
		{
		    int x = event.button.x;
		    int y = event.button.y;
		    convert_real_window_xy( x, y, wm );
		    if( event.type == SDL_MOUSEBUTTONDOWN )
			send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, x, y, key, 0, 1024, wm );
		    else
			send_event( 0, EVT_MOUSEBUTTONUP, wm->mods, x, y, key, 0, 1024, wm );
		}
		break;
	    case SDL_MOUSEMOTION:
#ifndef SDL12
#ifdef MULTITOUCH
		if( event.motion.which == SDL_TOUCH_MOUSEID )
		{
		    //Events that were generated by a touch input device, and not a real mouse. 
		    //Ignore such events, because we can handle SDL_TouchFingerEvent.
		    break;
		}
#endif
#endif
		{
		    key = 0;
		    if( event.motion.state & SDL_BUTTON( SDL_BUTTON_LEFT ) ) key |= MOUSE_BUTTON_LEFT;
		    if( event.motion.state & SDL_BUTTON( SDL_BUTTON_MIDDLE ) ) key |= MOUSE_BUTTON_MIDDLE;
		    if( event.motion.state & SDL_BUTTON( SDL_BUTTON_RIGHT ) ) key |= MOUSE_BUTTON_RIGHT;
		    int x = event.motion.x;
		    int y = event.motion.y;
		    convert_real_window_xy( x, y, wm );
		    send_event( 0, EVT_MOUSEMOVE, wm->mods, x, y, key, 0, 1024, wm );
		}
		break;
	    case SDL_KEYDOWN:
	    case SDL_KEYUP:
		hide_mod = 0;
		key = event.key.keysym.sym;
		if( key > 255 )
		{
		    switch( key )
		    {
			case SDLK_UP: key = KEY_UP; break;
			case SDLK_DOWN: key = KEY_DOWN; break;
			case SDLK_LEFT: key = KEY_LEFT; break;
			case SDLK_RIGHT: key = KEY_RIGHT; break;
			case SDLK_INSERT: key = KEY_INSERT; break;
			case SDLK_HOME: key = KEY_HOME; break;
			case SDLK_END: key = KEY_END; break;
			case SDLK_PAGEUP: key = KEY_PAGEUP; break;
			case SDLK_PAGEDOWN: key = KEY_PAGEDOWN; break;
			case SDLK_F1: key = KEY_F1; break;
			case SDLK_F2: key = KEY_F2; break;
			case SDLK_F3: key = KEY_F3; break;
			case SDLK_F4: key = KEY_F4; break;
			case SDLK_F5: key = KEY_F5; break;
			case SDLK_F6: key = KEY_F6; break;
			case SDLK_F7: key = KEY_F7; break;
			case SDLK_F8: key = KEY_F8; break;
			case SDLK_F9: key = KEY_F9; break;
			case SDLK_F10: key = KEY_F10; break;
			case SDLK_F11: key = KEY_F11; break;
			case SDLK_F12: key = KEY_F12; break;
			case SDLK_CAPSLOCK: key = KEY_CAPS; break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT:
			    if( event.type == SDL_KEYDOWN ) wm->mods |= EVT_FLAG_SHIFT; else wm->mods &= ~EVT_FLAG_SHIFT;
			    key = KEY_SHIFT;
			    break;
			case SDLK_RCTRL:
			case SDLK_LCTRL:
			    if( event.type == SDL_KEYDOWN ) wm->mods |= EVT_FLAG_CTRL; else wm->mods &= ~EVT_FLAG_CTRL;
			    key = KEY_CTRL;
			    break;
			case SDLK_RALT:
			case SDLK_LALT:
			    if( event.type == SDL_KEYDOWN ) wm->mods |= EVT_FLAG_ALT; else wm->mods &= ~EVT_FLAG_ALT;
			    key = KEY_ALT;
			    break;
			case SDLK_MODE:
			    if( event.type == SDL_KEYDOWN ) wm->mods |= EVT_FLAG_MODE; else wm->mods &= ~EVT_FLAG_MODE;
			    key = 0;
			    break;
#ifdef SDL12
			case SDLK_KP0: key = '0'; break;
			case SDLK_KP1: key = '1'; break;
			case SDLK_KP2: key = '2'; break;
			case SDLK_KP3: key = '3'; break;
			case SDLK_KP4: key = '4'; break;
			case SDLK_KP5: key = '5'; break;
			case SDLK_KP6: key = '6'; break;
			case SDLK_KP7: key = '7'; break;
			case SDLK_KP8: key = '8'; break;
			case SDLK_KP9: key = '9'; break;
#else
			case SDLK_KP_0: key = '0'; break;
			case SDLK_KP_1: key = '1'; break;
			case SDLK_KP_2: key = '2'; break;
			case SDLK_KP_3: key = '3'; break;
			case SDLK_KP_4: key = '4'; break;
			case SDLK_KP_5: key = '5'; break;
			case SDLK_KP_6: key = '6'; break;
			case SDLK_KP_7: key = '7'; break;
			case SDLK_KP_8: key = '8'; break;
			case SDLK_KP_9: key = '9'; break;
#endif
			case SDLK_KP_ENTER: key = KEY_ENTER; break;
			default:
			    key += KEY_UNKNOWN;
			    //or don't touch the key code (it's unicode from SDL), and set flags |= UNICODE;
			    break;
		    }
		}
		else
		{
		    switch( key )
		    {
			case SDLK_RETURN: key = KEY_ENTER; break;
			case SDLK_DELETE: key = KEY_DELETE; break;
			case SDLK_BACKSPACE: key = KEY_BACKSPACE; break;
		    }
		}
#ifdef OS_MAEMO
		if( wm->mods & EVT_FLAG_MODE )
		{
		    switch( key )
		    {
			case 'q': key = '1'; hide_mod = EVT_FLAG_MODE; break;
			case 'w': key = '2'; hide_mod = EVT_FLAG_MODE; break;
			case 'e': key = '3'; hide_mod = EVT_FLAG_MODE; break;
			case 'r': key = '4'; hide_mod = EVT_FLAG_MODE; break;
			case 't': key = '5'; hide_mod = EVT_FLAG_MODE; break;
			case 'y': key = '6'; hide_mod = EVT_FLAG_MODE; break;
			case 'u': key = '7'; hide_mod = EVT_FLAG_MODE; break;
			case 'i': key = '8'; hide_mod = EVT_FLAG_MODE; break;
			case 'o': key = '9'; hide_mod = EVT_FLAG_MODE; break;
			case 'p': key = '0'; hide_mod = EVT_FLAG_MODE; break;
			case 'Q': key = '1'; hide_mod = EVT_FLAG_MODE; break;
			case 'W': key = '2'; hide_mod = EVT_FLAG_MODE; break;
			case 'E': key = '3'; hide_mod = EVT_FLAG_MODE; break;
			case 'R': key = '4'; hide_mod = EVT_FLAG_MODE; break;
			case 'T': key = '5'; hide_mod = EVT_FLAG_MODE; break;
			case 'Y': key = '6'; hide_mod = EVT_FLAG_MODE; break;
			case 'U': key = '7'; hide_mod = EVT_FLAG_MODE; break;
			case 'I': key = '8'; hide_mod = EVT_FLAG_MODE; break;
			case 'O': key = '9'; hide_mod = EVT_FLAG_MODE; break;
			case 'P': key = '0'; hide_mod = EVT_FLAG_MODE; break;
			case KEY_LEFT: key = KEY_UP; hide_mod = EVT_FLAG_MODE; break;
			case KEY_RIGHT: key = KEY_DOWN; hide_mod = EVT_FLAG_MODE; break;
		    }
		}
#endif
		if( key != 0 )
		{
#ifdef SDL12
		    if( event.type == SDL_KEYDOWN || key == KEY_CAPS )
#else
		    if( event.type == SDL_KEYDOWN )
#endif
	    		send_event( 0, EVT_BUTTONDOWN, wm->mods & ~hide_mod, 0, 0, key, event.key.keysym.scancode, 1024, wm );
		    else
	    		send_event( 0, EVT_BUTTONUP, wm->mods & ~hide_mod, 0, 0, key, event.key.keysym.scancode, 1024, wm );
		}
		break;
#ifdef SDL12
	    //SDL 1.2:
	    case SDL_VIDEORESIZE:
		//slog( "RESIZE %d %d; ZOOM %d;\n", event.resize.w, event.resize.h, wm->screen_zoom );
		SDL_SetVideoMode( 
		    event.resize.w, event.resize.h, COLORBITS,
                    SDL_HWSURFACE | SDL_RESIZABLE ); // Resize window
		wm->real_window_width = event.resize.w;
		wm->real_window_height = event.resize.h;
		wm->screen_xsize = event.resize.w;
		wm->screen_ysize = event.resize.h;
		win_zoom_apply( wm );
		win_angle_apply( wm );
		dd->sdl_zoom_buffer = smem_resize( dd->sdl_zoom_buffer, wm->screen_xsize * wm->screen_ysize * COLORLEN );
		if( wm->root_win )
		{
		    bool need_recalc = false;
		    if( wm->root_win->x + wm->root_win->xsize > wm->screen_xsize ) 
		    {
			wm->root_win->xsize = wm->screen_xsize - wm->root_win->x;
			need_recalc = true;
		    }
		    if( wm->root_win->y + wm->root_win->ysize > wm->screen_ysize ) 
		    {
			wm->root_win->ysize = wm->screen_ysize - wm->root_win->y;
			need_recalc = true;
		    }
		    if( need_recalc ) recalc_regions( wm );
		}
		send_event( wm->root_win, EVT_SCREENRESIZE, wm );
		break;
#else
	    //SDL 2:
	    case SDL_WINDOWEVENT:
	        switch( event.window.event ) 
	        {
	    	    case SDL_WINDOWEVENT_SHOWN:
	    	    case SDL_WINDOWEVENT_EXPOSED:
	    		send_event( wm->root_win, EVT_DRAW, wm );
	    		break;
	    	    case SDL_WINDOWEVENT_SIZE_CHANGED:
			wm->real_window_width = event.window.data1;
			wm->real_window_height = event.window.data2;
			wm->screen_xsize = event.window.data1;
			wm->screen_ysize = event.window.data2;
			win_zoom_apply( wm );
			win_angle_apply( wm );
			SDL_DestroyTexture( dd->sdl_screen );
			//SDL_RenderSetLogicalSize( dd->sdl_renderer, wm->screen_xsize, wm->screen_ysize );
			wm->fb_xsize = wm->screen_xsize;
			wm->fb_ysize = wm->screen_ysize;
			if( wm->screen_angle & 1 ) { int t = wm->fb_xsize; wm->fb_xsize = wm->fb_ysize; wm->fb_ysize = t; }
    			dd->sdl_screen = SDL_CreateTexture( dd->sdl_renderer, dd->sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, wm->fb_xsize, wm->fb_ysize );
    			SDL_SetTextureBlendMode( dd->sdl_screen, SDL_BLENDMODE_NONE );
    			{
    			    size_t new_fb_size = wm->fb_xsize * wm->fb_ysize * COLORLEN;
			    if( new_fb_size > smem_get_size( wm->fb ) ) wm->fb = (COLORPTR)smem_resize( wm->fb, new_fb_size );
			}
			if( wm->root_win )
			{
			    bool need_recalc = false;
			    if( wm->root_win->x + wm->root_win->xsize > wm->screen_xsize ) 
			    {
				wm->root_win->xsize = wm->screen_xsize - wm->root_win->x;
				need_recalc = true;
			    }
			    if( wm->root_win->y + wm->root_win->ysize > wm->screen_ysize ) 
			    {
				wm->root_win->ysize = wm->screen_ysize - wm->root_win->y;
				need_recalc = true;
			    }
			    if( need_recalc ) recalc_regions( wm );
			}
			send_event( wm->root_win, EVT_SCREENRESIZE, wm );
	    		break;
	    	    default: break;
	        }
	        break;
	    case SDL_MOUSEWHEEL:
		{
		    int y = event.wheel.y;
#if SDL_VERSION_ATLEAST( 2, 0, 4 )
		    if( event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ) y = -y;
#endif
		    if( y > 0 )
			key = MOUSE_BUTTON_SCROLLUP;
		    else
			key = MOUSE_BUTTON_SCROLLDOWN;
		    send_event( 0, EVT_MOUSEBUTTONDOWN, wm->mods, wm->pen_x, wm->pen_y, key, 0, 1024, wm );
		}
		break;
#ifdef MULTITOUCH
	    case SDL_FINGERMOTION:
	    case SDL_FINGERDOWN:
	    case SDL_FINGERUP:
		{
		    int x, y;
		    if( dd->sdl_multitouch_xy_normalized )
		    {
			x = event.tfinger.x * wm->real_window_width;
			y = event.tfinger.y * wm->real_window_height;
		    }
		    else
		    {
			x = event.tfinger.x;
			y = event.tfinger.y;
		    }
		    int pressure = event.tfinger.pressure * 1024;
		    WM_TOUCH_ID id = event.tfinger.touchId + event.tfinger.fingerId;
		    if( event.type == SDL_FINGERDOWN )
		    {
			collect_touch_events( 0, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, pressure, id, wm );
		    }
		    if( event.type == SDL_FINGERMOTION )
		    {
			collect_touch_events( 1, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, pressure, id, wm );
		    }
		    if( event.type == SDL_FINGERUP )
		    {
			collect_touch_events( 2, TOUCH_FLAG_REALWINDOW_XY, wm->mods, x, y, pressure, id, wm );
		    }
		    send_touch_events( wm );
		}
		break;
#endif
#endif
	    case SDL_QUIT:
		send_event( 0, EVT_QUIT, wm );
		break;
	}
    }
    
    return evt_count;
}

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 1;
    
    wm->dd = smem_new( sizeof( sdl_data ) );
    smem_zero( wm->dd );
    sdl_data* dd = (sdl_data*)wm->dd;
    
    while( 1 )
    {
	if( !dd ) break;

#ifdef SDL12
	//if( XInitThreads() == 0 ) slog( "XInitThreads failed\n" ); //Required for OpenGL?
#endif

	wm->prev_frame_time = 0;
	wm->frame_len = 1000 / wm->max_fps;
	wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

	xsize = sconfig_get_int_value( APP_CFG_WIN_XSIZE, xsize, 0 );
	ysize = sconfig_get_int_value( APP_CFG_WIN_YSIZE, ysize, 0 );
    
	dd->sdl_wm_name = name;
	dd->sdl_wm_xsize = xsize;
	dd->sdl_wm_ysize = ysize;
	dd->sdl_wm_flags = wm->flags;
	if( sdl_init( wm ) )
	{
	    break;
	}

#ifdef OS_MAEMO
	if( sconfig_get_int_value( "noautolock", -1, 0 ) != -1 ) 
	{
	    int err = sthread_create( &dd->sdl_screen_control_thread, wm->sd, &screen_control_thread, (void*)wm, 0 );
	    if( err )
	    {
		slog( "sthread_create error %d\n", err );
		break;
	    }
	}
#endif

	retval = 0;
	break;
    }

    if( retval )
    {
	smem_free( wm->dd ); 
	wm->dd = 0;
    }

    return retval;
}

void device_end( window_manager* wm )
{
    sdl_data* dd = (sdl_data*)wm->dd;

    sdl_deinit( wm );

#ifdef OS_MAEMO
    if( sconfig_get_int_value( "noautolock", -1, 0 ) != -1 ) 
    {
	dd->sdl_screen_control_thread_exit_request = 1;
	sthread_destroy( &dd->sdl_screen_control_thread, 1000 * 5 );
    }
#endif
    
    smem_free( wm->dd ); wm->dd = 0;
}

void device_event_handler( window_manager* wm )
{
    sdl_data* dd = (sdl_data*)wm->dd;
    bool pause = false;

    if( sdl_handle_event( wm ) == 0 )
    {
        //No SDL events:
        if( wm->events_count == 0 )
        {
            //And no WM events:
    	    pause = true;
        }
    }

    //if( pause ) stime_sleep( wm->frame_len );

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

void device_screen_lock( WINDOWPTR win, window_manager* wm )
{
    sdl_data* dd = (sdl_data*)wm->dd;
    if( wm->screen_lock_counter == 0 )
    {
#ifdef SDL12
	//SDL 1.2:
	if( wm->screen_zoom == 1 )
	{
	    if( SDL_MUSTLOCK( dd->sdl_screen ) ) 
	    {
		if( SDL_LockSurface( dd->sdl_screen ) < 0 ) 
		{
		    return;
		}
	    }
	    wm->fb = (COLORPTR)dd->sdl_screen->pixels;
	    wm->fb_offset = 0;
	    wm->fb_ypitch = dd->sdl_screen->pitch / COLORLEN;
	    wm->fb_xpitch = 1;
	}
	else 
	{
	    wm->fb = (COLORPTR)dd->sdl_zoom_buffer;
	    wm->fb_offset = 0;
	    wm->fb_ypitch = wm->real_window_width / wm->screen_zoom;
	    wm->fb_xpitch = 1;
	}
#else
	//SDL 2:
	wm->fb_offset = 0;
	wm->fb_ypitch = wm->fb_xsize;
	wm->fb_xpitch = 1;
#endif
	switch( wm->screen_angle )
	{
	    case 1:
		//90:
		{
		    wm->fb_offset = wm->fb_ypitch * ( wm->screen_xsize - 1 );
	            int ttt = wm->fb_ypitch;
    		    wm->fb_ypitch = wm->fb_xpitch;
    		    wm->fb_xpitch = -ttt;
		}
		break;
	    case 2:
		//180:
		{
		    wm->fb_offset = wm->fb_ypitch * ( wm->screen_ysize - 1 ) + wm->fb_xpitch * ( wm->screen_xsize - 1 );
	    	    wm->fb_ypitch = -wm->fb_ypitch;
    		    wm->fb_xpitch = -wm->fb_xpitch;
		}
		break;
	    case 3:
		//270:
		{
		    wm->fb_offset = wm->fb_xpitch * ( wm->screen_ysize - 1 );
	            int ttt = wm->fb_ypitch;
	            wm->fb_ypitch = -wm->fb_xpitch;
	            wm->fb_xpitch = ttt;
		}
		break;
	}
    }
    wm->screen_lock_counter++;

    if( wm->screen_lock_counter > 0 )
	wm->screen_is_active = true;
    else
	wm->screen_is_active = false;
}

void device_screen_unlock( WINDOWPTR win, window_manager* wm )
{
    sdl_data* dd = (sdl_data*)wm->dd;
    if( wm->screen_lock_counter == 1 )
    {
#ifdef SDL12
	//SDL 1.2:
	if( wm->screen_zoom == 1 )
	{
	    if( SDL_MUSTLOCK( dd->sdl_screen ) ) 
	    {
		SDL_UnlockSurface( dd->sdl_screen );
	    }
	}
	else 
	{
	}
	wm->fb = 0;
#else
	//SDL 2:
#endif
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

void device_vsync( bool vsync, window_manager* wm )
{
}

void device_redraw_framebuffer( window_manager* wm )
{
    if( wm->screen_changed == 0 ) return;
    sdl_data* dd = (sdl_data*)wm->dd;

    /*stime_ms_t cur_time = stime_ms();
    static int fps = 0;
    static stime_ms_t fps_t = 0;
    fps++;
    if( cur_time >= fps_t + 1000 )
    {
        printf( "FPS: %d\n", fps );
        fps = 0;
        fps_t = cur_time;
    }*/

    if( wm->flags & WIN_INIT_FLAG_FRAMEBUFFER )
    {
	if( wm->screen_lock_counter == 0 )
	{
#ifdef SDL12
	    //SDL 1.2:
    	    if( wm->screen_zoom == 2 )
    	    {
    		while( 1 )
        	{
            	    if( SDL_MUSTLOCK( dd->sdl_screen ) )
            	    {
                	if( SDL_LockSurface( dd->sdl_screen ) < 0 )
                	{
                    	    break;
                	}
            	    }
            	    COLORPTR s = (COLORPTR)dd->sdl_screen->pixels;
            	    uint fb_ypitch = dd->sdl_screen->pitch / COLORLEN;
            	    uint fb_xpitch = 1;
        	    COLORPTR src = (COLORPTR)dd->sdl_zoom_buffer;
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
            	    if( SDL_MUSTLOCK( dd->sdl_screen ) )
            	    {
                	SDL_UnlockSurface( dd->sdl_screen );
            	    }
            	    break;
        	}
    	    }
    	    SDL_UpdateRect( dd->sdl_screen, 0, 0, 0, 0 );
#else
	    //SDL 2:
	    SDL_UpdateTexture( dd->sdl_screen, NULL, wm->fb, wm->fb_xsize * COLORLEN );
	    SDL_RenderCopy( dd->sdl_renderer, dd->sdl_screen, NULL, NULL );
	    SDL_RenderPresent( dd->sdl_renderer );
#endif
	    dd->sdl_prev_frame_time = stime_ticks();
    	    wm->screen_changed = 0;
	}
	else
	{
    	    slog( "Can't redraw the framebuffer: screen locked!\n" );
	}
    }
    else
    {
	//No framebuffer:
	wm->screen_changed = 0;
    }
}

void device_change_name( const char* name, window_manager* wm )
{
    if( !name ) return;
#ifdef SDL12
    SDL_WM_SetCaption( name, name );
#else
    sdl_data* dd = (sdl_data*)wm->dd;
    SDL_SetWindowTitle( dd->sdl_window, name );
#endif
}

void device_draw_line( int x1, int y1, int x2, int y2, COLOR color, window_manager* wm ) 
{
}

void device_draw_frect( int x, int y, int xsize, int ysize, COLOR color, window_manager* wm )
{
}

void device_draw_image(
    int dest_x, int dest_y,
    int dest_xs, int dest_ys,
    int src_x, int src_y,
    sdwm_image* img,
    window_manager* wm )
{
}
