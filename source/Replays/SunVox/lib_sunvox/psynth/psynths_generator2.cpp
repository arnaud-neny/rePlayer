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

#include "psynth_net.h"
#include "sunvox_engine.h"
extern int8_t g_gen_drawn_waveform[ 32 ];
#define MODULE_DATA	psynth_generator2_data
#define MODULE_HANDLER	psynth_generator2
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define GEN2_SFREQ	44100
#define MAX_CHANNELS	32
#define MAX_SUBCHANNELS	2
#define GEN2_MAX_PITCH ( (int)( 10 * 12 ) * 256 )
#define ENV_VOL_PREC (int)10
#define ENV_VOL_MAX ( (int)32768 << ENV_VOL_PREC )
#define GEN2_TICK_SIZE 64
#define FILTER_TICK_SIZE 64
#define FILTERS 2
enum f_env_mode
{
    f_env_off = 0,
    f_env_sustain_off,
    f_env_sustain_on,
    f_env_modes
};
enum 
{
    gen_type_tri = 0,
    gen_type_saw,
    gen_type_sq,
    gen_type_noise,
    gen_type_drawn_wave,
    gen_type_sin,
    gen_type_hsin,
    gen_type_asin,
    gen_type_drawn_wave_spline,
    gen_type_noise_spline,
    gen_type_white_noise,
    gen_type_pink_noise,
    gen_type_red_noise,
    gen_type_blue_noise,
    gen_type_violet_noise,
    gen_type_grey_noise,
    gen_type_harmonics,
    gen_types
};
#define TYPE_FLAG_NOISE		( 1 << 0 )
#define TYPE_FLAG_NOISE_COLORED	( 1 << 1 )
const static uint8_t g_type_flags[ gen_types ] = 
{
    0,
    0,
    0,
    TYPE_FLAG_NOISE,
    0,
    0,
    0,
    0,
    0,
    TYPE_FLAG_NOISE,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    TYPE_FLAG_NOISE | TYPE_FLAG_NOISE_COLORED,
    0
};
#define SET_PHASE( PTR, PHASE, GEN_TYPE ) PTR = ( (PHASE) << 16 ) >> g_type_shift[ GEN_TYPE ]
const static uint8_t g_type_shift[ gen_types ] =
{
    11, 
    11, 
    10, 
    0, 
    10, 
    10, 
    10, 
    11, 
    10, 
    0, 
    0,
    0,
    0,
    0,
    0,
    0,
    0 
};
#define MAX_TYPE ( gen_types - 1 )
#define MAX_DUTY_CYCLE 1024
#define MAX_PITCH2 2000
#define MAX_FILTER_TYPE 8
#define MAX_FILTER_FREQ 14000
#define MAX_FILTER_RES 1530
struct gen2_subchannel
{
    uint64_t	ptr; 
    uint64_t   	delta;
    int		pitch;
    int		target_pitch;
    int		pitch_delta;
    uint32_t	noise_seed;
    PS_STYPE2	noise_smp[ 5 ];
};
struct gen2_filter_channel
{
    PS_STYPE2	d1, d2, d3;
};
struct gen2_channel
{
    int	    	playing; 
    uint    	id; 
    int	    	vel; 
    int    	env_vol; 
    int	    	sustain;
    int		tick_counter;
    int    	f_env_vol; 
    int	    	f_sustain;
    int		f_tick_counter;
    int		f_cur_freq;
    int		f_cur_res;
    bool	f_init_request;
    gen2_filter_channel f[ FILTERS ];
    gen2_subchannel sc[ MAX_SUBCHANNELS ];
    PS_STYPE	last_sample;
    bool	no_last_sample;
    int		anticlick_counter; 
    PS_STYPE	anticlick_sample;
    psmoother	vol[ MAX_SUBCHANNELS ]; 
    psmoother	osc2_vol; 
    bool	first_frame;
    PS_CTYPE	local_volume;
    PS_CTYPE	local_type;
    PS_CTYPE   	local_pan;
    PS_CTYPE	local_duty_cycle;
    PS_CTYPE	local_f_type;
    PS_CTYPE	local_f_freq;
    PS_CTYPE	local_f_res;
};
struct gen2_options
{
    uint8_t	volume_env_scaling; 
    uint8_t	filter_env_scaling; 
    uint8_t	volume_scaling; 
    uint8_t	filter_scaling; 
    uint8_t	filter_velocity_scaling; 
    uint8_t	freq_div; 
    uint8_t	no_smooth_freq; 
    uint8_t	filter_scaling_reverse; 
    uint8_t	retain_phase; 
    uint8_t	random_phase; 
    uint8_t	filter_pitch; 
    uint8_t	filter_res_velocity_scaling; 
    uint8_t	fast_zero_attack_release; 
    uint8_t	freq_accuracy; 
    uint8_t	always_play_osc2; 
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
    PS_CTYPE   		ctl_volume;
    PS_CTYPE   		ctl_type;
    PS_CTYPE   		ctl_pan;
    PS_CTYPE   		ctl_attack;
    PS_CTYPE   		ctl_release;
    PS_CTYPE   		ctl_sustain;
    PS_CTYPE   		ctl_exp;
    PS_CTYPE   		ctl_duty_cycle;
    PS_CTYPE		ctl_pitch2;
    PS_CTYPE		ctl_f_type;
    PS_CTYPE		ctl_f_freq;
    PS_CTYPE		ctl_f_res;
    PS_CTYPE		ctl_f_attack;
    PS_CTYPE		ctl_f_release;
    PS_CTYPE		ctl_f_exp;
    PS_CTYPE		ctl_f_envelope_mode;
    PS_CTYPE   		ctl_channels;
    PS_CTYPE   		ctl_mode;
    PS_CTYPE   		ctl_noise;
    PS_CTYPE		ctl_osc2_vol;
    PS_CTYPE		ctl_osc2_mode;
    PS_CTYPE		ctl_osc2_phase;
    psynth_net*		pnet;
    gen2_channel	channels[ MAX_CHANNELS ];
    bool	    	no_active_channels;
    int8_t* 	  	noise_data;
    int8_t     		drawn_wave[ 32 ];
    bool		drawn_wave_changed;
    gen2_options*	opt;
    int	            	search_ptr;
    uint*           	linear_freq_tab;
    uint8_t*          	vibrato_tab;
    uint32_t		noise_seed;
    uint32_t		random_seed; 
    psmoother_coefs	smoother_coefs;
    PS_STYPE		add_buf[ GEN2_TICK_SIZE ];
#ifndef ONLY44100
    psynth_resampler*	resamp;
#endif
#ifdef SUNVOX_GUI
    window_manager* wm;
#endif
};
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct gen2_visual_data
{
    WINDOWPTR			win;
    MODULE_DATA*		module_data;
    int				mod_num;
    psynth_net*			pnet;
    WINDOWPTR			opt;
    int				curve_ysize;
};
int gen2_opt_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    gen2_visual_data* data = (gen2_visual_data*)user_data;
    gen2_options* opt = data->module_data->opt;
    psynth_opt_menu* m = opt_menu_new( win );
    opt_menu_add( m, STR_PS_VOLUME_ENV_SCALING_PER_KEY, opt->volume_env_scaling, 127 );
    opt_menu_add( m, STR_PS_FILTER_ENV_SCALING_PER_KEY, opt->filter_env_scaling, 126 );
    opt_menu_add( m, STR_PS_VOLUME_SCALING_PER_KEY, opt->volume_scaling, 125 );
    opt_menu_add( m, STR_PS_FILTER_FREQ_SCALING_PER_KEY, opt->filter_scaling, 124 );
    opt_menu_add( m, STR_PS_FILTER_FREQ_SCALING_PER_KEY2, opt->filter_scaling_reverse, 120 );
    opt_menu_add( m, STR_PS_FILTER_FREQ_IS_NOTE_FREQ, opt->filter_pitch, 117 );
    opt_menu_add( m, STR_PS_FILTER_FREQ_SCALING_ON_VEL, opt->filter_velocity_scaling, 123 );
    opt_menu_add( m, STR_PS_FILTER_RES_SCALING_ON_VEL, opt->filter_res_velocity_scaling, 116 );
    opt_menu_add( m, STR_PS_FREQ_DIV_2, opt->freq_div, 122 );
    opt_menu_add( m, STR_PS_SMOOTH_FREQ_CHANGE, !opt->no_smooth_freq, 121 );
    opt_menu_add( m, STR_PS_INCREASED_FREQ_ACCURACY, opt->freq_accuracy, 114 );
    opt_menu_add( m, STR_PS_RETAIN_PHASE, opt->retain_phase, 119 );
    opt_menu_add( m, STR_PS_RANDOM_PHASE, opt->random_phase, 118 );
    opt_menu_add( m, STR_PS_TRUE_ZERO_ATTACK_RELEASE, opt->fast_zero_attack_release, 115 );
    opt_menu_add( m, STR_PS_ALWAYS_PLAY_OSC2, opt->always_play_osc2, 113 );
    int sel = opt_menu_show( m );
    opt_menu_remove( m );
    int ctl = -1;
    int ctl_val = 0;
    switch( sel )
    {
	case 0: ctl = 127; ctl_val = !opt->volume_env_scaling; break;
	case 1: ctl = 126; ctl_val = !opt->filter_env_scaling; break;
	case 2: ctl = 125; ctl_val = !opt->volume_scaling; break;
	case 3: ctl = 124; ctl_val = !opt->filter_scaling; break;
	case 4: ctl = 120; ctl_val = !opt->filter_scaling_reverse; break;
	case 5: ctl = 117; ctl_val = !opt->filter_pitch; break;
	case 6: ctl = 123; ctl_val = !opt->filter_velocity_scaling; break;
	case 7: ctl = 116; ctl_val = !opt->filter_res_velocity_scaling; break;
	case 8: ctl = 122; ctl_val = !opt->freq_div; break;
	case 9: ctl = 121; ctl_val = opt->no_smooth_freq; break;
	case 10: ctl = 114; ctl_val = !opt->freq_accuracy; break;
	case 11: ctl = 119; ctl_val = !opt->retain_phase; break;
	case 12: ctl = 118; ctl_val = !opt->random_phase; break;
	case 13: ctl = 115; ctl_val = !opt->fast_zero_attack_release; break;
	case 14: ctl = 113; ctl_val = !opt->always_play_osc2; break;
    }
    if( ctl >= 0 ) psynth_handle_ctl_event( data->mod_num, ctl - 1, ctl_val, data->pnet );
    draw_window( data->win, wm );
    return 0;
}
void gen2_draw_line( int x1, int y1, int x2, int y2, int exp, COLOR color, window_manager* wm )
{
    if( exp )
    {
	int steps = 16;
	int xsize = x2 - x1;
	int ysize = y2 - y1;
	int x = x1;
	int y = y1;
	for( int i = 1; i <= steps; i++ )
	{
	    int i2;
	    if( exp == 1 )
		i2 = dsp_curve( i * 32768 / steps, dsp_curve_type_exponential1 );
	    else
		i2 = dsp_curve( i * 32768 / steps, dsp_curve_type_exponential2 );
	    int xx = x1 + ( xsize * i2 ) / 32768;
	    int yy = y1 + ( ysize * i ) / steps;
	    draw_line( x, y, xx, yy, color, wm );
	    x = xx;
	    y = yy;
	}
    }
    else
    {
	draw_line( x1, y1, x2, y2, color, wm );
    }
}
int gen2_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    gen2_visual_data* data = (gen2_visual_data*)win->data;
    int rx = evt->x - win->screen_x;
    int ry = evt->y - win->screen_y;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( gen2_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		data->pnet = (psynth_net*)win->host;
		set_button_icon( BUTTON_ICON_MENU, ((sunvox_engine*)data->pnet->host)->win );
		wm->opt_button_flags = BUTTON_FLAG_FLAT;
		data->opt = new_window( wm_get_string( STR_WM_OPTIONS ), 0, 0, 1, wm->scrollbar_size, wm->button_color, win, button_handler, wm );
		set_handler( data->opt, gen2_opt_button_handler, data, wm );
		set_window_controller( data->opt, 0, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->opt, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->scrollbar_size, CEND );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	    if( evt->key == MOUSE_BUTTON_LEFT )
	    {
		if( data->module_data->ctl_type == gen_type_drawn_wave ||
		    data->module_data->ctl_type == gen_type_drawn_wave_spline ||
		    data->module_data->ctl_type == gen_type_harmonics )
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
			    if( data->module_data->ctl_type == gen_type_harmonics )
			    {
				if( abs( ( curve_ysize / 2 ) - ry ) <= wm->scrollbar_size / 8 )
				    v = 0;
			    }
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
                int num_str_xsize = string_x_size( "00", wm );
                int bar_xsize = win->xsize / 32;
                bool show_numbers = false;
		if( data->module_data->ctl_type == gen_type_harmonics && win->ysize > wm->scrollbar_size && bar_xsize > num_str_xsize )
		    show_numbers = true;
		COLOR c0 = blend( wm->color2, win->color, 200 );
		COLOR c = blend( wm->color2, win->color, 150 );
		wm->cur_font_color = wm->color2;
		if( data->module_data->ctl_type == gen_type_drawn_wave ||
		    data->module_data->ctl_type == gen_type_drawn_wave_spline ||
		    data->module_data->ctl_type == gen_type_harmonics )
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
			    COLOR color = c;
			    if( v == 0 ) { v = 1; color = c0; }
			    int bxsize = x2 - x - b;
			    draw_frect( x, y, bxsize, v, color, wm );
			    if( show_numbers )
			    {
				char ts[ 8 ];
				int_to_string( i + 1, ts );
				if( i & 1 ) wm->cur_opacity = 128;
				draw_string( ts, x + ( bar_xsize - string_x_size( ts, wm ) ) / 2, curve_ysize / 2, wm );
				wm->cur_opacity = 255;
			    }
			}
			if( !data->module_data->drawn_wave_changed )
			{
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
			case gen_type_tri:
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
			case gen_type_drawn_wave:
			case gen_type_harmonics:
			    v = -data->module_data->drawn_wave[ ( xx / 32 ) & 31 ];
			    break;
			case gen_type_drawn_wave_spline:
			    {
				int p = xx / 32;
				int y0 = -data->module_data->drawn_wave[ ( p - 1 ) & 31 ];
				int y1 = -data->module_data->drawn_wave[ ( p + 0 ) & 31 ];
				int y2 = -data->module_data->drawn_wave[ ( p + 1 ) & 31 ];
				int y3 = -data->module_data->drawn_wave[ ( p + 2 ) & 31 ];
				v = catmull_rom_spline_interp_int16( y0, y1, y2, y3, ( xx & 31 ) << 10 );
			    }
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
			default:
			    if( g_type_flags[ data->module_data->ctl_type ] & TYPE_FLAG_NOISE )
			    {
				v = pseudo_random( &rnd ) & 255;
				v -= 128;
			    }
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
		    int ax = ( ( ( data->module_data->ctl_attack * data->module_data->ctl_attack ) / 256 ) * xsize ) / 256;
		    int rx = ( ( ( data->module_data->ctl_release * data->module_data->ctl_release ) / 256 ) * xsize ) / 256;
		    wm->cur_opacity = 80;
		    draw_frect( 0, yy, ax + rx, 1, c, wm );
		    wm->cur_opacity = 255;
		    int exp1 = 0;
		    int exp2 = 0;
		    if( data->module_data->ctl_exp )
		    {
			exp1 = 1;
			exp2 = 2;
		    }
		    gen2_draw_line( 0, yy, ax, yy - ysize, exp1, wm->color2, wm );
		    gen2_draw_line( ax, yy - ysize, ax + rx, yy, exp2, wm->color2, wm );
		}
		if( data->module_data->ctl_f_attack || data->module_data->ctl_f_release )
		{
		    int ax = ( ( ( data->module_data->ctl_f_attack * data->module_data->ctl_f_attack ) / 256 ) * xsize ) / 256;
		    int rx = ( ( ( data->module_data->ctl_f_release * data->module_data->ctl_f_release ) / 256 ) * xsize ) / 256;
		    wm->cur_opacity = 80;
		    draw_frect( 0, yy, ax + rx, 1, c, wm );
		    wm->cur_opacity = 128;
		    int exp1 = 0;
		    int exp2 = 0;
		    if( data->module_data->ctl_f_exp )
		    {
			exp1 = 1;
			exp2 = 2;
		    }
		    gen2_draw_line( 0, yy, ax, yy - ysize, exp1, c, wm );
		    gen2_draw_line( ax, yy - ysize, ax + rx, yy, exp2, c, wm );
		    wm->cur_opacity = 255;
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
static void gen2_reset_channel( gen2_channel* chan )
{
    chan->playing = 0;
    chan->id = ~0;
    chan->no_last_sample = 1;
}
static void gen2_prepare_channel( MODULE_DATA* data, gen2_channel* chan )
{
    if( chan->playing )
    {
	if( chan->no_last_sample )
	{
	    chan->anticlick_counter = 0;
	}
	else
	{
    	    if( data->opt->fast_zero_attack_release && data->ctl_attack == 0 )
	    {
		chan->no_last_sample = 1;
		chan->anticlick_counter = 0;
	    }
	    else
	    {
		chan->anticlick_counter = 32768;
		chan->anticlick_sample = chan->last_sample;
	    }
	}
    }
    else
    {
	chan->no_last_sample = 1;
	chan->anticlick_counter = 0;
	chan->first_frame = 1;
    }
    for( int i = 0; i < MAX_SUBCHANNELS; i++ )
    {
	gen2_subchannel* sc = &chan->sc[ i ];
	if( data->opt->retain_phase == 0 && data->opt->random_phase == 0 )
	{
	    sc->ptr = 0;
	    if( i == 1 ) SET_PHASE( sc->ptr, data->ctl_osc2_phase, data->ctl_type );
	}
	else
	{
	    if( data->opt->random_phase )
	    {
		sc->ptr = pseudo_random( &data->random_seed ) + ( pseudo_random( &data->random_seed ) << 16 );
	    }
	}
	sc->target_pitch = sc->pitch;
	sc->pitch_delta = 0;
	if( g_type_flags[ data->ctl_type ] & TYPE_FLAG_NOISE_COLORED )
	{
	    SMEM_CLEAR_STRUCT( sc->noise_smp );
	}
    }
    chan->env_vol = 0;
    chan->sustain = 1;
    chan->tick_counter = 0;
    chan->f_env_vol = 0;
    chan->f_sustain = 1;
    chan->f_tick_counter = 0;
    for( int i = 0; i < FILTERS; i++ )
    {
	chan->f[ i ].d1 = 0;
	chan->f[ i ].d2 = 0;
	chan->f[ i ].d3 = 0;
    }
    chan->f_init_request = 1;
    chan->playing = 1;
    chan->local_volume = 256;
    chan->local_type = -1;
    chan->local_pan = 128;
    chan->local_duty_cycle = -1;
    chan->local_f_type = -1;
    chan->local_f_freq = MAX_FILTER_FREQ;
    chan->local_f_res = MAX_FILTER_RES;
}
static void gen2_subchannel_set_pitch( MODULE_DATA* data, gen2_channel* chan, int subchan, int pitch )
{
    int freq;
    chan->sc[ subchan ].pitch = pitch;
    pitch /= 4;
    if( data->opt->freq_div )
	pitch += 64 * 12;
    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, pitch );
    if( data->opt->freq_accuracy )
	{ PSYNTH_GET_DELTA64_HQ( GEN2_SFREQ, freq, chan->sc[ subchan ].delta ); }
    else
	{ PSYNTH_GET_DELTA64( GEN2_SFREQ, freq, chan->sc[ subchan ].delta ); }
}
static void gen2_set_offset( MODULE_DATA* data, gen2_channel* chan, int offset ) 
{
    if( data->opt->retain_phase || data->opt->random_phase )
	return;
    int type = data->ctl_type;
    if( chan->local_type >= 0 )
        type = chan->local_type;
    uint64_t ptr;
    SET_PHASE( ptr, offset, type );
    for( int i = 0; i < MAX_SUBCHANNELS; i++ )
	chan->sc[ i ].ptr = ptr;
    if( (int)data->ctl_osc2_phase != offset )
	SET_PHASE( chan->sc[ 1 ].ptr, ( offset + data->ctl_osc2_phase ) & 32767, type );
}
static void gen2_set_pitch( MODULE_DATA* data, gen2_channel* chan, int pitch )
{
    gen2_subchannel_set_pitch( data, chan, 0, pitch );
    for( int i = 1; i < MAX_SUBCHANNELS; i++ )
    {
	chan->sc[ i ].pitch = pitch;
	chan->sc[ i ].delta = chan->sc[ 0 ].delta;
    }
}
static void gen2_set_target_pitch( MODULE_DATA* data, gen2_channel* chan, int pitch )
{
    int delta;
    if( data->opt->no_smooth_freq )
	delta = ( pitch - chan->sc[ 0 ].pitch ) / 1;
    else
	delta = ( pitch - chan->sc[ 0 ].pitch ) / 12;
    if( delta == 0 )
    {
	if( pitch > chan->sc[ 0 ].pitch )
	    delta = 1;
	else
	    delta = -1;
    }
    chan->sc[ 0 ].target_pitch = pitch;
    chan->sc[ 0 ].pitch_delta = delta;
}
#ifdef PS_STYPE_FLOATINGPOINT
const int FP_MUL = 1;
#else
const int FP_MUL = 1 << 15; 
#endif
static uint64_t gen2_render_sin( MODULE_DATA* data, PS_STYPE2 vol, PS_STYPE* RESTRICT render_buf, int frames, uint64_t ptr, uint64_t delta, bool add )
{
    if( ( delta >> 16 ) >= 16 ) vol = 0; 
    if( vol == 0 )
    {
	if( !add )
	{
	    for( int i = 0; i < frames; i++ ) render_buf[ i ] = 0;
	}
	ptr += delta * frames;
	return ptr;
    }
    if( vol == PS_STYPE_ONE )
    {
	if( add )
	{
    	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
		for( int i = 0; i < frames; i++ )
		{
	    	    PS_STYPE2 v;
	    	    uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    if( (uint)ptr & ( 16 << 16 ) ) v = -v;
		    render_buf[ i ] += v;
		    ptr += delta;
		}
	    }
	    else
	    {
		for( int i = 0; i < frames; i++ )
		{
	    	    PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
	    	    float sin_ptr = (float)( (uint)ptr & ((1<<(16+5))-1) ) / (float)( 65536*16 );
		    v = PS_STYPE_SIN( sin_ptr * (float)M_PI );
#else
		    PS_STYPE2 v2;
		    uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    sin_ptr = ( sin_ptr + 1 ) & 255;
		    PS_INT16_TO_STYPE( v2, data->vibrato_tab[ sin_ptr ] * 128 );
		    if( (uint)ptr & ( 16 << 16 ) ) { v = -v; v2 = -v2; }
	    	    if( sin_ptr == 0 ) { v2 = -v2; }
		    int c = (uint)ptr & 4095;
		    int cc = 4096 - c;
		    v = ( v * cc + v2 * c ) / 4096;
#endif
		    render_buf[ i ] += v;
		    ptr += delta;
		}
	    }
	}
	else
	{
    	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
		for( int i = 0; i < frames; i++ )
		{
	    	    PS_STYPE2 v;
	    	    uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    if( (uint)ptr & ( 16 << 16 ) ) v = -v;
		    render_buf[ i ] = v;
		    ptr += delta;
		}
	    }
	    else
	    {
		for( int i = 0; i < frames; i++ )
		{
	    	    PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
	    	    float sin_ptr = (float)( (uint)ptr & ((1<<(16+5))-1) ) / (float)( 65536*16 );
		    v = PS_STYPE_SIN( sin_ptr * (float)M_PI );
#else
		    PS_STYPE2 v2;
		    uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    sin_ptr = ( sin_ptr + 1 ) & 255;
		    PS_INT16_TO_STYPE( v2, data->vibrato_tab[ sin_ptr ] * 128 );
		    if( (uint)ptr & ( 16 << 16 ) ) { v = -v; v2 = -v2; }
	    	    if( sin_ptr == 0 ) { v2 = -v2; }
		    int c = (uint)ptr & 4095;
		    int cc = 4096 - c;
		    v = ( v * cc + v2 * c ) / 4096;
#endif
		    render_buf[ i ] = v;
		    ptr += delta;
		}
	    }
	}
    }
    else
    {
        if( data->ctl_mode > MODE_HQ_MONO )
        {
	    for( int i = 0; i < frames; i++ )
	    {
	        PS_STYPE2 v;
	        uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		if( (uint)ptr & ( 16 << 16 ) ) v = -v;
		v = v * vol / PS_STYPE_ONE;
		if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		ptr += delta;
	    }
	}
	else
	{
	    for( int i = 0; i < frames; i++ )
	    {
	        PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
	        float sin_ptr = (float)( (uint)ptr & ((1<<(16+5))-1) ) / (float)( 65536*16 );
		v = PS_STYPE_SIN( sin_ptr * (float)M_PI );
#else
		PS_STYPE2 v2;
		uint sin_ptr = ( (uint)ptr >> 12 ) & 255;
		PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		sin_ptr = ( sin_ptr + 1 ) & 255;
		PS_INT16_TO_STYPE( v2, data->vibrato_tab[ sin_ptr ] * 128 );
		if( (uint)ptr & ( 16 << 16 ) ) { v = -v; v2 = -v2; }
		if( sin_ptr == 0 ) { v2 = -v2; }
		int c = (uint)ptr & 4095;
		int cc = 4096 - c;
		v = ( v * cc + v2 * c ) / 4096;
#endif
		v = v * vol / PS_STYPE_ONE;
		if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		ptr += delta;
	    }
	}
    }
    return ptr;
}
static void gen2_render_waveform( MODULE_DATA* data, gen2_channel* chan, int subchan, bool no_wave, bool add, PS_STYPE* RESTRICT render_buf, int frames )
{
    gen2_subchannel* sc = &chan->sc[ subchan ];
    int type = data->ctl_type;
    if( chan->local_type >= 0 )
	type = chan->local_type;
    if( no_wave )
    {
	if( add == 0 )
	{
	    for( int i = 0; i < frames; i++ )
	    {
		render_buf[ i ] = 0;
	    }
	}
	sc->ptr += sc->delta * frames;
	return;
    }
    int duty_cycle;
    if( chan->local_duty_cycle >= 0 )
	duty_cycle = chan->local_duty_cycle;
    else
	duty_cycle = data->ctl_duty_cycle;
    uint64_t wave_ptr = sc->ptr; 
    uint64_t delta = sc->delta;  
    switch( type )
    {
        case gen_type_tri: 
    	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    int ptr = wave_ptr >> 3;
		    int acc = 32767 - ( ptr & 0xFFFF );
		    if( ptr & 0x10000 ) acc = -acc;
		    PS_INT16_TO_STYPE( v, acc );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
	    	    wave_ptr += delta;
		}
	    }
	    else
	    {
	        int prev_ptr = wave_ptr >> 3;
	        for( int i = 0; i < frames; i++ )
		{
		    PS_STYPE2 v;
		    wave_ptr += delta;
		    int ptr = wave_ptr >> 3;
		    int ptr_offset = ptr - prev_ptr;
		    int acc = 0;
		    if( ptr_offset > 1 )
		    {
		        int p = prev_ptr;
		        int size = ptr_offset;
		        while( size > 0 )
		        {
		    	    int cur_size = ( ( p & 0xFFFF0000 ) + 0x00010000 ) - p;
			    if( cur_size > size ) cur_size = size;
			    int v1 = 32767 - ( p & 0xFFFF );
			    int v2 = 32767 - ( ( p + cur_size - 1 ) & 0xFFFF );
			    if( p & 0x10000 ) { v1 = -v1; v2 = -v2; }
			    acc += cur_size * ( v1 - ( v1 - v2 ) / 2 );
			    size -= cur_size;
			    p += cur_size;
			}
			acc /= ptr_offset;
		    }
		    else
		    {
		        acc = 32767 - ( ptr & 0xFFFF );
		        if( ptr & 0x10000 ) acc = -acc;
		    }
		    PS_INT16_TO_STYPE( v, acc );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    prev_ptr = ptr;
		}
	    }
	    break;
	case gen_type_saw: 
	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    int ptr = wave_ptr >> 4;
		    int acc = 32767 - ( ptr & 0xFFFF );
		    PS_INT16_TO_STYPE( v, acc );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    else
	    {
	        int prev_ptr = wave_ptr >> 4;
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    wave_ptr += delta;
		    int ptr = wave_ptr >> 4;
		    int ptr_offset = ptr - prev_ptr;
		    int acc = 0;
		    if( ptr_offset > 1 )
		    {
		        int p = prev_ptr;
		        int size = ptr_offset;
		        while( size > 0 )
		        {
		    	    int cur_size = ( ( p & 0xFFFF0000 ) + 0x00010000 ) - p;
			    if( cur_size > size ) cur_size = size;
			    int v1 = 32767 - ( p & 0xFFFF );
			    int v2 = 32767 - ( ( p + cur_size - 1 ) & 0xFFFF );
			    acc += cur_size * ( v1 - ( v1 - v2 ) / 2 );
			    size -= cur_size;
			    p += cur_size;
			}
			acc /= ptr_offset;
		    }
		    else
		    {
			acc = 32767 - ( ptr & 0xFFFF );
		    }
		    PS_INT16_TO_STYPE( v, acc );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    prev_ptr = ptr;
		}
	    }
	    break;
	case gen_type_sq: 
	    {
	        PS_STYPE2 dc = -( ( duty_cycle - MAX_DUTY_CYCLE / 2 ) * PS_STYPE_ONE ) / ( MAX_DUTY_CYCLE / 2 );
	        if( data->ctl_mode > MODE_HQ_MONO )
	        {
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v;
			int ptr = wave_ptr >> 5;
			if( ( ptr & 0xFFFF ) / 64 < duty_cycle )
			    v = PS_STYPE_ONE;
			else
			    v = -PS_STYPE_ONE;
			v += dc;
			if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
			wave_ptr += delta;
		    }
		}
		else
		{
		    int prev_ptr = wave_ptr >> 5;
		    for( int i = 0; i < frames; i++ )
		    {
		        PS_STYPE2 v;
		        wave_ptr += delta;
		        int ptr = wave_ptr >> 5;
		        int ptr_offset = ptr - prev_ptr;
		        int acc = 0;
		        if( ptr_offset > 1 )
			{
			    int p = prev_ptr;
			    int size = ptr_offset;
			    while( size > 0 )
			    {
			        int cur_size;
			        int sign;
			        if( ( p & 0xFFFF ) / 64 < duty_cycle )
			        {
			    	    cur_size = ( ( p & 0xFFFF0000 ) + duty_cycle * 64 ) - p;
				    sign = 1;
				}
				else
				{
				    cur_size = ( ( p & 0xFFFF0000 ) + 0x00010000 ) - p;
				    sign = -1;
				}
				if( cur_size > size ) cur_size = size;
				acc += cur_size * 32767 * sign;
				size -= cur_size;
				p += cur_size;
			    }
			    acc /= ptr_offset;
			}
			else
			{
			    if( ( ptr & 0xFFFF ) / 64 < duty_cycle )
			        acc = 32767;
			    else
			        acc = -32767;
			}
			PS_INT16_TO_STYPE( v, acc );
			v += dc;
			if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
			prev_ptr = ptr;
		    }
		}
	    }
	    break;
	case gen_type_noise: 
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	            PS_STYPE2 v;
	    	    PS_INT16_TO_STYPE( v, data->noise_data[ ( (uint)wave_ptr >> 16 ) & 32767 ] * 256 );
	    	    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    break;
	case gen_type_noise_spline: 
	    {
    	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    uint ptr = (uint)wave_ptr >> 16; 
		    int y0 = data->noise_data[ ( ptr - 1 ) & 32767 ] * 256;
            	    int y1 = data->noise_data[ ptr & 32767 ] * 256;
            	    int y2 = data->noise_data[ ( ptr + 1 ) & 32767 ] * 256;
            	    int y3 = data->noise_data[ ( ptr + 2 ) & 32767 ] * 256;
            	    int y = catmull_rom_spline_interp_int16( y0, y1, y2, y3, ( (uint)wave_ptr & 65535 ) >> 1 );
		    PS_INT16_TO_STYPE( v, y );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    break;
	case gen_type_drawn_wave: 
	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    PS_INT16_TO_STYPE( v, data->drawn_wave[ ( (uint)wave_ptr >> 16 ) & 31 ] * 256 );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    else
	    {
	        int prev_ptr = wave_ptr >> 5;
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    wave_ptr += delta;
		    int ptr = wave_ptr >> 5;
		    int ptr_offset = ptr - prev_ptr;
		    int acc = 0;
		    if( ptr_offset > 1 )
		    {
		        int p = prev_ptr;
		        int size = ptr_offset;
		        while( size > 0 )
		        {
		            int cur_size = ( ( p & 0xFFFFF800 ) + 0x00010000 / 32 ) - p;
		    	    if( cur_size > size ) cur_size = size;
			    acc += cur_size * data->drawn_wave[ ( p & 0xFFFF ) >> 11 ] * 256;
			    size -= cur_size;
			    p += cur_size;
			}
			acc /= ptr_offset;
			PS_INT16_TO_STYPE( v, acc );
		    }
		    else
		    {
		        PS_INT16_TO_STYPE( v, data->drawn_wave[ ( ptr & 0xFFFF ) >> 11 ] * 256 );
		    }
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    prev_ptr = ptr;
		}
	    }
	    break;
	case gen_type_drawn_wave_spline: 
	    {
    	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    uint ptr = (uint)wave_ptr >> 16; 
		    int y0 = data->drawn_wave[ ( ptr - 1 ) & 31 ] * 256;
            	    int y1 = data->drawn_wave[ ptr & 31 ] * 256;
            	    int y2 = data->drawn_wave[ ( ptr + 1 ) & 31 ] * 256;
            	    int y3 = data->drawn_wave[ ( ptr + 2 ) & 31 ] * 256;
            	    int y = catmull_rom_spline_interp_int16( y0, y1, y2, y3, ( (uint)wave_ptr & 65535 ) >> 1 );
		    PS_INT16_TO_STYPE( v, y );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    break;
	case gen_type_sin: 
	    wave_ptr = gen2_render_sin( data, PS_STYPE_ONE, render_buf, frames, wave_ptr, delta, add );
	    break;
	case gen_type_harmonics: 
	    {
	        for( int i = 0; i < 32; i++ )
	        {
	    	    uint64_t ptr = wave_ptr * ( i + 1 );
		    uint64_t delta2 = delta * ( i + 1 );
		    int v = data->drawn_wave[ i ];
		    if( v == 127 ) v = 128;
		    PS_STYPE2 vol = (PS_STYPE2)v * PS_STYPE_ONE / 128;
		    vol = vol * vol / PS_STYPE_ONE;
		    vol = vol * vol / PS_STYPE_ONE;
		    if( v < 0 ) vol = -vol;
		    gen2_render_sin( data, vol, render_buf, frames, ptr, delta2, add );
		    add = true;
		}
		wave_ptr += delta * frames;
	    }
	    break;
	case gen_type_hsin: 
	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    uint sin_ptr = ( (uint)wave_ptr >> 12 ) & 255;
		    if( (uint)wave_ptr & ( 16 << 16 ) ) 
		        v = 0;
		    else
		    {
		        PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    }
		    v -= ( ( (PS_STYPE2)84 * PS_STYPE_ONE ) / 256 );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    else
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
		    if( (uint)wave_ptr & ( 16 << 16 ) ) 
		        v = 0;
		    else
		    {
			float sin_ptr = (float)( (uint)wave_ptr & ((1<<(16+5))-1) ) / (float)( 65536*16 );
			v = PS_STYPE_SIN ( sin_ptr * (float)M_PI );
		    }
#else
		    if( (uint)wave_ptr & ( 16 << 16 ) )
		        v = 0;
		    else
		    {
		        PS_STYPE2 v2;
		        uint sin_ptr = ( (uint)wave_ptr >> 12 ) & 255;
		        PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		        sin_ptr = ( sin_ptr + 1 ) & 255;
		        PS_INT16_TO_STYPE( v2, data->vibrato_tab[ sin_ptr ] * 128 );
		        if( sin_ptr == 0 ) v2 = 0;
		        int c = (uint)wave_ptr & 4095;
		        int cc = 4096 - c;
		        v = ( v * cc + v2 * c ) / 4096;
		    }
#endif
		    v -= ( ( (PS_STYPE2)84 * PS_STYPE_ONE ) / 256 );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    break;
	case gen_type_asin: 
	    if( data->ctl_mode > MODE_HQ_MONO )
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    PS_STYPE2 v;
		    uint sin_ptr = ( (uint)wave_ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    v -= ( ( (PS_STYPE2)162 * PS_STYPE_ONE ) / 256 );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    else
	    {
	        for( int i = 0; i < frames; i++ )
	        {
		    PS_STYPE2 v;
#ifdef PS_STYPE_FLOATINGPOINT
		    float sin_ptr = (float)( (uint)wave_ptr & ((1<<(16+4))-1) ) / (float)( 65536*16 );
		    v = PS_STYPE_SIN( sin_ptr * (float)M_PI );
#else
		    PS_STYPE2 v2;
		    uint sin_ptr = ( (uint)wave_ptr >> 12 ) & 255;
		    PS_INT16_TO_STYPE( v, data->vibrato_tab[ sin_ptr ] * 128 );
		    sin_ptr = ( sin_ptr + 1 ) & 255;
		    PS_INT16_TO_STYPE( v2, data->vibrato_tab[ sin_ptr ] * 128 );
		    int c = (uint)wave_ptr & 4095;
		    int cc = 4096 - c;
		    v = ( v * cc + v2 * c ) / 4096;
#endif
		    v -= ( ( (PS_STYPE2)162 * PS_STYPE_ONE ) / 256 );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		    wave_ptr += delta;
		}
	    }
	    break;
	case gen_type_white_noise: 
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    int rnd = psynth_rand2( &sc->noise_seed );
		    PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		}
	    }
	    break;
    	case gen_type_pink_noise: 
	case gen_type_blue_noise: 
	    {
	        const PS_STYPE2 b0 = 0.99765 * FP_MUL;
	        const PS_STYPE2 b1 = 0.96300 * FP_MUL;
	        const PS_STYPE2 b2 = 0.57000 * FP_MUL;
	        const PS_STYPE2 a0 = 0.0990460 * FP_MUL;
	        const PS_STYPE2 a1 = 0.2965164 * FP_MUL;
	        const PS_STYPE2 a2 = 1.0526913 * FP_MUL;
	        const PS_STYPE2 a3 = 0.1848 * FP_MUL;
	        for( int i = 0; i < frames; i++ )
	        {
	    	    int rnd = psynth_rand2( &sc->noise_seed );
		    PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
		    sc->noise_smp[ 0 ] = ( b0 * sc->noise_smp[ 0 ] + v * a0 ) / FP_MUL;
		    sc->noise_smp[ 1 ] = ( b1 * sc->noise_smp[ 1 ] + v * a1 ) / FP_MUL;
		    sc->noise_smp[ 2 ] = ( b2 * sc->noise_smp[ 2 ] + v * a2 ) / FP_MUL;
		    sc->noise_smp[ 3 ] = sc->noise_smp[ 0 ] + sc->noise_smp[ 1 ] + sc->noise_smp[ 2 ] + ( v * a3 ) / FP_MUL;
		    if( type == gen_type_blue_noise )
		    {
		        v = ( sc->noise_smp[ 3 ] - sc->noise_smp[ 4 ] ) / 2;
		        sc->noise_smp[ 4 ] = sc->noise_smp[ 3 ];
		    }
		    else
		    {
		        v = sc->noise_smp[ 3 ] / 4;
		    }
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		}
	    }
	    break;
	case gen_type_red_noise: 
	case gen_type_grey_noise: 
	    {
	        const PS_STYPE2 a0 = 4.548848e-03 * FP_MUL;
	        const PS_STYPE2 b1 = -9.954512e-01 * FP_MUL;
	        for( int i = 0; i < frames; i++ )
	        {
	    	    int rnd = psynth_rand2( &sc->noise_seed ) * 8;
		    PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
		    sc->noise_smp[ 0 ] = ( a0 * v - b1 * sc->noise_smp[ 0 ] ) / FP_MUL;
		    v = sc->noise_smp[ 0 ];
		    if( add ) render_buf[ i ] += v; else render_buf[ i ] = v;
		}
		if( type == gen_type_grey_noise )
		{
		    for( int i = 0; i < frames; i++ )
		    {
		        int rnd = psynth_rand2( &sc->noise_seed );
		        PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
		        PS_STYPE2 v2 = ( v - sc->noise_smp[ 1 ] ) / 2;
		        sc->noise_smp[ 1 ] = v;
		        render_buf[ i ] += v2;
		    }
		}
	    }
	    break;
	case gen_type_violet_noise: 
	    {
	        for( int i = 0; i < frames; i++ )
	        {
	    	    int rnd = psynth_rand2( &sc->noise_seed );
		    PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
		    PS_STYPE2 v2 = ( v - sc->noise_smp[ 0 ] ) / 2;
		    sc->noise_smp[ 0 ] = v;
		    if( add ) render_buf[ i ] += v2; else render_buf[ i ] = v2;
		}
	    }
	    break;
    }
    sc->ptr = wave_ptr;
}
static void gen2_apply_noise( MODULE_DATA* data, PS_STYPE* RESTRICT render_buf, int frames )
{
    if( data->ctl_noise == 0 ) return;
    for( int i = 0; i < frames; i++ )
    {
	PS_STYPE2 v;
	data->noise_seed = ( data->noise_seed * 196314165 ) + 907633515; 
	int r = (int)data->noise_seed >> 16;
	v = render_buf[ i ] + ( ( ( r * data->ctl_noise ) / 256 ) * PS_STYPE_ONE ) / 32768;
	render_buf[ i ] = v;
    }
}
#define FILTER_OUTPUT_CONTROL \
    if( outp >= 1 << bits ) \
	outp = ( 1 << bits ) - 1; \
    else if( outp <= ( -1 << bits ) ) \
	outp = ( -1 << bits ) + 1;
static void gen2_apply_envelope( MODULE_DATA* data, gen2_channel* chan, PS_STYPE* RESTRICT render_buf, int frames )
{
    int i;
    int sustain;
    int env_vol;
    int playing;
    int ctl_f_attack = data->ctl_f_attack;
    int ctl_f_release = data->ctl_f_release;
    if( data->opt->filter_env_scaling )
    {
	int s = GEN2_MAX_PITCH - chan->sc[ 0 ].pitch;
	if( s > 10000 )
	{
	    s -= 10000;
	    s >>= 8;
	    if( s < 0 ) s = 0;
	    if( s > 256 ) s = 256;
	    ctl_f_attack -= ( s * ( 512 - ctl_f_attack ) ) / 512;
	    ctl_f_release -= ( s * ( 512 - ctl_f_release ) ) / 512;
	    if( ctl_f_attack < 0 ) ctl_f_attack = 0;
	    if( ctl_f_release < 0 ) ctl_f_release = 0;
	}
    }
    int attack = ( 256 - ctl_f_attack );
    int release = ( 256 - ctl_f_release );
    if( attack > 192 ) attack = 192 + ( attack - 192 ) * 8;
    if( release > 192 ) release = 192 + ( release - 192 ) * 8;
    attack = attack * attack;
    release = release * release;
    attack += 32;
    release += 32;
    int ctl_exp = data->ctl_exp;
    int wtype = data->ctl_type;
    if( chan->local_type >= 0 )
        wtype = chan->local_type;
    int type = data->ctl_f_type;
    if( chan->local_f_type >= 0 )
	type = chan->local_f_type;
    bool f_enabled = type > 0;
    type--;
    int rolloff_steps = type / 4 + 1;
    if( type > 3 ) type -= 4;
    int ctl_f_freq = ( chan->local_f_freq * data->ctl_f_freq ) / MAX_FILTER_FREQ;
    int ctl_f_res = ( chan->local_f_res * data->ctl_f_res ) / MAX_FILTER_RES;
    if( data->opt->filter_pitch )
    {
	int oct_offset = 5;
	switch( wtype )
	{
	    case gen_type_tri:
	    case gen_type_saw:
	    case gen_type_asin:
		oct_offset--;
		break;
	}
	if( data->opt->freq_div )
	    oct_offset++;
	PSYNTH_GET_FREQ( data->linear_freq_tab, ctl_f_freq, chan->sc[ 0 ].pitch / 4 + 64 * 12 * oct_offset );
	if( ctl_f_freq >= MAX_FILTER_FREQ ) ctl_f_freq = MAX_FILTER_FREQ;
    }
    else
    {
	if( data->opt->filter_scaling )
	{
	    int s = chan->sc[ 0 ].pitch;
	    s = ( s * 256 ) / GEN2_MAX_PITCH;
	    s += 16;
	    if( s < 0 ) s = 0;
	    if( s > 256 ) s = 256;
	    ctl_f_freq = ( ctl_f_freq * s ) / 256;
	}
	if( data->opt->filter_scaling_reverse )
	{
	    int s = chan->sc[ 0 ].pitch;
	    s = GEN2_MAX_PITCH - s;
	    s = ( s * 256 ) / GEN2_MAX_PITCH;
	    s += 16;
	    if( s < 0 ) s = 0;
	    if( s > 256 ) s = 256;
	    ctl_f_freq = ( ctl_f_freq * s ) / 256;
	}
    }
    if( data->opt->filter_velocity_scaling )
    {
	int s = chan->vel;
	ctl_f_freq = ( ctl_f_freq * s ) / 256;
    }
    if( data->opt->filter_res_velocity_scaling )
    {
	int s = chan->vel;
	ctl_f_res = ( ctl_f_res * s ) / 256;
    }
    if( chan->f_init_request )
    {
	chan->f_cur_freq = ctl_f_freq * 256;
	chan->f_cur_res = ctl_f_res * 256;
	chan->f_init_request = 0;
    }
    if( f_enabled )
    {
	int sustain = chan->f_sustain;
	int env_vol = chan->f_env_vol;
	bool fixed_filter = 0;
	if( data->pnet->base_host_version >= 0x02010300 ) fixed_filter = 1;
        if( data->opt->fast_zero_attack_release )
	{
	    if( data->ctl_f_attack == 0 )
	    {
		if( sustain )
		{
		    env_vol = ENV_VOL_MAX;
		    if( data->ctl_f_envelope_mode != f_env_sustain_on )
			sustain = 0;
	        }
	    }
	    if( data->ctl_f_release == 0 )
	    {
		if( sustain == 0 )
		{
		    env_vol = 0;
		}
	    }
        }
	i = 0;
	while( i < frames )
	{
	    int cur_size = FILTER_TICK_SIZE - chan->f_tick_counter;
	    if( i + cur_size > frames )
		cur_size = frames - i;
	    if( chan->f_tick_counter == 0 )
	    {
		if( chan->f_cur_freq / 256 > ctl_f_freq )
		{
		    chan->f_cur_freq -= 4 * MAX_FILTER_FREQ;
		    if( chan->f_cur_freq / 256 < ctl_f_freq )
			chan->f_cur_freq = ctl_f_freq * 256;
		}
		if( chan->f_cur_freq / 256 < ctl_f_freq )
		{
		    chan->f_cur_freq += 4 * MAX_FILTER_FREQ;
		    if( chan->f_cur_freq / 256 > ctl_f_freq )
			chan->f_cur_freq = ctl_f_freq * 256;
		}
		if( chan->f_cur_res / 256 > ctl_f_res )
		{
		    chan->f_cur_res -= 4 * MAX_FILTER_RES;
		    if( chan->f_cur_res / 256 < ctl_f_res )
			chan->f_cur_res = ctl_f_res * 256;
		}
		if( chan->f_cur_res / 256 < ctl_f_res )
		{
		    chan->f_cur_res += 4 * MAX_FILTER_RES;
		    if( chan->f_cur_res / 256 > ctl_f_res )
			chan->f_cur_res = ctl_f_res * 256;
		}
		if( sustain && env_vol < ENV_VOL_MAX )
		{
		    env_vol += attack * FILTER_TICK_SIZE;
		    if( env_vol >= ENV_VOL_MAX )
		    {
			env_vol = ENV_VOL_MAX;
			if( data->ctl_f_envelope_mode != f_env_sustain_on )
			    sustain = 0;
		    }
		}
		else
		{
		    if( ( sustain && env_vol == ENV_VOL_MAX ) || ( !sustain && env_vol == 0 ) )
		    {
		    }
		    else
		    {
			env_vol -= release * FILTER_TICK_SIZE;
			if( env_vol < 0 )
			{
			    env_vol = 0;
			    sustain = 0;
			}
		    }
		}
	    }
	    int fs;
	    PS_STYPE2 c, f, q;
	    int bits = ( sizeof( PS_STYPE ) * 8 ) - 1;
	    int i1 = i;
	    int i2 = i + cur_size;
	    fs = GEN2_SFREQ;
	    int f_cur_freq = chan->f_cur_freq / 256;
	    if( data->ctl_f_envelope_mode != f_env_off )
		f_cur_freq = ( f_cur_freq * ( env_vol >> ENV_VOL_PREC ) ) / 32768;
	    if( data->ctl_f_exp && !data->opt->filter_pitch )
		f_cur_freq = ( f_cur_freq * f_cur_freq ) / MAX_FILTER_FREQ;
	    if( fixed_filter )
	    {
		if( data->ctl_mode >= MODE_LQ )
		{ 
		    if( data->opt->filter_pitch ) f_cur_freq *= 2;
		    c = ( f_cur_freq * 32768 / 2 ) / fs;
		}
		else
		{ 
		    c = ( f_cur_freq * 32768 ) / ( fs * 2 );
		}
		f = ( 6433 * c ) / 32768;
		if( f >= 1024 ) f = 1023;
	    }
	    for( int fnum = 0; fnum < rolloff_steps; fnum++ )
	    {
		gen2_filter_channel* fchan;
		int res = chan->f_cur_res / 256;
		fchan = &chan->f[ fnum ];
		i = i1;
		if( fnum < rolloff_steps - 1 ) res = 0;
	    q = 1536 - res;
	    if( !fixed_filter )
	    {
		if( data->ctl_mode > MODE_HQ_MONO ) f_cur_freq /= 2;
		c = ( f_cur_freq * 32768 ) / ( fs * 2 );
		f = ( 6433 * c ) / 32768;
		if( f >= 1024 ) f = 1023;
	    }
	    PS_STYPE2 d1 = fchan->d1;
	    PS_STYPE2 d2 = fchan->d2;
	    PS_STYPE2 d3 = fchan->d3;
	    bool dont_render = 0;
	    if( f == 0 )
	    {
		if( type == 0 )
		{
		    for( ; i < i2; i++ )
		    {
			PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			outp = d2 / 32;
			FILTER_OUTPUT_CONTROL;
#else
			outp = d2;
			psynth_denorm_add_white_noise( outp );
#endif
			d2 = ( d2 * 255 ) / 256;
			render_buf[ i ] = outp;
		    }
		    dont_render = 1;
		}
		if( type == 2 )
		{
		    for( ; i < i2; i++ )
		    {
			PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			outp = d1 / 32;
			FILTER_OUTPUT_CONTROL;
#else
			outp = d1;
			psynth_denorm_add_white_noise( outp );
#endif
			d1 = ( d1 * 255 ) / 256;
			render_buf[ i ] = outp;
		    }
		    dont_render = 1;
		}
	    }
	    if( dont_render == 0 )
	    switch( type )
	    {
		case 0:
		    if( data->ctl_mode > MODE_HQ_MONO )
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = low / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = low / 2;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    else
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 slow = low;
			    low = low + ( f * band ) / 1024;
			    high = inp - low - ( q * band ) / 1024;
			    band = ( f * high ) / 1024 + band;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = ( ( d2 + slow ) / 2 ) / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = ( d2 + slow ) / 2;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    break;
		case 1:
		    if( data->ctl_mode > MODE_HQ_MONO )
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = high / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = high;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    else
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 shigh = high;
			    low = low + ( f * band ) / 1024;
			    high = inp - low - ( q * band ) / 1024;
			    band = ( f * high ) / 1024 + band;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = ( ( d3 + shigh ) / 2 ) / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = ( d3 + shigh ) / 2;
#endif
			    d1 = band;
			    d2 = low;
			    d3 = high;
			    render_buf[ i ] = outp;
			}
		    }
		    break;
		case 2:
		    if( data->ctl_mode > MODE_HQ_MONO )
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = band / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = band;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    else
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 sband = band;
			    low = low + ( f * band ) / 1024;
			    high = inp - low - ( q * band ) / 1024;
			    band = ( f * high ) / 1024 + band;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = ( ( d1 + sband ) / 2 ) / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = ( d1 + sband ) / 2;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    break;
		case 3:
		    if( data->ctl_mode > MODE_HQ_MONO )
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = ( high + low ) / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = high + low;
#endif
			    d1 = band;
			    d2 = low;
			    render_buf[ i ] = outp;
			}
		    }
		    else
		    {
			for( ; i < i2; i++ )
			{
			    PS_STYPE2 inp = render_buf[ i ];
			    psynth_denorm_add_white_noise( inp );
#ifndef PS_STYPE_FLOATINGPOINT
			    inp *= 32;
#endif
			    PS_STYPE2 low = d2 + ( f * d1 ) / 1024;
			    PS_STYPE2 high = inp - low - ( q * d1 ) / 1024;
			    PS_STYPE2 band = ( f * high ) / 1024 + d1;
			    PS_STYPE2 snotch = high + low;
			    low = low + ( f * band ) / 1024;
			    high = inp - low - ( q * band ) / 1024;
			    band = ( f * high ) / 1024 + band;
			    PS_STYPE2 outp;
#ifndef PS_STYPE_FLOATINGPOINT
			    outp = ( ( d3 + snotch ) / 2 ) / 32;
			    FILTER_OUTPUT_CONTROL;
#else
			    outp = ( d3 + snotch ) / 2;
#endif
			    d1 = band;
			    d2 = low;
			    d3 = high + low;
			    render_buf[ i ] = outp;
			}
		    }
		    break;
	    }
	    fchan->d1 = d1;
	    fchan->d2 = d2;
	    fchan->d3 = d3;
	    } 
    	    chan->f_tick_counter += cur_size;
	    chan->f_tick_counter &= FILTER_TICK_SIZE - 1;
	}
	chan->f_sustain = sustain;
	chan->f_env_vol = env_vol;
    }
    int ctl_attack = data->ctl_attack;
    int ctl_release = data->ctl_release;
    if( data->opt->volume_env_scaling )
    {
	int s = GEN2_MAX_PITCH - chan->sc[ 0 ].pitch;
	if( s > 10000 )
	{
	    s -= 10000;
	    s >>= 8;
	    if( s < 0 ) s = 0;
	    if( s > 256 ) s = 256;
	    ctl_attack -= ( s * ( 512 - ctl_attack ) ) / 512;
	    ctl_release -= ( s * ( 512 - ctl_release ) ) / 512;
	    if( ctl_attack < 0 ) ctl_attack = 0;
	    if( ctl_release < 0 ) ctl_release = 0;
	}
    }
    attack = ( 256 - ctl_attack );
    release = ( 256 - ctl_release );
    if( attack > 192 ) attack = 192 + ( attack - 192 ) * 8;
    if( release > 192 ) release = 192 + ( release - 192 ) * 8;
    attack = attack * attack;
    release = release * release;
    attack += 32;
    release += 32;
    sustain = chan->sustain;
    env_vol = chan->env_vol;
    playing = chan->playing;
    if( data->opt->fast_zero_attack_release )
    {
	if( data->ctl_attack == 0 )
	{
	    if( sustain )
	    {
		env_vol = ENV_VOL_MAX;
		if( data->ctl_sustain == 0 )
		    sustain = 0;
	    }
	}
	if( data->ctl_release == 0 )
	{
	    if( sustain == 0 )
	    {
		env_vol = 0;
	    }
	}
    }
    i = 0;
    while( i < frames )
    {
	if( sustain && env_vol == ENV_VOL_MAX )
	    break;
	if( sustain && env_vol < ENV_VOL_MAX )
	{
	    for( ; i < frames; i++ )
	    {
		PS_STYPE2 v = render_buf[ i ];
		int env = env_vol >> ENV_VOL_PREC;
		if( ctl_exp )
		    env = ( env * env ) >> 15;
		v *= env;
		v /= 32768;
		render_buf[ i ] = v;
		env_vol += attack;
		if( env_vol >= ENV_VOL_MAX )
		{
		    i++;
		    env_vol = ENV_VOL_MAX;
		    if( data->ctl_sustain == 0 )
			sustain = 0;
		    break;
		}
	    }
	}
	else
	{
	    for( ; i < frames; i++ )
	    {
		PS_STYPE2 v = render_buf[ i ];
		int env = env_vol >> ENV_VOL_PREC;
		if( ctl_exp )
		    env = ( env * env ) >> 15;
		v *= env;
		v /= 32768;
		render_buf[ i ] = v;
		env_vol -= release;
		if( env_vol < 0 )
		{
		    i++;
		    env_vol = 0;
		    playing = 0;
		    for( ; i < frames; i++ ) render_buf[ i ] = 0;
		    break;
		}
	    }
	}
    }
    chan->sustain = sustain;
    chan->env_vol = env_vol;
    chan->playing = playing;
    if( chan->playing == 0 )
	chan->id = ~0;
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
	    retval = (PS_RETTYPE)"Analog generator";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)
"Этот модуль в отличие от Generator дает более мягкое и естественное звучание. Цена качества - требовательность к ресурсам.\n"
"Звучит лучше на частоте дискретизации 44100Гц.\n"
"Локальные контроллеры: Громкость, Форма волны, Панорама, Коэф.заполнения, Фильтр, Ф.частота, Ф.резонанс";
                        break;
                    }
		    retval = (PS_RETTYPE)"Virtual Analog Generator.\nSounds better at a sample rate of 44100Hz\nLocal controllers: Volume, Waveform, Panning, Duty cycle, Filter type, Filter freq, Filter resonance";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#AEFF00";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 22, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 256, 80, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_WAVEFORM_TYPE ), ps_get_string( STR_PS_GEN2_WAVE_TYPES ), 0, MAX_TYPE, 0, 1, &data->ctl_type, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 2, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), "", 0, 256, 0, 0, &data->ctl_attack, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), "", 0, 256, 0, 0, &data->ctl_release, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_sustain, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_EXP_ENVELOPE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_exp, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DUTY_CYCLE ), "", 0, MAX_DUTY_CYCLE, MAX_DUTY_CYCLE / 2, 0, &data->ctl_duty_cycle, MAX_DUTY_CYCLE / 2, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OSC2_PITCH ), ps_get_string( STR_PS_SEMITONE64 ), 0, MAX_PITCH2, MAX_PITCH2 / 2, 0, &data->ctl_pitch2, MAX_PITCH2 / 2, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 8, -1000, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FILTER ), ps_get_string( STR_PS_GEN2_FILTER_TYPES ), 0, MAX_FILTER_TYPE, 0, 1, &data->ctl_f_type, 0, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_FREQ ), ps_get_string( STR_PS_HZ ), 0, MAX_FILTER_FREQ, MAX_FILTER_FREQ, 0, &data->ctl_f_freq, -1, 2, pnet );
	    psynth_set_ctl_flags( mod_num, 10, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_RESONANCE ), "", 0, MAX_FILTER_RES, 0, 0, &data->ctl_f_res, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_EXP_FREQ ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_f_exp, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_ATTACK ), "", 0, 256, 0, 0, &data->ctl_f_attack, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_RELEASE ), "", 0, 256, 0, 0, &data->ctl_f_release, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_F_ENVELOPE ), ps_get_string( STR_PS_GEN2_F_ENVELOPE_TYPES ), 0, f_env_modes - 1, f_env_off, 1, &data->ctl_f_envelope_mode, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 16, 1, &data->ctl_channels, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODES - 1, MODE_HQ_MONO, 1, &data->ctl_mode, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOISE ), "", 0, 256, 0, 0, &data->ctl_noise, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OSC2_VOL ), "", 0, 32768, 32768, 0, &data->ctl_osc2_vol, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OSC2_MODE ), ps_get_string( STR_PS_GEN2_OSC2_MODES ), 0, 8, 0, 1, &data->ctl_osc2_mode, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OSC2_PHASE ), "", 0, 32768, 0, 0, &data->ctl_osc2_phase, -1, 0, pnet );
	    data->pnet = pnet;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		gen2_reset_channel( &data->channels[ c ] );
		for( int i = 0; i < MAX_SUBCHANNELS; i++ )
		{
		    gen2_subchannel* sc = &data->channels[ c ].sc[ i ];
		    sc->noise_seed = (uint32_t)stime_ns() + pseudo_random() + mod_num * 3 + i * 7 + mod->id * 3079;
		}
	    }
	    data->no_active_channels = 1;
	    data->noise_data = psynth_get_noise_table();
	    memmove( data->drawn_wave, g_gen_drawn_waveform, 32 );
	    data->linear_freq_tab = g_linear_freq_tab;
	    data->vibrato_tab = g_hsin_tab;
	    data->noise_seed = (uint32_t)stime_ns() + pseudo_random() + 11 + mod->id * 3079;
	    data->random_seed = (uint32_t)stime_ns() + pseudo_random() + 22 + mod->id * 3079;
	    psmoother_init( &data->smoother_coefs, 100, pnet->sampling_freq );
	    psynth_get_temp_buf( mod_num, pnet, 0 ); 
#ifndef ONLY44100
	    data->resamp = NULL;
	    if( pnet->sampling_freq != GEN2_SFREQ )
	    {
		data->resamp = psynth_resampler_new( pnet, mod_num, GEN2_SFREQ, pnet->sampling_freq, 0, 0 );
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
		    mod->visual = new_window( "Generator GUI", 0, 0, 10, 10, wm->color1, 0, pnet, gen2_visual_handler, wm );
		    mod->visual_min_ysize = MAX_CURVE_YSIZE;
		    gen2_visual_data* gv_data = (gen2_visual_data*)mod->visual->data;
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
		data->opt = (gen2_options*)
		    psynth_get_chunk_data_autocreate( mod_num, 1, sizeof( gen2_options ), 0, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
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
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
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
		int outputs_num = psynth_get_number_of_outputs( mod );
	        PS_STYPE** resamp_outputs = outputs;
	        int resamp_offset = offset;
	        int resamp_frames = frames;
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
		bool empty_output = true;
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen2_channel* chan = &data->channels[ c ];
		    if( chan->playing )
		    {
			data->no_active_channels = 0;
			bool first_frame = chan->first_frame;
			bool first_frame2 = first_frame;
			if( first_frame )
			    chan->first_frame = 0;
			int chan_vol = ( data->ctl_volume * chan->local_volume ) / 256;
			if( data->opt->volume_scaling )
			{
			    int s = chan->sc[ 0 ].pitch;
			    s = ( s * 256 ) / GEN2_MAX_PITCH;
			    s += 16;
			    if( s < 0 ) s = 0;
			    if( s > 256 ) s = 256;
			    chan_vol = ( chan_vol * s ) / 256;
			}
			PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, resamp_outputs, outputs_num, resamp_offset );
			bool no_wave = 0;
			if( chan->sc[ 0 ].pitch < 10000 )
			{
			    if( chan->sc[ 0 ].pitch <= 1700 )
			    {
				chan_vol = 0;
				no_wave = 1;
			    }
			    else
			    {
				chan_vol *= chan->sc[ 0 ].pitch - 1700;
				chan_vol /= ( 10000 - 1700 );
			    }
			}
			{
			    int i = 0;
			    while( i < resamp_frames )
			    {
				int cur_size = GEN2_TICK_SIZE - chan->tick_counter;
				if( i + cur_size > resamp_frames )
				    cur_size = resamp_frames - i;
				if( chan->tick_counter == 0 )
				{
				    gen2_subchannel* sc0 = &chan->sc[ 0 ];
				    if( sc0->pitch != sc0->target_pitch )
				    {
					sc0->pitch += sc0->pitch_delta;
					if( sc0->pitch_delta > 0 )
					{
					    if( sc0->pitch > sc0->target_pitch )
						sc0->pitch = sc0->target_pitch;
					}
					else
					{
					    if( sc0->pitch < sc0->target_pitch )
						sc0->pitch = sc0->target_pitch;
					}
					gen2_set_pitch( data, chan, sc0->pitch );
				    }
				}
				gen2_render_waveform( data, chan, 0, no_wave, 0, render_buf + i, cur_size );
				if( data->ctl_pitch2 == MAX_PITCH2 / 2 && data->opt->always_play_osc2 == 0 )
				{
				    gen2_render_waveform( data, chan, 1, 1, 1, render_buf + i, cur_size );
				}
				else
				{
				    gen2_subchannel_set_pitch( data, chan, 1, chan->sc[ 0 ].pitch - ( data->ctl_pitch2 - 1000 ) * 4 );
				    if( first_frame2 )
				    {
					first_frame2 = 0;
				        psmoother_reset( &chan->osc2_vol, data->ctl_osc2_vol, 32768 );
				    }
				    PS_STYPE2 target_osc2_vol = psmoother_target( data->ctl_osc2_vol, 32768 ); 
				    bool smoothing = psmoother_check( &chan->osc2_vol, target_osc2_vol );
				    if( smoothing == false && data->ctl_osc2_vol == 32768 && data->ctl_osc2_mode == 0 )
					gen2_render_waveform( data, chan, 1, no_wave, 1, render_buf + i, cur_size );
				    else
				    {
					gen2_render_waveform( data, chan, 1, no_wave, 0, data->add_buf, cur_size );
					if( smoothing )
					{
					    for( int b = 0; b < cur_size; b++ )
					    {
#ifdef PS_STYPE_FLOATINGPOINT
            					data->add_buf[ b ] *= psmoother_val( &data->smoother_coefs, &chan->osc2_vol, target_osc2_vol );
#else
            					data->add_buf[ b ] = (PS_STYPE2)data->add_buf[ b ] * psmoother_val( &data->smoother_coefs, &chan->osc2_vol, target_osc2_vol ) / 32768;
#endif
					    }
					}
					else
					{
					    if( data->ctl_osc2_vol != 32768 )
					    {
						for( int b = 0; b < cur_size; b++ )
						{
#ifdef PS_STYPE_FLOATINGPOINT
            					    data->add_buf[ b ] *= target_osc2_vol;
#else
            					    data->add_buf[ b ] = (PS_STYPE2)data->add_buf[ b ] * target_osc2_vol / 32768;
#endif
						}
					    }
					}
					PS_STYPE* RESTRICT render_buf2 = render_buf + i;
					switch( data->ctl_osc2_mode )
					{
					    case 0: 
						for( int b = 0; b < cur_size; b++ ) render_buf2[ b ] += data->add_buf[ b ];
						break;
					    case 1: 
						for( int b = 0; b < cur_size; b++ ) render_buf2[ b ] -= data->add_buf[ b ];
						break;
					    case 2: 
#ifdef PS_STYPE_FLOATINGPOINT
						for( int b = 0; b < cur_size; b++ ) render_buf2[ b ] *= data->add_buf[ b ];
#else
						for( int b = 0; b < cur_size; b++ ) render_buf2[ b ] = (PS_STYPE2)render_buf2[ b ] * (PS_STYPE2)data->add_buf[ b ] / PS_STYPE_ONE;
#endif
						break;
					    case 3: 
						for( int b = 0; b < cur_size; b++ )
						{
						    PS_STYPE v1 = render_buf2[ b ];
						    PS_STYPE v2 = data->add_buf[ b ];
						    if( v2 < v1 ) render_buf2[ b ] = v2;
						}
						break;
					    case 4: 
						for( int b = 0; b < cur_size; b++ )
						{
						    PS_STYPE v1 = render_buf2[ b ];
						    PS_STYPE v2 = data->add_buf[ b ];
						    if( v2 > v1 ) render_buf2[ b ] = v2;
						}
						break;
					    case 5: 
						for( int b = 0; b < cur_size; b++ )
						{
						    int v1, v2;
						    PS_STYPE_TO_INT16( v1, render_buf2[ b ] );
						    PS_STYPE_TO_INT16( v2, data->add_buf[ b ] );
						    v1 = v1 & v2;
						    PS_INT16_TO_STYPE( render_buf2[ b ], v1 );
						}
						break;
					    case 6: 
						for( int b = 0; b < cur_size; b++ )
						{
						    int v1, v2;
						    PS_STYPE_TO_INT16( v1, render_buf2[ b ] );
						    PS_STYPE_TO_INT16( v2, data->add_buf[ b ] );
						    v1 = v1 ^ v2;
						    PS_INT16_TO_STYPE( render_buf2[ b ], v1 );
						}
						break;
					    case 7: 
						for( int b = 0; b < cur_size; b++ )
						{
						    PS_STYPE v1 = render_buf2[ b ];
						    PS_STYPE v2 = data->add_buf[ b ];
						    PS_STYPE v1_abs = PS_STYPE_ABS( v1 );
						    PS_STYPE v2_abs = PS_STYPE_ABS( v2 );
						    if( v2_abs < v1_abs ) render_buf2[ b ] = v2;
						}
						break;
					    case 8: 
						for( int b = 0; b < cur_size; b++ )
						{
						    PS_STYPE v1 = render_buf2[ b ];
						    PS_STYPE v2 = data->add_buf[ b ];
						    PS_STYPE v1_abs = PS_STYPE_ABS( v1 );
						    PS_STYPE v2_abs = PS_STYPE_ABS( v2 );
						    if( v2_abs > v1_abs ) render_buf2[ b ] = v2;
						}
						break;
					}
				    }
				}
				chan->tick_counter += cur_size;
				chan->tick_counter &= GEN2_TICK_SIZE - 1;
				i += cur_size;
			    } 
			}
			gen2_apply_noise( data, render_buf, resamp_frames );
			gen2_apply_envelope( data, chan, render_buf, resamp_frames );
			if( resamp_frames )
			{
			    if( chan->anticlick_counter )
			    {
				for( int i = 0; i < resamp_frames; i++ )
				{
				    PS_STYPE2 v = chan->anticlick_sample;
				    v *= chan->anticlick_counter;
				    v /= (PS_STYPE2)32768;
				    render_buf[ i ] += v;
				    chan->anticlick_counter -= ( 32768 / 32 );
				    if( chan->anticlick_counter < 0 )
				    {
					chan->anticlick_counter = 0;
					break;
				    }
				}
			    }
			    chan->last_sample = render_buf[ resamp_frames - 1 ];
			    chan->no_last_sample = 0;
			}
			int target_volume[ MAX_CHANNELS ]; 
			target_volume[ 0 ] = ( chan_vol * chan->vel ) / 2; 
			for( int ch = 1; ch < MODULE_OUTPUTS; ch++ ) target_volume[ ch ] = target_volume[ 0 ];
			int ctl_pan = data->ctl_pan + ( chan->local_pan - 128 );
			if( ctl_pan < 0 ) ctl_pan = 0;
			if( ctl_pan > 256 ) ctl_pan = 256;
			if( ctl_pan < 128 )
			{
			    if( MODULE_OUTPUTS > 1 ) { target_volume[ 1 ] = target_volume[ 1 ] * ctl_pan / 128; }
			}
			else
			{
			    target_volume[ 0 ] = ( target_volume[ 0 ] * ( 128 - ( ctl_pan - 128 ) ) ) / 128;
			}
			if( first_frame )
			{
			    first_frame = 0;
			    for( int ch = 0; ch < MODULE_OUTPUTS; ch++ )
				psmoother_reset( &chan->vol[ ch ], target_volume[ ch ], 32768 );
			}
			for( int ch = 0; ch < outputs_num; ch++ )
			{
			    PS_STYPE* out = resamp_outputs[ ch ] + resamp_offset;
			    PS_STYPE2 target_vol = psmoother_target( target_volume[ ch ], 32768 ); 
			    psmoother* svol = &chan->vol[ ch ];
			    if( psmoother_check( svol, target_vol ) )
			    {
				for( int i = 0; i < resamp_frames; i++ )
				{
				    PS_STYPE2 v = render_buf[ i ];
#ifdef PS_STYPE_FLOATINGPOINT
            			    v = v * psmoother_val( &data->smoother_coefs, svol, target_vol );
#else
            			    v = v * psmoother_val( &data->smoother_coefs, svol, target_vol ) / 32768;
#endif
				    if( retval == 0 )
					out[ i ] = v;
				    else
					out[ i ] += v;
				}
				empty_output = false;
			    }
			    else
			    {
				int vol = target_volume[ ch ]; 
				PS_STYPE2 vol2 = PS_NORM_STYPE( vol, 32768 );
				if( retval == 0 )
				{
				    if( vol != 32768 )
				    {
					if( vol == 0 )
					{
					    for( int i = 0; i < resamp_frames; i++ ) out[ i ] = 0;
					}
					else
					{
					    for( int i = 0; i < resamp_frames; i++ )
					    {
						out[ i ] = PS_NORM_STYPE_MUL( render_buf[ i ], vol2, 32768 );
					    }
					    empty_output = false;
					}
				    }
				    else
				    {
					if( out != render_buf )
					{
					    for( int i = 0; i < resamp_frames; i++ ) out[ i ] = render_buf[ i ];
					}
					empty_output = false;
				    }
				}
				else
				{
				    if( vol != 32768 )
				    {
					if( vol == 0 )
					{
					}
					else
					{
					    for( int i = 0; i < resamp_frames; i++ )
					    {
						out[ i ] += PS_NORM_STYPE_MUL( render_buf[ i ], vol2, 32768 );
					    }
					    empty_output = false;
					}
				    }
				    else
				    {
					for( int i = 0; i < resamp_frames; i++ ) out[ i ] += render_buf[ i ];
					empty_output = false;
				    }
				}
			    }
			}
			retval = 1;
		    }
		}
		if( retval && empty_output )
		    retval = 2; 
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
			    if( data->ctl_sustain )
				data->channels[ c ].sustain = 0;
			    if( pnet->base_host_version >= 0x01070303 )
			    {
				if( data->ctl_f_envelope_mode == f_env_sustain_on )
				    data->channels[ c ].f_sustain = 0;
			    }
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
		gen2_channel* chan = &data->channels[ c ];
		gen2_set_pitch( data, chan, event->note.pitch );
		gen2_prepare_channel( data, chan );
		chan->vel = event->note.velocity;
		chan->id = event->id;
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
		    gen2_set_target_pitch( data, &data->channels[ c ], event->note.pitch );
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
		gen2_channel* chan = &data->channels[ c ];
		if( chan->id == event->id )
		{
		    gen2_set_offset( data, chan, event->sample_offset.sample_offset );
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
		    if( data->ctl_sustain )
			data->channels[ c ].sustain = 0;
		    if( data->ctl_f_envelope_mode == f_env_sustain_on )
			data->channels[ c ].f_sustain = 0;
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].sustain = 0;
		data->channels[ c ].f_sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
		gen2_reset_channel( &data->channels[ c ] );
	    data->no_active_channels = 1;
#ifndef ONLY44100
	    psynth_resampler_reset( data->resamp );
#endif
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
			    case 0:
				data->channels[ c ].local_volume = ( event->controller.ctl_val * 256 ) / 32768;
				retval = 1;
				break;
			    case 1:
				{
				    int type = event->controller.ctl_val;
				    if( type > MAX_TYPE ) type = MAX_TYPE;
				    data->channels[ c ].local_type = type;
				}
				retval = 1;
				break;
			    case 2:
				data->channels[ c ].local_pan = event->controller.ctl_val >> 7;
				retval = 1;
				break;
			    case 7:
				data->channels[ c ].local_duty_cycle = ( event->controller.ctl_val * MAX_DUTY_CYCLE ) / 32768;
				retval = 1;
				break;
			    case 9:
				{
				    int type = event->controller.ctl_val;
				    if( type > MAX_FILTER_TYPE ) type = MAX_FILTER_TYPE;
				    data->channels[ c ].local_f_type = type;
				}
				retval = 1;
				break;
			    case 10:
				data->channels[ c ].local_f_freq = ( event->controller.ctl_val * MAX_FILTER_FREQ ) / 32768;
				retval = 1;
				break;
			    case 11:
				data->channels[ c ].local_f_res = ( event->controller.ctl_val * MAX_FILTER_RES ) / 32768;
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
		gen2_options* opt = data->opt;
                int v = event->controller.ctl_val;
                switch( event->controller.ctl_num + 1 )
                {
                    case 127: opt->volume_env_scaling = v!=0; retval = 1; break;
                    case 126: opt->filter_env_scaling = v!=0; retval = 1; break;
                    case 125: opt->volume_scaling = v!=0; retval = 1; break;
                    case 124: opt->filter_scaling = v!=0; retval = 1; break;
                    case 123: opt->filter_velocity_scaling = v!=0; retval = 1; break;
                    case 122: opt->freq_div = v!=0; retval = 1; break;
                    case 121: opt->no_smooth_freq = !v; retval = 1; break;
                    case 120: opt->filter_scaling_reverse = v!=0; retval = 1; break;
                    case 119: opt->retain_phase = v!=0; if( v ) opt->random_phase = 0; retval = 1; break;
                    case 118: opt->random_phase = v!=0; if( v ) opt->retain_phase = 0; retval = 1; break;
                    case 117: opt->filter_pitch = v!=0; if( v ) { opt->filter_scaling = 0; opt->filter_scaling_reverse = 0; } retval = 1; break;
                    case 116: opt->filter_res_velocity_scaling = v!=0; retval = 1; break;
                    case 115: opt->fast_zero_attack_release = v!=0; retval = 1; break;
                    case 114: opt->freq_accuracy = v!=0; retval = 1; break;
                    case 113: opt->always_play_osc2 = v!=0; retval = 1; break;
                    default: break;
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
	    data->noise_data = 0;
#ifndef ONLY44100
	    psynth_resampler_remove( data->resamp );
#endif
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
