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
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_compressor_data
#define MODULE_HANDLER	psynth_compressor
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define COMPRESSOR_BUF_SIZE 512 
#define COMPRESSOR_SCOPE_SIZE 2048
enum
{
    compressor_mode_peak = 0,
    compressor_mode_rms,
    compressor_mode_peak_zero_latency,
    compressor_modes
};
struct MODULE_DATA
{
    PS_CTYPE	ctl_volume;
    PS_CTYPE	ctl_threshold;
    PS_CTYPE	ctl_slope;
    PS_CTYPE	ctl_attack;
    PS_CTYPE	ctl_release;
    PS_CTYPE	ctl_peakmode;
    PS_CTYPE	ctl_sidechain;
    int		alg_version; 
    int		tick_counter;
    int		tick_size; 
    PS_STYPE2	peak;
#ifdef PS_STYPE_FLOATINGPOINT
    float	rms;
#else
    uint64_t 	rms;
#endif
    int		peak_cnt;
    PS_STYPE2	env; 
    PS_STYPE2	gain; 
    PS_STYPE2	gain_delta;
    PS_STYPE*	buf[ MODULE_OUTPUTS ];
    uint	buf_ptr;
#ifdef SUNVOX_GUI
    uint8_t	buf_peaks[ COMPRESSOR_SCOPE_SIZE ];
    uint8_t	buf_gain[ COMPRESSOR_SCOPE_SIZE ];
    int		scope_ptr;
    int		scope_ptr2;
#endif
    bool	empty;
    bool	ctls_changed;
    PS_STYPE2 	threshold;
    bool 	zero_threshold;
    PS_STYPE2 	slope;
    PS_STYPE2 	attack_coef; 
    PS_STYPE2 	release_coef; 
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#ifdef SUNVOX_GUI
struct compressor_visual_data
{
    MODULE_DATA*	module_data;
    int		    	mod_num;
    psynth_net*	    	pnet;
};
int compressor_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    compressor_visual_data* data = (compressor_visual_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( compressor_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->pnet = (psynth_net*)win->host;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		if( !data ) break;
		psynth_module* mod;
		MODULE_DATA* mdata;
		if( data->mod_num >= 0 )
		{
		    mod = &data->pnet->mods[ data->mod_num ];
		    mdata = (MODULE_DATA*)mod->data_ptr;
		}
		else break;
		wbd_lock( win );
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		char ts[ 512 ];
		sprintf( ts, "%s: %s", ps_get_string( STR_PS_SIDE_CHAIN ), ps_get_string( STR_PS_NONE ) );
		if( mdata->ctl_sidechain > 0 && mdata->ctl_sidechain <= mod->input_links_num )
		{
		    int l = mod->input_links[ mdata->ctl_sidechain - 1 ];
		    if( (unsigned)l < (unsigned)data->pnet->mods_num )
		    {
			psynth_module* s = &data->pnet->mods[ l ];
			char ts2[ 3 ];
			ts2[ 0 ] = int_to_hchar( l / 16 );
			ts2[ 1 ] = int_to_hchar( l & 15 );
			ts2[ 2 ] = 0;
			sprintf( ts, "%s: %s.%s", ps_get_string( STR_PS_SIDE_CHAIN ), ts2, s->name );
		    }
		}
		wm->cur_font_color = wm->color2;
		draw_string( ts, 0, 0, wm );
		int ychar = char_y_size( wm ) + 1;
		int y = ychar;
		int scope_ptr = mdata->scope_ptr - 1;
		scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
		int xsize = win->xsize;
		int ysize = wm->scrollbar_size * 6;
		if( ysize > win->ysize - ( y + wm->interelement_space ) )
                    ysize = win->ysize - ( y + wm->interelement_space );
                if( xsize > 4 && ysize > 4 )
                {
		    int prev_x = xsize;
		    int prev_y = 0;
		    int prev_v = 0;
		    int skip = 1;
#if CPUMARK < 10
		    skip = 2;
#endif
		    for( int i = 0; i < 1024; i += skip )
		    {
			int x = xsize - 1 - ( xsize * i ) / 1024;
			int v = mdata->buf_peaks[ scope_ptr ];
			if( v )
			{
			    v = ( v * ysize ) / 256;
			    if( v > prev_v ) prev_v = v;
			}
			if( x != prev_x )
			{
			    int y2 = y + ysize - prev_v;
			    draw_frect( x, y2, prev_x - x, prev_v, wm->color2, wm );
			    prev_v = 0;
			    prev_x = x;
			}
			scope_ptr -= skip;
			scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
		    }
		    wm->cur_opacity = 128;
		    int ty = ( mdata->ctl_threshold * ysize ) / 512;
		    draw_frect( 0, ychar, xsize, ysize - ty, blend( win->color, wm->color2, 128 ), wm );
		    draw_frect( 0, ychar + ysize - ty, xsize, 1, win->color, wm );
		    wm->cur_opacity = 255;
		    scope_ptr = mdata->scope_ptr - 1;
		    scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
		    prev_x = 0;
		    prev_y = 0;
		    for( int i = 0; i < 1024; i += skip )
		    {
			int x = xsize - 1 - ( xsize * i ) / 1024;
			int v = mdata->buf_gain[ scope_ptr ];
			if( v )
			{
			    v = ( v * ysize ) / 256;
			    int y = ychar + ysize - 1 - v;
			    if( prev_x != x || prev_y != y )
			    {
				draw_frect( x, y, 1, 1, wm->color3, wm );
			    }
			    prev_y = y;
			}
			prev_x = x;
			scope_ptr -= skip;
			scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
		    }
		    wm->cur_opacity = 128;
            	    if( ( xsize > ( wm->corners_len + wm->corners_size ) * 2 ) && ( ysize > ( wm->corners_len + wm->corners_size ) * 2 ) )
			draw_corners( wm->corners_size, ychar + wm->corners_size, xsize - wm->corners_size * 2, ysize - wm->corners_size * 2, wm->corners_size, wm->corners_len, wm->color3, wm );
		    wm->cur_opacity = 255;
		}
		wbd_draw( wm );
		wbd_unlock( wm );
	    }
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
	    retval = (PS_RETTYPE)"Compressor";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Компрессор аудиосигнала с боковой цепью (сайдчейн).\nThreshold 256 = 0 dB.\nАлгоритм компрессора немного отличается, если базовая версия SunVox больше 1.7.3.3.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Dynamic range compressor with side-chain.\nThreshold 256 = 0 dB.\nСompression algorithm is slightly different since SunVox 1.7.3.3.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF3000";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_DONT_FILL_INPUT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 7, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_THRESHOLD ), "", 0, 512, 256, 0, &data->ctl_threshold, 256, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SLOPE ), "%", 0, 200, 100, 0, &data->ctl_slope, 100, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), ps_get_string( STR_PS_MS ), 0, 500, 1, 0, &data->ctl_attack, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), ps_get_string( STR_PS_MS ), 1, 1000, 300, 0, &data->ctl_release, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_COMPRESSOR_MODES ), 0, compressor_modes - 1, 0, 1, &data->ctl_peakmode, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SIDE_CHAIN_INPUT ), "", 0, 32, 0, 1, &data->ctl_sidechain, -1, 2, pnet );
	    data->alg_version = 0;
	    if( pnet->base_host_version >= 0x01070303 )
	    {
		data->alg_version = 1;
	    }
	    data->tick_counter = 0;
	    data->tick_size = ( pnet->sampling_freq * 256 ) / 1000;
	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
	    {
		data->buf[ ch ] = SMEM_ZALLOC2( PS_STYPE, COMPRESSOR_BUF_SIZE );
	    }
	    data->buf_ptr = 0;
	    data->peak = 0;
	    data->rms = 0;
	    data->peak_cnt = 0;
	    if( data->alg_version == 0 )
		data->env = 0;
	    else
		data->env = PS_STYPE_ONE * PS_STYPE_ONE;
	    data->gain = 0;
	    data->gain_delta = 0;
	    data->ctls_changed = true;
	    data->empty = true;
#ifdef SUNVOX_GUI
	    smem_clear( data->buf_peaks, COMPRESSOR_SCOPE_SIZE );
	    smem_clear( data->buf_gain, COMPRESSOR_SCOPE_SIZE );
	    data->scope_ptr = 0;
	    data->scope_ptr2 = 0;
	    {
		data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "Compressor GUI", 0, 0, 10, 10, wm->color1, 0, pnet, compressor_visual_handler, wm );
		    mod->visual_min_ysize = 0;
		    compressor_visual_data* cv_data = (compressor_visual_data*)mod->visual->data;
		    cv_data->module_data = data;
		    cv_data->mod_num = mod_num;
		    cv_data->pnet = pnet;
		}
	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    data->tick_counter = 0;
	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
	    {
		smem_zero( data->buf[ ch ] );
	    }
	    data->buf_ptr = 0;
	    data->peak = 0;
	    data->rms = 0;
	    data->peak_cnt = 0;
	    if( data->alg_version == 0 )
		data->env = 0;
	    else
		data->env = PS_STYPE_ONE * PS_STYPE_ONE;
	    data->gain = 0;
	    data->gain_delta = 0;
	    data->empty = 1;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			data->empty = 0;
			break;
		    }
		}
		if( data->empty ) break;
    		if( data->ctls_changed )
    		{
    		    float srate = 1000;
    		    data->ctls_changed = false;
		    data->threshold = (PS_STYPE2)data->ctl_threshold * PS_STYPE_ONE / 256;
		    data->zero_threshold = false;
    		    if( data->alg_version == 1 || data->ctl_peakmode >= compressor_mode_peak_zero_latency )
		    {
			if( data->ctl_peakmode >= compressor_mode_peak_zero_latency ) srate = pnet->sampling_freq;
			if( data->threshold == 0 )
			{
			    data->threshold += 0.5F / 256.0F * PS_STYPE_ONE;
			    data->zero_threshold = true;
			}
		    }
		    data->slope = (PS_STYPE2)data->ctl_slope * PS_STYPE_ONE / 100;
		    data->attack_coef = 0;
		    data->release_coef = 0;
		    if( data->ctl_attack > 0 ) data->attack_coef = expf( -1.0F / ( srate * ( (float)data->ctl_attack / 1000.0F ) ) ) * PS_STYPE_ONE * PS_STYPE_ONE;
		    if( data->ctl_release > 0 ) data->release_coef = expf( -1.0F / ( srate * ( (float)data->ctl_release / 1000.0F ) ) ) * PS_STYPE_ONE * PS_STYPE_ONE;
		}
		for( int i = 0; i < mod->input_links_num; i++ )
		{
		    if( i + 1 == data->ctl_sidechain ) continue;
                    psynth_module* m = psynth_get_module( mod->input_links[ i ], pnet );
                    if( !m ) continue;
                    int mcc = psynth_get_number_of_outputs( m );
                    if( mcc == 0 ) continue;
                    if( !m->channels_out[ 0 ] ) continue; 
                    PS_STYPE** outputs2 = m->channels_out;
                    PS_STYPE* in = NULL;
                    int in_empty = 0;
                    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
                    {
                        if( outputs2[ ch ] && ch < mcc )
                        {
                            in = outputs2[ ch ] + offset;
                            in_empty = m->out_empty[ ch ] - offset;
                        }
                        PS_STYPE* RESTRICT main_in = inputs[ ch ] + offset;
			if( retval == 0 )
			{
                            if( in_empty >= frames )
                            {
                                for( int p = 0; p < frames; p++ )
                                    main_in[ p ] = 0;
                            }
                            else
                            {
				for( int p = 0; p < frames; p++ )
                                    main_in[ p ] = in[ p ];
                            }
                        }
			else
			{
                            if( in_empty >= frames )
                            {
                            }
                            else
                            {
				for( int p = 0; p < frames; p++ )
                                    main_in[ p ] += in[ p ];
                            }
                        }
                    }
                    retval = 1;
		}
		PS_STYPE** sc_outputs = NULL;
		int sc_num_outputs = 0;
		if( data->ctl_sidechain > 0 && data->ctl_sidechain <= mod->input_links_num )
		{
		    psynth_module* sc = psynth_get_module( mod->input_links[ data->ctl_sidechain - 1 ], pnet );
		    if( sc )
		    {
			if( sc->channels_out[ 0 ] )
			{
			    sc_num_outputs = psynth_get_number_of_outputs( sc );
			    if( sc_num_outputs )
				sc_outputs = sc->channels_out;
			}
		    }
		}
		if( data->ctl_peakmode >= compressor_mode_peak_zero_latency )
		{
		    PS_STYPE* RESTRICT in0 = inputs[ 0 ] + offset;
		    PS_STYPE* RESTRICT in1 = inputs[ 1 ] + offset;
		    PS_STYPE* RESTRICT sc_in0 = in0;
		    PS_STYPE* RESTRICT sc_in1 = in1;
		    if( sc_outputs )
		    {
			sc_in0 = sc_outputs[ 0 ] + offset;
			sc_in1 = sc_in0;
			if( sc_num_outputs > 1 )
			    sc_in1 = sc_outputs[ 1 ] + offset;
		    }
		    PS_STYPE* RESTRICT out0 = outputs[ 0 ] + offset;
		    PS_STYPE* RESTRICT out1 = outputs[ 1 ] + offset;
#ifdef SUNVOX_GUI
		    PS_STYPE2 max_peak = 0;
		    PS_STYPE2 min_gain = 10 * PS_STYPE_ONE;
#endif
		    if( retval == 0 )
		    {
			for( int i = 0; i < frames; i++ ) in0[ i ] = 0;
			for( int i = 0; i < frames; i++ ) in1[ i ] = 0;
		    }
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v;
			PS_STYPE2 peak = 0;
			v = PS_STYPE_ABS( sc_in0[ i ] ); if( v > peak ) peak = v;
			v = PS_STYPE_ABS( sc_in1[ i ] ); if( v > peak ) peak = v;
#ifdef SUNVOX_GUI
			if( peak > max_peak ) max_peak = peak;
#endif
			PS_STYPE2 gain = PS_STYPE_ONE;
			if( peak > data->threshold )
			{
			    if( data->zero_threshold )
			        gain = 0;
			    else
#ifdef PS_STYPE_FLOATINGPOINT
			        gain = data->threshold / ( data->threshold + ( peak - data->threshold ) * data->slope );
#else
			        gain = ( data->threshold << PS_STYPE_BITS ) / ( data->threshold + ( ( ( peak - data->threshold ) * data->slope ) >> PS_STYPE_BITS ) );
#endif
			}
			PS_STYPE2 theta = gain * PS_STYPE_ONE > data->env ? data->release_coef : data->attack_coef;
#ifdef PS_STYPE_FLOATINGPOINT
    			data->env = ( 1.0 - theta ) * gain + theta * data->env;
			gain = data->env;
#else
    			data->env = ( (int64_t)( PS_STYPE_ONE * PS_STYPE_ONE - theta ) * ( gain << PS_STYPE_BITS ) + (int64_t)theta * data->env ) >> ( PS_STYPE_BITS * 2 );
			gain = data->env >> PS_STYPE_BITS;
#endif
#ifdef SUNVOX_GUI
			if( gain < min_gain ) min_gain = gain;
#endif
			out0[ i ] = PS_NORM_STYPE_MUL( in0[ i ], gain, PS_STYPE_ONE );
			out1[ i ] = PS_NORM_STYPE_MUL( in1[ i ], gain, PS_STYPE_ONE );
		    }
#ifdef SUNVOX_GUI
		    int peak_i = max_peak * 128 / PS_STYPE_ONE;
		    if( peak_i > 255 ) peak_i = 255;
		    int gain_i = min_gain * 128 / PS_STYPE_ONE;
		    min_gain = min_gain * data->ctl_volume / 256;
		    if( gain_i > 255 ) gain_i = 255;
		    data->scope_ptr2 += frames;
		    int tick_size2 = data->tick_size * 2 / 256; 
		    while( data->scope_ptr2 >= tick_size2 )
		    {
			data->buf_peaks[ data->scope_ptr ] = peak_i;
			data->buf_gain[ data->scope_ptr ] = gain_i;
			data->scope_ptr++;
			data->scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
			data->scope_ptr2 -= tick_size2;
		    }
		    mod->draw_request++;
#endif
		    if( data->ctl_volume != 256 )
		    {
			PS_STYPE2 vol = PS_NORM_STYPE( data->ctl_volume, 256 );
			for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
			{
			    PS_STYPE* RESTRICT out = outputs[ ch ] + offset;
			    for( int i = 0; i < frames; i++ )
			    {
				out[ i ] = PS_NORM_STYPE_MUL( out[ i ], vol, 256 );
			    }
			}
		    }
		    retval = 1;
		    if( data->ctl_volume == 0 ) retval = 2;
		    break;
		}
		int ptr = 0;
		while( ptr < frames )
		{
		    int size = frames - ptr;
		    if( size > ( data->tick_size - data->tick_counter ) / 256 ) size = ( data->tick_size - data->tick_counter ) / 256;
		    if( ( data->tick_size - data->tick_counter ) & 255 ) size++; 
		    if( size > frames - ptr ) size = frames - ptr;
		    if( size < 0 ) size = 0;
		    uint start_buf_ptr = data->buf_ptr;
		    uint buf_ptr = data->buf_ptr;
		    if( retval == 0 )
		    {
			for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
			{
			    PS_STYPE* RESTRICT buf = data->buf[ ch ];
			    buf_ptr = data->buf_ptr;
			    for( int i = ptr; i < ptr + size; i++ )
			    {
				buf[ buf_ptr ] = 0;
				buf_ptr++;
				buf_ptr &= ( COMPRESSOR_BUF_SIZE - 1 );
			    }
			}
		    }
		    if( sc_outputs )
		    {
			PS_STYPE* in = 0;
			for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
			{
			    if( sc_outputs[ ch ] && ch < sc_num_outputs )
				in = sc_outputs[ ch ] + offset;
			    if( in )
			    {
			        if( data->ctl_peakmode == compressor_mode_peak )
				{
				    for( int i = ptr; i < ptr + size; i++ )
				    {
				        PS_STYPE2 v = in[ i ];
				        if( v < 0 ) v = -v;
				        if( v > data->peak ) data->peak = v;
				    }
				}
				else
				{
				    for( int i = ptr; i < ptr + size; i++ )
				    {
				        PS_STYPE2 v = in[ i ];
				        data->rms += v * v;
				    }
				}
			    }
			}
			if( retval )
			{
			    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
			    {
				PS_STYPE* RESTRICT in = inputs[ ch ] + offset;
				PS_STYPE* RESTRICT buf = data->buf[ ch ];
				buf_ptr = data->buf_ptr;
				for( int i = ptr; i < ptr + size; i++ )
				{
				    PS_STYPE2 v = in[ i ];
				    buf[ buf_ptr ] = v;
				    buf_ptr++;
				    buf_ptr &= ( COMPRESSOR_BUF_SIZE - 1 );
				}
			    }
			}
		    }
		    else
		    {
			if( retval )
			{
			    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
			    {
				PS_STYPE* RESTRICT in = inputs[ ch ] + offset;
				PS_STYPE* RESTRICT buf = data->buf[ ch ];
				buf_ptr = data->buf_ptr;
				if( data->ctl_peakmode == compressor_mode_peak )
				{
				    for( int i = ptr; i < ptr + size; i++ )
				    {
					PS_STYPE2 v = in[ i ];
					if( v < 0 ) v = -v;
					if( v > data->peak ) data->peak = v;
					buf[ buf_ptr ] = in[ i ];
					buf_ptr++;
					buf_ptr &= ( COMPRESSOR_BUF_SIZE - 1 );
				    }
				}
				else
				{
				    for( int i = ptr; i < ptr + size; i++ )
				    {
					PS_STYPE2 v = in[ i ];
					data->rms += v * v;
					buf[ buf_ptr ] = in[ i ];
					buf_ptr++;
					buf_ptr &= ( COMPRESSOR_BUF_SIZE - 1 );
				    }
				}
			    }
			}
		    }
		    data->buf_ptr = buf_ptr;
		    data->peak_cnt += size * MODULE_OUTPUTS;
		    PS_STYPE2 cur_gain = data->gain;
		    PS_STYPE2 gain_delta = data->gain_delta;
		    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		    {
			PS_STYPE* RESTRICT out = outputs[ ch ] + offset;
			PS_STYPE* RESTRICT buf = data->buf[ ch ];
			buf_ptr = ( start_buf_ptr - pnet->sampling_freq / 1000 ) & ( COMPRESSOR_BUF_SIZE - 1 );
			cur_gain = data->gain;
			for( int i = ptr; i < ptr + size; i++ )
			{
			    PS_STYPE2 v = buf[ buf_ptr ];
			    buf_ptr++;
			    buf_ptr &= ( COMPRESSOR_BUF_SIZE - 1 );
#ifdef PS_STYPE_FLOATINGPOINT
			    v *= cur_gain;
#else
			    v *= cur_gain >> PS_STYPE_BITS;
			    v >>= PS_STYPE_BITS;
#endif
			    out[ i ] = v;
			    cur_gain += gain_delta;
			}
		    }
		    data->gain = cur_gain;
		    ptr += size;
		    data->tick_counter += 256 * size;
		    if( data->tick_counter >= data->tick_size )
		    {
			data->tick_counter -= data->tick_size;
			PS_STYPE2 peak;
			if( data->ctl_peakmode == compressor_mode_peak )
			{
			    peak = data->peak;
			}
			else
			{
#ifdef PS_STYPE_FLOATINGPOINT
			    peak = sqrt( data->rms / data->peak_cnt );
#else
			    data->rms /= data->peak_cnt;
			    peak = sqrt_newton( (uint)data->rms );
#endif
			}
			data->peak = 0;
			data->rms = 0;
			data->peak_cnt = 0;
			PS_STYPE2 gain = PS_STYPE_ONE;
			if( data->alg_version == 0 )
			{
			    PS_STYPE2 theta = peak * PS_STYPE_ONE > data->env ? data->attack_coef : data->release_coef;
#ifdef PS_STYPE_FLOATINGPOINT
    			    data->env = ( 1.0 - theta ) * peak + theta * data->env;
			    if( data->env > data->threshold )
				gain = gain - ( data->env - data->threshold ) * data->slope;
#else
    			    data->env = ( (int64_t)( PS_STYPE_ONE * PS_STYPE_ONE - theta ) * ( peak << PS_STYPE_BITS ) + (int64_t)theta * data->env ) >> ( PS_STYPE_BITS * 2 );
			    if( ( data->env >> PS_STYPE_BITS ) > data->threshold )
				gain = gain - ( ( ( ( data->env >> PS_STYPE_BITS ) - data->threshold ) * data->slope ) >> PS_STYPE_BITS );
#endif
			}
			else
			{
			    if( peak > data->threshold )
			    {
				if( data->zero_threshold )
			    	    gain = 0;
				else
#ifdef PS_STYPE_FLOATINGPOINT
			    	    gain = data->threshold / ( data->threshold + ( peak - data->threshold ) * data->slope );
#else
			    	    gain = ( data->threshold << PS_STYPE_BITS ) / ( data->threshold + ( ( ( peak - data->threshold ) * data->slope ) >> PS_STYPE_BITS ) );
#endif
			    }
			    PS_STYPE2 theta = gain * PS_STYPE_ONE > data->env ? data->release_coef : data->attack_coef;
#ifdef PS_STYPE_FLOATINGPOINT
    			    data->env = ( 1.0 - theta ) * gain + theta * data->env;
			    gain = data->env;
#else
    			    data->env = ( (int64_t)( PS_STYPE_ONE * PS_STYPE_ONE - theta ) * ( gain << PS_STYPE_BITS ) + (int64_t)theta * data->env ) >> ( PS_STYPE_BITS * 2 );
			    gain = data->env >> PS_STYPE_BITS;
#endif
			}
			if( data->ctl_volume != 256 ) gain = gain * data->ctl_volume / 256;
			if( gain < 0 ) gain = 0;
			int tick_size2 = data->tick_size / 256;
#ifdef PS_STYPE_FLOATINGPOINT
			data->gain_delta = ( gain - data->gain ) / tick_size2;
			if( PS_STYPE_IS_MIN( data->gain_delta ) )
			{
			    data->gain_delta = 0;
			    data->gain = gain;
			}
#else
			data->gain_delta = ( ( gain << PS_STYPE_BITS ) - data->gain ) / tick_size2;
			if( PS_STYPE_IS_MIN( data->gain_delta ) )
			{
			    data->gain_delta = 0;
			    data->gain = gain << PS_STYPE_BITS;
			}
#endif
#ifdef SUNVOX_GUI
			int peak_i = peak * 128 / PS_STYPE_ONE;
			if( peak_i > 255 ) peak_i = 255;
			data->buf_peaks[ data->scope_ptr ] = peak_i;
			int gain_i = gain * 128 / PS_STYPE_ONE;
			if( gain_i > 255 ) gain_i = 255;
			data->buf_gain[ data->scope_ptr ] = gain_i;
			data->scope_ptr++;
			data->scope_ptr &= COMPRESSOR_SCOPE_SIZE - 1;
			mod->draw_request++;
#endif
		    }
		}
		retval = 1;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
    	    data->ctls_changed = true;
    	    break;
	case PS_CMD_CLOSE:
	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
	    {
		smem_free( data->buf[ ch ] );
	    }
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
