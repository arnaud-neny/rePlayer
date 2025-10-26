/*
    wm_hnd_virtual_keyboard.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

struct kbd_data
{
    WINDOWPTR		win;
    int			font_scale;
    int 		mx, my;
    int 		selected_key;
    int			selected_key_x;
    int			selected_key_y;
    bool 		shift;
    bool		show_char;
    WINDOWPTR		send_event_to;
    int			result; //0 - nothing; 2 - cancel; 3 - enter;

    BUTTON_AUTOREPEAT_VARS;
};

const int XKEYS = 12;
const int YKEYS = 5;
const float KEYSCALE = 1.5; //size = keys * scrollbar * scale
const char* BIG_BTN_STR = STR_LARGEST_CHAR STR_LARGEST_CHAR;
const int BIG_BTN_SIZE = 2;

#define KEY_PRINTABLE( K ) ( (K) >= 0x20 && (K) < 127 )
#define KEY_WITH_AUTOREPEAT( K ) ( (K) >= 8 )

const char* kbd_text1[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", NULL,
                            "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", NULL }; //shift+
int    kbd_key1[] =       { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
                            '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+' }; //shift+
int  kbd_size1[] =        { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1 };

const char* kbd_text2[] = { "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", NULL,
                            "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", NULL }; //shift+
int    kbd_key2[] =       { 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
                            'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}' }; //shift+
int  kbd_size2[] =        { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1 };

const char* kbd_text3[] = { "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "\\", NULL,
                            "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "|", NULL }; //shift+
int    kbd_key3[] =       { 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\\', 0,
                            'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '|' }; //shift+
int  kbd_size3[] =        { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1 };

const char* kbd_text4[] = { "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", STR_LEFT, NULL,
                            "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", STR_LEFT, NULL }; //shift+
int    kbd_key4[] =       { 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_BACKSPACE, 0,
                            'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', KEY_BACKSPACE }; //shift+
int  kbd_size4[] =        { 1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   2 };

const char* kbd_text5[] = { "...", STR_UP, "`", "", "x", "OK", NULL,
                            "...", STR_UP, "~", "", "x", "OK", NULL }; //shift+
int    kbd_key5[] =       { 5, 4, '`', ' ', 2, 3, 0,
                            5, 4, '~',' ', 2, 3, 0 }; //shift+
int  kbd_size5[] =        { 1, 1, 1, 5, 2, 2 };

const char** kbd_text_arr[ YKEYS ];
int* kbd_key_arr[ YKEYS ];
int* kbd_size_arr[ YKEYS ];
bool kbd_initialized = false;

static void kbd_draw_line( kbd_data* data, const char** text, int* size, int* key_codes, int y )
{
    WINDOWPTR win = data->win;
    window_manager* wm = win->wm;
    int cell_xsize = ( win->xsize << 15 ) / XKEYS;
    int cell_ysize = win->ysize / YKEYS;
    int p = 0;
    int x = 0;
    y += ( win->ysize - ( cell_ysize * YKEYS ) ) / 2;
    while( 1 )
    {
	const char* str = text[ p ];
	if( str == NULL ) break;
	int button_size = cell_xsize * size[ p ];
	int button_size2 = ( ( x + button_size ) >> 15 ) - ( x >> 15 );
	
	//Bounds correction:
	if( ( x >> 15 ) + button_size2 + ( cell_xsize >> 15 ) > win->xsize )
	    button_size2 = win->xsize - ( x >> 15 ) + 1;
	if( y + cell_ysize + 4 > win->ysize )
	    cell_ysize = win->ysize - y + 1;

	int str_size = string_x_size( str, wm );
	int light = 0;
	int kc = key_codes[ p ];
	if( kc == 4 && data->shift )
	    light = 1;
	if( data->mx >= (x>>15) && data->mx < (x>>15) + button_size2 &&
	    data->my >= y && data->my < y + cell_ysize )
	{
	    data->selected_key = (unsigned)key_codes[ p ];
	    data->selected_key_x = x >> 15;
	    data->selected_key_y = y;
	    light = 1;
	}
	COLOR button_color = wm->color2;
	int button_grad1 = 100;
	int button_grad2 = 64;
	if( kc >= 2 && kc <= 5 )
	{
	    //Cancel or OK (enter):
	    button_grad1 = 150;
	    button_grad2 = 100;
	    if( kc == 2 ) button_color = blend( button_color, wm->red, 128 ); //cancel
	    if( kc == 3 ) button_color = blend( button_color, wm->green, 128 ); //ok
	}
	if( light )
	{
	    button_grad1 = 255;
	    button_grad2 = 150;
	}
	draw_vgradient( ( x >> 15 ), y, button_size2 - 1, cell_ysize - 1, button_color, button_grad1, button_grad2, wm );
	wm->cur_font_color = wm->color3;
	draw_string( 
	    str, 
	    ( x >> 15 ) + ( button_size2 - str_size ) / 2, 
	    y + ( cell_ysize - char_y_size( wm ) ) / 2,
	    wm );
	x += button_size;
	p++;
    }
}

static int kbd_get_text_scale( WINDOWPTR win )
{
    int rv = 256;
    window_manager* wm = win->wm;
    if( ( win->xsize / XKEYS * BIG_BTN_SIZE >= font_string_x_size( BIG_BTN_STR, win->font, wm ) * 2 ) &&
        ( win->ysize / YKEYS >= font_char_y_size( win->font, wm ) * 2 ) )
        rv = 512;
    return rv;
}

static void send_key( kbd_data* data, bool focus_on_text_window )
{
    if( !data->send_event_to ) return;
    window_manager* wm = data->win->wm;
    if( focus_on_text_window )
	set_focus_win( data->send_event_to, wm );
    int k = data->selected_key;
    uint flags = 0;
    if( k >= 0x41 &&
        k <= 0x5A &&
        data->shift )
    {
        k += 0x20;
        flags |= EVT_FLAG_SHIFT;
    }
    send_event( data->send_event_to, EVT_BUTTONDOWN, flags, 0, 0, data->selected_key, 0, 1024, wm );
    send_event( data->send_event_to, EVT_BUTTONUP, flags, 0, 0, data->selected_key, 0, 1024, wm );
    draw_window( data->win, wm );
}

static void keyboard_timer( void* vdata, sundog_timer* timer, window_manager* wm )
{
    //Autorepeat timer
    kbd_data* data = (kbd_data*)vdata;
    if( KEY_WITH_AUTOREPEAT( data->selected_key ) )
    {
	if( data->send_event_to ) send_event( data->send_event_to, EVT_SHOWCHAR, 0, 0, 0, 0, 0, 1024, wm ); //hide char
	send_key( data, false );
	BUTTON_AUTOREPEAT_ACCELERATE( data );
    }
}

int keyboard_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    kbd_data* data = (kbd_data*)win->data;
    int cell_ysize = win->ysize / YKEYS;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( kbd_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    BUTTON_AUTOREPEAT_INIT( data );
	    data->font_scale = 256;
	    data->mx = -1;
	    data->my = -1;
	    data->selected_key = 0;
	    data->shift = 0;
	    data->send_event_to = 0;
	    data->result = 0;
	    if( kbd_initialized == false )
	    {
		kbd_text_arr[ 0 ] = kbd_text1;
		kbd_text_arr[ 1 ] = kbd_text2;
		kbd_text_arr[ 2 ] = kbd_text3;
		kbd_text_arr[ 3 ] = kbd_text4;
		kbd_text_arr[ 4 ] = kbd_text5;
		kbd_key_arr[ 0 ] = kbd_key1;
		kbd_key_arr[ 1 ] = kbd_key2;
		kbd_key_arr[ 2 ] = kbd_key3;
		kbd_key_arr[ 3 ] = kbd_key4;
		kbd_key_arr[ 4 ] = kbd_key5;
		kbd_size_arr[ 0 ] = kbd_size1;
		kbd_size_arr[ 1 ] = kbd_size2;
		kbd_size_arr[ 2 ] = kbd_size3;
		kbd_size_arr[ 3 ] = kbd_size4;
		kbd_size_arr[ 4 ] = kbd_size5;
		kbd_initialized = true;
	    }
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
            BUTTON_AUTOREPEAT_DEINIT( data );
            retval = 1;
            break;
	case EVT_DRAW:
	{
	    wbd_lock( win );

	    wm->cur_font_scale = kbd_get_text_scale( win );

	    draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );

	    int prev_selected_key = data->selected_key;
	    data->selected_key = 0;
	    for( int i = 0, y = 0; i < YKEYS; i++, y += cell_ysize )
	    {
	        const char** text = kbd_text_arr[ i ];
	        int* keys = kbd_key_arr[ i ];
	        int* size = kbd_size_arr[ i ];
	        if( data->shift )
	        {
	    	    while( *text ) text++;
		    text++;
		    while( *keys ) keys++;
		    keys++;
		}
		kbd_draw_line( data, text, size, keys, y );
	    }
	    wm->cur_opacity = BORDER_OPACITY;
	    draw_frect( 0, 0, 1, win->ysize, BORDER_COLOR_WITHOUT_OPACITY, wm );
	    draw_frect( win->xsize - 1, 0, 1, win->ysize, BORDER_COLOR_WITHOUT_OPACITY, wm );
	    draw_frect( 0, win->ysize - 1, win->xsize, 1, BORDER_COLOR_WITHOUT_OPACITY, wm );

	    wbd_draw( wm );
	    wbd_unlock( wm );

	    if( prev_selected_key != data->selected_key )
	    {
		BUTTON_AUTOREPEAT_START( data, keyboard_timer );
	    	if( data->send_event_to )
		{
		    int k = 0;
		    if( KEY_PRINTABLE( data->selected_key ) && data->selected_key != 0x20 )
	    		k = data->selected_key;
		    send_event( data->send_event_to, EVT_SHOWCHAR, 0, 0, 0, k, 0, 1024, wm );
		    if( k )
			data->show_char = true;
		}
	    }

	    retval = 1;
	    break;
	}
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    data->mx = rx;
	    data->my = ry;
	    draw_window( win, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONUP:
	    BUTTON_AUTOREPEAT_DEINIT( data );
	    if( data->show_char && data->send_event_to )
	    {
		send_event( data->send_event_to, EVT_SHOWCHAR, 0, 0, 0, 0, 0, 1024, wm ); //hide char
		data->show_char = false;
	    }
	    switch( data->selected_key )
	    {
		case 2:
		case 3:
		    //Hide keyboard:
		    data->result = data->selected_key;
		    hide_keyboard_for_text_window( wm );
		    break;
		case 4:
		    //Shift:
		    data->shift ^= 1;
		    data->mx = -1;
		    data->my = -1;
		    draw_window( win, wm );
		    break;
		case 5:
		    //Show menu:
		    if( data->send_event_to )
		    {
			char* menu = SMEM_ALLOC2( char, 8 );
			if( menu )
			{
			    menu[ 0 ] = 0;
			    SMEM_STRCAT_D( menu, wm_get_string( STR_WM_COPY_ALL ) );
			    SMEM_STRCAT_D( menu, "\n" );
			    SMEM_STRCAT_D( menu, wm_get_string( STR_WM_PASTE ) );
			    int v = popup_menu( wm_get_string( STR_WM_EDIT ), menu, win->screen_x + data->selected_key_x, win->screen_y + data->selected_key_y, wm->menu_color, wm );
			    smem_free( menu );
			    int keycode = 0;
			    if( v == 0 )
			    {
				//Copy all:
				keycode = 'c';
			    }
			    if( v == 1 )
			    {
				//Paste:
				keycode = 'v';
			    }
			    if( keycode )
			    {
				set_focus_win( data->send_event_to, wm );
				send_event( data->send_event_to, EVT_BUTTONDOWN, EVT_FLAG_CTRL, 0, 0, keycode, 0, 1024, wm );
				send_event( data->send_event_to, EVT_BUTTONUP, EVT_FLAG_CTRL, 0, 0, keycode, 0, 1024, wm );
			    }
			}
		    }
		    data->mx = -1;
		    data->my = -1;
		    draw_window( win, wm );
		    break;
		default:
		    send_key( data, true );
		    data->mx = -1;
		    data->my = -1;
                    draw_window( win, wm );
		    break;
	    }
	    data->mx = -1;
	    data->my = -1;
	    data->selected_key = 0;
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

int keyboard_bg_handler( sundog_event* evt, window_manager* wm )
{
    //if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    switch( evt->type )
    {
	case EVT_AFTERCREATE:
	    retval = 1;
	    break;
	    
	case EVT_DRAW:
	{
	    wbd_lock( win );

	    draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );

	    if( wm->vk_win )
	    {
		WINDOWPTR k = wm->vk_win; //parent window (text + kbd)
		if( k->name )
		{
		    if( strcmp( k->name, "KBD" ) )
		    {
			wm->cur_font_color = wm->color2;
			draw_string( k->name, k->x, k->y - string_y_size( k->name, wm ) - wm->interelement_space, wm );
		    }
		}
	    }

	    wbd_draw( wm );
	    wbd_unlock( wm );
	    
	    retval = 1;
	    break;
	}
	
	case EVT_BEFORESHOW:
	case EVT_SCREENRESIZE:
	{
	    WINDOWPTR p = win->parent; //root window (desktop)
	    win->x = 0;
	    win->y = 0;
	    win->xsize = p->xsize;
	    win->ysize = p->ysize;
	
	    if( wm->vk_win )
	    {
		WINDOWPTR k = wm->vk_win; //parent window (text + kbd)
		
		int xsize;
		int ysize;
		int x;
		int y;
		
		xsize = XKEYS * (int)( (float)wm->scrollbar_size * KEYSCALE );
		ysize = ( YKEYS + 1 ) * (int)( (float)wm->scrollbar_size * KEYSCALE );
		if( xsize > wm->desktop_xsize ) xsize = wm->desktop_xsize;
		if( ysize > wm->desktop_ysize ) ysize = wm->desktop_ysize;
		x = ( wm->desktop_xsize - xsize ) / 2;
		y = ( wm->desktop_ysize - ysize ) / 2;
		
		k->x = x;
		k->y = y;
		k->xsize = xsize;
		k->ysize = ysize;

		//Text field:
		int text_ysize = ysize / ( YKEYS + 1 );
		k->childs[ 0 ]->xsize = xsize - 2;
		k->childs[ 0 ]->ysize = text_ysize - 1;

		//Keyboard:
		k->childs[ 1 ]->y = text_ysize;
		k->childs[ 1 ]->xsize = xsize;
		k->childs[ 1 ]->ysize = ysize - text_ysize;

		//Text field zoom:
        	text_set_zoom( k->childs[ 0 ], kbd_get_text_scale( k->childs[ 1 ] ), wm );
	    }

	    break;
	}
	case EVT_MOUSEMOVE:
        case EVT_MOUSEBUTTONDOWN:
    	    retval = 1;
    	    break;
        case EVT_MOUSEBUTTONUP:
	    hide_keyboard_for_text_window( wm );
    	    retval = 1;
    	    break;
    }
    return retval;
}

int keyboard_text_field_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    kbd_data* data = (kbd_data*)user_data;
    int a = text_get_last_action( win );
    if( a == TEXT_ACTION_ESCAPE )
	data->result = 2;
    else
	data->result = 3;
    hide_keyboard_for_text_window( wm );
    return 0;
}

void hide_keyboard_for_text_window( window_manager* wm )
{
    if( wm->vk_win )
    {
	hide_window( wm->vk_win );
    }
}

void show_keyboard_for_text_window( WINDOWPTR text, const char* name, window_manager* wm )
{
    if( wm->vk_win ) return;
    
    WINDOWPTR bg = new_window( "KBD BG", 0, 0, 1, 1, wm->color1, wm->root_win, keyboard_bg_handler, wm );
    bg->flags |= WIN_FLAG_ALWAYS_UNFOCUSED;
    
    if( name == NULL ) name = "KBD";
    wm->vk_win = new_window( name, 0, 0, 1, 1, BORDER_COLOR( wm->color1 ), wm->root_win, null_handler, wm );
    wm->vk_win->flags |= WIN_FLAG_ALWAYS_UNFOCUSED;
    WINDOWPTR edit_win = new_window( "kbd text", 1, 1, 10, 10, wm->text_background, wm->vk_win, text_handler, wm );
    text_set_flags( edit_win, text_get_flags( edit_win ) | TEXT_FLAG_CALL_HANDLER_ON_ESCAPE | TEXT_FLAG_NO_VIRTUAL_KBD );
    char* text_source = text_get_text( text, wm );
    if( text_source )
    {
	text_set_text( edit_win, text_source, wm );
	text_set_cursor_position( edit_win, text_get_cursor_position( text, wm ), wm );
	set_focus_win( edit_win, wm );
    }
    WINDOWPTR keys = new_window( 
	"kbd",
	0, 0, 10, 10,
	wm->dialog_color, 
	wm->vk_win, 
	keyboard_handler, 
	wm );
    keys->flags |= WIN_FLAG_ALWAYS_UNFOCUSED;
    kbd_data* kdata = (kbd_data*)keys->data;
    kdata->send_event_to = edit_win;
    set_handler( edit_win, keyboard_text_field_handler, kdata, wm );
    show_window( bg );
    show_window( wm->vk_win );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    while( 1 )
    {
	sundog_event evt;
	EVENT_LOOP_BEGIN( &evt, wm );
	if( EVENT_LOOP_END( wm ) ) break;
	if( wm->vk_win->visible == 0 ) break;
	if( keys->visible == 0 ) break;
	if( text->visible == 0 ) break;
    }
    if( kdata->result == 3 )
    {
	//ENTER pressed:
	char* old_text = text_get_text( text, wm );
	char* new_text = text_get_text( edit_win, wm );
	if( smem_strcmp( old_text, new_text ) != 0 )
	{
	    text_set_text( text, new_text, wm );
	    text_changed( text );
	}
    }
    remove_window( wm->vk_win, wm ); wm->vk_win = NULL;
    remove_window( bg, wm );
    
    set_focus_win( text->parent, wm );
    
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
}
