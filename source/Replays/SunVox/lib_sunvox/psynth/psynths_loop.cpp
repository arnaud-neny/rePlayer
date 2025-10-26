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
#define MODULE_DATA	psynth_loop_data
#define MODULE_HANDLER	psynth_loop
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define CHANGED_BUFSIZE         ( 1 << 0 )
#define CHANGED_IOCHANNELS	( 1 << 1 )
#define LEN_UNIT_MAX	6
struct MODULE_DATA
{
    PS_CTYPE	ctl_volume;
    PS_CTYPE   	ctl_len;
    PS_CTYPE   	ctl_stereo;
    PS_CTYPE   	ctl_repeat;
    PS_CTYPE	ctl_mode;
    PS_CTYPE	ctl_len_units;
    PS_CTYPE	ctl_max_buf_size;
    PS_CTYPE	ctl_on_noteon;
    int	    	rep_counter;
    bool    	anticlick; 			  
    PS_STYPE2	anticlick_from[ MODULE_OUTPUTS ]; 
    PS_STYPE*  	buf[ MODULE_OUTPUTS ];
    int	    	buf_size;
    int		len; 
    int	    	buf_ptr;
    bool	buf_clean;
    bool    	empty;
    int	    	tick_size;
    uint8_t   	ticks_per_line;
    uint32_t    changed; 
};
static void loop_change_ctl_limits( int mod_num, psynth_net* pnet )
{
    if( pnet->base_host_version < 0x02010000 ) return;
    psynth_module* mod = NULL;
    MODULE_DATA* data = NULL;
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
    if( data->ctl_len_units >= 1 ) max = 8192;
    int cnum = 1;
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
static void loop_handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->changed & CHANGED_IOCHANNELS )
    {
	if( data->ctl_stereo )
	{
	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
	    {
		psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
		psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
	    }
	}
	else
	{
	    if( psynth_get_number_of_outputs( mod ) != 1 )
	    {
		psynth_set_number_of_outputs( 1, mod_num, pnet );
		psynth_set_number_of_inputs( 1, mod_num, pnet );
	    }
	}
    }
    if( data->changed & CHANGED_BUFSIZE )
    {
        int len = 0;
        switch( data->ctl_len_units )
        {
            case 0:
                len = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line * (uint64_t)data->ctl_len ) / 256 );
                len /= 128;
                if( pnet->base_host_version < 0x02010000 )
                {
            	    if( len > pnet->sampling_freq / 3 )
            		len = pnet->sampling_freq / 3;
		}
                break;
            case 1:
            case 2:
            case 3:
                len = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line * (uint64_t)data->ctl_len ) / 256 );
                len /= data->ctl_len_units;
                break;
            case 4:
                len = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ctl_len ) / 256 );
                break;
            case 5:
                len = ( pnet->sampling_freq * data->ctl_len ) / 1000;
                break;
            case 6:
                if( data->ctl_len == 0 )
                    len = pnet->sampling_freq;
                else
                    len = pnet->sampling_freq / data->ctl_len;
                break;
        }
        if( len == 0 ) len = 1;
        int max_buf_size = data->ctl_max_buf_size * pnet->sampling_freq;
        if( len > max_buf_size ) len = max_buf_size;
        bool buf_size_changed = false;
        while( len > data->buf_size )
        {
            if( data->buf_size == 0 )
                data->buf_size = len; 
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
        data->len = len;
        if( data->buf_ptr >= len ) data->buf_ptr %= len;
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
	    retval = (PS_RETTYPE)"Loop";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Эффект многократного повторения звука.\nДля сброса нужно либо поменять контроллер \"Повторы\", либо послать модулю какую-нибудь ноту.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Tape loop - repeats a fragment of the incoming sound a specified number of times.\nTo reset the loop: either change the \"Repeats\" ctl, or send some note to this module.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF7FC5";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 8, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 256, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LENGTH ), "", 0, 256, 256, 0, &data->ctl_len, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_MONO_STEREO ), 0, 1, 1, 1, &data->ctl_stereo, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LOOP_REPEAT ), "", 0, 128, 0, 1, &data->ctl_repeat, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_LOOP_MODES ), 0, 1, 0, 1, &data->ctl_mode, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LENGTH_UNIT ), ps_get_string( STR_PS_LOOP_LENGTH_UNITS ), 0, LEN_UNIT_MAX, 0, 1, &data->ctl_len_units, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MAX_BUF_SIZE ), ps_get_string( STR_PS_SEC ), 1, 60 * 4, 4, 1, &data->ctl_max_buf_size, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ON_NOTEON ), ps_get_string( STR_PS_LOOP_NOTEON_MODES ), 0, 1, 0, 1, &data->ctl_on_noteon, -1, 0, pnet );
	    data->buf_clean = true;
	    data->empty = true;
	    data->changed = 0xFFFFFFFF;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
            loop_handle_changes( mod, mod_num );
            loop_change_ctl_limits( mod_num, pnet );
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
	    data->rep_counter = 0;
	    data->anticlick = 0;
	    for( int i = 0; i < MODULE_OUTPUTS; i++ ) data->anticlick_from[ i ] = 0;
	    data->empty = 1;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( data->changed ) loop_handle_changes( mod, mod_num );
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		int outputs_num = psynth_get_number_of_outputs( mod );
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			data->empty = 0;
			break;
		    }
		}
		if( data->empty ) break;
		data->buf_clean = false;
		int len = data->len;
		int rep_counter = 0;
		bool anticlick = 0;
		int buf_ptr = 0;
		PS_STYPE2 volume = PS_NORM_STYPE( data->ctl_volume, 256 );
		int anticlick_len = pnet->sampling_freq / 1000;
		if( anticlick_len > len / 2 ) anticlick_len = len / 2;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    PS_STYPE* cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    rep_counter = data->rep_counter;
		    anticlick = data->anticlick;
		    int i = 0;
		    while( i < frames )
		    {
			int size2 = len - buf_ptr;
			if( size2 > frames - i ) size2 = frames - i;
			int i_end = i + size2;
			if( rep_counter == 0 )
			{
			    if( anticlick )
			    {
				while( i < i_end )
				{
				    if( buf_ptr < anticlick_len )
				    {
					int c = ( buf_ptr << 8 ) / anticlick_len;
					out[ i ] = ( in[ i ] * c ) / 256 + ( data->anticlick_from[ ch ] * (256-c) ) / 256;
				    }
				    else
				    {
					out[ i ] = in[ i ];
				    }
				    cbuf[ buf_ptr ] = in[ i ];
				    buf_ptr++;
				    i++;
				}
			    }
			    else 
			    {
				while( i < i_end )
				{
				    out[ i ] = in[ i ];
				    cbuf[ buf_ptr ] = in[ i ];
				    buf_ptr++;
				    i++;
				}
			    }
			}
			else
			{
			    switch( data->ctl_mode )
			    {
				case 0:
				    {
					PS_STYPE2 interp_from = cbuf[ len - 1 ];
					while( i < i_end )
					{
					    if( buf_ptr < anticlick_len )
					    {
						int c = ( buf_ptr << 8 ) / anticlick_len;
						PS_STYPE2 v = ( (PS_STYPE2)cbuf[ buf_ptr ] * c ) / 256 + ( interp_from * (256-c) ) / 256;
						out[ i ] = v;
					    }
					    else
					    {
					        out[ i ] = cbuf[ buf_ptr ];
					    }
					    buf_ptr++;
					    i++;
					}
				    }
				    break;
				case 1:
				    if( rep_counter & 1 )
				    {
					while( i < i_end )
					{
					    out[ i ] = cbuf[ len - 1 - buf_ptr ];
					    buf_ptr++;
					    i++;
					}
				    }
				    else
				    {
					while( i < i_end )
					{
					    out[ i ] = cbuf[ buf_ptr ];
					    buf_ptr++;
					    i++;
					}
				    }
				    break;
			    }
			}
			if( buf_ptr >= len ) 
			{
			    rep_counter++;
			    anticlick = 0;
			    if( rep_counter > data->ctl_repeat && data->ctl_repeat != 128 )
			    {
				if( rep_counter > 1 )
				{
				    if( data->ctl_mode == 1 && ( ( rep_counter & 1 ) == 0 ) )
					data->anticlick_from[ ch ] = cbuf[ 0 ];
				    else
					data->anticlick_from[ ch ] = cbuf[ len - 1 ];
				    anticlick = 1;
				}
				rep_counter = 0;
			    }
			    buf_ptr = 0;
			}
		    }
		    if( data->ctl_volume != 256 )
		    {
			for( i = 0; i < frames; i++ )
			{
			    PS_STYPE2 v = out[ i ];
			    v = PS_NORM_STYPE_MUL( v, volume, 256 );
			    out[ i ] = v;
			}
		    }
		}
		data->buf_ptr = buf_ptr;
		data->rep_counter = rep_counter;
		data->anticlick = anticlick;
		retval = 1;
		if( data->ctl_volume == 0 ) retval = 2;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
            switch( event->controller.ctl_num )
            {
        	case 2:
        	    data->changed |= CHANGED_IOCHANNELS;
        	    break;
                case 3: 
	    	    data->rep_counter = 0;
		    data->buf_ptr = 0;
            	    break;
                case 1: 
                case 6: 
                    data->changed |= CHANGED_BUFSIZE;
                    break;
                case 5: 
                    data->ctl_len_units = event->controller.ctl_val;
                    if( data->ctl_len_units > LEN_UNIT_MAX ) data->ctl_len_units = LEN_UNIT_MAX;
                    loop_change_ctl_limits( mod_num, pnet );
                    data->changed |= CHANGED_BUFSIZE;
                    retval = 1;
                    break;
            }
            break;
	case PS_CMD_NOTE_ON:
	    switch( data->ctl_on_noteon )
	    {
		default:
    		    data->rep_counter = 0;
		    data->buf_ptr = 0;
		    break;
		case 1:
		    data->buf_ptr = 0;
		    break;
	    }
	    break;
	case PS_CMD_SPEED_CHANGED:
	    data->tick_size = event->speed.tick_size;
	    data->ticks_per_line = event->speed.ticks_per_line;
	    if( data->ctl_len_units <= 4 )
                data->changed |= CHANGED_BUFSIZE;
	    retval = 1;
	    break;
	case PS_CMD_RESET_MSB:
            data->changed |= CHANGED_IOCHANNELS;
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
