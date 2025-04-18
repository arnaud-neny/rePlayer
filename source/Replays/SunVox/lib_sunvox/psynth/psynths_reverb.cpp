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
#include "psynths_dc_blocker.h"
#define MODULE_DATA	psynth_reverb_data
#define MODULE_HANDLER	psynth_reverb
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_COMBS		8
#define MAX_ALLPASSES		4
#define MAX_BUF_SCALE		8
#define STEREO_SPREAD		23
#ifdef PS_STYPE_FLOATINGPOINT
    #define ALLPASS_FEEDBACK	(PS_STYPE2)0.5
#else
    #define ALLPASS_FEEDBACK	128
#endif
#ifdef PS_STYPE_FLOATINGPOINT
    #define INPUT_MUL			1
#else
    #define INPUT_MUL			4
#endif
struct comb_filter
{
    PS_STYPE2*	buf;
    int		buf_size;
    int		buf_ptr;
    PS_STYPE2	feedback;
    PS_STYPE2  	filterstore;
    PS_STYPE2  	damp1;
    PS_STYPE2  	damp2;
};
struct allpass_filter
{
    PS_STYPE2*	buf;
    int		buf_size;
    int		buf_ptr;
};
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_dry;
    PS_CTYPE  	ctl_wet;
    PS_CTYPE   	ctl_feedback;
    PS_CTYPE   	ctl_damp;
    PS_CTYPE   	ctl_width;
    PS_CTYPE   	ctl_freeze;
    PS_CTYPE   	ctl_mode;
    PS_CTYPE   	ctl_allpass;
    PS_CTYPE   	ctl_room_size;
    PS_CTYPE   	ctl_seed;
    comb_filter		combs[ MAX_COMBS * 2 ];
    int			biggest_comb;
    allpass_filter  	allpasses[ MAX_ALLPASSES * 2 ];
    int		    	filters_reinit_request;
    bool		filters_clean;
    bool	    	empty; 
    int                 empty_frames_counter; 
    int                 empty_frames_counter_max;
    int			empty_check_buf_ptr;
    dc_filter		dc;
};
static const int g_comb_base_size[ MAX_COMBS ] = 
{
    1116,
    1188,
    1277,
    1356,
    1422,
    1491,
    1557,
    1617
};
static const int g_allpass_base_size[ MAX_ALLPASSES ] = 
{
    556,
    441,
    341,
    225
};
static void recalc_filter_buf_size( MODULE_DATA* data, psynth_net* pnet )
{
    int scale = data->ctl_room_size;
    if( scale < 1 ) scale = 1;
    int delta;
    if( pnet->sampling_freq != 44100 )
	delta = ( pnet->sampling_freq << 11 ) / 44100;
    else
	delta = 1 << 11;
    uint seed = data->ctl_seed;
    int combs_changed = 0;
    for( int i = 0; i < MAX_COMBS * 2; i++ )
    {
        comb_filter* f = &data->combs[ i ];
        int bsize = g_comb_base_size[ i % MAX_COMBS ];
        if( data->ctl_seed > 0 )
    	{
    	    uint r = bsize / 2 + ( pseudo_random( &seed ) % bsize );
    	    bsize = r;
    	}
    	int64_t size;
        if( i < MAX_COMBS )
    	    size = (int64_t)bsize * scale;
    	else
    	    size = (int64_t)( bsize + STEREO_SPREAD ) * scale;
    	size = ( ( size * delta ) >> 11 ) / 16;
	if( f->buf_size != size )
	{
	    f->buf_size = size;
	    combs_changed++;
	}
    }
    {
	int biggest_comb = 0;
	int max_size = data->combs[ 0 ].buf_size;
	int channels = 2;
	int filters_add = 2;
        if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO ) channels = 1;
	if( data->ctl_mode < MODE_LQ ) filters_add = 1;
	for( int i = 0; i < MAX_COMBS * channels; i += filters_add )
	{
	    int size = data->combs[ i ].buf_size;
	    if( size > max_size )
	    {
		max_size = size;
		biggest_comb = i;
	    }
	}
	if( data->biggest_comb != biggest_comb )
	{
	    data->biggest_comb = biggest_comb;
	    combs_changed++;
	}
    }
    if( combs_changed )
    {
	data->empty_frames_counter = 0;
	data->empty_check_buf_ptr = 0;
    }
    for( int i = 0; i < MAX_ALLPASSES * 2; i++ )
    {
        allpass_filter* f = &data->allpasses[ i ];
        int bsize = g_allpass_base_size[ i % MAX_ALLPASSES ];
        if( data->ctl_seed > 0 )
    	{
    	    uint r = bsize / 2 + ( pseudo_random( &seed ) % bsize );
    	    bsize = r;
    	}
    	int64_t size;
        if( i < MAX_ALLPASSES )
    	    size = (int64_t)bsize * scale;
    	else
    	    size = (int64_t)( bsize + STEREO_SPREAD ) * scale;
	f->buf_size = ( ( size * delta ) >> 11 ) / 16;
    }
}
static void realloc_filter_bufs( MODULE_DATA* data, psynth_net* pnet )
{
    for( int i = 0; i < MAX_COMBS * 2; i++ )
    {
        comb_filter* f = &data->combs[ i ];
        size_t old_size = smem_get_size( f->buf ) / sizeof( PS_STYPE2 );
        if( f->buf_size > (int)old_size )
        {
    	    size_t new_size = f->buf_size + 128;
    	    f->buf = (PS_STYPE2*)smem_resize2( f->buf, new_size * sizeof( PS_STYPE2 ) );
        }
    }
    for( int i = 0; i < MAX_ALLPASSES * 2; i++ )
    {
        allpass_filter* f = &data->allpasses[ i ];
        size_t old_size = smem_get_size( f->buf ) / sizeof( PS_STYPE2 );
        if( f->buf_size > (int)old_size )
        {
    	    size_t new_size = f->buf_size + 128;
    	    f->buf = (PS_STYPE2*)smem_resize2( f->buf, new_size * sizeof( PS_STYPE2 ) );
        }
    }
}
static void reinit_filter_parameters( MODULE_DATA* data, psynth_net* pnet )
{
    int i;
    int feedback;
    int damp;
    if( data->ctl_freeze )
    {
	feedback = 256;
	damp = 256;
    }
    else
    {
    	feedback = ( data->ctl_feedback * 71 ) / 256 + 179;
	float lowpass_freq = (float)( 256 - data->ctl_damp ) * 128 + 128;
	float lowpass_rc = 1.0F / ( ( 2 * M_PI ) * lowpass_freq );
	float lowpass_delta_t = 1.0F / (float)pnet->sampling_freq;
	float lowpass_alpha = lowpass_delta_t / ( lowpass_rc + lowpass_delta_t );
	damp = (int)( lowpass_alpha * 256.0F );
    }
    for( i = 0; i < MAX_COMBS * 2; i++ )
    {
#ifdef PS_STYPE_FLOATINGPOINT
    	data->combs[ i ].feedback = (PS_STYPE2)feedback / (PS_STYPE2)256;
    	data->combs[ i ].damp1 = (PS_STYPE2)damp / (PS_STYPE2)256;
    	data->combs[ i ].damp2 = (PS_STYPE2)1.0 - data->combs[ i ].damp1;
#else
    	data->combs[ i ].feedback = feedback;
    	data->combs[ i ].damp1 = damp;
    	data->combs[ i ].damp2 = 256 - damp;
#endif
    }
}
static void clean_filters( MODULE_DATA* data )
{
    if( data->filters_clean ) return;
    for( int i = 0; i < MAX_COMBS * 2; i++ )
    {
	PS_STYPE2* buf = data->combs[ i ].buf;
	smem_clear( buf, sizeof( PS_STYPE2 ) * data->combs[ i ].buf_size );
	data->combs[ i ].filterstore = 0;
    }
    for( int i = 0; i < MAX_ALLPASSES * 2; i++ )
    {
	PS_STYPE2* buf = data->allpasses[ i ].buf;
	smem_clear( buf, sizeof( PS_STYPE2 ) * data->allpasses[ i ].buf_size );
    }
    data->filters_clean = true;
}
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
	    retval = (PS_RETTYPE)"Reverb";
	    break;
	case PS_CMD_GET_INFO:
	    {
		const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Ревербератор - эффект эха с многократными отражениями звука в большом помещении. Придает ощущение глубины пространства.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Reverberation effect - echo with numerous reflections to make the sound more natural.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFD37F";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    {
		psynth_resize_ctls_storage( mod_num, 10, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_DRY ), "", 0, 256, 256, 0, &data->ctl_dry, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_WET ), "", 0, 256, 40, 0, &data->ctl_wet, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 256, 256, 0, &data->ctl_feedback, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_DAMP ), "", 0, 256, 128, 0, &data->ctl_damp, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_STEREO_WIDTH ), "", 0, 256, 256, 0, &data->ctl_width, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREEZE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_freeze, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODES - 1, MODE_HQ, 1, &data->ctl_mode, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_ALLPASS_FILTER ), ps_get_string( STR_PS_ALLPASS_MODES ), 0, 2, 1, 1, &data->ctl_allpass, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_ROOM_SIZE ), "", 0, 16 * MAX_BUF_SCALE, 16, 0, &data->ctl_room_size, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_SEED ), "", 0, 32768, 0, 1, &data->ctl_seed, -1, 2, pnet );
        	dc_filter_init( &data->dc, pnet->sampling_freq );
		data->filters_reinit_request = 1;
		data->filters_clean = true;
		data->biggest_comb = 0;
		data->empty = true;
		data->empty_frames_counter_max = pnet->sampling_freq * 5;
		data->empty_frames_counter = 0;
		data->empty_check_buf_ptr = 0;
	    }
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    recalc_filter_buf_size( data, pnet );
	    for( int i = 0; i < MAX_COMBS * 2; i++ )
	    {
		comb_filter* f = &data->combs[ i ];
    		f->buf = (PS_STYPE2*)smem_new( f->buf_size * sizeof( PS_STYPE2 ) );
    		smem_zero( f->buf );
		f->buf_ptr = 0;
		f->filterstore = 0;
	    }
	    for( int i = 0; i < MAX_ALLPASSES * 2; i++ )
	    {
		allpass_filter* f = &data->allpasses[ i ];
    		f->buf = (PS_STYPE2*)smem_new( f->buf_size * sizeof( PS_STYPE2 ) );
    		smem_zero( f->buf );
		f->buf_ptr = 0;
	    }
	    clean_filters( data );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    clean_filters( data );
	    dc_filter_stop( &data->dc );
	    data->empty = true;
	    data->empty_frames_counter = 0;
	    data->empty_check_buf_ptr = 0;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
	        if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
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
			data->empty = false;
			data->empty_frames_counter = 0;
			data->empty_check_buf_ptr = 0;
			input_signal = true;
			break;
		    }
		}
		if( data->empty ) break;
		if( input_signal == false && data->ctl_freeze == 0 )
		{
		    bool reset_check = false;
		    if( data->empty_frames_counter >= data->empty_frames_counter_max )
		    {
		    	comb_filter* f = &data->combs[ data->biggest_comb ];
		    	int check_offset = data->empty_check_buf_ptr;
		    	int check_size = f->buf_size - check_offset;
		    	int max_frames = frames / 2;
		    	if( max_frames == 0 ) max_frames = 1;
		    	if( check_size > max_frames ) check_size = max_frames;
		    	for( int i = check_offset; i < check_offset + check_size; i++ )
		    	{
		    	    if( PS_STYPE_IS_NOT_MIN( f->buf[ i ] ) )
			    {
			    	reset_check = true;
			    	check_size = i - check_offset;
			    	break;
			    }
			}
			data->empty_check_buf_ptr += check_size;
			if( data->empty_check_buf_ptr >= f->buf_size )
			{
			    data->empty = true;
			    break;
			}
		    }
		    if( reset_check )
		    {
			data->empty_frames_counter = 0;
		    }
		    data->empty_frames_counter += frames;
		}
		PS_STYPE* inL_ch = inputs[ 0 ] + offset;
		PS_STYPE* inR_ch = inputs[ 1 ] + offset;
		PS_STYPE* outL_ch = outputs[ 0 ] + offset;
		PS_STYPE* outR_ch = outputs[ 1 ] + offset;
		if( outputs_num == 1 )
		{
		    inR_ch = 0;
		    outR_ch = 0;
		}
		else
		{
		    outputs_num = 2; 
		}
		int filters_add;
		if( data->ctl_mode < MODE_LQ )
		    filters_add = 1;
		else
		    filters_add = 2; 
		if( data->filters_reinit_request )
		{
		    reinit_filter_parameters( data, pnet );
		    recalc_filter_buf_size( data, pnet );
		    realloc_filter_bufs( data, pnet );
		    data->filters_reinit_request = 0;
		}
		PS_STYPE2 ctl_wet = PS_NORM_STYPE( data->ctl_wet, 256 );
		if( filters_add > 1 ) ctl_wet *= 2;
		if( data->ctl_allpass == 0 || data->ctl_allpass == 2 ) ctl_wet *= 4;
		for( int ch = 0; ch < outputs_num; ch++ )
		{
		    PS_STYPE* in;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    if( input_signal )
			in = inputs[ ch ] + offset;
		    else
			in = 0;
		    if( dc_filter_run( &data->dc, ch, in, out, frames ) )
		    {
			for( int i = 0; i < frames; i++ )
			{
			    PS_STYPE v = 0;
			    psynth_denorm_add_white_noise( v );
			    out[ i ] = v;
			}
		    }
		}
		data->filters_clean = false;
		switch( data->ctl_allpass )
		{
		    default:
		    {
			for( int i = 0; i < frames; i++ )
			{
			    PS_STYPE2 outL = 0;
			    PS_STYPE2 outR = 0;
			    PS_STYPE2 input = outL_ch[ i ];
			    if( inR_ch ) input = ( input + outR_ch[ i ] ) / (PS_STYPE2)2;
			    input *= INPUT_MUL; 
			    for( int a = 0; a < MAX_COMBS * outputs_num; a += filters_add )
			    {
				comb_filter* f = &data->combs[ a ];
				PS_STYPE2 f_out = f->buf[ f->buf_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				f->filterstore = ( f_out * f->damp1 ) + ( f->filterstore * f->damp2 );
				f->buf[ f->buf_ptr ] = input + ( f->filterstore * f->feedback );
#else
				PS_STYPE2 f_v;
				f->filterstore = ( f_out * f->damp1 + f->filterstore * f->damp2 ) / 256;
				f_v = input + ( f->filterstore * f->feedback ) / 256;
				f->buf[ f->buf_ptr ] = f_v;
#endif
				if( a < MAX_COMBS ) outL += f_out; else outR += f_out;
				f->buf_ptr++;
				if( f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
			    }
			    outL_ch[ i ] = PS_NORM_STYPE_MUL( outL / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			    if( inR_ch ) outR_ch[ i ] = PS_NORM_STYPE_MUL( outR / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			}
		    }
		    break;
		    case 1:
		    {
			for( int i = 0; i < frames; i++ )
			{
			    PS_STYPE2 outL = 0;
			    PS_STYPE2 outR = 0;
			    PS_STYPE2 input = outL_ch[ i ];
			    if( inR_ch ) input = ( input + outR_ch[ i ] ) / (PS_STYPE2)2;
			    input *= INPUT_MUL; 
			    for( int a = 0; a < MAX_COMBS * outputs_num; a += filters_add )
			    {
				comb_filter* f = &data->combs[ a ];
				PS_STYPE2 f_out = f->buf[ f->buf_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				f->filterstore = ( f_out * f->damp1 ) + ( f->filterstore * f->damp2 );
				f->buf[ f->buf_ptr ] = input + ( f->filterstore * f->feedback );
#else
				PS_STYPE2 f_v;
				f->filterstore = ( f_out * f->damp1 + f->filterstore * f->damp2 ) / 256;
				f_v = input + ( f->filterstore * f->feedback ) / 256;
				f->buf[ f->buf_ptr ] = f_v;
#endif
				if( a < MAX_COMBS ) outL += f_out; else outR += f_out;
				f->buf_ptr++;
				if( f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
			    }
			    for( int a = 0; a < MAX_ALLPASSES * outputs_num; a += filters_add )
			    {
				allpass_filter* f = &data->allpasses[ a ];
				PS_STYPE2 f_bufout = f->buf[ f->buf_ptr ];
				PS_STYPE2 f_out;
				PS_STYPE2 f_in;
				if( a < MAX_ALLPASSES ) f_in = outL; else f_in = outR;
				f_out = -f_in + f_bufout;
#ifdef PS_STYPE_FLOATINGPOINT
				f->buf[ f->buf_ptr ] = f_in + ( f_bufout * ALLPASS_FEEDBACK );
#else
				f->buf[ f->buf_ptr ] = f_in + ( f_bufout * ALLPASS_FEEDBACK ) / 256;
#endif
				if( a < MAX_ALLPASSES ) outL = f_out; else outR = f_out;
				f->buf_ptr++;
				if( f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
			    }
			    outL_ch[ i ] = PS_NORM_STYPE_MUL( outL / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			    if( inR_ch ) outR_ch[ i ] = PS_NORM_STYPE_MUL( outR / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			}
		    }
		    break;
		    case 2:
		    {
			for( int i = 0; i < frames; i++ )
			{
			    PS_STYPE2 outL = 0;
			    PS_STYPE2 outR = 0;
			    PS_STYPE2 input = outL_ch[ i ];
			    if( inR_ch ) input = ( input + outR_ch[ i ] ) / (PS_STYPE2)2;
			    input *= INPUT_MUL; 
			    for( int a = 0; a < MAX_COMBS * outputs_num; a += filters_add )
			    {
				comb_filter* f = &data->combs[ a ];
				PS_STYPE2 f_out = f->buf[ f->buf_ptr ];
#ifdef PS_STYPE_FLOATINGPOINT
				f->filterstore = ( f_out * f->damp1 ) + ( f->filterstore * f->damp2 );
				f->buf[ f->buf_ptr ] = input + ( f->filterstore * f->feedback );
#else
				PS_STYPE2 f_v;
				f->filterstore = ( f_out * f->damp1 + f->filterstore * f->damp2 ) / 256;
				f_v = input + ( f->filterstore * f->feedback ) / 256;
				f->buf[ f->buf_ptr ] = f_v;
#endif
				if( a < MAX_COMBS ) outL += f_out; else outR += f_out;
				f->buf_ptr++;
				if( f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
			    }
			    for( int a = 0; a < MAX_ALLPASSES * outputs_num; a += filters_add )
			    {
				allpass_filter* f = &data->allpasses[ a ];
				PS_STYPE2 f_bufout = f->buf[ f->buf_ptr ];
				PS_STYPE2 f_out;
				PS_STYPE2 f_in;
				if( a < MAX_ALLPASSES ) f_in = outL; else f_in = outR;
#ifdef PS_STYPE_FLOATINGPOINT
				PS_STYPE2 buf_write = f_in + ( f_bufout * ALLPASS_FEEDBACK );
				f->buf[ f->buf_ptr ] = buf_write;
				f_out = -buf_write * ALLPASS_FEEDBACK + f_bufout;
#else
				PS_STYPE2 buf_write = f_in + ( f_bufout * ALLPASS_FEEDBACK ) / 256;
				f->buf[ f->buf_ptr ] = buf_write;
				f_out = ( -buf_write * ALLPASS_FEEDBACK ) / 256 + f_bufout;
#endif
				if( a < MAX_ALLPASSES ) outL = f_out; else outR = f_out;
				f->buf_ptr++;
				if( f->buf_ptr >= f->buf_size ) f->buf_ptr = 0;
			    }
			    outL_ch[ i ] = PS_NORM_STYPE_MUL( outL / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			    if( inR_ch ) outR_ch[ i ] = PS_NORM_STYPE_MUL( outR / (PS_STYPE2)(16*INPUT_MUL), ctl_wet, 256 );
			}
		    }
		    break;
		}
		if( data->ctl_dry > 0 )
		{
		    if( data->ctl_dry == 256 )
		    {
			for( int i = 0; i < frames; i++ ) outL_ch[ i ] += inL_ch[ i ];
			if( inR_ch ) for( int i = 0; i < frames; i++ ) outR_ch[ i ] += inR_ch[ i ];
		    }
		    else
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			PS_STYPE dry = (PS_STYPE)data->ctl_dry / 256.0F;
			for( int i = 0; i < frames; i++ ) outL_ch[ i ] += inL_ch[ i ] * dry;
			if( inR_ch ) for( int i = 0; i < frames; i++ ) outR_ch[ i ] += inR_ch[ i ] * dry;
#else
			PS_STYPE2 dry = data->ctl_dry;
			for( int i = 0; i < frames; i++ ) outL_ch[ i ] += (PS_STYPE)( (PS_STYPE2)( inL_ch[ i ] * dry ) / 256 );
			if( inR_ch ) for( int i = 0; i < frames; i++ ) outR_ch[ i ] += (PS_STYPE)( (PS_STYPE2)( inR_ch[ i ] * dry ) / 256 );
#endif
		    }
		}
		if( outputs_num == 2 && data->ctl_width != 256 )
		{
		    int ctl_width = data->ctl_width / 2 + 128;
		    int ctl_width2 = 256 - ctl_width;
#ifdef PS_STYPE_FLOATINGPOINT
		    PS_STYPE ctl_fwidth = (PS_STYPE)ctl_width / 256.0F;
		    PS_STYPE ctl_fwidth2 = (PS_STYPE)ctl_width2 / 256.0F;
		    for( int i = 0; i < frames; i++ )
		    {
			outL_ch[ i ] = outL_ch[ i ] * ctl_fwidth + outR_ch[ i ] * ctl_fwidth2;
			outR_ch[ i ] = outR_ch[ i ] * ctl_fwidth + outL_ch[ i ] * ctl_fwidth2;
		    }
#else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 outL = 
			    ( (PS_STYPE2)outL_ch[ i ] * ctl_width ) / 256 + 
			    ( (PS_STYPE2)outR_ch[ i ] * ctl_width2 ) / 256;
			PS_STYPE2 outR = 
			    ( (PS_STYPE2)outR_ch[ i ] * ctl_width ) / 256 + 
			    ( (PS_STYPE2)outL_ch[ i ] * ctl_width2 ) / 256;
			outL_ch[ i ] = outL;
			outR_ch[ i ] = outR;
		    }
#endif
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    data->filters_reinit_request = 1;
	    break;
	case PS_CMD_CLOSE:
	    for( int i = 0; i < MAX_COMBS * 2; i++ )
	    {
    		smem_free( data->combs[ i ].buf );
	    }
	    for( int i = 0; i < MAX_ALLPASSES * 2; i++ )
	    {
    		smem_free( data->allpasses[ i ].buf );
	    }
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
