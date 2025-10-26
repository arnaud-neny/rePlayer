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
void xm_new_pattern( uint16_t num, uint16_t rows, uint16_t channels, xm_song* song )
{
    if( !song ) return;
    if( num >= MAX_XM_PATTERNS ) return;
    xm_pattern* pat;
    pat = SMEM_ZALLOC2( xm_pattern, 1 );
    song->patterns[ num ] = pat;
    pat->rows = rows; 
    pat->channels = channels;
    pat->data_size = rows * channels * sizeof( xm_note ); 
    pat->pattern_data = (xm_note*)SMEM_ZALLOC( pat->data_size );
}
void xm_remove_pattern( uint16_t num, xm_song* song )
{
    if( !song ) return;
    if( num >= MAX_XM_PATTERNS ) return;
    xm_pattern* pat = song->patterns[ num ];
    if( pat )
    {
	smem_free( pat->pattern_data );
	pat->pattern_data = NULL;
	smem_free( pat );
    }
    song->patterns[ num ] = 0;
}
