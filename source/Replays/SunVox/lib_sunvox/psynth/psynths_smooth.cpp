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

#include "psynth_net.h"
#define MODULE_DATA	psynth_smooth_data
#define MODULE_HANDLER	psynth_smooth
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define CHANGED_IOCHANNELS      ( 1 << 0 )
#ifdef PS_STYPE_FLOATINGPOINT
    #define FP_PREC		1
#else
    #define FP_PREC		32768
#endif
struct MODULE_DATA
{
    PS_CTYPE		ctl_rise; 
    PS_CTYPE		ctl_fall; 
    PS_CTYPE		ctl_rise_only;
    PS_CTYPE		ctl_scale;
    PS_CTYPE		ctl_mono;
    PS_CTYPE		ctl_mode;
    PS_STYPE2		v[ MODULE_OUTPUTS ];
    PS_STYPE2		c1; 
    PS_STYPE2		c2; 
    uint32_t            changed; 
    bool		empty;
};
static void smooth_handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->changed & CHANGED_IOCHANNELS )
    {
	bool channels_changed = false;
	if( data->ctl_mono )
	{
	    if( psynth_get_number_of_outputs( mod ) != 1 )
	    {
		psynth_set_number_of_outputs( 1, mod_num, pnet );
		psynth_set_number_of_inputs( 1, mod_num, pnet );
		channels_changed = true;
	    }
	}
	else
	{
	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
	    {
		psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
		psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
		channels_changed = true;
	    }
	}
	if( channels_changed )
        {
	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		data->v[ ch ] = 0;
        }
    }
    data->changed = 0;
}
static void smooth_recalc( MODULE_DATA* data, psynth_net* pnet )
{
    PS_CTYPE ctl_rise = data->ctl_rise;
    PS_CTYPE ctl_fall = data->ctl_fall;
    if( data->ctl_rise_only ) ctl_fall = ctl_rise;
    PS_STYPE_F rise = ctl_rise;
    PS_STYPE_F fall = ctl_fall;
    PS_STYPE_F c1;
    PS_STYPE_F c2;
    if( data->ctl_scale != 100 )
    {
        rise = rise * data->ctl_scale / (PS_STYPE_F)100;
        fall = fall * data->ctl_scale / (PS_STYPE_F)100;
    }
    if( data->ctl_mode == 0 )
    {
        c1 = rise / (PS_STYPE_F)pnet->sampling_freq; 
        c2 = fall / (PS_STYPE_F)pnet->sampling_freq;
	data->c1 = c1 * (PS_STYPE_F)PS_STYPE_ONE * (PS_STYPE_F)FP_PREC;
	data->c2 = c2 * (PS_STYPE_F)PS_STYPE_ONE * (PS_STYPE_F)FP_PREC;
#ifndef PS_STYPE_FLOATINGPOINT
	if( data->c1 == 0 && ctl_rise != 0 ) data->c1 = 1;
	if( data->c2 == 0 && ctl_rise != 0 ) data->c2 = 1;
#endif
    }
    if( data->ctl_mode == 1 )
    {
        c1 = exp( (PS_STYPE_F)-2.0 * (PS_STYPE_F)M_PI * ( rise / (PS_STYPE_F)pnet->sampling_freq ) ); 
        c2 = exp( (PS_STYPE_F)-2.0 * (PS_STYPE_F)M_PI * ( fall / (PS_STYPE_F)pnet->sampling_freq ) );
	data->c1 = c1 * (PS_STYPE_F)FP_PREC;
	data->c2 = c2 * (PS_STYPE_F)FP_PREC;
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
	    retval = (PS_RETTYPE)"Smooth";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Модуль Smooth пытается повторить волну входящего сигнала, сглаживая резкие перепады в соответствии с параметрами ПОДЪЕМ и СПАД";
                        break;
                    }
		    retval = (PS_RETTYPE)"The Smooth module attempts to follow the waveform of the incoming signal, smoothing out sharp changes in accordance with the Rise and Fall parameters";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#C07FFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 6, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RISE ), "", 0, 32768, 5000, 0, &data->ctl_rise, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FALL ), "", 0, 32768, 5000, 0, &data->ctl_fall, -1, 0, pnet );
	    psynth_set_ctl_flags( mod_num, 0, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_set_ctl_flags( mod_num, 1, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FALL_IS_RISE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_rise_only, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SCALE ), "%", 0, 400, 100, 0, &data->ctl_scale, 100, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_SMOOTH_MODES ), 0, 1, 0, 1, &data->ctl_mode, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_STEREO_MONO ), 0, 1, 0, 1, &data->ctl_mono, -1, 1, pnet );
	    data->changed = 0xFFFFFFFF;
	    data->empty = true;
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
            smooth_handle_changes( mod, mod_num );
	    smooth_recalc( data, pnet );
            retval = 1;
            break;
	case PS_CMD_CLEAN:
	    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
		data->v[ ch ] = 0;
	    data->empty = true;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( data->changed ) smooth_handle_changes( mod, mod_num );
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
		if( input_signal == false )
		{
		    if( data->empty )
		    {
			break;
		    }
		}
		data->empty = false;
		PS_STYPE2 c1 = data->c1;
		PS_STYPE2 c2 = data->c2;
		if( data->ctl_mode == 0 )
		{
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			PS_STYPE* out = outputs[ ch ] + offset;
			PS_STYPE2 v = data->v[ ch ];
			if( input_signal )
			{
			    PS_STYPE* in = inputs[ ch ] + offset;
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 in_val = in[ i ] * FP_PREC;
				if( v < in_val )
			        {
				    v += c1;
				    if( v >= in_val ) v = in_val;
				}
				else
				{
				    v -= c2;
				    if( v < in_val ) v = in_val;
				}
				out[ i ] = v / FP_PREC;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
				if( v < 0 )
				{
				    v += c1;
				    if( v >= 0 ) v = 0;
				}
				else
				{
				    v -= c2;
				    if( v < 0 ) v = 0;
				}
				out[ i ] = v / FP_PREC;
			    }
			    if( PS_STYPE_IS_MIN( v / FP_PREC ) ) data->empty = true;
			}
			data->v[ ch ] = v;
		    }
		}
		if( data->ctl_mode == 1 )
		{
		    PS_STYPE2 c1_ = FP_PREC - c1;
		    PS_STYPE2 c2_ = FP_PREC - c2;
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			PS_STYPE* out = outputs[ ch ] + offset;
			PS_STYPE2 v = data->v[ ch ];
			if( input_signal )
			{
			    PS_STYPE* in = inputs[ ch ] + offset;
			    for( int i = 0; i < frames; i++ )
			    {
				PS_STYPE2 in_val = in[ i ] * FP_PREC;
#ifdef PS_STYPE_FLOATINGPOINT
				if( v < in_val )
				    v = v * c1 + in_val * c1_;
				else
				    v = v * c2 + in_val * c2_;
#else
				if( v < in_val )
				    v = ( (int64_t)v * c1 + (int64_t)in_val * c1_ ) / FP_PREC;
				else
				    v = ( (int64_t)v * c2 + (int64_t)in_val * c2_ ) / FP_PREC;
#endif
				out[ i ] = v / FP_PREC;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
#ifdef PS_STYPE_FLOATINGPOINT
				if( v < 0 )
				    v = v * c1;
				else
				    v = v * c2;
#else
				if( v < 0 )
				    v = (int64_t)v * c1 / FP_PREC;
				else
				    v = (int64_t)v * c2 / FP_PREC;
#endif
				out[ i ] = v / FP_PREC;
			    }
			    if( PS_STYPE_IS_MIN( v / FP_PREC ) ) data->empty = true;
			}
			data->v[ ch ] = v;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
            switch( event->controller.ctl_num )
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
            	    psynth_set_ctl2( mod, event );
        	    retval = 1;
		    smooth_recalc( data, pnet );
                    break;
                case 5: 
                    data->changed |= CHANGED_IOCHANNELS;
                    break;
            }
            break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
