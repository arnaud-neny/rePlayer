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
#define MODULE_DATA	psynth_lfo_data
#define MODULE_HANDLER	psynth_lfo
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_DUTY_CYCLE 	256
#define FREQ_UNIT_MAX  	8
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_type;
    PS_CTYPE   	ctl_amp;
    PS_CTYPE   	ctl_freq;
    PS_CTYPE   	ctl_shape;
    PS_CTYPE   	ctl_set_phase;
    PS_CTYPE   	ctl_mono;
    PS_CTYPE   	ctl_freq_units;
    PS_CTYPE    ctl_duty_cycle;
    PS_CTYPE	ctl_gen;
    PS_CTYPE    ctl_freq_scale;
    PS_CTYPE	ctl_smooth;
    PS_CTYPE	ctl_sin_quality;
    PS_CTYPE	ctl_transpose;
    PS_CTYPE	ctl_finetune;
    uint16_t*  	sin_tab;
    uint64_t    ptr;
    uint64_t	delta;
    bool	recalc_delta;
    uint	rand; 
    uint	rand_prev; 
    uint	rand_prev_phase;
    uint 	rand_seed;
    int     	tick_size; 
    uint8_t   	ticks_per_line;
};
static void lfo_change_ctl_limits( int mod_num, psynth_net* pnet )
{
    if( pnet->base_host_version < 0x01090202 ) return;
    psynth_module* mod = 0;
    MODULE_DATA* data = 0;
    if( mod_num >= 0 )
    {
        mod = &pnet->mods[ mod_num ];
        data = (MODULE_DATA*)mod->data_ptr;
    }
    else
    {
        return;
    }
    int min = 1;
    int max = 256;
    switch( data->ctl_freq_units )
    {
        case 1:
            max = 4000;
            break;
        case 2:
            max = 8192 * 2;
            break;
        case 7:
            min = 0;
            max = 256;
            break;
        case 8:
            min = 0;
            max = 256*100;
            break;
    }
    int cnum = 3;
    if( mod->ctls[ cnum ].max != max || mod->ctls[ cnum ].min != min )
    {
        psynth_change_ctl_limits(
            mod_num,
            cnum,
            min,
            max,
            max,
            pnet );
        mod->full_redraw_request++;
    }
}
#define SET_PHASE( CHECK_VERSION ) \
{ \
    if( ( CHECK_VERSION && pnet->base_host_version >= 0x01090300 ) || CHECK_VERSION == 0 ) \
    { \
	psynth_event e; \
        SMEM_CLEAR_STRUCT( e ); \
	e.command = PS_CMD_SET_GLOBAL_CONTROLLER; \
        e.controller.ctl_num = 5; \
	e.controller.ctl_val = data->ctl_set_phase << 7; \
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
	    retval = (PS_RETTYPE)"LFO";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Амплитудная модуляция встроенным генератором низкой частоты.\n"
                    	    STR_NOTE_RESET_LFO_TO_RU " \"Установ.фазу\".";
                        break;
                    }
		    retval = (PS_RETTYPE)"LFO-based amplitude modulator. LFO = Low Frequency Oscillation.\n"
			STR_NOTE_RESET_LFO_TO " \"Set phase\" value.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#E47FFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 15, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TYPE ), ps_get_string( STR_PS_LFO_TYPES ), 0, 1, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_AMPLITUDE ), "", 0, 256, 256, 0, &data->ctl_amp, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ ), "", 1, 2048, 256, 0, &data->ctl_freq, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WAVEFORM_TYPE ), ps_get_string( STR_PS_LFO_WAVEFORM_TYPES ), 0, 7, 2, 1, &data->ctl_shape, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SET_PHASE ), "", 0, 256, 0, 0, &data->ctl_set_phase, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_UNIT ), ps_get_string( STR_PS_LFO_FREQ_UNITS ), 0, FREQ_UNIT_MAX, 0, 1, &data->ctl_freq_units, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DUTY_CYCLE ), "", 0, MAX_DUTY_CYCLE, MAX_DUTY_CYCLE / 2, 0, &data->ctl_duty_cycle, MAX_DUTY_CYCLE / 2, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_GENERATOR ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_gen, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_SCALE ), "%", 0, 200, 100, 0, &data->ctl_freq_scale, 100, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SMOOTH_TRANSITIONS ), ps_get_string( STR_PS_LFO_SMOOTH_TRANSITIONS ), 0, 1, 1, 1, &data->ctl_smooth, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SIN_QUALITY ), ps_get_string( STR_PS_SIN_Q_MODES ), 0, 3, 0, 1, &data->ctl_sin_quality, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TRANSPOSE ), ps_get_string( STR_PS_SEMITONE ), 0, 256, 128, 1, &data->ctl_transpose, 128, 1, pnet );
    	    psynth_set_ctl_show_offset( mod_num, 13, -128, pnet );
    	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FINETUNE ), ps_get_string( STR_PS_SEMITONE256 ), 0, 512, 256, 0, &data->ctl_finetune, 256, 1, pnet );
    	    psynth_set_ctl_show_offset( mod_num, 14, -256, pnet );
	    data->ptr = 0;
	    data->rand = 0;
	    data->rand_prev = 0;
            data->rand_prev_phase = 0;
            data->rand_seed = (uint32_t)stime_ns() + pseudo_random() + mod_num * 49157 + mod->id * 3079;
	    data->sin_tab = (uint16_t*)psynth_get_sine_table( 2, false, 9, 65535 );
	    data->recalc_delta = true;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
            lfo_change_ctl_limits( mod_num, pnet );
	    SET_PHASE( 1 );
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    data->ptr = 0;
	    SET_PHASE( 1 );
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    while( 1 )
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
	        if( data->ctl_mono )
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
		if( data->recalc_delta )
		{
		    data->recalc_delta = false;
		    switch( data->ctl_freq_units )
		    {
			case 0:
			    data->delta = (uint64_t)( (uint64_t)data->ctl_freq << 36 ) / pnet->sampling_freq;
			    break;
			case 1:
			    data->delta = (uint64_t)( ( (uint64_t)64 << 36 ) * 1000 ) / ( data->ctl_freq * pnet->sampling_freq );
			    break;
			case 2:
			    data->delta = (uint64_t)( ( (uint64_t)data->ctl_freq << 36 ) * 64 ) / pnet->sampling_freq;
			    break;
			case 3:
			    data->delta = (uint64_t)( ( (uint64_t)64 << 36 ) * 256 ) / ( (unsigned)data->ctl_freq * data->tick_size );
			    break;
			case 4:
			case 5:
			case 6:
			    data->delta = (uint64_t)( ( (uint64_t)64 << 36 ) * 256 ) / ( (uint64_t)data->ctl_freq * data->tick_size * data->ticks_per_line / ( data->ctl_freq_units - 3 ) );
			    break;
			case 7:
			    {
				float note = data->ctl_freq;
				float freq = powf( 2.0f, note / 12.0f ) * 522.6875f / 32.0f;
				if( freq > pnet->sampling_freq / 2 )
				    data->delta = (uint64_t)1 << 41;
				else
				{
				    freq *= (uint64_t)1<<42;
				    data->delta = (uint64_t)( freq ) / pnet->sampling_freq;
				}
			    }
			    break;
			case 8:
			    {
				float note = data->ctl_freq;
				float freq = powf( 2.0f, note / (100.0f*12.0f) ) * 522.6875f / 32.0f;
				if( freq > pnet->sampling_freq / 2 )
				    data->delta = (uint64_t)1 << 41;
				else
				{
				    freq *= (uint64_t)1<<42;
				    data->delta = (uint64_t)( freq ) / pnet->sampling_freq;
				}
			    }
			    break;
		    }
		    int scale = data->ctl_freq_scale;
		    if( scale != 100 ) data->delta = data->delta * scale / 100;
		    int transpose = data->ctl_transpose - 128;
		    int finetune = data->ctl_finetune - 256;
		    if( transpose || finetune )
		    {
			double n = transpose + finetune / 256.0;
			double s = pow( 2.0, n / 12.0 );
			data->delta = data->delta * s;
		    }
		    if( data->delta < 1 ) data->delta = 1;
		    if( !( data->ctl_shape == 5 || data->ctl_shape == 7 ) )
		    {
			if( data->delta > ( (uint64_t)256 << (17+16) ) ) data->delta = (uint64_t)256 << (17+16);
		    }
		}
		bool no_input_signal = true;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			no_input_signal = false;
			break;
		    }
		}
		if( data->ctl_gen ) no_input_signal = false;
		if( data->ctl_volume == 0 ) no_input_signal = true;
		if( no_input_signal ) 
		{
		    data->ptr += data->delta * frames;
		    break;
		}
		int ctl_amp = data->ctl_amp;
		int ctl_volume = data->ctl_volume;
		uint64_t ptr = 0;
		uint rand = 0;
		uint rand_prev = 0;
        	uint rand_prev_phase = 0;
        	uint rand_seed = 0;
		PS_STYPE_F sin_coef = (PS_STYPE_F)0.5 * ctl_amp / (PS_STYPE_F)256.0;
		int sin_quality = data->ctl_sin_quality;
		if( sin_quality == 0 )
		{
#if defined(PS_STYPE_FLOATINGPOINT) && ( CPUMARK >= 10 )
		    sin_quality = 3; 
#else
		    sin_quality = 1; 
#endif
		}
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* RESTRICT in = inputs[ ch ] + offset;
		    PS_STYPE* RESTRICT out = outputs[ ch ] + offset;
		    uint off = 0;
		    bool apply_out_volume = false;
		    bool apply_lfo_amp = false;
		    ptr = data->ptr;
		    if( data->ctl_shape == 5 || data->ctl_shape == 7 )
		    {
			rand = data->rand;
			rand_prev = data->rand_prev;
			rand_prev_phase = data->rand_prev_phase;
			rand_seed = data->rand_seed;
		    }
		    if( data->ctl_type == 1 )
		    {
			if( ch == 1 )
			    off = 128;
		    }
		    switch( data->ctl_shape )
		    {
			case 0:
			case 2:
			    {
				if( data->ctl_shape == 2 ) off *= 2;
				uint64_t add = ( (uint64_t)off << (17+16) ) - ( (uint64_t)1 << (24+16) ); 
				switch( sin_quality )
				{
				    case 1: 
				    add += (uint64_t)1 << (25+16); 
				    ptr += add;
				    for( int i = 0; i < frames; i++ )
				    {
					int amp = -(int)data->sin_tab[ ( ptr >> (17+16) ) & 511 ]; 
					amp *= ctl_amp;
					amp += 65536 * 256;
					amp /= 256;
					PS_STYPE2 out_val = PS_STYPE_ONE * amp;
					out_val /= (PS_STYPE2)65536;
					out[ i ] = out_val;
					ptr += data->delta;
				    }
				    ptr -= add;
				    break;
				    case 2: 
				    add += (uint64_t)1 << (25+16); 
				    ptr += add;
				    for( int i = 0; i < frames; i++ )
				    {
					int amp1 = -(int)data->sin_tab[ ( ptr >> (17+16) ) & 511 ]; 
					int amp2 = -(int)data->sin_tab[ ( ( ptr >> (17+16) ) + 1 ) & 511 ]; 
					int c = ( ptr >> (17+16-15) ) & 32767;
					int amp = amp1 * (32768-c) + amp2 * c; 
					amp /= 32768; 
					amp *= ctl_amp;
					amp += 65536 * 256;
					amp /= 256;
					PS_STYPE2 out_val = PS_STYPE_ONE * amp;
					out_val /= (PS_STYPE2)65536;
					out[ i ] = out_val;
					ptr += data->delta;
				    }
				    ptr -= add;
				    break;
				    case 3: 
				    ptr += add;
				    for( int i = 0; i < frames; i++ )
				    {
					PS_STYPE_F amp = PS_STYPE_SIN( ( (PS_STYPE_F)( (ptr>>16) & ( ( 1 << 26 ) - 1 ) ) / (PS_STYPE_F)( 1 << 26 ) ) * (PS_STYPE_F)( M_PI * 2 ) ) - (PS_STYPE_F)1;
					out[ i ] = ( amp * sin_coef + (PS_STYPE_F)1 ) * PS_STYPE_ONE;
					ptr += data->delta;
				    }
				    ptr -= add;
				    break;
				}
				apply_out_volume = true;
			    }
			    break;
			case 1:
			    {
				int v1 = ctl_volume;
				int v2 = ( ( 256 - ctl_amp ) * ctl_volume ) / 256;
				uint len = data->ctl_duty_cycle;
				if( off ) 
				{
				    if( pnet->base_host_version >= 0x02010100 )
				    {
					int tmp = v1;
					v1 = v2;
					v2 = tmp;
					off = 0;
				    }
				}
				if( len < 1 ) len = 1;
				if( len > MAX_DUTY_CYCLE - 2 ) len = MAX_DUTY_CYCLE - 2;
				if( data->ctl_smooth == 0 )
				    for( int i = 0; i < frames; i++ )
				    {
					int amp;
					uint p = ( ( ptr >> (18+16) ) + off ) & 255;
					if( p >= len )
					    amp = v1 << 6; 
					else
					    amp = v2 << 6;
					out[ i ] = ( PS_STYPE_ONE * amp ) / ( 256 << 6 );
					ptr += data->delta;
				    }
				else
				    for( int i = 0; i < frames; i++ )
				    {
					int amp;
					uint p = ( ( ptr >> (18+16) ) + off ) & 255;
					if( p >= len )
					{
					    if( p >= len + 1 )
						amp = v1 << 6; 
					    else
					    {
						uint c = ( ( ptr >> (3+16) ) + ( off << 15 ) ) & 32767;
						amp = ( ( v1 * c ) + ( v2 * (32768-c) ) ) >> 9;
					    }
					}
					else
					{
					    if( p >= 1 ) 
						amp = v2 << 6;
					    else
					    {
						uint c = ( ( ptr >> (3+16) ) + ( off << 15 ) ) & 32767;
						amp = ( ( v2 * c ) + ( v1 * (32768-c) ) ) >> 9;
					    }
					}
					out[ i ] = ( PS_STYPE_ONE * amp ) / ( 256 << 6 );
					ptr += data->delta;
				    }
			    }
			    break;
			case 3:
			case 4:
			    {
				int shape = data->ctl_shape;
				if( off ) 
				{
				    if( pnet->base_host_version >= 0x02010100 )
				    {
					if( shape == 3 )
					    shape = 4;
					else
					    shape = 3;
					off = 0;
				    }
				}
#ifdef PS_STYPE_FLOATINGPOINT
				off <<= 18; 
				for( int i = 0; i < frames; i++ )
				{
				    int p = ( ( ptr >> 16 ) + off ) & ((1<<26)-1);
				    int amp;
				    if( shape == 3 )
				        amp = - ( ((1<<26)-1) - p );
				    else
				        amp = -p;
				    if( data->ctl_smooth && p < (128<<11) )
				    {
				        if( shape == 3 )
				    	    amp = ( (int64_t)amp * p ) / (128<<11);
					else
					    amp = ( (int64_t)amp * p + ( -((1<<26)-1) * (int64_t)( (128<<11) - p ) ) ) / (128<<11);
				    }
				    out[ i ] = amp * (PS_STYPE2)ctl_amp / (PS_STYPE2)256 / (PS_STYPE2)(1<<26) + (PS_STYPE2)1;
				    ptr += data->delta;
				}
#else
				off *= 128;
				for( int i = 0; i < frames; i++ )
				{
				    int p = ( ( ptr >> 27 ) + off ) & 32767;
				    int amp;
				    if( shape == 3 )
				        amp = - ( 32767 - p );
				    else
				        amp = -p;
				    if( data->ctl_smooth && p < 128 )
				    {
				        if( shape == 3 )
				    	    amp = ( amp * p ) / 128;
					else
					    amp = ( amp * p + ( -32767 * ( 128 - p ) ) ) / 128;
				    }
				    amp = amp * ctl_amp / 256 + 32768;
				    out[ i ] = PS_STYPE_ONE * amp / 32768;
				    ptr += data->delta;
				}
#endif
				apply_out_volume = true;
			    }
			    break;
			case 5:
			    {
				if( data->ctl_smooth == 0 )
				    for( int i = 0; i < frames; i++ )
				    {
					uint p1 = ( ptr >> (11+16) ) & 32767;
					uint p2 = ( ptr >> (11+16) ) >> 15;
					if( rand_prev_phase != p2 )
                			{
                    			    rand_prev_phase = p2;
                    			    rand_prev = rand;
		                    	    rand = pseudo_random( &rand_seed );
                			}
                			int amp = rand;
					if( off ) amp = 32767 - amp;
					PS_STYPE2 out_val = PS_STYPE_ONE;
					out_val *= amp;
					out_val /= 32768;
					out[ i ] = out_val;
					ptr += data->delta;
				    }
				else
				    for( int i = 0; i < frames; i++ )
				    {
					uint p1 = ( ptr >> (11+16) ) & 32767;
					uint p2 = ( ptr >> (11+16) ) >> 15;
					if( rand_prev_phase != p2 )
                			{
                    			    rand_prev_phase = p2;
                    			    rand_prev = rand;
		                    	    rand = pseudo_random( &rand_seed );
                			}
                			int amp;
					if( p1 < 128 )
					    amp = ( ( rand * p1 ) + ( rand_prev * ( 127 - p1 ) ) ) >> 7;
					else
					    amp = rand;
					if( off )
					    amp = 32767 - amp;
					PS_STYPE2 out_val = PS_STYPE_ONE;
					out_val *= amp;
					out_val /= 32768;
					out[ i ] = out_val;
					ptr += data->delta;
				    }
				apply_out_volume = true;
				apply_lfo_amp = true;
			    }
			    break;
			case 7:
			    {
				for( int i = 0; i < frames; i++ )
				{
				    uint p2 = ptr >> 42;
				    if( rand_prev_phase != p2 )
                		    {
                    			rand_prev_phase = p2;
                    			rand_prev = rand;
		                        rand = pseudo_random( &rand_seed ); 
                		    }
#ifdef PS_STYPE_FLOATINGPOINT
				    uint p1 = ( ptr >> 16 ) & ((1<<26)-1);
                		    int amp = ( ( (int64_t)rand * p1 ) + ( (int64_t)rand_prev * ( (1<<26) - p1 ) ) ) >> 15; 
				    if( off ) amp = (32767<<11) - amp;
				    out[ i ] = amp / (PS_STYPE2)(1<<26);
#else
				    uint p1 = ( ptr >> 27 ) & 32767;
                		    int amp = ( ( rand * p1 ) + ( rand_prev * ( 32768 - p1 ) ) ) >> 15; 
				    if( off ) amp = 32767 - amp;
				    out[ i ] = PS_STYPE_ONE * amp / 32768;
#endif
				    ptr += data->delta;
				}
				apply_out_volume = true;
				apply_lfo_amp = true;
			    }
			    break;
			case 6:
			    {
#ifdef PS_STYPE_FLOATINGPOINT
				off <<= 19; 
				for( int i = 0; i < frames; i++ )
				{
				    int v = ( ( ptr >> 15 ) + off ) & ((1<<27)-1);
				    if( v > (1<<26) ) v = (1<<27) - v;
				    PS_STYPE2 v2 = (PS_STYPE2)-v / (PS_STYPE2)(1<<26);
				    out[ i ] = v2 * (PS_STYPE2)ctl_amp / (PS_STYPE2)256 + (PS_STYPE2)1;
				    ptr += data->delta;
				}
#else
				off <<= 8; 
                                for( int i = 0; i < frames; i++ )
                                {
                                    int v = ( ( ptr >> 26 ) + off ) & 65535;
                                    if( v > 32768 ) v = 65536 - v;
                                    v = -v;
                                    v = v * ctl_amp / 256;
                                    v += 32768;
                                    out[ i ] = PS_STYPE_ONE * (PS_STYPE2)v / 32768;
                                    ptr += data->delta;
                                }
#endif
				apply_out_volume = true;
			    }
			    break;
		    }
		    if( apply_lfo_amp )
		    {
			if( ctl_amp != 256 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 out_val = out[ i ] * ctl_amp;
				out_val += PS_STYPE_ONE * ( 256 - ctl_amp );
				out_val /= (PS_STYPE2)256;
				out[ i ] = out_val;
			    }
			}
		    }
		    if( data->ctl_gen == 0 )
		    {
			for( int i = 0; i < frames; i++ )
			{
#ifdef PS_STYPE_FLOATINGPOINT
			    out[ i ] *= in[ i ];
#else
			    PS_STYPE2 out_val = out[ i ];
			    out_val *= in[ i ];
			    out_val >>= PS_STYPE_BITS;
			    out[ i ] = out_val;
#endif
			}
		    }
		    if( apply_out_volume )
		    {
			if( ctl_volume != 256 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 out_val = out[ i ] * ctl_volume;
				out_val /= (PS_STYPE2)256;
				out[ i ] = out_val;
			    }
			}
		    }
		}
		data->ptr = ptr;
		if( data->ctl_shape == 5 || data->ctl_shape == 7 )
		{
		    data->rand = rand;
		    data->rand_prev = rand_prev;
		    data->rand_prev_phase = rand_prev_phase;
		    data->rand_seed = rand_seed;
		}
		retval = 1;
		break;
	    }
#ifdef SUNVOX_GUI
	    {
	        psynth_ctl* ctl = &mod->ctls[ 5 ];
	        int new_normal_val = ( data->ptr >> (18+16) ) & 255;
	        if( ctl->normal_value != new_normal_val )
	        {
	    	    ctl->normal_value = new_normal_val;
		    mod->draw_request++;
		}
    	    }
#endif
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    switch( event->controller.ctl_num )
	    {
		case 3: 
		case 4: 
		case 10: 
		case 13: 
		case 14: 
            	    data->recalc_delta = true;
		    break;
		case 5: 
		    data->ptr = (uint64_t)( event->controller.ctl_val >> 6 ) << (17+16);
		    break;
		case 7:
            	    data->ctl_freq_units = event->controller.ctl_val;
            	    if( data->ctl_freq_units > FREQ_UNIT_MAX ) data->ctl_freq_units = FREQ_UNIT_MAX;
            	    lfo_change_ctl_limits( mod_num, pnet );
            	    data->recalc_delta = true;
            	    retval = 1;
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
            data->recalc_delta = true;
            retval = 1;
            break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
