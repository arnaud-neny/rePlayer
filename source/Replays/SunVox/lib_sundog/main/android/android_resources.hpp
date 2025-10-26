#pragma once

//Unpack the built-in app resources from the "files" ZIP archive to the current working directory 1:/

#include "main/android/sundog_bridge.h"

void copy_resources( window_manager* wm )
{
    show_status_message( wm_get_string( STR_WM_RES_UNPACKING ), 0, wm );
    wm->device_redraw_framebuffer( wm );
    int rv = android_sundog_copy_resources( wm->sd );
    char* ts = SMEM_ALLOC2( char, 4096 );
    switch( rv )
    {
	case 0:
	    break;
        default:
    	    {
    		const char* err = wm_get_string( STR_WM_RES_UNKNOWN );
    		switch( rv )
    		{
    		    case -1: err = wm_get_string( STR_WM_RES_LOCAL_RES ); break;
    		    case -2: err = wm_get_string( STR_WM_RES_SMEM_CARD ); break;
    		    default: break;
    		}
    		sprintf( ts, "!%s %d:\n%s", wm_get_string( STR_WM_RES_UNPACK_ERROR ), rv, err );
    		dialog( 0, ts, "OK", wm );
    	    }
            wm->exit_request = 1;
            break;
    }
    smem_free( ts );
}

void check_resources( window_manager* wm )
{
}
