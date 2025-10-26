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
#define MODULE_DATA	psynth_pitch_shifter_data
#define MODULE_HANDLER	psynth_pitch_shifter
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define PITCH_BITS 			15
#define PITCH_SEMITONE			10
#define PITCH_OCTAVES			5
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE	ctl_volume;
    PS_CTYPE	ctl_pitch;
    PS_CTYPE	ctl_pitch_scale;
    PS_CTYPE	ctl_feedback;
    PS_CTYPE	ctl_grain_size;
    PS_CTYPE	ctl_mode;
    PS_CTYPE	ctl_ret;
    uint*       linear_freq_tab;
    int		max_grain_size;
    int		min_grain_size;
    int	    	buf_size;
    PS_STYPE*	buf[ MODULE_OUTPUTS ];
    int	    	buf_ptr;
    bool	buf_clean;
    int64_t	out_ptr;
    int64_t	out_delta; 
    int		mix; 
    int		mix_step;
    int	    	empty_frames_counter;
    bool	recalc_req;
};
static void recalc_delta( MODULE_DATA* data )
{
    int base_freq;
    int freq;
    int pitch = data->ctl_pitch - PITCH_OCTAVES * 12 * PITCH_SEMITONE;
    int scale = data->ctl_pitch_scale;
    if( scale < 1 ) scale = 1;
    PSYNTH_GET_FREQ( data->linear_freq_tab, base_freq, PITCH_OCTAVES * 12 * 64 );
    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, PITCH_OCTAVES * 12 * 64 - ( ( pitch * data->ctl_pitch_scale * 64 ) / 100 ) / PITCH_SEMITONE );
    data->out_delta = ( (int64_t)freq << PITCH_BITS ) / (int64_t)base_freq;
    data->out_delta -= 1 << PITCH_BITS;
}
static void set_mix( MODULE_DATA* data )
{
    data->mix = 32768 << 15;
    if( data->ctl_ret && data->out_delta == 0 && data->ctl_feedback == 0 )
	data->mix = 0;
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
	    retval = (PS_RETTYPE)"Pitch shifter";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Pitch shifter - модуль для изменения тональности любого звука в реальном времени.\nПолутон = 10. Октава = 120.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Pitch shifter is a module for changing the pitch of any sound in real time.\nSemitone = 10. Octave = 120.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFD800";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 7, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PITCH ), ps_get_string( STR_PS_SEMITONE10 ), 0, PITCH_OCTAVES * 12 * PITCH_SEMITONE * 2, PITCH_OCTAVES * 12 * PITCH_SEMITONE, 0, &data->ctl_pitch, PITCH_OCTAVES * 12 * PITCH_SEMITONE, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 1, -( PITCH_OCTAVES * 12 * PITCH_SEMITONE ), pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PITCH_SCALE ), "%", 0, 200, 100, 0, &data->ctl_pitch_scale, 100, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 256, 0, 0, &data->ctl_feedback, 0, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_GRAIN_SIZE ), "", 0, 256, 64, 0, &data->ctl_grain_size, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODE_LQ_MONO, MODE_HQ, 1, &data->ctl_mode, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PLAY_ORIGINAL_IF_PITCH0 ), ps_get_string( STR_PS_PITCHSHIFTER_PLAYORIG_MODES ), 0, 2, 0, 1, &data->ctl_ret, 0, 1, pnet );
	    data->linear_freq_tab = g_linear_freq_tab;
	    data->max_grain_size = pnet->sampling_freq / 4;
	    data->min_grain_size = pnet->sampling_freq / 5512;
	    data->buf_size = round_to_power_of_two( data->max_grain_size * 2 );
	    for( int i = 0; i < MODULE_OUTPUTS; i++ ) data->buf[ i ] = SMEM_ZALLOC2( PS_STYPE, data->buf_size );
	    data->buf_ptr = 0;
	    data->buf_clean = true;
	    data->out_ptr = 0;
	    data->out_delta = 0;
	    data->mix_step = ( 32768 << 15 ) / pnet->sampling_freq; 
	    data->empty_frames_counter = data->buf_size;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    recalc_delta( data );
	    set_mix( data );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    recalc_delta( data );
	    set_mix( data );
	    data->buf_ptr = 0;
	    data->out_ptr = 0;
	    if( data->buf_clean == false )
	    {
		for( int i = 0; i < MODULE_OUTPUTS; i++ ) smem_zero( data->buf[ i ] );
		data->buf_clean = true;
	    }
	    data->empty_frames_counter = data->buf_size + 1;
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
		    if( data->ctl_feedback == 0 )
		    {
			if( data->empty_frames_counter >= data->buf_size )
			{
			    if( data->empty_frames_counter < 10000000 )
			    {
				data->out_ptr = 0;
				data->buf_clean = true;
				set_mix( data );
				data->empty_frames_counter = 10000000;
			    }
			    break;
			}
			data->empty_frames_counter += frames;
		    }
		}
		data->buf_clean = false;
		if( data->recalc_req )
		{
		    recalc_delta( data );
		    data->recalc_req = false;
		}
		int len = ( data->ctl_grain_size * data->max_grain_size ) / 256;
		if( len < data->min_grain_size ) len = data->min_grain_size;
		int len2 = len / 2;
		int64_t len3 = (int64_t)len2 << PITCH_BITS;
		uint buf_size_mask = data->buf_size - 1;
		int mix_target = 32768 << 15;
		int mix_step = data->mix_step;
            	if( data->ctl_ret )
            	{
            	    if( data->out_delta == 0 && data->ctl_feedback == 0 ) mix_target = 0;
            	    if( data->ctl_ret == 2 ) mix_step *= 4; 
            	}
		int ptr = data->buf_ptr;
		int64_t out_ptr = data->out_ptr;
		int mix = data->mix;
		for( int ch = 0; ch < outputs_num; ch++ )
            	{
            	    PS_STYPE* in = inputs[ ch ] + offset;
            	    PS_STYPE* out = outputs[ ch ] + offset;
    		    ptr = data->buf_ptr;
    		    out_ptr = data->out_ptr;
    		    mix = data->mix;
    		    PS_STYPE* buf = data->buf[ ch ];
		    if( data->ctl_mode <= MODE_HQ_MONO )
		    {
			for( int i = 0; i < frames; i++ )
                	{
                    	    buf[ ptr ] = in[ i ];
                   	    if( out_ptr > 0 )
                    	    {
                    		out_ptr %= len3;
                    	    }
                    	    else
                    	    {
                    		while( out_ptr < 0 ) out_ptr += len3;
                    	    }
                    	    int out_ptr2 = out_ptr >> PITCH_BITS;
                    	    int out_ptr3 = ( ptr - len2 + out_ptr2 ) & buf_size_mask;
                    	    int out_ptr4 = ( out_ptr3 - len2 ) & buf_size_mask;
                    	    int c1 = out_ptr & ( ( 1 << PITCH_BITS ) - 1 );
                    	    int c2 = ( 1 << PITCH_BITS ) - c1;
                    	    PS_STYPE2 val1 = ( buf[ out_ptr3 ] * c2 + buf[ ( out_ptr3 + 1 ) & buf_size_mask ] * c1 ) / ( 1 << PITCH_BITS );
                    	    PS_STYPE2 val2 = ( buf[ out_ptr4 ] * c2 + buf[ ( out_ptr4 + 1 ) & buf_size_mask ] * c1 ) / ( 1 << PITCH_BITS );
                    	    c1 = ( out_ptr2 << 15 ) / len2;
                    	    c2 = 32768 - c1;
                    	    PS_STYPE2 val = ( val1 * c2 + val2 * c1 ) / 32768;
                    	    out[ i ] = val;
                    	    if( data->ctl_feedback )
                    	    {
                    	        val = ( val * data->ctl_feedback ) / 256;
                    	        buf[ ptr ] += val;
                    	    }
                    	    ptr++; 
                    	    ptr &= buf_size_mask;
                    	    out_ptr += data->out_delta;
                    	}
                    }
                    else
                    {
			for( int i = 0; i < frames; i++ )
                	{
                    	    buf[ ptr ] = in[ i ];
                    	    if( out_ptr > 0 )
                    	    {
                    		out_ptr %= len3;
                    	    }
                    	    else
                    	    {
                    		while( out_ptr < 0 ) out_ptr += len3;
                    	    }
                    	    int out_ptr2 = out_ptr >> PITCH_BITS;
                    	    int out_ptr3 = ( ptr - len2 + out_ptr2 ) & buf_size_mask;
                    	    int out_ptr4 = ( out_ptr3 - len2 ) & buf_size_mask;
                    	    int c1 = ( out_ptr2 << 15 ) / len2;
                    	    int c2 = 32768 - c1;
                    	    PS_STYPE2 val = ( buf[ out_ptr3 ] * c2 + buf[ out_ptr4 ] * c1 ) / 32768;
                    	    out[ i ] = val;
                    	    if( data->ctl_feedback )
                    	    {
                    	        val = ( val * data->ctl_feedback ) / 256;
                    	        buf[ ptr ] += val;
                    	    }
                    	    ptr++;
                    	    ptr &= buf_size_mask;
                    	    out_ptr += data->out_delta;
                    	}
            	    }
            	    if( data->ctl_volume != 256 )
            	    {
            		for( int i = 0; i < frames; i++ )
                	{
                	    PS_STYPE2 val = out[ i ];
                	    val = ( val * data->ctl_volume ) / 256;
                	    out[ i ] = val;
                	}
            	    }
            	    if( mix == mix_target )
            	    {
            		if( mix == 0 )
            		    smem_copy( out, in, frames * sizeof( PS_STYPE ) );
            		else
            		{
            		    if( mix < 32768 << 15 )
            		    {
            			for( int i = 0; i < frames; i++ )
                		{
                		    PS_STYPE2 val =
                			(PS_STYPE2)out[ i ] * ( mix >> 15 ) / 32768 + 
                			(PS_STYPE2)in[ i ] * ( 32768 - ( mix >> 15 ) ) / 32768;
                		    out[ i ] = val;
                		}
            		    }
            		}
            	    }
            	    else
            	    {
            		for( int i = 0; i < frames; i++ )
                	{
                	    PS_STYPE2 val =
                		(PS_STYPE2)out[ i ] * ( mix >> 15 ) / 32768 + 
                		(PS_STYPE2)in[ i ] * ( 32768 - ( mix >> 15 ) ) / 32768;
                	    out[ i ] = val;
            		    if( mix < mix_target )
            		    {
            			mix += mix_step;
            			if( mix >= mix_target ) mix = mix_target;
            		    }
            		    else
            		    {
            			mix -= mix_step;
            			if( mix < mix_target ) mix = mix_target;
            		    }
                	}
            	    }
            	}
            	data->buf_ptr = ptr;
            	data->out_ptr = out_ptr;
            	data->mix = mix;
            	retval = 1;
            	if( data->ctl_volume == 0 ) retval = 2;
	    }
	    break;
	case PS_CMD_NOTE_ON:
	case PS_CMD_SET_FREQ:
	    {
		int scale = data->ctl_pitch_scale;
	        if( scale < 1 ) scale = 1;
		int pitch = ( PS_NOTE_TO_PITCH( 5 * 12 ) - event->note.pitch ) * PITCH_SEMITONE * 100 / scale / 256;
		pitch += PITCH_OCTAVES * 12 * PITCH_SEMITONE;
		if( data->ctl_pitch != pitch )
		{
		    data->ctl_pitch = pitch;
    		    data->recalc_req = true;
#ifdef SUNVOX_GUI
            	    mod->draw_request++;
#endif
		}
	    }
            retval = 1;
            break;
        case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
    	    switch( event->controller.ctl_num )
    	    {
    		case 1: case 2: 
    		    data->recalc_req = true;
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
