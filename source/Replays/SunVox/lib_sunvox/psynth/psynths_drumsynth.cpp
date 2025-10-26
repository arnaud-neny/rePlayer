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
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_drumsynth_data
#define MODULE_HANDLER	psynth_drumsynth
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define DRUMSYNTH_SFREQ	44100
#define MAX_CHANNELS	8
#define ENV_POINTS	4
#define HT_TYPE_BAND	0
#define HT_TYPE_HIGH	1
#define HT_TYPE_PURE	8
inline uint32_t random1( int random_type, uint32_t* s )
{
    if( random_type == 0 )
    {
	return psynth_rand( s );
    }
    else
    {
	*s = *s * 1103515245 + 12;
	return ( *s & 32767 );
    }
}
struct gen_channel
{
    bool 	playing;
    bool	bd;
    bool	ht;
    uint8_t	type;
    uint    	id;
    int	    	vel;
    int		sin_state[ 2 ];
    int		sin_coef[ ENV_POINTS ];
    int		sin_amp[ ENV_POINTS ];
    int		sin_time[ ENV_POINTS ];
    int		sin_point;
    int		sin_point_counter;
    int		sin_point_coef;
    int		sin_point_coef_delta;
    int		sin_point_amp;
    int		sin_point_amp_delta;
    int 	ht_q;
    int 	ht_type; 
    int		ht_random_type;
    int		ht_state[ 2 ];
    int		ht_coef[ ENV_POINTS ];
    int		ht_amp[ ENV_POINTS ];
    int		ht_time[ ENV_POINTS ];
    int		ht_point;
    int		ht_point_counter;
    int		ht_point_coef;
    int		ht_point_coef_delta;
    int		ht_point_amp;
    int		ht_point_amp_delta;
    int		ht_prev_val;
    uint	ht_random;
    int		highpass; 
    PS_STYPE	highpass_prev_vals[ 4 ];
    psynth_renderbuf_pars       renderbuf_pars;
    PS_CTYPE       local_pan;
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_volume;
    PS_CTYPE		ctl_pan;
    PS_CTYPE		ctl_channels;
    PS_CTYPE		ctl_bd_vol;
    PS_CTYPE		ctl_bd_env;
    PS_CTYPE		ctl_bd_tone;
    PS_CTYPE		ctl_bd_length;
    PS_CTYPE		ctl_bd_pan;
    PS_CTYPE		ctl_ht_vol;
    PS_CTYPE		ctl_ht_length;
    PS_CTYPE		ctl_ht_pan;
    PS_CTYPE		ctl_sd_vol;
    PS_CTYPE		ctl_sd_tone;
    PS_CTYPE		ctl_sd_length;
    PS_CTYPE		ctl_sd_pan;
    gen_channel		channels[ MAX_CHANNELS ];
    bool    		no_active_channels;
    int	    		search_ptr;
#ifndef ONLY44100
    psynth_resampler*   resamp;
#endif
    psmoother_coefs     smoother_coefs;
#ifdef SUNVOX_GUI
    window_manager* 	wm;
#endif
};
#ifdef SUNVOX_GUI
struct ds_visual_data
{
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    int			min_ysize;
};
int ds_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    ds_visual_data* data = (ds_visual_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( ds_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->pnet = (psynth_net*)win->host;
	    data->min_ysize = font_string_y_size( ps_get_string( STR_PS_DRUMSYNTH_DESC ), win->font, wm );
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
	case EVT_DRAW:
	    wbd_lock( win );
	    draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
	    wm->cur_font_color = wm->color2;
	    draw_string( ps_get_string( STR_PS_DRUMSYNTH_DESC ), 0, 0, wm );
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
    }
    return retval;
}
#endif
static void drumsynth_reset_channel( gen_channel* chan )
{
    chan->playing = 0;
    chan->id = ~0;
    chan->sin_state[ 0 ] = 32768;
    chan->sin_state[ 1 ] = 0;
    chan->sin_point = -1;
    chan->bd = 0;
    chan->ht_type = 0;
    chan->ht_random_type = 0;
    chan->ht_q = 10000;
    chan->ht_state[ 0 ] = 0;
    chan->ht_state[ 1 ] = 0;
    chan->ht_point = -1;
    chan->ht_prev_val = 0;
    chan->ht_random *= 328753;
    chan->ht = 0;
    chan->highpass = 0;
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
	    retval = (PS_RETTYPE)"DrumSynth";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)
"Синтезатор ударных инструментов. Имеет встроенный набор звуков, сгруппированных по нотам:\n * C, C#, D, D# - бочка (bass);\n * E, F, F# - хай-хэт (hi-hat);\n * G, G#, A, A#, B - малый барабан (snare).\n"
"Звучит лучше на частоте дискретизации 44100Гц.\n"
"Локальные контроллеры: Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)
"Drum synthesizer with variety of predefined sounds.\nDistribution of sounds:\n * C, C#, D, D# - bass drum;\n * E, F, F# - hi-hat;\n * G, G#, A, A#, B - snare drum.\n"
"Sounds better at a sample rate of 44100Hz.\n"
"Local controllers: Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#00CBFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;
	case PS_CMD_INIT:
	    {
		int n;
		psynth_resize_ctls_storage( mod_num, 15, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
		n = psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
		psynth_set_ctl_show_offset( mod_num, n, -128, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 4, 1, &data->ctl_channels, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_BASS_VOLUME ), "", 0, 512, 200, 0, &data->ctl_bd_vol, 256, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_BASS_POWER ), "", 0, 256, 256, 0, &data->ctl_bd_env, 256, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_BASS_TONE ), "", 0, 256, 64, 0, &data->ctl_bd_tone, 256, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_BASS_LENGTH ), "", 0, 256, 64, 0, &data->ctl_bd_length, 256, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_HIHAT_VOLUME ), "", 0, 512, 256, 0, &data->ctl_ht_vol, 256, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_HIHAT_LENGTH ), "", 0, 256, 64, 0, &data->ctl_ht_length, 256, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_SNARE_VOLUME ), "", 0, 512, 256, 0, &data->ctl_sd_vol, 256, 3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_SNARE_TONE ), "", 0, 256, 128, 0, &data->ctl_sd_tone, 256, 3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_SNARE_LENGTH ), "", 0, 256, 64, 0, &data->ctl_sd_length, 256, 3, pnet );
		n = psynth_register_ctl( mod_num, ps_get_string( STR_PS_BASS_PAN ), "", 0, 256, 128, 0, &data->ctl_bd_pan, 128, 1, pnet );
		psynth_set_ctl_show_offset( mod_num, n, -128, pnet );
		n = psynth_register_ctl( mod_num, ps_get_string( STR_PS_HIHAT_PAN ), "", 0, 256, 128, 0, &data->ctl_ht_pan, 128, 2, pnet );
		psynth_set_ctl_show_offset( mod_num, n, -128, pnet );
		n = psynth_register_ctl( mod_num, ps_get_string( STR_PS_SNARE_PAN ), "", 0, 256, 128, 0, &data->ctl_sd_pan, 128, 3, pnet );
		psynth_set_ctl_show_offset( mod_num, n, -128, pnet );
	    }
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		drumsynth_reset_channel( &data->channels[ c ] );
		smem_clear( data->channels[ c ].highpass_prev_vals, sizeof( data->channels[ c ].highpass_prev_vals ) );
		data->channels[ c ].ht_random = 0;
	    }
	    data->no_active_channels = 1;
	    data->search_ptr = 0;
	    psmoother_init( &data->smoother_coefs, 100, DRUMSYNTH_SFREQ );
	    psynth_get_temp_buf( mod_num, pnet, 0 ); 
#ifndef ONLY44100
	    data->resamp = NULL;
	    if( pnet->sampling_freq != DRUMSYNTH_SFREQ )
	    {
                data->resamp = psynth_resampler_new( pnet, mod_num, DRUMSYNTH_SFREQ, pnet->sampling_freq, 0, 0 );
                for( int i = 0; i < MODULE_OUTPUTS; i++ ) psynth_resampler_input_buf( data->resamp, i );
            }
#endif
#ifdef SUNVOX_GUI
	    {
		data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "DrumSynth GUI", 0, 0, 10, 10, wm->color1, 0, pnet, ds_visual_handler, wm );
		    ds_visual_data* v_data = (ds_visual_data*)mod->visual->data;
		    mod->visual_min_ysize = v_data->min_ysize;
		    v_data->module_data = data;
		    v_data->mod_num = mod_num;
		    v_data->pnet = pnet;
		}
	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		drumsynth_reset_channel( &data->channels[ c ] );
	    }
	    data->no_active_channels = 1;
#ifndef ONLY44100
	    psynth_resampler_reset( data->resamp );
#endif
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
                int outputs_num = MODULE_OUTPUTS;
		PS_STYPE** resamp_outputs = outputs;
                uint resamp_offset = offset;
		uint resamp_frames = frames;
#ifndef ONLY44100
		PS_STYPE* resamp_bufs[ MODULE_OUTPUTS ];
		PS_STYPE* resamp_outputs2[ MODULE_OUTPUTS ];
                if( data->resamp )
                {
                    for( int ch = 0; ch < outputs_num; ch++ )
                    {
                        resamp_bufs[ ch ] = psynth_resampler_input_buf( data->resamp, ch );
                        resamp_outputs2[ ch ] = resamp_bufs[ ch ] + psynth_resampler_input_buf_offset( data->resamp );
                    }
                    resamp_outputs = &resamp_outputs2[ 0 ];
                    resamp_offset = 0;
                    resamp_frames = psynth_resampler_begin( data->resamp, 0, frames );
                }
#endif
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( !chan->playing ) continue;
		    data->no_active_channels = 0;
		    int ctl_volume = data->ctl_volume;
		    int chan_vol = ctl_volume;
		    PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, resamp_outputs, outputs_num, resamp_offset );
		    if( chan->bd )
		    {
		        if( chan->sin_point < 0 ) 
		        {
		    	    chan->sin_point = 0;
			    chan->sin_point_counter = chan->sin_time[ 1 ] - chan->sin_time[ 0 ];
			    chan->sin_point_coef_delta = ( ( chan->sin_coef[ 1 ] - chan->sin_coef[ 0 ] ) << 15 ) / chan->sin_point_counter;
			    chan->sin_point_coef = chan->sin_coef[ 0 ] << 15;
			    chan->sin_point_amp_delta = ( ( chan->sin_amp[ 1 ] - chan->sin_amp[ 0 ] ) << 15 ) / chan->sin_point_counter;
			    chan->sin_point_amp = chan->sin_amp[ 0 ] << 15;
			}
			uint i = 0;
			while( 1 )
			{
			    if( chan->sin_point_amp == ( 32768 << 15 ) && chan->sin_point_amp_delta == 0 )
			    {
			        for( ; i < resamp_frames && chan->sin_point_counter > 0; i++ )
			        {
			    	    int coef = chan->sin_point_coef >> 15;
				    chan->sin_state[ 0 ] = chan->sin_state[ 0 ] - ( ( coef * chan->sin_state[ 1 ] ) >> 15 );
				    chan->sin_state[ 1 ] = chan->sin_state[ 1 ] + ( ( coef * chan->sin_state[ 0 ] ) >> 15 );
				    int iv = chan->sin_state[ 1 ];
				    PS_STYPE2 v;
				    PS_INT16_TO_STYPE( v, iv );
				    render_buf[ i ] = v;
				    chan->sin_point_coef += chan->sin_point_coef_delta;
				    chan->sin_point_counter--;
				}
			    }
			    else
			    {
			        for( ; i < resamp_frames && chan->sin_point_counter > 0; i++ )
			        {
			    	    int coef = chan->sin_point_coef >> 15;
				    chan->sin_state[ 0 ] = chan->sin_state[ 0 ] - ( ( coef * chan->sin_state[ 1 ] ) >> 15 );
				    chan->sin_state[ 1 ] = chan->sin_state[ 1 ] + ( ( coef * chan->sin_state[ 0 ] ) >> 15 );
				    int iv = chan->sin_state[ 1 ];
				    iv *= chan->sin_point_amp >> 15;
				    iv >>= 15;
				    PS_STYPE2 v;
				    PS_INT16_TO_STYPE( v, iv );
				    render_buf[ i ] = v;
				    chan->sin_point_coef += chan->sin_point_coef_delta;
				    chan->sin_point_amp += chan->sin_point_amp_delta;
				    chan->sin_point_counter--;
				}
			    }
			    if( chan->sin_point_counter > 0 ) break;
			    chan->sin_point++;
			    if( chan->sin_point > ENV_POINTS - 2 )
			    {
			        chan->playing = 0;
			        for( ; i < resamp_frames; i++ ) render_buf[ i ] = 0;
			        break;
			    }
			    else
			    {
			        chan->sin_point_counter = chan->sin_time[ chan->sin_point + 1 ] - chan->sin_time[ chan->sin_point ];
			        chan->sin_point_coef_delta = ( ( chan->sin_coef[ chan->sin_point + 1 ] - chan->sin_coef[ chan->sin_point ] ) << 15 ) / chan->sin_point_counter;
			        chan->sin_point_amp_delta = ( ( chan->sin_amp[ chan->sin_point + 1 ] - chan->sin_amp[ chan->sin_point ] ) << 15 ) / chan->sin_point_counter;
			    }
			}
		    }
		    if( chan->ht )
		    {
		        int q = chan->ht_q;
		        int type = chan->ht_type;
		        int random_type = chan->ht_random_type;
		        int d1 = chan->ht_state[ 0 ];
		        int d2 = chan->ht_state[ 1 ];
		        if( chan->ht_point < 0 ) 
		        {
		    	    chan->ht_point = 0;
			    chan->ht_point_counter = chan->ht_time[ 1 ] - chan->ht_time[ 0 ];
			    chan->ht_point_coef_delta = ( ( chan->ht_coef[ 1 ] - chan->ht_coef[ 0 ] ) << 15 ) / chan->ht_point_counter;
			    chan->ht_point_coef = chan->ht_coef[ 0 ] << 15;
			    chan->ht_point_amp_delta = ( ( chan->ht_amp[ 1 ] - chan->ht_amp[ 0 ] ) << 15 ) / chan->ht_point_counter;
			    chan->ht_point_amp = chan->ht_amp[ 0 ] << 15;
			}
			uint i = 0;
			while( 1 )
			{
			    if( chan->ht_point_amp == ( 32768 << 15 ) && chan->ht_point_amp_delta == 0 )
			    {
			        if( type == HT_TYPE_PURE )
			        {
			    	    for( ; i < resamp_frames && chan->ht_point_counter > 0; i++ )
				    {
				        int iv = ( random1( random_type, &chan->ht_random ) * 2 ) - 32768;
				        PS_STYPE2 v = 0;
				        PS_INT16_TO_STYPE( v, iv );
				        render_buf[ i ] = v;
				        chan->ht_point_counter--;
				    }
				}
				else
				{
				    for( ; i < resamp_frames && chan->ht_point_counter > 0; i++ )
				    {
				        int coef = chan->ht_point_coef >> 15;
				        int iv = ( random1( random_type, &chan->ht_random ) * 2 ) - 32768;
				        iv /= 2;
				        int low = d2 + ( ( coef * d1 ) >> 15 );
				        int high = iv - low - ( ( q * d1 ) >> 15 );
				        int band = ( ( coef * high ) >> 15 ) + d1;
				        d1 = band;
				        d2 = low;
				        if( type == 0 )
				    	    iv = band;
					else
					    iv = high;
					iv *= 2;
					PS_STYPE2 v = 0;
					PS_INT16_TO_STYPE( v, iv );
					render_buf[ i ] = v;
					chan->ht_point_coef += chan->ht_point_coef_delta;
					chan->ht_point_counter--;
				    }
				}
			    }
			    else
			    {
			        if( type == HT_TYPE_PURE )
			        {
			    	    for( ; i < resamp_frames && chan->ht_point_counter > 0; i++ )
				    {
				        int iv = ( random1( random_type, &chan->ht_random ) * 2 ) - 32768;
				        iv *= chan->ht_point_amp >> 16;
				        iv >>= 14;
				        PS_STYPE2 v;
				        PS_INT16_TO_STYPE( v, iv );
				        render_buf[ i ] = v;
				        chan->ht_point_amp += chan->ht_point_amp_delta;
				        chan->ht_point_counter--;
				    }
				}
				else
				{
				    for( ; i < resamp_frames && chan->ht_point_counter > 0; i++ )
				    {
				        int coef = chan->ht_point_coef >> 15;
				        int iv = ( random1( random_type, &chan->ht_random ) * 2 ) - 32768;
				        iv /= 2;
				        int low = d2 + ( ( coef * d1 ) >> 15 );
				        int high = iv - low - ( ( q * d1 ) >> 15 );
				        int band = ( ( coef * high ) >> 15 ) + d1;
				        d1 = band;
				        d2 = low;
				        if( type == 0 )
				    	    iv = band;
					else
					    iv = high;
					iv *= 2;
					iv *= chan->ht_point_amp >> 16;
					iv >>= 14;
					PS_STYPE2 v;
					PS_INT16_TO_STYPE( v, iv );
					render_buf[ i ] = v;
					chan->ht_point_coef += chan->ht_point_coef_delta;
					chan->ht_point_amp += chan->ht_point_amp_delta;
					chan->ht_point_counter--;
				    }
				}
			    }
			    if( chan->ht_point_counter > 0 ) break;
			    chan->ht_point++;
			    if( chan->ht_point > ENV_POINTS - 2 )
			    {
			        chan->playing = 0;
			        for( ; i < resamp_frames; i++ ) render_buf[ i ] = 0;
			        break;
			    }
			    else
			    {
			        chan->ht_point_counter = chan->ht_time[ chan->ht_point + 1 ] - chan->ht_time[ chan->ht_point ];
			        chan->ht_point_coef_delta = ( ( chan->ht_coef[ chan->ht_point + 1 ] - chan->ht_coef[ chan->ht_point ] ) << 15 ) / chan->ht_point_counter;
			        chan->ht_point_amp_delta = ( ( chan->ht_amp[ chan->ht_point + 1 ] - chan->ht_amp[ chan->ht_point ] ) << 15 ) / chan->ht_point_counter;
			    }
			}
			chan->ht_state[ 0 ] = d1;
			chan->ht_state[ 1 ] = d2;
		    }
		    for( int h = 0; h < chan->highpass; h++ )
		    {
		        for( uint i = 0; i < resamp_frames; i++ )
		        {
		    	    PS_STYPE v = render_buf[ i ];
			    render_buf[ i ] = ( v - chan->highpass_prev_vals[ h ] ) / 2;
			    chan->highpass_prev_vals[ h ] = v;
			}
		    }
		    if( !chan->playing ) chan->id = ~0;
		    int pan = data->ctl_pan + ( chan->local_pan - 128 );
		    switch( chan->type )
		    {
		        case 0: pan += ( data->ctl_bd_pan - 128 ); break;
		        case 1: pan += ( data->ctl_ht_pan - 128 ); break;
		        case 2: pan += ( data->ctl_sd_pan - 128 ); break;
		        default: break;
		    }
		    retval = psynth_renderbuf2output(
		        retval,
		        resamp_outputs, outputs_num, resamp_offset, resamp_frames,
		        render_buf, NULL, resamp_frames,
		        chan_vol * chan->vel / 2, pan * 256,
		        &chan->renderbuf_pars, &data->smoother_coefs,
		        DRUMSYNTH_SFREQ );
		} 
#ifndef ONLY44100
		if( data->resamp ) retval = psynth_resampler_end( data->resamp, retval, resamp_bufs, outputs, outputs_num, offset );
#endif
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
                            data->channels[ c ].id = ~0;
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
		c = data->search_ptr;
		gen_channel* chan = &data->channels[ c ];
		drumsynth_reset_channel( chan );
		chan->playing = 1;
		chan->vel = event->note.velocity;
		chan->id = event->id;
		chan->local_pan = 128;
		chan->renderbuf_pars.start = true;
		int n;
		if( pnet->base_host_version == 0x01070000 )
		    n = event->note.root_note;
		else
		{
		    n = PS_PITCH_TO_NOTE( event->note.pitch );
		    if( n < 0 ) n = 0;
		    if( n >= 10 * 12 ) n = 10 * 12 - 1;
		}
		int oct = n / 12;
		int nn = n - oct * 12;
		int bd_env = data->ctl_bd_env;
		int bd_tone = data->ctl_bd_tone;
		bd_tone *= bd_tone; bd_tone /= 256;
		int bd_len = data->ctl_bd_length;
		int ht_len = data->ctl_ht_length;
		int sd_len = data->ctl_sd_length;
		int sd_tone = data->ctl_sd_tone;
		sd_tone *= sd_tone; sd_tone /= 256;
		switch( nn )
		{
		    case 0:
		    case 1:
		    case 2:
		    case 3:
			chan->type = 0;
			chan->vel = ( chan->vel * data->ctl_bd_vol ) / 256;
			chan->bd = 1;
			switch( oct )
			{
			    case 0:
				chan->sin_coef[ 0 ] = 10000;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 0;
				chan->sin_amp[ 2 ] = 0;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1;
				break;
			    case 1:
				chan->sin_coef[ 0 ] = 0;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 0;
				chan->sin_amp[ 2 ] = 0;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1;
				break;
			    case 2:
				chan->sin_coef[ 0 ] = 5000;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 0;
				chan->sin_amp[ 2 ] = 0;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1;
				break;
			    case 3:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 0;
				chan->sin_amp[ 2 ] = 0;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1;
				break;
			    case 4:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 200 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * (128+bd_env/2) ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 100 + ( 2000 * bd_tone ) / 256;
				chan->sin_amp[ 2 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1 + ( 6000 * bd_len ) / 128;
				break;
			    case 5:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 200 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * (128+bd_env/2) ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 90 + ( 2000 * bd_tone ) / 256;
				chan->sin_amp[ 2 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 400 + ( 15000 * bd_tone ) / 256;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1 + ( 3000 * bd_len ) / 128;
				break;
			    case 6:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 0;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 200 + ( 4000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * (128+bd_env/2) ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 80 + ( 2000 * bd_tone ) / 256;
				chan->sin_amp[ 2 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1 + ( 6000 * bd_len ) / 128;
				break;
			    case 7:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 8000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 32768 * (128+bd_env/2) ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 200 + ( 2000 * bd_tone ) / 256;
				chan->sin_amp[ 2 ] = ( 32768 * bd_env ) / 256;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1 + ( 6000 * bd_len ) / 128;
				break;
			    case 8:
				chan->sin_coef[ 0 ] = 32768;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				chan->sin_coef[ 1 ] = 400 + ( 8000 * bd_tone ) / 256;
				chan->sin_amp[ 1 ] = ( 20768 * (128+bd_env/2) ) / 256;
				chan->sin_time[ 1 ] = 8;
				chan->sin_coef[ 2 ] = 200 + ( 2000 * bd_tone ) / 256;
				chan->sin_amp[ 2 ] = ( 10768 * bd_env ) / 256;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 6000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1 + ( 6000 * bd_len ) / 128;
				break;
			    case 9:
				chan->sin_coef[ 0 ] = 20 + ( 32768 * bd_tone ) / 256;
				chan->sin_amp[ 0 ] = 32768;
				chan->sin_time[ 0 ] = 0;
				{
				    int div = 2 + nn;
				    chan->sin_coef[ 1 ] = chan->sin_coef[ 0 ] / div;
				    chan->sin_amp[ 1 ] = ( ( ( 20 + 32768 ) / div ) * bd_env ) / 256;
				    chan->sin_time[ 1 ] = 8 + ( 10000 * bd_len ) / 128;
				}
				chan->sin_coef[ 2 ] = 0;
				chan->sin_amp[ 2 ] = 0;
				chan->sin_time[ 2 ] = chan->sin_time[ 1 ] + 1 + ( 10000 * bd_len ) / 128;
				chan->sin_coef[ 3 ] = 0;
				chan->sin_amp[ 3 ] = 0;
				chan->sin_time[ 3 ] = chan->sin_time[ 2 ] + 1;
				break;
			}
			switch( nn )
			{
			    case 1:
				chan->sin_time[ 1 ] += 50;
				chan->sin_time[ 2 ] += 50;
				chan->sin_time[ 3 ] += 50;
				break;
			    case 2:
				chan->sin_time[ 1 ] += 200;
				chan->sin_time[ 2 ] += 200;
				chan->sin_time[ 3 ] += 200;
				break;
			    case 3:
				chan->sin_time[ 1 ] += 400;
				chan->sin_time[ 2 ] += 400;
				chan->sin_time[ 3 ] += 400;
				break;
			}
			break;
		    case 4:
		    case 5:
		    case 6:
			chan->type = 1;
			chan->vel = ( chan->vel * data->ctl_ht_vol ) / 256;
			chan->ht = 1;
			chan->ht_q = 20000;
			chan->ht_coef[ 0 ] = 22000;
			chan->ht_coef[ 1 ] = 22000;
			chan->ht_coef[ 2 ] = 22000;
			chan->ht_coef[ 3 ] = 0;
			switch( oct )
			{
			    case 0:
				chan->ht_amp[ 0 ] = 0;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32878;
				chan->ht_time[ 1 ] = 1 + ( 5000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + 0;
				break;
			    case 1:
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32878;
				chan->ht_time[ 1 ] = 1 + ( 5000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 1000;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + 5000;
				break;
			    case 2:
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 0;
				chan->ht_time[ 1 ] = 1 + ( 1000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 32878;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 1000 * ht_len ) / 128;
				break;
			    case 3:
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32768 / 4;
				chan->ht_time[ 1 ] = 1 + ( 3000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 50000 * ht_len ) / 128;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1;
				break;
			    case 4:
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32768 / 2;
				chan->ht_time[ 1 ] = 1 + ( 5000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 30000 * ht_len ) / 128;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1;
				break;
			    case 5:
				chan->ht_amp[ 0 ] = 0;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32768;
				chan->ht_time[ 1 ] = 1 + ( 2000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 2000 * ht_len ) / 128;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1;
				break;
			    case 6:
				chan->ht_amp[ 0 ] = 0;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 32878 / 3;
				chan->ht_time[ 1 ] = 1 + ( 30000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 32768;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 60000 * ht_len ) / 128;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 2000 * ht_len ) / 128;
				break;
			    case 7:
			    case 8:
			    case 9:
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_amp[ 1 ] = 0;
				chan->ht_time[ 1 ] = 1 + ( 2000 * ht_len ) / 128;
				chan->ht_amp[ 2 ] = 32768;
				if( oct == 8 ) chan->ht_amp[ 2 ] /= 3;
				if( oct == 9 ) chan->ht_amp[ 2 ] /= 6;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 2000 * ht_len ) / 128;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 2000 * ht_len ) / 128;
				break;
			}
			switch( nn )
			{
			    case 4: 
				chan->ht_type = HT_TYPE_PURE;
				chan->highpass = 1;
				break;
			    case 5: 
				chan->ht_type = HT_TYPE_PURE;
				chan->highpass = 2;
				break;
			    case 6: 
				chan->ht_type = HT_TYPE_PURE;
				chan->highpass = 2;
				chan->ht_time[ 1 ] /= 4;
				chan->ht_time[ 1 ] += 1;
				chan->ht_time[ 2 ] /= 4;
				chan->ht_time[ 2 ] += 4;
				chan->ht_time[ 3 ] /= 4;
				chan->ht_time[ 3 ] += 8;
				break;
			}
			break;
		    case 7:
		    case 8:
		    case 9:
		    case 10:
		    case 11:
			chan->type = 2;
			chan->vel = ( chan->vel * data->ctl_sd_vol ) / 256;
			chan->ht = 1;
			if( ( nn - 7 ) & 1 )
			    chan->ht_type = HT_TYPE_HIGH;
			else
			    chan->ht_type = HT_TYPE_BAND;
			if( nn == 11 )
			{
			    chan->ht_random_type = 1;
			    chan->ht_random = 1;
			    chan->ht_type = HT_TYPE_BAND;
			}
			if( nn == 10 )
			{
			    chan->ht_random_type = 1;
			    chan->ht_random = 0;
			    chan->ht_type = HT_TYPE_BAND;
			}
			if( nn == 9 )
			    chan->ht_q = 40000;
			else
			    chan->ht_q = 20000;
			switch( oct )
			{
			    case 0:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 1 ] = 32768;
				chan->ht_time[ 1 ] = 1 + ( 8000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 2 ] = 5000;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_coef[ 3 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 30000 * sd_len ) / 128;
				break;
			    case 1:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = 0;
				chan->ht_amp[ 1 ] = 0;
				chan->ht_time[ 1 ] = 1 + ( 10000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = 0;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_coef[ 3 ] = 0;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1;
				break;
			    case 2:
			    case 3:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = 100;
				chan->ht_amp[ 1 ] = 0;
				chan->ht_time[ 1 ] = 1 + ( 5000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = 100 + ( 11000 * sd_tone ) / 256;
				chan->ht_amp[ 2 ] = 20000;
				if( oct == 3 )
				    chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 5000 * sd_len ) / 128;
				else    
				    chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 12000 * sd_len ) / 128;
				chan->ht_coef[ 3 ] = 0;
				chan->ht_amp[ 3 ] = 0;
				if( oct == 3 )
				    chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 5000 * sd_len ) / 128;
				else
				    chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 15000 * sd_len ) / 128;
				break;
			    case 4:
			    case 5:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				if( oct == 4 )
				    chan->ht_coef[ 1 ] = chan->ht_coef[ 0 ] / 2;
				else
				    chan->ht_coef[ 1 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 1 ] = 32768 / 2;
				chan->ht_time[ 1 ] = 1 + ( 10000 * sd_len ) / 128;
				if( oct == 4 )
				    chan->ht_coef[ 2 ] = chan->ht_coef[ 1 ] / 2;
				else
				    chan->ht_coef[ 2 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 2 ] = 32768 / 4;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 10000 * sd_len ) / 128;
				if( oct == 4 )
				    chan->ht_coef[ 3 ] = 1;
				else
				    chan->ht_coef[ 3 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 15000 * sd_len ) / 128;
				break;
			    case 6:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 1 ] = 5000;
				chan->ht_time[ 1 ] = 1 + ( 5000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 2 ] = 2000;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 10000 * sd_len ) / 128;
				chan->ht_coef[ 3 ] = chan->ht_coef[ 0 ];
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 20000 * sd_len ) / 128;
				break;
			    case 7:
				chan->ht_coef[ 0 ] = 1;
				chan->ht_amp[ 0 ] = 0;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 1 ] = 32768;
				chan->ht_time[ 1 ] = 1 + ( 5000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = 0;
				chan->ht_amp[ 2 ] = 0;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1 + ( 5000 * sd_len ) / 128;
				chan->ht_coef[ 3 ] = 0;
				chan->ht_amp[ 3 ] = 0;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1;
				break;
			    case 8:
			    case 9:
				chan->ht_coef[ 0 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 0 ] = 32768;
				chan->ht_time[ 0 ] = 0;
				chan->ht_coef[ 1 ] = 0;
				if( oct == 8 )
				    chan->ht_amp[ 1 ] = 0;
				else
				    chan->ht_amp[ 1 ] = 32768;
				chan->ht_time[ 1 ] = 1 + ( 2000 * sd_len ) / 128;
				chan->ht_coef[ 2 ] = 100 + ( 22000 * sd_tone ) / 256;
				chan->ht_amp[ 2 ] = 32768;
				chan->ht_time[ 2 ] = chan->ht_time[ 1 ] + 1;
				chan->ht_coef[ 3 ] = 0;
				if( oct == 8 )
				    chan->ht_amp[ 3 ] = 0;
				else
				    chan->ht_amp[ 3 ] = 32768;
				chan->ht_time[ 3 ] = chan->ht_time[ 2 ] + 1 + ( 2000 * sd_len ) / 128;
				break;
			}
			break;
		    default: break;
		}
		data->no_active_channels = 0;
		retval = 1;
	    }
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
	case PS_CMD_SET_LOCAL_CONTROLLER:
            if( data->no_active_channels ) break;
            for( int c = 0; c < data->ctl_channels; c++ )
            {
                gen_channel* chan = &data->channels[ c ];
                if( chan->id == event->id )
                {
                    switch( event->controller.ctl_num )
                    {
                        case 1:
                            chan->local_pan = event->controller.ctl_val >> 7;
                            retval = 1;
                            break;
                        default: break;
                    }
                    break;
                }
            }
            break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
            {
		remove_window( mod->visual, data->wm );
		mod->visual = 0;
	    }
#endif
#ifndef ONLY44100
	    psynth_resampler_remove( data->resamp );
#endif	    
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
