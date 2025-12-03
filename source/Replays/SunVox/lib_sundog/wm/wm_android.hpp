/*
    wm_android.h - platform-dependent module : Android
    This file is part of the SunDog engine.
    Copyright (C) 2011 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include <errno.h>

#include "main/android/sundog_bridge.h"

#include "wm_opengl.hpp"

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;

    slog( "Android version: %s\n", g_android_version_correct );

    wm->prev_frame_time = 0;
    wm->frame_len = 1000 / wm->max_fps;
    wm->ticks_per_frame = stime_ticks_per_second() / wm->max_fps;

    wm->flags |= WIN_INIT_FLAG_FULLSCREEN;
    wm->flags |= WIN_INIT_FLAG_TOUCHCONTROL;
    /*if( g_android_version_nums[ 0 ] >= 10 )
    {
	wm->flags |= WIN_INIT_FLAG_NOSYSBARS;
	//16 dec 2019:
	//Starting from OS 10, the navigation bar is always shown on top of the content.
	//Maybe because each app is now required to take into account the size of the insets?
	//Experiments with styles and options did not help...
	//Simple workaround: just switch to the immersive fullscreen mode. At least it works fine in the emulator.
    }*/

    if( wm->flags & WIN_INIT_FLAG_NOSYSBARS )
	android_sundog_set_system_ui_visibility( wm->sd, 0 );
    else
	android_sundog_set_system_ui_visibility( wm->sd, 1 );
    android_sundog_set_safe_area( wm->sd );

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
    wm->fb = (COLORPTR)g_android_sundog_framebuffer;
    wm->fb_offset = 0;
    wm->fb_ypitch = wm->sd->screen_xsize;
    wm->fb_xpitch = 1;
#else
    wm->screen_buffer_preserved = android_sundog_get_gl_buffer_preserved( wm->sd );
    slog( "screen_buffer_preserved = %d\n", wm->screen_buffer_preserved );
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
    android_sundog_event_handler( wm );
    if( wm->exit_request ) return;

    int resize = 0;

    volatile int w = wm->sd->screen_changed_w;
    while( w != wm->sd->screen_changed_r )
    {
	slog( "Screen changed...\n" );
        wm->sd->screen_changed_r = w;
        resize = 1;

        EGLDisplay display = android_sundog_get_egl_display( wm->sd );
	EGLSurface surface = android_sundog_get_egl_surface( wm->sd );

	for( int i = 0; i < 5; i++ ) //delayed surface resize?
	{
	    EGLint w = 0;
	    EGLint h = 0;
	    eglQuerySurface( display, surface, EGL_WIDTH, &w );
	    eglQuerySurface( display, surface, EGL_HEIGHT, &h );
	    if( w && h )
	    {
		bool size_changed = false;
		if( wm->sd->screen_xsize != w || wm->sd->screen_ysize != h ) size_changed = true;
		wm->sd->screen_xsize = w;
		wm->sd->screen_ysize = h;
		wm->real_window_width = wm->sd->screen_xsize;
		wm->real_window_height = wm->sd->screen_ysize;
		android_sundog_set_safe_area( wm->sd );
		wm->real_window_safe_area.x = wm->sd->screen_safe_area[ 0 ];
	        wm->real_window_safe_area.y = wm->sd->screen_safe_area[ 1 ];
	        wm->real_window_safe_area.w = wm->sd->screen_safe_area[ 2 ];
	        wm->real_window_safe_area.h = wm->sd->screen_safe_area[ 3 ];
		wm->screen_xsize = wm->real_window_width;
		wm->screen_ysize = wm->real_window_height;
		wm->screen_safe_area = wm->real_window_safe_area;
		resize = 2;
		if( size_changed )
		{
		    slog( "New screen size: %d %d (%d %d %d %d)\n", w, h, 
			wm->sd->screen_safe_area[ 0 ], wm->sd->screen_safe_area[ 1 ],
			wm->sd->screen_safe_area[ 2 ], wm->sd->screen_safe_area[ 3 ] );
		    break;
		}
		android_sundog_screen_redraw( wm->sd ); //eglSwapBuffers can resize surface prior to copying its pixels to the native window
	    }
	    stime_sleep( 10 );
	}

        stime_sleep( 5 );
        w = wm->sd->screen_changed_w;
    }

    if( resize )
    {
	if( resize == 2 )
	{
    	    win_zoom_apply( wm );
	    win_angle_apply( wm );
    	    gl_resize( wm );
    	}
        send_event( wm->root_win, EVT_SCREENRESIZE, wm );
    }

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

bool device_video_capture_supported( window_manager* wm )
{
    return false;
}

const char* device_video_capture_get_file_ext( window_manager* wm )
{
    return "mp4";
}

int device_video_capture_start( window_manager* wm )
{
    /*int rv = android_sundog_glcapture_start( wm->screen_xsize, wm->screen_ysize, wm->vcap_in_fps, wm->vcap_in_bitrate_kb );
    if( rv == 0 )
    {
	uint flags = 0;
	if( wm->vcap_in_flags & VCAP_FLAG_AUDIO_FROM_INPUT ) flags |= SCAP_FLAG_INPUT;
	sundog_sound_capture_start( wm->sd->ss, "1:/glcapture.wav", flags );
    }
    return rv;*/
    return -1;
}

int device_video_capture_frame_begin( window_manager* wm )
{
    //return android_sundog_glcapture_frame_begin();
    return -1;
}

int device_video_capture_frame_end( window_manager* wm )
{
    //return android_sundog_glcapture_frame_end();
    return -1;
}

int device_video_capture_stop( window_manager* wm )
{
    /*int rv = android_sundog_glcapture_stop();
    sundog_sound_capture_stop( wm->sd->ss );
    return rv;*/
    return -1;
}

int device_video_capture_encode( window_manager* wm )
{
    /*int rv = -1;
    char* name_in = sfs_make_filename( wm->sd, wm->vcap_in_name, true );
    rv = android_sundog_glcapture_encode( name_in );
    smem_free( name_in );
    return rv;*/
    return -1;
}
