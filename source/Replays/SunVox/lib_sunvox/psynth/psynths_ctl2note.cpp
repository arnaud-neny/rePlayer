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
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_ctl2note_data
#define MODULE_HANDLER	psynth_ctl2note
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	0
struct MODULE_DATA
{
    PS_CTYPE		ctl_pitch;
    PS_CTYPE		ctl_note;
    PS_CTYPE		ctl_notes;
    PS_CTYPE		ctl_transpose;
    PS_CTYPE		ctl_finetune;
    PS_CTYPE		ctl_vel;
    PS_CTYPE		ctl_state;
    PS_CTYPE		ctl_noteon;
    PS_CTYPE		ctl_noteoff;
    PS_CTYPE		ctl_rec;
    psynth_event	out_evt;
#ifdef SUNVOX_GUI
    int                 rec_track;
    int                 rec_session_id;
    window_manager* 	wm;
#endif
};
enum {
    cmd_note_on = 0,
    cmd_note_off,
    cmd_note_change,
    cmd_vel_change
};
#define FINAL_PITCH( DATA ) \
    ( DATA->ctl_pitch * DATA->ctl_notes / ( 32768 / 256 ) \
    + ( DATA->ctl_note + (DATA->ctl_transpose-128) + mod->relative_note ) * 256 \
    + ( DATA->ctl_finetune - 256 ) \
    + mod->finetune )
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct ctl2note_visual_data
{
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
    WINDOWPTR   	win;
    int			min_ysize;
    char		ts[ 512 ];
};
int ctl2note_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    ctl2note_visual_data* data = (ctl2note_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( ctl2note_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
        	data->win = win;
        	data->pnet = (psynth_net*)win->host;
        	data->module_data = NULL;
        	data->mod_num = -1;
        	data->min_ysize = font_char_y_size( win->font, wm ) * 3;
    	    }
            retval = 1;
            break;
        case EVT_DRAW:
    	    {
    		MODULE_DATA* data2 = data->module_data;
		psynth_module* mod = &data->pnet->mods[ data->mod_num ];
    		wbd_lock( win );
                draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
                wm->cur_font_color = wm->color2;
                int y = 0;
                int line_ysize = char_y_size( wm );
		int off = 256 * 22 * 12; 
		int p = FINAL_PITCH( data2 );
		float n = (float)p / 256; 
		p += off;
		if( p >= 0 )
		{
            	    sprintf( data->ts, "%s = %c%d (%.02f)", sv_get_string( STR_SV_NOTE ), g_n2c[ ( p / 256 ) % 12 ], ( p / 256 ) / 12 - off/256/12, n );
            	    draw_string( data->ts, 0, y, wm );
            	}
            	p -= off;
                y += line_ysize;
		float freq = pow( 2, n / 12 ) * PS_NOTE0_FREQ;
                sprintf( data->ts, "%s = %.02f %s", ps_get_string( STR_PS_FREQUENCY ), freq, ps_get_string( STR_PS_HZ ) );
                draw_string( data->ts, 0, y, wm );
                y += line_ysize;
                if( data2->ctl_notes )
                {
            	    sprintf( data->ts, "1 %s = %.02f", ps_get_string( STR_PS_SEMITONE ), (float)32768 / data2->ctl_notes );
            	    draw_string( data->ts, 0, y, wm );
            	    y += line_ysize;
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
static void ctl2note_send( int mod_num, psynth_net* pnet, int offset, int cmd )
{
    psynth_module* mod = &pnet->mods[ mod_num ];
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_event* evt = &data->out_evt;
    evt->offset = offset;
    evt->id = ( SUNVOX_VIRTUAL_PATTERN_CTL2NOTE << 16 ) | mod_num;
    evt->note.velocity = data->ctl_vel;
    switch( cmd )
    {
	case cmd_note_on:
	    data->ctl_state = 1;
	    evt->command = PS_CMD_NOTE_ON;
	    break;
	case cmd_note_off:
	    data->ctl_state = 0;
	    evt->command = PS_CMD_NOTE_OFF;
	    break;
	case cmd_note_change:
	    evt->command = PS_CMD_SET_FREQ;
	    break;
	case cmd_vel_change:
	    evt->command = PS_CMD_SET_VELOCITY;
	    evt->note.velocity = data->ctl_vel;
	    break;
    }
    int pitch = PS_NOTE0_PITCH - FINAL_PITCH( data );
    if( cmd != cmd_vel_change )
    {
	psynth_multisend_pitch( mod, evt, pnet, pitch );
    }
    else
    {
	psynth_multisend( mod, evt, pnet );
    }
#ifdef SUNVOX_GUI
    sunvox_engine* sv = (sunvox_engine*)pnet->host;
    if( data->ctl_rec && sv->recording )
    {
	stime_ticks_t t = pnet->out_time + ( offset * stime_ticks_per_second() ) / pnet->sampling_freq;
        int line = sunvox_frames_get_value( SUNVOX_VF_CHAN_LINENUM, t, sv );
        if( pnet->th_num > 1 ) smutex_lock( &sv->rec_mutex );
        for( int i = 0, mt = 0; i < mod->output_links_num; i++ )
        {
    	    int l = mod->output_links[ i ];
            if( !psynth_get_module( l, pnet ) ) continue;
            int type = REC_PAT_CTL2NOTE;
            if( data->rec_session_id != sv->rec_session_id )
            {
                data->rec_session_id = sv->rec_session_id;
                data->rec_track = sv->rec_tracks[ type ];
                sv->rec_tracks[ type ]++;
            }
            else
            {
                if( data->rec_track + mt >= sv->rec_tracks[ type ] )
                    sv->rec_tracks[ type ] = data->rec_track + mt + 1;
            }
            if( sunvox_record_is_ready_for_event( sv ) )
            {
                sunvox_record_write_time( type, line, 0, sv );
                if( cmd == cmd_note_off )
                {
                    sunvox_record_write_byte( rec_evt_noteoff + ( type << REC_EVT_PAT_OFFSET ), sv );
                    sunvox_record_write_int( data->rec_track + mt, sv );
                }
                else
                {
                    int e = rec_evt_pitchslide;
                    if( cmd == cmd_note_on ) e = rec_evt_pitchon;
                    sunvox_record_write_byte( e + ( type << REC_EVT_PAT_OFFSET ), sv );
                    sunvox_record_write_int( pitch, sv );
                    sunvox_record_write_byte( data->ctl_vel / 2, sv );
                    sunvox_record_write_int( l, sv );
                    sunvox_record_write_int( data->rec_track + mt, sv );
                }
            }
            mt++;
        }
        if( pnet->th_num > 1 ) smutex_unlock( &sv->rec_mutex );
    }
#endif
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
	    retval = (PS_RETTYPE)"Ctl2Note";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Ctl2Note преобразует значение контроллера \"Тон\" в ноту (на выходе - команды вкл/выкл ноты).";
                        break;
                    }
		    retval = (PS_RETTYPE)"Ctl2Note converts the value of the \"Pitch\" controller to a note (Note ON/OFF commands at the output).";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFC2BB";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS:
	    retval = PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_GET_STOP_COMMANDS | PSYNTH_FLAG_NO_RENDER;
	    break;
	case PS_CMD_GET_FLAGS2:
	    retval = PSYNTH_FLAG2_NOTE_SENDER | PSYNTH_FLAG2_GET_MUTED_COMMANDS;
	    break;
	case PS_CMD_INIT:
	    {
        	psynth_resize_ctls_storage( mod_num, 10, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PITCH ), "", 0, 32768, 0, 0, &data->ctl_pitch, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FIRST_NOTE ), "", 0, 120, 0, 1, &data->ctl_note, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANGE_SEMITONES ), "", 0, 120, 120, 1, &data->ctl_notes, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_TRANSPOSE ), "", 0, 256, 128, 1, &data->ctl_transpose, 128, 1, pnet );
        	psynth_set_ctl_show_offset( mod_num, 3, -128, pnet );
        	psynth_register_ctl( mod_num, ps_get_string( STR_PS_FINETUNE ), "", 0, 512, 256, 0, &data->ctl_finetune, 256, 1, pnet );
        	psynth_set_ctl_show_offset( mod_num, 4, -256, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_VELOCITY ), "", 0, 256, 256, 0, &data->ctl_vel, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_STATE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_state, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOTEON_MODE ), ps_get_string( STR_PS_NOTEON_MODES ), 0, 1, 1, 1, &data->ctl_noteon, -1, 3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOTEOFF_MODE ), ps_get_string( STR_PS_NOTEOFF_MODES ), 0, 2, 1, 1, &data->ctl_noteoff, -1, 3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RECORD_NOTES ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_rec, -1, 4, pnet ); 
                smem_clear( &data->out_evt, sizeof( data->out_evt ) );
#ifdef SUNVOX_GUI
		data->rec_session_id = -1;
        	data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "Ctl2Note GUI", 0, 0, 1, 1, wm->button_color, 0, pnet, ctl2note_visual_handler, wm );
            	    ctl2note_visual_data* data1 = (ctl2note_visual_data*)mod->visual->data;
            	    mod->visual_min_ysize = data1->min_ysize;
            	    data1->module_data = data;
            	    data1->mod_num = mod_num;
            	    data1->pnet = pnet;
            	}
#endif
	    }
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    data->ctl_state = 0;
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    data->ctl_state = 0;
	    mod->draw_request++;
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	case PS_CMD_STOP:
	    if( data->ctl_state ) ctl2note_send( mod_num, pnet, event->offset, cmd_note_off );
	    mod->draw_request++;
	    retval = 1;
	    break;
	case PS_CMD_MUTED:
	    if( data->ctl_state ) ctl2note_send( mod_num, pnet, event->offset, cmd_note_off );
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
    	    {
    		int prev_state = data->ctl_state;
    		int cmd = -1;
        	psynth_set_ctl2( mod, event );
        	retval = 1;
		if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
    		switch( event->controller.ctl_num )
        	{
        	    case 0: case 1: case 2: case 3: case 4: 
        		if( data->ctl_noteon == 1 )
        		{
        		    if( data->ctl_state )
        			cmd = cmd_note_change;
        		    else
        			cmd = cmd_note_on;
        		}
        		if( data->ctl_noteoff == 1 && data->ctl_pitch == 0 ) cmd = cmd_note_off;
        		if( data->ctl_noteoff == 2 && data->ctl_pitch == 32768 ) cmd = cmd_note_off;
        		break;
        	    case 5: 
        		cmd = cmd_vel_change;
        		break;
        	    case 6: 
        		if( prev_state != data->ctl_state )
        		{
        		    if( data->ctl_state )
        			cmd = cmd_note_on;
        		    else
        			cmd = cmd_note_off;
        		}
        		break;
        	}
        	if( cmd >= 0 ) ctl2note_send( mod_num, pnet, event->offset, cmd );
    	    }
    	    break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = NULL;
    	    }
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
