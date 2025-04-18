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
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_pitch2ctl_data
#define MODULE_HANDLER	psynth_pitch2ctl
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	0
#define MAX_CHANNELS	16 
struct pitch2ctl_channel
{
    uint                id;
    int                 pitch;
    uint                counter;
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_mode;
    PS_CTYPE		ctl_note_off;
    PS_CTYPE		ctl_note;
    PS_CTYPE		ctl_notes;
    PS_CTYPE		ctl_min;
    PS_CTYPE		ctl_max;
    PS_CTYPE		ctl_num;
    int                 mod_num;
    psynth_net*         pnet;
    pitch2ctl_channel	channels[ MAX_CHANNELS ];
    uint                ch_flags;
    int			search_ptr;
    uint		counter;
    uint		fg; 
    psynth_event	out_evt;
#ifdef SUNVOX_GUI
    window_manager* 	wm;
#endif
};
enum 
{
    pitch2ctl_mode_freq = 0,
    pitch2ctl_mode_pitch,
    pitch2ctl_modes    
};
enum
{
    pitch2ctl_noteoff_action_none = 0,
    pitch2ctl_noteoff_action_down,
    pitch2ctl_noteoff_action_up,
    pitch2ctl_noteoff_actions
};
#ifdef SUNVOX_GUI
struct pitch2ctl_visual_data
{
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
    WINDOWPTR   	win;
};
int pitch2ctl_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    pitch2ctl_visual_data* data = (pitch2ctl_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( pitch2ctl_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
        	data->win = win;
        	data->pnet = (psynth_net*)win->host;
        	data->module_data = 0;
        	data->mod_num = -1;        	
    	    }
            retval = 1;
            break;
        case EVT_DRAW:
    	    {
    		wbd_lock( win );
                draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		psynth_module* m = psynth_get_module( data->mod_num, data->pnet );
                if( m )
                {
                    for( int i = 0; i < m->output_links_num; i++ )
                    {
                        int l = m->output_links[ i ];
                        psynth_module* m2 = psynth_get_module( l, data->pnet );
                        if( m2 && ( m2->flags & PSYNTH_FLAG_EXISTS ) )
                        {
	    		    psynth_draw_ctl_info( 
				l, 
				data->pnet, 
				data->module_data->ctl_num, 
				data->module_data->ctl_min, 
				data->module_data->ctl_max, 
				0, 0, 0, 0,
				win );
			    break;
			}
		    }
		}
                wbd_draw( wm );
                wbd_unlock( wm );
    	    }
    	    retval = 1;
    	    break;
	case EVT_MOUSEBUTTONDOWN:
        case EVT_MOUSEMOVE:
        case EVT_MOUSEBUTTONUP:
        case EVT_TOUCHBEGIN:
        case EVT_TOUCHEND:
        case EVT_TOUCHMOVE:
            retval = 1;
            break;
        case EVT_BEFORECLOSE:
            retval = 1;
            break;
    }
    return retval;
}
#endif
static void pitch2ctl_channel_reset( MODULE_DATA* data, uint c )
{
    if( c >= MAX_CHANNELS ) return;
    pitch2ctl_channel* chan = &data->channels[ c ];
    chan->id = 0xFFFFFFFF;
    data->ch_flags &= ~( 1 << c );
}
static void pitch2ctl_reset( MODULE_DATA* data )
{
    for( uint c = 0; c < MAX_CHANNELS; c++ )
        pitch2ctl_channel_reset( data, c );
    data->counter = 0;
    data->fg = 0;
    data->ch_flags = 0;
}
static void pitch2ctl_send( MODULE_DATA* data, psynth_event* src )
{
    psynth_net* pnet = data->pnet;
    psynth_module* mod = &pnet->mods[ data->mod_num ];
    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) return;
    int ctl_num = data->ctl_num - 1;
    if( ctl_num < 0 ) return;
    int note_off = pitch2ctl_noteoff_action_none;
    if( data->ch_flags == 0 )
    {
	note_off = data->ctl_note_off;
	if( note_off == pitch2ctl_noteoff_action_none )
	    return;
    }
    pitch2ctl_channel* chan = &data->channels[ data->fg ];
    data->out_evt.offset = src->offset;
    data->out_evt.id = src->id;
    data->out_evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
    data->out_evt.controller.ctl_num = ctl_num;
    data->out_evt.controller.ctl_val = 0;
    int freq = 0;
    int pitch = 0;
    if( note_off == pitch2ctl_noteoff_action_none )
    {
	switch( data->ctl_mode )
	{
	    case pitch2ctl_mode_freq:
		{
		    int oct_offset = 5;
		    PSYNTH_GET_FREQ( g_linear_freq_tab, freq, chan->pitch / 4 + 64 * 12 * oct_offset );
		}
		break;
	    case pitch2ctl_mode_pitch:
		{
		    pitch = PS_NOTE0_PITCH - chan->pitch;
		    pitch -= data->ctl_note * 256;
		}
		break;
	}
    }
    for( int i = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
        if( (unsigned)l < (unsigned)pnet->mods_num )
        {
            psynth_module* s = &pnet->mods[ l ];
            if( s->flags & PSYNTH_FLAG_EXISTS )
            {
        	if( 1 ) 
        	{
        	    int val = 0;
        	    if( note_off == pitch2ctl_noteoff_action_none )
        	    {
			switch( data->ctl_mode )
			{
			    case pitch2ctl_mode_freq:
	    			val = psynth_get_scaled_ctl_value( l, ctl_num, freq, false, pnet );
	    			if( data->ctl_min != 0 || data->ctl_max != 32768 )
			    	{
	    			    int range = data->ctl_max - data->ctl_min;
	    			    val = data->ctl_min + ( val * range ) / 32768;
	    			}
	    			break;
	    		    case pitch2ctl_mode_pitch:
	    			val = pitch;
	    			{
	    			    int range = data->ctl_max - data->ctl_min;
	    			    int notes = data->ctl_notes;
	    			    if( notes < 1 ) notes = 1;
	    			    val = data->ctl_min + ( val * range ) / ( notes * 256 );
	    			}
	    			break;
	    		}
	    	    }
	    	    else
	    	    {
	    		switch( note_off )
	    		{
	    		    case pitch2ctl_noteoff_action_down: val = data->ctl_min; break;
	    		    case pitch2ctl_noteoff_action_up: val = data->ctl_max; break;
	    		}
	    	    }
	    	    if( data->ctl_max >= data->ctl_min )
	    	    {
	    		if( val < data->ctl_min ) val = data->ctl_min;
	    		if( val > data->ctl_max ) val = data->ctl_max;
	    	    }
	    	    else
	    	    {
	    		if( val < data->ctl_max ) val = data->ctl_max;
	    		if( val > data->ctl_min ) val = data->ctl_min;
		    }
	    	    data->out_evt.controller.ctl_val = val;
            	    psynth_add_event( l, &data->out_evt, pnet );
            	}
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
	    retval = (PS_RETTYPE)"Pitch2Ctl";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Pitch2Ctl преобразует ноты в значения контроллера. Например, чем выше нота, тем больше громкость.\n"
			    "Ноты подаются на вход модуля. На выход подключается другой модуль, в котором находится интересующий нас контроллер.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Pitch2Ctl converts the incoming notes to the controller values (in some another connected module).";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFC2BB";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_NO_RENDER; break;
	case PS_CMD_GET_FLAGS2: retval = PSYNTH_FLAG2_NOTE_RECEIVER; break;
	case PS_CMD_INIT:
	    {
		data->mod_num = mod_num;
        	data->pnet = pnet;
        	psynth_resize_ctls_storage( mod_num, 7, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_PITCH2CTL_MODES ), 0, pitch2ctl_modes - 1, 0, 1, &data->ctl_mode, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_ON_NOTEOFF ), ps_get_string( STR_PS_PITCH2CTL_NOTEOFF_ACTION ), 0, pitch2ctl_noteoff_actions - 1, 0, 1, &data->ctl_note_off, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FIRST_NOTE ), "", 0, 256, 0, 1, &data->ctl_note, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANGE_SEMITONES ), "", 0, 256, 120, 1, &data->ctl_notes, 120, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_MIN ), "", 0, 32768, 0, 0, &data->ctl_min, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_MAX ), "", 0, 32768, 32768, 0, &data->ctl_max, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_CTL ), "", 0, 255, 0, 1, &data->ctl_num, -1, 2, pnet );
                for( uint c = 0; c < MAX_CHANNELS; c++ )
                    smem_clear( &data->channels[ c ], sizeof( pitch2ctl_channel ) );
		pitch2ctl_reset( data );
                data->search_ptr = 0;
                data->counter = 0;
                data->fg = 0;
                data->ch_flags = 0;
                smem_clear( &data->out_evt, sizeof( data->out_evt ) );
#ifdef SUNVOX_GUI
        	data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "Pitch2Ctl GUI", 0, 0, 1, 1, wm->button_color, 0, pnet, pitch2ctl_visual_handler, wm );
            	    mod->visual_min_ysize = 0;
            	    pitch2ctl_visual_data* data1 = (pitch2ctl_visual_data*)mod->visual->data;
            	    data1->module_data = data;
            	    data1->mod_num = mod_num;
            	    data1->pnet = pnet;
            	}
#endif
	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_ON:
	    {
		uint c = 0;
		if( data->ch_flags )
		{
            	    for( c = 0; c < MAX_CHANNELS; c++ )
            	    {
            		if( data->channels[ c ].id == event->id )
                	{
                    	    pitch2ctl_channel_reset( data, c );
                    	    break;
                	}
            	    }
            	    for( c = 0; c < MAX_CHANNELS; c++ )
            	    {
                	if( data->channels[ data->search_ptr ].id == 0xFFFFFFFF ) break;
                	data->search_ptr++;
                	if( data->search_ptr >= MAX_CHANNELS ) data->search_ptr = 0;
            	    }
            	    if( c == MAX_CHANNELS )
            	    {
                	data->search_ptr++;
                	if( data->search_ptr >= MAX_CHANNELS ) data->search_ptr = 0;
            	    }
            	}
            	else
            	{
            	    data->search_ptr = 0;
            	}
                c = data->search_ptr;
                pitch2ctl_channel* chan = &data->channels[ c ];
                chan->id = event->id;
                chan->counter = data->counter; data->counter++;
                chan->pitch = event->note.pitch;
                data->ch_flags |= 1 << c;
                data->fg = c;
                pitch2ctl_send( data, event );
	    }
	    retval = 1;
	    break;
    	case PS_CMD_SET_FREQ:
    	    {
    		if( data->ch_flags == 0 ) break;
    		for( uint c = 0; c < MAX_CHANNELS; c++ )
                {
                    pitch2ctl_channel* chan = &data->channels[ c ];
                    if( chan->id == event->id )
                    {
                        chan->pitch = event->note.pitch;
                        if( data->fg == c )
                        {
            		    pitch2ctl_send( data, event );
                        }
                        break;
                    }
                }
    	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    {
    		if( data->ch_flags == 0 ) break;
    		for( uint c = 0; c < MAX_CHANNELS; c++ )
                {
                    pitch2ctl_channel* chan = &data->channels[ c ];
                    if( chan->id == event->id )
                    {
			pitch2ctl_channel_reset( data, c );
    			if( data->ch_flags )
    			{
                            uint cnt = 0;
                            uint last_ch = 0;
                            for( uint c2 = 0; c2 < MAX_CHANNELS; c2++ )
                            {
                                if( data->ch_flags & ( 1 << c2 ) )
                                {
                                    pitch2ctl_channel* ch2 = &data->channels[ c2 ];
                                    if( ch2->counter >= cnt )
                                    {
                                        cnt = ch2->counter;
                                        last_ch = c2;
                                    }
                                }
                            }
                            if( data->fg != last_ch )
                            {
                        	data->fg = last_ch;
            		        pitch2ctl_send( data, event );
                      	    }
    			}
    			else
    			{
            		    pitch2ctl_send( data, event );
			}
                        break;
                    }
                }
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	case PS_CMD_ALL_NOTES_OFF:
	    {
		pitch2ctl_reset( data );
	        if( event->command == PS_CMD_ALL_NOTES_OFF )
	    	    pitch2ctl_send( data, event );
            }
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = 0;
    	    }
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
