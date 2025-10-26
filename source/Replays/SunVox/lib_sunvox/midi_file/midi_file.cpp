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
#include "midi_file.h"
void midi_file_remove( midi_file* mf )
{
    if( mf == 0 ) return;
    if( mf->tracks )
    {
	for( int i = 0; i < mf->tracks_num; i++ )
	{
	    midi_track_remove( mf->tracks[ i ] );
	    mf->tracks[ i ] = 0;
	}
	smem_free( mf->tracks );
    }
    smem_free( mf );
}
midi_file* midi_file_new()
{
    midi_file* mf = SMEM_ZALLOC2( midi_file, 1 );
    if( !mf ) return 0;
    return mf;
}
void midi_track_remove( midi_track* mt )
{
    if( mt == 0 ) return;
    smem_free( mt->data );
    smem_free( mt );
}
midi_track* midi_track_new()
{
    midi_track* mt = SMEM_ZALLOC2( midi_track, 1 );
    if( !mt ) return 0;
    return mt;
}
static bool check_signature( sfs_file f )
{
    bool rv = false;
    char sign[ 5 ];
    SMEM_CLEAR_STRUCT( sign );
    sfs_rewind( f );
    sfs_read( sign, 1, 4, f );
    if( smem_strcmp( sign, "MThd" ) == 0 ) 
    {
	rv = true;
    }
    sfs_rewind( f );
    return rv;
}
midi_file* midi_file_load_from_fd( sfs_file f )
{
    midi_file* mf = NULL;
    if( !check_signature( f ) ) return NULL;
    while( 1 )
    {
        mf = midi_file_new();
        if( !mf ) break;
	char chunk_id[ 5 ];
	chunk_id[ 4 ] = 0;
	uint chunk_size;
	int tnum = 0;
	while( 1 )
	{
	    if( sfs_read( chunk_id, 1, 4, f ) != 4 ) break;
	    if( sfs_read( &chunk_size, 1, 4, f ) != 4 ) break;
	    chunk_size = INT32_SWAP( chunk_size );
	    if( smem_strcmp( chunk_id, "MThd" ) == 0 )
	    {
	        if( sfs_read( &mf->format, 1, 2, f ) != 2 ) break;
	        if( sfs_read( &mf->tracks_num, 1, 2, f ) != 2 ) break;
	        if( sfs_read( &mf->stime_div, 1, 2, f ) != 2 ) break;
	        mf->format = INT16_SWAP( mf->format );
	        mf->tracks_num = INT16_SWAP( mf->tracks_num );
	        mf->stime_div = INT16_SWAP( mf->stime_div );
	        slog( "MIDI Format: %d\n", mf->format );
	        slog( "MIDI Number of tracks: %d\n", mf->tracks_num );
	        slog( "MIDI Time division: %x\n", mf->stime_div );
	        if( mf->stime_div & 0x8000 )
	        {
	    	    int8_t fps = ( ( (uint16_t)mf->stime_div >> 8 ) & 255 );
		    fps = -fps;
		    mf->fps = fps;
		    if( fps == 29 ) mf->fps = 29.97;
		    mf->ticks_per_frame = mf->stime_div & 0xFF;
		    slog( "MIDI FPS: %f; Ticks per frame: %d\n", mf->fps, mf->ticks_per_frame );
		}
		else
		{
		    mf->ticks_per_quarter_note = mf->stime_div;
		    slog( "MIDI Ticks per quarter note: %d\n", mf->ticks_per_quarter_note );
		}
		mf->tracks = SMEM_ALLOC2( midi_track*, mf->tracks_num );
		if( !mf->tracks ) break;
		for( int i = 0; i < mf->tracks_num; i++ )
		{
		    mf->tracks[ i ] = midi_track_new();
		}
		continue;
	    }
	    if( smem_strcmp( chunk_id, "MTrk" ) == 0 )
	    {
	        slog( "MIDI Track %d; %d bytes\n", tnum, chunk_size );
	        mf->tracks[ tnum ]->data = SMEM_ALLOC2( uint8_t, chunk_size );
	        if( !mf->tracks[ tnum ]->data ) break;
	        sfs_read( mf->tracks[ tnum ]->data, 1, chunk_size, f );
	        tnum++;
	        continue;
	    }
	    sfs_seek( f, chunk_size, 1 );
	}
        break;
    }
    return mf;
}
midi_file* midi_file_load( const char* filename )
{
    midi_file* mf = 0;
    sfs_file f = sfs_open( filename, "rb" );
    if( f )
    {
        mf = midi_file_load_from_fd( f );
        sfs_close( f );
    }
    return mf;
}
