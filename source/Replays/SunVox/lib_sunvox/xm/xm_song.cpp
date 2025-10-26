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
#include "xm.h"
#ifdef SHOW_DEBUG_MESSAGES
    #define DPRINT( fmt, ARGS... ) slog( fmt, ## ARGS )
#else
    #define DPRINT( fmt, ... ) {} // rePlayer
#endif
void xm_remove_song( xm_song* song )
{
    if( !song ) return;
    for( int i = 0; i < MAX_XM_PATTERNS; i++ ) xm_remove_pattern( i, song );
    for( int i = 0; i < MAX_XM_INSTRUMENTS; i++ ) xm_remove_instrument( i, song );
    smem_free( song );
}
xm_song* xm_new_song()
{
    xm_song* song = SMEM_ZALLOC2( xm_song, 1 );
    if( !song ) return 0;
    char* temp_s = (char*)"Extended Module: ";
    for( int tp = 0; tp < 17; tp++ ) song->id_text[ tp ] = temp_s[ tp ];
    song->reserved1 = 0x1A;
    temp_s = (char*)"SunVox              ";
    for( int tp = 0; tp < 20; tp++ ) song->tracker_name[ tp ] = temp_s[ tp ];
    for( int tp = 0; tp < 20; tp++ ) song->name[ tp ] = 0;
    song->version = 0x0104;
    song->header_size = 0x114;
    song->length = 1;
    song->patterns_num = 1;
    song->patterntable[ 0 ] = 0;
    song->instruments_num = 0;
    song->restart_position = 0;
    song->tempo = 6;
    song->bpm = 125;
    return song;
}
uint32_t xm_read_int32( sfs_file f )
{
    uint32_t res = 0, c;
    c = sfs_getc( f ); res = c;
    c = sfs_getc( f ); c <<= 8; res += c;
    c = sfs_getc( f ); c <<= 16; res += c;
    c = sfs_getc( f ); c <<= 24; res += c;
    return res;
}
uint16_t xm_read_int16( sfs_file f )
{
    uint16_t res = 0, c;
    c = sfs_getc( f ); res = c;
    c = sfs_getc( f ); c <<= 8; res += c;
    return res;
}
uint16_t xm_read_int16_big_endian( sfs_file f )
{
    uint16_t res = 0, c;
    c = sfs_getc( f ); res = c << 8;
    c = sfs_getc( f ); res += c;
    return res;
}
int mod_load( sfs_file f, xm_song* song )
{
    if( !song ) return -1;
    song->type = SONG_TYPE_MOD;
    int rv = 0;
    xm_instrument* ins;
    xm_sample* smp;
    DPRINT( "Loading MOD...\n" );
    int oldMOD = 1;
    sfs_seek( f, 1080, 1 );
    char tt[ 4 ];
    sfs_read( tt, 1, 4, f );
    if( tt[ 0 ] == 'M' && tt[ 2 ] == 'K' ) oldMOD = 0;
    if( tt[ 0 ] == 'F' && tt[ 1 ] == 'L' && tt[ 2 ] == 'T' ) oldMOD = 0;
    if( tt[ 1 ] == 'C' && tt[ 2 ] == 'H' && tt[ 3 ] == 'N' ) oldMOD = 0;
    if( tt[ 0 ] == 'C' && tt[ 1 ] == 'D' && tt[ 2 ] == '8' && tt[ 3 ] == '1' ) oldMOD = 0;
    if( tt[ 0 ] == 'O' && tt[ 1 ] == 'K' && tt[ 2 ] == 'T' && tt[ 3 ] == 'A' ) oldMOD = 0;
    if( tt[ 2 ] == 'C' && tt[ 3 ] == 'N' ) oldMOD = 0;
    if( oldMOD ) DPRINT( "MOD: It's old MOD with 15 samples\n" );
    song->instruments_num = 31;
    if( oldMOD ) song->instruments_num = 15;
    sfs_rewind( f );
    char* name = (char*)song->name;
    sfs_read( name, 1, 20, f );
    DPRINT( "MOD: Loading instruments...\n" );
    char sample_name[ 22 ];
    int sample_size = 0;
    char sample_type = 0;
    char sample_finetune = 0;
    uint8_t sample_volume = 0;
    int sample_repoff = 0;
    int sample_replen = 0;
    for( int i = 0; i < song->instruments_num; i++ )
    {
	DPRINT( "MOD: instrument %d\n", i );
	sfs_read( sample_name, 1, 22, f );
	sample_size = xm_read_int16_big_endian( f ); sample_size *= 2;
	sample_finetune = sfs_getc( f );
	sample_volume = sfs_getc( f );
	sample_repoff = xm_read_int16_big_endian( f ); sample_repoff *= 2;
	sample_replen = xm_read_int16_big_endian( f ); sample_replen *= 2;
	if( sample_replen <= 2 ) sample_replen = 0;
	if( sample_replen ) sample_type = 1; 
	    else sample_type = 0;
	xm_new_instrument( i, sample_name, 1, song );
	ins = song->instruments[ i ];
	ins->finetune = 0;
	ins->panning = 80;
	ins->volume = 0x40;
	ins->relative_note = 0;
	if( sample_size )
	{
	    xm_new_sample( 0, i, "", sample_size, sample_type, song );
	    smp = ins->samples[ 0 ];
	    smp = ins->samples[ 0 ];
	    smp->reppnt = sample_repoff;
	    smp->replen = sample_replen;
	    smp->volume = sample_volume;
	    uint8_t ftune = sample_finetune; ftune <<= 4;
	    smp->finetune = (int8_t)ftune;
	}
	else
	{
	    ins->samples_num = 0;
	}
    }
    uint16_t song_len;
    song_len = sfs_getc( f );
     sfs_getc( f );
    sfs_read( song->patterntable, 1, 128, f );
    song->length = song_len;
    char modtype[ 5 ]; modtype[ 4 ] = 0;
    uint16_t channels = 4;
    if( !oldMOD )
    {
	sfs_read( modtype, 1, 4, f );
	if( modtype[ 0 ] == '6' ) channels = 6;
	if( modtype[ 0 ] == '8' || modtype[ 0 ] == 'O' ) channels = 8;
	if( modtype[ 0 ] == 'C' ) channels = 12; 
	if( modtype[ 0 ] == 'X' ) channels = 16; 
    }
    song->channels = channels;
    DPRINT( "MOD: Loading patterns...\n" );
    int maxp = 0;
    for( int p = 0; p < 128; p ++ )
    {
	if( song->patterntable[ p ] > maxp ) maxp = song->patterntable[ p ];
    }
    song->patterns_num = maxp + 1;
    for( int p = 0; p <= maxp; p++ )
    {
	xm_new_pattern( p, 64, channels, song );
	xm_pattern* pat = song->patterns[ p ];
	xm_note* pat_data = pat->pattern_data;
	int pat_ptr = 0;
	for( int l = 0; l < 64; l++ )
	{
	    for( int c = 0; c < channels; c++ )
	    {
		uint8_t sampperiod = sfs_getc( f );
		uint8_t period1 = sfs_getc( f );
		uint8_t sampeffect = sfs_getc( f );
		uint8_t effect1 = sfs_getc( f );
		uint16_t sample, period, effect;
		sample = ( sampperiod & 0xF0 ) | ( sampeffect >> 4 );
		period = ( (sampperiod & 0xF) << 8 ) | period1;
		effect = ( (sampeffect & 0xF) << 8 ) | effect1;
		int nn = 0;
		if( period )
		{
		    float freq = 8363.0F * 1712.0F / (float)period;
		    freq /= 128;
		    float pitch = LOG2( freq / 16.333984375 ) * 12;
		    nn = 1 + (int)( pitch + 0.5 );
		    if( nn < 1 ) nn = 1;
		    if( nn > 96 ) nn = 96;
		}
		pat_data[ pat_ptr ].n = nn;
		uint8_t fx = effect >> 8;
		uint8_t par = effect & 0xFF;
		if( ( fx == 1 || fx == 2 || fx == 0xA ) && par == 0 ) fx = 0;
		pat_data[ pat_ptr ].inst = sample;
		pat_data[ pat_ptr ].vol = 0;
		pat_data[ pat_ptr ].fx = fx;
		pat_data[ pat_ptr ].par = par;
		pat_ptr++;
	    }
	}
    }
    DPRINT( "MOD: Loading sample data...\n" );
    for( int i = 0; i < song->instruments_num; i++ )
    {
	ins = song->instruments[ i ];
	smp = ins->samples[ 0 ];
	if( smp )
	{
	    DPRINT( "MOD: sample %d\n", i );
	    int8_t* smp_data = (int8_t*)smp->data;
	    if( smp->length )
	    {
		int len = sfs_read( smp_data, 1, smp->length, f );
		if( smp->length != (unsigned)len )
		{
		    DPRINT( "MOD: sample loading error\n" );
		    DPRINT( "MOD: sample length: %d; loaded: %d\n", smp->length, len );
		    smem_clear( smp_data + len, smp->length - len );
		}
	    }
	}
    }
    DPRINT( "MOD loaded.\n" );
    return rv;
}
int xm_load( sfs_file f, xm_song* song )
{
    if( !song ) return -1;
    song->type = SONG_TYPE_XM;
    int rv = 0;
    xm_pattern* pat; 
    xm_note* data; 
    xm_instrument* ins; 
    xm_sample* smp; 
    int16_t* s_data; 
    int8_t* cs_data; 
    uint32_t a1,a2,a3,a4,a5,a6,a7,a8; 
    uint32_t sp; 
    char name[ 32 ];
    DPRINT( "Loading XM...\n" );
    sfs_read( song, 1, 64, f ); 
    DPRINT( "XM: header size = %d (std.276) \n", song->header_size );
    uint32_t h1 = song->header_size - 4;
    uint32_t h2 = 0;
    if( h1 > 272 )
    {
	h2 = h1 - 272;
	h1 = 272;
    }
    sfs_read( &song->length, 1, h1, f ); 
    if( h2 )
    {
	sfs_seek( f, h2, SFS_SEEK_CUR );
    }
    DPRINT( "XM: length = %d\n", song->length );
    DPRINT( "XM: patterns = %d\n", song->patterns_num );
    DPRINT( "XM: channels = %d\n", song->channels );
    DPRINT( "XM: tempo = %d\n", song->tempo );
    for( uint32_t pnum = 0; pnum < song->patterns_num; pnum++ )
    {
	uint32_t hlen = xm_read_int32( f );	
	uint32_t ptype = sfs_getc( f );    	
	uint32_t prows = xm_read_int16( f );  	
	uint32_t psize = xm_read_int16( f );  	
	DPRINT( "XM: pat %d: hlen %d; ptype %d; rows %d; psize %d;\n", pnum, hlen, ptype, prows, psize );
	xm_new_pattern( pnum, prows, song->channels, song );
	if( psize == 0 ) continue; 		
	pat = song->patterns[ pnum ];
	data = pat->pattern_data;
	a4 = song->channels * prows; 
	for( uint32_t b = 0; b < a4; b++ )
	{ 
	    a3 = sfs_getc( f );
	    xm_note* np = &data[ b ];
	    if( a3 & 0x80 )
	    {
		memset( np, 0, sizeof( xm_note ) );
		if( a3 & 1 ) np->n = sfs_getc( f );
		if( a3 & 2 ) np->inst = sfs_getc( f );
		if( a3 & 4 ) np->vol = sfs_getc( f );
		if( a3 & 8 ) np->fx = sfs_getc( f );
		if( a3 & 16 ) np->par = sfs_getc( f );
	    }
	    else
	    {
		np->n = a3;
		np->inst = sfs_getc( f );
		np->vol = sfs_getc( f );
		np->fx = sfs_getc( f );
		np->par = sfs_getc( f );
	    }
	}
    }
    for( uint32_t inum = 0; inum < song->instruments_num; inum++ )
    {
	DPRINT( "XM: LOADING INSTRUMENT %d...\n", inum );
	int ins_header_size = xm_read_int32( f ); 
	if( sfs_eof( f ) )
	{
	    DPRINT( "XM: EOF\n" );
	    break;
	}
	DPRINT( "XM: instrument header size = %d (std.243)\n", ins_header_size );
	if( ins_header_size < 0 || ins_header_size > 1024 * 1024 )
	{
	    DPRINT( "XM: wrong instr. header size.\n" );
	    break;
	}
	sfs_read( name, 1, 22, f ); 
	sfs_getc( f ); 
	int num_of_samples = xm_read_int16( f ); 
	DPRINT( "XM: number of samples = %d\n", num_of_samples );
	xm_new_instrument( inum, name, (uint16_t)num_of_samples, song );
	ins = song->instruments[ inum ];
	if( ins_header_size <= 29 ) continue; 
	if( num_of_samples == 0 ) 
	{
	    sfs_seek( f, ins_header_size - 29, 1 ); 
	    continue; 
	}
	int smp_header_size = 0;
	if( ins_header_size >= ( 29 + 214 ) )
	{
	    smp_header_size = xm_read_int32( f );	
	    if( sfs_eof( f ) )
	    {
		DPRINT( "XM: EOF\n" );
		break;
	    }
	    DPRINT( "XM: sample header size = %d (std.40)\n", smp_header_size );
	    if( smp_header_size < 0 || smp_header_size > 1024 * 1024 )
	    {
	        DPRINT( "XM: wrong sample header size.\n" );
	        break;
	    }
	    sfs_read( ins->sample_number, 1, 96, f ); 	
	    sfs_read( ins->volume_points, 1, 48, f ); 	
	    sfs_read( ins->panning_points, 1, 48, f );	
	    ins->volume_points_num = sfs_getc( f ); 	
	    ins->panning_points_num = sfs_getc( f );	
	    ins->vol_sustain = sfs_getc( f );       	
	    ins->vol_loop_start = sfs_getc( f );    	
	    ins->vol_loop_end = sfs_getc( f );      	
	    ins->pan_sustain = sfs_getc( f );       	
	    ins->pan_loop_start = sfs_getc( f );    	
	    ins->pan_loop_end = sfs_getc( f );      	
	    ins->volume_type = sfs_getc( f );       	
    	    ins->panning_type = sfs_getc( f );      	
	    ins->vibrato_type = sfs_getc( f );      	
	    ins->vibrato_sweep = sfs_getc( f );     	
	    ins->vibrato_depth = sfs_getc( f );     	
	    ins->vibrato_rate = sfs_getc( f );      	
	    a1 = xm_read_int16( f );                 	
	    ins->volume = 0x40;
	    ins->finetune = 0;
	    ins->panning = 0x80;
	    ins->relative_note = 0;
	    ins->flags = 0;
	    int ext_bytes = 0;
	    if( ins_header_size >= 29 + 214 + 2 && ins_header_size <= 29 + 214 + EXT_INST_BYTES ) 
	    {
		ins->volume = sfs_getc( f ); 
		ins->finetune = (int8_t)sfs_getc( f );
		sfs_read( &ins->panning, 1, ins_header_size - ( 29 + 214 ), f );
		ext_bytes = ins_header_size - ( 29 + 214 );
	    }
	    else
	    {
		xm_read_int16( f ); 
	    }
	    ins->volume_fadeout = (uint16_t)a1;
	    int already_loaded = ( 29 + 214 + ext_bytes );
	    int seek_bytes = ins_header_size - already_loaded;
	    if( seek_bytes > 0 )
	    {
		sfs_seek( f, ins_header_size - already_loaded, 1 );
	    }
	}
	else
	{
	    sfs_seek( f, ins_header_size - 29, 1 );
	}
	if( smp_header_size <= 0 ) continue;
	for( int snum = 0; snum < num_of_samples; snum++ )
	{
	    name[ 0 ] = 0;
	    a1 = xm_read_int32( f );	
	    a2 = xm_read_int32( f ); 	
	    a3 = xm_read_int32( f ); 	
	    a4 = sfs_getc( f );      	
	    a5 = (int8_t)sfs_getc( f );	
	    a6 = sfs_getc( f );      	
	    a7 = sfs_getc( f );      	
	    a8 = (int8_t)sfs_getc( f );	
	    if( smp_header_size >= 18 ) sfs_getc( f ); 
	    if( smp_header_size >= 40 ) sfs_read( name, 1, 22, f ); 
	    xm_new_sample( snum, inum, name, a1, a6, song );
	    smp = ins->samples[ snum ];
	    smp->reppnt = a2;
	    smp->replen = a3;
	    smp->volume = (uint8_t) a4;
	    smp->finetune = (int8_t) a5;
	    smp->panning = (uint8_t) a7;
	    smp->relative_note = (int8_t) a8;
	    if( smp_header_size > 40 ) sfs_seek( f, smp_header_size - 40, 1 );
	}
	for( int snum = 0; snum < num_of_samples; snum++ )
	{
	    smp = ins->samples[ snum ];
	    if( smp->length == 0 ) continue;
	    DPRINT( "XM: LOADING SAMPLE %d: len=%d (0x%x) bytes; type=%x...\n", snum, smp->length, smp->length, smp->type );
	    if( sfs_read( smp->data, 1, smp->length, f ) != smp->length )
	    {
	        rv = -1;
	        break;
	    }
	    if( ( smp->type >> 4 ) & 1 )
	    {
		DPRINT( "XM: 16bit sample\n" );
	        int16_t old_s, new_s;
		old_s = 0;
		s_data = (int16_t*) smp->data;
		for( sp = 0; sp < smp->length / 2; sp++ ) 
		{
		    new_s = s_data[ sp ] + old_s;
		    s_data[ sp ] = new_s;
		    old_s = new_s;
		}
		xm_bytes2frames( smp, song );
	    }
	    else
	    {
		DPRINT( "XM: 8bit sample\n" );
	        int8_t c_old_s, c_new_s;
		c_old_s = 0;
		cs_data = (int8_t*) smp->data;
		for( sp = 0; sp < smp->length; sp++ )
		{
		    c_new_s = cs_data[ sp ] + c_old_s;
		    cs_data[ sp ] = c_new_s;
		    c_old_s = c_new_s;
		}
	    }
	}
    }
    DPRINT( "XM loaded.\n" );
    return rv;
}
static bool check_signature( sfs_file f )
{
    bool rv = false;
    char sign[ 16 ];
    SMEM_CLEAR_STRUCT( sign );
    sfs_rewind( f );
    sfs_read( sign, 1, 15, f );
    if( smem_strcmp( sign, "Extended Module" ) == 0 )
	rv = true;
    sfs_rewind( f );
    return rv;
}
xm_song* xm_load_song_from_fd( sfs_file f )
{
    xm_song* song = xm_new_song();
    if( !song ) return NULL;
    if( check_signature( f ) )
    {
        if( xm_load( f, song ) != 0 )
        {
    	    xm_remove_song( song );
    	    song = NULL;
        }
    }
    else
    {
        if( mod_load( f, song ) != 0 )
        {
    	    xm_remove_song( song );
    	    song = NULL;
        }
    }
    return song;
}
xm_song* xm_load_song( const char* filename )
{
    xm_song* song = NULL;
    sfs_file f = sfs_open( filename, "rb" );
    if( f )
    {
	song = xm_load_song_from_fd( f );
	sfs_close( f );
    }
    return song;
}
