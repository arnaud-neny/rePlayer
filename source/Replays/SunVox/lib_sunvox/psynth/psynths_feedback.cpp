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
#define MODULE_DATA	psynth_feedback_data
#define MODULE_HANDLER	psynth_feedback
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE	ctl_mono;
    PS_STYPE*	buf; 
    uint	buf_size;
    uint 	buf_wp;
    uint	buf_wp_start;
    bool        buf_clean;
};
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
	    retval = (PS_RETTYPE)"Feedback";
	    break;
	case PS_CMD_GET_INFO:
	    {
		const char* lang = slocale_get_lang();
	        while( 1 )
	        {
	            if( smem_strstr( lang, "ru_" ) )
	            {
			retval = (PS_RETTYPE)"Feedback - модуль обратной связи.\nSunVox запрещает подключение модулей в бесконечную петлю (выход идет на вход), но это можно сделать, если пропустить петлю через два последовательных модуля Feedback (см.примеры).\nЗадержка внутри Feedback = 20 мс";
			break;
		    }
		    retval = (PS_RETTYPE)"Feedback.\nGenerally the feedback is not allowed in SunVox - you can't create an endless loop between the modules. But you can do it by placing two Feedback modules inside of the loop (see examples).\nInternal Feedback delay = 20 ms";
		    break;
		}
	    }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF0000";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_RENDER_SETUP_COMMANDS | PSYNTH_FLAG_FEEDBACK; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 10000, 1000, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 0, pnet );
	    data->buf_size = (int)round_to_power_of_two( pnet->max_buf_size * 2 );
	    data->buf = SMEM_ZALLOC2( PS_STYPE, data->buf_size * MODULE_OUTPUTS );
	    data->buf_wp = 0;
	    data->buf_wp_start = 0;
	    data->buf_clean = true;
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    if( data->buf_clean == false )
	    {
		smem_zero( data->buf );
		data->buf_clean = true;
	    }
	    retval = 1;
	    break;
	case PS_CMD_RENDER_SETUP:
	    data->buf_wp_start = data->buf_wp;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		bool receiver = false;
		int input_mod_num = -1;
		psynth_module* input_mod = 0;
                for( int i = 0; i < mod->input_links_num; i++ )
                {
            	    input_mod_num = mod->input_links[ i ];
            	    if( (unsigned)input_mod_num < pnet->mods_num )
            	    {
                	input_mod = &pnet->mods[ input_mod_num ];
                	if( input_mod->handler == MODULE_HANDLER )
                	{
                	    receiver = true;
                	    break;
                	}
            	    }
                }
                bool mono = data->ctl_mono;
                int vol = data->ctl_volume;
		if( receiver )
		{
		    MODULE_DATA* input_data = (MODULE_DATA*)input_mod->data_ptr;
		    if( input_data->ctl_mono ) mono = true;
		    vol = ( vol * input_data->ctl_volume ) / 10000;
		}
	        if( mono )
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
		if( receiver )
		{
		    int outputs_num = psynth_get_number_of_outputs( input_mod_num, pnet );
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			PS_STYPE* out = outputs[ ch ] + offset;
			MODULE_DATA* input_data = (MODULE_DATA*)input_mod->data_ptr;
			PS_STYPE* buf = input_data->buf + input_data->buf_size * ch;
			uint rp = input_data->buf_wp_start + offset - pnet->max_buf_size;
			rp &= ( input_data->buf_size - 1 );
			if( vol != 0 )
			{
			    if( vol == 10000 )
			    {
				for( int i = 0; i < frames; i++ )
				{
				    out[ i ] = buf[ rp ];
				    rp = ( rp + 1 ) & ( input_data->buf_size - 1 );
				}
			    }
			    else
			    {
#ifdef PS_STYPE_FLOATINGPOINT
				PS_STYPE fvol = (PS_STYPE)vol / 10000.0F;
				for( int i = 0; i < frames; i++ )
				{
				    out[ i ] = buf[ rp ] * fvol;
				    rp = ( rp + 1 ) & ( input_data->buf_size - 1 );
				}
#else
				int vol2 = ( vol * 32768 ) / 10000;
				for( int i = 0; i < frames; i++ )
				{
				    PS_STYPE2 v = buf[ rp ];
				    v = ( v * vol2 ) >> 15;
				    out[ i ] = v;
				    rp = ( rp + 1 ) & ( input_data->buf_size - 1 );
				}
#endif
			    }
			    retval = 3;
			} 
		    }
		}
		else
		{
		    int inputs_num = psynth_get_number_of_inputs( mod_num, pnet );
		    uint wp = data->buf_wp;
		    for( int ch = 0; ch < inputs_num; ch++ )
		    {
			PS_STYPE* in = inputs[ ch ] + offset;
			PS_STYPE* buf = data->buf + data->buf_size * ch;
			bool input_signal = false;
			if( mod->in_empty[ ch ] < offset + frames ) 
			{
			    data->buf_clean = false;
			    input_signal = true;
			}
			wp = data->buf_wp;
			if( input_signal == false && data->buf_clean )
			{
			    wp = ( wp + frames ) & ( data->buf_size - 1 );
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
				buf[ wp ] = in[ i ];
				wp = ( wp + 1 ) & ( data->buf_size - 1 );
			    }
			}
		    }
		    data->buf_wp = wp;
		}
	    }
	    break;
	case PS_CMD_CLOSE:
	    smem_free( data->buf );
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
