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
#include "midi_file/midi_file.h"
#include "xm/xm.h"
static bool check_signature( sfs_file f )
{
    uint8_t sign[ 8 ];
    memset( sign, 0, sizeof( sign ) );
    sfs_rewind( f );
    sfs_read( sign, 8, 1, f );
    if( memcmp( sign, g_sunvox_sign, sizeof( sign ) ) == 0 ) return true;
    sfs_rewind( f );
    return false;
}
struct midi_bpm
{
    uint t; 
    uint16_t bpm;
    uint16_t beat_len; 
};
int set_bpm_map( midi_bpm** bpm_map, size_t offset, uint t, uint16_t bpm, uint16_t beat_len )
{
    if( bpm_map == 0 ) return 1;
    if( *bpm_map == 0 ) return 1;
    size_t size = smem_get_size( *bpm_map ) / sizeof( midi_bpm );
    if( offset >= size )
    {
	size_t new_size = size + 256;
	*bpm_map = SMEM_ZRESIZE2( *bpm_map, midi_bpm, new_size );
	if( *bpm_map == NULL ) return 1;
    }
    (*bpm_map)[ offset ].t = t;
    (*bpm_map)[ offset ].bpm = bpm;
    (*bpm_map)[ offset ].beat_len = beat_len;
    return 0;
}
static int get_xm_pattern_position( xm_song* xm, int p )
{
    if( p > xm->length ) return -1;
    int rv = 0;
    for( int i = 0; i < p; i++ )
    {
        int pat_id = xm->patterntable[ i ];
        int pat_rows = 0;
        xm_pattern* pat = xm->patterns[ pat_id ];
        if( pat )
        {
    	    rv += pat->rows;
        }
    }
    return rv;
}
static void set_pattern_note( int pat_num, int line, int track, sunvox_note* n, sunvox_engine* s )
{
    if( s->pats == 0 ) return;
    if( (unsigned)pat_num < (unsigned)s->pats_num )
    {
	sunvox_pattern* pat = s->pats[ pat_num ];
	if( pat == 0 ) return;
	if( line >= pat->lines )
	{
	    sunvox_pattern_set_number_of_lines( pat_num, line + 1, false, s );
	}
	else
	{
	    if( n->note == NOTECMD_NOTE_OFF )
	    {
		if( track < pat->channels )
		{
		    uint8_t note = pat->data[ line * pat->data_xsize + track ].note;
		    if( note > 0 && note < NOTECMD_NOTE_OFF )
		    {
			line++;
			n->ctl = 0;
			if( line >= pat->lines )
			    sunvox_pattern_set_number_of_lines( pat_num, line + 1, false, s );
		    }
		}
	    }
	}
	if( track >= pat->channels )
	    sunvox_pattern_set_number_of_channels( pat_num, track + 1, s );
	pat->data[ line * pat->data_xsize + track ] = *n;
    }
}
static sunvox_note* get_pattern_note( int pat_num, int line, int track, sunvox_engine* s )
{
    if( s->pats == 0 ) return 0;
    if( (unsigned)pat_num >= (unsigned)s->pats_num ) return 0;
    sunvox_pattern* pat = s->pats[ pat_num ];
    if( pat == 0 ) return 0;
    if( line >= pat->lines ) return 0;
    if( track >= pat->channels ) return 0;
    return &pat->data[ line * pat->data_xsize + track ];
}
#define APPLY_MIDI_EVENT_OFFSET() \
    if( s->midi_import_quantization == 0 ) \
    { \
	uint offset = \
    	    ( ( ( global_t & ( global_ticks_per_beat - 1 ) ) % global_ticks_per_line ) * 32 ) \
            / global_ticks_per_line; \
        if( offset >= 32 ) offset = 31; \
        if( offset != 0 ) n.ctl = ( n.ctl & 0xFF00 ) | ( 0x0040 + offset ); \
    }
int sunvox_load_proj_from_fd( sfs_file f, uint load_flags, sunvox_engine* s )
{
    int rv = 0;
    xm_song* xm = NULL;
    midi_file* mf = NULL;
    while( 1 )
    {
	if( check_signature( f ) ) break; 
/* rePlayer begin
	load_flags |= SUNVOX_PROJ_LOAD_SET_BASE_VERSION_TO_CURRENT;
	mf = midi_file_load_from_fd( f );
	if( mf ) break; 
	sfs_rewind( f );
	xm = xm_load_song_from_fd( f );
	if( xm ) break; 
*/ // rePlayer end
        return -1; 
	break;
    }
    sunvox_load_state* st = sunvox_new_load_state( s, f );
    bool load_patterns = false;
    bool load_modules = false;
    if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
    {
	load_patterns = true;
	load_modules = true;
    }
    if( load_flags & SUNVOX_PROJ_LOAD_MERGE_PATTERNS ) load_patterns = true;
    if( load_flags & SUNVOX_PROJ_LOAD_MERGE_MODULES ) load_modules = true;
    if( load_flags & SUNVOX_PROJ_LOAD_MERGE )
    {
	bool stop = true;
	if( load_modules && !load_patterns ) stop = false;
	if( stop && s->playing ) sunvox_stop( s );
	SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_STOP );
    }
    else
    {
	sunvox_engine_close( s );
	uint engine_flags = ( s->flags & ~SUNVOX_FLAG_DEFAULT_CONTENT ) | SUNVOX_FLAG_DONT_START;
	if( load_flags & SUNVOX_PROJ_LOAD_CREATE_PATTERN )
	    engine_flags |= SUNVOX_FLAG_CREATE_PATTERN;
	sunvox_engine_init( engine_flags, s->freq, s->win, s->ss, s->stream_control, s->stream_control_data, s );
	if( load_flags & SUNVOX_PROJ_LOAD_BACKUP_REQUIRED )
	{
	    s->prev_change_counter1 = -1;
	    s->prev_change_counter2 = -1;
	}
        if( load_flags & SUNVOX_PROJ_LOAD_SET_BASE_VERSION_TO_CURRENT )
    	    s->base_version = SUNVOX_ENGINE_VERSION;
	else
	    s->base_version = 0x01070000;
        s->net->base_host_version = s->base_version;
	for( uint sn = 1; sn < s->net->mods_num; sn++ )
	{
	    if( s->net->mods[ sn ].flags & PSYNTH_FLAG_EXISTS )
		psynth_remove_module( sn, s->net );
	}
    	s->flags &= ~( SUNVOX_FLAG_SUPERTRACKS | SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0 | SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0 | SUNVOX_FLAG_KBD_ROUNDROBIN );
	s->jump_request_mode = 0;
    	for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE; i++ )
    	    s->supertrack_mute[ i ] = 0;
    }
    bool loading = true;
    void* pat_data = NULL;
    char* pat_name = NULL;
    int pat_channels = 0;
    int pat_lines = 0;
    int pat_ysize = 0;
    void* pat_icon = NULL;
    uint pat_parent_id = 0;
    bool pat_parent_id_loaded = false;
    int pat_parent = -1;
    uint pat_flags = 0;
    uint pat_parent_flags = 0;
    int pat_x = 0;
    int pat_y = 0;
    int pat_cnt = 0;
    ssymtab* pat_remap = NULL; 
    uint8_t pat_fg[ 3 ];
    uint8_t pat_bg[ 3 ];
    pat_fg[ 0 ] = 0;
    pat_fg[ 1 ] = 0;
    pat_fg[ 2 ] = 0;
    pat_bg[ 0 ] = 255;
    pat_bg[ 1 ] = 255;
    pat_bg[ 2 ] = 255;
    uint s_flags = 0;
    char s_name[ 32 + 1 ]; s_name[ 0 ] = 0;
    PS_RETTYPE (*s_module)( PSYNTH_MODULE_HANDLER_PARAMETERS ) = NULL;
    int s_x = 0;
    int s_y = 0;
    int s_z = 0;
    int s_scale = 256;
    uint s_visualizer_pars = 0;
    bool s_visual_pars_exist = 0;
    uint8_t s_color[ 3 ];
    memset( &s_color, 255, 3 );
    int s_finetune = 0;
    int s_relative_note = 0;
    int* s_links = NULL; 
    int* s_links2 = NULL; 
    int** s_links2_list = NULL; 
    int* s_links0 = NULL; 
    PS_CTYPE* s_ctls = NULL;
    uint* s_midi_pars = NULL;
    char* s_midi_out_name = NULL;
    int s_midi_out_ch = 0;
    int s_midi_out_bank = -1;
    int s_midi_out_prog = -1;
    uint s_midi_in_flags = 0;
    int s_cnt = 0;
    ssymtab* s_remap = NULL; 
    uint c_num = 0;
    int chunk_num = 0;
    int chunks_num = 0;
    psynth_chunk* chunks = NULL;
    uint32_t prev_version = 0; 
    uint32_t new_mod_flags2 = 0; 
    if( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED ) new_mod_flags2 = PSYNTH_FLAG2_JUST_LOADED;
    const int mod_xstep = 128;
    const int mod_ystep = 144;
    if( xm )
    {
	if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
	{
	    char song_name[ 21 ];
	    song_name[ 20 ] = 0;
    	    smem_copy( song_name, xm->name, 20 );
    	    if( ( load_flags & SUNVOX_PROJ_LOAD_WITHOUT_NAME ) == 0 )
    	    {
		if( s->proj_name ) smem_free( s->proj_name );
		s->proj_name = SMEM_STRDUP( song_name );
	    }
	    if( xm->bpm == 0 ) xm->bpm = 125;
	    if( xm->tempo == 0 ) xm->tempo = 6;
	    s->bpm = xm->bpm;
	    s->speed = xm->tempo;
	    s->flags |= SUNVOX_FLAG_SUPERTRACKS;
	}
	int sunvox_instr[ 128 ]; 
	int output_mod = -1;
        for( int i = 0; i < 128; i++ ) sunvox_instr[ i ] = -1;
	if( ( load_flags & SUNVOX_PROJ_LOAD_MERGE ) && !( load_flags & SUNVOX_PROJ_LOAD_MERGE_MODULES ) )
	{
    	    for( int i = 0; i < 128; i++ ) sunvox_instr[ i ] = i + 1;
	}
	if( load_modules )
	{
    	    int mods = 0;
    	    for( int ins_num = 0; ins_num < xm->instruments_num; ins_num++ )
    	    {
        	xm_instrument* ins = xm->instruments[ ins_num ];
                if( !ins ) continue;
                if( ins->samples_num == 0 ) continue;
            	bool samples_avail = false;
            	for( int ss = 0; ss < ins->samples_num; ss++ )
            	{
                    if( ins->samples[ ss ] && ins->samples[ ss ]->length > 0 && ins->samples[ ss ]->data )
                    {
                    	samples_avail = true;
                        break;
                    }
                }
                if( !samples_avail ) continue;
                char instr_name[ 23 ];
                instr_name[ 22 ] = 0;
                smem_copy( instr_name, ins->name, 22 );
                bool name_empty = true;
                for( int n = 0; n < 22; n++ )
                {
                    if( instr_name[ n ] != ' ' && instr_name[ n ] != 0 )
                    {
                        name_empty = false;
                        break;
                    }
                    if( instr_name[ n ] == 0 ) break;
                }
                if( name_empty )
                {
                    instr_name[ 0 ] = 0;
                    sprintf( instr_name, "%02x", mods + 1 );
                }
                int sx = ( mods % 6 - 2 ) * mod_xstep;
                int sy = ( mods / 6 ) * mod_ystep;
                sunvox_instr[ ins_num ] = psynth_add_module( -1, psynth_sampler, instr_name, 0, sx, sy, 0, xm->bpm, xm->tempo, s->net );
                psynth_set_flags2( sunvox_instr[ ins_num ], new_mod_flags2, 0, s->net );
                if( (unsigned)sunvox_instr[ ins_num ] < (unsigned)s->net->mods_num )
                {
                    if( output_mod < 0 )
                    {
                	output_mod = psynth_add_module( -1, psynth_amplifier, "Amplifier", 0, 512, 512+mod_ystep, 0, xm->bpm, xm->tempo, s->net );
                	psynth_set_flags2( output_mod, new_mod_flags2, 0, s->net );
                	psynth_do_command( output_mod, PS_CMD_SETUP_FINISHED, s->net );
                	psynth_make_link( 0, output_mod, s->net );
                    }
                    psynth_module* mod = &s->net->mods[ sunvox_instr[ ins_num ] ];
                    int c = ( ins_num * 4 ) % g_module_colors_num;
                    mod->color[ 0 ] = g_module_colors[ c * 3 + 0 ];
                    mod->color[ 1 ] = g_module_colors[ c * 3 + 1 ];
                    mod->color[ 2 ] = g_module_colors[ c * 3 + 2 ];
                    psynth_do_command( sunvox_instr[ ins_num ], PS_CMD_SETUP_FINISHED, s->net );
                    psynth_make_link( output_mod, sunvox_instr[ ins_num ], s->net );
                    xm_save_instrument( ins_num, "3:/sunvox_instr.xi", xm );
                    sampler_load( "3:/sunvox_instr.xi", 0, sunvox_instr[ ins_num ], s->net, -1, 0 );
                    sfs_remove_file( "3:/sunvox_instr.xi" );
            	}
                mods++;
    	    }
    	}
	if( load_patterns )
	{
    	    int max_xm_channels = 64;
    	    int sunvox_y_pats = max_xm_channels / MAX_PATTERN_TRACKS;
    	    int* sunvox_pats = SMEM_ZALLOC2( int, 256 * sunvox_y_pats );
    	    int16_t* chan_module = SMEM_ZALLOC2( int16_t, max_xm_channels );
    	    int16_t* chan_note = SMEM_ZALLOC2( int16_t, max_xm_channels );
    	    uint8_t* chan_instr = SMEM_ZALLOC2( uint8_t, max_xm_channels );
    	    uint8_t* chan_vol = SMEM_ZALLOC2( uint8_t, max_xm_channels );
    	    uint8_t* chan_eff_0A_par = SMEM_ZALLOC2( uint8_t, max_xm_channels );
    	    if( sunvox_pats && chan_module && chan_note )
    	    {
    		int x = 0;
    		for( int i = 0; i < 256 * sunvox_y_pats; i++ ) sunvox_pats[ i ] = -1;
    		for( int i = 0; i < max_xm_channels; i++ )
    		{
    		    chan_module[ i ] = -1;
    		    chan_note[ i ] = -1;
    		    chan_vol[ i ] = 0x40;
    		}
    		for( int p = 0; p < xm->length; p++ )
        	{
            	    int pat_id = xm->patterntable[ p ];
            	    int pat_rows = 0;
            	    xm_pattern* pat = xm->patterns[ pat_id ];
            	    if( pat )
            	    {
            		pat_rows = pat->rows;
            		for( int ny = 0; ny < pat_rows; ny++ )
            		{
            		    for( int nx = 0; nx < pat->channels; nx++ )
            		    {
            			int src_ptr = ny * pat->channels + nx;
                        	xm_note* note = &pat->pattern_data[ src_ptr ];
                        	if( note->fx == 0xD && note->par == 0 )
                        	{
                        	    pat_rows = ny + 1;
                        	    break;
                        	}
                        	if( note->fx == 0xB )
                        	{
                        	    pat_rows = ny + 1;
                        	    if( note->par < xm->length && note->par > p )
                        		p = note->par - 1;
                        	    break;
                        	}
            		    }
            		}
            	    }
            	    if( pat && pat_rows )
            	    {
                	if( sunvox_pats[ pat_id * sunvox_y_pats + 0 ] == -1 )
                	{
                	    int channels = pat->channels;
                    	    for( int j = 0; channels > 0; j++, channels -= MAX_PATTERN_TRACKS )
                    	    {
                    		int ch = channels;
                    		if( ch > MAX_PATTERN_TRACKS ) ch = MAX_PATTERN_TRACKS;
                        	int n = sunvox_new_pattern( pat_rows, ch, x, j * 32, x + ( j << 8 ), s );
                        	if( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED )
                        	    sunvox_change_pattern_flags( n, 0, SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED, true, s );
				if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
                        	    sunvox_change_pattern_flags( n, SUNVOX_PATTERN_FLAG_NO_NOTES_OFF, 0, true, s );
                        	sunvox_change_pattern_flags( n, SUNVOX_PATTERN_FLAG_NO_ICON, 0, true, s );
                        	char temp_name[ 3 ];
                        	temp_name[ 0 ] = int_to_hchar( ( pat_id >> 4 ) & 15 );
                        	temp_name[ 1 ] = int_to_hchar( pat_id & 15 );
                        	temp_name[ 2 ] = 0;
                        	sunvox_rename_pattern( n, (const char*)temp_name, s );
                        	sunvox_pats[ pat_id * sunvox_y_pats + j ] = n;
                        	sunvox_make_icon( n, s );
                    	    }
    			    for( int ny = 0; ny < pat_rows; ny++ )
                    	    {
                        	for( int nx = 0; nx < pat->channels; nx++ )
                        	{
                            	    int sub_pattern_num = nx / MAX_PATTERN_TRACKS;
                            	    sunvox_pattern* spat = s->pats[ sunvox_pats[ pat_id * sunvox_y_pats + sub_pattern_num ] ];
                            	    int src_ptr = ny * pat->channels + nx;
                            	    int dest_ptr = ny * spat->data_xsize + nx - ( sub_pattern_num * MAX_PATTERN_TRACKS );
                            	    xm_note* note = &pat->pattern_data[ src_ptr ];
                            	    sunvox_note* snote = &spat->data[ dest_ptr ];
                            	    bool change_vol = 0;
                            	    if( note->inst > 0 )
                            	    {
                                	chan_vol[ nx ] = 0x40;
                            	    }
                            	    if( note->n >= 1 && note->n <= 96 )
                            	    {
                            		if( note->inst == 0 )
                            		    note->inst = chan_instr[ nx ];
                                	snote->note = note->n;
                                	chan_note[ nx ] = snote->note;
                            	    }
                            	    if( note->n == 97 ) 
                            	    {
                                	snote->note = 128;
                                	chan_note[ nx ] = -1;
                            	    }
                            	    if( note->inst > 0 )
                            	    {
                                	if( sunvox_instr[ note->inst - 1 ] == -1 )
                                    	    snote->mod = 255; 
                                	else
                                	{
                                    	    chan_module[ nx ] = snote->mod = sunvox_instr[ note->inst - 1 ] + 1;
                                	}
                                	chan_instr[ nx ] = note->inst;
                            		change_vol = 1;
                            	    }
                            	    if( note->vol >= 0x10 && note->vol <= 0x50 )
                            	    {
                                	chan_vol[ nx ] = note->vol - 0x10;
                                	change_vol = 1;
                            	    }
                            	    switch( note->fx )
                            	    {
                            		case 0x00:
                                    	    if( note->par )
                                    	    {
                                        	snote->ctl = 0x0008;
                                        	if( xm->type == SONG_TYPE_XM )
                                        	    snote->ctl_val = ( ( (uint16_t)note->par & 0x0F ) << 8 ) + ( (uint16_t)( note->par & 0xF0 ) >> 4 );
                                        	else
                                        	    snote->ctl_val = ( (uint16_t)note->par & 0x0F ) + ( (uint16_t)( note->par & 0xF0 ) << 4 );
                                    	    }
                                    	    break;
                                	case 0x01:
                                    	    snote->ctl = 0x0001;
                                    	    snote->ctl_val = (uint16_t)note->par * 4;
                                    	    break;
                                	case 0x02:
                                	    snote->ctl = 0x0002;
                                    	    snote->ctl_val = (uint16_t)note->par * 4;
                                    	    break;
                                	case 0x03:
                                    	    snote->ctl = 0x0003;
                                    	    snote->ctl_val = (uint16_t)note->par * 4;
                                    	    break;
                                	case 0x04: 
                                    	    snote->ctl = 0x0004;
                                    	    {
                                    	        int speed = ( note->par >> 4 ) & 0xF;
                                    		int amp = note->par & 0xF;
                                    		snote->ctl_val = (uint16_t)( ( speed << 8 ) | amp );
                                    	    }
                                    	    break;
                                	case 0x08:
                                    	    if( chan_module[ nx ] != -1 )
                                    	    {
                                        	if( snote->note < 1 || snote->note > 128 )
                                        	{
                                            	    snote->mod = 0; 
                                        	}
                                        	snote->ctl = 0x0200;
                                        	snote->ctl_val = (uint16_t)( note->par / 2 ) << 8;
                                    	    }
                                    	    break;
                                	case 0x09:
                                    	    snote->ctl = 0x0009;
                                    	    snote->ctl_val = (uint16_t)note->par;
                                    	    if( xm->type == SONG_TYPE_XM )
                                    	    {
                                        	if( snote->note < 1 || snote->note >= 128 )
                                        	{
                                    		    snote->ctl = 0;
                                    		    snote->ctl_val = 0;
                                        	}
                                    	    }
                                    	    break;
                                	case 0x0A:
                                	    {
                                		int par = note->par;
                                		if( par == 0 ) par = chan_eff_0A_par[ nx ];
                                		chan_eff_0A_par[ nx ] = par;
                                    		snote->ctl = 0x000A;
                                    		snote->ctl_val =
                                        	    ( (uint16_t)( ( ( par >> 4 ) & 0xF ) * 4 ) << 8 ) |
                                        	    ( (uint16_t)( ( ( par >> 0 ) & 0xF ) * 4 ) << 0 );
                                    	    }
                                    	    break;
                                	case 0x0C:
                                	    chan_vol[ nx ] = note->par;
                                	    change_vol = 1;
                                    	    break;
                                	case 0x0E:
                                	    switch( ( note->par >> 4 ) & 0xF )
                                	    {
                                		case 0x1: 
                                		    snote->ctl = 0x0011;
                                		    snote->ctl_val = (uint16_t)( note->par & 0xF ) * 4;
                                		    break;
                                		case 0x2: 
                                		    snote->ctl = 0x0012;
                                		    snote->ctl_val = (uint16_t)( note->par & 0xF ) * 4;
                                		    break;
                                		case 0x9: 
                                		    snote->ctl = 0x0019;
                                		    snote->ctl_val = (uint16_t)( note->par & 0xF );
                                		    break;
                                		case 0xA: 
                                		    {
                                			int par = note->par;
                                			if( par == 0 ) par = chan_eff_0A_par[ nx ];
                                			chan_eff_0A_par[ nx ] = par;
                                    			snote->ctl = 0x001A;
                                    			snote->ctl_val = ( ( par & 0xF ) << 8 ) * 4;
                                    		    }
                                		    break;
                                		case 0xB: 
                                		    {
                                			int par = note->par;
                                			if( par == 0 ) par = chan_eff_0A_par[ nx ];
                                			chan_eff_0A_par[ nx ] = par;
                                    			snote->ctl = 0x001A;
                                    			snote->ctl_val = ( par & 0xF ) * 4;
                                    		    }
                                		    break;
                                		case 0xC: 
                                		    snote->ctl = 0x001B;
                                		    snote->ctl_val = (uint16_t)( note->par & 0xF ) | 0x1000;
                                		    break;
                                		case 0xD: 
                                		    snote->ctl = 0x001D;
                                		    snote->ctl_val = (uint16_t)( note->par & 0xF );
                                		    break;
                                	    }
                                	    break;
                                	case 0x0F:
                                    	    snote->ctl = 0x000F;
                                    	    snote->ctl_val = (uint16_t)note->par;
                                    	    break;
                            	    }
                            	    if( change_vol )
                            	    {
                            		if( chan_vol[ nx ] == 0x40 && snote->mod && snote->note )
                            		{
                            		}
                            		else
                            		{
                            		    snote->vel = chan_vol[ nx ] * 2 + 1;
                            		}
                            	    }
                        	}
                    	    }
            		}
            		else
                	{
                    	    for( int j = 0; j < sunvox_y_pats; j++ )
                    	    {
                    		if( sunvox_pats[ pat_id * sunvox_y_pats + j ] != -1 )
                            	    sunvox_new_pattern_clone( sunvox_pats[ pat_id * sunvox_y_pats + j ], x, j * 32, s );
                    	    }
                	}
                	x += pat_rows;
            	    }
        	}
    		smem_free( sunvox_pats );
    		smem_free( chan_module );
    		smem_free( chan_note );
    		smem_free( chan_instr );
    		smem_free( chan_vol );
    		smem_free( chan_eff_0A_par );
    	    }
    	    {
    		int cfg_pat = sunvox_new_pattern( 64, 1, 0, -32, 1894, s );
        	sunvox_change_pattern_flags( cfg_pat, SUNVOX_PATTERN_FLAG_NO_ICON, 0, true, s );
        	sunvox_rename_pattern( cfg_pat, "Options (compatibility with old tracker formats)", s );
    		if( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED )
        	    sunvox_change_pattern_flags( cfg_pat, 0, SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED, true, s );
    		sunvox_make_icon( cfg_pat, s );
    		sunvox_pattern* cfg_pat_data = sunvox_get_pattern( cfg_pat, s );
    		sunvox_note* n = &cfg_pat_data->data[ 0 ];
    		n->ctl = 0x0034; 
    		n->ctl_val = 0x0100; 
    		n++;
    		n->ctl = 0x0034; 
    		n->ctl_val = 0x0200; 
    	    }
    	}
	s->type = xm->type + 1; // rePlayer
	xm_remove_song( xm );
	goto sunvox_proj_load_end;
    }
    if( mf && mf->tracks_num && mf->tracks )
    {
	if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
	{
    	    s->bpm = 120;
    	    s->speed = s->midi_import_tpl;
    	}
        uint lines_per_beat = 4 * ( 6 / s->speed );
        uint bpm_map_size = 0;
        midi_bpm* bpm_map = SMEM_ZALLOC2( midi_bpm, 256 );
        if( !bpm_map ) goto sunvox_proj_load_end;
        set_bpm_map( &bpm_map, bpm_map_size, 0, 120, 4 );
        bpm_map_size++;
        int tempo_pat = -1;
	if( load_patterns )
	{
    	    tempo_pat = sunvox_new_pattern( 32, 1, 0, 0, 1, s );
    	    if( tempo_pat < 0 )
    	    {
    		slog( "MIDI: Can't create a new pattern\n" );
    		goto sunvox_proj_load_end;
    	    }
    	    if( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED )
    		sunvox_change_pattern_flags( tempo_pat, 0, SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED, true, s );
	    sunvox_rename_pattern( tempo_pat, "Tempo", s );
    	    sunvox_change_pattern_flags( tempo_pat, SUNVOX_PATTERN_FLAG_NO_ICON, 0, true, s );
    	    sunvox_make_icon( tempo_pat, s );
    	}
	int output_mod = -1;
	uint midi_channels_used = 0; 
	int mods[ 16 ]; 
	for( int i = 0; i < 16; i++ ) mods[ i ] = -1;
	int cur_mod = -1;
        int mods_num;
	if( mf->format == 0 )
	    mods_num = 16; 
	else
	    mods_num = 1; 
	for( int iter = 0; iter < 2; iter++ )
	{
	    for( int tnum = 0; tnum < mf->tracks_num; tnum++ )
	    {
		slog( "Track %d decoding...\n", tnum );
		int notes_pat = -1;
		if( iter == 1 )
		{
		    if( load_modules )
		    {
			for( int m = 0; m < mods_num; m++ )
			{
			    int sx = ( ( tnum + m ) % 6 - 2 ) * mod_xstep;
                	    int sy = ( ( tnum + m ) / 6 ) * mod_ystep;
                	    mods[ m ] = psynth_add_module( -1, psynth_generator2, "MIDI Track", 0, sx, sy, 0, s->bpm, s->speed, s->net );
                	    psynth_set_flags2( mods[ m ], new_mod_flags2, 0, s->net );
                	    int ss = mods[ m ];
			    psynth_module* mm = psynth_get_module( ss, s->net );
			    if( mm )
                	    {
                		if( output_mod < 0 )
                		{
                		    output_mod = psynth_add_module( -1, psynth_amplifier, "Amplifier", 0, 512, 512+mod_ystep, 0, s->bpm, s->speed, s->net );
            			    psynth_set_flags2( output_mod, new_mod_flags2, 0, s->net );
                		    psynth_do_command( output_mod, PS_CMD_SETUP_FINISHED, s->net );
                		    psynth_make_link( 0, output_mod, s->net );
                		}
                    		int c = ( ( tnum + m ) * 4 ) % g_module_colors_num;
                    		mm->color[ 0 ] = g_module_colors[ c * 3 + 0 ];
                    		mm->color[ 1 ] = g_module_colors[ c * 3 + 1 ];
                    		mm->color[ 2 ] = g_module_colors[ c * 3 + 2 ];
                    		mm->ctls[ 1 ].val[ 0 ] = 5; 
                    		mm->ctls[ 3 ].val[ 0 ] = 40; 
                    		mm->ctls[ 4 ].val[ 0 ] = 225; 
                    		mm->ctls[ 5 ].val[ 0 ] = 0; 
                    		psynth_do_command( ss, PS_CMD_SETUP_FINISHED, s->net );
                    		psynth_make_link( output_mod, ss, s->net );
                    	    }
                	}
            	    }
            	    else
            	    {
			if( mf->format == 0 )
			{
			    for( int i = 0; i < 16; i++ ) mods[ i ] = i + 1;
			}
			else
			{
			    mods[ 0 ] = tnum + 1;
			}
		    }
		    if( load_patterns )
		    {
    			notes_pat = sunvox_new_pattern( 32, 1, 0, ( tnum + 1 ) * 32, 8 + tnum, s );
    			if( notes_pat < 0 )
    			{
    			    slog( "MIDI: Can't create a new pattern\n" );
    			    break;
    			}
    			if( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED )
                            sunvox_change_pattern_flags( notes_pat, 0, SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED, true, s );
    			sunvox_make_icon( notes_pat, s );
    		    }
                }
		bool ignore_next_tracks = false;
		bool no_notes_in_track = true;
		midi_track* mt = mf->tracks[ tnum ];
		if( mt == 0 ) continue;
		uint8_t* tdata = mt->data;
		if( tdata == 0 ) continue;
		uint tdata_size = smem_get_size( tdata );
		const uint global_ticks_per_beat = 32768;
                uint global_ticks_per_line = global_ticks_per_beat / lines_per_beat;
		uint global_t = 0; 
		uint global_t_rem = 0; 
		uint bpm_map_pos = 0;
		uint t = 0;
		uint16_t midi_notes[ MAX_PATTERN_TRACKS ]; 
		memset( &midi_notes, 255, MAX_PATTERN_TRACKS * 2 );
		uint evt_type = 0;
		uint evt_chan = 0;
		uint meta_chan = 0;
		uint signature_denominator = 4;
		uint tempo = 60000000 / 120;
		uint cur_bpm = 120;
		uint cur_beat_len = 4;
		uint val = 0;
		int bank = 0;
		int prog = 0;
		uint c = 0;
		uint p = 0;
		while( p < tdata_size )
		{
		    val = tdata[ p++ ]; if( val & 0x80 ) { val &= 0x7F; do { c = tdata[ p++ ]; val = ( val << 7 ) + ( c & 0x7F ); } while( c & 0x80 ); }
		    if( iter == 1 && val > 0 )
		    {
			if( mf->ticks_per_frame )
			{
			    global_t += ( ( ( global_ticks_per_beat * s->bpm ) / 60 ) * val ) / ( mf->ticks_per_frame * mf->fps );
			}
			else
			{
			    uint cur_t = t;
			    uint tdelta;
			    while( 1 )
			    {
				if( bpm_map_pos < bpm_map_size )
				{
				    if( bpm_map[ bpm_map_pos ].t <= t + val )
				    {
					tdelta = bpm_map[ bpm_map_pos ].t - cur_t;
					uint ticks_per_beat = ( mf->ticks_per_quarter_note * 4 ) / cur_beat_len;
					global_t += ( tdelta * global_ticks_per_beat + global_t_rem ) / ticks_per_beat;
					global_t_rem = ( tdelta * global_ticks_per_beat + global_t_rem ) % ticks_per_beat;
					cur_t = bpm_map[ bpm_map_pos ].t;
					cur_bpm = bpm_map[ bpm_map_pos ].bpm;
					cur_beat_len = bpm_map[ bpm_map_pos ].beat_len;
					bpm_map_pos++;
					sunvox_note n;
					SMEM_CLEAR_STRUCT( n );
					n.ctl = 0x1F;
					n.ctl_val = cur_bpm;
					set_pattern_note( tempo_pat, ( lines_per_beat * global_t ) / global_ticks_per_beat, 0, &n, s );
					continue;
				    }
				}
				tdelta = t + val - cur_t;
				uint ticks_per_beat = ( mf->ticks_per_quarter_note * 4 ) / cur_beat_len;
				global_t += ( tdelta * global_ticks_per_beat + global_t_rem ) / ticks_per_beat;
				global_t_rem = ( tdelta * global_ticks_per_beat + global_t_rem ) % ticks_per_beat;
				break;
			    }
			}
		    }
		    t += val;
		    c = tdata[ p ];
		    if( c & 0x80 )
		    {
			evt_type = c >> 4;
			evt_chan = c & 15;
                        if( mf->format == 0 )
                        {
			    cur_mod = mods[ evt_chan ];
			}
			else
			{
			    cur_mod = mods[ 0 ];
			}
			if( load_modules )
			{
			    if( evt_type >= 0x8 && evt_type <= 0xE )
			    {
				psynth_module* mm = psynth_get_module( cur_mod, s->net );
				if( mm )
				{
				    mm->midi_out_ch = evt_chan;
				}
			    }
			}
			p++;
		    }
		    int line = ( lines_per_beat * global_t ) / global_ticks_per_beat;
		    switch( evt_type )
		    {
			case 0x8:
			case 0x9:
			    {
				uint8_t note = tdata[ p++ ];
				uint8_t vel = tdata[ p++ ];
				if( iter == 1 )
				{
				    if( evt_type == 0x8 || vel == 0 )
				    {
					for( int track_num = 0; track_num < MAX_PATTERN_TRACKS; track_num++ )
					{
					    if( midi_notes[ track_num ] == ( ( evt_chan << 8 ) | note ) ) 
					    {
						sunvox_note n;
                                    		SMEM_CLEAR_STRUCT( n );
                                    		n.note = NOTECMD_NOTE_OFF;
                                    		APPLY_MIDI_EVENT_OFFSET();
                                    		set_pattern_note( notes_pat, line, track_num, &n, s );
						midi_notes[ track_num ] = 0xFFFF;
						midi_channels_used |= 1 << evt_chan;
						no_notes_in_track = 0;
						break;
					    }
					}
				    }
				    else
				    {
					for( int track_num = 0; track_num < MAX_PATTERN_TRACKS; track_num++ )
					{
					    sunvox_note* prev_note = get_pattern_note( notes_pat, line, track_num, s );
					    bool track_filled = false;
					    if( prev_note )
					    {
						if( prev_note->mod > 0 )
						{
						    track_filled = true;
						}
					    }
					    if( midi_notes[ track_num ] == 0xFFFF && track_filled == false )
					    {
						midi_notes[ track_num ] = ( evt_chan << 8 ) | note;
						sunvox_note n;
                                    		SMEM_CLEAR_STRUCT( n );
                                    		n.note = note + 1;
                                    		n.vel = vel + 1;
                                    		if( vel == 127 )
                                    		    n.vel = 0;
                                    		n.mod = cur_mod + 1;
                                    		APPLY_MIDI_EVENT_OFFSET();
                                    		set_pattern_note( notes_pat, line, track_num, &n, s );
						midi_channels_used |= 1 << evt_chan;
						no_notes_in_track = 0;
						break;
					    }
					}
				    }
				}
			    }
			    break;
			case 0xA:
			    {
				int note = tdata[ p++ ];
				int vel = tdata[ p++ ];
				if( iter == 1 )
				{
				}
			    }
			    break;
			case 0xB:
			    {
				int ctl_num = tdata[ p++ ];
				int ctl_val = tdata[ p++ ];
				if( iter == 1 )
				{
				    if( ctl_num == 0 )
				    {
					bank = ( bank & 127 ) | ( ctl_val << 7 );
				    }
				    if( ctl_num == 0x20 )
				    {
					bank = ( bank & ( 127 << 7 ) ) | ctl_val;
				    }
				    if( load_modules )
					psynth_set_midi_prog( cur_mod, bank, prog, s->net );
				    for( int track_num = 0; track_num < MAX_PATTERN_TRACKS; track_num++ )
				    {
				        sunvox_note* prev_note = get_pattern_note( notes_pat, line, track_num, s );
				        bool track_filled = false;
				        if( prev_note )
				        {
				    	    if( prev_note->mod != 0 || prev_note->ctl != 0 || prev_note->ctl_val != 0 )
					    {
					        track_filled = true;
					    }
					}
					if( midi_notes[ track_num ] == 0xFFFF && track_filled == false )
					{
					    sunvox_note n;
                                    	    SMEM_CLEAR_STRUCT( n );
                                    	    n.mod = cur_mod + 1;
                                    	    n.ctl = ( ctl_num + 0x80 ) << 8;
                                    	    n.ctl_val = ctl_val << 8;
                                    	    APPLY_MIDI_EVENT_OFFSET();
                                    	    set_pattern_note( notes_pat, line, track_num, &n, s );
					    midi_channels_used |= 1 << evt_chan;
					    no_notes_in_track = 0;
					    break;
					}
				    }
				}
			    }
			    break;
			case 0xC:
			    {
				prog = tdata[ p++ ];
				if( iter == 1 )
				{
				    if( load_modules )
					psynth_set_midi_prog( cur_mod, bank, prog, s->net );
				}
			    }
			    break;
			case 0xD:
			    {
				int chan_vel = tdata[ p++ ];
				if( iter == 1 )
				{
				}
			    }
			    break;
			case 0xE:
			    {
				int lsb = tdata[ p++ ];
				int msb = tdata[ p++ ];
				if( iter == 1 )
				{
				}
			    }
			    break;
			case 0xF:
			    if( evt_chan == 0xF )
			    {
				int meta_type = tdata[ p++ ];
				val = tdata[ p++ ]; if( val & 0x80 ) { val &= 0x7F; do { c = tdata[ p++ ]; val = ( val << 7 ) + ( c & 0x7F ); } while( c & 0x80 ); }
				int meta_size = val;
				switch( meta_type )
				{
				    case 0x0: 
					if( meta_size == 2 )
					{
					    int msb = tdata[ p ];
					    int lsb = tdata[ p + 1 ];
					    int num = ( msb << 8 ) | lsb;
					}
					break;
				    case 0x1:
				    case 0x2:
				    case 0x3:
				    case 0x4:
				    case 0x5:
				    case 0x6:
				    case 0x7:
					{
					    char* text = SMEM_ALLOC2( char, meta_size + 1 );
					    if( text )
					    {
						smem_copy( text, tdata + p, meta_size );
						text[ meta_size ] = 0;
						for( int i = meta_size - 1; i >= 0; i-- )
						{
						    if( text[ i ] == ' ' ) 
							text[ i ] = 0;
						    else
							break;
						}
						if( iter == 1 )
						{
						    switch( meta_type )
						    {
							case 0x01: slog( "MIDI Meta Event: Text Event: %s\n", text ); break;
							case 0x02: 
							    slog( "MIDI Meta Event: Copyright Notice: %s\n", text ); 
							    if( ( load_flags & SUNVOX_PROJ_LOAD_WITHOUT_NAME ) == 0 )
							    {
								smem_free( s->proj_name );
								s->proj_name = text;
							    }
							    text = 0;
							    break;
							case 0x03: 
							    slog( "MIDI Meta Event: Track Name: %s\n", text ); 
							    if( load_modules )
								psynth_rename( cur_mod, text, s->net );
							    sunvox_rename_pattern( notes_pat, (const char*)text, s );
							    sunvox_change_pattern_flags( notes_pat, SUNVOX_PATTERN_FLAG_NO_ICON, 0, true, s );
							    break;
							case 0x04: 
							    slog( "MIDI Meta Event: Instrument Name: %s\n", text ); 
							    if( load_modules )
								psynth_rename( cur_mod, text, s->net );
							    break;
							case 0x05: slog( "MIDI Meta Event: Lyrics: %s\n", text ); break;
							case 0x06: slog( "MIDI Meta Event: Marker: %s\n", text ); break;
							case 0x07: slog( "MIDI Meta Event: Cue Point: %s\n", text ); break;
						    }
						}
						smem_free( text );
					    }
					}
					break;
				    case 0x20:
					if( meta_size == 1 )
					{
					    meta_chan = tdata[ p ];
					    if( iter == 1 )
					    {
						if( load_modules )
						{
						    psynth_module* mm = psynth_get_module( cur_mod, s->net );
						    if( mm )
						    {
							mm->midi_out_ch = meta_chan;
						    }
						}
					    }
					}
					break;
				    case 0x2F:
					if( meta_size == 0 )
					{
					}
					break;
				    case 0x51:
					if( meta_size == 3 )
					{
					    tempo = tdata[ p ] << 16; 
					    tempo |= tdata[ p + 1 ] << 8;
					    tempo |= tdata[ p + 2 ];
					    if( iter == 0 )
					    {
						uint bpm = ( 15000000 * signature_denominator ) / tempo;
						set_bpm_map( &bpm_map, bpm_map_size, t, bpm, signature_denominator );
						bpm_map_size++;
						ignore_next_tracks = 1;
						slog( "MIDI Meta Event: Set Tempo %d. BPM %d\n", tempo, bpm );
					    }
					}
					break;
				    case 0x58:
					if( meta_size == 4 )
					{
					    int numerator = tdata[ p ]; 
					    signature_denominator = 1 << tdata[ p + 1 ]; 
					    int metronome = tdata[ p + 2 ];
					    int num32 = tdata[ p + 3 ];
					    if( iter == 0 )
					    {
						uint bpm = ( 15000000 * signature_denominator ) / tempo;
						set_bpm_map( &bpm_map, bpm_map_size, t, bpm, signature_denominator );
						bpm_map_size++;
						ignore_next_tracks = 1;
						slog( "MIDI Meta Event: Time Signature %d / %d; Metronome %d; Number of 32nd per quarter note: %d. BPM %d\n", numerator, signature_denominator, metronome, num32, bpm );
					    }
					}
					break;
				    default:
					break;
				}
				p += meta_size;
			    }
			    else
			    {
				val = tdata[ p++ ]; if( val & 0x80 ) { val &= 0x7F; do { c = tdata[ p++ ]; val = ( val << 7 ) + ( c & 0x7F ); } while( c & 0x80 ); }
				p += val;
			    }
			    break;
		    }
		}
		if( ignore_next_tracks ) break;
		if( iter == 1 )
		{
		    if( load_modules )
		    {
                	if( mf->format == 0 )
                	{
                	}
                	else
                	{
			    if( no_notes_in_track )
				psynth_remove_module( cur_mod, s->net );
			}
		    }
		}
	    }
	}
	if( load_modules )
	{
	    if( mf->format == 0 )
	    {
		for( int i = 0; i < 16; i++ )
		{
		    if( ( midi_channels_used & ( 1 << i ) ) == 0 )
		    {
			psynth_remove_module( mods[ i ], s->net );
		    }
		}
	    }
	}
	smem_free( bpm_map );
	s->type = 3; // rePlayer
	midi_file_remove( mf );
	goto sunvox_proj_load_end;
    }
    while( loading )
    {
        if( load_block( st ) ) { rv = 2; break; }
        if( st->block_id == -1 ) break;
        if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
        {
    	    switch( st->block_id )
    	    {
    		case BID_BVER:
		    if( ( load_flags & SUNVOX_PROJ_LOAD_SET_BASE_VERSION_TO_CURRENT ) == 0 )
		    {
    			s->base_version = st->block_data_int; 
			s->net->base_host_version = s->base_version;
		    }
    		    continue;
    		    break;
    		case BID_VERS: prev_version = st->block_data_int; continue; break;
    		case BID_FLGS:
    		    if( st->block_data_int & ( 1 << 0 ) ) s->flags |= SUNVOX_FLAG_SUPERTRACKS;
    		    if( st->block_data_int & ( 1 << 1 ) ) s->flags |= SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0;
    		    if( st->block_data_int & ( 1 << 2 ) ) s->flags |= SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0;
    		    if( st->block_data_int & ( 1 << 3 ) ) s->flags |= SUNVOX_FLAG_KBD_ROUNDROBIN;
    		    if( st->block_data_int & ( 1 << 4 ) ) s->net->midi_out_7bit = 1; else s->net->midi_out_7bit = 0;
    		    continue;
    		    break;
    		case BID_SFGS: s->sync_flags = st->block_data_int; continue; break;
    		case BID_BPM: s->bpm = st->block_data_int; if( s->bpm == 0 ) s->bpm = 125; continue; break;
    		case BID_SPED: s->speed = st->block_data_int; if( s->speed == 0 ) s->speed = 6; continue; break;
    		case BID_TGRD: s->tgrid = st->block_data_int; continue; break;
    		case BID_TGD2: s->tgrid2 = st->block_data_int; continue; break;
    		case BID_NAME:
    		    if( ( load_flags & SUNVOX_PROJ_LOAD_IGNORE_EMPTY_NAME ) && st->block_data && ((char*)st->block_data)[ 0 ] == 0 )
    		    {
    		    }
    		    else
    		    {
    			if( ( load_flags & SUNVOX_PROJ_LOAD_WITHOUT_NAME ) == 0 )
    			{
    			    if( s->proj_name ) smem_free( s->proj_name );
    			    s->proj_name = (char*)st->block_data;
    			    st->block_data = NULL;
    			}
    		    }
    		    continue;
    		    break;
    		case BID_GVOL: s->net->global_volume = st->block_data_int; continue; break;
    		case BID_MSCL: s->module_scale = st->block_data_int; continue; break;
    		case BID_MZOO: s->module_zoom = st->block_data_int; continue; break;
    		case BID_MXOF: s->xoffset = st->block_data_int; continue; break;
    		case BID_MYOF: s->yoffset = st->block_data_int; continue; break;
    		case BID_LMSK: s->layers_mask = st->block_data_int; continue; break;
    		case BID_CURL: s->cur_layer = st->block_data_int; continue; break;
    		case BID_TIME:
    		    s->line_counter = st->block_data_int; 
#ifdef SUNVOX_GUI
    		    s->center_timeline_request = 1; 
#endif
    		    continue;
    		    break;
    		case BID_REPS: s->restart_pos = st->block_data_int; continue; break;
    		case BID_SELS: s->selected_module = st->block_data_int; continue; break;
    		case BID_LGEN: s->last_selected_generator = st->block_data_int; continue; break;
    		case BID_PATN: s->pat_num = st->block_data_int; continue; break;
    		case BID_PATT: s->pat_track = st->block_data_int; continue; break;
    		case BID_PATL: s->pat_line = st->block_data_int; continue; break;
    		case BID_STMT:
    		{
    		    uint32_t* mute = (uint32_t*)st->block_data;
	    	    for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE && i < (int)( st->block_size / sizeof( uint32_t ) ); i++ )
	    		s->supertrack_mute[ i ] = mute[ i ];
    		    continue;
    		    break;
    		}
    		case BID_JAMD: s->jump_request_mode = st->block_data_int; continue; break;
    	    }
    	}
    	if( load_patterns )
    	{
    	    switch( st->block_id )
    	    {
    		case BID_PDTA: pat_data = st->block_data; st->block_data = NULL; continue; break;
    		case BID_PNME:
    		{
    		    pat_name = (char*)st->block_data; st->block_data = NULL;
    		    size_t old_size = smem_get_size( pat_name );
    		    pat_name = SMEM_ZRESIZE2( pat_name, char, old_size + 4 );
    		    continue;
    		    break;
    		}
    		case BID_PLIN: pat_lines = st->block_data_int; continue; break;
    		case BID_PCHN: pat_channels = st->block_data_int; continue; break;
    		case BID_PYSZ: pat_ysize = st->block_data_int; continue; break;
    		case BID_PICO: pat_icon = st->block_data; st->block_data = NULL; continue; break;
		case BID_PPAR: pat_parent = st->block_data_int; continue; break;
		case BID_PPAR2: pat_parent_id = st->block_data_int; pat_parent_id_loaded = true; continue; break;
    		case BID_PFLG: pat_parent_flags = st->block_data_int; continue; break;
    		case BID_PFFF: pat_flags = st->block_data_int; continue; break;
    		case BID_PXXX: pat_x = st->block_data_int; continue; break;
    		case BID_PYYY: pat_y = st->block_data_int; continue; break;
    		case BID_PFGC: smem_copy( pat_fg, st->block_data, 3 ); continue; break;
    		case BID_PBGC: smem_copy( pat_bg, st->block_data, 3 ); continue; break;
    		case BID_PEND:
    		{
    		    int pat_num;
		    if( pat_parent >= 0 )
		    {
			pat_num = sunvox_new_pattern_empty_clone( pat_parent, pat_x, pat_y, s ); 
		    }
		    else
		    {
			pat_num = sunvox_new_pattern( pat_lines, pat_channels, pat_x, pat_y, 0, s );
		    }
		    if( pat_num < 0 ) { rv = 3; loading = false; break; }
		    sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
		    pat_info->flags = pat_flags | SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED;
		    if( pat_parent >= 0 )
		    {
			pat_info->flags |= SUNVOX_PATTERN_INFO_FLAG_CLONE; 
			if( pat_parent_id_loaded )
			{
			    pat_info->parent_num = -1;
			    for( int i = 0; i < s->pats_num; i++ )
		    	    {
		    		sunvox_pattern* p = sunvox_get_pattern( i, s );
                		sunvox_pattern_info* pinfo = sunvox_get_pattern_info( i, s );
		    		if( p && !( pinfo->flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) )
		    		{
				    if( p->id == pat_parent_id )
	    	    		    {
	    	    		        pat_info->parent_num = i;
	    	    	    		break;
	    	    		    }
	    	    		}
			    }
			    if( pat_info->parent_num == -1 )
			    {
				slog( "Can't find the parent pat. with ID %x. Clone will be removed.\n", pat_parent_id );
				sunvox_remove_pattern( pat_num, s );
			    }
			}
		    }
		    else
		    {
			pat_info->flags &= ~SUNVOX_PATTERN_INFO_FLAG_CLONE; 
			if( pat_icon == 0 || pat_lines <= 0 || pat_channels <= 0 )
			{
			    sunvox_remove_pattern( pat_num, s );
			}
			else
			{
			    sunvox_pattern* pat = s->pats[ pat_num ];
			    smem_free( pat->data );
			    smem_free( pat->name );
#ifdef SUNVOX_LIB
			    if( prev_version < 0x01090500 )
			    {
				sunvox_note* nn = (sunvox_note*)pat_data;
				for( int i = 0; i < pat_channels * pat_lines; i++ )
				{
				    if( nn->mod > 255 )
				    {
					nn->mod &= 255;
				    }
				    nn++;
				}
			    }
#endif 
			    pat->data = (sunvox_note*)pat_data;
			    pat->name = pat_name;
			    pat->flags = pat_parent_flags;
			    pat_data = NULL;
			    pat_name = NULL;
			    pat_parent_flags = 0;
			    pat->data_xsize = pat_channels;
			    pat->data_ysize = pat_lines;
		    	    smem_copy( pat->icon, pat_icon, 32 );
			    smem_copy( pat->fg, pat_fg, 3 );
			    smem_copy( pat->bg, pat_bg, 3 );
			    smem_free( pat_icon );
			    pat_icon = NULL;
			    pat->ysize = pat_ysize;
			    sunvox_make_icon( pat_num, s );
			    if( pat_cnt != pat_num && pat_remap == 0 )
		    	    {
	    		        pat_remap = ssymtab_new( 5 );
				if( !pat_remap )
				{
				    rv = 101;
				    goto sunvox_proj_load_end;
    				}
			    }
			    if( pat_remap )
			    {
				ssymtab_iset( pat_cnt, pat_num, pat_remap ); 
			    }
			}
		    }
    		    if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
    		    {
			if( s->pat_num == pat_cnt )
			{
			    s->pat_num = pat_num;
			}
		    }
		    smem_free( pat_icon ); pat_icon = NULL;
		    smem_free( pat_data ); pat_data = NULL;
		    smem_free( pat_name ); pat_name = NULL;
		    pat_lines = 0;
		    pat_parent_id = 0;
		    pat_parent_id_loaded = false;
		    pat_parent = -1;
		    pat_cnt++;
		    continue;
		    break;
		}
	    } 
	} 
	if( load_modules )
	{
    	    switch( st->block_id )
    	    {
		case BID_SFFF: s_flags = st->block_data_int; continue; break;
		case BID_SNAM:
		{
		    int slen = (int)st->block_size;
		    if( slen < 0 ) slen = 0;
		    if( slen > 32 ) slen = 32;
		    smem_copy( s_name, st->block_data, slen );
		    s_name[ slen ] = 0;
		    continue;
		    break;
		}
		case BID_STYP:
		{
		    if( s_flags & PSYNTH_FLAG_EXISTS )
		    {
			s_module = get_module_handler_by_name( (const char*)st->block_data, s );
			if( !s_module )
			    slog( "Module type \"%s\" not found\n", st->block_data );
		    }
		    else
		    {
	    		s_module = NULL;
		    }
		    continue;
		    break;
		}
		case BID_SFIN: s_finetune = st->block_data_int; continue; break;
		case BID_SREL: s_relative_note = st->block_data_int; continue; break;
		case BID_SXXX: s_x = st->block_data_int; continue; break;
		case BID_SYYY: s_y = st->block_data_int; continue; break;
		case BID_SZZZ: s_z = st->block_data_int; continue; break;
		case BID_SSCL: s_scale = st->block_data_int; continue; break;
		case BID_SVPR: if( ( load_flags & SUNVOX_PROJ_LOAD_WITHOUT_VISUALIZER ) == 0 ) { s_visualizer_pars = st->block_data_int; s_visual_pars_exist = 1; } continue; break;
		case BID_SCOL: smem_copy( s_color, st->block_data, 3 ); continue; break;
		case BID_SMII: s_midi_in_flags = st->block_data_int; continue; break;
		case BID_SMIN: s_midi_out_name = (char*)st->block_data; st->block_data = NULL; continue; break;
		case BID_SMIC: s_midi_out_ch = st->block_data_int; continue; break;
		case BID_SMIB: s_midi_out_bank = st->block_data_int; continue; break;
		case BID_SMIP: s_midi_out_prog = st->block_data_int; continue; break;
		case BID_SLNK: s_links = (int*)st->block_data; st->block_data = NULL; continue; break;
		case BID_SLnK: s_links2 = (int*)st->block_data; st->block_data = NULL; continue; break;
		case BID_CVAL:
		{
		    if( c_num + 1 > smem_get_size( s_ctls ) / sizeof( PS_CTYPE ) )
        	    {
            		s_ctls = SMEM_RESIZE2( s_ctls, PS_CTYPE, c_num + 16 );
            		if( !s_ctls ) { rv = 4; loading = false; break; }
        	    }
        	    if( s_ctls )
        	    {
            		s_ctls[ c_num ] = st->block_data_int;
            		c_num++;
        	    }
        	    continue;
        	    break;
		}
		case BID_CMID:
    		{
    		    s_midi_pars = (uint*)st->block_data;
        	    st->block_data = NULL;
        	    continue;
        	    break;
    		}
		case BID_CHNK:
		{
	    	    chunks_num = st->block_data_int;
		    chunks = SMEM_ZALLOC2( psynth_chunk, chunks_num );
		    if( !chunks ) { rv = 5; loading = false; break; }
		    continue;
		    break;
		}
		case BID_CHNM: chunk_num = st->block_data_int; continue; break;
		case BID_CHDT:
		{
		    chunks[ chunk_num ].data = st->block_data;
		    st->block_data = NULL;
		    continue;
		    break;
		}
		case BID_CHFF:
		{
		    chunks[ chunk_num ].flags = st->block_data_int;
		    continue;
		    break;
		}
		case BID_CHFR:
		{
		    chunks[ chunk_num ].freq = st->block_data_int;
		    continue;
		    break;
		}
		case BID_SEND:
		{
		    int s_num = -1;
		    if( !( s_flags & PSYNTH_FLAG_EXISTS ) )
		    {
	    		if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
	    		{
		    	    s_num = psynth_add_module( -1, psynth_empty, "", 0, 0, 0, 0, s->bpm, s->speed, s->net );
			    if( s_num < 0 ) { rv = 6; loading = false; break; }
			    psynth_remove_module( s_num, s->net );
			    s->net->mods[ s_num ].flags2 = PSYNTH_FLAG2_LAST; 
			    s->net->mods[ s_num ].flags = PSYNTH_FLAG_EXISTS; 
			}
		    }
		    else
		    {
			if( s_cnt == 0 )
		    	    s_num = 0; 
			else
			{
		    	    s_num = psynth_add_module( -1, s_module, s_name, s_flags, s_x, s_y, s_z, s->bpm, s->speed, s->net );
			    psynth_set_flags2( s_num, PSYNTH_FLAG2_JUST_LOADED, 0, s->net );
			}
			if( s_num < 0 ) { rv = 7; loading = false; break; }
			psynth_module* m = &s->net->mods[ s_num ];
			if( s_cnt != 0 )
			{
		    	    if( s_links )
		    	    {
		    	        m->input_links = s_links;
				m->input_links_num = smem_get_size( s_links ) / sizeof( int );
			    }
			    if( s_links2 )
			    {
				SMEM_OBJARRAY_WRITE_D( (void***)&s_links2_list, s_links2, false, s_num );
			    }
			    if( s_cnt != s_num && s_remap == 0 )
		    	    {
	    			s_remap = ssymtab_new( 5 );
				if( s_remap == 0 ) 
				{
				    rv = 102;
				    goto sunvox_proj_load_end;
    				}
			    }
			    if( s_remap )
			    {
		    		ssymtab_iset( s_cnt, s_num, s_remap ); 
			    }
			}
			else
			{
	    		    s_links0 = s_links;
			    if( s_links2 )
			    {
				SMEM_OBJARRAY_WRITE_D( (void***)&s_links2_list, s_links2, false, 0 );
			    }
			}
	    		if( ( load_flags & SUNVOX_PROJ_LOAD_MERGE ) && s_cnt == 0 )
	    		{
	    		}
	    		else
	    		{
			    m->finetune = s_finetune;
			    m->relative_note = s_relative_note;
			    m->x = s_x;
			    m->y = s_y;
			    m->z = s_z;
			    m->scale = (uint16_t)s_scale;
			    if( s_visual_pars_exist )
			    {
		    		m->visualizer_pars = s_visualizer_pars;
			    }
			    smem_copy( &m->color, &s_color, 3 );
			    if( c_num > m->ctls_num )
                		psynth_change_ctls_num( s_num, c_num, s->net );
            		    if( s_ctls )
            		    {
				for( uint cc = 0; cc < c_num; cc++ )
				{
				    psynth_ctl* c = &m->ctls[ cc ];
				    if( c->val )
					*c->val = s_ctls[ cc ];
				}
			    }
			    if( s_midi_pars )
            		    {
                		uint midi_pars_num = smem_get_size( s_midi_pars ) / ( sizeof( uint ) * 2 );
	            		if( midi_pars_num > c_num ) midi_pars_num = c_num;
    		    		for( uint cc = 0; cc < midi_pars_num; cc++ )
            			{
            		    	    psynth_set_ctl_midi_in( s_num, cc, s_midi_pars[ cc * 2 + 0 ], s_midi_pars[ cc * 2 + 1 ], s->net );
                		}
	            		smem_free( s_midi_pars );
    		    		s_midi_pars = NULL;
            		    }
			    if( chunks )
			    {
				for( int c = 0; c < chunks_num; c++ )
				{
				    if( chunks[ c ].data )
				    {
					psynth_new_chunk( s_num, c, &chunks[ c ], s->net );
				    }
				}
				smem_free( chunks );
				chunks = NULL;
			    }
			    m->midi_in_flags = s_midi_in_flags;
			    psynth_handle_midi_in_flags( s_num, s->net );
			    psynth_set_midi_prog( s_num, s_midi_out_bank, s_midi_out_prog, s->net );
			    psynth_open_midi_out( s_num, s_midi_out_name, s_midi_out_ch, s->net );
			    psynth_do_command( s_num, PS_CMD_SETUP_FINISHED, s->net );
			}
		    }
		    s_links = NULL;
		    s_links2 = NULL;
		    s_flags = 0;
		    s_module = NULL;
		    s_scale = 256;
		    s_visual_pars_exist = 0;
		    memset( &s_color, 255, 3 );
		    s_finetune = 0;
		    s_relative_note = 0;
		    smem_free( s_midi_out_name );
		    s_midi_out_name = NULL;
		    s_midi_out_ch = 0;
		    s_midi_out_bank = -1;
		    s_midi_out_prog = -1;
		    s_midi_in_flags = 0;
		    c_num = 0;
		    s_cnt++;
		    continue;
		    break;
		}
	    } 
	} 
    }
    if( load_patterns )
    {
	for( int i = 0; i < s->pats_num; i++ )
	{
	    sunvox_pattern_info* pat_info = &s->pats_info[ i ];
	    if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED )
	    {
		if( !( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED ) )
		    pat_info->flags &= ~SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED;
		if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_CLONE )
		{
		    int parent = pat_info->parent_num;
		    if( pat_remap )
		    {
			parent = ssymtab_iget( parent, parent, pat_remap );
			pat_info->parent_num = parent;
		    }
		    if( (unsigned)parent < (unsigned)s->pats_num )
		    {
		        s->pats[ i ] = s->pats[ parent ];
		    }
		    else
		    {
		        slog( "Can't find parent for pattern %d. Will be removed.\n", i );
			sunvox_remove_pattern( i, s );
		    }
		}
		else
		{
		    sunvox_pattern* pat = s->pats[ i ];
		    if( pat )
		    {
			if( s_remap )
			{
		    	    sunvox_note* sn = pat->data;
		    	    for( int sn_cnt = 0; sn_cnt < pat->data_xsize * pat->data_ysize; sn_cnt++ )
		    	    {
		    		if( sn->mod > 1 )
				    sn->mod = ssymtab_iget( sn->mod - 1, sn->mod - 1, s_remap ) + 1;
		    		sn++;
		    	    }
			}
		    }
		}
		if( s->pats[ i ] == (sunvox_pattern*)1 )
		{
		    slog( "Wrong pattern %d address!!!\n", i );
		    s->pats[ i ] = 0;
		}
	    }
	}
	sunvox_check_solo_mode( s );
    }
    if( load_modules )
    {
	for( uint i = 1; i < s->net->mods_num; i++ )
	{
	    psynth_module* m = &s->net->mods[ i ];
	    if( m->flags == PSYNTH_FLAG_EXISTS && m->flags2 == PSYNTH_FLAG2_LAST )
	    {
		m->flags = 0;
		m->flags2 = 0;
	    }
	    if( ( m->flags & PSYNTH_FLAG_EXISTS ) && ( m->flags2 & PSYNTH_FLAG2_JUST_LOADED ) )
	    {
		if( !( load_flags & SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED ) )
		    m->flags2 &= ~PSYNTH_FLAG2_JUST_LOADED;
		if( m->input_links_num && m->input_links )
		{
	    	    for( uint l = 0; l < (unsigned)m->input_links_num; l++ )
	    	    {
	    		int in = m->input_links[ l ];
			if( s_remap )
			{
			    in = ssymtab_iget( in, in, s_remap );
			    m->input_links[ l ] = in;
			}
	    		if( (unsigned)in < (unsigned)s->net->mods_num )
			{
			    psynth_module* input_mod = &s->net->mods[ in ];
			    if( ( input_mod->flags & PSYNTH_FLAG_EXISTS ) == 0 )
			    {
				slog( "%d: %s: missing input module %d\n", i, m->name, in );
				m->input_links[ l ] = 0;
			    }
			    else
			    {
				int link_slot = -1;
				if( s_links2_list )
				{
				    if( i < smem_get_size( s_links2_list ) / sizeof( int* ) )
				    {
					int* links2 = s_links2_list[ i ];
					if( links2 )
					{
					    if( l < smem_get_size( links2 ) / sizeof( int ) )
					    {
						link_slot = links2[ l ];
					    }
					}
				    }
				}
				psynth_add_link( false, in, i, link_slot, s->net );
			    }
			}
		    }
		}
	    }
	}
	if( s_links0 )
	{
	    for( size_t l = 0; l < smem_get_size( s_links0 ) / sizeof( int ); l++ )
	    {
	    	int in = s_links0[ l ];
		if( s_remap )
		{
		    in = ssymtab_iget( in, in, s_remap );
		}
		psynth_make_link( 0, in, s->net );
	    }
	}
	if( !( load_flags & SUNVOX_PROJ_LOAD_MERGE ) )
	{
	    psynth_remove_empty_modules_at_the_end( s->net );
	}
    }
sunvox_proj_load_end:
    sunvox_remove_load_state( st );
#ifdef DEBUG_MESSAGES
    sunvox_print_patterns( s );
#endif
    sunvox_update_icons( s );
    ssymtab_delete( pat_remap );
    ssymtab_delete( s_remap );
    if( s_links2_list )
    {
	for( size_t i = 0; i < smem_get_size( s_links2_list ) / sizeof( int* ); i++ )
	{
	    smem_free( s_links2_list[ i ] );
	}
	smem_free( s_links2_list );
    }
    smem_free( s_links0 );
    smem_free( s_ctls );
    if( load_flags & SUNVOX_PROJ_LOAD_MAKE_TIMELINE_STATIC )
    {
        s->flags &= ~SUNVOX_FLAG_STATIC_TIMELINE;
	sunvox_sort_patterns( s );
        s->flags |= SUNVOX_FLAG_STATIC_TIMELINE;
    }
    if( load_flags & SUNVOX_PROJ_LOAD_MERGE )
    {
	SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_PLAY );
    }
    else
    {
	s->initialized = 1;
    }
    if( rv )
    {
	slog( "sunvox_load_proj_from_fd() error %d\n", rv );
    }
    return rv;
}
int sunvox_load_proj( const char* name, uint load_flags, sunvox_engine* s )
{
    int rv = 0;
    sfs_file f = sfs_open( name, "rb" );
    if( f )
    {
	rv = sunvox_load_proj_from_fd( f, load_flags, s );
	sfs_close( f );
    }
    else
    {
	slog( "Can't open file %s\n", name );
	return -1;
    }
    if( rv == 0 )
    {
	if( s->base_version > SUNVOX_ENGINE_VERSION )
	{
	    slog( 
		"WARNING: project \"%s\" was created in a newer SunVox v%d.%d.%d.%d, so it may be loaded incorrectly (some data may be lost or broken)!\n", 
		name, 
		( s->base_version >> 24 ) & 255,
		( s->base_version >> 16 ) & 255,
		( s->base_version >> 8 ) & 255,
		s->base_version & 255
	    );
	}
    }
    return rv;
}
bool sunvox_proj_filefmt_supported( sfs_file_fmt fmt )
{
    return
	fmt == SFS_FILE_FMT_SUNVOX ||
	fmt == SFS_FILE_FMT_MIDI ||
	fmt == SFS_FILE_FMT_XM ||
	fmt == SFS_FILE_FMT_MOD;
}
