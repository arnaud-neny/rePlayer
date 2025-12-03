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
#define MODULE_DATA	psynth_generator_data
#define MODULE_HANDLER	psynth_generator
#define MODULE_INPUTS	1
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	16
int8_t g_gen_drawn_waveform[ 32 ] =
{
    0,
    -100,
    -90,
    0,
    90,
    -119,
    -20,
    45,
    2,
    -20,
    111,
    -23,
    2,
    -98,
    60,
    32,
    100,
    50,
    0,
    -50,
    65,
    98,
    50,
    32,
    -90,
    -120,
    100,
    90,
    59,
    21,
    0,
    54
};
struct gen_channel
{
    bool	playing;
    uint    	id; 
    int	    	vel;
    uint32_t   	ptr; 
    uint32_t   	delta; 
    uint    	env_vol;
    int	    	sustain;
    PS_CTYPE   	local_type;
    PS_CTYPE   	local_pan;
};
enum {
    MODE_STEREO = 0,
    MODE_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_type;
    PS_CTYPE   	ctl_pan;
    PS_CTYPE   	ctl_attack;
    PS_CTYPE   	ctl_release;
    PS_CTYPE   	ctl_channels;
    PS_CTYPE   	ctl_mode;
    PS_CTYPE   	ctl_sustain;
    PS_CTYPE   	ctl_mod;
    PS_CTYPE   	ctl_duty_cycle;
    gen_channel	channels[ MAX_CHANNELS ];
    bool	no_active_channels;
    int8_t*	noise_data;
    int8_t     	drawn_wave[ 32 ];
    bool	drawn_wave_changed;
    int	        search_ptr;
    bool        wrong_saw_generation;
    uint*       linear_freq_tab;
    uint8_t*	vibrato_tab;
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#define GET_VAL_TRIANGLE \
    if( ( (ptr>>16) & 15 ) < 8 ) \
    { \
	PS_INT16_TO_STYPE( val, -32767 + (signed)( (ptr>>3) & 65535 ) ); \
    } \
    else \
    { \
	PS_INT16_TO_STYPE( val, 32767 - (signed)( (ptr>>3) & 65535 ) ); \
    }
#define GET_VAL_SAW \
    PS_INT16_TO_STYPE( val, 32767 - (signed)( (ptr>>4) & 65535 ) );
#define GET_VAL_SAW2 \
    PS_INT16_TO_STYPE( val, 32767 - (signed)( (ptr&65535) / 32 + ( ( (ptr>>16) & 31 ) * 4096 ) ) );
#define GET_VAL_RECTANGLE \
    val = min_val; \
    if( ( (ptr>>11) & 1023 ) > duty_cycle ) val = max_val;
#define GET_VAL_NOISE \
    PS_INT16_TO_STYPE( val, noise_data[ ( (ptr>>16) + add ) & 32767 ] * 256 );
#define GET_VAL_DIRTY \
    PS_INT16_TO_STYPE( val, drawn_wave[ (ptr>>18) & 31 ] * 256 );
#ifdef PS_STYPE_FLOATINGPOINT
#define GET_VAL_SIN \
    { \
	int ptr2 = ( ptr - ( 4 << 16 ) ) & ( ( 1 << 21 ) - 1 ); \
	float sin_ptr = (float)ptr2 / (float)( 1 << 20 ); \
	val = PS_STYPE_SIN( sin_ptr * (float)M_PI ); \
    }
#else
#define GET_VAL_SIN \
    { \
	int ptr2 = ptr - ( 4 << 16 ); \
	if( ptr2 & ( 16 << 16 ) ) \
	{ \
	    PS_INT16_TO_STYPE( val, -vibrato_tab[ ( ptr2 >> 12 ) & 255 ] * 128 ); \
	} \
	else \
	{ \
	    PS_INT16_TO_STYPE( val, vibrato_tab[ ( ptr2 >> 12 ) & 255 ] * 128 ); \
	} \
    }
#endif
#ifdef PS_STYPE_FLOATINGPOINT
#define GET_VAL_HSIN \
    { \
	int ptr2 = ( ptr - ( 4 << 16 ) ) & ( ( 1 << 21 ) - 1 ); \
	val = 0; \
	if( ( ptr2 & ( 16 << 16 ) ) == 0 ) { \
	    float sin_ptr = (float)ptr2 / (float)( 1 << 20 ); \
	    val = PS_STYPE_SIN( sin_ptr * (float)M_PI ); \
	} \
    }
#else
#define GET_VAL_HSIN \
    { \
	int ptr2 = ptr - ( 4 << 16 ); \
	val = 0; \
	if( ( ptr2 & ( 16 << 16 ) ) == 0 ) { \
	    PS_INT16_TO_STYPE( val, vibrato_tab[ ( ptr2 >> 12 ) & 255 ] * 128 ); \
	} \
    }
#endif
#ifdef PS_STYPE_FLOATINGPOINT
#define GET_VAL_ASIN \
    { \
	int ptr2 = ( ptr - ( 4 << 16 ) ) & ( ( 1 << 20 ) - 1 ); \
	float sin_ptr = (float)ptr2 / (float)( 1 << 20 ); \
	val = PS_STYPE_SIN( sin_ptr * (float)M_PI ); \
    }
#else
#define GET_VAL_ASIN \
    { \
	int ptr2 = ptr - ( 4 << 16 ); \
	PS_INT16_TO_STYPE( val, vibrato_tab[ ( ptr2 >> 12 ) & 255 ] * 128 ); \
    }
#endif
#ifdef PS_STYPE_FLOATINGPOINT
#define GET_VAL_PSIN \
    { \
	int ptr2 = ( ptr - ( 4 << 16 ) ) & ( ( 1 << 20 ) - 1 ); \
	val = 0; \
	if( ( (ptr2>>10) & 1023 ) < duty_cycle ) { \
	    float sin_ptr = (float)ptr2 / (float)( 1 << 20 ); \
	    val = PS_STYPE_SIN( sin_ptr * (float)M_PI ); \
	} \
    }
#else
#define GET_VAL_PSIN \
    { \
	int ptr2 = ptr - ( 4 << 16 ); \
	val = 0; \
	if( ( (ptr2>>10) & 1023 ) < duty_cycle ) { \
	    PS_INT16_TO_STYPE( val, vibrato_tab[ ( ptr2 >> 12 ) & 255 ] * 128 ); \
	} \
    }
#endif
#define APPLY_VOLUME \
    val *= vol; \
    val /= 256;
#define APPLY_ENV_VOLUME \
    val *= ( env_vol >> 15 ); \
    val /= 32768;
#define END_ADD \
    out[ i ] += val; \
    ptr += delta; \
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
#define END_ADD_NOENV \
    out[ i ] += val; \
    ptr += delta;
enum
{
    gen_type_tri = 0,
    gen_type_saw,
    gen_type_sq,
    gen_type_noise,
    gen_type_drawn,
    gen_type_sin,
    gen_type_hsin,
    gen_type_asin,
    gen_type_psin,
    gen_types
};
#ifdef SUNVOX_GUI
struct generator_visual_data
{
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    int                 curve_ysize;
};
int generator_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    generator_visual_data* data = (generator_visual_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( generator_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->pnet = (psynth_net*)win->host;
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( data->module_data->ctl_type == 4 )
		{
		    int curve_ysize = data->curve_ysize;
                    if( curve_ysize > 0 )
                    {
			int i = ( rx * 32 ) / win->xsize;
			if( i >= 0 && i < 32 )
			{
			    int v = ( ( ( curve_ysize / 2 ) - ry ) * 256 ) / curve_ysize;
			    if( v < -127 ) v = -127;
			    if( v > 127 ) v = 127;
			    data->module_data->drawn_wave[ i ] = v;
			    data->module_data->drawn_wave_changed = true;
			    draw_window( win, wm );
			    data->pnet->change_counter++;
			}
		    }
		}
	    }
	case EVT_MOUSEBUTTONUP:
	    retval = 1;
	    break;
	case EVT_DRAW:
	    wbd_lock( win );
	    draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
	    {
		int yy = 0;
		int xsize = wm->scrollbar_size * 4;
		int ysize = wm->scrollbar_size;
		if( xsize > win->xsize ) xsize = win->xsize;
		COLOR c = blend( wm->color2, win->color, 150 );
		if( data->module_data->ctl_type == 4 )
		{
		    int curve_ysize = psynth_get_curve_ysize( 0, win->ysize, win );
                    data->curve_ysize = curve_ysize;
                    if( curve_ysize > 0 )
                    {
			int b = 0;
			if( win->xsize / 32 >= 4 ) b = 1;
			for( int i = 0; i < 32; i++ )
			{
			    int v = data->module_data->drawn_wave[ i ];
			    v = ( v * curve_ysize ) / 256;
			    int x = ( i * win->xsize ) / 32;
			    int x2 = ( ( i + 1 ) * win->xsize ) / 32;
			    int y;
			    if( v > 0 ) 
				y = curve_ysize / 2 - v;
			    else
			    {
				v = -v;
				y = curve_ysize / 2;
			    }
			    if( v == 0 ) v = 1;
			    draw_frect( x, y, x2 - x - b, v, c, wm );
			}
		        if( !data->module_data->drawn_wave_changed )
		    	{
			    wm->cur_font_color = wm->color2;
			    draw_string( ps_get_string( STR_PS_DRAW_THIS_WAVEFORM ), 0, ( ysize + wm->interelement_space ) * 2, wm );
			}
		    }
		}
		c = wm->color2;
		int prev_y;
		wm->cur_opacity = 80;
		draw_frect( 0, yy + ysize / 2, xsize, 1, c, wm );
		wm->cur_opacity = 255;
		unsigned int rnd = 0;
		for( int x = 0; x < xsize; x++ )
		{
		    int xx = ( x * 256 * 4 ) / xsize;
		    int v;
		    switch( data->module_data->ctl_type )
		    {
			default: 
			    xx += 128;
			    if( xx & 256 ) v = 255 - ( xx & 255 ); else v = xx & 255;
			    v -= 128;
			    break;
			case gen_type_saw:
			    v = xx & 255;
			    v -= 128;
			    break;
			case gen_type_sq:
			    if( xx & 256 ) v = 128; else v = -128;
			    break;
			case gen_type_noise:
			    {
				v = pseudo_random( &rnd ) & 255;
				v -= 128;
			    }
			    break;
			case gen_type_drawn:
			    v = -data->module_data->drawn_wave[ ( xx / 32 ) & 31 ];
			    break;
			case gen_type_sin:
			    v = -g_hsin_tab[ xx & 255 ];
			    if( xx & 256 ) v = -v;
			    v /= 2;
			    break;
			case gen_type_hsin:
			    v = -g_hsin_tab[ xx & 255 ];
			    if( xx & 256 ) v = 0;
			    v /= 2;
			    break;
			case gen_type_asin:
			    v = -g_hsin_tab[ xx & 255 ];
			    v /= 2;
			    break;
			case gen_type_psin:
			    v = -g_hsin_tab[ xx & 255 ];
			    if( xx & 128 ) v = 0;
			    v /= 2;
			    break;
		    }
		    int y = yy + ysize / 2 + ( v * ysize ) / 256;
		    if( x == 0 )
		    {
			draw_frect( x, y, 1, 1, c, wm );
		    }
		    else 
		    {
			draw_line( x - 1, prev_y, x, y, c, wm );
		    }
		    prev_y = y;
		}
		yy += wm->interelement_space + ysize * 2;
		if( data->module_data->ctl_attack || data->module_data->ctl_release )
		{
		    int ax = ( data->module_data->ctl_attack * xsize ) / 512;
		    int rx = ( data->module_data->ctl_release * xsize ) / 512;
		    wm->cur_opacity = 80;
		    draw_frect( 0, yy, ax + rx, 1, c, wm );
		    wm->cur_opacity = 255;
		    draw_line( 0, yy, ax, yy - ysize, wm->color2, wm );
		    draw_line( ax, yy - ysize, ax + rx, yy, wm->color2, wm );
		}
	    }
	    wbd_draw( wm );
	    wbd_unlock( wm );
	    retval = 1;
	    break;
    }
    return retval;
}
#endif
static void gen_set_offset( MODULE_DATA* data, gen_channel* chan, uint offset ) 
{
    int type = chan->local_type;
    uint ptr = 0;
    uint shift = 0;
    switch( type )
    {
        case gen_type_tri:
        case gen_type_saw:
            shift = 11;
            break;
        case gen_type_sq:
            shift = 10;
            break;
        case gen_type_noise:
            shift = 0;
            break;
        case gen_type_drawn:
            shift = 8;
            break;
        case gen_type_sin:
        case gen_type_hsin:
            shift = 10;
            break;
        case gen_type_asin:
        case gen_type_psin:
            shift = 11;
            break;
        default: break;
    }
    ptr = ( offset << 16 ) >> shift;
    chan->ptr = ptr;
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
	    retval = (PS_RETTYPE)"Generator";
	    break;
	case PS_CMD_GET_INFO:
    	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Генератор периодических колебаний с огибающей громкости.\nЛокальные контроллеры: Форма волны, Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)"Generator of periodic signal waveforms with the volume envelope.\nLocal controllers: Waveform, Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#05FF00";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 10, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 128, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WAVEFORM_TYPE ), ps_get_string( STR_PS_GEN_WAVE_TYPES ), 0, gen_types - 1, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 2, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), ps_get_string( STR_PS_SEC256 ), 0, 512, 0, 0, &data->ctl_attack, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), ps_get_string( STR_PS_SEC256 ), 0, 512, 0, 0, &data->ctl_release, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_STEREO_MONO ), 0, MODES - 1, MODE_MONO, 1, &data->ctl_mode, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_sustain, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FREQ_MODULATION ), "", 0, 256, 0, 0, &data->ctl_mod, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DUTY_CYCLE ), "", 0, 1022, 511, 0, &data->ctl_duty_cycle, 511, 2, pnet );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].ptr = 0;
		data->channels[ c ].env_vol = 0;
		data->channels[ c ].sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    data->noise_data = psynth_get_noise_table();
	    memmove( data->drawn_wave, g_gen_drawn_waveform, 32 );
	    data->search_ptr = 0;
	    data->wrong_saw_generation = 0;
	    data->linear_freq_tab = g_linear_freq_tab;
	    data->vibrato_tab = g_hsin_tab;
#ifdef SUNVOX_GUI
	    {
		data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "Generator GUI", 0, 0, 10, 10, wm->color1, 0, pnet, generator_visual_handler, wm );
		    mod->visual_min_ysize = MAX_CURVE_YSIZE;
		    generator_visual_data* gv_data = (generator_visual_data*)mod->visual->data;
		    gv_data->module_data = data;
		    gv_data->mod_num = mod_num;
		    gv_data->pnet = pnet;
		}
	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		int8_t* wave = (int8_t*)psynth_get_chunk_data( mod_num, 0, pnet );
		if( wave )
		{
		    smem_copy( data->drawn_wave, wave, 32 );
		    data->drawn_wave_changed = true;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_BEFORE_SAVE:
            {
                if( data->drawn_wave_changed )
                {
                    int8_t* wave = (int8_t*)psynth_get_chunk_data( mod_num, 0, pnet );
                    if( !wave )
                    {
                        psynth_new_chunk( mod_num, 0, 32, 0, 44100, pnet ); 
                        wave = (int8_t*)psynth_get_chunk_data( mod_num, 0, pnet );
                    }
                    smem_copy( wave, data->drawn_wave, 32 );
                }
            }
            retval = 1;
            break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		if( data->ctl_mode == MODE_MONO )
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
		bool no_env = false;
		int attack_len;
		int attack_delta;
		int release_len;
		int release_delta;
		bool env_ready = false;
		int zero_channels = 0; 
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( chan->playing == 0 ) continue;
		    data->no_active_channels = 0;
		    if( env_ready == false )
		    {
		        if( data->ctl_attack == 0 && data->ctl_release == 0 ) no_env = true;
			attack_delta = 1 << 30;
			release_delta = 1 << 30;
		        attack_len = ( pnet->sampling_freq * data->ctl_attack ) / 256;
		        release_len = ( pnet->sampling_freq * data->ctl_release ) / 256;
		        if( pnet->base_host_version < 0x01090600 ) { attack_len &= ~1023; release_len &= ~1023; }
			if( attack_len != 0 ) attack_delta = ( 1 << 30 ) / attack_len;
			if( release_len != 0 ) release_delta = ( 1 << 30 ) / release_len;
			env_ready = true;
		    }
		    uint32_t delta = chan->delta;
		    int sustain = chan->sustain;
		    uint ptr = 0;
		    bool playing = 0;
		    int sustain_enabled = data->ctl_sustain;
		    uint env_vol = 0;
		    int outputs_num = psynth_get_number_of_outputs( mod );
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
		        sustain = chan->sustain;
		        ptr = chan->ptr;
		        env_vol = chan->env_vol;
		        playing = chan->playing;
		        if( no_env )
		        {
		    	    if( sustain_enabled == 0 ) sustain = 0; 
			    if( sustain == 0 ) playing = 0;
			}
			if( playing == 0 ) continue;
			int vol = ( data->ctl_volume * chan->vel ) >> 8;
			int ctl_pan = data->ctl_pan + ( chan->local_pan - 128 );
			if( ctl_pan < 0 ) ctl_pan = 0;
			if( ctl_pan > 256 ) ctl_pan = 256;
			if( ctl_pan < 128 )
			{
			    if( ch == 1 ) { vol *= ctl_pan; vol >>= 7; }
			}
			else
			{
			    if( ch == 0 ) { vol *= 128 - ( ctl_pan - 128 ); vol >>= 7; }
			}
			if( vol != 0 ) retval = 1;
			int8_t* noise_data = data->noise_data;
			int8_t* drawn_wave = data->drawn_wave;
			uint8_t* vibrato_tab = data->vibrato_tab;
			int add = ch * 32768 / 2;
			int local_type = chan->local_type;
			PS_STYPE2 val = 0;
			uint duty_cycle = data->ctl_duty_cycle;
			PS_STYPE* in = inputs[ 0 ] + offset;
			PS_STYPE* out = outputs[ ch ] + offset;
			if( ( zero_channels & ( 1 << ch ) ) == 0 )
			{
			    int empty = mod->out_empty[ ch ] - offset;
			    if( empty < 0 ) empty = 0;
			    if( empty < frames ) smem_clear( &out[ empty ], ( frames - empty ) * sizeof( PS_STYPE ) );
			    zero_channels |= 1 << ch;
			}
			if( data->ctl_mod )
			{
    			    PS_STYPE2 max_val;
			    PS_STYPE2 min_val;
			    PS_INT16_TO_STYPE( max_val, 32767 );
			    PS_INT16_TO_STYPE( min_val, -32767 );
			    if( pnet->base_host_version < 0x02000005 )
			    {
				max_val *= vol; max_val /= 256;
				min_val *= vol; min_val /= 256;
			    }
			    uint32_t mod_delta;
			    int mod_freq;
			    PSYNTH_GET_FREQ( data->linear_freq_tab, mod_freq, 100 );
			    PSYNTH_GET_DELTA64( pnet->sampling_freq, mod_freq, mod_delta );
			    mod_delta = ( (uint64_t)mod_delta * ( ( data->ctl_mod << ( 16 - 8 ) ) / 8 ) ) >> 16;
			    for( int i = 0; i < frames; i++ ) 
			    {
			        switch( local_type )
			        {
			    	    default: 
				        GET_VAL_TRIANGLE;
				        break;
				    case gen_type_saw: 
				        GET_VAL_SAW;
				        break;
				    case gen_type_sq: 
				        GET_VAL_RECTANGLE;
				        break;
				    case gen_type_noise: 
				        GET_VAL_NOISE;
				        break;
				    case gen_type_drawn: 
				        GET_VAL_DIRTY;
				        break;
				    case gen_type_sin: 
				        GET_VAL_SIN;
				        break;
				    case gen_type_hsin: 
				        GET_VAL_HSIN;
				        break;
				    case gen_type_asin: 
				        GET_VAL_ASIN;
				        break;
				    case gen_type_psin: 
				        GET_VAL_PSIN;
				        break;
				}
				APPLY_VOLUME;
				APPLY_ENV_VOLUME;
				END_ADD;
				PS_STYPE2 in_v = in[ i ];
				bool neg = false;
				if( in_v < 0 )
				{
				    neg = true;
				    in_v = -in_v;
				}
				if( in_v > PS_STYPE_ONE ) in_v = PS_STYPE_ONE;
				uint mul;
				PS_STYPE_TO_INT16( mul, in_v );
				uint32_t mod_delta2 = ( (uint64_t)mod_delta * mul ) >> 16;
				if( neg == false )
				    ptr += mod_delta2;
				else
				    ptr -= mod_delta2;
			    }
			}
			else
			{
			    bool wrong_saw = false;
			    if( data->wrong_saw_generation && local_type == 1 ) wrong_saw = true; 
			    if( no_env && wrong_saw == false )
			    {
			        switch( local_type )
			        {
			    	    default: for( int i = 0; i < frames; i++ ) { GET_VAL_TRIANGLE; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_saw: for( int i = 0; i < frames; i++ ) { GET_VAL_SAW; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_sq:
				    {
					PS_STYPE2 max_val;
					PS_STYPE2 min_val;
					PS_INT16_TO_STYPE( max_val, 32767 );
					PS_INT16_TO_STYPE( min_val, -32767 );
					max_val *= vol; max_val /= 256;
					min_val *= vol; min_val /= 256;
					for( int i = 0; i < frames; i++ ) { GET_VAL_RECTANGLE; END_ADD_NOENV } break;
				    }
				    case gen_type_noise: for( int i = 0; i < frames; i++ ) { GET_VAL_NOISE; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_drawn: for( int i = 0; i < frames; i++ ) { GET_VAL_DIRTY; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_sin: for( int i = 0; i < frames; i++ ) { GET_VAL_SIN; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_hsin: for( int i = 0; i < frames; i++ ) { GET_VAL_HSIN; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_asin: for( int i = 0; i < frames; i++ ) { GET_VAL_ASIN; APPLY_VOLUME; END_ADD_NOENV; } break;
				    case gen_type_psin: for( int i = 0; i < frames; i++ ) { GET_VAL_PSIN; APPLY_VOLUME; END_ADD_NOENV; } break;
				}
			    }
			    else
			    {
			        switch( local_type )
			        {
			    	    default: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_TRIANGLE;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;
				    case gen_type_saw: 
					if( wrong_saw == false )
					    for( int i = 0; i < frames; i++ ) 
					    {
						GET_VAL_SAW;
						APPLY_VOLUME;
						APPLY_ENV_VOLUME;
						END_ADD; 
					    }
					else
					    for( int i = 0; i < frames; i++ ) 
					    {
						GET_VAL_SAW2;
						APPLY_VOLUME;
						APPLY_ENV_VOLUME;
						END_ADD; 
					    }
					break;
				    case gen_type_sq: 
				    {
					PS_STYPE2 max_val;
					PS_STYPE2 min_val;
					PS_INT16_TO_STYPE( max_val, 32767 );
					PS_INT16_TO_STYPE( min_val, -32767 );
					max_val *= vol; max_val /= 256;
					min_val *= vol; min_val /= 256;
					for( int i = 0; i < frames; i++ )
					{
					    GET_VAL_RECTANGLE;
					    APPLY_ENV_VOLUME;
					    END_ADD;
					}
					break;
				    }
				    case gen_type_noise: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_NOISE;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;
				    case gen_type_drawn: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_DIRTY;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;
				    case gen_type_sin: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_SIN;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;
				    case gen_type_hsin: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_HSIN;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;
				    case gen_type_asin: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_ASIN;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;    
				    case gen_type_psin: 
					for( int i = 0; i < frames; i++ ) 
					{
					    GET_VAL_PSIN;
					    APPLY_VOLUME;
					    APPLY_ENV_VOLUME;
					    END_ADD; 
					}
					break;    
				}
			    } 
			} 
		    } 
		    chan->ptr = ptr;
		    chan->delta = delta;
		    chan->env_vol = env_vol;
		    chan->sustain = sustain;
		    chan->playing = playing;
		    if( !playing ) chan->id = ~0;
		} 
		if( retval == 0 )
		{
		    if( zero_channels )
		    {
			retval = 2; 
		    }
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
			if( pnet->base_host_version >= 0x02010301 )
			{
			    for( c = 0; c < data->ctl_channels; c++ )
			    {
				gen_channel* ch = &data->channels[ data->search_ptr ];
				if( ch->sustain == 0 ) break;
				data->search_ptr++;
				if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
			    }
			}
		    }
		}
		else 
		{
		    data->search_ptr = 0;
		}
		uint32_t delta;
		int freq;
		PSYNTH_GET_FREQ( data->linear_freq_tab, freq, event->note.pitch / 4 );
		PSYNTH_GET_DELTA64( pnet->sampling_freq, freq, delta );
		c = data->search_ptr;
		gen_channel* chan = &data->channels[ c ];
		chan->playing = 1;
		chan->vel = event->note.velocity;
		chan->delta = delta;
		chan->ptr = 4 << 16;
		chan->id = event->id;
		if( data->ctl_attack == 0 )
		    chan->env_vol = 1 << 30; 
		else
		    chan->env_vol = 0;
		chan->sustain = 1;
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
		    uint32_t delta;
		    int freq;
		    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, event->note.pitch / 4 );
		    PSYNTH_GET_DELTA64( pnet->sampling_freq, freq, delta );
		    data->channels[ c ].delta = delta;
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
        case PS_CMD_SET_SAMPLE_OFFSET:
        case PS_CMD_SET_SAMPLE_OFFSET2:
            if( data->no_active_channels ) break;
            for( int c = 0; c < MAX_CHANNELS; c++ )
            {
                gen_channel* chan = &data->channels[ c ];
                if( chan->id == event->id )
                {
                    gen_set_offset( data, chan, event->sample_offset.sample_offset );
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
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		gen_channel* chan = &data->channels[ c ];
		if( chan->id == event->id )
		{
		    switch( event->controller.ctl_num )
		    {
			case 1:
			    {
				PS_CTYPE type = event->controller.ctl_val;
				if( type > gen_types - 1 ) type = gen_types - 1;
				chan->local_type = type;
			    }
			    retval = 1;
			    break;
			case 2:
			    chan->local_pan = event->controller.ctl_val >> 7;
			    retval = 1;
			    break;
		    }
		    break;
		}
	    }
	    break;
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    if( event->controller.ctl_num == 0x98 ) 
		data->wrong_saw_generation = 1;
	    break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = 0;
    	    }
#endif
	    data->noise_data = 0;
	    retval = 1;
	    break;
        case PS_CMD_READ_CURVE:
        case PS_CMD_WRITE_CURVE:
            {
                int8_t* curve = NULL;
                int chunk_id = -1;
                int len = 0;
                int reqlen = event->offset;
                switch( event->id )
                {
                    case 0: chunk_id = 0; len = 32; break;
                    default: break;
                }
                if( chunk_id >= 0 )
                {
            	    if( chunk_id == 0 ) curve = data->drawn_wave;
                    if( curve && event->curve.data )
                    {
                        if( reqlen == 0 ) reqlen = len;
                        if( reqlen > len ) reqlen = len;
                        if( event->command == PS_CMD_READ_CURVE )
                            for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve[ i ] / 127.0F; }
                        else
                        {
                            for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * 127.0F; LIMIT_NUM( v, -128, 127 ); curve[ i ] = v; }
                            if( chunk_id == 0 ) data->drawn_wave_changed = true;
                        }
                        retval = reqlen;
                    }
                }
            }
            break;
	default: break;
    }
    return retval;
}
