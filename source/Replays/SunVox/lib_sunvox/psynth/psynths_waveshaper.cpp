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

#include "psynth.h"
#include "psynths_dc_blocker.h"
#define MODULE_DATA	psynth_waveshaper_data
#define MODULE_HANDLER	psynth_waveshaper
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define SHAPE_SIZE	256
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_in_volume;
    PS_CTYPE	ctl_mix;
    PS_CTYPE	ctl_out_volume;
    PS_CTYPE	ctl_symmetric;
    PS_CTYPE	ctl_mode;
    PS_CTYPE	ctl_dc_filter;
    uint16_t*	shape;
    dc_filter   f;
    int     	empty_frames_counter;
    int     	empty_frames_counter_max;
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct waveshaper_visual_data
{
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    WINDOWPTR		win;
    WINDOWPTR		reset;
    WINDOWPTR		smooth;
    WINDOWPTR		load;
    WINDOWPTR		save;
    bool		pushed;
    int			win_y;
    int			win_ysize;
    int 		prev_i;
    int			prev_vel;
    int			min_ysize;
};
int waveshaper_reset_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    waveshaper_visual_data* data = (waveshaper_visual_data*)user_data;
    for( int i = 0; i < SHAPE_SIZE; i++ )
    {
	data->module_data->shape[ i ] = i * ( 65536 / SHAPE_SIZE );
    }
    draw_window( data->win, wm );
    data->pnet->change_counter++;
    return 0;
}
int waveshaper_smooth_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    waveshaper_visual_data* data = (waveshaper_visual_data*)user_data;
    for( int a = 0; a < 4; a++ )
    {
	for( int i = 1; i < SHAPE_SIZE - 1; i++ )
	{
	    int v1;
	    int v2;
	    v1 = data->module_data->shape[ i - 1 ];
	    v2 = data->module_data->shape[ i + 1 ];
	    v1 = ( v1 + v2 ) / 2;
	    data->module_data->shape[ i ] = v1;
	}
    }
    draw_window( data->win, wm );
    data->pnet->change_counter++;
    return 0;
}
int waveshaper_load_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    waveshaper_visual_data* data = (waveshaper_visual_data*)user_data;
    char ts[ 512 ];
    sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_LOAD_RAW_DATA ), SHAPE_SIZE );
    char* name = fdialog( ts, 0, ".sunvox_loadcurve_ws", 0, FDIALOG_FLAG_LOAD, wm );
    if( name )
    {
	sfs_file f = sfs_open( name, "rb" );
	if( f )
	{
	    sfs_read( data->module_data->shape, 2, SHAPE_SIZE, f );
	    sfs_close( f );
	    draw_window( data->win, wm );
	    data->pnet->change_counter++;
	}
    }
    return 0;
}
int waveshaper_save_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    waveshaper_visual_data* data = (waveshaper_visual_data*)user_data;
    char ts[ 512 ];
    sprintf( ts, "%s (%d x 16bit unsigned int)", ps_get_string( STR_PS_SAVE_RAW_DATA ), SHAPE_SIZE );
    char* name = fdialog( ts, "waveform16bit", ".sunvox_savecurve_ws", 0, 0, wm );
    if( name )
    {
	sfs_file f = sfs_open( name, "wb" );
	if( f )
	{
	    sfs_write( data->module_data->shape, 2, SHAPE_SIZE, f );
	    sfs_close( f );
	}
    }
    return 0;
}
int waveshaper_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    waveshaper_visual_data* data = (waveshaper_visual_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( waveshaper_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
    		data->win = win;
    		data->pnet = (psynth_net*)win->host;
    		data->pushed = 0;
    		data->win_ysize = 0;
    	        const char* bname;
                int bxsize;
                int bysize = wm->text_ysize;
                int x = 0;
                int y = 0;
    		bname = wm_get_string( STR_WM_RESET ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );	
    		data->reset = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
        	set_handler( data->reset, waveshaper_reset_handler, data, wm );
        	x += bxsize + wm->interelement_space2;
    		bname = ps_get_string( STR_PS_SMOOTH ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
    		wm->opt_button_flags = BUTTON_FLAG_AUTOREPEAT;
    		data->smooth = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
        	set_handler( data->smooth, waveshaper_smooth_handler, data, wm );
        	x += bxsize + wm->interelement_space2;
    		bname = wm_get_string( STR_WM_LOAD ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
		data->load = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
        	set_handler( data->load, waveshaper_load_handler, data, wm );
        	x += bxsize + wm->interelement_space2;
    		bname = wm_get_string( STR_WM_SAVE ); bxsize = button_get_optimal_xsize( bname, win->font, true, wm );
		data->save = new_window( bname, x, 0, bxsize, bysize, wm->button_color, win, button_handler, wm );
        	set_handler( data->save, waveshaper_save_handler, data, wm );
        	x += bxsize + wm->interelement_space2;
        	x = 0;
        	y += bysize + wm->interelement_space;
        	data->min_ysize = y + MAX_CURVE_YSIZE;
            }
            retval = 1;
            break;
        case EVT_MOUSEBUTTONDOWN:
            if( rx >= 0 && rx < win->xsize && ry >= data->win_y && ry < data->win_y + data->win_ysize )
    	    {
    		data->pushed = 1;
    	    }
        case EVT_MOUSEMOVE:
    	    if( data->pushed && data->win_ysize > 0 && win->xsize > 0 )
    	    {
    		int x_items = SHAPE_SIZE;
    		int y_items = 65536;
    		int i = ( ( rx - wm->scrollbar_size / 2 ) * x_items ) / ( win->xsize - wm->scrollbar_size );
    		if( i < 0 ) i = 0;
    		if( i >= x_items ) i = x_items - 1;
    		int vel = ( ( data->win_ysize - ( ry - data->win_y ) ) * y_items ) / data->win_ysize;
                if( vel < 0 ) vel = 0;
                if( vel >= y_items ) vel = y_items - 1;
        	uint16_t* v = data->module_data->shape;
        	if( evt->type == EVT_MOUSEBUTTONDOWN )
        	{
        	    v[ i ] = (uint16_t)vel;
        	}
        	else
        	{
        	    if( i != data->prev_i )
        	    {
        		uint vel2;
        		uint dv;
        		int i2;
        		int i2_end;
        		if( i > data->prev_i )
        		{
        		    i2 = data->prev_i;
        		    i2_end = i;
        		    vel2 = data->prev_vel << 15;
        		    dv = ( ( vel - data->prev_vel ) << 15 ) / ( i - data->prev_i );
        		}
        		else
        		{
        		    i2 = i;
        		    i2_end = data->prev_i;
        		    vel2 = vel << 15;
        		    dv = ( ( data->prev_vel - vel ) << 15 ) / ( data->prev_i - i );
        		}
        		for( ; i2 < i2_end; i2 ++ )
        		{
        		    if( (unsigned)i2 < (unsigned)x_items )
        			v[ i2 ] = (uint16_t)( vel2 >> 15 );
        		    vel2 += dv;
        		}
        	    }
        	    else
        	    {
        		v[ i ] = (uint16_t)vel;
        	    }
        	}
        	data->prev_i = i;
        	data->prev_vel = vel;
        	draw_window( win, wm );
		data->pnet->change_counter++;
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
        	COLOR c = blend( wm->color2, win->color, 150 );
        	uint16_t* v = data->module_data->shape;
        	int xstart = ( wm->scrollbar_size / 2 ) << 15;
        	int x = xstart;
        	int y = wm->text_ysize + wm->interelement_space;
        	int win_xsize = win->xsize - wm->scrollbar_size;
        	int xd = ( win_xsize << 15 ) / SHAPE_SIZE;
        	int win_ysize = psynth_get_curve_ysize( y, MAX_CURVE_YSIZE, win );
        	data->win_y = y;
        	data->win_ysize = win_ysize;
        	if( win_ysize > 0 && win_xsize > 0 )
        	{
        	    for( int i = 0; i < SHAPE_SIZE; i++ )
        	    {
        		int vel = v[ i ];
        		if( vel == 65535 ) vel = 65536;
        		int ysize = ( win_ysize * vel ) / 65536;
        		draw_frect( x >> 15, y + win_ysize - ysize, ( ( x + xd ) >> 15 ) - ( x >> 15 ), ysize, c, wm );
        		if( i == 0 )
        		{
        		    draw_frect( 0, y + win_ysize - ysize, ( x >> 15 ) - 1, ysize, c, wm );
        		}
        		if( i == SHAPE_SIZE - 1 )
        		{
        		    draw_frect( ( ( x + xd ) >> 15 ) + 1, y + win_ysize - ysize, wm->scrollbar_size, ysize, c, wm );
        		}
        		x += xd;
        	    }
		    psynth_draw_grid( xstart >> 15, y, ( ( xstart + xd * SHAPE_SIZE ) >> 15 ) - ( xstart >> 15 ), win_ysize, win );
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
	    retval = (PS_RETTYPE)"WaveShaper";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"WaveShaper позволяет изменять форму исходного сигнала, используя график зависимости выхода (ось Y) от входа (ось X).";
                        break;
                    }
		    retval = (PS_RETTYPE)"WaveShaper is an audio effect that changes an audio signal by mapping an input signal to the output signal by applying the user-defined curve";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#006BFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 6, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INPUT_VOLUME ), "", 0, 512, 256, 0, &data->ctl_in_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MIX ), "", 0, 256, 256, 0, &data->ctl_mix, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUTPUT_VOLUME ), "", 0, 512, 256, 0, &data->ctl_out_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SYMMETRIC ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_symmetric, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODES - 1, MODE_HQ, 1, &data->ctl_mode, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DC_BLOCKER ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_dc_filter, -1, 0, pnet );
            dc_filter_init( &data->f, pnet->sampling_freq );
#ifdef PS_STYPE_FLOATINGPOINT
            data->empty_frames_counter_max = pnet->sampling_freq;
#else
            data->empty_frames_counter_max = pnet->sampling_freq / 2;
#endif
            data->empty_frames_counter = data->empty_frames_counter_max;
#ifdef SUNVOX_GUI
            {
        	data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "WaveShaper GUI", 0, 0, 10, 10, wm->color1, 0, pnet, waveshaper_visual_handler, wm );
            	    waveshaper_visual_data* ws_data = (waveshaper_visual_data*)mod->visual->data;
            	    mod->visual_min_ysize = ws_data->min_ysize;
            	    ws_data->module_data = data;
            	    ws_data->mod_num = mod_num;
            	    ws_data->pnet = pnet;
            	}
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		data->shape = (uint16_t*)psynth_get_chunk_data( mod_num, 0, pnet );
		if( !data->shape )
		{
		    psynth_new_chunk( mod_num, 0, SHAPE_SIZE * sizeof( uint16_t ), 0, 0, pnet );
		    data->shape = (uint16_t*)psynth_get_chunk_data( mod_num, 0, pnet );
		    for( int i = 0; i < SHAPE_SIZE; i++ )
		    {
			data->shape[ i ] = i * ( 65536 / SHAPE_SIZE );
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    dc_filter_stop( &data->f );
            data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
                PS_STYPE** outputs = mod->channels_out;
                int frames = mod->frames;
                int offset = mod->offset;
                if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
                {
            	    if( psynth_get_number_of_outputs( mod ) != 1 )
            	    {
                	psynth_set_number_of_outputs( 1, mod_num, pnet );
                	psynth_set_number_of_inputs( 1, mod_num, pnet );
            	    }
                }
                else
                {
            	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
            	    {
                	psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
                	psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
            	    }
                }
                int outputs_num = psynth_get_number_of_outputs( mod );
                bool input_signal = 0;
                for( int ch = 0; ch < outputs_num; ch++ )
                {
                    if( mod->in_empty[ ch ] < offset + frames )
                    {
                        input_signal = 1;
                        break;
                    }
                }
                if( input_signal )
                {
                    data->empty_frames_counter = 0;
                }
                else
                {
                    data->empty_frames_counter += frames;
                    if( data->empty_frames_counter >= data->empty_frames_counter_max )
                    {
                	bool sleep = true;
                	if( pnet->base_host_version >= 0x02010200 )
            		{
            		    if( data->ctl_dc_filter == 0 )
            		    {
            			if( data->ctl_symmetric )
            			{
            			    if( data->shape[ 0 ] ) sleep = 0;
            			}
            			else
            			{
            			    int sp = 32768 / ( 65536 / SHAPE_SIZE );
				    if( data->shape[ sp ] != 32768 ) sleep = 0;
            			}
            		    }
            		}
            		if( sleep )
            		    break;
            		else
            		    data->empty_frames_counter = 0;
                    }
                }
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    if( data->ctl_in_volume != 256 )
		    {
			for( int i = 0; i < frames; i++ ) out[ i ] = (PS_STYPE2)in[ i ] * data->ctl_in_volume / (PS_STYPE2)256;
			in = out;
		    }
		    switch( data->ctl_mode )
		    {
			case MODE_LQ:
			case MODE_LQ_MONO:
			    if( data->ctl_symmetric )
			    {
				for( int i = 0; i < frames; i++ )
				{
				    PS_STYPE2 v = in[ i ];
				    int v16;
				    PS_STYPE_TO_INT16( v16, v )
				    if( v16 > 0 )
				    {
					int res = data->shape[ v16 / ( 32768 / SHAPE_SIZE ) ] / 2;
					PS_INT16_TO_STYPE( out[ i ], res );
				    }
				    else
				    {
					v16 = -v16;
					if( v16 == 32768 ) v16 = 32767;
					int res = -data->shape[ v16 / ( 32768 / SHAPE_SIZE ) ] / 2;
					PS_INT16_TO_STYPE( out[ i ], res );
				    }
				}
			    }
			    else
			    {
				for( int i = 0; i < frames; i++ )
				{
				    PS_STYPE2 v = in[ i ];
				    int v16;
				    PS_STYPE_TO_INT16( v16, v )
				    v16 += 32768;
				    int res = data->shape[ v16 / ( 65536 / SHAPE_SIZE ) ];
				    res -= 32768;
				    PS_INT16_TO_STYPE( out[ i ], res );
				}
			    }
			    break;
			case MODE_HQ:
			case MODE_HQ_MONO:
			    if( data->ctl_symmetric )
			    {
#ifdef PS_STYPE_FLOATINGPOINT
				if( data->shape[ 0 ] == 0 )
				{
				    for( int i = 0; i < frames; i++ )
				    {
					PS_STYPE2 v = in[ i ];
					LIMIT_NUM( v, -(PS_STYPE)(32767-128) / (PS_STYPE)32768, (PS_STYPE)(32767-128) / (PS_STYPE)32768 );
					v *= (PS_STYPE)256;
					if( v > 0 )
					{
					    int iv = v;
					    PS_STYPE frac = v - iv;
					    PS_STYPE res1 = data->shape[ iv ];
					    PS_STYPE res2 = data->shape[ iv + 1 ];
					    out[ i ] = ( res1 * ( (PS_STYPE)1 - frac ) + res2 * frac ) * ((PS_STYPE)1/(PS_STYPE)65536);
					}
					else
					{
					    v = -v;
					    int iv = v;
					    PS_STYPE2 frac = v - iv;
					    PS_STYPE2 res1 = data->shape[ iv ];
					    PS_STYPE2 res2 = data->shape[ iv + 1 ];
					    out[ i ] = -( res1 * ( (PS_STYPE)1 - frac ) + res2 * frac ) * ((PS_STYPE)1/(PS_STYPE)65536);
					}
				    }
				}
				else
				{
				    for( int i = 0; i < frames; i++ )
				    {
					PS_STYPE2 v = in[ i ] + (PS_STYPE)0x1.FEp-62;
					LIMIT_NUM( v, -(PS_STYPE)(32767-128) / (PS_STYPE)32768, (PS_STYPE)(32767-128) / (PS_STYPE)32768 );
					v *= (PS_STYPE)256;
					if( v > 0 )
					{
					    int iv = v;
					    PS_STYPE frac = v - iv;
					    PS_STYPE res1 = data->shape[ iv ];
					    PS_STYPE res2 = data->shape[ iv + 1 ];
					    out[ i ] = ( res1 * ( (PS_STYPE)1 - frac ) + res2 * frac ) * ((PS_STYPE)1/(PS_STYPE)65536);
					}
					else
					{
					    v = -v;
					    int iv = v;
					    PS_STYPE2 frac = v - iv;
					    PS_STYPE2 res1 = data->shape[ iv ];
					    PS_STYPE2 res2 = data->shape[ iv + 1 ];
					    out[ i ] = -( res1 * ( (PS_STYPE)1 - frac ) + res2 * frac ) * ((PS_STYPE)1/(PS_STYPE)65536);
					}
				    }
				}
#else
				for( int i = 0; i < frames; i++ )
				{
				    PS_STYPE2 v = in[ i ];
				    int v16;
				    PS_STYPE_TO_INT16( v16, v )
				    if( v16 > 0 )
				    {
					if( v16 > 32767 - ( 32768 / SHAPE_SIZE ) )
					    v16 = 32767 - ( 32768 / SHAPE_SIZE );
					int res1 = data->shape[ v16 / ( 32768 / SHAPE_SIZE ) ];
					int res2 = data->shape[ v16 / ( 32768 / SHAPE_SIZE ) + 1 ];
					int c = v16 & ( ( 32768 / SHAPE_SIZE ) - 1 );
					int cc = ( 32768 / SHAPE_SIZE ) - 1 - c;
					int res = ( res1 * cc + res2 * c ) / ( 32768 / SHAPE_SIZE );
					res /= 2;
					PS_INT16_TO_STYPE( out[ i ], res );
				    }
				    else
				    {
					v16 = -v16;
					if( v16 > 32767 - ( 32768 / SHAPE_SIZE ) )
					    v16 = 32767 - ( 32768 / SHAPE_SIZE );
					int res1 = data->shape[ v16 / ( 32768 / SHAPE_SIZE ) ];
					int res2 = data->shape[ v16 / ( 32768 / SHAPE_SIZE ) + 1 ];
					int c = v16 & ( ( 32768 / SHAPE_SIZE ) - 1 );
					int cc = ( 32768 / SHAPE_SIZE ) - 1 - c;
					int res = -( res1 * cc + res2 * c ) / ( 32768 / SHAPE_SIZE );
					res /= 2;
					PS_INT16_TO_STYPE( out[ i ], res );
				    }
				}
#endif
			    }
			    else
			    {
				for( int i = 0; i < frames; i++ )
				{
				    PS_STYPE2 v = in[ i ];
#ifdef PS_STYPE_FLOATINGPOINT
				    v += (PS_STYPE)1;
				    LIMIT_NUM( v, 0, (PS_STYPE)(65535-256) / (PS_STYPE)32768 );
				    v *= (PS_STYPE)128;
				    int iv = v;
				    PS_STYPE frac = v - iv;
				    PS_STYPE res1 = data->shape[ iv ];
				    PS_STYPE res2 = data->shape[ iv + 1 ];
				    out[ i ] = ( ( res1 * ( (PS_STYPE)1 - frac ) + res2 * frac ) - (PS_STYPE)32768 ) * ((PS_STYPE)1/(PS_STYPE)32768);
#else
				    int v16;
				    PS_STYPE_TO_INT16( v16, v );
				    v16 += 32768; 
				    uint p = v16 / ( 65536 / SHAPE_SIZE ); 
				    int res1;
				    int res2;
				    res1 = data->shape[ p ];
				    if( p + 1 < SHAPE_SIZE )
					res2 = data->shape[ p + 1 ];
				    else
					res2 = res1;
				    int c = v16 & ( ( 65536 / SHAPE_SIZE ) - 1 );
                                    int cc = ( 65536 / SHAPE_SIZE ) - 1 - c;
                                    int res = ( res1 * cc + res2 * c ) / ( 65536 / SHAPE_SIZE );
				    res -= 32768;
				    PS_INT16_TO_STYPE( out[ i ], res );
#endif
				}
			    }
			    break;
		    }
		    in = inputs[ ch ] + offset;
		    if( data->ctl_mix != 256 )
		    {
                        for( int i = 0; i < frames; i++ )
                        {
                            PS_STYPE2 v1 = in[ i ];
                            PS_STYPE2 v2 = out[ i ];
                            v2 = v2 * data->ctl_mix / (PS_STYPE2)256;
                            v1 = v1 * ( 256 - data->ctl_mix ) / (PS_STYPE2)256;
                            out[ i ] = v1 + v2;
                        }
		    }
		    if( data->ctl_out_volume != 256 )
		    {
			for( int i = 0; i < frames; i++ ) out[ i ] = (PS_STYPE2)out[ i ] * data->ctl_out_volume / (PS_STYPE2)256;
			in = out;
		    }
		    if( data->ctl_dc_filter )
		    {
			dc_filter_run( &data->f, ch, out, out, frames );
		    }
		}
	    }
	    retval = 1;
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
                uint16_t* curve = NULL;
                int chunk_id = -1;
                int len = 0;
                int reqlen = event->offset;
                switch( event->id )
                {
                    case 0: chunk_id = 0; len = 256; break;
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
                            for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve[ i ] / 65535.0F; }
                        else
                            for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * 65535.0F; LIMIT_NUM( v, 0, 65535 ); curve[ i ] = v; }
                        retval = reqlen;
                    }
                }
            }
            break;
	default: break;
    }
    return retval;
}
