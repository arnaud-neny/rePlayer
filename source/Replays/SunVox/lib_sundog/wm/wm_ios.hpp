/*
    wm_ios.h - platform-dependent module : iOS
    This file is part of the SunDog engine.
    Copyright (C) 2009 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include <errno.h>
#include <sys/sem.h>    //semaphores ...

#include "main/ios/sundog_bridge.h"

#ifdef OPENGL
    #include "wm_opengl.hpp"
#endif

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;
    
    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

    wm->flags |= WIN_INIT_FLAG_FULLSCREEN;
    wm->flags |= WIN_INIT_FLAG_TOUCHCONTROL;
    
    xsize = wm->sd->screen_xsize;
    ysize = wm->sd->screen_ysize;
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;
    wm->real_window_safe_area.x = wm->sd->screen_safe_area[ 0 ];
    wm->real_window_safe_area.y = wm->sd->screen_safe_area[ 1 ];
    wm->real_window_safe_area.w = wm->sd->screen_safe_area[ 2 ];
    wm->real_window_safe_area.h = wm->sd->screen_safe_area[ 3 ];

    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    wm->screen_safe_area = wm->real_window_safe_area;
    win_angle_apply( wm );

    if( wm->screen_ppi == 0 ) wm->screen_ppi = wm->sd->screen_ppi;
    win_zoom_init( wm );
    win_zoom_apply( wm );

#ifdef FRAMEBUFFER
    /*wm->fb = (COLORPTR)g_ios_sundog_framebuffer;
    wm->fb_offset = 0;
    wm->fb_ypitch = g_ios_sundog_screen_xsize;
    wm->fb_xpitch = 1;*/
    #error FRAMEBUFFER mode is not supported on iOS
#else
    #ifdef GLNORETAIN
	wm->screen_buffer_preserved = 0;
    #else
	wm->screen_buffer_preserved = 1;
    #endif
#endif

#ifdef OPENGL
    if( gl_init( wm ) ) return 1;
    gl_resize( wm );
#endif

    return retval;
}

void device_end( window_manager* wm )
{
#ifdef OPENGL
    gl_deinit( wm );
#endif
}

void device_event_handler( window_manager* wm )
{
    ios_sundog_event_handler( wm );
    if( wm->exit_request ) return;

    bool resize = false;
    volatile int w = wm->sd->screen_changed_w;
    while( w != wm->sd->screen_changed_r )
    {
	//printf( "resize %d\n", wm->sd->screen_changed_r );
	wm->sd->screen_changed_r = w;
	resize = true;
	wm->real_window_width = wm->sd->screen_xsize;
	wm->real_window_height = wm->sd->screen_ysize;
	wm->real_window_safe_area.x = wm->sd->screen_safe_area[ 0 ];
	wm->real_window_safe_area.y = wm->sd->screen_safe_area[ 1 ];
	wm->real_window_safe_area.w = wm->sd->screen_safe_area[ 2 ];
	wm->real_window_safe_area.h = wm->sd->screen_safe_area[ 3 ];
	wm->screen_xsize = wm->real_window_width;
	wm->screen_ysize = wm->real_window_height;
	wm->screen_safe_area = wm->real_window_safe_area;
	stime_sleep( 5 );
	w = wm->sd->screen_changed_w;
    }
    if( resize )
    {
	win_zoom_apply( wm );
	win_angle_apply( wm );
	gl_resize( wm );
	send_event( wm->root_win, EVT_SCREENRESIZE, wm );
    }

    //if( wm->events_count == 0 )
	//stime_sleep( wm->frame_len );

    stime_ticks_t cur_t = stime_ticks();
    if( cur_t - wm->prev_frame_time >= wm->ticks_per_frame )
    {
	wm->frame_event_request = 1;
	wm->prev_frame_time = cur_t;
    }
    else
    {
	if( wm->events_count == 0 )
	{
	    int pause_ms = ( wm->ticks_per_frame - ( cur_t - wm->prev_frame_time ) ) * 1000 / stime_ticks_per_second();
            stime_sleep( pause_ms );
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
}
