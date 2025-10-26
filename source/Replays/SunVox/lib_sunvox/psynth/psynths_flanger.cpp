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
#define MODULE_DATA	psynth_flanger_data
#define MODULE_HANDLER	psynth_flanger
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
struct MODULE_DATA
{
    PS_CTYPE   	ctl_dry;
    PS_CTYPE   	ctl_wet;
    PS_CTYPE   	ctl_feedback;
    PS_CTYPE   	ctl_delay; 
    PS_CTYPE   	ctl_response; 
    PS_CTYPE   	ctl_vibrato_speed;
    PS_CTYPE   	ctl_vibrato_amp;
    PS_CTYPE   	ctl_vibrato_type;
    PS_CTYPE   	ctl_set_vibrato_phase;
    PS_CTYPE   	ctl_vibrato_speed_units;
    int	    	buf_size;
    PS_STYPE*  	buf[ MODULE_OUTPUTS ];
    int	    	buf_ptr;
    bool	buf_empty;
    int	    	tick_counter; 
    PS_CTYPE   	floating_delay; 
    PS_CTYPE   	prev_floating_delay;
    uint    	vibrato_ptr; 
    uint8_t*  	vibrato_tab;
    bool    	empty;
    int     	tick_size; 
    uint8_t   	ticks_per_line;
};
#define RESP_PREC		    19
#define PREC			    10
#define MASK			    1023
#define SET_PHASE( CHECK_VERSION ) \
{ \
    if( ( CHECK_VERSION && pnet->base_host_version >= 0x01090300 ) || CHECK_VERSION == 0 ) \
    { \
        psynth_event e; \
        SMEM_CLEAR_STRUCT( e ); \
        e.command = PS_CMD_SET_GLOBAL_CONTROLLER; \
        e.controller.ctl_num = 8; \
        e.controller.ctl_val = data->ctl_set_vibrato_phase << 7; \
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
	    retval = (PS_RETTYPE)"Flanger";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Фланжер.\nМаксимальная длина задержки = 1/64 секунды.\n"
                    	    STR_NOTE_RESET_LFO_TO_RU " \"Установ.фазу LFO\".";
                        break;
                    }
		    retval = (PS_RETTYPE)"Flanger.\nMax delay is 1/64 second.\nUse lower \"response\" values for smooth change of the Flanger parameters.\n"
			STR_NOTE_RESET_LFO_TO " \"Set LFO phase\" value.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#9C7FFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 10, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DRY ), "", 0, 256, 256, 0, &data->ctl_dry, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WET ), "", 0, 256, 128, 0, &data->ctl_wet, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 256, 128, 0, &data->ctl_feedback, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY ), "", 8, 1000, 200, 0, &data->ctl_delay, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESPONSE ), "", 0, 256, 2, 0, &data->ctl_response, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ ), "", 0, 512, 8, 0, &data->ctl_vibrato_speed, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_AMP ), "", 0, 256, 32, 0, &data->ctl_vibrato_amp, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_WAVEFORM ), "hsin;sin", 0, 1, 0, 1, &data->ctl_vibrato_type, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SET_LFO_PHASE ), "", 0, 256, 0, 0, &data->ctl_set_vibrato_phase, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LFO_FREQ_UNIT ), ps_get_string( STR_PS_FLANGER_LFO_FREQ_UNITS ), 0, 6, 0, 1, &data->ctl_vibrato_speed_units, -1, 2, pnet );
	    data->buf_size = pnet->sampling_freq / 64;
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
	    {
		data->buf[ i ] = SMEM_ZALLOC2( PS_STYPE, data->buf_size );
	    }
	    data->buf_ptr = 0;
	    data->buf_empty = true;
	    data->floating_delay = data->ctl_delay << RESP_PREC;
	    data->tick_counter = 0;
	    data->vibrato_ptr = 0;
	    data->vibrato_tab = g_hsin_tab;
	    data->empty = 1;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    SET_PHASE( 1 );
	case PS_CMD_APPLY_CONTROLLERS:
	    data->floating_delay = data->ctl_delay << RESP_PREC;
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    if( data->buf_empty == false )
	    {
		for( int i = 0; i < MODULE_OUTPUTS; i++ )
		{
	    	    smem_zero( data->buf[ i ] );
		}
		data->buf_empty = true;
	    }
	    data->buf_ptr = 0;
	    data->tick_counter = 0;
	    data->vibrato_ptr = 0;
	    data->empty = true;
	    SET_PHASE( 1 );
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
		data->buf_empty = false;
		int tick_size;
		switch( data->ctl_vibrato_speed_units )
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
		uint32_t floating_delay_step = ( data->ctl_response << 2 ) * ( ( 256 << RESP_PREC ) / (uint)pnet->sampling_freq );
		PS_STYPE2 ctl_dry = PS_NORM_STYPE( data->ctl_dry, 256 );
	        PS_STYPE2 ctl_wet = PS_NORM_STYPE( data->ctl_wet, 256 );
                PS_STYPE2 ctl_feedback = PS_NORM_STYPE( data->ctl_feedback, 256 );
		int ptr = 0;
		while( 1 )
		{
		    int new_sample_frames = frames - ptr;
    		    if( new_sample_frames > ( tick_size - data->tick_counter ) / 256 ) new_sample_frames = ( tick_size - data->tick_counter ) / 256;
    		    if( ( tick_size - data->tick_counter ) & 255 ) new_sample_frames++; 
    		    if( new_sample_frames > frames - ptr ) new_sample_frames = frames - ptr;
    		    if( new_sample_frames < 0 ) new_sample_frames = 0;
		    uint vib_ptr = 0;
		    switch( data->ctl_vibrato_speed_units )
            	    {
                	case 0:
                	    vib_ptr = ( data->vibrato_ptr / 8 ) & 511;
                	    break;
                	case 1:
                	case 2:
                	    vib_ptr = ( data->vibrato_ptr / 1000 ) & 511;
                	    break;
                	case 3:
                	    vib_ptr = ( data->vibrato_ptr / data->tick_size ) & 511;
                	    break;
                	case 4:
                	case 5:
                	case 6:
                	    vib_ptr = ( data->vibrato_ptr / ( ( data->tick_size * data->ticks_per_line ) / 8 ) ) & 511;
                	    break;
            	    }
		    int vib_value = 0;
		    if( data->ctl_vibrato_type == 0 )
		    {
			vib_value = (int)data->vibrato_tab[ vib_ptr / 2 ] * 2 - 256;
		    }
		    else if( data->ctl_vibrato_type == 1 )
		    {
			if( vib_ptr < 256 )
			    vib_value = (int)data->vibrato_tab[ vib_ptr ];
			else
			    vib_value = -(int)data->vibrato_tab[ vib_ptr & 255 ];
		    }
		    vib_value *= data->ctl_vibrato_amp;
		    vib_value /= 256;
		    int cur_delay = data->ctl_delay;
		    if( cur_delay - data->ctl_vibrato_amp < 8 )
			cur_delay = 8 + data->ctl_vibrato_amp;
		    if( cur_delay + data->ctl_vibrato_amp > 1000 )
			cur_delay = 1000 - data->ctl_vibrato_amp;
		    cur_delay += vib_value;
		    int buf_ptr = 0;
		    int floating_delay = 0;
		    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		    {
			PS_STYPE* in = inputs[ ch ] + offset;
			PS_STYPE* out = outputs[ ch ] + offset;
			PS_STYPE* cbuf = data->buf[ ch ];
			buf_ptr = data->buf_ptr;
			floating_delay = data->floating_delay;
			if( input_signal )
			{
			    for( int i = ptr; i < ptr + new_sample_frames; i++ )
			    {
				int sub_fp = ( floating_delay >> (RESP_PREC-5) ) * data->buf_size;
				int sub = sub_fp / 32768;
				int ptr2 = buf_ptr - sub;
				if( ptr2 < 0 ) ptr2 += data->buf_size;
				int ptr3 = buf_ptr - ( sub + 1 );
				if( ptr3 < 0 ) ptr3 += data->buf_size;
				int c = sub_fp & 32767;
				PS_STYPE2 buf_val = ( cbuf[ ptr2 ] * (PS_STYPE2)(32768-c) +  cbuf[ ptr3 ] * c ) / (PS_STYPE2)32768;
				PS_STYPE2 out_val = buf_val;
				out_val = PS_NORM_STYPE_MUL( out_val, ctl_wet, 256 );
				out[ i ] = out_val;
				out_val = in[ i ];
				out_val = PS_NORM_STYPE_MUL( out_val, ctl_dry, 256 );
				out[ i ] += out_val;
				out_val = buf_val;
				out_val = PS_NORM_STYPE_MUL( out_val, ctl_feedback, 256 );
				psynth_denorm_add_white_noise( out_val );
				cbuf[ buf_ptr ] = in[ i ] + out_val;
				buf_ptr++;
				if( buf_ptr >= data->buf_size ) buf_ptr = 0;
				if( ( floating_delay >> RESP_PREC ) > cur_delay )
				{
				    floating_delay -= floating_delay_step;
				    if( ( floating_delay >> RESP_PREC ) < cur_delay )
					floating_delay = cur_delay << RESP_PREC;
				}
				else
				if( ( floating_delay >> RESP_PREC ) < cur_delay )
				{
				    floating_delay += floating_delay_step;
				    if( ( floating_delay >> RESP_PREC ) > cur_delay )
					floating_delay = cur_delay << RESP_PREC;
				}
			    }
			}
			else
			{
			    for( int i = ptr; i < ptr + new_sample_frames; i++ )
			    {
				int sub_fp = ( floating_delay >> (RESP_PREC-5) ) * data->buf_size;
				int sub = sub_fp / 32768;
				int ptr2 = buf_ptr - sub;
				if( ptr2 < 0 ) ptr2 += data->buf_size;
				int ptr3 = buf_ptr - ( sub + 1 );
				if( ptr3 < 0 ) ptr3 += data->buf_size;
				int c = sub_fp & 32767;
				PS_STYPE2 buf_val = ( cbuf[ ptr2 ] * (PS_STYPE2)(32768-c) +  cbuf[ ptr3 ] * c ) / (PS_STYPE2)32768;
				PS_STYPE2 out_val = buf_val;
				out_val = PS_NORM_STYPE_MUL( out_val, ctl_wet, 256 );
				out[ i ] = out_val;
				out_val = buf_val;
				out_val = PS_NORM_STYPE_MUL( out_val, ctl_feedback, 256 );
				psynth_denorm_add_white_noise( out_val );
				cbuf[ buf_ptr ] = out_val;
				buf_ptr++;
				if( buf_ptr >= data->buf_size ) buf_ptr = 0;
				if( ( floating_delay >> RESP_PREC ) > cur_delay )
				{
				    floating_delay -= floating_delay_step;
				    if( ( floating_delay >> RESP_PREC ) < cur_delay )
					floating_delay = cur_delay << RESP_PREC;
				}
				else
				    if( ( floating_delay >> RESP_PREC ) < cur_delay )
				{
				    floating_delay += floating_delay_step;
				    if( ( floating_delay >> RESP_PREC ) > cur_delay )
					floating_delay = cur_delay << RESP_PREC;
				}
			    }
			}
		    }
		    data->buf_ptr = buf_ptr;
		    data->floating_delay = floating_delay;
		    ptr += new_sample_frames;
		    data->tick_counter += new_sample_frames * 256;
		    if( data->tick_counter >= tick_size ) 
		    {
			switch( data->ctl_vibrato_speed_units )
			{
			    case 0:
				data->vibrato_ptr += data->ctl_vibrato_speed;
				data->vibrato_ptr &= 4095;
				break;
			    case 1:
				if( data->ctl_vibrato_speed > 0 )
				{
				    data->vibrato_ptr += ( 1000 * 512 ) / data->ctl_vibrato_speed;
				    data->vibrato_ptr %= ( 1000 * 512 );
				}
				break;
			    case 2:
				data->vibrato_ptr += data->ctl_vibrato_speed * 512;
				data->vibrato_ptr %= 1000 * 512;
				break;
			    case 3:
				if( data->ctl_vibrato_speed )
				{
				    data->vibrato_ptr += ( tick_size * 512 ) / data->ctl_vibrato_speed;
				    data->vibrato_ptr %= ( data->tick_size * 512 );
				}
				break;
			    case 4:
			    case 5:
			    case 6:
				if( data->ctl_vibrato_speed )
				{
				    data->vibrato_ptr += ( ( ( tick_size / 8 ) * 512 ) / data->ctl_vibrato_speed ) * ( data->ctl_vibrato_speed_units - 3 );
				    data->vibrato_ptr %= ( ( ( data->tick_size * data->ticks_per_line ) / 8 ) * 512 );
				}
				break;
			}
			data->tick_counter %= tick_size;
#ifdef SUNVOX_GUI
                        {
                            psynth_ctl* ctl = &mod->ctls[ 8 ];
                            uint bits = 0;
                            uint div = 1;
                            switch( data->ctl_vibrato_speed_units )
                            {
                                case 0:
                                    bits = 12;
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
                            int new_normal_val = data->vibrato_ptr;
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
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    if( event->controller.ctl_num == 8 )
	    {
		switch( data->ctl_vibrato_speed_units )
                {
                    case 0:
                        data->vibrato_ptr = ( event->controller.ctl_val / 8 ) & 4095;
                        break;
                    case 1:
                    case 2:
                        data->vibrato_ptr = ( ( event->controller.ctl_val / 32 ) * ( 1000 * 512 ) ) / 1024;
                        break;
                    case 3:
                        data->vibrato_ptr = ( ( ( event->controller.ctl_val / 32 ) * data->tick_size ) / 1024 ) * 512;
                        break;
                    case 4:
                    case 5:
                    case 6:
                        data->vibrato_ptr = ( ( ( event->controller.ctl_val / 32 ) * ( ( data->tick_size * data->ticks_per_line ) / 8 ) ) / 1024 ) * 512;
                        break;
                }
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
