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
#include "xm/xm.h"
static int sunvox_get_num_pats( sunvox_engine* s )
{
    int pats_num = s->pats_num;
    for( int i = s->pats_num - 1; i >= 0; i-- )
    {
	if( !s->pats[ i ] )
	    pats_num--;
	else
	    break;
    }
    return pats_num;
}
static int sunvox_get_num_mods( sunvox_engine* s )
{
    psynth_net* net = s->net;
    int mods_num = net->mods_num;
    for( int i = (int)net->mods_num - 1; i >= 0; i-- )
    {
	psynth_module* mod = &net->mods[ i ];
	if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 )
	    mods_num--;
	else
	    break;
    }
    return mods_num;
}
int* sunvox_save_get_mod_remap_table( sunvox_engine* s, uint32_t save_flags )
{
    if( ( save_flags & SUNVOX_PROJ_SAVE_OPTIMIZE_SLOTS ) == 0 ) return NULL;
    psynth_net* net = s->net;
    int mods_num = sunvox_get_num_mods( s );
    int* rv = NULL;
    rv = SMEM_ALLOC2( int, mods_num );
    if( !rv ) return NULL;
    for( int i = 0; i < mods_num; i++ ) rv[ i ] = -1;
    int i2 = 0;
    for( int i = 0; i < mods_num; i++ )
    {
        psynth_module* mod = &net->mods[ i ];
	if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) continue;
	if( ( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 ) && ( mod->flags2 & PSYNTH_FLAG2_SELECTED2 ) == 0 ) continue;
	rv[ i ] = i2;
	i2++;
    }
    return rv;
}
int* sunvox_save_get_pat_remap_table( sunvox_engine* s, uint32_t save_flags )
{
    if( ( save_flags & SUNVOX_PROJ_SAVE_OPTIMIZE_SLOTS ) == 0 ) return NULL;
    psynth_net* net = s->net;
    int pats_num = sunvox_get_num_pats( s );
    int* rv = NULL;
    rv = SMEM_ZALLOC2( int, pats_num );
    if( !rv ) return NULL;
    int i2 = 0;
    for( int i = 0; i < pats_num; i++ )
    {
        sunvox_pattern* pat = s->pats[ i ];
        sunvox_pattern_info* pat_info = &s->pats_info[ i ];
        if( !pat ) continue;
        if( ( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 ) && ( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_SELECTED2 ) == 0 ) continue;
        rv[ i ] = i2;
	i2++;
    }
    return rv;
}
int sunvox_save_proj_to_fd( sfs_file f, uint32_t save_flags, sunvox_engine* s )
{
    int retval = 1;
    psynth_net* net = s->net;
    uint32_t version = SUNVOX_ENGINE_VERSION;
    sunvox_save_state* st = sunvox_new_save_state( s, f );
    int pats_num = sunvox_get_num_pats( s );
    int mods_num = sunvox_get_num_mods( s );
    st->mod_remap = sunvox_save_get_mod_remap_table( s, save_flags );
    st->pat_remap = sunvox_save_get_pat_remap_table( s, save_flags );
    sfs_write( g_sunvox_sign, 1, 8, f );
    int temp_val;
    if( save_block( BID_VERS, 4, &version, st ) ) goto save_proj_error;
    if( save_block( BID_BVER, 4, &s->base_version, st ) ) goto save_proj_error;
    temp_val = 0;
    if( s->flags & SUNVOX_FLAG_SUPERTRACKS ) temp_val |= 1 << 0;
    if( s->flags & SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0 ) temp_val |= 1 << 1;
    if( s->flags & SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0 ) temp_val |= 1 << 2;
    if( s->flags & SUNVOX_FLAG_KBD_ROUNDROBIN ) temp_val |= 1 << 3;
    if( net->midi_out_7bit ) temp_val |= 1 << 4;
    if( save_block( BID_FLGS, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->sync_flags; if( save_block( BID_SFGS, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->bpm; if( save_block( BID_BPM, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->speed; if( save_block( BID_SPED, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->tgrid; if( save_block( BID_TGRD, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->tgrid2; if( save_block( BID_TGD2, 4, &temp_val, st ) ) goto save_proj_error;
    temp_val = s->net->global_volume; if( save_block( BID_GVOL, 4, &temp_val, st ) ) goto save_proj_error;
    if( s->proj_name ) { if( save_block( BID_NAME, smem_strlen( s->proj_name ) + 1, s->proj_name, st ) ) goto save_proj_error; }
    if( save_block( BID_MSCL, 4, &s->module_scale, st ) ) goto save_proj_error;
    if( save_block( BID_MZOO, 4, &s->module_zoom, st ) ) goto save_proj_error;
    if( save_block( BID_MXOF, 4, &s->xoffset, st ) ) goto save_proj_error;
    if( save_block( BID_MYOF, 4, &s->yoffset, st ) ) goto save_proj_error;
    if( save_block( BID_LMSK, 4, &s->layers_mask, st ) ) goto save_proj_error;
    if( save_block( BID_CURL, 4, &s->cur_layer, st ) ) goto save_proj_error;
    if( s->line_counter ) { if( save_block( BID_TIME, 4, &s->line_counter, st ) ) goto save_proj_error; }
    if( s->restart_pos ) { if( save_block( BID_REPS, 4, &s->restart_pos, st ) ) goto save_proj_error; }
    if( st->mod_remap && s->selected_module >= 0 && s->selected_module < mods_num )
    {
	if( save_block( BID_SELS, 4, &st->mod_remap[ s->selected_module ], st ) ) goto save_proj_error;
    }
    else
    {
	if( save_block( BID_SELS, 4, &s->selected_module, st ) ) goto save_proj_error;
    }
    if( st->mod_remap && s->last_selected_generator >= 0 && s->last_selected_generator < mods_num )
    {
	if( save_block( BID_LGEN, 4, &st->mod_remap[ s->last_selected_generator ], st ) ) goto save_proj_error;
    }
    else
    {
	if( save_block( BID_LGEN, 4, &s->last_selected_generator, st ) ) goto save_proj_error;
    }
    if( st->pat_remap && s->pat_num >= 0 && s->pat_num < pats_num )
    {
	if( save_block( BID_PATN, 4, &st->pat_remap[ s->pat_num ], st ) ) goto save_proj_error;
    }
    else
    {
	if( save_block( BID_PATN, 4, &s->pat_num, st ) ) goto save_proj_error;
    }
    if( save_block( BID_PATT, 4, &s->pat_track, st ) ) goto save_proj_error;
    if( save_block( BID_PATL, 4, &s->pat_line, st ) ) goto save_proj_error;
    temp_val = 0;
    for( int i = 0; i < SUPERTRACK_BITARRAY_SIZE; i++ )
	if( s->supertrack_mute[ i ] ) temp_val++;
    if( temp_val )
    {
	if( save_block( BID_STMT, sizeof( s->supertrack_mute ), s->supertrack_mute, st ) ) goto save_proj_error;
    }
    if( s->jump_request_mode )
    {
	temp_val = s->jump_request_mode;
	if( save_block( BID_JAMD, 4, &temp_val, st ) ) goto save_proj_error;
    }
    for( int i = 0; i < pats_num; i++ )
    {
	sunvox_pattern* pat = s->pats[ i ];
	sunvox_pattern_info* pat_info = &s->pats_info[ i ];
	bool save_pat = false;
	if( pat )
	{
	    save_pat = true;
	    if( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 )
	    {
		save_pat = false;
		if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_SELECTED2 )
		    save_pat = true;
	    }
	}
        if( save_pat )
        {
    	    if( !( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) )
	    {
	        int max_tracks = pat->channels;
	        if( pat->data )
		{
		    if( save_flags & SUNVOX_PROJ_SAVE_OPTIMIZE_PAT_SIZE )
		    {
			max_tracks = 0;
			for( int yy = 0; yy < pat->lines; yy++ )
			{
		    	    int pat_ptr = yy * pat->data_xsize;
		    	    for( int xx = 0; xx < pat->channels; xx++ )
		    	    {
				sunvox_note* snote = &pat->data[ pat_ptr ];
				if( snote->note ||
			    	    snote->vel ||
			    	    snote->mod ||
			    	    snote->ctl ||
			    	    snote->ctl_val )
				{
			    	    if( xx + 1 > max_tracks )
			    	        max_tracks = xx + 1;
				}
				pat_ptr++;
			    }
			}
			if( max_tracks == 0 ) max_tracks = 1;
		    }
		    if( ( pat->data_xsize == max_tracks ) && st->mod_remap == NULL )
		    {
			if( save_block( BID_PDTA, pat->lines * pat->data_xsize * sizeof( sunvox_note ), pat->data, st ) ) goto save_proj_error;
		    }
		    else
		    {
			sunvox_note* tpat = SMEM_ALLOC2( sunvox_note, max_tracks * pat->lines );
			if( !tpat ) goto save_proj_error;
			int pptr = 0;
			for( int yy = 0; yy < pat->lines; yy++ )
			{
		    	    int pat_ptr = yy * pat->data_xsize;
		    	    for( int xx = 0; xx < max_tracks; xx++ )
		    	    {
		    	        tpat[ pptr ] = pat->data[ pat_ptr ];
				pptr++;
				pat_ptr++;
			    }
			}
			if( st->mod_remap )
			{
			    for( int i = 0; i < pptr; i++ )
			    {
				sunvox_note* n = &tpat[ i ];
				int m = n->mod;
				if( m )
				{
				    m--;
				    if( m < mods_num )
				    {
					int new_m = st->mod_remap[ m ];
					if( new_m >= 0 )
					{
					    n->mod = new_m + 1;
					}
				    }
				}
			    }
			}
			if( save_block( BID_PDTA, pptr * sizeof( sunvox_note ), tpat, st ) ) goto save_proj_error;
			smem_free( tpat );
		    }
		}
		if( pat->name )
		{
		    if( save_block( BID_PNME, smem_strlen( pat->name ) + 1, pat->name, st ) ) goto save_proj_error;
		}
		if( save_block( BID_PCHN, 4, &max_tracks, st ) ) goto save_proj_error;
		if( save_block( BID_PLIN, 4, &pat->lines, st ) ) goto save_proj_error;
		if( save_block( BID_PYSZ, 4, &pat->ysize, st ) ) goto save_proj_error;
		if( save_block( BID_PFLG, 4, &pat->flags, st ) ) goto save_proj_error;
		if( save_block( BID_PICO, 32, pat->icon, st ) ) goto save_proj_error;
		if( save_block( BID_PFGC, 3, pat->fg, st ) ) goto save_proj_error;
		if( save_block( BID_PBGC, 3, pat->bg, st ) ) goto save_proj_error;
	    }
	    else
	    {
	        if( st->pat_remap )
	        {
		    if( save_block( BID_PPAR, 4, &st->pat_remap[ pat_info->parent_num ], st ) ) goto save_proj_error;
		}
		else
		{
	    	    if( save_block( BID_PPAR, 4, &pat_info->parent_num, st ) ) goto save_proj_error;
	    	}
		if( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 )
		{
		    sunvox_pattern* parent_pat = sunvox_get_pattern( pat_info->parent_num, s );
	    	    sunvox_pattern_info* parent_info = sunvox_get_pattern_info( pat_info->parent_num, s );
	    	    if( parent_pat && parent_info )
	    	    {
	    		if( !( parent_info->flags & SUNVOX_PATTERN_INFO_FLAG_SELECTED2 ) )
	    		{
			    if( save_block( BID_PPAR2, 4, &parent_pat->id, st ) ) goto save_proj_error;
			}
	    	    }
	    	}
	    }
	    uint pat_info_flags = pat_info->flags & SUNVOX_PATTERN_INFO_SAVE_FLAGS_MASK;
	    if( save_block( BID_PFFF, 4, &pat_info_flags, st ) ) goto save_proj_error;
	    if( save_block( BID_PXXX, 4, &pat_info->x, st ) ) goto save_proj_error;
	    if( save_block( BID_PYYY, 4, &pat_info->y, st ) ) goto save_proj_error;
	}
	if( st->pat_remap == NULL || ( st->pat_remap && save_pat ) )
	{
	    if( save_block( BID_PEND, 0, 0, st ) ) goto save_proj_error;
	}
    }
    for( int i = 0; i < mods_num; i++ )
    {
	psynth_module* mod = &net->mods[ i ];
	bool save_module = false;
	if( mod->flags & PSYNTH_FLAG_EXISTS )
	{
	    save_module = true;
	    if( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 )
	    {
		save_module = false;
		if( mod->flags2 & PSYNTH_FLAG2_SELECTED2 )
		    save_module = true;
	    }
	}
	if( save_module )
	{
	    uint save_mod_flags = 
		SUNVOX_MODULE_SAVE_ADD_INFO |
		SUNVOX_MODULE_SAVE_WITHOUT_FILE_MARKERS | 
		SUNVOX_MODULE_SAVE_WITHOUT_OUT_LINKS;
	    if( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 ) save_mod_flags |= SUNVOX_MODULE_SAVE_LINKS_SELECTED2;
	    if( sunvox_save_module_to_fd( i, f, save_mod_flags, s, st ) )
		goto save_proj_error;
	}
	if( st->mod_remap == NULL || ( st->mod_remap && save_module ) )
	{
    	    if( save_block( BID_SEND, 0, 0, st ) ) goto save_proj_error;
    	}
    }
    retval = 0;
save_proj_error:
    sunvox_remove_save_state( st );
    if( save_flags & SUNVOX_PROJ_SAVE_SELECTED2 )
    {
	for( int i = 0; i < s->pats_num; i++ )
        {
            sunvox_pattern_info* pat_info = &s->pats_info[ i ];
            pat_info->flags &= ~SUNVOX_PATTERN_INFO_FLAG_SELECTED2;
        }
        for( int i = 0; i < (int)net->mods_num; i++ )
        {
            psynth_module* m = &s->net->mods[ i ];
            m->flags2 &= ~PSYNTH_FLAG2_SELECTED2;
        }
    }
    if( retval )
    {
	slog( "sunvox_save_proj_to_fd() error %d\n", retval );
    }
    return retval;
}
int sunvox_save_proj( const char* name, uint32_t save_flags, sunvox_engine* s )
{
    int rv = 0;
    if( name == 0 ) return -1;
    sfs_file f = sfs_open( name, "wb" );
    if( f )
    {
	rv = sunvox_save_proj_to_fd( f, save_flags, s );
	sfs_close( f );
    }
    else
    {
	slog( "Can't open file %s for writing\n", name );
	return -1;
    }
    return rv;
}
