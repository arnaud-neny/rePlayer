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
#include "psynth/psynths_sampler.h"
PS_RETTYPE ( *g_psynths [] )( PSYNTH_MODULE_HANDLER_PARAMETERS ) = 
{
    0,
    &psynth_generator2,
    &psynth_drumsynth,
    &psynth_fm,
    &psynth_fm2,
    &psynth_generator,
    &psynth_input,
    &psynth_kicker,
    &psynth_vplayer,
    &psynth_sampler,
    &psynth_spectravoice,
    0,
    &psynth_amplifier,
    &psynth_compressor,
    &psynth_dc_blocker,
    &psynth_delay,
    &psynth_distortion,
    &psynth_echo,
    &psynth_eq,
    &psynth_fft,
    &psynth_filter,
    &psynth_filter2,
    &psynth_flanger,
    &psynth_lfo,
    &psynth_loop,
    &psynth_modulator,
    &psynth_pitch_shifter,
    &psynth_reverb,
    &psynth_smooth,
    &psynth_vfilter,
    &psynth_vibrato,
    &psynth_waveshaper,
    0,
    &psynth_adsr,
    &psynth_ctl2note,
    &psynth_feedback,
    &psynth_glide,
    &psynth_gpio,
    &psynth_metamodule,
    &psynth_multictl,
    &psynth_multisynth,
    &psynth_pitch2ctl,
    &psynth_pitch_detector,
    &psynth_sound2ctl,
    &psynth_velocity2ctl,
};
#define PSYNTHS_NUM ( sizeof( g_psynths ) / sizeof( void* ) )
const int g_psynths_num = PSYNTHS_NUM;
PS_RETTYPE ( *g_psynths_eff [ PSYNTHS_NUM ] )( PSYNTH_MODULE_HANDLER_PARAMETERS );
int g_psynths_eff_num = 0;
const char* g_sunvox_midi_in_keys[] = { "midi_kbd", "midi_kbd2", "midi_kbd3", "midi_kbd4", "midi_kbd5" };
const char* g_sunvox_midi_in_ch_keys[] = { "midi_kbd_ch", "midi_kbd2_ch", "midi_kbd3_ch", "midi_kbd4_ch", "midi_kbd5_ch" };
const char* g_sunvox_midi_in_names[] = { "Keyboard1", "Keyboard2", "Keyboard3", "Keyboard4", "Keyboard5" };
const char* g_sunvox_block_id_names[] =
{
    "BVER",
    "VERS",
    "SFGS",
    "BPM ",
    "SPED",
    "TGRD",
    "TGD2",
    "NAME",
    "GVOL",
    "MSCL",
    "MZOO",
    "MXOF",
    "MYOF",
    "LMSK",
    "CURL",
    "TIME",
    "REPS",
    "SELS",
    "LGEN",
    "PATN",
    "PATT",
    "PATL",
    "PDTA",
    "PNME",
    "PLIN",
    "PCHN",
    "PYSZ",
    "PICO",
    "PPAR",
    "PPR#",
    "PFLG",
    "PFFF",
    "PXXX",
    "PYYY",
    "PFGC",
    "PBGC",
    "PEND",
    "SFFF",
    "SNAM",
    "STYP",
    "SFIN",
    "SREL",
    "SXXX",
    "SYYY",
    "SZZZ",
    "SSCL",
    "SVPR",
    "SCOL",
    "SMII",
    "SMIN",
    "SMIC",
    "SMIB",
    "SMIP",
    "SLNK",
    "SLnK",
    "CVAL",
    "CMID",
    "CHNK",
    "CHNM",
    "CHDT",
    "CHFF",
    "CHFR",
    "SEND",
    "SLNk",
    "SLnk",
    "FLGS",
    "STMT",
    "JAMD",
};
ssymtab* g_sunvox_block_ids = NULL;
const uint8_t g_sunvox_sign[] = { 'S', 'V', 'O', 'X', 0, 0, 0, 0 };
const uint8_t g_sunvox_module_sign[] = { 'S', 'S', 'Y', 'N', 0, 0, 0, 0 };
int sunvox_global_init()
{
    int rv = 0;
    if( strcmp( g_sunvox_block_id_names[ 64 ], "SLnk" ) )
    {
	slog( "Wrong SunVox block ID names!\n" );
	return -1;
    }
    g_sunvox_block_ids = ssymtab_new( 3 );
    if( !g_sunvox_block_ids ) return -2;
    for( int i = 0; i < BID_COUNT; i++ )
    {
	const char* block_id_name = g_sunvox_block_id_names[ i ];
	if( strlen( block_id_name ) != 4 )
	{
	    slog( "Wrong SunVox block ID name: %s\n", block_id_name );
	    return -3;
	}
	ssymtab_iset( block_id_name, i, g_sunvox_block_ids );
    }
    if( psynth_global_init() ) rv = -4;
    return rv;
}
int sunvox_global_deinit()
{
    int rv = 0;
    if( psynth_global_deinit() ) rv = -1;
    ssymtab_delete( g_sunvox_block_ids ); g_sunvox_block_ids = NULL;
    return rv;
}
void sunvox_engine_init(
    uint flags,
    int freq,
    WINDOWPTR win, 
    sundog_sound* ss, 
    tsunvox_sound_stream_control stream_control,
    void* stream_control_data,
    sunvox_engine* s )
{
    if( sizeof( size_t ) != sizeof( void* ) )
    {
	slog( "ERROR! sizeof( size_t ) != sizeof( void* ). Can't use sunvox_pattern_short and sunvox_pattern.\n" );
	return;
    }
    if( stream_control ) stream_control( SUNVOX_STREAM_LOCK, stream_control_data, 0 );
    smem_clear( s, sizeof( sunvox_engine ) );
    s->stream_control = stream_control;
    s->stream_control_data = stream_control_data;
    if( stream_control ) stream_control( SUNVOX_STREAM_UNLOCK, stream_control_data, 0 );
    s->win = win; 
    s->ss = ss; 
    s->base_version = SUNVOX_ENGINE_VERSION;
    s->flags = flags;
    s->sync_flags = SUNVOX_SYNC_FLAGS_DEFAULT;
    s->freq = freq;
    s->single_pattern_play = -1;
    s->next_single_pattern_play = -1;
    s->bpm = 125;
    s->speed = 6;
    s->prev_bpm = 0;
    s->prev_speed = 0;
    s->sync_resolution = freq / 882;
    s->tgrid = 4;
    s->tgrid2 = 4;
    s->pat_id_counter = stime_ms() + ( pseudo_random() << 16 );
    char proj_name[ 128 ];
    sprintf( proj_name, "%d-%02d-%02d %02d-%02d", stime_year(), stime_month(), stime_day(), stime_hours(), stime_minutes() );
    s->proj_name = SMEM_ALLOC2( char, smem_strlen( proj_name ) + 1 );
    s->proj_name[ 0 ] = 0;
    SMEM_STRCAT_D( s->proj_name, proj_name );
    if( g_psynths_eff_num == 0 )
    {
	int group = -1;
	for( int i = 0; i < g_psynths_num; i++ )
	{
	    if( g_psynths[ i ] == 0 ) 
	    {
		group++;
		if( group >= 2 )
		{
		    break;
		}
		continue;
	    }
	    psynth_event evt;
	    evt.command = PS_CMD_GET_FLAGS;
	    uint flags = g_psynths[ i ]( -1, &evt, 0 );
	    if( flags & PSYNTH_FLAG_EFFECT )
	    {
		if( ( flags & ( PSYNTH_FLAG_DONT_FILL_INPUT | PSYNTH_FLAG_NO_RENDER | PSYNTH_FLAG_GENERATOR ) ) == 0 )
		{
		    g_psynths_eff[ g_psynths_eff_num ] = g_psynths[ i ];
		    g_psynths_eff_num++;
		}
	    }
	}
    }
    s->net = SMEM_ALLOC2( psynth_net, 1 );
    uint psynth_flags = 0;
    if( flags & SUNVOX_FLAG_MAIN ) psynth_flags |= PSYNTH_NET_FLAG_MAIN;
    if( flags & SUNVOX_FLAG_CREATE_MODULES ) psynth_flags |= PSYNTH_NET_FLAG_CREATE_MODULES;
    if( flags & SUNVOX_FLAG_NO_SCOPE ) psynth_flags |= PSYNTH_NET_FLAG_NO_SCOPE;
    if( flags & SUNVOX_FLAG_NO_MIDI ) psynth_flags |= PSYNTH_NET_FLAG_NO_MIDI;
    if( flags & SUNVOX_FLAG_NO_MODULE_CHANNELS ) psynth_flags |= PSYNTH_NET_FLAG_NO_MODULE_CHANNELS;
    psynth_init( psynth_flags, freq, s->bpm, s->speed, s, s->base_version, s->net );
    s->selected_module = 0;
    s->last_selected_generator = -1;
    s->module_scale = 256;
    s->module_zoom = 256;
    s->xoffset = 0;
    s->yoffset = 0;
    if( flags & SUNVOX_FLAG_MAIN )
	s->user_commands = sring_buf_new( sizeof( sunvox_user_cmd ) * MAX_USER_COMMANDS, 0 ); 
    else
	s->user_commands = sring_buf_new( sizeof( sunvox_user_cmd ) * MAX_USER_COMMANDS_FOR_METAMODULE, 0 );
    if( ( s->flags & SUNVOX_FLAG_NO_KBD_EVENTS ) == 0 )
    {
	s->kbd = SMEM_ZALLOC2( sunvox_kbd_events, 1 );
	s->kbd->prev_track = -1;
	s->kbd->vel = 128;
    }
    if( ( s->flags & SUNVOX_FLAG_NO_GUI ) == 0 )
    {
	s->out_ui_events = sring_buf_new( sizeof( sunvox_ui_evt ) * MAX_UI_COMMANDS, SRING_BUF_FLAG_SINGLE_RTHREAD | SRING_BUF_FLAG_SINGLE_WTHREAD );
    }
    s->cur_playing_pats[ 0 ] = -1;
    clean_pattern_state( &s->virtual_pat_state, s ); s->virtual_pat_tracks = 0;
    if( !( flags & SUNVOX_FLAG_DYNAMIC_PATTERN_STATE ) )
    {
	s->pat_state_size = MAX_PLAYING_PATS;
	s->pat_state = SMEM_ZALLOC2( sunvox_pattern_state, s->pat_state_size );
	for( int i = 0; i < s->pat_state_size; i++ ) 
	    clean_pattern_state( &s->pat_state[ i ], s );
    }
    if( flags & SUNVOX_FLAG_CREATE_PATTERN )
    {
	int p = sunvox_new_pattern( SUNVOX_PATTERN_DEFAULT_LINES( s ), SUNVOX_PATTERN_DEFAULT_TRACKS( SV ), 0, 0, (uint)( stime_ms() + stime_seconds() ), s );
	sunvox_make_icon( p, s );
	sunvox_update_icons( s );
    }
    sunvox_sort_patterns( s );
    if( !( flags & SUNVOX_FLAG_PLAYER_ONLY ) )
    {
    }
    s->rand_next = stime_ms();
    s->velocity = 256;
#ifndef NOMIDI
    if( ( flags & SUNVOX_FLAG_NO_MIDI ) == 0 )
    {
	s->midi = SMEM_ZALLOC2( sunvox_midi, 1 );
    }
    if( s->midi )
    {
	for( int i = 0; i < SUNVOX_MIDI_IN_PORTS; i++ )
        {
	    s->midi->kbd[ i ] = -1;
    	    s->midi->kbd_prev_ctl[ i ] = -1;
	    s->midi->kbd_ctl_data[ i ] = -1;
	}
	for( int i = 0; i < SUNVOX_MIDI_IN_PORTS - 1; i++ )
	{
	    s->midi->kbd[ i ] = 
		sundog_midi_client_open_port( 
		    &s->net->midi_client, g_sunvox_midi_in_names[ i ], sconfig_get_str_value( g_sunvox_midi_in_keys[ i ], 0, 0 ), MIDI_PORT_READ );
    	    s->midi->kbd_ch[ i ] = 
    		sconfig_get_int_value( g_sunvox_midi_in_ch_keys[ i ], 1, 0 ) - 1;
    	}
#if defined(OS_APPLE)
        s->midi->kbd[ SUNVOX_MIDI_IN_PORT_PUBLIC ] = sundog_midi_client_open_port( &s->net->midi_client, "SunVox", "public port", MIDI_PORT_READ );
        s->midi->kbd_ch[ SUNVOX_MIDI_IN_PORT_PUBLIC ] = -1;
#endif
	if( sconfig_get_int_value( "midi_no_vel", -1, 0 ) != -1 )
	    s->midi->kbd_ignore_velocity = 1;
	else
	    s->midi->kbd_ignore_velocity = 0;
	s->midi->kbd_octave_offset = sconfig_get_int_value( "midi_oct_off", 0, NULL );
	{
    	    s->midi->kbd_sync = -1;
	    int sync = sconfig_get_int_value( "midi_sync", -1, NULL ); 
	    if( sync == 0 ) s->midi->kbd_sync = 256;
	    if( sync == 1 || sync < 0 ) s->midi->kbd_sync = -1;
	    if( sync == 2 ) s->midi->kbd_sync = SUNVOX_MIDI_IN_PORT_PUBLIC;
	    if( sync >= 3 ) s->midi->kbd_sync = sync - 3;
    	}
    }
#endif
    s->midi_import_tpl = 3;
    s->midi_import_quantization = 0;
    COMPILER_MEMORY_BARRIER();
    if( !( flags & SUNVOX_FLAG_DONT_START ) )
	s->initialized = 1;
}
void sunvox_engine_close( sunvox_engine* s )
{
    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_LOCK );
    if( s->initialized == 0 )
    {
	SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_UNLOCK );
	return;
    }
    s->initialized = 0;
    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_UNLOCK );
    if( s->pats )
    {
	for( int p = 0; p < s->pats_num; p++ )
	    sunvox_remove_pattern( p, s );
	smem_free( s->pats );
	s->pats = NULL;
	s->pats_num = 0;
    }
    smem_free( s->pats_info ); s->pats_info = NULL;
    smem_free( s->sorted_pats ); s->sorted_pats = NULL;
    smem_free( s->pat_state ); s->pat_state = NULL;
    if( !( s->flags & SUNVOX_FLAG_NO_GUI ) )
    {
#ifdef SUNVOX_GUI
	if( s->icons_map )
	{
	    remove_image( s->icons_map );
	    s->icons_map = NULL;
	}
	if( s->icons_flags )
	{
	    smem_free( s->icons_flags );
	    s->icons_flags = NULL;
	}
#endif
    }
    smem_free( s->proj_name ); s->proj_name = NULL;
    if( s->net )
    {
	psynth_close( s->net );
	s->net = NULL;
    }
    if( !( s->flags & SUNVOX_FLAG_PLAYER_ONLY ) )
    {
    }
    smem_free( s->psynth_events );
    sring_buf_delete( s->user_commands );
    sring_buf_delete( s->out_ui_events );
    smem_free( s->kbd );
#ifndef NOMIDI
    smem_free( s->midi );
#endif
}
void sunvox_rename( sunvox_engine* s, const char* proj_name )
{
    if( !s ) return;
    if( !s->proj_name ) s->proj_name = SMEM_ALLOC2( char, 1 );
    if( !s->proj_name ) return;
    s->proj_name[ 0 ] = 0;
    SMEM_STRCAT_D( s->proj_name, proj_name );
}
static int psynth_sunvox_stream_control( sunvox_stream_command cmd, void* user_data, sunvox_engine* s )
{
    psynth_sunvox* ps = (psynth_sunvox*)user_data;
    int rv = 0;
    switch( cmd )
    {
        case SUNVOX_STREAM_LOCK:
        case SUNVOX_STREAM_STOP:
            smutex_lock( psynth_get_mutex( ps->mod_num, ps->pnet ) );
            break;
        case SUNVOX_STREAM_UNLOCK:
        case SUNVOX_STREAM_PLAY:
            smutex_unlock( psynth_get_mutex( ps->mod_num, ps->pnet ) );
            break;
        case SUNVOX_STREAM_SYNC_PLAY:
        case SUNVOX_STREAM_SYNC:
        case SUNVOX_STREAM_ENABLE_INPUT:
        case SUNVOX_STREAM_DISABLE_INPUT:
        case SUNVOX_STREAM_IS_SUSPENDED:
            break;
    }
    return rv;
}
psynth_sunvox* psynth_sunvox_new( psynth_net* pnet, uint mod_num, uint sunvox_engines_count, uint flags )
{
    int err = 1;
    psynth_sunvox* ps = SMEM_ZALLOC2( psynth_sunvox, 1 );
    while( 1 )
    {
	if( ps == NULL ) { err = 2; break; }
        ps->pnet = pnet;
	ps->mod_num = mod_num;
	ps->flags = 
            SUNVOX_FLAG_PLAYER_ONLY | SUNVOX_FLAG_NO_GUI | SUNVOX_FLAG_NO_SCOPE | SUNVOX_FLAG_NO_MIDI | SUNVOX_FLAG_NO_GLOBAL_SYS_EVENTS |
            SUNVOX_FLAG_NO_KBD_EVENTS | SUNVOX_FLAG_DYNAMIC_PATTERN_STATE |
            flags;
        ps->s = SMEM_ZALLOC2( sunvox_engine*, sunvox_engines_count );
        if( ps->s == NULL ) { err = 3; break; }
        ps->s_cnt = sunvox_engines_count;
        for( uint i = 0; i < sunvox_engines_count; i++ )
        {
    	    ps->s[ i ] = SMEM_ALLOC2( sunvox_engine, 1 );
    	    if( ps->s[ i ] == NULL ) continue;
            sunvox_engine_init( ps->flags, pnet->sampling_freq, 0, 0, psynth_sunvox_stream_control, ps, ps->s[ i ] );
        }
        if( ps->flags & SUNVOX_FLAG_NO_MODULE_CHANNELS )
        {
	    for( uint i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
	    {
		ps->temp_bufs[ i ] = SMEM_ALLOC2( PS_STYPE, ps->s[ 0 ]->net->max_buf_size );
	    }
	}
        err = 0;
        break;
    }
    if( err )
    {
	slog( "psynth_sunvox_new() error %d\n", err );
	smem_free( ps );
	ps = 0;
    }
    return ps;
}
void psynth_sunvox_remove( psynth_sunvox* ps )
{
    if( ps == 0 ) return;
    for( uint i = 0; i < ps->s_cnt; i++ )
    {
	if( ps->s[ i ] == 0 ) continue;
	sunvox_engine_close( ps->s[ i ] );
        smem_free( ps->s[ i ] );
    }
    for( uint i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
    {
	smem_free( ps->temp_bufs[ i ] );
    }
    smem_free( ps->s );
    smem_free( ps );
}
int psynth_sunvox_set_module(
    PS_RETTYPE (*mod_handler)( PSYNTH_MODULE_HANDLER_PARAMETERS ),
    const char* mod_filename,
    sfs_file mod_file,
    uint mod_count,
    psynth_sunvox* ps )
{
    int err = 1;
    while( 1 )
    {
	if( ps == 0 ) break;
	sunvox_engine* s = ps->s[ 0 ];
	if( s == 0 ) break;
	psynth_clear( s->net );
	size_t fpos = 0;
	if( mod_file )
	{
	    fpos = sfs_tell( mod_file );
	}
	for( uint mc = 0; mc < mod_count; mc++ )
	{
	    int m = -1;
	    if( mod_handler )
	    {
		psynth_event module_evt;
        	module_evt.command = PS_CMD_GET_NAME;
                char* mod_name = (char*)mod_handler( -1, &module_evt, 0 );
	        m = psynth_add_module( -1, mod_handler, mod_name, 0, 0, 0, 0, s->bpm, s->speed, s->net );
	        psynth_do_command( m, PS_CMD_SETUP_FINISHED, s->net );
	    }
	    if( mod_filename )
	    {
	        m = sunvox_load_module( -1, 0, 0, 0, mod_filename, 0, s );
	    }
	    if( mod_file )
	    {
		sfs_seek( mod_file, fpos, SFS_SEEK_SET );
		m = sunvox_load_module_from_fd( -1, 0, 0, 0, mod_file, 0, s );
	    }
	    if( m >= 0 )
	    {
	    }
	}
	err = 0;
	break;
    }
    return err;
}
psynth_module* psynth_sunvox_get_module( psynth_sunvox* ps )
{
    if( ps == 0 ) return 0;
    sunvox_engine* s = ps->s[ 0 ];
    if( s == 0 ) return 0;
    psynth_net* pnet = s->net;
    psynth_module* mod = psynth_get_module( 1, pnet );
    return mod;
}
int psynth_sunvox_save_module( sfs_file f, psynth_sunvox* ps )
{
    int rv = -999;
    if( f == 0 ) return rv;
    if( !ps ) return rv;
    sunvox_engine* s = ps->s[ 0 ];
    if( !s ) return rv;
    psynth_net* pnet = s->net;
    rv = sunvox_save_module_to_fd( 1, f, 0, s, NULL );
    return rv;
}
void psynth_sunvox_apply_module( 
    uint mod_num, 
    PS_STYPE** channels, int channels_count, uint offset, 
    uint size, 
    psynth_sunvox* ps )
{
    if( !channels ) return;
    if( !ps ) return;
    sunvox_engine* s = ps->s[ 0 ];
    if( !s ) return;
    psynth_net* pnet = s->net;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    pnet->buf_size = size;
    PS_STYPE* prev_ch = NULL;
    for( int ch = 0; ch < PSYNTH_MAX_CHANNELS; ch++ ) 
    {
        if( ch < channels_count ) prev_ch = channels[ ch ] + offset;
        mod->channels_in[ ch ] = prev_ch;
        mod->in_empty[ ch ] = 0;
    }
    for( int ch = 0; ch < PSYNTH_MAX_CHANNELS; ch++ )
    {
        mod->channels_out[ ch ] = ps->temp_bufs[ ch ];
        mod->out_empty[ ch ] = 0;
    }
    psynth_event mod_evt;
    mod_evt.command = PS_CMD_RENDER_REPLACE;
    psynth_handle_event( mod_num, &mod_evt, pnet );
    for( int ch = 0; ch < channels_count; ch++ )
    {
        if( ch < mod->output_channels ) prev_ch = mod->channels_out[ ch ];
        smem_copy( channels[ ch ] + offset, prev_ch, sizeof( PS_STYPE ) * size );
    }
}
PS_RETTYPE psynth_sunvox_handle_module_event( 
    uint mod_num, 
    psynth_event* evt,
    psynth_sunvox* ps )
{
    PS_RETTYPE rv = 0;
    if( !ps ) return 0;
    sunvox_engine* s = ps->s[ 0 ];
    if( !s ) return 0;
    psynth_net* pnet = s->net;
    uint m = mod_num;
    if( mod_num == 0 ) m = 1;
    while( 1 )
    {
	psynth_module* mod = psynth_get_module( m, pnet );
	if( mod == 0 ) break;
	rv = psynth_handle_event( m, evt, pnet );
	if( mod_num != 0 ) break;
	m++;
    }
    return rv;
}
PS_RETTYPE ( *get_module_handler_by_name( const char* name, sunvox_engine* s ) ) ( PSYNTH_MODULE_HANDLER_PARAMETERS )
{
    for( int i = 0; i < g_psynths_num; i++ )
    {
	if( g_psynths[ i ] )
	{
	    psynth_event module_evt;
    	    module_evt.command = PS_CMD_GET_NAME;
	    char* mod_name = (char*)g_psynths[ i ]( -1, &module_evt, s->net );
	    if( smem_strcmp( mod_name, name ) == 0 )
	    {
		return g_psynths[ i ];
	    }
	}
    }
    return &psynth_empty;
}
sunvox_load_state* sunvox_new_load_state( sunvox_engine* s, sfs_file f )
{
    sunvox_load_state* state = SMEM_ZALLOC2( sunvox_load_state, 1 );
    if( !state ) return NULL;
    state->s = s;
    state->f = f;
    return state;
}
sunvox_save_state* sunvox_new_save_state( sunvox_engine* s, sfs_file f )
{
    sunvox_save_state* state = SMEM_ZALLOC2( sunvox_save_state, 1 );
    if( !state ) return NULL;
    state->s = s;
    state->f = f;
    return state;
}
void sunvox_remove_load_state( sunvox_load_state* state )
{
    if( !state ) return;
    smem_free( state->block_data );
    smem_free( state );
}
void sunvox_remove_save_state( sunvox_save_state* state )
{
    if( !state ) return;
    smem_free( state->mod_remap );
    smem_free( state->pat_remap );
    smem_free( state );
}
int save_block( sunvox_block_id id, size_t size, void* ptr, sunvox_save_state* state )
{
    int rv = 0;
    const char* block_id_name = g_sunvox_block_id_names[ id ];
    while( 1 )
    {
        if( sfs_write( block_id_name, 1, 4, state->f ) != 4 ) { rv = 1; break; }
	if( sfs_write( &size, 1, 4, state->f ) != 4 ) { rv = 2; break; }
	if( size )
    	{
    	    if( !ptr )
    	    {
    	        for( size_t i = 0; i < size; i++ ) sfs_putc( 0, state->f );
    	    }
    	    else
    	    {
    	        if( sfs_write( ptr, 1, size, state->f ) != size ) { rv = 3; break; }
    	    }
	}
	break;
    }
    if( rv != 0 )
    {
	slog( "save_block(%s," PRINTF_SIZET "%d) error %d\n", block_id_name, PRINTF_SIZET_CONV size, rv );
    }
    return rv;
}
int load_block( sunvox_load_state* state )
{
    state->block_id = -1;
    if( sfs_read( &state->block_id_name, 1, 4, state->f ) != 4 ) return 0;
    state->block_id = ssymtab_iget( state->block_id_name, BID_COUNT, g_sunvox_block_ids );
    if( sfs_read( &state->block_size, 1, 4, state->f ) != 4 )
    {
	slog( "load_block(): can't read the size of block %s\n", state->block_id_name );
	return -1;
    }
    if( state->block_size )
    {
	if( smem_get_size( state->block_data ) != state->block_size )
	{
	    smem_free( state->block_data );
	    state->block_data = SMEM_ALLOC( state->block_size );
	    if( !state->block_data ) return -1;
	}
	size_t r = sfs_read( state->block_data, 1, state->block_size, state->f );
	if( r != state->block_size )
	{
	    slog( "load_block(): can't read the data of block %s; required size " PRINTF_SIZET "; received " PRINTF_SIZET "\n", 
	        state->block_id_name,
	        PRINTF_SIZET_CONV state->block_size,
	        PRINTF_SIZET_CONV r );
	    smem_free( state->block_data );
	    state->block_data = NULL;
	    return -1;
	}
	if( state->block_size >= 4 )
	{
	    smem_copy( &state->block_data_int, state->block_data, 4 );
	}
    }
    else
    {
	smem_free( state->block_data );
	state->block_data = NULL;
    }
    return 0;
}
void sunvox_set_position( int pos, sunvox_engine* s )
{
    s->line_counter = pos;
    sunvox_sort_patterns( s );
    sunvox_select_current_playing_patterns( 0, s );
}
void clean_pattern_state( sunvox_pattern_state* state, sunvox_engine* s )
{
    for( int i = 0; i < MAX_PATTERN_TRACKS; i++ )
    {
	sunvox_track_eff* eff = &state->effects[ i ];
	eff->flags = 0;
	eff->vel_speed = 0;
	eff->vib_amp = 0;
	eff->porta_speed = 256 * 4;
    }
    state->track_status = 0;
    for( int i = 0; i < MAX_PATTERN_TRACKS; i++ ) state->track_module[ i ] = -1;
}
int sunvox_get_playing_status( sunvox_engine* s )
{
    return s->playing;
}
void sunvox_play_stop( sunvox_engine* s )
{
    if( s->playing == 0 )
    {
	sunvox_play( 0, false, -1, s );
    }
    else
    {
	if( s->single_pattern_play == -1 )
	{
	    sunvox_stop( s );
	}
	else
	{
	    sunvox_play( 0, false, -1, s );
	}
    }
}
void sunvox_play( int pos, bool jump_to_pos, int pat_num, sunvox_engine* s )
{
    if( !( s->flags & SUNVOX_FLAG_NO_GUI ) )
    {
#ifdef SUNVOX_GUI
	s->check_timeline_cursor = true;
#endif
    }
    while( pat_num >= 0 )
    {
	if( (unsigned)pat_num < (unsigned)s->pats_num && s->pats )
        {
	    sunvox_pattern* pat = s->pats[ pat_num ];
    	    if( pat )
            {
	        pos = s->pats_info[ pat_num ].x;
	        jump_to_pos = true;
    	        break;
            }
	}
	pat_num = -1;
	break;
    }
    if( s->playing == 0 )
    {
	if( jump_to_pos )
	{
	    s->line_counter = pos;
	}
	sunvox_sort_patterns( s );
	sunvox_select_current_playing_patterns( 0, s );
	s->line_counter--;
	s->proj_len = sunvox_get_proj_frames( 0, 0, s );
	{
	    sunvox_user_cmd cmd;
	    SMEM_CLEAR_STRUCT( cmd );
            cmd.t = stime_ticks();
	    if( pat_num < 0 )
	    {
	    }
	    else
	    {
		cmd.n.ctl_val = pat_num + 1;
	    }
	    cmd.n.note = NOTECMD_PLAY;
	    sunvox_send_user_command( &cmd, s );
	}
	if( ( s->flags & SUNVOX_FLAG_ONE_THREAD ) || SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_IS_SUSPENDED ) )
	{
	    sunvox_handle_all_commands_UNSAFE( s );
	}
	else
	{
	    stime_ticks_t timeout = stime_ticks_per_second() / 5;
	    stime_ticks_t t = stime_ticks();
	    while( s->playing == 0 ) 
	    {
		if( stime_ticks() - t >= timeout ) break;
		stime_sleep( 5 );   
	    }
	}
    }
    else
    {
	if( jump_to_pos )
	{
	    sunvox_rewind( pos, pat_num, s );
	}
	else
	{
	    if( s->single_pattern_play >= 0 )
	    {
		sunvox_user_cmd cmd;
		SMEM_CLEAR_STRUCT( cmd );
	        cmd.t = stime_ticks();
    		cmd.n.note = NOTECMD_PATPLAY_OFF;
		sunvox_send_user_command( &cmd, s );
	    }
	}
    }
    s->start_time = stime_ms();
    s->cur_time = s->start_time;
}
void sunvox_rewind( int pos, int pat_num, sunvox_engine* s )
{
    int playing;
    playing = s->playing;
    if( playing ) sunvox_stop( s );
    sunvox_set_position( pos, s );
    if( playing ) sunvox_play( 0, false, pat_num, s );
}
static int sunvox_just_stop( sunvox_engine* s )
{
    int rv;
    sunvox_user_cmd cmd;
    SMEM_CLEAR_STRUCT( cmd );
    if( s->playing )
    {
	cmd.t = stime_ticks();
	cmd.n.note = NOTECMD_STOP;
	sunvox_send_user_command( &cmd, s );
	if( ( s->flags & SUNVOX_FLAG_ONE_THREAD ) || SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_IS_SUSPENDED ) )
	{
	    sunvox_handle_all_commands_UNSAFE( s );
	}
	else
        {
    	    stime_ticks_t timeout = stime_ticks_per_second() / 5;
    	    stime_ticks_t t = stime_ticks();
    	    while( s->playing ) 
    	    {
        	if( stime_ticks() - t >= timeout ) break;
		stime_sleep( 5 );  
	    }
	}
	rv = 0;
    }
    else
    {
	SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_STOP );
	cmd.t = stime_ticks() - stime_ticks_per_second() * 2;
	cmd.n.note = NOTECMD_CLEAN_MODULES; 
	sunvox_send_user_command( &cmd, s );
	sunvox_render_data rdata;
	SMEM_CLEAR_STRUCT( rdata );
	int16_t temp_buf[ 8 ];
        rdata.buffer_type = sound_buffer_int16;
        rdata.buffer = temp_buf;
        rdata.frames = 8;
        rdata.channels = 1;
        rdata.out_time = stime_ticks();
        sunvox_render_piece_of_sound( &rdata, s );
        SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_PLAY );
	rv = 1;
    }
    if( !( s->flags & SUNVOX_FLAG_NO_GUI ) )
    {
#ifdef SUNVOX_GUI
	s->check_timeline_cursor = false;
#endif
    }
    return rv;
}
int sunvox_stop( sunvox_engine* s )
{
    int rv = sunvox_just_stop( s );
    return rv;
}
int sunvox_stop_and_cancel_record( sunvox_engine* s )
{
    int rv = sunvox_just_stop( s );
    return rv;
}
