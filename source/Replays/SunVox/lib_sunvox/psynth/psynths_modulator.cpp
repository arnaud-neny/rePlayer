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
#define MODULE_DATA	psynth_modulator_data
#define MODULE_HANDLER	psynth_modulator
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define CHANGED_MAX_DELAY	( 1 << 0 )
#ifndef PS_STYPE_FLOATINGPOINT
    #define INTERP( v1, v2, p )		( ( v1 * ( PS_STYPE_ONE - p ) + v2 * p ) / PS_STYPE_ONE )
    #define INTERP2( v1, v2, p )	( ( v1 * ( PS_STYPE_ONE*2 - p ) + v2 * p ) / (PS_STYPE_ONE*2) )
    #define INTERP_SPLINE( v1, v2, v3, v4, p )	catmull_rom_spline_interp_int16( v1, v2, v3, v4, p << ( 15 - PS_STYPE_BITS ) )
    #define INTERP2_SPLINE( v1, v2, v3, v4, p )	catmull_rom_spline_interp_int16( v1, v2, v3, v4, p << ( 15 - (PS_STYPE_BITS+1) ) )
#else
    #define INTERP( v1, v2, p )		( v1 * (PS_STYPE_ONE-p) + v2 * p )
    #define INTERP2( v1, v2, p )	( v1 * (PS_STYPE_ONE-p) + v2 * p )
    #define INTERP_SPLINE( v1, v2, v3, v4, p )	catmull_rom_spline_interp_float32( v1, v2, v3, v4, p )
    #define INTERP2_SPLINE( v1, v2, v3, v4, p )	catmull_rom_spline_interp_float32( v1, v2, v3, v4, p )
#endif
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE	ctl_type;
    PS_CTYPE	ctl_mono;
    PS_CTYPE	ctl_max_delay;
    PS_CTYPE	ctl_interp;
    int     	max_delay;
    int		buf_size;
    PS_STYPE*  	buf[ MODULE_OUTPUTS ];
    bool	buf_clean;
    uint     	buf_ptr;
    int		empty_frames_counter;
    uint32_t    changed; 
};
static void modulator_handle_changes( MODULE_DATA* data, psynth_net* pnet )
{
    if( data->changed & CHANGED_MAX_DELAY )
    {
	switch( data->ctl_max_delay )
	{
	    default: data->max_delay = pnet->sampling_freq / 25; break;
	    case 1: data->max_delay = pnet->sampling_freq * 0.08; break;
	    case 2: data->max_delay = pnet->sampling_freq * 0.2; break;
	    case 3: data->max_delay = pnet->sampling_freq * 0.5; break;
	    case 4: data->max_delay = pnet->sampling_freq; break;
	    case 5: data->max_delay = pnet->sampling_freq * 2; break;
	    case 6: data->max_delay = pnet->sampling_freq * 4; break;
	    case 7: data->max_delay = pnet->sampling_freq * 8; break;
	    case 8: data->max_delay = pnet->sampling_freq * 16; break;
	    case 9: data->max_delay = pnet->sampling_freq * 32; break;
	}
	int prev_buf_size = data->buf_size;
	int new_buf_size = round_to_power_of_two( data->max_delay + 4 ); 
	data->max_delay--;
	if( new_buf_size > prev_buf_size )
	{
	    data->buf_size = new_buf_size;
            for( int i = 0; i < MODULE_OUTPUTS; i++ )
            {
                data->buf[ i ] = SMEM_RESIZE2( data->buf[ i ], PS_STYPE, data->buf_size );
                smem_zero( data->buf[ i ] );
            }
    	    data->buf_clean = true;
    	    data->buf_ptr = 0;
    	    data->empty_frames_counter = data->buf_size;
	}
    }
    data->changed = 0;
}
#ifndef PS_STYPE_FLOATINGPOINT
    #define PHASE_MOD() \
        int v = out[ p ]; \
	v += PS_STYPE_ONE; \
        if( v < 0 ) v = 0; \
	if( v > PS_STYPE_ONE * 2 ) v = PS_STYPE_ONE * 2; \
        v = v * data->max_delay;   \
	int interp = v & ( ( PS_STYPE_ONE * 2 ) - 1 ); \
        v >>= PS_STYPE_BITS + 1;
    #define PHASE_MOD_ABS() \
	int v = out[ p ]; \
    	if( v < 0 ) v = -v; \
	if( v > PS_STYPE_ONE ) v = PS_STYPE_ONE; \
	v = v * data->max_delay; \
	int interp = v & ( PS_STYPE_ONE - 1 ); \
	v >>= PS_STYPE_BITS;
#else
    #define PHASE_MOD() \
	PS_STYPE vf = out[ p ]; \
        vf = ( vf + PS_STYPE_ONE ) * (PS_STYPE)0.5; \
        if( vf < 0 ) vf = 0; \
	if( vf > PS_STYPE_ONE ) vf = PS_STYPE_ONE; \
        int64_t v = vf * (PS_STYPE)interp_max; \
        v = v * data->max_delay; \
	PS_STYPE interp = (PS_STYPE)( v & (interp_max-1) ) / (PS_STYPE)interp_max; \
        v >>= interp_prec;
    #define PHASE_MOD_ABS() \
	PS_STYPE vf = out[ p ]; \
	if( vf < 0 ) vf = -vf; \
	if( vf > PS_STYPE_ONE ) vf = PS_STYPE_ONE; \
	int64_t v = vf * (PS_STYPE)interp_max; \
	v = v * data->max_delay; \
	PS_STYPE interp = (PS_STYPE)( v & (interp_max-1) ) / (PS_STYPE)interp_max; \
	v >>= interp_prec;
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
	    retval = (PS_RETTYPE)"Modulator";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Модуль амплитудной или фазовой модуляции.\nПервый подключенный на вход сигнал считается несущим (Carrier). Все последующие подключенные на вход - модулирующие (Modulator), которые влияют на громкость или частоту первого.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Amplitude or Phase modulator.\nFirst input = Carrier. Other inputs = Modulators.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF7FD1";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_DONT_FILL_INPUT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 5, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODULATION_TYPE ), ps_get_string( STR_PS_MODULATION_TYPES ), 0, 10, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MAX_PM_DELAY_LEN ), ps_get_string( STR_PS_PM_DELAY_LENS ), 0, 9, 0, 1, &data->ctl_max_delay, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PM_INTERPOLATION ), ps_get_string( STR_PS_SAMPLE_INTERP_TYPES ), 0, 2, 1, 1, &data->ctl_interp, -1, 0, pnet );
	    data->changed = 0xFFFFFFFF;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
            modulator_handle_changes( data, pnet );
            retval = 1;
            break;
	case PS_CMD_CLEAN:
            data->empty_frames_counter = data->buf_size;
            if( data->ctl_type >= 1 )
            {
        	if( !data->buf_clean )
        	{
        	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
        	    {
            		smem_zero( data->buf[ i ] );
        	    }
        	    data->buf_clean = true;
        	}
    	    }
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( data->changed ) modulator_handle_changes( data, pnet );
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
		bool no_input_signal = true;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			no_input_signal = false;
			break;
		    }
		}
		bool phase_modulation = 0;
		if( data->ctl_type == 1 || data->ctl_type == 2 ) phase_modulation = 1;
		if( !phase_modulation )
		{
		    if( data->ctl_volume == 0 ) no_input_signal = 1;
		    if( no_input_signal )
			break;
		}
		else
		{
		    if( no_input_signal ) 
		    {
			if( data->empty_frames_counter >= data->buf_size )
                	{
                    	    data->buf_ptr += frames;
                    	    break;
                	}
                	data->empty_frames_counter += frames;
		    }
		    else
		    {
			data->empty_frames_counter = 0;
		    }
		}
		data->buf_clean = false;
		PS_STYPE* pm_carrier[ MODULE_OUTPUTS ]; 
		for( int i = 0; i < mod->input_links_num; i++ )
		{
                    psynth_module* m = psynth_get_module( mod->input_links[ i ], pnet );
                    if( !m ) continue;
                    int mcc = psynth_get_number_of_outputs( m );
                    if( mcc == 0 ) continue;
                    if( !m->channels_out[ 0 ] ) continue; 
                    if( !phase_modulation )
                    {
			if( !( m->flags & PSYNTH_FLAG_OUTPUT_IS_EMPTY ) )
                        {
			    PS_STYPE* in = NULL;
			    PS_STYPE** outputs2 = m->channels_out;
			    for( int ch = 0; ch < outputs_num; ch++ )
			    {
			        if( outputs2[ ch ] && ch < mcc )
			    	    in = outputs2[ ch ] + offset;
			        PS_STYPE* out = outputs[ ch ] + offset;
				if( retval == 0 )
				{
				    for( int p = 0; p < frames; p++ )
				        out[ p ] = in[ p ];
				}
				else
				{
				    switch( data->ctl_type )
				    {
					case 0: 
					    for( int p = 0; p < frames; p++ )
					    {
				    		PS_STYPE2 v = out[ p ];
				    		v *= in[ p ];
#ifndef PS_STYPE_FLOATINGPOINT
				    		v /= PS_STYPE_ONE;
#endif
				    		out[ p ] = v;
					    }
					    break;
					case 3: 
					    for( int p = 0; p < frames; p++ ) out[ p ] += in[ p ];
					    break;
					case 4: 
					    for( int p = 0; p < frames; p++ ) out[ p ] -= in[ p ];
					    break;
					case 5: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                PS_STYPE v1 = out[ p ];
                                                PS_STYPE v2 = in[ p ];
                                                if( v2 < v1 ) out[ p ] = v2;
                                            }
                                            break;
                                        case 6: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                PS_STYPE v1 = out[ p ];
                                                PS_STYPE v2 = in[ p ];
                                                if( v2 > v1 ) out[ p ] = v2;
                                            }
                                            break;
                                        case 7: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                int v1, v2;
                                                PS_STYPE_TO_INT16( v1, out[ p ] );
                                                PS_STYPE_TO_INT16( v2, in[ p ] );
                                                v1 = v1 & v2;
                                                PS_INT16_TO_STYPE( out[ p ], v1 );
                                            }
                                            break;
                                        case 8: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                int v1, v2;
                                                PS_STYPE_TO_INT16( v1, out[ p ] );
                                                PS_STYPE_TO_INT16( v2, in[ p ] );
                                                v1 = v1 ^ v2;
                                                PS_INT16_TO_STYPE( out[ p ], v1 );
                                            }
                                            break;
                                        case 9: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                PS_STYPE v1 = out[ p ];
                                                PS_STYPE v2 = in[ p ];
                                                PS_STYPE v1_abs = PS_STYPE_ABS( v1 );
                                                PS_STYPE v2_abs = PS_STYPE_ABS( v2 );
                                                if( v2_abs < v1_abs ) out[ p ] = v2;
                                            }
                                            break;
                                        case 10: 
                                            for( int p = 0; p < frames; p++ )
                                            {
                                                PS_STYPE v1 = out[ p ];
                                                PS_STYPE v2 = in[ p ];
                                                PS_STYPE v1_abs = PS_STYPE_ABS( v1 );
                                                PS_STYPE v2_abs = PS_STYPE_ABS( v2 );
                                                if( v2_abs > v1_abs ) out[ p ] = v2;
                                            }
                                            break;
					default: break;
				    }
				}
                            }
                            retval = 1;
                        }
                    }
                    else
                    {
			PS_STYPE* in = NULL;
                        PS_STYPE** outputs2 = m->channels_out;
			for( int ch = 0; ch < outputs_num; ch++ )
                        {
			    if( outputs2[ ch ] && ch < mcc )
                                in = outputs2[ ch ] + offset;
                            PS_STYPE* out = outputs[ ch ] + offset;
                            if( retval == 0 )
                            {
                                pm_carrier[ ch ] = in;
                            }
                            else
                            {
				if( retval == 1 )
				{
				    for( int p = 0; p < frames; p++ )
					out[ p ] = in[ p ];
				}
                                else
				{
				    for( int p = 0; p < frames; p++ )
					out[ p ] += in[ p ];
				}
                            }
                        }
			retval++;
                    }
		} 
		if( retval )
		{
		    if( phase_modulation )
		    {
			if( retval == 1 )
			{
			    for( int ch = 0; ch < outputs_num; ch++ )
			    {
				PS_STYPE* in = pm_carrier[ ch ];
				PS_STYPE* out = outputs[ ch ] + offset;
				for( int p = 0; p < frames; p++ )
				{
				    out[ p ] = in[ p ];
				}
			    }
			}
			else
			{
			    uint buf_ptr = data->buf_ptr;
			    uint buf_mask = data->buf_size - 1;
			    uint interp_prec = 24;
			    uint interp_max = 16777216; 
#ifdef PS_STYPE_FLOATINGPOINT
			    if( pnet->base_host_version < 0x02010100 )
			    {
				interp_prec = 15;
				interp_max = 32768;
			    }
#endif
			    for( int ch = 0; ch < outputs_num; ch++ )
			    {
				PS_STYPE* in = pm_carrier[ ch ];
				PS_STYPE* out = outputs[ ch ] + offset;
				PS_STYPE* cbuf = data->buf[ ch ];
				buf_ptr = data->buf_ptr;
				if( data->ctl_type == 1 )
				{
				    switch( data->ctl_interp )
				    {
					case 0: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD();
					    out[ p ] = cbuf[ ( buf_ptr - (int)v ) & buf_mask ];
					}
					break;
					case 1: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD();
					    PS_STYPE2 v1 = cbuf[ ( buf_ptr - (int)v ) & buf_mask ];
					    PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( (int)v + 1 ) ) & buf_mask ];
					    out[ p ] = INTERP2( v1, v2, interp );
					}
					break;
					case 2: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD();
					    PS_STYPE2 v1 = cbuf[ ( buf_ptr - ( (int)v - 1 ) ) & buf_mask ];
					    PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( (int)v + 0 ) ) & buf_mask ];
					    PS_STYPE2 v3 = cbuf[ ( buf_ptr - ( (int)v + 1 ) ) & buf_mask ];
					    PS_STYPE2 v4 = cbuf[ ( buf_ptr - ( (int)v + 2 ) ) & buf_mask ];
					    out[ p ] = INTERP2_SPLINE( v1, v2, v3, v4, interp );
					}
					break;
				    }
				}
				else
				{
				    switch( data->ctl_interp )
				    {
					case 0: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD_ABS();
					    out[ p ] = cbuf[ ( buf_ptr - (int)v ) & buf_mask ];
					}
					break;
					case 1: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD_ABS();
					    PS_STYPE2 v1 = cbuf[ ( buf_ptr - (int)v ) & buf_mask ];
					    PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( (int)v + 1 ) ) & buf_mask ];
					    out[ p ] = INTERP( v1, v2, interp );
					}
					break;
					case 2: 
					for( int p = 0; p < frames; p++, buf_ptr++ )
					{
					    cbuf[ buf_ptr & buf_mask ] = in[ p ]; 
					    PHASE_MOD_ABS();
					    PS_STYPE2 v1 = cbuf[ ( buf_ptr - ( (int)v - 1 ) ) & buf_mask ];
					    PS_STYPE2 v2 = cbuf[ ( buf_ptr - ( (int)v + 0 ) ) & buf_mask ];
					    PS_STYPE2 v3 = cbuf[ ( buf_ptr - ( (int)v + 1 ) ) & buf_mask ];
					    PS_STYPE2 v4 = cbuf[ ( buf_ptr - ( (int)v + 2 ) ) & buf_mask ];
					    out[ p ] = INTERP_SPLINE( v1, v2, v3, v4, interp );
					}
					break;
				    }
				}
			    }
			    data->buf_ptr = buf_ptr;
			}
			retval = 1;
		    } 
		    if( data->ctl_volume != 256 )
		    {
			for( int ch = 0; ch < outputs_num; ch++ )
			{
			    PS_STYPE* out = outputs[ ch ] + offset;
#ifdef PS_STYPE_FLOATINGPOINT
			    float v = (float)data->ctl_volume / 256.0F;
			    for( int p = 0; p < frames; p++ )
				out[ p ] *= v;
#else
			    for( int p = 0; p < frames; p++ )
			    {
				PS_STYPE2 v = out[ p ];
				v *= data->ctl_volume;
				v /= 256;
				out[ p ] = (PS_STYPE)v;
			    }
#endif
			}
		    }
		}
		if( retval ) retval = 1;
		if( data->ctl_volume == 0 ) retval = 2;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
            switch( event->controller.ctl_num )
            {
                case 3: 
                    data->changed |= CHANGED_MAX_DELAY;
                    break;
            }
            break;
	case PS_CMD_CLOSE:
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
            {
                smem_free( data->buf[ i ] );
            }
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
