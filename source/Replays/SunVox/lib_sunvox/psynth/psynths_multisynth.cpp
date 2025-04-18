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
#define MODULE_DATA	psynth_multisynth_data
#define MODULE_HANDLER	psynth_multisynth
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	0
#define MAX_CHANNELS	64
#define HISTORY_SIZE	64
#define GET_EVENT psynth_event e = *event; PSYNTH_EVT_ID_INC( e.id, mod_num );
#define OPT_FLAG_GEN_MISSED_NOTEOFF	( 1 << 0 ) 
#define OPT_FLAG_ROUND_PITCH		( 1 << 1 ) 
#define OPT_FLAG_ROUND_PITCH2		( 1 << 2 ) 
#define OPT_FLAG_REC_CURVE3		( 1 << 3 ) 
#define OPT_FLAG_NOTE_DIFF		( 1 << 5 ) 
#define OPT_FLAG_OUTSLOT_offset		6
#define OPT_FLAG_OUTSLOT_NOTE		( 1 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAG_OUTSLOT_POLYCHANNEL	( 2 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAG_OUTSLOT_ROUNDROBIN	( 3 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAG_OUTSLOT_RANDOM1	( 4 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAG_OUTSLOT_RANDOM2	( 5 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAG_OUTSLOT_RANDOM3	( 6 << OPT_FLAG_OUTSLOT_offset ) 
#define OPT_FLAGS_OUTSLOT ( 15 << OPT_FLAG_OUTSLOT_offset )
struct multisynth_options
{
    uint8_t	static_freq; 
    uint8_t	ignore_vel0; 
    uint8_t	curve_num; 
    uint8_t	trigger; 
    uint32_t	flags;
};
#define CHAN_FLAG_NOTEOFF_REQUEST	( 1 << 0 )
#define CHAN_FLAG_FROM_HISTORY		( 1 << 1 ) 
struct ms_channel
{
    uint32_t	id;
    int 	pitch;
    int		original_pitch;
#ifdef SUNVOX_GUI
    uint16_t	vel;
#endif
    uint8_t	out; 
    uint8_t	flags;
};
struct history_item
{
    uint32_t	id;
    int 	pitch;
    uint8_t	out; 
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_transpose;
    PS_CTYPE   	ctl_random;
    PS_CTYPE   	ctl_velocity;
    PS_CTYPE   	ctl_finetune;
    PS_CTYPE   	ctl_random_phase;
    PS_CTYPE   	ctl_random_velocity;
    PS_CTYPE	ctl_phase;
    PS_CTYPE	ctl_curve2_mix;
    uint    	rand1;
    uint    	rand2;
    multisynth_options* opt;
    ms_channel	channels[ MAX_CHANNELS + 1 ]; 
    int		active_chans; 
    int		outslot_cnt; 
    int		prev_random_outslot;
    uint32_t	curve_changed;
    int		rec_curve3_cnt;
    history_item	hist[ HISTORY_SIZE ]; 
    uint		hist_wp; 
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#define NUM_CURVES 3
const int g_curve_chunk_id[ NUM_CURVES ] = { 0, 2, 3 };
const int g_curve_item_size[ NUM_CURVES ] = { 1, 1, 2 }; 
const int g_curve_len[ NUM_CURVES ] = { 128, 257, 128 };
static void reset_curve( int num, int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    int len = g_curve_len[ num ];
    uint8_t* curve = (uint8_t*)psynth_get_chunk_data( mod_num, g_curve_chunk_id[ num ], pnet );
    if( !curve ) return;
    switch( num )
    {
	case 0:
	    {
		memset( curve, 255, len );
	    }
	    break;
	case 1:
	    {
		for( int i = 0; i < len; i++ ) 
		{
		    int v;
		    if( i < 256 )
		        v = i;
		    else
		        v = 255;
		    curve[ i ] = v;
		}
	    }
	    break;
	case 2:
	    {
		uint16_t* curve16 = (uint16_t*)curve;
		for( int i = 0; i < len; i++ ) 
		{
		    curve16[ i ] = 16384 + i * 256;
		}
	    }
	    break;
    }
    data->curve_changed &= ~( 1 << num );
    data->opt->flags &= ~OPT_FLAG_REC_CURVE3;
    pnet->change_counter++;
}
static uint8_t* new_curve( int num, int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return NULL;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    int chunk_id = g_curve_chunk_id[ num ];
    int len = g_curve_len[ num ];
    int size = len * g_curve_item_size[ num ];
    uint8_t* curve = (uint8_t*)psynth_get_chunk_data( mod_num, chunk_id, pnet );
    if( curve )
    {
	data->curve_changed |= 1 << num;
    }
    else
    {
	psynth_new_chunk( mod_num, chunk_id, size, 0, 0, pnet );
	reset_curve( num, mod_num, pnet );
	curve = (uint8_t*)psynth_get_chunk_data( mod_num, chunk_id, pnet );
    }
    pnet->change_counter++;
    return curve;
}
static uint8_t* get_curve( int num, size_t* size, int mod_num, psynth_net* pnet )
{
    int chunk_id = g_curve_chunk_id[ num ];
    uint8_t* curve = (uint8_t*)psynth_get_chunk_data( mod_num, chunk_id, pnet );
    psynth_get_chunk_info( mod_num, chunk_id, pnet, size, 0, 0 );
    return curve;
}
static int curve_val( int num, int mod_num, psynth_net* pnet, int pos, int val, int w )
{
    int rv = 0;
    size_t size = 0;
    uint8_t* c = get_curve( num, &size, mod_num, pnet );
    if( !c ) return 0;
    int len = size / g_curve_item_size[ num ];
    if( (unsigned)pos >= (unsigned)len ) return 0;
    switch( num )
    {
	case 0: case 1:
	    rv = c[ pos ];
	    if( w ) c[ pos ] = val;
	    break;
	case 2:
	    rv = ((uint16_t*)c)[ pos ];
	    if( w ) ((uint16_t*)c)[ pos ] = val;
	    break;
    }
    if( w ) pnet->change_counter++;
    return rv;
}
static void curve_set_vals( int num, int mod_num, psynth_net* pnet, int pos, int val, int vals )
{
    for( int i = pos; i < pos + vals; i++ )
	curve_val( num, mod_num, pnet, i, val, 1 );
}
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct multisynth_visual_data
{
    WINDOWPTR		win;
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    bool		pushed;
    int			curve_y;
    int			curve_ysize;
    int 		prev_i;
    int			prev_val;
    int			set_start_vel;
    int			set_end_vel;
    int			set_start_note;
    int			set_end_note;
    int			set_repeat;
    int			set_vel;
    char*		set_freq_or_note;
    WINDOWPTR		opt;
    WINDOWPTR		reset;
    WINDOWPTR		smooth;
    WINDOWPTR		load;
    WINDOWPTR		save;
    WINDOWPTR 		set;
    int			min_ysize;
};
int multisynth_visual_handler( sundog_event* evt, window_manager* wm ); 
int multisynth_opt_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    multisynth_options* opt = data->module_data->opt;
    char* ts = (char*)smem_new( 2048 );
    psynth_opt_menu* m = opt_menu_new( win );
    opt_menu_add( m, STR_PS_USE_STATIC_NOTE_C5, opt->static_freq, 127 );
    opt_menu_add( m, STR_PS_IGNORE_NOTES_WITH_ZERO_VEL, opt->ignore_vel0, 126 );
    opt_menu_add( m, STR_PS_TRIGGER, opt->trigger, 124 );
    opt_menu_add( m, STR_PS_GEN_MISSED_NOTEOFF, ( opt->flags & OPT_FLAG_GEN_MISSED_NOTEOFF ) != 0, 123 );
    opt_menu_add( m, STR_PS_ROUND_PITCH, ( opt->flags & OPT_FLAG_ROUND_PITCH ) != 0, 122 );
    opt_menu_add( m, STR_PS_ROUND_PITCH2, ( opt->flags & OPT_FLAG_ROUND_PITCH2 ) != 0, 121 );
    opt_menu_add( m, STR_PS_REC_CURVE3, ( opt->flags & OPT_FLAG_REC_CURVE3 ) != 0, 120 );
    opt_menu_add( m, STR_PS_NOTE_DIFFERENCE, ( opt->flags & OPT_FLAG_NOTE_DIFF ) != 0, 118 );
    opt_menu_add( m, STR_PS_OUTSLOT_NOTE, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_NOTE, 117 );
    opt_menu_add( m, STR_PS_OUTSLOT_POLYCHANNEL, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_POLYCHANNEL, 116 );
    opt_menu_add( m, STR_PS_OUTSLOT_ROUNDROBIN, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_ROUNDROBIN, 115 );
    opt_menu_add( m, STR_PS_OUTSLOT_RANDOM1, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM1, 114 );
    opt_menu_add( m, STR_PS_OUTSLOT_RANDOM2, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM2, 113 );
    opt_menu_add( m, STR_PS_OUTSLOT_RANDOM3, ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM3, 112 );
    sprintf( ts, "%s1: %s@", ps_get_string( STR_PS_CURVE ), ps_get_string( STR_PS_CURVE_TYPE1 ) );
    opt_menu_add_cstr( m, ts, opt->curve_num == 0, 125 );
    sprintf( ts, "%s2: %s@", ps_get_string( STR_PS_CURVE ), ps_get_string( STR_PS_CURVE_TYPE2 ) );
    opt_menu_add_cstr( m, ts, opt->curve_num == 1, 125 );
    sprintf( ts, "%s3: %s@", ps_get_string( STR_PS_CURVE ), ps_get_string( STR_PS_CURVE_TYPE3 ) );
    opt_menu_add_cstr( m, ts, opt->curve_num == 2, 125 );
    int sel = opt_menu_show( m );
    opt_menu_remove( m );
    smem_free( ts );
    int ctl = -1;
    int ctl_val = 0;
    switch( sel )
    {
        case 0: ctl = 127; ctl_val = !opt->static_freq; break;
        case 1: ctl = 126; ctl_val = !opt->ignore_vel0; break;
        case 2: ctl = 124; ctl_val = !opt->trigger; break;
        case 3: ctl = 123; ctl_val = !( opt->flags & OPT_FLAG_GEN_MISSED_NOTEOFF ); break;
        case 4: ctl = 122; ctl_val = !( opt->flags & OPT_FLAG_ROUND_PITCH ); break;
        case 5: ctl = 121; ctl_val = !( opt->flags & OPT_FLAG_ROUND_PITCH2 ); break;
        case 6: ctl = 120; ctl_val = !( opt->flags & OPT_FLAG_REC_CURVE3 ); break;
        case 7: ctl = 118; ctl_val = !( opt->flags & OPT_FLAG_NOTE_DIFF ); break;
        case 8: ctl = 117; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_NOTE ); break;
        case 9: ctl = 116; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_POLYCHANNEL ); break;
        case 10: ctl = 115; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_ROUNDROBIN ); break;
        case 11: ctl = 114; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM1 ); break;
        case 12: ctl = 113; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM2 ); break;
        case 13: ctl = 112; ctl_val = !( ( opt->flags & OPT_FLAGS_OUTSLOT ) == OPT_FLAG_OUTSLOT_RANDOM3 ); break;
        case 14: ctl = 125; ctl_val = 0; break;
        case 15: ctl = 125; ctl_val = 1; break;
        case 16: ctl = 125; ctl_val = 2; break;
        default: break;
    }
    if( ctl >= 0 ) psynth_handle_ctl_event( data->mod_num, ctl - 1, ctl_val, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
int multisynth_reset_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    reset_curve( data->module_data->opt->curve_num, data->mod_num, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
int multisynth_smooth_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    multisynth_options* opt = data->module_data->opt;
    int len = g_curve_len[ opt->curve_num ];
    int* temp = (int*)smem_new( len * sizeof( int ) );
    for( int a = 0; a < 2; a++ )
    {
        for( int i = 1; i < len - 1; i++ )
        {
    	    int v = 0;
	    v += curve_val( opt->curve_num, data->mod_num, data->pnet, i - 1, 0, 0 );
	    v += curve_val( opt->curve_num, data->mod_num, data->pnet, i, 0, 0 );
	    v += curve_val( opt->curve_num, data->mod_num, data->pnet, i + 1, 0, 0 );
            v /= 3;
            temp[ i ] = v;
        }
        if( len > 2 )
        {
    	    for( int i = 1; i < len - 1; i++ )
    	    {
		curve_val( opt->curve_num, data->mod_num, data->pnet, i, temp[ i ], 1 );
    	    }
    	}
    }
    data->module_data->curve_changed |= 1 << opt->curve_num;
    smem_free( temp );
    draw_window( data->win, wm );
    data->pnet->change_counter++;
    return 0;
}
int multisynth_load_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    multisynth_options* opt = data->module_data->opt;
    size_t size = 0;
    uint8_t* curve = get_curve( opt->curve_num, &size, data->mod_num, data->pnet );
    if( !curve || !size ) return 0;
    char ts[ 512 ];
    if( g_curve_item_size[ opt->curve_num ] == 1 )
	sprintf( ts, "%s (%d x 8bit unsigned int)", ps_get_string( STR_PS_LOAD_RAW_DATA ), (int)size );
    else
	sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_LOAD_RAW_DATA ), (int)size/2 );
    char dialog_id[ 128 ];
    sprintf( dialog_id, ".sunvox_loadcurve_ms%d", opt->curve_num );
    char* name = fdialog( ts, 0, dialog_id, 0, FDIALOG_FLAG_LOAD, wm );
    if( name )
    {
        sfs_file f = sfs_open( name, "rb" );
        if( f )
        {
            sfs_read( curve, 1, size, f );
    	    data->module_data->curve_changed |= 1 << opt->curve_num;
            sfs_close( f );
	    data->pnet->change_counter++;
        }
    }
    draw_window( data->win, wm );
    return 0;
}
int multisynth_save_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    multisynth_options* opt = data->module_data->opt;
    size_t size = 0;
    uint8_t* curve = get_curve( opt->curve_num, &size, data->mod_num, data->pnet );
    if( curve == 0 || size == 0 ) return 0;
    char ts[ 512 ];
    const char* ext = "curve8bit";
    if( g_curve_item_size[ opt->curve_num ] == 1 )
	sprintf( ts, "%s (%d x 8bit unsigned int)", ps_get_string( STR_PS_SAVE_RAW_DATA ), (int)size );
    else
    {
	sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_SAVE_RAW_DATA ), (int)size/2 );
	ext = "curve16bit";
    }
    char dialog_id[ 128 ];
    sprintf( dialog_id, ".sunvox_savecurve_ms%d", opt->curve_num );
    char* name = fdialog( ts, ext, dialog_id, 0, 0, wm );
    if( name )
    {
        sfs_file f = sfs_open( name, "wb" );
        if( f )
        {
            sfs_write( curve, 1, size, f );
            sfs_close( f );
        }
    }
    draw_window( data->win, wm );
    return 0;
}
static int multisynth_set_dialog_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_item* dlist = dialog_get_items( win );
    WINDOWPTR visual_win = (WINDOWPTR)user_data;
    if( visual_win->flags & WIN_FLAG_TRASH ) return 1;
    if( visual_win->win_handler != multisynth_visual_handler ) return 1;
    if( visual_win->id != dialog_get_par( win, 0 ) ) return 1;
    multisynth_visual_data* data = (multisynth_visual_data*)visual_win->data;
    int curve_num = dialog_get_par( win, 1 );
    int curve_len = g_curve_len[ curve_num ];
    uint8_t* curve = get_curve( curve_num, NULL, data->mod_num, data->pnet );
    if( !curve ) return 1;
    int rv = 1; 
    if( win->action_result == 0 )
    {
    	int val = 0;
    	if( curve_num == 2 )
    	{
    	    char* str_val = dialog_get_item( dlist, 'val ' )->str_val;
    	    smem_free( data->set_freq_or_note );
    	    data->set_freq_or_note = smem_strdup( str_val );
    	    int n = psynth_str2note( str_val );
    	    if( n != PSYNTH_UNKNOWN_NOTE )
    	    {
    	        val = 16384 + n * 256;
    	    }
    	    else
    	    {
    	        float freq = atof( str_val );
    	        val = PS_NOTE0_PITCH - PS_GFREQ_TO_PITCH( freq ) + 16384;
    	    }
	}
	else
	{
    	    data->set_vel = dialog_get_item( dlist, 'val ' )->int_val;
    	    val = data->set_vel;
    	}
    	int n1 = -1;
    	int n2 = -1;
    	data->set_repeat = dialog_get_item( dlist, 'rept' )->int_val;
    	if( curve_num == 0 || curve_num == 2 )
    	{
    	    n1 = psynth_str2note( dialog_get_item( dlist, 'from' )->str_val );
    	    n2 = psynth_str2note( dialog_get_item( dlist, 'to  ' )->str_val );
    	    if( n1 != PSYNTH_UNKNOWN_NOTE && n2 != PSYNTH_UNKNOWN_NOTE )
    	    {
    	        data->set_start_note = n1;
    		data->set_end_note = n2;
    	    }
    	}
    	else
    	{
    	    n1 = dialog_get_item( dlist, 'from' )->int_val;
    	    n2 = dialog_get_item( dlist, 'to  ' )->int_val;
    	    data->set_start_vel = n1;
    	    data->set_end_vel = n2;
    	}
    	if( n1 >= 0 && n2 >= 0 )
    	{
    	    if( n1 > n2 )
    	    {
    	        int temp = n1;
    	        n1 = n2;
    	        n2 = temp;
    	    }
    	    int len = n2 - n1 + 1;
    	    if( data->set_repeat )
    	    {
    	        int i = n1;
    	        while( 1 )
    	        {
    	    	    i -= data->set_repeat;
    		    if( i + len <= 0 ) break;
    		}
    		while( 1 )
    		{
		    curve_set_vals( curve_num, data->mod_num, data->pnet, i, val, len );
		    i += data->set_repeat;
		    if( i >= curve_len ) break;
    		}
    	    }
    	    else
    	    {
	        curve_set_vals( curve_num, data->mod_num, data->pnet, n1, val, len );
	    }
    	}
    	data->module_data->curve_changed |= 1 << curve_num;
	data->pnet->change_counter++;
	draw_window( visual_win, wm );
    }
    return rv;
}
int multisynth_set_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    multisynth_visual_data* data = (multisynth_visual_data*)user_data;
    multisynth_options* opt = data->module_data->opt;
    dialog_item* dlist = nullptr;
    WINDOWPTR dwin = nullptr;
    while( 1 )
    {
        dialog_item* di = nullptr;
        if( opt->curve_num == 0 || opt->curve_num == 2 )
        {
	    char note1[ 3 ];
    	    char note2[ 3 ];
    	    note1[ 0 ] = g_n2c[ data->set_start_note % 12 ];
    	    note1[ 1 ] = int_to_hchar( data->set_start_note / 12 );
    	    note1[ 2 ] = 0;
	    note2[ 0 ] = g_n2c[ data->set_end_note % 12 ];
	    note2[ 1 ] = int_to_hchar( data->set_end_note / 12 );
	    note2[ 2 ] = 0;
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_LABEL;
    	    di->str_val = (char*)ps_get_string( STR_PS_START_NOTE );
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_TEXT;
    	    di->str_val = note1;
    	    di->flags |= DIALOG_ITEM_FLAG_STR_AUTOREMOVE;
    	    di->id = 'from';
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_LABEL;
    	    di->str_val = (char*)ps_get_string( STR_PS_END_NOTE );
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_TEXT;
    	    di->str_val = note2;
    	    di->flags |= DIALOG_ITEM_FLAG_STR_AUTOREMOVE;
    	    di->id = 'to  ';
    	}
    	else
    	{
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_SLIDER;
    	    di->min = 0;
    	    di->max = 256;
    	    di->str_val = (char*)ps_get_string( STR_PS_START_VELOCITY );
    	    di->int_val = data->set_start_vel;
    	    di->id = 'from';
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_SLIDER;
    	    di->min = 0;
    	    di->max = 256;
    	    di->str_val = (char*)ps_get_string( STR_PS_END_VELOCITY );
    	    di->int_val = data->set_end_vel;
    	    di->id = 'to  ';
    	}
        if( opt->curve_num == 2 )
        {
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_LABEL;
    	    di->str_val = (char*)ps_get_string( STR_PS_FREQ_OR_NOTE );
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_TEXT;
    	    di->str_val = data->set_freq_or_note;
    	    di->flags |= DIALOG_ITEM_FLAG_STR_AUTOREMOVE;
    	    di->id = 'val ';
        }
        else
        {
    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_SLIDER;
    	    di->min = 0;
    	    di->max = 255;
    	    di->str_val = (char*)ps_get_string( STR_PS_VELOCITY );
    	    di->int_val = data->set_vel;
    	    di->id = 'val ';
    	}
    	di = dialog_new_item( &dlist ); if( !di ) break;
    	di->type = DIALOG_ITEM_SLIDER;
    	di->min = 0;
    	di->max = 128;
    	di->str_val = (char*)ps_get_string( STR_PS_REPEAT_WITH_PERIOD );
    	di->int_val = data->set_repeat;
    	di->id = 'rept';
        wm->opt_dialog_items = dlist;
        dwin = dialog_open( ps_get_string( STR_PS_SET ), nullptr, wm_get_string( STR_WM_OKCANCEL ), DIALOG_FLAG_SINGLE, wm ); 
        if( !dwin ) break;
        set_handler( dwin->childs[ 0 ], multisynth_set_dialog_handler, data->win, wm );
        dialog_set_flags( dwin, DIALOG_FLAG_AUTOREMOVE_ITEMS );
        dialog_set_par( dwin, 0, data->win->id );
        dialog_set_par( dwin, 1, opt->curve_num );
        dlist = nullptr; 
        break;
    }
    smem_free( dlist );
    return 0;
}
int multisynth_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    multisynth_visual_data* data = (multisynth_visual_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( multisynth_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
    		data->win = win;
    		data->pnet = (psynth_net*)win->host;
    		data->pushed = 0;
    		data->curve_y = 0;
    		data->curve_ysize = 0;
    		data->set_start_vel = 0;
    		data->set_end_vel = 256;
    		data->set_start_note = 0;
    		data->set_end_note = 127;
    		data->set_vel = 255;
    		data->set_freq_or_note = smem_strdup( "440" );
		const char* bname;
		int bxsize;
		int bysize = wm->text_ysize;
		int x = 0;
		int y = 0;
		bname = wm_get_string( STR_WM_OPTIONS ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->opt = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->opt, multisynth_opt_button_handler, data, wm );
                x += bxsize + wm->interelement_space2;
		bname = wm_get_string( STR_WM_RESET ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
                data->reset = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
    		set_handler( data->reset, multisynth_reset_handler, data, wm );
                x += bxsize + wm->interelement_space2;
		bname = ps_get_string( STR_PS_SMOOTH ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT;
                data->smooth = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->smooth, multisynth_smooth_handler, data, wm );
                x += bxsize + wm->interelement_space2;
		bname = wm_get_string( STR_WM_LOAD ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
    		data->load = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
    		set_handler( data->load, multisynth_load_handler, data, wm );
                x += bxsize + wm->interelement_space2;
		bname = wm_get_string( STR_WM_SAVE ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
    		data->save = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->save, multisynth_save_handler, data, wm );
                x += bxsize + wm->interelement_space2;
		bname = ps_get_string( STR_PS_SET ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
    		data->set = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
                set_handler( data->set, multisynth_set_handler, data, wm );
                x += bxsize + wm->interelement_space2;
                x = 0;
                y += bysize + wm->interelement_space;
                data->min_ysize = y + MAX_CURVE_YSIZE;
            }
            retval = 1;
            break;
        case EVT_BEFORECLOSE:
    	    retval = 1;
    	    smem_free( data->set_freq_or_note );
    	    break;
        case EVT_MOUSEBUTTONDOWN:
    	    if( rx >= 0 && rx < win->xsize && ry >= data->curve_y && ry < data->curve_y + data->curve_ysize )
    	    {
    		data->pushed = 1;
    	    }
        case EVT_MOUSEMOVE:
    	    if( data->pushed && data->curve_ysize > 0 && win->xsize > 0 )
    	    {
    		int curve_num = data->module_data->opt->curve_num;
    		int x_items;
    		int y_items = 256 << ((g_curve_item_size[ curve_num ]-1)*8);
    		uint8_t* v = get_curve( curve_num, NULL, data->mod_num, data->pnet );
    		if( v )
    		{
    		    x_items = g_curve_len[ curve_num ];
    		    int i = ( ( rx - wm->scrollbar_size / 2 ) * x_items ) / ( win->xsize - wm->scrollbar_size );
    		    if( i < 0 ) i = 0;
    		    if( i >= x_items ) i = x_items - 1;
    		    int val = ( ( data->curve_ysize - ( ry - data->curve_y ) ) * y_items ) / data->curve_ysize;
    		    if( curve_num == 2 )
    		    {
    			val /= 2;
    			val += 32768/2;
    		    }
        	    if( val < 0 ) val = 0;
        	    if( val >= y_items ) val = y_items - 1;
        	    if( evt->type == EVT_MOUSEBUTTONDOWN )
        	    {
			curve_val( curve_num, data->mod_num, data->pnet, i, val, 1 );
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
				    curve_val( curve_num, data->mod_num, data->pnet, i2, val2 >> 15, 1 );
        			val2 += dv;
        		    }
        		}
        		else
        		{
			    curve_val( curve_num, data->mod_num, data->pnet, i, val, 1 );
        		}
        	    }
    		    data->module_data->curve_changed |= 1 << data->module_data->opt->curve_num;
        	    data->prev_i = i;
        	    data->prev_val = val;
        	    draw_window( win, wm );
		    data->pnet->change_counter++;
        	}
    	    }
    	    retval = 1;
    	    break;
    	case EVT_MOUSEBUTTONUP:
    	    data->pushed = 0;
    	    retval = 1;
    	    break;
    	case EVT_DRAW:
    	    wbd_lock( win );
            draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
            {
        	COLOR c1 = blend( wm->color2, win->color, 150 );
        	COLOR c2 = blend( wm->color2, win->color, 170 );
    		int curve_num = data->module_data->opt->curve_num;
    		uint8_t* v = get_curve( curve_num, NULL, data->mod_num, data->pnet );
    		if( v )
    		{
    		    int item_size = g_curve_item_size[ curve_num ];
        	    int x_octave;
        	    int x_items = g_curve_len[ curve_num ];
        	    uint val_shift = 0;
        	    int val_add = 0;
        	    if( curve_num == 0 || curve_num == 2 )
        	    {
        		x_octave = 12;
        	    }
        	    else
        	    {
        		x_octave = 16;
        	    }
        	    if( curve_num == 2 )
        	    {
        		val_shift = 1;
        		val_add = -32768;
        	    }
        	    if( item_size == 1 )
        	    {
        		val_shift += 8;
        	    }
        	    int xstart = ( wm->scrollbar_size / 2 ) << 15;
        	    int x = xstart;
        	    int y = wm->text_ysize + wm->interelement_space;
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
        		    if( item_size == 2 ) val = ((uint16_t*)v)[ i ];
        		    val = ( val << val_shift ) + val_add;
        		    if( val >= 65535 ) val = 65536;
        		    if( val < 0 ) val = 0;
        		    int ysize = ( curve_ysize * val ) / 65536;
        		    if( val && ysize == 0 ) ysize = 1;
        		    COLOR c;
        		    if( ( i / x_octave ) & 1 )
        			c = c2;
        	    	    else
        			c = c1;
        		    draw_frect( x >> 15, y + curve_ysize - ysize, ( ( x + xd ) >> 15 ) - ( x >> 15 ), ysize, c, wm );
        		    if( i == 0 )
        		    {
        			draw_frect( 0, y + curve_ysize - ysize, ( x >> 15 ) - 1, ysize, c, wm );
        		    }
        		    if( i == x_items - 1 )
        		    {
        			draw_frect( ( ( x + xd ) >> 15 ) + 1, y + curve_ysize - ysize, wm->scrollbar_size, ysize, c, wm );
        		    }
        		    x += xd;
        		}
        		for( int i = 0; i < data->module_data->active_chans; i++ )
        		{
        		    if( data->module_data->channels[ i ].id != 0xFFFFFFFF )
        		    {
        			int n;
        			if( curve_num == 0 || curve_num == 2 )
        			{
        			    n = PS_PITCH_TO_NOTE( data->module_data->channels[ i ].pitch );
        			    if( n < 0 ) n = 0;
        			    if( n > 127 ) n = 127;
        			}
        			else
        			{
        			    n = data->module_data->channels[ i ].vel;
        			}
        			x = xstart + n * xd;
        			wm->cur_opacity = 128;
        			int bar_xsize = ( ( x + xd ) >> 15 ) - ( x >> 15 );
        			if( bar_xsize == 0 ) bar_xsize = 1;
        			draw_frect( x >> 15, y, bar_xsize, curve_ysize, wm->color3, wm );
        			wm->cur_opacity = 255;
        		    }
        		}
        	        psynth_draw_grid( xstart >> 15, y, ( ( xstart + xd * x_items ) >> 15 ) - ( xstart >> 15 ), curve_ysize, win );
        		wm->cur_font_color = wm->color2;
        		{
        		    char ts[ 256 ];
	        	    const char* curve_str = ps_get_string( STR_PS_VEL_CURVE );
	        	    if( curve_num == 2 ) curve_str = ps_get_string( STR_PS_PITCH_CURVE );
        		    sprintf( ts, "%s %d %s", curve_str, 1 + curve_num, ps_get_string( STR_PS_CURVE_DESCR ) );
                	    draw_string( ts, 0, y, wm );
        		}
        		const char* curve_name = NULL;
        		switch( curve_num )
        		{
        		    case 0: curve_name = ps_get_string( STR_PS_CURVE_TYPE1 ); break;
        		    case 1: curve_name = ps_get_string( STR_PS_CURVE_TYPE2 ); break;
        		    case 2: curve_name = ps_get_string( STR_PS_CURVE_TYPE3 ); break;
        		}
                	draw_string( curve_name, 0, y + char_y_size( wm ), wm );
                	if( curve_num == 0 || curve_num == 2 )
                	{
                	    if( curve_ysize > char_y_size( wm ) * 3 && char_x_size( '#', wm ) < ((xd*12)>>15) )
                	    {
        			x = xstart;
        			for( int i = 0; i < x_items; i += 12 )
        			{
        			    char ts[ 16 ];
        			    sprintf( ts, "%d", i / 12 );
                		    draw_string( ts, x >> 15, y + curve_ysize - char_y_size( wm ), wm );
        			    x += xd * 12;
        			}
        		    }
        		}
        		else
        		{
                	    int step = 16;
                	    while( 1 )
                	    {
                		if( curve_ysize > char_y_size( wm ) * 3 && char_x_size( '#', wm ) * 4 < (int)((xd*step)>>15) )
                		{
        			    x = xstart;
        			    for( int i = 0; i < x_items; i += step )
        			    {
        				char ts[ 16 ];
        				sprintf( ts, "%d", i );
                			draw_string( ts, x >> 15, y + curve_ysize - char_y_size( wm ), wm );
        				x += xd * step;
        			    }
        			    break;
        			}
        			step <<= 1;
        			if( step > x_items ) break;
        		    }
			}
        	    }
        	}
    	    }
            wbd_draw( wm );
            wbd_unlock( wm );
    	    retval = 1;
    	    break;
    }
    return retval;
}
#endif
#define VEL_FORMULA_DESC "out=curve2(in*vel*random*curve1(pitch))"
static void calc_active_chans( MODULE_DATA* data, int last_disabled_chan )
{
    int i = last_disabled_chan;
    while( i >= 0 )
    {
	if( data->channels[ i ].id != 0xFFFFFFFF ) break;
	if( data->active_chans - 1 <= i )
	{
	    data->active_chans = i;
	    i--;
	}
	else
	{
	    break;
	}
    }
}
static void release_chan( MODULE_DATA* data, int cn )
{
    ms_channel* chan = &data->channels[ cn ];
    uint wp = ( data->hist_wp - 1 ) & (HISTORY_SIZE-1);
    if( data->hist[ wp ].id == chan->id ) 
	data->hist_wp = wp;
    history_item* hist = &data->hist[ data->hist_wp ];
    hist->id = chan->id;
    hist->pitch = chan->pitch;
    hist->out = chan->out;
    data->hist_wp = ( data->hist_wp + 1 ) & (HISTORY_SIZE-1);
    chan->id = 0xFFFFFFFF;
    chan->flags = 0;
}
static int convert_outslot( psynth_module* mod, int n ) 
{
    for( int i = 0, i2 = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
	if( l >= 0 )
	{
	    if( n == i2 )
	    {
	        return i;
	    }
	    i2++;
	}
    }
    return -1;
}
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
	    retval = (PS_RETTYPE)"MultiSynth";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = 
                    	    (PS_RETTYPE)"MultiSynth принятые звуковые сообщения (нота, высота тона, фаза, динамика) пересылает всем подключенным на его выход модулям-приемникам.\n"
                    	    "Формула динамики: " VEL_FORMULA_DESC;
                        break;
                    }
		    retval = 
			(PS_RETTYPE)"MultiSynth sends the incoming events (note, pitch, phase, velocity) to any number of connected modules (receivers). "
			"It can also modify incoming events.\n"
                    	"Velocity formula: " VEL_FORMULA_DESC;
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#DEFF00";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS:
	    retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_NO_RENDER;
	    break;
	case PS_CMD_GET_FLAGS2:
	    retval = PSYNTH_FLAG2_NOTE_IO | PSYNTH_FLAG2_GET_MUTED_COMMANDS;
	    break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 8, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TRANSPOSE ), "", 0, 256, 128, 1, &data->ctl_transpose, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 0, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_PITCH ), "", 0, 4096, 0, 0, &data->ctl_random, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VELOCITY ), "", 0, 256, 256, 0, &data->ctl_velocity, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FINETUNE ), "", 0, 512, 256, 0, &data->ctl_finetune, 256, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 3, -256, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_PHASE ), "", 0, 0x8000, 0, 0, &data->ctl_random_phase, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_VELOCITY ), "", 0, 0x8000, 0, 0, &data->ctl_random_velocity, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PHASE ), "", 0, 0x8000, 0, 0, &data->ctl_phase, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CURVE2_INFLUENCE ), "", 0, 256, 256, 0, &data->ctl_curve2_mix, -1, 0, pnet );
	    data->rand1 = stime_ms() + mod_num * 33;
	    data->rand2 = data->rand1 + mod_num * 39;
	    for( int i = 0; i < MAX_CHANNELS + 1; i++ ) 
	    {
		data->channels[ i ].id = 0xFFFFFFFF;
	    }
	    data->active_chans = 0;
	    for( int i = 0; i < HISTORY_SIZE; i++ ) 
	    {
		data->hist[ i ].id = 0xFFFFFFFF;
	    }
#ifdef SUNVOX_GUI
            {
        	data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "MultiSynth GUI", 0, 0, 10, 10, wm->color1, 0, pnet, multisynth_visual_handler, wm );
            	    multisynth_visual_data* ms_data = (multisynth_visual_data*)mod->visual->data;
            	    mod->visual_min_ysize = ms_data->min_ysize;
            	    ms_data->module_data = data;
            	    ms_data->mod_num = mod_num;
            	    ms_data->pnet = pnet;
            	}
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		data->opt = (multisynth_options*)
		    psynth_get_chunk_data_autocreate( mod_num, 1, sizeof( multisynth_options ), 0, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
		for( int i = 0; i < NUM_CURVES; i++ )
		{
		    new_curve( i, mod_num, pnet );
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_BEFORE_SAVE:
	    {
		for( int i = 0; i < NUM_CURVES; i++ )
		{
		    if( ( data->curve_changed & (1<<i) ) == 0 )
		    {
			psynth_set_chunk_flags( mod_num, g_curve_chunk_id[ i ], pnet, PS_CHUNK_FLAG_DONT_SAVE, 0 );
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_AFTER_SAVE:
	    {
		for( int i = 0; i < NUM_CURVES; i++ )
		{
		    if( ( data->curve_changed & (1<<i) ) == 0 )
		    {
			psynth_set_chunk_flags( mod_num, g_curve_chunk_id[ i ], pnet, 0, PS_CHUNK_FLAG_DONT_SAVE );
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int i = 0; i < MAX_CHANNELS + 1; i++ ) 
	    {
		data->channels[ i ].id = 0xFFFFFFFF;
		data->channels[ i ].pitch = 0;
		data->channels[ i ].flags = 0;
	    }
	    data->active_chans = 0;
	    for( int i = 0; i < HISTORY_SIZE; i++ ) 
	    {
		data->hist[ i ].id = 0xFFFFFFFF;
	    }
#ifdef SUNVOX_GUI
	    mod->draw_request++;
#endif
	    retval = 1;
	    break;
	case PS_CMD_NOTE_ON:
	    if( data->opt->flags & OPT_FLAG_REC_CURVE3 )
	    {
    		int in_pitch = PS_NOTE0_PITCH - event->note.pitch;
    		if( in_pitch >= 0 )
    		{
    		    in_pitch %= 256 * 12;
    		    int in_note = in_pitch / 256;
    		    uint16_t* pitch_curve = (uint16_t*)psynth_get_chunk_data( mod, 3 );
    		    if( data->rec_curve3_cnt == 0 )
    		    {
    			for( int oct = 0; oct < 11; oct++ )
    			{
    			    int from = oct * 12;
    			    int to = from + 12; if( to > 128 ) to = 128;
    			    for( int i = from; i < to; i++ )
    			    {
    				pitch_curve[ i ] = 16384 + in_pitch + oct * 256 * 12;
    			    }
    			}
    		    }
    		    else
    		    {
    			for( int oct = 0; oct < 11; oct++ )
    			{
    			    int i = oct * 12 + in_note;
    			    if( i < 128 )
    			    {
    				int p0 = pitch_curve[ i ];
    				int p = 16384 + in_pitch + oct * 256 * 12;
    				if( p > p0 )
    				{
    				    int oct_end = oct * 12 + 12;
    				    if( oct_end > 128 ) oct_end = 128;
    				    while( i < oct_end && p > pitch_curve[ i ] )
    				    {
    					pitch_curve[ i ] = p;
    					i++;
    				    }
    				}
    				if( p < p0 )
    				{
    				    int oct_begin = oct * 12;
    				    while( i >= oct_begin && p < pitch_curve[ i ] )
    				    {
    					pitch_curve[ i ] = p;
    					i--;
    				    }
    				}
    			    }
    			}
		    }
		    data->rec_curve3_cnt++;
    		    data->curve_changed |= 1 << 2;
#ifdef SUNVOX_GUI
		    mod->draw_request++;
#endif
		}
		retval = 1;
		break;
	    }
	case PS_CMD_SET_FREQ:
    	case PS_CMD_SET_VELOCITY:
	    {
        	if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
		GET_EVENT;
		multisynth_options* opt = data->opt;
		ms_channel* chan = &data->channels[ MAX_CHANNELS ];
		int chan_num = MAX_CHANNELS;
		for( int i = 0; i < data->active_chans; i++ )
		{
		    if( data->channels[ i ].id == e.id )
		    {
		        chan_num = i;
		        chan = &data->channels[ i ];
		        if( e.command == PS_CMD_NOTE_ON )
		        {
			    int outslot_mode = opt->flags & OPT_FLAGS_OUTSLOT;
		    	    if( ( opt->flags & OPT_FLAG_GEN_MISSED_NOTEOFF ) || ( outslot_mode && outslot_mode != OPT_FLAG_OUTSLOT_POLYCHANNEL ) )
		    	    {
		    		e.command = PS_CMD_NOTE_OFF;
		    		if( chan->out == 0 )
				    psynth_multisend( mod, &e, pnet );
				else
				    if( chan->out - 1 < mod->output_links_num )
					psynth_add_event( mod->output_links[ chan->out - 1 ], &e, pnet );
		    		e.command = PS_CMD_NOTE_ON;
		    	    }
		    	}
		        break;
		    }
		}
		if( chan_num == MAX_CHANNELS )
		{
		    for( int i = 0; i < MAX_CHANNELS; i++ )
		    {
		        if( data->channels[ i ].id == 0xFFFFFFFF )
		        {
		    	    chan_num = i;
		    	    chan = &data->channels[ i ];
		    	    if( e.command != PS_CMD_NOTE_ON )
		    	    {
		    		uint rp = ( data->hist_wp - 1 ) & (HISTORY_SIZE-1);
		    		uint h = 0;
		    		for( h = 0; h < HISTORY_SIZE; h++ )
		    		{
		    		    history_item* hist = &data->hist[ rp ];
		    		    if( hist->id == e.id )
		    		    {
		    			chan->pitch = hist->pitch;
		    			chan->out = hist->out;
		    			chan->flags |= CHAN_FLAG_FROM_HISTORY;
		    			break;
		    		    }
		    		    rp = ( rp - 1 ) & (HISTORY_SIZE-1);
		    		}
		    	    }
			    break;
			}
		    }
		}
            	int pitch = 0;
            	if( e.command == PS_CMD_SET_VELOCITY )
            	{
		    pitch = chan->pitch;
            	}
            	else
            	{
            	    if( opt->static_freq && pnet->base_host_version < 0x01090000 )
            		pitch = PS_NOTE_TO_PITCH( 5 * 12 );
            	    else
            		pitch = e.note.pitch;
            	    pitch = pitch - ( data->ctl_finetune - 256 ) - ( data->ctl_transpose - 128 ) * 256;
		    if( pnet->base_host_version == 0x01070000 )
			pitch -= mod->finetune;
		    if( data->ctl_random )
		    {
			int r = pseudo_random( &data->rand1 );
			r &= 8191;
			r -= 4096;
			r *= data->ctl_random;
			r /= 4096;
			pitch += r;
		    }
		}
    		uint8_t* v1 = (uint8_t*)psynth_get_chunk_data( mod, 0 );
    		uint8_t* v2 = (uint8_t*)psynth_get_chunk_data( mod, 2 );
    		int n = PS_PITCH_TO_NOTE( pitch );
    		if( n < 0 ) n = 0;
    		if( n > 127 ) n = 127;
    		int vv;
    		int vel = ( (int)e.note.velocity * data->ctl_velocity ) / 256;
    		if( data->ctl_random_velocity )
    		{
    		    int rnd = 32768 - ( ( pseudo_random( &data->rand1 ) * data->ctl_random_velocity ) >> 15 );
    		    vel = ( vel * rnd ) >> 15;
    		}
    		vv = v1[ n ]; if( vv != 255 ) vel = ( vel * vv ) / 255;
#ifdef SUNVOX_GUI
    		int show_vel = vel;
#endif
		if( data->ctl_curve2_mix )
		{
		    vel = ( vel * ( 256 - data->ctl_curve2_mix ) + v2[ vel ] * data->ctl_curve2_mix ) >> 8;
		    if( vel >= 255 ) vel = 256;
		}
     		e.note.velocity = (int16_t)vel;
		bool note_closed = false;
		if( opt->trigger && e.command == PS_CMD_NOTE_ON )
		{
		    for( int i = 0; i < data->active_chans; i++ )
		    {
			ms_channel* ch2 = &data->channels[ i ];
			if( ch2->pitch == pitch && ch2->id != 0xFFFFFFFF )
			{
			    e.id = ch2->id;
			    e.command = PS_CMD_NOTE_OFF;
			    release_chan( data, i );
			    calc_active_chans( data, i );
			    note_closed = true;
			    chan_num = i;
			    chan = ch2;
			    break;
			}
		    }
		}
		if( note_closed == false )
		{
		    chan->id = e.id;
		    chan->pitch = pitch;
            	    if( e.command != PS_CMD_SET_VELOCITY )
		        chan->original_pitch = e.note.pitch;
		    if( e.command == PS_CMD_NOTE_ON )
		    {
			int outslot_mode = opt->flags & OPT_FLAGS_OUTSLOT;
		        if( outslot_mode )
		        {
		    	    int outs = 0; for( int i = 0; i < mod->output_links_num; i++ ) { if( mod->output_links[ i ] >= 0 ) outs++; }
			    if( outs )
			    {
			        int out = 0;
			        switch( outslot_mode )
			        {
			    	    case OPT_FLAG_OUTSLOT_NOTE:
			    		out = PS_PITCH_TO_NOTE( pitch ) % outs;
			    		chan->out = convert_outslot( mod, out ) + 1;
			    		break;
			    	    case OPT_FLAG_OUTSLOT_POLYCHANNEL:
			    		out = chan_num % outs;
			    		chan->out = convert_outslot( mod, out ) + 1;
			    		break;
			    	    case OPT_FLAG_OUTSLOT_ROUNDROBIN:
			    		{
			    		    int out2 = 0;
			    		    for( int a = 0; a < outs; a++ )
			    		    {
			    			if( data->outslot_cnt >= outs ) data->outslot_cnt = 0;
			    			out = data->outslot_cnt;
			    			out2 = convert_outslot( mod, out ) + 1;
						for( int i = 0; i < data->active_chans; i++ )
						{
					    	    ms_channel* chan2 = &data->channels[ i ];
						    if( chan2->id == 0xFFFFFFFF ) continue;
						    if( chan2->flags & CHAN_FLAG_FROM_HISTORY )
							continue; 
						    if( chan2->out == out2 )
						    {
							out2 = 0;
							break;
						    }
						}
			    			data->outslot_cnt++;
						if( out2 )
						{
						    chan->out = out2;
						    break;
						}
			    		    }
					    if( out2 == 0 )
					    {
			    			if( data->outslot_cnt >= outs ) data->outslot_cnt = 0;
			    			out = data->outslot_cnt;
			    			chan->out = convert_outslot( mod, out ) + 1;
			    			data->outslot_cnt++;
					    }
			    		}
			    		break;
			    	    case OPT_FLAG_OUTSLOT_RANDOM1:
			    		out = pseudo_random( &data->rand2 ) * outs / 32768;
			    		chan->out = convert_outslot( mod, out ) + 1;
			    		break;
			    	    case OPT_FLAG_OUTSLOT_RANDOM2:
			    		out = pseudo_random( &data->rand2 ) % outs;
			    		chan->out = convert_outslot( mod, out ) + 1;
			    		break;
			    	    case OPT_FLAG_OUTSLOT_RANDOM3:
			    		while( 1 )
			    		{
			    		    out = pseudo_random( &data->rand2 ) * outs / 32768;
			    		    if( outs < 3 ) break;
			    		    if( out != data->prev_random_outslot ) break;
			    		}
			    		chan->out = convert_outslot( mod, out ) + 1;
			    		data->prev_random_outslot = out;
			    		break;
			    	}
            		    } 
            		} 
            		else
            		{
            		    chan->out = 0;
            		}
		    }
#ifdef SUNVOX_GUI
		    chan->vel = show_vel;
#endif
		    if( chan_num >= data->active_chans ) { data->active_chans = chan_num + 1; if( data->active_chans > MAX_CHANNELS ) data->active_chans = MAX_CHANNELS; }
		}
#ifdef SUNVOX_GUI
		mod->draw_request++;
#endif
		if( opt->static_freq && pnet->base_host_version >= 0x01090000 )
		{
		    pitch = PS_NOTE_TO_PITCH( 5 * 12 );
            	    pitch = pitch - ( data->ctl_finetune - 256 ) - ( data->ctl_transpose - 128 ) * 256;
		    if( data->ctl_random )
		    {
			int r = pseudo_random( &data->rand1 );
			r &= 8191;
			r -= 4096;
			r *= data->ctl_random;
			r /= 4096;
			pitch += r;
		    }
		}
		if( data->curve_changed & (1<<2) )
		{
    		    uint16_t* pitch_curve = (uint16_t*)psynth_get_chunk_data( mod, 3 );
    		    int in_pitch = PS_NOTE0_PITCH - pitch;
		    if( opt->flags & OPT_FLAG_ROUND_PITCH ) in_pitch += 128; 
    		    int in_note = ( in_pitch + 65536 ) / 256 - 256;
    		    int out_pitch;
    		    if( opt->flags & OPT_FLAG_ROUND_PITCH2 )
    		    {
    			in_pitch += 16384;
    			if( in_note < 0 ) in_note = 0;
    			if( in_note > 127 ) in_note = 127;
    			out_pitch = pitch_curve[ in_note ];
    			int diff = abs( in_pitch - out_pitch );
    			for( int i = in_note + 1; i < 128; i++ )
    			{
    			    int p = pitch_curve[ i ];
    			    int diff2 = abs( in_pitch - p );
    			    if( diff2 > diff ) break;
    			    if( diff2 < diff ) { diff = diff2; out_pitch = p; }
    			}
    			for( int i = in_note - 1; i >= 0; i-- )
    			{
    			    int p = pitch_curve[ i ];
    			    int diff2 = abs( in_pitch - p );
    			    if( diff2 > diff ) break;
    			    if( diff2 < diff ) { diff = diff2; out_pitch = p; }
    			}
    		    }
    		    else
    		    {
    			if( in_note < 0 || in_note >= 127 )
    			{
    			    if( in_note < 0 )
    				out_pitch = pitch_curve[ 0 ];
    			    else
    				out_pitch = pitch_curve[ 127 ];
    			}
    			else
    			{
			    out_pitch = pitch_curve[ in_note ];
			    if( !( opt->flags & OPT_FLAG_ROUND_PITCH ) )
			    {
				int c = in_pitch & 255;
				out_pitch = ( out_pitch * ( 256 - c ) + pitch_curve[ in_note + 1 ] * c ) / 256;
			    }
			}
		    }
		    pitch = PS_NOTE0_PITCH - ( out_pitch - 16384 );
		}
		else
		{
		    if( opt->flags & ( OPT_FLAG_ROUND_PITCH | OPT_FLAG_ROUND_PITCH2 ) )
		    {
    			int in_pitch = PS_NOTE0_PITCH - pitch;
			in_pitch += 128; 
    			int in_note = ( in_pitch + 65536 ) / 256 - 256;
			pitch = PS_NOTE0_PITCH - in_note * 256;
		    }
		}
		if( opt->flags & OPT_FLAG_NOTE_DIFF )
		{
		    pitch = pitch - chan->original_pitch + PS_NOTE_TO_PITCH( 5 * 12 );
		}
		if( !( opt->ignore_vel0 && e.note.velocity == 0 ) )
		{
		    int link0 = 0;
		    int link1 = mod->output_links_num;
		    if( chan->out )
		    {
		        link0 = chan->out - 1;
		        link1 = link0 + 1;
		    }
		    for( int i = link0; i < link1; i++ )
		    {
			int l = mod->output_links[ i ];
			if( (unsigned)l < (unsigned)pnet->mods_num )
			{
			    psynth_module* s = &pnet->mods[ l ];
			    if( s->flags & PSYNTH_FLAG_EXISTS )
			    {
				e.note.pitch = pitch - s->finetune - s->relative_note * 256;
				if( pnet->base_host_version < 0x01080000 ) if( e.note.pitch >= 7680 * 4 ) e.note.pitch = 7680 * 4;
				psynth_add_event( l, &e, pnet );
			    }
			}
		    }
		    if( ( data->ctl_random_phase || data->ctl_phase ) && e.command == PS_CMD_NOTE_ON )
		    {
			for( int i = link0; i < link1; i++ )
			{
			    int l = mod->output_links[ i ];
			    if( (unsigned)l < (unsigned)pnet->mods_num )
			    {
				psynth_module* s = &pnet->mods[ l ];
				if( s->flags & PSYNTH_FLAG_EXISTS )
				{
				    e.command = PS_CMD_SET_SAMPLE_OFFSET2;
				    e.sample_offset.sample_offset = ( pseudo_random( &data->rand1 ) * data->ctl_random_phase ) >> 15;
				    e.sample_offset.sample_offset += data->ctl_phase;
				    psynth_add_event( l, &e, pnet );
				}
			    }
			}
		    }
		}
		retval = 1;
	    }
	    break;
        case PS_CMD_SET_SAMPLE_OFFSET:
        case PS_CMD_SET_SAMPLE_OFFSET2:
    	    {
        	if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
    		GET_EVENT;
		multisynth_options* opt = data->opt;
		if( e.command == PS_CMD_SET_SAMPLE_OFFSET2 )
		{
		    e.sample_offset.sample_offset += data->ctl_phase;
		    if( e.sample_offset.sample_offset > 32768 )
			e.sample_offset.sample_offset = 32768;
		}
		if( opt->flags & OPT_FLAGS_OUTSLOT )
		{
		    for( int i = 0; i < data->active_chans; i++ )
		    {
			ms_channel* chan = &data->channels[ i ];
			if( chan->id == e.id )
			{
			    if( chan->out == 0 )
				psynth_multisend( mod, &e, pnet );
			    else
				if( chan->out - 1 < mod->output_links_num ) psynth_add_event( mod->output_links[ chan->out - 1 ], &e, pnet );
		    	    break;
			}
		    }
		}
		else
		    psynth_multisend( mod, &e, pnet );
		retval = 1;
    	    }
    	    break;
	case PS_CMD_MUTED:
	    {
		for( int i = 0; i < data->active_chans; i++ )
		{
		    ms_channel* chan = &data->channels[ i ];
		    if( chan->id != 0xFFFFFFFF )
		    {
        		psynth_event e = *event;
        		e.command = PS_CMD_NOTE_OFF;
        		e.note.velocity = 256;
			e.note.pitch = 0;
			e.id = chan->id;
			psynth_multisend( mod, &e, pnet );
		    }
		    release_chan( data, i );
		}
		data->active_chans = 0;
	    }
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    {
        	if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
		GET_EVENT;
		for( int i = 0; i < data->active_chans; i++ ) release_chan( data, i );
		data->active_chans = 0;
#ifdef SUNVOX_GUI
		mod->draw_request++;
#endif
		retval = 1;
	    }
	    break;
	case PS_CMD_NOTE_OFF:
	    {
        	if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
		GET_EVENT;
		multisynth_options* opt = data->opt;
		int out = 0;
		if( opt->trigger )
		{
		    retval = 1;
		    break;
		}
		for( int i = 0; i < data->active_chans; i++ )
		{
		    ms_channel* chan = &data->channels[ i ];
		    if( chan->id == e.id )
		    {
			out = chan->out;
		        release_chan( data, i );
		        calc_active_chans( data, i );
		        break;
		    }
		}
#ifdef SUNVOX_GUI
		mod->draw_request++;
#endif
		if( out == 0 )
		    psynth_multisend( mod, &e, pnet );
		else
		    if( out - 1 < mod->output_links_num )
			psynth_add_event( mod->output_links[ out - 1 ], &e, pnet );
		retval = 1;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
    	    {
    		multisynth_options* opt = data->opt;
                int v = event->controller.ctl_val;
                switch( event->controller.ctl_num + 1 )
                {
                    case 127: opt->static_freq = v!=0; retval = 1; break;
                    case 126: opt->ignore_vel0 = v!=0; retval = 1; break;
                    case 125: opt->curve_num = v % 3; retval = 1; break;
                    case 124: opt->trigger = v!=0; retval = 1; break;
                    case 123: opt->flags &= ~OPT_FLAG_GEN_MISSED_NOTEOFF; if( v ) opt->flags |= OPT_FLAG_GEN_MISSED_NOTEOFF; retval = 1; break;
                    case 122: opt->flags &= ~(OPT_FLAG_ROUND_PITCH|OPT_FLAG_ROUND_PITCH2); if( v ) opt->flags |= OPT_FLAG_ROUND_PITCH; retval = 1; break;
                    case 121: opt->flags &= ~(OPT_FLAG_ROUND_PITCH|OPT_FLAG_ROUND_PITCH2); if( v ) opt->flags |= OPT_FLAG_ROUND_PITCH2; retval = 1; break;
                    case 120: opt->flags &= ~OPT_FLAG_REC_CURVE3; if( v ) { opt->flags |= OPT_FLAG_REC_CURVE3; data->rec_curve3_cnt = 0; } else { data->rec_curve3_cnt = 0; } retval = 1; break;
                    case 118: opt->flags &= ~OPT_FLAG_NOTE_DIFF; if( v ) opt->flags |= OPT_FLAG_NOTE_DIFF; retval = 1; break;
                    case 117: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_NOTE; retval = 1; break;
                    case 116: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_POLYCHANNEL; retval = 1; break;
                    case 115: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_ROUNDROBIN; retval = 1; break;
                    case 114: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_RANDOM1; retval = 1; break;
                    case 113: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_RANDOM2; retval = 1; break;
                    case 112: opt->flags &= ~OPT_FLAGS_OUTSLOT; if( v ) opt->flags |= OPT_FLAG_OUTSLOT_RANDOM3; retval = 1; break;
                    default: break;
                }
            }
    	    break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = 0;
    	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_READ_CURVE:
	case PS_CMD_WRITE_CURVE:
	    {
		int chunk_id = -1;
		int len = g_curve_len[ event->id ];
		int reqlen = event->offset;
		if( (unsigned)event->id < (unsigned)NUM_CURVES ) chunk_id = g_curve_chunk_id[ event->id ];
		if( chunk_id >= 0 )
		{
        	    uint8_t* curve = (uint8_t*)psynth_get_chunk_data( mod_num, chunk_id, pnet );
        	    if( !curve ) curve = new_curve( event->id, mod_num, pnet );
        	    if( curve && event->curve.data )
        	    {
        		if( reqlen == 0 ) reqlen = len;
        		if( reqlen > len ) reqlen = len;
        		if( g_curve_item_size[ event->id ] == 1 )
        		{
        		    if( event->command == PS_CMD_READ_CURVE )
        			for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve[ i ] / 255.0F; }
        		    else
        			for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * 255.0F; LIMIT_NUM( v, 0, 255 ); curve[ i ] = v; }
        		}
        		else
        		{
        		    uint16_t* curve16 = (uint16_t*)curve;
        		    if( event->command == PS_CMD_READ_CURVE )
        			for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve16[ i ] / 65535.0F; }
        		    else
        			for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * 65535.0F; LIMIT_NUM( v, 0, 65535 ); curve16[ i ] = v; }
        		}
        		if( event->command == PS_CMD_WRITE_CURVE )
			    data->curve_changed |= 1 << event->id;
        		retval = reqlen;
        	    }
        	}
    	    }
	    break;
	default: break;
    }
    return retval;
}
