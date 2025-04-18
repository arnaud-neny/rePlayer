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
#define MODULE_DATA	psynth_kicker_data
#define MODULE_HANDLER	psynth_kicker
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	4
#define WAVETYPE_MAX	2
#define MAX_ENV_VOL	( 1024 * 64 )
struct gen_channel
{
    bool    	playing;
    uint	id;
    int	    	vel;
    uint    	ptr;
    uint    	delta;
    uint    	env_vol;
    int	    	sustain;
    bool    	accel;
    psynth_renderbuf_pars       renderbuf_pars;
    PS_CTYPE   	local_type;
    PS_CTYPE	local_pan;
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_type;
    PS_CTYPE   	ctl_pan;
    PS_CTYPE   	ctl_attack;
    PS_CTYPE   	ctl_release;
    PS_CTYPE   	ctl_vol_add;
    PS_CTYPE   	ctl_env_accel;
    PS_CTYPE   	ctl_channels;
    PS_CTYPE   	ctl_anticlick;
    gen_channel	channels[ MAX_CHANNELS ];
    bool    	no_active_channels;
    int	    	search_ptr;
    psmoother_coefs     smoother_coefs;
};
#define GET_VAL_TRIANGLE \
    PS_STYPE2 val; \
    if( ( ( ptr >> 16 ) & 15 ) < 8 ) \
    { \
	PS_INT16_TO_STYPE( val, (int)( -32767 + ( ( ptr & 0xFFFF ) / 8 + ( ( ( ptr >> 16 ) & 7 ) * 8192 ) ) ) ); \
    } \
    else \
    { \
	PS_INT16_TO_STYPE( val, (int)( 32767 - ( ( ptr & 0xFFFF ) / 8 + ( ( ( ptr >> 16 ) & 7 ) * 8192 ) ) ) ); \
    }
#define GET_VAL_RECTANGLE \
    PS_STYPE2 val = 0; \
    if( ( ptr >> 16 ) & 16 ) \
	val = max_val; \
    else \
	val = min_val;
#ifdef PS_STYPE_FLOATINGPOINT
#define GET_VAL_SIN \
    PS_STYPE2 val; \
    uint ptr2 = ptr + ( 128 << 12 ); \
    ptr2 &= ( ( 512 << 12 ) - 1 ); \
    val = PS_STYPE_SIN( M_PI * 2 * ( (PS_STYPE)ptr2 / (PS_STYPE)( 512 << 12 ) ) );
#else
#define GET_VAL_SIN \
    PS_STYPE2 val; \
    uint ptr2 = ptr + ( 128 << 12 ); \
    int v1, v2, cc; \
    cc = ( ptr2 << ( 15 - 12 ) ) & 32767; \
    v1 = -g_hsin_tab[ ( ptr2 >> 12 ) & 255 ] * 128; \
    v2 = -g_hsin_tab[ ( ( ptr2 >> 12 ) + 1 ) & 255 ] * 128; \
    v1 = ( v1 * ( 32767 - cc ) + v2 * cc ) >> 15; \
    if( ( ptr2 >> ( 12 + 8 ) ) & 1 ) v1 = -v1; \
    PS_INT16_TO_STYPE( val, v1 );
#endif
#define APPLY_ENV_VOLUME \
    uint new_env_vol = ( env_vol >> 14 ) + vol_add; \
    if( new_env_vol > MAX_ENV_VOL ) new_env_vol = MAX_ENV_VOL; \
    if( new_env_vol > ( env_vol >> ( 14 - 3 ) ) ) new_env_vol = ( env_vol >> ( 14 - 3 ) );   \
    val = val * new_env_vol / MAX_ENV_VOL;
#define LAST_PART_REPLACE \
    render_buf[ i ] = val; \
    ptr += ( delta * ( env_vol >> 20 ) ) / 1024; \
    if( sustain ) \
    { \
	env_vol += attack_delta; \
	if( env_vol >= ( 1 << 30 ) ) { env_vol = ( 1 << 30 ); sustain = 0; } \
    } \
    else \
    { \
	env_vol -= release_delta; \
	if( env_vol > env_accel << 20 ) \
	    env_vol -= release_delta; \
	else \
	    if( accel ) { accel = 0; env_vol = env_accel << 20; } \
	if( env_vol > ( 1 << 30 ) ) { env_vol = 0; playing = 0; i++; break; } \
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
	    retval = (PS_RETTYPE)"Kicker";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Генератор барабана \"бочка\".\nЛокальные контроллеры: Форма волны, Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)"Bass drum synthesizer.\nLocal controllers: Waveform, Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#009BFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 9, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 256, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WAVEFORM_TYPE ), ps_get_string( STR_PS_KICKER_WAVE_TYPES ), 0, WAVETYPE_MAX, 2, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 2, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), ps_get_string( STR_PS_SEC256 ), 0, 512, 0, 0, &data->ctl_attack, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), ps_get_string( STR_PS_SEC256 ), 0, 512, 32, 0, &data->ctl_release, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ADD_VOL ), "", 0, 1024, 0, 0, &data->ctl_vol_add, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ENV_ACCEL ), "", 0, 1024, 256, 0, &data->ctl_env_accel, 256, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 1, 1, &data->ctl_channels, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_NO_CLICK_AT_THE_BEGINNING ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_anticlick, -1, 2, pnet );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
		data->channels[ c ].accel = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    data->search_ptr = 0;
	    psmoother_init( &data->smoother_coefs, 100, pnet->sampling_freq );
	    psynth_get_temp_buf( mod_num, pnet, 0 ); 
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
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
                int outputs_num = MODULE_OUTPUTS;
		int attack_len;
                int attack_delta;
                int release_len;
                int release_delta;
        	uint env_accel;
		int vol_add;
                bool env_ready = false;
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( chan->playing == 0 ) continue;
                    data->no_active_channels = 0;
                    if( env_ready == false )
                    {
			attack_delta = 1 << 30;
			release_delta = 1 << 30;
			attack_len = ( pnet->sampling_freq * data->ctl_attack ) / 256;
			release_len = ( pnet->sampling_freq * data->ctl_release ) / 256;
			if( pnet->base_host_version < 0x01090600 ) { attack_len &= ~1023; release_len &= ~1023; }
			if( attack_len != 0 ) attack_delta = ( 1 << 30 ) / attack_len;
			if( release_len != 0 ) release_delta = ( 1 << 30 ) / release_len;
			env_accel = 1024 - data->ctl_env_accel;
			vol_add = data->ctl_vol_add;
			if( pnet->base_host_version <= 0x01070000 || pnet->base_host_version >= 0x01090400 )
			    vol_add = vol_add * MAX_ENV_VOL / 1024;
			env_ready = true;
		    }
		    PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, outputs, outputs_num, offset );
		    uint ptr = chan->ptr;
		    uint env_vol = chan->env_vol;
		    int sustain = chan->sustain;
		    bool accel = chan->accel;
		    bool playing = chan->playing;
		    uint delta = chan->delta;
		    int vol = data->ctl_volume * chan->vel / 2;
		    int rendered = 0;
		    {
			int i = 0;
			switch( chan->local_type )
			{
		    	    case 0: 
			    for( i = 0; i < frames; i++ ) {
				GET_VAL_TRIANGLE;
				APPLY_ENV_VOLUME;
			    	LAST_PART_REPLACE; }
			    break;
    			    case 1: 
    			    {
    			        PS_STYPE2 max_val;
			        PS_STYPE2 min_val;
			        PS_INT16_TO_STYPE( max_val, 32767 );
			        PS_INT16_TO_STYPE( min_val, -32767 );
			        if( vol != 32768 )
			        { 
			    	    max_val = max_val * vol / 32768;
			    	    min_val = min_val * vol / 32768;
				}
				for( i = 0; i < frames; i++ ) {
			    	    GET_VAL_RECTANGLE;
			    	    APPLY_ENV_VOLUME;
			    	    LAST_PART_REPLACE; }
			    }
			    break;
			    case 2: 
			    for( i = 0; i < frames; i++ ) {
				GET_VAL_SIN;
				APPLY_ENV_VOLUME;
				LAST_PART_REPLACE; }
			    break;
			}
			rendered = i;
		    }
		    chan->ptr = ptr;
		    chan->delta = delta;
		    chan->env_vol = env_vol;
		    chan->sustain = sustain;
		    chan->accel = accel;
		    chan->playing = playing;
		    if( !playing ) chan->id = ~0;
		    retval = psynth_renderbuf2output(
			retval,
			outputs, outputs_num, offset, frames,
			render_buf, NULL, rendered,
			vol, ( data->ctl_pan + ( chan->local_pan - 128 ) ) * 256,
			&chan->renderbuf_pars, &data->smoother_coefs,
			pnet->sampling_freq );
		} 
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
		chan->playing = 1;
		chan->vel = event->note.velocity;
		chan->delta = delta_l | ( delta_h << 16 );
		if( data->ctl_anticlick )
		{
		    if( data->ctl_type == 2 )
			chan->ptr = 128 << 12; 
		    else
			chan->ptr = 0 | ( 4 << 16 );
		}
		else
		    chan->ptr = 0;
		chan->id = event->id;
		chan->env_vol = 0;
		chan->sustain = 1;
		chan->accel = 1;
		chan->renderbuf_pars.start = true;
		chan->local_type = data->ctl_type;
		chan->local_pan = 128;
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
		    data->channels[ c ].delta = delta_l | ( delta_h << 16 );
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
			    chan->local_type = event->controller.ctl_val;
			    if( chan->local_type > WAVETYPE_MAX ) chan->local_type = WAVETYPE_MAX;
			    retval = 1;
			    break;
			case 2:
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
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
