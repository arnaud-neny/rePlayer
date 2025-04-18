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
#include "sunvox_engine.h"
#include "psynths_adsr.h"
#define MODULE_DATA	psynth_adsr_data
#define MODULE_HANDLER	psynth_adsr
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	16
#define CHANGED_IOCHANNELS      ( 1 << 0 )
const uint32_t STATE_FLAG_D = 1 << 0; 
const uint32_t STATE_FLAG_R = 1 << 1; 
struct poly_channel
{
    uint32_t id;
};
struct MODULE_DATA
{
    PS_CTYPE            ctl_volume; 
    PS_CTYPE            ctl_a;
    PS_CTYPE            ctl_d;
    PS_CTYPE            ctl_s;
    PS_CTYPE            ctl_r;
    PS_CTYPE            ctl_acurve;
    PS_CTYPE            ctl_dcurve;
    PS_CTYPE            ctl_rcurve;
    PS_CTYPE            ctl_sustain; 
    PS_CTYPE            ctl_sustain_pedal; 
    PS_CTYPE            ctl_state; 
    PS_CTYPE            ctl_noteon; 
    PS_CTYPE            ctl_noteoff; 
    PS_CTYPE            ctl_mode; 
    PS_CTYPE            ctl_smooth; 
    adsr_env		a;
    poly_channel	chans[ MAX_CHANNELS ];
    int			active_chans;
    uint32_t            changed; 
#ifdef SUNVOX_GUI
    window_manager*	wm;
#endif
};
static int get_curve_val( int x, int c )
{
    switch( c )
    {
	default: case 0: break;
	case 1: x = ( x * x ) >> 15; break;
	case 2: x = ( x * x ) >> 15; x = ( x * x ) >> 15; break;
	case 3: x = x - 32768; x = 32768 - ( ( x * x ) >> 15 ); break;
	case 4: x = x - 32768; x = ( x * x ) >> 15; x = 32768 - ( ( x * x ) >> 15 ); break;
        case 5: x = ( sinf( (float)M_PI * x / 32768.0F - (float)M_PI/2 ) + 1 ) * 32768/2; break;
        case 6: if( x ) x = 32768; break;
        case 7: x = x * 32; if( x > 32768 ) x = 32768; break;
        case 8: x = ( x >> 14 ) << 14; break;
        case 9: x = ( x >> 13 ) << 13; break;
        case 10: x = ( x >> 12 ) << 12; break;
        case 11: x = ( x >> 11 ) << 11; break;
    }
    return x;
}
static int get_curve_val_inv( int y, int c )
{
    switch( c )
    {
	default: case 0: break;
	case 1: y = sqrt( (float)( y << 15 ) ); break;
	case 2: y = sqrt( sqrt( y * 32768.0 * 32768.0 * 32768.0 ) ); break;
	case 3: y = 32768 - sqrt( (float)( ( 32768 - y ) << 15 ) ); break;
	case 4: y = 32768 - sqrt( sqrt( 32768.0 * 32768.0 * 32768.0 * ( 32768 - y ) ) ); break;
	case 5: y = ( asinf( y / (32768.0F/2) - 1.0F ) + (float)M_PI/2 ) * 32768.0F / (float)M_PI; break;
	case 6: if( y ) y = 1; break;
	case 7: y = y / 32; break;
        case 8: y = ( y >> 14 ) << 14; break;
        case 9: y = ( y >> 13 ) << 13; break;
        case 10: y = ( y >> 12 ) << 12; break;
        case 11: y = ( y >> 11 ) << 11; break;
    }
    return y;
}
static void adsr_env_reset_volume_filter( adsr_env* a )
{
    psmoother_reset( &a->vol, a->ctl_volume, 32768 );
}
static void adsr_env_restart( adsr_env* a )
{
    a->v = 0;
    a->v4 = 0;
    if( a->v5 )
    {
	if( a->ctl_smooth )
	{
	    int last_smp;
	    PS_STYPE_TO_INT16( last_smp, a->v5 );
	    switch( a->ctl_smooth )
	    {
		case 1: a->v4 = last_smp << 15; break;
		case 2: a->v = get_curve_val_inv( last_smp, a->ctl_acurve ) << 15; break;
	    }
	}
    }
    else
    {
	adsr_env_reset_volume_filter( a );
    }
    a->v2 = 32768 << 15;
    a->state_flags = 0;
    if( a->ctl_a == 0 )
    {
	a->v = 32768 << 15; 
	a->state_flags |= STATE_FLAG_D; 
    }
    a->playing = true;
}
void adsr_env_start( adsr_env* a )
{
    adsr_env_restart( a );
    a->pressed = 1;
}
void adsr_env_stop( adsr_env* a )
{
    a->pressed = 0;
}
void adsr_env_reset( adsr_env* a )
{
    a->state_flags = 0;
    a->playing = false;
    a->v4 = 0;
    a->v5 = 0;
    adsr_env_reset_volume_filter( a );
}
void adsr_env_init( adsr_env* a, bool filled_with_zeros, int srate )
{
    if( !filled_with_zeros ) memset( a, 0, sizeof( adsr_env ) );
    a->ctl_volume = 32768;
    a->ctl_a = 100;
    a->ctl_d = 100;
    a->ctl_s = 32768 / 2;
    a->ctl_r = 100;
    a->ctl_sustain = 1;
    a->ctl_smooth = 2;
    a->srate = srate;
    a->delta_recalc_request = true;
    psmoother_init( &a->smoother_coefs, 100, srate );
    adsr_env_reset_volume_filter( a );
}
void adsr_env_change_srate( adsr_env* a, int srate )
{
    if( a->srate != srate )
    {
	a->srate = srate;
	a->delta_recalc_request = true;
	psmoother_init( &a->smoother_coefs, 100, srate );
    }
}
void adsr_env_change_adr( adsr_env* a, int alen, int dlen, int rlen )
{
    if( a->ctl_a != alen || a->ctl_d != dlen || a->ctl_r != rlen )
    {
	a->ctl_a = alen;
	a->ctl_d = dlen;
	a->ctl_r = rlen;
	a->delta_recalc_request = true;
    }
}
void adsr_env_deinit( adsr_env* a )
{
}
psynth_buf_state adsr_env_run( adsr_env* a, PS_STYPE* RESTRICT out, int frames, int* out_frames )
{
    if( !a->playing )
    {
	if( out_frames ) *out_frames = 0;
	return PS_BUF_SILENCE;
    }
    psynth_buf_state retval = PS_BUF_FILLED;
    if( a->delta_recalc_request )
    {
        a->delta_recalc_request = false;
	int len;
	len = (uint64_t)a->ctl_a * (uint64_t)a->srate / 1000;
	if( !len ) len = 1;
	a->ad = ( 32768 << 15 ) / len;
	if( !a->ad ) a->ad = 1;
	len = (uint64_t)a->ctl_d * (uint64_t)a->srate / 1000;
	if( !len ) len = 1;
	a->dd = ( 32768 << 15 ) / len;
	if( !a->dd ) a->dd = 1;
	len = (uint64_t)a->ctl_r * (uint64_t)a->srate / 1000;
	if( !len ) len = 1;
	a->rd = ( 32768 << 15 ) / len;
	if( !a->rd ) a->rd = 1;
    }
    bool pressed = a->pressed;
    if( pressed == false )
    {
	if( a->ctl_sustain_pedal == 0 )
	{
    	    a->state_flags |= STATE_FLAG_R; 
	}
    }
    int i = 0;
    for( ; i < frames; i++ )
    {
	if( a->state_flags & STATE_FLAG_R )
	{
	    a->v2 -= a->rd;
	    if( a->v2 < 0 )
	    {
    	        a->v2 = 0;
    		a->v5 = 0;
    		a->state_flags = 0; 
    		a->playing = false;
    		if( a->ctl_sustain == 2 && ( pressed || a->ctl_sustain_pedal ) )
    		{
    		    adsr_env_restart( a );
    		}
    		else
    		{
		    adsr_env_reset_volume_filter( a );
    		    break;
    		}
	    }
	}
	if( a->state_flags & STATE_FLAG_D )
	{
	    a->v -= a->dd;
	    if( a->v < 0 )
	    {
    		a->v = 0;
    		if( a->ctl_sustain != 1 )
    		{
        	    a->state_flags |= STATE_FLAG_R;
    		}
	    }
	}
	else
	{
	    a->v += a->ad;
	    if( a->v >= 32768 << 15 )
	    {
    		a->v = 32768 << 15;
    		a->state_flags |= STATE_FLAG_D; 
	    }
	}
	int c;
	if( a->state_flags & STATE_FLAG_D )
	{
	    c = a->ctl_dcurve;
	}
	else
	{
	    c = a->ctl_acurve;
	}
	int v = get_curve_val( a->v >> 15, c );
        int v2 = get_curve_val( a->v2 >> 15, a->ctl_rcurve );
	if( a->state_flags & STATE_FLAG_D )
	{
    	    v = ( ( v * ( 32768 - a->ctl_s ) ) >> 15 ) + a->ctl_s;
	}
#ifdef PS_STYPE_FLOATINGPOINT
	out[ i ] = (PS_STYPE)( v * v2 ) / (PS_STYPE)( 1 << 30 );
#else
	out[ i ] = ( v * v2 ) >> ( 30 - PS_STYPE_BITS );
#endif
    }
    if( a->v4 )
    {
        for( int i2 = 0; i2 < i; i2++ )
        {
    	    a->v4 -= a->ad * 2;
            if( a->v4 < 0 ) { a->v4 = 0; break; }
            PS_STYPE2 v;
	    PS_INT16_TO_STYPE( v, a->v4 >> 15 );
	    out[ i2 ] += v;
        }
    }
    a->v5 = 0; if( a->playing && i == frames ) a->v5 = out[ frames - 1 ];
    bool static_vol = true;
    int ctl_volume = a->ctl_volume;
#ifndef PS_STYPE_FLOATINGPOINT
    if( ctl_volume >= 32768 * 8 )
	ctl_volume = 32768 * 8 - 1;
#endif
    if( a->ctl_smooth )
    {
	PS_STYPE2 target_vol = psmoother_target( ctl_volume, 32768 );
	if( psmoother_check( &a->vol, target_vol ) )
	{
	    static_vol = false;
	    for( int i2 = 0; i2 < i; i2++ )
	    {
#ifdef PS_STYPE_FLOATINGPOINT
		out[ i2 ] *= psmoother_val( &a->smoother_coefs, &a->vol, target_vol );
#else
	        out[ i2 ] = (PS_STYPE2)out[ i2 ] * psmoother_val( &a->smoother_coefs, &a->vol, target_vol ) / 32768;
#endif
	    }
	}
    }
    if( static_vol )
    {
        if( ctl_volume < 32768 )
        {
    	    if( ctl_volume == 0 )
    	    {
    		retval = PS_BUF_FILLED_SILENCE;
		for( int i2 = 0; i2 < i; i2++ )
	    	    out[ i2 ] = 0;
    	    }
    	    else
    	    {
    		PS_STYPE2 ctl_volume2 = PS_NORM_STYPE( ctl_volume, 32768 );
		for( int i2 = 0; i2 < i; i2++ )
	    	    out[ i2 ] = PS_NORM_STYPE_MUL( out[ i2 ], ctl_volume2, 32768 );
	    }
        }
        if( ctl_volume > 32768 )
        {
    	    PS_STYPE2 ctl_volume2 = PS_NORM_STYPE( ctl_volume, 32768 );
	    for( int i2 = 0; i2 < i; i2++ )
		out[ i2 ] = PS_NORM_STYPE_MUL2( out[ i2 ], ctl_volume2, 32768, 32768 * 8 );
        }
    }
    if( out_frames ) *out_frames = i;
    for( ; i < frames; i++ ) out[ i ] = 0;
    return retval;
}
#ifdef SUNVOX_GUI
struct adsr_visual_data
{
    MODULE_DATA*        module_data;
    int                 mod_num;
    psynth_net*         pnet;
    int                 min_ysize;
};
static void adsr_line( int x1, int xsize, int y1, int y2, COLOR color, int curve, window_manager* wm )
{
    int x0 = x1;
    int y0 = y1;
    int steps = 16;
    for( int i = 1; i <= steps; i++ )
    {
	int c = get_curve_val( i * ( 32768 / steps ), curve );
	int x = x1 + xsize * i / steps;
	int y = y1 + (y2-y1) * c / 32768;
        draw_line( x0, y0, x, y, color, wm );
	x0 = x;
	y0 = y;
    }
}
int adsr_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    adsr_visual_data* data = (adsr_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( adsr_visual_data );
            break;
        case EVT_AFTERCREATE:
            data->pnet = (psynth_net*)win->host;
            data->min_ysize = wm->scrollbar_size * 2;
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
            int xx = wm->scrollbar_size;
            int yy = xx;
            int x = 0;
            int y = 0;
            int sl = y + yy - data->module_data->ctl_s * yy / 32768;
            COLOR c1 = wm->color2;
            COLOR c2 = blend( wm->color1, wm->color2, 64 );
            wm->cur_font_color = c1;
	    draw_line( x, y, x, y + yy, c2, wm );
	    adsr_line( x, xx, y + yy, y, c1, data->module_data->ctl_acurve, wm );
            draw_string( "A", x + ( xx - char_x_size('A',wm) ) / 2, y + yy + 1, wm );
	    x += xx;
	    draw_line( x, y, x, y + yy, c2, wm );
	    adsr_line( x+xx, -xx, sl, y, c1, data->module_data->ctl_dcurve, wm );
            draw_string( "D", x + ( xx - char_x_size('D',wm) ) / 2, y + yy + 1, wm );
	    x += xx;
	    draw_line( x, y, x, y + yy, c2, wm );
	    if( data->module_data->ctl_sustain != 1 )
		wm->cur_opacity = 100;
	    adsr_line( x, xx, sl, sl, c1, 0, wm );
            draw_string( "S", x + ( xx - char_x_size('S',wm) ) / 2, y + yy + 1, wm );
	    wm->cur_opacity = 255;
	    x += xx;
	    draw_line( x, y, x, y + yy, c2, wm );
	    adsr_line( x+xx, -xx, y + yy, sl, c1, data->module_data->ctl_rcurve, wm );
            draw_string( "R", x + ( xx - char_x_size('R',wm) ) / 2, y + yy + 1, wm );
	    x += xx;
	    draw_line( x, y, x, y + yy, c2, wm );
            wbd_draw( wm );
            wbd_unlock( wm );
            retval = 1;
            break;
    }
    return retval;
}
#endif
static void adsr_handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->changed & CHANGED_IOCHANNELS )
    {
        if( data->ctl_mode < 2 )
        {
	    if( psynth_get_number_of_outputs( mod ) != 1 )
    	    {
    	        psynth_set_number_of_inputs( 1, mod_num, pnet );
    	        psynth_set_number_of_outputs( 1, mod_num, pnet );
    	    }
	}
	else
	{
    	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
    	    {
    		psynth_set_number_of_inputs( MODULE_INPUTS, mod_num, pnet );
    		psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
    	    }
	}
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
	    retval = (PS_RETTYPE)"ADSR";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Генератор огибающей ADSR. Используйте Sound2Ctl для преобразования сигнала огибающей в управляющие команды (автоматизация контроллеров).";
                        break;
                    }
		    retval = (PS_RETTYPE)"ADSR envelope generator. Use Sound2Ctl to convert the envelope signal into control commands (controllers automation).";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF7F95";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    adsr_env_init( &data->a, true, pnet->sampling_freq );
	    psynth_resize_ctls_storage( mod_num, 15, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 32768, data->a.ctl_volume, 0, &data->ctl_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK ), ps_get_string( STR_PS_MS ), 0, 10000, data->a.ctl_a, 0, &data->ctl_a, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DECAY ), ps_get_string( STR_PS_MS ), 0, 10000, data->a.ctl_d, 0, &data->ctl_d, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN_LEVEL ), "", 0, 32768, data->a.ctl_s, 0, &data->ctl_s, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE ), ps_get_string( STR_PS_MS ), 0, 10000, data->a.ctl_r, 0, &data->ctl_r, -1, 1, pnet );
	    psynth_set_ctl_flags( mod_num, 1, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_set_ctl_flags( mod_num, 2, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_set_ctl_flags( mod_num, 4, PSYNTH_CTL_FLAG_EXP3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ATTACK_CURVE ), ps_get_string( STR_PS_ADSR_CURVE_TYPES ), 0, ADSR_CURVE_TYPES-1, data->a.ctl_acurve, 1, &data->ctl_acurve, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DECAY_CURVE ), ps_get_string( STR_PS_ADSR_CURVE_TYPES ), 0, ADSR_CURVE_TYPES-1, data->a.ctl_dcurve, 1, &data->ctl_dcurve, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_RELEASE_CURVE ), ps_get_string( STR_PS_ADSR_CURVE_TYPES ), 0, ADSR_CURVE_TYPES-1, data->a.ctl_rcurve, 1, &data->ctl_rcurve, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN ), ps_get_string( STR_PS_ADSR_OFF_ON_REPEAT ), 0, ADSR_SUSTAIN_MODES-1, data->a.ctl_sustain, 1, &data->ctl_sustain, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SUSTAIN_PEDAL ), ps_get_string( STR_PS_OFF_ON ), 0, 1, data->a.ctl_sustain_pedal, 1, &data->ctl_sustain_pedal, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_STATE ), ps_get_string( STR_PS_STOP_START ), 0, 1, 0, 1, &data->ctl_state, -1, 4, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ON_NOTEON ), ps_get_string( STR_PS_ADSR_NOTEON_ACTIONS ), 0, 2, 1, 1, &data->ctl_noteon, -1, 5, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ON_NOTEOFF ), ps_get_string( STR_PS_ADSR_NOTEOFF_ACTIONS ), 0, 2, 1, 1, &data->ctl_noteoff, -1, 5, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), ps_get_string( STR_PS_ADSR_MODES ), 0, 2, 0, 1, &data->ctl_mode, -1, 5, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SMOOTH_TRANSITIONS ), ps_get_string( STR_PS_ADSR_SMOOTH_TRANSITIONS ), 0, ADSR_SMOOTH_MODES-1, data->a.ctl_smooth, 1, &data->ctl_smooth, -1, 5, pnet );
	    for( int c = 0; c < MAX_CHANNELS; c++ ) data->chans[ c ].id = ~0;
	    data->changed = 0xFFFFFFFF;
#ifdef SUNVOX_GUI
            {
                data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
                    mod->visual = new_window( "ADSR GUI", 0, 0, 10, 10, wm->color1, 0, pnet, adsr_visual_handler, wm );
                    adsr_visual_data* v_data = (adsr_visual_data*)mod->visual->data;
                    mod->visual_min_ysize = v_data->min_ysize;
                    v_data->module_data = data;
                    v_data->mod_num = mod_num;
                    v_data->pnet = pnet;
                }
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    adsr_handle_changes( mod, mod_num );
	    data->a.ctl_volume = data->ctl_volume;
	    data->a.ctl_a = data->ctl_a;
	    data->a.ctl_d = data->ctl_d;
	    data->a.ctl_s = data->ctl_s;
	    data->a.ctl_r = data->ctl_r;
	    data->a.ctl_acurve = data->ctl_acurve;
	    data->a.ctl_dcurve = data->ctl_dcurve;
	    data->a.ctl_rcurve = data->ctl_rcurve;
	    data->a.ctl_sustain = data->ctl_sustain;
	    data->a.ctl_sustain_pedal = data->ctl_sustain_pedal;
	    data->a.pressed = data->ctl_state;
	    data->a.ctl_smooth = data->ctl_smooth;
	    adsr_env_reset( &data->a );
	    if( data->ctl_state ) { adsr_env_start( &data->a ); mod->draw_request++; }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    adsr_env_reset( &data->a );
	    for( int c = 0; c < MAX_CHANNELS; c++ ) data->chans[ c ].id = ~0;
	    data->active_chans = 0;
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( !data->a.playing ) break;
		if( data->changed ) adsr_handle_changes( mod, mod_num );
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		int outputs_num = psynth_get_number_of_outputs( mod );
		PS_STYPE* out = outputs[ 0 ] + offset;
		retval = adsr_env_run( &data->a, out, frames, NULL );
                if( data->ctl_mode && retval )
                {
            	    for( int ch = outputs_num - 1; ch >= 0; ch-- )
            	    {
			PS_STYPE* in = inputs[ ch ] + offset;
			PS_STYPE* out2 = outputs[ ch ] + offset;
			for( int i2 = 0; i2 < frames; i2++ )
			{
			    out2[ i2 ] = (PS_STYPE2)in[ i2 ] * (PS_STYPE2)out[ i2 ] / PS_STYPE_ONE;
			}
            	    }
                }
	    }
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    if( 1 ) 
	    {
		psynth_set_ctl2( mod, event );
            	retval = 1;
		switch( event->controller.ctl_num )
		{
		    case 0: data->a.ctl_volume = data->ctl_volume; break;
		    case 1: data->a.ctl_a = data->ctl_a; data->a.delta_recalc_request = true; break;
		    case 2: data->a.ctl_d = data->ctl_d; data->a.delta_recalc_request = true; break;
		    case 3: data->a.ctl_s = data->ctl_s; break;
		    case 4: data->a.ctl_r = data->ctl_r; data->a.delta_recalc_request = true; break;
		    case 5: data->a.ctl_acurve = data->ctl_acurve; break;
		    case 6: data->a.ctl_dcurve = data->ctl_dcurve; break;
		    case 7: data->a.ctl_rcurve = data->ctl_rcurve; break;
		    case 8: data->a.ctl_sustain = data->ctl_sustain; break;
		    case 9: data->a.ctl_sustain_pedal = data->ctl_sustain_pedal; break;
		    case 10:
            		if( data->a.pressed != data->ctl_state || ( !data->a.playing && data->ctl_state ) )
            		{
        		    data->a.pressed = data->ctl_state;
            		    if( data->a.pressed ) adsr_env_start( &data->a );
            		}
			break;
		    case 13: data->changed |= CHANGED_IOCHANNELS; break; 
		    case 14: data->a.ctl_smooth = data->ctl_smooth; break;
		}
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c = 0;
                if( data->active_chans > 0 )
                {
                    for( c = 0; c < MAX_CHANNELS; c++ )
                    {
                        poly_channel* chan = &data->chans[ c ];
                        if( chan->id == event->id ) break;
                    }
                    if( c >= MAX_CHANNELS )
                    {
                        for( c = 0; c < MAX_CHANNELS; c++ )
                        {
                            poly_channel* chan = &data->chans[ c ];
                            if( chan->id == (uint32_t)~0 )
                            {
                                chan->id = event->id;
                                data->active_chans++;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    poly_channel* chan = &data->chans[ c ];
                    chan->id = event->id;
                    data->active_chans++;
                }
		if( data->ctl_noteon )
		{
		    bool start = false;
		    if( data->ctl_noteon == 2 ) start = true;
		    if( data->ctl_noteon == 1 )
		    {
            		if( c < MAX_CHANNELS )
                	{
                	    if( data->active_chans == 1 ) start = true;
                	}
		    }
		    if( start )
		    {
			adsr_env_start( &data->a );
            		data->ctl_state = data->a.pressed;
			mod->draw_request++;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    {
		if( data->active_chans > 0 )
                {
                    for( int c = 0; c < MAX_CHANNELS; c++ )
                    {
                        poly_channel* chan = &data->chans[ c ];
                        if( chan->id == event->id )
                        {
                            chan->id = ~0;
                            data->active_chans--;
                            break;
                        }
                    }
                }
		if( data->ctl_noteoff )
		{
		    bool stop = false;
		    if( data->ctl_noteoff == 2 ) stop = true;
		    if( data->ctl_noteoff == 1 )
		    {
                	if( data->active_chans == 0 ) stop = true;
            	    }
		    if( stop )
		    {
			adsr_env_stop( &data->a );
			data->ctl_state = data->a.pressed;
			mod->draw_request++;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    if( data->ctl_noteoff )
	    {
		adsr_env_stop( &data->a );
		data->ctl_state = data->a.pressed;
		mod->draw_request++;
		for( int c = 0; c < MAX_CHANNELS; c++ )
        	{
            	    data->chans[ c ].id = ~0;
        	}
		data->active_chans = 0;
	    }
	    retval = 1;
	    break;
	case PS_CMD_RESET_MSB:
            data->changed |= CHANGED_IOCHANNELS;
            break;
	case PS_CMD_CLOSE:
	    adsr_env_deinit( &data->a );
#ifdef SUNVOX_GUI
            if( mod->visual && data->wm )
            {
                remove_window( mod->visual, data->wm );
                mod->visual = NULL;
            }
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
