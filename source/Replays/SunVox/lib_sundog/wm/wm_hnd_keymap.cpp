/*
    wm_hnd_keymap.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2014 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#define SHORTCUT_NAME_SIZE	128

struct keymap_data
{
    WINDOWPTR win;
    WINDOWPTR list_actions;
    WINDOWPTR shortcuts[ KEYMAP_ACTION_KEYS ];
    WINDOWPTR shortcuts_del[ KEYMAP_ACTION_KEYS ];
    char shortcut_names[ KEYMAP_ACTION_KEYS * SHORTCUT_NAME_SIZE ];
    WINDOWPTR reset;
    WINDOWPTR ok;
    int action_num;
    bool capturing;
    int capture_num;
    int capture_key_counter;
    int capture_key;
    uint capture_flags;
    int capture_x;
    int capture_y;
    int capture_scancode;
};

void keymap_refresh_shortcut_info( keymap_data* data )
{
    window_manager* wm = data->win->wm;
    slist_data* ldata = list_get_data( data->list_actions, wm );
    for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
    {
        rename_window( data->shortcuts[ i ], "..." );
        data->shortcuts[ i ]->color = wm->button_color;
    }
    int action_num = ldata->selected_item;
    if( slist_get_attr( action_num, ldata ) == 1 )
	action_num = -1; //group name
    else
    {
	for( int i = 0; i <= ldata->selected_item; i++ ) if( slist_get_attr( i, ldata ) == 1 ) action_num--;
    }
    data->action_num = action_num;
    if( action_num >= 0 )
    {
	for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
	{
	    sundog_keymap_key* k = keymap_get_key( 0, 0, action_num, i, wm );
	    if( !k ) continue;
	    char* name = &data->shortcut_names[ i * SHORTCUT_NAME_SIZE ];
	    name[ 0 ] = 0;
	    bool n = 0;
	    if( k->flags & EVT_FLAG_CTRL ) { smem_strcat( name, SHORTCUT_NAME_SIZE, "Ctrl" ); n = 1; }
	    if( k->flags & EVT_FLAG_ALT ) { if( n ) smem_strcat( name, SHORTCUT_NAME_SIZE, "+" ); smem_strcat( name, SHORTCUT_NAME_SIZE, "Alt" ); n = 1; }
	    if( k->flags & EVT_FLAG_SHIFT ) { if( n ) smem_strcat( name, SHORTCUT_NAME_SIZE, "+" ); smem_strcat( name, SHORTCUT_NAME_SIZE, "Shift" ); n = 1; }
	    if( k->flags & EVT_FLAG_MODE ) { if( n ) smem_strcat( name, SHORTCUT_NAME_SIZE, "+" ); smem_strcat( name, SHORTCUT_NAME_SIZE, "Mode" ); n = 1; }
	    if( k->flags & EVT_FLAG_CMD ) { if( n ) smem_strcat( name, SHORTCUT_NAME_SIZE, "+" ); smem_strcat( name, SHORTCUT_NAME_SIZE, "Cmd" ); n = 1; }
	    if( n ) smem_strcat( name, SHORTCUT_NAME_SIZE, "+" );
	    const char* key_name = get_key_name( k->key );
	    if( key_name )
	    {
		smem_strcat( name, SHORTCUT_NAME_SIZE, key_name );
	    }
	    else
	    {
		char key_code[ 16 ];
		int_to_string( k->key, key_code );
		smem_strcat( name, SHORTCUT_NAME_SIZE, key_code );
	    }
	    if( k->key >= KEY_MIDI_NOTE && k->key <= KEY_MIDI_PROG )
	    {
		char ts[ SHORTCUT_NAME_SIZE ];
		ts[ 0 ] = 0;
		int ch = ( ( k->pars1 >> 16 ) & 0xFFFF ) + 1;
		int val = k->pars1 & 0xFFFF;
		switch( k->key )
		{
		    case KEY_MIDI_NOTE: sprintf( ts, "%d:Note%d", ch, val ); break;
            	    case KEY_MIDI_CTL: sprintf( ts, "%d:CC%d", ch, val ); break;
            	    case KEY_MIDI_NRPN: sprintf( ts, "%d:NRPN%d", ch, val ); break;
            	    case KEY_MIDI_RPN: sprintf( ts, "%d:RPN%d", ch, val ); break;
            	    case KEY_MIDI_PROG: sprintf( ts, "%d:Prog%d", ch, val ); break;
            	}
		smem_strcat( name, SHORTCUT_NAME_SIZE, ts );
	    }
	    rename_window( data->shortcuts[ i ], name );
	    data->shortcuts[ i ]->color = wm->button_color;
	}
    }
    for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
    {
        draw_window( data->shortcuts[ i ], wm );
    }
}

int keymap_actions_list_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_data* data = (keymap_data*)user_data;
    switch( list_get_last_action( win, wm ) )
    {
        case LIST_ACTION_ESCAPE:
	    remove_window( wm->keymap_win, wm );
	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );
    	    break;
    	default:
	    if( data->capturing ) data->capturing = 0;
	    keymap_refresh_shortcut_info( data );
    	    break;
    }
    return 0;
}

int keymap_shortcut_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_data* data = (keymap_data*)user_data;
    if( data->capturing )
    {
	data->capturing = 0;
	keymap_refresh_shortcut_info( data );
    }
    else
    {
	data->capturing = 1;
	data->capture_key_counter = 0;
	data->capture_key = 0;
	data->capture_flags = 0;
	data->capture_x = 0;
	data->capture_y = 0;
	data->capture_scancode = 0;
	for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
	{
	    if( data->shortcuts[ i ] == win )
	    {
		data->capture_num = i;
		break;
	    }
	}
	set_focus_win( data->win, wm );
	rename_window( win, wm_get_string( STR_WM_ENTER_SHORTCUT ) );
	win->color = BUTTON_HIGHLIGHT_COLOR;
	draw_window( win, wm );
    }
    return 0;
}

int keymap_delete_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_data* data = (keymap_data*)user_data;
    if( data->capturing ) data->capturing = 0;
    if( data->action_num < 0 ) return 0;
    for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
    {
        if( data->shortcuts_del[ i ] == win )
        {
    	    keymap_bind( 0, 0, 0, 0, data->action_num, i, KEYMAP_BIND_OVERWRITE | KEYMAP_BIND_FIX_CONFLICTS, wm );
	    keymap_save( 0, 0, wm );
	    keymap_refresh_shortcut_info( data );
	    break;
	}
    }
    return 0;
}

int keymap_reset_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_data* data = (keymap_data*)user_data;
    if( data->capturing ) data->capturing = 0;
    if( data->action_num < 0 ) return 0;
    for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
    {
	keymap_bind( 0, 0, 0, 0, data->action_num, i, KEYMAP_BIND_RESET_TO_DEFAULT | KEYMAP_BIND_FIX_CONFLICTS, wm );
    }
    keymap_save( 0, 0, wm );
    keymap_refresh_shortcut_info( data );
    return 0;
}

int keymap_ok_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_data* data = (keymap_data*)user_data;
    if( data->capturing ) data->capturing = 0;
    remove_window( wm->keymap_win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    return 0;
}

int keymap_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    keymap_data* data = (keymap_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( keymap_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		data->capturing = 0;
		
		int static_text_size = font_char_y_size( win->font, wm ) + 2;
		
		int y = wm->interelement_space;
		WINDOWPTR label = new_window( wm_get_string( STR_WM_ACTION ), 2, y, 32, static_text_size, win->color, win, label_handler, wm );
                set_window_controller( label, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( label, 2, wm, CPERC, (WCMD)100, CEND );
                label = new_window( wm_get_string( STR_WM_SHORTCUTS2 ), 2, y, 32, static_text_size, win->color, win, label_handler, wm );
                set_window_controller( label, 0, wm, CPERC, (WCMD)100 / 2, CEND );
                set_window_controller( label, 2, wm, CPERC, (WCMD)100, CEND );
                y += static_text_size + wm->interelement_space;
                
                data->list_actions = new_window( "", 0, 0, 1, 1, wm->list_background, win, list_handler, wm );
                set_handler( data->list_actions, keymap_actions_list_handler, data, wm );
                set_window_controller( data->list_actions, 0, wm, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->list_actions, 1, wm, (WCMD)y, CEND );
                set_window_controller( data->list_actions, 2, wm, CPERC, (WCMD)100 / 2, CSUB, (WCMD)wm->interelement_space, CEND );
                set_window_controller( data->list_actions, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
                slist_data* ldata = list_get_data( data->list_actions, wm );
		slist_reset_items( ldata );
		const char* prev_group = NULL;
		char* group_ts = (char*)smem_new( 512 );
		for( int i = 0; ; i++ )
		{
		    const char* action_name = keymap_get_action_name( NULL, 0, i, wm );
		    if( !action_name ) break;
		    const char* group = keymap_get_action_group_name( NULL, 0, i, wm );
		    if( group != prev_group )
		    {
			sprintf( group_ts, "%s:", group );
            		slist_add_item( group_ts, 1, ldata );
			prev_group = group;
		    }
            	    slist_add_item( action_name, 0, ldata );
            	}
            	smem_free( group_ts );
            	ldata->selected_item = 0;

                WINDOWPTR w;
                for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
                {
            	    int yy = y + i * ( wm->scrollbar_size + wm->interelement_space );

            	    wm->opt_button_flags = BUTTON_FLAG_FLAT;
            	    w = new_window( wm_get_string( STR_WM_SHORTCUT ), 0, yy, 1, wm->scrollbar_size, wm->button_color, win, button_handler, wm );
		    set_window_controller( w, 0, wm, CPERC, (WCMD)100 / 2, CEND );
		    set_window_controller( w, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space + wm->scrollbar_size, CEND );
		    set_handler( w, keymap_shortcut_handler, data, wm );
		    data->shortcuts[ i ] = w;

            	    wm->opt_button_flags = BUTTON_FLAG_FLAT;
            	    w = new_window( "x", 0, yy, 1, wm->scrollbar_size, wm->button_color, win, button_handler, wm );
            	    button_set_text_color( w, wm->color2 );
		    set_window_controller( w, 0, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space + wm->scrollbar_size, CEND );
		    set_window_controller( w, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		    set_handler( w, keymap_delete_handler, data, wm );
		    data->shortcuts_del[ i ] = w;
                }
            	wm->opt_button_flags = BUTTON_FLAG_FLAT;
            	w = new_window( wm_get_string( STR_WM_RESET_TO_DEF ), 0, y + KEYMAP_ACTION_KEYS * ( wm->scrollbar_size + wm->interelement_space ), 1, wm->scrollbar_size, wm->button_color, win, button_handler, wm );
		set_window_controller( w, 0, wm, CPERC, (WCMD)100 / 2, CEND );
		set_window_controller( w, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_handler( w, keymap_reset_handler, data, wm );
		data->reset = w;

                keymap_refresh_shortcut_info( data );

		data->ok = new_window( wm_get_string( STR_WM_CLOSE ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
		set_handler( data->ok, keymap_ok_handler, data, wm );
		int x = wm->interelement_space;
		set_window_controller( data->ok, 0, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		x += wm->button_xsize;
		set_window_controller( data->ok, 2, wm, (WCMD)x, CEND );
		set_window_controller( data->ok, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BUTTONDOWN:
	    if( data->capturing )
	    {
		if( evt->key < KEY_SHIFT || evt->key >= KEY_UNKNOWN )
		{
		    data->capture_key = evt->key;
		    data->capture_flags |= evt->flags;
		    data->capture_x = evt->x;
		    data->capture_y = evt->y;
		    data->capture_scancode = evt->scancode;
		    data->capture_key_counter++;
		}
	    }
	    else
	    {
		if( evt->key == KEY_ESCAPE )
        	{
            	    send_event( win, EVT_CLOSEREQUEST, wm );
        	}
	    }
	    retval = 1;
	    break;
	case EVT_BUTTONUP:
	    if( data->capturing )
	    {
		if( evt->key < KEY_SHIFT || evt->key >= KEY_UNKNOWN )
		{
		    data->capture_flags |= evt->flags;
		    data->capture_key_counter--;
		    if( data->capture_key_counter <= 0 )
		    {
			data->capturing = 0;
			uint pars1 = 0;
			uint pars2 = 0;
			if( data->capture_key >= KEY_MIDI_NOTE && data->capture_key <= KEY_MIDI_PROG )
			{
			    //Currently the x,y parameters are used for the MIDI key events only
			    pars1 = data->capture_x | ( data->capture_y << 16 );
			}
			if( data->action_num >= 0 )
			{
			    keymap_bind2( 0, 0, data->capture_key, data->capture_flags, pars1, pars2, data->action_num, data->capture_num, KEYMAP_BIND_OVERWRITE | KEYMAP_BIND_FIX_CONFLICTS, wm );
			    keymap_save( 0, 0, wm );
			    keymap_refresh_shortcut_info( data );
			}
		    }
		}
	    }
	    retval = 1;
	    break;
	case EVT_CLOSEREQUEST:
            {
		keymap_ok_handler( data, NULL, wm );
            }
            retval = 1;
            break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    wm->keymap_win = 0;
	    break;
    }
    return retval;
}
