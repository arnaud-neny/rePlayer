/*
    wm_hnd_color_theme.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2010 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

const uint8_t g_color_themes[] = 
{
    //0
    0, 0, 0,
    0, 16, 0,
    120, 200, 120,
    255, 255, 255,
    
    //1
    255, 255, 255,
    150, 145, 140,
    60, 50, 40,
    0, 0, 0,

    //2
    0, 0, 0,
    14, 24, 14,
    130, 220, 130,
    255, 255, 255,

    //3
    0, 0, 0,
    20, 20, 20,
    180, 180, 180,
    255, 255, 255,

    //4
    0, 0, 0,
    20, 0, 0,
    240, 130, 130,
    255, 255, 255,

    //5
    0, 0, 0,
    0, 0, 20,
    140, 140, 240,
    255, 255, 255,

    //6
    255, 255, 255,
    140, 140, 140,
    50, 50, 50,
    0, 0, 0,

    //7
    255, 255, 255,
    130, 140, 150,
    40, 50, 60,
    0, 0, 0,

    //8
    255, 255, 255,
    130, 110, 100,
    80, 20, 10,
    0, 0, 0,

    //9
    0, 0, 0,
    10, 10, 40,
    120, 120, 220,
    255, 255, 255,

    //10
    0, 0, 0,
    20, 0, 20,
    200, 120, 200,
    255, 255, 255,

    //11
    0, 0, 0,
    20, 20, 0,
    0, 210, 100,
    255, 255, 255,

    //12
    0, 0, 0,
    40, 40, 40,
    190, 190, 190,
    255, 255, 255,

    //13
    255, 255, 255,
    80, 80, 90,
    16, 16, 32,
    0, 0, 0,

    //14
    255, 255, 255,
    80, 90, 80,
    16, 32, 16,
    0, 0, 0,

    //15
    255, 255, 255,
    70, 100, 100,
    16, 40, 40,
    0, 0, 0,

    //16
    255, 255, 255,
    70, 100, 180,
    16, 40, 40,
    0, 0, 0,

    //17
    0, 0, 0,
    10, 20, 20,
    180, 190, 200,
    255, 255, 255,

    //18
    0, 0, 0,
    10, 30, 30,
    200, 120, 100,
    255, 255, 255,

    //19
    0, 0, 0,
    28, 28, 28,
    200, 200, 50,
    255, 255, 255,
    
    //20
    255, 255, 255,
    80, 140, 210,
    16, 40, 80,
    0, 0, 0,

    //21
    255, 255, 255,
    80, 210, 210,
    32, 80, 80,
    0, 0, 0,

    //22
    255, 255, 255,
    220, 220, 150,
    80, 80, 32,
    0, 0, 0,

    //23
    255, 255, 255,
    220, 220, 220,
    80, 80, 80,
    0, 0, 0,

    //24
    255, 255, 255,
    220, 210, 200,
    80, 40, 32,
    0, 0, 0,
};

const uint8_t g_color_themes_remap[] = 
{
    0,
    2,
    11,
    5,
    9,
    10,
    12,
    3,
    17,
    18,
    4,
    19,
    23,
    1,
    6,
    14,
    15,
    7,
    16,
    20,
    21,
    24,
    8,
    22,
};

bool g_color_theme_changed = false;

struct colortheme_data
{
    WINDOWPTR win;
    
    int grid_xcells;
    int grid_ycells;
    int prev_color_theme;
    
    WINDOWPTR c[ 3 * 4 ];
    bool user_theme;
    bool user_theme_changed;
    
    WINDOWPTR ok;
    WINDOWPTR cancel;
};

static void colortheme_set_ctls( colortheme_data* data )
{
    window_manager* wm = data->win->wm;
    int cc = 0;
    for( int x = 0; x < 4; x++ )
    {
	COLOR col = colortheme_get_color( wm->color_theme, x, wm );
        if( data->user_theme )
        {
    	    char ckey[ 4 ];
	    ckey[ 0 ] = 'c';
	    ckey[ 1 ] = '_';
	    ckey[ 2 ] = '0' + x;
	    ckey[ 3 ] = 0;
	    char* tv; tv = sconfig_get_str_value( (const char*)ckey, 0, 0 ); if( tv ) col = get_color_from_string( tv );
	}
	for( int y = 0; y < 3; y++ )
	{
    	    int v = 0;
	    switch( y )
	    {
	        case 0: v = red( col ); break;
	        case 1: v = green( col ); break;
	        case 2: v = blue( col ); break;
		default: break;
	    }			
	    WINDOWPTR w = data->c[ cc ];
	    scrollbar_set_value( w, v, wm );
    	    cc++;
	}
    }
}

static void colortheme_save_ctls( colortheme_data* data )
{
    window_manager* wm = data->win->wm;
    if( !data->user_theme ) return;
    int cc = 0;
    for( int x = 0; x < 4; x++ )
    {
        char ckey[ 4 ];
        char col_str[ 32 ];
        ckey[ 0 ] = 'c';
        ckey[ 1 ] = '_';
        ckey[ 2 ] = '0' + x;
        ckey[ 3 ] = 0;
        
        WINDOWPTR w = data->c[ cc ];
        int rr = scrollbar_get_value( data->c[ cc ], wm );
        int gg = scrollbar_get_value( data->c[ cc + 1 ], wm );
        int bb = scrollbar_get_value( data->c[ cc + 2 ], wm );
        COLOR col = get_color( rr, gg, bb );
	get_string_from_color( col_str, sizeof( col_str ), col );
        
        sconfig_set_str_value( (const char*)ckey, col_str, 0 ); 
	
	cc += 3;
    }
}

static int colortheme_ok_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    colortheme_data* data = (colortheme_data*)user_data;

    sconfig_set_int_value( APP_CFG_COLOR_THEME, wm->color_theme, 0 );
    if( data->user_theme )
    {
	sconfig_set_int_value( APP_CFG_CUSTOM_COLORS, 1, 0 );
	colortheme_save_ctls( data );
    }
    else
    {
	sconfig_remove_key( APP_CFG_CUSTOM_COLORS, 0 );
    }
    sconfig_save( 0 );

    bool changed = false;
    if( wm->color_theme != data->prev_color_theme || data->user_theme_changed ) changed = true;
    
    remove_window( wm->colortheme_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    if( !changed ) return 0;

    if( dialog( 0, wm_get_string( STR_WM_COLOR_THEME_MSG_RESTART ), wm_get_string( STR_WM_YESNO ), wm ) == 0 )
    {
	wm->exit_request = 1;
	wm->restart_request = 1;
	g_color_theme_changed = true;
    }
    else 
    {
    }
    
    return 0;
}

static int colortheme_cancel_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int prev_color_theme = 0;
    {
	colortheme_data* data = (colortheme_data*)user_data;
	prev_color_theme = data->prev_color_theme;
    }
    remove_window( wm->colortheme_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    wm->color_theme = prev_color_theme;
    return 0;
}

static int colortheme_ctl_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    colortheme_data* data = (colortheme_data*)user_data;
    data->user_theme = true;
    data->user_theme_changed = true;
    draw_window( data->win, wm );
    return 0;
}

#define COLOR_RECT_PERC 5
#define CTL_YSIZE ( wm->controller_ysize + wm->interelement_space2 )
#define CTL_YSIZE_NORM wm->controller_ysize

int colortheme_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    colortheme_data* data = (colortheme_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;    
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( colortheme_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		g_color_theme_changed = false;
		data->prev_color_theme = wm->color_theme;
		data->grid_xcells = 1;
		data->grid_ycells = 1;
		/*int i = 0;
		while( data->grid_xcells * data->grid_ycells < (int)sizeof( g_color_themes_remap ) )
		{
		    if( i & 1 ) 
		    {
			data->grid_ycells++;
		    }
		    else 
		    {
			
			data->grid_xcells++;
		    }
		    i++;
		}*/
		data->grid_xcells = 6;
		data->grid_ycells = 4;
		
		int cc = 0;
		for( int x = 0; x < 4; x++ )
		{
		    for( int y = 0; y < 3; y++ )
		    {
			const char* name = "";
			switch( y )
			{
			    case 0: name = wm_get_string( STR_WM_RED ); break;
			    case 1: name = wm_get_string( STR_WM_GREEN ); break;
			    case 2: name = wm_get_string( STR_WM_BLUE ); break;
			    default: break;
			}
			wm->opt_scrollbar_compact = true;
            		WINDOWPTR w = new_window( "Theme color ctl", 0, 0, 1, 1, wm->scroll_color, win, scrollbar_handler, wm );
	                scrollbar_set_name( w, name, wm );
            		scrollbar_set_parameters( w, 0, 255, 1, 1, wm );
	                scrollbar_set_normal_value( w, 255, wm );
            		set_handler( w, colortheme_ctl_handler, data, wm );
            		int yy = ( 3 - y ) * CTL_YSIZE + wm->button_ysize + wm->interelement_space * 2 - wm->interelement_space2;
            		set_window_controller( w, 0, wm, CPERC, (WCMD)x * 25 + COLOR_RECT_PERC, CMINVAL, (WCMD)wm->interelement_space, CEND );
        	        set_window_controller( w, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)yy, CEND );
	                set_window_controller( w, 2, wm, CPERC, (WCMD)x * 25 + 25, CBACKVAL1, CMAXVAL, (WCMD)wm->interelement_space, CEND );
	                set_window_controller( w, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)yy - CTL_YSIZE_NORM , CEND );
	                data->c[ cc ] = w;
			cc++;
		    }
		}
		
		data->user_theme = false;
		data->user_theme_changed = false;
	        if( sconfig_get_int_value( APP_CFG_CUSTOM_COLORS, -1, 0 ) != -1 ) data->user_theme = true;
		colortheme_set_ctls( data );
		
		data->ok = new_window( wm_get_string( STR_WM_OK ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
		data->cancel = new_window( wm_get_string( STR_WM_CANCEL ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
		set_handler( data->ok, colortheme_ok_handler, data, wm );
		set_handler( data->cancel, colortheme_cancel_handler, data, wm );
		int x = wm->interelement_space;
		set_window_controller( data->ok, 0, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		x += wm->button_xsize;
		set_window_controller( data->ok, 2, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
		x += wm->interelement_space2;
		set_window_controller( data->cancel, 0, wm, (WCMD)x, CEND );
		set_window_controller( data->cancel, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		x += wm->button_xsize;
		set_window_controller( data->cancel, 2, wm, (WCMD)x, CEND );
		set_window_controller( data->cancel, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
		
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		
		int t = 0;
		float grid_xsize = (float)( win->xsize - wm->interelement_space * 2 ) / data->grid_xcells;
		float grid_ysize = (float)( data->c[ 0 ]->y - wm->interelement_space * 2 ) / data->grid_ycells;
		for( int grid_y = 0; grid_y < data->grid_ycells; grid_y++ )
		{
		    for( int grid_x = 0; grid_x < data->grid_xcells; grid_x++ )
		    {
			int t2 = g_color_themes_remap[ t ];
			
			float fx = (float)wm->interelement_space + (float)grid_x * grid_xsize;
			float fy = (float)wm->interelement_space + (float)grid_y * grid_ysize;
			int x = fx;
			int y = fy;
			int xsize = (int)( ( fx + grid_xsize ) - (float)x );
			int ysize = (int)( ( fy + grid_ysize ) - (float)y );
			xsize--;
			ysize--;
			if( grid_x == data->grid_xcells - 1 ) xsize = win->xsize - wm->interelement_space - x;
			if( grid_y == data->grid_ycells - 1 ) ysize = data->c[ 0 ]->y - wm->interelement_space - y;
			
			COLOR cc[ 4 ];
			for( int c = 0; c < 4; c++ )
			{
			    cc[ c ] = colortheme_get_color( t2, c, wm );
			}
			
			int xsize2 = xsize / 4;
			draw_frect( x, y, xsize2, ysize, cc[ 0 ], wm );
			draw_frect( x + xsize2, y, xsize2, ysize, cc[ 1 ], wm );
			draw_frect( x + xsize2 * 2, y, xsize2, ysize, cc[ 2 ], wm );
			draw_frect( x + xsize2 * 3, y, xsize - xsize2 * 3, ysize, cc[ 3 ], wm );
			
			//Show info:
			//char ts[ 32 ]; sprintf( ts, "%d", t2 ); wm->cur_font_color = get_color( 255, 128, 0 ); draw_string( ts, x, y, wm );
						
			draw_vgradient( x, y, xsize, ysize / 2, wm->color3, 100, 0, wm );
			draw_vgradient( x, y + ysize / 2, xsize, ysize - ysize / 2, win->color, 0, 100, wm );
			
			wm->cur_opacity = 128;
			draw_rect( x, y, xsize - 1, ysize - 1, win->color, wm );
			wm->cur_opacity = 255;
			
			if( t2 == wm->color_theme )
			{
			    wm->cur_opacity = 128;
			    draw_corners( x + 1 + wm->corners_size, y + 1 + wm->corners_size, xsize - 2 - wm->corners_size * 2, ysize - 2 - wm->corners_size * 2, wm->corners_size, wm->corners_len, wm->color0, wm );
			    wm->cur_opacity = 255;
			    draw_corners( x + wm->corners_size, y + wm->corners_size, xsize - wm->corners_size * 2, ysize - wm->corners_size * 2, wm->corners_size, wm->corners_len, wm->color3, wm );
			}
			
			t++;
		    }
		}

		int cc = 0;
		for( int x = 0; x < 4; x++ )
		{
		    WINDOWPTR w = data->c[ cc ];
		    int rr = scrollbar_get_value( data->c[ cc ], wm );
		    int gg = scrollbar_get_value( data->c[ cc + 1 ], wm );
		    int bb = scrollbar_get_value( data->c[ cc + 2 ], wm );
		    COLOR col = get_color( rr, gg, bb );
		    int xsize;
		    int xx;
		    if( x == 0 )
		    {
			xx = wm->interelement_space;
		    }
		    else
		    {
			WINDOWPTR w2 = data->c[ cc - 3 ];
			xx = w2->x + w2->xsize + wm->interelement_space;
		    }
		    xsize = w->x - xx;
		    draw_frect( xx, w->y, xsize, CTL_YSIZE * 3 - wm->interelement_space2, col, wm );
		    cc += 3;
		}
		
		wbd_draw( wm );
		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    {
		if( evt->key == KEY_ENTER )
		{
		    colortheme_ok_handler( data, data->ok, wm );
		    retval = 1;
		}
		if( evt->key == KEY_ESCAPE )
        	{
            	    send_event( win, EVT_CLOSEREQUEST, wm );
            	    retval = 1;
        	}
		int t = wm->color_theme;
		int t2 = 0; for( ; t2 < (int)sizeof( g_color_themes_remap ); t2++ ) if( g_color_themes_remap[ t2 ] == t ) break;
		int prev_t2 = t2;
		if( evt->key == KEY_LEFT || evt->key == KEY_UP )
		{
		    if( evt->key == KEY_LEFT ) t2--;
		    if( evt->key == KEY_UP ) t2 -= data->grid_xcells;
		    retval = 1;
		}
		if( evt->key == KEY_RIGHT || evt->key == KEY_DOWN )
		{
		    if( evt->key == KEY_RIGHT ) t2++;
		    if( evt->key == KEY_DOWN ) t2 += data->grid_xcells;
		    retval = 1;
		}
		if( t2 != prev_t2 && t2 >= 0 && t2 < (int)sizeof( g_color_themes_remap ) )
		{
		    if( data->user_theme )
		    {
			data->user_theme = false;
			data->user_theme_changed = true;
		    }
		    t = g_color_themes_remap[ t2 ];
		    wm->color_theme = t;
		    colortheme_set_ctls( data );
		    draw_window( win, wm );
		}
	    }
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( evt->key & MOUSE_BUTTON_LEFT )
	    {
		int t = 0;
		int win_ysize = win->ysize - CTL_YSIZE * 3;
		float grid_xsize = (float)( win->xsize - wm->interelement_space * 2 ) / data->grid_xcells;
		float grid_ysize = (float)( data->c[ 0 ]->y - wm->interelement_space * 2 ) / data->grid_ycells;
		for( int grid_y = 0; grid_y < data->grid_ycells; grid_y++ )
		{
		    for( int grid_x = 0; grid_x < data->grid_xcells; grid_x++ )
		    {
			int t2 = g_color_themes_remap[ t ];
			
			float fx = (float)wm->interelement_space + (float)grid_x * grid_xsize;
			float fy = (float)wm->interelement_space + (float)grid_y * grid_ysize;
			int x = fx;
			int y = fy;
			int xsize = (int)grid_xsize;
			int ysize = (int)grid_ysize;
			if( rx >= x && ry >= y && rx < x + xsize && ry < y + ysize )
			{
			    if( data->user_theme )
			    {
				data->user_theme = false;
				data->user_theme_changed = true;
			    }
			    wm->color_theme = t2;
			    colortheme_set_ctls( data );
			    draw_window( win, wm );
			    break;
			}
			
			t++;
		    }
		}
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_CLOSEREQUEST:
            {
        	colortheme_cancel_handler( data, NULL, wm );
            }
            retval = 1;
            break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    wm->colortheme_win = 0;
	    break;
    }
    return retval;
}

COLOR colortheme_get_color( int theme, int item, window_manager* wm )
{
    if( theme < 0 || theme >= (int)sizeof( g_color_themes ) / ( 3 * 4 ) ||
	item < 0 || item > 3 )
    {
	return 0;
    }
    uint8_t rr = g_color_themes[ theme * 3 * 4 + item * 3 + 0 ];
    uint8_t gg = g_color_themes[ theme * 3 * 4 + item * 3 + 1 ];
    uint8_t bb = g_color_themes[ theme * 3 * 4 + item * 3 + 2 ];
    return get_color( rr, gg, bb );
}
