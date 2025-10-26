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
uint sunvox_get_time_map( sunvox_time_map_item* map, uint32_t* frame_map, int start_line, int len, sunvox_engine* s )
{
    if( len <= 0 ) return 0;
    smem_clear( map, sizeof( sunvox_time_map_item ) * len );
    map[ 0 ].bpm = s->bpm;
    map[ 0 ].tpl = s->speed;
    for( int p = 0; p < s->pats_num; p++ )
    {
	sunvox_pattern* pat = s->pats[ p ];
	if( !pat ) continue;
	if( !pat->data ) continue;
	sunvox_pattern_info* pat_info = &s->pats_info[ p ];
	if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_MUTE ) continue;
        if( s->pats_solo_mode )
        {
            if( !( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_SOLO ) ) continue;
        }
	int pat_x = pat_info->x;
	int pat_lines = pat->lines;
	CROP_REGION( pat_x, pat_lines, start_line, len );
	if( pat_lines <= 0 ) continue;
	int pat_xoffset = pat_x - pat_info->x;
	int pat_xx = pat_x - start_line;
	for( int ln = pat_xx, lp = pat_xoffset * pat->data_xsize; ln < pat_xx + pat_lines; ln++, lp += pat->data_xsize )
	{
	    for( int cn = 0, ptr = lp; cn < pat->channels; cn++, ptr++ )
	    {
	        sunvox_note* n = &pat->data[ ptr ];
	        if( ( n->ctl & 0xFF ) == 0x0F || ( n->ctl & 0xFF ) == 0x1F )
	        {
	            if( ( n->ctl & 0xFF ) == 0x0F && n->ctl_val < 32 )
	    	    {
	    		int tpl = n->ctl_val;
			if( tpl <= 1 ) tpl = 1;
			map[ ln ].tpl = tpl;
		    }
		    else
		    {
		        int bpm = n->ctl_val;
		        if( bpm < 1 ) bpm = 1;
		        if( bpm > 16000 ) bpm = 16000;
			map[ ln ].bpm = bpm;
		    }
		}
	    }
	}
    } 
    uint64_t frame_cnt = 0;
    uint frame_add = 0;
    sunvox_time_map_item v = map[ 0 ];
    for( int i = 0; i < len; i++ )
    {
        sunvox_time_map_item v2 = map[ i ];
        if( v2.bpm || v2.tpl )
        {
    	    if( v2.bpm ) v.bpm = v2.bpm;
	    if( v2.tpl ) v.tpl = v2.tpl;
	    uint one_tick = PSYNTH_TICK_SIZE( s->net->sampling_freq, v.bpm );
	    frame_add = one_tick * v.tpl;
	}
	map[ i ] = v;
	if( frame_map ) frame_map[ i ] = frame_cnt >> 8;
	frame_cnt += frame_add;
    }
    return frame_cnt >> 8;
}
int sunvox_get_proj_lines( sunvox_engine* s )
{
    int number_of_lines = 0;
    for( int p = 0; p < s->pats_num; p++ )
    {
	sunvox_pattern* pat = s->pats[ p ];
	sunvox_pattern_info* pat_info = &s->pats_info[ p ];
	if( pat && pat_info->x + pat->lines > 0 )
	{
	    if( pat_info->x + pat->lines > number_of_lines )
		number_of_lines = pat_info->x + pat->lines;
	}
    }
    return number_of_lines;
}
uint32_t sunvox_get_proj_frames( int start_line, int line_cnt, sunvox_engine* s ) 
{
    if( line_cnt == 0 )
    {
	line_cnt = sunvox_get_proj_lines( s ) - start_line;
    }
    if( line_cnt <= 0 ) return 0;
    uint32_t frame_cnt = 0;
    sunvox_time_map_item* map = SMEM_ALLOC2( sunvox_time_map_item, line_cnt );
    if( map )
    {
	frame_cnt = sunvox_get_time_map( map, NULL, start_line, line_cnt, s );
	smem_free( map );
    }
    return frame_cnt;
}
