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
#define MODULE_DATA	psynth_vibrato_data
#define MODULE_HANDLER	psynth_vibrato
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define INTERP_PREC	14
#define INTERP_MASK	( ( 1 << INTERP_PREC ) - 1 )
#define INTERP( val1, val2, p )   ( ( val1 * (INTERP_MASK - ((p)&INTERP_MASK)) + val2 * ((p)&INTERP_MASK) ) / ( 1 << INTERP_PREC ) )
#define FREQ_UNIT_MAX	6
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_amp;
    PS_CTYPE   	ctl_freq;
    PS_CTYPE   	ctl_mono;
    PS_CTYPE   	ctl_set_phase;
    PS_CTYPE   	ctl_freq_units;
    PS_CTYPE	ctl_exp_amp;
    int	    	buf_size;
    PS_STYPE*  	buf[ MODULE_OUTPUTS ];
    int	    	buf_ptr;
    bool	buf_clean;
#ifndef PS_STYPE_FLOATINGPOINT
    uint16_t* 	sin_tab;
#endif
    uint    	ptr;
    int	    	empty_frames_counter;
    int     	tick_size; 
    uint8_t   	ticks_per_line;
};
static void vibrato_change_ctl_limits( int mod_num, psynth_net* pnet )
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
    int max = 2048;
    switch( data->ctl_freq_units )
    {
        case 1:
            max = 4000;
            break;
        case 2:
            max = 8192 * 2;
            break;
    }
    int cnum = 2;
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
#define SET_PHASE( CHECK_VERSION ) \
{ \
    if( ( CHECK_VERSION && pnet->base_host_version >= 0x01090300 ) || CHECK_VERSION == 0 ) \
    { \
        psynth_event e; \
        SMEM_CLEAR_STRUCT( e ); \
        e.command = PS_CMD_SET_GLOBAL_CONTROLLER; \
        e.controller.ctl_num = 4; \
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
	    retval = (PS_RETTYPE)"Vibrato";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Vibrato - модуль периодического изменения высоты тона входящего сигнала.\n"
                    	    STR_NOTE_RESET_LFO_TO_RU " \"Установ.фазу\".";
                        break;
                    }
		    retval = (PS_RETTYPE)"Vibrato is a musical effect consisting of a regular, pulsating change of pitch.\n"
			STR_NOTE_RESET_LFO_TO " \"Set phase\" value.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FFFC4";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 7, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 256, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_AMPLITUDE ), "", 0, 256, 16, 0, &data->ctl_amp, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ ), "", 1, 2048, 256, 0, &data->ctl_freq, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SET_PHASE ), "", 0, 256, 0, 0, &data->ctl_set_phase, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_UNIT ), ps_get_string( STR_PS_LFO_FREQ_UNITS ), 0, FREQ_UNIT_MAX, 0, 1, &data->ctl_freq_units, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_EXP_AMP ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_exp_amp, -1, 0, pnet );
	    data->buf_size = pnet->sampling_freq / 25;
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
	    {
		data->buf[ i ] = SMEM_ZALLOC2( PS_STYPE, data->buf_size );
	    }
	    data->buf_ptr = 0;
	    data->buf_clean = true;
	    data->ptr = 0;
#ifndef PS_STYPE_FLOATINGPOINT
	    data->sin_tab = (uint16_t*)psynth_get_sine_table( 2, false, 9, 256 * ( 1 << ( INTERP_PREC - 8 ) ) );
#endif
	    data->empty_frames_counter = data->buf_size;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
            vibrato_change_ctl_limits( mod_num, pnet );
	    SET_PHASE( 1 );
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    if( data->buf_clean == false )
	    {
		for( int i = 0; i < MODULE_OUTPUTS; i++ )
		{
		    smem_zero( data->buf[ i ] );
		}
		data->buf_clean = true;
	    }
	    data->buf_ptr = 0;
	    data->empty_frames_counter = data->buf_size;
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
		uint delta;
		switch( data->ctl_freq_units )
		{
		    case 0:
			delta = ( (unsigned)data->ctl_freq << 20 ) / pnet->sampling_freq;
			break;
		    case 1:
			delta = (uint64_t)( (uint64_t)( 64 << 20 ) * (uint64_t)1000 ) / ( data->ctl_freq * pnet->sampling_freq );
			break;
		    case 2:
			delta = (uint64_t)( ( (uint64_t)data->ctl_freq << 20 ) * 64 ) / pnet->sampling_freq;
			break;
		    case 3:
			delta = (uint64_t)( (uint64_t)( 64 << 20 ) * (uint64_t)256 ) / ( (unsigned)data->ctl_freq * data->tick_size );
			break;
		    case 4:
		    case 5:
		    case 6:
			delta = (uint64_t)( (uint64_t)( 64 << 20 ) * (uint64_t)256 ) / 
			    ( (uint64_t)data->ctl_freq * (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line / ( data->ctl_freq_units - 3 ) );
			break;
		}
		int outputs_num = psynth_get_number_of_outputs( mod );
		bool input_signal = false;
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
		    data->empty_frames_counter = 0;
		}
		else
		{
		    if( data->empty_frames_counter >= data->buf_size )
		    {
			data->ptr += delta * frames;
			break;
		    }
		    data->empty_frames_counter += frames;
		}
		data->buf_clean = false;
		int buf_size = data->buf_size;
		int buf_size_amp;
		if( data->ctl_exp_amp )
		{
		    buf_size_amp = ( data->buf_size * data->ctl_amp * data->ctl_amp ) / ( 256 * 256 );
		    if( data->ctl_amp > 0 && buf_size_amp == 0 )
			buf_size_amp = 1;
		}
		else
		    buf_size_amp = ( data->buf_size * data->ctl_amp ) / 256;
		if( buf_size_amp > 1 ) buf_size_amp--;
		int buf_ptr;
		uint ptr;
		PS_STYPE2 ctl_volume = PS_NORM_STYPE( data->ctl_volume, 256 );
#ifndef PS_STYPE_FLOATINGPOINT
		uint16_t* sin_tab = data->sin_tab;
#endif
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    PS_STYPE* cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    ptr = data->ptr;
		    for( int i = 0; i < frames; i++ )
		    {
		        cbuf[ buf_ptr ] = in[ i ];
		        PS_STYPE2 out_val;
#ifdef PS_STYPE_FLOATINGPOINT
			PS_STYPE p = (PS_STYPE)( ptr & ( ( 512 << 17 ) - 1 ) ) / ( 512 << 17 );
			PS_STYPE amp = ( PS_STYPE_SIN( p * 2 * M_PI ) + 1.0 ) / 2;
			PS_STYPE buf_ptr1 = (PS_STYPE)buf_ptr - amp * (PS_STYPE)buf_size_amp;
			if( buf_ptr1 < 0 ) buf_ptr1 += buf_size;
			if( buf_ptr1 >= buf_size ) buf_ptr1 -= buf_size; 
			int buf_ptr1_i = (int)buf_ptr1;
			PS_STYPE buf_c = buf_ptr1 - (PS_STYPE)buf_ptr1_i;
			int buf_ptr2_i = buf_ptr1_i + 1;
			if( buf_ptr2_i >= buf_size ) buf_ptr2_i = 0;
			out_val = cbuf[ buf_ptr1_i ] * ( 1 - buf_c ) + cbuf[ buf_ptr2_i ] * buf_c;
#else
			int p = (int)( ptr >> ( 17 - INTERP_PREC ) ) & INTERP_MASK;
			int amp = sin_tab[ ( ptr >> 17 ) & 511 ] * ( INTERP_MASK - p );
			amp += sin_tab[ ( ( ptr >> 17 ) + 1 ) & 511 ] * p;
			amp >>= INTERP_PREC;
		        int buf_ptr1 = buf_ptr * ( 1 << INTERP_PREC ) - amp * buf_size_amp;
			if( buf_ptr1 < 0 ) buf_ptr1 += buf_size * ( 1 << INTERP_PREC );
			int buf_ptr_1 = buf_ptr1 / ( 1 << INTERP_PREC );
			int buf_ptr_2 = buf_ptr_1 + 1;
			if( buf_ptr_2 >= buf_size ) buf_ptr_2 = 0;
			out_val = INTERP( cbuf[ buf_ptr_1 ], cbuf[ buf_ptr_2 ], buf_ptr1 );
#endif
			out_val = PS_NORM_STYPE_MUL( out_val, ctl_volume, 256 );
			out[ i ] = out_val;
			buf_ptr++;
			if( buf_ptr >= buf_size ) buf_ptr = 0;
			ptr += delta;
		    }
		}
		data->buf_ptr = buf_ptr;
		data->ptr = ptr;
		retval = 1;
		if( data->ctl_volume == 0 ) retval = 2;
		break;
	    }
#ifdef SUNVOX_GUI
            {
                psynth_ctl* ctl = &mod->ctls[ 4 ];
                int new_normal_val = ( data->ptr >> 18 ) & 255;
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
	    if( event->controller.ctl_num == 4 )
	    {
		data->ptr = ( event->controller.ctl_val >> 6 ) << 17;
	    }
	    if( event->controller.ctl_num == 5 )
            {
                data->ctl_freq_units = event->controller.ctl_val;
                if( data->ctl_freq_units > FREQ_UNIT_MAX ) data->ctl_freq_units = FREQ_UNIT_MAX;
                vibrato_change_ctl_limits( mod_num, pnet );
                retval = 1;
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
