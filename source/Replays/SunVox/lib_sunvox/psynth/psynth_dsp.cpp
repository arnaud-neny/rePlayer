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

#include "psynth.h"
biquad_filter* biquad_filter_new( uint32_t flags )
{
    biquad_filter* f = SMEM_ZALLOC2( biquad_filter, 1 );
    if( !f ) return NULL;
    f->type = 1 << BFT_STAGES_OFFSET;
    f->flags = flags;
    return f;
}
void biquad_filter_remove( biquad_filter* f )
{
    smem_free( f );
}
static void biquad_filter_calc_coeffs( biquad_filter* f )
{
    f->flags &= ~BIQUAD_FILTER_FLAG_IGNORE_STAGES;
    biquad_filter_ftype ftype = get_biquad_filter_ftype( f->type );
    biquad_filter_float freq = f->freq;
    biquad_filter_float omega = 2 * M_PI * freq / (biquad_filter_float)f->sfreq;
    if( ftype >= BIQUAD_FILTER_1POLE_LPF )
    {
	switch( ftype )
	{
	    case BIQUAD_FILTER_1POLE_LPF:
	    {
		biquad_filter_float a1 = exp( -omega );
                biquad_filter_float b0 = 1.0 - a1;
                f->state.b[ 0 ] = b0;
		f->state.b[ 1 ] = 0;
		f->state.b[ 2 ] = 0;
                f->state.a[ 1 ] = -a1;
		f->state.a[ 2 ] = 0;
		break;
	    }
	    case BIQUAD_FILTER_1POLE_HPF:
	    {
		biquad_filter_float a1 = exp( -omega );
                biquad_filter_float b0 = 1.0 - a1;
                f->state.b[ 0 ] = 1 - b0;
		f->state.b[ 1 ] = -a1;
		f->state.b[ 2 ] = 0;
                f->state.a[ 1 ] = -a1;
		f->state.a[ 2 ] = 0;
		break;
	    }
	    default: break;
	}
	return;
    }
    biquad_filter_float alpha = 0;
    biquad_filter_float tsin;
    biquad_filter_float tcos;
    biquad_filter_float Q = f->Q;
    biquad_filter_float dBgain = f->dBgain;
    if( Q < 0.00001 ) Q = 0.00001;
    int stages = get_biquad_filter_stages( f->type );
    if( stages > 1 )
    {
	int ignore_stages_after_type = BIQUAD_FILTER_APF;
	if( f->flags & BIQUAD_FILTER_FLAG_USE_STAGES_FOR_APF ) ignore_stages_after_type++;
	if( ftype >= ignore_stages_after_type )
	{
	    f->flags |= BIQUAD_FILTER_FLAG_IGNORE_STAGES;
	}
	else
	{
	    if( ftype != BIQUAD_FILTER_BPF_CPG && ftype != BIQUAD_FILTER_NOTCH )
	    {
		if( Q > 1 ) Q = pow( Q, 1.0F / stages );
    		dBgain = pow( dBgain, 1.0F / stages );
    	    }
    	}
    }
    tsin = sin( omega );
    tcos = cos( omega );
    switch( get_biquad_filter_qtype( f->type ) )
    {
	case BIQUAD_FILTER_Q_IS_Q: alpha = tsin / ( 2 * Q ); break;
	case BIQUAD_FILTER_Q_IS_BANDWIDTH: alpha = tsin * sinh( log( 2 ) / 2 * Q * omega / tsin ); break;
	default: break;
    }
    biquad_filter_float a0, a1, a2;
    biquad_filter_float b0, b1, b2;
    switch( ftype )
    {
	case BIQUAD_FILTER_LPF:
	    b0 = ( 1 - tcos ) / 2;
            b1 = 1 - tcos;
            b2 = b0;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
	    break;
        case BIQUAD_FILTER_HPF:
    	    b0 = ( 1 + tcos ) / 2;
            b1 = -( 1 + tcos );
            b2 = b0;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
    	    break;
	case BIQUAD_FILTER_BPF_CSG:
	    b0 = tsin / 2;
            b1 = 0;
            b2 = -b0;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
	    break;
	case BIQUAD_FILTER_BPF_CPG:
	    b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
	    break;
	case BIQUAD_FILTER_NOTCH:
	    b0 = 1;
            b1 = -2 * tcos;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
	    break;
	case BIQUAD_FILTER_APF:
	    b0 = 1 - alpha;
            b1 = -2 * tcos;
            b2 = 1 + alpha;
            a0 = 1 + alpha;
            a1 = -2 * tcos;
            a2 = 1 - alpha;
	    break;
	case BIQUAD_FILTER_PEAKING:
	{
	    biquad_filter_float A = pow( 10, ( dBgain / 40 ) );
	    b0 = 1 + alpha * A;
            b1 = -2 * tcos;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * tcos;
            a2 = 1 - alpha / A;
	    break;
	}
	case BIQUAD_FILTER_LOWSHELF:
	{
	    biquad_filter_float A = pow( 10, ( dBgain / 40 ) );
	    if( get_biquad_filter_qtype( f->type ) == BIQUAD_FILTER_Q_IS_SHELF_SLOPE )
		alpha = tsin / 2 * sqrt( ( A + 1 / A ) * ( 1 / Q - 1 ) + 2 );
	    b0 = A * ( ( A + 1 ) - ( A - 1 ) * tcos + 2 * sqrt( A ) * alpha );
            b1 = 2 * A * ( ( A - 1 ) - ( A + 1 ) * tcos );
            b2 = A * ( ( A + 1 ) - ( A - 1 ) * tcos - 2 * sqrt( A ) * alpha );
            a0 = ( A + 1 ) + ( A - 1 ) * tcos + 2 * sqrt( A ) * alpha;
            a1 = -2 * ( ( A - 1 ) + ( A + 1 ) * tcos );
            a2 = ( A + 1 ) + ( A - 1 ) * tcos - 2 * sqrt( A ) * alpha;
	    break;
	}
	case BIQUAD_FILTER_HIGHSHELF:
	{
	    biquad_filter_float A = pow( 10, ( dBgain / 40 ) );
	    if( get_biquad_filter_qtype( f->type ) == BIQUAD_FILTER_Q_IS_SHELF_SLOPE )
		alpha = tsin / 2 * sqrt( ( A + 1 / A ) * ( 1 / Q - 1 ) + 2 );
	    b0 = A * ( ( A + 1 ) + ( A - 1 ) * tcos + 2 * sqrt( A ) * alpha );
            b1 = -2 * A * ( ( A - 1 ) + ( A + 1 ) * tcos );
            b2 = A * ( ( A + 1 ) + ( A - 1 ) * tcos - 2 * sqrt( A ) * alpha );
            a0 = ( A + 1 ) - ( A - 1 ) * tcos + 2 * sqrt( A ) * alpha;
            a1 = 2 * ( ( A - 1 ) - ( A + 1 ) * tcos );
            a2 = ( A + 1 ) - ( A - 1 ) * tcos - 2 * sqrt( A ) * alpha;
	    break;
	}
	default:
	    break;
    }
    biquad_filter_float a = 1 / a0;
    b0 *= a;
    b1 *= a;
    b2 *= a;
    a1 *= a;
    a2 *= a;
    f->state.b[ 0 ] = b0;
    f->state.b[ 1 ] = b1;
    f->state.b[ 2 ] = b2;
    f->state.a[ 1 ] = a1;
    f->state.a[ 2 ] = a2;
}
void biquad_filter_stop( biquad_filter* f )
{
    int stages = get_biquad_filter_stages2( f );
    smem_clear( &f->state.x, BIQUAD_FILTER_STAGE_BYTES * stages );
    smem_clear( &f->state.y, BIQUAD_FILTER_STAGE_BYTES * stages );
    f->interp_len = 0;
}
static void biquad_filter_copy_state( biquad_filter* f, biquad_filter_state* dest, biquad_filter_state* src )
{
    int stages = get_biquad_filter_stages2( f );
    memcpy( dest->a, src->a, sizeof( dest->a ) * 2 );
    memcpy( dest->x, src->x, BIQUAD_FILTER_STAGE_BYTES * stages );
    memcpy( dest->y, src->y, BIQUAD_FILTER_STAGE_BYTES * stages );
}
static void biquad_filter_stop_and_interp( biquad_filter* f, int interp_len )
{
    if( interp_len ) biquad_filter_copy_state( f, &f->interp_state, &f->state );
    biquad_filter_stop( f );
    f->interp_len = interp_len;
    for( int i = 0; i < BIQUAD_FILTER_CHANNELS; i++ ) f->interp_ptr[ i ] = 0;
}
static void biquad_filter_interp( biquad_filter* f, int interp_len )
{
    if( interp_len )
    {
	biquad_filter_copy_state( f, &f->interp_state, &f->state );
	f->interp_len = interp_len;
	for( int i = 0; i < BIQUAD_FILTER_CHANNELS; i++ ) f->interp_ptr[ i ] = 0;
    }
}
bool biquad_filter_change( 
    biquad_filter* 		f, 
    int				interp_len,
    uint32_t	 		type, 
    int				sfreq,
    biquad_filter_float         freq,
    biquad_filter_float         dBgain,
    biquad_filter_float         Q )
{
    bool rv = false;
    bool recalc = false;
    bool stop = false;
    bool interp = false;
    if( f->type != type )
    {
        f->type = type;
        stop = true;
        recalc = true;
    }
    if( f->sfreq != sfreq )
    {
        f->sfreq = sfreq;
        stop = true;
        recalc = true;
    }
    LIMIT_NUM( freq, 0, f->sfreq / 2 - 50 );
    if( f->freq != freq )
    {
	if( get_biquad_filter_ftype( type ) >= BIQUAD_FILTER_1POLE_LPF )
	{
    	    if( freq < f->freq )
    	    {
    		if( freq < 0.1 )
    		{
        	    stop = true;
        	    interp = true;
    		}
    	    }
	}
	else
	{
	    bool unstable = false;
    	    if( freq < f->freq )
    	    {
    		biquad_filter_float d = f->freq / freq;
    		if( d > 2 ) unstable = true;
    		if( freq < 40 ) unstable = true; 
    	    }
    	    else
    	    {
    	        if( freq > f->sfreq / 2 - 400 ) unstable = true;
    	    }
	    if( unstable )
    	    {
        	stop = true;
        	interp = true;
    	    }
    	}
        f->freq = freq;
        recalc = true;
    }
    if( f->dBgain != dBgain )
    {
	f->dBgain = dBgain;
        recalc = true;
    }
    if( f->Q != Q )
    {
        f->Q = Q;
        recalc = true;
    }
    if( recalc )
    {
	if( stop )
	{
	    if( interp )
    	        biquad_filter_stop_and_interp( f, interp_len );
    	    else
    		biquad_filter_stop( f );
	}
	else
	{
	    if( f->type & BFT_SMOOTH )
		biquad_filter_interp( f, interp_len );
	}
        biquad_filter_calc_coeffs( f );
        rv = true;
    }
    return rv;
}
void biquad_filter_run( biquad_filter* f, int ch, PS_STYPE* in, PS_STYPE* out, size_t len )
{
    biquad_filter_ftype ftype = get_biquad_filter_ftype( f->type );
    int stages = get_biquad_filter_stages2( f );
    for( int scnt = 0; scnt < 2; scnt++ ) 
    {
        biquad_filter_state* fs = &f->state;
        int interp_ptr = 0;
	size_t size = len;
        if( scnt )
        {
    	    interp_ptr = f->interp_ptr[ ch ];
	    if( (unsigned)interp_ptr >= (unsigned)f->interp_len )
	        break;
    	    fs = &f->interp_state;
    	    if( (size_t)( f->interp_len - interp_ptr ) < size )
    		size = f->interp_len - interp_ptr;
    	}
        biquad_filter_float a1 = fs->a[ 1 ];
        biquad_filter_float a2 = fs->a[ 2 ];
        biquad_filter_float b0 = fs->b[ 0 ];
        biquad_filter_float b1 = fs->b[ 1 ];
        biquad_filter_float b2 = fs->b[ 2 ];
	biquad_filter_float* RESTRICT buf = f->buf;
	size_t i = 0;
	while( i < size )
	{
	    size_t s = size - i;
	    if( s > (unsigned)BIQUAD_FILTER_BUF_SIZE ) s = BIQUAD_FILTER_BUF_SIZE;
	    for( size_t i2 = 0; i2 < s; i2++, i++ )
	    {
#ifdef PS_STYPE_FLOATINGPOINT
		biquad_filter_float v = in[ i ];
		psynth_denorm_add_white_noise( v );
		buf[ i2 ] = v;
#else
		biquad_filter_float v = (biquad_filter_float)in[ i ] / (biquad_filter_float)PS_STYPE_ONE;
		buf[ i2 ] = v;
#endif
	    }
	    i -= s;
	    for( int stage = 0; stage < stages; stage++ )
	    {
		int ch2 = stage * BIQUAD_FILTER_CHANNELS + ch;
	        biquad_filter_float x0;
    		biquad_filter_float x1 = fs->x[ BIQUAD_FILTER_POLES * ch2 + 0 ];
		biquad_filter_float x2 = fs->x[ BIQUAD_FILTER_POLES * ch2 + 1 ];
		biquad_filter_float y0;
		biquad_filter_float y1 = fs->y[ BIQUAD_FILTER_POLES * ch2 + 0 ];
		biquad_filter_float y2 = fs->y[ BIQUAD_FILTER_POLES * ch2 + 1 ];
		if( ftype >= BIQUAD_FILTER_1POLE_LPF )
		{
		    if( ftype == BIQUAD_FILTER_1POLE_HPF )
			for( size_t i2 = 0; i2 < s; i2++ )
			{
			    if( 1 )
			    {
		    		x0 = buf[ i2 ];
		    		y0 = ( 1 - b0 ) * x0 - a1 * y1;
				y1 = y0;
				buf[ i2 ] -= y0;
			    }
			    else
			    {
		    		x0 = buf[ i2 ];
		    		y0 = b0 * x0 + b1 * x1
		    		             - a1 * y1;
				x1 = x0;
				y1 = y0;
				buf[ i2 ] = y0;
		    	    }
			}
		    else
			for( size_t i2 = 0; i2 < s; i2++ )
			{
		    	    x0 = buf[ i2 ];
		    	    y0 = b0 * x0 - a1 * y1;
			    y1 = y0;
			    buf[ i2 ] = y0;
			}
		}
		else
		{
		    if( 1 )
		    {
			for( size_t i2 = 0; i2 < s; i2++ )
			{
		    	    x0 = buf[ i2 ];
		    	    y0 = b0 * x0 + b1 * x1 + b2 * x2
		    			 - a1 * y1 - a2 * y2;
    			    x2 = x1;
			    x1 = x0;
		    	    y2 = y1;
			    y1 = y0;
			    buf[ i2 ] = y0;
			}
		    }
		    else
		    {
			biquad_filter_float tau = 1 - a1 + a2;
			biquad_filter_float g = sqrt( (4/tau) * (1+a1+a2) );
			biquad_filter_float R = 2 * (1-a2) / (g*tau);
			biquad_filter_float c0 = (b0-b1+b2) / tau;
			biquad_filter_float c1 = 4 * (b0-b2) / (g*tau);
			biquad_filter_float c2 = 4 * (b0+b1+b2) / (g*g*tau);
			g *= 0.5;
			for( size_t i2 = 0; i2 < s; i2++ )
			{
			    biquad_filter_float in = buf[ i2 ];
			    biquad_filter_float hp = (in - (g + 2*R) * y1 - y2) / (g*g + 2*g*R + 1);
		    	    biquad_filter_float bp = g*hp + y1;
		    	    biquad_filter_float lp = g*bp + y2;
		    	    y1 = g*hp + bp;
		    	    y2 = g*bp + lp;
		    	    buf[ i2 ] = c0*hp + c1*bp + c2*lp;
			}
		    }
		}
		fs->x[ BIQUAD_FILTER_POLES * ch2 + 0 ] = x1;
		fs->x[ BIQUAD_FILTER_POLES * ch2 + 1 ] = x2;
		fs->y[ BIQUAD_FILTER_POLES * ch2 + 0 ] = y1;
		fs->y[ BIQUAD_FILTER_POLES * ch2 + 1 ] = y2;
	    }
	    if( scnt == 0 )
	    {
		for( size_t i2 = 0; i2 < s; i2++, i++ )
		{
#ifdef PS_STYPE_FLOATINGPOINT
		    out[ i ] = buf[ i2 ];
#else
		    out[ i ] = (PS_STYPE)( buf[ i2 ] * (biquad_filter_float)PS_STYPE_ONE );
#endif
		}
	    }
	    else
	    {
		for( size_t i2 = 0; i2 < s; i2++, i++, interp_ptr++ )
		{
		    PS_STYPE2 v1 = out[ i ];
		    PS_STYPE2 v2;
#ifdef PS_STYPE_FLOATINGPOINT
		    v2 = buf[ i2 ];
#else
		    v2 = (PS_STYPE2)( buf[ i2 ] * (biquad_filter_float)PS_STYPE_ONE );
#endif
		    v1 = (PS_STYPE2)( v1 * interp_ptr + v2 * ( f->interp_len - interp_ptr ) ) / (PS_STYPE2)f->interp_len;
		    out[ i ] = (PS_STYPE)v1;
		}
	    }
	}
        if( scnt )
        {
    	    f->interp_ptr[ ch ] = interp_ptr;
	}
    }
}
biquad_filter_float biquad_filter_freq_response( biquad_filter* f, biquad_filter_float freq )
{
    biquad_filter_state* fs = &f->state;
    biquad_filter_float w = freq / f->sfreq * M_PI * 2;
    biquad_filter_float a1 = fs->a[ 1 ];
    biquad_filter_float a2 = fs->a[ 2 ];
    biquad_filter_float b0 = fs->b[ 0 ];
    biquad_filter_float b1 = fs->b[ 1 ];
    biquad_filter_float b2 = fs->b[ 2 ];
    biquad_filter_float rv = sqrt( 
	( b0 * b0 + b1 * b1 + b2 * b2 + 2 * ( b0 * b1 + b1 * b2 ) * cos( w ) + 2 * ( b0 * b2 ) * cos( 2 * w ) ) /
	( 1 + a1 * a1 + a2 * a2 + 2 * ( a1 + a1 * a2 ) * cos( w ) + 2 * a2 * cos( 2 * w ) )
    );
    biquad_filter_ftype ftype = get_biquad_filter_ftype( f->type );
    biquad_filter_float t = rv;
    if( ( f->flags & BIQUAD_FILTER_FLAG_IGNORE_STAGES ) == 0 )
	for( int i = 1; i < get_biquad_filter_stages( f->type ); i++ ) rv *= t;
    return rv;
}
#ifndef PSYNTH_OVERSAMPLER_DISABLED
const float g_psynth_oversampler_sinc_2x[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS ] = { 1.75465e-09, -2.00738e-09, 0.222552, 0.554896, 0.222552, -2.00738e-09, 1.75465e-09 };
const float g_psynth_oversampler_sinc_4x[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS ] = { -1.94438e-09, 0.0359841, 0.246617, 0.434798, 0.246617, 0.0359841, -1.94438e-09 };
psynth_oversampler* psynth_oversampler_new()
{
    psynth_oversampler* os = SMEM_ZALLOC2( psynth_oversampler, 1 );
    if( !os ) return NULL;
    return os;
}
void psynth_oversampler_remove( psynth_oversampler* os )
{
    smem_free( os );
}
void psynth_oversampler_stop( psynth_oversampler* os )
{
    smem_clear( &os->up, sizeof( os->up ) );
    smem_clear( &os->down, sizeof( os->down ) );
}
void psynth_oversampler_init( psynth_oversampler* os, int scale, PS_STYPE* src, PS_STYPE* dest, size_t size )
{
    os->scale = scale;
    os->src = src;
    os->dest = dest;
    os->size = size;
    os->ptr = 0;
}
PS_STYPE* psynth_oversampler_begin( psynth_oversampler* os, int ch )
{
    if( (unsigned)ch >= PSYNTH_OVERSAMPLER_CHANNELS ) 
    {
	slog( "Invalid oversampler channel %d\n", ch );
	return 0;
    }
    if( os->scale <= 1 ) return 0;
    if( os->size == 0 ) return 0;
    os->buf_size = os->size * os->scale;
    if( os->buf_size > PSYNTH_OVERSAMPLER_BUF_SIZE )
	os->buf_size = PSYNTH_OVERSAMPLER_BUF_SIZE;
    PS_STYPE* RESTRICT src = os->src;
    PS_STYPE* RESTRICT buf = os->buf;
    PS_STYPE2 y0 = os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 0 ];
    PS_STYPE2 y1 = os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 1 ];
    PS_STYPE2 y2 = os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 2 ];
    PS_STYPE2 y3 = os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 3 ];
    if( os->scale == 2 )
    {
	for( size_t i = 0; i < os->buf_size; i += 2, src++ )
	{
	    y0 = y1;
	    y1 = y2;
	    y2 = y3;
	    y3 = *src;
            PS_STYPE2 a = ( 3 * ( y1 - y2 ) - y0 + y3 ) / 2;
            PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
            PS_STYPE2 c = ( y2 - y0 ) / 2;
	    PS_STYPE2 v = ( ( ( a / 2 ) + b ) / 2 + c ) / 2 + y1;
	    buf[ i ] = y1; 
	    buf[ i + 1 ] = v;
	}
    }
    if( os->scale == 4 )
    {
	for( size_t i = 0; i < os->buf_size; i += 4, src++ )
	{
	    y0 = y1;
	    y1 = y2;
	    y2 = y3;
	    y3 = *src;
            PS_STYPE2 a = ( 3 * ( y1 - y2 ) - y0 + y3 ) / 2;
            PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
            PS_STYPE2 c = ( y2 - y0 ) / 2;
#ifdef PS_STYPE_FLOATINGPOINT
	    PS_STYPE2 v1 = ( ( ( a * 0.25 ) + b ) * 0.25 + c ) * 0.25 + y1;
	    PS_STYPE2 v2 = ( ( ( a * 0.5 ) + b ) * 0.5 + c ) * 0.5 + y1;
	    PS_STYPE2 v3 = ( ( ( a * 0.75 ) + b ) * 0.75 + c ) * 0.75 + y1;
#else
	    PS_STYPE2 v1 = ( ( ( a / 4 ) + b ) / 4 + c ) / 4 + y1;
	    PS_STYPE2 v2 = ( ( ( a / 2 ) + b ) / 2 + c ) / 2 + y1;
	    PS_STYPE2 v3 = ( ( ( ( ( ( a * 3 ) / 4 ) + b ) * 3 ) / 4 + c ) * 3 ) / 4 + y1;
#endif
	    buf[ i ] = y1; 
	    buf[ i + 1 ] = v1;
	    buf[ i + 2 ] = v2;
	    buf[ i + 3 ] = v3;
	}
    }
    os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 0 ] = y0;
    os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 1 ] = y1;
    os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 2 ] = y2;
    os->up[ ch * PSYNTH_OVERSAMPLER_UPSMP_VALS + 3 ] = y3;
    os->src = src;
    return buf;
}
int psynth_oversampler_end( psynth_oversampler* os, int ch )
{
    if( (unsigned)ch >= PSYNTH_OVERSAMPLER_CHANNELS ) 
    {
	slog( "Invalid oversampler channel %d\n", ch );
	return 1;
    }
    if( os->scale <= 1 ) return 1;
    size_t new_size = os->buf_size / os->scale;
    PS_STYPE* RESTRICT buf = os->buf;
    PS_STYPE* RESTRICT dest = os->dest + os->ptr;
    PS_STYPE2 c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS ];
    for( int i = 0; i < PSYNTH_OVERSAMPLER_DOWNSMP_VALS; i++ ) c[ i ] = os->down[ ch * PSYNTH_OVERSAMPLER_DOWNSMP_VALS + i ];
    if( os->scale == 2 )
    {
	for( size_t i = 0; i < new_size; i++, buf += 2 )
	{
	    for( int i2 = 0; i2 < PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 2; i2++ ) c[ i2 ] = c[ i2 + 2 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 2 ] = buf[ 0 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 1 ] = buf[ 1 ];
	    PS_STYPE2 v = 0;
	    for( int i2 = 0; i2 < PSYNTH_OVERSAMPLER_DOWNSMP_VALS; i2++ ) v += c[ i2 ] * g_psynth_oversampler_sinc_2x[ i2 ];
	    dest[ i ] = v;
	}
    }
    if( os->scale == 4 )
    {
	for( size_t i = 0; i < new_size; i++, buf += 4 )
	{
	    for( int i2 = 0; i2 < PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 4; i2++ ) c[ i2 ] = c[ i2 + 4 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 4 ] = buf[ 0 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 3 ] = buf[ 1 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 2 ] = buf[ 2 ];
	    c[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS - 1 ] = buf[ 3 ];
	    PS_STYPE2 v = 0;
	    for( int i2 = 0; i2 < PSYNTH_OVERSAMPLER_DOWNSMP_VALS; i2++ ) v += c[ i2 ] * g_psynth_oversampler_sinc_4x[ i2 ];
	    dest[ i ] = v;
	}
    }
    for( int i = 0; i < PSYNTH_OVERSAMPLER_DOWNSMP_VALS; i++ ) os->down[ ch * PSYNTH_OVERSAMPLER_DOWNSMP_VALS + i ] = c[ i ];
    os->size -= new_size;
    os->ptr += new_size;
    if( os->size == 0 ) return 1;
    return 0;
}
#endif
int psynth_renderbuf2output(
    int retval, 
    PS_STYPE** outputs, int outputs_num, int out_offset, int out_frames, 
    PS_STYPE* render_buf1, PS_STYPE* render_buf2, int rendered_frames,
    int volume, int pan,
    psynth_renderbuf_pars* RESTRICT renderbuf_pars,
    psmoother_coefs* RESTRICT smoother_coefs,
    int srate )
{
#ifdef PS_STYPE_FLOATINGPOINT
    const PS_STYPE2 norm = 32768;
#else
    const PS_STYPE2 norm = 32768 / 4;
    volume /= 4;
    if( volume > 32768 ) volume = 32768;
#endif
    if( !render_buf2 ) render_buf2 = render_buf1;
    PS_STYPE* render_bufs[ 2 ];
    render_bufs[ 0 ] = render_buf1;
    render_bufs[ 1 ] = render_buf2;
    bool vol_smoothing = false;
    bool empty_buf = ( retval == 0 || retval == 2 );
    const int tmp_buf_size = 64;
    PS_STYPE tmp_buf[ tmp_buf_size ];
    int anticlick_counter = 0;
    for( int ch = 0; ch < outputs_num; ch++ )
    {
        PS_STYPE* out = outputs[ ch ] + out_offset;
        PS_STYPE* render_buf = render_bufs[ ch ];
	anticlick_counter = renderbuf_pars->anticlick_counter;
        int vol = volume;
        if( pan != 32768 )
        {
	    if( pan < 32768 )
	    {
		if( ch == 1 )
		{
		    if( pan < 0 ) pan = 0;
		    vol = (int64_t)volume * pan / 32768;
		}
	    }
	    else
	    {
		if( ch == 0 )
		{
		    if( pan > 32768 * 2 ) pan = 32768 * 2;
		    vol = (int64_t)volume * ( 32768 - ( pan - 32768 ) ) / 32768;
		}
	    }
	}
	if( renderbuf_pars->start )
	{
	    psmoother_reset( &renderbuf_pars->vol[ ch ], vol, norm );
	    anticlick_counter = 0;
	}
	if( renderbuf_pars->anticlick )
	{
	    anticlick_counter = 32768;
            renderbuf_pars->anticlick_sample[ ch ] = renderbuf_pars->last_sample[ ch ];
	}
	PS_STYPE2 target_vol = psmoother_target( vol, norm ); 
	int ptr = 0;
	while( ptr < rendered_frames )
	{
	    int size = rendered_frames - ptr;
	    if( size > tmp_buf_size ) size = tmp_buf_size;
	    bool sm = psmoother_check( &renderbuf_pars->vol[ ch ], target_vol );
	    if( sm == false && anticlick_counter == 0 ) break;
	    if( sm )
	    {
		vol_smoothing = true;
		for( int i = 0; i < size; i++ )
		{
	    	    PS_STYPE2 v = psmoother_val( smoother_coefs, &renderbuf_pars->vol[ ch ], target_vol );
		    tmp_buf[ i ] = PS_NORM_STYPE_MUL2( render_buf[ i + ptr ], v, 32768, 32768 * 4 );
		}
	    }
	    else
	    {
		if( vol == norm )
		{
	    	    for( int i = 0; i < size; i++ )
		        tmp_buf[ i ] = render_buf[ i + ptr ];
		}
		else
		{
		    if( vol == 0 )
		    {
			for( int i = 0; i < size; i++ ) tmp_buf[ i ] = 0;
		    }
		    else
		    {
	    		PS_STYPE2 vol2 = PS_NORM_STYPE( vol, norm ); 
	    		for( int i = 0; i < size; i++ )
		    	    tmp_buf[ i ] = PS_NORM_STYPE_MUL( render_buf[ i + ptr ], vol2, norm );
		    }
		}
	    }
	    if( anticlick_counter )
    	    {
        	int anticlick_step = 1024; 
        	if( srate != 44100 ) anticlick_step = anticlick_step * 44100 / srate;
        	for( int i = 0; i < size; i++ )
        	{
            	    PS_STYPE2 v = renderbuf_pars->anticlick_sample[ ch ];
            	    PS_STYPE2 v2 = tmp_buf[ i ];
            	    v = v * (PS_STYPE2)anticlick_counter / (PS_STYPE2)32768;
            	    v2 = v2 * (PS_STYPE2)( 32768 - anticlick_counter ) / (PS_STYPE2)32768;
            	    tmp_buf[ i ] = v2 + v;
            	    anticlick_counter -= anticlick_step;
            	    if( anticlick_counter < 0 )
            	    {
            		anticlick_counter = 0;
                	break;
            	    }
        	}
    	    }
	    if( empty_buf )
	    {
	    	for( int i = 0; i < size; i++ ) out[ i + ptr ] = tmp_buf[ i ];
	    }
	    else
	    {
	    	for( int i = 0; i < size; i++ ) out[ i + ptr ] += tmp_buf[ i ];
	    }
	    renderbuf_pars->last_sample[ ch ] = tmp_buf[ size - 1 ];
	    ptr += size;
	}
	if( ptr < rendered_frames )
	{
	    if( vol == norm )
	    {
		renderbuf_pars->last_sample[ ch ] = render_buf[ rendered_frames - 1 ];
		if( empty_buf )
		{
		    if( out != render_buf )
		    {
	    		for( int i = ptr; i < rendered_frames; i++ ) out[ i ] = render_buf[ i ];
		    }
		}
		else
		{
	    	    for( int i = ptr; i < rendered_frames; i++ ) out[ i ] += render_buf[ i ];
		}
	    }
	    else
	    {
		if( vol == 0 )
		{
		    renderbuf_pars->last_sample[ ch ] = 0;
		    if( empty_buf )
		    {
	    		for( int i = ptr; i < rendered_frames; i++ ) out[ i ] = 0;
		    }
		}
		else
		{
	    	    PS_STYPE2 vol2 = PS_NORM_STYPE( vol, norm ); 
		    renderbuf_pars->last_sample[ ch ] = PS_NORM_STYPE_MUL( render_buf[ rendered_frames - 1 ], vol2, norm );
		    if( empty_buf )
		    {
	    		for( int i = ptr; i < rendered_frames; i++ )
		    	    out[ i ] = PS_NORM_STYPE_MUL( render_buf[ i ], vol2, norm );
		    }
		    else
		    {
	    		for( int i = ptr; i < rendered_frames; i++ )
		    	    out[ i ] += PS_NORM_STYPE_MUL( render_buf[ i ], vol2, norm );
		    }
		}
	    }
	}
	if( empty_buf )
	{
	    for( int i = rendered_frames; i < out_frames; i++ ) out[ i ] = 0;
	}
    }
    retval = 1;
    if( empty_buf && volume == 0 && !vol_smoothing ) retval = 2;
    renderbuf_pars->start = false;
    renderbuf_pars->anticlick = false;
    renderbuf_pars->anticlick_counter = anticlick_counter;
    return retval;
}
