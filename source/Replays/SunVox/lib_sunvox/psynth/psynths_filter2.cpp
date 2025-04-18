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

#include "psynth.h"
#define MODULE_DATA	psynth_filter2_data
#define MODULE_HANDLER	psynth_filter2
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_FREQ	22000
#define MAX_Q		32768
#define MAX_GAIN	32768
#define MAX_VOLUME	32768
#define MAX_ROLLOFF	(8-1)
#define MAX_RESPONSE	1000
#if MAX_ROLLOFF + 1 > BIQUAD_FILTER_STAGES
    #error "Not enough BIQUAD_FILTER_STAGES"
#endif
struct MODULE_DATA
{
    PS_CTYPE   		ctl_volume;
    PS_CTYPE   		ctl_type;
    PS_CTYPE   		ctl_freq;
    PS_CTYPE		ctl_freq_fine;
    PS_CTYPE		ctl_freq_scale;
    PS_CTYPE		ctl_exp_freq;
    PS_CTYPE   		ctl_q;
    PS_CTYPE		ctl_gain;
    PS_CTYPE   		ctl_rolloff;
    PS_CTYPE		ctl_oversampling;
    PS_CTYPE   		ctl_response;
    PS_CTYPE   		ctl_mode; 
    PS_CTYPE   		ctl_mix;
    PS_CTYPE   		ctl_lfo_freq;
    PS_CTYPE   		ctl_lfo_amp;
    PS_CTYPE   		ctl_lfo_type;
    PS_CTYPE   		ctl_set_lfo_phase;
    PS_CTYPE   		ctl_lfo_freq_units;
    biquad_filter* 	f;
    int	    		tick_counter;   
    bool		first_reinit_without_interp; 
    int     		lfo_phase;
    int			lfo_rand;
    int			lfo_rand_prev;
    uint8_t*  		hsin;
    float		floating_freq;
    float		floating_q;
    float		floating_gain;
    float   		floating_vol;
    float   		floating_mix;
    bool                empty; 
    int	    		empty_frames_counter;
    int	    		empty_frames_counter_max;
    int     		host_tick_size; 
    uint8_t   		ticks_per_line;
#ifdef SUNVOX_GUI
    window_manager* 	wm;
#endif
};
static float filter2_get_freq( MODULE_DATA* data );
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct filter2_visual_data
{
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
};
int filter2_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    filter2_visual_data* data = (filter2_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( filter2_visual_data );
            break;
        case EVT_AFTERCREATE:
            data->pnet = (psynth_net*)win->host;
            retval = 1;
            break;
        case EVT_DRAW:
    	    {
        	wbd_lock( win );
        	draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
        	biquad_filter* f = data->module_data->f;
        	biquad_filter_float decibels = 18; 
        	biquad_filter_float decibel_size = win->ysize / 2 / decibels;
        	COLOR c2 = blend( wm->color2, win->color, 128 );
        	int xstep = 1;
#if CPUMARK < 10
		xstep = 2;
#endif
        	int min_freq = 20;
        	int max_freq = f->sfreq / 2;
        	biquad_filter_float octaves = LOG2( (biquad_filter_float)max_freq / (biquad_filter_float)min_freq );
        	int prev_y = 0;
        	if( get_biquad_filter_ftype( f->type ) == BIQUAD_FILTER_APF )
        	{
		    int y = win->ysize / 2;
		    draw_frect( 0, y, win->xsize, win->ysize - y, c2, wm );
        	    draw_line( 0, y, win->xsize, y, wm->color3, wm );
        	}
        	else
        	{
        	    for( int x = 0; x < win->xsize; x += xstep )
        	    {
        		biquad_filter_float freq = (biquad_filter_float)min_freq * pow( 2, ( (biquad_filter_float)x * octaves ) / win->xsize );
        		biquad_filter_float v = biquad_filter_freq_response( f, freq );
        		if( v < 9e-51 ) 
        		    v = -1000;
        		else
		    	    v = 20 * log10( v );
			v *= decibel_size;
			int y = win->ysize / 2 - v;
			if( y > win->ysize * 4 ) y = win->ysize * 4;
			draw_frect( x, y, xstep, win->ysize - y, c2, wm );
        		if( x > 0 ) draw_line( x - xstep, prev_y, x, y, wm->color3, wm );
        		prev_y = y;
        	    }
        	}
        	if( char_y_size( wm ) * 2 < win->ysize )
        	{
        	    biquad_filter_float freq = filter2_get_freq( data->module_data );
        	    biquad_filter_float x = LOG2( freq / min_freq ) / octaves * win->xsize;
        	    wm->cur_opacity = 64;
        	    draw_frect( x, 0, 1, win->ysize, wm->color3, wm );
        	    wm->cur_opacity = 255;
        	    char ts[ 256 ];
        	    sprintf( ts, "%.02f", freq );
        	    int str_xsize = string_x_size( ts, wm );
        	    wm->cur_font_color = wm->color3;
        	    if( x + str_xsize > win->xsize )
        		x -= str_xsize;
        	    draw_string( ts, x, win->ysize - char_y_size( wm ), wm );
        	}
        	draw_decibel_grid( 0, 0, win->xsize, win->ysize, decibels, -decibels, 6, wm );
        	draw_frequency_grid( 0, 0, win->xsize, win->ysize, min_freq, max_freq, wm );
        	wbd_draw( wm );
        	wbd_unlock( wm );
    	    }
            retval = 1;
            break;
    }
    return retval;
}
#endif
static float filter2_get_freq( MODULE_DATA* data )
{
    float f = (float)data->ctl_freq + ( (float)data->ctl_freq_fine / 1000.0F - 1 );
    if( data->ctl_exp_freq )
    {
	f = f * f / (float)MAX_FREQ;
    }
    return f * data->ctl_freq_scale / 100.0F;
}
static float filter2_get_freq_with_lfo( MODULE_DATA* data )
{
    float freq = filter2_get_freq( data );
    if( data->ctl_lfo_amp )
    {
        int lfo_phase = 0;
        switch( data->ctl_lfo_freq_units )
        {
    	    case 0:
    		lfo_phase = data->lfo_phase / 16;
	        break;
	    case 1:
	    case 2:
	        lfo_phase = data->lfo_phase / 1000;
	        break;
	    case 3:
	        lfo_phase = data->lfo_phase / data->host_tick_size;
	        break;
	    case 4:
	    case 5:
	    case 6:
	        lfo_phase = data->lfo_phase / ( ( data->host_tick_size * data->ticks_per_line ) / 8 );
	        break;
	    default:
	        break;
	}
	lfo_phase &= 511;
	int v = 0;
	switch( data->ctl_lfo_type )
	{
	    case 0:
		if( lfo_phase < 256 )
		    v = (int)data->hsin[ lfo_phase ];
		else
		    v = -(int)data->hsin[ lfo_phase & 255 ];
		break;
	    case 1:
		v = lfo_phase - 255;
		break;
	    case 2:
		v = -lfo_phase + 255;
		break;
	    case 3:
		if( lfo_phase < 256 )
		    v = 255;
		else
		    v = -255;
		break;
	    case 4:
		{
		    if( data->lfo_rand_prev != lfo_phase / 256 )
		    {
			data->lfo_rand_prev = lfo_phase / 256;
			data->lfo_rand = pseudo_random() & 511;
		    }
		    v = data->lfo_rand - 255;
		}
		break;
	}
	float lfo_amp = (float)data->ctl_lfo_amp / (float)MAX_VOLUME;
	float lfo_value = ( ( (float)v * ( MAX_FREQ / 2 ) ) / 256 ) * lfo_amp;
	float lfo_offset = 0;
	float lfo_max = lfo_amp * ( MAX_FREQ / 2 );
	if( freq - lfo_max < 0 )
	    lfo_offset = -( freq - lfo_max );
	if( freq + lfo_max > MAX_FREQ )
	    lfo_offset = -( ( freq + lfo_max ) - MAX_FREQ );
	freq += lfo_value + lfo_offset;
    }
    return freq;
}
static void filter2_set_floating_values( MODULE_DATA* data )
{
    data->floating_freq = filter2_get_freq_with_lfo( data );
    data->floating_q = data->ctl_q;
    data->floating_gain = data->ctl_gain;
    data->floating_vol = data->ctl_volume;
    data->floating_mix = data->ctl_mix;
}
static void filter2_floating_step( MODULE_DATA* data )
{
    int r = data->ctl_response;
    if( r < 1 ) r = 1;
    float response = (float)r / (float)MAX_RESPONSE;
    data->floating_freq = ( 1 - response ) * data->floating_freq + response * filter2_get_freq_with_lfo( data );
    data->floating_q = ( 1 - response ) * data->floating_q + response * (float)data->ctl_q;
    data->floating_gain = ( 1 - response ) * data->floating_gain + response * (float)data->ctl_gain;
    data->floating_vol = ( 1 - response ) * data->floating_vol + response * (float)data->ctl_volume;
    data->floating_mix = ( 1 - response ) * data->floating_mix + response * (float)data->ctl_mix;
}
#define SET_PHASE( CHECK_VERSION ) \
{ \
    if( ( CHECK_VERSION && pnet->base_host_version >= 0x01090300 ) || CHECK_VERSION == 0 ) \
    { \
        psynth_event e; \
        smem_clear_struct( e ); \
        e.command = PS_CMD_SET_GLOBAL_CONTROLLER; \
        e.controller.ctl_num = 15; \
        e.controller.ctl_val = data->ctl_set_lfo_phase << 7; \
        mod->handler( mod_num, &e, pnet ); \
    } \
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
	    retval = (PS_RETTYPE)"Filter Pro";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Рекурсивный фильтр повышенной точности (64 бита).\n"
                    	    STR_NOTE_RESET_LFO_TO_RU " \"Установ.фазу LFO\".";
                        break;
                    }
		    retval = (PS_RETTYPE)"High-quality 64-bit IIR filter.\n"
			STR_NOTE_RESET_LFO_TO " \"Set LFO phase\" value.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FB5FF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 17, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, MAX_VOLUME, MAX_VOLUME, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TYPE ), ps_get_string( STR_PS_FILTER2_TYPES ), 0, BIQUAD_FILTER_TYPES - 1, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ ), ps_get_string( STR_PS_HZ ), 0, MAX_FREQ, MAX_FREQ, 0, &data->ctl_freq, -1, 0, pnet );
	    psynth_set_ctl_flags( mod_num, 2, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_FINETUNE ), ps_get_string( STR_PS_HZ1000 ), 0, 2000, 1000, 0, &data->ctl_freq_fine, 1000, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 3, -1000, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_SCALE ), "%", 0, 200, 100, 0, &data->ctl_freq_scale, 100, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_EXP_FREQ ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_exp_freq, -1, 0, pnet );
	    psynth_register_ctl( mod_num, "Q", "", 0, MAX_Q, MAX_Q / 2, 0, &data->ctl_q, MAX_Q / 2, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FILTER2_GAIN ), "", 0, MAX_GAIN, MAX_GAIN / 2, 0, &data->ctl_gain, MAX_GAIN / 2, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 7, -MAX_GAIN / 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ROLLOFF ), ps_get_string( STR_PS_FILTER_ROLLOFF_VALS ), 0, MAX_ROLLOFF, 0, 1, &data->ctl_rolloff, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESPONSE ), "", 0, MAX_RESPONSE, MAX_RESPONSE * 0.25, 0, &data->ctl_response, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_FILTER2_MODES ), 0, 3, 0, 1, &data->ctl_mode, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MIX ), "", 0, MAX_VOLUME, MAX_VOLUME, 0, &data->ctl_mix, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ ), "", 0, 1024, 8, 0, &data->ctl_lfo_freq, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_AMP ), "", 0, MAX_VOLUME, 0, 0, &data->ctl_lfo_amp, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_WAVEFORM ), ps_get_string( STR_PS_FILTER_LFO_WAVEFORM_TYPES ), 0, 4, 0, 1, &data->ctl_lfo_type, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SET_LFO_PHASE ), "", 0, 256, 0, 0, &data->ctl_set_lfo_phase, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ_UNIT ), ps_get_string( STR_PS_FILTER_LFO_FREQ_UNITS ), 0, 6, 0, 1, &data->ctl_lfo_freq_units, -1, 2, pnet );
	    data->ctl_oversampling = 0;
	    {
		uint32_t fflags = 0;
		if( pnet->base_host_version >= 0x02010200 )
		{
		    fflags |= BIQUAD_FILTER_FLAG_USE_STAGES_FOR_APF;
		}
		data->f = biquad_filter_new( fflags );
	    }
	    filter2_set_floating_values( data );
	    data->tick_counter = 0;
	    data->lfo_phase = 0;
	    data->lfo_rand = 0;
	    data->lfo_rand_prev = 0;
	    data->hsin = g_hsin_tab;
	    data->empty = true;
	    data->empty_frames_counter_max = pnet->sampling_freq * 2;
#ifdef SUNVOX_GUI
            {
                data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
                    mod->visual = new_window( "Filter2 GUI", 0, 0, 10, 10, wm->color1, 0, pnet, filter2_visual_handler, wm );
                    mod->visual_min_ysize = 0;
            	    filter2_visual_data* fv_data = (filter2_visual_data*)mod->visual->data;
                    fv_data->module_data = data;
                    fv_data->mod_num = mod_num;
                    fv_data->pnet = pnet;
                }
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    SET_PHASE( 1 );
	    filter2_set_floating_values( data );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    biquad_filter_stop( data->f );
	    data->tick_counter = 0;
	    data->empty = true;
	    data->empty_frames_counter = 0;
	    SET_PHASE( 1 );
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
		bool channels_changed = false;
		int outputs_num = psynth_get_number_of_outputs( mod );
		if( ( data->ctl_mode & 1 ) == 1 )
		{
		    if( outputs_num != 1 )
		    {
			psynth_set_number_of_outputs( 1, mod_num, pnet );
			psynth_set_number_of_inputs( 1, mod_num, pnet );
			channels_changed = true;
		    }
		}
		else
		{
		    if( outputs_num != MODULE_OUTPUTS )
		    {
			psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
			psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
			channels_changed = true;
		    }
		}
		if( channels_changed )
		{
		    outputs_num = psynth_get_number_of_outputs( mod );
		    biquad_filter_stop( data->f );
		}
		bool input_signal = false;
		bool check_filter_output = false;
		bool finished = true;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			input_signal = true;
			break;
		    }
		}
		if( input_signal )
		{
		    data->empty = false;
		    data->empty_frames_counter = 0;
		}
		else
		{
		    if( data->empty == false )
		    {
			if( data->empty_frames_counter < data->empty_frames_counter_max )
			{
		    	    data->empty_frames_counter += frames;
			}
			else
			{
			    check_filter_output = true;
			    data->empty_frames_counter = 0;
			}
		    }
		}
		int tick_size;
		if( data->ctl_lfo_freq_units == 1 || data->ctl_lfo_freq_units == 2 ) 
		    tick_size = ( pnet->sampling_freq * 256 ) / 1000; 
		else
		    tick_size = ( pnet->sampling_freq * 256 ) / 200; 
		if( data->ctl_response == MAX_RESPONSE )
		{
		    filter2_floating_step( data );
		}
		int ptr = 0;
		while( 1 )
		{
		    int size = frames - ptr;
    		    if( size > ( tick_size - data->tick_counter ) / 256 ) size = ( tick_size - data->tick_counter ) / 256;
    		    if( ( tick_size - data->tick_counter ) & 255 ) size++; 
    		    if( size > frames - ptr ) size = frames - ptr;
    		    if( size < 0 ) size = 0;
		    biquad_filter* f = data->f;
		    biquad_filter_float Q = data->floating_q / ( MAX_Q / 2 );
                    Q *= Q;
                    Q *= Q;
		    biquad_filter_float gain = data->floating_gain - ( MAX_GAIN / 2 );
		    gain = gain / ( MAX_GAIN / 2 ) * 24;
                    int interp_len = tick_size / 256;
                    if( data->ctl_response == MAX_RESPONSE ) 
                        interp_len = 0; 
                    if( data->first_reinit_without_interp )
                    {
                	data->first_reinit_without_interp = false;
                	interp_len = 0;
                    }
		    if( biquad_filter_change( 
		        f,
		        interp_len,
		        make_biquad_filter_type( (biquad_filter_ftype)data->ctl_type, BIQUAD_FILTER_Q_IS_Q, data->ctl_rolloff + 1, ( data->ctl_mode & (1<<1) ) != 0 ),
		        pnet->sampling_freq << data->ctl_oversampling,
		        data->floating_freq,
		        gain,
		        Q ) )
		    {
		        mod->draw_request++;
		    }
		    if( !data->empty )
		    {
			int vol = data->floating_vol;
			int mix = data->floating_mix;
    			for( int ch = 0; ch < outputs_num; ch++ )
			{
			    PS_STYPE* in = inputs[ ch ] + offset + ptr;
			    PS_STYPE* out = outputs[ ch ] + offset + ptr;
			    if( data->ctl_oversampling == 0 )
			    {
				biquad_filter_run( f, ch, in, out, size );
			    }
			    else
			    {
			    }
			    if( check_filter_output && finished )
			    {
				for( int i = 0; i < size; i++ )
				    if( PS_STYPE_IS_NOT_MIN( out[ i ] ) )
				    {
					finished = false;
					break;
				    }
			    }
			    if( vol == 0 )
			    {
				for( int i = 0; i < size; i++ ) 
				    out[ i ] = 0;
			    }
			    else
			    {
				if( mix == 0 )
				{
				    for( int i = 0; i < size; i++ )
					out[ i ] = in[ i ];
				}
				else
				{
				    if( mix != MAX_VOLUME )
				    {
					PS_STYPE2 vol2 = mix;
				        PS_STYPE2 vol3 = MAX_VOLUME - mix;
					for( int i = 0; i < size; i++ )
					{
					    PS_STYPE2 v = ( (PS_STYPE2)in[ i ] * vol3 + (PS_STYPE2)out[ i ] * vol2 ) / MAX_VOLUME;
					    out[ i ] = (PS_STYPE)v;
    					}
				    }
				}
				if( vol != MAX_VOLUME )
				{
			    	    for( int i = 0; i < size; i++ )
			    	    {
			    		PS_STYPE2 v = out[ i ];
					v = ( v * vol ) / MAX_VOLUME;
					out[ i ] = (PS_STYPE)v;
				    }
				}
			    }
			}
			if( vol != 0 ) retval = 1;
		    }
		    ptr += size;
		    data->tick_counter += size * 256;
		    if( data->tick_counter >= tick_size ) 
		    {
			filter2_floating_step( data );
			data->tick_counter %= tick_size;
			switch( data->ctl_lfo_freq_units )
			{
			    case 0:
				data->lfo_phase += data->ctl_lfo_freq;
				data->lfo_phase &= 8191;
				break;
			    case 1:
				if( data->ctl_lfo_freq > 0 )
				{
				    data->lfo_phase += ( 1000 * 512 ) / data->ctl_lfo_freq;
				    data->lfo_phase %= ( 1000 * 512 );
				}
				break;
			    case 2:
				data->lfo_phase += data->ctl_lfo_freq * 512;
				data->lfo_phase %= 1000 * 512;
				break;
			    case 3:
				if( data->ctl_lfo_freq )
				{
				    data->lfo_phase += ( tick_size * 512 ) / data->ctl_lfo_freq;
				    data->lfo_phase %= ( data->host_tick_size * 512 );
				}
				break;
			    case 4:
			    case 5:
			    case 6:
				if( data->ctl_lfo_freq )
				{
				    data->lfo_phase += ( ( ( tick_size / 8 ) * 512 ) / data->ctl_lfo_freq ) * ( data->ctl_lfo_freq_units - 3 );
				    data->lfo_phase %= ( ( ( data->host_tick_size * data->ticks_per_line ) / 8 ) * 512 );
				}
				break;
			}
#ifdef SUNVOX_GUI
			{
                    	    psynth_ctl* ctl = &mod->ctls[ 15 ];
                    	    uint bits = 0;
                	    uint div = 1;
                	    switch( data->ctl_lfo_freq_units )
                    	    {
                    	        case 0:
                            	    bits = 13;
                            	    break;
                        	case 1:
                        	case 2:
                            	    div = 1000;
                            	    bits = 9;
                            	    break;
                        	case 3:
                            	    div = data->host_tick_size;
                            	    bits = 9;
                            	    break;
                        	case 4:
                        	case 5:
                        	case 6:
                            	    div = data->host_tick_size * data->ticks_per_line / 8;
                            	    bits = 9;
                    		    break;
                    	    }
                    	    int new_normal_val = data->lfo_phase;
                    	    if( div > 1 ) new_normal_val /= div;
                    	    new_normal_val = ( new_normal_val >> ( bits - 8 ) ) & 255;
                    	    if( ctl->normal_value != new_normal_val )
                    	    {
                        	ctl->normal_value = new_normal_val;
                        	mod->draw_request++;
                    	    }
			}
#endif
		    } 
		    if( ptr >= frames ) break;
		}
		if( check_filter_output && finished )
		{
		    data->empty = true;
                    data->empty_frames_counter = 0;
		}
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    switch( event->controller.ctl_num )
	    {
		case 15:
		    switch( data->ctl_lfo_freq_units )
    		    {
			case 0:
			    data->lfo_phase = ( event->controller.ctl_val / 4 ) & 8191;
			    break;
			case 1:
			case 2:
			    data->lfo_phase = ( ( event->controller.ctl_val / 32 ) * ( 1000 * 512 ) ) / 1024;
			    break;
			case 3:
			    data->lfo_phase = ( ( ( event->controller.ctl_val / 32 ) * data->host_tick_size ) / 1024 ) * 512;
			    break;
			case 4:
			case 5:
			case 6:
			    data->lfo_phase = ( ( ( event->controller.ctl_val / 32 ) * ( ( data->host_tick_size * data->ticks_per_line ) / 8 ) ) / 1024 ) * 512;
			    break;
    		    }
    		    break;
	    }
	    break;
	case PS_CMD_APPLY_CONTROLLERS:
	    filter2_set_floating_values( data );
	    data->first_reinit_without_interp = true;
	    retval = 1;
	    break;
        case PS_CMD_NOTE_ON:
    	    SET_PHASE( 0 );
            retval = 1;
            break;
	case PS_CMD_SPEED_CHANGED:
            data->host_tick_size = event->speed.tick_size;
            data->ticks_per_line = event->speed.ticks_per_line;
            retval = 1;
            break;
	case PS_CMD_CLOSE:
	    biquad_filter_remove( data->f );
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
