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
int sunvox_load_module_from_fd( int mod_num, int x, int y, int z, sfs_file f, uint load_flags, sunvox_engine* s )
{
    int retval = -1;
    bool add_info = false;
    if( load_flags & SUNVOX_MODULE_LOAD_ADD_INFO )
    {
	x = 0;
	y = 0;
	add_info = true;
    }
    uint8_t sign[ 8 ];
    memset( sign, 0, sizeof( sign ) );
    sfs_read( sign, 8, 1, f );
    if( memcmp( sign, g_sunvox_module_sign, sizeof( sign ) ) ) return -1;
    bool loading = true;
    bool locked = 0;
    PS_CTYPE* s_ctls = NULL;
    uint* s_midi_pars = NULL;
    int* s_input_links = NULL;
    int* s_input_links2 = NULL; 
    int* s_output_links = NULL;
    int* s_output_links2 = NULL; 
    int s_color[ 3 ];
    s_color[ 0 ] = 255;
    s_color[ 1 ] = 255;
    s_color[ 2 ] = 255;
    int s_scale = 256;
    uint s_visualizer_pars = 0;
    bool s_visual_pars_exist = 0;
    char* s_midi_out_name = NULL;
    int s_midi_out_ch = 0;
    int s_midi_out_bank = -1;
    int s_midi_out_prog = -1;
    uint s_midi_in_flags = 0;
    uint c_num = 0;
    int chunk_num = 0;
    int chunks_num = 0;
    psynth_chunk* chunks = NULL;
    char s_name[ 32 + 1 ]; s_name[ 0 ] = 0;
    uint flags = 0;
    int finetune = 0;
    int relative = 0;
    PS_RETTYPE (*s_module)( PSYNTH_MODULE_HANDLER_PARAMETERS ) = NULL;
    sunvox_load_state* st = sunvox_new_load_state( s, f );
    while( loading )
    {
	if( load_block( st ) ) break;
	if( st->block_id == -1 ) break;
	switch( st->block_id )
	{
	    case BID_VERS: continue; break;
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
	    case BID_SFFF:
		flags = st->block_data_int;
		if( load_flags & SUNVOX_MODULE_LOAD_IGNORE_MSB_FLAGS )
		    flags &= ~PSYNTH_FLAG_MSB;
		continue;
		break;
	    case BID_SFIN: finetune = st->block_data_int; continue; break;
	    case BID_SREL: relative = st->block_data_int; continue; break;
	    case BID_SSCL: s_scale = st->block_data_int; continue; break;
	    case BID_SVPR: s_visualizer_pars = st->block_data_int; s_visual_pars_exist = 1; continue; break;
	    case BID_SCOL: smem_copy( s_color, st->block_data, 3 ); continue; break;
	    case BID_SMII: s_midi_in_flags = st->block_data_int; continue; break;
	    case BID_SMIN: s_midi_out_name = (char*)st->block_data; st->block_data = NULL; continue; break;
	    case BID_SMIC: s_midi_out_ch = st->block_data_int; continue; break;
	    case BID_SMIB: s_midi_out_bank = st->block_data_int; continue; break;
	    case BID_SMIP: s_midi_out_prog = st->block_data_int; continue; break;
	    case BID_STYP:
		s_module = get_module_handler_by_name( (const char*)st->block_data, s );
		continue;
		break;
	    case BID_CVAL:
		if( c_num + 1 > smem_get_size( s_ctls ) / sizeof( PS_CTYPE ) )
		{
		    s_ctls = SMEM_RESIZE2( s_ctls, PS_CTYPE, c_num + 16 );
		    if( !s_ctls )
		    {
		        slog( "ERROR: ctls allocation error!\n" ); 
		    }
		}
		if( s_ctls )
		{
		    s_ctls[ c_num ] = st->block_data_int; 
		    c_num++; 
		}
		continue;
		break;
	    case BID_CMID:
		s_midi_pars = (uint*)st->block_data;
		st->block_data = NULL;
		continue;
		break;
	    case BID_CHNK:
		chunks_num = st->block_data_int;
		chunks = SMEM_ZALLOC2( psynth_chunk, chunks_num );
		continue;
		break;
	    case BID_CHNM: chunk_num = st->block_data_int; continue; break;
	    case BID_CHDT:
		chunks[ chunk_num ].data = st->block_data; 
		st->block_data = NULL;
		continue;
		break;
	    case BID_CHFF:
		chunks[ chunk_num ].flags = st->block_data_int;
		continue;
		break;
	    case BID_CHFR:
		chunks[ chunk_num ].freq = st->block_data_int;
		continue;
		break;
	    case BID_SEND:
	        if( s_module == psynth_empty )
		{
		    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_LOCK );
		    retval = psynth_add_module( mod_num, s_module, "Unsupported", flags, x, y, z, s->bpm, s->speed, s->net );
		    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_UNLOCK );
		}
		else
		{
		    SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_LOCK ); locked = 1;
		    retval = psynth_add_module( mod_num, s_module, s_name, flags, x, y, z, s->bpm, s->speed, s->net );
		    if( retval < 0 ) { loading = false; break; }
		    psynth_module* m = &s->net->mods[ retval ];
    		    m->finetune = finetune;
		    m->relative_note = relative;
		    m->scale = (uint16_t)s_scale;
		    if( s_visual_pars_exist )
		    {
		        m->visualizer_pars = s_visualizer_pars;
		    }
		    smem_copy( &m->color, &s_color, 3 );
		    if( s_input_links )
		    {
			m->input_links = s_input_links;
			m->input_links_num = smem_get_size( s_input_links ) / sizeof( int );
		    }
		    if( s_output_links )
		    {
			m->output_links = s_output_links;
			m->output_links_num = smem_get_size( s_output_links ) / sizeof( int );
		    }
		    if( s_input_links )
		    {
			for( int i = 0; i < m->input_links_num; i++ )
			{
			    if( m->input_links[ i ] >= 0 )
			    {
				int link_slot = -1;
				if( s_input_links2 ) link_slot = s_input_links2[ i ];
		    		psynth_add_link( false, m->input_links[ i ], retval, link_slot, s->net );
			    }
			}
		    }
		    if( s_output_links )
		    {
			for( int i = 0; i < m->output_links_num; i++ )
			{
			    if( m->output_links[ i ] >= 0 )
			    {
				int link_slot = -1;
				if( s_output_links2 ) link_slot = s_output_links2[ i ];
				psynth_add_link( true, m->output_links[ i ], retval, link_slot, s->net );
			    }
			}
		    }
		    if( c_num > m->ctls_num )
            		psynth_change_ctls_num( retval, c_num, s->net );
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
			    psynth_set_ctl_midi_in( retval, cc, s_midi_pars[ cc * 2 + 0 ], s_midi_pars[ cc * 2 + 1 ], s->net );
			}
		    }
		    if( chunks )
		    {
			for( int c = 0; c < chunks_num; c++ )
                	{
                    	    if( chunks[ c ].data )
                    	    {
                    		psynth_new_chunk( retval, c, &chunks[ c ], s->net );
                        	chunks[ c ].data = NULL;
                    	    }
                	}
		    }
            	    m->midi_in_flags = s_midi_in_flags;
            	    psynth_handle_midi_in_flags( retval, s->net );
		    psynth_set_midi_prog( retval, s_midi_out_bank, s_midi_out_prog, s->net );
		    psynth_open_midi_out( retval, s_midi_out_name, s_midi_out_ch, s->net );
		    psynth_do_command( retval, PS_CMD_SETUP_FINISHED, s->net );
		}
		loading = false;
		break;
	} 
	if( add_info )
	{
	    switch( st->block_id )
	    {
		case BID_SXXX: x = st->block_data_int; continue; break;
	        case BID_SYYY: y = st->block_data_int; continue; break;
	        case BID_SZZZ: z = st->block_data_int; continue; break;
    	        case BID_SLNK: s_input_links = (int*)st->block_data; st->block_data = NULL; continue; break;
	        case BID_SLnK: s_input_links2 = (int*)st->block_data; st->block_data = NULL; continue; break;
	        case BID_SLNk: s_output_links = (int*)st->block_data; st->block_data = NULL; continue; break;
	        case BID_SLnk: s_output_links2 = (int*)st->block_data; st->block_data = NULL; continue; break;
	    } 
	}
    }
    if( locked ) { SUNVOX_SOUND_STREAM_CONTROL( s, SUNVOX_STREAM_UNLOCK ); locked = 0; }
    smem_free( s_midi_out_name );
    smem_free( s_input_links2 );
    smem_free( s_output_links2 );
    smem_free( s_ctls );
    smem_free( s_midi_pars );
    if( chunks )
    {
	for( int i = 0; i < chunks_num; i++ )
	{
	    smem_free( chunks[ i ].data );
	}
	smem_free( chunks );
    }
    sunvox_remove_load_state( st );
    return retval;
}
int sunvox_load_module( int mod_num, int x, int y, int z, const char* name, uint load_flags, sunvox_engine* s )
{
    int retval = -1;
    sfs_file f = sfs_open( name, "rb" );
    if( f )
    {
	retval = sunvox_load_module_from_fd( mod_num, x, y, z, f, load_flags, s );
	sfs_close( f );
    }
    return retval;
}
