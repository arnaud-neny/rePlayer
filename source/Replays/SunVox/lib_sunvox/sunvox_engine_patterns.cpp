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
void sunvox_sort_patterns( sunvox_engine* s )
{
    if( s->flags & SUNVOX_FLAG_STATIC_TIMELINE )
    {
	return;
    }
    s->proj_lines = 0;
    if( s->pats && s->pats_num )
    {
	if( !s->sorted_pats )
	{
	    s->sorted_pats = SMEM_ALLOC2( int, s->pats_num );
	}
	else
	{
	    if( s->pats_num > (int)( smem_get_size( s->sorted_pats ) / sizeof( int ) ) )
	    {
		s->sorted_pats = SMEM_RESIZE2( s->sorted_pats, int, s->pats_num + 32 );
	    }
	}
	s->sorted_pats_num = 0;
	for( int i = 0; i < s->pats_num; i++ )
	{
	    if( !s->pats[ i ] ) continue;
    	    s->sorted_pats[ s->sorted_pats_num ] = i;
	    int max_x = s->pats_info[ i ].x + s->pats[ i ]->lines;
	    if( max_x > s->proj_lines ) s->proj_lines = max_x;
	    s->sorted_pats_num++;
	}
        for( int i = 1; i < s->sorted_pats_num; i++ )
        {
    	    int j = i;
    	    int n = s->sorted_pats[ i ];
    	    sunvox_pattern* pat = s->pats[ n ];
	    sunvox_pattern_info* pat_info = &s->pats_info[ n ];
    	    while( j > 0 )
    	    {
    		int n2 = s->sorted_pats[ j - 1 ];
    		sunvox_pattern* pat2 = s->pats[ n2 ];
		sunvox_pattern_info* pat_info2 = &s->pats_info[ n2 ];
		if( !( ( pat_info2->x > pat_info->x ) || ( pat_info2->x == pat_info->x && pat_info2->x + pat2->lines > pat_info->x + pat->lines ) ) ) break;
		s->sorted_pats[ j ] = n2;
		j--;
	    }
	    if( j != i ) s->sorted_pats[ j ] = n;
        }
    }
    else
    {
	s->sorted_pats_num = 0;
    }
    if( s->flags & SUNVOX_FLAG_DYNAMIC_PATTERN_STATE )
    {
	int pp = 0;
        if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
        {
	    for( int i = 0; i < s->pats_num; i++ )
	    {
		if( !s->pats[ i ] ) continue;
		int state_ptr = ( s->pats_info[ i ].y + 16 ) / 32;
		if( state_ptr + 1 > pp ) pp = state_ptr + 1;
	    }
        }
        else
        {
	    pp = sunvox_get_mpp( s );
	}
	if( pp > MAX_PLAYING_PATS ) pp = MAX_PLAYING_PATS;
	if( pp > s->pat_state_size )
	{
	    s->pat_state = SMEM_ZRESIZE2( s->pat_state, sunvox_pattern_state, pp );
	    if( s->pat_state )
	    {
		for( int i = s->pat_state_size; i < pp; i++ )
    		    clean_pattern_state( &s->pat_state[ i ], s );
		s->pat_state_size = pp;
	    }
	}
    }
    if( s->sorted_pats_num )
    {
        if( s->flags & SUNVOX_FLAG_SUPERTRACKS )
        {
    	    for( int p = 0; p < s->sorted_pats_num; p++ )
	    {
    		int n = s->sorted_pats[ p ];
		sunvox_pattern_info* pat_info = &s->pats_info[ n ];
		int state_ptr = ( pat_info->y + 16 ) / 32;
		if( state_ptr < 0 ) state_ptr = 0;
		if( state_ptr >= s->pat_state_size ) state_ptr = s->pat_state_size - 1;
    	        pat_info->state_ptr = state_ptr;
    	        pat_info->track_status = 0;
	    }
        }
        else
        {
	    int pats[ MAX_PLAYING_PATS ]; 
	    int pats_num = 0;
	    pats[ 0 ] = -1;
    	    for( int p = 0; p < s->sorted_pats_num; p++ )
	    {
    		int n = s->sorted_pats[ p ];
		sunvox_pattern_info* pat_info = &s->pats_info[ n ];
		int state_ptr = s->pat_state_size - 1;
		int pats_num2 = pats_num + 1;
	        if( pats_num2 > s->pat_state_size ) pats_num2 = s->pat_state_size;
		for( int i = 0; i < pats_num2; i++ )
    		{
    		    int n2 = pats[ i ];
    		    if( n2 == -1 )
    		    {
    		        state_ptr = i;
    		        break;
    		    }
		    sunvox_pattern* pat2 = s->pats[ n2 ];
    		    sunvox_pattern_info* pat2_info = &s->pats_info[ n2 ];
        	    if( pat2_info->x + pat2->lines <= pat_info->x )
    		    {
    			state_ptr = i;
    			break;
        	    }
    		}
    	        pats[ state_ptr ] = n;
    	        pat_info->state_ptr = state_ptr;
    	        pat_info->track_status = 0;
    	        if( state_ptr >= pats_num )
    	        {
    	    	    pats_num = state_ptr + 1;
    		    if( pats_num < s->pat_state_size ) pats[ pats_num ] = -1;
    		}
    	    }
    	}
    }
}
int sunvox_get_mpp( sunvox_engine* s ) 
{
    int pats[ MAX_PLAYING_PATS ];
    int pats_num = 0;
    int max = 0;
    int nn = 0;
    for( int i = 0; i < MAX_PLAYING_PATS; i++ ) pats[ i ] = -1;
    for( int p = 0; p < s->sorted_pats_num; p++ )
    {
    	int n = s->sorted_pats[ p ];
	sunvox_pattern* pat = s->pats[ n ];
	sunvox_pattern_info* pat_info = &s->pats_info[ n ];
	for( int i = 0; i < MAX_PLAYING_PATS; i++ )
        {
            if( pats[ i ] == -1 )
            {
                pats[ i ] = n;
                if( i >= pats_num ) pats_num = i + 1;
                nn++;
                break;
            }
        }
	for( int i = 0; i < pats_num; i++ )
        {
    	    int n2 = pats[ i ];
            if( n2 != -1 )
            {
		sunvox_pattern* pat2 = s->pats[ n2 ];
        	sunvox_pattern_info* pat2_info = &s->pats_info[ n2 ];
        	if( pat2_info->x + pat2->lines <= pat_info->x )
        	{
        	    pats[ i ] = -1;
        	    nn--;
        	}
            }
        }
        if( nn > max ) {   max = nn; }
        for( int i = pats_num - 1; i >= 0; i-- )
        {
    	    if( pats[ i ] == -1 )
    		pats_num = i;
    	    else
    		break;
        }
    }
    return max;
}
void sunvox_select_current_playing_patterns( int first_sorted_pat, sunvox_engine* s )
{
    if( first_sorted_pat < 0 ) first_sorted_pat = 0;
    s->cur_playing_pats[ 0 ] = -1;
    s->last_sort_pat = -1;
    if( s->sorted_pats_num )
    {
	int p = 0;
	for( int i = first_sorted_pat; i < s->sorted_pats_num; i++ )
	{
	    int pat_num = s->sorted_pats[ i ];
	    sunvox_pattern* pat = s->pats[ pat_num ];
	    sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
	    if( s->line_counter >= pat_info->x &&
		s->line_counter < pat_info->x + pat->lines )
	    {
		int state_ptr = pat_info->state_ptr;
		if( !s->pat_state[ state_ptr ].busy )
		{
		    clean_pattern_state( &s->pat_state[ state_ptr ], s );
	    	    s->pat_state[ state_ptr ].busy = true;
		}
		s->cur_playing_pats[ p ] = i; 
		p++; if( p >= s->pat_state_size ) break;
	    }
	    if( pat_info->x > s->line_counter )
	    {
		break;
	    }
	    s->last_sort_pat = i;
	}
	if( p < s->pat_state_size ) s->cur_playing_pats[ p ] = -1;
    }
}
int sunvox_get_free_pattern_num( sunvox_engine* s )
{
    if( !s->pats )
    {
	s->pats = SMEM_ZALLOC2( sunvox_pattern*, 16 );
	if( !s->pats ) return -1;
	s->pats_info = SMEM_ZALLOC2( sunvox_pattern_info, 16 );
	if( !s->pats_info ) return -1;
	s->pats_num = 16;
    }
    int p = 0;
    for( p = 0; p < s->pats_num; p++ )
    {
	if( !s->pats[ p ] ) break;
    }
    if( p >= s->pats_num )
    {
	s->pats_num += 16;
	s->pats = SMEM_ZRESIZE2( s->pats, sunvox_pattern*, s->pats_num );
	if( !s->pats ) return -1;
	s->pats_info = SMEM_ZRESIZE2( s->pats_info, sunvox_pattern_info, s->pats_num );
	if( !s->pats_info ) return -1;
    }
    return p;
}
void sunvox_icon_generator( uint16_t* icon, uint icon_seed )
{
    int mirror = pseudo_random( &icon_seed );
    for( int i = 0; i < 16; i++ )
    {
        icon[ i ] = (uint16_t)pseudo_random( &icon_seed ) * 233;
        if( mirror & 16 )
        {
            if( mirror & 2 )
            {
                if( i > 0 && ( pseudo_random( &icon_seed ) & 1 ) )
                    icon[ i ] = icon[ i - 1 ] + 2;
            }
            else
            {
                if( mirror & 4 )
                {
                    if( i > 0 && ( pseudo_random( &icon_seed ) & 1 ) )
                        icon[ i ] = icon[ i - 1 ] << 1;
                }
                else
                {
                    if( mirror & 8 )
                    {
                        if( i > 0 && ( pseudo_random( &icon_seed ) & 1 ) )
                            icon[ i ] = icon[ i - 1 ] >> 1;
                    }
                }
            }
        }
        for( int i2 = 0; i2 < 8; i2++ )
        {
            if( icon[ i ] & ( 1 << i2 ) )
                icon[ i ] |= ( 0x8000 >> i2 );
            else
                icon[ i ] &= ~( 0x8000 >> i2 );
        }
    }
    if( mirror & 1 )
    {
        for( int i = 0; i < 8; i++ ) icon[ 15 - i ] = icon[ i ];
    }
}
void sunvox_new_pattern_with_num( int pat_num, int lines, int channels, int x, int y, uint icon_seed, sunvox_engine* s )
{
    if( pat_num < 0 ) return;
    if( (unsigned)pat_num >= (unsigned)s->pats_num )
    {
	s->pats_num += 16;
	s->pats = SMEM_ZRESIZE2( s->pats, sunvox_pattern*, s->pats_num );
	if( !s->pats ) return;
	s->pats_info = SMEM_ZRESIZE2( s->pats_info, sunvox_pattern_info, s->pats_num );
	if( !s->pats_info ) return;
    }
    s->pats[ pat_num ] = SMEM_ALLOC2( sunvox_pattern, 1 );
    sunvox_pattern* pat = s->pats[ pat_num ];
    if( !pat ) return;
    sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
    pat->lines = lines;
    pat->channels = channels;
    pat->data = SMEM_ZALLOC2( sunvox_note, channels * lines );
    if( !pat->data ) return;
    pat->data_xsize = channels;
    pat->data_ysize = lines;
    pat->ysize = SUNVOX_PATTERN_DEFAULT_YSIZE( s );
    pat->flags = 0;
    pat->id = s->pat_id_counter; s->pat_id_counter++;
    pat_info->x = x;
    pat_info->y = y;
    pat_info->flags = 0;
    sunvox_icon_generator( pat->icon, icon_seed );
    pat->fg[ 0 ] = 0;
    pat->fg[ 1 ] = 0;
    pat->fg[ 2 ] = 0;
    pat->bg[ 0 ] = 255;
    pat->bg[ 1 ] = 255;
    pat->bg[ 2 ] = 255;
    pat->icon_num = -1; 
    pat->name = NULL;
    pat_info->state_ptr = 0;
}
int sunvox_new_pattern( int lines, int channels, int x, int y, uint icon_seed, sunvox_engine* s )
{
    int pat_num = sunvox_get_free_pattern_num( s );
    sunvox_new_pattern_with_num( pat_num, lines, channels, x, y, icon_seed, s );
    return pat_num;
}
int sunvox_new_pattern_empty_clone( int parent, int x, int y, sunvox_engine* s )
{
    int pat_num = sunvox_get_free_pattern_num( s );
    s->pats[ pat_num ] = (sunvox_pattern*)1;
    sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
    smem_clear( pat_info, sizeof( sunvox_pattern_info ) );
    pat_info->x = x;
    pat_info->y = y;
    pat_info->parent_num = parent;
    return pat_num;
}
int sunvox_new_pattern_clone( int pat_num, int x, int y, sunvox_engine *s )
{
    if( (unsigned)pat_num < (unsigned)s->pats_num && s->pats[ pat_num ] )
    {
	uint prev_flags = s->pats_info[ pat_num ].flags;
	if( prev_flags & SUNVOX_PATTERN_INFO_FLAG_CLONE )
	{
	    for( int i = 0; i < s->pats_num; i++ )
	    {
		if( s->pats[ pat_num ] == s->pats[ i ] && 
		    !( s->pats_info[ i ].flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) )
		    pat_num = i;
	    }
	}
	int p = 0;
	for( ; p < s->pats_num; p++ )
	{
	    if( !s->pats[ p ] ) break;
	}
	if( p >= s->pats_num )
	{
	    s->pats = SMEM_ZRESIZE2( s->pats, sunvox_pattern*, s->pats_num + 16 );
	    if( !s->pats ) return -1;
	    s->pats_info = SMEM_ZRESIZE2( s->pats_info, sunvox_pattern_info, s->pats_num + 16 );
	    if( !s->pats_info ) return -1;
	    s->pats_num += 16;
	}
	s->pats[ p ] = s->pats[ pat_num ];
	s->pats_info[ p ].x = x;
	s->pats_info[ p ].y = y;
	s->pats_info[ p ].flags = SUNVOX_PATTERN_INFO_FLAG_CLONE | ( prev_flags & ( SUNVOX_PATTERN_INFO_FLAG_MUTE | SUNVOX_PATTERN_INFO_FLAG_SOLO ) );
	s->pats_info[ p ].parent_num = pat_num;
	s->pats_info[ p ].state_ptr = 0;
	return p;
    }
    return -1;
}
void sunvox_detach_clone( int pat_num, sunvox_engine* s ) 
{
    sunvox_pattern* pat = sunvox_get_pattern( pat_num, s );
    if( !pat ) return;
    sunvox_pattern_info* pat_info = sunvox_get_pattern_info( pat_num, s );
    if( ( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) == 0 ) return;
    pat_info->flags &= ~SUNVOX_PATTERN_INFO_FLAG_CLONE;
    sunvox_pattern* pat2 = (sunvox_pattern*)SMEM_CLONE( pat );
    pat2->data = (sunvox_note*)SMEM_CLONE( pat->data );
    pat2->name = (char*)SMEM_CLONE( pat->name );
    pat2->icon_num = -1; 
    pat2->id = s->pat_id_counter; s->pat_id_counter++;
    s->pats[ pat_num ] = pat2;
}
void sunvox_convert_to_clone( int pat_num, int parent, sunvox_engine* s ) 
{
    sunvox_pattern* pat = sunvox_get_pattern( pat_num, s );
    if( !pat ) return;
    sunvox_pattern_info* pat_info = sunvox_get_pattern_info( pat_num, s );
    if( pat_info->flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) return; 
    sunvox_pattern* parent_pat = sunvox_get_pattern( parent, s );
    if( !parent_pat ) return;
    sunvox_remove_pattern( pat_num, s );
    pat_info->flags |= SUNVOX_PATTERN_INFO_FLAG_CLONE;
    pat_info->parent_num = parent;
    s->pats[ pat_num ] = parent_pat;
}
void sunvox_remove_pattern( int pat_num, sunvox_engine* s )
{
    if( pat_num >= 0 && (unsigned)pat_num < (unsigned)s->pats_num )
    {
	sunvox_pattern* pat = s->pats[ pat_num ];
	if( pat )
	{
	    if( s->pats_info[ pat_num ].flags & SUNVOX_PATTERN_INFO_FLAG_CLONE )
	    {
		s->pats[ pat_num ] = NULL;
	    }
	    else
	    {
		if( pat->data ) smem_free( pat->data );
		if( pat->name ) smem_free( pat->name );
		sunvox_remove_icon( pat->icon_num, s );
		smem_free( pat );
		s->pats[ pat_num ] = NULL;
		for( int i = 0; i < s->pats_num; i++ )
		{
		    if( s->pats[ i ] == pat )
		    {
			s->pats[ i ] = NULL;
		    }
		}
	    }
	}
    }
}
void sunvox_change_pattern_flags( int pat_num, uint pat_flags, uint pat_info_flags, bool reset_set, sunvox_engine* s )
{
    if( pat_num >= 0 && (unsigned)pat_num < (unsigned)s->pats_num )
    {
	sunvox_pattern* pat = s->pats[ pat_num ];
	if( pat )
	{
	    if( reset_set )
		pat->flags |= pat_flags;
	    else
		pat->flags &= ~pat_flags;
	}
	sunvox_pattern_info* pat_info = &s->pats_info[ pat_num ];
	if( reset_set )
	    pat_info->flags |= pat_info_flags;
	else
	    pat_info->flags &= ~pat_info_flags;
    }
}
void sunvox_rename_pattern( int pat_num, const char* name, sunvox_engine* s )
{
    if( (unsigned)pat_num >= (unsigned)s->pats_num ) return;
    sunvox_pattern* pat = s->pats[ pat_num ];
    if( !pat ) return;
    smem_free( pat->name );
    pat->name = SMEM_STRDUP( name );
}
int sunvox_get_pattern_num_by_name( const char* name, sunvox_engine* s )
{
    if( !name ) return -1;
    for( int i = 0; i < s->pats_num; i++ )
    {
	sunvox_pattern* pat = s->pats[ i ];
	if( pat )
	{
	    if( pat->name )
	    {
		if( strcmp( pat->name, name ) == 0 ) return i;
	    }
	}
    }
    return -1;
}
sunvox_note* sunvox_get_pattern_event( int pat_num, int track, int line, sunvox_engine* s )
{
    sunvox_pattern* pat = sunvox_get_pattern( pat_num, s );
    if( pat )
    {
	if( (unsigned)track < (unsigned)pat->channels && (unsigned)line < (unsigned)pat->lines )
	{
	    return &pat->data[ line * pat->data_xsize + track ];
	}
    }
    return NULL;
}
int sunvox_get_free_icon_number( sunvox_engine *s )
{
    if( s->flags & SUNVOX_FLAG_NO_GUI ) return 0;
    if( s->win == 0 ) return 0;
#ifdef SUNVOX_GUI
    if( s->icons_map == 0 )
    {
	s->icons_map = new_image( SUNVOX_ICON_MAP_XSIZE, 16, 0, SUNVOX_ICON_MAP_XSIZE, 16, IMAGE_ALPHA8, s->win->wm );
	s->icons_flags = SMEM_ZALLOC2( int8_t, SUNVOX_ICON_MAP_XSIZE / 16 );
    }
    int i;
    int icons_num = smem_get_size( s->icons_flags );
    int busy_icons = 0;
    for( i = 0; i < icons_num; i++ )
    {
	if( s->icons_flags[ i ] == 0 )
	    break;
	busy_icons++;
    }
    if( i == icons_num )
    {
	s->icons_map = resize_image( s->icons_map, 0, s->icons_map->xsize, s->icons_map->ysize * 2 );
	s->icons_flags = SMEM_ZRESIZE2( s->icons_flags, int8_t, icons_num * 2 );
    }
    s->icons_flags[ i ] = 1;
    return i;
#else
    return 0;
#endif
}
void sunvox_remove_icon( int num, sunvox_engine *s )
{
    if( s->flags & SUNVOX_FLAG_NO_GUI ) return;
#ifdef SUNVOX_GUI
    if( num >= 0 )
	s->icons_flags[ num ] = 0;
#endif
}
void sunvox_make_icon( int pat_num, sunvox_engine *s )
{
    if( s->flags & SUNVOX_FLAG_NO_GUI ) return;
#ifdef SUNVOX_GUI
    if( pat_num >= 0 && (unsigned)pat_num < (unsigned)s->pats_num )
    {
	sunvox_pattern* pat = s->pats[ pat_num ];
	if( pat ) 
	{
	    if( pat->icon_num == -1 )
	    {
		pat->icon_num = sunvox_get_free_icon_number( s );
	    }
	    int icon_x = pat->icon_num & ( ( SUNVOX_ICON_MAP_XSIZE / 16 ) - 1 );
	    int icon_y = pat->icon_num / ( SUNVOX_ICON_MAP_XSIZE / 16 );
	    int icon_ptr = ( icon_y * 16 ) * s->icons_map->xsize + ( icon_x * 16 );
	    uint8_t* map = (uint8_t*)s->icons_map->data;
	    map += icon_ptr;
	    int add = s->icons_map->xsize - 16;
	    for( int y = 0; y < 16; y++ )
    	    {
                uint16_t val = pat->icon[ y ];
                for( int x = 0; x < 16; x++ )
                {
                    if( val & 0x8000 )
                        *map = 255;
                    else
                        *map = 0;
                    val <<= 1;
		    map++;
                }
		map += add;
	    }
	}
    }
#endif
}
void sunvox_update_icons( sunvox_engine *s )
{
    if( s->flags & SUNVOX_FLAG_NO_GUI ) return;
#ifdef SUNVOX_GUI
    if( s->icons_map )
    {
    	update_image( s->icons_map );
    }
#endif
}
void sunvox_print_patterns( sunvox_engine* s )
{
    for( int i = 0; i < s->pats_num; i++ )
    {
	printf( "%03d x:%04d y:%04d ", i, s->pats_info[ i ].x, s->pats_info[ i ].y );
	if( s->pats[ i ] == 0 ) printf( "EMPTY " );
	if( s->pats_info[ i ].flags & SUNVOX_PATTERN_INFO_FLAG_CLONE ) printf( "CLONE (parent %d) ", s->pats_info[ i ].parent_num );
	printf( "\n" );
    }
}
void sunvox_pattern_set_number_of_channels( int pat_num, int cnum, sunvox_engine* s )
{
    if( (unsigned)pat_num < (unsigned)s->pats_num && s->pats[ pat_num ] )
    {
	sunvox_pattern* pat = s->pats[ pat_num ];
	if( cnum > pat->data_xsize )
	{
	    size_t new_size = cnum * pat->data_ysize * sizeof( sunvox_note );
	    sunvox_note* new_data = (sunvox_note*)SMEM_ZRESIZE( pat->data, new_size );
	    if( new_data )
	    {
		int old_xsize = pat->data_xsize;
		pat->data = new_data;
		pat->data_xsize = cnum;
		int new_ptr = ( pat->data_xsize * pat->data_ysize ) - 1;
		int old_ptr = ( old_xsize * pat->data_ysize ) - 1;
		for( int y = pat->data_ysize - 1; y >= 0 ; y-- )
		{
		    for( int x = pat->data_xsize - 1; x >= 0 ; x-- )
		    {
			if( x >= old_xsize ) 
			{
			    smem_clear( &new_data[ new_ptr ], sizeof( sunvox_note ) );
			}
			else 
			{
			    smem_copy( &new_data[ new_ptr ], &new_data[ old_ptr ], sizeof( sunvox_note ) );
			    old_ptr--;
			}
			new_ptr--;
		    }
		}
	    }
	}
	pat->channels = cnum;
    }
}
int sunvox_pattern_set_number_of_lines( int pat_num, int lnum, bool rescale_content, sunvox_engine* s )
{
    if( (unsigned)pat_num >= (unsigned)s->pats_num ) return -1;
    sunvox_pattern* pat = s->pats[ pat_num ];
    if( !pat ) return -1;
    if( lnum > pat->data_ysize )
    {
        size_t new_size = pat->data_xsize * lnum * sizeof( sunvox_note );
        sunvox_note* new_data = (sunvox_note*)SMEM_ZRESIZE( pat->data, new_size );
        if( new_data )
        {
    	    pat->data = new_data;
    	    pat->data_ysize = lnum;
	}
	else
	{
	    slog( "sunvox_pattern_set_number_of_lines(): memory realloc (%d,%d) error\n", (int)lnum, (int)new_size );
	    return -1;
	}
    }
    while( rescale_content )
    {
	if( lnum > pat->lines )
	{
	    int s = lnum / pat->lines;
	    if( s <= 1 ) break;
	    for( int y = lnum - s; y >= 0; y -= s )
    	    {
    		for( int x = 0; x < pat->data_xsize; x++ )
        	{
        	    smem_copy( &pat->data[ y * pat->data_xsize + x ], &pat->data[ ( y / s ) * pat->data_xsize + x ], sizeof( sunvox_note ) );
        	    for( int yy = 1; yy < s; yy++ )
        	    {
            		smem_clear( &pat->data[ ( y + yy ) * pat->data_xsize + x ], sizeof( sunvox_note ) );
            	    }
        	}
    	    }
    	    break;
    	}
	if( lnum < pat->lines )
	{
	    int s = pat->lines / lnum;
	    if( s <= 1 ) break;
	    for( int y = 0; y < lnum; y++ )
    	    {
    		for( int x = 0; x < pat->data_xsize; x++ )
        	{
        	    smem_copy( &pat->data[ y * pat->data_xsize + x ], &pat->data[ ( y * s ) * pat->data_xsize + x ], sizeof( sunvox_note ) );
        	}
    	    }
	    for( int y = lnum; y < pat->lines; y++ )
    	    {
    		for( int x = 0; x < pat->data_xsize; x++ )
        	{
            	    smem_clear( &pat->data[ y * pat->data_xsize + x ], sizeof( sunvox_note ) );
        	}
    	    }
    	    break;
	}
	break;
    }
    pat->lines = lnum;
    return 0;
}
void sunvox_check_solo_mode( sunvox_engine* s )
{
    s->pats_solo_mode = 0;
    for( int i = 0; i < s->pats_num; i++ )
    {
        if( s->pats[ i ] && ( s->pats_info[ i ].flags & SUNVOX_PATTERN_INFO_FLAG_SOLO ) )
        {
            s->pats_solo_mode = 1;
            break;
        }
    }
}
int sunvox_pattern_shift( int pat_num, int track, int line, int lines, int cnt, int polyrhythm_len, sunvox_engine* s )
{
    sunvox_pattern* pat = sunvox_get_pattern( pat_num, s );
    if( !pat ) return -1;
    if( (unsigned)track >= (unsigned)pat->channels ) return -1;
    if( cnt == 0 ) return -1;
    if( lines == 0 ) lines = pat->lines;
    CROP_REGION( line, lines, 0, pat->lines );
    if( lines <= 0 ) return -1;
    const int tmp_size = 16;
    sunvox_note tmp[ tmp_size ];
    while( cnt < 0 ) cnt += lines;
    while( cnt > 0 )
    {
        int cnt2 = cnt;
        if( cnt2 > tmp_size ) cnt2 = tmp_size;
        sunvox_note* pp;
        if( polyrhythm_len )
    	    pp = &pat->data[ track + pat->data_xsize * ( line+polyrhythm_len - cnt2 ) ];
    	else
    	    pp = &pat->data[ track + pat->data_xsize * ( line+lines - cnt2 ) ];
        for( int l = 0; l < cnt2; l++ )
        {
            tmp[ l ] = *pp;
            pp += pat->data_xsize;
        }
        pp = &pat->data[ track + pat->data_xsize * ( line+lines - 1 ) ];
        for( int l = line+lines - 1; l >= line+cnt2; l-- )
        {
            *pp = *( pp - cnt2 * pat->data_xsize );
            pp -= pat->data_xsize;
        }
        pp = &pat->data[ track + pat->data_xsize * line ];
        for( int l = 0; l < cnt2; l++ )
        {
            *pp = tmp[ l ];
            pp += pat->data_xsize;
        }
        cnt -= cnt2;
    }
    return 0;
}
int sunvox_check_pattern_evts( int pat_num, int x, int y, int xsize, int ysize, sunvox_engine* sv )
{
    sunvox_pattern* pat = sunvox_get_pattern( pat_num, sv );
    if( !pat ) return 0;
    CROP_REGION( x, xsize, 0, pat->channels );
    if( xsize <= 0 ) return 0;
    CROP_REGION( y, ysize, 0, pat->lines );
    if( ysize <= 0 ) return 0;
    int rv = 0;
    for( int yy = 0; yy < ysize; yy++ )
    {
        sunvox_note* pat_data = pat->data + (yy+y) * pat->data_xsize + x;
        for( int xx = 0; xx < xsize; xx++ )
        {
            if( pat_data->note ) rv |= ( 1 << 0 );
            if( pat_data->vel ) rv |= ( 1 << 1 );
            if( pat_data->mod ) rv |= ( 1 << 2 );
            if( pat_data->ctl & 0xFF00 ) rv |= ( 1 << 3 );
            if( pat_data->ctl & 0x00FF ) rv |= ( 1 << 4 );
            if( pat_data->ctl_val & 0xFF00 ) rv |= ( 1 << 5 );
            if( pat_data->ctl_val & 0x00FF ) rv |= ( 1 << 6 );
            pat_data++;
        }
    }
    return rv;
}
int sunvox_save_pattern_buf( const char* filename, sunvox_note* buf, int xsize, int ysize )
{
    sfs_file f = sfs_open( filename, "wb" );
    if( !f ) return -1;
    const char* sign = "SVOXPATD";
    sfs_write( sign, 1, 8, f );
    sfs_write( &xsize, 4, 1, f );
    sfs_write( &ysize, 4, 1, f );
    sfs_write( buf, sizeof( sunvox_note ), xsize * ysize, f );
    sfs_close( f );
    return 0;
}
sunvox_note* sunvox_load_pattern_buf( const char* filename, int* xsize, int* ysize )
{
    sfs_file f = sfs_open( filename, "rb" );
    if( !f ) return NULL;
    sunvox_note* buf = NULL;
    char sign[ 9 ];
    sign[ 8 ] = 0;
    sfs_read( &sign, 8, 1, f );
    if( smem_strcmp( sign, "SVOXPATD" ) == 0 )
    {
        sfs_read( xsize, 4, 1, f );
        sfs_read( ysize, 4, 1, f );
        if( *xsize > 0 && *ysize > 0 )
        {
            int s = xsize[0] * ysize[0];
            buf = SMEM_ZALLOC2( sunvox_note, s );
            if( buf )
            {
                sfs_read( buf, sizeof( sunvox_note ), s, f );
            }
        }
    }
    sfs_close( f );
    return buf;
}
