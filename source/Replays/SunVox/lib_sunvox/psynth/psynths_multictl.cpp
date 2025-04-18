/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2024 Alexander Zolotov <nightradio@gmail.com>
WarmPlace.ru

MINIFIED VERSION

License: (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#include "psynth_net.h"
#define MODULE_DATA		psynth_multictl_data
#define MODULE_HANDLER		psynth_multictl
#define MODULE_INPUTS		0
#define MODULE_OUTPUTS		0
#define MULTICTL_SLOTS		16
#define MULTICTL_CURVE_POINTS	257 
#define MAX_RESPONSE            1000
struct multictl_output_slot
{
    PS_CTYPE min;
    PS_CTYPE max;
    PS_CTYPE ctl;
    uint32_t flags; 
    int future_use2;
    int future_use3;
    int future_use4;
    int future_use5;
};
static void reset_output_slot( multictl_output_slot* slot )
{
    slot->min = 0;
    slot->max = 32768;
    slot->ctl = 0;
    slot->flags = 0;
}
struct MODULE_DATA
{
    PS_CTYPE	ctl_val;
    PS_CTYPE	ctl_gain;
    PS_CTYPE	ctl_quant; 
    PS_CTYPE	ctl_offset;
    PS_CTYPE	ctl_response;
    PS_CTYPE    ctl_freq;
    psynth_event ctl_evt;
    multictl_output_slot* slots;
    float	floating_val;
    int		tick_counter; 
    int		tick_size;
    int		pars_recalc_request;
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
static void multictl_create_curve( int mod_num, psynth_net* pnet );
static void multictl_curve_reset( int mod_num, psynth_net* pnet );
static uint16_t* multictl_get_curve( int mod_num, psynth_net* pnet );
static int multictl_curve_val( int mod_num, psynth_net* pnet, int pos, int val, int w );
static void multictl_curve_set_vals( int mod_num, psynth_net* pnet, int pos, int val, int vals );
static int multictl_get_curve_val( int input, uint16_t* curve );
static int multictl_get_val( int val, uint16_t* val_curve, multictl_output_slot* slot, MODULE_DATA* data );
#define FOR_EACH_SLOT \
    for( int slot_n = 0, link_n = 0; slot_n < MULTICTL_SLOTS; slot_n++ ) \
    { \
    	multictl_output_slot* slot = &slots[ slot_n ]; \
    	if( slot->flags & 1 ) \
    	    if( link_n ) \
    		link_n--; \
	if( link_n >= mod->output_links_num ) break; \
	int l = mod->output_links[ link_n ]; \
	link_n++; \
	psynth_module* m = psynth_get_module( l, pnet ); \
	if( !m ) continue;
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct multictl_visual_data
{
    WINDOWPTR		win;
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
    bool		ui_links_reinit_request;
    WINDOWPTR		win_min[ MULTICTL_SLOTS ];
    WINDOWPTR		win_max[ MULTICTL_SLOTS ];
    WINDOWPTR		win_ctl[ MULTICTL_SLOTS ];
    WINDOWPTR		win_add[ MULTICTL_SLOTS ];
    int8_t		win_slot_num[ MULTICTL_SLOTS ];
    bool		pushed;
    int         	prev_i;
    int         	prev_val;
    int 		curve_y;
    int			curve_ysize;
    int			set_start_val;
    int			set_end_val;
    int			set_val;
    int			set_repeat;
    WINDOWPTR   	reset;
    WINDOWPTR   	smooth;
    WINDOWPTR   	load;
    WINDOWPTR   	save;
    WINDOWPTR   	set;
};
static int multictl_visual_handler( sundog_event* evt, window_manager* wm ); 
static int multictl_slot_min_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    int slot = -1;
    for( int i = 0; i < MULTICTL_SLOTS; i++ )
    {
	if( data->win_min[ i ] == win )
	{
	    slot = data->win_slot_num[ i ];
	    break;
	}
    }
    if( slot >= 0 )
    {
	psynth_module* mod = nullptr;
        if( data->mod_num >= 0 )
    	    mod = &data->pnet->mods[ data->mod_num ];
	multictl_output_slot* slots = data->module_data->slots;
	if( mod && slots )
	{
	    int val = scrollbar_get_value( win, wm );
	    slots[ slot ].min = val;
	    draw_window( data->win, wm );
	    data->pnet->change_counter++;
	}
    }
    return 0;
}
static int multictl_slot_max_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    int slot = -1;
    for( int i = 0; i < MULTICTL_SLOTS; i++ )
    {
	if( data->win_max[ i ] == win )
	{
	    slot = data->win_slot_num[ i ];
	    break;
	}
    }
    if( slot >= 0 )
    {
	psynth_module* mod = nullptr;
        if( data->mod_num >= 0 )
    	    mod = &data->pnet->mods[ data->mod_num ];
	multictl_output_slot* slots = data->module_data->slots;
	if( mod && slots )
	{
	    int val = scrollbar_get_value( win, wm );
	    slots[ slot ].max = val;
	    draw_window( data->win, wm );
	    data->pnet->change_counter++;
	}
    }
    return 0;
}
static int multictl_slot_ctl_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    int slot = -1;
    for( int i = 0; i < MULTICTL_SLOTS; i++ )
    {
	if( data->win_ctl[ i ] == win )
	{
	    slot = data->win_slot_num[ i ];
	    break;
	}
    }
    if( slot >= 0 )
    {
	psynth_module* mod = nullptr;
        if( data->mod_num >= 0 )
    	    mod = &data->pnet->mods[ data->mod_num ];
	multictl_output_slot* slots = data->module_data->slots;
	if( mod && slots )
	{
	    int val = scrollbar_get_value( win, wm );
	    slots[ slot ].ctl = val;
	    draw_window( data->win, wm );
	    data->pnet->change_counter++;
	}
    }
    return 0;
}
static int multictl_slot_add_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    int slot_n = -1;
    for( int i = 0; i < MULTICTL_SLOTS; i++ )
    {
	if( data->win_add[ i ] == win )
	{
	    slot_n = data->win_slot_num[ i ];
	    break;
	}
    }
    if( slot_n >= 0 )
    {
	psynth_module* mod = nullptr;
        if( data->mod_num >= 0 )
    	    mod = &data->pnet->mods[ data->mod_num ];
	multictl_output_slot* slots = data->module_data->slots;
	sunvox_engine* sv = (sunvox_engine*)data->pnet->host;
	if( mod && slots && sv )
	{
	    SUNVOX_SOUND_STREAM_CONTROL( sv, SUNVOX_STREAM_LOCK ); 
    	    if( slots[ slot_n ].flags & 1 )
    	    {
	        for( int i = slot_n; i < MULTICTL_SLOTS; i++ )
	        {
    	    	    multictl_output_slot* slot1 = &slots[ i ];
    		    if( i+1 < MULTICTL_SLOTS )
    		    {
    		        multictl_output_slot* slot2 = &slots[ i+1 ];
    		        *slot1 = *slot2;
    		    }
    		    else
    		    {
			reset_output_slot( slot1 );
    		    }
		}
    	    }
    	    else
    	    {
    	        if( slot_n < MULTICTL_SLOTS - 1 )
    	        {
    	    	    for( int i = MULTICTL_SLOTS - 1; i > slot_n; i-- )
    		    {
    		        multictl_output_slot* slot1 = &slots[ i ];
    		        multictl_output_slot* slot2 = &slots[ i-1 ];
    		        *slot1 = *slot2;
    		    }
    		    slots[ slot_n+1 ].flags |= 1;
    		}
	    }
	    SUNVOX_SOUND_STREAM_CONTROL( sv, SUNVOX_STREAM_UNLOCK );
            data->ui_links_reinit_request = true;
	    mod->full_redraw_request++;
	    draw_window( data->win, wm );
	    data->pnet->change_counter++;
	}
    }
    return 0;
}
static int multictl_reset_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    multictl_curve_reset( data->mod_num, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
static int multictl_smooth_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    int size = MULTICTL_CURVE_POINTS;
    uint16_t* curve = multictl_get_curve( data->mod_num, data->pnet );
    if( !curve ) return 0;
    uint16_t* temp = (uint16_t*)smem_new( size * sizeof( uint16_t ) );
    for( int a = 0; a < 2; a++ )
    {
        for( int i = 1; i < size - 1; i++ )
        {
            int v = curve[ i - 1 ] + curve[ i ] + curve[ i + 1 ];
            v /= 3;
            temp[ i ] = v;
        }
        smem_copy( &curve[ 1 ], &temp[ 1 ], ( size - 2 ) * sizeof( uint16_t ) );
    }
    smem_free( temp );
    draw_window( data->win, wm );
    data->pnet->change_counter++;
    return 0;
}
static int multictl_load_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    uint size = MULTICTL_CURVE_POINTS;
    uint16_t* curve = multictl_get_curve( data->mod_num, data->pnet );
    if( !curve ) return 0;
    char ts[ 512 ];
    sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_LOAD_RAW_DATA ), size );
    char* name = fdialog( ts, 0, ".sunvox_loadcurve_mc", 0, FDIALOG_FLAG_LOAD, wm );
    if( name )
    {
        sfs_file f = sfs_open( name, "rb" );
        if( f )
        {
            sfs_read( curve, 1, size * sizeof( uint16_t ), f );
            sfs_close( f );
	    data->pnet->change_counter++;
        }
    }
    draw_window( data->win, wm );
    return 0;
}
static int multictl_save_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    uint size = MULTICTL_CURVE_POINTS;
    uint16_t* curve = multictl_get_curve( data->mod_num, data->pnet );
    if( !curve ) return 0;
    char ts[ 512 ];
    sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_SAVE_RAW_DATA ), size );
    char* name = fdialog( ts, "curve16bit", ".sunvox_savecurve_mc", 0, 0, wm );
    if( name )
    {
        sfs_file f = sfs_open( name, "wb" );
        if( f )
        {
            sfs_write( curve, 1, size * sizeof( uint16_t ), f );
            sfs_close( f );
        }
    }
    draw_window( data->win, wm );
    return 0;
}
static int multictl_set_dialog_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_item* dlist = dialog_get_items( win );
    WINDOWPTR visual_win = (WINDOWPTR)user_data;
    if( visual_win->flags & WIN_FLAG_TRASH ) return 1;
    if( visual_win->win_handler != multictl_visual_handler ) return 1;
    if( visual_win->id != dialog_get_par( win, 0 ) ) return 1;
    multictl_visual_data* data = (multictl_visual_data*)visual_win->data;
    int rv = 1; 
    if( win->action_result == 0 )
    {
	data->set_val = dialog_get_item( dlist, 'val ' )->int_val;
        int y = data->set_val;
        int x1 = dialog_get_item( dlist, 'from' )->int_val;
	int x2 = dialog_get_item( dlist, 'to  ' )->int_val;
        data->set_start_val = x1;
        data->set_end_val = x2;
        data->set_repeat = dialog_get_item( dlist, 'rept' )->int_val;
        if( x1 >= 0 && x2 >= 0 )
        {
            if( x1 > x2 )
            {
                int temp = x1;
                x1 = x2;
                x2 = temp;
            }
            int len = x2 - x1 + 1;
            if( data->set_repeat )
            {
                int i = x1;
                while( 1 )
                {
                    i -= data->set_repeat;
                    if( i + len <= 0 ) break;
                }
                while( 1 )
                {
                    multictl_curve_set_vals( data->mod_num, data->pnet, i, y, len );
                    i += data->set_repeat;
                    if( i >= MULTICTL_CURVE_POINTS ) break;
                }
            }
            else
            {
                multictl_curve_set_vals( data->mod_num, data->pnet, x1, y, len );
            }
        }
        data->pnet->change_counter++;
        draw_window( visual_win, wm );
    }
    return rv;
}
static int multictl_set_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multictl_visual_data* data = (multictl_visual_data*)user_data;
    dialog_item* dlist = nullptr;
    WINDOWPTR dwin = nullptr;
    while( 1 )
    {
        dialog_item* di = nullptr;
	di = dialog_new_item( &dlist ); if( !di ) break;
        di->type = DIALOG_ITEM_SLIDER;
        di->min = 0;
        di->max = MULTICTL_CURVE_POINTS - 1;
        di->str_val = (char*)ps_get_string( STR_PS_MULTICTL_START_X );
        di->int_val = data->set_start_val;
        di->id = 'from';
        di = dialog_new_item( &dlist ); if( !di ) break;
        di->type = DIALOG_ITEM_SLIDER;
        di->min = 0;
        di->max = MULTICTL_CURVE_POINTS - 1;
        di->str_val = (char*)ps_get_string( STR_PS_MULTICTL_END_X );
        di->int_val = data->set_end_val;
        di->id = 'to  ';
        di = dialog_new_item( &dlist ); if( !di ) break;
        di->type = DIALOG_ITEM_SLIDER;
        di->min = 0;
        di->max = 32768;
        di->str_val = (char*)ps_get_string( STR_PS_MULTICTL_Y );
        di->int_val = data->set_val;
        di->id = 'val ';
        di = dialog_new_item( &dlist ); if( !di ) break;
        di->type = DIALOG_ITEM_SLIDER;
        di->min = 0;
        di->max = MULTICTL_CURVE_POINTS - 1;
        di->str_val = (char*)ps_get_string( STR_PS_REPEAT_WITH_PERIOD );
        di->int_val = data->set_repeat;
        di->id = 'rept';
        wm->opt_dialog_items = dlist;
        dwin = dialog_open( ps_get_string( STR_PS_SET ), nullptr, wm_get_string( STR_WM_OKCANCEL ), DIALOG_FLAG_SINGLE, wm ); 
        if( !dwin ) break;
        set_handler( dwin->childs[ 0 ], multictl_set_dialog_handler, data->win, wm );
        dialog_set_flags( dwin, DIALOG_FLAG_AUTOREMOVE_ITEMS );
        dialog_set_par( dwin, 0, data->win->id );
        dlist = nullptr; 
        break;
    }
    smem_free( dlist );
    return 0;
}
static void multictl_handle_reinit_reqs( multictl_visual_data* data )
{
    if( !data->ui_links_reinit_request ) return;
    psynth_module* mod = NULL;
    if( data->mod_num >= 0 )
	mod = &data->pnet->mods[ data->mod_num ];
    multictl_output_slot* slots = data->module_data->slots;
    if( !mod || !slots ) return;
    window_manager* wm = data->win->wm;
    psynth_net* pnet = data->pnet;
    data->ui_links_reinit_request = false;
    int win_num = 0;
    WINDOWPTR last_ctl_win = 0;
    FOR_EACH_SLOT
    	scrollbar_set_parameters( data->win_min[ win_num ], slot->min, 32768, 1, 1, wm );
    	scrollbar_set_parameters( data->win_max[ win_num ], slot->max, 32768, 1, 1, wm );
    	scrollbar_set_parameters( data->win_ctl[ win_num ], slot->ctl, 255, 1, 1, wm );
    	scrollbar_set_normal_value( data->win_min[ win_num ], 32768, wm );
    	scrollbar_set_normal_value( data->win_max[ win_num ], 32768, wm );
    	scrollbar_set_normal_value( data->win_ctl[ win_num ], 255, wm );
    	scrollbar_set_hex_mode( data->win_ctl[ win_num ], scrollbar_hex_normal, wm );
    	if( slot->flags & 1 )
    	    rename_window( data->win_add[ win_num ], "-" );
    	else
    	    rename_window( data->win_add[ win_num ], "+" );
    	show_window2( data->win_min[ win_num ] );
    	show_window2( data->win_max[ win_num ] );
    	show_window2( data->win_ctl[ win_num ] );
    	show_window2( data->win_add[ win_num ] );
    	data->win_slot_num[ win_num ] = slot_n;
    	win_num++;
    }
    if( win_num > 0 )
    {
	last_ctl_win = data->win_min[ win_num - 1 ];
    }
    for( ; win_num < MULTICTL_SLOTS; win_num++ )
    {
    	hide_window2( data->win_min[ win_num ] );
    	hide_window2( data->win_max[ win_num ] );
    	hide_window2( data->win_ctl[ win_num ] );
    	hide_window2( data->win_add[ win_num ] );
    }
    int btn_y;
    if( last_ctl_win )
	btn_y = last_ctl_win->y + last_ctl_win->ysize + wm->interelement_space;
    else
	btn_y = 0;
    data->reset->y = btn_y;
    data->smooth->y = btn_y;
    data->load->y = btn_y;
    data->save->y = btn_y;
    data->set->y = btn_y;
    mod->visual_min_ysize = btn_y + data->reset->ysize + wm->interelement_space + MAX_CURVE_YSIZE;
}
static int multictl_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    multictl_visual_data* data = (multictl_visual_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( multictl_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
    		win->flags |= WIN_FLAG_BEFORERESIZE_ENABLED;
    		data->win = win;
    		data->pnet = (psynth_net*)win->host;
        	data->module_data = NULL;
        	data->mod_num = -1;
        	data->ui_links_reinit_request = true;
        	data->set_start_val = 0;
        	data->set_end_val = MULTICTL_CURVE_POINTS - 1;
        	data->set_val = 32768;
        	data->set_repeat = 0;
        	int ychar = font_char_y_size( win->font, wm );
        	int y = ychar + wm->interelement_space + ychar;
        	for( int i = 0; i < MULTICTL_SLOTS; i++ )
        	{
        	    WINDOWPTR w;
            	    wm->opt_scrollbar_compact = true;
            	    w = new_window( "Min", 0, y, 32, wm->controller_ysize, wm->scroll_color, win, scrollbar_handler, wm );
            	    scrollbar_set_name( w, sv_get_string( STR_SV_MIN ), wm );
            	    set_handler( w, multictl_slot_min_handler, (void*)data, wm );
            	    w->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
            	    data->win_min[ i ] = w;
            	    wm->opt_scrollbar_compact = true;
            	    w = new_window( "Max", 0, y, 32, wm->controller_ysize, wm->scroll_color, win, scrollbar_handler, wm );
            	    scrollbar_set_name( w, sv_get_string( STR_SV_MAX ), wm );
            	    set_handler( w, multictl_slot_max_handler, (void*)data, wm );
            	    w->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
            	    data->win_max[ i ] = w;
            	    wm->opt_scrollbar_compact = true;
            	    w = new_window( "Controller", 0, y, 32, wm->controller_ysize, wm->scroll_color, win, scrollbar_handler, wm );
            	    scrollbar_set_name( w, sv_get_string( STR_SV_CONTROLLER ), wm );
            	    set_handler( w, multictl_slot_ctl_handler, (void*)data, wm );
            	    w->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
            	    data->win_ctl[ i ] = w;
            	    wm->opt_button_flags = BUTTON_FLAG_FLAT;
            	    w = new_window( "+", 0, y, 32, wm->controller_ysize, wm->scroll_color, win, button_handler, wm );
            	    set_handler( w, multictl_slot_add_handler, (void*)data, wm );
            	    w->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
            	    data->win_add[ i ] = w;
            	    data->win_slot_num[ i ] = -1;
        	    y += ychar + wm->controller_ysize + wm->interelement_space;
            	}
            	data->pushed = 0;
            	data->curve_y = 0;
                data->curve_ysize = 0;
            	const char* bname;
                int bxsize;
                int bysize = wm->text_ysize;
                int x = 0;
                bname = wm_get_string( STR_WM_RESET ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->reset = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->reset, multictl_reset_handler, data, wm );
                x += bxsize + wm->interelement_space2;
                bname = ps_get_string( STR_PS_SMOOTH ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT;
                data->smooth = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->smooth, multictl_smooth_handler, data, wm );
                x += bxsize + wm->interelement_space2;
                bname = wm_get_string( STR_WM_LOAD ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->load = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->load, multictl_load_handler, data, wm );
                x += bxsize + wm->interelement_space2;
                bname = wm_get_string( STR_WM_SAVE ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->save = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->save, multictl_save_handler, data, wm );
                x += bxsize + wm->interelement_space2;
                bname = ps_get_string( STR_PS_SET ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->set = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->set, multictl_set_handler, data, wm );
                x += bxsize + wm->interelement_space2;
            }
            retval = 1;
            break;
        case EVT_MOUSEBUTTONDOWN:
            if( rx >= 0 && rx < win->xsize && ry >= data->curve_y && ry < data->curve_y + data->curve_ysize )
            {
                data->pushed = 1;
            }
        case EVT_MOUSEMOVE:
            if( data->pushed && data->curve_ysize > 0 && win->xsize > 0 )
            {
                int x_items = MULTICTL_CURVE_POINTS;
            	int max_val = 32768;
            	uint16_t* v = multictl_get_curve( data->mod_num, data->pnet );
                int i = ( ( rx - wm->scrollbar_size / 2 ) * x_items ) / ( win->xsize - wm->scrollbar_size );
                if( i < 0 ) i = 0;
                if( i >= x_items ) i = x_items - 1;
                int val = ( ( data->curve_ysize - ( ry - data->curve_y ) ) * max_val ) / data->curve_ysize;
                if( val < 0 ) val = 0;
                if( val > max_val ) val = max_val;
                if( evt->type == EVT_MOUSEBUTTONDOWN )
                {
                    v[ i ] = val;
                }
                else
                {
                    if( i != data->prev_i )
                    {
                        int val2;
                        int dv;
                        int i2;
                        int i2_end;
                        if( i > data->prev_i )
                        {
                            i2 = data->prev_i;
                            i2_end = i;
                            val2 = data->prev_val << 15;
                            dv = ( ( val - data->prev_val ) << 15 ) / ( i - data->prev_i );
                        }
                        else
                        {
                            i2 = i;
                            i2_end = data->prev_i;
                            val2 = val << 15;
                            dv = ( ( data->prev_val - val ) << 15 ) / ( data->prev_i - i );
                        }
                        for( ; i2 < i2_end; i2 ++ )
                        {
                            if( (unsigned)i2 < (unsigned)x_items )
                                v[ i2 ] = val2 >> 15;
                            val2 += dv;
                        }
                    }
                    else
                    {
                        v[ i ] = val;
                    }
                }
                data->prev_i = i;
                data->prev_val = val;
                draw_window( win, wm );
                data->pnet->change_counter++;
            }
            retval = 1;
            break;
        case EVT_MOUSEBUTTONUP:
            data->pushed = 0;
            retval = 1;
            break;
        case EVT_BEFORERESIZE:
    	    for( int i = 0; i < MULTICTL_SLOTS; i++ )
    	    {
        	int slot_n = data->win_slot_num[ i ];
        	if( slot_n < 0 ) continue;
        	int xsize = win->xsize - wm->controller_ysize;
        	WINDOWPTR w1 = data->win_min[ i ];
        	w1->x = 0;
        	w1->xsize = xsize / 3;
        	WINDOWPTR w2 = data->win_max[ i ];
        	w2->x = w1->x + w1->xsize + wm->interelement_space;
        	w2->xsize = xsize / 3 - wm->interelement_space;
        	WINDOWPTR w3 = data->win_ctl[ i ];
        	w3->x = w2->x + w2->xsize + wm->interelement_space;
        	w3->xsize = xsize / 3 - wm->interelement_space;
        	WINDOWPTR w4 = data->win_add[ i ];
        	w4->x = w3->x + w3->xsize + wm->interelement_space;
        	w4->xsize = win->xsize - w4->x;
    	    }
    	    retval = 1;
    	    break;
        case EVT_REINIT:
	    multictl_handle_reinit_reqs( data );
            retval = 1;
    	    break;
        case EVT_DRAW:
    	    {
            	psynth_module* mod = NULL;
            	if( data->mod_num >= 0 )
            	    mod = &data->pnet->mods[ data->mod_num ];
		multictl_output_slot* slots = data->module_data->slots;
		if( !mod || !slots ) break;
		psynth_net* pnet = data->pnet;
		multictl_handle_reinit_reqs( data );
		uint16_t* val_curve = multictl_get_curve( data->mod_num, data->pnet );
    		wbd_lock( win );
                draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		{
		    wm->cur_font_color = wm->color2;
		    int links = 0;
            	    int ychar = char_y_size( wm );
            	    int y = ychar + wm->interelement_space;
		    FOR_EACH_SLOT
                    	links++;
		        const char* str_dot = ".";
		        int str_dot_xsize = string_x_size( str_dot, wm );
		        int x = 0;
	        	{
	        	    char ts[ 16 ];
	        	    hex_int_to_string( l, ts );
	        	    draw_string( ts, 0, y, wm ); x = string_x_size( ts, wm );
                    	    draw_string( str_dot, x, y, wm ); x += str_dot_xsize;
	        	}
	        	draw_string( m->name, x, y, wm ); x += string_x_size( m->name, wm );
		        if( slot->ctl != 0 ) 
		        {
                            char ctl_val[ 64 ];
                            char ctl_name_ts[ 32 ];
                            const char* ctl_name = NULL;
                            int val = multictl_get_val( data->module_data->ctl_val, val_curve, slot, data->module_data );
                            int off = data->module_data->ctl_offset - 16384;
			    if( (unsigned)slot->ctl <= m->ctls_num )
			    {
		        	int cnum = slot->ctl - 1;
                        	psynth_ctl* ctl = &m->ctls[ cnum ];
				ctl_name = ctl->name;
                        	if( ctl->type == 0 )
                        	{
                        	    int range = ctl->max - ctl->min;
                            	    sprintf( ctl_val, " = %d / ", ctl->min + ctl->show_offset + ( val * range ) / 32768 );
                        	}
                        	else
                        	{
                        	    if( val < ctl->min ) val = ctl->min;
                        	    if( val > ctl->max ) val = ctl->max;
                        	    sprintf( ctl_val, " = %d / ", val + ctl->show_offset );
                        	}
                            }
                            else
                            {
                        	if( slot->ctl >= 0x80 )
                        	{
                            	    int midi_ctl = slot->ctl - 0x80;
                            	    sprintf( ctl_name_ts, "MIDI %d", midi_ctl );
                            	    ctl_name = ctl_name_ts;
                                    int range = 128;
                                    if( midi_ctl < 32 ) range = 16384; 
                                    int v = ( val * range ) / 32768;
                                    sprintf( ctl_val, " = %d / ", v );
                            	}
                            }
                            if( ctl_name )
                            {
                            	draw_string( str_dot, x, y, wm ); x += str_dot_xsize;
                            	draw_string( ctl_name, x, y, wm ); x += string_x_size( ctl_name, wm );
                            	draw_string( ctl_val, x, y, wm ); x += string_x_size( ctl_val, wm );
                            	psynth_draw_ctl_info(
				    l,
				    data->pnet,
				    slot->ctl,
				    slot->min,
				    slot->max,
			    	    off,
				    x, y,
				    DRAW_CTL_INFO_FLAG_RANGEONLY,
				    win );
			    }
			}
		        y += ychar + wm->controller_ysize + wm->interelement_space;
    	    	    }
    	    	    if( links )
			draw_string( ps_get_string( STR_PS_SEND_VALUE_TO ), 0, 0, wm );
    	    	}
    	    	{
            	    COLOR color = blend( wm->color2, win->color, 150 );
            	    COLOR c2 = blend( wm->color2, win->color, 170 );
            	    uint16_t* v = val_curve;
            	    int x_items = MULTICTL_CURVE_POINTS;
            	    int max_val = 32768;
            	    int xstart = ( wm->scrollbar_size / 2 ) << 15;
            	    int x = xstart;
            	    int y = data->reset->y + data->reset->ysize + wm->interelement_space;
            	    int curve_xsize = win->xsize - wm->scrollbar_size;
            	    int xd = ( curve_xsize << 15 ) / x_items;
            	    int curve_ysize = psynth_get_curve_ysize( y, MAX_CURVE_YSIZE, win );
            	    data->curve_y = y;
            	    data->curve_ysize = curve_ysize;
            	    if( curve_ysize > 0 && curve_xsize > 0 )
            	    {
                	for( int i = 0; i < x_items; i++ )
                	{
                    	    int val = v[ i ];
                    	    int ysize = ( curve_ysize * val ) / max_val;
                    	    draw_frect( x >> 15, y + curve_ysize - ysize, ( ( x + xd ) >> 15 ) - ( x >> 15 ), ysize, color, wm );
                    	    if( i == 0 )
                    	    {
                        	draw_frect( 0, y + curve_ysize - ysize, ( x >> 15 ) - 1, ysize, color, wm );
                    	    }
                    	    if( i == x_items - 1 )
                    	    {
                        	draw_frect( ( ( x + xd ) >> 15 ) + 1, y + curve_ysize - ysize, wm->scrollbar_size, ysize, color, wm );
                    	    }
                    	    x += xd;
                	}
                	psynth_draw_grid( xstart >> 15, y, ( ( xstart + xd * x_items ) >> 15 ) - ( xstart >> 15 ), curve_ysize, win );
                	wm->cur_font_color = wm->color2;
                	draw_string( ps_get_string( STR_PS_CURVE ), 0, y, wm );
                	draw_string( ps_get_string( STR_PS_CURVE_MULTICTL ), 0, y + char_y_size( wm ), wm );
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
        case EVT_BEFORECLOSE:
            retval = 1;
            break;
    }
    return retval;
}
#endif
static void multictl_create_curve( int mod_num, psynth_net* pnet )
{
    if( psynth_get_chunk_data( mod_num, 1, pnet ) == 0 )
    {
	psynth_new_chunk( mod_num, 1, MULTICTL_CURVE_POINTS * sizeof( uint16_t ), 0, 0, pnet );
	multictl_curve_reset( mod_num, pnet );
    }
}
static void multictl_curve_reset( int mod_num, psynth_net* pnet )
{
    uint16_t* curve = (uint16_t*)psynth_get_chunk_data( mod_num, 1, pnet );
    if( curve )
    {
        for( int i = 0; i < MULTICTL_CURVE_POINTS; i++ )
        {
            curve[ i ] = i * 128;
        }
	pnet->change_counter++;
    }
}
static uint16_t* multictl_get_curve( int mod_num, psynth_net* pnet )
{
    return (uint16_t*)psynth_get_chunk_data( mod_num, 1, pnet );
}
static int multictl_curve_val( int mod_num, psynth_net* pnet, int pos, int val, int w )
{
    int rv = 0;
    uint16_t* c = multictl_get_curve( mod_num, pnet );
    if( !c ) return 0;
    int len = MULTICTL_CURVE_POINTS;
    if( (unsigned)pos >= (unsigned)len ) return 0;
    rv = c[ pos ];
    if( w )
    {
	c[ pos ] = val;
	pnet->change_counter++;
    }
    return rv;
}
static void multictl_curve_set_vals( int mod_num, psynth_net* pnet, int pos, int val, int vals )
{
    for( int i = pos; i < pos + vals; i++ )
        multictl_curve_val( mod_num, pnet, i, val, 1 );
}
static int multictl_get_curve_val( int input, uint16_t* curve )
{
    int output;
    int c1 = input & 127;
    int c2 = 128 - c1;
    if( input < 32768 )
        output = ( (int)curve[ input >> 7 ] * c2 + (int)curve[ ( input >> 7 ) + 1 ] * c1 ) >> 7;
    else
        output = curve[ 256 ];
    return output;
}
static int multictl_get_val( int val, uint16_t* val_curve, multictl_output_slot* slot, MODULE_DATA* data )
{
    if( val_curve )
    {
        if( slot->min < slot->max )
        {
    	    val = multictl_get_curve_val( val, val_curve );
	}
	else
	{
	    val = 32768 - multictl_get_curve_val( 32768 - val, val_curve );
	}
    }
    val = ( val * data->ctl_gain ) / 256;
    if( val > 32768 ) val = 32768;
    int range = slot->max - slot->min;
    if( data->ctl_quant < 32768 )
    {
        int quant = data->ctl_quant - 1; 
	if( quant < 1 ) quant = 1;
	int step_n = val * quant / 32768; 
	int val2 = step_n * 32768 / quant; 
        val = slot->min + ( range * val2 ) / 32768;
    }
    else
    {
        val = slot->min + ( range * val ) / 32768;
    }
    val += ( data->ctl_offset - 16384 );
    if( val < 0 ) val = 0;
    if( val > 32768 ) val = 32768;
    return val;
}
static void multictl_calc_pars( MODULE_DATA* data, psynth_net* pnet )
{
    data->tick_size = 1;
    if( data->ctl_freq > 0 )
        data->tick_size = ( pnet->sampling_freq * 256 ) / data->ctl_freq;
    if( data->tick_size <= 0 ) data->tick_size = 1;
}
static void multictl_floating_step( MODULE_DATA* data, float response )
{
    data->floating_val = ( 1 - response ) * data->floating_val + response * (float)data->ctl_val;
}
static void multictl_send( int mod_num, psynth_net* pnet, int val, int offset )
{
    psynth_module* mod = &pnet->mods[ mod_num ];
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    uint16_t* val_curve = multictl_get_curve( mod_num, pnet );
    multictl_output_slot* slots = data->slots;
    FOR_EACH_SLOT
	if( slot->ctl != 0 ) 
	{
    	    data->ctl_evt.offset = offset;
    	    data->ctl_evt.controller.ctl_num = slot->ctl - 1;
    	    data->ctl_evt.controller.ctl_val = multictl_get_val( val, val_curve, slot, data );
    	    psynth_add_event( l, &data->ctl_evt, pnet );
	}
    }
}
#define VAL_FORMULA_DESC "out=quant(curve(in))+offset"
PS_RETTYPE MODULE_HANDLER( 
    PSYNTH_MODULE_HANDLER_PARAMETERS
    )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
	mod = &pnet->mods[ mod_num ];
	data = (MODULE_DATA*)mod->data_ptr;
    }
    PS_RETTYPE retval = 0;
    switch( event->command )
    {
	case PS_CMD_GET_DATA_SIZE:
	    retval = sizeof( MODULE_DATA );
	    break;
	case PS_CMD_GET_NAME:
	    retval = (PS_RETTYPE)"MultiCtl";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)
"MultiCtl позволяет управлять несколькими контроллерами (на разных модулях) одновременно, изменяя значение всего одного базового контроллера. "
"Макс. кол-во управляемых контроллеров - 16.\n"
"Формула: " VAL_FORMULA_DESC;
                        break;
                    }
		    retval = (PS_RETTYPE)
"With MultiCtl you can change the values of multiple controllers (in different modules) at once. "
"Maximum number of connected controllers - 16.\n"
"Formula: " VAL_FORMULA_DESC;
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#DEFF00";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_OUTPUT_IS_EMPTY; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 6, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VALUE ), "", 0, 32768, 0, 0, &data->ctl_val, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_GAIN ), "", 0, 1024, 256, 0, &data->ctl_gain, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_QUANTIZATION ), ps_get_string( STR_PS_LEVELS ), 0, 32768, 32768, 0, &data->ctl_quant, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_OFFSET ), "", 0, 32768, 16384, 0, &data->ctl_offset, 16384, 0, pnet );
            psynth_set_ctl_show_offset( mod_num, 3, -16384, pnet );
            psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESPONSE ), "", 0, MAX_RESPONSE, MAX_RESPONSE * 1, 0, &data->ctl_response, -1, 1, pnet );
            psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_RATE ), ps_get_string( STR_PS_HZ ), 1, 32768, 150, 0, &data->ctl_freq, 50, 1, pnet );
            psynth_set_ctl_flags( mod_num, 5, PSYNTH_CTL_FLAG_EXP3, pnet );
	    smem_clear( &data->ctl_evt, sizeof( psynth_event ) );
            data->ctl_evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
            data->slots = nullptr;
	    data->floating_val = data->ctl_val;
	    data->tick_counter = 0;
	    data->pars_recalc_request = 0;
	    multictl_calc_pars( data, pnet );
#ifdef SUNVOX_GUI
            {
        	data->wm = nullptr;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "MultiCtl GUI", 0, 0, 10, 10, wm->button_color, 0, pnet, multictl_visual_handler, wm );
            	    mod->visual_min_ysize = 0;
            	    multictl_visual_data* data1 = (multictl_visual_data*)mod->visual->data;
            	    data1->module_data = data;
            	    data1->mod_num = mod_num;
            	    data1->pnet = pnet;
            	}
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		data->slots = (multictl_output_slot*)psynth_get_chunk_data( mod_num, 0, pnet );
		if( !data->slots )
		{
		    psynth_new_chunk( mod_num, 0, MULTICTL_SLOTS * sizeof( multictl_output_slot ), 0, 0, pnet );
		    data->slots = (multictl_output_slot*)psynth_get_chunk_data( mod_num, 0, pnet );
		    if( data->slots )
		    {
			for( int i = 0; i < MULTICTL_SLOTS; i++ )
			{
			    reset_output_slot( &data->slots[ i ] );
			}
		    }
		}
		multictl_create_curve( mod_num, pnet );
		data->floating_val = data->ctl_val;
		multictl_calc_pars( data, pnet );
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    data->floating_val = data->ctl_val;
	    data->tick_counter = 0;
#ifdef SUNVOX_GUI
	    {
        	psynth_ctl* ctl = &mod->ctls[ 0 ];
        	if( ctl->normal_value != ctl->max )
            	{
            	    ctl->normal_value = ctl->max;
            	    mod->draw_request++;
            	}
            }
#endif
	    retval = 1;
	    break;
#ifdef SUNVOX_GUI
	case PS_CMD_OUTPUT_LINKS_CHANGED:
	    {
		if( mod->visual )
		{
        	    multictl_visual_data* data1 = (multictl_visual_data*)mod->visual->data;
            	    data1->ui_links_reinit_request = true;
        	}
		mod->full_redraw_request++;
	    }
	    retval = 1;
	    break;
#endif
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
            if( event->controller.ctl_num < 4 )
            {
        	retval = 1;
        	psynth_set_ctl2( mod, event );
    	    	if( data->ctl_response >= MAX_RESPONSE )
    	    	{
		    data->floating_val = data->ctl_val;
		    multictl_send( mod_num, pnet, data->ctl_val, event->offset );
            	}
            }
            else
    	    {
    		if( event->controller.ctl_num == 5 ) 
		    data->pars_recalc_request++;
	    }
            break;
        case PS_CMD_RENDER_REPLACE:
            {
		if( data->pars_recalc_request )
		{
	    	    data->pars_recalc_request = 0;
		    multictl_calc_pars( data, pnet );
		}
	        if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
    	    	if( data->ctl_response >= MAX_RESPONSE ) 
    	    	{
#ifdef SUNVOX_GUI
            	    psynth_ctl* ctl = &mod->ctls[ 0 ];
            	    if( ctl->normal_value != ctl->max )
            	    {
                	ctl->normal_value = ctl->max;
                	mod->draw_request++;
            	    }
#endif
    	    	    break;
    	    	}
    	    	int offset = mod->offset;
                int frames = mod->frames;
                int r = data->ctl_response;
                if( r < 1 ) r = 1;
                float response = (float)r / (float)MAX_RESPONSE;
                response *= response;
		int ptr = 0;
                while( 1 )
                {
                    int buf_size = frames - ptr;
                    if( buf_size > ( data->tick_size - data->tick_counter ) / 256 ) buf_size = ( data->tick_size - data->tick_counter ) / 256;
                    if( ( data->tick_size - data->tick_counter ) & 255 ) buf_size++; 
                    if( buf_size > frames - ptr ) buf_size = frames - ptr;
                    if( buf_size < 0 ) buf_size = 0;
                    ptr += buf_size;
                    data->tick_counter += buf_size * 256;
                    if( data->tick_counter >= data->tick_size )
                    {
                        data->tick_counter %= data->tick_size;
			int prev_val = data->floating_val;
			multictl_floating_step( data, response );
			if( prev_val != (int)data->floating_val )
			{
			    int offset2;
                            if( ptr > 0 )
                                offset2 = offset + ptr - 1;
                            else
                                offset2 = offset;
			    multictl_send( mod_num, pnet, data->floating_val, offset2 );
			}
                    }
                    if( ptr >= frames ) break;
            	}
#ifdef SUNVOX_GUI
            	psynth_ctl* ctl = &mod->ctls[ 0 ];
            	int new_normal_val = data->floating_val;
            	if( ctl->normal_value != new_normal_val )
            	{
            	    ctl->normal_value = new_normal_val;
            	    mod->draw_request++;
            	}
#endif
	    }
	    break;
	case PS_CMD_CLOSE:
	    data->slots = nullptr;
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = nullptr;
    	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_READ_CURVE:
        case PS_CMD_WRITE_CURVE:
            {
                uint16_t* curve = NULL;
                int chunk_id = -1;
                int len = 0;
                int reqlen = event->offset;
                switch( event->id )
                {
                    case 0: chunk_id = 1; len = MULTICTL_CURVE_POINTS; break;
                    default: break;
                }
                if( chunk_id >= 0 )
                {
                    curve = (uint16_t*)psynth_get_chunk_data( mod_num, chunk_id, pnet );
                    if( curve && event->curve.data )
                    {
                        if( reqlen == 0 ) reqlen = len;
                        if( reqlen > len ) reqlen = len;
                        if( event->command == PS_CMD_READ_CURVE )
                            for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve[ i ] / 32768.0F; }
                        else
                            for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * 32768.0F; LIMIT_NUM( v, 0, 32768 ); curve[ i ] = v; }
                        retval = reqlen;
                    }
                }
            }
            break;
	default: break;
    }
    return retval;
}
