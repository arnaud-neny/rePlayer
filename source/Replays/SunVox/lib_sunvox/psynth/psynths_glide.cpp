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

#include "psynth_net.h"
#define MODULE_DATA	psynth_glide_data
#define MODULE_HANDLER	psynth_glide
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	0
#define MAX_CHANNELS			32 
#define MAX_RESPONSE    		1000
#define PITCH_SEMITONE                  10
#define PITCH_OCTAVES                   5
#define GET_EVENT psynth_event e = *event; PSYNTH_EVT_ID_INC( e.id, mod_num );
struct glide_channel
{
    float		state; 
    uint                id;
    int                 vel;
    int			pitch; 
    float		floating_pitch;
    int			parent_channel;
    uint		counter;
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_response;
    PS_CTYPE   		ctl_freq;
    PS_CTYPE		ctl_reset_on_start;
    PS_CTYPE		ctl_poly;
    PS_CTYPE   		ctl_pitch;
    PS_CTYPE   		ctl_pitch_scale;
    PS_CTYPE		ctl_reset;
    PS_CTYPE		ctl_octave;
    PS_CTYPE		ctl_freq_mul;
    PS_CTYPE		ctl_freq_div;
    int         	tick_counter; 
    glide_channel	channels[ MAX_CHANNELS + 1 ]; 
    uint		ch_flags_on; 
    int                 search_ptr;
    int 		prev_ch;
    uint		ch_counter;
};
static void glide_channel_reset( MODULE_DATA* data, int c )
{
    if( (unsigned)c > MAX_CHANNELS ) return;
    glide_channel* chan = &data->channels[ c ];
    chan->state = 0;
    chan->id = ~0;
    chan->parent_channel = -1;
    if( c < MAX_CHANNELS )
    {
	data->ch_flags_on &= ~( 1 << c );
    }
}
static void glide_channel_off( MODULE_DATA* data, int c )
{
    if( (unsigned)c > MAX_CHANNELS ) return;
    glide_channel* chan = &data->channels[ c ];
    if( chan->state >= 1 )
	chan->state = 0.999;
    if( c < MAX_CHANNELS )
    {
	data->ch_flags_on &= ~( 1 << c );
    }
}
static void glide_reset( MODULE_DATA* data )
{
    data->tick_counter = 0;
    for( int c = 0; c <= MAX_CHANNELS; c++ )
    {
	data->channels[ c ].counter = 0;
        glide_channel_reset( data, c );
    }
    data->prev_ch = -1;
}
static int glide_calc_pitch( MODULE_DATA* data, int pitch )
{
    int pitch_offset = data->ctl_pitch - PITCH_OCTAVES * 12 * PITCH_SEMITONE;
    int scale = data->ctl_pitch_scale;
    if( scale < 1 ) scale = 1;
    pitch = pitch - pitch_offset * scale * 256 / 100 / PITCH_SEMITONE - (data->ctl_octave-10)*12*256;
    if( data->ctl_freq_mul > 1 || data->ctl_freq_div > 1 )
    {
	float freq = PS_PITCH_TO_GFREQ( pitch );
	if( data->ctl_freq_mul > 1 ) freq *= data->ctl_freq_mul;
	if( data->ctl_freq_div > 1 ) freq /= data->ctl_freq_div;
	pitch = PS_GFREQ_TO_PITCH( freq );
    }
    return pitch;
}
static void glide_floating_step( MODULE_DATA* data, int c, float response )
{
    if( (unsigned)c > MAX_CHANNELS ) return;
    glide_channel* chan = &data->channels[ c ];
    if( chan->state < 1 )
    {
	chan->state = ( 1 - response ) * chan->state;
	if( chan->state < 0.01 )
	{
	    chan->state = 0;
	}
    }
    chan->floating_pitch = ( 1 - response ) * chan->floating_pitch + response * (float)glide_calc_pitch( data, chan->pitch );
}
static void glide_multisend( psynth_module* mod, psynth_net* pnet, psynth_event* event )
{
    for( int i = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
        if( (unsigned)l < (unsigned)pnet->mods_num )
        {
            psynth_module* s = &pnet->mods[ l ];
            if( s->flags & PSYNTH_FLAG_EXISTS )
            {
                psynth_add_event( l, event, pnet );
            }
        }
    }
}
static void glide_multisend_pitch( psynth_module* mod, psynth_net* pnet, psynth_event* event, int pitch )
{
    for( int i = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
        if( (unsigned)l < (unsigned)pnet->mods_num )
        {
            psynth_module* s = &pnet->mods[ l ];
            if( s->flags & PSYNTH_FLAG_EXISTS )
            {
        	event->note.pitch = pitch - s->finetune - s->relative_note * 256;
                psynth_add_event( l, event, pnet );
            }
        }
    }
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
	    retval = (PS_RETTYPE)"Glide";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Glide принимает на вход ноты, а на выходе выдает команды плавного перехода между этими нотами. К выходу нужно подключать, например, модули-генераторы.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Glide is similar to the MultiSynth (which sends the input events to the connected output modules), but it also adds the commands of smooth transition between the notes.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#4EFF00";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_OUTPUT_IS_EMPTY; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_SENDER; break;
	case PS_CMD_INIT:
	    {
		psynth_resize_ctls_storage( mod_num, 10, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESPONSE ), "", 0, MAX_RESPONSE, MAX_RESPONSE * 0.5, 0, &data->ctl_response, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_RATE ), ps_get_string( STR_PS_HZ ), 1, 32768, 150, 0, &data->ctl_freq, 150, 0, pnet );
		psynth_set_ctl_flags( mod_num, 1, PSYNTH_CTL_FLAG_EXP3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESET_ON_1ST_NOTE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_reset_on_start, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_poly, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PITCH ), ps_get_string( STR_PS_SEMITONE10 ), 0, PITCH_OCTAVES * 12 * PITCH_SEMITONE * 2, PITCH_OCTAVES * 12 * PITCH_SEMITONE, 0, &data->ctl_pitch, PITCH_OCTAVES * 12 * PITCH_SEMITONE, 1, pnet );
        	psynth_set_ctl_show_offset( mod_num, 4, -( PITCH_OCTAVES * 12 * PITCH_SEMITONE ), pnet );
        	psynth_register_ctl( mod_num, ps_get_string( STR_PS_PITCH_SCALE ), "%", 0, 200, 100, 0, &data->ctl_pitch_scale, 100, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RESET ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_reset, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_OCTAVE ), "", 0, 20, 10, 1, &data->ctl_octave, 10, 1, pnet );
        	psynth_set_ctl_show_offset( mod_num, 7, -10, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_MUL ), "", 1, 256, 1, 1, &data->ctl_freq_mul, 1, 1, pnet );
		psynth_set_ctl_flags( mod_num, 8, PSYNTH_CTL_FLAG_EXP3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_DIV ), "", 1, 256, 1, 1, &data->ctl_freq_div, 1, 1, pnet );
		psynth_set_ctl_flags( mod_num, 9, PSYNTH_CTL_FLAG_EXP3, pnet );
        	data->tick_counter = 0;
        	data->ch_flags_on = 0;
        	for( int c = 0; c <= MAX_CHANNELS; c++ )
        	{
        	    smem_clear( &data->channels[ c ], sizeof( glide_channel ) );
            	    glide_channel_reset( data, c );
            	}
    		data->search_ptr = 0;
    		data->prev_ch = -1;
    		data->ch_counter = 0;
    	    }
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    {
		glide_reset( data );
	    }
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		int offset = mod->offset;
                int frames = mod->frames;
		if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE )
		{
		    if( data->ch_flags_on )
		    {
			for( int c = 0; c <= MAX_CHANNELS; c++ )
			{
    			    glide_channel* chan = &data->channels[ c ];
            		    if( data->ch_flags_on & ( 1 << c ) )
            		    {
                    		psynth_event e;
                    		smem_clear_struct( e );
                    		e.command = PS_CMD_NOTE_OFF;
                    		e.offset = offset;
                    		e.note.velocity = 256;
                    		e.note.pitch = 0;
                    		e.id = chan->id;
                    		psynth_multisend( mod, &e, pnet );
            			glide_channel_off( data, c );
                	    }
            		}
            	    }
		    break;
		}
		int r = data->ctl_response;
	        if( r < 1 ) r = 1;
	        float response = (float)r / (float)MAX_RESPONSE;
		psynth_event freq_evt;
		smem_clear_struct( freq_evt );
		freq_evt.command = PS_CMD_SET_FREQ;
		int first_ch = 0;
    		int last_ch = MAX_CHANNELS - 1;
    		if( data->ctl_poly == 0 )
    		{
    		    first_ch = MAX_CHANNELS;
    		    last_ch = MAX_CHANNELS;
    		}
		int tick_size = 1;
                if( data->ctl_freq > 0 )
                    tick_size = ( pnet->sampling_freq * 256 ) / data->ctl_freq;
                if( tick_size <= 0 ) tick_size = 1;
                int ptr = 0;
                while( 1 )
                {
                    int buf_size = frames - ptr;
                    if( buf_size > ( tick_size - data->tick_counter ) / 256 ) buf_size = ( tick_size - data->tick_counter ) / 256;
                    if( ( tick_size - data->tick_counter ) & 255 ) buf_size++; 
                    if( buf_size > frames - ptr ) buf_size = frames - ptr;
                    if( buf_size < 0 ) buf_size = 0;
                    ptr += buf_size;
                    data->tick_counter += buf_size * 256;
                    if( data->tick_counter >= tick_size )
                    {
                	data->tick_counter %= tick_size;
	    		for( int c = first_ch; c <= last_ch; c++ )
    			{
    			    glide_channel* chan = &data->channels[ c ];
            		    if( chan->state != 0 )
            		    {
            			int prev_chan_pitch = (int)chan->floating_pitch;
            			glide_floating_step( data, c, response );
            			if( (int)chan->floating_pitch != prev_chan_pitch )
            			{
            			    int offset2;
                                    if( ptr > 0 )
                                        offset2 = offset + ptr - 1;
                                    else
                                        offset2 = offset;
            			    freq_evt.id = chan->id;
            			    freq_evt.offset = offset2;
            			    freq_evt.note.velocity = chan->vel;
            			    glide_multisend_pitch( mod, pnet, &freq_evt, (int)chan->floating_pitch );
            			}
			    }
        		}
                    }
                    if( ptr >= frames ) break;
                }
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		GET_EVENT;
		int c;
		bool channel_replace = false;
		for( c = 0; c < MAX_CHANNELS; c++ )
                {
                    if( data->channels[ c ].id == e.id )
                    {
            		channel_replace = true;
                	glide_channel_reset( data, c );
                        break;
                    }
                }
                for( c = 0; c < MAX_CHANNELS; c++ )
                {
                    if( data->channels[ data->search_ptr ].state == 0 ) break;
                    data->search_ptr++;
                    if( data->search_ptr >= MAX_CHANNELS ) data->search_ptr = 0;
                }
                if( c == MAX_CHANNELS )
                {
                    data->search_ptr++;
                    if( data->search_ptr >= MAX_CHANNELS ) data->search_ptr = 0;
            	    for( c = 0; c < MAX_CHANNELS; c++ )
            	    {
            	        if( data->channels[ data->search_ptr ].state < 1 ) break;
            		data->search_ptr++;
            	    	if( data->search_ptr >= MAX_CHANNELS ) data->search_ptr = 0;
            	    }
                }
		c = data->search_ptr;
		glide_channel* chan = &data->channels[ c ];
		chan->state = 1;
                chan->id = e.id;
                chan->counter = data->ch_counter; data->ch_counter++;
                chan->vel = e.note.velocity;
                chan->pitch = e.note.pitch;
                if( data->prev_ch < 0 || ( data->ctl_reset_on_start && ( data->ch_flags_on == 0 ) ) || data->ctl_response == MAX_RESPONSE )
            	    chan->floating_pitch = glide_calc_pitch( data, chan->pitch );
            	else
            	{
            	    if( data->ctl_poly )
            		chan->floating_pitch = data->channels[ data->prev_ch ].floating_pitch;
            	    else
            		chan->floating_pitch = data->channels[ MAX_CHANNELS ].floating_pitch;
            	}
            	data->prev_ch = c;
                if( data->ctl_poly == 0 )
                {
            	    glide_channel* mchan = &data->channels[ MAX_CHANNELS ];
            	    if( data->ch_flags_on == 0 )
            	    {
            		if( channel_replace )
            		{
            		    if( mchan->id != chan->id )
            		    {
            		        psynth_event e2 = e;
            			e2.command = PS_CMD_NOTE_OFF;
            			e2.id = mchan->id;
            			glide_multisend_pitch( mod, pnet, &e2, (int)mchan->floating_pitch );
            		    }
			}
            		mchan[ 0 ] = chan[ 0 ];
            		mchan->parent_channel = c;
            	    }
            	    else
            	    {
            		mchan->vel = chan->vel;
            		mchan->pitch = chan->pitch;
            		mchan->parent_channel = c;
            	    }
                }
                bool first; if( data->ch_flags_on == 0 ) first = true; else first = false;
            	data->ch_flags_on |= 1 << c;
                if( !( first == false && data->ctl_poly == 0 ) )
		    if( ( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) == 0 )
            		glide_multisend_pitch( mod, pnet, &e, (int)chan->floating_pitch );
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_FREQ:
	    {
		GET_EVENT;
		for( int c = 0; c < MAX_CHANNELS; c++ )
        	{
            	    glide_channel* chan = &data->channels[ c ];
                    if( chan->id == e.id )
            	    {
                	chan->pitch = e.note.pitch;
                	chan->vel = e.note.velocity;
                	if( chan->state < 1 ) chan->state = 0.999;
            		if( data->ctl_poly == 0 )
            		{
            		    glide_channel* mchan = &data->channels[ MAX_CHANNELS ];
            		    if( mchan->parent_channel == c )
            		    {
            			mchan->pitch = chan->pitch;
            			mchan->vel = chan->vel;
            			if( mchan->state < 1 ) mchan->state = 0.999;
            		    }
            		}
                	break;
            	    }
        	}
	    }
	    retval = 1;
	    break;
    	case PS_CMD_SET_VELOCITY:
	    {
		GET_EVENT;
		for( int c = 0; c < MAX_CHANNELS; c++ )
        	{
            	    glide_channel* chan = &data->channels[ c ];
                    if( chan->id == e.id )
            	    {
                	chan->vel = e.note.velocity;
            		if( data->ctl_poly == 0 )
            		{
            		    glide_channel* mchan = &data->channels[ MAX_CHANNELS ];
            		    if( mchan->parent_channel == c )
            		    {
            			mchan->vel = chan->vel;
				e.id = mchan->id;
            		    }
            		}
                	break;
            	    }
        	}
            	glide_multisend( mod, pnet, &e );
	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    {
		if( data->ch_flags_on == 0 ) break;
		GET_EVENT;
		for( int c = 0; c < MAX_CHANNELS; c++ )
        	{
                    if( ( data->ch_flags_on & ( 1 << c ) ) && data->channels[ c ].id == e.id )
            	    {
            		glide_channel_off( data, c );
            		if( data->ctl_poly == 0 )
            		{
            		    glide_channel* mchan = &data->channels[ MAX_CHANNELS ];
            		    if( mchan->parent_channel == c )
            		    {
            			if( data->ch_flags_on == 0 )
            			{
            			    glide_channel_off( data, MAX_CHANNELS );
            			    e.id = mchan->id;
            			    glide_multisend( mod, pnet, &e );
            			}
            			else
            			{
            			    uint cnt = 0;
            			    int last_ch = -1;
            			    for( int c2 = 0; c2 < MAX_CHANNELS; c2++ )
            			    {
            				if( data->ch_flags_on & ( 1 << c2 ) )
            				{
            				    glide_channel* ch2 = &data->channels[ c2 ];
            				    if( ch2->counter >= cnt )
            				    {
            					cnt = ch2->counter;
            					last_ch = c2;
            				    }
            				}
            			    }
            			    if( last_ch >= 0 )
            			    {
            				glide_channel* ch2 = &data->channels[ last_ch ];
            				mchan->vel = ch2->vel;
		            		mchan->pitch = ch2->pitch;
            				mchan->parent_channel = last_ch;
            			    }
            			}
            		    }
            		}
                	break;
            	    }
        	}
		if( data->ctl_poly )
            	    glide_multisend( mod, pnet, &e );
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    {
		GET_EVENT;
		if( data->ch_flags_on )
		{
		    for( int c = 0; c <= MAX_CHANNELS; c++ )
            		glide_channel_off( data, c );
            	}
		glide_multisend( mod, pnet, &e );
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    {
		bool release_all_channels = false;
		switch( event->controller.ctl_num )
		{
		    case 3: 
			release_all_channels = true;
			break;
		    case 6: 
			release_all_channels = true;
			data->ctl_reset = 0;
			retval = 1;
			break;
		}
		if( release_all_channels )
		{
		    psynth_event e;
		    smem_clear_struct( e );
		    e.command = PS_CMD_ALL_NOTES_OFF;
		    glide_multisend( mod, pnet, &e );
            	    glide_reset( data );
		}
	    }
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
