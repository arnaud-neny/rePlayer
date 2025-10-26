/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2025 Alexander Zolotov <nightradio@gmail.com>
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
#define MODULE_DATA	psynth_sound2ctl_data
#define MODULE_HANDLER	psynth_sound2ctl
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	0
#define UNKNOWN_CTL_VAL	0x10000
struct sound2ctl_options
{
    uint8_t		record_values;
    uint8_t		optimize;
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_freq;
    PS_CTYPE		ctl_stereo;
    PS_CTYPE		ctl_abs;
    PS_CTYPE		ctl_gain;
    PS_CTYPE		ctl_smooth;
    PS_CTYPE		ctl_alg;
    PS_CTYPE   		ctl_min;
    PS_CTYPE   		ctl_max;
    PS_CTYPE		ctl_num;
    int			tick_counter; 
    PS_STYPE2		accum; 
    int 		accum_cnt;
    bool		accum_empty;
    psynth_event	ctl_evt;
    int			prev_ctl_val;
    int                 empty_frames_counter;
    int                 empty_frames_counter_max;
    sound2ctl_options* 	opt;
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct sound2ctl_visual_data
{
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
    WINDOWPTR 		win;
    WINDOWPTR 		opt;
};
int sound2ctl_opt_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    sound2ctl_visual_data* data = (sound2ctl_visual_data*)user_data;
    sound2ctl_options* opt = data->module_data->opt;
    psynth_opt_menu* m = opt_menu_new( win );
    opt_menu_add( m, STR_PS_RECORD_VALUES, opt->record_values, 127 );
    opt_menu_add( m, STR_PS_SEND_CHANGES_ONLY, opt->optimize, 126 );
    int sel = opt_menu_show( m );
    opt_menu_remove( m );
    int ctl = -1;
    int ctl_val = 0;
    switch( sel )
    {
        case 0: ctl = 127; ctl_val = !opt->record_values; break;
        case 1: ctl = 126; ctl_val = !opt->optimize; break;
        default: break;
    }
    if( ctl >= 0 ) psynth_handle_ctl_event( data->mod_num, ctl - 1, ctl_val, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
int sound2ctl_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    sound2ctl_visual_data* data = (sound2ctl_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( sound2ctl_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
        	data->win = win;
        	data->pnet = (psynth_net*)win->host;
        	data->module_data = 0;
        	data->mod_num = -1;
        	set_button_icon( BUTTON_ICON_MENU, ((sunvox_engine*)data->pnet->host)->win );
                wm->opt_button_flags = BUTTON_FLAG_FLAT;
                data->opt = new_window( wm_get_string( STR_WM_OPTIONS ), 0, 0, 1, wm->scrollbar_size, wm->button_color, win, button_handler, wm );
                set_handler( data->opt, sound2ctl_opt_handler, data, wm );
                set_window_controller( data->opt, 0, wm, CPERC, (WCMD)100, CEND );
                set_window_controller( data->opt, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
    	    }
            retval = 1;
            break;
        case EVT_DRAW:
    	    {
    		wbd_lock( win );
                draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		psynth_module* m = psynth_get_module( data->mod_num, data->pnet );
                if( m )
                {
                    for( int i = 0; i < m->output_links_num; i++ )
                    {
                        int l = m->output_links[ i ];
                        psynth_module* m2 = psynth_get_module( l, data->pnet );
                        if( m2 && ( m2->flags & PSYNTH_FLAG_EXISTS ) )
                        {
			    psynth_draw_ctl_info( 
	    			l,
				data->pnet, 
	    			data->module_data->ctl_num, 
	    			data->module_data->ctl_min, 
	    			data->module_data->ctl_max, 
	    			0, 0, 0, 0,
				win );
			    break;
			}
		    }
		}
                wbd_draw( wm );
                wbd_unlock( wm );
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
        case EVT_BEFORECLOSE:
            retval = 1;
            break;
    }
    return retval;
}
#endif
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
	    retval = (PS_RETTYPE)"Sound2Ctl";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Sound2Ctl конвертирует звук в цифровое значение контроллера. Для временного отключения данного модуля нажмите кнопку Mute (M) или установите контроллер Вых.контроллер в ноль.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Sound2Ctl converts the audio signal to the numeric value of any selected controller.\nWays to disable this module:\n 1) mute it;\n 2) set OUT Controller to 0.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFFFFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_OUTPUT_IS_EMPTY; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 9, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_RATE ), ps_get_string( STR_PS_HZ ), 1, 32768, 50, 0, &data->ctl_freq, 50, 0, pnet );
	    psynth_set_ctl_flags( mod_num, 0, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_MONO_STEREO ), 0, 1, 0, 1, &data->ctl_stereo, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ABSOLUTE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_abs, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_GAIN ), "", 0, 1024, 256, 0, &data->ctl_gain, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SMOOTH ), "", 0, 256, 128, 0, &data->ctl_smooth, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "LQ;HQ", 0, 1, 1, 1, &data->ctl_alg, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_MIN ), "", 0, 32768, 0, 0, &data->ctl_min, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_MAX ), "", 0, 32768, 32768, 0, &data->ctl_max, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_CTL ), "", 0, 255, 0, 1, &data->ctl_num, -1, 1, pnet );
	    data->tick_counter = 0;
	    data->accum = 0;
	    data->accum_cnt = 0;
	    data->accum_empty = 1;
	    smem_clear( &data->ctl_evt, sizeof( psynth_event ) );
	    data->ctl_evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
	    data->prev_ctl_val = UNKNOWN_CTL_VAL;
	    data->empty_frames_counter_max = pnet->sampling_freq * 2;
#ifdef SUNVOX_GUI
            {
        	data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "Sound2Ctl GUI", 0, 0, 10, 10, wm->button_color, 0, pnet, sound2ctl_visual_handler, wm );
            	    mod->visual_min_ysize = 0;
            	    sound2ctl_visual_data* data1 = (sound2ctl_visual_data*)mod->visual->data;
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
		bool opt_created = false;
                data->opt = (sound2ctl_options*)
            	    psynth_get_chunk_data_autocreate( mod_num, 0, sizeof( sound2ctl_options ), &opt_created, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
            	if( opt_created )
            	{
            	    if( pnet->base_host_version >= 0x01090600 ) data->opt->optimize = 1;
            	}
            }
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    data->tick_counter = 0;
	    data->accum = 0;
	    data->accum_cnt = 0;
	    data->accum_empty = 1;
	    data->prev_ctl_val = UNKNOWN_CTL_VAL;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		int offset = mod->offset;
		int frames = mod->frames;
	        if( data->ctl_num == 0 ) break;
	        if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
                bool input_signal = 0;
        	if( mod->in_empty[ 0 ] < offset + frames ) input_signal = 1;
	        if( data->ctl_stereo == 0 )
                {
            	    if( psynth_get_number_of_inputs( mod ) != 1 )
            	    {
                	psynth_set_number_of_inputs( 1, mod_num, pnet );
            	    }
                }
                else
                {
            	    if( psynth_get_number_of_inputs( mod ) != 2 )
            	    {
                	psynth_set_number_of_inputs( 2, mod_num, pnet );
        	    }
            	    if( mod->in_empty[ 1 ] < offset + frames ) input_signal = 1;
            	    if( input_signal )
            	    {
			PS_STYPE* in1 = inputs[ 0 ] + offset;
			PS_STYPE* in2 = inputs[ 1 ] + offset;
		        for( int i = 0; i < frames; i++ ) 
		    	    in1[ i ] = ( (PS_STYPE2)in1[ i ] + in2[ i ] ) / (PS_STYPE2)2;
		    }
                }
		if( input_signal )
		{
		    data->empty_frames_counter = 0;
            	    if( data->ctl_abs )
            	    {
		        PS_STYPE* in = inputs[ 0 ] + offset;
			for( int i = 0; i < frames; i++ ) in[ i ] = PS_STYPE_ABS( in[ i ] );
            	    }
            	}
            	else
            	{
            	    if( data->empty_frames_counter < data->empty_frames_counter_max )
                    {
                        data->empty_frames_counter += frames;
                    }
                    else
                    {
                	if( data->opt->optimize ) break;
            	    }
            	}
		int tick_size = 1;
		if( data->ctl_freq > 0 )
		    tick_size = ( pnet->sampling_freq * 256 ) / data->ctl_freq;
		if( tick_size <= 0 ) tick_size = 1;
		psynth_event* ctl_evt = &data->ctl_evt;
		int ctl_min = data->ctl_min;
		int ctl_max = data->ctl_max;
		int range = ctl_max - ctl_min;
		int ctl_smooth = data->ctl_smooth;
		if( ctl_smooth >= 256 ) ctl_smooth = 255;
		int ctl_smooth2 = 256 - ctl_smooth;
		PS_STYPE2 accum = data->accum;
		int accum_cnt = data->accum_cnt;
		bool accum_empty = data->accum_empty;
		int ptr = 0;
		while( 1 )
                {
                    int buf_size = frames - ptr;
                    if( buf_size > ( tick_size - data->tick_counter ) / 256 ) buf_size = ( tick_size - data->tick_counter ) / 256;
                    if( ( tick_size - data->tick_counter ) & 255 ) buf_size++; 
                    if( buf_size > frames - ptr ) buf_size = frames - ptr;
                    if( buf_size < 0 ) buf_size = 0;
		    PS_STYPE* in = inputs[ 0 ] + offset;
		    if( data->ctl_alg == 0 )
		    {
			if( accum_empty )
			{
			    accum = in[ ptr ];
			    accum_empty = 0;
			}
		    }
		    else
		    {
			for( int i = ptr; i < ptr + buf_size; i++ )
			{
			    PS_STYPE val = in[ i ];
			    accum += val;
			}
			accum_cnt += buf_size;
			accum_empty = 0;
		    }
                    ptr += buf_size;
                    data->tick_counter += buf_size * 256;
                    if( data->tick_counter >= tick_size )
                    {
                	if( data->ctl_alg == 1 )
                	{
                	    accum /= accum_cnt;
                	}
                	int ctl_val;
                	if( data->ctl_gain != 256 )
                	{
                	    accum = ( accum * data->ctl_gain ) / 256;
                	}
                	ctl_val = accum * (PS_STYPE2)32768 / PS_STYPE_ONE;
                	if( ctl_val > 32768 ) ctl_val = 32768;
                	if( data->ctl_abs == 0 )
                	{
                	    if( ctl_val < -32768 ) ctl_val = -32768;
                	    ctl_val = ( ctl_val + 32768 ) / 2;
                	}
                	int r = ctl_val * range;
                	ctl_val = ctl_min + r / 32768;
                	if( data->prev_ctl_val == UNKNOWN_CTL_VAL )
                	{
                	    if( ctl_val == 0 )
                	    {
                		data->prev_ctl_val = ctl_val;
                	    }
                	}
                	else
                	{
                	    ctl_val = ( data->prev_ctl_val * ctl_smooth + ctl_val * ctl_smooth2 ) / 256;
                	}
                	if( data->opt->optimize == 0 || data->prev_ctl_val != ctl_val )
                	{
                	    data->empty_frames_counter = 0;
                	    data->prev_ctl_val = ctl_val;
                	    for( int i = 0; i < mod->output_links_num; i++ )
                	    {
                    		int l = mod->output_links[ i ];
                    		psynth_module* dest = psynth_get_module( l, pnet );
                    		if( !dest ) continue;
				int offset2;
                    		if( ptr > 0 )
                            	    offset2 = offset + ptr - 1;
                        	else
                            	    offset2 = offset;
                        	ctl_evt->offset = offset2;
                        	ctl_evt->controller.ctl_num = data->ctl_num - 1;
                        	ctl_evt->controller.ctl_val = ctl_val;
                        	psynth_add_event( l, ctl_evt, pnet );
#ifdef SUNVOX_GUI
				sunvox_engine* sv = (sunvox_engine*)pnet->host;
				if( data->opt->record_values && sv->recording )
				{
			    	    stime_ticks_t t = pnet->out_time + ( offset2 * stime_ticks_per_second() ) / pnet->sampling_freq;
				    if( pnet->th_num > 1 ) smutex_lock( &sv->rec_mutex );
				    if( sunvox_record_is_ready_for_event( sv ) )
				    {
			    		sunvox_record_write_time( REC_PAT_SOUND2CTL, sunvox_frames_get_value( SUNVOX_VF_CHAN_LINENUM, t, sv ), 0, sv );
		    			sunvox_record_write_byte( rec_evt_ctl + ( REC_PAT_SOUND2CTL << REC_EVT_PAT_OFFSET ), sv );
                			sunvox_record_write_int( data->ctl_num - 1, sv );
		            		sunvox_record_write_int( ctl_val, sv );
                			sunvox_record_write_int( l, sv );
                		    }
				    if( pnet->th_num > 1 ) smutex_unlock( &sv->rec_mutex );
				}
#endif
				break;
                	    }
                	}
                	accum = 0;
                	accum_cnt = 0;
                	accum_empty = 1;
                	data->tick_counter %= tick_size;
                    }
                    if( ptr >= frames ) break;
                }
                data->accum = accum;
                data->accum_cnt = accum_cnt;
                data->accum_empty = accum_empty;
	    }
	    retval = 0; 
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    {
		data->empty_frames_counter = 0;
		sound2ctl_options* opt = data->opt;
		int v = event->controller.ctl_val;
		switch( event->controller.ctl_num + 1 )
		{
		    case 3:
			data->ctl_abs = v;
			data->accum = 0;
		    	data->accum_cnt = 0;
			data->accum_empty = 1;
			retval = 1;
			break;
	    	    case 127: opt->record_values = v!=0; retval = 1; break;
    		    case 126: opt->optimize = v!=0; retval = 1; break;
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
	default: break;
    }
    return retval;
}
