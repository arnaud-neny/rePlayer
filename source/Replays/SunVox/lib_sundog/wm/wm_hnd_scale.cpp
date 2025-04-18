/*
    wm_hnd_scale.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2011 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

struct ui_scale_data
{
    WINDOWPTR win;
    WINDOWPTR ok;
    WINDOWPTR cancel;
    WINDOWPTR auto_win;
    WINDOWPTR ppi;
    WINDOWPTR scale;
    WINDOWPTR fscale;
    
    int content_ysize;

    bool auto_scale;
    int cur_ppi;
    int cur_scale;
    int cur_fscale;

    bool ui_scale_changed;
};

void ui_scale_reinit( WINDOWPTR win )
{
    window_manager* wm = win->wm;
    
    ui_scale_data* data = (ui_scale_data*)win->data;
    char ts[ 512 ];
    const char* v;

    scrollbar_set_parameters( data->ppi, data->cur_ppi, 600, 10, 1, wm );
    scrollbar_set_parameters( data->scale, data->cur_scale, 1024, 10, 1, wm );
    scrollbar_set_parameters( data->fscale, data->cur_fscale, 1024, 10, 1, wm );

    if( data->auto_scale )
    {
	v = wm_get_string( STR_WM_ON_CAP );
	data->ppi->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	data->scale->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	data->fscale->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
	hide_window( data->ppi );
	hide_window( data->scale );
	hide_window( data->fscale );
    }
    else
    {
	v = wm_get_string( STR_WM_OFF_CAP );
	data->ppi->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	data->scale->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	data->fscale->flags &= ~WIN_FLAG_ALWAYS_INVISIBLE;
	show_window( data->ppi );
	show_window( data->scale );
	show_window( data->fscale );
    }

    sprintf( ts, "%s = %s", wm_get_string( STR_WM_AUTO_SCALE ), v );
    button_set_text( data->auto_win, ts );
}

int ui_scale_ok_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_data* data = (ui_scale_data*)user_data;

    if( data->ui_scale_changed )
    {
	if( data->auto_scale )
	{
	    sconfig_remove_key( APP_CFG_PPI, 0 );
	    sconfig_remove_key( APP_CFG_SCALE, 0 );
	    sconfig_remove_key( APP_CFG_FONT_SCALE, 0 );
	}
	else
	{
	    int ppi = data->cur_ppi;
	    int scale = data->cur_scale;
	    int fscale = data->cur_fscale;
	    if( ppi < 10 ) ppi = 10;
	    if( scale < 10 ) scale = 10;
	    if( fscale < 10 ) fscale = 10;
	    sconfig_set_int_value( APP_CFG_PPI, ppi, 0 );
	    sconfig_set_int_value( APP_CFG_SCALE, scale, 0 );
	    sconfig_set_int_value( APP_CFG_FONT_SCALE, fscale, 0 );
	}
	sconfig_save( 0 );
	wm->prefs_restart_request = true;
    }
    
    remove_window( wm->ui_scale_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    return 0;
}

int ui_scale_cancel_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    remove_window( wm->ui_scale_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

int ui_scale_def_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_data* data = (ui_scale_data*)user_data;

    data->auto_scale ^= 1;
    ui_scale_reinit( data->win );
    recalc_regions( wm );
    draw_window( data->win, wm );
    data->ui_scale_changed = true;

    return 0;
}

int ui_scale_ppi_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_data* data = (ui_scale_data*)user_data;
    
    data->cur_ppi = scrollbar_get_value( win, wm );
    draw_window( data->win, wm );
    data->ui_scale_changed = true;
    
    return 0;
}

int ui_scale_scale_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_data* data = (ui_scale_data*)user_data;
    
    data->cur_scale = scrollbar_get_value( win, wm );
    if( data->cur_scale < 1 ) data->cur_scale = 1;
    draw_window( data->win, wm );
    data->ui_scale_changed = true;
    
    return 0;
}

int ui_scale_fscale_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_data* data = (ui_scale_data*)user_data;
    
    data->cur_fscale = scrollbar_get_value( win, wm );
    if( data->cur_fscale < 1 ) data->cur_fscale = 1;
    draw_window( data->win, wm );
    data->ui_scale_changed = true;
    
    return 0;
}

int ui_scale_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    ui_scale_data* data = (ui_scale_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( ui_scale_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		int y = wm->interelement_space;
		data->auto_win = new_window( wm_get_string( STR_WM_AUTO_SCALE ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
                set_handler( data->auto_win, ui_scale_def_handler, data, wm );
                set_window_controller( data->auto_win, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->auto_win, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
                y += wm->text_ysize + wm->interelement_space;

		wm->opt_scrollbar_compact = true;
                data->ppi = new_window( "PPI", 0, y, 1, wm->controller_ysize, wm->button_color, win, scrollbar_handler, wm );
                scrollbar_set_name( data->ppi, wm_get_string( STR_WM_PPI ), wm );
                set_handler( data->ppi, ui_scale_ppi_handler, data, wm );
                set_window_controller( data->ppi, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->ppi, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
                y += wm->controller_ysize + wm->interelement_space;

		wm->opt_scrollbar_compact = true;
                data->scale = new_window( "Scale", 0, y, 1, wm->controller_ysize, wm->button_color, win, scrollbar_handler, wm );
                scrollbar_set_name( data->scale, wm_get_string( STR_WM_BUTTON_SCALE ), wm );
                set_handler( data->scale, ui_scale_scale_handler, data, wm );
                set_window_controller( data->scale, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->scale, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
                y += wm->controller_ysize + wm->interelement_space;

		wm->opt_scrollbar_compact = true;
                data->fscale = new_window( "Font scale", 0, y, 1, wm->controller_ysize, wm->button_color, win, scrollbar_handler, wm );
                scrollbar_set_name( data->fscale, wm_get_string( STR_WM_FONT_SCALE ), wm );
                set_handler( data->fscale, ui_scale_fscale_handler, data, wm );
                set_window_controller( data->fscale, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->fscale, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
                y += wm->controller_ysize + wm->interelement_space;

                data->content_ysize = y;

		data->cur_ppi = wm->screen_ppi * wm->screen_zoom;
		data->cur_scale = (int)( wm->screen_scale * 256 );
		data->cur_fscale = (int)( wm->screen_font_scale * 256 );
		if( sconfig_get_int_value( APP_CFG_PPI, 0, 0 ) == 0 && sconfig_get_int_value( APP_CFG_SCALE, 0, 0 ) == 0 )
		    data->auto_scale = 1;
		else
		    data->auto_scale = 0;
		ui_scale_reinit( win );

    		data->ui_scale_changed = false;

		data->ok = new_window( wm_get_string( STR_WM_OK ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
		data->cancel = new_window( wm_get_string( STR_WM_CANCEL ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
		set_handler( data->ok, ui_scale_ok_handler, data, wm );
		set_handler( data->cancel, ui_scale_cancel_handler, data, wm );
		int x = wm->interelement_space;
		set_window_controller( data->ok, 0, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		x += wm->button_xsize;
		set_window_controller( data->ok, 2, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
		x += 1;
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
		
		if( data->auto_scale == 0 )
		{
		    int i = char_y_size( wm ) + 2;
		    int y = data->content_ysize;
		    
		    int ppi = data->cur_ppi;
		    if( wm->screen_zoom > 1 ) ppi /= wm->screen_zoom;

		    /*wm->cur_opacity = 100;
		    int xs = ( ppi * data->cur_scale ) / 256;
		    draw_frect( ( win->xsize - xs ) / 2, data->content_ysize, xs, i * 2, wm->color2, wm );
		    wm->cur_opacity = 255;*/
		    
		    draw_frect( ( win->xsize - ppi ) / 2, data->content_ysize, ppi, i, wm->color2, wm );
		    wm->cur_font_color = win->color;
		    draw_string( wm_get_string( STR_WM_INCH ), ( win->xsize - string_x_size( wm_get_string( STR_WM_INCH ), wm ) ) / 2, data->content_ysize + ( i - char_y_size( wm ) ) / 2, wm );
		    y += i + wm->interelement_space;

		    wm->cur_font_color = wm->color3;

		    int new_font_zoom = win_calc_font_zoom( ppi, wm->screen_zoom, (float)data->cur_scale / 256, (float)data->cur_fscale / 256 );
		    int cur_font_zoom = wm->font_zoom;
		    wm->cur_font_scale = ( new_font_zoom * 256 ) / cur_font_zoom;
		    int bysize = (int)( (float)ppi * (float)TEXT_YSIZE_COEFF * ( (float)data->cur_scale / 256 ) );
		    if( bysize < char_y_size( wm ) + 2 ) bysize = char_y_size( wm ) + 2;
		    int str_xsize = string_x_size( wm_get_string( STR_WM_BUTTON ), wm );
		    int bsize = (int)( (float)ppi * (float)SCROLLBAR_SIZE_COEFF * ( (float)data->cur_scale / 256 ) );
		    int bxsize = str_xsize + bsize;
		    draw_frect( ( win->xsize - bxsize ) / 2, y, bxsize, bysize, wm->button_color, wm );
		    draw_rect( ( win->xsize - bxsize ) / 2, y, bxsize - 1, bysize - 1, BORDER_COLOR( wm->button_color ), wm );
		    draw_string( wm_get_string( STR_WM_BUTTON ), ( win->xsize - str_xsize ) / 2, y + ( bysize - char_y_size( wm ) ) / 2, wm );
		    wm->cur_font_scale = 256;
		}
		
		wbd_draw( wm );
		wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
            if( evt->key == KEY_ESCAPE )
            {
        	send_event( win, EVT_CLOSEREQUEST, wm );
                retval = 1;
            }
            break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_CLOSEREQUEST:
            {
        	ui_scale_cancel_handler( data, NULL, wm );
            }
            retval = 1;
            break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    wm->ui_scale_win = 0;
	    break;
    }
    return retval;
}
