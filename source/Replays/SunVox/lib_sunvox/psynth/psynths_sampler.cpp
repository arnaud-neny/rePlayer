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
#include "psynths_sampler.h"
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_sampler_data
#define MODULE_HANDLER	psynth_sampler
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	32
#define MAX_SAMPLES	128 
#define XI_ENV_POINTS	12 
#define REC_FRAMES	( 1024 * 64 )
#define CHUNK_INS			0
#define CHUNK_SMP( smp_num )	    	( 1 + smp_num * 2 )
#define CHUNK_SMP_DATA( smp_num )   	( 1 + smp_num * 2 + 1 )
#define CHUNK_OPT			CHUNK_SMP( MAX_SAMPLES )
#define CHUNK_ENV( env_num )		( CHUNK_OPT + 1 + env_num )
#define CHUNK_EFFECT_MOD		( CHUNK_ENV( 8 ) )
#define SUBMOD_MAX_CHANNELS 		MAX_CHANNELS
#define SUBMOD_SUNVOX_FLAGS		SUNVOX_FLAG_NO_MODULE_CHANNELS
#define INTERP_PREC ( PSYNTH_FP64_PREC - 1 ) 
#define INS_SIGN 'SAMP'
#define INS_VERSION 6
#define LOAD_XI_FLAG_SET_MAX_VOLUME	1
struct sampler_options
{
    bool	rec_on_play; 
    bool	rec_mono; 
    bool	rec_reduced_freq; 
    bool	rec_16bit; 
    bool	finish_rec_on_stop; 
    bool	ignore_vel_for_volume; 
    bool	freq_accuracy; 
    uint8_t	fit_to_pattern; 
};
enum
{
    ENV_VOL = 0,
    ENV_PAN,
    ENV_PITCH,
    ENV_EFF1,
    ENV_EFF2,
    ENV_EFF3,
    ENV_EFF4,
    ENV_COUNT 
};
#define FIRST_ENV_WITH_SUBDIV	ENV_PITCH
#define LAST_EFF_ENV		ENV_EFF4
const uint g_envelopes_signed = 
    ( 1 << ENV_PAN ) |
    ( 1 << ENV_PITCH );
#define ENV_FLAG_ENABLED		( 1 << 0 )
#define ENV_FLAG_SUSTAIN		( 1 << 1 )
#define ENV_FLAG_LOOP			( 1 << 2 )
struct sampler_envelope_header
{
    uint16_t	flags;
    uint8_t	eff_ctl;
    uint8_t	gain; 
    uint8_t 	velocity_influence; 
    uint8_t 	unused3;
    uint8_t 	unused4;
    uint8_t 	unused5;
    uint16_t	point_count;
    uint16_t	sustain;
    uint16_t	loop_start;
    uint16_t	loop_end;
    uint16_t	unused6;
    uint16_t	unused7;
};
struct sampler_envelope
{
    sampler_envelope_header h; 
    uint	p[ 32768 ]; 
};
#define GEN_CHANNEL_FLAG_PLAYING	( 1 << 0 )
#define GEN_CHANNEL_FLAG_SUSTAIN	( 1 << 1 ) 
#define GEN_CHANNEL_FLAG_LOOP		( 1 << 2 ) 
#define GEN_CHANNEL_FLAG_BACK		( 1 << 3 ) 
#define GEN_CHANNEL_FLAG_ENV_START	( 1 << 4 ) 
struct gen_channel
{
    uint	flags;
    uint	id;
    int 	vel; 
    SMPPTR	ptr_h;
    int		ptr_l;
    uint	delta_h;
    uint	delta_l;
    int		cur_pitch;
    int		cur_pitch_delta; 
    int		pitch_add1; 
    int		pitch_add2; 
    int		final_pitch; 
    PS_STYPE	last_s[ MODULE_OUTPUTS ]; 
    PS_STYPE	anticlick_in_s[ MODULE_OUTPUTS ]; 
    uint16_t	anticlick_in_counter; 
    uint16_t	anticlick_out_counter; 
    int		tick_counter;   	
    int		tick_counter2;   	
    int		subtick_counter;	
    uint	env_pos[ ENV_COUNT ];	
    uint16_t	env_prev[ ENV_COUNT ];	
    uint16_t	env_next[ ENV_COUNT ];	
    int		prev_eff_ctl_val;
    int16_t	vib_pos;	
    int16_t	vib_frame;	
    int		fadeout;	
    int		vol_step;	
    int		l_delta;	
    int		r_delta;	
    int		l_cur;		
    int		r_cur;		
    int		l_old;		
    int		r_old;		
    int16_t    	smp_num;
    int16_t    	smp_note_num;
    PS_CTYPE   	local_pan;
};
struct MODULE_DATA
{
    PS_CTYPE	ctl_volume;
    PS_CTYPE	ctl_pan;
    PS_CTYPE	ctl_smp_int;
    PS_CTYPE	ctl_env_int;
    PS_CTYPE	ctl_channels;
    PS_CTYPE	ctl_rec_threshold;
    PS_CTYPE	ctl_tick_scale;
    PS_CTYPE	ctl_record;
    gen_channel	channels[ MAX_CHANNELS ];
    bool    	no_active_channels;
    bool	editor_play; 
    int	    	search_ptr;
    int		last_ch;
    uint*   	linear_freq_tab;
    uint8_t*  	vibrato_tab;
    bool    	fdialog_opened;
    int		sample_base_pitch[ MAX_SAMPLES ];
    int		sample_base_freq[ MAX_SAMPLES ];
    sampler_envelope* env[ ENV_COUNT ];
    uint16_t*  	env_buf[ ENV_COUNT ]; 
    bool	eff_env; 
    uint8_t	ticks_per_line;
    int     	tick_size; 
    int     	tick_size2; 
    int		min_tick_size2;
    uint16_t	tick_subdiv; 
    uint16_t	anticlick_len; 
    psynth_sunvox*	ps; 
    int		mod_num; 
    void*  	rec_buf;
    bool	rec;
    uint8_t	rec_channels : 2;
    bool	rec_reduced_freq : 1;
    bool	rec_16bit : 1;
    bool	rec_play_pressed : 1; 
    volatile bool	rec_pause;
    std::atomic_uint	rec_wp;
    uint	rec_frames;
    smutex	rec_btn_mutex;
    sthread	rec_thread;
    std::atomic_int	rec_thread_stop_request; 
    std::atomic_int	rec_thread_state; 
    ssemaphore	rec_thread_sem;
    sampler_options* opt;
#ifdef SUNVOX_GUI
    window_manager* 	wm;
    gen_channel		editor_player_channel;
#endif
};
#define SAMPLE_TYPE_FLAG_LOOPRELEASE	( 1 << 2 )
#define SAMPLE_TYPE_FLAG_STEREO		( 1 << 6 )
struct sample 
{
    uint32_t	    	length; 
    uint32_t	    	reppnt;
    uint32_t	    	replen;
    uint8_t	    	volume;
    int8_t	    	finetune; 
    uint8_t	    	type; 
    uint8_t	    	panning;
    int8_t	    	relative_note; 
    uint8_t	  	reserved2;
    char		name[ 22 ];
    uint32_t		start_pos; 
};
struct instrument 
{
    uint32_t    unused1;
    char 	name[ 22 ];
    uint16_t   	unused2;
    uint16_t  	samples_num;
    uint16_t	unused3;
    uint32_t    unused4;
    uint8_t  	smp_num_old[ 96 ]; 
    uint16_t  	volume_points_old[ XI_ENV_POINTS * 2 ]; 
    uint16_t  	panning_points_old[ XI_ENV_POINTS * 2 ];
    uint8_t   	volume_points_num_old;
    uint8_t   	panning_points_num_old;
    uint8_t   	vol_sustain_old;
    uint8_t   	vol_loop_start_old;
    uint8_t   	vol_loop_end_old;
    uint8_t   	pan_sustain_old;
    uint8_t   	pan_loop_start_old;
    uint8_t   	pan_loop_end_old;
    uint8_t   	volume_type_old;
    uint8_t   	panning_type_old;
    uint8_t   	vibrato_type; 
    uint8_t   	vibrato_sweep;
    uint8_t   	vibrato_depth;
    uint8_t   	vibrato_rate;
    uint16_t  	volume_fadeout;
    uint8_t	volume_old; 
    int8_t	finetune;
    uint8_t   	unused5;
    int8_t	relative_note;
    uint32_t	unused6;
    uint32_t	sign; 
    uint32_t	version; 
    uint8_t  	smp_num[ 128 ];
    uint32_t	max_version; 
    int32_t	editor_cursor;
    int32_t	editor_selected_size; 
};
static void reset_sampler_channel( gen_channel* ch );
static void recalc_base_pitch( int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net );
static int get_base_note( int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net, bool use_module_props );
static void set_base_note( int base_note, int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net );
static void editor_play( psynth_net* net, int mod_num, int smp_num, bool play, SMPPTR start_pos );
static uint16_t* refresh_instrument_envelope( uint16_t* RESTRICT src, uint pnum, uint16_t* RESTRICT dest );
static void refresh_envelopes( MODULE_DATA* data, uint8_t env );
static void refresh_envelope_pointers( int mod_num, psynth_net* pnet );
static void handle_envelope_flags( MODULE_DATA* data );
static const char* get_envelope_name( uint num, bool short_str );
static void bytes2frames( sample* smp );
static void sampler_calc_final_pitch( gen_channel* chan, MODULE_DATA* data, psynth_module* mod );
static void save_audio_file( 
    const char* filename,
    int mod_num, 
    sfs_file_fmt file_format,
    int q, 
    psynth_net* net, 
    int sample_num );
static void* create_raw_instrument_or_sample( 
    const char* name, 
    int mod_num, 
    uint data_bytes, 
    int bits, 
    int channels, 
    int freq, 
    psynth_net* net, 
    int sample_num );
static int load_audio_file(
    sfs_file f,
    const char* name,
    int mod_num,
    psynth_net* net,
    int sample_num );
static int load_xi_instrument( 
    sfs_file f, 
    uint flags, 
    int mod_num, 
    psynth_net* net,
    int sample_num );
static int load_instrument_or_sample(
    const char* filename, 
    sfs_file f,
    uint flags, 
    int mod_num, 
    psynth_net* net, 
    int sample_num );
#include "psynths_sampler_gui.h"
static void remove_instrument_and_samples( int mod_num, psynth_net* pnet )
{
    for( int i = 0; i < CHUNK_OPT; i++ )
    {
	psynth_remove_chunk( mod_num, i, pnet );
    }
    pnet->change_counter++;
}
static void reset_sampler_channel( gen_channel* ch )
{
    memset( &ch->env_pos, 0, sizeof( ch->env_pos ) );
    for( int i = ENV_PAN; i < ENV_COUNT; i++ )
    {
	if( g_envelopes_signed & ( 1 << i ) )
	{
	    ch->env_prev[ i ] = 32768 / 2;
	    ch->env_next[ i ] = 32768 / 2;
	}
	else
	{
	    ch->env_prev[ i ] = 0;
	    ch->env_next[ i ] = 0;
	}
    }
    ch->flags |= GEN_CHANNEL_FLAG_SUSTAIN;
    ch->fadeout = 65536;
    ch->vib_pos = 0;
    ch->vib_frame = 0;
    ch->flags |= GEN_CHANNEL_FLAG_ENV_START;
    ch->vol_step = 0;
    ch->ptr_h = 0;
    ch->ptr_l = 0;
    ch->flags &= ~GEN_CHANNEL_FLAG_LOOP;
    ch->flags &= ~GEN_CHANNEL_FLAG_BACK;
    ch->tick_counter = 0xFFFFFFF; 
    ch->tick_counter2 = 0;
}
static void release_sampler_channel( gen_channel* ch, instrument* ins, MODULE_DATA* data )
{
    if( ch->flags & GEN_CHANNEL_FLAG_SUSTAIN )
    {
	ch->flags &= ~GEN_CHANNEL_FLAG_SUSTAIN;
	if( ( data->env[ ENV_VOL ]->h.flags & ENV_FLAG_ENABLED ) == 0 ) 
	{
	    ch->anticlick_out_counter = 32768;
	}
    }
}
static void recalc_base_pitch( int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net )
{
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
    if( !smp ) return; 
    int freq = 0;
    psynth_get_chunk_info( mod_num, CHUNK_SMP_DATA( smp_num ), net, 0, 0, &freq );
    if( freq == 0 ) freq = 44100;
    int base_pitch = PS_SFREQ_TO_PITCH( freq );
    base_pitch = PS_NOTE0_PITCH / 4 - ( ( PS_NOTE0_PITCH - base_pitch ) / 4 );
    base_pitch *= 4;
    module_data->sample_base_pitch[ smp_num ] = base_pitch;
    module_data->sample_base_freq[ smp_num ] = freq;
}
static int get_base_note( int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net, bool use_module_props )
{
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
    if( !smp ) return -1; 
    int base_pitch = module_data->sample_base_pitch[ smp_num ];
    int base_note = PS_PITCH_TO_NOTE( base_pitch + smp->finetune * 2 ) - smp->relative_note;
    if( use_module_props )
    {
	psynth_module* m = psynth_get_module( mod_num, net );
	if( m )
	{
    	    base_note -= m->relative_note;
            int ft = m->finetune;
            if( ft >= 128 ) base_note--;
            if( ft < -128 ) base_note++;
        }
    }
    return base_note;
}
static void set_base_note( int base_note, int smp_num, int mod_num, MODULE_DATA* module_data, psynth_net* net )
{
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
    if( !smp ) return; 
    recalc_base_pitch( smp_num, mod_num, module_data, net );
    int base_pitch = module_data->sample_base_pitch[ smp_num ];
    smp->finetune = ( ( PS_NOTE0_PITCH - base_pitch ) & 255 ) / 2;
    smp->relative_note = PS_PITCH_TO_NOTE( base_pitch ) - base_note;
    net->change_counter++;
}
static void set_loop( int smp_num, uint start, uint length, uint8_t type, int mod_num, psynth_net* net )
{
    if( mod_num >= 0 )
    {
    }
    else return;
    if( smp_num < 0 ) smp_num = 0;
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
    if( !smp ) return; 
    smp->type = ( smp->type & ~3 );
    if( start < smp->length && start + length <= smp->length )
    {
	smp->reppnt = start;
	smp->replen = length;
	smp->type |= ( type & 3 );
    }
    net->change_counter++;
}
#ifdef SUNVOX_GUI
static void editor_play( psynth_net* net, int mod_num, int smp_num, bool play, SMPPTR start_pos )
{
    psynth_module* mod = NULL;
    MODULE_DATA* module_data = NULL;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        module_data = (MODULE_DATA*)mod->data_ptr;
    }
    else return;
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, net );
    if( !ins ) return;
    gen_channel* ch = &module_data->editor_player_channel;
    if( play )
    {
	sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
	if( !smp ) return;
	memset( ch, 0, sizeof( gen_channel ) );
	ch->smp_num = smp_num;
	int note = 12 * 5;
	int pitch = PS_NOTE_TO_PITCH( note );
	pitch = ( pitch >> 2 ) - ( smp->relative_note * 64 ) - ( ins->relative_note * 64 ) - ( smp->finetune >> 1 ) - ( ins->finetune >> 1 );
	ch->cur_pitch = pitch * 4;
	ch->cur_pitch_delta = ch->cur_pitch - pitch;
	ch->final_pitch = 0xFFFFFFF;
	sampler_calc_final_pitch( ch, module_data, mod );
	ch->flags |= GEN_CHANNEL_FLAG_PLAYING;
	ch->vel = 256;
	ch->id = 0x12345678;
	ch->local_pan = 128;
	reset_sampler_channel( ch );
	if( start_pos < 0 ) start_pos = 0;
	if( start_pos >= (SMPPTR)smp->length ) start_pos = 0;
	ch->ptr_h = start_pos;
	module_data->no_active_channels = false;
	module_data->editor_play = true;
    }
    else
    {
	ch->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
	module_data->editor_play = false;
    }
}
#endif
static uint16_t* refresh_instrument_envelope( 
    uint16_t* RESTRICT src, 
    uint pnum, 
    uint16_t* RESTRICT dest )
{
    if( pnum == 0 ) return dest;
    uint len = smem_get_size( dest ) / sizeof( uint16_t ); 
    uint new_len = 0;
    uint len_add = 64;
    for( uint p = 0; p < pnum * 2; p += 2 )
    {
	uint l = src[ p ] + len_add;
	if( l > new_len ) new_len = l;
    }
    if( new_len > len )
    {
	dest = (uint16_t*)smem_resize2( dest, new_len * sizeof( uint16_t ) );
	if( dest == 0 ) return 0;
	len = new_len;
    }
    uint max_t = new_len - len_add + 1; 
    if( pnum >= 2 )
    {
	int prev_t = 0;
	for( uint p = 0; p < (uint)( pnum - 1 ) * 2; p += 2 )
	{
	    int t1 = src[ p ];
	    int v1 = src[ p + 1 ];
	    int t2 = src[ p + 2 ];
	    int v2 = src[ p + 3 ];
	    if( t2 < prev_t ) 
	    {
		break;
	    }
	    prev_t = t2;
	    int delta;
	    int val = v1 << 8; 
	    if( t2 - t1 == 0 ) delta = 0; else delta = ( ( v2 - v1 ) << 8 ) / ( t2 - t1 );
	    for( int i = t1; i <= t2; i++ )
	    {
		dest[ i ] = (uint16_t) ( val >> 8 ); 
    		val += delta;
	    }
	    dest[ t2 ] = (uint16_t)v2;
	}
    }
    else
    {
	uint16_t v = src[ 0 + 1 ];
	for( uint i = 0; i <= max_t; i++ ) dest[ i ] = v;
    }
    return dest;
}
static void refresh_envelopes( MODULE_DATA* data, uint8_t env )
{
    for( int i = 0; i < ENV_COUNT; i++ )
    {
        if( env & ( 1 << i ) )
	{
	    data->env_buf[ i ] = refresh_instrument_envelope( (uint16_t*)data->env[ i ]->p, data->env[ i ]->h.point_count, data->env_buf[ i ] );
	}
    }
}
static void refresh_envelope_pointers( int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    for( int i = 0; i < ENV_COUNT; i++ ) 
    {
        data->env[ i ] = (sampler_envelope*)psynth_get_chunk_data( mod_num, CHUNK_ENV( i ), pnet );
    }
}
static void handle_envelope_flags( MODULE_DATA* data )
{
    data->tick_subdiv = 0;
    data->eff_env = false;
    for( int i = FIRST_ENV_WITH_SUBDIV; i < ENV_COUNT; i++ )
    {
	if( data->env[ i ]->h.flags & ENV_FLAG_ENABLED )
	{
	    data->tick_subdiv = 3;
	    break;
	}
    }
    for( int i = ENV_EFF1; i <= LAST_EFF_ENV; i++ )
    {
	if( data->env[ i ]->h.flags & ENV_FLAG_ENABLED )
	{
	    data->eff_env = true;
	    break;
	}
    }
}
#ifdef SUNVOX_GUI
static const char* get_envelope_name( uint num, bool short_str )
{
    const char* rv = 0;
    switch( num )
    {
	case ENV_VOL: rv = ps_get_string( STR_PS_VOLUME ); break;
	case ENV_PAN: rv = ps_get_string( STR_PS_PANNING ); break;
	case ENV_PITCH: rv = ps_get_string( STR_PS_PITCH ); break;
	case ENV_EFF1: if( short_str ) rv = ps_get_string( STR_PS_EFF1_SHORT ); else rv = ps_get_string( STR_PS_EFF1 ); break;
	case ENV_EFF2: if( short_str ) rv = ps_get_string( STR_PS_EFF2_SHORT ); else rv = ps_get_string( STR_PS_EFF2 ); break;
	case ENV_EFF3: if( short_str ) rv = ps_get_string( STR_PS_EFF3_SHORT ); else rv = ps_get_string( STR_PS_EFF3 ); break;
	case ENV_EFF4: if( short_str ) rv = ps_get_string( STR_PS_EFF4_SHORT ); else rv = ps_get_string( STR_PS_EFF4 ); break;
	default: break;
    }
    return rv;
}
#endif
static void optimize_envelopes( int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    for( int i = 0; i < ENV_COUNT; i++ ) 
    {
	sampler_envelope* env = data->env[ i ];
	size_t new_size = sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count;
	psynth_resize_chunk( mod_num, CHUNK_ENV( i ), new_size, pnet );
    }
    refresh_envelope_pointers( mod_num, pnet );
}
static void make_default_envelopes( int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr; 
    for( uint env_num = 0; env_num < ENV_COUNT; env_num++ )
    {
	sampler_envelope* env = data->env[ env_num ];
	smem_zero( env );
	env->h.gain = 100;
	env->h.point_count = 1;
    }
    sampler_envelope* env = data->env[ ENV_VOL ];
    env->h.point_count = 3;
    env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
        mod_num, CHUNK_ENV( ENV_VOL ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
    env->p[ 0 ] = 0 | ( 32768 << 16 );
    env->p[ 1 ] = 8 | ( 0 << 16 );
    env->p[ 2 ] = 32 | ( 0 << 16 );
    env->h.sustain = 0;
    env->h.flags |= ENV_FLAG_ENABLED | ENV_FLAG_SUSTAIN;
    env = data->env[ ENV_PAN ];
    env->h.point_count = 4;
    env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
        mod_num, CHUNK_ENV( ENV_PAN ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
    env->p[ 0 ] = 0 | ( 16384 << 16 );
    env->p[ 1 ] = 64 | ( 8192 << 16 );
    env->p[ 2 ] = 128 | ( 24576 << 16 );
    env->p[ 3 ] = 180 | ( 16384 << 16 );
    env = data->env[ ENV_PITCH ];
    env->h.point_count = 2;
    env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
        mod_num, CHUNK_ENV( ENV_PITCH ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
    env->p[ 0 ] = 0 | ( 16384 << 16 );
    env->p[ 1 ] = 64 | ( 16384 << 16 );
    for( uint env_num = ENV_EFF1; env_num <= LAST_EFF_ENV; env_num++ )
    {
	env = data->env[ env_num ];
	env->h.point_count = 2;
	env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
    	    mod_num, CHUNK_ENV( env_num ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
        env->p[ 0 ] = 0 | ( 32768 << 16 );
	env->p[ 1 ] = 64 | ( 32768 << 16 );
    }
    refresh_envelope_pointers( mod_num, pnet );
    handle_envelope_flags( data );
    pnet->change_counter++;
}
static void convert_old_envelopes_to_new( int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
    if( !ins ) return;
    sampler_envelope* env;
    env = data->env[ ENV_VOL ];
    if( env )
    {
	smem_zero( env );
	env->h.gain = 100;
	if( ins->volume_type_old & 1 ) env->h.flags |= ENV_FLAG_ENABLED;
	if( ins->volume_type_old & 2 ) env->h.flags |= ENV_FLAG_SUSTAIN;
	if( ins->volume_type_old & 4 ) env->h.flags |= ENV_FLAG_LOOP;
	env->h.sustain = ins->vol_sustain_old;
	env->h.loop_start = ins->vol_loop_start_old;
	env->h.loop_end = ins->vol_loop_end_old;
	env->h.point_count = ins->volume_points_num_old;
	env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
	    mod_num, CHUNK_ENV( ENV_VOL ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
	for( uint i = 0; i < env->h.point_count; i++ )
	{
	    uint x = ins->volume_points_old[ i * 2 ];
	    uint y = ins->volume_points_old[ i * 2 + 1 ];
	    env->p[ i ] = x | ( y << ( 16 + 9 ) );
	}
    }
    env = data->env[ ENV_PAN ];
    if( env )
    {
	smem_zero( env );
	env->h.gain = 100;
	if( ins->panning_type_old & 1 ) env->h.flags |= ENV_FLAG_ENABLED;
	if( ins->panning_type_old & 2 ) env->h.flags |= ENV_FLAG_SUSTAIN;
	if( ins->panning_type_old & 4 ) env->h.flags |= ENV_FLAG_LOOP;
	env->h.sustain = ins->pan_sustain_old;
	env->h.loop_start = ins->pan_loop_start_old;
	env->h.loop_end = ins->pan_loop_end_old;
	env->h.point_count = ins->panning_points_num_old;
	env = (sampler_envelope*)psynth_get_chunk_data_autocreate( 
	    mod_num, CHUNK_ENV( ENV_PAN ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ) * env->h.point_count, 0, 0, pnet );
	for( uint i = 0; i < env->h.point_count; i++ )
	{
	    uint x = ins->panning_points_old[ i * 2 ];
	    uint y = ins->panning_points_old[ i * 2 + 1 ];
	    env->p[ i ] = x | ( y << ( 16 + 9 ) );
	}
    }
    refresh_envelope_pointers( mod_num, pnet );
    handle_envelope_flags( data );
}
static void convert_new_envelopes_to_old( int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
    if( !ins ) return;
    sampler_envelope* env;
    env = data->env[ ENV_VOL ];
    if( env )
    {
	ins->volume_type_old = 0;
	if( env->h.flags & ENV_FLAG_ENABLED ) ins->volume_type_old |= 1;
	if( env->h.flags & ENV_FLAG_SUSTAIN ) ins->volume_type_old |= 2;
	if( env->h.flags & ENV_FLAG_LOOP ) ins->volume_type_old |= 4;
	ins->vol_sustain_old = env->h.sustain;
	ins->vol_loop_start_old = env->h.loop_start;
	ins->vol_loop_end_old = env->h.loop_end;
	ins->volume_points_num_old = env->h.point_count;
	if( ins->volume_points_num_old > XI_ENV_POINTS )
	    ins->volume_points_num_old = XI_ENV_POINTS; 
	for( uint i = 0; i < ins->volume_points_num_old; i++ )
	{
	    uint x = env->p[ i ] & 0xFFFF;
	    uint y = ( env->p[ i ] >> ( 16 + 9 ) ) & 0xFFFF;
	    ins->volume_points_old[ i * 2 ] = x;
	    ins->volume_points_old[ i * 2 + 1 ] = y;
	}
    }
    env = data->env[ ENV_PAN ];
    if( env )
    {
	ins->panning_type_old = 0;
	if( env->h.flags & ENV_FLAG_ENABLED ) ins->panning_type_old |= 1;
	if( env->h.flags & ENV_FLAG_SUSTAIN ) ins->panning_type_old |= 2;
	if( env->h.flags & ENV_FLAG_LOOP ) ins->panning_type_old |= 4;
	ins->pan_sustain_old = env->h.sustain;
	ins->pan_loop_start_old = env->h.loop_start;
	ins->pan_loop_end_old = env->h.loop_end;
	ins->panning_points_num_old = env->h.point_count;
	if( ins->panning_points_num_old > XI_ENV_POINTS )
	    ins->panning_points_num_old = XI_ENV_POINTS; 
	for( uint i = 0; i < ins->panning_points_num_old; i++ )
	{
	    uint x = env->p[ i ] & 0xFFFF;
	    uint y = ( env->p[ i ] >> ( 16 + 9 ) ) & 0xFFFF;
	    ins->panning_points_old[ i * 2 ] = x;
	    ins->panning_points_old[ i * 2 + 1 ] = y;
	}
    }
}
static void bytes2frames( sample* smp )
{
    int bits = 8;
    int channels = 1;
    if( ( ( smp->type >> 4 ) & 3 ) == 0 ) bits = 8;
    if( ( ( smp->type >> 4 ) & 3 ) == 1 ) bits = 16;
    if( ( ( smp->type >> 4 ) & 3 ) == 2 ) bits = 32;
    if( smp->type & SAMPLE_TYPE_FLAG_STEREO ) channels = 2;
    smp->length = smp->length / ( ( bits / 8 ) * channels );
    smp->reppnt = smp->reppnt / ( ( bits / 8 ) * channels );
    smp->replen = smp->replen / ( ( bits / 8 ) * channels );
}
static void new_instrument( const char* name, int mod_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
    if( !ins ) return;
    ins->name[ 0 ] = 0;
    if( name )
    {
	for( int i = 0; i < 22; i++ )
	{
	    ins->name[ i ] = name[ i ];
	    if( name[ i ] == 0 ) break;
	}
    }
    ins->volume_points_num_old = 0;
    ins->panning_points_num_old = 0;
    ins->volume_old = 64;
    ins->sign = INS_SIGN;
    ins->version = INS_VERSION;
    make_default_envelopes( mod_num, pnet );
    pnet->change_counter++;
}
static void reset_sample_pars( int length, sample* smp )
{
    smp->length = length;
    smp->volume = 0x40;
    smp->panning = 0x80;
    smp->relative_note = 0;
    smp->finetune = 0;
    smp->reppnt = 0;
    smp->replen = 0;
}
static void* create_raw_instrument_or_sample( 
    const char* name, 
    int mod_num, 
    uint data_bytes, 
    int bits, 
    int channels, 
    int freq,
    psynth_net* net, 
    int sample_num )
{
    psynth_module* mod = NULL;
    MODULE_DATA* module_data = NULL;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        module_data = (MODULE_DATA*)mod->data_ptr;
    }
    else return NULL;
    net->change_counter++;
    if( sample_num < 0 )
    {
	remove_instrument_and_samples( mod_num, net );
    }
    if( sample_num < 0 ) psynth_new_chunk( mod_num, CHUNK_INS, sizeof( instrument ), 0, 0, net );
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, net );
    if( !ins ) return NULL;
    if( sample_num < 0 )
    {
	new_instrument( name, mod_num, net );
	refresh_envelopes( module_data, 0xFF );
    }
    int new_sample_num = 0;
    if( sample_num >= 0 ) new_sample_num = sample_num;
    sample* smp;
    void* smp_data;
    psynth_new_chunk( mod_num, CHUNK_SMP( new_sample_num ), sizeof( sample ), 0, 0, net );
    smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( new_sample_num ), net );
    reset_sample_pars( data_bytes, smp );
    if( smp )
    {
	uint chunk_flags = 0;
	if( bits == 32 ) chunk_flags |= PS_CHUNK_SMP_FLOAT32;
	if( bits == 16 ) chunk_flags |= PS_CHUNK_SMP_INT16;
	if( bits == 8 ) chunk_flags |= PS_CHUNK_SMP_INT8;
	chunk_flags |= ( channels - 1 ) << PS_CHUNK_SMP_CH_OFFSET;
	psynth_new_chunk( mod_num, CHUNK_SMP_DATA( new_sample_num ), data_bytes, chunk_flags, freq, net );
	smp_data = psynth_get_chunk_data( mod_num, CHUNK_SMP_DATA( new_sample_num ), net );
        if( smp_data )
	{
	    smp->type = 0;
	    if( bits == 16 ) smp->type = 1 << 4;
	    if( bits == 32 ) smp->type = 2 << 4;
	    if( channels == 2 ) smp->type |= 64;
	    set_base_note( 5 * 12, new_sample_num, mod_num, module_data, net );
	    bytes2frames( smp );
    	    ins->samples_num = 0;
    	    for( int ss = 0; ss < MAX_SAMPLES; ss++ )
    	    {
    		if( psynth_get_chunk_data( mod_num, CHUNK_SMP( ss ), net ) )
		    ins->samples_num = ss + 1;
	    }
	    recalc_base_pitch( new_sample_num, mod_num, module_data, net );
	    return smp_data;
	}
    }
    return NULL;
}
static int load_audio_file(
    sfs_file f,
    const char* name,
    int mod_num,
    psynth_net* net,
    int sample_num )
{
    int rv = -1;
    sfs_file_fmt fmt = sfs_get_file_format( nullptr, f );
    bool can_read = false;
    switch( fmt )
    {
	case SFS_FILE_FMT_OGG: can_read = true; break;
	case SFS_FILE_FMT_MP3: can_read = true; break;
	case SFS_FILE_FMT_FLAC: can_read = true; break;
	case SFS_FILE_FMT_WAVE: can_read = true; break;
	case SFS_FILE_FMT_AIFF: can_read = true; break;
	default: break;
    }
    if( !can_read ) return -1;
    net->change_counter++;
    while( 1 )
    {
	sfs_rewind( f );
	sfs_sound_decoder_data d = sfs_sound_decoder_data();
	sundog_engine* sd = nullptr; GET_SD_FROM_PSYNTH_NET( net, sd );
	int rv2 = sfs_sound_decoder_init( sd, nullptr, f, fmt, SFS_SDEC_CONVERT_INT24_TO_FLOAT32 | SFS_SDEC_CONVERT_INT32_TO_FLOAT32 | SFS_SDEC_CONVERT_FLOAT64_TO_FLOAT32, &d );
	if( rv2 )
	{
    	    slog( "sfs_sound_decoder_init() error %d\n", rv2 );
            break;
        }
	int bits = 8;
	bool use_tmp_buf = false;
        switch( d.sample_format2 )
        {
            case SFMT_INT8: bits = 8; break;
            case SFMT_INT16: bits = 16; break;
            case SFMT_FLOAT32: bits = 32; break;
            default: break;
        }
        int new_frame_size = d.channels * ( bits / 8 );
	void* smp_data = create_raw_instrument_or_sample( name, mod_num, d.len * new_frame_size, bits, d.channels, d.rate, net, sample_num );
	if( smp_data )
	{
            sfs_sound_decoder_read2( &d, smp_data, d.len );
            rv = 0;
        }
	if( rv == 0 )
	{
	    set_loop( sample_num, d.loop_start, d.loop_len, d.loop_type, mod_num, net );
	}
        sfs_sound_decoder_deinit( &d );
	break;
    }
    return rv;
}
static void save_audio_file(
    const char* filename,
    int mod_num,
    sfs_file_fmt file_format,
    int q, 
    psynth_net* net,
    int sample_num )
{
    psynth_module* mod = NULL;
    MODULE_DATA* module_data = NULL;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        module_data = (MODULE_DATA*)mod->data_ptr;
    }
    else return;
#ifdef SUNVOX_GUI
    if( module_data->wm )
	show_status_message( sv_get_string( STR_SV_SAVING ), 1000, module_data->wm );
#endif
    instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, net );
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( sample_num ), net );
    void* smp_data = psynth_get_chunk_data( mod_num, CHUNK_SMP_DATA( sample_num ), net );
    while( ins && smp && smp_data )
    {
	uint flags = 0;
	int freq = 0;
	size_t sdata_size = 0;
	if( psynth_get_chunk_info( mod_num, CHUNK_SMP_DATA( sample_num ), net, &sdata_size, &flags, &freq ) )
	{
	    slog( "Can't get sample properties\n" );
	    return;
	}
	if( freq == 0 ) freq = 44100;
	sfs_sample_format sample_format = SFMT_INT8;
	int channels = ( ( flags & PS_CHUNK_SMP_CH_MASK ) >> PS_CHUNK_SMP_CH_OFFSET ) + 1;
	if( ( flags & PS_CHUNK_SMP_TYPE_MASK ) == PS_CHUNK_SMP_INT8 ) sample_format = SFMT_INT8;
	if( ( flags & PS_CHUNK_SMP_TYPE_MASK ) == PS_CHUNK_SMP_INT16 ) sample_format = SFMT_INT16;
	if( ( flags & PS_CHUNK_SMP_TYPE_MASK ) == PS_CHUNK_SMP_FLOAT32 ) sample_format = SFMT_FLOAT32;
	size_t len = sdata_size / ( g_sfs_sample_format_sizes[ sample_format ] * channels ); 
	sfs_sound_encoder_data e = sfs_sound_encoder_data();
	sundog_engine* sd = nullptr; GET_SD_FROM_PSYNTH_NET( net, sd );
        bool with_loop_info = smp->replen && ( smp->type & 3 ) != 0;
	if( with_loop_info )
	{
	    e.loop_type = smp->type & 3;
	    e.loop_start = smp->reppnt;
	    e.loop_len = smp->replen;
	}
	e.compression_level = q;
	int rv2 = sfs_sound_encoder_init(
	    sd,
	    filename, 0,
	    file_format,
	    sample_format,
	    freq, 
	    channels,
	    len, 
	    0,
	    &e );
	if( rv2 )
	{
    	    slog( "sfs_sound_encoder_init() error %d\n", rv2 );
            break;
        }
        sfs_sound_encoder_write( &e, smp_data, len );
	sfs_sound_encoder_deinit( &e );
	break;
    }
#ifdef SUNVOX_GUI
    if( module_data->wm )
	hide_status_message( module_data->wm );
#endif
}
static void load_xi_sample_data( sfs_file f, int mod_num, MODULE_DATA* module_data, psynth_net* net, sample* src_smp, int dest_slot, bool psytexx_ext )
{
    net->change_counter++;
    psynth_new_chunk( mod_num, CHUNK_SMP( dest_slot ), sizeof( sample ), 0, 0, net );
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( dest_slot ), net );
    if( !smp ) return;
    smem_copy( smp, src_smp, sizeof( sample ) );
    int xi_smp_type = smp->type;
    int xi_smp_type0 = smp->type & 15;
    int xi_smp_type1 = ( smp->type >> 4 ) & 15;
    int smp_type = 0; 
    int smp_stereo = 0; 
    if( psytexx_ext )
    {
	if( ( xi_smp_type1 & 3 ) == 1 ) smp_type = 1; 
	if( ( xi_smp_type1 & 3 ) == 2 ) smp_type = 2; 
	if( xi_smp_type1 & 4 ) smp_stereo = 1;
    }
    else
    {
	if( xi_smp_type1 & 1 ) smp_type = 1; 
	if( xi_smp_type1 & 2 ) smp_stereo = 2;
    }
    smp->type &= 0x0F;
    if( smp_type ) smp->type |= smp_type << 4;
    if( smp_stereo ) smp->type |= 0x40;
    if( smp->length )
    {
	uint chunk_flags = 0;
	if( smp_type == 0 ) chunk_flags |= PS_CHUNK_SMP_INT8;
	if( smp_type == 1 ) chunk_flags |= PS_CHUNK_SMP_INT16;
	if( smp_type == 2 ) chunk_flags |= PS_CHUNK_SMP_FLOAT32;
	if( smp_stereo ) chunk_flags |= PS_CHUNK_SMP_STEREO;
	psynth_new_chunk( mod_num, CHUNK_SMP_DATA( dest_slot ), smp->length, chunk_flags, 44100, net );
	recalc_base_pitch( dest_slot, mod_num, module_data, net );
    }
    int8_t* smp_data = (int8_t*)psynth_get_chunk_data( mod_num, CHUNK_SMP_DATA( dest_slot ), net );
    if( !smp_data ) return;
    sfs_read( smp_data, smp->length, 1, f ); 
    if( smp_type == 0 )
    {
        int8_t c_old_s = 0;
	int8_t* cs_data = (int8_t*) smp_data;
	int8_t c_new_s;
	if( smp_stereo == 2 )
	{
	    int8_t* new_data = (int8_t*)smem_new( smp->length );
	    int new_p = 0;
	    for( uint sp = 0; sp < smp->length / 2; sp++ )
	    {
		c_new_s = cs_data[ sp ] + c_old_s;
		new_data[ new_p ] = c_new_s;
		c_old_s = c_new_s;
		new_p += 2;
	    }
    	    c_old_s = 0;
	    new_p = 1;
	    for( uint sp = smp->length / 2; sp < smp->length; sp++ )
	    {
		c_new_s = cs_data[ sp ] + c_old_s;
		new_data[ new_p ] = c_new_s;
		c_old_s = c_new_s;
		new_p += 2;
	    }
	    psynth_replace_chunk_data( mod_num, CHUNK_SMP_DATA( dest_slot ), new_data, net );
	}
	else
	{
	    for( uint sp = 0; sp < smp->length; sp++ )
	    {
		c_new_s = cs_data[ sp ] + c_old_s;
		cs_data[ sp ] = c_new_s;
		c_old_s = c_new_s;
	    }
	}
    }
    if( smp_type == 1 )
    {
        int16_t old_s = 0;
        int16_t* s_data = (int16_t*) smp_data;
	int16_t new_s;
	if( smp_stereo == 2 )
	{
	    int16_t* new_data = (int16_t*)smem_new( smp->length );
	    int new_p = 0;
	    for( uint sp = 0; sp < smp->length / 4; sp++ )
	    {
		new_s = s_data[ sp ] + old_s;
		new_data[ new_p ] = new_s;
		old_s = new_s;
		new_p += 2;
	    }
    	    old_s = 0;
	    new_p = 1;
	    for( uint sp = smp->length / 4; sp < smp->length / 2; sp++ )
	    {
		new_s = s_data[ sp ] + old_s;
		new_data[ new_p ] = new_s;
		old_s = new_s;
		new_p += 2;
	    }
	    psynth_replace_chunk_data( mod_num, CHUNK_SMP_DATA( dest_slot ), new_data, net );
	}
	else
	{
	    for( uint sp = 0; sp < smp->length / 2; sp++ )
	    {
		new_s = s_data[ sp ] + old_s;
		s_data[ sp ] = new_s;
		old_s = new_s;
	    }
	}
    }
    if( smp_type || smp_stereo )
    {
        bytes2frames( smp );
    }
}
static int load_xi_instrument(
    sfs_file f, 
    uint flags, 
    int mod_num, 
    psynth_net* net,
    int sample_num )
{
    psynth_module* mod = NULL;
    MODULE_DATA* module_data = NULL;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        module_data = (MODULE_DATA*)mod->data_ptr;
    }
    net->change_counter++;
    instrument* ins = NULL;
    bool ins_need_free = false;
    if( sample_num < 0 )
    {
	remove_instrument_and_samples( mod_num, net );
	psynth_new_chunk( mod_num, CHUNK_INS, sizeof( instrument ), 0, 0, net );
	ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, net );
    }
    else
    {
	ins = (instrument*)smem_znew( sizeof( instrument ) );
	ins_need_free = true;
    }
    if( !ins ) return 1;
    bool empty_names = false;
    bool psytexx_ext = false; 
    char name[ 32 ];
    int8_t temp[ 32 ];
    if( f )
    {
	sfs_read( name, 21 - 12, 1, f ); 
	sfs_read( name, 22, 1, f ); 
	name[ 22 ] = 0;
	if( empty_names ) smem_clear_struct( name );
	sfs_read( temp, 23, 1, f );
	temp[ 20 ] = 0;
	if( strstr( (char*)temp, "amplicity" ) ) psytexx_ext = true;
	int v1 = temp[ 21 ];
	int v2 = temp[ 22 ];
	if( v1 == 0x50 && v2 == 0x50 ) psytexx_ext = true;
	if( sample_num < 0 ) new_instrument( name, mod_num, net );
        sfs_read( ins->smp_num_old, 208, 1, f ); 
	if( psytexx_ext )
	{
	    sfs_read( &ins->volume_old, 2, 1, f ); 
	    sfs_read( temp, 3, 1, f ); 
	    ins->relative_note = temp[ 1 ];
	    sfs_read( temp, 22 - ( 2 + 3 ), 1, f );
	}
	else
    	{
	    sfs_read( temp, 22, 1, f );
	    ins->finetune = 0;
	    ins->relative_note = 0;
	}
	sfs_read( &ins->samples_num, 2, 1, f ); 
	ins->volume_old = 64;
	smem_copy( ins->smp_num, ins->smp_num_old, 96 );
	memset( ins->smp_num + 96, ins->smp_num_old[ 95 ], 128 - 96 );
	if( sample_num < 0 )
	{
	    convert_old_envelopes_to_new( mod_num, net );
	    refresh_envelopes( module_data, 0xFF );
	}
	sample* smps = (sample*)smem_znew( sizeof( sample ) * ins->samples_num );
	if( smps )
	{
	    for( int s = 0; s < ins->samples_num; s++ )
	    {
		sample* smp = &smps[ s ];
		sfs_read( smp, 40, 1, f ); 
		if( empty_names ) smem_clear_struct( smp->name );
		if( flags & LOAD_XI_FLAG_SET_MAX_VOLUME ) smp->volume = 64;
	    }
	    if( sample_num < 0 )
	    {
		for( int s = 0; s < ins->samples_num; s++ )
		    load_xi_sample_data( f, mod_num, module_data, net, &smps[ s ], s, psytexx_ext );
	    }
	    else
	    {
		for( int s = 0; s < ins->samples_num; s++ )
		{
		    sample* smp = &smps[ s ];
		    if( smp->length )
		    {
			load_xi_sample_data( f, mod_num, module_data, net, smp, sample_num, psytexx_ext );
			break;
		    }
		}
	    }
	    smem_free( smps );
	}
	if( sample_num >= 0 )
	{
	    instrument* ins2 = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, net );
    	    ins2->samples_num = 0;
    	    for( int s = 0; s < MAX_SAMPLES; s++ )
    	    {
    		if( psynth_get_chunk_data( mod_num, CHUNK_SMP( s ), net ) )
		    ins2->samples_num = s + 1;
	    }
	}
    } 
    if( ins_need_free )
	smem_free( ins );
    return 0;
}
enum
{
    EXT_SMP_CONV_UNKNOWN = 0,
    EXT_SMP_CONV_NONE,
    EXT_SMP_CONV_FFMPEG,
    EXT_SMP_CONV_AVCONV
};
int g_external_sample_converter = EXT_SMP_CONV_UNKNOWN;
static int load_instrument_or_sample( 
    const char* filename, 
    sfs_file f,
    uint flags, 
    int mod_num, 
    psynth_net* net, 
    int sample_num )
{
    psynth_module* mod = NULL;
    MODULE_DATA* module_data = NULL;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        module_data = (MODULE_DATA*)mod->data_ptr;
    }
#ifdef SUNVOX_GUI
    if( module_data->wm )
	show_status_message( sv_get_string( STR_SV_LOADING ), 1000, module_data->wm );
#endif
    int instr_created = 0;
    int8_t temp[ 8 ];
    int fn = 0;
    if( filename )
    {
	for( fn = 0; ; fn++ ) if( filename[ fn ] == 0 ) break;
	for( ; fn >= 0; fn-- ) if( filename[ fn ] == '/' ) { fn++; break; }
    }
    size_t file_size = 0;
    bool need_close = false;
    if( f == 0 )
    {
	file_size = sfs_get_file_size( filename );
	f = sfs_open( filename, "rb" );
	need_close = true;
    }
    else
    {
	size_t file_begin = 0;
	sfs_rewind( f );
	sfs_seek( f, 0, SFS_SEEK_END );
        file_size = sfs_tell( f ) - file_begin;
	sfs_rewind( f );
    }
    if( f )
    {
	sfs_read( temp, 4, 1, f );
	if( temp[ 0 ] == 'E' && temp[ 1 ] == 'x' && temp[ 2 ] == 't' )
	{
	    sfs_read( temp, 8, 1, f );
	    if( temp[ 5 ] == 'I' )
	    {
		if( load_xi_instrument( f, flags, mod_num, net, sample_num ) == 0 )
		{
		    instr_created = 1;
		}
	    }
	}
	if( !instr_created )
	{
	    if( load_audio_file( f, filename + fn, mod_num, net, sample_num ) == 0 )
	    {
		instr_created = 1;
	    }
	}
	if( need_close ) sfs_close( f );
	if( !instr_created && filename )
	{
#if !defined(OS_IOS) && !defined(OS_ANDROID) && !defined(OS_WINCE)
	    char* src_name = sfs_make_filename( NULL, filename, true );
	    char* dest_name = sfs_make_filename( NULL, "3:/sunvox_temp.wav", true );
	    char* log_name = sfs_make_filename( NULL, "3:/ffmpeg_log.txt", true );
	    char* cmd = (char*)smem_new( smem_strlen( src_name ) + smem_strlen( dest_name ) + 1024 );
	    sfs_remove_file( dest_name );
	    if( g_external_sample_converter == EXT_SMP_CONV_UNKNOWN )
	    {
		int r;
		while( 1 )
		{
#ifdef OS_UNIX
		    sprintf( cmd, "ffmpeg -version >\"%s\"", log_name );
#else
		    sprintf( cmd, "ffmpeg -version" );
#endif
		    r = system( cmd );
		    if( r == 0 ) { g_external_sample_converter = EXT_SMP_CONV_FFMPEG; slog( "FFMPEG found\n" ); break; }
#ifdef OS_UNIX
		    sprintf( cmd, "avconv -version >\"%s\"", log_name );
#else
		    sprintf( cmd, "avconv -version" );
#endif
		    r = system( cmd ); if( r == 0 ) { g_external_sample_converter = EXT_SMP_CONV_AVCONV; slog( "AVCONV found\n" ); break; }
		    g_external_sample_converter = EXT_SMP_CONV_NONE;
		    break;
		}
	    }
	    if( g_external_sample_converter > EXT_SMP_CONV_NONE )
	    {
		if( src_name && dest_name )
		{
		    if( g_external_sample_converter == EXT_SMP_CONV_FFMPEG )
		    {
			sprintf( cmd, "ffmpeg -loglevel quiet -i \"%s\" -acodec pcm_s16le \"%s\"", src_name, dest_name );
		    }
		    if( g_external_sample_converter == EXT_SMP_CONV_AVCONV )
		    {
			sprintf( cmd, "avconv -loglevel quiet -i \"%s\" -c:a pcm_s16le \"%s\"", src_name, dest_name );
		    }
		    system( cmd );
		    size_t file_size = sfs_get_file_size( dest_name );
		    if( file_size )
		    {
			f = sfs_open( dest_name, "rb" );
			if( f )
			{
			    if( load_audio_file( f, filename + fn, mod_num, net, sample_num ) == 0 )
			    {
				instr_created = 1;
			    }
			    sfs_close( f );
			    sfs_remove_file( dest_name );
			}
		    }
		}
	    }
	    smem_free( src_name );
	    smem_free( dest_name );
	    smem_free( log_name );
	    smem_free( cmd );
#endif
	}
    }
#ifdef SUNVOX_GUI
    if( module_data->wm )
	hide_status_message( module_data->wm );
#endif
    if( !instr_created )
    {
	return 1;
    }
    return 0;
}
int sampler_load( const char* filename, sfs_file f, int mod_num, psynth_net* net, int sample_num, bool load_unsupported_files_as_raw )
{
    int rv = -1;
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
        mod = &net->mods[ mod_num ];
        data = (MODULE_DATA*)mod->data_ptr;
    }
    else
    {
        return -1;
    }
    int mutex_res = smutex_lock( psynth_get_mutex( mod_num, net ) );
    if( mutex_res != 0 )
    {
        slog( "sampler_load: mutex lock error %d\n", mutex_res );
        return -1;
    }
    if( sample_num >= 0 )
    {
        psynth_remove_chunk( mod_num, CHUNK_SMP( sample_num ), net );
        psynth_remove_chunk( mod_num, CHUNK_SMP_DATA( sample_num ), net );
    }
    int rv2 = load_instrument_or_sample( filename, f, LOAD_XI_FLAG_SET_MAX_VOLUME, mod_num, net, sample_num );
    if( rv2 == 0 )
    {
	rv = 0;
    }
    else
    {
        if( load_unsupported_files_as_raw )
        {
#ifdef SUNVOX_GUI
            if( load_raw_instrument_or_sample( filename, mod_num, net, data->wm, sample_num ) == -2 ) return -1;
            rv = 0;
#endif
        }
    }
    for( int c = 0; c < data->ctl_channels; c++ ) data->channels[ c ].flags &= ~GEN_CHANNEL_FLAG_PLAYING;
    smutex_unlock( psynth_get_mutex( mod_num, net ) );
#ifdef SUNVOX_GUI
    if( mod->visual )
    {
	sampler_visual_data* smp_data = (sampler_visual_data*)mod->visual->data;
	if( smp_data )
	{
	    sample_editor_data_r* smp_ed_r = get_sample_editor_data_r( smp_data->editor_window );
	    if( smp_ed_r )
	    {
    		invalidate_smp_peaks( smp_ed_r );
	    }
	}
    }
#endif
    return rv;
}
int sampler_par( psynth_net* net, int mod_num, int smp_num, int par_num, int par_val, int set )
{
    if( mod_num < 0 ) return 0;
    if( smp_num < 0 ) return 0;
    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( smp_num ), net );
    if( !smp ) return 0; 
    int prev_val = 0;
    switch( par_num )
    {
	case 0:
	    prev_val = smp->reppnt;
	    if( set )
	    {
		if( par_val >= 0 && par_val < (int)smp->length && par_val + (int)smp->replen <= (int)smp->length )
		    smp->reppnt = par_val;
	    }
	    break;
	case 1:
	    prev_val = smp->replen;
	    if( set )
	    {
		if( par_val >= 0 && smp->reppnt + par_val <= smp->length )
		    smp->replen = par_val;
	    }
	    break;
	case 2:
	    prev_val = smp->type & 3;
	    if( set )
	    {
		smp->type = ( smp->type & ~3 ) | ( par_val & 3 );
	    }
	    break;
	case 3:
	    prev_val = 0;
	    if( smp->type & SAMPLE_TYPE_FLAG_LOOPRELEASE )
		prev_val = 1;
	    if( set )
	    {
		smp->type = ( smp->type & ~SAMPLE_TYPE_FLAG_LOOPRELEASE ) | ( ( par_val & 1 ) << 2 );
	    }
	    break;
	case 4:
	    prev_val = smp->volume;
	    if( set )
	    {
		if( par_val >= 0 && par_val <= 64 )
		    smp->volume = par_val;
	    }
	    break;
	case 5:
	    prev_val = smp->panning;
	    if( set )
	    {
		if( par_val >= 0 && par_val <= 255 )
		    smp->panning = par_val;
	    }
	    break;
	case 6:
	    prev_val = smp->finetune;
	    if( set )
	    {
		if( par_val >= -128 && par_val <= 127 )
		    smp->finetune = par_val;
	    }
	    break;
	case 7:
	    prev_val = smp->relative_note;
	    if( set )
	    {
		if( par_val >= -128 && par_val <= 127 )
		    smp->relative_note = par_val;
	    }
	    break;
	case 8:
	    prev_val = smp->start_pos;
	    if( set )
	    {
		if( par_val >= 0 && par_val < (int)smp->length )
		    smp->start_pos = par_val;
	    }
	    break;
	default: break;
    }
    net->change_counter++;
    return prev_val;
}
static inline uint sampler_render( 
    gen_channel* chan, 
    instrument* ins, 
    sample* smp, 
    void* smp_data,
    int ctl_smp_int, 
    PS_STYPE** outputs, 
    int outputs_num, 
    int frames )
{
    PS_STYPE* RESTRICT out0 = NULL;
    PS_STYPE* RESTRICT out1 = NULL;
    out0 = outputs[ 0 ];
    if( outputs_num > 1 )
	out1 = outputs[ 1 ];
    SMPPTR reppnt = smp->reppnt;
    SMPPTR replen = smp->replen;
    SMPPTR repend = reppnt + replen;
    PS_STYPE2 s0; 
    PS_STYPE2 s1; 
    int8_t* smp8 = (int8_t*) smp_data;
    int16_t* smp16 = (int16_t*) smp_data;
    float* smp32f = (float*) smp_data;
    uint8_t smp_type = smp->type;
    uint8_t loop_type = smp->type & 3;
    uint8_t smp_bits = ( smp_type >> 4 ) & 3;
    SMPPTR smp_len = smp->length;
    bool smp_stereo = ( smp_type & SAMPLE_TYPE_FLAG_STEREO ) != 0;
    if( loop_type == 0 )
    {
	replen = 0;
    }
    else
    {
	if( ( chan->flags & GEN_CHANNEL_FLAG_SUSTAIN ) == 0 )
	{
	    if( smp_type & SAMPLE_TYPE_FLAG_LOOPRELEASE )
	    {
		chan->flags &= ~GEN_CHANNEL_FLAG_BACK;
		replen = 0;
	    }
	}
    }
    int i = 0;
    for( ; i < frames; i++ )
    {
	if( replen )
	{
	    if( chan->ptr_h >= repend )
	    {
		if( loop_type == 1 ) 
		{
		    while( 1 )
		    {
			chan->ptr_h -= replen;
			if( chan->ptr_h < repend ) break;
		    }
		}
		else
		{
		    SMPPTR rep_part = ( chan->ptr_h - reppnt ) / replen; 
		    if( rep_part & 1 )
		    {
			chan->flags |= GEN_CHANNEL_FLAG_BACK;
			SMPPTR temp_ptr_h = chan->ptr_h;
			int temp_ptr_l = chan->ptr_l;
			chan->ptr_h = reppnt + replen * ( rep_part + 1 );
			chan->ptr_l = 0;
			PSYNTH_FP64_SUB( chan->ptr_h, chan->ptr_l, temp_ptr_h, temp_ptr_l );
			chan->ptr_h += reppnt;
			if( chan->ptr_h == repend && chan->ptr_h > 0 ) 
			{
			    chan->ptr_h = repend - 1;
			    chan->ptr_l = ( 1 << PSYNTH_FP64_PREC ) - 1;
			}
		    }
		    else
		    {
			chan->flags &= ~GEN_CHANNEL_FLAG_BACK;
			chan->ptr_h -= replen * rep_part;
		    }
		}
	    }
	    if( ( chan->flags & GEN_CHANNEL_FLAG_BACK ) && chan->ptr_h < reppnt )
	    {
		SMPPTR temp_ptr_h2 = chan->ptr_h;
		int temp_ptr_l2 = chan->ptr_l;
		chan->ptr_h = reppnt;
		chan->ptr_l = 0;
		PSYNTH_FP64_SUB( chan->ptr_h, chan->ptr_l, temp_ptr_h2, temp_ptr_l2 );
		SMPPTR rep_part = chan->ptr_h / replen;
		chan->ptr_h += reppnt;
		if( rep_part & 1 )
		{
		    chan->flags |= GEN_CHANNEL_FLAG_BACK;
		    SMPPTR temp_ptr_h = chan->ptr_h;
		    int temp_ptr_l = chan->ptr_l;
		    chan->ptr_h = reppnt + replen * ( rep_part + 1 );
		    chan->ptr_l = 0;
		    PSYNTH_FP64_SUB( chan->ptr_h, chan->ptr_l, temp_ptr_h, temp_ptr_l );
		    chan->ptr_h += reppnt;
		}
		else
		{
		    chan->flags &= ~GEN_CHANNEL_FLAG_BACK;
		    chan->ptr_h -= replen * rep_part;
		}
	    }
	} 
	SMPPTR s_offset = chan->ptr_h;
	if( (unsigned)s_offset >= (unsigned)smp_len )
	{
	    chan->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
	    chan->id = ~0;
	    break;
	}
        if( ctl_smp_int )
        {
	    SMPPTR s_offset0;
	    SMPPTR s_offset2;
	    SMPPTR s_offset3;
	    s_offset2 = s_offset + 1;
#ifdef PS_STYPE_FLOATINGPOINT
	    if( ctl_smp_int == 2 )
	    {
	        s_offset3 = s_offset + 2;
	        s_offset0 = s_offset - 1;
	        if( replen )
	        {
		    if( loop_type == 1 )
		    {
		        if( ( chan->flags & GEN_CHANNEL_FLAG_LOOP ) && s_offset0 < reppnt ) s_offset0 = repend - 1;
		        if( s_offset2 >= repend ) { s_offset2 -= replen; }
		        if( s_offset3 >= repend ) { s_offset3 -= replen; chan->flags |= GEN_CHANNEL_FLAG_LOOP; if( s_offset3 >= smp_len ) s_offset3 = smp_len - 1; }
		    }
		    else 
		    {
		        if( ( chan->flags & GEN_CHANNEL_FLAG_LOOP ) && s_offset0 < reppnt ) s_offset0 = reppnt;
		        if( s_offset2 >= repend ) { s_offset2 = ( repend - 1 ) - ( s_offset2 - repend ); }
		        if( s_offset3 >= repend ) { s_offset3 = ( repend - 1 ) - ( s_offset3 - repend ); chan->flags |= GEN_CHANNEL_FLAG_LOOP; if( s_offset3 >= smp_len ) s_offset3 = smp_len - 1; }
		        if( s_offset3 < 0 ) s_offset3 = 0;
		    }
		}
		else
		{
		    if( s_offset2 >= smp_len ) s_offset2 = smp_len - 1;
		    if( s_offset3 >= smp_len ) s_offset3 = smp_len - 1;
		}
		if( s_offset0 < 0 ) s_offset0 = 0;
	    }
	    else
#endif
	    {
	        if( replen )
	        {
		    if( loop_type == 1 )
		    {
		        if( s_offset2 >= repend ) s_offset2 = reppnt;
		    }
		    else
		    {
		        if( s_offset2 >= repend ) s_offset2 = s_offset;
	    	    }
		}
		else
		{
		    if( s_offset2 >= smp_len ) s_offset2 = smp_len - 1;
		}
	    }
	    switch( smp_bits )
	    {
		case 0:
		    if( smp_stereo )
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp8[ s_offset0 << 1 ];
			    PS_STYPE2 y1 = smp8[ s_offset << 1 ];
			    PS_STYPE2 y2 = smp8[ s_offset2 << 1 ];
			    PS_STYPE2 y3 = smp8[ s_offset3 << 1 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s0 /= 128.0F;
			    y0 = smp8[ ( s_offset0 << 1 ) + 1 ];
			    y1 = smp8[ ( s_offset << 1 ) + 1 ];
			    y2 = smp8[ ( s_offset2 << 1 ) + 1 ];
			    y3 = smp8[ ( s_offset3 << 1 ) + 1 ];
			    a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    c2 = ( y2 - y0 ) / 2;
			    s1 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s1 /= 128.0F;
			}
			else
#endif
			{
			    int iv1;
			    int iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    SMPPTR s_xoffset = s_offset << 1;
			    SMPPTR s_xoffset2 = s_offset2 << 1;
			    iv1 = smp8[ s_xoffset ]; iv1 <<= 8; iv1 *= intr2; 
			    iv2 = smp8[ s_xoffset2 ]; iv2 <<= 8; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s0, iv1 );
			    iv1 = smp8[ s_xoffset + 1 ]; iv1 <<= 8; iv1 *= intr2; 
			    iv2 = smp8[ s_xoffset2 + 1 ]; iv2 <<= 8; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s1, iv1 );
			}
		    }
		    else
		    { 
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp8[ s_offset0 ];
			    PS_STYPE2 y1 = smp8[ s_offset ];
			    PS_STYPE2 y2 = smp8[ s_offset2 ];
			    PS_STYPE2 y3 = smp8[ s_offset3 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s0 /= 128.0F;
			}
			else
#endif
			{
			    int iv1;
			    int iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    iv1 = smp8[ s_offset ]; iv1 <<= 8; iv1 *= intr2;
			    iv2 = smp8[ s_offset2 ]; iv2 <<= 8; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s0, iv1 );
			}
		    }
		    break;
		case 1:
		    if( smp_stereo )
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp16[ s_offset0 << 1 ];
			    PS_STYPE2 y1 = smp16[ s_offset << 1 ];
			    PS_STYPE2 y2 = smp16[ s_offset2 << 1 ];
			    PS_STYPE2 y3 = smp16[ s_offset3 << 1 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s0 /= 32768.0F;
			    y0 = smp16[ ( s_offset0 << 1 ) + 1 ];
			    y1 = smp16[ ( s_offset << 1 ) + 1 ];
			    y2 = smp16[ ( s_offset2 << 1 ) + 1 ];
			    y3 = smp16[ ( s_offset3 << 1 ) + 1 ];
			    a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    c2 = ( y2 - y0 ) / 2;
			    s1 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s1 /= 32768.0F;
			}
			else
#endif
			{
			    int iv1;
			    int iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    SMPPTR s_xoffset = s_offset << 1;
			    SMPPTR s_xoffset2 = s_offset2 << 1;
			    iv1 = smp16[ s_xoffset ]; iv1 *= intr2; 
			    iv2 = smp16[ s_xoffset2 ]; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s0, iv1 );
			    iv1 = smp16[ s_xoffset + 1 ]; iv1 *= intr2; 
			    iv2 = smp16[ s_xoffset2 + 1 ]; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s1, iv1 );
			}
		    }
		    else
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp16[ s_offset0 ];
			    PS_STYPE2 y1 = smp16[ s_offset ];
			    PS_STYPE2 y2 = smp16[ s_offset2 ];
			    PS_STYPE2 y3 = smp16[ s_offset3 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    s0 /= 32768.0F;
			}
			else
#endif
			{
			    int iv1;
			    int iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    iv1 = smp16[ s_offset ]; iv1 *= intr2;
			    iv2 = smp16[ s_offset2 ]; iv2 *= intr;
			    iv1 += iv2; iv1 >>= INTERP_PREC;
			    PS_INT16_TO_STYPE( s0, iv1 );
			}
		    }
		    break;
		case 2:
		    if( smp_stereo )
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp32f[ s_offset0 << 1 ];
			    PS_STYPE2 y1 = smp32f[ s_offset << 1 ];
			    PS_STYPE2 y2 = smp32f[ s_offset2 << 1 ];
			    PS_STYPE2 y3 = smp32f[ s_offset3 << 1 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			    y0 = smp32f[ ( s_offset0 << 1 ) + 1 ];
			    y1 = smp32f[ ( s_offset << 1 ) + 1 ];
			    y2 = smp32f[ ( s_offset2 << 1 ) + 1 ];
			    y3 = smp32f[ ( s_offset3 << 1 ) + 1 ];
			    a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    c2 = ( y2 - y0 ) / 2;
			    s1 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			}
			else
#endif
			{
			    float iv1;
			    float iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    SMPPTR s_xoffset = s_offset << 1;
			    SMPPTR s_xoffset2 = s_offset2 << 1;
			    iv1 = smp32f[ s_xoffset ]; iv1 *= (float)intr2 / 32768.0F; 
			    iv2 = smp32f[ s_xoffset2 ]; iv2 *= (float)intr / 32768.0F;
			    iv1 += iv2;
			    s0 = iv1 * PS_STYPE_ONE;
			    iv1 = smp32f[ s_xoffset + 1 ]; iv1 *= (float)intr2 / 32768.0F; 
			    iv2 = smp32f[ s_xoffset2 + 1 ]; iv2 *= (float)intr / 32768.0F;
			    iv1 += iv2;
			    s1 = iv1 * PS_STYPE_ONE;
			}
		    }
		    else
		    {
#ifdef PS_STYPE_FLOATINGPOINT
			if( ctl_smp_int == 2 )
			{
			    PS_STYPE2 y0 = smp32f[ s_offset0 ];
			    PS_STYPE2 y1 = smp32f[ s_offset ];
			    PS_STYPE2 y2 = smp32f[ s_offset2 ];
			    PS_STYPE2 y3 = smp32f[ s_offset3 ];
			    PS_STYPE2 mu = (PS_STYPE2)chan->ptr_l / (PS_STYPE2)( 1 << PSYNTH_FP64_PREC );
			    PS_STYPE2 a = ( 3 * ( y1-y2 ) - y0 + y3 ) / 2;
			    PS_STYPE2 b = 2 * y2 + y0 - ( 5 * y1 + y3 ) / 2;
			    PS_STYPE2 c2 = ( y2 - y0 ) / 2;
			    s0 = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + y1;
			}
			else
#endif
			{
			    float iv1;
			    float iv2;
			    uint intr = chan->ptr_l >> ( PSYNTH_FP64_PREC - INTERP_PREC ); 
			    uint intr2 = ( 1 << INTERP_PREC ) - 1 - intr;
			    iv1 = smp32f[ s_offset ]; iv1 *= (float)intr2 / 32768.0F;
			    iv2 = smp32f[ s_offset2 ]; iv2 *= (float)intr / 32768.0F;
			    iv1 += iv2;
			    s0 = iv1 * PS_STYPE_ONE;
			}
		    }
		    break;
	    } 
	}
	else
	{
	    switch( smp_bits )
	    {
	        case 0:
		    if( smp_stereo )
		    {
			SMPPTR s_xoffset = s_offset << 1;
			PS_INT16_TO_STYPE( s0, (int)smp8[ s_xoffset ] << 8 );
			PS_INT16_TO_STYPE( s1, (int)smp8[ s_xoffset + 1 ] << 8 );
		    }
		    else
		    {
			PS_INT16_TO_STYPE( s0, (int)smp8[ s_offset ] << 8 );
		    }
		    break;
		case 1:
		    if( smp_stereo )
		    {
			SMPPTR s_xoffset = s_offset << 1;
			PS_INT16_TO_STYPE( s0, smp16[ s_xoffset ] );
			PS_INT16_TO_STYPE( s1, smp16[ s_xoffset + 1 ] );
		    }
		    else
		    {
			PS_INT16_TO_STYPE( s0, smp16[ s_offset ] );
		    }
    		    break;
		case 2:
		    if( smp_stereo )
		    {
			SMPPTR s_xoffset = s_offset << 1;
			s0 = smp32f[ s_xoffset ] * PS_STYPE_ONE; 
			s1 = smp32f[ s_xoffset + 1 ] * PS_STYPE_ONE;
		    }
		    else
		    {
			s0 = smp32f[ s_offset ] * PS_STYPE_ONE;
		    }
		    break;
	    }
	}
	out0[ i ] = s0;
	if( smp_stereo )
	{
	    if( out1 )
	    {
		out1[ i ] = s1;
	    }
	}
	if( chan->flags & GEN_CHANNEL_FLAG_BACK )
	{
	    PSYNTH_FP64_SUB( chan->ptr_h, chan->ptr_l, chan->delta_h, chan->delta_l )
	}
	else
	{
	    PSYNTH_FP64_ADD( chan->ptr_h, chan->ptr_l, chan->delta_h, chan->delta_l )
	}
    } 
    if( i > 0 )
    {
	if( outputs_num > 1 )
	{
	    if( smp_stereo == false )
	    {
		for( int c = 1; c < outputs_num; c++ )
		{
		    PS_STYPE* RESTRICT outx = outputs[ c ];
		    for( int i2 = 0; i2 < i; i2++ ) outx[ i2 ] = out0[ i2 ];
		}
	    }
	    else
	    {
		for( int c = 2; c < outputs_num; c++ )
		{
		    PS_STYPE* RESTRICT outx = outputs[ c ];
		    for( int i2 = 0; i2 < i; i2++ ) outx[ i2 ] = out1[ i2 ];
		}
	    }
	}
    }
    return i;
}
static inline void sampler_apply_volume_envelope( 
    gen_channel* chan, 
    int ctl_env_int, 
    PS_STYPE** bufs, 
    int bufs_num, 
    int frames )
{
    if( ctl_env_int && chan->vol_step && ( chan->l_delta || chan->r_delta ) )
    {
	int frames2 = frames;
	if( chan->vol_step < frames ) frames2 = chan->vol_step;
	for( int b = 0; b < bufs_num; b++ )
	{
	    PS_STYPE* RESTRICT buf = bufs[ b ];
	    int vol;
	    int delta;
	    if( b == 0 ) 
	    {
		vol = chan->l_cur;
		delta = chan->l_delta;
	    }
	    else
	    {
		vol = chan->r_cur;
		delta = chan->r_delta;
	    }
	    for( int i = 0; i < frames2; i++ )
	    {
		vol += delta;
#ifdef PS_STYPE_FLOATINGPOINT
		buf[ i ] = buf[ i ] * (PS_STYPE)( vol >> 12 ) * (PS_STYPE)( 1.0 / 32768.0 );
#else
		buf[ i ] = ( (PS_STYPE2)buf[ i ] * ( vol >> 12 ) ) >> 15;
#endif
	    }
	    for( int i = frames2; i < frames; i++ )
	    {
#ifdef PS_STYPE_FLOATINGPOINT
		buf[ i ] = buf[ i ] * (PS_STYPE)( vol >> 12 ) * (PS_STYPE)( 1.0 / 32768.0 );
#else
		buf[ i ] = ( (PS_STYPE2)buf[ i ] * ( vol >> 12 ) ) >> 15;
#endif
	    }
	    if( b == 0 ) 
		chan->l_cur = vol;
	    else
		chan->r_cur = vol;
	}
	chan->vol_step -= frames2;
    }
    else
    {
	for( int b = 0; b < bufs_num; b++ )
	{
	    PS_STYPE* RESTRICT buf = bufs[ b ];
	    PS_STYPE2 vol = chan->l_cur >> 12;
	    if( b > 0 ) vol = chan->r_cur >> 12;
	    if( vol == 32768 ) continue;
	    if( vol == 0 )
	    {
		for( int i = 0; i < frames; i++ ) buf[ i ] = 0;
	    }
	    else
	    {
#ifdef PS_STYPE_FLOATINGPOINT
		vol /= 32768.0F;
#endif
		for( int i = 0; i < frames; i++ )
		{
#ifdef PS_STYPE_FLOATINGPOINT
		    buf[ i ] *= vol;
#else
		    PS_STYPE2 v = buf[ i ];
		    v = ( v * vol ) >> 15;
		    buf[ i ] = v;
#endif
		}
	    }
	}
    }
}
static inline void sampler_anticlick_in(
    gen_channel* chan,
    int alen,
    PS_STYPE** bufs, 
    int bufs_num, 
    int frames,
    psynth_net* pnet )
{
    int vdelta = 32768 / alen;
    if( vdelta < 1 ) vdelta = 1;
    int cnt;
    for( int b = 0; b < bufs_num; b++ )
    {
        cnt = chan->anticlick_in_counter;
	PS_STYPE* buf = bufs[ b ];
	for( int i = 0; i < frames; i++ )
        {
    	    PS_STYPE2 s1 = chan->anticlick_in_s[ b ];
    	    PS_STYPE2 s2 = buf[ i ];
            s1 *= cnt;
            s2 *= 32768 - cnt;
            s1 /= 32768;
            s2 /= 32768;
            s1 += s2;
            buf[ i ] = s1;
            cnt -= vdelta;
            if( cnt < 0 ) 
            {
                cnt = 0;
                break;
            }
        }
    }
    chan->anticlick_in_counter = cnt;
}
static inline bool sampler_anticlick_out(
    gen_channel* chan,
    int alen,
    PS_STYPE** bufs, 
    int bufs_num, 
    int frames,
    psynth_net* pnet )
{
    bool finished = false;
    int vdelta = 32768 / alen;
    if( vdelta < 1 ) vdelta = 1;
    int cnt;
    for( int b = 0; b < bufs_num; b++ )
    {
        cnt = chan->anticlick_out_counter;
	PS_STYPE* buf = bufs[ b ];
	for( int i = 0; i < frames; i++ )
        {
    	    PS_STYPE2 s = buf[ i ];
            s *= cnt;
            s /= 32768;
            buf[ i ] = s;
            cnt -= vdelta;
            if( cnt < 0 ) 
            {
                cnt = 0;
            }
        }
    }
    chan->anticlick_out_counter = cnt;
    if( cnt == 0 )
    {
	finished = true;
	chan->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
	chan->id = ~0;
    }
    return finished;
}
static void sampler_calc_final_pitch( 
    gen_channel* chan, 
    MODULE_DATA* data,
    psynth_module* mod )
{
    psynth_net* pnet = mod->pnet;
    int new_freq;
    int new_pitch = chan->cur_pitch + chan->pitch_add1 + chan->pitch_add2;
    if( chan->final_pitch != new_pitch )
    {
	chan->final_pitch = new_pitch;
	if( data->opt->freq_accuracy )
	{
	    int base_pitch = data->sample_base_pitch[ chan->smp_num ];
	    int p = base_pitch - new_pitch;
	    if( p % ( 12 * 256 ) == 0 )
	    {
		int base_freq = data->sample_base_freq[ chan->smp_num ];
		if( p >= 0 )
		{
		    int offset = p / ( 12 * 256 );
		    new_freq = base_freq << offset;
		}
		else
		{
		    int offset = -p / ( 12 * 256 );
		    new_freq = base_freq >> offset;
		}
		PSYNTH_GET_DELTA_HQ( pnet->sampling_freq, new_freq, chan->delta_h, chan->delta_l );
		return;
	    }
	    PSYNTH_GET_FREQ( data->linear_freq_tab, new_freq, new_pitch / 4 );
	    PSYNTH_GET_DELTA_HQ( pnet->sampling_freq, new_freq, chan->delta_h, chan->delta_l );
	    return;
	}
	PSYNTH_GET_FREQ( data->linear_freq_tab, new_freq, new_pitch / 4 );
	PSYNTH_GET_DELTA( pnet->sampling_freq, new_freq, chan->delta_h, chan->delta_l );
    }
}
static inline void sampler_eff_clean( MODULE_DATA* data, uint ch )
{
    psynth_event evt;
    evt.command = PS_CMD_CLEAN;
    psynth_sunvox_handle_module_event( 1 + ch, &evt, data->ps );
}
static inline void sampler_eff_apply( MODULE_DATA* data, uint ch )
{
    psynth_event evt;
    evt.command = PS_CMD_APPLY_CONTROLLERS;
    psynth_sunvox_handle_module_event( 1 + ch, &evt, data->ps );
}
static inline void sampler_eff_ctl( MODULE_DATA* data, gen_channel* chan, uint ch, uint ctl_num, int val )
{
    psynth_event evt;
    evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
    evt.controller.ctl_num = ctl_num;
    psynth_module* mod = psynth_sunvox_get_module( data->ps );
    if( mod )
    {
	if( (unsigned)ctl_num < mod->ctls_num )
	{
	    psynth_ctl* ctl = &mod->ctls[ ctl_num ];
	    if( ctl->type == 0 )
	    {
		evt.controller.ctl_val = val;
	    }
	    else
	    {
		evt.controller.ctl_val = ( ( val * ( ctl->max - ctl->min ) ) >> 15 ) + ctl->min;
	    }
	    psynth_sunvox_handle_module_event( 1 + ch, &evt, data->ps );
	}
    }
    chan->prev_eff_ctl_val = val;
}
static inline void handle_fit_to_pattern(
    MODULE_DATA* data,
    psynth_module* mod,
    gen_channel* chan,
    PS_STYPE** bufs,
    int bufs_num,
    int buf_rendered_frames )
{
    if( chan->tick_counter2 == data->opt->fit_to_pattern * data->ticks_per_line )
    {
	chan->tick_counter2 = 0;
        sample* smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( chan->smp_num ) );
        int p = chan->ptr_h * 32 / smp->length;
        if( p == 0 || p == 31 )
        {
    	    chan->ptr_h = 0;
	    chan->ptr_l = 0;
	    if( buf_rendered_frames )
	    {
		for( int b = 0; b < bufs_num; b++ )
    		{
    		    PS_STYPE s = bufs[ b ][ buf_rendered_frames - 1 ];
        	    chan->anticlick_in_s[ b ] = s;
        	    if( s != 0 && chan->anticlick_in_counter == 0 )
        	    {
            		chan->anticlick_in_counter = 32768;
        	    }
    		}
    	    }
	}
    }
    chan->tick_counter2++;
}
static inline void sampler_tick( 
    gen_channel* chan,
    uint ch,
    instrument* ins,
    MODULE_DATA* data,
    psynth_module* mod )
{
    psynth_net* pnet = mod->pnet;
    bool first_tick = false;
    if( chan->flags & GEN_CHANNEL_FLAG_ENV_START ) 
    {
	first_tick = true;
	chan->flags &= ~GEN_CHANNEL_FLAG_ENV_START;
    }
    int l_vol, r_vol; 
    uint env_num = ENV_VOL;
    sampler_envelope* env = data->env[ env_num ];
    uint16_t* points = (uint16_t*)env->p;
    if( ( env->h.flags & ENV_FLAG_ENABLED ) && data->env_buf[ env_num ] ) 
    {
	if( env->h.flags & ENV_FLAG_LOOP ) 
	{ 
	    if( chan->env_pos[ env_num ] >= points[ env->h.loop_end << 1 ] )  
		chan->env_pos[ env_num ] = points[ env->h.loop_start << 1 ];
	}
	bool envelope_finished = false;
	if( chan->env_pos[ env_num ] > points[ ( env->h.point_count - 1 ) << 1 ] ) 
	{
	    chan->env_pos[ env_num ] = points[ ( env->h.point_count - 1 ) << 1 ];
	    envelope_finished = true;
	}
	r_vol = data->env_buf[ env_num ][ chan->env_pos[ env_num ] ];
	if( !( env->h.flags & ENV_FLAG_LOOP ) ) 
	{ 
	    if( envelope_finished && r_vol == 0 )
	    {
		chan->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
		chan->id = ~0;
	    }
	}
	if( ( chan->flags & GEN_CHANNEL_FLAG_SUSTAIN ) == 0 ) 
	{
	    if( chan->fadeout > 0 )
	    {
		chan->fadeout -= ins->volume_fadeout << 1;
		if( chan->fadeout < 0 ) chan->fadeout = 0;
	    }
	    else 
	    {
		chan->fadeout = 0;
		chan->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
		chan->id = ~0;
	    }
	}
	if( chan->fadeout != 65536 )
	{
	    r_vol *= chan->fadeout;
	    r_vol >>= 16;
	}
	if( env->h.gain != 100 ) r_vol = ( r_vol * env->h.gain ) / 100;
	if( env->h.velocity_influence != 0 && chan->vel != 256 ) 
	{
	    int vv = 100 - env->h.velocity_influence;
	    r_vol = ( ( ( r_vol * chan->vel ) >> 8 ) * env->h.velocity_influence + r_vol * vv ) / 100;
	}
	if( chan->env_pos[ env_num ] >= points[ env->h.sustain << 1 ] ) 
	{
	    if( env->h.flags & ENV_FLAG_SUSTAIN ) 
	    { 
		if( ( chan->flags & GEN_CHANNEL_FLAG_SUSTAIN ) == 0 ) chan->env_pos[ env_num ]++; 
	    } 
	    else 
	    {
		chan->env_pos[ env_num ]++;
	    }
	} 
	else 
	{
	    chan->env_pos[ env_num ]++;
	}
    } 
    else 
    { 
	r_vol = 32768;
	if( ( chan->flags & GEN_CHANNEL_FLAG_SUSTAIN ) == 0 )
	{
	    if( chan->anticlick_out_counter == 0 )
	    {
		if( chan->flags & GEN_CHANNEL_FLAG_PLAYING )
		{
		    chan->anticlick_out_counter = 32768;
		}
	    }
	}
    }
    l_vol = r_vol;
    int env_val[ ENV_COUNT ];
    for( int i = ENV_PAN; i < ENV_COUNT; i++ )
    {
	if( g_envelopes_signed & ( 1 << i ) )
	{
	    env_val[ i ] = 32768 / 2;
	}
	else
	{
	    env_val[ i ] = 0;
	}
    }
    for( env_num = ENV_PAN; env_num < ENV_COUNT; env_num++ )
    {
	env = data->env[ env_num ];
	points = (uint16_t*)env->p;
	if( ( env->h.flags & ENV_FLAG_ENABLED ) && data->env_buf[ env_num ] ) 
	{ 
	    if( env->h.flags & ENV_FLAG_LOOP ) 
	    { 
		if( chan->env_pos[ env_num ] >= points[ env->h.loop_end << 1 ] )  
		    chan->env_pos[ env_num ] = points[ env->h.loop_start << 1 ];
	    }
	    if( chan->env_pos[ env_num ] > points[ ( env->h.point_count - 1 ) << 1 ] ) 
		chan->env_pos[ env_num ] = points[ ( env->h.point_count - 1 ) << 1 ];
	    int v = data->env_buf[ env_num ][ chan->env_pos[ env_num ] ];
	    if( g_envelopes_signed & ( 1 << env_num ) ) v -= 32768 / 2;
	    if( env->h.gain != 100 ) v = ( v * env->h.gain ) / 100;
	    if( env->h.velocity_influence != 0 && chan->vel != 256 ) 
	    {
		int vv = 100 - env->h.velocity_influence;
		v = ( ( ( v * chan->vel ) >> 8 ) * env->h.velocity_influence + v * vv ) / 100;
	    }
	    if( g_envelopes_signed & ( 1 << env_num ) ) v += 32768 / 2;
	    if( first_tick )
		chan->env_prev[ env_num ] = v;
	    else
		chan->env_prev[ env_num ] = chan->env_next[ env_num ];
	    chan->env_next[ env_num ] = v;
	    if( env_num >= FIRST_ENV_WITH_SUBDIV && data->ctl_env_int != 0 )
	    {
		env_val[ env_num ] = chan->env_prev[ env_num ];
	    }
	    else
	    {
		env_val[ env_num ] = v;
	    }
	    if( chan->env_pos[ env_num ] >= points[ env->h.sustain << 1 ] ) 
	    {
		if( env->h.flags & ENV_FLAG_SUSTAIN ) 
		{ 
		    if( ( chan->flags & GEN_CHANNEL_FLAG_SUSTAIN ) == 0 ) chan->env_pos[ env_num ]++; 
		} 
		else 
		{
		    chan->env_pos[ env_num ]++;
		}
	    } 
	    else 
	    {
		chan->env_pos[ env_num ]++;
	    }
	}
    }
    if( data->eff_env && data->ps )
    {
	if( first_tick )
	{
	    sampler_eff_clean( data, ch );
	}
	for( env_num = ENV_EFF1; env_num <= LAST_EFF_ENV; env_num++ )
	{
	    env = data->env[ env_num ];
	    if( ( env->h.flags & ENV_FLAG_ENABLED ) == 0 ) continue;
	    if( first_tick || chan->prev_eff_ctl_val != env_val[ env_num ] )
	    {
		sampler_eff_ctl( data, chan, ch, env->h.eff_ctl, env_val[ env_num ] );
	    }
	}
	if( first_tick )
	{
	    sampler_eff_apply( data, ch );
	}
    }
    sample* smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( chan->smp_num ) );
    int pan = env_val[ ENV_PAN ];
    pan += ( data->ctl_pan - 128 ) * 128;
    pan += ( chan->local_pan - 128 ) * 128;
    pan += ( (signed)smp->panning - 128 ) * 128;
    if( pan < 0 ) pan = 0;
    if( pan > 32768 ) pan = 32768;
    int lpan = 32768 - pan;
    int rpan = pan;
    if( lpan > 32768 / 2 ) lpan = 32768 / 2;
    if( rpan > 32768 / 2 ) rpan = 32768 / 2;
    l_vol = ( l_vol * lpan ) >> 14;
    r_vol = ( r_vol * rpan ) >> 14;
    l_vol = ( l_vol * smp->volume ) >> 6;
    r_vol = ( r_vol * smp->volume ) >> 6;
    l_vol = ( l_vol * data->ctl_volume ) >> 9;
    r_vol = ( r_vol * data->ctl_volume ) >> 9;
    if( data->opt->ignore_vel_for_volume == false )
    {
	l_vol = ( l_vol * chan->vel ) >> 8;
	r_vol = ( r_vol * chan->vel ) >> 8;
    }
    if( data->ctl_env_int )
    {
	if( first_tick )
	{
	    chan->l_old = l_vol;
	    chan->r_old = r_vol;
	}
	chan->vol_step = data->tick_size2 >> 8;
	chan->l_delta = ( ( l_vol - chan->l_old ) << 12 ) / chan->vol_step; 
	chan->r_delta = ( ( r_vol - chan->r_old ) << 12 ) / chan->vol_step;
	chan->l_cur = chan->l_old << 12;
	chan->r_cur = chan->r_old << 12;
	chan->l_old = l_vol;
	chan->r_old = r_vol;
    }
    else
    {
	chan->l_cur = l_vol << 12; 
	chan->r_cur = r_vol << 12; 
    }
    if( ins->vibrato_depth )
    {
	int pos = chan->vib_pos & 255;
	int v = 0;
	switch( ins->vibrato_type & 3 ) 
	{
	    case 0: 
	        v = ( data->vibrato_tab[ pos ] * ins->vibrato_depth ) >> 8;
		if( chan->vib_pos > 0 ) v = -v;
		break;
	    case 1: 
		v = -chan->vib_pos >> 1;
		v = ( v * ins->vibrato_depth ) >> 4;
		break;
	    case 2: 
		v = ( ins->vibrato_depth * 127 ) >> 4;
		if( chan->vib_pos < 0 ) v = -v;
		break;
	    case 3: 
		break;
	}
	if( ins->vibrato_sweep ) 
	{
	    v = ( v * chan->vib_frame ) / ins->vibrato_sweep;
	    chan->vib_frame += 2;
	    if( chan->vib_frame > ins->vibrato_sweep ) chan->vib_frame = ins->vibrato_sweep;
	}
	chan->vib_pos += ins->vibrato_rate << 1;
	if( chan->vib_pos > 255 ) chan->vib_pos -= 512;
	chan->pitch_add1 = v * 4;
    }
    else
    {
	chan->pitch_add1 = 0;
    }
    chan->pitch_add2 = -( env_val[ ENV_PITCH ] - 32768 / 2 );
    sampler_calc_final_pitch( chan, data, mod );
}
static inline void sampler_subtick( 
    gen_channel* chan,
    uint ch,
    MODULE_DATA* data,
    psynth_module* mod,
    int subtick_num )
{
    psynth_net* pnet = mod->pnet;
    for( uint env_num = FIRST_ENV_WITH_SUBDIV; env_num < ENV_COUNT; env_num++ )
    {
	sampler_envelope* env = data->env[ env_num ];
	if( ( env->h.flags & ENV_FLAG_ENABLED ) == 0 ) continue;
	int prev = chan->env_prev[ env_num ];
	int next = chan->env_next[ env_num ];
	int v = prev;
	if( v != next )
	{
	    int d = next - prev;
	    v += ( d * subtick_num ) >> data->tick_subdiv;
	}
	if( env_num == ENV_PITCH )
	{
	    chan->pitch_add2 = -( v - 32768 / 2 );
	    sampler_calc_final_pitch( chan, data, mod );
	}
	if( env_num >= ENV_EFF1 && env_num <= LAST_EFF_ENV )
	{
	    if( data->ps )
	    {
		if( chan->prev_eff_ctl_val != v )
		{
		    sampler_eff_ctl( data, chan, ch, env->h.eff_ctl, v );
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
	    retval = (PS_RETTYPE)"Sampler";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)"Сэмплер позволяет загружать, проигрывать и записывать аудио-файлы (сэмплы). Поддерживаются следующие форматы: WAV, XI, AIFF, OGG, MP3, FLAC, RAW. В версии для Linux также поддерживаются все форматы, которые распознают ffmpeg и avconv.\nЛокальные контроллеры: Панорама";
                        break;
                    }
		    retval = (PS_RETTYPE)"Sampler.\nSupported file formats: WAV, XI, AIFF, OGG (Vorbis), MP3, FLAC, RAW.\nLocal controllers: Panning";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF9000";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_GET_SPEED_CHANGES | PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_USE_MUTEX; break;
	case PS_CMD_INIT:
	    {
		psynth_resize_ctls_storage( mod_num, 8, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, -1, 0, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
		psynth_set_ctl_show_offset( mod_num, 1, -128, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_SAMPLE_INTERPOLATION ), ps_get_string( STR_PS_SAMPLE_INTERP_TYPES ), 0, 2, 2, 1, &data->ctl_smp_int, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_ENVELOPE_INTERPOLATION ), ps_get_string( STR_PS_ENVELOPE_INTERP_TYPES ), 0, 1, 1, 1, &data->ctl_env_int, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 8, 1, &data->ctl_channels, -1, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_REC_THRESHOLD ), "", 0, 10000, 4, 0, &data->ctl_rec_threshold, -1, 3, pnet );
		psynth_set_ctl_flags( mod_num, 5, PSYNTH_CTL_FLAG_EXP3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_TICK_LENGTH ), "", 0, 2048, 128, 0, &data->ctl_tick_scale, 128, 4, pnet );
		psynth_set_ctl_flags( mod_num, 6, PSYNTH_CTL_FLAG_EXP3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RECORD ), ps_get_string( STR_PS_SAMPLER_REC_MODES ), 0, 2, 0, 1, &data->ctl_record, -1, 3, pnet );
		for( int c = 0; c < MAX_CHANNELS; c++ )
		{
		    smem_clear( &data->channels[ c ], sizeof( gen_channel ) );
		    data->channels[ c ].id = ~0;
		}
		data->no_active_channels = true;
		data->search_ptr = 0;
		data->last_ch = 0;
		data->linear_freq_tab = g_linear_freq_tab;
		data->vibrato_tab = g_hsin_tab;
		data->fdialog_opened = false;
		data->tick_size = 1;
		data->tick_size2 = 1;
		data->min_tick_size2 = (int64_t)64 * 256 * pnet->sampling_freq / 44100; 
		data->tick_subdiv = 0;
		data->anticlick_len = 32 * pnet->sampling_freq / 44100;
		data->ps = 0;
		data->eff_env = false;
		for( int i = 0; i < ENV_COUNT; i++ ) 
		{
		    data->env_buf[ i ] = 0;
		    psynth_new_chunk( mod_num, CHUNK_ENV( i ), sizeof( sampler_envelope_header ) + sizeof( uint32_t ), 0, 0, pnet );
		}
		refresh_envelope_pointers( mod_num, pnet );
		for( int p = 0; p < MODULE_OUTPUTS; p++ ) psynth_get_temp_buf( mod_num, pnet, p ); 
		data->mod_num = mod_num;
		data->rec_buf = NULL;
		data->rec = 0;
		data->rec_channels = 2;
		data->rec_reduced_freq = 0;
		data->rec_16bit = 0;
		data->rec_play_pressed = false;
		data->rec_frames = 0;
		atomic_init( &data->rec_wp, (uint)0 );
		atomic_init( &data->rec_thread_stop_request, (int)0 );
		atomic_init( &data->rec_thread_state, (int)0 );
		smutex_init( &data->rec_btn_mutex, 0 );
		psynth_new_chunk( mod_num, CHUNK_INS, sizeof( instrument ), 0, 0, pnet );
		instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
		new_instrument( "", mod_num, pnet );
		refresh_envelopes( data, 0xFF );
#ifdef SUNVOX_GUI
		data->wm = nullptr;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "Sampler GUI", 0, 0, 10, 10, wm->color1, 0, pnet, sampler_visual_handler, wm );
		    sampler_visual_data* smp_data = (sampler_visual_data*)mod->visual->data;
		    mod->visual_min_ysize = smp_data->min_ysize;
		    smp_data->module_data = data;
		    smp_data->mod_num = mod_num;
		    smp_data->pnet = pnet;
		    sampler_editor_data* data2 = (sampler_editor_data*)smp_data->editor_window->childs[ 0 ]->data;
		    data2->module_data = data;
		    data2->mod_num = mod_num;
		    data2->pnet = pnet;
		    sample_editor_data_l* data3 = (sample_editor_data_l*)data2->samples_l->data;
		    data3->module_data = data;
		    data3->mod_num = mod_num;
		    data3->pnet = pnet;
		    sample_editor_data_r* data4 = (sample_editor_data_r*)data2->samples_r->data;
		    data4->module_data = data;
		    data4->mod_num = mod_num;
		    data4->pnet = pnet;
		    data4->ldata = data3;
		    envelope_editor_data_l* data5 = (envelope_editor_data_l*)data2->envelopes_l->data;
		    data5->module_data = data;
		    data5->mod_num = mod_num;
		    data5->pnet = pnet;
		    psynth_submod_change( data5->submod, mod_num, SUBMOD_MAX_CHANNELS, SUBMOD_SUNVOX_FLAGS, &data->ps );
		    envelope_editor_data_r* data6 = (envelope_editor_data_r*)data2->envelopes_r->data;
		    data6->module_data = data;
		    data6->mod_num = mod_num;
		    data6->pnet = pnet;
		}
#endif
	    }
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		size_t size = 0;
		uint flags = 0;
		int freq = 0;
		psynth_get_chunk_info( mod_num, CHUNK_INS, pnet, &size, &flags, &freq );
		if( size < sizeof( instrument ) )
		{
		    psynth_resize_chunk( mod_num, CHUNK_INS, sizeof( instrument ), pnet );
		}
		for( uint s = 0; s < MAX_SAMPLES; s++ )
		{
		    psynth_get_chunk_info( mod_num, CHUNK_SMP( s ), pnet, &size, &flags, &freq );
		    if( size < sizeof( sample ) )
		    {
			psynth_resize_chunk( mod_num, CHUNK_SMP( s ), sizeof( sample ), pnet );
		    }
		}
		refresh_envelope_pointers( mod_num, pnet );
		instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
		uint32_t prev_version = 0;
		uint32_t prev_max_version = 0;
		if( ins->sign != INS_SIGN )
		{
		    smem_copy( ins->smp_num, ins->smp_num_old, 96 );
    		    memset( ins->smp_num + 96, ins->smp_num_old[ 95 ], 128 - 96 );
    		    ins->sign = INS_SIGN;
    		    ins->version = INS_VERSION;
    		    ins->max_version = INS_VERSION;
		}
		else
		{
		    prev_version = ins->version;
		    prev_max_version = ins->max_version;
    		    ins->version = INS_VERSION;
    		    if( prev_version < 6 )
    		    {
    			ins->max_version = INS_VERSION;
			prev_max_version = prev_version;
		    }
    		    else
    		    {
    			if( INS_VERSION > ins->max_version ) ins->max_version = INS_VERSION;
    		    }
		}
		if( prev_version < 5 )
		{
		    convert_old_envelopes_to_new( mod_num, pnet );
		}
		if( prev_max_version < 6 )
		{
		    ins->editor_cursor = 0;
		    ins->editor_selected_size = 0;
		    for( uint s = 0; s < MAX_SAMPLES; s++ )
		    {
			sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( s ), pnet );
			if( smp ) smp->start_pos = 0;
		    }
		}
		for( uint s = 0; s < MAX_SAMPLES; s++ )
		{
		    sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( s ), pnet );
		    if( smp )
		    {
			if( psynth_get_chunk_data( mod_num, CHUNK_SMP_DATA( s ), pnet ) )
			{
			    psynth_get_chunk_info( mod_num, CHUNK_SMP_DATA( s ), pnet, &size, &flags, &freq );
			    flags &= ~(PS_CHUNK_SMP_TYPE_MASK|PS_CHUNK_SMP_CH_MASK);
			    int bits = 8;
			    int channels = 1;
			    switch( ( smp->type >> 4 ) & 3 )
			    {
				case 1: bits = 16; break;
				case 2: bits = 32; break;
			    }
			    if( smp->type & SAMPLE_TYPE_FLAG_STEREO ) channels = 2;
			    if( bits == 32 ) flags |= PS_CHUNK_SMP_FLOAT32;
			    if( bits == 16 ) flags |= PS_CHUNK_SMP_INT16;
			    if( bits == 8 ) flags |= PS_CHUNK_SMP_INT8;
			    flags |= ( channels - 1 ) << PS_CHUNK_SMP_CH_OFFSET;
			    psynth_set_chunk_info( mod_num, CHUNK_SMP_DATA( s ), pnet, flags, freq );
			    recalc_base_pitch( s, mod_num, data, pnet );
			}
		    }
		}
                ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
                refresh_envelopes( data, 0xFF );
                handle_envelope_flags( data );
                data->opt = (sampler_options*)
            	    psynth_get_chunk_data_autocreate( mod_num, CHUNK_OPT, sizeof( sampler_options ), 0, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
            	void* sunsynth_file = psynth_get_chunk_data( mod_num, CHUNK_EFFECT_MOD, pnet );
                if( sunsynth_file )
                {
            	    bool loaded = false;
                    size_t size = 0;
                    psynth_get_chunk_info( mod_num, CHUNK_EFFECT_MOD, pnet, &size, 0, 0 );
                    if( data->ps == 0 )
                    {
                	data->ps = psynth_sunvox_new( pnet, mod_num, 1, SUBMOD_SUNVOX_FLAGS );
                    }
                    if( data->ps )
                    {
                        sfs_file f = sfs_open_in_memory( sunsynth_file, size );
                        if( f )
                        {
                    	    psynth_sunvox_set_module( 0, 0, f, SUBMOD_MAX_CHANNELS, data->ps );
                            sfs_close( f );
                            loaded = true;
                        }
                    }
                    if( loaded )
                    {
#ifdef SUNVOX_GUI
                	data->ps->ui_reinit_request |= -1;
#endif
                	psynth_remove_chunk( mod_num, CHUNK_EFFECT_MOD, pnet );
                    }
                    else
                    {
                	slog( "Sampler effect load failed (%d bytes)\n", (int)size );
                    }
                }
	    }
	    retval = 1;
	    break;
	case PS_CMD_BEFORE_SAVE:
	    {
		instrument* ins = (instrument*)psynth_get_chunk_data( mod_num, CHUNK_INS, pnet );
		if( ins )
		{
		    smem_copy( ins->smp_num_old, ins->smp_num, 96 );
		    convert_new_envelopes_to_old( mod_num, pnet );
		    optimize_envelopes( mod_num, pnet );
		}
		if( data->ps )
		{
		    sfs_file f = sfs_open_in_memory( smem_new( 4 ), 4 );
            	    if( f )
            	    {
            		int err = psynth_sunvox_save_module( f, data->ps );
                	if( err != 0 )
                	{
                    	    char ts[ 512 ];
                    	    sprintf( ts, "Sampler %x effect save error %d", mod_num, err );
#ifdef SUNVOX_GUI
                    	    if( data->wm ) edialog_open( ts, NULL, data->wm );
#endif
			    smem_strcat( ts, sizeof( ts ), "\n" );
                    	    slog( ts );
                	}
                	else
                	{
                    	    void* sunsynth_file = sfs_get_data( f );
                    	    sunsynth_file = smem_resize( sunsynth_file, sfs_tell( f ) );
                    	    psynth_new_chunk( mod_num, CHUNK_EFFECT_MOD, 1, 0, 0, pnet );
                    	    psynth_replace_chunk_data( mod_num, CHUNK_EFFECT_MOD, sunsynth_file, pnet );
                	}
                	sfs_close( f );
            	    }
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
		if( data->rec && !data->rec_pause )
		{
		    bool no_input_signal = true;
		    for( int ch = 0; ch < MODULE_INPUTS; ch++ )
		    {
			if( mod->in_empty[ ch ] < offset + frames )
			{
			    no_input_signal = false;
			    break;
		        }
		    }
		    if( data->ctl_rec_threshold == 0 ) no_input_signal = false;
		    if( data->rec_frames > 0 || !no_input_signal || data->opt->rec_on_play )
		    {
			int rec_channels = MODULE_INPUTS;
			if( data->rec_channels < MODULE_INPUTS )
			    rec_channels = data->rec_channels;
			int skip = 0;
			if( data->rec_frames == 0 )
			{
			    if( data->opt->rec_on_play )
			    {
				if( data->rec_play_pressed == 0 )
				    skip = frames;
			    }
			    else
			    {
#ifdef PS_STYPE_FLOATINGPOINT
				PS_STYPE threshold = (PS_STYPE)data->ctl_rec_threshold / 10000.0F;
#else
				PS_STYPE threshold = ( data->ctl_rec_threshold * PS_STYPE_ONE ) / 10000;
				if( threshold == 0 && data->ctl_rec_threshold != 0 )
				    threshold = 1;
#endif
				for( int ch = 0; ch < rec_channels; ch++ )
				{ 
				    int skip2 = 0;
				    PS_STYPE* in = inputs[ ch ] + offset;
				    for( int i = 0; i < frames; i++ )
				    {
					PS_STYPE v = PS_STYPE_ABS( in[ i ] );
					if( v < threshold ) 
					    skip2++;
					else
					    break;
				    }
				    if( ch == 0 )
				    {
					skip = skip2;
				    }
				    else
				    {
					if( skip2 < skip ) skip = skip2;
				    }
				}
			    }
			}
			data->rec_frames += frames - skip;
			uint rec_wp = atomic_load_explicit( &data->rec_wp, std::memory_order_relaxed );
			for( int ch = 0; ch < rec_channels; ch++ )
			{
			    PS_STYPE* in = inputs[ ch ] + offset + skip;
			    PS_STYPE* rec_buf = (PS_STYPE*)data->rec_buf;
			    rec_wp = data->rec_wp;
			    if( data->rec_reduced_freq )
			    {
				for( int i = 0; i < frames - skip; i += 2 )
				{
				    rec_buf[ rec_wp * data->rec_channels + ch ] = in[ i ];
				    rec_wp++;
				    rec_wp &= ( REC_FRAMES - 1 );
				}
			    }
			    else 
			    {
				for( int i = 0; i < frames - skip; i++ )
				{
				    rec_buf[ rec_wp * data->rec_channels + ch ] = in[ i ];
				    rec_wp++;
				    rec_wp &= ( REC_FRAMES - 1 );
				}
			    }
			}
			COMPILER_MEMORY_BARRIER(); 
		        atomic_store_explicit( &data->rec_wp, rec_wp, std::memory_order_release );
		    }
		}
		if( data->no_active_channels )
		    break;
		instrument* ins = (instrument*)psynth_get_chunk_data( mod, CHUNK_INS );
		if( !ins ) break;
		int ctl_smp_int = data->ctl_smp_int;
#ifndef PS_STYPE_FLOATINGPOINT
		if( ctl_smp_int == 2 ) ctl_smp_int = 1; 
#endif
		data->tick_size2 = (uint64_t)data->tick_size * data->ctl_tick_scale / 128;
		if( data->tick_size2 < data->min_tick_size2 )
		    data->tick_size2 = data->min_tick_size2;
		int subtick_size = 0;
		if( data->tick_subdiv && data->ctl_env_int != 0 )
		{
		    subtick_size = data->tick_size2 >> data->tick_subdiv;
		    if( subtick_size < 1 ) subtick_size = 1;
		}
		data->no_active_channels = true;
#ifdef SUNVOX_GUI
		while( data->editor_play )
		{
		    gen_channel* chan = &data->editor_player_channel;
		    if( ( chan->flags & GEN_CHANNEL_FLAG_PLAYING ) == 0 ) break;
		    sample* smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( chan->smp_num ) );
		    void* smp_data = psynth_get_chunk_data( mod, CHUNK_SMP_DATA( chan->smp_num ) );
		    if( !smp ) break;
		    if( !smp_data ) break;
		    data->no_active_channels = false;
		    PS_STYPE* render_bufs[ MODULE_OUTPUTS ];
		    if( retval == 0 )
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
			    render_bufs[ p ] = outputs[ p ] + offset;
		    }
		    else
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
		    	    render_bufs[ p ] = psynth_get_temp_buf( mod_num, pnet, p );
		    }
		    int rendered = sampler_render( chan, ins, smp, smp_data, ctl_smp_int, render_bufs, MODULE_OUTPUTS, frames );
		    if( rendered < frames )
		    {
			chan->flags &= ~GEN_CHANNEL_FLAG_PLAYING;
			data->editor_play = false;
		    }
		    for( int p = 0; p < MODULE_OUTPUTS; p++ )
		    {
			PS_STYPE* buf = render_bufs[ p ];
			for( int i = 0; i < rendered; i++ )
			    buf[ i ] = (PS_STYPE2)buf[ i ] * data->ctl_volume / (PS_STYPE2)512;
		    }
		    if( retval == 0 )
		    {
			for( int p = 0; p < MODULE_OUTPUTS; p++ )
    	    		{
			    PS_STYPE* b = render_bufs[ p ];
			    for( int i = rendered; i < frames; i++ ) b[ i ] = 0;
			}
		    }
		    else
		    {
			for( int p = 0; p < MODULE_OUTPUTS; p++ )
			{
			    PS_STYPE* dest = outputs[ p ] + offset;
			    PS_STYPE* src = render_bufs[ p ];
			    for( int i = 0; i < rendered; i++ ) dest[ i ] += src[ i ];
			}
		    }
		    retval = 1;
		    break;
		}
#endif
		for( int c = 0; c < data->ctl_channels; c++ ) 
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( ( chan->flags & GEN_CHANNEL_FLAG_PLAYING ) == 0 ) continue;
		    sample* smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( chan->smp_num ) );
		    void* smp_data = psynth_get_chunk_data( mod, CHUNK_SMP_DATA( chan->smp_num ) );
		    if( !smp ) continue;
		    if( !smp_data ) continue;
		    data->no_active_channels = false;
    		    if( chan->tick_counter == 0xFFFFFFF )
    		    {
		        chan->tick_counter = data->tick_size2;
		        chan->subtick_counter = subtick_size;
		    }
		    PS_STYPE* render_bufs[ MODULE_OUTPUTS ];
		    if( retval == 0 )
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
			    render_bufs[ p ] = outputs[ p ] + offset;
		    }
		    else
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
		    	    render_bufs[ p ] = psynth_get_temp_buf( mod_num, pnet, p );
		    }
		    int ptr = 0;
		    bool sample_finished = false;
		    while( sample_finished == false )
		    {
			int buf_size = frames - ptr;
			if( buf_size > ( data->tick_size2 - chan->tick_counter ) / 256 ) buf_size = ( data->tick_size2 - chan->tick_counter ) / 256;
			if( ( data->tick_size2 - chan->tick_counter ) & 255 ) buf_size++; 
			if( subtick_size )
			{
			    if( buf_size > ( subtick_size - chan->subtick_counter ) / 256 ) buf_size = ( subtick_size - chan->subtick_counter ) / 256;
			    if( ( subtick_size - chan->subtick_counter ) & 255 ) buf_size++; 
			}
			if( buf_size > frames - ptr ) buf_size = frames - ptr;
			if( buf_size < 0 ) buf_size = 0;
			PS_STYPE* render_bufs2[ MODULE_OUTPUTS ];
			for( int p = 0; p < MODULE_OUTPUTS; p++ )
			    render_bufs2[ p ] = render_bufs[ p ] + ptr;
			if( chan->anticlick_out_counter )
			{
			    if( data->anticlick_len * 2 < buf_size )
				buf_size = data->anticlick_len * 2;
			}
			int rendered = sampler_render( chan, ins, smp, smp_data, ctl_smp_int, render_bufs2, MODULE_OUTPUTS, buf_size );
			if( rendered < buf_size )
			{
			    buf_size = rendered;
			    sample_finished = true;
			}
			if( rendered )
			{
			    if( data->eff_env && data->ps )
			    {
				psynth_sunvox_apply_module(
			    	    1 + c, 
			    	    render_bufs2, MODULE_OUTPUTS, 0,
			    	    rendered, 
				    data->ps );
			    }
			    sampler_apply_volume_envelope( chan, data->ctl_env_int, render_bufs2, MODULE_OUTPUTS, rendered );
			    if( chan->anticlick_in_counter ) 
			    {
			        sampler_anticlick_in( chan, data->anticlick_len, render_bufs2, MODULE_OUTPUTS, rendered, pnet );
			    }
			    if( chan->anticlick_out_counter ) 
			    {
			        if( sampler_anticlick_out( chan, data->anticlick_len, render_bufs2, MODULE_OUTPUTS, rendered, pnet ) )
				    sample_finished = true;
			    }
                        }
			ptr += buf_size;
			chan->tick_counter += buf_size * 256;
			if( chan->tick_counter >= data->tick_size2 ) 
			{
			    if( data->opt->fit_to_pattern && !sample_finished ) handle_fit_to_pattern( data, mod, chan, render_bufs2, MODULE_OUTPUTS, rendered );
			    sampler_tick( chan, c, ins, data, mod );
			    chan->tick_counter %= data->tick_size2;
			    if( subtick_size )
			    {
				chan->subtick_counter = chan->tick_counter % subtick_size;
			    }
			}
			else
			{
			    if( subtick_size )
			    {
				chan->subtick_counter += buf_size * 256;
				if( chan->subtick_counter >= subtick_size ) 
				{
				    int subtick_num = chan->tick_counter / subtick_size;
				    if( subtick_num < ( 1 << data->tick_subdiv ) )
				    {
					sampler_subtick( chan, c, data, mod, subtick_num );
				    }
				    chan->subtick_counter %= subtick_size;
				}
			    }
			}
			if( ptr >= frames ) break;
		    } 
		    if( ptr > 0 )
		    {
			if( retval == 0 )
			{
			    if( ptr < frames )
			    {
			        for( int p = 0; p < MODULE_OUTPUTS; p++ )
			        {
			    	    PS_STYPE* b = render_bufs[ p ];
			    	    for( int i = ptr; i < frames; i++ )
				        b[ i ] = 0;
				}
			    }
			    retval = 1;
			}
			else
			{
			    for( int p = 0; p < MODULE_OUTPUTS; p++ )
			    {
			        PS_STYPE* dest = outputs[ p ] + offset;
			        PS_STYPE* src = render_bufs[ p ];
			        for( int i = 0; i < ptr; i++ )
			    	    dest[ i ] += src[ i ];
			    }
			}
		    }
		    if( ptr >= frames )
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
		    	    chan->last_s[ p ] = render_bufs[ p ][ frames - 1 ];
		    }
		    else
		    {
		        for( int p = 0; p < MODULE_OUTPUTS; p++ )
		    	    chan->last_s[ p ] = 0;
		    }
		} 
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c;
		instrument* ins = (instrument*)psynth_get_chunk_data( mod, CHUNK_INS );
		if( !ins ) break;
		if( data->no_active_channels == false )
		{
		    for( c = 0; c < MAX_CHANNELS; c++ )
		    {
			if( data->channels[ c ].id == event->id )
			{
			    if( data->channels[ c ].flags & GEN_CHANNEL_FLAG_SUSTAIN )
			    {
				release_sampler_channel( &data->channels[ c ], ins, data );
			    }
			    data->channels[ c ].id = ~0;
			    break;
			}
		    }
		    for( c = 0; c < data->ctl_channels; c++ )
		    {
			if( ( data->channels[ data->search_ptr ].flags & GEN_CHANNEL_FLAG_PLAYING ) == 0 ) break;
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
		data->last_ch = c;
		gen_channel* ch = &data->channels[ c ];
		int finetune;
		if( pnet->base_host_version == 0x01070000 )
		    finetune = ( PS_NOTE0_PITCH - event->note.root_note * 256 ) - event->note.pitch;
		else
		    finetune = 0;
		int note_num;
		if( pnet->base_host_version == 0x01070000 )
		    note_num = event->note.root_note;
		else
		    note_num = PS_PITCH_TO_NOTE( event->note.pitch - 128 );
		if( note_num < 0 ) note_num = 0;
		int smp_note_num = note_num;
		if( smp_note_num >= 128 ) smp_note_num = 127;
		ch->smp_note_num = smp_note_num;
		int smp_num = ins->smp_num[ smp_note_num ];
		sample* smp;
		if( ins->samples_num && smp_num < ins->samples_num )
		{
		    smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( smp_num ) );
		    ch->smp_num = smp_num;
		    if( !smp ) break;
		}
		else
		{
		    break;
		}
		int pitch;
		if( pnet->base_host_version == 0x01070000 )
		{
		    note_num += smp->relative_note;
		    note_num += ins->relative_note;
		    if( note_num < 0 ) note_num = 0;
		    pitch = 7680 - ( note_num * 64 ) - ( smp->finetune >> 1 ) - ( ins->finetune >> 1 ) - ( finetune >> 2 );
		}
		else
		    pitch = ( event->note.pitch >> 2 ) - ( smp->relative_note * 64 ) - ( ins->relative_note * 64 ) - ( smp->finetune >> 1 ) - ( ins->finetune >> 1 ); 
		ch->cur_pitch = pitch * 4;
		ch->cur_pitch_delta = ch->cur_pitch - event->note.pitch;
		ch->final_pitch = 0xFFFFFFF;
    		ch->anticlick_in_counter = 0;
    		ch->anticlick_out_counter = 0;
		if( ch->flags & GEN_CHANNEL_FLAG_PLAYING )
		{ 
		    for( int p = 0; p < MODULE_OUTPUTS; p++ ) 
		    {
			PS_STYPE s = ch->last_s[ p ];
			ch->anticlick_in_s[ p ] = s;
			if( s != 0 && ch->anticlick_in_counter == 0 )
			{
			    ch->anticlick_in_counter = 32768;
			}
		    }
		}
		ch->flags |= GEN_CHANNEL_FLAG_PLAYING;
		ch->vel = event->note.velocity;
		ch->id = event->id;
		ch->local_pan = 128;
		reset_sampler_channel( ch );
		ch->ptr_h = smp->start_pos;
		data->no_active_channels = false;
		retval = 1;
	    }
	    break;
    	case PS_CMD_SET_FREQ:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		gen_channel* ch = &data->channels[ c ];
		if( ch->id == event->id )
		{
		    ch->vel = event->note.velocity;
		    ch->cur_pitch = event->note.pitch + ch->cur_pitch_delta;
		    sampler_calc_final_pitch( ch, data, mod );
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
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    instrument* ins = (instrument*)psynth_get_chunk_data( mod, CHUNK_INS );
		    if( !ins ) break;
		    sample* smp;
		    if( ins->samples_num && data->channels[ c ].smp_num < ins->samples_num )
		    {
			smp = (sample*)psynth_get_chunk_data( mod, CHUNK_SMP( data->channels[ c ].smp_num ) );
			if( smp == 0 ) break;
		    }
		    else
	    	    {
			break;
		    }
		    uint32_t new_offset;
		    if( event->command == PS_CMD_SET_SAMPLE_OFFSET )
		    {
			new_offset = smp->start_pos + event->sample_offset.sample_offset;
		    }
		    else
		    {
			uint32_t len = smp->length - smp->start_pos;
			uint64_t v = (uint64_t)len * event->sample_offset.sample_offset;
			v /= 0x8000;
			new_offset = smp->start_pos + (uint32_t)v;
		    }
		    if( new_offset >= smp->length )
			new_offset = smp->length - 1;
		    data->channels[ c ].ptr_h = new_offset;
		    data->channels[ c ].ptr_l = 0;
		    retval = 1;
		    break;
		}
	    }
	    break;
	case PS_CMD_NOTE_OFF:
	    {
		if( data->no_active_channels ) break;
		instrument* ins = (instrument*)psynth_get_chunk_data( mod, CHUNK_INS );
		if( !ins ) break;
		for( int c = 0; c < MAX_CHANNELS; c++ )
		{
		    if( data->channels[ c ].id == event->id )
		    {
			release_sampler_channel( &data->channels[ c ], ins, data );
			break;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    {
		instrument* ins = (instrument*)psynth_get_chunk_data( mod, CHUNK_INS );
		if( !ins ) break;
		for( int c = 0; c < MAX_CHANNELS; c++ )
		{
		    release_sampler_channel( &data->channels[ c ], ins, data );
		    data->channels[ c ].id = ~0;
		}
#ifdef SUNVOX_GUI
		editor_play( pnet, mod_num, 0, 0, 0 ); 
#endif
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].flags &= ~GEN_CHANNEL_FLAG_PLAYING;
		data->channels[ c ].id = ~0;
	    }
#ifdef SUNVOX_GUI
	    editor_play( pnet, mod_num, 0, 0, 0 ); 
#endif
	    data->no_active_channels = true;
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    switch( event->controller.ctl_num )
		    {
			case 1:
			    data->channels[ c ].local_pan = event->controller.ctl_val >> 7;
			    retval = 1;
			    break;
		    }
		    break;
		}
	    }
	    if( retval ) break;
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    {
		sampler_options* opt = data->opt;
                int v = event->controller.ctl_val;
                switch( event->controller.ctl_num + 1 )
                {
            	    case 8:
            	    {
            		sampler_rec( pnet, mod_num, SMP_REC_SS_FLAG_SYNTH_THREAD, v );
            		break;
            	    }
                    case 127: opt->rec_on_play = v!=0; retval = 1; break;
                    case 126: opt->rec_mono = v!=0; retval = 1; break;
                    case 125: opt->rec_reduced_freq = v!=0; retval = 1; break;
                    case 124: opt->rec_16bit = v!=0; retval = 1; break;
                    case 123: opt->finish_rec_on_stop = v!=0; retval = 1; break;
                    case 122: opt->ignore_vel_for_volume = v!=0; retval = 1; break;
                    case 121: opt->freq_accuracy = v!=0; retval = 1; break;
                    case 120: opt->fit_to_pattern = v; retval = 1; break;
                    default: break;
                }
            }
	    break;
	case PS_CMD_SPEED_CHANGED:
	    data->tick_size = event->speed.tick_size;
	    data->ticks_per_line = event->speed.ticks_per_line;
	    retval = 1;
	    break;
	case PS_CMD_PLAY:
	    if( data->opt->rec_on_play )
	    {
		data->rec_play_pressed = true;
	    }
	    retval = 1;
	    break;
	case PS_CMD_STOP:
	    if( data->opt->finish_rec_on_stop )
	    {
		if( data->rec )
		{
        	    sampler_rec( pnet, mod_num, SMP_REC_SS_FLAG_SYNTH_THREAD, 0 ); 
        	}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
		remove_window( mod->visual, data->wm );
		mod->visual = nullptr;
	    }
#endif
            if( !sthread_is_empty( &data->rec_thread ) )
            {
		sampler_rec( pnet, mod_num, SMP_REC_SS_FLAG_DONT_LOAD_FILE | SMP_REC_SS_FLAG_CLOSE_THREAD | SMP_REC_SS_FLAG_SYNTH_THREAD, 0 );
            }
	    for( int i = 0; i < ENV_COUNT; i++ ) smem_free( data->env_buf[ i ] );
	    psynth_sunvox_remove( data->ps );
	    smutex_destroy( &data->rec_btn_mutex );
	    smem_free( data->rec_buf );
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}
