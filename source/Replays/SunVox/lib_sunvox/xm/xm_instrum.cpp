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

#include "sundog.h"
#include "xm.h"
void xm_new_instrument( uint16_t num, const char* name, uint16_t samples, xm_song* song )
{
    if( !song ) return;
    xm_instrument* ins;
    ins = (xm_instrument*)smem_znew( sizeof( xm_instrument ) );
    for( int a = 0; a < 22; a++ )
    {
	ins->name[ a ] = name[ a ];
	if( name[ a ] == 0 ) break;
    }
    ins->samples_num = samples;
    for( int a = 0; a < MAX_XM_INS_SAMPLES; a++ )
    {
	ins->samples[ a ] = NULL;
    }
    ins->volume_points_num = 1;
    ins->panning_points_num = 1;
    song->instruments[ num ] = ins;
}
void xm_remove_instrument( uint16_t num, xm_song* song )
{
    if( !song ) return;
    xm_instrument* ins = song->instruments[ num ];
    if( !ins ) return;
    for( int a = 0; a < MAX_XM_INS_SAMPLES; a++ )
    {
        if( ins->samples[ a ] )
        {
    	    xm_remove_sample( a, num, song );
	}
    }
    smem_free( ins );
    song->instruments[ num ] = 0;
}
void xm_save_instrument( uint16_t num, const char* filename, xm_song* song )
{
    if( !song ) return;
    bool save_psytexx_ext = false;
    if( song->instruments[ num ] )
    {
	xm_instrument* ins = song->instruments[ num ];
	sfs_file f = sfs_open( filename, "wb" );
        if( f )
        {
	    sfs_write( (void*)"Extended Instrument: ", 1, 21, f );
	    sfs_write( &ins->name, 1, 22, f ); 
	    int8_t temp[ 30 ];
	    smem_clear_struct( temp );
	    temp[ 21 ] = 0x02;
	    temp[ 22 ] = 0x01;
	    if( save_psytexx_ext )
	    {
		temp[ 21 ] = 0x50;
		temp[ 22 ] = 0x50;
	    }
	    sfs_write( temp, 1, 23, f );
	    sfs_write( ins->sample_number, 1, 208, f ); 
	    if( save_psytexx_ext )
	    {
		sfs_write( &ins->volume, 1, 2 + EXT_INST_BYTES, f );
		sfs_write( temp, 1, 22 - ( 2 + EXT_INST_BYTES ), f ); 
	    }
	    else
	    {
		smem_clear_struct( temp );
		sfs_write( temp, 1, 22, f );
	    }
	    sfs_write( &ins->samples_num, 1, 2, f ); 
	    xm_sample* smp;
	    for( int s = 0; s < ins->samples_num; s++ )
	    {
	        smp = ins->samples[ s ];
		if( smp->type & 16 )
		{
		    xm_frames2bytes( smp, song );
		}
		sfs_write( smp, 1, 40, f ); 
	    }
	    int16_t* s_data; 	
	    int8_t* cs_data;	
	    for( int b = 0; b < ins->samples_num; b++ )
	    {
	        smp = ins->samples[ b ];
	        if( smp->length == 0 ) continue;
	        if( smp->type & 16 )
	        { 
	    	    uint32_t len = smp->length; 
		    int16_t old_s = 0;
		    int16_t old_s2 = 0;
		    int16_t new_s = 0;
		    s_data = (int16_t*) smp->data;
		    for( uint32_t sp = 0; sp < len / 2; sp++ )
		    {
		        old_s2 = s_data[ sp ];
			s_data[ sp ] = old_s2 - old_s;
			s_data[ sp ] = s_data[ sp ];
			old_s = old_s2;
		    }
		    sfs_write( s_data, 1, len, f );
		    old_s = 0;
		    for( uint32_t sp = 0; sp < len / 2; sp++ )
		    {
		        new_s = s_data[ sp ] + old_s;
			s_data[ sp ] = new_s;
			old_s = new_s;
		    }
		    xm_bytes2frames( smp, song );
		}
		else
		{ 
		    uint32_t len = smp->length; 
		    int8_t c_old_s = 0;
		    int8_t c_old_s2 = 0;
		    int8_t c_new_s = 0;
		    cs_data = (int8_t*) smp->data;
		    for( uint32_t sp = 0 ; sp < len; sp++ )
		    {
		        c_old_s2 = cs_data[ sp ];
			cs_data[ sp ] = c_old_s2 - c_old_s;
			c_old_s = c_old_s2;
		    }
		    sfs_write( cs_data, 1, len, f );
		    c_old_s = 0;
		    for( uint32_t sp = 0; sp < len; sp++ )
		    {
		        c_new_s = cs_data[ sp ] + c_old_s;
		        cs_data[ sp ] = c_new_s;
		        c_old_s = c_new_s;
		    }
		}
	    }
	    sfs_close( f );
	}
    }
}
