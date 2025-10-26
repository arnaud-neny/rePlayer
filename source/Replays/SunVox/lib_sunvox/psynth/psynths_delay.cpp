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
#define MODULE_DATA	psynth_delay_data
#define MODULE_HANDLER	psynth_delay
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define DELAY_UNIT_MAX	9
#define DELAY_MUL_MAX	15
struct delay_evt
{
    psynth_event	evt;
    uint16_t 		vol;
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_dry;
    PS_CTYPE   	ctl_wet;
    PS_CTYPE   	ctl_delay_l;
    PS_CTYPE   	ctl_delay_r;
    PS_CTYPE   	ctl_volume_l;
    PS_CTYPE   	ctl_volume_r;
    PS_CTYPE   	ctl_mono;
    PS_CTYPE   	ctl_inverse;
    PS_CTYPE	ctl_delay_units;
    PS_CTYPE	ctl_delay_mul;
    PS_CTYPE	ctl_feedback;
    PS_CTYPE	ctl_negative_feedback;
    PS_CTYPE	ctl_allpass;
    int		delay[ MODULE_OUTPUTS ]; 
    int	    	buf_size;
    PS_STYPE*	buf[ MODULE_OUTPUTS ];
    int	    	buf_ptr;
    bool        buf_clean;
    delay_evt*		evtbuf;
    int			evtbuf_size;
    int			evtbuf_wp;
    int			evtbuf_rp;
    int	    	empty_frames_counter;
    int		frame_counter;
    int     	tick_size;      
    uint8_t   	ticks_per_line;
    uint32_t	ctls_changed; 	
};
static void delay_change_ctl_limits( int mod_num, psynth_net* pnet )
{
    if( pnet->base_host_version < 0x01090202 ) return;
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
    switch( data->ctl_delay_units )
    {
        case 1:
            max = 4000;
            break;
        case 2:
            max = 8192;
            break;
        case 7: 
        case 8: 
	case 9: 
    	    max = 32768;
    	    break;
    }
    for( int cnum = 2; cnum <= 3; cnum++ )
    {
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
}
static void add_event( MODULE_DATA* data, psynth_event* event, int vol, int mod_num, int offset )
{
    int mask = data->evtbuf_size - 1;
    delay_evt* dest = &data->evtbuf[ data->evtbuf_wp ];
    data->evtbuf_wp = ( data->evtbuf_wp + 1 ) & mask;
    dest->evt = *event;
    dest->vol = vol;
    PSYNTH_EVT_ID_INC( dest->evt.id, mod_num );
    dest->evt.offset = offset;
    int len = ( data->evtbuf_wp - data->evtbuf_rp ) & mask;
    if( len >= data->evtbuf_size - 1 )
    {
    	data->evtbuf_size *= 4;
	delay_evt* new_buf = SMEM_ALLOC2( delay_evt, data->evtbuf_size );
	int rp = data->evtbuf_rp;
	int p = 0;
	while( rp != data->evtbuf_wp )
	{
	    new_buf[ p ] = data->evtbuf[ rp ];
	    p++;
	    rp = ( rp + 1 ) & mask;
	}
	data->evtbuf_rp = 0;
	data->evtbuf_wp = p;
	smem_free( data->evtbuf );
	data->evtbuf = new_buf;
    }
}
static void handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->ctls_changed & 1 )
    {
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
    }
    if( data->ctls_changed & 2 )
    {
	for( int i = 0; i < MODULE_OUTPUTS; i++ ) data->delay[ i ] = 0;
	switch( data->ctl_delay_units )
	{
    	    case 0:
		data->delay[ 0 ] = ( ( pnet->sampling_freq / 64 ) * data->ctl_delay_l ) >> 8;
		data->delay[ 1 ] = ( ( pnet->sampling_freq / 64 ) * data->ctl_delay_r ) >> 8;
    		break;
    	    case 1:
    		data->delay[ 0 ] = ( pnet->sampling_freq * data->ctl_delay_l ) / 1000;
    		data->delay[ 1 ] = ( pnet->sampling_freq * data->ctl_delay_r ) / 1000;
    		break;
    	    case 2:
    		if( data->ctl_delay_l == 0 )
            	    data->delay[ 0 ] = pnet->sampling_freq;
    		else
	    	    data->delay[ 0 ] = pnet->sampling_freq / data->ctl_delay_l;
    		if( data->ctl_delay_r == 0 )
            	    data->delay[ 1 ] = pnet->sampling_freq;
    		else
    		    data->delay[ 1 ] = pnet->sampling_freq / data->ctl_delay_r;
    		break;
    	    case 3:
    		data->delay[ 0 ] = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ctl_delay_l ) / 256 );
    		data->delay[ 1 ] = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ctl_delay_r ) / 256 );
    		break;
    	    case 4:
	    case 5:
    	    case 6:
    		data->delay[ 0 ] = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line * (uint64_t)data->ctl_delay_l ) / 256 );
    		data->delay[ 1 ] = (int)( ( (uint64_t)data->tick_size * (uint64_t)data->ticks_per_line * (uint64_t)data->ctl_delay_r ) / 256 );
    		data->delay[ 0 ] /= ( data->ctl_delay_units - 3 );
    		data->delay[ 1 ] /= ( data->ctl_delay_units - 3 );
    		break;
    	    case 7:
		data->delay[ 0 ] = (int64_t)data->ctl_delay_l * pnet->sampling_freq / 44100;
		data->delay[ 1 ] = (int64_t)data->ctl_delay_r * pnet->sampling_freq / 44100;
    		break;
    	    case 8:
		data->delay[ 0 ] = (int64_t)data->ctl_delay_l * pnet->sampling_freq / 48000;
		data->delay[ 1 ] = (int64_t)data->ctl_delay_r * pnet->sampling_freq / 48000;
    		break;
    	    case 9:
		data->delay[ 0 ] = data->ctl_delay_l;
		data->delay[ 1 ] = data->ctl_delay_r;
    		break;
	}
	int buf_size = data->buf_size;
        int max_buf_size = pnet->sampling_freq * 4;
        if( pnet->base_host_version >= 0x02000000 ) max_buf_size = pnet->sampling_freq * 60;
	for( int i = 0; i < MODULE_OUTPUTS; i++ )
	{
	    data->delay[ i ] *= data->ctl_delay_mul;
	    if( data->delay[ i ] == 0 ) data->delay[ i ] = 1;
	    if( data->delay[ i ] > max_buf_size ) data->delay[ i ] = max_buf_size;
	    if( data->delay[ i ] > buf_size ) buf_size = data->delay[ i ];
	}
	if( buf_size > data->buf_size )
	{
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
    	    {
    		data->buf[ i ] = SMEM_ZRESIZE2( data->buf[ i ], PS_STYPE, buf_size );
    	    }
    	    data->buf_size = buf_size;
	}
	if( data->buf_ptr >= data->buf_size )
    	    data->buf_ptr = 0;
    }
    data->ctls_changed = 0;
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
	    retval = (PS_RETTYPE)"Delay";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Delay задерживает звук и звуковые сообщения.\nМакс. длина задержки = 1 мин.";
                        break;
                    }
		    retval = (PS_RETTYPE)"This module delays the sound and the incoming events.\nMax delay length = 1 minute.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FFFDB";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_IO; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 13, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DRY ), "", 0, 512, 256, 0, &data->ctl_dry, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WET ), "", 0, 512, 256, 0, &data->ctl_wet, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY_L ), "", 0, 256, 128, 0, &data->ctl_delay_l, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY_R ), "", 0, 256, 160, 0, &data->ctl_delay_r, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME_L ), "", 0, 256, 256, 0, &data->ctl_volume_l, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME_R ), "", 0, 256, 256, 0, &data->ctl_volume_r, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INVERSE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_inverse, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY_UNIT ), ps_get_string( STR_PS_DELAY_UNITS ), 0, DELAY_UNIT_MAX, 0, 1, &data->ctl_delay_units, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DELAY_MUL ), "", 1, DELAY_MUL_MAX, 1, 1, &data->ctl_delay_mul, 1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 32768, 0, 0, &data->ctl_feedback, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_NEGATIVE_FEEDBACK ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_negative_feedback, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ALLPASS_FILTER ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_allpass, -1, 3, pnet );
	    data->buf_size = pnet->sampling_freq / 64;
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
	    {
		data->buf[ i ] = SMEM_ZALLOC2( PS_STYPE, data->buf_size );
	    }
	    data->buf_ptr = 0;
	    data->buf_clean = true;
	    data->empty_frames_counter = data->buf_size;
	    data->evtbuf_size = 16;
	    data->evtbuf = SMEM_ALLOC2( delay_evt, data->evtbuf_size );
	    data->ctls_changed = 0xFFFFFFFF;
	    retval = 1;
	    break;
        case PS_CMD_SETUP_FINISHED:
	    handle_changes( mod, mod_num );
            delay_change_ctl_limits( mod_num, pnet );
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
	    data->evtbuf_wp = 0;
	    data->evtbuf_rp = 0;
	    data->empty_frames_counter = data->buf_size;
	    data->frame_counter = 0;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( data->ctls_changed ) handle_changes( mod, mod_num );
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
            	data->frame_counter += frames;
            	if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE )
            	{
		    data->evtbuf_wp = 0;
		    data->evtbuf_rp = 0;
            	}
		while( data->evtbuf_rp != data->evtbuf_wp )
		{
		    delay_evt* evt0 = &data->evtbuf[ data->evtbuf_rp ];
		    psynth_event* evt = &evt0->evt;
		    if( data->frame_counter - ( evt->offset + data->delay[ 0 ] ) <= 0 ) break;
		    evt->offset = evt->offset + data->delay[ 0 ] - ( data->frame_counter - frames ) + offset;
		    if( evt->offset < 0 ) evt->offset = 0;
		    int pitch = evt->note.pitch;
		    int vel = evt->note.velocity;
		    for( int i = 0; i < mod->output_links_num; i++ )
            	    {
            		int l = mod->output_links[ i ];
            		psynth_module* dest = psynth_get_module( l, pnet );
            		if( dest )
            		{
            		    switch( evt->command )
            		    {
            			case PS_CMD_NOTE_ON:
            			case PS_CMD_SET_FREQ:
            			    evt->note.pitch = pitch - dest->finetune - dest->relative_note * 256;
            			case PS_CMD_SET_VELOCITY:
            			case PS_CMD_NOTE_OFF:
            			    if( pnet->base_host_version >= 0x02000000 )
            			    {
            				int w = data->ctl_volume_l * data->ctl_wet / 256;
            				int v = vel * evt0->vol * w / ( 256 * 256 );
            				if( v > 256 ) v = 256;
            				evt->note.velocity = v;
            			    }
            			    else
            				evt->note.velocity = vel * evt0->vol / 256;
            			    break;
            			default: break;
            		    }
            		    psynth_add_event( l, evt, pnet );
            		}
            	    }
            	    if( data->ctl_feedback )
            	    {
            		if( evt0->vol )
            		{
            		    if( evt->command != PS_CMD_SET_SAMPLE_OFFSET && evt->command != PS_CMD_SET_SAMPLE_OFFSET2 )
            		    {
            			evt->note.velocity = vel;
            			evt->note.pitch = pitch;
            		    }
            		    evt0->vol = (int)evt0->vol * data->ctl_feedback / 32768;
			    add_event( data, evt, evt0->vol, mod_num, data->frame_counter - frames + evt->offset );
			}
            	    }
		    data->evtbuf_rp = ( data->evtbuf_rp + 1 ) & ( data->evtbuf_size - 1 );
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
		    if( data->empty_frames_counter >= data->buf_size && data->ctl_feedback == 0 )
			break;
		    data->empty_frames_counter += frames;
		}
		data->buf_clean = false;
		int buf_ptr;
		PS_STYPE2 ctl_dry = PS_NORM_STYPE( data->ctl_dry, 256 );
		PS_STYPE2 ctl_feedback = PS_NORM_STYPE( data->ctl_feedback, 32768 );
		if( data->ctl_negative_feedback ) ctl_feedback = -ctl_feedback;
		int buf_size = data->buf_size;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    PS_STYPE* cbuf = data->buf[ ch ];
		    buf_ptr = data->buf_ptr;
		    int wet;
		    if( ch == 0 )
			wet = ( data->ctl_wet * data->ctl_volume_l ) / 256;
		    else
			wet = ( data->ctl_wet * data->ctl_volume_r ) / 256;
		    PS_STYPE2 ctl_wet = PS_NORM_STYPE( wet, 256 );
		    int delay = data->delay[ ch ];
		    if( ctl_feedback != 0 )
		    {
			PS_STYPE2 inv = 1;
			if( data->ctl_inverse ) inv = -1;
			if( data->ctl_allpass == 0 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				int ptr = buf_ptr - delay;
				if( ptr < 0 ) ptr += buf_size;
				PS_STYPE2 out_val = cbuf[ ptr ] * inv;
				out[ i ] = PS_NORM_STYPE_MUL( out_val, ctl_wet, 256 );
				cbuf[ buf_ptr ] = in[ i ] + PS_NORM_STYPE_MUL( out_val, ctl_feedback, 32768 );
				buf_ptr++;
				if( buf_ptr >= buf_size ) buf_ptr = 0;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
				int ptr = buf_ptr - delay;
				if( ptr < 0 ) ptr += buf_size;
				PS_STYPE2 out_val = cbuf[ ptr ] * inv;
				PS_STYPE2 buf_write = in[ i ] + PS_NORM_STYPE_MUL( out_val, ctl_feedback, 32768 );
				out_val += PS_NORM_STYPE_MUL( buf_write, -ctl_feedback, 32768 );
				out[ i ] = PS_NORM_STYPE_MUL( out_val, ctl_wet, 256 );
				cbuf[ buf_ptr ] = buf_write;
				buf_ptr++;
				if( buf_ptr >= buf_size ) buf_ptr = 0;
			    }
			}
		    }
		    else
		    {
			if( ctl_wet == 0 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				cbuf[ buf_ptr ] = in[ i ];
				out[ i ] = 0;
				buf_ptr++;
				if( buf_ptr >= buf_size ) buf_ptr = 0;
			    }
			}
			else
			{
			    if( data->ctl_inverse ) ctl_wet = -ctl_wet;
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 out_val;
				int ptr = buf_ptr - delay;
				if( ptr < 0 ) ptr += buf_size;
				out_val = PS_NORM_STYPE_MUL( cbuf[ ptr ], ctl_wet, 256 );
				out[ i ] = out_val;
				cbuf[ buf_ptr ] = in[ i ];
				buf_ptr++;
				if( buf_ptr >= buf_size ) buf_ptr = 0;
			    }
			}
		    }
		    if( data->ctl_dry > 0 )
		    {
			if( data->ctl_dry != 256 )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 out_val = PS_NORM_STYPE_MUL( in[ i ], ctl_dry, 256 );
				out[ i ] += out_val;
			    }
			}
			else
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
        	case 2: case 3: 
        	    data->ctls_changed |= 2;
		    break;
        	case 6: 
        	    data->ctls_changed |= 1;
        	    break;
        	case 8: 
        	    data->ctl_delay_units = event->controller.ctl_val;
        	    if( data->ctl_delay_units > DELAY_UNIT_MAX ) data->ctl_delay_units = DELAY_UNIT_MAX;
            	    delay_change_ctl_limits( mod_num, pnet );
        	    data->ctls_changed |= 2;
            	    retval = 1;
		    break;
        	case 9: 
        	    data->ctl_delay_mul = event->controller.ctl_val;
        	    if( data->ctl_delay_mul > DELAY_MUL_MAX ) data->ctl_delay_mul = DELAY_MUL_MAX;
        	    data->ctls_changed |= 2;
            	    retval = 1;
		    break;
            }
            break;
        case PS_CMD_NOTE_ON:
        case PS_CMD_NOTE_OFF:
        case PS_CMD_ALL_NOTES_OFF:
        case PS_CMD_SET_FREQ:
        case PS_CMD_SET_VELOCITY:
        case PS_CMD_SET_SAMPLE_OFFSET:
        case PS_CMD_SET_SAMPLE_OFFSET2:
    	    if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS )
    	    {
    		psynth_event e = *event; PSYNTH_EVT_ID_INC( e.id, mod_num );
    		psynth_multisend( mod, &e, pnet );
    	    }
    	    else
    	    {
		add_event( data, event, 256, mod_num, data->frame_counter );
	    }
    	    retval = 1;
    	    break;
	case PS_CMD_SPEED_CHANGED:
            data->tick_size = event->speed.tick_size;
            data->ticks_per_line = event->speed.ticks_per_line;
            if( data->ctl_delay_units >= 3 && data->ctl_delay_units <= 6 )
		data->ctls_changed |= 2;
            retval = 1;
            break;
        case PS_CMD_RESET_MSB:
    	    data->ctls_changed |= 1;
    	    break;
	case PS_CMD_CLOSE:
	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
		smem_free( data->buf[ i ] );
	    smem_free( data->evtbuf );
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
