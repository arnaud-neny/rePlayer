/*
    wm_macos.h - platform-dependent module : macOS
    This file is part of the SunDog engine.
    Copyright (C) 2009 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#pragma once

#include <errno.h>
#include <sys/sem.h>    //semaphores ...

#include "main/macos/sundog_bridge.h"

int g_event_loop_sem;
sthread g_frames_thread;
volatile bool g_frames_thread_exit_request = 0;
const bool g_treat_cmd_as_ctrl = true;
const bool g_treat_ctrl_lclick_as_rclick = false;

#include "wm_opengl.hpp"

#define EVENT semctl( g_event_loop_sem, 0, SETVAL, 1 )

void* frames_thread( void* arg )
{
    window_manager* wm = (window_manager*)arg;
    while( !g_frames_thread_exit_request )
    {
	stime_sleep( 1000 / wm->max_fps );
	wm->frame_event_request = 1;
	EVENT;
    }
    return NULL;
}

void macos_sundog_touches( int x, int y, int mode, int touch_id, window_manager* wm )
{
    if( wm == 0 ) return;
    convert_real_window_xy( x, y, wm );
    int button = 0;
    int mods = wm->mods;
    switch( mode >> 3 )
    {
	case 0: button = MOUSE_BUTTON_LEFT;
	    if( wm->mods_original & EVT_FLAG_CMD )
	    {
		button = MOUSE_BUTTON_MIDDLE;
		mods &= ~EVT_FLAG_CMD;
		if( g_treat_cmd_as_ctrl ) mods &= ~EVT_FLAG_CTRL;
		break;
	    }
	    if( g_treat_ctrl_lclick_as_rclick )
	    {
		if( mods & EVT_FLAG_CTRL )
		{
		    button = MOUSE_BUTTON_RIGHT;
		    mods &= ~EVT_FLAG_CTRL;
		}
	    }
	    break;
	case 1: button = MOUSE_BUTTON_RIGHT; break;
	case 2: button = MOUSE_BUTTON_MIDDLE; break;
    }
    bool evt = 0;
    switch( mode & 7 )
    {
	case 0:
	    send_event( 0, EVT_MOUSEBUTTONDOWN, mods, x, y, button, 0, 1024, wm );
	    evt = 1;
	    break;
	case 1:
	    send_event( 0, EVT_MOUSEMOVE, mods, x, y, button, 0, 1024, wm );
	    evt = 1;
	    break;
	case 2:
	    send_event( 0, EVT_MOUSEBUTTONUP, mods, x, y, button, 0, 1024, wm );
	    evt = 1;
	    break;
	case 3:
	    send_event( 0, EVT_MOUSEBUTTONDOWN, mods, x, y, MOUSE_BUTTON_SCROLLDOWN, 0, 1024, wm );
	    evt = 1;
	    break;
	case 4:
	    send_event( 0, EVT_MOUSEBUTTONDOWN, mods, x, y, MOUSE_BUTTON_SCROLLUP, 0, 1024, wm );
	    evt = 1;
	    break;
    }
    if( evt ) EVENT;
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//macOS Virtual Key Code -> SunDog Key Code
//vk is a physical key code, not a real character code corresponding to this button!
//Currently, only code matching for the standard English layout is supported.
//In future updates it must be converted to real character codes (keysym)! (like in X11,SDL,WIN,ANDROID,iOS)
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
int vk2key( unsigned short vk )
{
    int key;
    switch( vk )
    {
	case 53: key = KEY_ESCAPE; break;
	case 122: key = KEY_F1; break;
	case 120: key = KEY_F2; break;
	case 99: key = KEY_F3; break;
	case 118: key = KEY_F4; break;
	case 96: key = KEY_F5; break;
	case 97: key = KEY_F6; break;
	case 98: key = KEY_F7; break;
	case 100: key = KEY_F8; break;
	case 101: key = KEY_F9; break;
	case 109: key = KEY_F10; break;
	case 103: key = KEY_F11; break;
	case 111: key = KEY_F12; break;
	    
	case 114: key = KEY_INSERT; break;
	case 117: key = KEY_DELETE; break;
	case 115: key = KEY_HOME; break;
	case 119: key = KEY_END; break;
	case 116: key = KEY_PAGEUP; break;
	case 121: key = KEY_PAGEDOWN; break;
	    
   	case 126: key = KEY_UP; break;
   	case 125: key = KEY_DOWN; break;
   	case 123: key = KEY_LEFT; break;
   	case 124: key = KEY_RIGHT; break;
	    
   	case 50: key = '`'; break;
   	case 18: key = '1'; break;
   	case 19: key = '2'; break;
   	case 20: key = '3'; break;
   	case 21: key = '4'; break;
   	case 23: key = '5'; break;
   	case 22: key = '6'; break;
   	case 26: key = '7'; break;
   	case 28: key = '8'; break;
   	case 25: key = '9'; break;
   	case 29: key = '0'; break;
   	case 27: key = '-'; break;
   	case 24: key = '='; break;
   	case 51: key = KEY_BACKSPACE; break;
	    
   	case 48: key = KEY_TAB; break;
   	case 12: key = 'q'; break;
   	case 13: key = 'w'; break;
   	case 14: key = 'e'; break;
   	case 15: key = 'r'; break;
   	case 17: key = 't'; break;
   	case 16: key = 'y'; break;
   	case 32: key = 'u'; break;
   	case 34: key = 'i'; break;
   	case 31: key = 'o'; break;
   	case 35: key = 'p'; break;
   	case 33: key = '['; break;
   	case 30: key = ']'; break;
   	case 42: key = '\\'; break;
	    
	case 57: key = KEY_CAPS; break;
	case 0: key = 'a'; break;
	case 1: key = 's'; break;
	case 2: key = 'd'; break;
	case 3: key = 'f'; break;
	case 5: key = 'g'; break;
	case 4: key = 'h'; break;
	case 38: key = 'j'; break;
	case 40: key = 'k'; break;
	case 37: key = 'l'; break;
	case 41: key = ';'; break;
	case 39: key = '\''; break;
	case 36: key = KEY_ENTER; break;
	    
	case 56: key = KEY_SHIFT; break;
	case 60: key = KEY_SHIFT; break;
	case 6: key = 'z'; break;
	case 7: key = 'x'; break;
	case 8: key = 'c'; break;
	case 9: key = 'v'; break;
	case 11: key = 'b'; break;
	case 45: key = 'n'; break;
	case 46: key = 'm'; break;
	case 43: key = ','; break;
	case 47: key = '.'; break;
	case 44: key = '/'; break;
	    
	case 55: key = KEY_CMD; break;
	case 59: key = KEY_CTRL; break;
	case 58: key = KEY_ALT; break;
	case 49: key = KEY_SPACE; break;
	    
	default: key = KEY_UNKNOWN + vk;
    }
    return key;
}

void macos_sundog_key( unsigned short vk, bool pushed, window_manager* wm )
{
    if( !wm ) return;
    int key = vk2key( vk );
    if( key == KEY_CMD )
    {
	if( pushed )
	    wm->mods_original |= EVT_FLAG_CMD;
	else
	    wm->mods_original &= ~EVT_FLAG_CMD;
	if( g_treat_cmd_as_ctrl )
	    key = KEY_CTRL;
    }
    while( 1 )
    {
	if( wm->mods_original & EVT_FLAG_CMD )
	{
	    if( key == 'i' )
	    {
		if( pushed )
		    send_event( 0, EVT_BUTTONDOWN, wm->mods & ~( EVT_FLAG_CMD | EVT_FLAG_CTRL ), 0, 0, KEY_INSERT, vk, 1024, wm );
		else
		    send_event( 0, EVT_BUTTONUP, wm->mods & ~( EVT_FLAG_CMD | EVT_FLAG_CTRL ), 0, 0, KEY_INSERT, vk, 1024, wm );
		break;
	    }
	}
	if( pushed )
	{
	    switch( key )
	    {
		case KEY_CMD: wm->mods |= EVT_FLAG_CMD; break;
		case KEY_CTRL: wm->mods |= EVT_FLAG_CTRL; break;
		case KEY_ALT: wm->mods |= EVT_FLAG_ALT; break;
		case KEY_SHIFT: wm->mods |= EVT_FLAG_SHIFT; break;
	    }
	    send_event( 0, EVT_BUTTONDOWN, wm->mods, 0, 0, key, vk, 1024, wm );
	}
	else
	{
	    switch( key )
	    {
		case KEY_CMD: wm->mods &= ~EVT_FLAG_CMD; break;
		case KEY_CTRL: wm->mods &= ~EVT_FLAG_CTRL; break;
		case KEY_ALT: wm->mods &= ~EVT_FLAG_ALT; break;
		case KEY_SHIFT: wm->mods &= ~EVT_FLAG_SHIFT; break;
	    }
	    send_event( 0, EVT_BUTTONUP, wm->mods, 0, 0, key, vk, 1024, wm );
	}
	break;
    }
    EVENT;
}

int device_start( const char* name, int xsize, int ysize, window_manager* wm )
{
    int retval = 0;
    
    g_frames_thread_exit_request = 0;

    //Init the semaphore:
    g_event_loop_sem = semget( 827364, 1, 0666 | IPC_CREAT );
    if( g_event_loop_sem == -1 )
    {
	slog( "Error in semget() (%d)\n", errno );
	return 1;
    }
    semctl( g_event_loop_sem, 0, SETVAL, 0 );

    xsize = g_macos_sundog_screen_xsize;
    ysize = g_macos_sundog_screen_ysize;
    wm->real_window_width = xsize;
    wm->real_window_height = ysize;
    
    wm->screen_xsize = xsize;
    wm->screen_ysize = ysize;
    win_angle_apply( wm );
    
    if( name )
        macos_sundog_rename_window( name );

    if( wm->screen_ppi == 0 ) wm->screen_ppi = g_macos_sundog_screen_ppi;
    win_zoom_init( wm );
    win_zoom_apply( wm );

#ifdef GLNORETAIN
    wm->screen_buffer_preserved = 0;
#else
    wm->screen_buffer_preserved = 1;
#endif

    if( gl_init( wm ) ) return 1;
    gl_resize( wm );
    
    //Create frames thread:
    int err = sthread_create( &g_frames_thread, wm->sd, &frames_thread, (void*)wm, 0 );
    if( err )
    {
	slog( "sthread_create error %d\n", err );
	return 1;
    }
    
    return retval;
}

void device_end( window_manager* wm )
{
    gl_deinit( wm );
    
    //Stop the frames thread:
    g_frames_thread_exit_request = 1;
    sthread_destroy( &g_frames_thread, 1000 * 5 );
    
    //Close the semaphore:
    semctl( g_event_loop_sem, 0, IPC_RMID, 0 );
}

void device_event_handler( window_manager* wm )
{
    macos_sundog_event_handler( wm );
    
    if( wm->real_window_width != g_macos_sundog_screen_xsize ||
	wm->real_window_height != g_macos_sundog_screen_ysize ||
        g_macos_sundog_resize_request )
    {
        g_macos_sundog_resize_request = 0;
	wm->real_window_width = g_macos_sundog_screen_xsize;
	wm->real_window_height = g_macos_sundog_screen_ysize;
	wm->screen_xsize = g_macos_sundog_screen_xsize;
	wm->screen_ysize = g_macos_sundog_screen_ysize;
	win_zoom_apply( wm );
	if( wm->screen_angle & 1 )
        {
            int temp = wm->screen_xsize;
            wm->screen_xsize = wm->screen_ysize;
            wm->screen_ysize = temp;
        }
	gl_resize( wm );
	send_event( wm->root_win, EVT_SCREENRESIZE, wm );
    }
    
    if( wm->events_count == 0 )
    {
        struct sembuf operation[ 1 ];
        operation[ 0 ].sem_op  = -1; //while( semaphore == 0 ) {} semaphore--;
        operation[ 0 ].sem_num = 0;
        operation[ 0 ].sem_flg = SEM_UNDO;
        semop( g_event_loop_sem, operation, 1 );
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
    macos_sundog_rename_window( name );
}
