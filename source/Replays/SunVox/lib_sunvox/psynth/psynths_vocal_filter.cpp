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
#define MODULE_DATA	psynth_vfilter_data
#define MODULE_HANDLER	psynth_vfilter
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#ifdef PS_STYPE_FLOATINGPOINT
#else
    #define COEF_SIZE	11
#endif
#define CHANGED_PARS	( 1 << 0 )
inline int INTERP( int v1, int v2, int p ) 
{
    return ( ( (v1*(256-p)) + (v2*p) ) / 256 );
}
#define VOWELS_NUM	5
#define FORMANTS_NUM	5
const static uint16_t g_vowels1[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    800,	1150,	    2900,	3900,	    4950, 
    80,		90,	    120,	130,	    140, 
    256,	128,	    6,		25,	    0, 
    350,	2000,	    2800,	3600,	    4950,
    60,		100,	    120,	150,	    200,
    256,	25,	    45,		2,	    0,
    270,	2140,	    2950,	3900,	    4950,
    60,		90,	    100,	120,	    120,
    256,	64,	    12,		12,	    1,
    450,	800,	    2830,	3800,	    4950,
    70,		80,	    100,	130,	    135,
    256,	72,	    20,		20,	    0,
    325,	700,	    2700,	3800,	    4950,
    50,		60,	    170,	180,	    200,
    256,	40,	    4,		2,	    0,
};
const static uint16_t g_vowels2[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    800,	1150,	    2800,	3500,	    4950,
    80,		90,	    120,	130,	    140,
    256,	161,	    25,		4,	    0,
    400,	1600,	    2700,	3300,	    4950,
    60,		80,	    120,	150,	    200,
    256,	16,	    8,		4,	    0,
    350,	1700,	    2700,	3700,	    4950,
    50,		100,	    120,	150,	    200,
    256,	25,	    8,		4,	    0,
    450,	800,	    2830,	3500,	    4950,
    70,		80,	    100,	130,	    135,
    256,	90,	    40,		10,	    0,
    325,	700,	    2530,	3500,	    4950,
    50,		60,	    170,	180,	    200,
    256,	64,	    8,		2,	    0,
};
const static uint16_t g_vowels3[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    650,	1080,	    2650,	2900,	    3250,
    80,		90,	    120,	130,	    140,
    256,	128,	    114,	101,	    20,
    400,	1700,	    2600,	3200,	    3580,
    70,		80,	    100,	120,	    120,
    256,	51,	    64,		51,	    25,
    290,	1870,	    2800,	3250,	    3540,
    40,		90,	    100,	120,	    120,
    256,	45,	    32,		25,	    8,
    400,	800,	    2600,	2800,	    3000,
    40,		80,	    100,	120,	    120,
    256,	80,	    64,		64,	    12,
    350,	600,	    2700,	2900,	    3300,
    40,		60,	    100,	120,	    120,
    256,	25,	    36,		51,	    12,
};
const static uint16_t g_vowels4[ FORMANTS_NUM * 3 * VOWELS_NUM ] = {
    600,	1040,	    2250,	2450,	    2750,
    60,		70,	    110,	120,	    130,
    256,	114,	    90,		90,	    25,
    400,	1620,	    2400,	2800,	    3100,
    40,		80,	    100,	120,	    120,
    256,	64,	    90,		64,	    32,
    250,	1750,	    2600,	3050,	    3340,
    60,		90,	    100,	120,	    120,
    256,	8,	    40,		20,	    10,
    400,	750,	    2400,	2600,	    2900,
    40,		80,	    100,	120,	    120,
    256,	72,	    22,		25,	    2,
    350,	600,	    2400,	2675,	    2950,
    40,		80,	    100,	120,	    120,
    256,	25,	    6,		10,	    4,
};
enum {
    FF_TYPE_BANDPASS2, 
};
struct ff
{
    int		type;
    int		amp; 
    PS_STYPE2	x1, x2;
    PS_STYPE2	y1, y2;
    PS_STYPE2	b0a0, b2a0, a1a0, a2a0;
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_bw;
    PS_CTYPE   	ctl_amp_add;
    PS_CTYPE   	ctl_formants_num;
    PS_CTYPE   	ctl_vowel;
    PS_CTYPE   	ctl_character;
    PS_CTYPE   	ctl_mono;
    PS_CTYPE	ctl_random;
    PS_CTYPE	ctl_random_seed;
    PS_CTYPE	ctl_v1;
    PS_CTYPE	ctl_v2;
    PS_CTYPE	ctl_v3;
    PS_CTYPE	ctl_v4;
    PS_CTYPE	ctl_v5;
    ff	    	filters[ FORMANTS_NUM * MODULE_OUTPUTS ];
    const uint16_t* vowels1;
    const uint16_t* vowels2;
    const uint16_t* vowels3;
    const uint16_t* vowels4;
    int	    	empty_frames_counter;
    int	    	empty_frames_counter_max;
    uint32_t 	changed; 
};
static void ff_init( ff *f )
{
    smem_clear( f, sizeof( ff ) );
}
static void ff_stop( ff *f )
{
    f->x1 = 0;
    f->x2 = 0;
    f->y1 = 0;
    f->y2 = 0;
}
static void ff_set( ff *f, int type, int fs, int f0, int bw, int amp )
{
    f->type = type;
    f->amp = amp;
    float w0 = 2 * M_PI * ( (float)f0 / (float)fs );
    float cs, sn;
    cs = cos( w0 );
    sn = sin( w0 );
    if( bw >= fs / 2 ) bw = fs / 2;
    if( bw <= 0 ) bw = 1;
    float q = (float)f0 / (float)bw;
    float alpha = sn / ( 2 * q );
    float b0, b1, b2;
    float a0, a1, a2;
    switch( type )
    {
	case FF_TYPE_BANDPASS2:
	    b0 = alpha;
	    b1 = 0;
	    b2 = -alpha;
	    a0 = 1 + alpha;
	    a1 = -2 * cs;
	    a2 = 1 - alpha;
	    break;
    }
#ifdef PS_STYPE_FLOATINGPOINT
    f->b0a0 = b0 / a0;
    f->b2a0 = b2 / a0;
    f->a1a0 = a1 / a0;
    f->a2a0 = a2 / a0;
#else
    f->b0a0 = (PS_STYPE2)( ( b0 / a0 ) * (float)(1<<COEF_SIZE) );
    f->b2a0 = (PS_STYPE2)( ( b2 / a0 ) * (float)(1<<COEF_SIZE) );
    f->a1a0 = (PS_STYPE2)( ( a1 / a0 ) * (float)(1<<COEF_SIZE) );
    f->a2a0 = (PS_STYPE2)( ( a2 / a0 ) * (float)(1<<COEF_SIZE) );
#endif
}
static int ff_do( ff* f, PS_STYPE* out, PS_STYPE* in, int samples_num, int add )
{
    if( f->amp == 0 ) return 0;
    PS_STYPE2 b0a0 = f->b0a0;
    PS_STYPE2 b2a0 = f->b2a0;
    PS_STYPE2 a1a0 = f->a1a0;
    PS_STYPE2 a2a0 = f->a2a0;
    PS_STYPE2 x1 = f->x1;
    PS_STYPE2 x2 = f->x2;
    PS_STYPE2 y1 = f->y1;
    PS_STYPE2 y2 = f->y2;
#ifdef PS_STYPE_FLOATINGPOINT
    PS_STYPE2 amp = (PS_STYPE2)f->amp / (PS_STYPE2)256;
    for( int i = 0; i < samples_num; i++ )
    {
	PS_STYPE2 inp = in[ i ];
	psynth_denorm_add_white_noise( inp );
	PS_STYPE2 val = 
	    b0a0 * inp + b2a0 * x2 -
	    a1a0 * y1 - a2a0 * y2;
	x2 = x1;
	x1 = inp;
	y2 = y1;
	y1 = val;
	if( add )
	    out[ i ] += (PS_STYPE)( val * amp );
	else
	    out[ i ] = (PS_STYPE)( val * amp );
    }
#else
    PS_STYPE2 amp = (PS_STYPE2)f->amp;
    if( amp == 256 )
    {
	if( add )
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		PS_STYPE2 inp = in[ i ] << (14-COEF_SIZE);
		PS_STYPE2 val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] += (PS_STYPE)val;
	    }
	}
	else
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		PS_STYPE2 inp = in[ i ] << (14-COEF_SIZE);
		PS_STYPE2 val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] = (PS_STYPE)val;
	    }
	}
    }
    else
    {
	if( add )
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		PS_STYPE2 inp = in[ i ] << (14-COEF_SIZE);
		PS_STYPE2 val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] += ( ( val * amp ) / 256 );
	    }
	}
	else
	{
	    for( int i = 0; i < samples_num; i++ )
	    {
		PS_STYPE2 inp = in[ i ] << (14-COEF_SIZE);
		PS_STYPE2 val = 
		    ( b0a0 * inp + b2a0 * x2 -
		    a1a0 * y1 - a2a0 * y2 ) >> COEF_SIZE;
		x2 = x1;
		x1 = inp;
		y2 = y1;
		y1 = val;
		val >>= (14-COEF_SIZE);
		out[ i ] = ( ( val * amp ) / 256 );
	    }
	}
    }
#endif
    f->x1 = x1;
    f->x2 = x2;
    f->y1 = y1;
    f->y2 = y2;
    return 1; 
}
static char g_vowel_str[ 32 * 5 ];
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
	    retval = (PS_RETTYPE)"Vocal filter";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Формантный фильтр. Придает любому звуку окраску человеческого голоса";
                        break;
                    }
		    retval = (PS_RETTYPE)"Formant filter - designed to simulate the human vocal tract";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFC2BB";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 14, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FORMANT_WIDTH ), ps_get_string( STR_PS_HZ ), 0, 256, 128, 0, &data->ctl_bw, 128, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INTENSITY ), "", 0, 256, 128, 0, &data->ctl_amp_add, 128, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FORMANTS ), "", 1, FORMANTS_NUM, FORMANTS_NUM, 1, &data->ctl_formants_num, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOWEL ), "", 0, 256, 0, 0, &data->ctl_vowel, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOICE_TYPE ), ps_get_string( STR_PS_VOICE_TYPES ), 0, 3, 0, 1, &data->ctl_character, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_FREQ ), "", 0, 1024, 0, 0, &data->ctl_random, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_SEED ), "", 0, 32768, 0, 0, &data->ctl_random_seed, -1, 3, pnet );
	    if( ps_get_string( STR_PS_VOWEL2 )[ 0 ] != g_vowel_str[ 0 ] )
	    {
		for( int i = 0; i < 5; i++ )
		    sprintf( &g_vowel_str[ 32 * i ], "%s%d", ps_get_string( STR_PS_VOWEL2 ), i+1 );
	    }
	    psynth_register_ctl( mod_num, &g_vowel_str[ 32 * 0 ], ps_get_string( STR_PS_VOWEL_TYPES ), 0, 4, 0, 1, &data->ctl_v1, -1, 1, pnet );
	    psynth_register_ctl( mod_num, &g_vowel_str[ 32 * 1 ], ps_get_string( STR_PS_VOWEL_TYPES ), 0, 4, 1, 1, &data->ctl_v2, -1, 1, pnet );
	    psynth_register_ctl( mod_num, &g_vowel_str[ 32 * 2 ], ps_get_string( STR_PS_VOWEL_TYPES ), 0, 4, 2, 1, &data->ctl_v3, -1, 1, pnet );
	    psynth_register_ctl( mod_num, &g_vowel_str[ 32 * 3 ], ps_get_string( STR_PS_VOWEL_TYPES ), 0, 4, 3, 1, &data->ctl_v4, -1, 1, pnet );
	    psynth_register_ctl( mod_num, &g_vowel_str[ 32 * 4 ], ps_get_string( STR_PS_VOWEL_TYPES ), 0, 4, 4, 1, &data->ctl_v5, -1, 1, pnet );
	    {
		for( int f = 0; f < FORMANTS_NUM; f++ )
		{
		    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		    {
			ff_init( &data->filters[ MODULE_OUTPUTS * f + ch ] );
		    }
		}
	    }
	    data->vowels1 = g_vowels1;
	    data->vowels2 = g_vowels2;
	    data->vowels3 = g_vowels3;
	    data->vowels4 = g_vowels4;
	    data->changed = 0xFFFFFFFF;
#ifdef PS_STYPE_FLOATINGPOINT
	    data->empty_frames_counter_max = pnet->sampling_freq * 4;
#else
	    data->empty_frames_counter_max = pnet->sampling_freq * 1;
#endif
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    {
		for( int f = 0; f < FORMANTS_NUM; f++ )
		{
		    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		    {
			ff_stop( &data->filters[ MODULE_OUTPUTS * f + ch ] );
		    }
		}
	    }
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
	        if( data->ctl_mono )
	        {
	    	    if( psynth_get_number_of_outputs( mod ) != 1 )
	    	    {
	        	psynth_set_number_of_outputs( 1, mod_num, pnet );
	        	psynth_set_number_of_inputs( 1, mod_num, pnet );
	    	    }
	        }
	        else
	        {
	    	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
	    	    {
	        	psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
	        	psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
	    	    }
	        }
		int outputs_num = psynth_get_number_of_outputs( mod );
		bool input_signal = false;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			input_signal = true;
			break;
		    }
		}
		if( input_signal ) 
		{
		    data->empty_frames_counter = 0;
		}
		else
		{
		    if( data->empty_frames_counter >= data->empty_frames_counter_max )
			break;
		    data->empty_frames_counter += frames;
		}
		if( data->changed & CHANGED_PARS )
		{
		    int vn = data->ctl_vowel;
		    if( vn >= 256 ) vn = 255;
		    int pos = ( ( 256 * (VOWELS_NUM-1) ) * vn ) / 256;
		    int pos2 = pos / 256;
		    int pos3 = pos2 + 1;
		    int f[ FORMANTS_NUM ];
		    int b[ FORMANTS_NUM ];
		    int amp[ FORMANTS_NUM ];
		    const uint16_t* vowels = NULL;
		    switch( data->ctl_character )
		    {
			case 0: vowels = data->vowels1; break;
			case 1: vowels = data->vowels2; break;
			case 2: vowels = data->vowels3; break;
			case 3: vowels = data->vowels4; break;
		    }
		    int8_t vmap[ VOWELS_NUM ];
		    vmap[ 0 ] = data->ctl_v1;
		    vmap[ 1 ] = data->ctl_v2;
		    vmap[ 2 ] = data->ctl_v3;
		    vmap[ 3 ] = data->ctl_v4;
		    vmap[ 4 ] = data->ctl_v5;
		    pos2 = vmap[ pos2 ];
		    pos3 = vmap[ pos3 ];
		    for( int a = 0; a < data->ctl_formants_num; a++ )
		    {
			int f1 = vowels[ pos2 * FORMANTS_NUM * 3 + a ];
			int f2 = vowels[ pos3 * FORMANTS_NUM * 3 + a ];
			if( data->ctl_random )
			{
			    uint32_t rnd_seed;
			    rnd_seed = ( data->ctl_random_seed + data->ctl_character ) * 32768 + pos2 * FORMANTS_NUM * 3 + a;
			    int rnd1 = pseudo_random( &rnd_seed );
			    rnd_seed = ( data->ctl_random_seed + data->ctl_character ) * 32768 + pos3 * FORMANTS_NUM * 3 + a;
			    int rnd2 = pseudo_random( &rnd_seed );
			    rnd1 = ( rnd1 - 16384 ) * data->ctl_random / 1024;
			    rnd2 = ( rnd2 - 16384 ) * data->ctl_random / 1024;
			    f1 += rnd1 / 4;
			    f2 += rnd2 / 4;
			    if( f1 < 100 ) f1 = 100*2 - f1;
			    if( f2 < 100 ) f2 = 100*2 - f2;
			    if( f1 > 8000 ) f1 = 8000*2 - f1;
			    if( f2 > 8000 ) f2 = 8000*2 - f2;
			}
			f[ a ] = INTERP( f1, f2, pos & 255 );
			b[ a ] = INTERP( vowels[ pos2 * FORMANTS_NUM * 3 + FORMANTS_NUM + a ], vowels[ pos3 * FORMANTS_NUM * 3 + FORMANTS_NUM + a ], pos & 255 );
			amp[ a ] = INTERP( vowels[ pos2 * FORMANTS_NUM * 3 + FORMANTS_NUM*2 + a ], vowels[ pos3 * FORMANTS_NUM * 3 + FORMANTS_NUM*2 + a ], pos & 255 );
			amp[ a ] += data->ctl_amp_add;
			if( amp[ a ] > 256 ) amp[ a ] = 256;
			amp[ a ] = ( amp[ a ] * data->ctl_volume ) / 256;
		    }
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			for( int a = 0; a < data->ctl_formants_num; a++ )
			{
			    int bw = data->ctl_bw;
			    ff_set(
				&data->filters[ MODULE_OUTPUTS * a + ch ],
				FF_TYPE_BANDPASS2,
				pnet->sampling_freq,
				f[ a ],
				( b[ a ] * bw ) / 128,
				amp[ a ] );
			}
		    }
		}
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    int add = 0;
		    for( int a = 0; a < data->ctl_formants_num; a++ )
		    {
			if( ff_do( &data->filters[ MODULE_OUTPUTS * a + ch ], out, in, frames, add ) )
			{
			    retval = 1;
			    add = 1;
			}
		    }
		}
		data->changed = 0;
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    data->changed |= CHANGED_PARS;
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
