#pragma once

//Half of sine:
extern uint8_t g_hsin_tab[ 256 ];

#define FFT_FLAG_INVERSE	1

void fft( uint32_t flags, float* fi, float* fr, int size );
void fft( uint32_t flags, double* fi, double* fr, int size );
float fft_test( void );
//frequency bins:
//[ 0 ] = 0 (DC)
//[ 1 ] = sample rate / 2 / size
//[ 2 ] = sample rate / 2 / size * 2
// ...
//[ size / 2 ] = sample rate / 2
// ...
//[ size - 1 ] = sample rate / 2 / size

enum dsp_curve_type
{
    dsp_curve_type_linear,
    dsp_curve_type_exponential1, 
    dsp_curve_type_exponential2, 
    dsp_curve_type_spline, 
    dsp_curve_type_rectangular,
    dsp_curve_types
};
int dsp_curve( int x, dsp_curve_type type ); //Input: 0...32768; Output: 0...32768

enum dsp_win_fn
{
    dsp_win_fn_rectangular = 0,
    dsp_win_fn_triangular,
    dsp_win_fn_sine,
    dsp_win_fn_hann,
    dsp_win_fn_blackman,
    dsp_win_fn_nuttall,
    dsp_win_fns
};
float dsp_window_function( float* dest, int size, dsp_win_fn type );

#define DSP_LINEAR_INTERP( Y0, Y1, X, X_ONE ) \
    ( ( (Y0) * ( (X_ONE) - (X) ) + (Y1) * (X) ) / (X_ONE) )

//Catmull-Rom cubic spline interpolation:
// y1 - current point;
// x - 0 ... 32767 (y1...y2)
inline int catmull_rom_spline_interp_int16( int y0, int y1, int y2, int y3, int x )
{
    x >>= 2;
    int a = ( 3 * ( y1 - y2 ) - y0 + y3 ) / 2;
    int b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
    int c2 = ( y2 - y0 ) / 2;
    return ( ( ( ( ( ( ( ( a * x ) >> 13 ) + b ) * x ) >> 13 ) + c2 ) * x ) >> 13 ) + y1;
}
// x - 0 ... 0.99999 (y1...y2)
inline float catmull_rom_spline_interp_float32( float y0, float y1, float y2, float y3, float x )
{
    float a = ( 3 * ( y1 - y2 ) - y0 + y3 ) / 2;
    float b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
    float c2 = ( y2 - y0 ) / 2;
    return ( ( a * x + b ) * x + c2 ) * x + y1;
}
