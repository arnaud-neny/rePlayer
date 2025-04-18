/*
    wm_hnd.cpp - standard window handlers
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#define SIDEBTN 235 //Width of the side button. 1.0 = 256

int null_handler( sundog_event* evt, window_manager* wm )
{
    return 0;
}

int desktop_handler( sundog_event* evt, window_manager* wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    switch( evt->type )
    {
	case EVT_AFTERCREATE:
	case EVT_SCREENRESIZE:
	    win->x = 0;
	    win->y = 0;
	    win->xsize = wm->screen_xsize;
	    win->ysize = wm->screen_ysize;
#ifdef SCREEN_SAFE_AREA_SUPPORTED
	    if( wm->flags & WIN_INIT_FLAG_SHRINK_DESKTOP_TO_SAFE_AREA )
	    {
		if( wm->screen_safe_area.w && wm->screen_safe_area.h )
		{
		    win->x = wm->screen_safe_area.x;
		    win->y = wm->screen_safe_area.y;
		    win->xsize = wm->screen_safe_area.w;
		    win->ysize = wm->screen_safe_area.h;
		    win->flags |= WIN_FLAG_ALWAYS_HANDLE_DRAW_EVT;
		}
	    }
#endif
	    wm->desktop_xsize = win->xsize;
	    wm->desktop_ysize = win->ysize;
	    if( evt->type != EVT_SCREENRESIZE ) retval = 1;
	    break;
	case EVT_DRAW:
#ifdef SCREEN_SAFE_AREA_SUPPORTED
	    if( win->xsize != wm->screen_xsize || win->ysize != wm->screen_ysize )
	    {
		COLOR c = win->color;
		if( win->x > 0 ) wm->device_draw_frect( 0, 0, win->x, wm->screen_ysize, c, wm );
		if( win->x + win->xsize < wm->screen_xsize ) wm->device_draw_frect( win->x + win->xsize, 0, wm->screen_xsize - ( win->x + win->xsize ), wm->screen_ysize, c, wm );
		if( win->y > 0 ) wm->device_draw_frect( 0, 0, wm->screen_xsize, win->y, c, wm );
		if( win->y + win->ysize < wm->screen_ysize ) wm->device_draw_frect( 0, win->y + win->ysize, wm->screen_xsize, wm->screen_ysize - ( win->y + win->ysize ), c, wm );
		wm->screen_changed++;
	    }
#endif
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

//
//
//

#define DIVIDER_BINDS 2

struct divider_data
{
    WINDOWPTR win;
    
    int start_x;
    int start_y;
    int start_wx;
    int start_wy;
    int start_scroll;
    int pressed; //0 - nothing; 1 - whole divider; 2 - scrollbar button;
    COLOR color;
    int scroll_min;
    int scroll_max;
    int scroll_cur;
    int slider_size;
    WINDOWPTR binds[ DIVIDER_BINDS ]; //binded windows, that work in conjunction with the current divider
    int binds_x[ DIVIDER_BINDS ];
    int binds_y[ DIVIDER_BINDS ];
    WINDOWPTR push_win; //window that may be pushed by the divider and its binded windows;
    bool push_right : 1;
    bool push_down : 1;
    
    bool vert : 1;
    uint32_t flags;
    
    WINDOWPTR prev_focus_win;
};

void divider_push_start( divider_data* data, WINDOWPTR win )
{
    if( data->push_win == 0 ) return;
    if( data->flags & DIVIDER_FLAG_PUSHING_XAXIS )
    {
        if( data->push_win->x >= win->x )
    	    data->push_right = true;
	else
	    data->push_right = false;
    }
    if( data->flags & DIVIDER_FLAG_PUSHING_YAXIS )
    {
	if( data->push_win->y >= win->y )
	    data->push_down = true;
	else
	    data->push_down = false;
    }
}

void divider_push( divider_data* data, WINDOWPTR win )
{
    if( data->push_win == 0 ) return;
    if( data->flags & DIVIDER_FLAG_PUSHING_XAXIS )
    {
        if( data->push_right )
        {
    	    if( win->x + win->xsize > data->push_win->x )
	    {
	        //Push right:
	        data->push_win->x = win->x + win->xsize;
	    }
	}
	else
	{
	    if( win->x < data->push_win->x + data->push_win->xsize )
	    {
		//Push left:
		data->push_win->x = win->x - data->push_win->xsize;
	    }
	}
    }
    if( data->flags & DIVIDER_FLAG_PUSHING_YAXIS )
    {
        if( data->push_down )
        {
    	    if( win->y + win->ysize > data->push_win->y )
	    {
	        //Push down:
	        data->push_win->y = win->y + win->ysize;
	    }
	}
	else
	{
	    if( win->y < data->push_win->y + data->push_win->ysize )
	    {
		//Push up:
		data->push_win->y = win->y - data->push_win->ysize;
	    }
	}
    }
}

static int get_div_slider_val( divider_data* data, int osize )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    if( data->scroll_max == data->scroll_min ) return 0;
    int size = osize - data->slider_size;
    int y = ( data->scroll_cur - data->scroll_min ) * size / ( data->scroll_max - data->scroll_min );
    return y;
}

int divider_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    divider_data* data = (divider_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( divider_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    data->pressed = 0;
	    data->color = win->color;
	    data->scroll_min = wm->opt_divider_scroll_min;
	    data->scroll_max = wm->opt_divider_scroll_max;
	    data->slider_size = wm->scrollbar_size;
	    data->vert = wm->opt_divider_vertical;
	    data->flags = DIVIDER_FLAG_PUSHING_YAXIS;
	    data->scroll_cur = 0;
	    {
		for( int a = 0; a < DIVIDER_BINDS;a++ )
		    data->binds[ a ] = 0;
	    }
	    data->push_win = NULL;
	    data->prev_focus_win = NULL;
	    wm->opt_divider_scroll_min = 0;
	    wm->opt_divider_scroll_max = 0;
	    wm->opt_divider_vertical = false;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		int rx = evt->x - win->screen_x;
	        int ry = evt->y - win->screen_y;
		data->start_x = evt->x;
		data->start_y = evt->y;
		if( data->flags & DIVIDER_FLAG_DYNAMIC_XSIZE )
		{
		    data->start_wx = win->xsize;
		}
		else
		{
		    data->start_wx = win->x;
		}
		if( data->flags & DIVIDER_FLAG_DYNAMIC_YSIZE )
		{
		    data->start_wy = win->ysize;
		}
		else
		{
		    data->start_wy = win->y;
		}
		data->start_scroll = data->scroll_cur;
		for( int a = 0; a < DIVIDER_BINDS; a++ )
		{
		    if( data->binds[ a ] )
		    {
			data->binds_x[ a ] = data->binds[ a ]->x;
			data->binds_y[ a ] = data->binds[ a ]->y;
			divider_push_start( (divider_data*)data->binds[ a ]->data, data->binds[ a ] );
		    }
		}
		divider_push_start( data, win );
		data->pressed = 1;
		if( data->scroll_min != data->scroll_max && ( data->flags & DIVIDER_FLAG_WITH_SCROLL_BUTTON ) )
		{
		    int osize = win->ysize;
		    if( !data->vert ) osize = win->xsize;

		    if( osize >= data->slider_size * 2 )
		    {
			if( data->vert )
			{
			    int y = get_div_slider_val( data, win->ysize );
			    if( ry >= y && ry < y + data->slider_size )
			    {
				data->pressed = 2;
			    }
			}
			else
			{
			}
		    }
		}
		win->color = PUSHED_COLOR( data->color );
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		int dx = evt->x - data->start_x;
		int dy = evt->y - data->start_y;
		int dx2 = dx;
		int dy2 = dy;
		bool scroll_possible = false;
		int move_possible = 1 | 2; //1 - horizontal; 2 - vertical;

		if( data->pressed == 2 )
		{
		    //Slider:
		    int osize = win->ysize;
		    if( !data->vert ) osize = win->xsize;
		    int size = osize - data->slider_size;
		    if( size )
		    {
			int l = data->scroll_max - data->scroll_min;
			if( data->vert )
			{
			    dy2 = dy * l / size;
			    move_possible = 1;
			}
			else
			{
			    dx2 = dx * l / size;
			    move_possible = 2;
			}
			if( dx2 || dy2 )
			{
			    scroll_possible = true;
			}
		    }
		}
		else
		{
		    //Whole divider:
		    if( ( data->flags & DIVIDER_FLAG_WITH_SCROLL_BUTTON ) == 0 )
		        scroll_possible = true;
		}

		if( move_possible )
		{
		    if( move_possible & 1 )
		    {
			if( data->flags & DIVIDER_FLAG_DYNAMIC_XSIZE )
			{
			    win->xsize = data->start_wx + dx;
			}
			else
			{
			    win->x = data->start_wx + dx;
			}
		    }
		    if( move_possible & 2 )
		    {
			if( data->flags & DIVIDER_FLAG_DYNAMIC_YSIZE )
			{
			    win->ysize = data->start_wy + dy;
			}
			else
			{
			    win->y = data->start_wy + dy;
			}
		    }
		    for( int a = 0; a < DIVIDER_BINDS; a++ )
		    {
			if( data->binds[ a ] )
			{
			    if( move_possible & 1 )
				data->binds[ a ]->x = data->binds_x[ a ] + dx;
			    if( move_possible & 2 )
				data->binds[ a ]->y = data->binds_y[ a ] + dy;
			    divider_push( (divider_data*)data->binds[ a ]->data, data->binds[ a ] );
			}
		    }
		    divider_push( data, win );
		}

		if( win->action_handler && scroll_possible )
		{
		    int prev_val = data->scroll_cur;
		    if( data->vert )
		    {
			data->scroll_cur = data->start_scroll + dy2;
		    }
		    else
		    {
			data->scroll_cur = data->start_scroll + dx2;
		    }
		    if( data->scroll_cur < data->scroll_min )
		        data->scroll_cur = data->scroll_min;
		    if( data->scroll_cur > data->scroll_max )
		        data->scroll_cur = data->scroll_max;
		    if( prev_val != data->scroll_cur )
			win->action_handler( win->handler_data, win, wm );
		}

		if( move_possible )
		{
		    recalc_regions( wm );
		    draw_window( wm->root_win, wm );
		}
		else
		{
		    draw_window( win, wm );
		}

		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT && data->pressed )
	    {
		data->pressed = 0;
		set_focus_win( data->prev_focus_win, wm );
		win->color = data->color;
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    if( data->pressed )
	    {
		data->pressed = 0;
		win->color = data->color;
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_DRAW:
	    wbd_lock( win );
	    {
		COLOR color = win->color;
		draw_frect( 0, 0, win->xsize, win->ysize, color, wm );

		int cx = win->xsize / 2;
		int cy = win->ysize / 2;
		int s1 = wm->interelement_space;
		int s2 = wm->interelement_space2;
		draw_frect( cx - s1, cy - s1, s1, s1, BORDER_COLOR( color ), wm );
		draw_frect( cx + s2, cy - s1, s1, s1, BORDER_COLOR( color ), wm );
		draw_frect( cx - s1, cy + s2, s1, s1, BORDER_COLOR( color ), wm );
		draw_frect( cx + s2, cy + s2, s1, s1, BORDER_COLOR( color ), wm );
		int xx = 0;
		int yy = 0;
		if( data->vert ) yy = 1; else xx = 1;
		draw_rect( -xx, -yy, win->xsize + xx * 2 - 1, win->ysize + yy * 2 - 1, BORDER_COLOR( color ), wm );

		if( data->scroll_min != data->scroll_max )
		{
		    if( data->pressed || ( data->flags & DIVIDER_FLAG_WITH_SCROLL_BUTTON ) )
		    {
			int osize = win->ysize;
			if( !data->vert ) osize = win->xsize;

			data->slider_size = osize / 4;
			if( data->slider_size < wm->scrollbar_size )
			    data->slider_size = wm->scrollbar_size;

			if( osize >= data->slider_size * 2 )
			{
			    if( data->vert )
			    {
				COLOR c = wm->color2;
				int y = get_div_slider_val( data, osize );
				if( data->flags & DIVIDER_FLAG_WITH_SCROLL_BUTTON )
				{
				    if( data->pressed == 2 )
				    {
				        c = wm->color3;
					wm->cur_opacity = 32;
					draw_frect( 1, y, win->xsize - 2, data->slider_size, wm->color3, wm );
					wm->cur_opacity = 255;
				    }
				    draw_updown( win->xsize / 2, y + data->slider_size / 2, c, 0, wm );
				}
				else
				{
				    draw_updown( win->xsize / 2, win->ysize / 2, c, 0, wm );
				}
			    }
			    else
			    {
			    }
			}
		    }
		}
	    }
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void bind_divider_to( WINDOWPTR win, WINDOWPTR bind_to, int bind_num, window_manager* wm )
{
    divider_data* data = (divider_data*)win->data;
    data->binds[ bind_num ] = bind_to;
}

void set_divider_push( WINDOWPTR win, WINDOWPTR push_win, window_manager* wm )
{
    divider_data* data = (divider_data*)win->data;
    data->push_win = push_win;
}

void set_divider_scroll_parameters( WINDOWPTR win, int min, int max, window_manager* wm )
{
    divider_data* data = (divider_data*)win->data;
    data->scroll_min = min;
    data->scroll_max = max;
}

int get_divider_scroll_value( WINDOWPTR win )
{
    divider_data* data = (divider_data*)win->data;
    return data->scroll_cur;
}

void set_divider_scroll_value( WINDOWPTR win, int val )
{
    divider_data* data = (divider_data*)win->data;
    data->scroll_cur = val;
    if( data->scroll_cur < data->scroll_min )
	data->scroll_cur = data->scroll_min;
    if( data->scroll_cur > data->scroll_max )
	data->scroll_cur = data->scroll_max;
}

void divider_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( win )
    {
	divider_data* data = (divider_data*)win->data;
	if( data )
	{
	    data->flags = flags;
	}
    }
}

uint32_t divider_get_flags( WINDOWPTR win )
{
    if( win )
    {
	divider_data* data = (divider_data*)win->data;
	if( data )
	{
	    return data->flags;
	}
    }
    return 0;
}

//
//
//

struct label_data 
{
    WINDOWPTR prev_focus_win;
    uint32_t flags;
};

int label_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    label_data* data = (label_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( label_data );
	    break;
	case EVT_AFTERCREATE:
	    data->prev_focus_win = 0;
	    data->flags = 0;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    set_focus_win( data->prev_focus_win, wm );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    wbd_lock( win );
	    draw_frect( 0, 0, win->xsize, win->ysize, win->parent->color, wm );
	    //Draw name:
	    {
		const char* str = win->name;
		int tysize = string_y_size( str, wm );
		int ty = 0;
		while( 1 )
		{
		    if( data->flags & LABEL_FLAG_ALIGN_TOP ) { ty = 0; break; }
		    if( data->flags & LABEL_FLAG_ALIGN_BOTTOM ) { ty = win->ysize - tysize; break; }
		    ty = ( win->ysize - tysize ) / 2;
		    break;
		}
		if( data->flags & LABEL_FLAG_WINCOLOR_IS_TEXTCOLOR )
		    wm->cur_font_color = win->color;
		else
		    wm->cur_font_color = blend( wm->color2, win->parent->color, LABEL_OPACITY );
		if( data->flags & LABEL_FLAG_WORDWRAP )
		    draw_string_wordwrap( str, 0, ty, win->xsize, NULL, NULL, false, wm );
		else
		    draw_string( str, 0, ty, wm );
	    }
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void label_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( win )
    {
	label_data* data = (label_data*)win->data;
	if( data )
	{
	    data->flags = flags;
	}
    }
}

uint32_t label_get_flags( WINDOWPTR win )
{
    if( win )
    {
	label_data* data = (label_data*)win->data;
	if( data )
	{
	    return data->flags;
	}
    }
    return 0;
}

//
//
//

struct text_data 
{
    WINDOWPTR win;
    WINDOWPTR btn_clear;
    WINDOWPTR btn_inc;
    WINDOWPTR btn_dec;
    uint32_t flags;
    uint32_t* text;
    char* label;
    char* output_str;
    int str_xsize;
    int largest_char_xsize;
    int right_buttons_xsize;
    stime_ticks_t prev_autoscroll_time;
    int cur_pos;
    int zoom;
    int numeric;	//0 - text only;
			//1 - decimal integer;
			//2 - hexadecimal integer;
			//3 - float;
			//4 - time in seconds in the following format: HOURS:MINUTES:SECONDS.*
    int min;
    int max;
    int step;
    int xoffset;
    int last_action; //TEXT_ACTION_xxxx
    COLOR text_color;
    bool hide_zero : 1;
    bool ro : 1;
    bool editing : 1; //in focus and some data changed
    bool recalc_str_size : 1;
    bool make_cursor_visible : 1;
    bool active : 1;
    bool can_show_clear_btn : 1;
    WINDOWPTR prev_focus_win;
    uint32_t show_char;
};

int text_dec_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    text_data* data = (text_data*)user_data;
    if( data->numeric < 3 )
    {
	//int:
	int val = text_get_value( data->win, wm );
	val -= data->step;
	text_set_value( data->win, val, wm );
    }
    else
    {
	//float:
	double val = text_get_fvalue( data->win );
	val -= data->step;
	text_set_fvalue( data->win, val );
    }
    draw_window( data->win, wm );
    text_changed( data->win );
    return 0;
}

int text_inc_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    text_data* data = (text_data*)user_data;
    if( data->numeric < 3 )
    {
	//int:
	int val = text_get_value( data->win, wm );
	val += data->step;
	text_set_value( data->win, val, wm );
    }
    else
    {
	//float:
	double val = text_get_fvalue( data->win );
	val += data->step;
	text_set_fvalue( data->win, val );
    }
    draw_window( data->win, wm );
    text_changed( data->win );
    return 0;
}

int text_clear_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    text_data* data = (text_data*)user_data;
    text_set_text( data->win, "", wm );
    draw_window( data->win, wm );
    if( data->flags & TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES ) text_changed( data->win );
    return 0;
}

static bool text_reinit_clear_button( text_data* data )
{
    if( !data->btn_clear ) return false;
    bool changed = false;
    if( !data->ro && data->can_show_clear_btn && data->text && data->text[ 0 ] != 0 )
    {
        changed = show_window2( data->btn_clear );
    }
    else
    {
	changed = hide_window2( data->btn_clear );
    }
    return changed;
}

static void text_change_prev_focus( WINDOWPTR win, WINDOWPTR prev_focus_win )
{
    if( !win ) return;
    if( win->win_handler != text_handler ) return;
    text_data* data = (text_data*)win->data;
    data->prev_focus_win = prev_focus_win;
}

int text_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    text_data* data = (text_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( text_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    data->text = (uint32_t*)smem_znew( 32 * sizeof( uint32_t ) );
	    data->zoom = 256;
	    data->numeric = wm->opt_text_numeric;
	    data->min = wm->opt_text_num_min;
	    data->max = wm->opt_text_num_max;
	    data->hide_zero = wm->opt_text_num_hide_zero;
	    data->ro = wm->opt_text_ro;
	    data->step = 1;
	    data->text_color = wm->color3;
	    if( data->numeric )
	    {
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		WINDOWPTR b = new_window( "-", 1, 1, 1, 1, win->color, win, button_handler, wm );
		set_window_controller( b, 1, wm, (WCMD)0, CEND );
	        set_window_controller( b, 3, wm, CPERC, (WCMD)100, CEND );
		set_handler( b, text_dec_handler, data, wm );
		button_set_text_opacity( b, 150 );
        	button_set_text_color( b, wm->color2 );
		data->btn_dec = b;
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		b = new_window( "+", 1, 1, 1, 1, win->color, win, button_handler, wm );
		set_window_controller( b, 1, wm, (WCMD)0, CEND );
	        set_window_controller( b, 3, wm, CPERC, (WCMD)100, CEND );
		set_handler( b, text_inc_handler, data, wm );
		button_set_text_opacity( b, 150 );
        	button_set_text_color( b, wm->color2 );
		data->btn_inc = b;
	    }
	    else
	    {
		wm->opt_button_flags = BUTTON_FLAG_FLAT;
		WINDOWPTR b = new_window( "x", 1, 1, 1, 1, win->color, win, button_handler, wm );
		b->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		set_window_controller( b, 1, wm, (WCMD)0, CEND );
	        set_window_controller( b, 3, wm, CPERC, (WCMD)100, CEND );
		set_handler( b, text_clear_handler, data, wm );
		data->btn_clear = b;
		button_set_text_opacity( b, 150 );
        	button_set_text_color( b, wm->color2 );
	    }
	    data->prev_focus_win = NULL;
	    win->flags |= WIN_FLAG_AFTERRESIZE_ENABLED;
	    wm->opt_text_ro = false;
	    wm->opt_text_numeric = 0;
	    wm->opt_text_num_min = 0;
	    wm->opt_text_num_max = 0;
	    wm->opt_text_num_hide_zero = false;
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    if( data->ro )
	    {
		retval = 1;
		break;
	    }
	    data->active = true;
	    data->can_show_clear_btn = true;
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    if( data->ro )
	    {
		retval = 1;
		break;
	    }
	    if( data->active )
	    {
		data->active = 0;
		data->editing = false;
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONDOWN:
	{
	    if( data->ro )
	    {
		retval = 1;
		break;
	    }
	    int rx = evt->x - win->screen_x;
	    if( data->numeric )
	    {
		if( evt->key & MOUSE_BUTTON_SCROLLUP ) text_inc_handler( data, win, wm );
        	if( evt->key & MOUSE_BUTTON_SCROLLDOWN ) text_dec_handler( data, win, wm );
    	    }
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		wbd_lock( win );
		wm->cur_font_scale = data->zoom;
		data->prev_focus_win = wm->prev_focus_win;
		data->active = true;
		data->can_show_clear_btn = true;

		int x = data->xoffset;

		for( size_t p = 0; p < smem_get_size( data->text ) / sizeof( uint32_t ); p++ )
		{
		    uint32_t c = data->text[ p ];
		    int charx = char_x_size( c, wm );
		    if( c == 0 ) charx = win->xsize;
		    if( rx >= x && rx < x + charx )
		    {
		        data->cur_pos = p;
		        break;
		    }
		    x += charx;
		    if( c == 0 ) break;
		}

		//Auto-scroll:
		int xmove = 0;
		int r = win->xsize - data->right_buttons_xsize - data->largest_char_xsize;
		if( rx > r )
		    xmove = -data->largest_char_xsize;
		if( rx < data->largest_char_xsize )
		    xmove = data->largest_char_xsize;
		if( xmove )
		{
		    stime_ticks_t cur_t = stime_ticks();
		    if( cur_t - data->prev_autoscroll_time > stime_ticks_per_second() / 20 || data->prev_autoscroll_time == 0 )
		    {
			data->xoffset += xmove;
			data->prev_autoscroll_time = cur_t;
			if( data->xoffset + data->str_xsize < r )
			    data->xoffset = r - data->str_xsize;
			if( data->xoffset > 0 ) data->xoffset = 0;
		    }
		}

		wm->cur_font_scale = 256;
		wbd_unlock( wm );
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	}
	case EVT_MOUSEBUTTONUP:
	    if( data->ro )
	    {
		retval = 1;
		break;
	    }
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( data->active ) 
		{
		    if( ( data->flags & TEXT_FLAG_NO_VIRTUAL_KBD ) == 0 )
		    {
			if( wm->show_virtual_keyboard )
			{
			    show_keyboard_for_text_window( win, NULL, wm );
			}
		    }
		}
	    }
	    retval = 1;
	    break;

	case EVT_BUTTONDOWN:
	{
	    if( !data->active ) break;
	    if( data->ro ) break;
	    if( evt->key >= KEY_UNKNOWN ) break;

	    int cmd = 0; //1 - cut; 2 - copy; 3 - paste;
	    if( evt->flags & EVT_FLAG_CTRL )
	    {
		if( evt->key == 'c' ) cmd = 2;
		if( evt->key == 'v' ) cmd = 3;
    	    }
	    if( evt->flags & EVT_FLAG_SHIFT )
	    {
		if( evt->key == KEY_DELETE ) cmd = 1;
		if( evt->key == KEY_INSERT ) cmd = 3;
	    }
	    if( cmd )
	    {
		if( cmd == 1 ) text_clipboard_cut( win );
		if( cmd == 2 ) text_clipboard_copy( win );
		if( cmd == 3 ) text_clipboard_paste( win );
		if( cmd == 1 || cmd == 3 )
		{
		    if( data->flags & TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES )
			text_changed( win );
		}
		retval = 1;
		break;
    	    }

	    WINDOWPTR w2 = NULL; //switch to...
	    if( evt->key == KEY_TAB )
	    {
		WINDOWPTR parent = win->parent;
		if( parent && parent->childs && parent->childs_num )
		{
		    if( evt->flags & EVT_FLAG_SHIFT )
		    {
		        //Back:
		        for( int i = 0; i < parent->childs_num; i++ )
		        {
		    	    WINDOWPTR w = parent->childs[ i ];
			    if( !w ) continue;
			    if( w->win_handler != win->win_handler ) continue;
			    if( text_is_readonly( w ) ) continue;
			    int dx = w->x - win->x;
			    int dy = w->y - win->y;
			    if( dy <= 0 )
			    {
			        if( !( dy == 0 && dx >= 0 ) )
			        {
			            if( w2 == 0 )
			            {
			    	        w2 = w;
				    }
				    else
			    	    {
				        if( w->y > w2->y || ( w->y == w2->y && w->x > w2->x ) )
				        {
				            w2 = w;
				        }
				    }
				}
			    }
			}
		    }
		    else
		    {
			//Forward:
			for( int i = 0; i < parent->childs_num; i++ )
			{
			    WINDOWPTR w = parent->childs[ i ];
			    if( !w ) continue;
			    if( w->win_handler != win->win_handler ) continue;
			    if( text_is_readonly( w ) ) continue;
			    int dx = w->x - win->x;
			    int dy = w->y - win->y;
			    if( dy >= 0 )
			    {
			        if( !( dy == 0 && dx <= 0 ) )
			        {
			            if( w2 == 0 )
			            {
			        	w2 = w;
				    }
				    else
				    {
				        if( w->y < w2->y || ( w->y == w2->y && w->x < w2->x ) )
				        {
				            w2 = w;
				        }
				    }
				}
			    }
			}
		    }
		}
		if( w2 == win ) w2 = NULL;
		if( !w2 )
		{
		    retval = 1;
		    break;
		}
	    }

	    if( evt->key == KEY_ENTER || evt->key == KEY_ESCAPE || w2 )
	    {
	        data->active = 0;
	        data->editing = false;
	        data->can_show_clear_btn = false;
	    	set_focus_win( data->prev_focus_win, wm );
	        draw_window( win, wm );
	        data->last_action = TEXT_ACTION_CHANGED;
	        bool act = false;
	        if( evt->key == KEY_ENTER )
	        {
	    	    data->last_action = TEXT_ACTION_ENTER;
		    act = true;
		}
		if( evt->key == KEY_ESCAPE && ( data->flags & TEXT_FLAG_CALL_HANDLER_ON_ESCAPE ) )
		{
		    data->last_action = TEXT_ACTION_ESCAPE;
		    act = true;
		}
		if( w2 )
		{
		    data->last_action = TEXT_ACTION_SWITCH;
		    act = true;
		}
		if( win->action_handler && act )
		{
		    win->action_handler( win->handler_data, win, wm );
		}
		if( w2 )
		{
	    	    text_change_prev_focus( w2, data->prev_focus_win );
		    set_focus_win( w2, wm );
		    draw_window( w2, wm );
		}
	        data->prev_focus_win = nullptr;
		retval = 1;
		break;
	    }

	    bool changed = false;

	    uint32_t c = 0;
	    if( evt->key >= 0x20 && evt->key <= 0x7E ) c = evt->key;
	    if( evt->flags & EVT_FLAG_SHIFT )
	    {
	        if( evt->key >= 'a' && evt->key <= 'z' ) 
		{
		    c = evt->key - 0x20;
		}
		else
		{
		    //ONLY FOR ENGLISH (USA) LAYOUT!
		    //Fix it in future updates!
		    //(add EVT_CHAR (with unicode symbol) instead of EVT_BUTTONDOWN/EVT_BUTTONUP?)
		    //See SDL_TEXTEDITING and SDL_TEXTINPUT...
		    switch( evt->key )
		    {
		        case '0': c = ')'; break;
		        case '1': c = '!'; break;
		        case '2': c = '@'; break;
		        case '3': c = '#'; break;
		        case '4': c = '$'; break;
		        case '5': c = '%'; break;
		        case '6': c = '^'; break;
		        case '7': c = '&'; break;
		        case '8': c = '*'; break;
		        case '9': c = '('; break;
		        case '[': c = '{'; break;
		        case ']': c = '}'; break;
		        case ';': c = ':'; break;
		        case  39: c = '"'; break;
		        case ',': c = '<'; break;
		        case '.': c = '>'; break;
		        case '/': c = '?'; break;
		        case '-': c = '_'; break;
		        case '=': c = '+'; break;
		        case  92: c = '|'; break;
		        case '`': c = '~'; break;
		    }
		}
	    }

    	    uint32_t* text_buf = data->text;
	    size_t text_size = smem_get_size( text_buf ) / sizeof( uint32_t );

	    if( c == 0 )
	    {
	        if( evt->key == KEY_BACKSPACE )
	        {
	    	    if( data->cur_pos >= 1 )
		    {
		        data->cur_pos--;
		        for( size_t i = data->cur_pos; i < text_size - 1; i++ ) 
		    	    text_buf[ i ] = text_buf[ i + 1 ];
		    }
		    changed = true;
		}
		else
		if( evt->key == KEY_DELETE )
		{
		    if( data->text[ data->cur_pos ] != 0 )
		        for( size_t i = data->cur_pos; i < text_size - 1; i++ ) 
		            text_buf[ i ] = text_buf[ i + 1 ];
		    changed = true;
		}
		else
		if( evt->key == KEY_LEFT )
		{
		    if( data->cur_pos >= 1 )
		    {
		        data->cur_pos--;
		        data->make_cursor_visible = true;
		    }
		}
		else
		if( evt->key == KEY_RIGHT )
		{
		    if( text_buf[ data->cur_pos ] != 0 )
		    {
		        data->cur_pos++;
		        data->make_cursor_visible = true;
		    }
		}
		else
		if( evt->key == KEY_END )
		{
		    size_t i = 0;
		    for( i = 0; i < text_size; i++ ) 
		        if( data->text[ i ] == 0 ) break;
		    data->cur_pos = i;
		    data->make_cursor_visible = true;
		}
		else
		if( evt->key == KEY_HOME )
		{
		    data->cur_pos = 0;
		    data->make_cursor_visible = true;
		}
		else
		{
		    retval = 0;
		    break;
		}
	    }
	    else
	    {
	        //Add new char:
		if( smem_strlen_utf32( (const uint32_t*)data->text ) + 16 > text_size )
		{
		    data->text = (uint32_t*)smem_resize2( data->text, ( text_size + 16 ) * sizeof( uint32_t ) );
		    text_size += 16;
		    text_buf = data->text;
		}
		for( size_t i = text_size - 1; i >= (unsigned)data->cur_pos + 1; i-- )
		    text_buf[ i ] = text_buf[ i - 1 ];
		text_buf[ data->cur_pos ] = c;
		data->cur_pos++;
		changed = true;
	    }

	    if( changed )
	    {
	        data->editing = true;
	        data->recalc_str_size = true;
	        data->make_cursor_visible = true;
	    }

	    draw_window( win, wm );

	    if( ( data->flags & TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES ) && changed && win->action_handler )
	    {
	        data->last_action = TEXT_ACTION_CHANGED;
	        win->action_handler( win->handler_data, win, wm );
	    }

	    retval = 1;
	    break;
	}

	case EVT_DRAW:
	{
	    if( text_reinit_clear_button( data ) ) recalc_regions( wm );

	    wbd_lock( win );
	    wm->cur_font_scale = data->zoom;

	    //Get content size:
	    if( data->recalc_str_size )
	    {
		data->recalc_str_size = false;
		data->str_xsize = 0;
		for( size_t p = 0; p < smem_get_size( data->text ) / sizeof( uint32_t ); p++ )
		{
		    uint32_t c = data->text[ p ];
		    if( c == 0 ) break;
		    data->str_xsize += char_x_size( c, wm );
		}
		data->largest_char_xsize = string_x_size( STR_LARGEST_CHAR, wm );
	    }

	    int r = win->xsize - data->right_buttons_xsize - data->largest_char_xsize;
	    if( data->str_xsize < r ) data->xoffset = 0; //We can show the whole string -> reset the xoffset

	    for( int a = 0; a < 2; a++ )
	    {
		//Fill window:
		wm->cur_opacity = 255;
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );

		//Get XY:
		int x = 0;
		int y = ( win->ysize - char_y_size( wm ) ) / 2;
		if( data->ro )
		{
		    if( x + data->str_xsize >= ( win->xsize - 1 ) )
		    {
			x -= data->str_xsize + x - ( win->xsize - 1 );
		    }
		}
		x += data->xoffset;

		//Draw text and cursor:
		COLOR text_color = data->text_color;
		if( data->label )
		{
		    y = win->ysize / 2;
		    text_color = wm->color2;
		}
		wm->cur_font_color = text_color;
		size_t p = 0;
		int cursor_x = 0;
		int cursor_xsize = 0;
		for( p = 0; p < smem_get_size( data->text ) / sizeof( uint32_t ); p++ )
		{
	    	    uint32_t c = data->text[ p ];
		    if( c == 0 ) c = ' ';
		    int charx = char_x_size( c, wm );
	    	    if( data->cur_pos == (int)p && data->active )
	    	    {
	    		cursor_x = x;
	    		cursor_xsize = charx;
	    		draw_frect( x, 0, charx, win->ysize, blend( text_color, win->color, 128 ), wm );
	    	    }
		    draw_char( c, x, y, wm );
	    	    x += charx;
	    	    if( data->text[ p ] == 0 ) break;
		}
		if( p == 0 && !data->active )
		{
	    	    draw_string( "...", 0, y, wm );
		}
		if( data->label )
		{
		    wm->cur_font_color = data->text_color;
		    y = win->ysize / 2 - char_y_size( wm );
		    draw_string( data->label, 0, y, wm );
		}

		if( data->make_cursor_visible )
		{
		    data->make_cursor_visible = false;
		    if( cursor_xsize )
		    {
			if( cursor_x < 0 ) data->xoffset += -cursor_x;
			if( cursor_x + cursor_xsize > r ) data->xoffset -= cursor_x + cursor_xsize - r;
			continue;
		    }
		}

		break;
	    }

	    if( data->show_char )
	    {
		//Show some symbol (defined in user request):
		wm->cur_opacity = 160;
		draw_frect( 0, 0, win->xsize, win->ysize, wm->color0, wm );
		wm->cur_opacity = 255;
		int len = char_x_size( data->show_char, wm );
		wm->cur_font_color = wm->color3;
		draw_char( 
		    data->show_char, 
		    ( win->xsize - len ) / 2, 
		    ( win->ysize - char_y_size( wm ) ) / 2,
		    wm );
	    }

	    wm->cur_font_scale = 256;
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
	}

	case EVT_BEFORECLOSE:
	    hide_keyboard_for_text_window( wm );
	    smem_free( data->output_str );
	    smem_free( data->text );
	    smem_free( data->label );
	    retval = 1;
	    break;

	case EVT_SHOWCHAR:
	    {
		data->show_char = evt->key;
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;

	case EVT_AFTERRESIZE:
	    {
		data->right_buttons_xsize = 0;
		int n = 0;
		int bxsize = win->ysize * SIDEBTN / 256;
		if( bxsize > wm->scrollbar_size ) bxsize = wm->scrollbar_size;
		if( data->btn_inc )
		{
		    n++;
		    data->btn_inc->x = win->xsize - bxsize * n;
		    data->btn_inc->xsize = bxsize;
		}
		if( data->btn_dec )
		{
		    n++;
		    data->btn_dec->x = win->xsize - bxsize * n;
		    data->btn_dec->xsize = bxsize;
		}
		if( data->btn_clear )
		{
		    n++;
		    data->btn_clear->x = win->xsize - bxsize * n;
		    data->btn_clear->xsize = bxsize;
		}
		data->right_buttons_xsize = n * bxsize;
		if( data->right_buttons_xsize >= win->xsize - wm->scrollbar_size )
		{
		    //Hide the buttons:
	    	    hide_window2( data->btn_inc );
	    	    hide_window2( data->btn_dec );
	    	    data->can_show_clear_btn = false;
		}
		else
		{
	    	    show_window2( data->btn_inc );
	    	    show_window2( data->btn_dec );
		}
		text_reinit_clear_button( data );
	    }
	    retval = 1;
	    break;

	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void text_changed( WINDOWPTR win )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    if( win->action_handler )
    {
        data->last_action = TEXT_ACTION_CHANGED;
	win->action_handler( win->handler_data, win, win->wm );
    }
}

void text_clipboard_cut( WINDOWPTR win )
{
    text_clipboard_copy( win );
    text_set_text( win, "", win->wm );
}

static const char* text_clipboard_filename = "3:/text_clipboard";
void text_clipboard_copy( WINDOWPTR win )
{
    if( !win ) return;
#ifdef CAN_COPYPASTE
    window_manager* wm = win->wm;
    char* txt = text_get_text( win, wm );
    if( !txt ) return;
    sfs_file f = sfs_open( text_clipboard_filename, "wb" );
    if( f )
    {
	sfs_write( txt, 1, strlen( txt ), f );
	sfs_close( f );
	sclipboard_copy( wm->sd, text_clipboard_filename, 0 );
    }
#endif
}

void text_clipboard_paste( WINDOWPTR win )
{
    if( !win ) return;
#ifdef CAN_COPYPASTE
    window_manager* wm = win->wm;
    text_data* data = (text_data*)win->data;
    char* fname = sclipboard_paste( wm->sd, sclipboard_type_utf8_text, 0 );
    if( fname )
    {
	size_t len = sfs_get_file_size( fname );
	if( len )
	{
	    sfs_file f = sfs_open( fname, "rb" );
	    if( f )
	    {
		char* txt = (char*)smem_new( len + 1 );
		sfs_read( txt, 1, len, f );
		txt[ len ] = 0;
		sfs_close( f );
		text_insert_text( win, txt, data->cur_pos );
		smem_free( txt );
	    }
	}
	smem_free( fname );
    }
#endif
}

void text_set_text( WINDOWPTR win, const char* text, window_manager* wm )
{
    if( !win || !text ) return;
    if( smem_strcmp( text, text_get_text( win, wm ) ) == 0 ) return;
    size_t len = smem_strlen( text ) + 1;
    uint32_t* ts = (uint32_t*)smem_new( len * sizeof( uint32_t ) );
    utf8_to_utf32( ts, len, text );
    size_t len2 = smem_strlen_utf32( ts ) + 1;
    text_data* data = (text_data*)win->data;
    if( smem_get_size( data->text ) / sizeof( uint32_t ) < len2 )
    {
        //Resize text buffer:
        data->text = (uint32_t*)smem_resize( data->text, len2 * sizeof( uint32_t ) );
    }
    smem_copy( data->text, ts, len2 * sizeof( uint32_t ) );
    if( data->cur_pos > (int)len2 - 1 )
        data->cur_pos = (int)len2 - 1;
    //if( text_reinit_clear_button( data ) ) recalc_regions( wm );
    data->can_show_clear_btn = false;
    data->recalc_str_size = true;
    draw_window( win, wm );
    smem_free( ts );
}

void text_insert_text( WINDOWPTR win, const char* text, int pos )
{
    if( !win || !text ) return;
    window_manager* wm = win->wm;
    text_data* data = (text_data*)win->data;
    if( !data ) return;
    size_t text_len = strlen( text );
    size_t src_len = smem_strlen_utf32( data->text );
    size_t src_size = smem_get_size( data->text ) / sizeof( uint32_t );
    if( pos < 0 || pos > (int)src_len ) return;
    uint32_t* text32 = (uint32_t*)smem_new( ( text_len + 1 ) * sizeof( uint32_t ) );
    if( text32 )
    {
	utf8_to_utf32( text32, text_len + 1, text );
	size_t text32_len = smem_strlen_utf32( text32 );
	if( text32_len )
	{
	    size_t final_len = src_len + text32_len;
    	    uint32_t* final = (uint32_t*)smem_new( ( final_len + 1 ) * sizeof( uint32_t ) );
    	    if( final )
    	    {
    		smem_copy( final, data->text, pos * sizeof( uint32_t ) );
    		smem_copy( final + pos, text32, text32_len * sizeof( uint32_t ) );
    		smem_copy( final + pos + text32_len, data->text + pos, ( src_len - pos ) * sizeof( uint32_t ) );
    		final[ final_len ] = 0;
    		smem_free( data->text );
    		data->text = final;
    		data->cur_pos = pos + text32_len;
		data->recalc_str_size = true;
		data->make_cursor_visible = true;
	        draw_window( win, wm );
	    }
	}
	smem_free( text32 );
    }
}

void text_set_label( WINDOWPTR win, const char* label )
{
    if( !win ) return;
    window_manager* wm = win->wm;
    text_data* data = (text_data*)win->data;
    if( !data ) return;
    smem_free( data->label );
    data->label = smem_strdup( label );
    if( data->label ) win->font = wm->font_small;
}

void text_set_cursor_position( WINDOWPTR win, int cur_pos, window_manager* wm )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    data->cur_pos = cur_pos;
    data->make_cursor_visible = true;
    draw_window( win, wm );
}

void text_set_value( WINDOWPTR win, int val, window_manager* wm )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    if( val < data->min ) val = data->min;
    if( val > data->max ) val = data->max;
    char ts[ 64 ];
    switch( data->numeric )
    {
	default: int_to_string( val, ts ); break;
	case 2: hex_int_to_string( val, ts ); break;
	case 4: time_to_str( ts, sizeof(ts), val, 1, 0 ); break;
    }
    if( data->hide_zero && val == 0 )
    {
        ts[ 0 ] = 0;
    }
    text_set_text( win, (const char*)ts, wm );
}

void text_set_fvalue( WINDOWPTR win, double val )
{
    if( !win ) return;
    window_manager* wm = win->wm;
    text_data* data = (text_data*)win->data;
    if( val < data->min ) val = data->min;
    if( val > data->max ) val = data->max;
    char ts[ 64 ];
    switch( data->numeric )
    {
	default: int_to_string( val, ts ); break;
	case 2: hex_int_to_string( val, ts ); break;
	case 3: snprintf( ts, sizeof(ts), "%f", val ); truncate_float_str( ts ); break;
	case 4: time_to_str( ts, sizeof(ts), val * 1000, 1000, 0 ); break;
    }
    if( data->hide_zero && val == 0 )
    {
        ts[ 0 ] = 0;
    }
    text_set_text( win, (const char*)ts, wm );
}

void text_set_value2( WINDOWPTR win, int val, window_manager* wm )
{
    if( text_get_editing_state( win ) == false )
        text_set_value( win, val, wm );
}

char* text_get_text( WINDOWPTR win, window_manager* wm )
{
    if( !win ) return NULL;
    text_data* data = (text_data*)win->data;
    size_t size = smem_get_size( data->text ) * 2;
    if( data->output_str ) 
    {
        if( smem_get_size( data->output_str ) < size )
        {
    	    data->output_str = (char*)smem_resize( data->output_str, size );
	}
    }
    else
    {
        data->output_str = (char*)smem_new( size );
    }
    utf32_to_utf8( data->output_str, size, data->text );
    return data->output_str;
}

int text_get_cursor_position( WINDOWPTR win, window_manager* wm )
{
    if( !win ) return 0;
    text_data* data = (text_data*)win->data;
    return data->cur_pos;
}

int text_get_value( WINDOWPTR win, window_manager* wm )
{
    if( !win ) return 0;
    int val = 0;
    text_data* data = (text_data*)win->data;
    char* s = text_get_text( win, wm );
    switch( data->numeric )
    {
	default: val = string_to_int( s ); break;
	case 2: val = hex_string_to_int( s ); break;
	case 3: val = atof( s ); break;
	case 4: val = str_to_time( s, 1 ); break;
    }
    if( val < data->min ) val = data->min;
    if( val > data->max ) val = data->max;
    return val;
}

double text_get_fvalue( WINDOWPTR win )
{
    if( !win ) return 0;
    window_manager* wm = win->wm;
    double val = 0;
    text_data* data = (text_data*)win->data;
    char* s = text_get_text( win, wm );
    switch( data->numeric )
    {
	default: val = string_to_int( s ); break;
	case 2: val = hex_string_to_int( s ); break;
	case 3: val = atof( s ); break;
	case 4: val = (double)str_to_time( s, 1000 ) / 1000; break;
    }
    if( val < data->min ) val = data->min;
    if( val > data->max ) val = data->max;
    return val;
}

int text_get_last_action( WINDOWPTR win )
{
    if( !win ) return 0;
    text_data* data = (text_data*)win->data;
    return data->last_action;
}

void text_set_zoom( WINDOWPTR win, int zoom, window_manager* wm )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    data->zoom = zoom;
    data->recalc_str_size = true;
}

void text_set_color( WINDOWPTR win, COLOR c )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    data->text_color = c;
}

void text_set_range( WINDOWPTR win, int min, int max )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    data->min = min;
    data->max = max;
    draw_window( win, win->wm );
}

void text_set_step( WINDOWPTR win, int step )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    data->step = step;
    draw_window( win, win->wm );
}

bool text_is_readonly( WINDOWPTR win )
{
    if( !win ) return 0;
    text_data* data = (text_data*)win->data;
    return data->ro;
}

bool text_get_editing_state( WINDOWPTR win )
{
    if( !win ) return 0;
    text_data* data = (text_data*)win->data;
    return data->editing;
}

void text_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( !win ) return;
    text_data* data = (text_data*)win->data;
    if( data ) data->flags = flags;
}

uint32_t text_get_flags( WINDOWPTR win )
{
    if( !win ) return 0;
    text_data* data = (text_data*)win->data;
    if( data )
	return data->flags;
    return 0;
}

//
//
//

//base = click time + autorepeat_delay
void button_autorepeat_accelerate( sundog_timer* timer, stime_ms_t base, window_manager* wm )
{
    stime_ms_t t = stime_ms() - base;
    if( t < 1000 * 2 )
    {
	timer->delay = 1000 / wm->mouse_autorepeat_freq;
    }
    else
    {
	float t2 = (float)t / 1000; //sec number after the base point
	float m = powf( 3.0f, t2 / 3.0f );
	float f = wm->mouse_autorepeat_freq * m; //Hz
	if( f > 1000 ) f = 1000;
	timer->delay = 1000 / f;
    }
}

struct button_data
{
    WINDOWPTR win;
    sdwm_image_scaled img1;
    sdwm_image_scaled img2;
    char* menu;
    int menu_items;
    int menu_val;
    int prev_menu_val;
    int add_radius;

    uint16_t flags;
    bool pushed : 1;
    bool pen_inside : 1;
    bool focused : 1;

    char* text;
    COLOR text_color;
    uint8_t text_opacity;
    char* val; //Text value (instead of menu+menu_val)

    uint evt_flags;

    win_action_handler2_t	begin_handler;
    void* 			begin_handler_data;
    win_action_handler2_t 	end_handler;
    void* 			end_handler_data;

    WINDOWPTR prev_focus_win;

    BUTTON_AUTOREPEAT_VARS;
};

void button_timer( void* vdata, sundog_timer* timer, window_manager* wm )
{
    //Autorepeat timer
    button_data* data = (button_data*)vdata;
    if( data->pushed && data->pen_inside )
    {
	if( data->win->action_handler )
	{
	    data->flags |= BUTTON_FLAG_HANDLER_CALLED_FROM_TIMER;
	    data->win->action_handler( data->win->handler_data, data->win, wm );
	    data->flags &= ~BUTTON_FLAG_HANDLER_CALLED_FROM_TIMER;
	}
    }
    BUTTON_AUTOREPEAT_ACCELERATE( data );
}

static bool is_button_win_valid( WINDOWPTR win )
{
    if( !win ) return false;
    if( !win->data ) return false;
    if( win->win_handler != button_handler ) return false;
    if( smem_get_size( win->data ) != sizeof( button_data ) ) return false;
    return true;
}

static int button_menu_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR btn_win = (WINDOWPTR)user_data;
    if( !is_button_win_valid( btn_win ) ) return 1; //close
    button_data* data = (button_data*)btn_win->data;
    btn_win->action_result = win->action_result;
    if( win->action_result >= 0 )
	data->menu_val = win->action_result;
    win_action_handler2_t end_handler = data->end_handler;
    void* end_handler_data = data->end_handler_data;
    if( btn_win->action_handler )
	btn_win->action_handler( btn_win->handler_data, btn_win, wm );
    if( end_handler )
	end_handler( end_handler_data, btn_win );
    return 1;
}

int button_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    button_data* data = (button_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( button_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    BUTTON_AUTOREPEAT_INIT( data );
	    data->flags = wm->opt_button_flags;
	    data->text_color = wm->color3;
	    data->text_opacity = 255;
	    data->menu_val = -1;
	    data->prev_menu_val = -1;
	    if( wm->control_type == TOUCHCONTROL )
		data->add_radius = wm->scrollbar_size / 2;
	    else
		data->add_radius = 0;
	    if( wm->opt_button_image1.img )
	    {
		data->img1 = wm->opt_button_image1;
		data->img2 = wm->opt_button_image2;
	    }
	    if( data->flags & BUTTON_FLAG_SHOW_VALUE )
	    {
		win->font = wm->font_small;
		data->menu_val = 0;
	    }
	    smem_clear( &wm->opt_button_image1, sizeof( wm->opt_button_image1 ) );
	    smem_clear( &wm->opt_button_image2, sizeof( wm->opt_button_image2 ) );
	    wm->opt_button_flags = 0;
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
    	    if( data->flags & BUTTON_FLAG_AUTOREPEAT ) BUTTON_AUTOREPEAT_DEINIT( data );
	    smem_free( data->menu );
	    smem_free( data->text );
	    smem_free( data->val );
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    data->focused = true;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
	        int rx = evt->x - win->screen_x;
	        int ry = evt->y - win->screen_y;
		if( rx >= 0 && rx < win->xsize &&
		    ry >= 0 && ry < win->ysize )
		{
		    data->prev_menu_val = data->menu_val;
		    data->evt_flags = evt->flags;
		    data->pushed = 1;
		    data->pen_inside = 1;
		    if( data->flags & BUTTON_FLAG_AUTOREPEAT ) BUTTON_AUTOREPEAT_START( data, button_timer );
		    draw_window( win, wm );
		    if( data->begin_handler )
		    {
			data->begin_handler( data->begin_handler_data, win );
		    }
		    retval = 1;
		}
	    }
	    break;
	case EVT_MOUSEMOVE:
	    if( data->pushed )
	    {
	        int rx = evt->x - win->screen_x;
	        int ry = evt->y - win->screen_y;
		if( rx >= -data->add_radius && rx < win->xsize + data->add_radius &&
		    ry >= -data->add_radius && ry < win->ysize + data->add_radius )
		{
		    data->pen_inside = 1;
		}
		else
		{
		    data->pen_inside = 0;
		}
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT && data->pushed )
	    {
	        int rx = evt->x - win->screen_x;
	        int ry = evt->y - win->screen_y;
		data->pushed = 0;
		if( data->focused ) //for correct WIN_FLAG_ALWAYS_UNFOCUSED handling
		{
		    set_focus_win( data->prev_focus_win, wm );
		    data->focused = false;
		}
		if( rx >= -data->add_radius && rx < win->xsize + data->add_radius &&
		    ry >= -data->add_radius && ry < win->ysize + data->add_radius )
		{
		    if( data->menu )
		    {
		        //Show popup with text menu:
		        if( data->menu_items == 2 && ( ( data->flags & BUTTON_FLAG_SHOW_PREV_VALUE ) || ( data->flags & BUTTON_FLAG_SHOW_VALUE ) ) )
		        {
		    	    data->menu_val++;
		    	    if( data->menu_val >= data->menu_items ) data->menu_val = 0;
			    win->action_result = data->menu_val;
		        }
		        else
		        {
		    	    int prev = -1;
			    if( data->flags & BUTTON_FLAG_SHOW_PREV_VALUE ) prev = data->menu_val;
			    WINDOWPTR pp = popup_menu_open( win->name, data->menu, win->screen_x, win->screen_y, wm->menu_color, prev, wm );
		            if( pp )
            			set_handler( pp, button_menu_action_handler, win, wm );
            		    retval = 1;
            		    break;
			}
		    }
		}
		if( data->flags & BUTTON_FLAG_AUTOREPEAT ) BUTTON_AUTOREPEAT_DEINIT( data );
		draw_window( win, wm );
		win_action_handler2_t end_handler = data->end_handler;
		void* end_handler_data = data->end_handler_data;
		if( rx >= -data->add_radius && rx < win->xsize + data->add_radius &&
		    ry >= -data->add_radius && ry < win->ysize + data->add_radius &&
		    win->action_handler )
		{
		    win->action_handler( win->handler_data, win, wm );
		}
		if( end_handler )
		{
		    end_handler( end_handler_data, win );
		}
		retval = 1;
	    }
	    break;
	case EVT_UNFOCUS:
	    data->focused = false;
	    if( data->pushed )
	    {
		data->pushed = 0;
		if( data->flags & BUTTON_FLAG_AUTOREPEAT ) BUTTON_AUTOREPEAT_DEINIT( data );
		draw_window( win, wm );
		if( data->end_handler )
		{
		    data->end_handler( data->end_handler_data, win );
		}
		retval = 1;
	    }
	    break;
	case EVT_DRAW:
	{
	    COLOR col;

	    if( data->flags & BUTTON_FLAG_AUTOBACKCOLOR ) 
		col = win->parent->color;
	    else
		col = win->color;
	    if( data->pushed )
		col = PUSHED_COLOR( col );

	    wbd_lock( win );

	    draw_frect( 0, 0, win->xsize, win->ysize, col, wm );

	    if( data->img1.img )
	    {
		//Draw image:
		sdwm_image_scaled* img = &data->img1;
		if( data->pushed && data->img2.img )
		    img = &data->img2;
		wm->cur_opacity = data->text_opacity;
		draw_image_scaled(
		    ( win->xsize - img->dest_xsize ) / 2,
		    ( win->ysize - img->dest_ysize ) / 2 + data->pushed,
		    img,
		    wm );
		wm->cur_opacity = 255;
	    }
	    else
	    {
		COLOR text_color = blend( col, data->text_color, data->text_opacity );
		wm->cur_font_color = text_color;

		char* text;
		if( data->text )
		    text = data->text;
		else
		    text = (char*)win->name;
		if( text )
		{
		    //Draw name:
		    int tx;
		    int ty;
		    if( data->flags & BUTTON_FLAG_SHOW_VALUE )
		    {
			//Show current selected item (data->menu_val) of the popup menu
			tx = 1;
			ty = win->ysize / 2 - char_y_size( wm );
			draw_string( text, tx, ty, wm );
			//int text_xsize = string_x_size( text, wm );
			//draw_string( ":", tx + text_xsize, ty, wm );

			//Draw value:
			char ts[ 128 ];
			ts[ 0 ] = 0;
			ts[ 128 - 1 ] = 0;
			char* val = data->val;
			if( data->menu )
			{
			    int i = 0;
			    char* m = (char*)data->menu;
			    for( ; *m != 0; m++ )
			    {
				if( *m == 0xA || *m == 0xD ) 
				{
				    i++;
				    continue;
				}
				if( i == data->menu_val )
				{
				    for( int i2 = 0; i2 < 128 - 1; i2++ )
				    {
					ts[ i2 ] = *m;
					if( *m == 0 || *m == 0xD || *m == 0xA ) 
					{
					    ts[ i2 ] = 0;
					    break;
					}
					m++;
				    }
				    break;
				}
			    }
			    val = ts;
			}
			ty = win->ysize / 2 + 1;
			wm->cur_font_color = wm->color2; //blend( wm->color2, wm->blue, 64 );
			draw_string( val, tx, ty, wm );
		    }
		    else
		    {
			int xx = string_x_size( text, wm );
			tx = ( win->xsize - xx ) / 2;
			ty = ( win->ysize - char_y_size( wm ) ) / 2;
			if( xx > win->xsize )
			{
			    if( data->flags & BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW )
			    {
				tx = 1;
			    }
			}
			draw_string( text, tx, ty + data->pushed, wm );
		    }
		}
	    }

	    //Draw border:
	    if( ( data->flags & BUTTON_FLAG_FLAT ) == 0 )
	        draw_rect( 0, 0, win->xsize - 1, win->ysize - 1, BORDER_COLOR( col ), wm );

	    wbd_draw( wm );
	    wbd_unlock( wm );

	    retval = 1;
	    break;
	}
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void button_set_begin_handler( WINDOWPTR win, win_action_handler2_t handler, void* handler_data )
{
    if( !win ) return;
    if( win->win_handler != button_handler ) return;
    button_data* data = (button_data*)win->data;
    data->begin_handler = handler;
    data->begin_handler_data = handler_data;
}

void button_set_end_handler( WINDOWPTR win, win_action_handler2_t handler, void* handler_data )
{
    if( !win ) return;
    if( win->win_handler != button_handler ) return;
    button_data* data = (button_data*)win->data;
    data->end_handler = handler;
    data->end_handler_data = handler_data;
}

void button_set_menu( WINDOWPTR win, const char* menu )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    smem_free( data->menu );
    data->menu = smem_strdup( menu );
    if( data->menu )
    {
	data->menu_items = 1;
	const char* p = menu;
	while( 1 )
	{
	    char c = *p;
	    if( c == 0 ) break;
	    if( c == 0xA ) data->menu_items++;
	    p++;
	}
    }
}

void button_set_menu_val( WINDOWPTR win, int val )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    data->menu_val = val;
}

int button_get_menu_val( WINDOWPTR win )
{
    if( !win ) return -1;
    button_data* data = (button_data*)win->data;
    return data->menu_val;
}

int button_get_prev_menu_val( WINDOWPTR win )
{
    if( !win ) return -1;
    button_data* data = (button_data*)win->data;
    return data->prev_menu_val;
}

void button_set_text( WINDOWPTR win, const char* text )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    smem_free( data->text );
    if( text )
    {
        data->text = smem_strdup( text );
    }
    else 
    {
        data->text = 0;
    }
}

void button_set_text_color( WINDOWPTR win, COLOR c )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    data->text_color = c;
}

void button_set_text_opacity( WINDOWPTR win, uint8_t opacity )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    data->text_opacity = opacity;
}

int button_get_text_opacity( WINDOWPTR win )
{
    if( !win ) return 0;
    button_data* data = (button_data*)win->data;
    return data->text_opacity;
}

void button_set_val( WINDOWPTR win, const char* val )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    smem_free( data->val );
    if( val )
    {
        data->val = smem_strdup( val );
    }
    else 
    {
        data->val = 0;
    }
}

void button_set_images( WINDOWPTR win, sdwm_image_scaled* img1, sdwm_image_scaled* img2 )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    if( img1 )
        data->img1 = *img1;
    if( img2 )
	data->img2 = *img2;
}

void button_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( !win ) return;
    button_data* data = (button_data*)win->data;
    data->flags = (uint32_t)flags;
}

uint32_t button_get_flags( WINDOWPTR win )
{
    if( !win ) return 0;
    button_data* data = (button_data*)win->data;
    return (uint32_t)data->flags;
}

int button_get_evt_flags( WINDOWPTR win )
{
    if( !win ) return 0;
    button_data* data = (button_data*)win->data;
    return data->evt_flags;
}

int button_get_optimal_xsize( const char* button_name, int font, bool smallest_as_possible, window_manager* wm )
{
    int rv = 0;
    rv = font_string_x_size( button_name, font, wm ) + font_char_x_size( ' ', font, wm );
    if( smallest_as_possible )
    {
	if( rv < wm->small_button_xsize ) rv = wm->small_button_xsize;
    }
    else
    {
	if( rv < wm->button_xsize ) rv = wm->button_xsize;
    }
    return rv;
}

//
//
//

struct scrollbar_data
{
    WINDOWPTR win;
    WINDOWPTR but1;
    WINDOWPTR but2;
    bool but1_inc;
    bool vert;
    bool rev;
    bool compact_mode;
    bool flat;
    char* name;
    char* values;
    char values_delim; //delimiter character
    int max_value;
    int normal_value;
    int page_size;
    int step_size;
    int cur;
    bool bar_selected;
    int drag_start_x;
    int drag_start_y;
    int drag_start_val;
    int show_offset;
    uint32_t flags;

    int working_area;
    int min_work_area;
    int button_size;
    WINDOWPTR expanded;
    WINDOWPTR editor;

    char* name_str;
    char* value_str;

    int cur_x;
    int cur_y;

    int pos;
    int bar_size;
    float one_pixel_size;
    int move_region;

    scrollbar_hex hex;

    uint evt_flags;

    bool begin;
    int	(*begin_handler)( void*, WINDOWPTR, int );
    int	(*end_handler)( void*, WINDOWPTR, int );
    int	(*opt_handler)( void*, WINDOWPTR, int );
    void* begin_handler_data;
    void* end_handler_data;
    void* opt_handler_data;

    WINDOWPTR prev_focus_win;
};

int scrollbar_button_end( void* user_data, WINDOWPTR win )
{
    scrollbar_data* data = (scrollbar_data*)user_data;
    if( data->begin )
    {
	data->evt_flags = button_get_evt_flags( win );
	if( data->end_handler )
	    data->end_handler( data->end_handler_data, data->win, scrollbar_get_value( data->win, win->wm ) );
	data->begin = 0;
    }
    return 0;
}

int scrollbar_button( void* user_data, WINDOWPTR win, window_manager* wm )
{
    scrollbar_data* data = (scrollbar_data*)user_data;
    data->evt_flags = button_get_evt_flags( win );
    if( data->begin == 0 )
    {
	data->begin = 1;
	if( data->begin_handler )
	    data->begin_handler( data->begin_handler_data, data->win, scrollbar_get_value( data->win, wm ) );
    }
    bool inc = 0;
    if( data->but1 == win && data->but1_inc == 1 ) inc = 1;
    if( data->but2 == win && data->but1_inc == 0 ) inc = 1;
    if( inc )
    {
	data->cur += data->step_size;
	if( data->cur < 0 ) data->cur = 0;
	if( data->cur > data->max_value ) data->cur = data->max_value;
    }
    else 
    {
	data->cur -= data->step_size;
	if( data->cur < 0 ) data->cur = 0;
	if( data->cur > data->max_value ) data->cur = data->max_value;
    }
    draw_window( data->win, wm );
    if( data->win->action_handler )
	data->win->action_handler( data->win->handler_data, data->win, wm );
    return 0;
}

void draw_scrollbar_horizontal_selection( WINDOWPTR win, int x )
{
    window_manager* wm = win->wm;
    
    int xsize = wm->scrollbar_size / 2;

    wm->cur_opacity = 255;

    sdwm_polygon p;
    sdwm_vertex v[ 4 ];
    v[ 0 ].x = x;
    v[ 0 ].y = 0;
    v[ 0 ].c = wm->color3;
    v[ 0 ].t = 64;
    v[ 1 ].x = x + xsize;
    v[ 1 ].y = 0;
    v[ 1 ].t = 0;
    v[ 2 ].x = x + xsize;
    v[ 2 ].y = win->ysize;
    v[ 2 ].t = 0;
    v[ 3 ].x = x;
    v[ 3 ].y = win->ysize;
    v[ 3 ].t = 64;
    p.vnum = 4;
    p.v = v;
    wm->cur_flags = WBD_FLAG_ONE_COLOR;
    draw_polygon( &p, wm );

    v[ 1 ].x = x - xsize;
    v[ 2 ].x = x - xsize;
    draw_polygon( &p, wm );
}

static void draw_scrollbar_vertical_selection( WINDOWPTR win, int y )
{
    window_manager* wm = win->wm;

    int ysize = wm->scrollbar_size / 2;
    
    wm->cur_opacity = 255;
    
    sdwm_polygon p;
    sdwm_vertex v[ 4 ];
    v[ 0 ].x = 0;
    v[ 0 ].y = y;
    v[ 0 ].c = wm->color3;
    v[ 0 ].t = 64;
    v[ 1 ].x = 0;
    v[ 1 ].y = y + ysize;
    v[ 1 ].t = 0;
    v[ 2 ].x = win->xsize;
    v[ 2 ].y = y + ysize;
    v[ 2 ].t = 0;
    v[ 3 ].x = win->xsize;
    v[ 3 ].y = y;
    v[ 3 ].t = 64;
    p.vnum = 4;
    p.v = v;
    wm->cur_flags = WBD_FLAG_ONE_COLOR;
    draw_polygon( &p, wm );
    
    v[ 1 ].y = y - ysize;
    v[ 2 ].y = y - ysize;
    draw_polygon( &p, wm );
}

static void scrollbar_calc_button_size( WINDOWPTR win )
{
    scrollbar_data* data = (scrollbar_data*)win->data;
    if( data->vert )
    {
	data->button_size = data->but1->ysize;
    }
    else
    {
	data->button_size = data->but1->xsize;
    }
}

static void scrollbar_calc_working_area( WINDOWPTR win )
{
    scrollbar_data* data = (scrollbar_data*)win->data;
    window_manager* wm = win->wm;

    scrollbar_calc_button_size( win );

    data->working_area = 0;
    if( data->vert )
        data->working_area = win->ysize - data->button_size * 2;
    else
        data->working_area = win->xsize - data->button_size * 2;

    if( data->compact_mode )
    {
	if( data->min_work_area == 0 )
	    data->min_work_area = wm->scrollbar_size * 2;
	if( data->working_area < data->min_work_area )
	{
	    if( ( data->but1->flags & WIN_FLAG_ALWAYS_INVISIBLE ) == 0 )
	    {
	        hide_window( data->but1 );
	        hide_window( data->but2 );
	        data->but1->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	        data->but2->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	        recalc_regions( wm );
	    }
	    data->working_area += data->button_size * 2;
	    data->button_size = 0;
	}
	else
	{
	    if( data->but1->flags & WIN_FLAG_ALWAYS_INVISIBLE )
	    {
	        data->but1->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	        data->but2->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	        show_window( data->but1 );
	        show_window( data->but2 );
	        recalc_regions( wm );
	    }
	}
    }
}

static void scrollbar_get_strings( WINDOWPTR win )
{
    scrollbar_data* data = (scrollbar_data*)win->data;

    const int text_val_size = 256;

    data->name_str = data->name;
    if( !data->value_str ) data->value_str = (char*)smem_new( text_val_size + 64 );
    data->value_str[ 0 ] = 0;

    if( data->compact_mode )
    {
	char text_val[ text_val_size ];
	text_val[ 0 ] = 0;
	text_val[ text_val_size - 1 ] = 0;
	if( data->values )
	{
	    smem_split_str( text_val, text_val_size, data->values, data->values_delim, data->cur );
	}
	if( data->hex != scrollbar_hex_off && data->max_value )
	{
	    bool show_hex_val = false;
	    int val = data->cur + data->show_offset;
	    uint hex_val = 0;
	    switch( data->hex )
	    {
		case scrollbar_hex_scaled:
		    //Scaled to 0x0000 (0%) ... 0x8000 (100%)
	    	    hex_val = ( data->cur * 32768 ) / data->max_value;
	    	    if( ( data->cur * 32768 ) % data->max_value ) hex_val++;
	    	    if( hex_val > 32768 ) hex_val = 32768;
	    	    break;
		case scrollbar_hex_normal:
		    //Normal hex value without show_offset:
	    	    hex_val = data->cur;
	    	    break;
	    	case scrollbar_hex_normal_with_offset:
		    //Normal hex value with show_offset:
	    	    hex_val = val;
	    	    break;
	    	default: break;
	    }
	    if( text_val[ 0 ] ) show_hex_val = true;
	    if( hex_val > 9 || (signed)hex_val != val ) show_hex_val = true;
	    if( show_hex_val )
	    {
		if( text_val[ 0 ] )
	    	    sprintf( data->value_str, "%s (%x)", text_val, (unsigned int)hex_val );
	    	else
	    	    sprintf( data->value_str, "%d (%x)", val, (unsigned int)hex_val );
	    }
	    else
	    {
		sprintf( data->value_str, "%d", val );
	    }
	}
	else
	{
	    if( text_val[ 0 ] )
		sprintf( data->value_str, "%s", text_val );
	    else
		sprintf( data->value_str, "%d", data->cur + data->show_offset );
	}
    }
}

static float scrollbar_vconv( float val, uint32_t flags, bool to_display )
{
    if( flags & SCROLLBAR_FLAG_EXP2 )
    {
        val = 1 - val;
        if( to_display )
        {
            val = val * val;
        }
        else
        {
            if( val > 0 ) val = sqrt( val ); else val = 0;
        }
        return 1 - val;
    }
    if( flags & SCROLLBAR_FLAG_EXP3 )
    {
        val = 1 - val;
        if( to_display )
        {
            val = val * val * val;
        }
        else
        {
            if( val > 0 ) val = pow( val, 1.0F / 3.0F ); else val = 0;
        }
        return 1 - val;
    }
    return val;
}

struct scrollbar_editor_data
{
    WINDOWPTR win;
    scrollbar_data* sdata;
    int start_val;
    WINDOWPTR text;
    WINDOWPTR ok;
    WINDOWPTR cancel;
    int correct_ysize;
};

int scrollbar_editor_text_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    scrollbar_editor_data* data = (scrollbar_editor_data*)user_data;

    scrollbar_set_value( data->sdata->win, text_get_value( win, wm ) - data->sdata->show_offset, wm );
    scrollbar_call_handler( data->sdata->win );

    return 0;
}

int scrollbar_editor_ok_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    scrollbar_editor_data* data = (scrollbar_editor_data*)user_data;

    scrollbar_set_value( data->sdata->win, text_get_value( data->text, wm ) - data->sdata->show_offset, wm );
    scrollbar_call_handler( data->sdata->win );

    remove_window( data->win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );

    return 0;
}

int scrollbar_editor_cancel_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    scrollbar_editor_data* data = (scrollbar_editor_data*)user_data;

    if( data->start_val != data->sdata->cur )
    {
	scrollbar_set_value( data->sdata->win, data->start_val, wm );
	scrollbar_call_handler( data->sdata->win );
    }

    remove_window( data->win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );

    return 0;
}

int scrollbar_editor_handler( sundog_event* evt, window_manager* wm ) //host - scrollbar_data
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    scrollbar_editor_data* data = (scrollbar_editor_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( scrollbar_editor_data );
	    break;

	case EVT_AFTERCREATE:
	    {
		data->win = win;
		data->sdata = (scrollbar_data*)win->host;

		int y = wm->interelement_space;
		int x = wm->interelement_space;
		const char* bname = NULL;
		int bxsize = 0;

		wm->opt_text_numeric = 1;
        	wm->opt_text_num_min = data->sdata->show_offset;
        	wm->opt_text_num_max = data->sdata->max_value + data->sdata->show_offset;
        	data->text = new_window( "SBValue", 0, y, 1, wm->controller_ysize, wm->text_background, win, text_handler, wm );
        	data->start_val = data->sdata->cur;
        	text_set_value( data->text, data->sdata->cur + data->sdata->show_offset, wm );
        	set_handler( data->text, scrollbar_editor_text_handler, data, wm );
        	set_window_controller( data->text, 0, wm, (WCMD)wm->interelement_space, CEND );
        	set_window_controller( data->text, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		y += wm->controller_ysize + wm->interelement_space;

		bname = wm_get_string( STR_WM_OK ); bxsize = button_get_optimal_xsize( bname, win->font, false, wm );
        	data->ok = new_window( bname, x, 0, bxsize, 1, wm->button_color, win, button_handler, wm );
        	set_handler( data->ok, scrollbar_editor_ok_handler, data, wm );
        	set_window_controller( data->ok, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
        	set_window_controller( data->ok, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
        	x += bxsize + wm->interelement_space2;

		bname = wm_get_string( STR_WM_CANCEL ); bxsize = button_get_optimal_xsize( bname, win->font, false, wm );
        	data->cancel = new_window( bname, x, 0, bxsize, 1, wm->button_color, win, button_handler, wm );
        	set_handler( data->cancel, scrollbar_editor_cancel_handler, data, wm );
        	set_window_controller( data->cancel, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
        	set_window_controller( data->cancel, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
        	x += bxsize + wm->interelement_space2;

        	set_focus_win( data->text, wm );

		data->correct_ysize = y + wm->button_ysize + wm->interelement_space;
	    }
	    retval = 1;
	    break;

	case EVT_BEFORECLOSE:
	    data->sdata->editor = NULL;
	    retval = 1;
	    break;

	case EVT_SCREENRESIZE:
	    if( !win->visible ) break; //recalc_controllers() will not be performed, so we will get the wrong window parameters
        case EVT_BEFORESHOW:
            resize_window_with_decorator( win, 0, data->correct_ysize, wm );
            break;

	case EVT_BUTTONDOWN:
	case EVT_BUTTONUP:
        case EVT_MOUSEBUTTONDOWN:
        case EVT_MOUSEMOVE:
        case EVT_MOUSEBUTTONUP:
        case EVT_TOUCHBEGIN:
        case EVT_TOUCHEND:
        case EVT_TOUCHMOVE:
        case EVT_FOCUS:
        case EVT_UNFOCUS:
            retval = 1;
            break;
    }
    return retval;
}

struct scrollbar_expanded_data
{
    WINDOWPTR win;
    WINDOWPTR ctl;
};

int scrollbar_expanded_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    scrollbar_expanded_data* data = (scrollbar_expanded_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( scrollbar_expanded_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    data->ctl = 0;
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		scrollbar_data* cdata = (scrollbar_data*)data->ctl->data;
		scrollbar_get_strings( data->ctl );
		
		wbd_lock( win );

		wm->cur_opacity = 255;
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		
		int xc = 0;
                if( cdata->max_value != 0 )
                {
		    xc = scrollbar_vconv( (float)cdata->cur / cdata->max_value, cdata->flags, true ) * ( win->xsize - wm->interelement_space * 2 );
                    //xc = ( ( win->xsize - wm->interelement_space * 2 ) * ( ( cdata->cur << 10 ) / cdata->max_value ) ) >> 10;
                }
		draw_frect( wm->interelement_space, wm->interelement_space, xc, win->ysize - wm->interelement_space * 2, blend( wm->color2, win->color, 128 ), wm );

		//Show normal value:
		if( cdata->normal_value && cdata->normal_value < cdata->max_value )
		{
		    wm->cur_opacity = 255;
		    int cx = scrollbar_vconv( (float)cdata->normal_value / cdata->max_value, cdata->flags, true ) * ( win->xsize - wm->interelement_space * 2 );
		    cx += wm->interelement_space;
		    int grad_size = ( wm->scrollbar_size * 3 ) / 4;
		    COLOR grad_color = blend( wm->color3, wm->red, 160 );
		    draw_hgradient( cx - grad_size, wm->interelement_space, grad_size, win->ysize - wm->interelement_space * 2, grad_color, 0, 48, wm );
		    draw_hgradient( cx, wm->interelement_space, grad_size, win->ysize - wm->interelement_space * 2, grad_color, 48, 0, wm );
		    wm->cur_opacity = 52;
		    draw_frect( cx, wm->interelement_space, 1, win->ysize - wm->interelement_space * 2, wm->color3, wm );
		    wm->cur_opacity = 255;
		    draw_frect( win->xsize - wm->interelement_space, wm->interelement_space, wm->interelement_space, win->ysize - wm->interelement_space * 2, win->color, wm );
		}
		
		int ychar = char_y_size( wm );

		wm->cur_font_color = wm->color3;

		if( cdata->name_str )
		{
		    draw_string( cdata->name_str, wm->interelement_space, win->ysize / 2 - ychar, wm );
		}

		if( cdata->value_str )
		{
		    draw_string( cdata->value_str, wm->interelement_space, win->ysize / 2, wm );
		}
		
		wbd_draw( wm );
    		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
        case EVT_MOUSEMOVE:
    	    hide_window( win );
    	    retval = 1;
    	    break;
    }
    return retval;
}

int scrollbar_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    scrollbar_data* data = (scrollbar_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( scrollbar_data );
	    break;
	case EVT_AFTERCREATE:
	    win->flags |= WIN_FLAG_ALWAYS_HANDLE_DRAW_EVT;

	    data->win = win;
	    data->flat = wm->opt_scrollbar_flat;
	    data->begin_handler = wm->opt_scrollbar_begin_handler;
	    data->end_handler = wm->opt_scrollbar_end_handler;
	    data->opt_handler = wm->opt_scrollbar_opt_handler;
	    data->begin_handler_data = wm->opt_scrollbar_begin_handler_data;
	    data->end_handler_data = wm->opt_scrollbar_end_handler_data;
	    data->opt_handler_data = wm->opt_scrollbar_opt_handler_data;
	    data->begin = 0;
	    data->rev = wm->opt_scrollbar_reverse;
	    data->compact_mode = wm->opt_scrollbar_compact;

	    if( data->compact_mode )
	    {
		data->flat = 1;
		win->font = wm->font_small;
	    }

	    if( wm->opt_scrollbar_vertical )
	    {
		data->vert = 1;
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		data->but1 = new_window( STR_UP, 0, 0, 1, 1, win->color, win, button_handler, wm );
		button_set_end_handler( data->but1, scrollbar_button_end, data );
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		data->but2 = new_window( STR_DOWN, 0, 0, 1, 1, win->color, win, button_handler, wm );
		button_set_end_handler( data->but2, scrollbar_button_end, data );
		set_window_controller( data->but2, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->but2, 1, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->but2, 2, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->but1, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->but1, 2, wm, CPERC, (WCMD)100, CEND );
		if( data->compact_mode )
		{
		    set_window_controller( data->but2, 3, wm, CWIN, (WCMD)win, CXSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CEND );
		    set_window_controller( data->but1, 1, wm, CWIN, (WCMD)win, CXSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CSUB, CR0, CEND );
		    set_window_controller( data->but1, 3, wm, CWIN, (WCMD)win, CXSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CEND );
		}
		else
		{
		    set_window_controller( data->but2, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
		    set_window_controller( data->but1, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size * 2, CEND );
		    set_window_controller( data->but1, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
		}
		if( wm->opt_scrollbar_reverse )
		{
		    data->but1_inc = 1;
		}
		else
		{
		    data->but1_inc = 0;
		}
	    }
	    else
	    {
		data->vert = 0;
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		data->but1 = new_window( STR_RIGHT, 0, 0, 1, 1, win->color, win, button_handler, wm );
		button_set_end_handler( data->but1, scrollbar_button_end, data );
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT | BUTTON_FLAG_FLAT;
		data->but2 = new_window( STR_LEFT, 0, 0, 1, 1, win->color, win, button_handler, wm );
		button_set_end_handler( data->but2, scrollbar_button_end, data );
		set_window_controller( data->but1, 0, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->but1, 1, wm, CPERC, (WCMD)0, CEND );
		set_window_controller( data->but1, 3, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->but2, 1, wm, (WCMD)0, CEND );
		set_window_controller( data->but2, 3, wm, CPERC, (WCMD)100, CEND );
		if( data->compact_mode )
		{
		    set_window_controller( data->but1, 2, wm, CWIN, (WCMD)win, CYSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CEND );
		    set_window_controller( data->but2, 0, wm, CWIN, (WCMD)win, CYSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CSUB, CR0, CEND );
		    set_window_controller( data->but2, 2, wm, CWIN, (WCMD)win, CYSIZE, CMULDIV256, (WCMD)SIDEBTN, CMAXVAL, (WCMD)wm->scrollbar_size, CPUTR0, CPERC, (WCMD)100, CSUB, CR0, CEND );
		}
		else
		{
		    set_window_controller( data->but1, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
		    set_window_controller( data->but2, 0, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size * 2, CEND );
		    set_window_controller( data->but2, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
		}
		if( wm->opt_scrollbar_reverse )
		{
		    data->but1_inc = 0;
		}
		else
		{
		    data->but1_inc = 1;
		}
	    }
	    scrollbar_calc_button_size( win );
	    //data->working_area = 0;
	    //data->min_work_area = 0;
	    //data->expanded = 0;
	    //data->editor = NULL;
	    set_handler( data->but1, scrollbar_button, data, wm );
	    set_handler( data->but2, scrollbar_button, data, wm );
	    if( data->compact_mode )
	    {
		data->but1->color = win->color;
		data->but2->color = win->color;
	    }
	    button_set_text_opacity( data->but1, 150 );
	    button_set_text_opacity( data->but2, 150 );
	    button_set_text_color( data->but1, wm->color2 );
	    button_set_text_color( data->but2, wm->color2 );

	    data->name = 0;
	    data->values = 0;
	    data->values_delim = '/';
	    data->cur = 0;
	    data->max_value = 0;
	    data->normal_value = 0;
	    data->page_size = 0;
	    data->step_size = 1;
	    data->bar_selected = 0;
	    data->drag_start_x = 0;
	    data->drag_start_y = 0;
	    data->show_offset = 0;
	    data->one_pixel_size = 0;
	    
	    data->hex = scrollbar_hex_off;
	    
	    data->prev_focus_win = 0;
	    
	    data->evt_flags = 0;
	    	    
	    wm->opt_scrollbar_vertical = false;
	    wm->opt_scrollbar_reverse = false;
	    wm->opt_scrollbar_compact = false;
	    wm->opt_scrollbar_flat = 0;
	    wm->opt_scrollbar_begin_handler = 0;
	    wm->opt_scrollbar_end_handler = 0;
	    wm->opt_scrollbar_opt_handler = 0;
	    wm->opt_scrollbar_begin_handler_data = 0;
	    wm->opt_scrollbar_end_handler_data = 0;
	    wm->opt_scrollbar_opt_handler_data = 0;

	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    smem_free( data->name ); data->name = NULL;
	    smem_free( data->values ); data->values = NULL;
	    smem_free( data->value_str ); data->value_str = NULL;
	    remove_window( data->editor, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( !win->visible ) break;
	    {
        	data->evt_flags = evt->flags;
		int inc = 0;
        	if( evt->key & MOUSE_BUTTON_SCROLLUP ) inc = 1;
        	if( evt->key & MOUSE_BUTTON_SCROLLDOWN ) inc = -1;
        	if( inc )
        	{
        	    if( data->vert ) inc = -inc;
		    if( data->begin == 0 )
		    {
		    	data->begin = 1;
			if( data->begin_handler )
			    data->begin_handler( data->begin_handler_data, win, scrollbar_get_value( win, wm ) );
		    }
		    if( inc > 0 )
		    {
			data->cur += data->step_size;
			if( data->cur < 0 ) data->cur = 0;
			if( data->cur > data->max_value ) data->cur = data->max_value;
		    }
		    else 
		    {
			data->cur -= data->step_size;
			if( data->cur < 0 ) data->cur = 0;
			if( data->cur > data->max_value ) data->cur = data->max_value;
		    }
		    draw_window( win, wm );
		    if( win->action_handler )
			win->action_handler( win->handler_data, win, wm );
		    if( data->begin )
		    {
		    	if( data->end_handler )
			    data->end_handler( data->end_handler_data, win, scrollbar_get_value( win, wm ) );
			data->begin = 0;
		    }
        	}
            }
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( evt->type == EVT_MOUSEBUTTONDOWN )
		{
		    if( data->begin == 0 )
		    {
			data->begin = 1;
			if( data->begin_handler )
			    data->begin_handler( data->begin_handler_data, win, scrollbar_get_value( win, wm ) );
		    }
		}
		
		int value_changed = 1;

		data->cur_x = rx;
		data->cur_y = ry;
		if( data->compact_mode == 0 )
		{
		    //Normal mode:
		    if( evt->type == EVT_MOUSEBUTTONDOWN && 
			rx >= 0 && rx < win->xsize && ry >= 0 && ry < win->ysize )
		    {
			data->bar_selected = 1;
			data->drag_start_x = rx;
			data->drag_start_y = ry;
			data->drag_start_val = data->cur;
		    }
		    if( evt->type == EVT_MOUSEMOVE )
		    {
			//Move:
			if( data->bar_selected )
			{
			    int d = 0;
			    if( data->vert ) 
				d = ry - data->drag_start_y;
			    else
				d = rx - data->drag_start_x;
			    if( data->rev )
				data->cur = data->drag_start_val - (int)( (float)d * data->one_pixel_size );
			    else
				data->cur = data->drag_start_val + (int)( (float)d * data->one_pixel_size );
			}
		    }
		}
		else
		{
		    //Compact mode:
		    if( evt->type == EVT_MOUSEBUTTONDOWN &&
			rx >= 0 && ry >= 0 &&
			rx < win->xsize && ry < win->ysize )
		    {
			if( data->but1 && data->but1->visible == 0 )
			{
			    int new_xsize = wm->scrollbar_size * 10;
			    int new_ysize = wm->scrollbar_size * 2;
			    if( new_xsize > wm->desktop_xsize )
				new_xsize = wm->desktop_xsize;
			    int new_x = win->screen_x + ( win->xsize - new_xsize ) / 2;
			    int new_y = win->screen_y + ( win->ysize - new_ysize ) / 2;
			    if( new_x + new_xsize > wm->desktop_xsize ) new_x -= new_x + new_xsize - wm->desktop_xsize;
			    if( new_y + new_ysize > wm->desktop_ysize ) new_y -= new_y + new_ysize - wm->desktop_ysize;
			    if( new_x < 0 ) new_x = 0;
			    if( new_y < 0 ) new_y = 0;
			    data->expanded = new_window( "Expanded controller", new_x, new_y, new_xsize, new_ysize, wm->color0, wm->root_win, scrollbar_expanded_handler, wm );
			    scrollbar_expanded_data* edata = (scrollbar_expanded_data*)data->expanded->data;
			    edata->ctl = win;
			    show_window( data->expanded );
			    recalc_regions( wm );
			    draw_window( data->expanded, wm );
			}
			data->bar_selected = 1;
			data->drag_start_x = rx;
			data->drag_start_y = ry;
			data->drag_start_val = data->cur;
		    }
		    if( data->bar_selected == 1 && evt->type == EVT_MOUSEMOVE )
		    {
			if( data->working_area > 1 )
			{
			    int dx = rx - data->drag_start_x;
			    int dy = ry - data->drag_start_y;
			    float v; //display value
			    if( data->expanded == 0 )
			    {
				int s = data->working_area - 1;
				v = scrollbar_vconv( (float)data->drag_start_val / data->max_value, data->flags, true ) + (float)dx/s;
				//new_val = ( ( dx << 12 ) / s ) * data->max_value;
			    }
			    else
			    {
				int s = data->expanded->xsize - wm->interelement_space * 2;
				if( data->values )
				{
				    if( s / ( wm->scrollbar_size / 2 ) > data->max_value + 1 )
					s = ( data->max_value + 1 ) * ( wm->scrollbar_size / 2 );
				}
				v = scrollbar_vconv( (float)data->drag_start_val / data->max_value, data->flags, true ) + (float)(dx-dy)/s;
				//new_val = ( ( dx << 12 ) / s ) * data->max_value;
				//new_val -= ( ( dy << 12 ) / s ) * data->max_value;
			    }
			    int new_val = scrollbar_vconv( v, data->flags, false ) * data->max_value; //display -> internal
			    //new_val >>= 12;
			    //new_val += data->drag_start_val;
			    if( data->cur == new_val ) 
				value_changed = 0;
			    else
				data->cur = new_val;
			}
		    }
		    else
		    {
			value_changed = 0;
		    }
		}
		//Bounds control:
		if( data->cur < 0 ) data->cur = 0;
		if( data->cur > data->max_value ) data->cur = data->max_value;
		//Redraw it:
		draw_window( win, wm );
		if( data->expanded )
		    draw_window( data->expanded, wm );
		//User handler:
		if( win->action_handler && value_changed )
		    win->action_handler( win->handler_data, win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_FOCUS:
	    data->prev_focus_win = wm->prev_focus_win;
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    if( data->bar_selected )
	    {
		data->bar_selected = 0;
		if( data->expanded )
                {
                    remove_window( data->expanded, wm );
                    data->expanded = 0;
                    recalc_regions( wm );
                    draw_window( wm->root_win, wm );
                }
		draw_window( win, wm );
		if( data->begin )
		{
		    data->begin = 0;
		    if( data->end_handler )
			data->end_handler( data->end_handler_data, win, scrollbar_get_value( win, wm ) );
		}
		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( !win->visible ) break;
	    {
		if( evt->key == MOUSE_BUTTON_LEFT )
		{
		    data->bar_selected = 0;
		    if( data->expanded )
		    {
			remove_window( data->expanded, wm );
			data->expanded = 0;
			recalc_regions( wm );
			draw_window( wm->root_win, wm );
		    }
		    set_focus_win( data->prev_focus_win, wm );
		    draw_window( win, wm );
		    //User handler:
		    if( win->action_handler )
			win->action_handler( win->handler_data, win, wm );
		    if( data->begin )
		    {
			data->begin = 0;
			if( data->end_handler )
			    data->end_handler( data->end_handler_data, win, scrollbar_get_value( win, wm ) );
		    }
		}
		int dx = rx - data->drag_start_x;
		int dy = ry - data->drag_start_y;
		if( dx < 0 ) dx = -dx;
		if( dy < 0 ) dy = -dy;
		int d;
		if( dx > dy ) d = dx; else d = dy;
		if( evt->key == MOUSE_BUTTON_RIGHT || ( ( evt->flags & EVT_FLAG_DOUBLECLICK ) && d < wm->scrollbar_size / 3 ) )
		{
		    if( data->opt_handler )
		    {
			data->opt_handler( data->opt_handler_data, win, scrollbar_get_value( win, wm ) );
		    }
		    else
		    {
			if( data->compact_mode )
			{
			    if( !data->editor )
			    {
    				data->editor = new_window_with_decorator(
        			    data->name,
        			    wm->normal_window_xsize, wm->normal_window_ysize,
        			    wm->dialog_color,
        			    wm->root_win,
        			    data,
        			    scrollbar_editor_handler,
        			    0,
        			    wm );
        		    }
    			    show_window( data->editor );
    			    bring_to_front( data->editor, wm );
    			    recalc_regions( wm );
    			    draw_window( wm->root_win, wm );
    			}
		    }
		}
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		scrollbar_calc_working_area( win );
		scrollbar_get_strings( win );
		
		if( win->reg->numRects == 0 ) { retval = 1; break; } //Because WIN_FLAG_ALWAYS_HANDLE_DRAW_EVT is set
		
		char* name_str = data->name_str;
		if( data->working_area < data->min_work_area )
	        {
    		    if( name_str )
		    {
	    		int p = 0;
	    		while( 1 )
	    		{
			    int c = name_str[ p ];
			    if( c == 0 ) break;
			    if( c == '.' )
			    {
		    		name_str += p + 1;
				break;
			    }
			    if( ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) )
			    {
			    }
			    else break;
			    p++;
			}
		    }
		}

		wbd_lock( win );

		if( data->compact_mode == 0 )
		{
		    //Normal mode:
		    
		    if( data->max_value == 0 || data->page_size == 0 )
		    {
			data->bar_size = data->working_area;
			data->pos = 0;
		    }
		    else
		    {
			//Calculate move-region (in pixels)
			float ppage;
			if( data->max_value + 1 == 0 ) 
			    ppage = 1.0F;
			else
			    ppage = (float)data->page_size / (float)( data->max_value + 1 );
			if( ppage == 0 ) ppage = 1.0F;
			data->move_region = (int)( (float)data->working_area / ( ppage + 1.0F ) );
			if( data->move_region == 0 ) data->move_region = 1;

			//Caclulate slider size (in pixels)
			data->bar_size = data->working_area - data->move_region;

			//Calculate one pixel size
			data->one_pixel_size = (float)( data->max_value + 1 ) / (float)data->move_region;

			//Calculate slider position (in pixels)
			data->pos = (int)( ( (float)data->cur / (float)data->max_value ) * (float)data->move_region );

			if( data->bar_size < 2 ) data->bar_size = 2;
			if( data->rev )
			    data->pos = ( data->working_area - data->pos ) - data->bar_size;

			if( data->pos + data->bar_size > data->working_area )
			{
			    data->bar_size -= ( data->pos + data->bar_size ) - data->working_area;
			}
		    }

		    bool frozen = 0;
		    if( data->bar_size >= data->working_area ) frozen = 1;
		    COLOR bar_color;
		    COLOR bg_color;
		    COLOR marker_color;
		    bg_color = win->parent->color;
		    bar_color = blend( wm->color2, bg_color, 220 );
		    marker_color = blend( bar_color, bg_color, 128 );
		    if( frozen )
		    {
			button_set_text_opacity( data->but1, 64 );
			button_set_text_opacity( data->but2, 64 );
			draw_frect( 0, 0, win->xsize, win->ysize, bg_color, wm );
		    }
		    else 
		    {
			button_set_text_opacity( data->but1, 150 );
			button_set_text_opacity( data->but2, 150 );
			if( data->vert )
			{
			    draw_frect( 0, 0, win->xsize, data->pos, bg_color, wm );
			    draw_frect( 0, data->pos + data->bar_size, win->xsize, data->working_area - ( data->pos + data->bar_size ), bg_color, wm );

			    int ss = 2;
			    draw_frect( 0, 0, win->xsize, ss, marker_color, wm );
			    draw_frect( 0, data->working_area - ss, win->xsize, ss, marker_color, wm );
			    
			    draw_frect( 0, data->pos, win->xsize, data->bar_size, bar_color, wm );
			    draw_frect( 0, data->pos - 1, win->xsize, 1, BORDER_COLOR( bar_color ), wm );
			    draw_frect( 0, data->pos + data->bar_size, win->xsize, 1, BORDER_COLOR( bar_color ), wm );
			}
			else
			{
			    draw_frect( 0, 0, data->pos, win->ysize, bg_color, wm );
			    draw_frect( data->pos + data->bar_size, 0, data->working_area - ( data->pos + data->bar_size ), win->ysize, bg_color, wm );
			    
			    int ss = 2;
			    draw_frect( 0, 0, ss, win->xsize, marker_color, wm );
			    draw_frect( data->working_area - ss, 0, ss, win->xsize, marker_color, wm );
			    
			    draw_frect( data->pos, 0, data->bar_size, win->ysize, bar_color, wm );
			    draw_frect( data->pos - 1, 0, 1, win->ysize, BORDER_COLOR( bar_color ), wm );
			    draw_frect( data->pos + data->bar_size, 0, 1, win->ysize, BORDER_COLOR( bar_color ), wm );
			}
			if( data->bar_selected )
			{
			    wm->cur_opacity = wm->scroll_pressed_color_opacity;
			    draw_frect( 0, 0, win->xsize, win->ysize, wm->scroll_pressed_color, wm );
			    if( data->vert )
				draw_scrollbar_vertical_selection( win, data->cur_y );
			    else 
				draw_scrollbar_horizontal_selection( win, data->cur_x );
			    bg_color = blend( bg_color, wm->scroll_pressed_color, wm->scroll_pressed_color_opacity );
			}
		    }

		    data->but1->color = bg_color;
		    data->but2->color = bg_color;
		}
		else
		{
		    //Compact mode:

		    int start_x = 0;
		    COLOR bgcolor = win->color;
		    if( data->flags & SCROLLBAR_FLAG_COLORIZE ) 
			bgcolor = blend( wm->scroll_color, win->color, 16 );
		    if( data->bar_selected )
		    {
			bgcolor = blend( bgcolor, wm->scroll_pressed_color, wm->scroll_pressed_color_opacity );
		    }
		    data->but1->color = bgcolor;
		    data->but2->color = bgcolor;
		    draw_frect( 0, 0, win->xsize, win->ysize, bgcolor, wm );
		    
		    int ychar = char_y_size( wm );

		    int xc = 0;
		    if( data->max_value )
		    {
			xc = scrollbar_vconv( (float)data->cur / data->max_value, data->flags, true ) * data->working_area;
			//xc = ( data->working_area * ( ( data->cur << 10 ) / data->max_value ) ) >> 10;
		    }

		    COLOR fgcolor = wm->color2;
		    if( data->flags & SCROLLBAR_FLAG_COLORIZE )
		    {
			int c = 256;
			if( !wm->color3_is_brighter ) c = 80;
			fgcolor = blend( fgcolor, win->color, c );
		    }
		    if( data->bar_selected ) fgcolor = wm->color3;

		    wm->cur_opacity = 255;
		    if( data->but1->visible == 0 || data->bar_selected )
		    {
			//V.lines:
			COLOR lr_color = blend( fgcolor, bgcolor, 230 );
			//begin:
			if( data->but1->visible == 0 )
    			    draw_frect( start_x, 0, 1, win->ysize, lr_color, wm );
			//end:
			draw_frect( start_x + data->working_area - 1, 0, 1, win->ysize, lr_color, wm );
		    }
		    
		    //Value bar:
		    COLOR val_bar_color = blend( fgcolor, bgcolor, 170 );
		    draw_frect( start_x, 0, xc, win->ysize, val_bar_color, wm );
		    
		    if( data->bar_selected )
		    {
			//Show selection:
			draw_scrollbar_horizontal_selection( win, data->cur_x );
		    }

		    //Show normal value:
		    if( data->normal_value && data->normal_value < data->max_value )
		    {
			wm->cur_opacity = 255;
			int cx = scrollbar_vconv( (float)data->normal_value / data->max_value, data->flags, true ) * data->working_area;
			int grad_size = ( wm->scrollbar_size * 3 ) / 4;
			COLOR grad_color = blend( wm->color3, wm->red, 160 );
			draw_hgradient( cx - grad_size, 0, grad_size, win->ysize, grad_color, 0, 48, wm );
			draw_hgradient( cx, 0, grad_size, win->ysize, grad_color, 48, 0, wm );
			wm->cur_opacity = 52;
			draw_frect( cx, 0, 1, win->ysize, wm->color3, wm );
			wm->cur_opacity = 255;
		    }

		    COLOR name_color = wm->color3;
		    name_color = wm->color3;
		    if( data->bar_selected )
		    {
		    }
		    else
		    {
		        if( data->flags & SCROLLBAR_FLAG_COLORIZE ) 
		    	    name_color = blend( wm->color3, win->color, 60 );
		    }
		    wm->cur_opacity = 255;
		    wm->cur_font_color = name_color;
		    if( name_str ) draw_string( name_str, start_x, win->ysize / 2 - ychar, wm );

		    COLOR val_color = wm->color3;
		    if( data->bar_selected )
		    {
		    }
		    else
		    {
			val_color = wm->color2; //blend( wm->color2, wm->blue, 64 );
			if( data->flags & SCROLLBAR_FLAG_COLORIZE ) 
		    	    val_color = blend( name_color, val_bar_color, 100 );
		    }
		    wm->cur_opacity = 255;
		    wm->cur_font_color = val_color;
		    draw_string( data->value_str, start_x, win->ysize / 2 + 1, wm );
		    
		    if( data->flags & SCROLLBAR_FLAG_INPUT )
		    {
			wm->cur_font_color = bgcolor;
    			wm->cur_opacity = 200;
    			draw_string( STR_CORNER_LB, 0, win->ysize / 2 - char_y_size( wm ), wm );
    			draw_string( STR_CORNER_LT, 0, win->ysize / 2, wm );
    			wm->cur_opacity = 255;

			wm->cur_font_color = wm->color3;
    			wm->cur_opacity = 255;
    			draw_string( STR_CORNER_SMALL_LB, 0, win->ysize / 2 - char_y_size( wm ), wm );
    			draw_string( STR_CORNER_SMALL_LT, 0, win->ysize / 2, wm );
    			
    			draw_hgradient( 0, 0, win->ysize, win->ysize, wm->color3, 100, 0, wm );
		    }
		}
		
		wbd_draw( wm );
    		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void scrollbar_set_parameters( WINDOWPTR win, int cur, int max_value, int page_size, int step_size, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    if( cur != SCROLLBAR_DONT_CHANGE_VALUE ) data->cur = cur;
	    if( max_value >= 0 ) data->max_value = max_value;
	    if( page_size >= 0 ) data->page_size = page_size;
	    if( step_size >= 0 ) data->step_size = step_size;
	}
    }
}

void scrollbar_set_value( WINDOWPTR win, int val, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->cur = val;
	    if( data->cur < 0 ) data->cur = 0;
	    if( data->cur > data->max_value ) data->cur = data->max_value;
	}
    }
}

void scrollbar_set_value2( WINDOWPTR win, int val, window_manager* wm )
{
    if( scrollbar_get_editing_state( win ) == false )
        scrollbar_set_value( win, val, wm );
}

int scrollbar_get_value( WINDOWPTR win, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    return data->cur;
	}
    }
    return 0;
}

int scrollbar_get_evt_flags( WINDOWPTR win )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    return data->evt_flags;
	}
    }
    return 0;
}

int scrollbar_get_step( WINDOWPTR win )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    return data->step_size;
	}
    }
    return 0;
}

void scrollbar_set_name( WINDOWPTR win, const char* name, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    if( name == NULL )
	    {
		smem_free( data->name );
		data->name = NULL;
		return;
	    }
	    if( data->name == NULL )
	    {
		data->name = (char*)smem_new( smem_strlen( name ) + 1 );
	    }
	    else
	    {
		if( smem_strlen( name ) + 1 > smem_get_size( data->name ) )
		{
		    data->name = (char*)smem_resize( data->name, smem_strlen( name ) + 1 );
		}
	    }
	    smem_copy( data->name, name, smem_strlen( name ) + 1 );
	}
    }
}

void scrollbar_set_values( WINDOWPTR win, const char* values, char delimiter )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    if( values == 0 )
	    {
		smem_free( data->values );
		data->values = 0;
		return;
	    }
	    size_t len = smem_strlen( values );
	    if( data->values == 0 )
	    {
		data->values = (char*)smem_new( len + 1 );
	    }
	    else
	    {
		if( len + 1 > smem_get_size( data->values ) )
		{
		    data->values = (char*)smem_resize( data->values, len + 1 );
		}
	    }
	    smem_copy( data->values, values, len + 1 );
	    data->values_delim = delimiter;
	}
    }
}

void scrollbar_set_showing_offset( WINDOWPTR win, int offset, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->show_offset = offset;
	}
    }
}

void scrollbar_set_hex_mode( WINDOWPTR win, scrollbar_hex hex, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->hex = hex;
	}
    }
}

void scrollbar_set_normal_value( WINDOWPTR win, int normal_value, window_manager* wm )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->normal_value = normal_value;
	}
    }
}

void scrollbar_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    data->flags = flags;
	}
    }
}

uint32_t scrollbar_get_flags( WINDOWPTR win )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	if( data )
	{
	    return data->flags;
	}
    }
    return 0;
}

void scrollbar_call_handler( WINDOWPTR win )
{
    if( win == 0 ) return;
    window_manager* wm = win->wm;
    scrollbar_data* data = (scrollbar_data*)win->data;
    if( data->begin == 0 )
    {
	data->begin = 1;
	if( data->begin_handler )
	    data->begin_handler( data->begin_handler_data, win, scrollbar_get_value( win, wm ) );
    }
    if( win->action_handler )
	win->action_handler( win->handler_data, win, wm );
    if( data->begin )
    {
	data->evt_flags = 0;
	if( data->end_handler )
	    data->end_handler( data->end_handler_data, win, scrollbar_get_value( win, wm ) );
	data->begin = 0;
    }
}

bool scrollbar_get_editing_state( WINDOWPTR win )
{
    if( win )
    {
	scrollbar_data* data = (scrollbar_data*)win->data;
	return data->begin;
    }
    return 0;
}

//
//
//

struct resizer_data
{
    int8_t 		pushed; //1 - resizer; 2 - options
    int 		x;
    int 		y;
    int 		new_x;
    int 		new_y;
    int 		start_x;
    int 		start_y;
    int 		min_x;
    int 		min_y;
    uint32_t 		flags;
    win_action_handler_t	opt_handler;
    void* 			opt_handler_data;
    WINDOWPTR 		prev_focus_win;
};

static void get_resizer_ysize( WINDOWPTR win, resizer_data* data, window_manager* wm, int& opt_ysize, int& resizer_ysize )
{
    opt_ysize = 0;
    if( data->flags & RESIZER_FLAG_OPTBTN )
    {
        /*if( win->ysize > wm->scrollbar_size * 2.5 )
        {
    	    opt_ysize = wm->scrollbar_size;
	}*/
        if( win->ysize > wm->scrollbar_size * 1.5 )
        {
    	    opt_ysize = win->ysize - wm->scrollbar_size * 1.5;
    	    if( opt_ysize > wm->scrollbar_size ) opt_ysize = wm->scrollbar_size;
	}
        /*if( win->ysize > wm->scrollbar_size * 2 )
        {
    	    opt_ysize = win->ysize - wm->scrollbar_size * 2;
    	    opt_ysize *= 2;
    	    if( opt_ysize > wm->scrollbar_size ) opt_ysize = wm->scrollbar_size;
	}*/
    }
    resizer_ysize = win->ysize - opt_ysize;
}

int resizer_handler( sundog_event* evt, window_manager* wm )
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    resizer_data* data = (resizer_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( resizer_data );
	    break;
	case EVT_AFTERCREATE:
	    {
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		int opt_ysize;
		int resizer_ysize;
		get_resizer_ysize( win, data, wm, opt_ysize, resizer_ysize );

		int rx = evt->x - win->screen_x;
	        int ry = evt->y - win->screen_y;

		if( ry >= opt_ysize )
		    data->pushed = 1;
		else
		    data->pushed = 2; //options

		data->prev_focus_win = wm->prev_focus_win;
		data->start_x = evt->x;
		data->start_y = evt->y;
		data->x = win->xsize;
		data->y = win->ysize;
		data->new_x = data->x;
		data->new_y = data->y;
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT && data->pushed )
	    {
		if( data->pushed == 2 )
		{
		    int opt_ysize;
		    int resizer_ysize;
		    get_resizer_ysize( win, data, wm, opt_ysize, resizer_ysize );

		    int rx = evt->x - win->screen_x;
	    	    int ry = evt->y - win->screen_y;
	    	    
	    	    if( rx >= 0 && rx < win->xsize && ry >= 0 && ry < opt_ysize )
	    	    {
			if( data->opt_handler )
			    data->opt_handler( data->opt_handler_data, win, wm );
	    	    }
		}
		set_focus_win( data->prev_focus_win, wm );
		data->pushed = 0;
		data->x = data->new_x;
		data->y = data->new_y;
		draw_window( win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_UNFOCUS:
	    if( data->pushed )
	    {
		data->pushed = 0;
		data->x = data->new_x;
		data->y = data->new_y;
		draw_window( win, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT && data->pushed == 1 )
	    {
		int dx = data->start_x - evt->x;
		int dy = data->start_y - evt->y;
		if( data->flags & RESIZER_FLAG_TOPDOWN ) dy = -dy;
		data->new_x = data->x + dx;
		data->new_y = data->y + dy;
		if( data->new_x < data->min_x ) data->new_x = data->min_x;
		if( data->new_y < data->min_y ) data->new_y = data->min_y;
		if( win->action_handler )
		    win->action_handler( win->handler_data, win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
		
		int opt_ysize;
		int resizer_ysize;
		get_resizer_ysize( win, data, wm, opt_ysize, resizer_ysize );

		COLOR col;
		if( opt_ysize )
		{
		    col = blend( win->color, wm->color2, 8 );
		    if( data->pushed ) col = PUSHED_COLOR( col );
		    draw_frect( 0, 0, win->xsize, opt_ysize, col, wm );
		    int cxsize = char_x_size( '+', wm );
		    int cysize = char_y_size( wm );
		    wm->cur_font_color = wm->color2;
		    draw_char( '+', ( win->xsize - cxsize ) / 2, ( opt_ysize - cysize ) / 2, wm );
		}
		col = win->color;
		if( data->pushed == 1 ) col = PUSHED_COLOR( col );
		draw_frect( 0, opt_ysize, win->xsize, resizer_ysize, col, wm );
		draw_updown( win->xsize / 2, opt_ysize + resizer_ysize / 2, wm->color2, 0, wm );
		
		wbd_draw( wm );
		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void resizer_set_parameters( WINDOWPTR win, int min_x, int min_y )
{
    if( !win ) return;
    resizer_data* data = (resizer_data*)win->data;
    if( !data ) return;
    data->min_x = min_x;
    data->min_y = min_y;
}

void resizer_get_vals( WINDOWPTR win, int* x, int* y )
{
    if( !win ) return;
    resizer_data* data = (resizer_data*)win->data;
    if( !data ) return;
    if( x ) *x = data->new_x;
    if( y ) *y = data->new_y;
}

void resizer_set_opt_handler( WINDOWPTR win, win_action_handler_t handler, void* handler_data )
{
    if( !win ) return;
    resizer_data* data = (resizer_data*)win->data;
    if( !data ) return;
    data->opt_handler = handler;
    data->opt_handler_data = handler_data;
}

void resizer_set_flags( WINDOWPTR win, uint32_t flags )
{
    if( !win ) return;
    resizer_data* data = (resizer_data*)win->data;
    if( !data ) return;
    data->flags = flags;
}

uint32_t resizer_get_flags( WINDOWPTR win )
{
    if( !win ) return 0;
    resizer_data* data = (resizer_data*)win->data;
    if( !data ) return 0;
    return data->flags;
}
