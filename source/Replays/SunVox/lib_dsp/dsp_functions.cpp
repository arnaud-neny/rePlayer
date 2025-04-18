/*
    dsp_functions.cpp
    This file is an independent part of the SunDog engine.
    (SunDog headers are not required)
    Copyright (C) 2008 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include <stdint.h>
#include <math.h>

#include "dsp.h"

#ifdef SUNDOG_TEST
#include "sundog.h"
#endif

template < typename T >
static void do_fft( uint32_t flags, T* fi, T* fr, int size )
{
    int size2 = size / 2;

    T isign = -1;
    if( flags & FFT_FLAG_INVERSE )
    {
	//instead of { for( int i = 0; i < size; i++ ) fi[ i ] = -fi[ i ]; }
	isign = 1;
    }

    //Bit-reversal permutation:
    for( int i = 1, j = size2; i < size - 1; i++ )
    {
	if( i < j )
	{
	    T tr = fr[ j ];
	    T ti = fi[ j ];
	    fr[ j ] = fr[ i ];
	    fi[ j ] = fi[ i ];
	    fr[ i ] = tr;
	    fi[ i ] = ti;
	}
	int k = size2;
	while( k <= j )
	{
	    j -= k;
	    k >>= 1;
	}
	j += k;
    }

    //Danielson-Lanczos section:
    int mmax = 1;
    int istep;
    while( mmax < size )
    {
	istep = mmax << 1; //istep = 2; mmax = 1; ... istep = size; mmax = size >> 1;
	T theta = isign * M_PI / mmax; //Initialize the trigonometric recurrence
	T wtemp = sin( 0.5 * theta );
	T wpr = -2.0 * wtemp * wtemp;
	T wpi = sin( theta );
	T wr = 1.0;
	T wi = 0.0;
	for( int m = 0; m < mmax; m++ ) //m = 0..1; 0..2; ... 0..size/2;
	{
	    for( int i = m; i < size; i += istep ) 
	    {
		int j = i + mmax;
		T tr = wr * fr[ j ] - wi * fi[ j ];
		T ti = wr * fi[ j ] + wi * fr[ j ];
		fr[ j ] = fr[ i ] - tr;
		fi[ j ] = fi[ i ] - ti;
		fr[ i ] += tr;
		fi[ i ] += ti;
	    }
	    //Complex: w += w * wp;
	    wtemp = wr;
	    wr += wtemp * wpr - wi * wpi;
	    wi += wtemp * wpi + wi * wpr;
	}
	mmax = istep;
    }
    /*
    This form:
	T theta = isign * M_PI / mmax;
	T wtemp = sin( 0.5 * theta );
	T wpr = -2.0 * wtemp * wtemp;
	T wpi = sin( theta );
	...
        wtemp = wr;
        wr += wtemp * wpr - wi * wpi;
        wi += wtemp * wpi + wi * wpr;
    is more accurate than:
	T theta = -isign * M_PI / mmax;
	T wpr = cos( theta );
	T wpi = -sin( theta );
	...
	T tr = wr;
	wr = tr * wpr - wi * wpi;
	wi = tr * wpi + wi * wpr;
    cos(theta) may be 1.0 at some very small theta values;
    to avoid this problem wpr is computed as cos(theta)-1 using the half angle formula from trigonometry;
    read more: https://stackoverflow.com/questions/2220879/c-numerical-recipies-fft
    */

    if( flags & FFT_FLAG_INVERSE )
    {
	for( int i = 0; i < size; i++ ) 
	{
	    fr[ i ] = fr[ i ] / size;
	    fi[ i ] = -fi[ i ] / size;
	}
    }
}

void fft( uint32_t flags, float* fi, float* fr, int size )
{
    do_fft( flags, fi, fr, size );
}

void fft( uint32_t flags, double* fi, double* fr, int size )
{
    do_fft( flags, fi, fr, size );
}

#ifdef SUNDOG_TEST
const int g_fft_test_size = 8;
float g_fft_test_im[ g_fft_test_size ] = {};
float g_fft_test_re[ g_fft_test_size ] = { 1, 1, 1, 1, 0, 0, 0, 0 };
float g_fft_test_im2[ g_fft_test_size ] =
{
    0,
    -0x1.3504f2p+1,
    0,
    -0x1.a82788p-2,
    0,
    0x1.a8279p-2,
    0,
    0x1.3504fp+1
};
float g_fft_test_re2[ g_fft_test_size ] = { 4, 1, 0, 1, 0, 1, 0, 1 };
float fft_test( void )
{
    float err = 0;
    float fft_im[ g_fft_test_size ];
    float fft_re[ g_fft_test_size ];
    for( int i = 0; i < g_fft_test_size; i++ )
    {
	fft_im[ i ] = g_fft_test_im[ i ];
	fft_re[ i ] = g_fft_test_re[ i ];
    }
    fft( 0, fft_im, fft_re, 8 );
    for( int i = 0; i < g_fft_test_size; i++ )
    {
	//slog( "%f %f - %f %f\n", fft_im[i], fft_re[i], g_fft_test_im2[i], g_fft_test_re2[i] );
	err += fabs( fft_im[ i ] - g_fft_test_im2[ i ] );
	err += fabs( fft_re[ i ] - g_fft_test_re2[ i ] );
    }
    fft( FFT_FLAG_INVERSE, fft_im, fft_re, 8 );
    for( int i = 0; i < g_fft_test_size; i++ )
    {
	//slog( "%.8f %.8f - %.8f %.8f\n", fft_im[i], fft_re[i], g_fft_test_im[i], g_fft_test_re[i] );
	err += fabs( fft_im[ i ] - g_fft_test_im[ i ] );
	err += fabs( fft_re[ i ] - g_fft_test_re[ i ] );
    }
    //13 feb 2023: err = 0.0000015050172806
    return err;
}
#endif

int dsp_curve( int x, dsp_curve_type type ) //Input: 0...32768; Output: 0...32768
{
    int y = x;
    switch( type )
    {
        case dsp_curve_type_exponential1:
            {
                int y2 = 32768 - x;
                y = 32768 - ( ( y2 * y2 ) >> 15 );
            }
            break;
        case dsp_curve_type_exponential2:
    	    y = ( x * x ) >> 15;
            break;
        case dsp_curve_type_spline:
            y = (int)( ( ( sin( ( (float)x / 32768.0F ) * M_PI - M_PI / 2.0F ) + 1.0F ) / 2.0F ) * 32768.0F );
            break;
        case dsp_curve_type_rectangular:
            if( x < 16384 ) y = 0; else y = 32768;
            break;
        default:
    	    break;
    }
    return y;
}

float dsp_window_function( float* dest, int size, dsp_win_fn type )
{
    int size2 = size - 1;
    switch( type )
    {
        case dsp_win_fn_triangular:
            //Triangular:
            for( int i = 0; i < size; i++ )
            {
                float v;
                if( i < size / 2 )
                    v = (float)i / size * 2;
                else
                    v = ( 1.0 - (float)i / size ) * 2;
                dest[ i ] = v;
            }
            break;
        case dsp_win_fn_sine:
            //Sine:
            for( int i = 0; i < size; i++ )
            {
                float v = sinf( M_PI * i / size2 );
                dest[ i ] = v;
            }
            break;
        case dsp_win_fn_hann:
            //Hann:
            for( int i = 0; i < size; i++ )
            {
                float v = sinf( M_PI * i / size2 );
                v *= v;
                dest[ i ] = v;
            }
            break;
        case dsp_win_fn_blackman:
            //Blackman:
            for( int i = 0; i < size; i++ )
            {
                float v = 0.42 - 0.5 * cosf( 2 * M_PI * i / size2 ) + 0.08 * cosf( 4 * M_PI * i / size2 );
                dest[ i ] = v;
            }
            break;
        case dsp_win_fn_nuttall:
            //Nuttall:
            for( int i = 0; i < size; i++ )
            {
                float v = ( 2.0 * M_PI * i ) / size2;
                v = 0.355768 - 0.487396 * cosf( v ) + 0.144232 * cosf( 2 * v ) - 0.012604 * cosf( 3 * v );
                dest[ i ] = v;
            }
            break;
        default: break;
    }
    float sum = 0;
    for( int i = 0; i < size; i++ )
    {
        sum += dest[ i ];
    }
    return (float)size / sum; //Amplitude correction
}
