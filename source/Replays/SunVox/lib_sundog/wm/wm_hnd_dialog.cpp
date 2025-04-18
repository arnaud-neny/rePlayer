/*
    wm_hnd_dialog.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "dsp.h"

#define MAX_USER_PARS	8

#define DIALOG_ITEM_EMPTY_LINE_SIZE     wm->interelement_space
#define DIALOG_ITEM_TEXT_SIZE           wm->text_ysize
#define DIALOG_ITEM_NUMBER_SIZE         wm->text_with_label_ysize
#define DIALOG_ITEM_SLIDER_SIZE         wm->controller_ysize
#define DIALOG_ITEM_LABEL_SIZE          font_char_y_size( win->font, wm )
#define DIALOG_ITEM_POPUP_SIZE          wm->popup_button_ysize
#define DIALOG_ITEM_CHECKBOX_SIZE       DIALOG_ITEM_TEXT_SIZE

struct dialog_data
{
    WINDOWPTR win;
    char* text;
    char** buttons_text;
    WINDOWPTR* buttons;
    int buttons_num;
    dialog_item* items;
    bool items_created;
    int timer;
    stime_ms_t timer_start;
    COLOR base_color;
    WINDOWPTR focus_on_item;
    uint32_t flags;
    int user_pars[ MAX_USER_PARS ]; //some user-defined parameters of the dialog box
};

static WINDOWPTR get_dialog_win( WINDOWPTR win ) //win = dialog handler or decorator with dialog
{
    if( !win ) return NULL;
    if( win->win_handler != dialog_handler )
    {
	//win is decorator?
	if( win->childs )
	{
	    win = win->childs[ 0 ];
	    if( win->win_handler != dialog_handler ) return NULL;
	}
	else
	    return NULL;
    }
    return win;
}

int dialog_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_data* data = (dialog_data*)user_data;
    int button_num = -1;
    for( int i = 0; i < data->buttons_num; i++ )
    {
        if( data->buttons[ i ] == win )
        {
    	    button_num = i;
	    break;
	}
    }
    bool close = 0;
    WINDOWPTR dwin = data->win;
    int id = data->win->id;
    dwin->action_result = button_num;
    if( dwin->action_handler )
    {
	if( dwin->action_handler( dwin->handler_data, dwin, wm ) == 1 )
	    close = 1;
    }
    else
    {
	close = 1;
    }
    if( close )
    {
	if( ( dwin->flags & WIN_FLAG_TRASH ) == 0 && dwin->id == id )
	{
	    remove_window( dwin, wm );
    	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );
	}
    }
    return 0;
}

int dialog_item_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_data* data = (dialog_data*)user_data;
    if( data->items )
    {
	int i = 0;
	while( 1 )
	{
	    dialog_item* item = &data->items[ i ];

	    if( item->type == DIALOG_ITEM_NONE )
		break;

	    if( item->win == win )
	    {
		bool changed = 0;
		switch( item->type )
		{
    		    case DIALOG_ITEM_EMPTY_LINE:
    			break;
		    case DIALOG_ITEM_NUMBER:
		    case DIALOG_ITEM_NUMBER_HEX:
			{
			    int act = text_get_last_action( win );
			    if( act == TEXT_ACTION_CHANGED )
			    {
				int v = text_get_value( win, wm );
				if( item->int_val != v )
				{
				    item->int_val = v;
				    changed = 1;
				}
			    }
			}
			break;
		    case DIALOG_ITEM_NUMBER_FLOAT:
		    case DIALOG_ITEM_NUMBER_TIME:
			{
			    int act = text_get_last_action( win );
			    if( act == TEXT_ACTION_CHANGED )
			    {
				double v = text_get_fvalue( win );
				if( item->float_val != v )
				{
				    item->float_val = v;
				    changed = 1;
				}
			    }
			}
			break;
		    case DIALOG_ITEM_SLIDER:
			{
			    int v = scrollbar_get_value( win, wm ) + item->min;
			    if( item->int_val != v )
			    {
				item->int_val = v;
				changed = 1;
			    }
			}
			break;
		    case DIALOG_ITEM_TEXT:
			{
			    int act = text_get_last_action( win );
			    if( act == TEXT_ACTION_CHANGED )
			    {
				smem_free( item->str_val );
				item->str_val = smem_strdup( text_get_text( win, wm ) );
				changed = 1;
			    }
			}
			break;
		    case DIALOG_ITEM_POPUP:
			{
			    int v = button_get_menu_val( win );
			    if( item->int_val != v )
			    {
				item->int_val = v;
				changed = 1;
			    }
			}
			break;
		    case DIALOG_ITEM_CHECKBOX:
			item->int_val = ( item->int_val ^ 1 ) & 1;
			if( item->int_val )
			    win->color = BUTTON_HIGHLIGHT_COLOR;
			else
			    win->color = wm->button_color;
			changed = 1;
			draw_window( win, wm );
			break;
		}
		if( ( item->flags & DIALOG_ITEM_FLAG_CALL_HANDLER_ON_ANY_CHANGES ) && changed )
		{
		    if( data->win->action_handler )
		    {
			data->win->action_result = 1000 + i;
		        data->win->action_handler( data->win->handler_data, data->win, wm );
		    }
		}
		break;
	    }

	    i++;
	}
    }
    return 0;
}

void dialog_timer( void* user_data, sundog_timer* timer, window_manager* wm )
{
    dialog_data* data = (dialog_data*)user_data;
    stime_ms_t t = stime_ms() - data->timer_start;
    t = ( t * 256 ) / 1000 + 256 + 128;
    int v = g_hsin_tab[ t & 255 ];
    if( t & 256 ) v = -v;
    v = v / 2 + 128;
    v /= 2;
    data->win->color = blend( data->base_color, wm->color2, v );
    for( int i = 0; i < data->buttons_num; i++ )
    {
	data->buttons[ i ]->color = blend( wm->button_color, wm->color2, v );
    }
    draw_window( data->win->parent, wm );
}

void clear_dialog_constructor_options( window_manager* wm )
{
    wm->opt_dialog_items = NULL;
    wm->opt_dialog_buttons_text = NULL;
    wm->opt_dialog_text = NULL;
}

int dialog_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    dialog_data* data = (dialog_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( dialog_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		data->text = smem_strdup( wm->opt_dialog_text );
		data->items = wm->opt_dialog_items;
		data->items_created = false;
		data->focus_on_item = NULL;

		int but_ysize = wm->button_ysize;
	        int but_xsize = wm->button_xsize;

		data->timer = -1;
		if( data->text && data->text[ 0 ] == '!' )
		{
		    char* new_text = smem_strdup( data->text + 1 );
		    smem_free( data->text );
		    data->text = new_text;
		    data->timer = add_timer( dialog_timer, data, 0, wm );
		    data->timer_start = stime_ms();
		    data->base_color = win->color;
		}

		dialog_reinit_items( win, false );

		data->buttons_text = NULL;
		if( wm->opt_dialog_buttons_text )
		{
		    int buttons_num = 1;
		    int i, p, len;
		    int slen = smem_strlen( wm->opt_dialog_buttons_text );
		    for( i = 0; i < slen; i++ )
		    {
			if( wm->opt_dialog_buttons_text[ i ] == ';' )
			    buttons_num++;
		    }
		    data->buttons_text = (char**)smem_new( sizeof( char* ) * buttons_num );
		    data->buttons = (WINDOWPTR*)smem_new( sizeof( WINDOWPTR ) * buttons_num );
		    data->buttons_num = buttons_num;
		    smem_clear( data->buttons_text, sizeof( char* ) * buttons_num );
		    int word_start = 0;
		    for( i = 0; i < buttons_num; i++ )
		    {
			len = 0;
			while( 1 )
			{
			    if( wm->opt_dialog_buttons_text[ word_start + len ] == 0 || wm->opt_dialog_buttons_text[ word_start + len ] == ';' )
			    {
				break;
			    }
			    len++;
			}
			if( len > 0 )
			{
			    data->buttons_text[ i ] = (char*)smem_new( len + 1 );
			    for( p = word_start; p < word_start + len; p++ )
			    {
				data->buttons_text[ i ][ p - word_start ] = wm->opt_dialog_buttons_text[ p ];
			    }
			    data->buttons_text[ i ][ p - word_start ] = 0;
			}
			word_start += len + 1;
			if( word_start >= slen ) break;
		    }

		    //Create buttons:
		    int xoff = wm->interelement_space;
		    for( i = 0; i < buttons_num; i++ )
		    {
		        but_xsize = button_get_optimal_xsize( data->buttons_text[ i ], win->font, false, wm );
			xoff += but_xsize + wm->interelement_space2;
		    }
		    bool smallest = false;
		    int xsize = win->xsize;
		    if( xsize > wm->desktop_xsize )
			xsize = wm->desktop_xsize;
		    if( xoff >= xsize ) 
			smallest = true;
		    xoff = wm->interelement_space;
		    for( i = 0; i < buttons_num; i++ )
		    {
		        but_xsize = button_get_optimal_xsize( data->buttons_text[ i ], win->font, smallest, wm );
			data->buttons[ i ] = new_window( data->buttons_text[ i ], xoff, 0, but_xsize, 1, wm->button_color, win, button_handler, wm );
			set_handler( data->buttons[ i ], dialog_button_handler, data, wm );
			set_window_controller( data->buttons[ i ], 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
			set_window_controller( data->buttons[ i ], 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
			xoff += but_xsize + wm->interelement_space2;
		    }
		}

		clear_dialog_constructor_options( wm );
	    }
	    retval = 1;
	    break;
	case EVT_BEFORESHOW:
	    dialog_stack_add( win );
	    if( data->focus_on_item ) set_focus_win( data->focus_on_item, wm );
	    retval = 1;
	    break;
	case EVT_BEFOREHIDE:
	    dialog_stack_del( win );
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    if( data->buttons_text )
	    {
		for( int i = 0; i < data->buttons_num; i++ )
		{
		    if( data->buttons_text[ i ] )
		    {
			smem_free( data->buttons_text[ i ] );
		    }
		}
		smem_free( data->buttons_text );
	    }
	    smem_free( data->buttons );
	    smem_free( data->text );
	    if( data->items )
	    {
	        int i = 0;
		while( 1 )
		{
		    dialog_item* item = &data->items[ i ];
    		    if( item->type == DIALOG_ITEM_NONE )
			break;
		    if( item->flags & DIALOG_ITEM_FLAG_STR_AUTOREMOVE )
		    {
        	        smem_free( item->str_val );
        	        item->str_val = nullptr;
        	    }
    		    i++;
		}
	    }
	    if( data->flags & DIALOG_FLAG_AUTOREMOVE_ITEMS )
	    {
		smem_free( data->items );
	    }
	    remove_timer( data->timer, wm );
	    dialog_stack_del( win );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    wbd_lock( win );
	    wm->cur_font_color = wm->color3;
    	    draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
	    if( data->text )
	    {
	        draw_string_wordwrap( data->text, wm->interelement_space, wm->interelement_space, win->xsize - wm->interelement_space * 2, 0, 0, 0, wm );
	    }
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    if( evt->key == KEY_ESCAPE )
	    {
		dialog_button_handler( data, NULL, wm );
		retval = 1;
		break;
	    }
	    if( data->buttons )
	    {
		if( evt->key == 'y' || evt->key == KEY_ENTER )
		{
		    if( data->buttons_num >= 1 )
		    {
			dialog_button_handler( data, data->buttons[ 0 ], wm );
			retval = 1;
			break;
		    }
		}
		if( evt->key == 'n' )
		{
		    if( data->buttons_num >= 2 )
		    {
			dialog_button_handler( data, data->buttons[ data->buttons_num - 1 ], wm );
			retval = 1;
			break;
		    }
		}
		if( evt->key >= '1' && evt->key <= '9' )
		{
		    int n = evt->key - '1';
		    if( n < data->buttons_num )
		    {
			dialog_button_handler( data, data->buttons[ n ], wm );
			retval = 1;
			break;
		    }
		}
	    }
	    break;
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

int dialog_get_item_ysize( int type, WINDOWPTR pwin ) //pwin = dialog handler or decorator with dialog
{
    WINDOWPTR win = get_dialog_win( pwin );
    if( !win ) return 0;
    window_manager* wm = win->wm;
    int rv = 0;
    switch( type )
    {
        case DIALOG_ITEM_EMPTY_LINE:
    	    rv = DIALOG_ITEM_EMPTY_LINE_SIZE;
    	    break;
        case DIALOG_ITEM_NUMBER:
        case DIALOG_ITEM_NUMBER_HEX:
        case DIALOG_ITEM_NUMBER_FLOAT:
        case DIALOG_ITEM_NUMBER_TIME:
    	    rv = DIALOG_ITEM_NUMBER_SIZE;
            break;
        case DIALOG_ITEM_SLIDER:
            rv = DIALOG_ITEM_SLIDER_SIZE;
            break;
        case DIALOG_ITEM_TEXT:
            rv = DIALOG_ITEM_TEXT_SIZE;
            break;
        case DIALOG_ITEM_LABEL:
            rv = DIALOG_ITEM_LABEL_SIZE;
            break;
        case DIALOG_ITEM_POPUP:
            rv = DIALOG_ITEM_POPUP_SIZE;
            break;
        case DIALOG_ITEM_CHECKBOX:
            rv = DIALOG_ITEM_CHECKBOX_SIZE;
            break;
        default: break;
    }
    return rv;
}

void dialog_reinit_item( dialog_item* item ) //change item properties
{
    WINDOWPTR dwin = item->dwin;
    window_manager* wm = dwin->wm;
    dialog_data* data = (dialog_data*)dwin->data;
    WINDOWPTR w = item->win;
    if( !w ) return;
    switch( item->type )
    {
	case DIALOG_ITEM_EMPTY_LINE:
    	    break;
	case DIALOG_ITEM_NUMBER:
	case DIALOG_ITEM_NUMBER_HEX:
	case DIALOG_ITEM_NUMBER_FLOAT:
	case DIALOG_ITEM_NUMBER_TIME:
	    if( item->str_val )
		text_set_label( w, item->str_val );
	    text_set_range( w, item->min, item->max );
	    if( item->type >= DIALOG_ITEM_NUMBER_FLOAT )
	        text_set_fvalue( w, item->float_val );
	    else
	        text_set_value( w, item->int_val, wm );
	    break;
	case DIALOG_ITEM_SLIDER:
	    scrollbar_set_name( w, item->str_val, wm );
    	    scrollbar_set_parameters( w, item->int_val - item->min, item->max - item->min, 1, 1, wm );
            scrollbar_set_showing_offset( w, item->min, wm );
            scrollbar_set_normal_value( w, item->normal_val - item->min, wm );
	    break;
	case DIALOG_ITEM_TEXT:
	    if( item->str_val )
	    {
	        text_set_text( w, (const char*)item->str_val, wm );
	        if( data->items_created == false )
	    	    item->str_val = smem_strdup( item->str_val );
	    }
	    break;
	case DIALOG_ITEM_LABEL:
	    break;
	case DIALOG_ITEM_POPUP:
	    if( item->menu )
	    {
	        button_set_menu( w, item->menu );
	        item->menu = NULL;
	    }
	    button_set_menu_val( w, item->int_val );
	    break;
	case DIALOG_ITEM_CHECKBOX:
	    if( item->int_val )
	        w->color = BUTTON_HIGHLIGHT_COLOR;
	    else
	        w->color = wm->button_color;
	    break;
    }
}

void dialog_reinit_items( WINDOWPTR pwin, bool show_and_focus ) //create new items + change item props; pwin = dialog handler or decorator with dialog
{
    WINDOWPTR win = get_dialog_win( pwin );
    if( !win ) return;
    window_manager* wm = win->wm;
    dialog_data* data = (dialog_data*)win->data;
    if( !data->items ) return;

    int i = 0;
    int y = wm->interelement_space;
    int cols = 1; //current number of columns
    int colcnt = 0; //current column number
    while( 1 )
    {
	dialog_item* item = &data->items[ i ];

	if( item->type == DIALOG_ITEM_NONE )
	    break;

	item->dwin = win;

	WINDOWPTR w = NULL;
	int w_ysize = dialog_get_item_ysize( item->type, win );
	if( data->items_created )
	{
	    w = item->win;
	    if( w ) w_ysize = w->ysize;
	}
	switch( item->type )
	{
    	    case DIALOG_ITEM_EMPTY_LINE:
    		break;
	    case DIALOG_ITEM_NUMBER:
	    case DIALOG_ITEM_NUMBER_HEX:
	    case DIALOG_ITEM_NUMBER_FLOAT:
	    case DIALOG_ITEM_NUMBER_TIME:
		if( !w )
		{
		    switch( item->type )
		    {
			default: wm->opt_text_numeric = 1; break;
			case DIALOG_ITEM_NUMBER_HEX: wm->opt_text_numeric = 2; break;
	    	        case DIALOG_ITEM_NUMBER_FLOAT: wm->opt_text_numeric = 3; break;
			case DIALOG_ITEM_NUMBER_TIME: wm->opt_text_numeric = 4; break;
		    }
		    w = new_window( "d.item.num", 0, y, 1, w_ysize, wm->text_background, win, text_handler, wm );
		    text_set_flags( w, text_get_flags( w ) | TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES );
		}
		break;
	    case DIALOG_ITEM_SLIDER:
		if( !w )
		{
		    wm->opt_scrollbar_compact = true;
		    w = new_window( "d.item.slider", 0, y, 1, w_ysize, wm->scroll_color, win, scrollbar_handler, wm );
		}
		break;
	    case DIALOG_ITEM_TEXT:
		if( !w )
		{
		    w = new_window( "d.item.txt", 0, y, 1, w_ysize, wm->text_background, win, text_handler, wm );
		    text_set_flags( w, text_get_flags( w ) | TEXT_FLAG_CALL_HANDLER_ON_ANY_CHANGES );
		}
		break;
	    case DIALOG_ITEM_LABEL:
		if( !w )
		{
	    	    w = new_window( (const char*)item->str_val, 0, y, 1, w_ysize, win->color, win, label_handler, wm );
	    	}
		break;
	    case DIALOG_ITEM_POPUP:
		if( !w )
		{
		    wm->opt_button_flags = BUTTON_FLAG_SHOW_VALUE | BUTTON_FLAG_SHOW_PREV_VALUE | BUTTON_FLAG_FLAT;
		    w = new_window( (const char*)item->str_val, 0, y, 1, w_ysize, wm->button_color, win, button_handler, wm );
		}
		break;
	    case DIALOG_ITEM_CHECKBOX:
		if( !w )
		{
		    wm->opt_button_flags = BUTTON_FLAG_FLAT;
		    w = new_window( (const char*)item->str_val, 0, y, 1, w_ysize, wm->button_color, win, button_handler, wm );
		}
	}
	if( data->items_created == false )
	{
	    if( ( item->flags >> DIALOG_ITEM_FLAG_COLUMNS_OFFSET ) & DIALOG_ITEM_FLAG_COLUMNS_MASK )
		cols = 1 + ( ( item->flags >> DIALOG_ITEM_FLAG_COLUMNS_OFFSET ) & DIALOG_ITEM_FLAG_COLUMNS_MASK );
	    if( w )
	    {
		if( colcnt == 0 )
		    set_window_controller( w, 0, wm, (WCMD)wm->interelement_space, CEND );
		else
		    set_window_controller( w, 0, wm, CPERC, (WCMD)( 100 / cols * colcnt ), CEND );
		if( colcnt == cols - 1 )
		    set_window_controller( w, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		else
		    set_window_controller( w, 2, wm, CPERC, (WCMD)( 100 / cols * ( colcnt + 1 ) ), CSUB, (WCMD)wm->interelement_space, CEND );
		set_handler( w, dialog_item_handler, data, wm );
	    }
	    item->win = w;
	    colcnt++;
	    if( colcnt >= cols )
	    {
		y += w_ysize + wm->interelement_space;
		colcnt = 0;
		cols = 1;
	    }
	}
	dialog_reinit_item( item );

	if( item->flags & DIALOG_ITEM_FLAG_FOCUS )
	    data->focus_on_item = w;

	i++;
    }

    if( show_and_focus )
    {
	show_window( pwin );
        bring_to_front( pwin, wm );
        recalc_regions( wm );
    }

    data->items_created = true;
}

dialog_item* dialog_get_items( WINDOWPTR win )
{
    win = get_dialog_win( win );
    if( !win ) return NULL;
    dialog_data* data = (dialog_data*)win->data;
    return data->items;
}

void dialog_set_flags( WINDOWPTR win, uint32_t flags )
{
    win = get_dialog_win( win );
    if( !win ) return;
    dialog_data* data = (dialog_data*)win->data;
    data->flags |= flags;
}

void dialog_set_par( WINDOWPTR win, int par_id, int val )
{
    win = get_dialog_win( win );
    if( !win ) return;
    dialog_data* data = (dialog_data*)win->data;
    if( (unsigned)par_id < (unsigned)MAX_USER_PARS )
	data->user_pars[ par_id ] = val;
}

int dialog_get_par( WINDOWPTR win, int par_id )
{
    win = get_dialog_win( win );
    if( !win ) return 0;
    dialog_data* data = (dialog_data*)win->data;
    if( (unsigned)par_id < (unsigned)MAX_USER_PARS )
	return data->user_pars[ par_id ];
    return 0;
}

dialog_item* dialog_new_item( dialog_item** itemlist )
{
    if( !itemlist ) return NULL;
    if( *itemlist == NULL )
    {
	*itemlist = (dialog_item*)smem_znew( sizeof( dialog_item ) * 8 );
    }
    if( *itemlist == NULL ) return NULL;
    int size = (int)smem_get_size( *itemlist ) / sizeof( dialog_item );
    int i = 0;
    for( i = 0; i < size; i++ )
    {
	if( (*itemlist)[ i ].type == DIALOG_ITEM_NONE ) break;
    }
    if( i >= size - 1 )
    {
	*itemlist = (dialog_item*)smem_resize2( *itemlist, sizeof( dialog_item ) * ( size + 8 ) );
	if( *itemlist == NULL ) return NULL;
    }
    return &((*itemlist)[ i ]);
}

dialog_item* dialog_get_item( dialog_item* itemlist, uint32_t id )
{
    if( !itemlist ) return NULL;
    dialog_item* rv = itemlist;
    while( 1 )
    {
	if( rv->id == id ) break;
	if( rv->type == DIALOG_ITEM_NONE ) { rv = NULL; break; }
	rv++;
    }
    return rv;
}

//#define DIALOG_STACK_DEBUG
static void dialog_stack_print( window_manager* wm )
{
    if( wm->focus_win ) printf( "  Current focus: %s\n", wm->focus_win->name );
    int size = smem_get_size( wm->dstack ) / sizeof( dstack_item );
    for( int i = 0; i < size; i++ )
    {
	if( wm->dstack[ i ].win )
    	    printf( "  %d: %s ; FOCUS: %s\n", i, wm->dstack[ i ].win->name, wm->dstack[ i ].focus->name );
    	else
    	    printf( "  %d: NULL\n", i );
    }
}

void dialog_stack_add( WINDOWPTR win )
{
    window_manager* wm = win->wm;

    if( !wm->dstack )
	wm->dstack = (dstack_item*)smem_znew( 2 * sizeof( dstack_item ) );
    if( !wm->dstack ) return;

    int size = smem_get_size( wm->dstack ) / sizeof( dstack_item );
    int p = -1;
    for( int i = 0; i < size; i++ )
	if( wm->dstack[ i ].win == win )
	{
	    //Already exists:
#ifdef DIALOG_STACK_DEBUG
	    printf( "win %s is already on the dialog stack (%d)\n", win->name, i );
#endif
	    //p = i;
	    //break;
	    return;
	}
    if( p == -1 )
    {
	//Get an empty slot on top of the stack:
	//WWWW00000
	//    |
	//    p
	for( int i = size - 1; i > 0; i-- )
	    if( wm->dstack[ i ].win ) { p = i + 1; break; }
    }
    if( p == -1 )
    {
        //Initial state:
        wm->dstack[ 0 ].win = wm->focus_win;
        wm->dstack[ 0 ].focus = wm->focus_win;
        p = 1;
    }
    if( p >= size )
    {
	//Add item:
	size = p + 1;
	wm->dstack = (dstack_item*)smem_resize2( wm->dstack, size * sizeof( dstack_item ) );
	if( !wm->dstack ) return;
    }
    wm->dstack[ p ].win = win;
    wm->dstack[ p ].focus = win;

    if( wm->focus_win )
    {
	for( int i = 1; i < size; i++ )
	{
	    WINDOWPTR w = wm->dstack[ i ].win;
	    if( !w ) continue;
	    WINDOWPTR p = get_parent_by_win_handler( wm->focus_win, w->win_handler );
	    if( p == w )
	    {
		//wm->focus_win is inside dialog w;
		//that is, we are now focused on some internal element of dialog w;
		//when we return to this dialog, we will need to focus on this element;
		wm->dstack[ i ].focus = wm->focus_win;
	    }
	}
    }

    set_focus_win( win, wm );

#ifdef DIALOG_STACK_DEBUG
    printf( "ADD %s\n", win->name ); dialog_stack_print( wm );
#endif
}

void dialog_stack_del( WINDOWPTR win )
{
    window_manager* wm = win->wm;

    if( !wm->dstack ) return;

    int size = smem_get_size( wm->dstack ) / sizeof( dstack_item );
    bool filled = 0;
    int defocus = -1;
    for( int i = size - 1; i > 0; i-- )
    {
	WINDOWPTR w = wm->dstack[ i ].win;
	if( !w ) continue;
	if( w == win )
	{
	    wm->dstack[ i ].win = NULL; //just remove
	    if( !filled ) defocus = i;
	    break;
	}
	filled = 1;
    }
    if( defocus )
    {
	for( int i = defocus - 1; i >= 0; i-- )
	{
	    WINDOWPTR w = wm->dstack[ i ].win;
	    WINDOWPTR f = wm->dstack[ i ].focus;
	    if( !w ) continue;
	    if( f )
		set_focus_win( f, wm );
	    else
		set_focus_win( w, wm );
	    break;
	}
    }

#ifdef DIALOG_STACK_DEBUG
    printf( "DEL %s\n", win->name ); dialog_stack_print( wm );
#endif
}
