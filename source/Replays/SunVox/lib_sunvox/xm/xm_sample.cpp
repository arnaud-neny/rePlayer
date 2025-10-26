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
void xm_new_sample( uint16_t num, uint16_t ins_num, const char* name, int length, int type, xm_song* song )
{
    if( !song ) return;
    if( ins_num >= MAX_XM_INSTRUMENTS ) return;
    if( num >= MAX_XM_INS_SAMPLES ) return;
    xm_instrument* ins = song->instruments[ ins_num ];
    xm_sample* smp; 
    int16_t* data;
    int created_size;
    smp = SMEM_ZALLOC2( xm_sample, 1 );
    for( int a = 0 ; a < 22 ; a++ )
    {
	smp->name[ a ] = name[ a ];
	if( name[ a ] == 0 ) break;
    }
    smp->data = NULL;
    if( length )
    {
	data = (int16_t*)SMEM_ALLOC( length );
	smp->data = (int16_t*)data;
	created_size = length;
    }
    else
    {
	created_size = 0;
    }
    smp->length = created_size;
    smp->type = (uint8_t) type;
    smp->volume = 0x40;
    smp->panning = 0x80;
    smp->relative_note = 0;
    smp->finetune = 0;
    smp->reppnt = 0;
    smp->replen = 0;
    ins->samples[ num ] = smp;
}
void xm_remove_sample( uint16_t num, uint16_t ins_num, xm_song* song )
{
    if( !song ) return;
    if( ins_num >= MAX_XM_INSTRUMENTS ) return;
    if( num >= MAX_XM_INS_SAMPLES ) return;
    xm_instrument* ins = song->instruments[ ins_num ];
    if( !ins ) return;
    xm_sample* smp = ins->samples[ num ];
    if( !smp ) return;
    smem_free( smp->data );
    smp->data = NULL;
    smem_free( smp );
    ins->samples[ num ] = NULL;
}
void xm_bytes2frames( xm_sample* smp, xm_song* song )
{
    if( !song ) return;
    int bits = 8;
    int channels = 1;
    if( smp->type & 16 ) bits = 16;
    if( smp->type & 64 ) channels = 2;
    smp->length = smp->length / ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt / ( ( bits / 8 ) * channels );
    smp->replen = smp->replen / ( ( bits / 8 ) * channels );
}
void xm_frames2bytes( xm_sample* smp, xm_song* song )
{
    if( !song ) return;
    int bits = 8;
    int channels = 1;
    if( smp->type & 16 ) bits = 16;
    if( smp->type & 64 ) channels = 2;
    smp->length = smp->length * ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt * ( ( bits / 8 ) * channels );
    smp->replen = smp->replen * ( ( bits / 8 ) * channels );
}
