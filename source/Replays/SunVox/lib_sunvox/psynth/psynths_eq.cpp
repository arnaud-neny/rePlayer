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
#define MODULE_DATA	psynth_eq_data
#define MODULE_HANDLER	psynth_eq
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
struct eq_filter
{
    PS_STYPE2 lf; 
    PS_STYPE2 f1p0[ MODULE_OUTPUTS ]; 
    PS_STYPE2 f1p1[ MODULE_OUTPUTS ];
    PS_STYPE2 f1p2[ MODULE_OUTPUTS ];
    PS_STYPE2 f1p3[ MODULE_OUTPUTS ];
    PS_STYPE2 hf; 
    PS_STYPE2 f2p0[ MODULE_OUTPUTS ]; 
    PS_STYPE2 f2p1[ MODULE_OUTPUTS ];
    PS_STYPE2 f2p2[ MODULE_OUTPUTS ];
    PS_STYPE2 f2p3[ MODULE_OUTPUTS ];
    PS_STYPE2 sdm1[ MODULE_OUTPUTS ]; 
    PS_STYPE2 sdm2[ MODULE_OUTPUTS ]; 
    PS_STYPE2 sdm3[ MODULE_OUTPUTS ]; 
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_lgain;
    PS_CTYPE   	ctl_mgain;
    PS_CTYPE   	ctl_hgain;
    PS_CTYPE   	ctl_mono;
    eq_filter	f;
    int	    	empty_frames_counter;
    int	    	empty_frames_counter_max;
};
static void eq_clean( eq_filter* e )
{
    for( int i = 0; i < MODULE_OUTPUTS; i++ )
    {
	e->f1p0[ i ] = 0;
	e->f1p1[ i ] = 0;
	e->f1p2[ i ] = 0;
	e->f1p3[ i ] = 0;
	e->f2p0[ i ] = 0;
	e->f2p1[ i ] = 0;
	e->f2p2[ i ] = 0;
	e->f2p3[ i ] = 0;
	e->sdm1[ i ] = 0;
	e->sdm2[ i ] = 0;
	e->sdm3[ i ] = 0;
    }
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
	    retval = (PS_RETTYPE)"EQ";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"3-полосный эквалайзер";
                        break;
                    }
		    retval = (PS_RETTYPE)"3Band Equalizer";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#7FFDFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 4, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_LOW ), "", 0, 512, 256, 0, &data->ctl_lgain, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MIDDLE ), "", 0, 512, 256, 0, &data->ctl_mgain, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_HIGH ), "", 0, 512, 256, 0, &data->ctl_hgain, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, 1, 1, pnet );
	    eq_clean( &data->f );
#ifdef PS_STYPE_FLOATINGPOINT
	    data->f.lf = 2 * sin( M_PI * ( (double)1000 / (double)pnet->sampling_freq ) ); 
	    data->f.hf = 2 * sin( M_PI * ( (double)5000 / (double)pnet->sampling_freq ) );
#else
	    {
		float f = ( ( 2 * M_PI * 1000 ) / (float)pnet->sampling_freq ) * 32768.0F;
		data->f.lf = (PS_STYPE2)f;
		f = ( ( 2 * M_PI * 5000 ) / (float)pnet->sampling_freq ) * 32768.0F;
		data->f.hf = (PS_STYPE2)f;
	    }
#endif
#ifdef PS_STYPE_FLOATINGPOINT
	    data->empty_frames_counter_max = pnet->sampling_freq * 4;
#else
	    data->empty_frames_counter_max = pnet->sampling_freq * 1;
#endif
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    eq_clean( &data->f );
	    data->empty_frames_counter = data->empty_frames_counter_max;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int frames = mod->frames;
		int offset = mod->offset;
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
		{
		    int outputs_num = psynth_get_number_of_outputs( mod );
		    bool input_signal = 0;
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			if( mod->in_empty[ ch ] < offset + frames )
			{
			    input_signal = 1;
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
		    PS_STYPE2 lf = data->f.lf;
		    PS_STYPE2 hf = data->f.hf;
#ifdef PS_STYPE_FLOATINGPOINT
		    PS_STYPE2 lgain = (PS_STYPE2)data->ctl_lgain / 256;
		    PS_STYPE2 mgain = (PS_STYPE2)data->ctl_mgain / 256;
		    PS_STYPE2 hgain = (PS_STYPE2)data->ctl_hgain / 256;
#else
		    PS_STYPE2 lgain = (PS_STYPE2)data->ctl_lgain;
		    PS_STYPE2 mgain = (PS_STYPE2)data->ctl_mgain;
		    PS_STYPE2 hgain = (PS_STYPE2)data->ctl_hgain;
#endif
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			PS_STYPE* in = inputs[ ch ] + offset;
			PS_STYPE* out = outputs[ ch ] + offset;
			PS_STYPE2 f1p0 = data->f.f1p0[ ch ];
			PS_STYPE2 f1p1 = data->f.f1p1[ ch ];
			PS_STYPE2 f1p2 = data->f.f1p2[ ch ];
			PS_STYPE2 f1p3 = data->f.f1p3[ ch ];
			PS_STYPE2 f2p0 = data->f.f2p0[ ch ];
			PS_STYPE2 f2p1 = data->f.f2p1[ ch ];
			PS_STYPE2 f2p2 = data->f.f2p2[ ch ];
			PS_STYPE2 f2p3 = data->f.f2p3[ ch ];
			PS_STYPE2 sdm1 = data->f.sdm1[ ch ];
			PS_STYPE2 sdm2 = data->f.sdm2[ ch ];
			PS_STYPE2 sdm3 = data->f.sdm3[ ch ];
			for( int i = 0; i < frames; i++ )
			{
			    PS_STYPE2 input = in[ i ];
			    PS_STYPE2 l, m, h; 
#ifdef PS_STYPE_FLOATINGPOINT
			    psynth_denorm_add_white_noise( input );
			    f1p0 += ( lf * ( input - f1p0 ) );
			    f1p1 += ( lf * ( f1p0 - f1p1 ) );
			    f1p2 += ( lf * ( f1p1 - f1p2 ) );
			    f1p3 += ( lf * ( f1p2 - f1p3 ) );
			    l = f1p3;
			    f2p0 += ( hf * ( input - f2p0 ) );
			    f2p1 += ( hf * ( f2p0 - f2p1 ) );
			    f2p2 += ( hf * ( f2p1 - f2p2 ) );
			    f2p3 += ( hf * ( f2p2 - f2p3 ) );
			    h = sdm3 - f2p3;
			    m = sdm3 - ( h + l );
			    l *= lgain;
			    m *= mgain;
			    h *= hgain;
			    sdm3 = sdm2;
			    sdm2 = sdm1;
			    sdm1 = input;
			    out[ i ] = l + m + h;
#else
			    input <<= 1; 
			    f1p0 += ( lf * ( input - f1p0 ) ) >> 15; 
			    f1p1 += ( lf * ( f1p0 - f1p1 ) ) >> 15;
			    f1p2 += ( lf * ( f1p1 - f1p2 ) ) >> 15;
			    f1p3 += ( lf * ( f1p2 - f1p3 ) ) >> 15;
			    l = f1p3;
			    f2p0 += ( hf * ( input - f2p0 ) ) >> 15;
			    f2p1 += ( hf * ( f2p0 - f2p1 ) ) >> 15;
			    f2p2 += ( hf * ( f2p1 - f2p2 ) ) >> 15;
			    f2p3 += ( hf * ( f2p2 - f2p3 ) ) >> 15;
			    h = sdm3 - f2p3;
			    m = sdm3 - ( h + l );
			    l *= lgain;
			    m *= mgain;
			    h *= hgain;
			    sdm3 = sdm2;
			    sdm2 = sdm1;
			    sdm1 = input;
			    out[ i ] = (PS_STYPE)( ( ( l + m + h ) / 256 ) >> 1 );
#endif
			}
			data->f.f1p0[ ch ] = f1p0;
			data->f.f1p1[ ch ] = f1p1;
			data->f.f1p2[ ch ] = f1p2;
			data->f.f1p3[ ch ] = f1p3;
			data->f.f2p0[ ch ] = f2p0;
			data->f.f2p1[ ch ] = f2p1;
			data->f.f2p2[ ch ] = f2p2;
			data->f.f2p3[ ch ] = f2p3;
			data->f.sdm1[ ch ] = sdm1;
			data->f.sdm2[ ch ] = sdm2;
			data->f.sdm3[ ch ] = sdm3;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
