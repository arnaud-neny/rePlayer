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

#include "sundog.h"
#include "sunvox_engine.h"
void sunvox_send_user_command( sunvox_user_cmd* cmd, sunvox_engine* s ) 
{
    sring_buf_write_lock( s->user_commands );
    sring_buf_write( s->user_commands, cmd, sizeof( sunvox_user_cmd ) );
    sring_buf_write_unlock( s->user_commands );
}
static void sunvox_send_ui_event_kbd( sunvox_kbd_event* evt, bool ftrack_first, int ftrack, sunvox_engine* s )
{
    if( !s->out_ui_events ) return;
    sunvox_ui_evt e;
    e.type = SUNVOX_UI_EVT_KBD;
    e.data.kbd = *evt;
    e.data.kbd.flags |= 
	( ( ftrack << SUNVOX_KBD_FLAG_FTRACK_OFFSET ) & SUNVOX_KBD_FLAG_FTRACK_MASK ) |
	( ftrack_first ? SUNVOX_KBD_FLAG_FTRACK_FIRST : 0 );
    sring_buf_write_lock( s->out_ui_events );
    sring_buf_write( s->out_ui_events, &e, sizeof( e ) );
    sring_buf_write_unlock( s->out_ui_events );
}
static void sunvox_send_ui_event_state( sunvox_state state, sunvox_engine* s )
{
    if( !s->out_ui_events ) return;
    sunvox_ui_evt e;
    e.type = SUNVOX_UI_EVT_STATE;
    e.data.state = state;
    sring_buf_write_lock( s->out_ui_events );
    sring_buf_write( s->out_ui_events, &e, sizeof( e ) );
    sring_buf_write_unlock( s->out_ui_events );
}
static void sunvox_send_ui_event_wmevt( int type, int x, int y, int key, int pressure, sunvox_engine* s )
{
    if( !s->out_ui_events ) return;
    sunvox_ui_evt e;
    e.type = SUNVOX_UI_EVT_WMEVT;
    e.data.wmevt.type = type;
    e.data.wmevt.x = x;
    e.data.wmevt.y = y;
    e.data.wmevt.key = key;
    e.data.wmevt.pressure = pressure;
    sring_buf_write_lock( s->out_ui_events );
    sring_buf_write( s->out_ui_events, &e, sizeof( e ) );
    sring_buf_write_unlock( s->out_ui_events );
}
void sunvox_send_kbd_event( sunvox_kbd_event* evt, sunvox_engine* s ) 
{
    if( !s->kbd ) return;
    int wp = s->kbd->events_wp;
    int rp = s->kbd->events_rp;
    if( ( ( rp - wp ) & ( MAX_KBD_EVENTS - 1 ) ) == 1 ) return; 
    s->kbd->events[ wp ] = *evt;
    wp++;
    wp &= MAX_KBD_EVENTS - 1;
    COMPILER_MEMORY_BARRIER();
    s->kbd->events_wp = wp;
}
void sunvox_add_psynth_event_UNSAFE( int mod_num, psynth_event* evt, sunvox_engine* s )
{
    psynth_module* mod = psynth_get_module( mod_num, s->net );
    if( !mod ) return;
    if( s->psynth_events == NULL )
	s->psynth_events = SMEM_ALLOC2( sunvox_psynth_event, 16 );
    s->psynth_events[ s->psynth_events_count ].mod = mod_num;
    smem_copy( &s->psynth_events[ s->psynth_events_count ].evt, evt, sizeof( psynth_event ) );
    s->psynth_events_count++;
    size_t prev_size = smem_get_size( s->psynth_events ) / sizeof( sunvox_psynth_event );
    if( s->psynth_events_count >= prev_size )
    {
	s->psynth_events = SMEM_RESIZE2( s->psynth_events, sunvox_psynth_event, prev_size + 32 );
    }
}
static uint sunvox_check_speed( int offset, sunvox_engine* s )
{
    uint one_tick = PSYNTH_TICK_SIZE( s->net->sampling_freq, s->bpm );
    if( s->prev_bpm != s->bpm ||
	s->prev_speed != s->speed )
    {
	psynth_event module_evt;
	module_evt.command = PS_CMD_SPEED_CHANGED;
	module_evt.offset = offset;
	module_evt.speed.tick_size = one_tick;
	module_evt.speed.bpm = s->bpm;
	module_evt.speed.ticks_per_line = s->speed;
	for( uint mod_num = 0; mod_num < s->net->mods_num; mod_num++ )
	{
	    uint flags = s->net->mods[ mod_num ].flags;
	    if( ( flags & PSYNTH_FLAG_EXISTS ) && ( flags & PSYNTH_FLAG_GET_SPEED_CHANGES ) )
	    {
		psynth_add_event( mod_num, &module_evt, s->net );
	    }
	}
	s->prev_bpm = s->bpm;
	s->prev_speed = s->speed;
    }
    return one_tick;
}
static void sunvox_reset_timeline_activity( int offset, sunvox_engine* s )
{
    for( int i = 0; i < s->pat_state_size; i++ )
    {
        int p = s->cur_playing_pats[ i ];
        if( p == -1 ) break;
        if( (unsigned)p < (unsigned)s->sorted_pats_num )
        {
            int spat_num = s->sorted_pats[ p ];
            if( (unsigned)spat_num < (unsigned)s->pats_num && s->pats[ spat_num ] )
            {
                sunvox_pattern_info* spat_info = &s->pats_info[ spat_num ];
                spat_info->track_status = 0;
            }
        }
    }
    for( int i = 0; i < s->pat_state_size; i++ )
    {
	sunvox_pattern_state* state = &s->pat_state[ i ];
        if( !state->busy ) continue;
	for( int a = 0; a < MAX_PATTERN_TRACKS; a++ )
	{
	    if( state->track_status & ( 1 << a ) )
	    {
		int m = state->track_module[ a ];
		if( (unsigned)m < s->net->mods_num )
		{
		    psynth_event note_off_evt;
		    note_off_evt.command = PS_CMD_NOTE_OFF;
		    note_off_evt.id = ( i << 16 ) | a;
		    note_off_evt.offset = offset;
		    note_off_evt.note.velocity = 256;
		    psynth_add_event( m, &note_off_evt, s->net );
		}
	    }
	}
    	state->busy = false;
    	state->track_status = 0;
    }
    s->jump_request = false;
}
#define GET_PITCH( out_pitch, nn, xxyy, initial_only ) \
{ \
    int add = 0; \
    if( !initial_only ) add = -mod->finetune - mod->relative_note * 256 + s->pitch_offset; \
    if( nn == NOTECMD_SET_PITCH ) \
    { \
        out_pitch = xxyy + add; \
    } \
    else \
    { \
        out_pitch = ( 7680 * 4 ) - ( nn - 1 ) * 256 + add; \
        if( s->base_version < 0x01080000 ) { if( out_pitch >= 7680 * 4 ) out_pitch = 7680 * 4; } \
    } \
}
static void sunvox_handle_command( 
    int offset, 
    sunvox_note* snote,
    psynth_net* net,
    int pat_num,
    int track_num,
    sunvox_engine* s )
{
    sunvox_track_eff* eff;
    sunvox_pattern_state* state;
    sunvox_pattern_info* pat_info;
    psynth_event module_evt;
    module_evt.offset = offset;
    if( pat_num == SUNVOX_VIRTUAL_PATTERN )
    {
	pat_info = &s->virtual_pat_info;
	state = &s->virtual_pat_state;
	eff = &state->effects[ track_num ];
	if( track_num >= s->virtual_pat_tracks ) s->virtual_pat_tracks = track_num + 1;
	module_evt.id = ( pat_num << 16 ) | track_num;
    }
    else
    {
	if( snote->note == NOTECMD_PREV_TRACK )
	{
	    sunvox_note* snote2 = snote;
	    while( 1 )
	    {
		track_num--;
		if( track_num < 0 ) break;
		snote2 -= 1;
		if( snote2->note != NOTECMD_PREV_TRACK ) break;
	    }
	    if( track_num < 0 ) return;
	}
	pat_info = &s->pats_info[ pat_num ];
	state = &s->pat_state[ pat_info->state_ptr ];
	eff = &state->effects[ track_num ];
	module_evt.id = ( pat_info->state_ptr << 16 ) | track_num;
    }
    int vel = snote->vel;
    uint16_t ctl = snote->ctl;
    int ctl_val = snote->ctl_val;
    int delay_len = 0;
    int note = snote->note;
    uint8_t ctl_eff = ctl & 0x00FF;
    psynth_module* mod = NULL;
    int mod_num = snote->mod - 1;
    if( (unsigned)mod_num < (unsigned)net->mods_num ) mod = &net->mods[ mod_num ];
    bool note_num; 
    if( ( note > 0 && note < 128 ) || note == NOTECMD_SET_PITCH )
	note_num = true;
    else
	note_num = false;
    uint32_t track_bit = 1 << track_num;
    bool vel_handled = false;
    if( eff->flags & EFF_FLAG_TIMER_HANDLING )
    {
	if( eff->flags & EFF_FLAG_TIMER_RETRIG )
	{
	    eff->timer = eff->timer_init;
	}
	else
	{
	    if( eff->flags & EFF_FLAG_TIMER_CUT )
	    {
		note = NOTECMD_NOTE_OFF;
		note_num = false;
	    }
	    if( eff->flags & EFF_FLAG_TIMER_CUT2 )
	    {
		note = 0;
		note_num = false;
		vel = 1;
	    }
	}
    }
    else
    {
	if( ctl_eff )
	{
	    if( ctl_eff == 0x1D && ctl_val ) 
	    {
		goto delay;
	    }
	    if( ( ctl_eff > 0x40 ) && ( ctl_eff < 0x60 ) )
	    {
		delay_len = ctl_eff;
		goto delay;
	    }
	    switch( ctl_eff )
	    {
		case 0x20:
		    {
			int r = pseudo_random( &s->rand_next );
			r &= 0x7FFF;
			if( r >= ctl_val )
			{
			    note_num = false;
			    note = 0;
			    vel = 0;
			}
		    }
		    break;
		case 0x21:
		    {
			int r = pseudo_random( &s->rand_next );
			r &= 0x7FFF;
			if( r >= ctl_val )
			{
			    note_num = false;
			    note = 0;
			    vel = 0;
			}
			else
			{
			    if( vel )
				vel = ( ( ( vel - 1 ) * r ) / 0x8000 ) + 1;
			    else
				vel = ( r >> 8 ) + 1;
			}
		    }
		    break;
		case 0x22:
		    {
			if( ctl_val > 32768 ) ctl_val = 32768;
			uint r = pseudo_random( &s->rand_next );
			r &= 0x7FFF;
			r = ctl_val * r;
			ctl_val = r / 32768;
			if( ( r & 32767 ) > 16384 ) ctl_val++;
		    }
		    break;
		case 0x23:
		    {
			uint r = pseudo_random( &s->rand_next );
			r &= 0x7FFF;
			int ctl_type = 0;
			psynth_ctl* ctl_ptr = 0;
			if( mod ) 
			{
			    uint ctl_num = ( ( ctl >> 8 ) & 0xFF ) - 1;
			    if( ctl_num < mod->ctls_num )
			    {
			        ctl_ptr = &mod->ctls[ ctl_num ];
			        ctl_type = ctl_ptr->type;
			    }
			}
		    	uint v1 = ( ctl_val >> 8 ) & 255;
			uint v2 = ctl_val & 255;
			if( v1 > v2 ) { uint t = v1; v1 = v2; v2 = t; }
			if( ctl_type == 0 )
			{
			    uint range = v2 - v1;
			    v1 = ( v1 * 32768 ) / 255;
			    v2 = ( range * 32768 ) / 255;
			    r = v2 * r;
			    v2 = r / 32768;
			    if( ( r & 32767 ) > 16384 ) v2++;
			}
			else
			{
			    if( v1 < (unsigned)ctl_ptr->min ) v1 = ctl_ptr->min; else { if( v1 > (unsigned)ctl_ptr->max ) v1 = ctl_ptr->max; }
			    if( v2 < (unsigned)ctl_ptr->min ) v2 = ctl_ptr->min; else { if( v2 > (unsigned)ctl_ptr->max ) v2 = ctl_ptr->max; }
			    uint range = v2 - v1;
			    r = range * r;
			    v2 = r / 32768;
			    if( ( r & 32767 ) > 16384 ) v2++;
			}
			ctl_val = v1 + v2;
		    }
		    break;
		case 0x24: 
		case 0x25: 
		case 0x26: 
		case 0x27: 
		case 0x28: 
		case 0x29: 
		    if( pat_num != SUNVOX_VIRTUAL_PATTERN )
		    {
			int pos = 0;
			if( ctl_eff & 1 )
			{
			    int r = pseudo_random( &s->rand_next ) & 32767;
		    	    int v1 = ( ctl_val >> 8 ) & 255;
			    int v2 = ctl_val & 255;
			    if( v1 > v2 ) { int t = v1; v1 = v2; v2 = t; }
			    int range = v2 - v1;
			    pos = ( ( ( range + 1 ) * r ) >> 15 ) + v1;
			}
			else
			{
			    pos = ctl_val;
			}
			sunvox_pattern* pat = s->pats[ pat_num ];
			sunvox_note* snote2 = NULL;
			if( ( ctl_eff >> 1 ) & 1 )
			{
			    if( pos < pat->channels ) snote2 = snote - track_num + pos;
			}
			else
			{
			    if( pos < pat->lines ) 
			    {
				if( ctl_eff < 0x28 )
				    snote2 = &pat->data[ track_num + pos * pat->data_xsize ];
				else
				    snote2 = &pat->data[ pos * pat->data_xsize ]; 
			    }
			}
			bool no_note = true;
			if( snote2 )
			{
			    if( ( snote2->note > 0 && snote2->note < 128 ) || snote2->note == NOTECMD_SET_PITCH )
			    {
				no_note = false;
				int p = 0;
				GET_PITCH( p, snote2->note, snote2->ctl_val, true );
				if( note_num ) p -= ( note - 1 - 12 * 5 ) * 256; 
				if( mod == 0 )
				{
				    mod_num = snote2->mod - 1;
				    if( (unsigned)mod_num < (unsigned)net->mods_num ) mod = &net->mods[ mod_num ];
				}
				note_num = true;
				note = NOTECMD_SET_PITCH;
				ctl = 0;
				ctl_val = p;
			    }
			    else
			    {
				if( snote2->note == NOTECMD_NOTE_OFF )
				{
				    no_note = false;
				    note = NOTECMD_NOTE_OFF;
				    ctl = 0;
				    ctl_val = 0;
				}
			    }
			}
			if( no_note )
			{
			    note_num = false;
			    note = 0;
			    vel = 0;
			}
		    }
		    break;
		case 0x30:
		    {
			note_num = false;
			note = NOTECMD_STOP;
		    }
		    break;
	    }
	}
    }
    if( note_num )
    {
	if( mod ) 
	{
	    GET_PITCH( eff->evt_pitch0, note, ctl_val, false );
	    eff->evt_pitch = eff->evt_pitch0;
	    bool eff_not_allow_noteon = false;
	    while( 1 )
	    {
		switch( ctl_eff )
		{
		    case 0x3: eff_not_allow_noteon = true; break;
		    case 0x5: eff->evt_pitch -= ctl_val; break;
		    case 0x6: eff->evt_pitch += ctl_val; break;
		    default: break;
		}
		if( pat_num == SUNVOX_VIRTUAL_PATTERN ) break;
		sunvox_pattern* pat = s->pats[ pat_num ];
		sunvox_note* snote2 = snote + 1;
		for( int i = track_num + 1; i < pat->channels; i++, snote2++ )
		{
		    if( snote2->note != NOTECMD_PREV_TRACK ) break;
		    switch( snote2->ctl & 0xFF )
		    {
			case 0x3: eff_not_allow_noteon = true; break;
			case 0x5: eff->evt_pitch -= snote2->ctl_val; break;
			case 0x6: eff->evt_pitch += snote2->ctl_val; break;
			default: break;
		    }
		}
		break;
	    }
	    if( eff_not_allow_noteon == false )
	    {
		if( vel )
		    module_evt.note.velocity = ( vel - 1 ) * 2;
		else
		    module_evt.note.velocity = 256;
		if( s->velocity != 256 ) module_evt.note.velocity = ( (int)module_evt.note.velocity * (int)s->velocity ) / (int)256;
		eff->cur_vel = module_evt.note.velocity;
		vel_handled = true;
		if( note == NOTECMD_SET_PITCH )
		    module_evt.note.root_note = ( PS_NOTE0_PITCH - eff->evt_pitch ) / 256;
		else
	    	    module_evt.note.root_note = note - 1;
		eff->cur_pitch = eff->evt_pitch;
		if( ( state->track_status & track_bit ) &&
		    state->track_module[ track_num ] != mod_num )
		{
		    int mod_num2 = state->track_module[ track_num ];
		    module_evt.command = PS_CMD_NOTE_OFF;
		    psynth_add_event( mod_num2, &module_evt, net );
		}
		module_evt.command = PS_CMD_NOTE_ON;
		module_evt.note.pitch = eff->evt_pitch;
		psynth_add_event( mod_num, &module_evt, net );
		state->track_module[ track_num ] = mod_num;
		state->track_status |= track_bit;
		pat_info->track_status |= track_bit;
		eff->vib_phase = 0;
	    }
	}
    }
    switch( note )
    {
	case NOTECMD_PLAY:
	    {
		uint one_tick = PSYNTH_TICK_SIZE( s->net->sampling_freq, s->bpm );
		s->playing = 1;
		s->tick_counter = one_tick;
		if( pat_num == SUNVOX_VIRTUAL_PATTERN && ctl_eff == 0x1D )
		{
		    int delay_frames = 2;
		    int one_tick2 = (int)one_tick - delay_frames * 256;
		    while( one_tick2 < 0 ) one_tick2 += one_tick;
		    s->tick_counter = one_tick2;
		}
		s->speed_counter = s->speed - 1;
		s->single_pattern_play = (int)ctl_val - 1;
		s->next_single_pattern_play = -1;
		for( uint i = 0; i < net->mods_num; i++ )
		{
		    if( ( net->mods[ i ].flags & ( PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_GET_PLAY_COMMANDS ) ) == ( PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_GET_PLAY_COMMANDS ) )
		    {
			module_evt.command = PS_CMD_PLAY;
			psynth_add_event( i, &module_evt, net );
		    }
		}
		sunvox_send_ui_event_state( SUNVOX_STATE_PLAY, s );
	    }
	    break;
	case NOTECMD_STOP:
	    {
		sunvox_reset_timeline_activity( offset, s );
	        s->playing = 0;
		s->single_pattern_play = -1;
		s->next_single_pattern_play = -1;
		for( uint i = 0; i < net->mods_num; i++ )
		{
		    if( ( net->mods[ i ].flags & ( PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_GET_STOP_COMMANDS ) ) == ( PSYNTH_FLAG_EXISTS | PSYNTH_FLAG_GET_STOP_COMMANDS ) )
		    {
			module_evt.command = PS_CMD_STOP;
			psynth_add_event( i, &module_evt, net );
		    }
		}
		sunvox_send_ui_event_state( SUNVOX_STATE_STOP, s );
	    }
	    break;
	case NOTECMD_ALL_NOTES_OFF:
	    {
		for( uint i = 0; i < net->mods_num; i++ )
		{
		    if( net->mods[ i ].flags & PSYNTH_FLAG_EXISTS )
		    {
			module_evt.command = PS_CMD_ALL_NOTES_OFF;
			psynth_add_event( i, &module_evt, net );
		    }
		}
	    }
	    break;
	case NOTECMD_NOTE_OFF:
	    {
		int mod_num2 = state->track_module[ track_num ];
		if( (unsigned)mod_num2 < net->mods_num )
		{
		    if( state->track_status & track_bit )
		    {
			module_evt.command = PS_CMD_NOTE_OFF;
			if( vel )
			    module_evt.note.velocity = ( vel - 1 ) * 2;
			else
			    module_evt.note.velocity = 256;
			if( s->velocity != 256 ) module_evt.note.velocity = ( (int)module_evt.note.velocity * (int)s->velocity ) / (int)256;
			psynth_add_event( mod_num2, &module_evt, net );
			state->track_status &= ~track_bit;
			pat_info->track_status &= ~track_bit;
		    }
		}
	    }
	    break;
	case NOTECMD_CLEAN_MODULES:
	    {
		for( uint i = 0; i < net->mods_num; i++ )
		{
		    if( net->mods[ i ].flags & PSYNTH_FLAG_EXISTS )
		    {
			module_evt.command = PS_CMD_CLEAN;
			psynth_add_event( i, &module_evt, net );
		    }
		}
		if( s->kbd )
		{
		    s->kbd->slots_count = 0;
		    for( int i = 0; i < MAX_KBD_SLOTS; i++ ) s->kbd->slots[ i ].active = false;
		}
    		clean_pattern_state( &s->virtual_pat_state, s ); s->virtual_pat_tracks = 0;
    		if( s->kbd ) s->kbd->prev_track = -1;
		sunvox_send_ui_event_state( SUNVOX_STATE_CLEAN_MODULES, s );
	    }
	    break;
	case NOTECMD_CLEAN_MODULE:
	    {
		int mod_num2 = mod_num;
		if( mod_num2 < 0 )
		{
		    mod_num2 = state->track_module[ track_num ];
		}
		module_evt.command = PS_CMD_CLEAN;
		psynth_add_event( mod_num2, &module_evt, net );
	    }
	    break;
	case NOTECMD_PATPLAY_OFF:
	    {
		s->single_pattern_play = -1;
		s->next_single_pattern_play = -1;
		sunvox_send_ui_event_state( SUNVOX_STATE_PATPLAY_OFF, s );
	    }
	    break;
	case NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP:
	case NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2:
	    {
		if( s->playing )
                {
		    sunvox_note snote2;
		    SMEM_CLEAR_STRUCT( snote2 );
		    snote2.note = NOTECMD_STOP;
		    sunvox_handle_command( offset, &snote2, net, pat_num, track_num, s );
		}
		uint one_tick = PSYNTH_TICK_SIZE( net->sampling_freq, s->bpm );
		s->tick_counter = one_tick; 
	        sunvox_set_position( note == NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP ? ctl_val : -ctl_val, s );
	        s->line_counter--;
	        s->speed_counter = s->speed - 1;
	        s->jump_request = false;
	    }
	    break;
	case NOTECMD_JUMP:
	    {
		if( s->playing )
                {
		    sunvox_note snote2;
		    SMEM_CLEAR_STRUCT( snote2 );
		    snote2.note = NOTECMD_STOP;
		    sunvox_handle_command( offset, &snote2, net, pat_num, track_num, s );
		    snote2.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
                    snote2.ctl_val = ctl_val;
		    sunvox_handle_command( offset, &snote2, net, pat_num, track_num, s );
		    snote2.note = NOTECMD_PLAY;
                    snote2.ctl_val = 0;
		    sunvox_handle_command( offset, &snote2, net, pat_num, track_num, s );
                }
                else
                {
                    sunvox_set_position( ctl_val, s );
                }
	    }
	    break;
    }
    if( vel && vel_handled == false )
    {
	int mod_num2 = state->track_module[ track_num ];
	if( (unsigned)mod_num2 < net->mods_num )
	{
	    module_evt.note.velocity = ( vel - 1 ) * 2;
	    if( s->velocity != 256 ) module_evt.note.velocity = ( (int)module_evt.note.velocity * (int)s->velocity ) / (int)256;
	    eff->cur_vel = module_evt.note.velocity;
	    module_evt.command = PS_CMD_SET_VELOCITY;
	    psynth_add_event( mod_num2, &module_evt, net );
	    vel_handled = true;
	}
    }
    if( ctl & 0xFF00 )
    {
	int mod_num2 = mod_num;
	module_evt.command = PS_CMD_SET_LOCAL_CONTROLLER;
	if( mod_num2 >= 0 )
	{
	    if( note == 0 )
	    {
		module_evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
	    }
	}
	else
	{
	    mod_num2 = state->track_module[ track_num ];
	}
	if( (unsigned)mod_num2 < (unsigned)net->mods_num )
	{
	    module_evt.controller.ctl_num = ( ( ctl >> 8 ) & 0xFF ) - 1;
	    module_evt.controller.ctl_val = ctl_val;
	    psynth_add_event( mod_num2, &module_evt, net );
	}
    }
delay:
    if( eff->flags & EFF_FLAG_TIMER_HANDLING ) return; 
    if( note_num ) eff->flags |= EFF_FLAG_EVT_PITCH;
    while( ctl_eff )
    {
	int mod_num2 = mod_num;
	if( mod_num2 < 0 )
	{
	    mod_num2 = state->track_module[ track_num ];
	}
	if( delay_len > 0 ) 
	{
	    eff->timer = delay_len;
	    eff->flags |= EFF_FLAG_TIMER_DELAY;
	    break;
	}
	switch( ctl_eff )
	{
	    case 0x1:
		eff->flags |= EFF_FLAG_TONE_PORTA;
		eff->target_pitch = -7680 * 4;
		if( ctl_val )
		    eff->porta_speed = ctl_val * 4;
		break;
	    case 0x2:
		eff->flags |= EFF_FLAG_TONE_PORTA;
    		if( s->base_version < 0x01080000 )
    		    eff->target_pitch = 7680 * 4; 
    		else
    		    eff->target_pitch = 7680 * 4 * 2; 
		if( ctl_val )
		    eff->porta_speed = ctl_val * 4;
		break;
	    case 0x3:
		eff->flags |= EFF_FLAG_TONE_PORTA;
		if( ctl_val )
		    eff->porta_speed = ctl_val * 4;
		if( eff->flags & EFF_FLAG_EVT_PITCH )
		{
		    eff->target_pitch = eff->evt_pitch;
		    if( note == NOTECMD_SET_PITCH )
		    {
		        int delta = abs( eff->target_pitch - eff->cur_pitch );
    			if( s->base_version < 0x01090600 )
		    	    delta /= 6;
		    	else
		    	    delta /= s->speed;
		        if( delta == 0 ) delta = 1;
		        eff->porta_speed = delta;
		    }
		}
		break;
	    case 0x4:
		eff->flags |= EFF_FLAG_TONE_VIBRATO;
		if( ctl_val == 0 && s->base_version != 0x01070000 )
		    break; 
		eff->vib_freq = ( ctl_val >> 8 ) & 255;
		eff->vib_amp = ctl_val & 255;
		break;
	    case 0x5:
	    case 0x6:
		{
		    int new_evt_pitch = eff->evt_pitch0;
		    if( ctl_eff == 0x5 )
			new_evt_pitch -= ctl_val;
		    else
			new_evt_pitch += ctl_val;
		    if( new_evt_pitch != eff->evt_pitch )
		    {
			int diff = new_evt_pitch - eff->evt_pitch;
			eff->evt_pitch += diff;
			eff->cur_pitch += diff;
			eff->target_pitch += diff;
			eff->flags |= EFF_FLAG_PITCH_CHANGED;
		    }
		}
		break;
	    case 0x8:
		eff->arpeggio = (uint16_t)ctl_val;
		break;
	    case 0x9:
		module_evt.sample_offset.sample_offset = (uint)ctl_val * 256;
		module_evt.command = PS_CMD_SET_SAMPLE_OFFSET;
		psynth_add_event( mod_num2, &module_evt, net );
		break;
	    case 0x7:
		module_evt.sample_offset.sample_offset = (uint)ctl_val;
		module_evt.command = PS_CMD_SET_SAMPLE_OFFSET2;
		psynth_add_event( mod_num2, &module_evt, net );
		break;
	    case 0xA:
		if( ctl_val & 0xFF00 ) eff->vel_speed = ctl_val >> 8;
		if( ctl_val & 0xFF ) eff->vel_speed = -( ctl_val & 0xFF );
		break;
	    case 0xF:
		if( ctl_val < 32 ) 
		{
		    int prev_speed = s->speed;
		    s->speed = (uint8_t)ctl_val;
		    if( s->speed == 0 ) s->speed = 1;
		    if( prev_speed != s->speed )
		    {
			s->change_counter++;
			s->pars_changed++;
		    }
		}
		else
		{
		    if( ctl_val >= 0xF000 && ctl_val <= 0xF1FF )
		    {
			if( ctl_val < 0xF100 )
			{
			    s->tgrid = ctl_val & 255;
			    if( s->tgrid < 2 ) s->tgrid = 2;
			}
			else
			{
			    s->tgrid2 = ctl_val & 255;
			    if( s->tgrid2 < 2 ) s->tgrid2 = 2;
			}
			s->tgrid_changed++;
			s->pars_changed++;
			s->change_counter++;
		    }
		    else
		    {
			int prev_bpm = s->bpm;
			s->bpm = (uint16_t)ctl_val; 
			if( s->bpm == 0 ) s->bpm = 1;
			if( s->bpm > 16000 ) s->bpm = 16000;
			if( prev_bpm != s->bpm )
			{
			    s->change_counter++;
			    s->pars_changed++;
			}
		    }
		}
		break;
	    case 0x11:
	    case 0x12:
		if( ctl_val )
		    eff->porta_speed = ctl_val * 4;
		if( ctl_eff == 0x11 )
		{
		    eff->cur_pitch -= eff->porta_speed;
		    if( eff->cur_pitch < -7680 * 4 ) eff->cur_pitch = -7680 * 4;
		}
		else
		{
		    eff->cur_pitch += eff->porta_speed;
		    if( eff->cur_pitch > 7680 * 4 ) eff->cur_pitch = 7680 * 4;
		}
		eff->flags |= EFF_FLAG_PITCH_CHANGED;
		break;
	    case 0x13:
	    case 0x14:
		module_evt.mute.flags = 0;
		if( ctl_val & 0x001 )
		    module_evt.mute.flags |= PSYNTH_EVENT_MUTE;
		if( ctl_val & 0x010 )
		    module_evt.mute.flags |= PSYNTH_EVENT_SOLO;
		if( ctl_val & 0x100 )
		    module_evt.mute.flags |= PSYNTH_EVENT_BYPASS;
		if( ctl_eff == 0x13 )
		    module_evt.command = PS_CMD_SET_MSB;
		else
		    module_evt.command = PS_CMD_RESET_MSB;
		psynth_add_event( mod_num2, &module_evt, net );
		break;
	    case 0x15:
		if( ctl_val )
		{
		    module_evt.finetune.finetune = -512;
		    module_evt.finetune.relative_note = -512;
		    int v = ctl_val & 255;
		    if( v ) module_evt.finetune.finetune = ( v - 128 ) * 2;
		    v = ( ctl_val >> 8 ) & 255;
		    if( v ) module_evt.finetune.relative_note = v - 128;
		    module_evt.command = PS_CMD_FINETUNE;
		    psynth_add_event( mod_num2, &module_evt, net );
		}
		break;
	    case 0x19:
		eff->timer = ctl_val & 31;
		eff->timer_init = eff->timer;
		if( eff->timer > 0 ) 
		{
		    eff->timer++;
		    eff->flags |= EFF_FLAG_TIMER_RETRIG;
		}
		break;
	    case 0x1A:
		{
		    int prev = eff->cur_vel;
		    if( ctl_val & 0xFF00 )
		    {
			eff->cur_vel += ctl_val >> 8;
			if( eff->cur_vel > 256 ) eff->cur_vel = 256;
		    }
		    if( ctl_val & 0xFF )
		    {
			eff->cur_vel -= ctl_val & 0xFF;
			if( eff->cur_vel < 0 ) eff->cur_vel = 0;
		    }
		    if( prev != eff->cur_vel )
		    {
			eff->flags |= EFF_FLAG_VEL_CHANGED;
		    }
		}
		break;
	    case 0x1C:
		eff->timer = ( ctl_val & 31 ) + 1;
		if( ( ctl_val & 0xF000 ) == 0x1000 )
		    eff->flags |= EFF_FLAG_TIMER_CUT2; 
		else
		    eff->flags |= EFF_FLAG_TIMER_CUT; 
		break;
	    case 0x1D:
		eff->timer = ctl_val & 31;
		if( eff->timer > 0 ) 
		{
		    eff->timer++;
		    eff->flags |= EFF_FLAG_TIMER_DELAY;
		}
		break;
	    case 0x1F:
		{
		    int prev_bpm = s->bpm;
		    s->bpm = (uint16_t)ctl_val;
		    if( s->bpm == 0 ) s->bpm = 1;
		    if( s->bpm > 16000 ) s->bpm = 16000;
		    if( prev_bpm != s->bpm )
		    {
			s->pars_changed++;
			s->change_counter++;
		    }
		}
		break;
	    case 0x31:
	        if( ( s->flags & SUNVOX_FLAG_IGNORE_EFF31 ) == 0 )
	        {
	    	    switch( s->jump_request_mode )
		    {
	    		case 1: ctl_val = pat_info->x + ctl_val; break;
	    		case 2: ctl_val = pat_info->x - ctl_val; break;
			case 3: ctl_val = s->line_counter + 1 + ctl_val; break;
			case 4: ctl_val = s->line_counter + 1 - ctl_val; break;
			default: break;
	    	    }
		    s->jump_request_line = ctl_val;
	    	    s->jump_request = true;
	    	}
		break;
	    case 0x32:
	        s->jump_request_mode = ctl_val;
	        break;
	    case 0x33:
	        if( ( s->flags & SUNVOX_FLAG_IGNORE_SLOT_SYNC ) == 0 )
	        {
	    	    s->stream_control_par_sync = s->level1_offset + offset;
	    	    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_SYNC );
	    	}
	        break;
	    case 0x34:
		{
		    int set = ( ctl_val >> 8 ) & 0xFF;
		    int reset = ctl_val & 0xFF;
		    uint32_t prev_flags = s->flags;
		    bool prev_7bit = net->midi_out_7bit;
		    if( set & 1 ) s->flags |= SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0;
		    if( set & 2 ) s->flags |= SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0;
		    if( set & 4 ) s->flags |= SUNVOX_FLAG_KBD_ROUNDROBIN;
		    if( set & 8 ) net->midi_out_7bit = 1;
		    if( reset & 1 ) s->flags &= ~SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0;
		    if( reset & 2 ) s->flags &= ~SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0;
		    if( reset & 4 ) s->flags &= ~SUNVOX_FLAG_KBD_ROUNDROBIN;
		    if( reset & 8 ) net->midi_out_7bit = 0;
		    if( s->flags != prev_flags ) s->change_counter++;
		    if( net->midi_out_7bit != prev_7bit ) s->change_counter++;
		}
		break;
	    case 0x35:
	        {
		    module_evt.command = PS_CMD_MIDIMSG_SUPPORT;
		    module_evt.midimsg_support.msg = ( ctl_val >> 8 ) & 255;
		    module_evt.midimsg_support.ctl = ctl_val & 255;
		    psynth_add_event( mod_num2, &module_evt, net );
		}
	        break;
	    case 0x38:
		if( pat_num != SUNVOX_VIRTUAL_PATTERN )
		{
		    int r = pseudo_random( &s->rand_next );
		    r &= 0x7FFF;
		    if( ( r >> 7 ) <= ( ctl_val & 255 ) )
		    {
			int tn = ( ctl_val >> 8 ) & 255;
			sunvox_pattern* pat = s->pats[ pat_num ];
			if( tn < pat->channels )
			{
			    sunvox_note* snote2 = snote - track_num + tn;
			    memset( snote2, 0, sizeof( sunvox_note ) );
			    s->change_counter++;
			}
		    }
		}
		break;
	    case 0x39:
		sunvox_pattern_shift( pat_num, ( ctl_val >> 8 ) & 255, 0, 0, ctl_val & 255, 0, s );
		s->change_counter++;
		break;
	    case 0x3A:
		if( pat_num != SUNVOX_VIRTUAL_PATTERN )
		{
		    int tn = ( ctl_val >> 8 ) & 255;
		    int len = ctl_val & 255;
		    sunvox_pattern* pat = s->pats[ pat_num ];
		    if( len && len < pat->lines )
		    {
			int d = pat->lines % len;
			if( d )
			{
			    int offset = len - d;
			    sunvox_pattern_shift( pat_num, tn, 0, 0, offset, len, s );
			    s->change_counter++;
			}
		    }
		}
		break;
	    case 0x3B: 
	    case 0x3C: 
		if( pat_num != SUNVOX_VIRTUAL_PATTERN )
		{
		    int tn = ( ctl_val >> 8 ) & 255;
		    int pat_name = ctl_val & 255;
		    char name[ 3 ];
		    name[ 0 ] = int_to_hchar( ( pat_name >> 4 ) & 15 );
		    name[ 1 ] = int_to_hchar( pat_name & 15 );
		    name[ 2 ] = 0;
		    sunvox_pattern* src_pat = NULL;
		    sunvox_pattern* dest_pat = NULL;
		    if( ctl_eff == 0x3B )
		    {
			src_pat = s->pats[ pat_num ];
			int dest_pat_num = sunvox_get_pattern_num_by_name( name, s );
			dest_pat = sunvox_get_pattern( dest_pat_num, s );
		    }
		    else
		    {
			int src_pat_num = sunvox_get_pattern_num_by_name( name, s );
			src_pat = sunvox_get_pattern( src_pat_num, s );
			dest_pat = s->pats[ pat_num ];
		    }
		    if( src_pat && dest_pat )
		    {
			if( tn < src_pat->channels && tn < dest_pat->channels )
			{
			    sunvox_note* src = &src_pat->data[ tn ];
			    sunvox_note* dest = &dest_pat->data[ tn ];
			    int ysize = src_pat->lines;
			    if( ysize > dest_pat->lines ) ysize = dest_pat->lines;
			    for( int l = 0; l < ysize; l++ )
			    {
				*dest = *src;
				src += src_pat->data_xsize;
				dest += dest_pat->data_xsize;
			    }
			}
		    }
		    s->change_counter++;
		}
		break;
	    case 0x3D: 
		if( pat_num != SUNVOX_VIRTUAL_PATTERN )
		{
		    sunvox_pattern* pat = s->pats[ pat_num ];
		    int tn = ctl_val & 255;
		    int pars_line = ( ctl_val >> 8 ) & 255;
		    if( pars_line + 3 <= pat->lines && tn < pat->channels )
		    {
			sunvox_note* pars = &pat->data[ pars_line * pat->data_xsize ];
			int min = pars->ctl_val; pars += pat->data_xsize;
			int max = pars->ctl_val; pars += pat->data_xsize;
			int col = pars->ctl_val;
			int r = pseudo_random( &s->rand_next );
			if( min > max ) { int t = min; min = max; max = t; }
			uint16_t v = ( max - min + 1 ) * r / 32768 + min;
			sunvox_note* snote2 = snote - track_num + tn;
			switch( col )
			{
			    case 0: 
				if( v > 127 ) v = 127;
				snote2->note = v;
				break;
			    case 1: 
				if( v > 128 ) v = 128;
				snote2->vel = v + 1;
				break;
			    case 2: snote2->mod = v + 1; break; 
			    case 3: snote2->ctl = ( snote2->ctl & 0x00FF ) | ( ( v & 255 ) << 8 ); break; 
			    case 4: snote2->ctl = ( snote2->ctl & 0xFF00 ) | ( v & 255 ); break; 
			    case 5: snote2->ctl_val = ( snote2->ctl_val & 0x00FF ) | ( ( v & 255 ) << 8 ); break; 
			    case 6: snote2->ctl_val = ( snote2->ctl_val & 0xFF00 ) | ( v & 255 ); break; 
			    case 7: snote2->ctl_val = v; break; 
			}
			s->change_counter++;
		    }
		}
		break;
	}
	break;
    }
}
static void sunvox_reset_track_effect( sunvox_track_eff* eff )
{
    eff->flags &= EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK;
    eff->vel_speed = 0;
    eff->arpeggio = 0;
}
static void sunvox_handle_track_effects( 
    int offset,
    psynth_net* net,
    int pat_num,
    int track_num,
    sunvox_engine* s )
{
    sunvox_track_eff* eff;
    sunvox_pattern_state* state;
    sunvox_pattern_info* pat_info;
    int evt_id = track_num;
    if( pat_num == SUNVOX_VIRTUAL_PATTERN ) 
    {
	pat_info = &s->virtual_pat_info;
	state = &s->virtual_pat_state;
	eff = &s->virtual_pat_state.effects[ track_num ];
	evt_id |= pat_num << 16;
    }
    else
    {
	pat_info = &s->pats_info[ pat_num ];
	state = &s->pat_state[ pat_info->state_ptr ];
	eff = &s->pat_state[ pat_info->state_ptr ].effects[ track_num ];
	evt_id |= pat_info->state_ptr << 16;
	if( eff->timer )
	{
	    if( eff->timer > 0x40 )
	    {
		int new_timer = ( (int)eff->timer - 0x40 ) * s->speed / 32;
		eff->timer = new_timer + 1;
	    }
	    eff->timer--;
	    if( eff->timer == 0 &&
		( eff->flags & ( EFF_FLAG_TIMER_RETRIG | EFF_FLAG_TIMER_CUT | EFF_FLAG_TIMER_CUT2 | EFF_FLAG_TIMER_DELAY ) ) )
	    {
		eff->flags |= EFF_FLAG_TIMER_HANDLING;
		sunvox_pattern* pat = s->pats[ pat_num ];
		int line_num = s->line_counter - pat_info->x;
		if( line_num >= 0 && line_num < pat->lines && track_num < pat->channels )
		{
		    sunvox_note* snote = &pat->data[ line_num * pat->data_xsize + track_num ];
		    sunvox_handle_command( 
			offset,
			snote,
			net,
			pat_num,
			track_num,
			s );
		}
		eff->flags &= ~EFF_FLAG_TIMER_HANDLING;
	    }
	}
    }
    int mod_num = state->track_module[ track_num ];
    if( (unsigned)mod_num >= net->mods_num ) return;
    int pitch = eff->cur_pitch;
    if( eff->flags & EFF_FLAG_TONE_PORTA )
    {
        if( eff->cur_pitch != eff->target_pitch )
        {
    	    if( !( ( s->flags & SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0 ) && s->speed_counter == 0 ) )
    	    {
		if( eff->cur_pitch < eff->target_pitch )
    		{
    		    eff->cur_pitch += eff->porta_speed;
		    if( eff->cur_pitch > eff->target_pitch ) eff->cur_pitch = eff->target_pitch;
		}
		else
		{
		    eff->cur_pitch -= eff->porta_speed;
		    if( eff->cur_pitch < eff->target_pitch ) eff->cur_pitch = eff->target_pitch;
		}
		pitch = eff->cur_pitch;
		eff->flags |= EFF_FLAG_PITCH_CHANGED;
	    }
	}
    }
    if( ( eff->flags & EFF_FLAG_TONE_VIBRATO ) && eff->vib_amp )
    {
	int phase = eff->vib_phase / 256;
	int v = g_hsin_tab[ phase & 255 ];
	if( phase & 256 ) v = -v;
	pitch += ( v * eff->vib_amp ) / 64;
	eff->vib_phase += ( ( (int)eff->vib_freq * ( 512 / 32 ) ) * 256 ) / s->speed;
	eff->flags |= EFF_FLAG_PITCH_CHANGED;
    }
    if( eff->arpeggio )
    {
	switch( s->speed_counter % 3 )
	{
	    case 1: pitch -= ( ( eff->arpeggio >> 8 ) & 255 ) * 256; break;
	    case 2: pitch -= ( eff->arpeggio & 255 ) * 256; break;
	}
	eff->flags |= EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK;
	eff->flags |= EFF_FLAG_PITCH_CHANGED;
    }
    else if( eff->flags & EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK )
    {
	eff->flags &= ~( EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK );
	eff->flags |= EFF_FLAG_PITCH_CHANGED;
    }
    if( eff->flags & EFF_FLAG_PITCH_CHANGED )
    {
	eff->flags &= ~EFF_FLAG_PITCH_CHANGED;
	if( s->base_version < 0x01080000 )
	{
    	    if( pitch > 7680 * 4 ) pitch = 7680 * 4;
    	}
	psynth_event module_evt;
	module_evt.command = PS_CMD_SET_FREQ;
	module_evt.id = evt_id;
	module_evt.offset = offset;
	module_evt.note.pitch = pitch;
	module_evt.note.velocity = eff->cur_vel;
	psynth_add_event( mod_num, &module_evt, net );
    }
    if( eff->vel_speed )
    {
    	if( !( ( s->flags & SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0 ) && s->speed_counter == 0 ) )
        {
    	    int prev = eff->cur_vel;
	    eff->cur_vel += eff->vel_speed;
	    if( eff->cur_vel > 256 ) eff->cur_vel = 256;
	    if( eff->cur_vel < 0 ) eff->cur_vel = 0;
	    if( eff->cur_vel != prev )
	    {
		eff->flags |= EFF_FLAG_VEL_CHANGED;
	    }
    	}
    }
    if( eff->flags & EFF_FLAG_VEL_CHANGED )
    {
	eff->flags &= ~EFF_FLAG_VEL_CHANGED;
	psynth_event module_evt;
	module_evt.command = PS_CMD_SET_VELOCITY;
	module_evt.id = evt_id;
	module_evt.offset = offset;
	module_evt.note.velocity = eff->cur_vel;
    	psynth_add_event( mod_num, &module_evt, net );
    }
}
static int get_free_kbd_track( sunvox_engine* s )
{
    int free_track = s->kbd->prev_track;
    bool track_found = false;
    for( int cc = 0; cc < MAX_PATTERN_TRACKS; cc++ )
    {
        free_track = ( free_track + 1 ) & ( MAX_PATTERN_TRACKS - 1 );
        if( ( s->virtual_pat_state.track_status & ( 1 << free_track ) ) == 0 ) { track_found = true; break; }
    }
    s->kbd->prev_track = free_track;
    if( track_found ) return free_track;
    return -1;
}
enum kbd_slot_mode
{
    create_slot,
    remove_slot
};
static sunvox_kbd_slot* sunvox_get_kbd_slot( sunvox_kbd_event* evt, kbd_slot_mode mode, sunvox_engine* s )
{
    sunvox_kbd_slot* slot = NULL;
    sunvox_kbd_events* kbd = s->kbd;
    if( kbd->slots_count == 0 )
    {
	if( mode == create_slot )
	{
	    slot = &kbd->slots[ 0 ];
	    kbd->slots_count = 1;
	}
    }
    else
    {
	int vtype = ( evt->flags & SUNVOX_KBD_FLAG_VTYPE_MASK ) >> SUNVOX_KBD_FLAG_VTYPE_OFFSET;
	uint mask = SUNVOX_KBD_FLAG_TYPE_MASK | SUNVOX_KBD_FLAG_VTYPE_MASK | SUNVOX_KBD_FLAG_CHANNEL_MASK;
	if( vtype == SUNVOX_KBD_VTYPE_PITCH )
	{
	    for( uint i = 0; i < kbd->slots_count; i++ )
	    {
		sunvox_kbd_slot* tslot = &kbd->slots[ i ];
		if( tslot->active && ( tslot->flags & mask ) == ( evt->flags & mask ) )
		{
		    if( ( evt->flags & SUNVOX_KBD_FLAG_SPECIFIC_MODULE_ONLY ) && tslot->mod != evt->mod ) continue;
		    slot = tslot;
		    break;
		}
	    }
	}
	else
	{
	    for( uint i = 0; i < kbd->slots_count; i++ )
	    {
		sunvox_kbd_slot* tslot = &kbd->slots[ i ];
		if( tslot->active && ( tslot->flags & mask ) == ( evt->flags & mask ) && tslot->v == evt->v )
		{
		    if( ( evt->flags & SUNVOX_KBD_FLAG_SPECIFIC_MODULE_ONLY ) && tslot->mod != evt->mod ) continue;
		    slot = tslot;
		    break;
		}
	    }
	}
	if( slot )
	{
	    if( mode == remove_slot )
	    {
		slot->active = false;
		int i;
		for( i = kbd->slots_count - 1; i >= 0; i-- )
		{
		    if( kbd->slots[ i ].active ) break;
		}
		kbd->slots_count = i + 1;
	    }
	}
	else
	{
	    if( mode == create_slot )
	    {
		for( uint i = 0; i < MAX_KBD_SLOTS; i++ )
		{
		    if( kbd->slots[ i ].active == false )
		    {
			slot = &kbd->slots[ i ];
			if( i >= kbd->slots_count )
			    kbd->slots_count = i + 1;
			break;
		    }
		}
	    }
	}
    }
    return slot;
}
static void sunvox_handle_kbd_event( 
    int offset,
    sunvox_kbd_event* evt, 
    int mod_num, 
    sunvox_engine* s )
{
    if( !s->kbd ) return;
    int ch = ( evt->flags & SUNVOX_KBD_FLAG_CHANNEL_MASK ) >> SUNVOX_KBD_FLAG_CHANNEL_OFFSET; 
    if( mod_num > 0 )
    {
	bool can_handle = false;
	psynth_module* mod = psynth_get_module( mod_num, s->net );
	if( mod )
	{
	    int mod_ch = ( mod->midi_in_flags & PSYNTH_MIDI_IN_FLAG_CHANNEL_MASK ) >> PSYNTH_MIDI_IN_FLAG_CHANNEL_OFFSET;
	    if( ( ( mod->midi_in_flags & PSYNTH_MIDI_IN_FLAG_ALWAYS_ACTIVE ) || mod_num == s->last_selected_generator ) &&
		( mod_ch == 0 || mod_ch - 1 == ch ) &&
		( mod->midi_in_flags & PSYNTH_MIDI_IN_FLAG_NEVER ) == 0 )
	    {
		can_handle = true;
	    }
	}
	if( !can_handle )
	    return;
	evt->mod = mod_num;
    }
    bool ftrack_first = false; 
    int ftrack = 0; 
    while( 1 )
    {
	if( s->flags & SUNVOX_FLAG_KBD_ROUNDROBIN )
	{
	}
	else
	{
	    {
		if( !( evt->flags & SUNVOX_KBD_FLAG_ON ) )
		{
		    s->kbd->prev_track = -1;
		}
	    }
	}
	int vtype = ( evt->flags & SUNVOX_KBD_FLAG_VTYPE_MASK ) >> SUNVOX_KBD_FLAG_VTYPE_OFFSET;
	if( vtype == SUNVOX_KBD_VTYPE_NOTE )
	{
	    if( evt->flags & SUNVOX_KBD_FLAG_ON )
	    {
		sunvox_kbd_slot* slot = sunvox_get_kbd_slot( evt, create_slot, s );
		if( !slot ) break;
		if( slot->active && ( evt->flags & SUNVOX_KBD_FLAG_NOREPEAT ) )
		{
		    ftrack = slot->track;
		}
		else
		{
		    if( slot->active )
		    {
		        if( slot->track >= 0 )
		        {
			    sunvox_note snote;
			    snote.note = NOTECMD_NOTE_OFF;
			    snote.vel = 0;
			    snote.mod = 0;
			    snote.ctl = 0;
			    snote.ctl_val = 0;
			    sunvox_handle_command( 
			        offset,
			        &snote, s->net, SUNVOX_VIRTUAL_PATTERN,
			        slot->track,
			        s );
			}
		    }
		    ftrack_first = s->virtual_pat_state.track_status == 0;
		    ftrack = get_free_kbd_track( s );
		    slot->v = evt->v;
		    slot->mod = evt->mod;
		    slot->flags = evt->flags;
		    slot->track = ftrack;
		    slot->active = true;
		    if( ftrack >= 0 )
		    {
			uint vel2 = evt->vel * (uint)s->kbd->vel / 128;
			if( vel2 > 128 ) vel2 = 128;
			sunvox_note snote;
		    	snote.note = evt->v + 1;
		    	snote.vel = vel2 + 1;
		    	snote.mod = evt->mod + 1;
		    	snote.ctl = 0;
		    	snote.ctl_val = 0;
		    	sunvox_reset_track_effect( &s->virtual_pat_state.effects[ ftrack ] );
		    	sunvox_handle_command( 
		    	    offset,
			    &snote, s->net, SUNVOX_VIRTUAL_PATTERN,
			    ftrack,
			    s );
		    }
		}
	    }
	    else 
	    {
		sunvox_kbd_slot* slot = sunvox_get_kbd_slot( evt, remove_slot, s );
		if( slot && slot->track >= 0 )
		{
		    ftrack = slot->track;
		    sunvox_note snote;
		    snote.note = NOTECMD_NOTE_OFF;
		    snote.vel = 0;
		    snote.mod = 0;
		    snote.ctl = 0;
		    snote.ctl_val = 0;
		    sunvox_reset_track_effect( &s->virtual_pat_state.effects[ ftrack ] );
		    sunvox_handle_command( 
			offset,
			&snote, s->net, SUNVOX_VIRTUAL_PATTERN,
			ftrack,
			s );
		}
	    }
	    break;
	}
	if( vtype == SUNVOX_KBD_VTYPE_PITCH )
	{
	    if( evt->flags & SUNVOX_KBD_FLAG_ON )
	    {
		sunvox_kbd_slot* slot = sunvox_get_kbd_slot( evt, create_slot, s );
		if( !slot ) break;
		bool start = false;
		if( slot->active && slot->track >= 0 ) 
		{
		    start = false;
		    ftrack = slot->track;
		}
		else
		{
		    start = true;
		    ftrack_first = s->virtual_pat_state.track_status == 0;
		    ftrack = get_free_kbd_track( s );
		}
		slot->v = evt->v;
		slot->mod = evt->mod;
		slot->flags = evt->flags;
		slot->track = ftrack;
		slot->active = true;
		if( ftrack >= 0 )
		{
		    uint vel2 = evt->vel * (uint)s->kbd->vel / 128;
		    if( vel2 > 128 ) vel2 = 128;
		    sunvox_note snote;
		    snote.note = NOTECMD_SET_PITCH;
		    snote.vel = vel2 + 1;
		    snote.mod = evt->mod + 1;
		    if( start )
		        snote.ctl = 0; 
		    else
		        snote.ctl = 3; 
		    snote.ctl_val = evt->v;
		    sunvox_reset_track_effect( &s->virtual_pat_state.effects[ ftrack ] );
		    sunvox_handle_command( 
		        offset,
		        &snote, s->net, SUNVOX_VIRTUAL_PATTERN,
		        ftrack,
		        s );
		}
	    }
	    else
	    {
		sunvox_kbd_slot* slot = sunvox_get_kbd_slot( evt, remove_slot, s );
		if( slot && slot->track >= 0 )
		{
		    ftrack = slot->track;
		    sunvox_note snote;
		    snote.note = NOTECMD_NOTE_OFF;
		    snote.vel = 0;
		    snote.mod = 0;
		    snote.ctl = 0;
		    snote.ctl_val = 0;
		    sunvox_reset_track_effect( &s->virtual_pat_state.effects[ ftrack ] );
		    sunvox_handle_command( 
			offset,
			&snote, s->net, SUNVOX_VIRTUAL_PATTERN,
			ftrack,
			s );
		}
	    }
	    break;
	}
	break;
    }
    sunvox_send_ui_event_kbd( evt, ftrack_first, ftrack, s );
}
void sunvox_handle_all_commands_UNSAFE( sunvox_engine* s )
{
    sunvox_render_data rdata;
    SMEM_CLEAR_STRUCT( rdata );
    int16_t buf[ 1 * 2 ];
    rdata.buffer_type = sound_buffer_int16;
    rdata.buffer = buf;
    rdata.frames = 1;
    rdata.channels = 2;
    rdata.out_time = stime_ticks();
    s->flags |= SUNVOX_FLAG_IGNORE_SLOT_SYNC;
    sunvox_render_piece_of_sound( &rdata, s );
    s->flags &= ~SUNVOX_FLAG_IGNORE_SLOT_SYNC;
}
uint8_t g_metronome_click[ 16 ] = { 32, 0, 128, 32, 200, 180, 170, 100, 90, 200, 100, 32, 11, 25, 70, 4 };
static bool sunvox_render_piece_of_sound_level2(
    sunvox_render_data* rdata, 
    int f_current_buffer, 
    sunvox_engine* s )
{
    bool rv = false;
    window_manager* wm = NULL;
    if( s->win ) wm = s->win->wm;
    int ptr = 0;
    uint one_tick = 0;
    int metronome = -1;
    int metronome_shift = 0;
    int freq = s->freq;
    int frames = rdata->frames;
    int channels = rdata->channels;
    psynth_reset_events( s->net );
    int f_off = SUNVOX_VF_BUF_SRATE * f_current_buffer;
    int ptr2_step = ( freq << 8 ) / SUNVOX_VF_BUF_SRATE;
    int f_prev_size = s->f_buffer_size[ f_current_buffer ];
    int f_new_size = 0;
    one_tick = sunvox_check_speed( ptr, s );
    int ml = SUNDOG_MIDI_LATENCY( rdata->out_latency, rdata->out_latency2 );
    stime_ticks_t evt_latency = ( ( ( ml << 14 ) / freq ) * stime_ticks_per_second() ) >> 14;
    stime_ticks_t out_time;
#if defined(OS_IOS) && defined(AUDIOBUS)
#ifndef SUNDOG_MODULE
    if( !( s->flags & SUNVOX_FLAG_NO_GLOBAL_SYS_EVENTS ) && ( s->sync_flags & SUNVOX_SYNC_FLAG_OTHER1 ) )
    {
	if( g_snd_rewind_request )
	{
	    sunvox_user_cmd cmd;
	    SMEM_CLEAR_STRUCT( cmd );
	    cmd.n.note = NOTECMD_JUMP;
	    sunvox_send_user_command( &cmd, s );
	    g_snd_rewind_request = 0;
	}
	if( g_snd_play_request )
	{
	    sunvox_user_cmd cmd;
	    SMEM_CLEAR_STRUCT( cmd );
	    if( s->line_counter >= 0 )
	    {
		cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
		cmd.n.ctl_val = s->line_counter;
	    }
	    else
	    {
		cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2;
		cmd.n.ctl_val = -s->line_counter;
	    }
	    sunvox_send_user_command( &cmd, s );
	    cmd.n.ctl_val = 0;
	    cmd.n.note = NOTECMD_PLAY;
	    sunvox_send_user_command( &cmd, s );
	    g_snd_play_request = 0;
	}
	if( g_snd_stop_request )
	{
	    sunvox_user_cmd cmd;
	    SMEM_CLEAR_STRUCT( cmd );
	    cmd.n.note = NOTECMD_STOP;
	    sunvox_send_user_command( &cmd, s );
	    g_snd_stop_request = 0;
	}
    }
#endif
#endif
    while( 1 )
    {
	int size = frames - ptr;
	if( s->sync_mode )
	{
	    if( !( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI2 ) ) s->sync_mode = 0;
	    if( size > s->sync_resolution ) size = s->sync_resolution;
	}
	else
	{
	    if( size > (int)( ( one_tick - s->tick_counter ) / 256 ) ) size = ( one_tick - s->tick_counter ) / 256;
	    if( ( one_tick - s->tick_counter ) & 255 ) size++; 
	}
	if( size > frames - ptr ) size = frames - ptr;
	if( size < 0 ) size = 0;
	{
	    int t = ( s->line_counter * 32 ) + ( s->speed_counter * 32 ) / s->speed; 
	    f_new_size = ( ( ( ( s->level1_offset + ptr + size ) * 256 ) / freq ) * SUNVOX_VF_BUF_SRATE ) / 256;
	    if( f_new_size <= 0 ) f_new_size = 1;
	    if( f_new_size > SUNVOX_VF_BUF_SRATE ) f_new_size = SUNVOX_VF_BUF_SRATE;
	    for( int fp = s->f_buffer_size[ f_current_buffer ]; fp < f_new_size; fp ++ )
	    {
		s->f_line[ f_off + fp ] = t;
	    }
	    s->f_buffer_size[ f_current_buffer ] = f_new_size;
	}
        out_time = rdata->out_time + ( ( s->level1_offset + ptr ) * stime_ticks_per_second() ) / freq;
	if( s->kbd )
	{
	    while( s->kbd->events_rp != s->kbd->events_wp )
	    {
		sunvox_kbd_event* evt = &s->kbd->events[ s->kbd->events_rp ];
        	if( evt->t == 0 || ( ( out_time - ( evt->t + evt_latency ) ) & 0x80000000 ) == 0 )
		{
		    sunvox_handle_kbd_event( ptr, evt, 0, s );
		    int rp = ( s->kbd->events_rp + 1 ) & ( MAX_KBD_EVENTS - 1 );
		    s->kbd->events_rp = rp;
		}
		else 
		{
		    break;
		}
	    }
	}
#ifndef NOMIDI
	sunvox_midi* midi = s->midi;
	while( !( s->flags & SUNVOX_FLAG_NO_MIDI ) )
	{
            int k;
            sundog_midi_event* midi_evt;
            for( k = SUNVOX_MIDI_IN_PORTS - 1; k >= 0; k-- )
            {
        	midi_evt = sundog_midi_client_get_event( &s->net->midi_client, midi->kbd[ k ] );
        	if( midi_evt )
        	{
		    if( midi_evt->t == 0 || ( ( out_time - ( midi_evt->t + evt_latency ) ) & 0x80000000 ) == 0 )
		    {
        		break;
        	    }
        	    else
        	    {
        	    }
        	}
    	    }
    	    if( k < 0 ) break;
    	    bool accept_sync = ( midi->kbd_sync < 0 || midi->kbd_sync == k );
	    int midi_data_cnt = 0;
	    uint8_t midi_data[ 12 ];
	    const int print_midi_bytes = 0; 
	    for( int i = 0; i < midi_evt->size; i++ )
    	    {
		uint8_t b = midi_evt->data[ i ];
		if( print_midi_bytes == 1 ) slog( "%02x ", b );
		if( print_midi_bytes == 2 ) printf( "%02x ", b );
		if( b & 0x80 )
		{
		    midi->kbd_status[ k ] = b;
		    midi_data_cnt = 0;
            	    if( b >= 0xF0 )
                    {
                        switch( b & 15 )
                        {
                    	    case 0x8: 
                    		if( accept_sync && ( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI2 ) )
                    		{
                    		    s->sync_mode = 1;
                    		    s->sync_cnt++;
                    		}
                    		break;
                            case 0xA: 
                            case 0xB: 
                    		if( accept_sync && ( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI1 ) )
                                {
                            	    if( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI2 ) s->sync_mode = 1;
                                    sunvox_user_cmd cmd;
				    SMEM_CLEAR_STRUCT( cmd );
                                    cmd.t = midi_evt->t;
                                    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
                                    if( ( midi->kbd_status[ k ] & 15 ) == 0xA )
                                        cmd.n.ctl_val = 0;
                                    else
                                    {
                                	if( s->line_counter >= 0 )
                                    	    cmd.n.ctl_val = s->line_counter;
                                    	else
                                    	{
                                    	    cmd.n.ctl_val = -s->line_counter;
                                	    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2;
                                    	}
                                    }
                                    sunvox_send_user_command( &cmd, s );
                                    cmd.n.ctl_val = 0;
                                    cmd.n.note = NOTECMD_PLAY;
                                    sunvox_send_user_command( &cmd, s );
                                }
                                break;
                            case 0xC: 
                    		if( accept_sync && ( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI1 ) )
                                {
                    		    s->sync_mode = 0;
                                    sunvox_user_cmd cmd;
				    SMEM_CLEAR_STRUCT( cmd );
                                    cmd.t = midi_evt->t;
                            	    cmd.n.note = NOTECMD_STOP;
                                    sunvox_send_user_command( &cmd, s );
                                }
                                break;
                        }
		    }
		}
		else
		{
            	    if( midi->kbd_status[ k ] >= 0xF0 )
                    {
                        if( midi_data_cnt < 12 )
                        {
                            midi_data[ midi_data_cnt ] = b;
                    	    midi_data_cnt++;
                    	}
                        switch( midi->kbd_status[ k ] & 15 )
                        {
                    	    case 0:
                            	if( midi_data_cnt == 8 )
                                {
                                    midi_data_cnt = 0;
                            	    if( midi_data[ 0 ] != 0x6E ) break;
                            	    if( midi_data[ 1 ] == 0x6E )
                            	    {
                            	    }
                            	    if( midi_data[ 1 ] == 0x6D )
                            	    {
                            		sunvox_user_cmd cmd;
					SMEM_CLEAR_STRUCT( cmd );
                                	cmd.t = midi_evt->t;
                            		int arg1 = midi_data[ 2 ];
                            		union { uint32_t i; float f; } v;
                            		v.i =
                            		    ( (uint32_t)midi_data[ 3 ] << 28 ) |
                            		    ( (uint32_t)midi_data[ 4 ] << 21 ) |
                            		    ( (uint32_t)midi_data[ 5 ] << 14 ) |
                            		    ( (uint32_t)midi_data[ 6 ] << 7 ) |
                            		    ( (uint32_t)midi_data[ 7 ] << 0 );
                            		float arg2 = v.f;
                            		switch( arg1 )
                            		{
                            		    case 1: 
                            		    case 2: 
                            		        if( s->sync_flags & SUNVOX_SYNC_FLAG_OTHER1 )
                            		        {
		                                    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
		                                    if( arg1 == 1 )
		                                    {
                			                cmd.n.ctl_val = 0;
                			            }
		                                    else
		                                    {
                			                cmd.n.ctl_val = s->line_counter;
                                			if( s->line_counter >= 0 )
                                    	    		    cmd.n.ctl_val = s->line_counter;
                                    			else
                                    			{
                                    			    cmd.n.ctl_val = -s->line_counter;
                                			    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2;
                                    			}
                			            }
		                                    sunvox_send_user_command( &cmd, s );
                			            cmd.n.ctl_val = 0;
		                                    cmd.n.note = NOTECMD_PLAY;
		                                    sunvox_send_user_command( &cmd, s );
                            			}
                            		    case 0:
                            		        if( arg2 && ( s->sync_flags & SUNVOX_SYNC_FLAG_OTHER2 ) )
                            			{
		                                    cmd.n.note = 0;
                            			    cmd.n.ctl = 0x1F;
                            			    cmd.n.ctl_val = (uint16_t)arg2;
                                    		    sunvox_send_user_command( &cmd, s );
                            			}
                            			break;
                            		    case 3:
                            		        if( s->sync_flags & SUNVOX_SYNC_FLAG_OTHER1 )
                            		        {
            			                    cmd.n.note = NOTECMD_STOP;
                            			    sunvox_send_user_command( &cmd, s );
                            		        }
                            		        break;
                            		    case 4:
                            		        if( s->sync_flags & SUNVOX_SYNC_FLAG_OTHER3 )
                            		        {
						    int pos = arg2 * SUNVOX_TPB / s->speed;
            			                    cmd.n.note = NOTECMD_JUMP;
            			                    cmd.n.ctl_val = pos;
                            			    sunvox_send_user_command( &cmd, s );
                            			}
                            			break;
                            		}
                            	    }
                                }
                    		break;
                    	    case 2:
                    		if( accept_sync && ( s->sync_flags & SUNVOX_SYNC_FLAG_MIDI3 ) )
                    		{
                            	    if( midi_data_cnt == 2 )
                                    {
                                    	midi_data_cnt = 0;
                                	int pos = ( (int)midi_data[ 0 ] + ( (int)midi_data[ 1 ] << 7 ) ) * ( SUNVOX_TPB / 4 );
                                	pos /= s->speed;
			    		sunvox_user_cmd cmd;
					SMEM_CLEAR_STRUCT( cmd );
					cmd.n.note = NOTECMD_JUMP;
					cmd.n.ctl_val = pos;
				    	sunvox_send_user_command( &cmd, s );
                                    }
                    		}
                    		break;
                        }
                    }
                    else 
                    {
                        if( midi->kbd_ch[ k ] == -1 || ( midi->kbd_status[ k ] & 15 ) == midi->kbd_ch[ k ] )
                        {
                            if( midi_data_cnt < 12 )
                            {
                                midi_data[ midi_data_cnt ] = b;
                        	midi_data_cnt++;
                    	    }
                            psynth_midi_evt pmidi_evt;
                            pmidi_evt.type = psynth_midi_none;
                            pmidi_evt.ch = midi->kbd_status[ k ] & 15;
                            switch( ( midi->kbd_status[ k ] >> 4 ) & 7 )
                            {
                                case 0:
                                    if( midi_data_cnt == 2 )
                                    {
                                	pmidi_evt.type = psynth_midi_note;
                                	pmidi_evt.val1 = midi_data[ 0 ];
                                	pmidi_evt.val2 = 0;
                                	if( wm )
                                	{
                                	    sunvox_send_ui_event_wmevt(
                                		EVT_BUTTONUP,
                                		pmidi_evt.val1, 
                                		pmidi_evt.ch, 
                                		KEY_MIDI_NOTE, 
                                		1024, 
                                		s );
                                	}
                                        sunvox_kbd_event evt;
                                        SMEM_CLEAR_STRUCT( evt );
					int v = midi_data[ 0 ] + midi->kbd_octave_offset * 12;
					if( v >= 0 && v <= 127 )
					{
					    if( !keymap_midi_note_assigned( 0, midi_data[ 0 ], pmidi_evt.ch, wm ) )
					    {
						evt.v = v;
						evt.vel = midi_data[ 1 ];
						evt.flags = 
						    ( SUNVOX_KBD_TYPE_MIDI << SUNVOX_KBD_FLAG_TYPE_OFFSET ) |
						    ( ( pmidi_evt.ch << SUNVOX_KBD_FLAG_CHANNEL_OFFSET ) & SUNVOX_KBD_FLAG_CHANNEL_MASK );
						evt.t = midi_evt->t;
						sunvox_handle_kbd_event( ptr, &evt, 0, s );
						if( s->net->midi_in_mods )
						{
						    for( uint mm = 0; mm < s->net->midi_in_mods_num; mm++ )
						    {
							int mod_num = s->net->midi_in_mods[ mm ];
							if( mod_num != 0 && mod_num != s->last_selected_generator )
							{
							    evt.flags |= SUNVOX_KBD_FLAG_SPECIFIC_MODULE_ONLY;
							    sunvox_handle_kbd_event( ptr, &evt, mod_num, s );
							}
						    }
						}
					    }
					}
                                    	midi_data_cnt = 0;
                            		midi->kbd_prev_ctl[ k ] = -1;
                                    }
                                    break;
                                case 1:
                                    if( midi_data_cnt == 2 )
                                    {
                                	pmidi_evt.type = psynth_midi_note;
                                	pmidi_evt.val1 = midi_data[ 0 ];
                                	if( midi_data[ 1 ] == 0 )
                                	{
                                	    pmidi_evt.val2 = 0;
                                	}
                                	else
                                	{
                                	    if( midi->kbd_ignore_velocity )
                                		pmidi_evt.val2 = 127;
                                	    else
                                		pmidi_evt.val2 = midi_data[ 1 ];
                                	}
                                	if( wm )
                                	{
                                	    int wm_evt_type;
                                	    int wm_evt_pressure;
                                	    if( midi_data[ 1 ] != 0 )
                                		wm_evt_type = EVT_BUTTONDOWN;
                                	    else
                                		wm_evt_type = EVT_BUTTONUP;
                                	    if( midi_data[ 1 ] == 127 )
                                		wm_evt_pressure = 1024;
                                	    else
                                		wm_evt_pressure = midi_data[ 1 ] << 3;
                                	    sunvox_send_ui_event_wmevt( 
                                		wm_evt_type,
                                		pmidi_evt.val1, 
                                		pmidi_evt.ch, 
                                		KEY_MIDI_NOTE, 
                                		wm_evt_pressure, 
                                		s );
                                	}
                                        sunvox_kbd_event evt;
                                        SMEM_CLEAR_STRUCT( evt );
					int v = midi_data[ 0 ] + midi->kbd_octave_offset * 12;
					if( v >= 0 && v <= 127 )
					{
					    if( !keymap_midi_note_assigned( 0, midi_data[ 0 ], pmidi_evt.ch, wm ) )
					    {
						evt.flags = 
						    ( SUNVOX_KBD_TYPE_MIDI << SUNVOX_KBD_FLAG_TYPE_OFFSET ) |
						    ( ( pmidi_evt.ch << SUNVOX_KBD_FLAG_CHANNEL_OFFSET ) & SUNVOX_KBD_FLAG_CHANNEL_MASK );
						evt.v = v;
						evt.t = midi_evt->t;
						if( midi_data[ 1 ] != 0 )
						{
						    if( midi->kbd_ignore_velocity )
						    {
					    		evt.vel = 128;
					    	    }
						    else
						    {
					        	evt.vel = (int)midi_data[ 1 ] * 128 / (int)127;
					    	    }
					    	    evt.flags |= SUNVOX_KBD_FLAG_ON;
						}
						sunvox_handle_kbd_event( ptr, &evt, s->last_selected_generator, s );
						if( s->net->midi_in_mods )
						{
						    for( uint mm = 0; mm < s->net->midi_in_mods_num; mm++ )
						    {
							int mod_num = s->net->midi_in_mods[ mm ];
							if( mod_num != 0 && mod_num != s->last_selected_generator )
							{
							    evt.flags |= SUNVOX_KBD_FLAG_SPECIFIC_MODULE_ONLY;
							    sunvox_handle_kbd_event( ptr, &evt, mod_num, s );
							}
						    }
						}
					    }
					}
                                        midi_data_cnt = 0;
                            		midi->kbd_prev_ctl[ k ] = -1;
                                    }
                                    break;
                                case 2:
                                    if( midi_data_cnt == 2 )
                                    {
                                	pmidi_evt.type = psynth_midi_key_pressure;
                                	pmidi_evt.val1 = midi_data[ 0 ]; 
                                	pmidi_evt.val2 = midi_data[ 1 ]; 
                                        midi_data_cnt = 0;
                            		midi->kbd_prev_ctl[ k ] = -1;
                            	    }
                                    break;
                                case 3:
                            	    if( midi_data_cnt == 2 )
                                    {
                                	pmidi_evt.type = psynth_midi_ctl;
                                	pmidi_evt.val1 = midi_data[ 0 ];
                                	pmidi_evt.val2 = midi_data[ 1 ];
                                    	int prev_ctl = midi->kbd_prev_ctl[ k ];
                                    	switch( midi_data[ 0 ] )
                                    	{
                                    	    case 6: 
                                    		midi->kbd_ctl_data[ k ] = -1;
                                    		if( prev_ctl == 98 && midi->kbd_ctl_nrpn[ k ] != 16383 )
                                    		{
                                    		    midi->kbd_ctl_data[ k ] = midi_data[ 1 ];
                                		    pmidi_evt.type = psynth_midi_nrpn;
                                		    pmidi_evt.val1 = midi->kbd_ctl_nrpn[ k ];
                                		    pmidi_evt.val2 = midi->kbd_ctl_data[ k ];
                                    		}
                                    		if( prev_ctl == 100 && midi->kbd_ctl_rpn[ k ] != 16383 )
                                    		{
                                    		    midi->kbd_ctl_data[ k ] = midi_data[ 1 ];
                                		    pmidi_evt.type = psynth_midi_rpn;
                                		    pmidi_evt.val1 = midi->kbd_ctl_rpn[ k ];
                                		    pmidi_evt.val2 = midi->kbd_ctl_data[ k ];
                                    		}
                                    		break;
                                    	    case 38: 
                                    		if( prev_ctl == 6 )
                                    		{
                                    		}
                                    		break;
                                    	    case 99: 
                                    		midi->kbd_ctl_nrpn[ k ] = midi_data[ 1 ] << 7;
                                    		break;
                                    	    case 98: 
                                    		if( prev_ctl == 99 )
                                    		    midi->kbd_ctl_nrpn[ k ] |= midi_data[ 1 ];
                                    		else
                                    		    midi->kbd_ctl_nrpn[ k ] = 16383;
                                    		break;
                                    	    case 101: 
                                    		midi->kbd_ctl_rpn[ k ] = midi_data[ 1 ] << 7;
                                    		break;
                                    	    case 100: 
                                    		if( prev_ctl == 101 )
                                    		    midi->kbd_ctl_rpn[ k ] |= midi_data[ 1 ];
                                    		else
                                    		    midi->kbd_ctl_rpn[ k ] = 16383;
                                    		break;
                                    	}
                                	if( wm )
                                	{
                                	    int pp;
                                	    if( pmidi_evt.val2 & psynth_midi_val_14bit_flag )
                                		pp = pmidi_evt.val2 >> 7;
                                	    else
                                		pp = pmidi_evt.val2;
                                	    if( pp >= 127 )
                                	    {
                                		int wm_evt_key = 0;
                                		switch( pmidi_evt.type )
                                		{
                                		    case psynth_midi_ctl: wm_evt_key = KEY_MIDI_CTL; break;
                                		    case psynth_midi_nrpn: wm_evt_key = KEY_MIDI_NRPN; break;
	                                	    case psynth_midi_rpn: wm_evt_key = KEY_MIDI_RPN; break;
	                                	    default: break;
						}
						if( wm_evt_key )
						{
                                		    sunvox_send_ui_event_wmevt( EVT_BUTTONDOWN, pmidi_evt.val1, pmidi_evt.ch, wm_evt_key, 1024, s );
                                		    sunvox_send_ui_event_wmevt( EVT_BUTTONUP, pmidi_evt.val1, pmidi_evt.ch, wm_evt_key, 1024, s );
                                		}
                                	    }
                                	}
                                    	midi_data_cnt = 0;
                                	midi->kbd_prev_ctl[ k ] = midi_data[ 0 ];
                                    }
                            	    break;
                                case 4:
                            	    if( midi_data_cnt == 1 )
                                    {
                                	pmidi_evt.type = psynth_midi_prog;
                                	pmidi_evt.val1 = midi_data[ 0 ];
                                	pmidi_evt.val2 = 0;
                                	if( wm )
                                	{
                                	    sunvox_send_ui_event_wmevt( EVT_BUTTONDOWN, pmidi_evt.val1, pmidi_evt.ch, KEY_MIDI_PROG, 1024, s );
                                	    sunvox_send_ui_event_wmevt( EVT_BUTTONUP, pmidi_evt.val1, pmidi_evt.ch, KEY_MIDI_PROG, 1024, s );
                                	}
                                    	midi_data_cnt = 0;
                                	midi->kbd_prev_ctl[ k ] = -1;
                                    }
                            	    break;
                                case 5:
                            	    if( midi_data_cnt == 1 )
                                    {
                                	pmidi_evt.type = psynth_midi_channel_pressure;
                                	pmidi_evt.val1 = midi_data[ 0 ];
                                	pmidi_evt.val2 = 0;
                                    	midi_data_cnt = 0;
                            		midi->kbd_prev_ctl[ k ] = -1;
                                    }
                            	    break;
                                case 6:
                            	    if( midi_data_cnt == 2 )
                                    {
                                	pmidi_evt.type = psynth_midi_pitch;
                                	pmidi_evt.val1 = (uint)midi_data[ 0 ] + ( (uint)midi_data[ 1 ] << 7 ) + psynth_midi_val_14bit_flag;
                                	pmidi_evt.val2 = 0;
                                    	midi_data_cnt = 0;
                            		midi->kbd_prev_ctl[ k ] = -1;
                                    }
                            	    break;
                            }
                            if( pmidi_evt.type != psynth_midi_none )
                            {
                        	if( midi->kbd_cap_cnt < midi->kbd_cap_req )
                        	{
                        	    if( pmidi_evt.type >= psynth_midi_note && pmidi_evt.type <= psynth_midi_pitch )
                        	    {
                            		midi->kbd_cap[ midi->kbd_cap_cnt ] = pmidi_evt;
                            		midi->kbd_cap_cnt++;
                            	    }
                            	}
                            	psynth_handle_ctl_midi_in( &pmidi_evt, ptr, s->net );
			    }
                        }
                    }
		} 
	    }
	    if( print_midi_bytes == 1 ) slog( "\n" );
	    if( print_midi_bytes == 2 ) printf( "\n" );
	    sundog_midi_client_next_event( &s->net->midi_client, midi->kbd[ k ] );
	}
#endif
	bool jump_to_start_of_main_loop = 0;
	bool buf_locked = 0;
	while( sring_buf_avail( s->user_commands ) >= sizeof( sunvox_user_cmd ) )
	{
	    if( buf_locked == 0 )
	    {
		buf_locked = 1;
		sring_buf_read_lock( s->user_commands );
	    }
	    sunvox_user_cmd cmd;
	    if( sring_buf_read( s->user_commands, &cmd, sizeof( cmd ) ) == sizeof( cmd ) )
	    {
        	if( cmd.t == 0 || ( ( out_time - ( cmd.t + evt_latency ) ) & 0x80000000 ) == 0 )
        	{
        	    if( cmd.ch < MAX_PATTERN_TRACKS )
        	    {
        		sunvox_reset_track_effect( &s->virtual_pat_state.effects[ cmd.ch ] );
			sunvox_handle_command( ptr, &cmd.n, s->net, SUNVOX_VIRTUAL_PATTERN, cmd.ch, s );
		    }
		    sring_buf_next( s->user_commands, sizeof( cmd ) );
		    if( cmd.n.note == NOTECMD_PLAY ) 
		    {
			jump_to_start_of_main_loop = 1;
			break;
		    }
		}
		else
		{
		    break;
		}
	    }
	    else
	    {
		break;
	    }
	}
    	if( buf_locked )
	{
    	    buf_locked = 0;
	    sring_buf_read_unlock( s->user_commands );
	}
	if( jump_to_start_of_main_loop ) continue; 
	if( s->psynth_events )
	{
	    for( uint i = 0; i < s->psynth_events_count; i++ )
	    {
		psynth_event* evt = &s->psynth_events[ i ].evt;
		evt->offset = 0;
		psynth_add_event( s->psynth_events[ i ].mod, evt, s->net );
	    }
	    s->psynth_events_count = 0;
	}
#if defined(OS_IOS) && defined(AUDIOBUS)
#ifndef SUNDOG_MODULE
	if( !( s->flags & SUNVOX_FLAG_NO_GLOBAL_SYS_EVENTS ) )
	{
	    if( s->playing )
		g_snd_play_status = 1;
	    else
		g_snd_play_status = 0;
	}
#endif
#endif
	ptr += size;
	int handle_tick = 0;
	if( s->sync_mode )
	{
	    handle_tick = s->sync_cnt;
	    s->sync_cnt = 0;
	}
	else
	{
	    s->tick_counter += 256 * size;
	    if( s->tick_counter >= one_tick )
	    {
		s->tick_counter %= one_tick;
		handle_tick = 1;
	    }
	}
	while( handle_tick )
	{
	    handle_tick--;
	    if( s->playing == 1 )
	    {
		s->speed_counter++;
		if( s->speed_counter >= s->speed )
		{
		    s->speed_counter = 0;
		    int new_line_counter = s->line_counter + 1;
		    if( s->jump_request )
		    {
			new_line_counter = s->jump_request_line;
			s->jump_request = false;
		    }
		    int pnum = s->single_pattern_play;
		    if( pnum >= 0 )
		    {
			if( (unsigned)pnum < (unsigned)s->pats_num &&
			    s->pats[ pnum ] )
			{
			    if( new_line_counter >= s->pats_info[ pnum ].x + s->pats[ pnum ]->lines )
			    {
				if( s->next_single_pattern_play >= 0 &&
				    s->next_single_pattern_play != pnum )
				{
				    s->single_pattern_play = s->next_single_pattern_play;
				}
				new_line_counter = s->pats_info[ s->single_pattern_play ].x;
			    }
			}
		    }
		    if( new_line_counter >= s->proj_lines )
		    {
			if( s->stop_at_the_end_of_proj )
			{
			    sunvox_note temp_note;
			    temp_note.note = NOTECMD_STOP;
			    temp_note.vel = 0;
			    temp_note.mod = 0;
			    temp_note.ctl = 0;
			    temp_note.ctl_val = 0;
			    sunvox_handle_command( ptr, &temp_note, s->net, SUNVOX_VIRTUAL_PATTERN, 0, s );
			    break;
			}
			if( s->recording == 0 )
			    new_line_counter = s->restart_pos;
		    }
		    s->line_counter++;
		    if( new_line_counter != s->line_counter )
		    {
                        sunvox_reset_timeline_activity( ptr, s );
			s->line_counter = new_line_counter;
			sunvox_select_current_playing_patterns( 0, s );
		    }
		    int p = 0;
		    s->temp_pats[ 0 ] = -1;
		    for( int i = 0; i < s->pat_state_size; i++ )
		    {
			if( s->cur_playing_pats[ i ] == -1 ) break;
			if( (unsigned)s->cur_playing_pats[ i ] < (unsigned)s->sorted_pats_num )
			{
			    int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
			    if( (unsigned)pat_num < (unsigned)s->pats_num && s->pats[ pat_num ] )
			    {
				if( s->line_counter >= s->pats_info[ pat_num ].x &&
				    s->line_counter < s->pats_info[ pat_num ].x + s->pats[ pat_num ]->lines )
				{
				    s->temp_pats[ p ] = s->cur_playing_pats[ i ];
				    p++;
				}
				else
				{
				    int end_pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
				    sunvox_pattern* end_pat = s->pats[ end_pat_num ];
				    sunvox_pattern_info* end_pat_info = &s->pats_info[ end_pat_num ];
				    sunvox_pattern_state* state = &s->pat_state[ end_pat_info->state_ptr ];
				    if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
				    {
					if( !( end_pat->flags & SUNVOX_PATTERN_FLAG_NO_NOTES_OFF ) )
					{
					    if( end_pat_info->track_status )
					    {
						for( int a = 0; a < MAX_PATTERN_TRACKS; a++ )
						{
						    if( end_pat_info->track_status & ( 1 << a ) )
						    {
							if( state->track_status & ( 1 << a ) )
							{
							    int mod_num = state->track_module[ a ];
							    if( (unsigned)mod_num < s->net->mods_num )
							    {
								psynth_event module_evt;
								module_evt.command = PS_CMD_NOTE_OFF;
					    			module_evt.id = ( end_pat_info->state_ptr << 16 ) | a;
								module_evt.offset = ptr;
								module_evt.note.velocity = 256;
								psynth_add_event( mod_num, &module_evt, s->net );
								state->track_status &= ~( 1 << a );
					    		    }
					    		}
						    }
						}
					    }
					}
				    }
				    else
				    {
					for( int a = 0; a < MAX_PATTERN_TRACKS; a++ )
					{
					    if( state->track_status & ( 1 << a ) )
					    {
						int mod_num = state->track_module[ a ];
						if( (unsigned)mod_num < s->net->mods_num )
						{
						    psynth_event module_evt;
						    module_evt.command = PS_CMD_NOTE_OFF;
						    module_evt.id = ( end_pat_info->state_ptr << 16 ) | a;
						    module_evt.offset = ptr;
						    module_evt.note.velocity = 256;
						    psynth_add_event( mod_num, &module_evt, s->net );
						    state->track_status &= ~( 1 << a );
						}
					    }
					}
					state->busy = false;
				    }
				    end_pat_info->track_status = 0;
				}
			    }
			}
			s->last_sort_pat = s->cur_playing_pats[ i ];
		    }
		    int search_again = 1;
		    while( search_again == 1 && p < s->pat_state_size )
		    {
			search_again = 0;
			if( s->sorted_pats_num && s->last_sort_pat < s->sorted_pats_num - 1 )
			{
			    int next_sort_pat = s->last_sort_pat + 1;
			    if( s->pats[ s->sorted_pats[ next_sort_pat ] ] )
			    {
				int next_time = s->pats_info[ s->sorted_pats[ next_sort_pat ] ].x;
				if( next_time == s->line_counter )
				{
				    for( ; (unsigned)next_sort_pat < (unsigned)s->sorted_pats_num ; next_sort_pat++ )
				    {
					sunvox_pattern_info* pat_info = &s->pats_info[ s->sorted_pats[ next_sort_pat ] ];
					if( pat_info->x != next_time ) break;
					s->temp_pats[ p ] = next_sort_pat;
					int state_ptr = pat_info->state_ptr;
					if( !s->pat_state[ state_ptr ].busy )
					{
					    clean_pattern_state( &s->pat_state[ state_ptr ], s );
					    s->pat_state[ state_ptr ].busy = true;
					}
					p++; if( p >= s->pat_state_size ) break;
				    }
				}
				else
				{
				    if( next_time < s->line_counter )
				    {
					search_again = 1;
					s->last_sort_pat++;
				    }
				}
			    }
			}
		    }
		    uint32_t state_handled[ SUPERTRACK_BITARRAY_SIZE ];
		    for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE; i++ ) state_handled[ i ] = 0;
		    for( int i = 0; i < p; i++ )
		    {
		        s->cur_playing_pats[ i ] = s->temp_pats[ i ];
			int pat_num = s->sorted_pats[ s->temp_pats[ i ] ];
			sunvox_pattern* pat = s->pats[ pat_num ];
			sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
			sunvox_pattern_state* state = &s->pat_state[ pat_info->state_ptr ];
			uint32_t bit = 1 << ( pat_info->state_ptr & 31 );
			int prev_mutable_tracks = 0;
			if( ( state_handled[ pat_info->state_ptr / 32 ] & bit ) == 0 )
			{
			    prev_mutable_tracks = 0;
			    state->mutable_tracks = pat->channels;
			    state_handled[ pat_info->state_ptr / 32 ] |= bit;
			}
			else
			{
			    prev_mutable_tracks = state->mutable_tracks;
			    if( pat->channels > state->mutable_tracks )
				state->mutable_tracks = pat->channels;
			}
			for( int track = prev_mutable_tracks; track < state->mutable_tracks; track++ )
			    sunvox_reset_track_effect( &state->effects[ track ] );
		    }
		    if( p < s->pat_state_size ) s->cur_playing_pats[ p ] = -1;
		    for( int i = 0; i < p; i++ )
		    {
			int pat_num = s->sorted_pats[ s->temp_pats[ i ] ];
			sunvox_pattern* pat = s->pats[ pat_num ];
			sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
			sunvox_pattern_state* state = &s->pat_state[ pat_info->state_ptr ];
			bool mute = false;
			while( 1 )
			{
			    if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_MUTE ) { mute = 1; break; }
			    if( s->pats_solo_mode && ( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_SOLO ) == 0 ) { mute = 1; break; }
			    if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
			    {
				int st = ( pat_info->y + 16 ) / 32;
				if( s->supertrack_mute[ st / 32 ] & ( 1 << ( st & 31 ) ) ) { mute = 1; break; }
			    }
			    break;
			}
			if( mute )
			{
			    if( pat_info->track_status )
			    {
				for( int a = 0; a < MAX_PATTERN_TRACKS; a++ )
				{
				    if( pat_info->track_status & ( 1 << a ) )
				    {
					if( state->track_status & ( 1 << a ) )
					{
					    int mod_num = state->track_module[ a ];
					    if( (unsigned)mod_num < s->net->mods_num )
					    {
						psynth_event module_evt;
						module_evt.command = PS_CMD_NOTE_OFF;
			    			module_evt.id = ( pat_info->state_ptr << 16 ) | a;
						module_evt.offset = ptr;
						module_evt.note.velocity = 256;
						psynth_add_event( mod_num, &module_evt, s->net );
						state->track_status &= ~( 1 << a );
					    }
					}
				    }
				}
			    }
			    pat_info->track_status = 0;
			    continue;
			}
			int pat_ptr = ( s->line_counter - pat_info->x ) * pat->data_xsize;
			for( int track = 0; track < pat->channels; track++ )
			{
			    sunvox_handle_command( ptr, &pat->data[ pat_ptr ], s->net, pat_num, track, s );
			    pat_ptr++;
			}
			if( !s->playing ) break;
		    }
		    if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
		    {
			for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE; i++ )
			{
			    uint32_t mute = s->supertrack_mute[ i ];
			    if( mute )
			    {
				for( int i2 = 0; i2 < 32; i2++ )
				{
				    if( mute & ( 1 << i2 ) )
				    {
					int state_ptr = i * 32 + i2;
					sunvox_pattern_state* state = &s->pat_state[ state_ptr ];
					if( state->track_status )
					{
					    for( int a = 0; a < MAX_PATTERN_TRACKS; a++ )
					    {
						int mod_num = state->track_module[ a ];
						if( (unsigned)mod_num < s->net->mods_num )
						{
						    psynth_event module_evt;
						    module_evt.command = PS_CMD_NOTE_OFF;
			    			    module_evt.id = ( state_ptr << 16 ) | a;
						    module_evt.offset = ptr;
						    module_evt.note.velocity = 256;
						    psynth_add_event( mod_num, &module_evt, s->net );
						    state->track_status &= ~( 1 << a );
						}
					    }
					}
				    }
				}
			    }
			}
		    }
		    one_tick = sunvox_check_speed( ptr, s );
		} 
		uint32_t state_handled[ SUPERTRACK_BITARRAY_SIZE ];
		for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE; i++ ) state_handled[ i ] = 0;
		for( int i = 0; i < s->pat_state_size; i++ )
		{
		    int sp = s->cur_playing_pats[ i ];
		    if( sp == -1 ) break;
		    int pat_num = s->sorted_pats[ s->cur_playing_pats[ i ] ];
		    sunvox_pattern* pat = s->pats[ pat_num ];
		    sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
		    uint32_t bit = 1 << ( pat_info->state_ptr & 31 );
		    if( ( state_handled[ pat_info->state_ptr / 32 ] & bit ) == 0 )
		    {
			sunvox_pattern_state* state = &s->pat_state[ pat_info->state_ptr ];
			for( int track = 0; track < state->mutable_tracks; track++ )
			{
			    sunvox_handle_track_effects( ptr, s->net, pat_num, track, s );
			}
			state_handled[ pat_info->state_ptr / 32 ] |= bit;
		    }
		}
	    } 
	    for( int ch = 0; ch < s->virtual_pat_tracks; ch++ )
	    {
	        sunvox_handle_track_effects( ptr, s->net, SUNVOX_VIRTUAL_PATTERN, ch, s );
	    }
	}
	if( ptr >= frames )
	{
	    break;
	}
    } 
    if( s->recording ) rv = 1;
    psynth_render_setup( frames, rdata->out_time + ( (int64_t)s->level1_offset * stime_ticks_per_second() ) / freq, (char*)rdata->in_buffer, rdata->in_type, rdata->in_channels, s->net );
    psynth_render_all( s->net );
    int mods_num;
    if( rdata->out_file_encoders == nullptr )
    {
	mods_num = 1;
    }
    else
    {
	mods_num = s->net->mods_num;
    }
    if( !rdata->buffer ) mods_num = 0;
    for( int fnum = 0; fnum < mods_num; fnum++ )
    {
	psynth_module* mod = &s->net->mods[ fnum ];
	if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) continue;
	if( mod->flags & PSYNTH_FLAG_MUTE ) continue;
	sfs_sound_encoder_data* enc = nullptr;
	if( rdata->out_file_encoders )
	{
	    enc = rdata->out_file_encoders[ fnum ];
	}
	if( fnum > 0 && enc == nullptr ) continue;
	int sample_size = 1;
	for( int ch = 0; ch < channels; ch++ )
	{
	    PS_STYPE* chan = NULL;
	    int chan_empty = 0;
	    if( mod->flags & PSYNTH_FLAG_OUTPUT )
	    {
		if( ch < mod->input_channels )
		{
		    chan = mod->channels_in[ ch ];
		    chan_empty = mod->in_empty[ ch ];
		}
		if( chan == NULL )
		{
		    chan = mod->channels_in[ 0 ];
		    chan_empty = mod->in_empty[ 0 ];
		}
	    }
	    else
	    {
		if( ch < mod->output_channels )
		{
		    chan = mod->channels_out[ ch ];
		    chan_empty = mod->out_empty[ ch ];
		}
		if( chan == NULL )
		{
		    chan = mod->channels_out[ 0 ];
		    chan_empty = mod->out_empty[ 0 ];
		}
	    }
	    if( chan == NULL ) continue;
	    if( chan_empty < frames ) rv = 1;
	    switch( rdata->buffer_type )
	    {
		case sound_buffer_int16:
		    {
			int16_t* output = (int16_t*)rdata->buffer;
			output += ch;
			for( int i = 0; i < frames; i++ )
			{
			    int16_t result;
			    PS_STYPE_TO_INT16( result, chan[ i ] );
			    *output = result;
			    output += channels;
			}
			sample_size = 2;
		    }
		    break;
		case sound_buffer_float32:
		    {
			float* output = (float*)rdata->buffer;
			output += ch;
			for( int i = 0; i < frames; i++ )
			{
			    float result;
			    PS_STYPE_TO_FLOAT( result, chan[ i ] );
			    *output = result;
			    output += channels;
			}
			sample_size = 4;
		    }
		    break;
		default:
		    break;
	    }
	    if( fnum == 0 )
	    {
#if CPUMARK >= 10
		for( int i = 0; i < frames; i++ )
#else
		for( int i = 0; i < frames; i += 4 )
#endif
		{
		    if( chan[ i ] >= PS_STYPE_ONE || chan[ i ] <= -PS_STYPE_ONE )
		    {
			s->clipping_counter = freq;
			break;
		    }
		}
	    }
	}
	if( enc )
	{
	    sfs_sound_encoder_write( enc, rdata->buffer, frames );
	}
    }
    if( rdata->buffer )
    {
	int ptr2 = 0;
	switch( rdata->buffer_type )
	{
	    case sound_buffer_int16:
		{
		    int16_t* output = (int16_t*)rdata->buffer;
		    for( int fp = f_prev_size; fp < f_new_size; fp ++ )
		    {
			int val_l = 0;
			int val_r = 0;
			if( rv )
			{
			    val_l = output[ ( (ptr2>>8) * channels ) + 0 ];
			    if( channels > 1 )
				val_r = output[ ( (ptr2>>8) * channels ) + 1 ];
			    else
				val_r = val_l;
			    if( val_l < 0 ) val_l = -val_l;
			    if( val_r < 0 ) val_r = -val_r;
			    if( val_l > 32767 ) val_l = 32767;
			    if( val_r > 32767 ) val_r = 32767;
			}
			s->f_volume_l[ f_off + fp ] = val_l >> 7;
			s->f_volume_r[ f_off + fp ] = val_r >> 7;
			ptr2 += ptr2_step;
		    }
		}
		break;
	    case sound_buffer_float32:
		{
		    float* output = (float*)rdata->buffer;
		    for( int fp = f_prev_size; fp < f_new_size; fp ++ )
		    {
			int val_l = 0;
			int val_r = 0;
			if( rv )
			{
			    val_l = (int)( output[ ( (ptr2>>8) * channels ) + 0 ] * 32767 );
			    if( channels > 1 )
				val_r = (int)( output[ ( (ptr2>>8) * channels ) + 1 ] * 32767 );
			    else
				val_r = val_l;
			    if( val_l < 0 ) val_l = -val_l;
			    if( val_r < 0 ) val_r = -val_r;
			    if( val_l > 32767 ) val_l = 32767;
			    if( val_r > 32767 ) val_r = 32767;
			}
			s->f_volume_l[ f_off + fp ] = val_l >> 7;
			s->f_volume_r[ f_off + fp ] = val_r >> 7;
			ptr2 += ptr2_step;
		    }
		}
		break;
	    default:
		break;
	}
    }
    return rv;
}
bool sunvox_render_piece_of_sound( sunvox_render_data* rdata, sunvox_engine* s )
{
    rdata->silence = 1;
    if( !s ) return 0;
    if( !s->net ) return 0;
    if( s->initialized == 0 ) return 0;
    int frames = rdata->frames;
    void* in_buffer = rdata->in_buffer;
    void* buffer = rdata->buffer;
    stime_ticks_t out_time = rdata->out_time;
    s->f_current_buffer = ( s->f_current_buffer + 1 ) & SUNVOX_VF_BUFS_MASK;
    int f_current_buffer = s->f_current_buffer;
    s->f_buffer_start_time[ f_current_buffer ] = out_time;
    s->f_buffer_size[ f_current_buffer ] = 0;
    s->clipping_counter -= frames;
    if( s->clipping_counter < 0 ) 
	s->clipping_counter = 0;
    psynth_render_begin( rdata->out_time, s->net );
    int ptr = 0;
    while( 1 )
    {
	int size = frames - ptr;
	if( size > s->net->max_buf_size ) size = s->net->max_buf_size;
	if( size > 0 )
	{
	    rdata->frames = size;
	    if( buffer )
	    {
		switch( rdata->buffer_type )
		{
		    case sound_buffer_int16:
			rdata->buffer = (int16_t*)buffer + ptr * rdata->channels;
			break;
		    case sound_buffer_float32:
			rdata->buffer = (float*)buffer + ptr * rdata->channels;
			break;
		    default:
			break;
		}
	    }
	    if( in_buffer )
	    {
		switch( rdata->in_type )
		{
		    case sound_buffer_int16:
			rdata->in_buffer = (int16_t*)in_buffer + ptr * rdata->in_channels;
			break;
		    case sound_buffer_float32:
			rdata->in_buffer = (float*)in_buffer + ptr * rdata->in_channels;
			break;
		    default:
			break;
		}
	    }
	    s->level1_offset = ptr;
	    if( sunvox_render_piece_of_sound_level2( rdata, f_current_buffer, s ) )
		rdata->silence = 0;
	}
	ptr += size;
	if( ptr >= frames ) break;
    }
    psynth_render_end( frames, s->net );
    return 1;
}
int sunvox_frames_get_value( int channel, stime_ticks_t t, sunvox_engine* s )
{
    int buf = 0;
    stime_ticks_t t2 = stime_ticks_per_second() * 30;
    stime_ticks_t t_relative_point = t - t2;
    stime_ticks_t t_buf_start = 0;
    for( int i = 0; i < SUNVOX_VF_BUFS; i++ )
    {
	int t_buf_start2 = s->f_buffer_start_time[ i ] - t_relative_point;
	if( (int)t2 >= t_buf_start2 && t_buf_start2 > (int)t_buf_start )
	{
	    t_buf_start = t_buf_start2;
	    buf = i;
	}
    }
    stime_ticks_t frame = ( ( t2 - t_buf_start ) * SUNVOX_VF_BUF_SRATE ) / stime_ticks_per_second();
    uint size = s->f_buffer_size[ buf ];
    if( size > SUNVOX_VF_BUF_SRATE ) size = SUNVOX_VF_BUF_SRATE;
    if( size == 0 )
    {
	frame = 0;
    }
    else
    {
	if( frame >= (unsigned)size ) 
	    frame = size - 1;
    }
    switch( channel )
    {
	case SUNVOX_VF_CHAN_LINENUM:
	    return s->f_line[ buf * SUNVOX_VF_BUF_SRATE + frame ];
	    break;
	case SUNVOX_VF_CHAN_VOL0:
	    return s->f_volume_l[ buf * SUNVOX_VF_BUF_SRATE + frame ];
	    break;
	case SUNVOX_VF_CHAN_VOL1:
	    return s->f_volume_r[ buf * SUNVOX_VF_BUF_SRATE + frame ];
	    break;
	default: break;
    }
    return 0;
}
