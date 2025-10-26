#pragma once

//
// Biquad Filter
//

enum biquad_filter_ftype
{
    BIQUAD_FILTER_LPF = 0,
    BIQUAD_FILTER_HPF,
    BIQUAD_FILTER_BPF_CSG, //constant skirt gain, peak gain = Q
    BIQUAD_FILTER_BPF_CPG, //constant 0 dB peak gain
    BIQUAD_FILTER_NOTCH,
    BIQUAD_FILTER_APF,
    BIQUAD_FILTER_PEAKING,
    BIQUAD_FILTER_LOWSHELF,
    BIQUAD_FILTER_HIGHSHELF,
    BIQUAD_FILTER_1POLE_LPF, //1.2 - 1.5 times faster
    BIQUAD_FILTER_1POLE_HPF, //...
    BIQUAD_FILTER_TYPES
};

enum biquad_filter_qtype
{
    BIQUAD_FILTER_Q_IS_Q = 0,
    BIQUAD_FILTER_Q_IS_BANDWIDTH, //Q is bandwidth in octaves
    BIQUAD_FILTER_Q_IS_SHELF_SLOPE, //Q is shelf slope; for shelving EQ only; When S = 1, the shelf slope is as steep as it can be and remain monotonically increasing or decreasing gain with frequency.
};

#define BIQUAD_FILTER_POLES 		2
#define BIQUAD_FILTER_CHANNELS 		PSYNTH_MAX_CHANNELS
#define BIQUAD_FILTER_STAGES		8
#define BIQUAD_FILTER_BUF_SIZE		256

#ifndef PS_STYPE_ONE
    #error Include this file after the PS_STYPE_* configuration!
#endif
typedef double biquad_filter_float; //32bit float may be unstable (try lowfreq sine + HP 0Hz);
//32bit vs 64bit:
//  modern CPUs with FPU: almost no difference (for both 32 and 64 bit CPUs);
//  old CPUs without FPU: 32bit is 1.3 - 1.5 times faster;

struct biquad_filter_state
{
    biquad_filter_float		a[ BIQUAD_FILTER_POLES + 1 ];
    biquad_filter_float		b[ BIQUAD_FILTER_POLES + 1 ];
    biquad_filter_float		x[ BIQUAD_FILTER_POLES * BIQUAD_FILTER_CHANNELS * BIQUAD_FILTER_STAGES ]; //{stage0: {poles for ch0}, {poles for ch1}}, {stage1: ...}, {stage2: ...} ...
    biquad_filter_float		y[ BIQUAD_FILTER_POLES * BIQUAD_FILTER_CHANNELS * BIQUAD_FILTER_STAGES ];
};
#define BIQUAD_FILTER_STAGE_BYTES 		( sizeof(biquad_filter_float) * BIQUAD_FILTER_POLES * BIQUAD_FILTER_CHANNELS )

#define BIQUAD_FILTER_FLAG_IGNORE_STAGES	( 1 << 0 )
#define BIQUAD_FILTER_FLAG_USE_STAGES_FOR_APF	( 1 << 1 )

struct biquad_filter
{
    //Input parameters:
    uint32_t			type;
    int				sfreq;
    biquad_filter_float		freq;
    biquad_filter_float		dBgain;
    biquad_filter_float		Q;
    uint32_t			flags;

    //Filter state:
    biquad_filter_state		state;
    biquad_filter_state		interp_state;
    int				interp_len;
    int				interp_ptr[ BIQUAD_FILTER_CHANNELS ];

    //Internal filter stuff:
    biquad_filter_float		buf[ BIQUAD_FILTER_BUF_SIZE ];
};

#define BFT_FTYPE_BITS		5
#define BFT_FTYPE_OFFSET	0
#define BFT_QTYPE_BITS		2
#define BFT_QTYPE_OFFSET	BFT_FTYPE_BITS
#define BFT_STAGES_BITS		4
#define BFT_STAGES_OFFSET	( BFT_FTYPE_BITS + BFT_QTYPE_BITS )
#define BFT_SMOOTH_OFFSET	( BFT_FTYPE_BITS + BFT_QTYPE_BITS + BFT_STAGES_BITS )
#define BFT_SMOOTH		( 1 << BFT_SMOOTH_OFFSET ) //parameter smoothing
inline uint32_t make_biquad_filter_type( biquad_filter_ftype ftype, biquad_filter_qtype qtype, int stages, bool smooth )
{
    uint32_t rv = 0;
    rv |= ( (uint32_t)ftype ) << BFT_FTYPE_OFFSET;
    rv |= ( (uint32_t)qtype ) << BFT_QTYPE_OFFSET;
    rv |= ( (uint32_t)stages ) << BFT_STAGES_OFFSET;
    rv |= ( (uint32_t)smooth ) << BFT_SMOOTH_OFFSET;
    return rv;
}
inline biquad_filter_ftype get_biquad_filter_ftype( uint32_t type )
{
    return (biquad_filter_ftype)( ( type >> BFT_FTYPE_OFFSET ) & ( ( 1 << BFT_FTYPE_BITS ) - 1 ) );
}
inline biquad_filter_qtype get_biquad_filter_qtype( uint32_t type )
{
    return (biquad_filter_qtype)( ( type >> BFT_QTYPE_OFFSET ) & ( ( 1 << BFT_QTYPE_BITS ) - 1 ) );
}
inline int get_biquad_filter_stages( uint32_t type )
{
    return ( type >> BFT_STAGES_OFFSET ) & ( ( 1 << BFT_STAGES_BITS ) - 1 );
}
inline int get_biquad_filter_stages2( biquad_filter* f )
{
    int stages = get_biquad_filter_stages( f->type );
    if( f->flags & BIQUAD_FILTER_FLAG_IGNORE_STAGES ) stages = 1;
    return stages;
}

biquad_filter* biquad_filter_new( uint32_t flags );
void biquad_filter_remove( biquad_filter* f );
void biquad_filter_stop( biquad_filter* f );
bool biquad_filter_change(
    biquad_filter*              f,
    int                         interp_len,
    uint32_t	    	      	type,
    int                         sfreq,
    biquad_filter_float         freq,
    biquad_filter_float         dBgain,
    biquad_filter_float         Q );
void biquad_filter_run( biquad_filter* f, int ch, PS_STYPE* in, PS_STYPE* out, size_t len );
biquad_filter_float biquad_filter_freq_response( biquad_filter* f, biquad_filter_float freq );

//
// Oversampler
//

#define PSYNTH_OVERSAMPLER_BUF_SIZE	256
#define PSYNTH_OVERSAMPLER_UPSMP_VALS	4
#define PSYNTH_OVERSAMPLER_DOWNSMP_VALS	7
#define PSYNTH_OVERSAMPLER_CHANNELS	2

#define PSYNTH_OVERSAMPLER_DISABLED

struct psynth_oversampler
{
    int				scale; //2, 3, 4, 5, ...
    PS_STYPE*			src;
    PS_STYPE*			dest;
    size_t			size;
    size_t			ptr;
    PS_STYPE			buf[ PSYNTH_OVERSAMPLER_BUF_SIZE ];
    size_t			buf_size;
    PS_STYPE2			up[ PSYNTH_OVERSAMPLER_UPSMP_VALS * PSYNTH_OVERSAMPLER_CHANNELS ]; //Upsampler interpolator values
    PS_STYPE2			down[ PSYNTH_OVERSAMPLER_DOWNSMP_VALS * PSYNTH_OVERSAMPLER_CHANNELS ]; //Downsampler filter values
};

psynth_oversampler* psynth_oversampler_new();
void psynth_oversampler_remove( psynth_oversampler* os );
void psynth_oversampler_stop( psynth_oversampler* os );
void psynth_oversampler_init( psynth_oversampler* os, int scale, PS_STYPE* src, PS_STYPE* dest, size_t size );
PS_STYPE* psynth_oversampler_begin( psynth_oversampler* os, int ch );
int psynth_oversampler_end( psynth_oversampler* os, int ch );

//
// Smooth parameter changes
//

struct psmoother_coefs
{
    PS_STYPE2 a0; //1-pole filter coefficients
    PS_STYPE2 b1; //...
};
struct psmoother
{
    PS_STYPE2 v; //Current value (LP filtered)
#ifdef PS_STYPE_FLOATINGPOINT
    PS_STYPE2 v2; //Current value (LP filtered) - second order
#endif
#ifndef PS_STYPE_FLOATINGPOINT
    PS_STYPE2 remainder;
#endif
};

inline void psmoother_init( psmoother_coefs* c, float low_pass_filter_freq, float sample_rate )
{
    //1-pole low-pass filter coefficients:
    float b1 = exp( -2.0 * M_PI * ( low_pass_filter_freq / sample_rate ) );
    float a0 = 1.0 - b1;
#ifdef PS_STYPE_FLOATINGPOINT
    c->b1 = b1;
    c->a0 = a0;
#else
    c->b1 = 32768 * b1;
    if( c->b1 > 32767 ) c->b1 = 32767;
    c->a0 = 32768 - c->b1;
#endif
}
inline void psmoother_reset( psmoother* p, PS_STYPE2 cur_val, PS_STYPE2 norm_val )
{
#ifdef PS_STYPE_FLOATINGPOINT
    p->v = cur_val / norm_val;
    p->v2 = cur_val / norm_val;
#else
    p->v = cur_val * ( 32768 / norm_val );
    p->remainder = 0;
#endif
}
inline PS_STYPE2 psmoother_target( PS_STYPE2 target_val, PS_STYPE2 norm_val ) //get normalized target value: (0...1 or 0...32768)
{
#ifdef PS_STYPE_FLOATINGPOINT
    return target_val / norm_val;
#else
    return target_val * ( 32768 / norm_val );
#endif
}
inline bool psmoother_check( psmoother* p, PS_STYPE2 norm_target_val )
{
#ifdef PS_STYPE_FLOATINGPOINT
    if( PS_STYPE_ABS( p->v2 - norm_target_val ) > 1.0F / 32768.0F ) return true;
#else
    if( PS_STYPE_ABS( p->v - norm_target_val ) >= 1 ) return true;
#endif
    return false;
}
inline PS_STYPE2 psmoother_val( psmoother_coefs* RESTRICT c, psmoother* RESTRICT p, PS_STYPE2 norm_target_val ) //get next normalized value: (0...1 or 0...32768)
{
#ifdef PS_STYPE_FLOATINGPOINT
    p->v = norm_target_val * c->a0 + p->v * c->b1;
    p->v2 = p->v * c->a0 + p->v2 * c->b1;
    return p->v2;
#else
    int64_t v = (int64_t)norm_target_val * c->a0 + (int64_t)p->v * c->b1 + p->remainder;
    p->remainder = v & 32767;
    p->v = v / 32768;
    return p->v;
#endif
}

//
// Renderbuf helper
//

//1ch renderbuf:
#define PSYNTH_GET_RENDERBUF( RETVAL, OUTS, OUTS_NUM, OFFSET ) \
    (RETVAL==0||RETVAL==2) ? OUTS[ (OUTS_NUM) - 1 ] + (OFFSET) : psynth_get_temp_buf( mod_num, pnet, 0 )
//2ch renderbuf:
#define PSYNTH_GET_RENDERBUF2( RETVAL, OUTS, OUTS_NUM, OFFSET, OUT_NUM ) \
    (RETVAL==0||RETVAL==2) ? OUTS[ OUT_NUM ] + (OFFSET) : psynth_get_temp_buf( mod_num, pnet, OUT_NUM )
struct psynth_renderbuf_pars
{
    psmoother 	vol[ PSYNTH_MAX_CHANNELS ];
    PS_STYPE	last_sample[ PSYNTH_MAX_CHANNELS ];
    PS_STYPE	anticlick_sample[ PSYNTH_MAX_CHANNELS ];
    int		anticlick_counter; //0 - off; 1..32768 - active;
    bool 	start;
    bool	anticlick;
};
int psynth_renderbuf2output(
    int retval,
    PS_STYPE** outputs, int outputs_num, int out_offset, int out_frames,
    PS_STYPE* render_buf1, PS_STYPE* render_buf2, int rendered_frames,
    int vol, int pan,
    psynth_renderbuf_pars* RESTRICT renderbuf_pars,
    psmoother_coefs* RESTRICT smoother_coefs,
    int srate );
/*Synth example (mono oscillator -> volume/pan -> mono/stereo output):
on Note ON:
{
    if( chan->active )
    {
	if( !zero_attack )
	{
            chan->renderbuf_pars.anticlick = true;
        }
    }
    chan->renderbuf_pars.start = true;
}
for( polyphony channels )
{
    if( !chan->playing ) continue;
    PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, outputs, outputs_num, offset );
    //render something...
    //rendered_frames may be != frames
    retval = psynth_renderbuf2output(
	retval,
	outputs, outputs_num, offset, frames,
	render_buf, NULL, rendered_frames,
	local_vol, local_pan,
	renderbuf_pars, 
	smoother_coefs,
	pnet->sampling_freq );
}
*/

//
// Misc
//

inline uint32_t psynth_rand( uint32_t* seed ) //0...32767
{
    *seed = *seed * 1103515245 + 12345;
    return ( *seed >> 16 ) & 32767;
}

inline int psynth_rand2( uint32_t* seed ) //-32768...32767
{
    *seed = *seed * 1103515245 + 12345;
    return (int)(*seed) >> 16;
}
