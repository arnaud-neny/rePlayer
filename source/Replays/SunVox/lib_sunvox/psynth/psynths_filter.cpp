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
#define MODULE_DATA	psynth_filter_data
#define MODULE_HANDLER	psynth_filter
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define FILTERS		4
struct filter_channel
{
    PS_STYPE2	d1, d2, d3;
};
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   		ctl_volume;
    PS_CTYPE   		ctl_cutoff_freq;
    PS_CTYPE   		ctl_resonance;
    PS_CTYPE   		ctl_type;
    PS_CTYPE   		ctl_response;
    PS_CTYPE   		ctl_mode;
    PS_CTYPE   		ctl_impulse;
    PS_CTYPE   		ctl_mix;
    PS_CTYPE   		ctl_lfo_freq;
    PS_CTYPE   		ctl_lfo_amp;
    PS_CTYPE   		ctl_set_lfo_phase;
    PS_CTYPE   		ctl_exp;
    PS_CTYPE   		ctl_rolloff;
    PS_CTYPE   		ctl_lfo_freq_units;
    PS_CTYPE   		ctl_lfo_type;
    filter_channel	fchan[ MODULE_OUTPUTS * FILTERS ];
    int	   		tick_counter;   
    int     		lfo_phase;
    int                 lfo_rand;
    int                 lfo_rand_prev;
    uint8_t*  		hsin;
    PS_CTYPE  	 	floating_volume;
    PS_CTYPE   		floating_cutoff;
    PS_CTYPE  	 	floating_resonance;
    PS_CTYPE   		impulse;
    int	   	 	empty_frames_counter;
    int	   	 	empty_frames_counter_max;
    int     		tick_size; 
    uint8_t   		ticks_per_line;
};
#define FILTER_OUTPUT_CONTROL \
    if( outp >= 1 << bits ) \
	outp = ( 1 << bits ) - 1; \
    else if( outp <= ( -1 << bits ) ) \
	outp = ( -1 << bits ) + 1;
static void filter_set_floating_values( MODULE_DATA* data )
{
    data->floating_volume = data->ctl_volume * 256;
    if( data->ctl_exp )
	data->floating_cutoff = ( ( data->ctl_cutoff_freq * data->ctl_cutoff_freq ) / 14000 ) * 256;
    else
	data->floating_cutoff = data->ctl_cutoff_freq * 256;
    data->floating_resonance = data->ctl_resonance * 256;
}
#define SET_PHASE( CHECK_VERSION ) \
{ \
    if( ( CHECK_VERSION && pnet->base_host_version >= 0x01090300 ) || CHECK_VERSION == 0 ) \
    { \
        psynth_event e; \
        smem_clear_struct( e ); \
        e.command = PS_CMD_SET_GLOBAL_CONTROLLER; \
        e.controller.ctl_num = 10; \
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
	    retval = (PS_RETTYPE)"Filter";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Рекурсивный фильтр для выделения или подавления определённых частот.\nВ режиме HQ фильтр работает на полную мощность (double-sampled). В режиме LQ фильтр работает в два раза быстрее, но с потерей качества.\n"
                    	    STR_NOTE_RESET_LFO_TO_RU " \"Установ.фазу LFO\".";
                        break;
                    }
		    retval = (PS_RETTYPE)"IIR Filter - removes some unwanted frequency ranges.\nUse lower \"response\" values for smooth change of the Filter parameters.\n"
			STR_NOTE_RESET_LFO_TO " \"Set LFO phase\" value.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FD9FF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 15, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 256, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ ), ps_get_string( STR_PS_HZ ), 0, 14000, 14000, 0, &data->ctl_cutoff_freq, -1, 0, pnet );
	    psynth_set_ctl_flags( mod_num, 1, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESONANCE ), "", 0, 1530, 0, 0, &data->ctl_resonance, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TYPE ), ps_get_string( STR_PS_FILTER_TYPES ), 0, 3, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESPONSE ), "", 0, 256, 8, 0, &data->ctl_response, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODE_LQ_MONO, MODE_HQ, 1, &data->ctl_mode, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IMPULSE ), ps_get_string( STR_PS_HZ ), 0, 14000, 0, 0, &data->ctl_impulse, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MIX ), "", 0, 256, 256, 0, &data->ctl_mix, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ ), "", 0, 1024, 8, 0, &data->ctl_lfo_freq, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_AMP ), "", 0, 256, 0, 0, &data->ctl_lfo_amp, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SET_LFO_PHASE ), "", 0, 256, 0, 0, &data->ctl_set_lfo_phase, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_EXP_FREQ ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_exp, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ROLLOFF ), ps_get_string( STR_PS_FILTER_ROLLOFF_VALS ), 0, 3, 0, 1, &data->ctl_rolloff, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ_UNIT ), ps_get_string( STR_PS_FILTER_LFO_FREQ_UNITS ), 0, 6, 0, 1, &data->ctl_lfo_freq_units, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_WAVEFORM ), ps_get_string( STR_PS_FILTER_LFO_WAVEFORM_TYPES ), 0, 4, 0, 1, &data->ctl_lfo_type, -1, 2, pnet );
	    filter_set_floating_values( data );
	    for( int i = 0; i < MODULE_OUTPUTS * FILTERS; i++ )
	    {
		data->fchan[ i ].d1 = 0;
		data->fchan[ i ].d2 = 0;
		data->fchan[ i ].d3 = 0;
	    }
	    data->tick_counter = 0;
	    data->lfo_phase = 0;
	    data->lfo_rand = 0;
            data->lfo_rand_prev = 0;
	    data->hsin = g_hsin_tab;
	    data->impulse = 0;
	    data->empty_frames_counter_max = pnet->sampling_freq * 2;
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    SET_PHASE( 1 );
	case PS_CMD_APPLY_CONTROLLERS:
	    filter_set_floating_values( data );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int i = 0; i < MODULE_OUTPUTS * FILTERS; i++ )
	    {
		data->fchan[ i ].d1 = 0;
		data->fchan[ i ].d2 = 0;
		data->fchan[ i ].d3 = 0;
	    }
	    data->tick_counter = 0;
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    SET_PHASE( 1 );
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
		int ch;
		bool input_signal = 0;
		for( ch = 0; ch < outputs_num; ch++ )
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
		    if( data->empty_frames_counter < data->empty_frames_counter_max )
		    {
		        data->empty_frames_counter += frames;
			input_signal = 1;
		    } 
		}
		int fs;
		int rolloff_steps;
		int vol;
		PS_STYPE2 fvol;
		int tick_size;
		switch( data->ctl_lfo_freq_units )
		{
		    case 0:
			tick_size = ( pnet->sampling_freq / 200 ) * 256; 
			break;
		    case 1:
		    case 2:
			tick_size = ( pnet->sampling_freq * 256 ) / 1000; 
			break;
		    case 3:
		    case 4:
		    case 5:
		    case 6:
			tick_size = ( pnet->sampling_freq * 256 ) / 200; 
			break;
		}
		int ptr = 0;
		if( data->impulse )
		{
		    if( data->ctl_exp )
			data->floating_cutoff = ( ( data->impulse * data->impulse ) / 14000 ) * 256;
		    else
			data->floating_cutoff = data->impulse * 256;
		    data->impulse = 0;
		}
		while( 1 )
		{
		    int buf_size = frames - ptr;
    		    if( buf_size > ( tick_size - data->tick_counter ) / 256 ) buf_size = ( tick_size - data->tick_counter ) / 256;
    		    if( ( tick_size - data->tick_counter ) & 255 ) buf_size++; 
    		    if( buf_size > frames - ptr ) buf_size = frames - ptr;
    		    if( buf_size < 0 ) buf_size = 0;
		    if( input_signal == 0 ) goto no_input_signal;
		    fs = pnet->sampling_freq;
		    rolloff_steps = data->ctl_rolloff + 1;
		    vol = data->floating_volume / 256; 
		    if( pnet->base_host_version >= 0x01080000 )
		    {
			fvol = PS_NORM_STYPE( (PS_STYPE2)( vol * data->ctl_mix ) / (PS_STYPE2)256, 256 );
		    }
		    else
		    {
			fvol = PS_NORM_STYPE( (PS_STYPE2)( (int)( vol * data->ctl_mix ) / (int)256 ), 256 );
		    }
		    for( ch = 0; ch < outputs_num; ch++ )
		    {
			PS_STYPE* in = inputs[ ch ] + offset;
			PS_STYPE* out = outputs[ ch ] + offset;
			int c, f, q;
			int floating_resonance;
			PS_STYPE2 fvol2 = fvol;
			for( int fnum = 0; fnum < rolloff_steps; fnum++ )
			{
			if( pnet->base_host_version >= 0x02000000 )
			{
			    if( fnum < rolloff_steps - 1 )
				fvol2 = PS_NORM_STYPE( 256, 256 );
			    else
				fvol2 = fvol;
			}
			PS_STYPE* fin = in;
			if( fnum > 0 ) fin = out;
			floating_resonance = data->floating_resonance / 256;
			if( fnum < rolloff_steps - 1 ) floating_resonance = 0;
			if( data->ctl_mode == MODE_HQ || data->ctl_mode == MODE_HQ_MONO )
			{
			    c = ( ( data->floating_cutoff / 256 ) * 32768 ) / ( fs * 2 );
			    f = ( 6433 * c ) / 32768;
			    if( f >= 1024 ) f = 1023;
			    q = 1536 - floating_resonance;
			}
			if( data->ctl_mode == MODE_LQ || data->ctl_mode == MODE_LQ_MONO )
			{
			    c = ( ( ( data->floating_cutoff / 256 ) / 2 ) * 32768 ) / fs; 
			    f = ( 6433 * c ) / 32768;
			    if( f >= 1024 ) f = 1023;
			    q = 1536 - floating_resonance;
			}
			filter_channel* fchan = &data->fchan[ MODULE_OUTPUTS * fnum + ch ];
			if( data->ctl_mix > 0 )
			{
			    PS_STYPE2 d1 = fchan->d1;
			    PS_STYPE2 d2 = fchan->d2;
			    PS_STYPE2 d3 = fchan->d3;
			    int bits = ( sizeof( PS_STYPE ) * 8 ) - 1;
			    bool dont_render = false;
			    if( f == 0 )
			    {
				if( data->ctl_type == 0 )
				{
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = d2 / 32;
					FILTER_OUTPUT_CONTROL;
#else
					outp = d2;
					psynth_denorm_add_white_noise( outp );
#endif
					d2 = ( d2 * (PS_STYPE2)255 ) / (PS_STYPE2)256;
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					out[ i ] = outp;
				    }
				    dont_render = true;
				}
				if( data->ctl_type == 2 )
				{
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = d1 / 32;
					FILTER_OUTPUT_CONTROL;
#else
					outp = d1;
					psynth_denorm_add_white_noise( outp );
#endif
					d1 = ( d1 * (PS_STYPE2)255 ) / (PS_STYPE2)256;
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					out[ i ] = outp;
				    }
				    dont_render = true;
				}
			    }
			    if( dont_render == false )
			    {
			    if( data->ctl_mode == MODE_HQ || data->ctl_mode == MODE_HQ_MONO )
			    switch( data->ctl_type )
			    {
				case 0:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 slow = low;
					low = low + ( f * band ) / (PS_STYPE2)1024;
					high = inp - low - ( q * band ) / (PS_STYPE2)1024;
					band = ( f * high ) / (PS_STYPE2)1024 + band;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = ( ( d2 + slow ) / 2 ) / 32;
#else
					outp = ( d2 + slow ) / (PS_STYPE2)2;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 1:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 shigh = high;
					low = low + ( f * band ) / (PS_STYPE2)1024;
					high = inp - low - ( q * band ) / (PS_STYPE2)1024;
					band = ( f * high ) / (PS_STYPE2)1024 + band;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = ( ( d3 + shigh ) / 2 ) / 32;
#else
					outp = ( d3 + shigh ) / (PS_STYPE2)2;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					d3 = high;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 2:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 sband = band;
					low = low + ( f * band ) / (PS_STYPE2)1024;
					high = inp - low - ( q * band ) / (PS_STYPE2)1024;
					band = ( f * high ) / (PS_STYPE2)1024 + band;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = ( ( d1 + sband ) / 2 ) / 32;
#else
					outp = ( d1 + sband ) / (PS_STYPE2)2;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 3:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 snotch = high + low;
					low = low + ( f * band ) / (PS_STYPE2)1024;
					high = inp - low - ( q * band ) / (PS_STYPE2)1024;
					band = ( f * high ) / (PS_STYPE2)1024 + band;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = ( ( d3 + snotch ) / 2 ) / 32;
#else
					outp = ( d3 + snotch ) / (PS_STYPE2)2;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					d3 = high + low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
			    }
			    if( data->ctl_mode == MODE_LQ || data->ctl_mode == MODE_LQ_MONO )
			    switch( data->ctl_type )
			    {
				case 0:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = low / 32;
#else
					outp = low;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 1:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = high / 32;
#else
					outp = high;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 2:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = band / 32;
#else
					outp = band;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
				case 3:
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = fin[ i ];
					psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
					inp *= 32;
#endif
					PS_STYPE2 low = d2 + ( f * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 high = inp - low - ( q * d1 ) / (PS_STYPE2)1024;
					PS_STYPE2 band = ( f * high ) / (PS_STYPE2)1024 + d1;
					PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
					outp = ( high + low ) / 32;
#else
					outp = high + low;
#endif
					outp = PS_NORM_STYPE_MUL( outp, fvol2, 256 );
					d1 = band;
					d2 = low;
					out[ i ] = (PS_STYPE)outp;
				    }
				    break;
			    }
			    } 
			    fchan->d1 = d1;
			    fchan->d2 = d2;
			    fchan->d3 = d3;
			} 
			} 
			if( data->ctl_mix == 0 )
			{
			    if( vol == 256 )
			    {
				for( int i = ptr; i < ptr + buf_size; i++ )
				    out[ i ] = in[ i ];
			    }
			    else
			    {
				if( vol == 0 )
				{
				    for( int i = ptr; i < ptr + buf_size; i++ ) out[ i ] = 0;
				}
				else
				{
				    for( int i = ptr; i < ptr + buf_size; i++ )
				    {
					PS_STYPE2 inp = in[ i ];
					inp *= vol;
					inp /= 256;
					out[ i ] = (PS_STYPE)inp;
				    }
				}
			    }
			}
			if( data->ctl_mix > 0 && data->ctl_mix < 256 )
			{
			    PS_STYPE2 vol2 = (PS_STYPE2)( (int)data->floating_volume / (int)256 );
			    vol2 *= ( 256 - data->ctl_mix );
			    if( pnet->base_host_version >= 0x01080000 )
			    {
				vol2 = vol2 / (PS_STYPE2)256;
			    }
			    else
			    {
				vol2 = (PS_STYPE2)( (int)( (int)vol2 / (int)256 ) );
			    }
			    for( int i = ptr; i < ptr + buf_size; i++ )
			    {
				PS_STYPE2 inp = in[ i ];
				inp *= vol2;
				inp /= 256;
				inp += out[ i ];
				out[ i ] = (PS_STYPE)inp;
			    }
			}
		    } 
no_input_signal:
		    ptr += buf_size;
		    data->tick_counter += buf_size * 256;
		    if( data->tick_counter >= tick_size ) 
		    {
			int ctl_cutoff_freq = data->ctl_cutoff_freq;
			if( data->ctl_exp )
			{
			    ctl_cutoff_freq = ( ctl_cutoff_freq * ctl_cutoff_freq ) / 14000;
			}
			if( data->ctl_lfo_amp )
			{
			    int lfo_phase = 0;
			    switch( data->ctl_lfo_freq_units )
			    {
				case 0:
				    lfo_phase = ( data->lfo_phase / 16 ) & 511;
				    break;
				case 1:
				case 2:
				    lfo_phase = ( data->lfo_phase / 1000 ) & 511;
				    break;
				case 3:
				    lfo_phase = ( data->lfo_phase / data->tick_size ) & 511;
				    break;
				case 4:
				case 5:
				case 6:
				    lfo_phase = ( data->lfo_phase / ( ( data->tick_size * data->ticks_per_line ) / 8 ) ) & 511;
				    break;
			    }
			    int lfo_value = 0;
    			    switch( data->ctl_lfo_type )
		            {
		                case 0:
		                    if( lfo_phase < 256 )
		                        lfo_value = (int)data->hsin[ lfo_phase ];
		                    else
                			lfo_value = -(int)data->hsin[ lfo_phase & 255 ];
		                    break;
		                case 1:
		        	    lfo_value = lfo_phase - 255;
		                    break;
		                case 2:
		                    lfo_value = -lfo_phase + 255;
		                    break;
		                case 3:
		                    if( lfo_phase < 256 )
		                        lfo_value = 255;
            			    else
		                        lfo_value = -255;
            			    break;
		                case 4:
		                    {
		                        if( data->lfo_rand_prev != lfo_phase / 256 )
                			{
                    			    data->lfo_rand_prev = lfo_phase / 256;
		                            data->lfo_rand = pseudo_random() & 511;
		                        }
                			lfo_value = data->lfo_rand - 255;
		                    }
		                    break;
		            }
			    lfo_value *= 14000 / 2;
			    lfo_value /= 256;
			    lfo_value *= data->ctl_lfo_amp;
			    lfo_value /= 256;
			    int lfo_offset = 0;
			    int lfo_max = ( data->ctl_lfo_amp * ( 14000 / 2 ) ) / 256;
			    if( ctl_cutoff_freq - lfo_max < 0 )
				lfo_offset = -( ctl_cutoff_freq - lfo_max );
			    if( ctl_cutoff_freq + lfo_max > 14000 )
				lfo_offset = -( ( ctl_cutoff_freq + lfo_max ) - 14000 );
			    ctl_cutoff_freq += lfo_value + lfo_offset;
			}
			if( data->floating_cutoff / 256 > ctl_cutoff_freq )
			{
			    data->floating_cutoff -= data->ctl_response * 14000;
			    if( data->floating_cutoff / 256 < ctl_cutoff_freq )
				data->floating_cutoff = ctl_cutoff_freq * 256;
			}
			else
			if( data->floating_cutoff / 256 < ctl_cutoff_freq )
			{
			    data->floating_cutoff += data->ctl_response * 14000;
			    if( data->floating_cutoff / 256 > ctl_cutoff_freq )
				data->floating_cutoff = ctl_cutoff_freq * 256;
			}
			if( data->floating_resonance / 256 > data->ctl_resonance )
			{
			    data->floating_resonance -= data->ctl_response * 1530;
			    if( data->floating_resonance / 256 < data->ctl_resonance )
				data->floating_resonance = data->ctl_resonance * 256;
			}
			else
			if( data->floating_resonance / 256 < data->ctl_resonance )
			{
			    data->floating_resonance += data->ctl_response * 1530;
			    if( data->floating_resonance / 256 > data->ctl_resonance )
				data->floating_resonance = data->ctl_resonance * 256;
			}
			if( data->floating_volume / 256 > data->ctl_volume )
			{
			    data->floating_volume -= data->ctl_response * 256;
			    if( data->floating_volume / 256 < data->ctl_volume )
				data->floating_volume = data->ctl_volume * 256;
			}
			else
			if( data->floating_volume / 256 < data->ctl_volume )
			{
			    if( pnet->base_host_version == 0x01070000 )
				data->floating_volume += data->ctl_volume * 256;
			    else
				data->floating_volume += data->ctl_response * 256;
			    if( data->floating_volume / 256 > data->ctl_volume )
				data->floating_volume = data->ctl_volume * 256;
			}
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
				    data->lfo_phase %= ( data->tick_size * 512 );
				}
				break;
			    case 4:
			    case 5:
			    case 6:
				if( data->ctl_lfo_freq )
				{
				    data->lfo_phase += ( ( ( tick_size / 8 ) * 512 ) / data->ctl_lfo_freq ) * ( data->ctl_lfo_freq_units - 3 );
				    data->lfo_phase %= ( ( ( data->tick_size * data->ticks_per_line ) / 8 ) * 512 );
				}
				break;
			}
#ifdef SUNVOX_GUI
			{
            		    psynth_ctl* ctl = &mod->ctls[ 10 ];
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
				    div = data->tick_size;
				    bits = 9;
				    break;
				case 4:
				case 5:
				case 6:
				    div = data->tick_size * data->ticks_per_line / 8;
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
		retval = input_signal;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    switch( event->controller.ctl_num )
	    {
		case 10:
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
			    data->lfo_phase = ( ( ( event->controller.ctl_val / 32 ) * data->tick_size ) / 1024 ) * 512;
			    break;
			case 4:
			case 5:
			case 6:
			    data->lfo_phase = ( ( ( event->controller.ctl_val / 32 ) * ( ( data->tick_size * data->ticks_per_line ) / 8 ) ) / 1024 ) * 512;
			    break;
    		    }
    		    break;
		case 6:
		    data->impulse = (PS_CTYPE)( ( (int)event->controller.ctl_val * (int)mod->ctls[ 6 ].max ) / (int)32768 );
		    break;
	    }
	    break;
        case PS_CMD_NOTE_ON:
    	    SET_PHASE( 0 );
            retval = 1;
            break;
	case PS_CMD_SPEED_CHANGED:
            data->tick_size = event->speed.tick_size;
            data->ticks_per_line = event->speed.ticks_per_line;
            retval = 1;
            break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
