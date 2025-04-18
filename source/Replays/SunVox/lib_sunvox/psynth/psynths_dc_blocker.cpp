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
#include "psynths_dc_blocker.h"
#define MODULE_DATA	psynth_dc_blocker_data
#define MODULE_HANDLER	psynth_dc_blocker
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
struct MODULE_DATA
{
    PS_CTYPE		ctl_mono;
    dc_filter		f;
};
void dc_filter_init( dc_filter* f, int sample_rate )
{
    float dc_block_freq = 5.0F;
    float dc_block_pole = ( ( 2 * M_PI ) * dc_block_freq ) / (float)sample_rate;
#ifdef PS_STYPE_FLOATINGPOINT
    f->dc_block_pole = (PS_STYPE2)1.0 - dc_block_pole;
#else
    f->dc_block_pole = (PS_STYPE2)( 32768.0F * dc_block_pole );
#endif
#ifdef PS_STYPE_FLOATINGPOINT
    f->empty_frames_counter_max = sample_rate;
#else
    f->empty_frames_counter_max = sample_rate / 2;
#endif
    dc_filter_stop( f );
}
void dc_filter_stop( dc_filter* f )
{
    for( int i = 0; i < DC_FILTER_CHANNELS; i++ )
    {
        f->dc_block_acc[ i ] = 0;
	f->dc_block_prev_x[ i ] = 0;
        f->dc_block_prev_y[ i ] = 0;
	f->empty_samples_counter[ i ] = f->empty_frames_counter_max;
    }
}
bool dc_filter_run( dc_filter* f, int ch, PS_STYPE* in, PS_STYPE* out, int frames )
{
    PS_STYPE2 dc_block_pole = f->dc_block_pole;
    PS_STYPE2 dc_block_acc = f->dc_block_acc[ ch ];
    PS_STYPE2 dc_block_prev_x = f->dc_block_prev_x[ ch ];
    PS_STYPE2 dc_block_prev_y = f->dc_block_prev_y[ ch ];
    if( in )
    {
	f->empty_samples_counter[ ch ] = 0;
	for( int i = 0; i < frames; i++ )
	{
    	    PS_STYPE2 input = in[ i ];
#ifdef PS_STYPE_FLOATINGPOINT
    	    psynth_denorm_add_white_noise( input );
    	    dc_block_prev_y = input - dc_block_prev_x + dc_block_prev_y * dc_block_pole;
    	    dc_block_prev_x = input;
#else
    	    dc_block_acc -= dc_block_prev_x;
    	    dc_block_prev_x = input << 15;
    	    dc_block_acc += dc_block_prev_x;
    	    dc_block_acc -= dc_block_pole * dc_block_prev_y;
    	    dc_block_prev_y = dc_block_acc >> 15; 
#endif
    	    out[ i ] = dc_block_prev_y;
    	}
    }
    else
    {
	if( f->empty_samples_counter[ ch ] >= f->empty_frames_counter_max )
	{
	    return true;
	}
	else
	{
	    f->empty_samples_counter[ ch ] += frames;
	    for( int i = 0; i < frames; i++ )
	    {
#ifdef DENORMAL_NUMBERS
    		PS_STYPE2 input = 0;
    		psynth_denorm_add_white_noise( input );
#else
    		const PS_STYPE2 input = 0;
#endif
#ifdef PS_STYPE_FLOATINGPOINT
    		dc_block_prev_y = input - dc_block_prev_x + dc_block_prev_y * dc_block_pole;
    		dc_block_prev_x = input;
#else
    		dc_block_acc -= dc_block_prev_x;
    		dc_block_prev_x = input << 15;
    		dc_block_acc += dc_block_prev_x;
    		dc_block_acc -= dc_block_pole * dc_block_prev_y;
    		dc_block_prev_y = dc_block_acc >> 15; 
#endif
    		out[ i ] = dc_block_prev_y;
    	    }
	}
    }
    f->dc_block_acc[ ch ] = dc_block_acc;
    f->dc_block_prev_x[ ch ] = dc_block_prev_x;
    f->dc_block_prev_y[ ch ] = dc_block_prev_y;
    return false;
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
    int ch;
    switch( event->command )
    {
	case PS_CMD_GET_DATA_SIZE:
	    retval = sizeof( MODULE_DATA );
	    break;
	case PS_CMD_GET_NAME:
	    retval = (PS_RETTYPE)"DC Blocker";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Фильтр, убирающий DC смещение сигнала. Применяйте его, если на осциллографе видите, что центр сигнала явно смещен вверх или вниз.";
                        break;
                    }
		    retval = (PS_RETTYPE)"DC Blocking Filter";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFC77F";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet );
	    dc_filter_init( &data->f, pnet->sampling_freq );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    dc_filter_stop( &data->f );
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
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
		dc_filter* f = &data->f;
		bool input_signal = false;
		for( ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			input_signal = true;
			break;
		    }
		}
		if( input_signal == false )
		{
		    if( f->empty_samples_counter[ 0 ] >= f->empty_frames_counter_max )
			break;
		}
		for( ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    if( input_signal )
			in = inputs[ ch ] + offset;
		    else
			in = 0;
		    dc_filter_run( f, ch, in, out, frames );
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
