/*
    wm_hnd_popup.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#define POPUP_MAX_LINES			256
#define POPUP_MAX_COLUMNS		16
#define POPUP_FULL_COLUMN_XSIZE( XSIZE )	( XSIZE + wm->interelement_space2 )
#define POPUP_MAX_XSIZE_WITHOUT_SCROLL		( win->parent->xsize + wm->small_button_xsize )
#define POPUP_ITEM_FLAG_SHADED		( 1 << 0 ) // ...@
#define POPUP_ITEM_FLAG_BOLD		( 1 << 1 ) // ...@@
#define POPUP_ITEM_FLAG_TWO_SECTIONS	( 1 << 2 ) // ...|...

struct popup_data
{
    WINDOWPTR 	win;
    uint32_t 	flags;
    int 	border_size;
    bool	with_xscroll; //1 = xscroll is possible
    bool	xscroll_mode;
    int		xoffset;
    char* 	text;
    int16_t 	lines[ POPUP_MAX_LINES ];
    uint8_t 	line_flags[ POPUP_MAX_LINES ];
    int 	lines_num;
    int 	first_section_size; //for POPUP_ITEM_FLAG_TWO_SECTIONS
    int 	cols; //columns
    int16_t 	col_xsize[ POPUP_MAX_COLUMNS ];
    int16_t 	col_ysize[ POPUP_MAX_COLUMNS ];
    int		col_lines; //lines (items without menu caption) per column
    int 	current_selected;
    int 	prev_selected;
    int 	click_x;
    int		click_xoffset;
    bool	dont_close_on_unfocus;
};

static void popup_finish( popup_data* data )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    win->action_result = data->current_selected;
    int id = win->id;
    uint32_t flags = data->flags;
    bool close = 0;
    if( win->action_handler )
    {
	if( flags & POPUP_DIALOG_FLAG_DONT_AUTOCLOSE )
	    data->dont_close_on_unfocus = 1;
        if( win->action_handler( win->handler_data, win, wm ) == 1 )
    	    close = 1;
    }
    else
	close = 1;
    if( ( flags & POPUP_DIALOG_FLAG_DONT_AUTOCLOSE ) == 0 )
	close = 1;
    if( close )
    {
	if( ( win->flags & WIN_FLAG_TRASH ) == 0 && win->id == id )
	{
	    remove_window( win, wm );
            recalc_regions( wm );
            draw_window( wm->root_win, wm );
	}
    }
}

static void popup_get_item_pos( popup_data* data, int i, int* x, int* y, int* xsize, int* ysize )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    if( i < 0 || i >= data->lines_num ) return;
    int ii = 0;
    int xx = 0;
    int xoff = data->border_size + data->xoffset;
    int yoff = data->border_size;
    for( int c = 0; c < data->cols; c++ )
    {
	if( i >= ii && i < ii + data->col_lines )
	{
	    if( x ) *x = xx + xoff;
	    if( y ) *y = ( i % data->col_lines ) * wm->list_item_ysize + yoff;
	    if( xsize ) *xsize = data->col_xsize[ c ];
	    if( ysize ) *ysize = wm->list_item_ysize;
	    break;
	}
	ii += data->col_lines;
	xx += POPUP_FULL_COLUMN_XSIZE( data->col_xsize[ c ] );
    }
}

static void popup_check_scroll( popup_data* data )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    if( win->xsize > POPUP_MAX_XSIZE_WITHOUT_SCROLL )
    {
	data->with_xscroll = true;
    }
    else
    {
	data->with_xscroll = false;
	data->xoffset = 0;
    }
}

static void popup_make_item_visible( popup_data* data, int i )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    popup_check_scroll( data );
    if( data->with_xscroll == false ) return;
    int x = 0;
    int y = 0;
    int xsize = 0;
    int ysize = 0;
    popup_get_item_pos( data, i, &x, &y, &xsize, &ysize );
    if( xsize == 0 ) return;
    if( x < 0 )
	data->xoffset += -x;
    if( x + xsize > POPUP_MAX_XSIZE_WITHOUT_SCROLL )
	data->xoffset -= ( x + xsize ) - win->parent->xsize;
}

void clear_popup_dialog_constructor_options( window_manager* wm )
{
    wm->opt_popup_text = NULL;
}

int popup_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    popup_data* data = (popup_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( popup_data );
	    break;

	case EVT_AFTERCREATE:
	    {
		//Set focus:
		dialog_stack_add( win );

		//Data init:
		data->win = win;
	    	data->text = NULL;
		data->current_selected = -1;
		data->prev_selected = -1;
		data->first_section_size = 0;
		data->border_size = MAX_NUM( 2, wm->interelement_space );
		win->action_result = -1;
		smem_clear( data->line_flags, sizeof( data->line_flags ) );

		//Set window size:
		int xsize = 0;
		int ysize = 0;
		int x = win->x;
		int y = win->y;
		int name_xsize = 0;
		int name_ysize = 0;
		if( win->name && win->name[ 0 ] != 0 )
		{
		    name_xsize = font_string_x_size( win->name, win->font, wm ) + 1;
		    name_ysize = wm->list_item_ysize;
		}
		data->cols = 1;
		data->col_xsize[ 0 ] = 0;
		data->col_ysize[ 0 ] = name_ysize;
		data->col_lines = 0;
		if( wm->opt_popup_text )
		{
		    //Text popup:
		    data->text = (char*)smem_strdup( wm->opt_popup_text );
		    data->lines_num = 0;
		    data->lines[ 0 ] = 0;
		    int nl_code = 0;
		    int text_size = smem_strlen( data->text );
		    int line_start = 0;
		    for( int i = 0; i <= text_size; i++ )
		    {
			char c = data->text[ i ];
			if( c == '|' )
			{
			    int ss = font_string_x_size_limited( data->text + line_start, i - line_start + 1, win->font, wm );
			    if( ss > data->first_section_size )
				data->first_section_size = ss;
			    data->line_flags[ data->lines_num ] |= POPUP_ITEM_FLAG_TWO_SECTIONS; 
			}
			if( c == 0xA || c == 0 )
			{
			    data->text[ i ] = 0;
			    if( i - 1 >= 0 && data->text[ i - 1 ] == '@' )
			    {
				data->text[ i - 1 ] = 0; 
				data->line_flags[ data->lines_num ] |= POPUP_ITEM_FLAG_SHADED;
			    }
			    if( i - 2 >= 0 && data->text[ i - 2 ] == '@' )
			    {
				data->text[ i - 2 ] = 0; 
				data->line_flags[ data->lines_num ] |= POPUP_ITEM_FLAG_BOLD;
			    }
			    data->lines[ data->lines_num ] = line_start; //Save the start of this line
			    //Set X size:
			    int cur_x_size = font_string_x_size( data->text + line_start, win->font, wm );
                            if( cur_x_size > data->col_xsize[ data->cols - 1 ] ) data->col_xsize[ data->cols - 1 ] = cur_x_size + 1;
                            //Set Y size:
			    data->col_ysize[ data->cols - 1 ] += wm->list_item_ysize;
			    if( data->col_ysize[ data->cols - 1 ] > win->parent->ysize - data->border_size * 2 - wm->list_item_ysize )
                            {
                                data->col_xsize[ data->cols ] = 0;
                                data->col_ysize[ data->cols ] = name_ysize;
                                data->cols++;
                                if( data->cols > POPUP_MAX_COLUMNS )
                                {
                            	    slog( "Popup menu: too many columns!\n" );
                            	    data->cols = POPUP_MAX_COLUMNS;
                                }
                            }
			    //Go to the next string:
			    while( 1 )
			    {
				i++;
				if( i >= text_size + 1 ) break;
				line_start = i;
				if( data->text[ i ] != 0xA && data->text[ i ] != 0xD && data->text[ i ] != 0 && data->text[ i ] != '@' )
				    break;
			    }
			    data->lines_num++;
			    if( data->lines_num >= POPUP_MAX_LINES )
				break;
			}
		    }
		    for( int i = 0; i < data->cols; i++ ) 
		    {
			xsize += data->col_xsize[ i ];
		    }
		    if( xsize < name_xsize ) 
		    {
			data->col_xsize[ data->cols - 1 ] += name_xsize - xsize;
			xsize = name_xsize;
		    }
		    xsize += ( data->cols - 1 );
		    ysize += data->col_ysize[ 0 ];
		    data->col_lines = ( data->col_ysize[ 0 ] - name_ysize ) / wm->list_item_ysize;
		}
		xsize += data->border_size * 2;
		ysize += data->border_size * 2;
		if( xsize < wm->scrollbar_size * 4 && data->cols == 1 )
		{
		    xsize = wm->scrollbar_size * 4;
		    data->col_xsize[ 0 ] = xsize;
		}

		//Control window position:
		if( x + xsize > win->parent->xsize && x > 0 ) x -= ( x + xsize ) - win->parent->xsize;
		if( y + ysize > win->parent->ysize && y > 0 ) y -= ( y + ysize ) - win->parent->ysize;
		if( x < 0 ) x = 0;
		if( y < 0 ) y = 0;

		win->x = x;
		win->y = y;
		win->xsize = xsize;
		win->ysize = ysize;

		clear_popup_dialog_constructor_options( wm );

		retval = 1;
	    }
	    break;

	case EVT_BEFORECLOSE:
	    smem_free( data->text );
	    data->dont_close_on_unfocus = 1;
	    dialog_stack_del( win );
	    retval = 1;
	    break;

	case EVT_MOUSEBUTTONDOWN:
	    data->click_x = rx;
	    data->click_xoffset = data->xoffset;
    	    data->xscroll_mode = false;
	case EVT_MOUSEMOVE:
	    {
		if( evt->key == MOUSE_BUTTON_LEFT )
		{
		    if( data->with_xscroll && !data->xscroll_mode )
		    {
			if( abs( rx - data->click_x ) > wm->scrollbar_size/2 )
			{
			    data->xscroll_mode = true;
			}
		    }
		}

		if( data->xscroll_mode )
		{
		    data->xoffset = data->click_xoffset + ( rx - data->click_x );
		    draw_window( win, wm );
		    retval = 1;
		    break;
		}

		int prev_sel = data->current_selected;
		data->current_selected = -1;
		int cur_y = data->border_size;
		int name_ysize = 0;
		if( win->name && win->name[ 0 ] != 0 )
		{
		    name_ysize = wm->list_item_ysize;
		}
		cur_y += name_ysize;
		if( data->text && data->lines_num > 0 )
		{
		    int col_x = data->border_size + data->xoffset;
		    int line = 0;
		    for( int i = 0; i < data->cols; i++ )
		    {
			int xsize = data->col_xsize[ i ];
			int lines = ( win->ysize - name_ysize - data->border_size * 2 ) / wm->list_item_ysize;
			if( line + lines > data->lines_num ) lines = data->lines_num - line;
			if( rx >= col_x && rx < col_x + data->col_xsize[ i ] )
			{
			    if( ry >= cur_y && ry < cur_y + lines * wm->list_item_ysize )
			    {
				data->current_selected = line + ( ry - cur_y ) / wm->list_item_ysize;
				break;
			    }
			}
			col_x += POPUP_FULL_COLUMN_XSIZE( xsize );
			line += lines;
		    }
		}
		if( prev_sel != data->current_selected )
		    draw_window( win, wm );
	    }
	    retval = 1;
	    break;

	case EVT_MOUSEBUTTONUP:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( data->xscroll_mode )
		{
		    data->xscroll_mode = false;
		    retval = 1;
		    break;
		}

		if( data->current_selected >= 0 )
		{
		    //Successful selection:
		}
		else
		{
		    data->current_selected = -1;
		}
		popup_finish( data );
	    }
	    retval = 1;
	    break;

	case EVT_BUTTONDOWN:
            {
        	bool redraw = false;
        	if( evt->key == KEY_ESCAPE )
        	{
		    data->current_selected = -1;
		    popup_finish( data );
        	    retval = 1;
        	}
        	if( evt->key == KEY_ENTER || evt->key == KEY_SPACE )
        	{
		    popup_finish( data );
        	    retval = 1;
        	}
        	if( evt->key == KEY_UP )
        	{
		    if( data->current_selected > 0 ) data->current_selected--;
		    redraw = true;
        	    retval = 1;
        	}
        	if( evt->key == KEY_DOWN )
        	{
		    if( data->current_selected < data->lines_num - 1 ) data->current_selected++;
		    redraw = true;
        	    retval = 1;
        	}
        	if( evt->key == KEY_LEFT )
        	{
        	    data->current_selected -= data->col_lines;
        	    if( data->current_selected < 0 ) data->current_selected = 0;
		    redraw = true;
        	    retval = 1;
        	}
        	if( evt->key == KEY_RIGHT )
        	{
        	    data->current_selected += data->col_lines;
        	    if( data->current_selected >= data->lines_num ) data->current_selected = data->lines_num - 1;
		    redraw = true;
        	    retval = 1;
        	}
        	if( redraw )
        	{
		    popup_make_item_visible( data, data->current_selected );
        	    draw_window( win, wm );
        	}
            }
            break;

	case EVT_UNFOCUS:
	    if( data->dont_close_on_unfocus ) break;
	    remove_window( win, wm );
	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );
	    retval = 1;
	    break;

	case EVT_DRAW:
	    popup_check_scroll( data );
	    wbd_lock( win );
	    wm->cur_opacity = 255;
	    {
		int cur_y = data->border_size;
		int cur_x = data->border_size;
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );

		//Draw name:
		int name_ysize = 0;
		if( win->name && win->name[ 0 ] != 0 )
		{
		    name_ysize = wm->list_item_ysize;
		    wm->cur_font_color = wm->header_text_color;
		    draw_string( 
			win->name, 
			cur_x, 
			cur_y + ( wm->list_item_ysize - char_y_size( wm ) ) / 2, 
			wm );
		    cur_y += name_ysize;
		}

		//Draw text:
		if( data->text && data->lines_num > 0 )
		{
		    cur_x += data->xoffset;
		    int col = 0;
		    int xsize = data->col_xsize[ col ];
		    int ysize = data->col_ysize[ col ];
		    int y = 0;
		    for( int i = 0; i < data->lines_num; i++ )
		    {
			char* text = data->text + data->lines[ i ];
			int text_y = cur_y + ( wm->list_item_ysize - char_y_size( wm ) ) / 2;
			COLOR bg_color = win->color;
			if( i == data->current_selected )
			{
			    bg_color = blend( wm->color2, wm->color1, 32 );
			    wm->cur_font_color = wm->color0;
			}
			else
			{
			    if( y & 1 )
				bg_color = blend( bg_color, wm->color3, 16 );
			    if( data->line_flags[ i ] & POPUP_ITEM_FLAG_SHADED )
				bg_color = blend( bg_color, wm->color2, 90 );
			    wm->cur_font_color = wm->color3;
			    if( i == data->prev_selected )
				wm->cur_font_color = wm->alternative_text_color;
			}
			if( bg_color != win->color )
			{
			    draw_frect( 
				cur_x, 
				cur_y, 
				xsize, 
				wm->list_item_ysize, 
				bg_color,
				wm );
			}
			if( data->line_flags[ i ] & POPUP_ITEM_FLAG_TWO_SECTIONS )
			{
			    int p = 0;
			    while( 1 )
			    {
				char c = text[ p ];
				if( c == '|' )
				{
				    draw_string_limited( text, cur_x, text_y, p, wm );
				    draw_string( text + p + 1, cur_x + data->first_section_size, text_y, wm );
				    break;
				}
				p++;
			    }
			    wm->cur_opacity = 32;
			    draw_frect( 
				cur_x, 
				cur_y, 
				data->first_section_size, 
				wm->list_item_ysize, 
				wm->color3,
				wm );
			    wm->cur_opacity = 255;
			}
			else
			{
			    draw_string( text, cur_x, text_y, wm );
			}
			cur_y += wm->list_item_ysize;
			y++;
			if( cur_y - data->border_size >= ysize )
			{
			    //Go to the next column:
			    y = 0;
			    cur_y = data->border_size + name_ysize;
			    cur_x += POPUP_FULL_COLUMN_XSIZE( xsize );
			    col++;
			    xsize = data->col_xsize[ col ];
			}
		    }
		}

		draw_rect( 0, 0, win->xsize - 1, win->ysize - 1, BORDER_COLOR( win->color ), wm );

		retval = 1;
	    }
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    break;

	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
    }
    return retval;
}

void popup_set_prev( WINDOWPTR win, int v )
{
    if( !win ) return;
    if( !win->data ) return;
    if( smem_get_size( win->data ) != sizeof( popup_data ) ) return;
    popup_data* data = (popup_data*)win->data;
    data->prev_selected = v;
    popup_make_item_visible( data, v );
}
