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

#include "psynth.h"
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_spectravoice_data
#define MODULE_HANDLER	psynth_spectravoice
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	32
#define MAX_HARMONICS	16
#define MAX_SAMPLES	1
struct gen_channel
{
    int		playing;
    uint   	id;
    int	    	vel;
    int	    	ptr_h;
    int	    	ptr_l;
    uint   	delta_h;
    uint   	delta_l;
    uint   	env_vol;
    int	    	sustain;
    psynth_renderbuf_pars       renderbuf_pars;
    PS_CTYPE   	local_pan;
};
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODE_CUBIC,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   		ctl_volume;
    PS_CTYPE   		ctl_pan;
    PS_CTYPE   		ctl_attack;
    PS_CTYPE   		ctl_release;
    PS_CTYPE   		ctl_channels;
    PS_CTYPE   		ctl_mode;
    PS_CTYPE   		ctl_sustain;
    PS_CTYPE   		ctl_sample_size;
    PS_CTYPE   		ctl_harm;
    PS_CTYPE   		ctl_freq;
    PS_CTYPE   		ctl_freq_vol;
    PS_CTYPE   		ctl_freq_band;
    PS_CTYPE   		ctl_freq_type;
    PS_CTYPE   		ctl_samples;
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    gen_channel		channels[ MAX_CHANNELS ];
    bool    		no_active_channels;
    int	    		search_ptr;
    uint16_t* 		freq;
    uint8_t*  		freq_vol;
    uint8_t*  		freq_band;
    uint8_t*  		freq_type;
    int16_t*  		samples[ MAX_SAMPLES ];
    int	    		sample_size;
    int	    		note_offset;
    int     		base_pitch;
    smutex 		render_sound_mutex;
    sthread 		recalc_thread;
    volatile bool	recalc_thread_stop;
    volatile int	recalc_req;
    volatile int	recalc_req_answer;
    bool		correct_fft;
    psmoother_coefs     smoother_coefs;
#ifdef SUNVOX_GUI
    uint8_t		preview[ 256 ];
    window_manager*	wm;
#endif
};
#define APPLY_ENV_VOLUME \
    s *= ( env_vol >> 15 ); \
    s /= 32768;
#define ENV_CONTROL_REPLACE \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); if( sustain_enabled == 0 ) sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; break; } \
    }
static const int16_t g_fft_rsin[ 128 ] = {
0, 27572, 29794, 4624, -24798, -31421, -9155, 21527, 32418, 13503, -17825, -32766, -17581, 13767, 32459, 21307, 
-9433, -31502, -24607, 4911, 29914, 27414, -290, -27728, -29673, -4336, 24986, 31337, 8876, -21745, -32374, -13239, 
18068, 32764, 17336, -14030, -32497, -21086, 9711, 31580, 24415, -5197, -30031, -27254, 580, 27881, 29548, 4049, 
-25173, -31251, -8597, 21961, 32328, 12973, -18309, -32758, -17089, 14291, 32533, 20863, -9987, -31656, -24220, 5483, 
30146, 27092, -870, -28032, -29422, -3761, 25358, 31163, 8317, -22175, -32280, -12706, 18549, 32751, 16841, -14552, 
-32566, -20639, 10263, 31730, 24024, -5769, -30258, -26928, 1159, 28181, 29293, 3472, -25540, -31072, -8036, 22388, 
32229, 12438, -18788, -32741, -16592, 14811, 32597, 20413, -10538, -31801, -23826, 6054, 30369, 26762, -1449, -28328, 
-29162, -3184, 25721, 30979, 7754, -22599, -32175, -12169, 19024, 32728, 16341, -15069, -32625, -20185, 10812, 31870
};
static const int16_t g_fft_rcos[ 128 ] = {
32767, 17704, -13635, -32439, -21417, 9294, 31461, 24703, -4767, -29855, -27493, 145, 27650, 29734, 4480, -24892, 
-31379, -9016, 21636, 32396, 13371, -17947, -32765, -17459, 13899, 32478, 21197, -9572, -31541, -24511, 5054, 29973, 
27334, -435, -27805, -29611, -4192, 25080, 31294, 8737, -21853, -32352, -13106, 18189, 32761, 17213, -14161, -32515, 
-20975, 9849, 31619, 24318, -5340, -30089, -27173, 725, 27957, 29485, 3905, -25265, -31207, -8457, 22068, 32304, 
12839, -18429, -32755, -16965, 14422, 32550, 20751, -10125, -31693, -24122, 5626, 30203, 27010, -1014, -28107, -29358, 
-3617, 25449, 31118, 8176, -22282, -32255, -12572, 18669, 32746, 16716, -14682, -32582, -20526, 10401, 31766, 23925, 
-5912, -30314, -26845, 1304, 28255, 29228, 3328, -25631, -31026, -7895, 22494, 32202, 12304, -18906, -32734, -16466, 
14940, 32611, 20299, -10675, -31836, -23726, 6197, 30423, 26678, -1594, -28401, -29096, -3039, 25811, 30931, 7613
};
static int spectravoice_get_base_note( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return 0;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return 0;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    return (int)( ( (float)( 7680 - data->base_pitch ) / 64.0F ) + 0.5F );
}
static void spectravoice_get_base_pitch( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    int base_freq = 44100;
    int dist = 10000000;
    int pitch = 0;
    for( int p = 0; p < 7680; p++ )
    {
        int f;
        PSYNTH_GET_FREQ( g_linear_freq_tab, f, p );
        int d = f - base_freq;
        if( d < 0 ) d = -d;
        if( d <= dist )
        {
            dist = d;
            pitch = p;
        }
        else break;
    }
    data->base_pitch = pitch;
}
static void set_number_of_outputs( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
    {
        if( psynth_get_number_of_outputs( mod ) != 1 )
        {
    	    psynth_set_number_of_outputs( 1, mod_num, pnet );
	}
    }
    else
    {
        if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
        {
    	    psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
	}
    }
}
static void fft_with_normalization( int16_t* result, float* fi, float* fr, int fft_size )
{
    int i;
    fft( 0, fi, fr, fft_size );
    float max = 0;
    for( i = 0; i < fft_size; i++ )
    {
	float v = fr[ i ];
	if( v < 0 ) v = -v;
	if( v > max ) max = v;
    }
    if( max == 0 )
    {
	for( i = 0; i < fft_size; i++ ) result[ i ] = 0;
	return;
    }
    float coef = 1.0F / max;
    for( i = 0; i < fft_size; i++ )
    {
	fr[ i ] = fr[ i ] * coef;
    }
    for( i = 0; i < fft_size; i++ )
    {
	int ires = (int)( fr[ i ] * 32768 );
	if( ires > 32767 ) ires = 32767;
	if( ires < -32767 ) ires = -32767;
	result[ i ] = (int16_t)ires;
    }
}
static void recalc_samples( bool lock_sound_stream, MODULE_DATA* data, int mod_num, psynth_net* pnet )
{
    data->freq = (uint16_t*)psynth_get_chunk_data( mod_num, 0, pnet );
    data->freq_vol = (uint8_t*)psynth_get_chunk_data( mod_num, 1, pnet );
    data->freq_band = (uint8_t*)psynth_get_chunk_data( mod_num, 2, pnet );
    data->freq_type = (uint8_t*)psynth_get_chunk_data( mod_num, 3, pnet );
    if( lock_sound_stream ) 
    {
        int mutex_res = smutex_lock( &data->render_sound_mutex );
	if( mutex_res != 0 )
	{
	    slog( "SV recalc_samples: mutex lock error %d\n", mutex_res );
	    return;
	}
    }
    int prev_ss = data->sample_size;
    data->sample_size = 4096 << data->ctl_sample_size;
    if( data->sample_size != prev_ss )
    {
	smem_free( data->samples[ 0 ] );
	data->samples[ 0 ] = (int16_t*)smem_new( data->sample_size * sizeof( int16_t ) );
	smem_zero( data->samples[ 0 ] );
    }
    if( lock_sound_stream ) 
    {
        smutex_unlock( &data->render_sound_mutex );
    }
    int16_t* smp = data->samples[ 0 ];
    float* hr = (float*)smem_new( data->sample_size * sizeof( float ) );
    float* hi = (float*)smem_new( data->sample_size * sizeof( float ) );
    uint8_t* distribution = (uint8_t*)smem_new( 256 );
    smem_zero( hr );
    smem_zero( hi );
#ifdef SUNVOX_GUI
    float* preview_buf = (float*)smem_new( ( data->sample_size / 2 ) * sizeof( float ) );
    smem_zero( preview_buf );
#endif
    uint32_t rseed = 0;
    for( int n = 0; n < MAX_HARMONICS; n++ )
    {
	int clones = 1;
	int harmonic_vol = data->freq_vol[ n ];
	if( harmonic_vol == 0 ) continue;
	int bandwidth = data->freq_band[ n ];
	int bandwidth_add = 0;
	float vol_mul = 1;
	int freq_ptr = (int)( ( (uint64_t)data->freq[ n ] * (uint64_t)( data->sample_size / 2 ) ) / (uint64_t)22050 );
	LIMIT_NUM( freq_ptr, 0, data->sample_size / 2 );
	if( data->freq_type[ n ] == 18 )
	{
	    uint32_t rseed2 = n + bandwidth;
	    for( int i = 0; i < data->sample_size / 2; i++ )
	    {
		int vol = 0;
		if( (int)( pseudo_random( &rseed2 ) & 255 ) >= ( 245 + data->ctl_sample_size*2 ) )
		{
		    vol = pseudo_random( &rseed2 ) & 255;
		    vol = ( vol * vol ) / 255;
		}
		int sin_ptr = ( i - freq_ptr ) >> ( 4 + data->ctl_sample_size );
		sin_ptr += 128;
		if( sin_ptr < 0 || sin_ptr > 255 )
		    vol = 0;
		else
		    vol = vol * g_hsin_tab[ sin_ptr ] / 255;
		int amp = ( harmonic_vol * vol ) / 256;
		int phase = pseudo_random( &rseed );
		hi[ i ] += (float)( ( g_fft_rsin[ phase&127 ] * amp ) / 256 ) / 32767;
		hr[ i ] += (float)( ( g_fft_rcos[ phase&127 ] * amp ) / 256 ) / 32767;
	    }
	    int fadein = data->sample_size / 2 / 32;
	    for( int i = 0; i < fadein; i++ )
	    {
		float v = (float)i / fadein;
		v *= v;
		hi[ i ] *= v;
		hr[ i ] *= v;
	    }
	    continue;
	}
	switch( data->freq_type[ n ] )
	{
	    case 1:
		for( int a = 0; a < 256; a++ ) distribution[ a ] = 255;
		break;
	    case 0:
	    case 2:
	    case 3:
	    case 4:
	    case 5:
		for( int a = 0; a < 256; a++ ) distribution[ a ] = g_hsin_tab[ a ];
		break;
	    case 6:
		for( int a = 0; a < 128; a++ ) 
		{
		    distribution[ ( a + 64 ) & 255 ] = g_hsin_tab[ a * 2 ] / 2 + 127;
		    distribution[ ( ( a + 128 ) + 64 ) & 255 ] = -( g_hsin_tab[ a * 2 ] / 2 ) + 127;
		}
		break;
	    case 7:
		{
		    uint32_t rseed2 = bandwidth;
		    for( int a = 0; a < 256; a++ ) distribution[ a ] = 0;
    		    for( int a = 0; a < 8; a++ )
		    {
			distribution[ pseudo_random( &rseed2 ) & 255 ] = 255;
		    }
		    distribution[ 126 ] = 128;
		    distribution[ 127 ] = 255;
		    distribution[ 128 ] = 128;
		}
		break;
	    case 8:
		for( int a = 0; a < 128; a++ )
		{
		    distribution[ a ] = a * 2;
		    distribution[ a + 128 ] = ( 127 - a ) * 2;
		} 
		break;
	    case 9:
	    case 10: 
	    case 11: 
	    case 12: 
	    case 13: 
	    case 14: 
	    case 15: 
	    case 16: 
	    case 17: 
		for( int a = 0; a < 128; a++ )
		{
		    distribution[ a ] = a * 2;
		    distribution[ a + 128 ] = ( 127 - a ) * 2;
		}
		for( int a = 0; a < 256; a++ )
		{
		    int v = distribution[ a ];
		    v = ( v * v ) >> 8;
		    v = ( v * v ) >> 8;
		    distribution[ a ] = (uint8_t)v;
		}
		break;
	}
	switch( data->freq_type[ n ] )
        {
	    case 3: bandwidth_add = 2; break;
	    case 4: bandwidth_add = 4; break;
	    case 5: bandwidth_add = 8; break;
	}
	switch( data->freq_type[ n ] )
        {
    	    case 2:
    	    case 3:
    	    case 4:
    	    case 5:
    		clones = 8;
    		break;
    	    case 10:
    		clones = 16;
    		vol_mul = 0.8;
    		break;
    	    case 11:
    		clones = 16;
    		vol_mul = 0.6;
    		break;
    	    case 12:
    		clones = 16;
    		vol_mul = 0.5;
    		break;
    	    case 13:
    		clones = 16;
    		vol_mul = 0.25;
    		break;
    	    case 14:
    		clones = 16;
    		vol_mul = 0.8;
    		bandwidth_add = 8;
    		break;
    	    case 15:
    		clones = 16;
    		vol_mul = 0.6;
    		bandwidth_add = 9;
    		break;
    	    case 16:
    		clones = 16;
    		vol_mul = 0.5;
    		bandwidth_add = 10;
    		break;
    	    case 17:
    		clones = 16;
    		vol_mul = 0.25;
    		bandwidth_add = 11;
    		break;
    	}
	for( int cc = 0; cc < clones; cc++ )
	{
	    int ptr;
	    if( pnet->base_host_version < 0x01090000 )
		ptr = ( data->freq[ n ] * data->sample_size ) / 22050;
	    else
		ptr = (int)( ( (uint64_t)data->freq[ n ] * (uint64_t)data->sample_size ) / (uint64_t)22050 );
	    if( ptr >= data->sample_size ) ptr = data->sample_size - 1;
	    if( cc > 0 ) 
	    {
		ptr += ptr * cc;
		bandwidth += bandwidth_add;
		if( vol_mul != 1 )
		{
		    harmonic_vol = (int)( (float)harmonic_vol * vol_mul );
		}
	    }
	    ptr /= 2;
	    int bw = ( ( bandwidth + 1 ) * ( data->sample_size / 16 ) ) / 256;
	    bw /= 2;
	    if( bw <= 0 ) bw = 1;
	    int p1 = ptr - bw;
	    int p2 = ptr + bw;
	    if( p1 < 0 ) p1 = 0;
	    if( p2 >= data->sample_size / 2 ) p2 = data->sample_size / 2 - 1;
	    if( harmonic_vol )
	    {
		for( int i = p1; i <= p2; i++ )
		{
		    int d = i - ptr;
		    d *= ( (128<<14) / bw );
		    d >>= 14;
		    if( d < 128 && d >= -128 ) 
		    {
			int vol = distribution[ 128 + d ];
			int amp = ( harmonic_vol * vol ) / 256;
			int phase = pseudo_random( &rseed );
			hi[ i ] += (float)( ( g_fft_rsin[ phase&127 ] * amp ) / 256 ) / 32767;
			hr[ i ] += (float)( ( g_fft_rcos[ phase&127 ] * amp ) / 256 ) / 32767;
		    }
		}
	    }
	}
    }
#ifdef SUNVOX_GUI
    float max = 0;
    for( int i = 0; i < data->sample_size / 2; i++ )
    {
	float v = sqrt( hi[ i ] * hi[ i ] + hr[ i ] * hr[ i ] );
	if( v > max ) max = v;
	preview_buf[ i ] = v;
    }
    int s = 0;
    int tt = sizeof( data->preview );
    while( tt < data->sample_size / 2 )
    {
	s++;
	tt <<= 1;
    }
    for( int i = 0; i < (int)sizeof( data->preview ); i++ ) data->preview[ i ] = 0;
    if( max != 0 )
    {
	for( int i = 0; i < data->sample_size / 2; i++ )
	{
	    int v = preview_buf[ i ] / max * 255;
	    if( v > data->preview[ i >> s ] ) data->preview[ i >> s ] = (uint8_t)v;
	}
    }
#endif
    if( data->correct_fft )
    {
	for( int i = 0; i < data->sample_size / 2 - 1; i++ )
	{ 
	    hr[ data->sample_size - 1 - i ] = hr[ i + 1 ];
	    hi[ data->sample_size - 1 - i ] = -hi[ i + 1 ];
	}
    }
    else
    {
	for( int i = 0; i < data->sample_size / 2; i++ )
	{ 
	    hr[ data->sample_size - 1 - i ] = -hr[ i ];
	    hi[ data->sample_size - 1 - i ] = -hi[ i ];
	}
    }
    hi[ 0 ] = 0; 
    hr[ 0 ] = 0;
    fft_with_normalization( smp, hi, hr, data->sample_size );
    smem_free( hr );
    smem_free( hi );
    smem_free( distribution );
#ifdef SUNVOX_GUI
    smem_free( preview_buf );
#endif
}
void* spectravoice_recalc_proc( void* user_data )
{
    MODULE_DATA* data = (MODULE_DATA*)user_data;
    psynth_module* mod;
    if( data->mod_num >= 0 )
        mod = &data->pnet->mods[ data->mod_num ];
    else
	return 0;
    int wait_step = 100; 
    int wait_max = 8 * 1000; 
    int t = 0;
    while( !data->recalc_thread_stop )
    {
	if( data->recalc_req != data->recalc_req_answer )
	{
	    data->recalc_req_answer = data->recalc_req;
	    recalc_samples( true, data->module_data, data->mod_num, data->pnet );
	    mod->draw_request++;
	    t = 0;
	}
	else
	{
	    if( t >= wait_max ) break;
	    stime_sleep( wait_step );
	    t += wait_step;
	}
    }
    return 0;
}
#ifdef SUNVOX_GUI
struct spectravoice_visual_data
{
    WINDOWPTR		win;
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    WINDOWPTR		render_button;
};
int spectravoice_render_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    spectravoice_visual_data* data = (spectravoice_visual_data*)user_data;
    recalc_samples( true, data->module_data, data->mod_num, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
int spectravoice_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    spectravoice_visual_data* data = (spectravoice_visual_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( spectravoice_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->win = win;
	    data->pnet = (psynth_net*)win->host;
	    data->render_button = 0;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		int preview_size = sizeof( data->module_data->preview );
		COLOR preview_color = blend( wm->color2, win->color, 150 );
		int preview_start_x = 0;
                int preview_x = preview_start_x << 15;
                int preview_y = 0;
                int preview_xsize = win->xsize;
                int preview_ysize = wm->scrollbar_size * 6;
                if( preview_ysize > win->ysize - ( preview_y + wm->interelement_space ) )
                    preview_ysize = win->ysize - ( preview_y + wm->interelement_space );
                int xd = ( preview_xsize << 15 ) / preview_size;
                if( preview_ysize > 0 && preview_xsize > 0 )
                {
                    for( int i = 0; i < preview_size; i++ )
                    {
                        int v = data->module_data->preview[ i ];
                        int ysize = ( preview_ysize * v ) / 255;
                        draw_frect( preview_x >> 15, preview_y + preview_ysize - ysize, ( ( preview_x + xd ) >> 15 ) - ( preview_x >> 15 ), ysize, preview_color, wm );
                        preview_x += xd;
                    }
                }
                psynth_draw_simple_grid( preview_start_x, preview_y, preview_xsize, preview_ysize, win );
                int x = ( data->module_data->ctl_freq * preview_xsize ) / 22050;
                wm->cur_opacity = 200;
                draw_line( x, preview_y, x, preview_y + preview_ysize - 1, wm->color3, wm );
                wm->cur_opacity = 255;
                char ts[ 512 ];
                char ts2[ 3 ];
                uint base_note = spectravoice_get_base_note( data->mod_num, data->pnet ) - data->pnet->mods[ data->mod_num ].relative_note;
                int ft = data->pnet->mods[ data->mod_num ].finetune;
                if( ft >= 128 ) base_note--;
                if( ft < -128 ) base_note++;
                ts2[ 0 ] = g_n2c[ base_note % 12 ];
                ts2[ 1 ] = '?';
                int oct = base_note / 12;
                if( oct >= 0 && oct < 10 ) ts2[ 1 ] = '0' + oct;
                ts2[ 2 ] = 0;
                wm->cur_font_color = wm->color2;
                int y = 0;
                if( data->render_button ) y = wm->text_ysize + wm->interelement_space;
                sprintf( ts, "%s: %s\n44100 %s", ps_get_string( STR_PS_BASE_NOTE ), ts2, wm_get_string( STR_WM_HZ ) );
                draw_string( ts, 0, y, wm );
                sprintf( ts, "%s: %d", ps_get_string( STR_PS_SPECTRUM_SIZE ), 4096 << data->module_data->ctl_sample_size );
                draw_string( ts, 0, y + char_y_size( wm ) * 2, wm );
                wbd_draw( wm );
                wbd_unlock( wm );
                retval = 1;
            }
	    break;
    }
    return retval;
}
#endif
PS_RETTYPE MODULE_HANDLER( 
    PSYNTH_MODULE_HANDLER_PARAMETERS
    )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
        mod = &pnet->mods[ mod_num ];
        data = (MODULE_DATA*)mod->data_ptr;
    }
    PS_RETTYPE retval = 0;
    switch( event->command )
    {
	case PS_CMD_GET_DATA_SIZE:
	    retval = sizeof( MODULE_DATA );
	    break;
	case PS_CMD_GET_NAME:
	    retval = (PS_RETTYPE)"SpectraVoice";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"SpectraVoice выдает звук с заданным спектром. Спектр можно представить, как график зависимости амплитуды (ось Y) от частоты (ось X). На этом графике можно расположить 16 гармоник, для каждой указав положение, амплитуду, форму, ширину.\nЛокальные контроллеры: Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)"SpectraVoice synthesizes sound with a complex spectrum. The spectrum is a graph where the X-axis is the frequency and the Y-axis is the amplitude (loudness). You can place 16 harmonics on this graph, specifying the position, amplitude, shape and width for each.\nLocal controllers: Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF00E8";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;
	case PS_CMD_INIT:
	    data->module_data = data;
	    data->mod_num = mod_num;
	    data->pnet = pnet;
	    psynth_resize_ctls_storage( mod_num, 13, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 128, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 1, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), ps_get_string( STR_PS_SEC256 ), 0, 512, 1, 0, &data->ctl_attack, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), ps_get_string( STR_PS_SEC256 ), 0, 512, 512, 0, &data->ctl_release, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono;HQspline", 0, MODES - 1, MODE_CUBIC, 1, &data->ctl_mode, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_sustain, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SPECTRUM_RESOLUTION ), "", 0, 5, 1, 1, &data->ctl_sample_size, -5, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HARMONIC ), "", 0, MAX_HARMONICS-1, 0, 1, &data->ctl_harm, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HARMONIC_FREQ ), ps_get_string( STR_PS_HZ ), 0, 22050, 1098, 1, &data->ctl_freq, 1098, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HARMONIC_VOLUME ), "", 0, 255, 255, 1, &data->ctl_freq_vol, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HARMONIC_WIDTH ), "", 0, 255, 3, 1, &data->ctl_freq_band, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HARMONIC_TYPE ), ps_get_string( STR_PS_HARMONIC_TYPES ), 0, 18, 0, 1, &data->ctl_freq_type, -1, 3, pnet );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr_l = 0;
		data->channels[ c ].ptr_h = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    for( int s = 0; s < MAX_SAMPLES; s++ )
	    {
		data->samples[ s ] = 0;
	    }
	    data->sample_size = 0;
	    psynth_new_chunk( mod_num, 0, MAX_HARMONICS * sizeof( uint16_t ), 0, 0, pnet );
	    psynth_new_chunk( mod_num, 1, MAX_HARMONICS, 0, 0, pnet );
	    psynth_new_chunk( mod_num, 2, MAX_HARMONICS, 0, 0, pnet );
	    psynth_new_chunk( mod_num, 3, MAX_HARMONICS, 0, 0, pnet );
	    data->freq = (uint16_t*)psynth_get_chunk_data( mod_num, 0, pnet );
	    data->freq_vol = (uint8_t*)psynth_get_chunk_data( mod_num, 1, pnet );
	    data->freq_band = (uint8_t*)psynth_get_chunk_data( mod_num, 2, pnet );
	    data->freq_type = (uint8_t*)psynth_get_chunk_data( mod_num, 3, pnet );
	    for( int h = 0; h < MAX_HARMONICS; h++ )
	    {
		data->freq[ h ] = 0;
		data->freq_vol[ h ] = 0;
		data->freq_band[ h ] = 0;
		data->freq_type[ h ] = 0;
	    }
	    data->freq[ 0 ] = data->ctl_freq;
	    data->freq_vol[ 0 ] = data->ctl_freq_vol;
	    data->freq_band[ 0 ] = data->ctl_freq_band;
	    data->freq_type[ 0 ] = data->ctl_freq_type;
	    data->note_offset = 0;
	    data->search_ptr = 0;
	    psmoother_init( &data->smoother_coefs, 100, pnet->sampling_freq );
	    spectravoice_get_base_pitch( mod_num, pnet );
	    for( int p = 0; p < MODULE_OUTPUTS; p++ ) psynth_get_temp_buf( mod_num, pnet, p ); 
#ifdef SUNVOX_GUI
	    {
		data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "Spectravoice GUI", 0, 0, 10, 10, wm->color1, 0, pnet, spectravoice_visual_handler, wm );
		    mod->visual_min_ysize = 0;
		    spectravoice_visual_data* sv_data = (spectravoice_visual_data*)mod->visual->data;
		    sv_data->module_data = data;
		    sv_data->mod_num = mod_num;
		    sv_data->pnet = pnet;
		}
	    }
#endif
	    smutex_init( &data->render_sound_mutex, 0 );
	    sthread_clean( &data->recalc_thread );
	    data->recalc_thread_stop = false;
	    data->recalc_req = 0;
	    data->recalc_req_answer = 0;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
#if defined(PS_STYPE_INT16) && CPUMARK < 10
	    if( g_loading_builtin_sv_template )
	    {
		if( data->ctl_sample_size >= 5 )
		    data->ctl_sample_size = 2;
		else
		    data->ctl_sample_size = 1;
	    }
#endif
            recalc_samples( false, data, mod_num, pnet );
	    set_number_of_outputs( mod_num, pnet );
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		if( data->no_active_channels ) break;
		if( smutex_trylock( &data->render_sound_mutex ) != 0 )
		    break; 
		int outputs_num = psynth_get_number_of_outputs( mod );
		bool no_env = false;
		int attack_delta = 1 << 30;
		int release_delta = 1 << 30;
		if( data->ctl_attack == 0 && data->ctl_release == 0 )
		    no_env = true;
		else
		{
		    int attack_len = ( pnet->sampling_freq * data->ctl_attack ) / 256;
		    int release_len = ( pnet->sampling_freq * data->ctl_release ) / 256;
		    if( pnet->base_host_version < 0x01090600 ) { attack_len &= ~1023; release_len &= ~1023; }
		    if( attack_len ) attack_delta = ( 1 << 30 ) / attack_len;
		    if( release_len ) release_delta = ( 1 << 30 ) / release_len;
		}
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
                    if( !chan->playing ) continue;
                    PS_STYPE* render_bufs[ 2 ];
                    render_bufs[ 0 ] = NULL;
                    render_bufs[ 1 ] = NULL;
                    if( outputs_num == 1 )
                	render_bufs[ 0 ] = PSYNTH_GET_RENDERBUF( retval, outputs, outputs_num, offset );
            	    else
            	    {
                	render_bufs[ 0 ] = PSYNTH_GET_RENDERBUF2( retval, outputs, outputs_num, offset, 0 );
                	render_bufs[ 1 ] = PSYNTH_GET_RENDERBUF2( retval, outputs, outputs_num, offset, 1 );
            	    }
		    data->no_active_channels = 0;
		    uint delta_h = chan->delta_h;
		    uint delta_l = chan->delta_l;
		    int sustain = chan->sustain;
		    int ptr_h = 0;
		    int ptr_l = 0;
		    int playing = 0;
		    int sustain_enabled = data->ctl_sustain;
		    uint env_vol = 0;
		    int rendered_frames = 0;
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
		        PS_STYPE* out = render_bufs[ ch ];
		        sustain = chan->sustain;
		        ptr_h = chan->ptr_h;
		        ptr_l = chan->ptr_l;
		        env_vol = chan->env_vol;
		        playing = chan->playing;
			int16_t* smp = NULL;
			smp = data->samples[ 0 ];
			if( smp == NULL ) break;
			int sample_size_mask = data->sample_size - 1;
			PS_STYPE2 res;
			int poff = 0;
			if( ch == 1 ) poff = data->sample_size / 2;
			if( no_env )
			{
			    if( sustain_enabled == 0 ) sustain = 0;
			    if( sustain == 0 ) { playing = 0; break; }
			    rendered_frames = frames;
			}
			if( data->ctl_mode == MODE_LQ_MONO || data->ctl_mode == MODE_LQ )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				int s = smp[ (ptr_h+poff) & sample_size_mask ];
				PS_INT16_TO_STYPE( res, s );
				out[ i ] = (PS_STYPE)res;
				PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
			    }
			}
#ifdef PS_STYPE_FLOATINGPOINT
			if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_HQ )
#else
			else
#endif
			{
			    for( int i = 0; i < frames; i++ )
			    {
			        int s = smp[ (ptr_h+poff) & sample_size_mask ] * (32767-(ptr_l>>1));
				s += smp[ (ptr_h+poff+1) & sample_size_mask ] * (ptr_l>>1);
				s >>= 15;
				PS_INT16_TO_STYPE( res, s );
				out[ i ] = (PS_STYPE)res;
				PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
			    }
			}
#ifdef PS_STYPE_FLOATINGPOINT
			if( data->ctl_mode == MODE_CUBIC )
			{
			    for( int i = 0; i < frames; i++ )
			    {
			        int y0 = smp[ (ptr_h+poff-1) & sample_size_mask ];
			        int y1 = smp[ (ptr_h+poff) & sample_size_mask ];
			        int y2 = smp[ (ptr_h+poff+1) & sample_size_mask ];
			        int y3 = smp[ (ptr_h+poff+2) & sample_size_mask ];
			        int s = catmull_rom_spline_interp_int16( y0, y1, y2, y3, (ptr_l>>1) );
			        PS_INT16_TO_STYPE( res, s );
			        out[ i ] = (PS_STYPE)res;
			        PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
			    }
			}
#endif
			if( !no_env )
			{
			    int i = 0;
			    if( sustain )
			    {
			        if( sustain_enabled && env_vol == ( 1 << 30 ) )
			    	    i = frames; 
				else
				    for( ; i < frames; i++ )
				    {
					out[ i ] = ( (PS_STYPE2)out[ i ] * ( env_vol >> 15 ) ) / 32768;
					env_vol += attack_delta;
			    		if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); if( sustain_enabled == 0 ) { sustain = 0; i++; break; } }
				    }
			    }
			    if( sustain == 0 )
			    {
			        for( ; i < frames; i++ )
			        {
			    	    out[ i ] = ( (PS_STYPE2)out[ i ] * ( env_vol >> 15 ) ) / 32768;
			    	    env_vol -= release_delta;
		    		    if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; i++; break; }
				}
			    }
			    rendered_frames = i;
			}
		    } 
		    chan->ptr_h = ptr_h;
		    chan->ptr_l = ptr_l;
		    chan->delta_h = delta_h;
		    chan->delta_l = delta_l;
		    chan->env_vol = env_vol;
		    chan->sustain = sustain;
		    chan->playing = playing;
		    if( !playing ) chan->id = ~0;
		    int local_vol = ( data->ctl_volume * chan->vel ) / 2;
		    int local_pan = ( data->ctl_pan + ( chan->local_pan - 128 ) ) * 256;
		    retval = psynth_renderbuf2output( 
			retval, outputs, outputs_num, offset, frames, render_bufs[ 0 ], render_bufs[ 1 ], rendered_frames, 
			local_vol, local_pan,
			&chan->renderbuf_pars, &data->smoother_coefs, 
			pnet->sampling_freq );
		} 
		smutex_unlock( &data->render_sound_mutex );
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c;
		if( data->no_active_channels == 0 )
		{
		    for( c = 0; c < MAX_CHANNELS; c++ )
		    {
			if( data->channels[ c ].id == event->id )
			{
			    if( data->channels[ c ].sustain )
			    {
				data->channels[ c ].sustain = 0;
				data->channels[ c ].id = ~0;
			    }
			    else 
			    {
				data->channels[ c ].id = ~0;
			    }
			    break;
			}
		    }
		    for( c = 0; c < data->ctl_channels; c++ )
		    {
			if( data->channels[ data->search_ptr ].playing == 0 ) break;
			data->search_ptr++;
			if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		    if( c == data->ctl_channels )
		    {
			data->search_ptr++;
			if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		}
		else 
		{
		    data->search_ptr = 0;
		}
		uint delta_h, delta_l;
		int freq;
		PSYNTH_GET_FREQ( g_linear_freq_tab, freq, event->note.pitch / 4 );
		PSYNTH_GET_DELTA( pnet->sampling_freq, freq, delta_h, delta_l );
		c = data->search_ptr;
		gen_channel* chan = &data->channels[ c ];
		chan->renderbuf_pars.start = true;
		if( chan->playing && data->ctl_attack ) chan->renderbuf_pars.anticlick = true;
		chan->playing = 1;
		chan->vel = event->note.velocity;
		chan->delta_h = delta_h;
		chan->delta_l = delta_l;
		chan->ptr_h = ( data->note_offset & (data->sample_size-1) );
		data->note_offset += 333;
		chan->ptr_l = 0;
		chan->id = event->id;
		chan->env_vol = 0;
		chan->sustain = 1;
		data->channels[ c ].local_pan = 128;
		data->no_active_channels = 0;
		retval = 1;
	    }
	    break;
    	case PS_CMD_SET_FREQ:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    uint delta_h, delta_l;
		    int freq;
		    PSYNTH_GET_FREQ( g_linear_freq_tab, freq, event->note.pitch / 4 );
		    PSYNTH_GET_DELTA( pnet->sampling_freq, freq, delta_h, delta_l );
		    data->channels[ c ].delta_h = delta_h;
		    data->channels[ c ].delta_l = delta_l;
		    data->channels[ c ].vel = event->note.velocity;
		    break;
		}
	    }
	    retval = 1;
	    break;
    	case PS_CMD_SET_VELOCITY:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    data->channels[ c ].vel = event->note.velocity;
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    data->channels[ c ].sustain = 0;
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	    if( data->no_active_channels == 0 )
	    {
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    if( data->channels[ c ].id == event->id )
		    {
			switch( event->controller.ctl_num )
			{
			    case 1:
				data->channels[ c ].local_pan = event->controller.ctl_val >> 7;
				retval = 1;
				break;
			}
			break;
		    }
		}
		if( retval ) break;
	    }
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    {
		bool recalc = false;
		switch( event->controller.ctl_num )
		{
		    case 5:
			psynth_set_ctl2( mod, event );
			set_number_of_outputs( mod_num, pnet );
			retval = 1;
			break;
		    case 7:
			recalc = true;
			break;
		    case 8:
			data->freq = (uint16_t*)psynth_get_chunk_data( mod, 0 );
			data->freq_vol = (uint8_t*)psynth_get_chunk_data( mod, 1 );
			data->freq_band = (uint8_t*)psynth_get_chunk_data( mod, 2 );
			data->freq_type = (uint8_t*)psynth_get_chunk_data( mod, 3 );
			data->ctl_harm = event->controller.ctl_val;
			if( data->ctl_harm < 0 ) data->ctl_harm = 0;
			if( data->ctl_harm >= MAX_HARMONICS ) data->ctl_harm = MAX_HARMONICS - 1;
			data->ctl_freq = data->freq[ data->ctl_harm ];
			data->ctl_freq_vol = data->freq_vol[ data->ctl_harm ];
			data->ctl_freq_band = data->freq_band[ data->ctl_harm ];
			data->ctl_freq_type = data->freq_type[ data->ctl_harm ];
			mod->draw_request++;
			retval = 1;
			break;
		    case 9:
			data->freq = (uint16_t*)psynth_get_chunk_data( mod, 0 );
			data->freq[ data->ctl_harm ] = event->controller.ctl_val;
			recalc = true;
			break;
		    case 10:
			data->freq_vol = (uint8_t*)psynth_get_chunk_data( mod, 1 );
			data->freq_vol[ data->ctl_harm ] = event->controller.ctl_val;
			recalc = true;
			break;
	    	    case 11:
			data->freq_band = (uint8_t*)psynth_get_chunk_data( mod, 2 );
			data->freq_band[ data->ctl_harm ] = event->controller.ctl_val;
			recalc = true;
			break;
    		    case 12:
			data->freq_type = (uint8_t*)psynth_get_chunk_data( mod, 3 );
			data->freq_type[ data->ctl_harm ] = event->controller.ctl_val;
			recalc = true;
			break;
		    case 126:
			data->correct_fft = event->controller.ctl_val != 0;
			recalc = true;
			break;
		}
		if( recalc )
		{
		    data->recalc_req++;
		    sundog_engine* sd = nullptr; GET_SD_FROM_PSYNTH_NET( pnet, sd );
		    if( sthread_is_empty( &data->recalc_thread ) == 1 )
		    {
			sthread_create( &data->recalc_thread, sd, spectravoice_recalc_proc, data, 0 );
		    }
		    else
		    {
			if( sthread_is_finished( &data->recalc_thread ) == 1 )
			{
			    sthread_destroy( &data->recalc_thread, STHREAD_TIMEOUT_INFINITE );
			    sthread_create( &data->recalc_thread, sd, spectravoice_recalc_proc, data, 0 );
			}
		    }
		}
	    }
	    break;
	case PS_CMD_CLOSE:
	    data->recalc_thread_stop = true;
	    sthread_destroy( &data->recalc_thread, STHREAD_TIMEOUT_INFINITE );
	    smutex_destroy( &data->render_sound_mutex );
	    for( int s = 0; s < MAX_SAMPLES; s++ )
	    {
		if( data->samples[ s ] ) smem_free( data->samples[ s ] );
		data->samples[ s ] = 0;
	    }
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
		remove_window( mod->visual, data->wm );
		mod->visual = 0;
	    }
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
