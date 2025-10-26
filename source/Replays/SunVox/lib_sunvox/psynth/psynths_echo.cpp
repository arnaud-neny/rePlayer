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
#define MODULE_DATA	psynth_echo_data
#define MODULE_HANDLER	psynth_echo
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_FFREQ 	22000
#define DELAY_UNIT_MAX	6
#define CHANGED_BUFSIZE		( 1 << 0 )
#define CHANGED_FILTER		( 1 << 1 )
struct MODULE_DATA
{
    PS_CTYPE	ctl_dry;
    PS_CTYPE   	ctl_wet;
    PS_CTYPE   	ctl_feedback;
    PS_CTYPE   	ctl_delay;
    PS_CTYPE   	ctl_roffset;
    PS_CTYPE   	ctl_delay_units;
    PS_CTYPE   	ctl_roffset_val;
    PS_CTYPE   	ctl_filter;
    PS_CTYPE   	ctl_ffreq;
    int	    	max_buf_size;
    int	    	buf_size;
    int 	delay_len; 
    PS_STYPE*  	buf[ MODULE_OUTPUTS ];
    int	    	buf_ptr;
    bool	buf_clean;
    bool    	empty; 
    int     	tick_size; 
    uint8_t   	ticks_per_line;
    PS_STYPE2	f_a0; 
    PS_STYPE2   f_b1; 
    PS_STYPE2   f_v[ MODULE_OUTPUTS ];
#ifndef PS_STYPE_FLOATINGPOINT
    PS_STYPE2   f_v_rem[ MODULE_OUTPUTS ];
#endif
    uint32_t    changed; 
};
static void echo_change_ctl_limits( int mod_num, psynth_net* pnet )
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
    int max = 256;
    switch( data->ctl_delay_units )
    {
        case 1:
    	    max = 4000;
	    break;
        case 2:
	    max = 8192;
	    break;
    }
    int cnum = 3;
    if( mod->ctls[ cnum ].max != max )
    {
	psynth_change_ctl_limits(
	    mod_num,
	    cnum,
	    -1,
	    max,
	    max,
	    pnet );
	mod->full_redraw_request++;
    }
}
static void echo_handle_changes( MODULE_DATA* data, psynth_net* pnet )
{
    if( data->changed & CHANGED_BUFSIZE )
    {
	int delay_len = 0;
	switch( data->ctl_delay_units )
	{
	    case 0:
		delay_len = ( pnet->sampling_freq * data->ctl_delay ) >> 8;
		break;
	    case 1:
		delay_len = ( pnet->sampling_freq * data->ctl_delay ) / 1000;
		break;
	    case 2:
		if( data->ctl_delay == 0 )
		    delay_len = pnet->sampling_freq;
		else
		    delay_len = pnet->sampling_freq / data->ctl_delay;
		break;
	    case 3:
		delay_len = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ctl_delay ) / 256 );
		break;
	    case 4:
	    case 5:
	    case 6:
		delay_len = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line * (uint64_t)data->ctl_delay ) / 256 );
		delay_len /= ( data->ctl_delay_units - 3 );
		break;
	}
	if( delay_len == 0 ) delay_len = 1;
	if( delay_len > data->max_buf_size ) delay_len = data->max_buf_size;
	bool buf_size_changed = false;
	while( delay_len > data->buf_size )
	{
	    if( data->buf_size == 0 )
		data->buf_size = pnet->sampling_freq;
	    else
		data->buf_size += pnet->sampling_freq;
	    buf_size_changed = true;
	}
	if( buf_size_changed )
	{
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
            {
    		data->buf[ i ] = SMEM_ZRESIZE2( data->buf[ i ], PS_STYPE, data->buf_size );
    	    }
	}
	data->delay_len = delay_len;
	if( data->buf_ptr >= delay_len ) data->buf_ptr = 0;
    }
    if( data->changed & CHANGED_FILTER )
    {
	float a0 = 0;
	float b1 = 0;
	switch( data->ctl_filter )
	{
	    case 1: 
	    case 2: 
    		b1 = exp( -2.0 * M_PI * ( (float)data->ctl_ffreq / (float)pnet->sampling_freq ) );
	    	a0 = 1.0 - b1;
	    	break;
	    default: break;
	}
#ifdef PS_STYPE_FLOATINGPOINT
	data->f_b1 = b1;
	data->f_a0 = a0;
#else
	data->f_b1 = 32768 * b1;
	data->f_a0 = 32768 * a0;
#endif
    }
    data->changed = 0;
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
	    retval = (PS_RETTYPE)"Echo";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Стерео эхо.\nМаксимальная длина задержки = 4 секунды.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Echo.\nMax delay length = 4 seconds.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFA37F";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 9, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DRY ), "", 0, 256, 256, 0, &data->ctl_dry, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WET ), "", 0, 256, 40, 0, &data->ctl_wet, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 256, 128, 0, &data->ctl_feedback, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY ), "", 0, 256, 256, 0, &data->ctl_delay, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_R_CHANNEL_OFFSET ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_roffset, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY_UNIT ), ps_get_string( STR_PS_ECHO_DELAY_UNITS ), 0, DELAY_UNIT_MAX, 0, 1, &data->ctl_delay_units, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_R_CHANNEL_OFFSET ), ps_get_string( STR_PS_DELAY_OFFSET15 ), 0, 32768, 32768/2, 0, &data->ctl_roffset_val, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FILTER ), ps_get_string( STR_PS_ECHO_FILTERS ), 0, 2, 0, 1, &data->ctl_filter, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_FREQ ), ps_get_string( STR_PS_HZ ), 0, MAX_FFREQ, 2000, 0, &data->ctl_ffreq, -1, 1, pnet );
	    psynth_set_ctl_flags( mod_num, 8, PSYNTH_CTL_FLAG_EXP3, pnet );
	    data->max_buf_size = pnet->sampling_freq * 4;
	    data->buf_ptr = 0;
	    data->buf_clean = true;
	    data->empty = true;
	    data->changed = 0xFFFFFFFF;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    echo_change_ctl_limits( mod_num, pnet );
	    echo_handle_changes( data, pnet );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    if( data->buf_clean == false )
	    {
		for( int i = 0; i < MODULE_OUTPUTS; i++ )
		{
		    smem_zero( data->buf[ i ] );
		    data->f_v[ i ] = 0;
#ifndef PS_STYPE_FLOATINGPOINT
		    data->f_v_rem[ i ] = 0;
#endif
		}
		data->buf_clean = true;
	    }
	    data->buf_ptr = 0;
	    data->empty = true;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
		bool input_signal = false;
		for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			data->empty = false;
			input_signal = true;
			break;
		    }
		}
		if( data->empty ) break;
		if( data->changed ) echo_handle_changes( data, pnet );
		data->buf_clean = false;
		int buf_ptr;
		PS_STYPE2 ctl_dry = PS_NORM_STYPE( data->ctl_dry, 256 );
		PS_STYPE2 ctl_wet = PS_NORM_STYPE( data->ctl_wet, 256 );
		PS_STYPE2 ctl_feedback = PS_NORM_STYPE( data->ctl_feedback, 256 );
		int roffset = 0;
		if( data->ctl_roffset )
		    roffset = (uint64_t)data->delay_len * (uint64_t)data->ctl_roffset_val / 32768;
		for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    PS_STYPE* RESTRICT in = inputs[ ch ] + offset;
		    PS_STYPE* RESTRICT out = outputs[ ch ] + offset;
		    PS_STYPE* RESTRICT cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    if( data->ctl_filter )
		    {
			int write_offset = 0;
			if( ch && roffset ) write_offset = roffset;
			PS_STYPE2 f_v = data->f_v[ ch ];
#ifndef PS_STYPE_FLOATINGPOINT
			PS_STYPE2 f_v_rem = data->f_v_rem[ ch ];
#endif
			if( data->ctl_filter == 1 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 v = cbuf[ buf_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				f_v = v * data->f_a0 + f_v * data->f_b1;
#else
				f_v = v * data->f_a0 + f_v * data->f_b1 + f_v_rem;
				f_v_rem = f_v % 32768;
				f_v /= 32768;
#endif
				v = f_v;
				out[ i ] = PS_NORM_STYPE_MUL( v, ctl_wet, 256 );
				v = PS_NORM_STYPE_MUL( v, ctl_feedback, 256 );
				psynth_denorm_add_white_noise( v );
				cbuf[ buf_ptr ] = v;
				int ptr2 = buf_ptr + write_offset; if( ptr2 >= data->delay_len ) ptr2 -= data->delay_len;
				cbuf[ ptr2 ] += in[ i ];
				buf_ptr++;
				if( buf_ptr >= data->delay_len ) buf_ptr = 0;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 v = cbuf[ buf_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				f_v = v * data->f_a0 + f_v * data->f_b1;
#else
				f_v = v * data->f_a0 + f_v * data->f_b1 + f_v_rem;
				f_v_rem = f_v % 32768;
				f_v /= 32768;
#endif
				v = v - f_v;
				out[ i ] = PS_NORM_STYPE_MUL( v, ctl_wet, 256 );
				v = PS_NORM_STYPE_MUL( v, ctl_feedback, 256 );
				psynth_denorm_add_white_noise( v );
				cbuf[ buf_ptr ] = v;
				int ptr2 = buf_ptr + write_offset; if( ptr2 >= data->delay_len ) ptr2 -= data->delay_len;
				cbuf[ ptr2 ] += in[ i ];
				buf_ptr++;
				if( buf_ptr >= data->delay_len ) buf_ptr = 0;
			    }
			}
			data->f_v[ ch ] = f_v;
#ifndef PS_STYPE_FLOATINGPOINT
			data->f_v_rem[ ch ] = f_v_rem;
#endif
		    }
		    else
		    {
			if( input_signal )
			{
			    for( int i = 0; i < frames; i++ )
			    {
			        PS_STYPE2 v = cbuf[ buf_ptr ];
			        out[ i ] = PS_NORM_STYPE_MUL( v, ctl_wet, 256 );
			        v = PS_NORM_STYPE_MUL( v, ctl_feedback, 256 );
			        psynth_denorm_add_white_noise( v );
			        cbuf[ buf_ptr ] = v;
			        if( ch && roffset )
			        {
				    int ptr2 = buf_ptr + roffset;
			    	    if( ptr2 >= data->delay_len ) ptr2 -= data->delay_len;
			    	    cbuf[ ptr2 ] += in[ i ];
				}
				else
				{
			    	    cbuf[ buf_ptr ] += in[ i ];
				}
				buf_ptr++;
				if( buf_ptr >= data->delay_len ) buf_ptr = 0;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
			        PS_STYPE2 v = cbuf[ buf_ptr ];
			        out[ i ] = PS_NORM_STYPE_MUL( v, ctl_wet, 256 );
			        v = PS_NORM_STYPE_MUL( v, ctl_feedback, 256 );
			        psynth_denorm_add_white_noise( v );
			        cbuf[ buf_ptr ] = v;
			        buf_ptr++;
			        if( buf_ptr >= data->delay_len ) buf_ptr = 0;
			    }
			}
		    }
		    if( input_signal )
		    {
			if( data->ctl_dry > 0 && data->ctl_dry != 256 )
			{
			    for( int i = 0; i < frames; i++ )
				out[ i ] += PS_NORM_STYPE_MUL( in[ i ], ctl_dry, 256 );
			}
			if( data->ctl_dry == 256 )
			{
			    for( int i = 0; i < frames; i++ )
				out[ i ] += in[ i ];
			}
		    }
		}
		data->buf_ptr = buf_ptr;
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    switch( event->controller.ctl_num )
            {
		case 3:	
		    data->changed |= CHANGED_BUFSIZE;
		    break;
		case 5:	
		    data->ctl_delay_units = event->controller.ctl_val;
		    if( data->ctl_delay_units > DELAY_UNIT_MAX ) data->ctl_delay_units = DELAY_UNIT_MAX;
		    echo_change_ctl_limits( mod_num, pnet );
		    data->changed |= CHANGED_BUFSIZE;
		    retval = 1;
		    break;
		case 7:	
		case 8:	
		    data->changed |= CHANGED_FILTER;
		    break;
	    }
	    break;
	case PS_CMD_SPEED_CHANGED:
            data->tick_size = event->speed.tick_size;
            data->ticks_per_line = event->speed.ticks_per_line;
            if( data->ctl_delay_units >= 3 )
        	data->changed |= CHANGED_BUFSIZE;
            retval = 1;
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
