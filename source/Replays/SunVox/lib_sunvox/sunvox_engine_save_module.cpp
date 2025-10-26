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
static int* check_links( int* links, int links_num, psynth_module* mod, uint save_flags, sunvox_save_state* st )
{
    int* new_links = (int*)SMEM_CLONE( links );
    if( !new_links ) return NULL;
    for( int l = 0; l < links_num; l++ )
    {
        int ll = new_links[ l ];
        psynth_module* m = psynth_get_module( ll, mod->pnet );
        if( m )
        {
            if( save_flags & SUNVOX_MODULE_SAVE_LINKS_SELECTED2 )
            {
                if( !( m->flags2 & PSYNTH_FLAG2_SELECTED2 ) )
                    ll = -1;
            }
        }
        else
        {
            ll = -1;
        }
        if( st->mod_remap )
        {
    	    if( ll >= 0 )
    		ll = st->mod_remap[ ll ];
        }
        new_links[ l ] = ll;
    }
    return new_links;
}
static int* get_links2( psynth_net* pnet, int mod_num, int* links, int links_num, bool input )
{
    if( !links || links_num == 0 ) return NULL;
    int* rv = SMEM_ALLOC2( int, links_num );
    if( !rv ) return NULL;
    bool empty = true; 
    for( int i = 0; i < links_num; i++ )
    {
	rv[ i ] = -1;
	psynth_module* mod = psynth_get_module( links[ i ], pnet );
	if( !mod ) continue;
	if( input )
	{
	    if( mod->output_links )
	    {
		for( int i2 = 0; i2 < mod->output_links_num; i2++ )
		{
		    if( mod->output_links[ i2 ] == mod_num )
		    {
			rv[ i ] = i2;
			break;
		    }
		}
	    }
	}
	else
	{
	    if( mod->input_links )
	    {
		for( int i2 = 0; i2 < mod->input_links_num; i2++ )
		{
		    if( mod->input_links[ i2 ] == mod_num )
		    {
			rv[ i ] = i2;
			break;
		    }
		}
	    }
	}
	if( rv[ i ] > 0 ) 
	{
	    empty = false;
	}
    }
    if( empty )
    {
	smem_free( rv );
	rv = NULL;
    }
    return rv;
}
int sunvox_save_module_to_fd( int mod_num, sfs_file f, uint save_flags, sunvox_engine* s, sunvox_save_state* st )
{
    int retval = 1;
    psynth_net* net = s->net;
    bool new_st = false;
    while( f && (unsigned)mod_num < (unsigned)net->mods_num )
    {
	if( st == NULL )
	{
	    st = sunvox_new_save_state( s, f );
	    new_st = true;
	}
        psynth_module* mod = &net->mods[ mod_num ];
	if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 )
	{
	    slog( "No module %d\n", mod_num );
    	    break;
    	}
        bool add_info = false;
	if( save_flags & SUNVOX_MODULE_SAVE_ADD_INFO )
    	    add_info = true;
        psynth_do_command( mod_num, PS_CMD_BEFORE_SAVE, net );
	if( ( save_flags & SUNVOX_MODULE_SAVE_WITHOUT_FILE_MARKERS ) == 0 )
	{
    	    int version = SUNVOX_ENGINE_VERSION;
    	    sfs_write( g_sunvox_module_sign, 1, 8, f );
    	    if( save_block( BID_VERS, 4, &version, st ) ) break;
	}
        uint module_flags = mod->flags;
	if( save_block( BID_SFFF, 4, &module_flags, st ) ) break;
        if( save_block( BID_SNAM, 32, &mod->name, st ) ) break;
	psynth_event module_evt;
        module_evt.command = PS_CMD_GET_NAME;
	char* mod_type = (char*)mod->handler( mod_num, &module_evt, net );
        if( mod_type )
	{
	    if( save_block( BID_STYP, smem_strlen( mod_type ) + 1, mod_type, st ) ) break;
        }
	if( save_block( BID_SFIN, 4, &mod->finetune, st ) ) break;
        if( save_block( BID_SREL, 4, &mod->relative_note, st ) ) break;
        if( add_info )
	{
    	    if( save_block( BID_SXXX, 4, &mod->x, st ) ) break;
    	    if( save_block( BID_SYYY, 4, &mod->y, st ) ) break;
	    int zz = mod->z;
            if( save_block( BID_SZZZ, 4, &zz, st ) ) break;
	}
        int scale = mod->scale;
	if( save_block( BID_SSCL, 4, &scale, st ) ) break;
        if( ( save_flags & SUNVOX_MODULE_SAVE_WITHOUT_VISUALIZER ) == 0 )
        {
	    if( save_block( BID_SVPR, 4, &mod->visualizer_pars, st ) ) break;
        }
        if( save_block( BID_SCOL, 3, &mod->color, st ) ) break;
	if( save_block( BID_SMII, 4, &mod->midi_in_flags, st ) ) break;
	if( mod->midi_out_name )
	{
    	    if( save_block( BID_SMIN, smem_strlen( mod->midi_out_name ) + 1, mod->midi_out_name, st ) ) break;
	}
        if( save_block( BID_SMIC, 4, &mod->midi_out_ch, st ) ) break;
	if( save_block( BID_SMIB, 4, &mod->midi_out_bank, st ) ) break;
        if( save_block( BID_SMIP, 4, &mod->midi_out_prog, st ) ) break;
	if( add_info )
        {
	    if( mod->input_links_num && ( ( save_flags & SUNVOX_MODULE_SAVE_LINKS_SELECTED2 ) || st->mod_remap ) )
    	    {
    		int* new_links = check_links( mod->input_links, mod->input_links_num, mod, save_flags, st );
        	if( !new_links ) break;
        	if( save_block( BID_SLNK, mod->input_links_num * 4, new_links, st ) ) break;
                smem_free( new_links );
    	    }
	    else
	    {
		if( save_block( BID_SLNK, mod->input_links_num * 4, mod->input_links, st ) ) break;
	    }
	    int* links2 = get_links2( net, mod_num, mod->input_links, mod->input_links_num, true );
	    if( links2 )
	    {
		if( save_block( BID_SLnK, mod->input_links_num * 4, links2, st ) ) break;
		smem_free( links2 );
    	    }
    	    if( ( save_flags & SUNVOX_MODULE_SAVE_WITHOUT_OUT_LINKS ) == 0 )
	    {
		if( mod->output_links_num && ( ( save_flags & SUNVOX_MODULE_SAVE_LINKS_SELECTED2 ) || st->mod_remap ) )
		{
	    	    int* new_links = check_links( mod->output_links, mod->output_links_num, mod, save_flags, st );
        	    if( !new_links ) break;
            	    if( save_block( BID_SLNk, mod->output_links_num * 4, new_links, st ) ) break;
            	    smem_free( new_links );
		}
		else
		{
	    	    if( save_block( BID_SLNk, mod->output_links_num * 4, mod->output_links, st ) ) break;
		}
		int* links2 = get_links2( net, mod_num, mod->output_links, mod->output_links_num, false );
		if( links2 )
		{
	    	    if( save_block( BID_SLnk, mod->output_links_num * 4, links2, st ) ) break;
	    	    smem_free( links2 );
		}
	    }
	}
	if( mod->ctls_num )
	{
    	    if( sizeof( PS_CTYPE ) != 4 )
    	    {
    		slog( "Wrong PS_CTYPE size!\n" );
		break;
	    }
	    for( uint c = 0; c < mod->ctls_num; c++ )
	    {
		psynth_ctl* ctl = &mod->ctls[ c ];
		if( ctl->val == NULL )
		{
	            slog( "NULL ctl %d detected in module %d: %s\n", c, mod_num, mod->name );
		    break;
		}
		if( save_block( BID_CVAL, 4, ctl->val, st ) ) break;
	    }
	    uint* pars = SMEM_ALLOC2( uint, mod->ctls_num * 2 );
	    for( uint c = 0; c < mod->ctls_num; c++ )
	    {
		psynth_ctl* ctl = &mod->ctls[ c ];
		pars[ c * 2 + 0 ] = ctl->midi_pars1;
		pars[ c * 2 + 1 ] = ctl->midi_pars2;
	    }
	    if( save_block( BID_CMID, smem_get_size( pars ), pars, st ) ) break;
	    smem_free( pars );
	}
	if( mod->chunks )
	{
	    uint chunk_count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
    	    if( save_block( BID_CHNK, 4, &chunk_count, st ) ) break;
    	    for( uint cn = 0; cn < chunk_count; cn++ )
    	    {
        	psynth_chunk* c = mod->chunks[ cn ];
        	if( c && c->data )
        	{
    		    if( !( c->flags & PS_CHUNK_FLAG_DONT_SAVE ) )
            	    {
            	        if( save_block( BID_CHNM, 4, &cn, st ) ) break;
            		if( save_block( BID_CHDT, smem_get_size( c->data ), c->data, st ) ) break;
            		if( c->flags ) { if( save_block( BID_CHFF, 4, &c->flags, st ) ) break; }
            		if( c->freq ) { if( save_block( BID_CHFR, 4, &c->freq, st ) ) break; }
            	    }
        	}
    	    }
	}
	if( ( save_flags & SUNVOX_MODULE_SAVE_WITHOUT_FILE_MARKERS ) == 0 )
	{
    	    if( save_block( BID_SEND, 0, 0, st ) ) break;
	}
	psynth_do_command( mod_num, PS_CMD_AFTER_SAVE, net );
	retval = 0;
	break;
    }
    if( new_st )
	sunvox_remove_save_state( st );
    return retval;
}
int sunvox_save_module( int mod_num, const char* name, uint save_flags, sunvox_engine* s )
{
    int retval = 1;
    psynth_net* net = s->net;
    if( (unsigned)mod_num < (unsigned)net->mods_num && ( net->mods[ mod_num ].flags & PSYNTH_FLAG_EXISTS ) )
    {
	sfs_file f = sfs_open( name, "wb" );
	if( f )
	{
	    retval = sunvox_save_module_to_fd( mod_num, f, save_flags, s, NULL );
	    sfs_close( f );
	}
    }
    return retval;
}
