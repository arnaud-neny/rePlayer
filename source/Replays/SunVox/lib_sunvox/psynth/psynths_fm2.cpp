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
#define MODULE_DATA	psynth_fm2_data
#define MODULE_HANDLER	psynth_fm2
#define MODULE_VDATA	fm2_visual_data
#define MODULE_VHANDLER	fm2_visual
#define MODULE_INPUTS	1
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	32
#define PHASE_BITS	( 8 + 16 )
#define WAVETABLE_BITS	8
#define WAVETABLE_SIZE	( 1 << WAVETABLE_BITS )
#define WAVETABLE_MASK	( ( 1 << WAVETABLE_BITS ) - 1 )
#define NUM_OPS		5 
#define OP_BUF_SIZE	64
#define EMPTY_ID	(~0)
#define CHANGED_IOCHANNELS	( 1 << 0 )
#define CHANGED_RESAMPLER	( 1 << 1 )
#define CHANGED_PITCH_PARS	( 1 << 2 )
#define CHANGED_ADSR_SMOOTH	( 1 << 3 )
#define CHANGED_NOISE_FILTER	( 1 << 4 )
#define CHANGED_OP1_offset	5
#define CHANGED_OP1		( 1 << CHANGED_OP1_offset )
#define CHANGED_OP2		( 1 << (CHANGED_OP1_offset+1) )
#define CHANGED_OP1_VOL_offset	( CHANGED_OP1_offset + NUM_OPS )
#define CHANGED_OP1_VOL		( 1 << CHANGED_OP1_VOL_offset )
#define CHANGED_OP2_VOL		( 1 << (CHANGED_OP1_VOL_offset+1) )
#define NUM_RATES	7
static int g_rates[ NUM_RATES ] = { 8000, 11025, 16000, 22050, 32000, 44100, 0 };
enum { 
    mod_type_phase,
    mod_type_freq,
    mod_type_mul,
    mod_type_add,
    mod_type_sub,
    mod_type_min,
    mod_type_max,
    mod_type_and,
    mod_type_xor,
    mod_type_phase2,
    mod_type_minabs,
    mod_type_maxabs,
    mod_types
};
enum {
    FM2_PAR_VOLUME, 
    FM2_PAR_A, 
    FM2_PAR_D, 
    FM2_PAR_S, 
    FM2_PAR_R, 
    FM2_PAR_ACURVE, 
    FM2_PAR_DCURVE, 
    FM2_PAR_RCURVE, 
    FM2_PAR_SUSTAIN, 
    FM2_PAR_SUSTAIN_PEDAL, 
    FM2_PAR_ENV_SCALING, 
    FM2_PAR_VOL_SCALING, 
    FM2_PAR_VEL_SENS, 
    FM2_PAR_WAVEFORM, 
    FM2_PAR_NOISE, 
    FM2_PAR_PHASE, 
    FM2_PAR_FMUL, 
    FM2_PAR_CONST_PITCH, 
    FM2_PAR_SELFMOD, 
    FM2_PAR_FEEDBACK, 
    FM2_PAR_MOD, 
    FM2_PAR_MODE, 
    FM2_PARS
};
#define FM2_OP_CTLS	( FM2_PARS * NUM_OPS - 1 )
struct fm2_op_par_desc
{
    int16_t		name; 
    int16_t		label; 
    PS_CTYPE		min;
    PS_CTYPE		max;
    PS_CTYPE		def;
    PS_CTYPE		normal; 
    int16_t		offset; 
    uint16_t		flags; 
    uint8_t		type;
};
const static char* g_strings[] = { "1/1000" };
const static fm2_op_par_desc g_fm2_par_desc[ FM2_PARS ] =
{
    { STR_PS_VOLUME, -1, 0, 32768, 32768, -1, 0, 0, 0 },
    { STR_PS_ATTACK, STR_PS_MS, 0, 10000, 100, -1, 0, PSYNTH_CTL_FLAG_EXP3, 0 },
    { STR_PS_DECAY, STR_PS_MS, 0, 10000, 100, -1, 0, PSYNTH_CTL_FLAG_EXP3, 0 },
    { STR_PS_SUSTAIN_LEVEL, -1, 0, 32768, 32768/2, -1, 0, 0, 0 },
    { STR_PS_RELEASE, STR_PS_MS, 0, 10000, 100, -1, 0, PSYNTH_CTL_FLAG_EXP3, 0 },
    { STR_PS_ATTACK_CURVE, STR_PS_ADSR_CURVE_TYPES, 0, ADSR_CURVE_TYPES-1, 0, -1, 0, 0, 1 },
    { STR_PS_DECAY_CURVE, STR_PS_ADSR_CURVE_TYPES, 0, ADSR_CURVE_TYPES-1, 0, -1, 0, 0, 1 },
    { STR_PS_RELEASE_CURVE, STR_PS_ADSR_CURVE_TYPES, 0, ADSR_CURVE_TYPES-1, 0, -1, 0, 0, 1 },
    { STR_PS_SUSTAIN, STR_PS_ADSR_OFF_ON_REPEAT, 0, ADSR_SUSTAIN_MODES-1, 0, -1, 0, 0, 1 },
    { STR_PS_SUSTAIN_PEDAL, STR_PS_OFF_ON, 0, 1, 0, -1, 0, 0, 1 },
    { STR_PS_ENV_SCALING, -1, 0, 256, 128, 128, 128, 0, 0 },
    { STR_PS_VOL_SCALING, -1, 0, 256, 128, 128, 128, 0, 0 },
    { STR_PS_VEL_SENS, -1, 0, 256, 128+64, 128+64, 128, 0, 0 },
    { STR_PS_WAVEFORM_TYPE, STR_PS_FM2_WAVE_TYPES, 0, PSYNTH_BASE_WAVES, PSYNTH_BASE_WAVE_SIN + 1, -1, 0, 0, 1 },
    { STR_PS_NOISE, -1, 0, 32768, 0, -1, 0, 0, 0 },
    { STR_PS_PHASE, -1, 0, 32768, 0, -1, 0, 0, 0 },
    { STR_PS_FREQ_MUL, -2, 0, 32*1000, 1000, 1000, 0, PSYNTH_CTL_FLAG_EXP3, 0 },
    { STR_PS_CONST_PITCH, STR_PS_SEMITONE64, 0, 16384, 8192, 8192, 8192, 0, 0 },
    { STR_PS_SELF_MOD, -1, 0, 32768, 0, -1, 0, 0, 0 },
    { STR_PS_FEEDBACK, -1, 0, 32768, 0, -1, 0, PSYNTH_CTL_FLAG_EXP3, 0 },
    { STR_PS_MODULATION_TYPE, STR_PS_MODULATION_TYPES2, 0, mod_types - 1, 0, -1, 0, 0, 1 },
    { STR_PS_OUTPUT_MODE, -1, 0, 15, 0, -1, 0, 0, 1 }
};
struct fm2_operator
{
    adsr_env		env;
    PS_STYPE		feedback_val;
    uint32_t    	ptr; 
    uint64_t    	delta;
    int16_t		noise_v[ 4 ];
    uint32_t    	noise_ptr; 
};
struct gen_channel
{
    bool        	active; 
    bool		on; 
    uint32_t		id; 
    int         	vel; 
    int			pitch; 
    uint32_t 		noise_seed;
    fm2_operator	ops[ NUM_OPS ];
    psynth_renderbuf_pars	renderbuf_pars;
    PS_CTYPE   	 	local_pan; 
};
struct MODULE_DATA
{
    PS_CTYPE    	ctl_volume;
    PS_CTYPE    	ctl_pan;
    PS_CTYPE            ctl_srate;
    PS_CTYPE   		ctl_channels; 
    PS_CTYPE   		ctl_stereo;
    PS_CTYPE   		ctl_send; 
    PS_CTYPE		ctl_capture; 
    PS_CTYPE		ctl_adsr_smooth;
    PS_CTYPE		ctl_adsr_gain; 
    PS_CTYPE		ctl_noise_filter;
    PS_CTYPE		pars[ NUM_OPS * FM2_PARS ]; 
    char*		par_names[ NUM_OPS * FM2_PARS ];
    gen_channel		channels[ MAX_CHANNELS ];
    bool    		no_active_channels;
    int	    		search_ptr;
    int                 srate; 
    psynth_resampler*   resamp;
    bool		use_resampler;
    PS_STYPE*		wavetable; 
    PS_STYPE		custom_wave[ WAVETABLE_SIZE ];
    bool		custom_wave_changed;
    PS_STYPE*		in_buf; 
    PS_STYPE*		cap_buf; 
    int			cap_buf_ptr;
    uint32_t		op_buf_bits; 
    PS_STYPE		op_buf[ OP_BUF_SIZE * NUM_OPS ];
    PS_STYPE		tmp_buf[ OP_BUF_SIZE ]; 
    PS_STYPE		env_buf[ OP_BUF_SIZE ]; 
    int 		env_buf_frames; 
    psynth_buf_state 	env_buf_state;
    psmoother		feedback[ NUM_OPS ];
    psmoother		selfmod[ NUM_OPS ];
    psmoother_coefs	smoother_coefs;
    uint32_t		noise_delta; 
    uint32_t		changed; 
#ifdef SUNVOX_GUI
    window_manager* 	wm;
    volatile uint32_t	op_buf_bits_UI;
    volatile uint32_t	op_buf_bits_UI2;
#endif
};
#ifdef SUNVOX_GUI
struct MODULE_VDATA
{
    MODULE_DATA*        module_data;
    int                 mod_num;
    psynth_net*         pnet;
    int			char_ysize;
    int			op_size;
    int			op_size2; 
    int			op_link_yoffset;
    int			min_ysize;
};
int MODULE_VHANDLER( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    MODULE_VDATA* data = (MODULE_VDATA*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( MODULE_VDATA );
            break;
        case EVT_AFTERCREATE:
        {
            data->pnet = (psynth_net*)win->host;
            data->char_ysize = font_char_y_size( win->font, wm );
            data->op_size = data->char_ysize * 1.5;
            data->op_size2 = data->char_ysize * 2;
            data->op_link_yoffset = ( data->op_size2 - data->op_size ) / 2;
            if( data->op_link_yoffset < 2 ) data->op_link_yoffset = 2;
            data->min_ysize = ( data->op_size2 + data->op_size ) / 2 + data->op_link_yoffset * ( NUM_OPS - 1 ) + wm->interelement_space;
            retval = 1;
            break;
        }
        case EVT_DRAW:
    	{
    	    wbd_lock( win );
            draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
            int char_ysize = data->char_ysize;
            int op_size = data->op_size;
            int op_size2 = data->op_size2;
            int op_link_yoffset = data->op_link_yoffset;
            int x = op_size2 / 2;
            int start_x = x;
            int end_x = start_x + NUM_OPS * op_size2;
            int y = op_size2 / 2;
            uint32_t op_buf_bits = data->module_data->op_buf_bits_UI;
            data->module_data->op_buf_bits_UI2 = op_buf_bits;
            for( int opn = 0; opn < NUM_OPS; opn++ )
            {
        	COLOR col = blend( wm->color2, wm->color3, opn * 256 / NUM_OPS );
    		wm->cur_font_color = col;
        	if( op_buf_bits & ( 1 << opn ) )
        	{
        	    wm->cur_opacity = 128;
        	    draw_frect( x - op_size/2, y - op_size/2, op_size, op_size, col, wm );
        	}
        	wm->cur_opacity = 255;
        	PS_CTYPE* pars = &data->module_data->pars[ FM2_PARS * opn ];
        	int mode = pars[ FM2_PAR_MODE ];
        	int mod_type = pars[ FM2_PAR_MOD ];
        	if( mode == 0 ) wm->cur_opacity = 64;
        	uint32_t c = '1' + opn;
        	int char_xsize = char_x_size( c, wm );
        	draw_char( c, x - char_xsize/2, y - char_ysize/2, wm );
        	draw_rect( x - op_size/2, y - op_size/2, op_size, op_size, col, wm );
        	for( int i = 0; i < NUM_OPS + 1; i++ )
        	{
        	    if( mode & ( 1 << i ) )
        	    {
        		if( NUM_OPS - i == opn + 1 )
        		{
        		    draw_line( x + op_size/2, y, x + op_size2 - op_size/2, y, col, wm );
        		}
        		else
        		{
        		    int x2 = end_x - i * op_size2;
        		    int y2 = y + op_size/2;
        		    int y3 = y + op_size2/2 + opn * op_link_yoffset;
        		    draw_line( x2, y2, x2 - op_link_yoffset, y3, col, wm );
        		    draw_line( x, y + op_size/2, x + op_link_yoffset, y3, col, wm );
        		    draw_line( x + op_link_yoffset, y3, x2 - op_link_yoffset, y3, col, wm );
        		}
		    }
		}
		if( pars[ FM2_PAR_FEEDBACK ] )
		{
        	    if( !( mod_type == mod_type_freq || mod_type == mod_type_phase || mod_type == mod_type_phase2 ) )
        		wm->cur_opacity = 64;
		    draw_line( x + op_size/2, y, x + op_size2/2, y, col, wm );
		    draw_line( x + op_size2/2, y, x + op_size2/2, y - op_size2/2, col, wm );
		    draw_line( x, y - op_size/2, x, y - op_size2/2, col, wm );
		    draw_line( x, y - op_size2/2, x + op_size2/2, y - op_size2/2, col, wm );
		}
        	x += op_size2;
            }
    	    wm->cur_opacity = 255;
            draw_string( ps_get_string( STR_PS_OUTPUT ), x - op_size/2 + wm->interelement_space, y - char_ysize/2, wm );
            wbd_draw( wm );
            wbd_unlock( wm );
            retval = 1;
            break;
        }
    }
    return retval;
}
#endif
static void gen_channel_reset( gen_channel* chan )
{
    chan->active = false;
    chan->id = EMPTY_ID;
}
static int get_vel2( int v, int sens )
{
    v *= 128;
    switch( sens )
    {
	case 0: v = 32768; break;
	case 1:
	    {
    		int v2 = 32768 - v;
    		v2 = ( v2 * v2 ) >> 15;
    		v2 = ( v2 * v2 ) >> 15;
    		v = 32768 - v2;
    		break;
    	    }
	case 3:
	    {
    		v = ( v * v ) >> 15;
    		v = ( v * v ) >> 15;
    		break;
	    }
	case 4:
	    {
    		v = ( v * v ) >> 15;
    		v = ( v * v ) >> 15;
    		v = ( v * v ) >> 15;
    		break;
	    }
    }
    return v;
}
static int get_vel( int v, int sens )
{
    if( sens < 0 )
    {
	sens = -sens;
	v = 256 - v;
    }
    int s1 = sens / 32;
    int s2 = sens & 31;
    int v1 = get_vel2( v, s1 );
    int v2 = get_vel2( v, s1 + 1 );
    return ( v1 * (32-s2) + v2 * s2 ) / 32;
}
static int get_vol( int v, int s )
{
    LIMIT_NUM( v, 0, 32768 );
    if( s < 0 )
    {
        v = 32768 - v;
        s = -s;
    }
    else
    {
    }
    int s1 = s / 16;
    int s2 = s & 15;
    int v1 = 32768;
    int v2 = 32768 - ( ( v * v ) >> 15 );
    if( s1 > 0 )
    {
        v1 = v2;
        for( int ss = 1; ss < s1; ss++ )
        {
            v1 = ( v1 * v1 ) >> 15;
        }
        v2 = ( v1 * v1 ) >> 15;
    }
    return ( v1 * (16-s2) + v2 * s2 ) / 16;
}
static void fm2_change_env_volume( MODULE_DATA* data, gen_channel* chan, int opn )
{
    fm2_operator* op = &chan->ops[ opn ];
    adsr_env* env = &op->env;
    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
    PS_CTYPE vol = pars[ FM2_PAR_VOLUME ];
    PS_CTYPE vol_scaling = pars[ FM2_PAR_VOL_SCALING ] - 128;
    PS_CTYPE sens = pars[ FM2_PAR_VEL_SENS ] - 128;
    if( sens )
    {
	int v = get_vel( chan->vel, sens );
	vol = vol * v / 32768;
    }
    if( vol_scaling )
    {
	int v = get_vol( PS_NOTE0_PITCH - chan->pitch, vol_scaling );
	vol = vol * v / 32768;
    }
    env->ctl_volume = vol * data->ctl_adsr_gain / 1000;
}
static void fm2_change_env_volume( MODULE_DATA* data, gen_channel* chan )
{
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
	fm2_change_env_volume( data, chan, opn );
    }
}
static void fm2_change_env_len( MODULE_DATA* data, gen_channel* chan, int opn )
{
    fm2_operator* op = &chan->ops[ opn ];
    adsr_env* env = &op->env;
    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
    PS_CTYPE alen = pars[ FM2_PAR_A ];
    PS_CTYPE dlen = pars[ FM2_PAR_D ];
    PS_CTYPE rlen = pars[ FM2_PAR_R ];
    PS_CTYPE env_scaling = pars[ FM2_PAR_ENV_SCALING ] - 128;
    if( env_scaling )
    {
	int v = get_vol( PS_NOTE0_PITCH - chan->pitch, env_scaling );
	alen = alen * v / 32768;
	dlen = dlen * v / 32768;
	rlen = rlen * v / 32768;
    }
    adsr_env_change_adr( env, alen, dlen, rlen );
}
static void fm2_change_env_len( MODULE_DATA* data, gen_channel* chan )
{
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
	fm2_change_env_len( data, chan, opn );
    }
}
static void fm2_change_env_pars( MODULE_DATA* data, gen_channel* chan, int opn )
{
    fm2_operator* op = &chan->ops[ opn ];
    adsr_env* env = &op->env;
    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
    fm2_change_env_volume( data, chan, opn );
    fm2_change_env_len( data, chan, opn );
    env->ctl_s = pars[ FM2_PAR_S ];
    env->ctl_acurve = pars[ FM2_PAR_ACURVE ];
    env->ctl_dcurve = pars[ FM2_PAR_DCURVE ];
    env->ctl_rcurve = pars[ FM2_PAR_RCURVE ];
    env->ctl_sustain = pars[ FM2_PAR_SUSTAIN ];
    env->ctl_sustain_pedal = pars[ FM2_PAR_SUSTAIN_PEDAL ];
}
static void gen_channel_on( MODULE_DATA* data, gen_channel* chan, uint32_t id, int vel )
{
    if( chan->active )
    {
	bool zero_attack = true;
	for( int opn = 0; opn < NUM_OPS; opn++ )
	{
	    fm2_operator* op = &chan->ops[ opn ];
	    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
	    if( pars[ FM2_PAR_A ] && pars[ FM2_PAR_MODE ] )
	    {
	        zero_attack = false;
	        break;
	    }
	}
        if( !zero_attack )
        {
	    chan->renderbuf_pars.anticlick = true;
        }
    }
    else
    {
	for( int opn = 0; opn < NUM_OPS; opn++ )
	{
	    fm2_operator* op = &chan->ops[ opn ];
	    adsr_env_reset( &op->env );
	}
    }
    chan->active = true;
    chan->on = true;
    chan->id = id;
    chan->vel = vel;
    chan->local_pan = 128;
    chan->renderbuf_pars.start = true;
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
	fm2_operator* op = &chan->ops[ opn ];
	PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
	op->ptr = (uint32_t)pars[ FM2_PAR_PHASE ] << (PHASE_BITS-15);
	op->feedback_val = 0;
	fm2_change_env_pars( data, chan, opn );
	adsr_env_start( &op->env );
    }
    if( data->no_active_channels )
    {
	for( int opn = 0; opn < NUM_OPS; opn++ )
	{
	    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
	    psmoother_reset( &data->feedback[ opn ], pars[ FM2_PAR_FEEDBACK ], 32768 );
	    psmoother_reset( &data->selfmod[ opn ], pars[ FM2_PAR_SELFMOD ], 32768 );
	}
    }
}
static void gen_channel_off( MODULE_DATA* data, gen_channel* chan )
{
    chan->on = false;
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
	fm2_operator* op = &chan->ops[ opn ];
	adsr_env_stop( &op->env );
    }
}
static void gen_channel_recalc_pitch( MODULE_DATA* data, gen_channel* chan )
{
    int pitch = chan->pitch;
    pitch /= 4;
    pitch -= 64 * 12 * 3;
    int freq;
    uint64_t delta;
    PSYNTH_GET_FREQ( g_linear_freq_tab, freq, pitch );
    PSYNTH_GET_DELTA64_HQ( data->srate, freq, delta );
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
	fm2_operator* op = &chan->ops[ opn ];
	PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
	uint64_t delta2 = delta;
	int const_pitch = pars[ FM2_PAR_CONST_PITCH ] - g_fm2_par_desc[ FM2_PAR_CONST_PITCH ].offset;
	if( const_pitch )
	{
	    PSYNTH_GET_FREQ( g_linear_freq_tab, freq, PS_NOTE0_PITCH/4 - const_pitch - 64 * 12 * 3 );
	    PSYNTH_GET_DELTA64_HQ( data->srate, freq, delta2 );
	}
	int fmul = pars[ FM2_PAR_FMUL ];
	op->delta = delta2 * fmul / 1000;
    }
}
static void gen_channel_set_offset( MODULE_DATA* data, gen_channel* chan, int offset ) 
{
    for( int opn = 0; opn < NUM_OPS; opn++ )
    {
        fm2_operator* op = &chan->ops[ opn ];
        op->ptr = offset << (PHASE_BITS-15);
    }
}
static void fm2_change_phase( MODULE_DATA* data, int opn, int phase ) 
{
    if( !data->no_active_channels )
    {
        for( int c = 0; c < data->ctl_channels; c++ )
        {
	    gen_channel* chan = &data->channels[ c ];
	    if( !chan->active ) continue;
	    fm2_operator* op = &chan->ops[ opn ];
	    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
	    int diff = ( phase - pars[ FM2_PAR_PHASE ] );
    	    op->ptr += diff << (PHASE_BITS-15);
	}
    }
}
static void fm2_change_adsr_smooth( MODULE_DATA* data ) 
{
    for( int c = 0; c < MAX_CHANNELS; c++ )
    {
        gen_channel* chan = &data->channels[ c ];
        for( int opn = 0; opn < NUM_OPS; opn++ )
        {
    	    fm2_operator* op = &chan->ops[ opn ];
	    adsr_env* env = &op->env;
	    env->ctl_smooth = data->ctl_adsr_smooth;
	}
    }
}
static void fm2_reinit_resampler( psynth_module* mod, int mod_num )
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
	if( !data->resamp )
	{
    	    data->resamp = psynth_resampler_new( pnet, mod_num, data->srate, pnet->sampling_freq, 0, 0 );
    	    for( int i = 0; i < MODULE_OUTPUTS; i++ ) psynth_resampler_input_buf( data->resamp, i );
	}
	else
	{
    	    if( data->resamp->in_smprate != data->srate )
    	    {
        	psynth_resampler_change( data->resamp, data->srate, pnet->sampling_freq, 0, 0 );
		for( int i = 0; i < MODULE_OUTPUTS; i++ ) psynth_resampler_input_buf( data->resamp, i );
    	    }
	}
    }
    if( srate_changed )
    {
	psmoother_init( &data->smoother_coefs, 100, data->srate );
	if( !data->no_active_channels )
	{
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		gen_channel* chan = &data->channels[ c ];
		if( !chan->active ) continue;
		gen_channel_recalc_pitch( data, chan );
		for( int opn = 0; opn < NUM_OPS; opn++ )
		{
		    fm2_operator* op = &chan->ops[ opn ];
		    adsr_env_change_srate( &op->env, data->srate );
		}
	    }
	}
	for( int c = 0; c < MAX_CHANNELS; c++ )
        {
    	    gen_channel* chan = &data->channels[ c ];
	    for( int opn = 0; opn < NUM_OPS; opn++ )
	    {
	        fm2_operator* op = &chan->ops[ opn ];
	        adsr_env_change_srate( &op->env, data->srate );
	    }
	}
    }
}
static void fm2_handle_changes( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    if( data->changed & CHANGED_IOCHANNELS )
    {
	if( data->ctl_stereo )
	{
	    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
	    {
		psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
	    }
	}
	else
	{
	    if( psynth_get_number_of_outputs( mod ) != 1 )
	    {
		psynth_set_number_of_outputs( 1, mod_num, pnet );
	    }
	}
    }
    if( data->changed & CHANGED_RESAMPLER )
    {
        fm2_reinit_resampler( mod, mod_num );
    }
    if( data->changed & CHANGED_PITCH_PARS )
    {
	if( !data->no_active_channels )
	{
    	    for( int c = 0; c < data->ctl_channels; c++ )
    	    {
		gen_channel* chan = &data->channels[ c ];
		if( !chan->active ) continue;
		gen_channel_recalc_pitch( data, chan );
	    }
	}
    }
    if( data->changed & CHANGED_ADSR_SMOOTH )
    {
	fm2_change_adsr_smooth( data );
    }
    if( data->changed & CHANGED_NOISE_FILTER )
    {
	data->noise_delta = 1 << 24;
	if( data->ctl_noise_filter < 32768 )
	{
	    float freq = (float)data->ctl_noise_filter / 32768.0F;
	    freq = freq * freq * 44100;
	    if( freq < data->srate )
	    {
		data->noise_delta = freq / data->srate * (float)( 1 << 24 ); 
		if( data->noise_delta == 0 ) data->noise_delta = 1;
	    }
	}
    }
    if( !data->no_active_channels )
    {
	if( data->changed & ( ( ( 1 << NUM_OPS ) - 1 ) << CHANGED_OP1_offset ) )
	{
	    uint32_t op_bits = ( data->changed >> CHANGED_OP1_offset ) & ( ( 1 << NUM_OPS ) - 1 );
	    data->changed &= ~( op_bits << CHANGED_OP1_VOL_offset ); 
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		gen_channel* chan = &data->channels[ c ];
		if( !chan->active ) continue;
		int bit = 1 << CHANGED_OP1_offset;
		for( int opn = 0; opn < NUM_OPS; opn++, bit <<= 1 )
		    if( data->changed & bit )
			fm2_change_env_pars( data, chan, opn );
	    }
	}
	if( data->changed & ( ( ( 1 << NUM_OPS ) - 1 ) << CHANGED_OP1_VOL_offset ) )
	{
    	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
	        gen_channel* chan = &data->channels[ c ];
	        if( !chan->active ) continue;
	        int bit = 1 << CHANGED_OP1_VOL_offset;
	        for( int opn = 0; opn < NUM_OPS; opn++, bit <<= 1 )
	    	    if( data->changed & bit )
		        fm2_change_env_volume( data, chan, opn );
	    }
	}
    }
    data->changed = 0;
}
inline PS_STYPE get_wave_sample( PS_STYPE* wave, uint32_t ptr )
{
    uint32_t ptr_h = ptr >> ( PHASE_BITS - WAVETABLE_BITS );
#ifdef PS_STYPE_FLOATINGPOINT
    float ptr_l = ( ( ptr << ( 16 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 65535 ) / 65536.0F;
    return wave[ ptr_h & WAVETABLE_MASK ] * ( 1.0F - ptr_l ) + wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l;
#else
    uint32_t ptr_l = ( ptr << ( 15 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 32767;
    return ( (PS_STYPE2)wave[ ptr_h & WAVETABLE_MASK ] * ( 32768 - ptr_l ) + (PS_STYPE2)wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l ) / 32768;
#endif
}
inline PS_STYPE get_wave_sample2( PS_STYPE* wave, uint32_t ptr, PS_STYPE2 selfmod ) 
{
    PS_STYPE2 v = get_wave_sample( wave, ptr );
#ifdef PS_STYPE_FLOATINGPOINT
    ptr += (int)( v * selfmod * ( 1 << ( PHASE_BITS + 3 ) ) );
#else
    ptr += ( v * selfmod / 32768 ) << ( PHASE_BITS - PS_STYPE_BITS + 3 );
#endif
    uint32_t ptr_h = ptr >> ( PHASE_BITS - WAVETABLE_BITS );
#ifdef PS_STYPE_FLOATINGPOINT
    float ptr_l = ( ( ptr << ( 16 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 65535 ) / 65536.0F;
    return wave[ ptr_h & WAVETABLE_MASK ] * ( 1.0F - ptr_l ) + wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l;
#else
    uint32_t ptr_l = ( ptr << ( 15 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 32767;
    return ( (PS_STYPE2)wave[ ptr_h & WAVETABLE_MASK ] * ( 32768 - ptr_l ) + (PS_STYPE2)wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l ) / 32768;
#endif
}
inline PS_STYPE get_wave_sample3( PS_STYPE* wave, uint32_t ptr, PS_STYPE2 selfmod, bool feedback_env, PS_STYPE2 env ) 
{
    PS_STYPE2 v = get_wave_sample( wave, ptr );
    if( feedback_env ) v = v * env / PS_STYPE_ONE;
#ifdef PS_STYPE_FLOATINGPOINT
    ptr += (int)( v * selfmod * ( 1 << ( PHASE_BITS + 3 ) ) );
#else
    ptr += ( v * selfmod / 32768 ) << ( PHASE_BITS - PS_STYPE_BITS + 3 );
#endif
    uint32_t ptr_h = ptr >> ( PHASE_BITS - WAVETABLE_BITS );
#ifdef PS_STYPE_FLOATINGPOINT
    float ptr_l = ( ( ptr << ( 16 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 65535 ) / 65536.0F;
    return wave[ ptr_h & WAVETABLE_MASK ] * ( 1.0F - ptr_l ) + wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l;
#else
    uint32_t ptr_l = ( ptr << ( 15 - ( PHASE_BITS - WAVETABLE_BITS ) ) ) & 32767;
    return ( (PS_STYPE2)wave[ ptr_h & WAVETABLE_MASK ] * ( 32768 - ptr_l ) + (PS_STYPE2)wave[ ( ptr_h + 1 ) & WAVETABLE_MASK ] * ptr_l ) / 32768;
#endif
}
inline void fm2_op_gen( MODULE_DATA* data, gen_channel* chan, int opn, int frames )
{
    fm2_operator* op = &chan->ops[ opn ];
    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
    int waveform = pars[ FM2_PAR_WAVEFORM ] - 1;
    int mod_type = pars[ FM2_PAR_MOD ];
    bool feedback_env = false; 
    if( mod_type == mod_type_phase2 )
    {
	mod_type = mod_type_phase;
	feedback_env = true;
	if( !data->env_buf_state )
	    memset( data->env_buf, 0, frames * sizeof( PS_STYPE ) );
    }
    PS_STYPE2 target_feedback = psmoother_target( pars[ FM2_PAR_FEEDBACK ], 32768 ); 
    PS_STYPE2 target_selfmod = psmoother_target( pars[ FM2_PAR_SELFMOD ], 32768 ); 
    bool feedback_change = psmoother_check( &data->feedback[ opn ], target_feedback );
    bool selfmod_change = psmoother_check( &data->selfmod[ opn ], target_selfmod );
    PS_STYPE* RESTRICT buf = &data->op_buf[ opn * OP_BUF_SIZE ];
    PS_STYPE* RESTRICT buf2 = data->tmp_buf;
    PS_STYPE* RESTRICT env_buf = data->env_buf;
    PS_STYPE* wave = data->custom_wave;
    if( waveform >= 0 )
	wave = &data->wavetable[ waveform * PSYNTH_BASE_WAVE_SIZE ];
    uint32_t ptr = op->ptr;
    uint32_t start_ptr = ptr;
    uint64_t delta = op->delta;
    bool buf_filled = data->op_buf_bits & ( 1 << opn );
    bool simple_mode = false;
    if( buf_filled == false && !selfmod_change )
    {
        if( mod_type >= mod_type_mul ) simple_mode = true;
	if( target_feedback == 0 && !feedback_change ) simple_mode = true;
    }
    if( simple_mode )
    {
	if( mod_type == mod_type_mul || mod_type == mod_type_and || mod_type == mod_type_minabs )
	{
	}
	else
	{
	    if( target_selfmod )
		for( int i = 0; i < frames; i++ )
		{
		    buf[ i ] = get_wave_sample3( wave, ptr, target_selfmod, feedback_env, env_buf[ i ] );
		    ptr += delta;
		}
	    else
		for( int i = 0; i < frames; i++ )
		{
		    buf[ i ] = get_wave_sample( wave, ptr );
		    ptr += delta;
		}
	    switch( mod_type )
	    {
		case mod_type_sub:
		    for( int i = 0; i < frames; i++ ) buf[ i ] = -buf[ i ];
		    break;
		case mod_type_min:
		    for( int i = 0; i < frames; i++ ) if( buf[ i ] > 0 ) buf[ i ] = 0;
		    break;
		case mod_type_max:
		    for( int i = 0; i < frames; i++ ) if( buf[ i ] < 0 ) buf[ i ] = 0;
		    break;
		case mod_type_maxabs:
		    break;
	    }
	    data->op_buf_bits |= ( 1 << opn );
	}
    }
    else
    {
	if( !buf_filled )
	{
	    memset( buf, 0, frames * sizeof( PS_STYPE ) );
	    data->op_buf_bits |= ( 1 << opn );
	}
	if( feedback_change || selfmod_change )
	{
	    for( int i = 0; i < frames; i++ )
	    {
		PS_STYPE2 feedback = psmoother_val( &data->smoother_coefs, &data->feedback[ opn ], target_feedback );
		PS_STYPE2 selfmod = psmoother_val( &data->smoother_coefs, &data->selfmod[ opn ], target_selfmod );
		PS_STYPE2 v = buf[ i ];
		if( mod_type < mod_type_mul ) v += PS_NORM_STYPE_MUL( op->feedback_val, feedback, 32768 );
		uint32_t ptr2 = ptr;
		if( mod_type == mod_type_phase )
		{
#ifdef PS_STYPE_FLOATINGPOINT
		    ptr2 = ptr + (int)( v * ( 1 << PHASE_BITS ) );
#else
		    ptr2 = ptr + v * (int64_t)( 1 << PHASE_BITS ) / PS_STYPE_ONE;
#endif
		}
		PS_STYPE2 v2 = get_wave_sample3( wave, ptr2, selfmod, feedback_env, env_buf[ i ] );
		op->feedback_val = v2;
		if( feedback_env ) op->feedback_val = (PS_STYPE2)op->feedback_val * env_buf[ i ] / PS_STYPE_ONE;
		switch( mod_type )
		{
		    case mod_type_phase:
			buf[ i ] = v2;
			ptr += delta;
			break;
		    case mod_type_freq:
			buf[ i ] = v2;
			ptr = ptr + (int)( (PS_STYPE3)delta * ( v + PS_STYPE_ONE ) / PS_STYPE_ONE );
			break;
		    case mod_type_mul:
			buf[ i ] = (PS_STYPE2)buf[ i ] * v2 / PS_STYPE_ONE;
			ptr += delta;
			break;
		    case mod_type_add:
			buf[ i ] += v2;
			ptr += delta;
			break;
		    case mod_type_sub:
			buf[ i ] -= v2;
			ptr += delta;
			break;
		    case mod_type_min:
                        if( v2 < buf[ i ] ) buf[ i ] = v2;
			ptr += delta;
			break;
		    case mod_type_max:
                        if( v2 > buf[ i ] ) buf[ i ] = v2;
			ptr += delta;
			break;
		    case mod_type_and:
			{
                            int iv1, iv2;
                            PS_STYPE_TO_INT16( iv1, buf[ i ] );
                            PS_STYPE_TO_INT16( iv2, v2 );
                            iv1 = iv1 & iv2;
                            PS_INT16_TO_STYPE( buf[ i ], iv1 );
                        }
			ptr += delta;
			break;
		    case mod_type_xor:
			{
                            int iv1, iv2;
                            PS_STYPE_TO_INT16( iv1, buf[ i ] );
                            PS_STYPE_TO_INT16( iv2, v2 );
                            iv1 = iv1 ^ iv2;
                            PS_INT16_TO_STYPE( buf[ i ], iv1 );
                        }
			ptr += delta;
			break;
		    case mod_type_minabs:
                        if( PS_STYPE_ABS(v2) < PS_STYPE_ABS(buf[ i ]) ) buf[ i ] = v2;
			ptr += delta;
			break;
		    case mod_type_maxabs:
                        if( PS_STYPE_ABS(v2) > PS_STYPE_ABS(buf[ i ]) ) buf[ i ] = v2;
			ptr += delta;
			break;
		}
	    }
	}
	else
	{
	switch( mod_type )
	{
	    case mod_type_phase:
		if( target_selfmod )
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = buf[ i ];
			v += PS_NORM_STYPE_MUL( op->feedback_val, target_feedback, 32768 );
#ifdef PS_STYPE_FLOATINGPOINT
			uint32_t ptr2 = ptr + (int)( v * ( 1 << PHASE_BITS ) );
#else
			uint32_t ptr2 = ptr + v * (int64_t)( 1 << PHASE_BITS ) / PS_STYPE_ONE;
#endif
			v = get_wave_sample3( wave, ptr2, target_selfmod, feedback_env, env_buf[ i ] );
			buf[ i ] = v;
			if( feedback_env ) v = v * env_buf[ i ] / PS_STYPE_ONE;
			op->feedback_val = v;
			ptr += delta;
		    }
		else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = buf[ i ];
			v += PS_NORM_STYPE_MUL( op->feedback_val, target_feedback, 32768 );
#ifdef PS_STYPE_FLOATINGPOINT
			uint32_t ptr2 = ptr + (int)( v * ( 1 << PHASE_BITS ) );
#else
			uint32_t ptr2 = ptr + v * (int64_t)( 1 << PHASE_BITS ) / PS_STYPE_ONE;
#endif
			v = get_wave_sample( wave, ptr2 );
			buf[ i ] = v;
			if( feedback_env ) v = v * env_buf[ i ] / PS_STYPE_ONE;
			op->feedback_val = v;
			ptr += delta;
		    }
		break;
	    case mod_type_freq:
		if( target_selfmod )
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = buf[ i ];
			v += PS_NORM_STYPE_MUL( op->feedback_val, target_feedback, 32768 );
			PS_STYPE2 v2 = get_wave_sample2( wave, ptr, target_selfmod );
			op->feedback_val = v2;
			buf[ i ] = v2;
			ptr = ptr + (int)( (PS_STYPE3)delta * ( v + PS_STYPE_ONE ) / PS_STYPE_ONE );
		    }
		else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = buf[ i ];
			v += PS_NORM_STYPE_MUL( op->feedback_val, target_feedback, 32768 );
			PS_STYPE2 v2 = get_wave_sample( wave, ptr );
			op->feedback_val = v2;
			buf[ i ] = v2;
			ptr = ptr + (int)( (PS_STYPE3)delta * ( v + PS_STYPE_ONE ) / PS_STYPE_ONE );
		    }
		break;
	    case mod_type_mul:
		if( target_selfmod )
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample2( wave, ptr, target_selfmod );
	    		v = (PS_STYPE2)buf[ i ] * v / PS_STYPE_ONE;
			buf[ i ] = v;
			ptr += delta;
		    }
		else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample( wave, ptr );
	    		v = (PS_STYPE2)buf[ i ] * v / PS_STYPE_ONE;
			buf[ i ] = v;
			ptr += delta;
		    }
	        break;
	    case mod_type_add:
	        if( target_selfmod )
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample2( wave, ptr, target_selfmod );
			buf[ i ] += v;
			ptr += delta;
		    }
		else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample( wave, ptr );
			buf[ i ] += v;
			ptr += delta;
		    }
	        break;
	    case mod_type_sub:
	    case mod_type_min:
	    case mod_type_max:
	    case mod_type_and:
	    case mod_type_xor:
	    case mod_type_minabs:
	    case mod_type_maxabs:
	        if( target_selfmod )
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample2( wave, ptr, target_selfmod );
			buf2[ i ] = v;
			ptr += delta;
		    }
		else
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v = get_wave_sample( wave, ptr );
			buf2[ i ] = v;
			ptr += delta;
		    }
		switch( mod_type )
		{
		    case mod_type_sub:
			for( int i = 0; i < frames; i++ ) buf[ i ] -= buf2[ i ];
			break;
		    case mod_type_min:
			for( int i = 0; i < frames; i++ )
			{
                    	    if( buf2[ i ] < buf[ i ] ) buf[ i ] = buf2[ i ];
			}
			break;
		    case mod_type_max:
			for( int i = 0; i < frames; i++ )
			{
                    	    if( buf2[ i ] > buf[ i ] ) buf[ i ] = buf2[ i ];
			}
			break;
		    case mod_type_and:
			for( int i = 0; i < frames; i++ )
			{
                            int iv1, iv2;
                            PS_STYPE_TO_INT16( iv1, buf[ i ] );
                            PS_STYPE_TO_INT16( iv2, buf2[ i ] );
                            iv1 = iv1 & iv2;
                            PS_INT16_TO_STYPE( buf[ i ], iv1 );
                        }
			break;
		    case mod_type_xor:
			for( int i = 0; i < frames; i++ )
			{
                            int iv1, iv2;
                            PS_STYPE_TO_INT16( iv1, buf[ i ] );
                            PS_STYPE_TO_INT16( iv2, buf2[ i ] );
                            iv1 = iv1 ^ iv2;
                            PS_INT16_TO_STYPE( buf[ i ], iv1 );
                        }
			break;
		    case mod_type_minabs:
			for( int i = 0; i < frames; i++ )
			{
                    	    if( PS_STYPE_ABS(buf2[ i ]) < PS_STYPE_ABS(buf[ i ]) ) buf[ i ] = buf2[ i ];
			}
			break;
		    case mod_type_maxabs:
			for( int i = 0; i < frames; i++ )
			{
                    	    if( PS_STYPE_ABS(buf2[ i ]) > PS_STYPE_ABS(buf[ i ]) ) buf[ i ] = buf2[ i ];
			}
			break;
		}
		break;
	}
	}
    }
    op->ptr = ptr;
    int noise = pars[ FM2_PAR_NOISE ];
    if( noise )
    {
	if( ( data->op_buf_bits & ( 1 << opn ) ) == 0 )
	{
	    memset( buf, 0, frames * sizeof( PS_STYPE ) );
	}
	PS_STYPE2 nv = PS_NORM_STYPE( noise, 32768 );
	if( data->noise_delta != 1 << 24 )
	{
	    for( int i = 0; i < frames; i++ )
	    {
		int ptr = op->noise_ptr >> 24;
		int y0 = op->noise_v[ ptr & 3 ];
		int y1 = op->noise_v[ ( ptr + 1 ) & 3 ];
		int y2 = op->noise_v[ ( ptr + 2 ) & 3 ];
		int y3 = op->noise_v[ ( ptr + 3 ) & 3 ];
		int rnd = catmull_rom_spline_interp_int16( y0, y1, y2, y3, ( op->noise_ptr >> ( 24 - 15 ) ) & 32767 );
    		PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
    		buf[ i ] += PS_NORM_STYPE_MUL( v, nv, 32768 );
    		op->noise_ptr += data->noise_delta;
		int new_ptr = op->noise_ptr >> 24;
		if( new_ptr != ptr )
		{
		    op->noise_v[ ptr & 3 ] = psynth_rand2( &chan->noise_seed );
		}
    	    }
    	}
    	else
    	{
	    for( int i = 0; i < frames; i++ )
	    {
		int rnd = psynth_rand2( &chan->noise_seed );
    		PS_STYPE2 v; PS_INT16_TO_STYPE( v, rnd );
    		buf[ i ] += PS_NORM_STYPE_MUL( v, nv, 32768 );
    	    }
    	}
	data->op_buf_bits |= ( 1 << opn );
    }
}
inline void fm2_op_gen_env( MODULE_DATA* data, gen_channel* chan, int opn, int frames )
{
    fm2_operator* op = &chan->ops[ opn ];
    data->env_buf_frames = 0; 
    data->env_buf_state = adsr_env_run( &op->env, data->env_buf, frames, &data->env_buf_frames );
}
inline void fm2_op_apply_env( MODULE_DATA* data, gen_channel* chan, int opn, int frames )
{
    fm2_operator* op = &chan->ops[ opn ];
    bool buf1_filled = data->op_buf_bits & ( 1 << opn );
    PS_STYPE* RESTRICT buf1 = &data->op_buf[ opn * OP_BUF_SIZE ];
    PS_STYPE* RESTRICT buf2 = data->env_buf;
    if( data->env_buf_state )
    {
	if( buf1_filled )
	{
	    for( int i = 0; i < frames; i++ )
	    {
		buf1[ i ] = (PS_STYPE2)buf1[ i ] * (PS_STYPE2)buf2[ i ] / PS_STYPE_ONE;
	    }
	}
    }
    else
    {
	data->op_buf_bits &= ~( 1 << opn );
    }
}
inline void fm2_op_copy( MODULE_DATA* data, gen_channel* chan, int opn, int frames, PS_STYPE* main_out )
{
    fm2_operator* op = &chan->ops[ opn ];
    PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
    int mode = pars[ FM2_PAR_MODE ]; 
    PS_STYPE* buf = &data->op_buf[ opn * OP_BUF_SIZE ];
    int bit = 1;
    for( int b = 0; b < NUM_OPS - opn; b++, bit <<= 1 )
    {
        int out_opn = NUM_OPS - b;
        if( mode & bit )
        {
	    if( data->op_buf_bits & ( 1 << opn ) )
	    {
    		PS_STYPE* dest = main_out;
    		if( out_opn < NUM_OPS ) dest = &data->op_buf[ out_opn * OP_BUF_SIZE ];
		if( data->op_buf_bits & ( 1 << out_opn ) )
    		{
    		    for( int i = 0; i < frames; i++ ) dest[ i ] += buf[ i ];
    		}
    		else
    		{
    		    memmove( dest, buf, frames * sizeof( PS_STYPE ) );
    		    data->op_buf_bits |= ( 1 << out_opn );
    		}
    	    }
        }
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
	    retval = (PS_RETTYPE)"FMX";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)
"5-операторный синтезатор на основе алгоритма частотной модуляции (FM).\n"
"Локальные контроллеры: Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)
"5-operator Frequency Modulation (FM) Synthesizer.\n"
"Local controllers: Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#009BFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
#define OP_CTL 9
	    psynth_resize_ctls_storage( mod_num, OP_CTL + FM2_OP_CTLS + 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 32768, 32768 / 2, 0, &data->ctl_volume, -1, 0, pnet );
            psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
            psynth_set_ctl_show_offset( mod_num, 1, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_RATE ), "8000;11025;16000;22050;32000;44100;native", 0, 6, 5, 1, &data->ctl_srate, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 4, 1, &data->ctl_channels, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_CHANNELS ), ps_get_string( STR_PS_MONO_STEREO ), 0, 1, 0, 1, &data->ctl_stereo, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INPUT_TO_OP ), "", 0, NUM_OPS, 0, 1, &data->ctl_send, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INPUT_TO_CUSTOMWAVE ), ps_get_string( STR_PS_CAPTURE_MODES ), 0, 2, 0, 1, &data->ctl_capture, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_SMOOTH_TRANSITIONS2 ), ps_get_string( STR_PS_ADSR_SMOOTH_TRANSITIONS ), 0, ADSR_SMOOTH_MODES-1, 2, 1, &data->ctl_adsr_smooth, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_NOISE_FILTER ), "", 0, 32768, 32768, 0, &data->ctl_noise_filter, -1, 3, pnet );
	    for( int p = 0; p < FM2_PARS; p++ )
	    {
		const fm2_op_par_desc* desc = &g_fm2_par_desc[ p ];
		if( !desc->name ) break;
		char ts[ 256 ];
		const char* ctl_name = "";
		const char* ctl_label = "";
		if( desc->name >= 0 )
		    ctl_name = ps_get_string( (ps_string)desc->name );
		if( desc->label >= 0 )
		    ctl_label = ps_get_string( (ps_string)desc->label );
		else
		{
		    if( desc->label < -1 )
		    {
			ctl_label = g_strings[ -desc->label - 2 ];
		    }
		}
		int group = 4;
		if( p >= FM2_PAR_WAVEFORM ) group = 5;
		for( int opn = 0; opn < NUM_OPS; opn++ )
		{
		    sprintf( ts, "%d %s", opn + 1, ctl_name );
		    char* new_name = smem_strdup( ts );
		    data->par_names[ opn * FM2_PARS + p ] = new_name;
		    PS_CTYPE def = desc->def;
		    PS_CTYPE max = desc->max;
		    if( p == FM2_PAR_MODE )
		    {
			max = ( ( 1 << NUM_OPS ) - 1 ) >> opn;
			def = 0;
			if( opn == NUM_OPS - 2 ) def = 2;
			if( opn == NUM_OPS - 1 )
			{
			    def = 1;
			}
			data->pars[ opn * FM2_PARS + p ] = def;
			if( opn == NUM_OPS - 1 ) break;
		    }
		    if( p == FM2_PAR_SUSTAIN )
		    {
			if( opn == NUM_OPS - 1 )
			{
			    def = 1;
			}
		    }
		    bool show_offset = false;
		    int ctl = psynth_register_ctl( mod_num, new_name, ctl_label, desc->min, max, def, desc->type, &data->pars[ opn * FM2_PARS + p ], desc->normal, group, pnet );
		    if( desc->flags )
			psynth_set_ctl_flags( mod_num, ctl, desc->flags, pnet );
		    if( desc->offset )
			psynth_set_ctl_show_offset( mod_num, ctl, -desc->offset, pnet );
		}
	    }
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ENV_GAIN ), "1/1000", 0, 1000 * 8, 1000, 0, &data->ctl_adsr_gain, 1000, 3, pnet );
	    data->no_active_channels = true;
	    data->changed = 0xFFFFFFFF ^ CHANGED_ADSR_SMOOTH; 
	    data->wavetable = psynth_get_base_wavetable();
	    data->cap_buf_ptr = pnet->max_buf_size;
	    psynth_get_temp_buf( mod_num, pnet, 0 ); 
	    g_rates[ NUM_RATES - 1 ] = pnet->sampling_freq;
	    if( pnet->sampling_freq < 44100 ) 
            {
                slog( "FM2: WRONG SAMPLING RATE!\n" );
            }
#ifdef SUNVOX_GUI
            {
                data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
                    mod->visual = new_window( "FM2 GUI", 0, 0, 10, 10, wm->color1, 0, pnet, MODULE_VHANDLER, wm );
                    MODULE_VDATA* v_data = (MODULE_VDATA*)mod->visual->data;
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
	    fm2_handle_changes( mod, mod_num );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		gen_channel* chan = &data->channels[ c ];
		gen_channel_reset( chan );
		chan->noise_seed = stime_ms() + pseudo_random() + mod_num * 3 + c * 7;
		for( int opn = 0; opn < NUM_OPS; opn++ )
		{
		    fm2_operator* op = &chan->ops[ opn ];
		    adsr_env_init( &op->env, true, data->srate );
		    for( int r = 0; r < 4; r++ ) op->noise_v[ r ] = psynth_rand2( &chan->noise_seed );
		}
	    }
	    fm2_change_adsr_smooth( data );
	    {
		float* wave = (float*)psynth_get_chunk_data( mod_num, 0, pnet );
                if( wave )
                {
            	    for( int i = 0; i < WAVETABLE_SIZE; i++ )
            	    {
#ifdef PS_STYPE_FLOATINGPOINT
            		data->custom_wave[ i ] = wave[ i ];
#else
            		data->custom_wave[ i ] = wave[ i ] * (float)PS_STYPE_ONE;
#endif
            	    }
                }
            }
            if( data->ctl_send )
            {
	        if( !data->in_buf )
	    	    data->in_buf = (PS_STYPE*)smem_new( sizeof( PS_STYPE ) * ( pnet->max_buf_size * g_rates[ NUM_RATES - 1 ] / pnet->sampling_freq + 8 ) );
	    }
            if( data->ctl_capture )
            {
	        if( !data->cap_buf )
	    	    data->cap_buf = (PS_STYPE*)smem_new( sizeof( PS_STYPE ) * pnet->max_buf_size );
	    }
            retval = 1;
            break;
	case PS_CMD_BEFORE_SAVE:
	    if( data->custom_wave_changed )
	    {
		bool custom_wave_empty = true;
            	for( int i = 0; i < WAVETABLE_SIZE; i++ )
            	{
            	    if( data->custom_wave[ i ] )
            	    {
            	        custom_wave_empty = false;
            	        break;
            	    }
            	}
            	if( custom_wave_empty )
            	{
            	    psynth_remove_chunk( mod_num, 0, pnet ); 
            	}
            	else
            	{
		    float* wave = (float*)psynth_get_chunk_data_autocreate( mod_num, 0, sizeof( float ) * WAVETABLE_SIZE, NULL, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
		    if( wave )
		    {
            		for( int i = 0; i < WAVETABLE_SIZE; i++ )
            		{
#ifdef PS_STYPE_FLOATINGPOINT
            		    wave[ i ] = data->custom_wave[ i ];
#else
            		    wave[ i ] = (float)data->custom_wave[ i ] / (float)PS_STYPE_ONE;
#endif
            		}
            	    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		if( data->changed ) fm2_handle_changes( mod, mod_num );
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		int outputs_num = psynth_get_number_of_outputs( mod );
		if( data->ctl_capture )
		{
		    if( data->ctl_capture == 2 && data->cap_buf_ptr >= pnet->max_buf_size )
			data->cap_buf_ptr = 0;
		    if( data->cap_buf_ptr < pnet->max_buf_size )
		    {
			PS_STYPE* in = inputs[ 0 ] + offset;
			for( int i = 0; i < frames; i++ )
			{
			    data->cap_buf[ data->cap_buf_ptr ] = in[ i ];
			    data->cap_buf_ptr++;
			    if( data->cap_buf_ptr >= pnet->max_buf_size )
			    {
				uint src_p = 0;
				uint src_step = pnet->max_buf_size * 32768 / WAVETABLE_SIZE;
				src_step /= 2; 
				for( int a = 0; a < WAVETABLE_SIZE; a++ )
				{
				    uint src_p2 = src_p / 32768;
				    PS_STYPE2 v0 = data->cap_buf[ src_p2 ];
				    PS_STYPE2 v1 = v0;
				    if( (int)src_p2 + 1 < pnet->max_buf_size ) v1 = data->cap_buf[ src_p2 + 1 ];
				    int pp = src_p & 32767;
				    PS_STYPE2 v2 = ( v0 * ( (PS_STYPE2)32768 - pp ) + v1 * pp ) / (PS_STYPE2)32768;
				    data->custom_wave[ a ] = v2 * (PS_STYPE2)a / (PS_STYPE2)WAVETABLE_SIZE;
				    src_p += src_step;
				}
				for( int a = 0; a < WAVETABLE_SIZE; a++ )
				{
				    uint src_p2 = src_p / 32768;
				    PS_STYPE2 v0 = data->cap_buf[ src_p2 ];
				    PS_STYPE2 v1 = v0;
				    if( (int)src_p2 + 1 < pnet->max_buf_size ) v1 = data->cap_buf[ src_p2 + 1 ];
				    int pp = src_p & 32767;
				    PS_STYPE2 v2 = ( v0 * ( (PS_STYPE2)32768 - pp ) + v1 * pp ) / (PS_STYPE2)32768;
				    data->custom_wave[ a ] += v2 * (PS_STYPE2)(WAVETABLE_SIZE-a) / (PS_STYPE2)WAVETABLE_SIZE;
				    src_p += src_step;
				}
				data->custom_wave_changed = true;
				if( data->ctl_capture == 1 ) break; 
				data->cap_buf_ptr = 0;
			    }
			}
		    }
		}
		PS_STYPE** resamp_outputs = outputs;
                int resamp_offset = offset;
		int resamp_frames = frames;
		PS_STYPE* resamp_bufs[ MODULE_OUTPUTS ];
		PS_STYPE* resamp_outputs2[ MODULE_OUTPUTS ];
                if( data->use_resampler )
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
#ifdef SUNVOX_GUI
		uint32_t op_buf_bits_UI = 0;
#endif
                if( resamp_frames && !data->no_active_channels )
                {
            	    if( data->ctl_send )
            	    {
			if( mod->in_empty[ 0 ] >= offset + frames )
			    memset( data->in_buf, 0, sizeof( PS_STYPE ) * resamp_frames );
			else
			{
			    PS_STYPE* in = inputs[ 0 ] + offset;
			    uint in_ptr = 0;
			    uint in_step = frames * 32768 / resamp_frames;
            	    	    for( int i = 0; i < resamp_frames; i++ )
            		    {
            			uint hh = in_ptr / 32768;
#ifdef PS_STYPE_FLOATINGPOINT
            			float ll = (float)( in_ptr & 32767 ) / 32768.0f;
            			data->in_buf[ i ] = in[ hh ] * ( 1.0f - ll ) + in[ hh + 1 ] * ll;
#else
            			uint ll = in_ptr & 32767;
            			data->in_buf[ i ] = ( (PS_STYPE2)in[ hh ] * ( 32768 - ll ) + (PS_STYPE2)in[ hh + 1 ] * ll ) / 32768;
#endif
            			in_ptr += in_step;
            		    }
            		}
            	    }
		    psmoother tmp_feedback[ NUM_OPS ];
		    psmoother tmp_selfmod[ NUM_OPS ];
		    smem_copy( tmp_feedback, data->feedback, sizeof( psmoother ) * NUM_OPS );
		    smem_copy( tmp_selfmod, data->selfmod, sizeof( psmoother ) * NUM_OPS );
		    data->no_active_channels = true;
		    for( int c = 0; c < data->ctl_channels; c++ )
		    {
			gen_channel* chan = &data->channels[ c ];
			if( !chan->active ) continue;
			data->no_active_channels = false;
			PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, resamp_outputs, outputs_num, resamp_offset );
			smem_copy( data->feedback, tmp_feedback, sizeof( psmoother ) * NUM_OPS );
			smem_copy( data->selfmod, tmp_selfmod, sizeof( psmoother ) * NUM_OPS );
			int i = 0;
			while( 1 ) 
			{
			    int size = resamp_frames - i;
			    if( size > OP_BUF_SIZE ) size = OP_BUF_SIZE;
			    data->op_buf_bits = 0;
			    if( data->ctl_send )
			    {
				data->op_buf_bits = 1 << ( data->ctl_send - 1 );
				PS_STYPE* op_buf = &data->op_buf[ ( data->ctl_send - 1 ) * OP_BUF_SIZE ];
				for( int a = 0; a < OP_BUF_SIZE; a++ )
				    op_buf[ a ] = data->in_buf[ i + a ];
			    }
			    int main_frames = 0; 
			    for( int opn = 0; opn < NUM_OPS; opn++ )
			    {
				PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
				int mode = pars[ FM2_PAR_MODE ];
				if( mode == 0 ) continue;
				fm2_op_gen_env( data, chan, opn, size );
				fm2_op_gen( data, chan, opn, size ); 
				fm2_op_apply_env( data, chan, opn, size );
				fm2_op_copy( data, chan, opn, size, &render_buf[ i ] );
				if( mode & 1 )
				{
				    if( data->env_buf_frames > main_frames )
					main_frames = data->env_buf_frames;
				}
			    }
			    if( ( data->op_buf_bits & ( 1 << NUM_OPS ) ) == 0 )
			    {
				memset( &render_buf[ i ], 0, sizeof( PS_STYPE ) * size );
			    }
			    if( main_frames < size )
			    {
				chan->active = false;
				chan->id = EMPTY_ID;
				if( main_frames == 0 )
				{
				    for( int opn = 0; opn < NUM_OPS; opn++ )
				    {
					PS_CTYPE* pars = &data->pars[ opn * FM2_PARS ];
					PS_STYPE2 target_feedback = psmoother_target( pars[ FM2_PAR_FEEDBACK ], 32768 ); 
					PS_STYPE2 target_selfmod = psmoother_target( pars[ FM2_PAR_SELFMOD ], 32768 ); 
					if( psmoother_check( &data->feedback[ opn ], target_feedback ) )
					    for( int s = i + size; s < resamp_frames; s++ )
						psmoother_val( &data->smoother_coefs, &data->feedback[ opn ], target_feedback );
					if( psmoother_check( &data->selfmod[ opn ], target_selfmod ) )
					    for( int s = i + size; s < resamp_frames; s++ )
						psmoother_val( &data->smoother_coefs, &data->selfmod[ opn ], target_selfmod );
				    }
				    break;
				}
			    }
#ifdef SUNVOX_GUI
			    op_buf_bits_UI |= data->op_buf_bits;
#endif
			    i += size;
			    if( i >= resamp_frames ) break;
			}
			retval = psynth_renderbuf2output(
    			    retval,
		    	    resamp_outputs, outputs_num, resamp_offset, resamp_frames,
			    render_buf, NULL, i,
		            data->ctl_volume, 
		            ( data->ctl_pan + ( chan->local_pan - 128 ) ) * 256, 
		            &chan->renderbuf_pars, &data->smoother_coefs,
		            data->srate
		        );
		    } 
		} 
#ifdef SUNVOX_GUI
		if( op_buf_bits_UI != data->op_buf_bits_UI2 )
		{
		    data->op_buf_bits_UI = op_buf_bits_UI;
		    mod->draw_request++;
		}
#endif
		if( data->use_resampler ) retval = psynth_resampler_end( data->resamp, retval, resamp_bufs, outputs, outputs_num, offset );
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c = 0;
		if( !data->no_active_channels )
		{
		    for( c = 0; c < MAX_CHANNELS; c++ )
		    {
			gen_channel* chan = &data->channels[ c ];
			if( chan->id == event->id )
			{
			    gen_channel_off( data, chan );
			    chan->id = EMPTY_ID;
			    break;
			}
		    }
		    for( c = 0; c < data->ctl_channels; c++ )
		    {
			gen_channel* chan = &data->channels[ data->search_ptr ];
			if( !chan->active ) break;
			data->search_ptr++; if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		    if( c == data->ctl_channels )
		    {
			data->search_ptr++; if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		}
		else
		{
		    data->search_ptr = 0;
		}
		c = data->search_ptr;
                gen_channel* chan = &data->channels[ c ];
		chan->pitch = event->note.pitch;
                gen_channel_recalc_pitch( data, chan );
                gen_channel_on( data, chan, event->id, event->note.velocity );
		data->no_active_channels = false;
		retval = 1;
	    }
	    break;
    	case PS_CMD_SET_FREQ:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
                gen_channel* chan = &data->channels[ c ];
		if( chan->id == event->id )
		{
		    chan->pitch = event->note.pitch;
            	    chan->vel = event->note.velocity;
            	    gen_channel_recalc_pitch( data, chan );
            	    fm2_change_env_volume( data, chan );
            	    fm2_change_env_len( data, chan );
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_SAMPLE_OFFSET:
        case PS_CMD_SET_SAMPLE_OFFSET2:
            if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
            {
                gen_channel* chan = &data->channels[ c ];
                if( chan->id == event->id )
                {
                    gen_channel_set_offset( data, chan, event->sample_offset.sample_offset );
                }
            }
            retval = 1;
            break;
    	case PS_CMD_SET_VELOCITY:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
                gen_channel* chan = &data->channels[ c ];
		if( chan->id == event->id )
		{
            	    chan->vel = event->note.velocity;
            	    fm2_change_env_volume( data, chan );
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
                gen_channel* chan = &data->channels[ c ];
		if( chan->id == event->id )
		{
		    gen_channel_off( data, chan );
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
                gen_channel* chan = &data->channels[ c ];
		gen_channel_off( data, chan );
		chan->id = EMPTY_ID; 
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ ) gen_channel_reset( &data->channels[ c ] );
	    data->no_active_channels = true;
            psynth_resampler_reset( data->resamp );
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	    if( !data->no_active_channels )
	    {
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
            	if( retval ) break;
	    }
        case PS_CMD_SET_GLOBAL_CONTROLLER:
    	    if( event->controller.ctl_num >= OP_CTL && event->controller.ctl_num < OP_CTL + FM2_OP_CTLS )
    	    {
            	int c = event->controller.ctl_num - OP_CTL;
            	int opn = c % NUM_OPS;
            	int par = c / NUM_OPS;
                if( par <= FM2_PAR_SUSTAIN_PEDAL )
            	    data->changed |= CHANGED_OP1 << opn;
            	else
            	{
            	    switch( par )
            	    {
            		case FM2_PAR_ENV_SCALING:
            		    data->changed |= CHANGED_OP1 << opn;
            		    break;
            		case FM2_PAR_VOL_SCALING:
            		case FM2_PAR_VEL_SENS:
            		    data->changed |= CHANGED_OP1_VOL << opn;
            		    break;
            		case FM2_PAR_PHASE:
            		    fm2_change_phase( data, opn, event->controller.ctl_val );
            		    break;
            		case FM2_PAR_FMUL:
            		case FM2_PAR_CONST_PITCH:
            		    data->changed |= CHANGED_PITCH_PARS;
			    break;
            	    }
                }
    	    }
    	    else
    	    {
        	switch( event->controller.ctl_num )
        	{
            	    case 2: 
                	data->changed |= CHANGED_RESAMPLER | CHANGED_NOISE_FILTER;
                	break;
            	    case 4: 
            		data->changed |= CHANGED_IOCHANNELS;
                	break;
            	    case 5: 
            		if( event->controller.ctl_val )
            		{
	    		    if( !data->in_buf )
	    			data->in_buf = (PS_STYPE*)smem_new( sizeof( PS_STYPE ) * ( pnet->max_buf_size * g_rates[ NUM_RATES - 1 ] / pnet->sampling_freq + 8 ) );
            		}
            		break;
            	    case 6: 
            		if( event->controller.ctl_val )
            		{
			    if( data->cap_buf_ptr == pnet->max_buf_size )
				data->cap_buf_ptr = 0;
			    if( !data->cap_buf )
				data->cap_buf = (PS_STYPE*)smem_new( sizeof( PS_STYPE ) * pnet->max_buf_size );
			}
			else
			{
			    data->cap_buf_ptr = pnet->max_buf_size;
			}
            		break;
            	    case 7: 
                	data->changed |= CHANGED_ADSR_SMOOTH;
            		break;
            	    case 8: 
                	data->changed |= CHANGED_NOISE_FILTER;
            		break;
            	    case OP_CTL + FM2_OP_CTLS: 
            		data->changed |= ( ( ( 1 << NUM_OPS ) - 1 ) << CHANGED_OP1_VOL_offset );
            		break;
                }
            }
            break;
	case PS_CMD_CLOSE:
	    psynth_resampler_remove( data->resamp );
#ifdef SUNVOX_GUI
            if( mod->visual && data->wm )
            {
                remove_window( mod->visual, data->wm );
                mod->visual = NULL;
            }
#endif
	    for( int p = 0; p < NUM_OPS * FM2_PARS; p++ )
		smem_free( data->par_names[ p ] );
	    smem_free( data->in_buf );
	    smem_free( data->cap_buf );
	    retval = 1;
	    break;
	case PS_CMD_READ_CURVE:
        case PS_CMD_WRITE_CURVE:
            {
                PS_STYPE* curve = NULL;
                int len = 0;
                int reqlen = event->offset;
                switch( event->id )
                {
                    case 0: curve = data->custom_wave; len = WAVETABLE_SIZE; break;
                    default: break;
                }
                if( curve )
                {
                    if( event->curve.data )
                    {
                        if( reqlen == 0 ) reqlen = len;
                        if( reqlen > len ) reqlen = len;
                        if( event->command == PS_CMD_READ_CURVE )
                            for( int i = 0; i < reqlen; i++ ) { event->curve.data[ i ] = (float)curve[ i ] / (float)PS_STYPE_ONE; }
                        else
                        {
                            for( int i = 0; i < reqlen; i++ ) { float v = event->curve.data[ i ] * (float)PS_STYPE_ONE; curve[ i ] = v; }
                            if( event->id == 0 ) data->custom_wave_changed = true;
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
