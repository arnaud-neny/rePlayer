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
#define MODULE_DATA	psynth_fft_data
#define MODULE_HANDLER	psynth_fft
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define CHANGED_IOCHANNELS      ( 1 << 0 )
#define CHANGED_RESAMPLER       ( 1 << 1 )
#define CHANGED_BUF       	( 1 << 2 )
const static int g_rates[ 10 ] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000, 192000 };
struct MODULE_DATA
{
    PS_CTYPE		ctl_srate;
    PS_CTYPE		ctl_stereo;
    PS_CTYPE		ctl_buf_size; 
    PS_CTYPE		ctl_overlap;
    PS_CTYPE		ctl_feedback;
    PS_CTYPE		ctl_noisered;
    PS_CTYPE		ctl_phase_gain;
    PS_CTYPE		ctl_allpass;
    PS_CTYPE		ctl_random_freqs;
    PS_CTYPE		ctl_random_phase;
    PS_CTYPE		ctl_random_phase_shift;
    PS_CTYPE		ctl_shift;
    PS_CTYPE		ctl_deform1;
    PS_CTYPE		ctl_deform2;
    PS_CTYPE		ctl_low;
    PS_CTYPE		ctl_high;
    PS_CTYPE		ctl_volume;
    int			srate; 
    psynth_resampler*	resamp1; 
    psynth_resampler*	resamp2; 
    bool		use_resampler;
    int                 buf_size; 
    float		native_buf_size; 
    int			buf_ptr;
    float*		bufs[ MODULE_OUTPUTS ]; 
    float*		bufs2[ MODULE_OUTPUTS ]; 
    float*		feedback_bufs[ MODULE_OUTPUTS ];
    float*              fft_i; 
    float*              fft_r; 
    float*		fft_win;
    float		fft_phase_rotate[ MODULE_OUTPUTS ]; 
    bool		bufs_clean;
    bool		feedback_bufs_clean;
    bool                empty; 
    int			empty_frames_counter;
    uint32_t            noise_seed;
    uint32_t            changed; 
};
static void fft_reinit_resampler( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    bool srate_changed = false;
    int new_srate = g_rates[ data->ctl_srate ];
    if( new_srate != data->srate )
    {
        data->srate = new_srate;
        srate_changed = true;
    }
    data->use_resampler = false;
    if( data->srate != pnet->sampling_freq )
    {
        data->use_resampler = true;
	uint32_t flags = PSYNTH_RESAMP_FLAG_DONT_RESET_WHEN_EMPTY; 
	if( !data->resamp1 )
	{
	    data->resamp1 = psynth_resampler_new( pnet, mod_num, data->srate, pnet->sampling_freq, 0, flags );
	    data->resamp2 = psynth_resampler_new( pnet, mod_num, pnet->sampling_freq, data->srate, (int64_t)65536*65536 / data->resamp1->ratio_fp, flags | PSYNTH_RESAMP_FLAG_MODE1 );
    	    for( int i = 0; i < MODULE_OUTPUTS; i++ )
    	    {
    		psynth_resampler_input_buf( data->resamp1, i );
    		psynth_resampler_input_buf( data->resamp2, i );
    	    }
	}
	else
	{
    	    if( data->resamp1->in_smprate != data->srate )
    	    {
		psynth_resampler_change( data->resamp1, data->srate, pnet->sampling_freq, 0, flags );
		psynth_resampler_change( data->resamp2, pnet->sampling_freq, data->srate, (int64_t)65536*65536 / data->resamp1->ratio_fp, flags | PSYNTH_RESAMP_FLAG_MODE1 );
    	        for( int i = 0; i < MODULE_OUTPUTS; i++ )
    		{
    		    psynth_resampler_input_buf( data->resamp1, i );
    		    psynth_resampler_input_buf( data->resamp2, i );
    		}
    	    }
	}
    }
}
static void fft_reinit_bufs( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    int buf_size = 64 << data->ctl_buf_size;
    size_t new_size = buf_size * sizeof( float );
    size_t old_size = smem_get_size( data->bufs[ 0 ] );
    if( new_size > old_size )
    {
    	for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
    	{
    	    data->bufs[ ch ] = (float*)smem_resize2( data->bufs[ ch ], new_size );
    	    data->bufs2[ ch ] = (float*)smem_resize2( data->bufs2[ ch ], new_size );
    	    data->feedback_bufs[ ch ] = (float*)smem_resize2( data->feedback_bufs[ ch ], new_size );
    	}
        data->fft_i = (float*)smem_resize2( data->fft_i, new_size );
        data->fft_r = (float*)smem_resize2( data->fft_r, new_size );
        data->fft_win = (float*)smem_resize( data->fft_win, new_size );
    }
    if( data->buf_size != buf_size )
    {
        data->buf_size = buf_size;
        data->buf_ptr = 0;
        dsp_window_function( data->fft_win, buf_size, dsp_win_fn_hann );
    }
}
static void fft_handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->changed & CHANGED_IOCHANNELS )
    {
        if( data->ctl_stereo )
        {
            if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
            {
                psynth_set_number_of_inputs( MODULE_OUTPUTS, mod_num, pnet );
                psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
            }
        }
        else
        {
            if( psynth_get_number_of_outputs( mod ) != 1 )
            {
                psynth_set_number_of_inputs( 1, mod_num, pnet );
                psynth_set_number_of_outputs( 1, mod_num, pnet );
            }
        }
    }
    if( data->changed & CHANGED_RESAMPLER )
    {
        fft_reinit_resampler( mod, mod_num );
    }
    if( data->changed & CHANGED_BUF )
    {
        fft_reinit_bufs( mod, mod_num );
    }
    data->native_buf_size = data->buf_size;
    if( data->use_resampler )
    {
	data->native_buf_size = (float)( data->buf_size * pnet->sampling_freq ) / data->srate;
    }
    data->changed = 0;
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
	    retval = (PS_RETTYPE)"FFT";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Преобразователь частот на базе алгоритма FFT";
                        break;
                    }
		    retval = (PS_RETTYPE)"FFT-based frequency transformator";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#006BFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	{
	    int ctl;
	    psynth_resize_ctls_storage( mod_num, 17, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_RATE ), "8000;11025;16000;22050;32000;44100;48000;88200;96000;192000", 0, 9, 5, 1, &data->ctl_srate, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_MONO_STEREO ), 0, 1, 0, 1, &data->ctl_stereo, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BUF_SIZE ), "64;128;256;512;1024;2048;4096;8192", 0, 7, 4, 1, &data->ctl_buf_size, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BUF_OVERLAP ), "0;2x;4x", 0, 2, 1, 1, &data->ctl_overlap, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FEEDBACK ), "", 0, 32768, 0, 0, &data->ctl_feedback, -1, 0, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOISE_REDUCTION ), "", 0, 32768, 0, 0, &data->ctl_noisered, -1, 1, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FFT_PHASE_GAIN ), "", 0, 32768, 32768/2, 1, &data->ctl_phase_gain, 32768/2, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ALLPASS_FILTER ), "", 0, 32768, 0, 0, &data->ctl_allpass, -1, 1, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_FREQ_SPREAD ), "", 0, 32768, 0, 0, &data->ctl_random_freqs, -1, 2, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_PHASE ), "", 0, 32768, 0, 0, &data->ctl_random_phase, -1, 2, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_RANDOM_PHASE_LITE ), "", 0, 32768, 0, 0, &data->ctl_random_phase_shift, -1, 2, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_SHIFT ), "", 0, 8192, 4096, 0, &data->ctl_shift, 4096, 3, pnet );
	    psynth_set_ctl_show_offset( mod_num, ctl, -4096, pnet );
	    psynth_register_ctl( mod_num, "Deform1", "", 0, 32768, 0, 0, &data->ctl_deform1, -1, 3, pnet );
	    psynth_register_ctl( mod_num, "Deform2", "", 0, 32768, 0, 0, &data->ctl_deform2, -1, 3, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_HP_CUTOFF ), "", 0, 32768, 0, 0, &data->ctl_low, -1, 3, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_LP_CUTOFF ), "", 0, 32768, 32768, 0, &data->ctl_high, -1, 3, pnet );
	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 32768, 32768, 0, &data->ctl_volume, -1, 4, pnet );
    	    data->changed = 0xFFFFFFFF;
    	    data->bufs_clean = true;
    	    data->feedback_bufs_clean = true;
    	    data->empty = true;
	    retval = 1;
	    break;
	}
	case PS_CMD_SETUP_FINISHED:
            fft_handle_changes( mod, mod_num );
            data->noise_seed = stime_ms() + pseudo_random() + mod_num * 371;
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    psynth_resampler_reset( data->resamp1 );
	    psynth_resampler_reset( data->resamp2 );
    	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
    	    {
    		data->fft_phase_rotate[ ch ] = 0;
    		if( !data->bufs_clean )
		{
		    smem_zero( data->bufs[ ch ] );
		    smem_zero( data->bufs2[ ch ] );
    		}
    		if( !data->feedback_bufs_clean )
		{
		    smem_zero( data->feedback_bufs[ ch ] );
    		}
    	    }
    	    data->bufs_clean = true;
    	    data->feedback_bufs_clean = true;
    	    data->empty = true;
    	    data->empty_frames_counter = 0;
	    data->buf_ptr = 0;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	{
	    if( data->changed ) fft_handle_changes( mod, mod_num );
	    PS_STYPE** inputs = mod->channels_in;
	    PS_STYPE** outputs = mod->channels_out;
	    int frames = mod->frames;
	    int offset = mod->offset;
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
                data->empty = false;
                data->empty_frames_counter = 0;
            }
            else
            {
        	if( data->empty )
        	    break;
	    }
	    PS_STYPE** resamp_inputs = inputs;
	    PS_STYPE** resamp_outputs = outputs;
            int resamp_offset = offset;
            int resamp_frames = frames;
	    PS_STYPE* resamp1_bufs[ MODULE_OUTPUTS ];
	    PS_STYPE* resamp1_bufs2[ MODULE_OUTPUTS ];
	    PS_STYPE* resamp2_bufs[ MODULE_OUTPUTS ];
	    if( data->use_resampler )
	    {
            	resamp_inputs = resamp_outputs = resamp1_bufs2;
            	resamp_offset = 0;
		for( int ch = 0; ch < outputs_num; ch++ )
    		{
    	    	    resamp1_bufs[ ch ] = psynth_resampler_input_buf( data->resamp1, ch ); 
            	    resamp1_bufs2[ ch ] = resamp1_bufs[ ch ] + psynth_resampler_input_buf_offset( data->resamp1 );
            	    resamp2_bufs[ ch ] = psynth_resampler_input_buf( data->resamp2, ch ); 
        	}
		resamp_frames = psynth_resampler_begin( data->resamp1, 0, frames );
		psynth_resampler_begin( data->resamp2, frames, resamp_frames );
		for( int ch = 0; ch < outputs_num; ch++ )
    		{
            	    PS_STYPE* in = inputs[ ch ] + offset;
	    	    smem_copy( resamp2_bufs[ ch ] + psynth_resampler_input_buf_offset( data->resamp2 ), in, frames * sizeof( PS_STYPE ) );
        	}
		retval = psynth_resampler_end( data->resamp2, input_signal?1:2, resamp2_bufs, resamp1_bufs2, outputs_num, 0 );
	    }
	    else retval = input_signal?1:2;
    	    data->bufs_clean = false;
	    int buf_ptr = data->buf_ptr;
	    int buf_size = data->buf_size;
	    int hop_size = data->buf_size;
	    if( data->ctl_overlap == 1 ) hop_size /= 2;
	    if( data->ctl_overlap == 2 ) hop_size /= 4;
	    int buf_ptr_mask = buf_size - 1;
	    int buf_ptr_mask2 = hop_size - 1;
	    for( int ch = 0; ch < outputs_num; ch++ )
    	    {
            	PS_STYPE* in = resamp_inputs[ ch ] + resamp_offset;
            	PS_STYPE* out = resamp_outputs[ ch ] + resamp_offset;
            	float* buf = data->bufs[ ch ]; 
            	float* buf2 = data->bufs2[ ch ]; 
		buf_ptr = data->buf_ptr;
		for( int i = 0; i < resamp_frames; i++ )
        	{
        	    if( retval )
        	    {
            		PS_STYPE_TO_FLOAT( buf[ buf_ptr ], in[ i ] );
            	    }
            	    else
            		buf[ buf_ptr ] = 0;
            	    PS_FLOAT_TO_STYPE( out[ i ], buf2[ buf_ptr & buf_ptr_mask2 ] );
            	    buf_ptr++;
            	    if( ( buf_ptr & buf_ptr_mask2 ) == 0 )
            	    {
                    	for( int t = 0; t < buf_size; t++ )
                    	{
                    	    float v = buf[ ( buf_ptr - buf_size + t ) & buf_ptr_mask ];
                    	    data->fft_r[ t ] = v;
                    	}
                    	if( data->ctl_overlap == 2 )
                    	{
                    	    for( int t = 0; t < buf_size; t++ ) data->fft_r[ t ] *= data->fft_win[ t ];
                    	}
            		if( data->ctl_feedback )
            		{
            		    data->feedback_bufs_clean = false;
            		    PS_STYPE2 fb = PS_NORM_STYPE( data->ctl_feedback, 32768 );
            		    float* fbuf = data->feedback_bufs[ ch ];
                    	    for( int t = 0; t < buf_size; t++ )
                    		data->fft_r[ t ] += PS_NORM_STYPE_MUL( fbuf[ t ], fb, 32768 );
            		}
            		memset( data->fft_i, 0, buf_size * sizeof( float ) );
                    	fft( 0, data->fft_i, data->fft_r, buf_size );
                    	bool mirror = false; 
                    	if( data->ctl_random_freqs )
                    	{
                    	    float width = (float)data->ctl_random_freqs / 32768.0F * (float)(buf_size/16);
                    	    if( width >= 1 )
                    	    {
                    		float c = 1.0F / 16384.0F * width;
                    		for( int t = 1; t < buf_size / 2; t++ )
                    		{
                    		    int rnd = ( (signed)pseudo_random( &data->noise_seed ) - 16384 ) * c;
                    		    int t2 = t + rnd;
                    		    if( t2 < 1 ) t2 = 1;
                    		    if( t2 >= buf_size / 2 ) t2 = buf_size / 2 - 1;
                    		    float re = data->fft_r[ buf_size - t2 ];
                    		    float im = -data->fft_i[ buf_size - t2 ];
                    		    data->fft_r[ t ] = re;
                    		    data->fft_i[ t ] = im;
                    		}
                    		mirror = true;
                    	    }
                    	}
                    	if( data->ctl_noisered )
                    	{
                    	    float min = (float)data->ctl_noisered / 32768.0f;
                    	    min *= buf_size/2;
                    	    min *= min;
                    	    for( int t = 0; t <= buf_size / 2; t++ )
                    	    {
                    		float re = data->fft_r[ t ];
                    		float im = data->fft_i[ t ];
                    		if( re * re + im * im < min )
                    		{
                    		    data->fft_r[ t ] = 0;
                    		    data->fft_i[ t ] = 0;
                    		}
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_phase_gain != 32768/2 )
                    	{
                    	    if( data->ctl_phase_gain == 0 )
                    		for( int t = 1; t < buf_size / 2; t++ )
                    		{
                    		    float re = data->fft_r[ t ];
                    		    float im = data->fft_i[ t ];
                    		    float mod = sqrt( re * re + im * im );
                    		    data->fft_r[ t ] = mod;
                    		    data->fft_i[ t ] = 0;
                    		}
                    	    else
                    	    {
                    		float g = (float)data->ctl_phase_gain / 16384.0f;
                    		for( int t = 1; t < buf_size / 2; t++ )
                    		{
                    		    float re = data->fft_r[ t ];
                    		    float im = data->fft_i[ t ];
                    		    float phase = atan2( im, re ) * g;
                    		    float mod = sqrt( re * re + im * im );
                    		    data->fft_r[ t ] = mod * cos( phase );
                    		    data->fft_i[ t ] = mod * sin( phase );
                    		}
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_allpass )
                    	{
                    	    float w = (float)data->ctl_allpass / 32768.0f;
                    	    float c = cos( w * M_PI * 2 );
                    	    float s = sin( w * M_PI * 2 );
                    	    for( int t = 1; t < buf_size / 2; t++ )
                    	    {
                    		float rr = data->fft_r[ t ];
                    		float ii = data->fft_i[ t ];
                    		data->fft_r[ t ] = rr * c - ii * s;
                    		data->fft_i[ t ] = rr * s + ii * c;
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_random_phase )
                    	{
                    	    float c = 1.0F / 16384.0F * (float)M_PI * (float)data->ctl_random_phase / 32768.0F;
                    	    for( int t = 1; t < buf_size / 2; t++ )
                    	    {
                    		float rnd = ( (signed)pseudo_random( &data->noise_seed ) - 16384 ) * c;
                    		float re = data->fft_r[ t ];
                    		float im = data->fft_i[ t ];
                    		float phase = atan2( im, re ) + rnd;
                    		float mod = sqrt( re * re + im * im );
                    		data->fft_r[ t ] = mod * cos( phase );
                    		data->fft_i[ t ] = mod * sin( phase );
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_random_phase_shift )
                    	{
                    	    float w = (float)pseudo_random( &data->noise_seed ) / 32768.0f;
                    	    w = w * data->ctl_random_phase_shift / 32768.0f;
                    	    float c = cos( w * M_PI * 2 );
                    	    float s = sin( w * M_PI * 2 );
                    	    for( int t = 1; t < buf_size / 2; t++ )
                    	    {
                    		float rr = data->fft_r[ t ];
                    		float ii = data->fft_i[ t ];
                    		data->fft_r[ t ] = rr * c - ii * s;
                    		data->fft_i[ t ] = rr * s + ii * c;
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_shift != 4096 )
                    	{
                    	    int shift = ( data->ctl_shift - 4096 ) * ( buf_size / 2 ) / 4096;
                    	    if( shift )
                    	    {
                    		float pr = data->fft_phase_rotate[ ch ];
                    		float pr_cos = cos( pr );
                    		float pr_sin = sin( pr );
                    		if( shift < 0 )
                    		{
                    		    int shift2 = -shift;
                    		    int t = 1; 
                    		    data->fft_r[ buf_size / 2 ] /= 2;
                    		    for( ; t <= buf_size / 2 - shift2; t++ )
                    		    {
                    			float re = data->fft_r[ t + shift2 ];
                    			float im = data->fft_i[ t + shift2 ];
                    			float re2 = re * pr_cos - im * pr_sin;
                    			float im2 = re * pr_sin + im * pr_cos;
                    			data->fft_r[ t ] = re2;
                    			data->fft_i[ t ] = im2;
                    		    }
                    		    for( ; t <= buf_size / 2; t++ )
                    		    {
                    			data->fft_r[ t ] = 0;
                    			data->fft_i[ t ] = 0;
                    		    }
                    		}
                    		else
                    		{
                    		    int t = buf_size / 2;
                    		    data->fft_r[ 0 ] /= 2;
                    		    for( ; t > shift ; t-- )
                    		    {
                    			float re = data->fft_r[ t - shift ];
                    			float im = data->fft_i[ t - shift ];
                    			float re2 = re * pr_cos - im * pr_sin;
                    			float im2 = re * pr_sin + im * pr_cos;
                    			data->fft_r[ t ] = re2;
                    			data->fft_i[ t ] = im2;
                    		    }
                    		    for( ; t > 0 ; t-- )
                    		    {
                    			data->fft_r[ t ] = 0;
                    			data->fft_i[ t ] = 0;
                    		    }
                    		}
                    		pr += (float)shift / (float)( 1 << data->ctl_overlap ) * 2.0f * (float)M_PI;
                    		data->fft_phase_rotate[ ch ] = fmod( pr, 2 * M_PI );
                    		mirror = true;
                    	    }
                    	}
                    	if( data->ctl_deform1 )
                    	{
                    	    for( int t = 1; t <= buf_size / 2; t++ )
                    	    {
                    		float re = data->fft_r[ t ];
                    		float im = data->fft_i[ t ];
                    		float phase = atan2( im, re ) * t * data->ctl_deform1 / 32768.0f;
                    		float mod = sqrt( re * re + im * im );
                    		data->fft_r[ t ] = mod * cos( phase );
                    		data->fft_i[ t ] = mod * sin( phase );
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_deform2 )
                    	{
                    	    float p = (float)data->ctl_deform2 / 32768.0f * 0.9;
                    	    float max = 1.0f - p;
                    	    float mul2 = sqrt( max );
                    	    max *= max;
                    	    max *= max;
                    	    float mul1 = 1.0f / max;
                    	    max = buf_size / 2;
                    	    for( int t = 1; t <= buf_size / 2; t++ )
                    	    {
                    		float re = data->fft_r[ t ];
                    		float im = data->fft_i[ t ];
                    		re *= mul1;
                    		im *= mul1;
                    		float rrii = re * re + im * im;
                    		if( rrii != 0 )
                    		{
                    		    float d = max / sqrt( rrii );
                    		    if( d < 1 )
                    		    {
                    			re *= d;
                    			im *= d;
                    		    }
                    		}
                    		data->fft_r[ t ] = re * mul2;
                    		data->fft_i[ t ] = im * mul2;
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_low || data->ctl_high != 32768 )
                    	{
                    	    int low = data->ctl_low * ( buf_size / 2 ) / 32768;
                    	    int high = data->ctl_high * ( buf_size / 2 ) / 32768;
                    	    if( high < low )
                    	    {
                    		for( int t = high; t <= low; t++ )
                    		{
                    		    data->fft_r[ t ] = 0;
                    		    data->fft_i[ t ] = 0;
                    		}
                    	    }
                    	    else
                    	    {
                    		if( low )
                    		{
                    		    for( int t = 0; t <= low; t++ )
                    		    {
                    			data->fft_r[ t ] = 0;
                    			data->fft_i[ t ] = 0;
                    		    }
                    		}
                    		for( int t = high; t <= buf_size / 2; t++ )
                    		{
                    		    data->fft_r[ t ] = 0;
                    		    data->fft_i[ t ] = 0;
                    		}
                    	    }
                    	    mirror = true;
                    	}
                    	if( data->ctl_volume != 32768 )
                    	{
                    	    float vol = data->ctl_volume / 32768.0f;
                    	    for( int t = 0; t <= buf_size / 2; t++ ) data->fft_r[ t ] *= vol;
                    	    for( int t = 0; t <= buf_size / 2; t++ ) data->fft_i[ t ] *= vol;
                    	    mirror = true;
			}
                    	if( mirror )
                    	{
                    	    for( int t = 0; t < buf_size / 2 - 1; t++ )
    				data->fft_r[ buf_size - 1 - t ] = data->fft_r[ t + 1 ];
                    	    for( int t = 0; t < buf_size / 2 - 1; t++ )
        			data->fft_i[ buf_size - 1 - t ] = -data->fft_i[ t + 1 ];
                    	}
                    	fft( FFT_FLAG_INVERSE, data->fft_i, data->fft_r, buf_size );
            		if( data->ctl_feedback )
            		{
            		    float* fbuf = data->feedback_bufs[ ch ];
            		    memmove( fbuf, data->fft_r, buf_size * sizeof( float ) );
            		}
                    	if( data->ctl_overlap )
                    	{
                    	    for( int t = 0; t < buf_size; t++ ) data->fft_r[ t ] *= data->fft_win[ t ];
                    	    if( data->ctl_overlap == 2 )
                    	    {
                    		for( int t = 0; t < buf_size; t++ ) data->fft_r[ t ] *= 1.0f / 1.5f;
                    	    }
                    	}
                    	if( data->ctl_overlap )
                    	{
                    	    memmove( buf2, buf2 + hop_size, ( buf_size - hop_size ) * sizeof( float ) );
                    	    for( int t = 0; t < buf_size - hop_size; t++ ) buf2[ t ] += data->fft_r[ t ];
                    	    memmove( buf2 + buf_size - hop_size, data->fft_r + buf_size - hop_size, hop_size * sizeof( float ) );
                    	}
                    	else
                    	{
                    	    for( int t = 0; t < buf_size; t++ )
                    		buf2[ t ] = data->fft_r[ t ];
                    	}
            		if( buf_ptr >= buf_size )
                    	    buf_ptr = 0;
            	    }
            	}
            }
            data->buf_ptr = buf_ptr;
            retval = 1;
	    if( data->use_resampler )
	    {
		retval = psynth_resampler_end( data->resamp1, retval, resamp1_bufs, outputs, outputs_num, offset );
	    }
	    if( !input_signal )
	    {
    		bool finished = false;
    	        if( data->ctl_feedback )
    	        {
    	    	    finished = true;
    	    	    if( retval )
			for( int ch = 0; ch < outputs_num; ch++ )
	    		{
            		    PS_STYPE* out = outputs[ ch ] + offset;
        	    	    for( int i = 0; i < frames; i++ )
                    		if( PS_STYPE_IS_NOT_MIN( out[ i ] ) )
                        	{
                            	    finished = false;
                        	    break;
                        	}
                        }
    	        }
    	        else
        	{
        	    if( data->empty_frames_counter < data->native_buf_size * 2 )
                	data->empty_frames_counter += frames;
            	    else
            		finished = true;
		}
		if( finished )
		{
            	    data->empty = true;
            	    data->empty_frames_counter = 0;
		    if( data->use_resampler )
		    {
			psynth_resampler_reset( data->resamp1 );
			psynth_resampler_reset( data->resamp2 );
		    }
		}
	    }
	    break;
	}
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
            switch( event->controller.ctl_num )
            {
                case 0: 
                    data->changed |= CHANGED_RESAMPLER;
                    break;
                case 1: 
                    data->changed |= CHANGED_IOCHANNELS;
                    break;
                case 2: 
                    data->changed |= CHANGED_BUF;
            	    break;
            }
            break;
        case PS_CMD_RESET_MSB:
            data->changed |= CHANGED_IOCHANNELS;
            break;
	case PS_CMD_CLOSE:
	    psynth_resampler_remove( data->resamp1 );
	    psynth_resampler_remove( data->resamp2 );
    	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
    	    {
    		smem_free( data->bufs[ ch ] );
    		smem_free( data->bufs2[ ch ] );
    		smem_free( data->feedback_bufs[ ch ] );
    	    }
    	    smem_free( data->fft_i );
    	    smem_free( data->fft_r );
    	    smem_free( data->fft_win );
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
