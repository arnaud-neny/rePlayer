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
#define MODULE_DATA	psynth_distortion_data
#define MODULE_HANDLER	psynth_distortion
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#ifdef WITH_INTERPOLATION
    #define MAX_INTERP_POINTS	4
#else
    #define MAX_INTERP_POINTS	1
#endif
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_type;
    PS_CTYPE   	ctl_power;
    PS_CTYPE   	ctl_bitrate;
    PS_CTYPE   	ctl_freq;
#ifdef WITH_INTERPOLATION
    PS_CTYPE	ctl_interp;
#endif
    PS_CTYPE   	ctl_noise;
    uint32_t    noise_seed;
    uint	cnt;
    PS_STYPE   	smp[ MODULE_OUTPUTS * MAX_INTERP_POINTS ];
    int		smp_ptr;
    bool	smp_clean;
#ifndef PS_STYPE_FLOATINGPOINT
    PS_STYPE*	sin_tab; 
#endif
};
#ifndef PS_STYPE_FLOATINGPOINT
static int isqrt( int n ) 
{
    int g = 0x8000; 
    int c = 0x8000;
    for(;;)
    {
	if( g * g > n ) g ^= c;
	c >>= 1;
	if( c == 0 ) return g;
        g |= c;
    }
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
	    retval = (PS_RETTYPE)"Distortion";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Этот модуль вносит разные типы искажений в звук";
                        break;
                    }
		    retval = (PS_RETTYPE)"This module adds various types of distortion to the sound";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FFF88";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
#ifdef WITH_INTERPOLATION
	    psynth_resize_ctls_storage( mod_num, 7, pnet );
#else
	    psynth_resize_ctls_storage( mod_num, 6, pnet );
#endif
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 128, 0, &data->ctl_volume, 128, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TYPE ), ps_get_string( STR_PS_DISTORTION_TYPES ), 0, 10, 0, 1, &data->ctl_type, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POWER ), "", 0, 256, 0, 0, &data->ctl_power, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BIT_DEPTH ), "", 1, 16, 16, 1, &data->ctl_bitrate, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ ), ps_get_string( STR_PS_HZ ), 0, 44100, 44100, 0, &data->ctl_freq, -1, 2, pnet );
	    psynth_set_ctl_flags( mod_num, 4, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOISE ), "", 0, 256, 0, 0, &data->ctl_noise, -1, 3, pnet );
#ifdef WITH_INTERPOLATION
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INTERPOLATION ), ps_get_string( STR_PS_INTERP_TYPES ), 0, 2, 0, 1, &data->ctl_interp, -1, 2, pnet );
#endif
	    data->cnt = 0;
	    data->noise_seed = (uint32_t)stime_ns() + mod_num * 49157 + mod->id * 3079;
	    SMEM_CLEAR_STRUCT( data->smp );
	    data->smp_clean = true;
	    data->smp_ptr = 0;
#ifndef PS_STYPE_FLOATINGPOINT
	    data->sin_tab = (PS_STYPE*)psynth_get_sine_table( sizeof( PS_STYPE ), true, 9, PS_STYPE_ONE );
#endif
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    SMEM_CLEAR_STRUCT( data->smp );
	    data->smp_clean = true;
	    data->smp_ptr = 0;
	    data->cnt = 0;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
		bool no_input_signal = true;
		for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			no_input_signal = false;
			break;
		    }
		}
		if( no_input_signal ) break;
		PS_STYPE2 volume = PS_NORM_STYPE( data->ctl_volume, 128 );
		int type = data->ctl_type;
		int power = data->ctl_power;
		if( power > 255 ) power = 255;
		int bitrate = data->ctl_bitrate;
		int freq = data->ctl_freq; 
		if( freq <= 0 ) freq = 1;
		uint delta = 0;
		if( freq < 44100 )
		{
		    delta = ( (int64_t)freq << 32 ) / pnet->sampling_freq;
		    if( delta < 1 ) delta = 1;
		}
		PS_STYPE limit;
		PS_STYPE max_val;
		PS_INT16_TO_STYPE( limit, ( ( 256 - power ) << 7 ) );
		if( pnet->base_host_version >= 0x02010002 )
		{
		    PS_INT16_TO_STYPE( max_val, 32768 );
		}
		else
		{
		    PS_INT16_TO_STYPE( max_val, 32767 );
		}
		PS_STYPE2 coef = ( max_val * 256 ) / limit;
		uint cnt;
		int smp_ptr;
		int vars_changed = 0;
		for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    smem_copy( out, in, sizeof( PS_STYPE ) * frames ); 
		    if( bitrate < 16 && volume != 0 )
		    {
			for( int i = 0; i < frames; i++ )
			{
		    	    PS_STYPE2 v = out[ i ];
			    int res16;
			    PS_STYPE_TO_INT16( res16, v );
			    res16 >>= 16 - bitrate;
			    res16 <<= 16 - bitrate;
			    PS_INT16_TO_STYPE( v, res16 );
			    out[ i ] = v;
			}
		    }
		    if( freq < 44100 )
		    {
			vars_changed |= 1;
			if( data->smp_clean )
			{
			    data->smp_clean = false;
			    for( int i = 0; i < MODULE_OUTPUTS * MAX_INTERP_POINTS; i++ )
				data->smp[ i ] = out[ 0 ];
			}
			cnt = data->cnt;
			smp_ptr = 0;
			PS_STYPE* smp = &data->smp[ ch * MAX_INTERP_POINTS ];
#ifdef WITH_INTERPOLATION
			smp_ptr = data->smp_ptr;
			switch( data->ctl_interp )
			{
			    case 0: 
#endif
			    for( int i = 0; i < frames; i++ )
			    {
				uint prev_cnt = cnt;
				cnt += delta;
				if( cnt < prev_cnt ) smp[ smp_ptr ] = out[ i ];
				out[ i ] = smp[ smp_ptr ];
			    }
#ifdef WITH_INTERPOLATION
			    break;
			    case 1: 
			    for( int i = 0; i < frames; i++ )
			    {
				uint prev_cnt = cnt;
				cnt += delta;
				if( cnt < prev_cnt )
				{
				    smp_ptr = ( smp_ptr + 1 ) & (MAX_INTERP_POINTS-1);
			    	    smp[ smp_ptr ] = out[ i ];
				}
				PS_STYPE2 x = cnt >> 17;
				PS_STYPE2 v = ( x * smp[ smp_ptr ] + ( 32768 - x ) * smp[ ( smp_ptr - 1 ) & (MAX_INTERP_POINTS-1) ] ) / 32768;
				out[ i ] = v;
			    }
			    break;
			    case 2: 
			    for( int i = 0; i < frames; i++ )
			    {
				uint prev_cnt = cnt;
				cnt += delta;
				if( cnt < prev_cnt )
				{
				    smp_ptr = ( smp_ptr + 1 ) & (MAX_INTERP_POINTS-1);
			    	    smp[ smp_ptr ] = out[ i ];
				}
				PS_STYPE2 v0 = smp[ ( smp_ptr - 3 ) & (MAX_INTERP_POINTS-1) ];
            			PS_STYPE2 v1 = smp[ ( smp_ptr - 2 ) & (MAX_INTERP_POINTS-1) ];
            			PS_STYPE2 v2 = smp[ ( smp_ptr - 1 ) & (MAX_INTERP_POINTS-1) ];
            			PS_STYPE2 v3 = smp[ smp_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				PS_STYPE2 x = ( cnt >> 10 ) / ( 32768.0F * 128 );
				PS_STYPE2 v = catmull_rom_spline_interp_float32( v0, v1, v2, v3, x );
#else
				PS_STYPE2 x = cnt >> 17;
				PS_STYPE2 v = catmull_rom_spline_interp_int16( v0, v1, v2, v3, x );
#endif
				out[ i ] = v;
			    }
			    break;
			    default: break;
			}
#endif
		    }
		    if( power && volume != 0 )
		    {
			switch( type )
			{
			    case 0: case 1: 
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
		    		if( type == 1 )
		    		{
			    	    if( v > limit ) v = limit - ( v - limit );
			    	    if( -v > limit ) v = -limit + ( -v - limit );
				}
				if( v > limit ) v = limit;
				if( -v > limit ) v = -limit;
				v *= coef;
				v /= 256;
				out[ i ] = v;
			    }
			    break;
			    case 2: 
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
		    		v = v * coef / 256;
		    		PS_STYPE2 v2 = PS_STYPE_ABS( v );
			        if( v2 > PS_STYPE_ONE )
			        {
			    	    int vv = (int)v2 / PS_STYPE_ONE;
			    	    if( ( vv & 1 ) == 0 )
			    		v2 = v2 - vv * PS_STYPE_ONE;
			    	    else
			    		v2 = PS_STYPE_ONE - ( v2 - vv * PS_STYPE_ONE );
			        }
			    	if( v < 0 ) v2 = -v2;
				out[ i ] = v2;
			    }
			    break;
			    case 3: 
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
		    		v = v * coef / 256;
		    		PS_STYPE2 v2 = PS_STYPE_ABS( v + PS_STYPE_ONE );
			        if( v2 > PS_STYPE_ONE )
			        {
			    	    int vv = (int)v2 / ( PS_STYPE_ONE * 2 );
			    	    if( ( vv & 1 ) == 0 )
			    		v2 = v2 - vv * PS_STYPE_ONE * 2;
			    	    else
			    		v2 = ( PS_STYPE_ONE * 2 ) - ( v2 - vv * PS_STYPE_ONE * 2 );
			        }
				out[ i ] = v2 - PS_STYPE_ONE;
			    }
			    break;
			    case 4: 
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
		    		v = v * coef / 256;
		    		PS_STYPE2 v2 = PS_STYPE_ABS( v + PS_STYPE_ONE );
			        int vv = (int)v2 / ( PS_STYPE_ONE * 2 );
			        v2 = v2 - vv * PS_STYPE_ONE * 2;
				out[ i ] = v2 - PS_STYPE_ONE;
			    }
			    break;
			    case 5: 
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
		    		v = v * coef / 256;
		    		int32_t vv = v * ( 32768 / PS_STYPE_ONE ) + 32768;
		    		vv &= 65535;
		    		vv -= 32768;
		    		PS_INT16_TO_STYPE( v, vv );
				out[ i ] = v;
			    }
			    break;
			    case 6: 
			    {
#ifdef PS_STYPE_FLOATINGPOINT
		    		PS_STYPE2 p = data->ctl_power / (PS_STYPE2)256;
		    		PS_STYPE2 p2 = (PS_STYPE2)1 - p;
#else
		    		PS_STYPE2 p = data->ctl_power;
		    		PS_STYPE2 p2 = 256 - p;
#endif
				for( int i = 0; i < frames; i++ )
				{
		    		    PS_STYPE2 v0 = out[ i ];
		    		    PS_STYPE2 v = v0;
				    if( v < -PS_STYPE_ONE * (PS_STYPE2)2 ) v = -PS_STYPE_ONE * (PS_STYPE2)2;
				    else if( v > PS_STYPE_ONE * (PS_STYPE2)2 ) v = PS_STYPE_ONE * (PS_STYPE2)2;
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = (PS_STYPE2)1.5 * v - (PS_STYPE2)0.5 * v * v * v;
#else
		    		    v = ( (PS_STYPE2)(1.5*PS_STYPE_ONE) * v - ( ( v * v ) >> (PS_STYPE_BITS+1) ) * v ) >> PS_STYPE_BITS;
#endif
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v * p + v0 * p2;
#else
		    		    v = ( v * p + v0 * p2 ) / 256;
#endif
				    out[ i ] = v;
				}
			    }
			    break;
			    case 7: 
			    {
#ifdef PS_STYPE_FLOATINGPOINT
		    		PS_STYPE2 p = data->ctl_power / (PS_STYPE2)256;
		    		PS_STYPE2 p2 = (PS_STYPE2)1 - p;
#else
		    		PS_STYPE2 p = data->ctl_power;
		    		PS_STYPE2 p2 = 256 - p;
#endif
				for( int i = 0; i < frames; i++ )
				{
		    		    PS_STYPE2 v0 = out[ i ];
		    		    PS_STYPE2 v = v0;
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = PS_STYPE_SIN( v );
#else
				    v = v * (PS_STYPE2)(1/M_PI*256) * 4; 
				    int tab_ptr = v / ( PS_STYPE_ONE * 4 );
				    PS_STYPE2 iv1 = data->sin_tab[ tab_ptr & 511 ];
				    PS_STYPE2 iv2 = data->sin_tab[ ( tab_ptr + 1 ) & 511 ];
				    PS_STYPE ip = v & ( PS_STYPE_ONE * 4 - 1 );
				    v = DSP_LINEAR_INTERP( iv1, iv2, ip, PS_STYPE_ONE * 4 );
#endif
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v * p + v0 * p2;
#else
		    		    v = ( v * p + v0 * p2 ) / 256;
#endif
				    out[ i ] = v;
				}
			    }
			    break;
			    case 8: 
			    {
#ifdef PS_STYPE_FLOATINGPOINT
		    		PS_STYPE2 p = data->ctl_power / (PS_STYPE2)256;
		    		PS_STYPE2 p2 = (PS_STYPE2)1 - p;
#else
		    		PS_STYPE2 p = data->ctl_power;
		    		PS_STYPE2 p2 = 256 - p;
#endif
				for( int i = 0; i < frames; i++ )
				{
		    		    PS_STYPE2 v0 = out[ i ];
		    		    PS_STYPE2 v = v0;
		    		    if( v >= 0 )
		    		    {
		    			if( v > PS_STYPE_ONE ) v = PS_STYPE_ONE;
		    			else
		    			{
		    			    v = PS_STYPE_ONE - v;
#ifdef PS_STYPE_FLOATINGPOINT
		    			    v *= v;
		    			    v *= v;
#else
					    v = v * v / PS_STYPE_ONE;
					    v = v * v / PS_STYPE_ONE;
#endif
		    			    v = PS_STYPE_ONE - v;
		    			}
		    	    	    }
		    		    else
		    		    {
		    			if( v < -PS_STYPE_ONE ) v = -PS_STYPE_ONE;
		    			else
		    			{
		    			    v += PS_STYPE_ONE;
#ifdef PS_STYPE_FLOATINGPOINT
		    			    v *= v;
		    			    v *= v;
#else
					    v = v * v / PS_STYPE_ONE;
					    v = v * v / PS_STYPE_ONE;
#endif
		    			    v -= PS_STYPE_ONE;
		    			}
		    		    }
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v * p + v0 * p2;
#else
		    		    v = ( v * p + v0 * p2 ) / 256;
#endif
				    out[ i ] = v;
				}
			    }
			    break;
			    case 9: 
			    {
#ifdef PS_STYPE_FLOATINGPOINT
		    		PS_STYPE2 p = data->ctl_power / (PS_STYPE2)256;
		    		PS_STYPE2 p2 = (PS_STYPE2)1 - p;
#else
		    		PS_STYPE2 p = data->ctl_power;
		    		PS_STYPE2 p2 = 256 - p;
#endif
				for( int i = 0; i < frames; i++ )
				{
		    		    PS_STYPE2 v0 = out[ i ];
		    		    PS_STYPE2 v = v0 * (PS_STYPE2)2;
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v / sqrt( PS_STYPE_ONE + v * v );
#else
				    const int sqrt_mul = sqrt( PS_STYPE_ONE );
				    PS_STYPE2 vv = PS_STYPE_ONE + (PS_STYPE2)( ( (int64_t)v * v ) >> PS_STYPE_BITS );
				    v = v * PS_STYPE_ONE / ( (int)isqrt( vv ) * sqrt_mul );
#endif
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v * p + v0 * p2;
#else
		    		    v = ( v * p + v0 * p2 ) / 256;
#endif
				    out[ i ] = v;
				}
			    }
			    break;
			    case 10: 
			    {
#ifdef PS_STYPE_FLOATINGPOINT
		    		PS_STYPE2 p = data->ctl_power / (PS_STYPE2)256;
		    		PS_STYPE2 p2 = (PS_STYPE2)1 - p;
#else
		    		PS_STYPE2 p = data->ctl_power;
		    		PS_STYPE2 p2 = 256 - p;
#endif
				for( int i = 0; i < frames; i++ )
				{
		    		    PS_STYPE2 v0 = out[ i ];
		    		    PS_STYPE2 v = v0 * (PS_STYPE2)4;
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v / ( PS_STYPE_ONE + fabs( v ) );
#else
		    		    v = v * PS_STYPE_ONE / ( PS_STYPE_ONE + abs( v ) );
#endif
#ifdef PS_STYPE_FLOATINGPOINT
		    		    v = v * p + v0 * p2;
#else
		    		    v = ( v * p + v0 * p2 ) / 256;
#endif
				    out[ i ] = v;
				}
			    }
			    break;
			}
		    }
		    if( volume != 128 )
		    {
			if( volume == 0 )
			    for( int i = 0; i < frames; i++ )
			    {
				out[ i ] = 0;
			    }
			else
			    for( int i = 0; i < frames; i++ )
			    {
		    		PS_STYPE2 v = out[ i ];
				v = PS_NORM_STYPE_MUL( v, volume, 128 );
				out[ i ] = v;
			    }
		    }
		    if( data->ctl_noise > 0 && volume != 0 )
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			float noise = (float)data->ctl_noise / 256;
#else
			int noise = data->ctl_noise;
#endif
			for( int i = 0; i < frames; i++ )
			{
			    int r = psynth_rand2( &data->noise_seed );
			    PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
			    v = ( (PS_STYPE2)r / 32768 ) * noise;
			    v *= out[ i ];
#else
			    v = ( r * out[ i ] ) >> 15;
			    v = ( v * noise ) >> 8;
#endif
			    out[ i ] += v;
			}
		    }
		}
		if( vars_changed )
		{
	    	    data->cnt = cnt;
		    data->smp_ptr = smp_ptr;
		}
		retval = 1;
		if( volume == 0 ) retval = 2;
	    }
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
