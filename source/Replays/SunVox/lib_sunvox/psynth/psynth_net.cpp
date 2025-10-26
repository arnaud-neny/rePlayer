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
#define DEFAULT_MODULE_EVENTS_NUM 128
#define DEFAULT_HEAP_EVENTS_NUM 256
#if defined(OS_LINUX) && (CPUMARK >= 10) && defined(SMEM_USE_NAMES) && defined(SUNVOX_GUI)
    #define EVT_HEAP_DEBUG_MESSAGES
#endif
PS_RETTYPE psynth_empty( PSYNTH_MODULE_HANDLER_PARAMETERS )
{
    PS_RETTYPE retval = 0;
    switch( event->command )
    {
	case PS_CMD_GET_INPUTS_NUM: retval = PSYNTH_MAX_CHANNELS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = PSYNTH_MAX_CHANNELS; break;
	default: break;
    }
    return retval;
}
PS_RETTYPE psynth_bypass( PSYNTH_MODULE_HANDLER_PARAMETERS )
{
    psynth_module* mod = NULL;
    if( mod_num >= 0 )
    {
        mod = &pnet->mods[ mod_num ];
    }
    PS_RETTYPE retval = 0;
    switch( event->command )
    {
	case PS_CMD_GET_INPUTS_NUM: retval = PSYNTH_MAX_CHANNELS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = PSYNTH_MAX_CHANNELS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_RENDER_REPLACE:
    	    {
    		if( mod == NULL ) break;
        	PS_STYPE** inputs = mod->channels_in;
        	PS_STYPE** outputs = mod->channels_out;
        	if( !inputs[ 0 ] || !outputs[ 0 ] || ( mod->flags & PSYNTH_FLAG_OUTPUT_IS_EMPTY ) ) break;
        	int prev_outputs_num = psynth_get_number_of_outputs( mod );
        	int prev_inputs_num = psynth_get_number_of_inputs( mod );
        	int max_in_channels = 0; for( max_in_channels = 0; max_in_channels < PSYNTH_MAX_CHANNELS; max_in_channels++ ) if( !inputs[ max_in_channels ] ) break;
        	int max_out_channels = 0; for( max_out_channels = 0; max_out_channels < PSYNTH_MAX_CHANNELS; max_out_channels++ ) if( !outputs[ max_out_channels ] ) break;
        	if( prev_outputs_num != max_out_channels ) psynth_set_number_of_outputs( max_out_channels, mod_num, pnet );
        	if( prev_inputs_num != max_in_channels ) psynth_set_number_of_inputs( max_in_channels, mod_num, pnet );
        	int offset = mod->offset;
        	int frames = mod->frames;
                bool no_input_signal = true;
                for( int ch = 0; ch < max_in_channels; ch++ )
                {
                    if( mod->in_empty[ ch ] < offset + frames )
                    {
                        no_input_signal = false;
                        break;
                    }
                }
                if( no_input_signal ) break;
                PS_STYPE* in = NULL;
                PS_STYPE* out = NULL;
        	for( int ch = 0; ch < PSYNTH_MAX_CHANNELS; ch++ )
        	{
        	    if( inputs[ ch ] ) in = inputs[ ch ] + offset;
            	    if( in == NULL ) break;
            	    if( outputs[ ch ] ) out = outputs[ ch ] + offset;
            	    if( out == NULL ) break;
            	    for( int i = 0; i < frames; i++ ) out[ i ] = in[ i ];
        	}
        	retval = 1;
    	    }
    	    break;
	default: break;
    }
    return retval;
}
#define PSYNTH_FREQ_TABLE_BODY
#include "psynth_freq_table.h"
bool g_loading_builtin_sv_template = false;
const char g_n2c[ 12 ] = 
{
    'C', 'c', 'D', 'd', 'E', 'F', 'f', 'G', 'g', 'A', 'a', 'B'
};
const uint8_t g_module_colors[ 128 * 3 + 1 ] =
    "\377\0\0\377\30\0\3770\0\377H\0\377`\0\377x\0\377\220\0\377\250\0\377\300"
    "\0\377\330\0\377\360\0\366\377\0\336\377\0\306\377\0\256\377\0\226\377\0"
    "~\377\0f\377\0N\377\0""6\377\0\36\377\0\5\377\0\0\377\23\0\377+\0\377C\0"
    "\377[\0\377s\0\377\213\0\377\243\0\377\273\0\377\323\0\377\353\0\373\377"
    "\0\343\377\0\313\377\0\263\377\0\233\377\0\202\377\0k\377\0S\377\0:\377\0"
    "\"\377\0\12\377\16\0\377%\0\377>\0\377V\0\377n\0\377\206\0\377\236\0\377"
    "\266\0\377\316\0\377\346\0\377\376\0\377\377\0\350\377\0\320\377\0\270\377"
    "\0\240\377\0\210\377\0p\377\0X\377\0@\377\0(\377y\200\377\377\377\377\302"
    "\273\377\227\177\377\243\177\377\257\177\377\273\177\377\307\177\377\323"
    "\177\377\337\177\377\353\177\377\367\177\372\377\177\356\377\177\342\377"
    "\177\326\377\177\312\377\177\276\377\177\262\377\177\246\377\177\232\377"
    "\177\216\377\177\202\377\177\177\377\210\177\377\224\177\377\240\177\377"
    "\254\177\377\267\177\377\304\177\377\317\177\377\333\177\377\347\177\377"
    "\363\177\375\377\177\361\377\177\345\377\177\331\377\177\315\377\177\301"
    "\377\177\265\377\177\251\377\177\235\377\177\221\377\177\205\377\204\177"
    "\377\220\177\377\234\177\377\250\177\377\264\177\377\300\177\377\314\177"
    "\377\330\177\377\344\177\377\360\177\377\374\177\377\377\177\365\377\177"
    "\351\377\177\334\377\177\321\377\177\305\377\177\271\377\177\255\377\177"
    "\240\377\177\225\377BG";
atomic_vptr g_noise_table;
#define MAX_SINE_TABLES 16
atomic_vptr g_sine_tables[ MAX_SINE_TABLES ];
atomic_vptr g_base_wavetable;
int psynth_global_init()
{
    atomic_init( &g_noise_table, (void*)NULL );
    for( int i = 0; i < MAX_SINE_TABLES; i++ )
    {
	atomic_init( &g_sine_tables[ i ], (void*)NULL );
    }
    atomic_init( &g_base_wavetable, (void*)NULL );
    return 0;
}
int psynth_global_deinit()
{
    void* p = atomic_exchange( &g_noise_table, (void*)NULL ); smem_free( p );
    for( int i = 0; i < MAX_SINE_TABLES; i++ )
    {
	p = atomic_exchange( &g_sine_tables[ i ], (void*)NULL ); smem_free( p );
    }
    p = atomic_exchange( &g_base_wavetable, (void*)NULL ); smem_free( p );
    return 0;
}
#ifdef PSYNTH_MULTITHREADED
void* psynth_thread_body( void* data )
{
    psynth_thread* th = (psynth_thread*)data;
    printf( "PSYNTH THREAD %d BEGIN\n", th->n );
    while( 1 )
    {
	if( th->pnet->th_exit_request ) break;
	if( th->pnet->th_work )
	{
	    atomic_fetch_add( &th->pnet->th_work_cnt, 1 );
	    if( th->pnet->th_work )
	    {
	        printf( "TH%d: %d\n", th->n, stime_ticks() - th->pnet->th_work_t );
	    }
	    atomic_fetch_sub( &th->pnet->th_work_cnt, 1 );
	}
    }
    printf( "PSYNTH THREAD %d END\n", th->n );
    return NULL;
}
#endif
static void psynth_thread_init( int n, psynth_net* pnet )
{
    psynth_thread* th = &pnet->th[ n ];
    th->n = n;
    th->pnet = pnet;
    sundog_engine* sd = nullptr; GET_SD_FROM_PSYNTH_NET( pnet, sd );
#ifdef PSYNTH_MULTITHREADED
    if( n == 0 ) atomic_init( &pnet->th_work_cnt, 0 );
    if( n > 0 )
    {
	sthread_create( &th->th, sd, psynth_thread_body, th, 0 );
    }
#endif
}
static void psynth_thread_deinit( int n, psynth_net* pnet )
{
    psynth_thread* th = &pnet->th[ n ];
#ifdef PSYNTH_MULTITHREADED
    if( n > 0 )
    {
	sthread_destroy( &th->th, 1000 );
    }
#endif
    for( int i = 0; i < PSYNTH_MAX_CHANNELS; i++ ) smem_free( th->temp_buf[ i ] );
    for( int i = 0; i < PSYNTH_MAX_CHANNELS * 2; i++ ) smem_free( th->resamp_buf[ i ] );
}
void psynth_init( uint flags, int freq, int bpm, int tpl, void* host, uint base_host_version, psynth_net* pnet )
{
    smem_clear( pnet, sizeof( psynth_net ) ); 
    pnet->flags = flags;
    smutex_init( &pnet->mods_mutex, 0 );
    pnet->mods = SMEM_ZALLOC2( psynth_module, 4 );
    pnet->mods_num = 4;
    int heap_size = DEFAULT_HEAP_EVENTS_NUM;
#ifdef PSYNTH_MULTITHREADED
    heap_size *= 4;
    atomic_init( &pnet->events_num, 0 );
#endif
    pnet->events_heap = SMEM_ALLOC2( psynth_event, heap_size );
    pnet->th_num = 1;
#ifdef PSYNTH_MULTITHREADED
#endif
    pnet->th = SMEM_ZALLOC2( psynth_thread, pnet->th_num );
    for( int i = 0; i < pnet->th_num; i++ ) psynth_thread_init( i, pnet );
    if( !( flags & PSYNTH_NET_FLAG_NO_MIDI ) )
    {
	sunvox_engine* s = (sunvox_engine*)host;
	sundog_engine* sd = NULL;
	sundog_sound* ss = NULL;
	if( s )
	{
	    ss = s->ss;
	    if( ss ) sd = ss->sd;
	}
	sundog_midi_client_open( &pnet->midi_client, sd, ss, "SunVox", 0 );
	pnet->midi_in_map = ssymtab_new( 3 );
    }
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_SCOPE ) )
    {
	pnet->fft_size = sconfig_get_int_value( "fft", PSYNTH_DEF_FFT_SIZE, 0 );
	if( pnet->fft_size < 64 ) pnet->fft_size = 64;
	if( pnet->fft_size > 32768 ) pnet->fft_size = 32768;
	pnet->fft = SMEM_ALLOC2( PS_STYPE, pnet->fft_size );
    }
    pnet->fft_mod = -1;
    pnet->sampling_freq = freq;
    pnet->max_buf_size = (int)( (float)freq * 0.02F ); 
    pnet->global_volume = 80;
    pnet->host = host;
    pnet->base_host_version = base_host_version;
    pnet->prev_change_counter2 = -1;
    int out = psynth_add_module( -1, 0, "Output", PSYNTH_FLAG_OUTPUT, 512, 512, 0, bpm, tpl, pnet );
    if( flags & PSYNTH_NET_FLAG_CREATE_MODULES )
    {
    }
}
void psynth_close( psynth_net* pnet )
{
    if( pnet->mods )
    {
	for( uint i = 0; i < pnet->mods_num; i++ ) psynth_remove_module( i, pnet );
	smem_free( pnet->mods );
    }
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) )
	sundog_midi_client_close( &pnet->midi_client );
    if( pnet->midi_in_map )
    {
	ssymtab_item* map = ssymtab_get_list( pnet->midi_in_map );
	if( map )
	{
	    for( size_t i = 0; i < smem_get_size( map ) / sizeof( ssymtab_item ); i++ )
	    {
		ssymtab_item* item = &map[ i ];
		if( item->val.p )
		{
		    smem_free( item->val.p );
		}
	    }
	    smem_free( map );
	}
	ssymtab_delete( pnet->midi_in_map );
    }
    smem_free( pnet->midi_in_mods );
    pnet->midi_in_mods_num = 0;
    smem_free( pnet->fft );
    smutex_destroy( &pnet->mods_mutex );
    smem_free( pnet->events_heap );
    pnet->th_exit_request = true;
    for( int i = 0; i < pnet->th_num; i++ ) psynth_thread_deinit( i, pnet );
    smem_free( pnet->th );
    smem_free( pnet );
}
void psynth_clear( psynth_net* pnet )
{
    if( pnet->mods )
    {
	for( uint i = 1; i < pnet->mods_num; i++ ) psynth_remove_module( i, pnet );
    }
}
uint32_t g_psynth_module_id_cnt = 0;
int psynth_add_module(
    int n,
    psynth_handler_t handler,
    const char* name,
    uint flags, 
    int x, 
    int y,
    int z, 
    int bpm, 
    int tpl, 
    psynth_net* pnet )
{
    int err = 0;
    if( handler == NULL ) handler = psynth_empty;
    pnet->change_counter++;
    pnet->change_counter2++;
    if( n < 0 )
    {
	for( n = 0; (unsigned)n < pnet->mods_num; n++ )
	{
	    if( !( pnet->mods[ n ].flags & PSYNTH_FLAG_EXISTS ) ) break;
	}
	if( (unsigned)n == pnet->mods_num )
	{
	    pnet->mods = SMEM_ZRESIZE2( pnet->mods, psynth_module, pnet->mods_num + 4 );
	    if( !pnet->mods ) return -1;
	    n = pnet->mods_num;
	    pnet->mods_num += 4;
	}
    }
    psynth_module* s = &pnet->mods[ n ];
    smem_clear( s, sizeof( psynth_module ) );
    s->pnet = pnet;
    s->handler = handler;
    s->flags = PSYNTH_FLAG_EXISTS | flags;
    psynth_event evt;
    evt.command = PS_CMD_GET_FLAGS;
    s->flags |= handler( n, &evt, pnet );
    evt.command = PS_CMD_GET_FLAGS2;
    s->flags2 |= handler( n, &evt, pnet );
    s->x = x;
    s->y = y;
    s->z = z;
    s->scale = 256;
    s->visualizer_pars = sconfig_get_int_value( "visualizer_pars", PSYNTH_VIS_PARS_DEFAULT, 0 );
    evt.command = PS_CMD_GET_COLOR;
    get_color_from_string( (char*)handler( n, &evt, pnet ), &s->color[ 0 ], &s->color[ 1 ], &s->color[ 2 ] );
    s->events_num = 0;
    s->events = SMEM_ALLOC2( int, DEFAULT_MODULE_EVENTS_NUM );
    if( !s->events ) return -1;
    s->id = g_psynth_module_id_cnt++;
    if( name )
    {
	smem_strcat( s->name, sizeof( s->name ), name );
    }
    else
    {
	if( !( flags & PSYNTH_FLAG_OUTPUT ) )
	{
	    const char* mod_name;
	    evt.command = PS_CMD_GET_NAME;
	    mod_name = (const char*)handler( n, &evt, pnet );
	    if( !mod_name || mod_name[ 0 ] == 0 ) mod_name = "MODULE";
	    int mod_name_len = (int)strlen( mod_name );
	    smem_strcat( s->name, sizeof( s->name ), mod_name );
	    int max_num = 0;
	    for( uint a = 0; a < pnet->mods_num; a++ )
	    {
		int num = 0;
		if( (int)a == n ) continue;
		psynth_module* m = &pnet->mods[ a ];
		int name_len = (int)strlen( m->name );
		if( name_len < mod_name_len ) continue;
		if( memcmp( m->name, mod_name, mod_name_len ) ) continue; 
		char* num_str = m->name + mod_name_len;
		while( num_str[0] == '_' ) num_str++; 
		if( num_str[0] == 0 )
		{
		    num = 1;
		}
		else
		{
		    bool not_a_number = 0;
		    char* num_str2 = num_str;
		    while( *num_str2 != 0 )
		    {
			if( *num_str2 < '0' || *num_str2 > '9' )
			{
			    not_a_number = 1;
			    break;
			}
			num_str2++;
		    }
		    if( not_a_number == 0 )
		    {
			num = string_to_int( num_str );
		    }
		}
		if( num > 0 )
		{
		    if( num > max_num ) max_num = num;
		}
	    }
	    if( max_num > 0 )
	    {
		char num_str[ 16 ];
		int_to_string( max_num + 1, num_str );
		int name_len = (int)strlen( s->name );
		char c = s->name[ name_len - 1 ];
		if( c >= '0' && c <= '9' )
		{
		    smem_strcat( s->name, sizeof( s->name ), "_" );
		}
		smem_strcat( s->name, sizeof( s->name ), num_str );
	    }
	}
    }
    if( s->flags & PSYNTH_FLAG_USE_MUTEX )
	smutex_init( &s->mutex, 0 );
    s->chunks = NULL;
    int data_size = 0;
    evt.command = PS_CMD_GET_DATA_SIZE;
    data_size = handler( n, &evt, pnet );
    if( data_size )
    {
	s->data_ptr = SMEM_ZALLOC( data_size );
	if( !s->data_ptr ) return -1;
    }
    else
    {
	s->data_ptr = NULL;
    }
    evt.command = PS_CMD_INIT;
    handler( n, &evt, pnet );
    s->flags |= PSYNTH_FLAG_INITIALIZED;
    evt.command = PS_CMD_GET_INPUTS_NUM;
    s->input_channels = handler( n, &evt, pnet );
    evt.command = PS_CMD_GET_OUTPUTS_NUM;
    s->output_channels = handler( n, &evt, pnet );
    for( int ch = 0; ch < PSYNTH_MAX_CHANNELS; ch++ )
    {
	s->channels_in[ ch ] = NULL;
	s->channels_out[ ch ] = NULL;
    }
    int max_channels = s->output_channels;
    if( s->input_channels > max_channels ) max_channels = s->input_channels;
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MODULE_CHANNELS ) )
    {
	for( int ch = 0; ch < max_channels; ch++ )
	{
	    PS_STYPE* ch_buf = SMEM_ZALLOC2( PS_STYPE, pnet->max_buf_size );
	    if( !ch_buf ) { err = 1; break; }
	    s->channels_in[ ch ] = ch_buf;
	    s->in_empty[ ch ] = pnet->max_buf_size;
	    if( !( s->flags & PSYNTH_FLAG_OUTPUT ) )
	    {
		ch_buf = SMEM_ZALLOC2( PS_STYPE, pnet->max_buf_size );
		if( !ch_buf ) { err = 2; break; }
		s->channels_out[ ch ] = ch_buf;
		s->out_empty[ ch ] = pnet->max_buf_size;
	    }
	}
    }
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_SCOPE ) )
    {
	for( int ch = 0; ch < max_channels; ch++ )
	{
	    if( s->flags & PSYNTH_FLAG_NO_SCOPE_BUF )
		s->scope_buf[ ch ] = NULL;
	    else
	    {
		s->scope_buf[ ch ] = SMEM_ZALLOC2( PS_STYPE, PSYNTH_SCOPE_SIZE );
	    }
	}
    }
    if( s->flags & PSYNTH_FLAG_GET_SPEED_CHANGES )
    {
	evt.command = PS_CMD_SPEED_CHANGED;
	evt.speed.tick_size = PSYNTH_TICK_SIZE( pnet->sampling_freq, bpm );
	evt.speed.bpm = bpm;
	evt.speed.ticks_per_line = tpl;
	handler( n, &evt, pnet );
    }
    s->input_links_num = 0;
    s->input_links = NULL;
    s->output_links_num = 0;
    s->output_links = NULL;
    s->midi_out_name = 0;
    s->midi_out = -1;
    s->midi_out_bank = -1;
    s->midi_out_prog = -1;
#ifndef NOMIDI
    memset( s->midi_out_note_id, 255, sizeof( s->midi_out_note_id ) );
    SMEM_CLEAR_STRUCT( s->midi_out_note );
#endif
    if( err )
    {
	psynth_remove_module( n, pnet );
	n = -1;
    }
    return n;
}
void psynth_remove_module( uint mod_num, psynth_net* pnet )
{
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return;
    if( !( pnet->mods[ mod_num ].flags & PSYNTH_FLAG_EXISTS ) ) return;
    pnet->change_counter++;
    pnet->change_counter2++;
    psynth_module* mod = &pnet->mods[ mod_num ];
    psynth_event evt;
    evt.command = PS_CMD_CLOSE;
    mod->handler( mod_num, &evt, pnet );
    smem_free( mod->events );
    mod->events = 0;
    smem_free( mod->data_ptr ); 
    mod->data_ptr = 0;
    psynth_remove_chunks( mod_num, pnet );
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MODULE_CHANNELS ) )
    {
	for( uint i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
	{
	    if( mod->channels_in[ i ] ) 
	    {
		smem_free( mod->channels_in[ i ] );
		mod->channels_in[ i ] = NULL;
	    }
	    if( mod->channels_out[ i ] ) 
	    {
		smem_free( mod->channels_out[ i ] );
		mod->channels_out[ i ] = NULL;
	    }
	}
    }
    for( uint i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
    {
	smem_free( mod->scope_buf[ i ] );
	mod->scope_buf[ i ] = NULL;
    }
    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) )
    {
	if( mod->midi_out >= 0 )
	    psynth_all_midi_notes_off( mod_num, stime_ticks(), pnet );
	sundog_midi_client_close_port( &pnet->midi_client, mod->midi_out );
    }
    mod->midi_out = -1;
    smem_free( mod->midi_out_name );
    mod->midi_out_name = NULL;
    for( uint i = 0; i < mod->ctls_num; i++ )
    {
	psynth_ctl* ctl = &mod->ctls[ i ];
	if( PSYNTH_MIDI_PARS1_GET_TYPE( ctl->midi_pars1 ) != psynth_midi_none )
	{
	    psynth_set_ctl_midi_in( mod_num, i, PSYNTH_CTL_MIDI_PARS1_DEFAULT, PSYNTH_CTL_MIDI_PARS2_DEFAULT, pnet );
	}
    }
    if( mod->input_links_num && mod->input_links )
    {
	for( int i = 0; i < mod->input_links_num; i++ )
	{
	    int input_mod_num = mod->input_links[ i ];
	    if( (unsigned)input_mod_num < (unsigned)pnet->mods_num )
	    {
		bool output_changed = 0;
		psynth_module* input_module = &pnet->mods[ input_mod_num ];
		if( input_module->output_links_num && input_module->output_links )
		{
		    for( int l = 0; l < input_module->output_links_num; l++ )
		    {
			if( (unsigned)input_module->output_links[ l ] == mod_num )
			{
			    input_module->output_links[ l ] = -1;
			    output_changed = 1;
			}
		    }
		}
		if( output_changed ) psynth_do_command( input_mod_num, PS_CMD_OUTPUT_LINKS_CHANGED, pnet );
	    }
	}
	smem_free( mod->input_links );
	mod->input_links = 0;
	mod->input_links_num = 0;
	psynth_do_command( mod_num, PS_CMD_INPUT_LINKS_CHANGED, pnet );
    }
    if( mod->output_links_num && mod->output_links )
    {
	for( int i = 0; i < mod->output_links_num; i++ )
	{
	    int output_mod_num = mod->output_links[ i ];
	    if( (unsigned)output_mod_num < (unsigned)pnet->mods_num )
	    {
		bool input_changed = 0;
		psynth_module* output_module = &pnet->mods[ output_mod_num ];
		if( output_module->input_links_num && output_module->input_links )
		{
		    for( int l = 0; l < output_module->input_links_num; l++ )
		    {
			if( (unsigned)output_module->input_links[ l ] == mod_num )
			{
			    output_module->input_links[ l ] = -1;
			    input_changed = 1;
			}
		    }
		}
		if( input_changed ) psynth_do_command( output_mod_num, PS_CMD_INPUT_LINKS_CHANGED, pnet );
	    }
	}
	smem_free( mod->output_links );
	mod->output_links = 0;
	mod->output_links_num = 0;
	psynth_do_command( mod_num, PS_CMD_OUTPUT_LINKS_CHANGED, pnet );
    }
    smem_free( mod->ctls );
    mod->ctls = 0;
    mod->ctls_num = 0;
    if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
	smutex_destroy( &mod->mutex );
    mod->flags = 0;
}
void psynth_remove_empty_modules_at_the_end( psynth_net* pnet )
{
    int empty_mods = 0; 
    for( uint i = pnet->mods_num - 1; i >= 1; i-- )
    {
        psynth_module* m = &pnet->mods[ i ];
        if( ( m->flags & PSYNTH_FLAG_EXISTS ) == 0 )
            empty_mods++;
        else
            break;
    }
    if( empty_mods )
    {
	pnet->mods_num -= empty_mods;
	pnet->mods = SMEM_ZRESIZE2( pnet->mods, psynth_module, pnet->mods_num );
    }
}
smutex* psynth_get_mutex( uint mod_num, psynth_net* pnet )
{
    psynth_module* mod;
    if( (unsigned)mod_num < (unsigned)pnet->mods_num )
    {
        mod = &pnet->mods[ mod_num ];
        if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
        {
    	    return &mod->mutex;
        }
        else
        {
    	    slog( "Module %s has no mutex!\n", mod->name );
        }
    }
    return 0;
}
int psynth_get_module_by_name( const char* name, psynth_net* pnet )
{
    int retval = -1;
    for( uint i = 0; i < pnet->mods_num; i++ )
    {
	psynth_module* mod = &pnet->mods[ i ];
	if( !( mod->flags & PSYNTH_FLAG_EXISTS ) ) continue;
	if( smem_strcmp( mod->name, name ) == 0 ) return i;
    }
    return retval;
}
void psynth_rename( uint mod_num, const char* name, psynth_net* pnet )
{
    if( !name ) return;
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return;
    pnet->change_counter++;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( !( mod->flags & PSYNTH_FLAG_EXISTS ) ) return;
    for( size_t i = 0; i < smem_strlen( name ) + 1 && i < sizeof( mod->name ) - 1; i++ )
    {
	mod->name[ i ] = name[ i ];
    }
}
void psynth_set_flags( uint mod_num, uint32_t set, uint32_t reset, psynth_net* pnet )
{
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    volatile uint32_t new_flags = ( mod->flags & (~reset) ) | set;
    mod->flags = new_flags;
}
void psynth_set_flags2( uint mod_num, uint32_t set, uint32_t reset, psynth_net* pnet )
{
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    volatile uint32_t new_flags2 = ( mod->flags2 & (~reset) ) | set;
    mod->flags2 = new_flags2;
}
void psynth_add_link( bool input, uint mod_num, int link, int link_slot, psynth_net* pnet )
{
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return;
    if( (unsigned)link >= (unsigned)pnet->mods_num ) return;
    pnet->change_counter++;
    pnet->change_counter2++;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( !( mod->flags & PSYNTH_FLAG_EXISTS ) ) return;
    int* links;
    int links_num;
    if( input )
    {
	links = mod->input_links;
	links_num = mod->input_links_num;
    }
    else
    {
	links = mod->output_links;
	links_num = mod->output_links_num;
    }
    for( int i = 0; i < links_num; i++ )
        if( links[ i ] == link )
    	    return; 
    int i = 0;
    if( links_num == 0 )
    {
	if( link_slot < 0 )
	    links_num = 2;
	else
	    links_num = link_slot + 1;
	links = SMEM_ALLOC2( int, links_num );
	for( i = 0; i < links_num; i++ ) links[ i ] = -1;
    }
    if( link_slot < 0 )
    {
	for( i = 0; i < links_num; i++ )
	{
	    if( links[ i ] < 0 ) break;
	}
    }
    else
    {
	i = link_slot;
    }
    if( i >= links_num )
    {
	int new_count = i + 2;
	links = SMEM_RESIZE2( links, int, new_count );
	for( int i2 = links_num; i2 < new_count; i2++ ) 
	    links[ i2 ] = -1;
	links_num = new_count;
    }
    links[ i ] = link;
    if( input )
    {
	mod->input_links = links;
	mod->input_links_num = links_num;
	psynth_do_command( mod_num, PS_CMD_INPUT_LINKS_CHANGED, pnet );
    }
    else
    {
	mod->output_links = links;
	mod->output_links_num = links_num;
	psynth_do_command( mod_num, PS_CMD_OUTPUT_LINKS_CHANGED, pnet );
    }
}
void psynth_make_link( int out, int in, psynth_net* pnet )
{
    if( psynth_remove_link( out, in, pnet ) == 1 ) return;
    psynth_add_link( true, out, in, -1, pnet );
    psynth_add_link( false, in, out, -1, pnet );
}
void psynth_make_link( int out, int in, int out_slot, int in_slot, psynth_net* pnet )
{
    if( psynth_remove_link( out, in, pnet ) == 1 ) return;
    psynth_add_link( true, out, in, out_slot, pnet );
    psynth_add_link( false, in, out, in_slot, pnet );
}
int psynth_remove_link( int out, int in, psynth_net* pnet )
{
    pnet->change_counter++;
    pnet->change_counter2++;
    int retval = 0;
    bool in_in = 0;
    bool in_out = 0;
    bool out_in = 0;
    bool out_out = 0;
    psynth_module* mod = psynth_get_module( in, pnet );
    if( mod )
    {
	for( int i = 0; i < mod->input_links_num; i++ )
	    if( mod->input_links[ i ] == out )
	    {
		mod->input_links[ i ] = -1;
		in_in = 1;
		retval = 1;
	    }
	for( int i = 0; i < mod->output_links_num; i++ )
	    if( mod->output_links[ i ] == out )
	    {
		mod->output_links[ i ] = -1;
		in_out = 1;
		retval = 1;
	    }
    }
    mod = psynth_get_module( out, pnet );
    if( mod )
    {
	for( int i = 0; i < mod->input_links_num; i++ )
	    if( mod->input_links[ i ] == in )
	    {
		mod->input_links[ i ] = -1;
		out_in = 1;
		retval = 1;
	    }
	for( int i = 0; i < mod->output_links_num; i++ )
	    if( mod->output_links[ i ] == in )
	    {
		mod->output_links[ i ] = -1;
		out_out = 1;
		retval = 1;
	    }
    }
    if( in_in ) psynth_do_command( in, PS_CMD_INPUT_LINKS_CHANGED, pnet );
    if( in_out ) psynth_do_command( in, PS_CMD_OUTPUT_LINKS_CHANGED, pnet );
    if( out_in ) psynth_do_command( out, PS_CMD_INPUT_LINKS_CHANGED, pnet );
    if( out_out ) psynth_do_command( out, PS_CMD_OUTPUT_LINKS_CHANGED, pnet );
    return retval;
}
int psynth_check_link( int out, int in, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( in, pnet );
    if( !mod ) return 0;
    for( int i = 0; i < mod->input_links_num; i++ )
        if( mod->input_links[ i ] == out )
    	    return 2;
    for( int i = 0; i < mod->output_links_num; i++ )
        if( mod->output_links[ i ] == out )
	    return 1;
    return 0;
}
int psynth_open_midi_out( uint mod_num, char* dev_name, int channel, psynth_net* pnet )
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return 0;
    int rv = 0;
    if( pnet->mods_num && ( (unsigned)mod_num < (unsigned)pnet->mods_num ) )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	char port_name[ 128 ];
	snprintf( port_name, 128, "%d %s MIDI OUT", mod_num, mod->name );
	smem_free( mod->midi_out_name );
	mod->midi_out_name = NULL;
	if( dev_name )
	{
	    mod->midi_out_name = SMEM_ALLOC2( char, smem_strlen( dev_name ) + 1 );
	    mod->midi_out_name[ 0 ] = 0;
	    SMEM_STRCAT_D( mod->midi_out_name, dev_name );
	}
	sundog_midi_client_close_port( &pnet->midi_client, mod->midi_out );
	mod->midi_out_ch = channel;
	if( dev_name )
	{
	    mod->midi_out = sundog_midi_client_open_port( &pnet->midi_client, port_name, mod->midi_out_name, MIDI_PORT_WRITE );
	    psynth_set_midi_prog( mod_num, mod->midi_out_bank, mod->midi_out_prog, pnet );
	}
	else 
	    mod->midi_out = -1;
    }
    else
    {
	rv = -1;
    }
    return rv;
}
int psynth_set_midi_prog( uint mod_num, int bank, int prog, psynth_net* pnet )
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return 0;
    int rv = 0;
    if( pnet->mods_num && ( (unsigned)mod_num < (unsigned)pnet->mods_num ) )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	mod->midi_out_bank = bank;
	mod->midi_out_prog = prog;
#ifndef NOMIDI
	if( mod->midi_out >= 0 )
	{
	    uint8_t midi_data[ 8 ];
	    sundog_midi_event midi_evt;
	    midi_evt.t = stime_ticks();
	    midi_evt.data = midi_data;
	    midi_evt.size = 0;
	    if( mod->midi_out_bank >= 0 )
	    {
		midi_data[ 0 ] = 0xB0 | mod->midi_out_ch;
		midi_data[ 1 ] = 0; 
		midi_data[ 2 ] = (uint8_t)( mod->midi_out_bank >> 7 ); 
		midi_data[ 3 ] = 0xB0 | mod->midi_out_ch;
		midi_data[ 4 ] = 0x20; 
		midi_data[ 5 ] = (uint8_t)( mod->midi_out_bank & 127 ); 
		midi_evt.size = 6;
		sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
	    }
	    if( mod->midi_out_prog >= 0 )
	    {
		midi_data[ 0 ] = 0xC0 | mod->midi_out_ch;
		midi_data[ 1 ] = mod->midi_out_prog & 127;
		midi_evt.size = 2;
		sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
	    }
	}
#endif    
    }
    return rv;
}
void psynth_all_midi_notes_off( uint mod_num, stime_ticks_t t, psynth_net* pnet )
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return;
#ifndef NOMIDI
    if( pnet->mods_num && ( (unsigned)mod_num < (unsigned)pnet->mods_num )  )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->midi_out < 0 ) return;
	uint8_t midi_data[ 8 ];
	sundog_midi_event midi_evt;
	midi_evt.t = t;
	midi_evt.data = midi_data;
	midi_evt.size = 0;
	for( uint n = 0; n < sizeof( mod->midi_out_note ); n++ )
	{
	    if( mod->midi_out_note_id[ n ] != 0xFFFFFFFF )
	    {
		mod->midi_out_note_id[ n ] = 0xFFFFFFFF;
		midi_data[ 0 ] = 0x80 | mod->midi_out_ch;
		midi_data[ 1 ] = mod->midi_out_note[ n ] & 0x7F;
		midi_data[ 2 ] = 127;
		midi_evt.size = 3;
		sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
	    }
	}
    }
#endif
}
void psynth_reset_events( psynth_net* pnet )
{
#ifdef PSYNTH_MULTITHREADED
    if( atomic_exchange( &pnet->events_num, 0 ) == 0 ) return;
#else
    if( pnet->events_num == 0 ) return;
    pnet->events_num = 0;
#endif
    for( uint i = 0; i < pnet->mods_num; i++ )
    {
	psynth_module* mod = &pnet->mods[ i ];
	if( mod->flags & PSYNTH_FLAG_EXISTS )
	{
	    mod->events_num = 0;
	}
    }
}
void psynth_add_event( uint mod_num, psynth_event* evt, psynth_net* pnet )
{
    if( mod_num >= pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return;
#ifdef PSYNTH_MULTITHREADED
    int events_num = atomic_fetch_add( &pnet->events_num, 1 );
#else
    int events_num = pnet->events_num++;
#endif
    if( events_num >= (int)smem_get_size( pnet->events_heap ) / (int)sizeof( psynth_event ) )
    {
#ifdef PSYNTH_MULTITHREADED
	if( pnet->th_num == 1 )
#else
	if( 1 )
#endif
	{
#ifdef EVT_HEAP_DEBUG_MESSAGES
	    printf( "EVT HEAP RESIZE: %d -> %d\n", (int)( smem_get_size( pnet->events_heap ) / sizeof( psynth_event ) ), (int)events_num * 2 );
#endif
	    pnet->events_heap = SMEM_RESIZE2( pnet->events_heap, psynth_event, events_num * 2 );
	}
	else
	{
#ifdef EVT_HEAP_DEBUG_MESSAGES
	    printf( "EVT HEAP OVERFLOW\n" );
#endif
	    return;
	}
    }
    if( mod->events_num >= smem_get_size( mod->events ) / sizeof( int ) )
    {
#ifdef EVT_HEAP_DEBUG_MESSAGES
	printf( "EVT HEAP (%s) RESIZE: %d -> %d\n", mod->name, (int)( smem_get_size( mod->events ) / sizeof( int ) ), (int)mod->events_num * 2 );
#endif
	mod->events = SMEM_RESIZE2( mod->events, int, mod->events_num * 2 );
    }
    mod->events[ mod->events_num++ ] = events_num;
    pnet->events_heap[ events_num ] = *evt;
}
void psynth_multisend( psynth_module* mod, psynth_event* evt, psynth_net* pnet )
{
    for( int i = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
        psynth_module* dest = psynth_get_module( l, pnet );
        if( dest ) psynth_add_event( l, evt, pnet );
    }
}
void psynth_multisend_pitch( psynth_module* mod, psynth_event* evt, psynth_net* pnet, int pitch )
{
    for( int i = 0; i < mod->output_links_num; i++ )
    {
        int l = mod->output_links[ i ];
        psynth_module* dest = psynth_get_module( l, pnet );
        if( dest )
        {
    	    evt->note.pitch = pitch - dest->finetune - dest->relative_note * 256;
    	    psynth_add_event( l, evt, pnet );
    	}
    }
}
static void psynth_cpu_usage_clean( psynth_net* pnet )
{
    pnet->cpu_usage_t1 = stime_ticks();
    if( pnet->cpu_usage_enable & 1 )
	for( uint i = 0; i < pnet->mods_num; i++ )
	{
	    psynth_module* mod = &pnet->mods[ i ];
	    if( mod->flags & PSYNTH_FLAG_EXISTS )
		mod->cpu_usage_ticks = 0;
	}
}
typedef float cpu_usage_t;
static void psynth_cpu_usage_recalc( int frames, psynth_net* pnet )
{
    if( frames <= 16 ) return; 
    stime_ticks_t cpu_usage_ticks_alt = 0; 
    cpu_usage_t len = (cpu_usage_t)frames / pnet->sampling_freq; 
    if( pnet->cpu_usage_enable & 1 )
	for( uint i = 0; i < pnet->mods_num; i++ )
	{
	    psynth_module* mod = &pnet->mods[ i ];
	    if( mod->flags & PSYNTH_FLAG_EXISTS )
    	    {
    		cpu_usage_ticks_alt += mod->cpu_usage_ticks;
		cpu_usage_t usage = (cpu_usage_t)mod->cpu_usage_ticks / stime_ticks_per_second(); 
		usage = ( usage / len ) * 100; 
		if( usage > 100 ) usage = 100;
		if( usage > mod->cpu_usage )
		    mod->cpu_usage = usage;
	    }
	}
    stime_ticks_t cpu_usage_ticks = stime_ticks() - pnet->cpu_usage_t1;
    cpu_usage_t cpu_usage = (cpu_usage_t)cpu_usage_ticks / stime_ticks_per_second() / len * 100;
    if( cpu_usage > 100 ) cpu_usage = 100;
    if( cpu_usage > pnet->cpu_usage1 ) pnet->cpu_usage1 = cpu_usage;
    if( cpu_usage > pnet->cpu_usage2 ) pnet->cpu_usage2 = cpu_usage;
}
PS_STYPE* psynth_get_scope_buffer( int ch, int* offset, int* size, uint mod_num, stime_ticks_t t, psynth_net* pnet )
{
    psynth_module* mod = &pnet->mods[ mod_num ];
    int channels;
    PS_STYPE* data;
    if( mod->flags & PSYNTH_FLAG_OUTPUT )
    {
	channels = mod->input_channels;
	data = mod->channels_in[ ch ];
    }
    else
    {
	channels = mod->output_channels;
	data = mod->channels_out[ ch ];
    }
    if( (unsigned)ch >= (unsigned)channels ) return NULL;
    if( !data ) return NULL;
    PS_STYPE* buf = mod->scope_buf[ ch ];
    if( buf )
    {
	if( offset )
	{
#ifdef PSYNTH_SCOPE_MODE_SLOW_HQ
	    stime_ticks_t t_buf_start = 0;
	    int part = 0;
	    for( int i = 0; i < PSYNTH_SCOPE_PARTS; i++ )
	    {
		stime_ticks_t t_buf_start2 = pnet->scope_buf_start_time[ i ];
		if( ( t_buf_start2 - t ) & ( 1 << ( sizeof( stime_ticks_t ) * 8 - 1 ) ) ) 
		{
		    if( t_buf_start == 0 || ( t - t_buf_start2 ) < ( t - t_buf_start ) ) 
		    {
			t_buf_start = t_buf_start2;
			part = i;
		    }
		}
	    }
	    *offset = ( ( t - t_buf_start ) * pnet->sampling_freq ) / stime_ticks_per_second();
	    *offset += pnet->scope_buf_ptr[ part ];
#else
	    *offset = pnet->scope_buf_cur_ptr;
#endif
	}
	if( size )
	    *size = PSYNTH_SCOPE_SIZE;
	return buf;
    }
    return 0;
}
static void psynth_change_scope_buffers( stime_ticks_t t, psynth_net* pnet )
{
#ifdef PSYNTH_SCOPE_MODE_SLOW_HQ
    pnet->scope_buf_current++;
    pnet->scope_buf_current &= ( PSYNTH_SCOPE_PARTS - 1 );
    pnet->scope_buf_ptr[ pnet->scope_buf_current ] = pnet->scope_buf_cur_ptr;
    pnet->scope_buf_start_time[ pnet->scope_buf_current ] = t;
#endif
}
static void psynth_fill_scope_buffers( int buf_size, psynth_net* pnet )
{
    int scope_ptr_start = pnet->scope_buf_cur_ptr;
    for( uint i = 0; i < pnet->mods_num; i++ )
    {
	psynth_module* mod = &pnet->mods[ i ];
	int num_channels = mod->output_channels;
	PS_STYPE** channels_arr = mod->channels_out;
	int* empty_arr = mod->out_empty;
	if( mod->flags & PSYNTH_FLAG_OUTPUT )
	{
	    num_channels = mod->input_channels;
	    channels_arr = mod->channels_in;
	    empty_arr = mod->in_empty;
	}
	for( int ch = 0; ch < num_channels; ch++ )
	{
	    PS_STYPE* data = channels_arr[ ch ];
	    if( !data ) break;
	    PS_STYPE* scope_buf = mod->scope_buf[ ch ];
	    if( !scope_buf ) break;
	    int empty = empty_arr[ ch ];
	    int scope_ptr = scope_ptr_start;
	    if( empty > 0 )
	    {
		int i2 = 0;
		while( 1 )
		{
		    int size = PSYNTH_SCOPE_SIZE - scope_ptr;
		    if( size > buf_size - i2 ) size = buf_size - i2;
		    memset( scope_buf + scope_ptr, 0, size * sizeof( PS_STYPE ) );
		    i2 += size;
		    if( i2 >= buf_size ) break;
		    scope_ptr = ( scope_ptr + size ) & ( PSYNTH_SCOPE_SIZE - 1 );
		}
	    }
	    else
	    {
		int i2 = 0;
		while( 1 )
		{
		    int size = PSYNTH_SCOPE_SIZE - scope_ptr;
		    if( size > buf_size - i2 ) size = buf_size - i2;
		    memcpy( scope_buf + scope_ptr, data + i2, size * sizeof( PS_STYPE ) );
		    i2 += size;
		    if( i2 >= buf_size ) break;
		    scope_ptr = ( scope_ptr + size ) & ( PSYNTH_SCOPE_SIZE - 1 );
		}
	    }
	}
    }
    pnet->scope_buf_cur_ptr = ( scope_ptr_start + buf_size ) & ( PSYNTH_SCOPE_SIZE - 1 );
    int fft_mod = pnet->fft_mod;
    if( pnet->mods_num >= 1 && (unsigned)fft_mod < (unsigned)pnet->mods_num && pnet->fft )
    {
	psynth_module* out = &pnet->mods[ fft_mod ];
	if( ( out->flags & PSYNTH_FLAG_EXISTS ) != 0 && pnet->fft_locked == 0 )
	{
	    int size = pnet->fft_size - pnet->fft_ptr;
	    if( size > buf_size ) size = buf_size;
	    PS_STYPE* fft_buf = pnet->fft;
	    int fft_ptr = pnet->fft_ptr;
	    int num_channels = out->output_channels;
	    if( out->flags & PSYNTH_FLAG_OUTPUT )
		num_channels = out->input_channels;
	    for( int ch = 0; ch < num_channels; ch++ )
	    {
		fft_ptr = pnet->fft_ptr;
		PS_STYPE* data;
		if( out->flags & PSYNTH_FLAG_OUTPUT )
		    data = out->channels_in[ ch ];
		else
		    data = out->channels_out[ ch ];
		if( data )
		{
		    if( ch == 0 )
		    {
			for( int i = 0; i < size; i++ )
			{
			    fft_buf[ fft_ptr ] = data[ i ];
			    fft_ptr++;
			}
		    }
		    else
		    {
			for( int i = 0; i < size; i++ )
			{
			    PS_STYPE2 v = fft_buf[ fft_ptr ];
			    v += data[ i ];
			    v /= 2;
			    fft_buf[ fft_ptr ] = (PS_STYPE)v;
			    fft_ptr++;
			}
		    }
		}
	    }
	    pnet->fft_ptr = fft_ptr;
	    if( fft_ptr >= pnet->fft_size )
	    {
		pnet->fft_ptr = 0;
		pnet->fft_locked = 1;
	    }
	}
    }
}
#define PS_STYPE_BUF_CLEAN( BUF, BEGIN, END ) \
    if( (END) > (BEGIN) ) smem_clear( (BUF) + (BEGIN), ( (END) - (BEGIN) ) * sizeof( PS_STYPE ) );
#define PS_STYPE_BUF_COPY( DEST, SRC, BEGIN, END ) \
    if( (END) > (BEGIN) ) smem_copy( (DEST) + (BEGIN), (SRC) + (BEGIN), ( (END) - (BEGIN) ) * sizeof( PS_STYPE ) );
static void psynth_set_output_content( int offset, int size, int channels_filled, psynth_module* mod )
{
    int end = offset + size;
    for( int c = 0; c < mod->output_channels; c++ )
    {
        PS_STYPE* ch = mod->channels_out[ c ];
        if( !ch ) continue;
	int filled = channels_filled;
        if( filled == 3 )
        {
    	    filled = 2; 
    	    for( int i = offset; i < end; i++ )
    	    {
    		if( PS_STYPE_IS_NOT_MIN( ch[ i ] ) )
    		{
    		    filled = 1; 
    		    break;
    		}
    	    }
        }
        if( filled == 1 )
        {
    	    if( mod->out_empty[ c ] > offset )
    	        mod->out_empty[ c ] = offset;
    	}
    	else
    	{
    	    int offset2 = offset;
    	    if( offset2 <= mod->out_empty[ c ] )
    	    {
    	    	offset2 = mod->out_empty[ c ];
    	    	if( end > mod->out_empty[ c ] )
    	    	    mod->out_empty[ c ] = end;
    	    }
    	    if( filled == 0 )
    	    {
    		PS_STYPE_BUF_CLEAN( ch, offset2, end );
    	    }
    	}
    }
}
static void psynth_set_input_content( int offset, int size, bool filled, psynth_module* mod )
{
    int end = offset + size;
    for( int c = 0; c < mod->input_channels; c++ )
    {
        PS_STYPE* ch = mod->channels_in[ c ];
        if( ch )
        {
    	    if( filled == false )
    	    {
    		int offset2 = offset;
    		if( offset2 <= mod->in_empty[ c ] )
    		{
    		    offset2 = mod->in_empty[ c ];
    		    if( end > mod->in_empty[ c ] )
    			mod->in_empty[ c ] = end;
    		}
    		PS_STYPE_BUF_CLEAN( ch, offset2, end );
            }
            else
            {
    		if( mod->in_empty[ c ] > offset )
    		    mod->in_empty[ c ] = offset;
            }
        }
    }
}
#define RENDER_SET_OUTPUT_CONTENT( BEGIN, SIZE ) \
    psynth_set_output_content( BEGIN, SIZE, res, mod ); \
    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) psynth_set_output_content( BEGIN, SIZE, 0, mod );
static inline void psynth_set_ctl( psynth_module* mod, psynth_event* evt )
{
    uint ctl_num = evt->controller.ctl_num;
    if( ctl_num < mod->ctls_num )
    {
	psynth_ctl* ctl = &mod->ctls[ ctl_num ];
    	if( ctl->val )
    	{
	    uint ctl_val = evt->controller.ctl_val;
	    if( ctl_val > 0x8000 ) ctl_val = 0x8000;
	    if( ctl->type == 0 )
	    {
    		uint val = (uint)( ctl->max - ctl->min ) * ctl_val;
    		val >>= 15;
    		*ctl->val = (PS_CTYPE)( val + ctl->min );
	    }
	    else
	    {
    		*ctl->val = (PS_CTYPE)ctl_val;
    		if( *ctl->val < ctl->min ) *ctl->val = ctl->min;
    		if( *ctl->val > ctl->max ) *ctl->val = ctl->max;
	    }
	}
    }
}
void psynth_set_ctl2( psynth_module* mod, psynth_event* evt )
{
    psynth_set_ctl( mod, evt );
}
static void psynth_set_msb( psynth_module* mod, psynth_event* evt )
{
    if( evt->mute.flags & PSYNTH_EVENT_MUTE )
    {
        mod->flags |= PSYNTH_FLAG_MUTE;
	if( !( mod->flags & PSYNTH_FLAG_IGNORE_MUTE ) )
	    mod->realtime_flags |= PSYNTH_RT_FLAG_MUTE;
    }
    if( evt->mute.flags & PSYNTH_EVENT_SOLO )
    {
        mod->flags |= PSYNTH_FLAG_SOLO;
    }
    if( evt->mute.flags & PSYNTH_EVENT_BYPASS )
    {
        mod->flags |= PSYNTH_FLAG_BYPASS;
	mod->realtime_flags |= PSYNTH_RT_FLAG_BYPASS;
    }
}
static void psynth_reset_msb( psynth_module* mod, psynth_event* evt )
{
    psynth_net* pnet = mod->pnet;
    if( evt->mute.flags & PSYNTH_EVENT_MUTE )
    {
        mod->flags &= ~PSYNTH_FLAG_MUTE;
	if( !( mod->flags & PSYNTH_FLAG_IGNORE_MUTE ) && !pnet->all_modules_muted )
	    mod->realtime_flags &= ~PSYNTH_RT_FLAG_MUTE;
    }
    if( evt->mute.flags & PSYNTH_EVENT_SOLO )
    {
        mod->flags &= ~PSYNTH_FLAG_SOLO;
    }
    if( evt->mute.flags & PSYNTH_EVENT_BYPASS )
    {
        mod->flags &= ~PSYNTH_FLAG_BYPASS;
	mod->realtime_flags &= ~PSYNTH_RT_FLAG_BYPASS;
    }
}
static void psynth_finetune( psynth_module* mod, psynth_event* evt )
{
    bool changed = false;
    if( evt->finetune.finetune >= -256 )
    if( mod->finetune != evt->finetune.finetune )
    {
        mod->finetune = evt->finetune.finetune;
	changed = true;
    }
    if( evt->finetune.relative_note >= -256 )
    if( mod->relative_note != evt->finetune.relative_note )
    {
        mod->relative_note = evt->finetune.relative_note;
        changed = true;
    }
    mod->draw_request += changed;
}
static void psynth_midimsg_support( psynth_module* mod, psynth_event* evt )
{
    int msg = evt->midimsg_support.msg;
    if( msg >= 0 && msg < PSYNTH_MIDIMSG_MAP_SIZE )
    {
	for( int i = 0; i < PSYNTH_MIDIMSG_MAP_SIZE; i++ )
	{
	    if( mod->midi_out_msg_map.ctl[ i ] == evt->midimsg_support.ctl )
		mod->midi_out_msg_map.ctl[ i ] = 0;
	}
	mod->midi_out_msg_map.ctl[ msg ] = evt->midimsg_support.ctl;
	mod->midi_out_msg_map.active = 0;
	for( int i = 0; i < PSYNTH_MIDIMSG_MAP_SIZE; i++ )
	{
	    if( mod->midi_out_msg_map.ctl[ i ] )
	    {
		mod->midi_out_msg_map.active = 1;
		break;
	    }
	}
    }
}
void psynth_render_begin( stime_ticks_t out_time, psynth_net* pnet )
{
    if( pnet->cpu_usage_enable )
    {
	psynth_cpu_usage_clean( pnet );
    }
    psynth_change_scope_buffers( out_time, pnet );
}
void psynth_render_end( int frames, psynth_net* pnet )
{
    if( pnet->cpu_usage_enable )
    {
	psynth_cpu_usage_recalc( frames, pnet );
    }
}
void psynth_render_setup( int buf_size, stime_ticks_t out_time, void* in_buf, sound_buffer_type in_buf_type, int in_buf_channels, psynth_net* pnet )
{
    pnet->buf_size = buf_size;
    pnet->out_time = out_time;
    pnet->in_buf = in_buf;
    pnet->in_buf_type = in_buf_type;
    pnet->in_buf_channels = in_buf_channels;
    pnet->render_counter++;
}
static int psynth_render( int start_mod, psynth_net* pnet )
{
    int retval = 0;
    psynth_module* mod = &pnet->mods[ start_mod ];
    if( !( mod->flags & PSYNTH_FLAG_EXISTS ) ) return -1;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_RENDERED ) return 0;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_LOCKED ) return -1;
    int buf_size = pnet->buf_size;
    psynth_module* in;
    mod->realtime_flags |= PSYNTH_RT_FLAG_LOCKED;
    mod->realtime_flags &= ~( PSYNTH_RT_FLAG_MUTE | PSYNTH_RT_FLAG_SOLO | PSYNTH_RT_FLAG_BYPASS );
    bool dont_fill_input;
    if( !( mod->flags & PSYNTH_FLAG_IGNORE_MUTE ) )
    {
	if( mod->flags & PSYNTH_FLAG_MUTE ) mod->realtime_flags |= PSYNTH_RT_FLAG_MUTE;
	if( mod->flags & PSYNTH_FLAG_SOLO ) mod->realtime_flags |= PSYNTH_RT_FLAG_SOLO;
	if( mod->flags & PSYNTH_FLAG_BYPASS ) mod->realtime_flags |= PSYNTH_RT_FLAG_BYPASS;
    }
    if( mod->flags & PSYNTH_FLAG_DONT_FILL_INPUT ) dont_fill_input = 1; else dont_fill_input = 0;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS ) dont_fill_input = 0;
    bool input_rendered = false;
    for( int inp = 0; inp < mod->input_links_num; inp++ )
    {
	int input_mod_num = mod->input_links[ inp ];
	if( (unsigned)input_mod_num < pnet->mods_num )
	{
	    in = &pnet->mods[ input_mod_num ];
	    if( mod->flags & PSYNTH_FLAG_FEEDBACK )
	    {
		if( in->flags & PSYNTH_FLAG_FEEDBACK )
		{
		    if( in->realtime_flags & PSYNTH_RT_FLAG_SOLO ) mod->realtime_flags |= PSYNTH_RT_FLAG_SOLO;
		    continue;
		}
	    }
	    if( !( in->realtime_flags & PSYNTH_RT_FLAG_RENDERED ) )
	    {
		if( psynth_render( input_mod_num, pnet ) != 0 )
		{
		    continue;
		}
	    }
	    if( in->realtime_flags & PSYNTH_RT_FLAG_SOLO ) mod->realtime_flags |= PSYNTH_RT_FLAG_SOLO;
	    if( in->realtime_flags & PSYNTH_RT_FLAG_RENDERED )
	    {
		PS_STYPE* prev_in_channel = nullptr;
		int prev_ch = 0;
		if( !input_rendered )
		{
		    for( int ch = 0; ch < mod->input_channels; ch++ )
		    {
			PS_STYPE* in_data;
			PS_STYPE* out_data = mod->channels_in[ ch ];
			int in_ch;
			if( ch >= in->output_channels ) 
			{ 
			    in_data = prev_in_channel; 
			    in_ch = prev_ch; 
			}
			else
			{
			    in_data = in->channels_out[ ch ];
			    in_ch = ch;
			}
			prev_in_channel = in_data;
			prev_ch = in_ch;
			if( in_data && out_data )
			{
			    if( in->out_empty[ in_ch ] < buf_size )
			    {
				if( !dont_fill_input )
				{
				    PS_STYPE_BUF_CLEAN( out_data, mod->in_empty[ ch ], in->out_empty[ in_ch ] );
				    PS_STYPE_BUF_COPY( out_data, in_data, in->out_empty[ in_ch ], buf_size );
				}
				mod->in_empty[ ch ] = in->out_empty[ in_ch ];
			    }
			    else
			    {
				if( !dont_fill_input )
				{
			    	    PS_STYPE_BUF_CLEAN( out_data, mod->in_empty[ ch ], buf_size );
			    	}
				mod->in_empty[ ch ] = buf_size;
			    }
			    if( mod->in_empty[ ch ] < buf_size )
				input_rendered = true;
			}
		    }
		}
		else
		{
		    for( int ch = 0; ch < mod->input_channels; ch++ )
		    {
			PS_STYPE* in_data = in->channels_out[ ch ]; if( ch >= in->output_channels ) in_data = 0;
			PS_STYPE* out_data = mod->channels_in[ ch ];
			int in_ch = ch;
			if( in_data == 0 ) { in_data = prev_in_channel; in_ch = prev_ch; }
			prev_in_channel = in_data;
			prev_ch = in_ch;
			if( in_data && out_data )
			{
			    if( !dont_fill_input )
			    {
				for( int i = in->out_empty[ in_ch ]; i < buf_size; i++ )
				{
				    out_data[ i ] += in_data[ i ];
				}
			    }
			    if( in->out_empty[ in_ch ] < mod->in_empty[ ch ] )
				mod->in_empty[ ch ] = in->out_empty[ in_ch ];
			}
		    }
		}
	    }
	}
    }
    if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
    {
	if( smutex_trylock( &mod->mutex ) != 0 )
	{
    	    psynth_set_output_content( 0, buf_size, 0, mod ); 
    	    goto ignore_module;
    	}
    }
    if( !input_rendered && ( mod->flags & PSYNTH_FLAG_EFFECT ) && !( mod->realtime_flags & PSYNTH_RT_FLAG_DONT_CLEAN_INPUT ) )
    {
        for( int c = 0; c < mod->input_channels; c++ )
        {
	    PS_STYPE* ch = mod->channels_in[ c ];
	    if( ch ) 
	    {
		if( buf_size > mod->in_empty[ c ] )
		{
		    PS_STYPE_BUF_CLEAN( ch, mod->in_empty[ c ], buf_size );
		    mod->in_empty[ c ] = buf_size;
		}
	    }
	}
    }
    if( mod->flags & PSYNTH_FLAG_INITIALIZED )
    {
        stime_ticks_t synth_start_time = 0;
        if( pnet->cpu_usage_enable & 1 )
        {
            synth_start_time = stime_ticks();
        }
	if( !( mod->flags & PSYNTH_FLAG_IGNORE_MUTE ) )
	{
	    if( pnet->all_modules_muted )
	    {
		if( ( mod->realtime_flags & PSYNTH_RT_FLAG_SOLO ) && ( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) == 0 )
		{
		}
		else
		    mod->realtime_flags |= PSYNTH_RT_FLAG_MUTE;
	    }
	    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE )
	    {
		if( mod->flags2 & PSYNTH_FLAG2_GET_MUTED_COMMANDS )
		{
		    psynth_event module_evt;
		    SMEM_CLEAR_STRUCT( module_evt );
		    module_evt.command = PS_CMD_MUTED;
		    mod->handler( start_mod, &module_evt, pnet );
		}
	    }
	}
	psynth_handler_t mod_handler2 = mod->handler; 
	if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS ) mod_handler2 = psynth_bypass;
	if( mod->events_num == 0 )
	{
	    if( !( mod->flags & PSYNTH_FLAG_NO_RENDER ) )
	    {
		mod->frames = buf_size;
		mod->offset = 0;
		psynth_event module_evt;
		module_evt.command = PS_CMD_RENDER_REPLACE;
		int res = mod_handler2( start_mod, &module_evt, pnet );
		RENDER_SET_OUTPUT_CONTENT( 0, buf_size ); 
	    }
	}
	else
	{
	    for( uint i = 1; i < mod->events_num; i++ )
	    {
		int key_evt_num = mod->events[ i ];
		int key = pnet->events_heap[ key_evt_num ].offset;
		uint j = i;
		while( j > 0 && pnet->events_heap[ mod->events[ j - 1 ] ].offset > key )
		{
		    mod->events[ j ] = mod->events[ j - 1 ];
		    j--;
		}
		if( j != i ) mod->events[ j ] = key_evt_num;
	    }
#ifndef NOMIDI
	    if( mod->midi_out >= 0 && !( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) )
	    {
		for( uint i = 0; i < mod->events_num; i++ )
		{
		    psynth_event* evt = &pnet->events_heap[ mod->events[ i ] ];
		    stime_ticks_t t = pnet->out_time + ( evt->offset * stime_ticks_per_second() ) / pnet->sampling_freq;
		    uint8_t midi_data[ 16 ];
		    sundog_midi_event midi_evt;
		    midi_evt.t = t;
		    midi_evt.data = midi_data;
		    midi_evt.size = 0;
		    switch( evt->command )
		    {
			case PS_CMD_NOTE_ON:
			    {
				int n;
				int free_n = -1;
				for( n = 0; n < (int)sizeof( mod->midi_out_note ); n++ )
				{
				    if( mod->midi_out_note_id[ n ] == 0xFFFFFFFF && free_n == -1 )
					free_n = n;
				    if( mod->midi_out_note_id[ n ] == evt->id )
				    {
					free_n = n;
					break;
				    }
				}
				if( free_n >= 0 )
				{
				    if( mod->midi_out_note_id[ free_n ] != 0xFFFFFFFF )
				    {
					mod->midi_out_note_id[ free_n ] = 0xFFFFFFFF;
					midi_data[ 0 ] = 0x80 | mod->midi_out_ch;
					midi_data[ 1 ] = mod->midi_out_note[ free_n ] & 0x7F;
					midi_data[ 2 ] = 127;
					midi_evt.size = 3;
					sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
	    			    }
				    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
				    mod->midi_out_note_id[ free_n ] = evt->id;
				    mod->midi_out_note[ free_n ] = ( PS_NOTE0_PITCH - evt->note.pitch ) / 256;
				    midi_data[ 0 ] = 0x90 | mod->midi_out_ch;
				    midi_data[ 1 ] = mod->midi_out_note[ free_n ] & 0x7F;
				    midi_data[ 2 ] = evt->note.velocity / 2;
				    if( midi_data[ 2 ] > 127 ) midi_data[ 2 ] = 127;
				    midi_evt.size = 3;
				    sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
				}
			    }
			    break;
			case PS_CMD_NOTE_OFF:
			    {
				int n;
				int free_n = -1;
				for( n = 0; n < (int)sizeof( mod->midi_out_note ); n++ )
				{
				    if( mod->midi_out_note_id[ n ] == evt->id )
				    {
					free_n = n;
					break;
				    }
				}
				if( free_n >= 0 )
				{
				    mod->midi_out_note_id[ free_n ] = 0xFFFFFFFF;
				    midi_data[ 0 ] = 0x80 | mod->midi_out_ch;
				    midi_data[ 1 ] = mod->midi_out_note[ free_n ] & 0x7F;
				    midi_data[ 2 ] = 127;
				    midi_evt.size = 3;
				    sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
				}
			    }
			    break;
			case PS_CMD_ALL_NOTES_OFF:
			    {
				psynth_all_midi_notes_off( start_mod, t, pnet );
			    }
			    break;
			case PS_CMD_CLEAN:
			    if( ( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) == 0 )
			    {
				psynth_all_midi_notes_off( start_mod, t, pnet );
				midi_data[ 0 ] = 0xB0 | mod->midi_out_ch;
				midi_data[ 1 ] = 0x78; 
				midi_data[ 2 ] = 0;
				midi_evt.size = 3;
				sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
			    }
			    break;
			case PS_CMD_SET_LOCAL_CONTROLLER:
			case PS_CMD_SET_GLOBAL_CONTROLLER:
			    if( ( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) == 0 )
			    {
				uint ctl_num1 = evt->controller.ctl_num + 1;
				uint ctl_val = evt->controller.ctl_val;
				if( ctl_num1 >= 0x80 )
				{
				    int msg_binding = -1;
				    if( mod->midi_out_msg_map.active )
				    {
					for( int p = 0; p < PSYNTH_MIDIMSG_MAP_SIZE; p++ )
        				{
			                    if( mod->midi_out_msg_map.ctl[ p ] == ctl_num1 )
            				    {
            					msg_binding = p;
                				break;
            				    }
        				}
        			    }
        			    if( msg_binding >= 0 )
        			    {
        				int v;
					switch( msg_binding )
				        {
					    case PSYNTH_MIDIMSG_MAP_PROG:
						midi_data[ 0 ] = 0xC0 | mod->midi_out_ch;
						v = ctl_val >> 8; if( v > 127 ) v = 127;
						midi_data[ 1 ] = v;
						midi_evt.size = 2;
						break;
				    	    case PSYNTH_MIDIMSG_MAP_PRESSURE:
						midi_data[ 0 ] = 0xD0 | mod->midi_out_ch;
						v = ctl_val >> 8; if( v > 127 ) v = 127;
						midi_data[ 1 ] = v;
						midi_evt.size = 2;
				    		break;
					    case PSYNTH_MIDIMSG_MAP_PITCH:
						midi_data[ 0 ] = 0xE0 | mod->midi_out_ch;
				    		v = ctl_val >> 1; if( v >= 16384 ) v = 16383;
						midi_data[ 1 ] = v & 127; 
						midi_data[ 2 ] = v >> 7; 
						midi_evt.size = 3;
						break;
				        }
        			    }
        			    else
        			    {
					ctl_num1 -= 0x80;
					midi_data[ 0 ] = 0xB0 | mod->midi_out_ch;
					midi_data[ 1 ] = ctl_num1; 
					if( ctl_num1 < 32 && !pnet->midi_out_7bit )
					{
				    	    uint32_t v = ctl_val >> 1;
					    if( v >= 16384 ) v = 16383;
					    midi_data[ 2 ] = v >> 7; 
					    midi_data[ 3 ] = 0xB0 | mod->midi_out_ch;
					    midi_data[ 4 ] = ctl_num1 + 32; 
					    midi_data[ 5 ] = v & 127; 
					    midi_evt.size = 6;
					}
					else
					{
					    midi_data[ 2 ] = ctl_val >> 8;
					    if( midi_data[ 2 ] > 127 ) midi_data[ 2 ] = 127;
					    midi_evt.size = 3;
					}
				    }
				    sundog_midi_client_send_event( &pnet->midi_client, mod->midi_out, &midi_evt );
				}
			    }
			    break;
			default: break;
		    }
		}
	    }
#endif
	    mod->offset = 0;
	    for( uint i = 0; i < mod->events_num; i++ )
	    {
		int evt_num = mod->events[ i ];
		mod->frames = pnet->events_heap[ evt_num ].offset - mod->offset;
		if( mod->frames > 0 && !( mod->flags & PSYNTH_FLAG_NO_RENDER ) )
		{
		    psynth_event module_evt;
		    module_evt.command = PS_CMD_RENDER_REPLACE;
		    int res = mod_handler2( start_mod, &module_evt, pnet ); 
		    RENDER_SET_OUTPUT_CONTENT( mod->offset, mod->frames ); 
		}
		mod->offset += mod->frames;
		psynth_event* evt = &pnet->events_heap[ evt_num ];
		int handled = mod->handler( start_mod, evt, pnet );
		evt = &pnet->events_heap[ evt_num ]; 
		switch( evt->command )
		{
		    case PS_CMD_CLEAN:
			{
			    bool empty_input = true; 
			    for( int c = 0; c < mod->input_channels; c++ )
			    {
				PS_STYPE* ch = mod->channels_in[ c ];
				if( !ch ) continue;
				if( mod->in_empty[ c ] < buf_size )
				{
				    int start = mod->in_empty[ c ];
				    if( start < mod->offset ) start = mod->offset;
				    for( int a = start; a < buf_size; a++ )
				    {
					if( ch[ a ] != 0 )
					{
					    empty_input = false;
					    c = 100;
					    break;
					}
				    }
				}
			    }
			    if( empty_input )
			    {
				psynth_set_input_content( 0, buf_size, 0, mod );
			    }
			}
			break;
		    case PS_CMD_SET_LOCAL_CONTROLLER:
		    case PS_CMD_SET_GLOBAL_CONTROLLER:
			pnet->change_counter++;
			if( !handled ) psynth_set_ctl( mod, evt );
			mod->draw_request++;
			break;
		    case PS_CMD_SET_MSB:
			pnet->change_counter++;
			pnet->change_counter2++;
			if( !handled ) psynth_set_msb( mod, evt );
			mod_handler2 = mod->handler;
			if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS ) mod_handler2 = psynth_bypass;
			break;
		    case PS_CMD_RESET_MSB:
			pnet->change_counter++;
			pnet->change_counter2++;
			if( !handled ) psynth_reset_msb( mod, evt );
			mod_handler2 = mod->handler;
			if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS ) mod_handler2 = psynth_bypass;
			break;
		    case PS_CMD_FINETUNE:
			pnet->change_counter++;
			if( !handled ) psynth_finetune( mod, evt );
			break;
		    case PS_CMD_MIDIMSG_SUPPORT:
			pnet->change_counter++;
			if( !handled ) psynth_midimsg_support( mod, evt );
			break;
		    default:
			break;
		}
	    }
	    if( mod->offset < buf_size && !( mod->flags & PSYNTH_FLAG_NO_RENDER ) )
	    {
		mod->frames = buf_size - mod->offset;
		psynth_event module_evt;
		module_evt.command = PS_CMD_RENDER_REPLACE;
		int res = mod_handler2( start_mod, &module_evt, pnet );
		RENDER_SET_OUTPUT_CONTENT( mod->offset, mod->frames ); 
	    }
	}
        if( pnet->cpu_usage_enable & 1 )
        {
            stime_ticks_t synth_end_time = stime_ticks();
	    mod->cpu_usage_ticks += synth_end_time - synth_start_time;
	}
    }
    if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
	smutex_unlock( &mod->mutex );
ignore_module:
    volatile uint new_ui_flags = mod->ui_flags;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) new_ui_flags |= PSYNTH_UI_FLAG_MUTE; else new_ui_flags &= ~PSYNTH_UI_FLAG_MUTE;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_SOLO ) new_ui_flags |= PSYNTH_UI_FLAG_SOLO; else new_ui_flags &= ~PSYNTH_UI_FLAG_SOLO;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_BYPASS ) new_ui_flags |= PSYNTH_UI_FLAG_BYPASS; else new_ui_flags &= ~PSYNTH_UI_FLAG_BYPASS;
    if( mod->ui_flags != new_ui_flags )
    {
	mod->ui_flags = new_ui_flags;
	mod->draw_request++;
    }
    mod->realtime_flags &= ~PSYNTH_RT_FLAG_LOCKED;
    mod->realtime_flags |= PSYNTH_RT_FLAG_RENDERED;
    return retval;
}
static int psynth_render_module0( psynth_net* pnet )
{
    int retval = 0;
    psynth_module* mod = &pnet->mods[ 0 ];
    if( mod->realtime_flags & PSYNTH_RT_FLAG_RENDERED ) return 0;
    if( mod->realtime_flags & PSYNTH_RT_FLAG_LOCKED ) return -1;
    int buf_size = pnet->buf_size;
    psynth_module* in;
    mod->realtime_flags |= PSYNTH_RT_FLAG_LOCKED;
    bool main_input_rendered[ PSYNTH_MAX_CHANNELS ];
    for( int ch = 0; ch < mod->input_channels; ch++ ) main_input_rendered[ ch ] = 0;
    for( int inp = 0; inp < mod->input_links_num; inp++ )
    {
	if( (unsigned)mod->input_links[ inp ] < (unsigned)pnet->mods_num )
	{
	    in = &pnet->mods[ mod->input_links[ inp ] ];
	    if( !( in->realtime_flags & PSYNTH_RT_FLAG_RENDERED ) )
	    {
		psynth_render( mod->input_links[ inp ], pnet );
	    }
	    if( in->realtime_flags & PSYNTH_RT_FLAG_RENDERED )
	    {
		PS_STYPE* prev_in_channel = 0;
		int prev_ch = 0;
		for( int ch = 0; ch < mod->input_channels; ch++ )
		{
		    PS_STYPE* in_data;
		    PS_STYPE* out_data = mod->channels_in[ ch ];
		    int in_ch;
		    if( ch >= in->output_channels ) 
		    { 
			in_data = prev_in_channel; 
			in_ch = prev_ch; 
		    }
		    else
		    {
			in_data = in->channels_out[ ch ];
			in_ch = ch;
		    }
		    prev_in_channel = in_data;
		    prev_ch = in_ch;
		    if( in_data && out_data )
		    {
			if( main_input_rendered[ ch ] )
	    		{
			    for( int i = in->out_empty[ in_ch ]; i < buf_size; i++ )
			    {
				out_data[ i ] += in_data[ i ];
			    }
			    if( in->out_empty[ in_ch ] < mod->in_empty[ ch ] )
				mod->in_empty[ ch ] = in->out_empty[ in_ch ];
			}
			else
			{
			    if( in->out_empty[ in_ch ] < buf_size )
			    {
				PS_STYPE_BUF_CLEAN( out_data, mod->in_empty[ ch ], in->out_empty[ in_ch ] );
				PS_STYPE_BUF_COPY( out_data, in_data, in->out_empty[ in_ch ], buf_size );
				mod->in_empty[ ch ] = in->out_empty[ in_ch ];
			    }
			    else
			    {
				PS_STYPE_BUF_CLEAN( out_data, mod->in_empty[ ch ], buf_size );
				mod->in_empty[ ch ] = buf_size;
			    }
			    if( mod->in_empty[ ch ] < buf_size )
			    {
				main_input_rendered[ ch ] = 1;
			    }
			}
		    }
		}
	    }
	}
    }
    for( int ch = 0; ch < mod->input_channels; ch++ ) 
    {
	PS_STYPE* ch_data = mod->channels_in[ ch ];
	if( !ch_data ) continue;
	if( main_input_rendered[ ch ] )
	{
	    int global_volume = pnet->global_volume;
	    if( global_volume != 256 )
	    {
		for( int i = mod->in_empty[ ch ]; i < buf_size; i++ )
		{
		    PS_STYPE2 v = ch_data[ i ];
		    v *= global_volume;
		    v /= 256;
		    ch_data[ i ] = (PS_STYPE)v;
		}
	    }
	}
	else 
	{
	    if( buf_size > mod->in_empty[ ch ] )
	    {
		PS_STYPE_BUF_CLEAN( ch_data, mod->in_empty[ ch ], buf_size );
		mod->in_empty[ ch ] = buf_size;
	    }
	}
    }
    mod->realtime_flags &= ~PSYNTH_RT_FLAG_LOCKED;
    mod->realtime_flags |= PSYNTH_RT_FLAG_RENDERED;
    return retval;
}
void psynth_render_all( psynth_net* pnet )
{
    pnet->all_modules_muted = 0;
    for( uint i = 0; i < pnet->mods_num; i++ ) 
    {
	psynth_module* mod = &pnet->mods[ i ];
	if( mod->flags & PSYNTH_FLAG_EXISTS )
	{
	    mod->realtime_flags &= ~PSYNTH_RT_FLAG_RENDERED;
	    if( mod->flags & PSYNTH_FLAG_SOLO )
		pnet->all_modules_muted = 1;
	    if( mod->flags & PSYNTH_FLAG_GET_RENDER_SETUP_COMMANDS )
		psynth_do_command( i, PS_CMD_RENDER_SETUP, pnet );
	}
    }
    if( pnet->prev_change_counter2 != pnet->change_counter2 )
    {
	pnet->prev_change_counter2 = pnet->change_counter2;
	for( uint i = 0; i < pnet->mods_num; i++ ) 
	{
	    psynth_module* mod = &pnet->mods[ i ];
	    if( mod->flags & PSYNTH_FLAG_EXISTS )
	    {
	        mod->realtime_flags &= ~( PSYNTH_RT_FLAG_MUTE | PSYNTH_RT_FLAG_SOLO | PSYNTH_RT_FLAG_BYPASS );
	    }
	}
    }
#ifdef PSYNTH_MULTITHREADED
    pnet->th_work_t = stime_ticks();
    pnet->th_work = true;
#endif
    psynth_render_module0( pnet );
    for( uint i = 1; i < pnet->mods_num; i++ ) psynth_render( i, pnet );
#ifdef PSYNTH_MULTITHREADED
    pnet->th_work = false;
    COMPILER_MEMORY_BARRIER();
    stime_ticks_t t1 = stime_ticks();
    while( 1 )
    {
	if( atomic_load( &pnet->th_work_cnt ) == 0 ) break;
    }
    stime_ticks_t t2 = stime_ticks();
#endif
    if( ( pnet->flags & PSYNTH_NET_FLAG_NO_SCOPE ) == 0 )
	psynth_fill_scope_buffers( pnet->buf_size, pnet );
}
PS_RETTYPE psynth_handle_event( uint mod_num, psynth_event* evt, psynth_net* pnet )
{
    PS_RETTYPE res = 0;
    while( 1 )
    {
	psynth_module* mod = psynth_get_module( mod_num, pnet );
	if( !mod ) break;
        if( evt->command == PS_CMD_RENDER_REPLACE )
	{
	    if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
	    {
		if( smutex_trylock( &mod->mutex ) != 0 )
		{
            	    psynth_set_output_content( 0, pnet->buf_size, 0, mod ); 
            	    break;
        	}
	    }
	    mod->frames = pnet->buf_size;
	    mod->offset = 0;
	}
	res = mod->handler( mod_num, evt, pnet );
	switch( evt->command )
	{
	    case PS_CMD_RENDER_REPLACE:
		{
		    RENDER_SET_OUTPUT_CONTENT( 0, pnet->buf_size );
		}
		break;
	    case PS_CMD_SET_LOCAL_CONTROLLER:
	    case PS_CMD_SET_GLOBAL_CONTROLLER:
		pnet->change_counter++;
		if( !res ) psynth_set_ctl( mod, evt );
		break;
	    case PS_CMD_SET_MSB:
		pnet->change_counter++;
		pnet->change_counter2++;
		if( !res ) psynth_set_msb( mod, evt );
		break;
	    case PS_CMD_RESET_MSB:
		pnet->change_counter++;
		pnet->change_counter2++;
		if( !res ) psynth_reset_msb( mod, evt );
		break;
	    case PS_CMD_FINETUNE:
		pnet->change_counter++;
		if( !res ) psynth_finetune( mod, evt );
		break;
	    case PS_CMD_MIDIMSG_SUPPORT:
		pnet->change_counter++;
		if( !res ) psynth_midimsg_support( mod, evt );
		break;
	    case PS_CMD_WRITE_CURVE:
		pnet->change_counter++;
		break;
	    default: break;
	}
        if( evt->command == PS_CMD_RENDER_REPLACE )
	{
	    if( mod->flags & PSYNTH_FLAG_USE_MUTEX )
	    {
		smutex_unlock( &mod->mutex );
	    }
	}
	break;
    }
    return res;
}
PS_RETTYPE psynth_handle_ctl_event( uint mod_num, int ctl_num, int ctl_val, psynth_net* pnet )
{
    psynth_event evt;
    SMEM_CLEAR_STRUCT( evt );
    evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
    evt.controller.ctl_num = ctl_num;
    evt.controller.ctl_val = ctl_val;
    return psynth_handle_event( mod_num, &evt, pnet );
}
PS_RETTYPE psynth_do_command( uint mod_num, psynth_command cmd, psynth_net* pnet )
{
    psynth_event evt;
    evt.command = cmd;
    return psynth_handle_event( mod_num, &evt, pnet );
}
int psynth_curve( uint mod_num, int curve_num, float* data, int len, bool w, psynth_net* pnet )
{
    if( !data ) return 0;
    psynth_event evt;
    SMEM_CLEAR_STRUCT( evt );
    if( w )
	evt.command = PS_CMD_WRITE_CURVE;
    else
	evt.command = PS_CMD_READ_CURVE;
    evt.id = curve_num;
    evt.offset = len;
    evt.curve.data = data;
    return psynth_handle_event( mod_num, &evt, pnet );
}
void psynth_resize_ctls_storage( uint mod_num, uint ctls_num, psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    size_t new_size = ctls_num * sizeof( psynth_ctl );
    if( smem_get_size( m->ctls ) < new_size )
    {
	m->ctls = (psynth_ctl*)SMEM_ZRESIZE( m->ctls, new_size );
	if( !m->ctls )
	    m->ctls_num = 0;
    }
}
void psynth_change_ctls_num( uint mod_num, uint ctls_num, psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    psynth_resize_ctls_storage( mod_num, ctls_num, pnet );
    if( m->ctls )
    {
	if( ctls_num > PSYNTH_MAX_CTLS )
	{
	    ctls_num = PSYNTH_MAX_CTLS;
	    slog( "Controllers count limit for %s\n", m->name );
	}
	m->ctls_num = ctls_num;
    }
}
int psynth_register_ctl( 
    uint mod_num, 
    const char* ctl_name, 
    const char* ctl_label, 
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_def,
    uint8_t ctl_type,
    PS_CTYPE* ctl_value,
    PS_CTYPE ctl_normal_value, 
    uint8_t ctl_group,
    psynth_net* pnet )
{
    int rv = -1;
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return -1;
    if( m->ctls_num + 1 > smem_get_size( m->ctls ) / sizeof( psynth_ctl ) )
    {
	slog( "Ctls storage resize for %s\n", m->name );
	psynth_resize_ctls_storage( mod_num, m->ctls_num + 1, pnet );
    }
    if( m->ctls == 0 ) return -1;
    psynth_ctl* ctl = &m->ctls[ m->ctls_num ];
    ctl->name = ctl_name;
    ctl->label = ctl_label;
    ctl->min = ctl_min;
    ctl->max = ctl_max;
    ctl->def = ctl_def;
    ctl->val = ctl_value;
    if( ctl_normal_value == -1 ) ctl_normal_value = ctl_max;
    ctl->normal_value = ctl_normal_value;
    if( ctl_normal_value > ctl_max )
    {
        slog( "WARNING: ctl_normal_value > ctl_max in %s\n", ctl_name );
    }
    ctl->show_offset = 0;
    ctl->type = ctl_type;
    ctl->group = ctl_group;
    ctl->midi_pars1 = PSYNTH_CTL_MIDI_PARS1_DEFAULT;
    ctl->midi_pars2 = PSYNTH_CTL_MIDI_PARS2_DEFAULT;
    *ctl_value = ctl_def;
    rv = m->ctls_num;
    m->ctls_num++;
    if( m->ctls_num > PSYNTH_MAX_CTLS )
    {
        m->ctls_num = PSYNTH_MAX_CTLS;
        slog( "Controllers count limit for %s\n", m->name );
    }
    return rv;
}
void psynth_change_ctl( 
    uint mod_num, 
    uint ctl_num, 
    const char* ctl_name, 
    const char* ctl_label, 
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_def,
    int ctl_type,
    PS_CTYPE* ctl_value,
    PS_CTYPE ctl_normal_value,
    int ctl_group,
    psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    if( ctl_num >= m->ctls_num ) return;
    psynth_ctl* ctl = &m->ctls[ ctl_num ];
    if( ctl_name ) ctl->name = ctl_name;
    if( ctl_label ) ctl->label = ctl_label;
    if( ctl_min >= 0 ) ctl->min = ctl_min;
    if( ctl_max >= 0 ) ctl->max = ctl_max;
    if( ctl_def >= 0 ) ctl->def = ctl_def;
    if( ctl_value ) ctl->val = ctl_value;
    if( ctl_normal_value >= 0 ) ctl->normal_value = ctl_normal_value;
    if( ctl->normal_value > ctl->max )
    {
        slog( "WARNING: ctl_normal_value > ctl_max in %s\n", ctl->name );
    }
    if( ctl_type >= 0 ) ctl->type = ctl_type;
    if( ctl_group >= 0 ) ctl->group = ctl_group;
    if( ctl_value ) *ctl_value = ctl->def;
}
void psynth_change_ctl_limits( 
    uint mod_num, 
    uint ctl_num, 
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_normal_value,
    psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    if( ctl_num >= m->ctls_num ) return;
    psynth_ctl* ctl = &m->ctls[ ctl_num ];
    if( ctl_min >= 0 ) ctl->min = ctl_min;
    if( ctl_max >= 0 ) ctl->max = ctl_max;
    if( ctl_normal_value >= 0 ) ctl->normal_value = ctl_normal_value;
    if( ctl->normal_value > ctl->max )
    {
	slog( "WARNING: ctl_normal_value > ctl_max in %s\n", ctl->name );
    }
    if( ctl->val )
    {
        if( *ctl->val < ctl_min ) *ctl->val = ctl_min;
        if( *ctl->val > ctl_max ) *ctl->val = ctl_max;
    }
}
void psynth_set_ctl_show_offset( uint mod_num, uint ctl_num, PS_CTYPE ctl_show_offset, psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    if( ctl_num >= m->ctls_num ) return;
    psynth_ctl* ctl = &m->ctls[ ctl_num ];
    ctl->show_offset = ctl_show_offset;
}
void psynth_set_ctl_flags( uint mod_num, uint ctl_num, uint32_t flags, psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return;
    if( ctl_num >= m->ctls_num ) return;
    psynth_ctl* ctl = &m->ctls[ ctl_num ];
    ctl->flags = flags;
}
int psynth_get_scaled_ctl_value(
    uint mod_num, 
    uint ctl_num,
    int val,
    bool positive_val_without_offset,
    psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( !m ) return -1;
    psynth_ctl* ctl = NULL;
    uint32_t v = 0;
    if( ctl_num < m->ctls_num )
	ctl = &m->ctls[ ctl_num ];
    else
    {
	int midi_ctl = ctl_num + 1 - 0x80;
	if( midi_ctl >= 0 )
	{
	    if( midi_ctl < 32 )
		v = val << 1; 
	    else
		v = val << 8; 
	}
	else
	{
	    v = val;
	}
    }
    if( ctl )
    {
	if( positive_val_without_offset == false )
	{
	    val -= ctl->show_offset;
    	    val -= ctl->min;
	}
	if( ctl->type == 0 )
	{
	    if( ctl->max - ctl->min )
    		v = (uint)( val << 15 ) / ( ctl->max - ctl->min );
    	    else
    		v = (uint)( val << 15 );
    	    uint v2 = v * ( ctl->max - ctl->min );
	    v2 /= 32768;
    	    if( (signed)v2 < val )
    		v++;
	}
	else
	{
    	    v = val + ctl->min;
	}
    }
    if( v > 0x8000 ) v = 0x8000;
    return v;
}
int psynth_get_scaled_ctl_value( uint mod_num, uint ctl_num, psynth_net* pnet )
{
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return -1;
    psynth_ctl* ctl = psynth_get_ctl( mod, ctl_num, pnet );
    if( !ctl ) return -1;
    int val = ctl->val[ 0 ];
    if( ctl->type == 0 )
	val = ( ( val - ctl->min ) << 15 ) / ( ctl->max - ctl->min );
    return val;
}
void psynth_get_ctl_val_str(
    uint mod_num, 
    uint ctl_num,
    int val, 
    char* out_str,
    psynth_net* pnet )
{
    psynth_module* m = psynth_get_module( mod_num, pnet );
    if( m == 0 ) return;
    if( ctl_num >= m->ctls_num ) return;
    psynth_ctl* ctl = &m->ctls[ ctl_num ];
    out_str[ 0 ] = 0;
    const char* label = (const char*)ctl->label;
    if( label && label[ 0 ] == 0 ) label = 0;
    int format = 0;
    if( val < 0 ) val = 0;
    if( val > 32768 ) val = 32768;
    uint v = (uint)( ctl->max - ctl->min ) * val;
    v >>= 15;
    v = v + ctl->min;
    if( label )
    {
	if( ctl->type == 0 )
	{
	    format = 1;
        }
        else
        {
    	    if( smem_strchr( label, ';' ) )
    	    {
    		format = 2;
    	    }
    	    else
    	    {
    		format = 1;
    	    }
        }
    }
    else
    {
	format = 0;
    }
    switch( format )
    {
	case 1:
	    {
        	sprintf( out_str, "%d %s", v + ctl->show_offset, label );
    	    }
	    break;
	case 2:
	    {
		const int ts_size = 256;
	        char ts[ ts_size ];
	        ts[ 0 ] = 0;
	        smem_split_str( ts, ts_size, label, ';', v );
        	sprintf( out_str, "%d %s", v, ts );
	    }
	    break;
	default:
	    {
    		sprintf( out_str, "%d", v + ctl->show_offset );
    	    }
	    break;
    }
}
PS_STYPE* psynth_get_temp_buf( uint mod_num, psynth_net* pnet, uint buf_num )
{
    if( buf_num >= PSYNTH_MAX_CHANNELS ) return NULL;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return NULL;
#ifdef PSYNTH_MULTITHREADED
    psynth_thread* th = &pnet->th[ mod->th_id ];
#else
    psynth_thread* th = &pnet->th[ 0 ];
#endif
    PS_STYPE* buf = th->temp_buf[ buf_num ];
    if( !buf )
    {
        buf = SMEM_ALLOC2( PS_STYPE, pnet->max_buf_size );
        th->temp_buf[ buf_num ] = buf;
    }
    return buf;
}
int psynth_resampler_change( psynth_resampler* r, int in_smprate, int out_smprate, int ratio_fp, uint32_t flags )
{
    if( !r ) return -1;
    r->flags = flags;
    r->in_smprate = in_smprate;
    r->out_smprate = out_smprate;
    if( ratio_fp )
    {
	r->ratio_fp = ratio_fp;
    }
    else
    {
	r->ratio_fp = (int64_t)in_smprate * 65536 / out_smprate;
    }
    r->input_buf_size = 0;
    r->input_empty_frames_max = PSYNTH_RESAMP_BUF_TAIL;
    if( ( flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE1 )
    {
	r->input_delay = ( (int64_t)( PSYNTH_RESAMP_INTERP_AFTER + 1 ) * r->ratio_fp ) / 65536 + PSYNTH_RESAMP_INTERP_AFTER + 1; 
	r->input_empty_frames_max += r->input_delay;
	int prev_size = smem_get_size( r->input_delay_bufs[ 0 ] ) / sizeof( PS_STYPE );
	int new_size = r->input_delay * sizeof( PS_STYPE );
	if( new_size > prev_size )
	{
	    for( int i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
	    {
		if( !r->input_delay_bufs_empty ) smem_zero( r->input_delay_bufs[ i ] );
		r->input_delay_bufs[ i ] = (PS_STYPE*)SMEM_ZRESIZE( r->input_delay_bufs[ i ], new_size );
	    }
	}
	r->input_delay_bufs_empty = true;
    }
    psynth_resampler_reset( r );
    return 0;
}
psynth_resampler* psynth_resampler_new( psynth_net* pnet, uint mod_num, int in_smprate, int out_smprate, int ratio_fp, uint32_t flags )
{
    psynth_resampler* r = SMEM_ZALLOC2( psynth_resampler, 1 );
    if( !r ) return NULL;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return NULL;
    r->pnet = pnet;
    r->mod = mod;
    psynth_resampler_change( r, in_smprate, out_smprate, ratio_fp, flags );
    return r;
}
PS_STYPE* psynth_resampler_input_buf( psynth_resampler* r, uint buf_num )
{
    if( !r ) return NULL;
    if( buf_num >= PSYNTH_MAX_CHANNELS ) return NULL;
#ifdef PSYNTH_MULTITHREADED
    psynth_thread* th = &r->pnet->th[ r->mod->th_id ];
#else
    psynth_thread* th = &r->pnet->th[ 0 ];
#endif
    bool mode1 = ( r->flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE1;
    bool create = false;
    PS_STYPE* buf = th->resamp_buf[ buf_num + PSYNTH_MAX_CHANNELS * mode1 ];
    int prev_size = smem_get_size( buf ) / sizeof( PS_STYPE );
    if( r->input_buf_size == 0 || r->input_buf_size > prev_size ) create = true;
    if( create )
    {
	int size = ( ( (int64_t)r->pnet->max_buf_size * r->ratio_fp * r->out_smprate ) / r->pnet->sampling_freq / 65536 ) + 4; 
	if( mode1 ) size += r->input_delay;
	size += PSYNTH_RESAMP_BUF_TAIL * 2;
	r->input_buf_size = size;
	if( buf )
	{
	    if( size > prev_size )
	    {
    		buf = SMEM_RESIZE2( buf, PS_STYPE, size + 32 );
	    }
	}
	else
	{
    	    buf = SMEM_ALLOC2( PS_STYPE, size );
	}
	th->resamp_buf[ buf_num + PSYNTH_MAX_CHANNELS * mode1 ] = buf;
    }
    return buf;
}
void psynth_resampler_remove( psynth_resampler* r )
{
    if( !r ) return;
    if( ( r->flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE1 )
    {
	for( int i = 0; i < PSYNTH_MAX_CHANNELS; i++ )
	    smem_free( r->input_delay_bufs[ i ] );
    }
    smem_free( r );
}
void psynth_resampler_reset( psynth_resampler* r )
{
    if( !r ) return;
    r->state = 0;
    r->input_ptr_fp = PSYNTH_RESAMP_BUF_TAIL << 16;
    if( ( r->flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE2 )
    {
	r->input_ptr_fp = ( PSYNTH_RESAMP_INTERP_BEFORE + 1 ) << 16;
    }
    SMEM_CLEAR_STRUCT( r->input_buf_tail );
    r->input_empty_frames = 0;
    if( ( r->flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE1 )
    {
	r->input_offset = 0;
	if( !r->input_delay_bufs_empty )
	{
	    for( int i = 0; i < PSYNTH_MAX_CHANNELS; i++ ) smem_zero( r->input_delay_bufs[ i ] );
	    r->input_delay_bufs_empty = true;
	}
    }
}
int psynth_resampler_begin( psynth_resampler* r, int input_frames, int output_frames )
{
    if( !r ) return 0;
    r->input_frames_avail = input_frames;
    if( output_frames )
    {
	r->output_frames = output_frames;
	r->input_frames_fp = output_frames * r->ratio_fp;
	uint input_last_required_frame = ( ( r->input_ptr_fp + r->input_frames_fp - r->ratio_fp ) >> 16 ) + PSYNTH_RESAMP_INTERP_AFTER;
	r->input_frames = ( ( input_last_required_frame + 1 ) - PSYNTH_RESAMP_BUF_TAIL ) & 0xFFFF;
    }
    else
    {
	r->input_frames = input_frames;
	r->output_frames = 0;
	uint l = ( PSYNTH_RESAMP_BUF_TAIL + input_frames - PSYNTH_RESAMP_INTERP_AFTER ) * 65536 - 1;
	if( r->input_ptr_fp <= l )
	    r->output_frames = ( l - r->input_ptr_fp ) / r->ratio_fp + 1;
	r->input_frames_fp = r->output_frames * r->ratio_fp;
    }
    return r->input_frames;
}
int psynth_resampler_end(
    psynth_resampler* r,
    int input_filled,
    PS_STYPE** inputs, 
    PS_STYPE** outputs,
    int output_count,
    int output_offset )
{
    int retval = 0;
    if( !r ) return 0;
    if( input_filled == 1 || input_filled == 3 ) r->state = 1;
    bool skip_processing = false;
    if( r->state == -1 ) skip_processing = true;
    if( r->state )
    {
	int input_offset = 0;
	if( ( r->flags & PSYNTH_RESAMP_FLAG_MODE ) == PSYNTH_RESAMP_FLAG_MODE1 )
	{
	    if( !skip_processing )
	    {
		r->input_delay_bufs_empty = false;
    		for( int ch = 0; ch < output_count; ch++ )
    		{
        	    PS_STYPE* RESTRICT buf = inputs[ ch ];
        	    PS_STYPE* RESTRICT delay_buf = r->input_delay_bufs[ ch ];
    		    smem_copy( buf + PSYNTH_RESAMP_BUF_TAIL, delay_buf, r->input_delay * sizeof( PS_STYPE ) );
    		    smem_copy( delay_buf, buf + PSYNTH_RESAMP_BUF_TAIL + r->input_frames_avail, r->input_delay * sizeof( PS_STYPE ) ); 
    		}
    	    }
            if( r->input_offset < 0 ) {   r->input_offset = 0; }
            if( r->input_offset > r->input_delay - 1 ) {   r->input_offset = r->input_delay - 1; }
    	    input_offset = r->input_offset;
            r->input_offset += r->input_frames - r->input_frames_avail;
	}
        if( !skip_processing )
        {
    	    for( int ch = 0; ch < output_count; ch++ )
	    {
        	PS_STYPE* RESTRICT out = outputs[ ch ] + output_offset;
        	PS_STYPE* RESTRICT buf = inputs[ ch ] + input_offset;
        	PS_STYPE* RESTRICT input_buf_tail = &r->input_buf_tail[ PSYNTH_RESAMP_BUF_TAIL * ch ];
        	if( input_filled == 0 )
        	{
            	    for( uint i = PSYNTH_RESAMP_BUF_TAIL; i < r->input_frames + PSYNTH_RESAMP_BUF_TAIL; i++ )
                	buf[ i ] = 0;
        	}
    		for( uint i = 0; i < PSYNTH_RESAMP_BUF_TAIL; i++ )
    		{
    	    	    buf[ i ] = input_buf_tail[ i ];
    		}
        	uint input_ptr_fp = r->input_ptr_fp;
        	for( uint i = 0; i < r->output_frames; i++ )
        	{
            	    uint c = input_ptr_fp & 0xFFFF;
            	    uint p = input_ptr_fp >> 16;
#ifdef PSYNTH_RESAMP_INTERP_SPLINE
            	    PS_STYPE2 v0 = buf[ p - 1 ];
            	    PS_STYPE2 v1 = buf[ p ];
            	    PS_STYPE2 v2 = buf[ p + 1 ];
            	    PS_STYPE2 v3 = buf[ p + 2 ];
            	    PS_STYPE2 mu = (PS_STYPE2)c / (PS_STYPE2)0x10000;
            	    PS_STYPE2 a = ( 3 * ( v1 - v2 ) - v0 + v3 ) / 2;
            	    PS_STYPE2 b = 2 * v2 + v0 - ( 5 * v1 + v3 ) / 2;
            	    PS_STYPE2 c2 = ( v2 - v0 ) / 2;
            	    out[ i ] = ( ( ( a * mu ) + b ) * mu + c2 ) * mu + v1;
#else
            	    PS_STYPE2 v1 = buf[ p ];
            	    PS_STYPE2 v2 = buf[ p + 1 ];
            	    out[ i ] = ( v2 * c + v1 * ( 0xFFFF - c ) ) / 0x10000;
#endif
            	    input_ptr_fp += r->ratio_fp;
        	}
        	uint last_data_ptr = r->input_frames;
        	if( last_data_ptr )
        	{
        	    for( uint i = 0; i < PSYNTH_RESAMP_BUF_TAIL; i++ )
        	    {
        		input_buf_tail[ i ] = buf[ last_data_ptr + i ];
        	    }
    		}
    	    }
    	    retval = 1;
        }
        r->input_ptr_fp = r->input_ptr_fp + r->input_frames_fp - ( r->input_frames << 16 );
        r->input_empty_frames = 0;
        if( input_filled == 0 || input_filled == 2 )
        {
            r->input_empty_frames += r->input_frames;
    	    if( r->input_empty_frames >= r->input_empty_frames_max )
    	    {
	        r->input_delay_bufs_empty = true;
    		if( r->flags & PSYNTH_RESAMP_FLAG_DONT_RESET_WHEN_EMPTY )
    		    r->state = -1;
    		else
    		    psynth_resampler_reset( r );
    	    }
        }
    }
    return retval;
}
void psynth_new_chunk( uint mod_num, uint num, size_t size, uint flags, int freq, psynth_net* pnet )
{
    psynth_chunk c;
    c.data = SMEM_ZALLOC( size );
    if( c.data )
    {
	c.flags = flags;
	c.freq = freq;
	psynth_new_chunk( mod_num, num, &c, pnet );
    }
}
void psynth_new_chunk( uint mod_num, uint num, psynth_chunk* c, psynth_net* pnet )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( !mod->chunks )
	{
	    uint init_chunks = 4;
	    if( num >= init_chunks ) init_chunks = num + 1;
	    mod->chunks = SMEM_ZALLOC2( psynth_chunk*, init_chunks );
	}
	psynth_chunk* chunk = SMEM_ALLOC2( psynth_chunk, 1 );
	if( chunk )
	{
	    *chunk = *c;
	    if( num * sizeof( psynth_chunk* ) < smem_get_size( mod->chunks ) )
		psynth_remove_chunk( mod_num, num, pnet );
	    mod->chunks = SMEM_COPY_D2( mod->chunks, psynth_chunk*, num, 0, &chunk, 1 );
	}
    }
}
void psynth_replace_chunk_data( uint mod_num, uint num, void* data, psynth_net* pnet )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    smem_free( c->data );
		    c->data = data;
		}
	    }
	}
    }
}
void* psynth_get_chunk_data( uint mod_num, uint num, psynth_net* pnet )
{
    void* retval = 0;
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	retval = psynth_get_chunk_data( mod, num );
    }
    return retval;
}
void* psynth_get_chunk_data_autocreate( uint mod_num, uint num, size_t size, bool* created, uint flags, psynth_net* pnet )
{
    if( created ) *created = false;
    void* rv = psynth_get_chunk_data( mod_num, num, pnet );
    if( rv == NULL )
    {
        psynth_new_chunk( mod_num, num, size, 0, 0, pnet );
        rv = psynth_get_chunk_data( mod_num, num, pnet );
        if( created && rv )
    	    *created = true;
    }
    else
    {
        size_t old_size = 0;
        psynth_get_chunk_info( mod_num, num, pnet, &old_size, 0, 0 );
        if( old_size < size || 
            ( ( flags & PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE ) && old_size > size ) 
          )
        {
            rv = psynth_resize_chunk( mod_num, num, size, pnet );
        }
    }
    return rv;
}
int psynth_get_chunk_info( uint mod_num, uint num, psynth_net* pnet, size_t* size, uint* flags, int* freq )
{
    int retval = 1;
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    retval = 0;
		    if( size )
			*size = smem_get_size( c->data );
		    if( flags )
			*flags = c->flags;
		    if( freq )
			*freq = c->freq;
		}
	    }
	}
    }
    return retval;
}
void psynth_set_chunk_info( uint mod_num, uint num, psynth_net* pnet, uint flags, int freq )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    c->flags = flags;
		    c->freq = freq;
		}
	    }
	}
    }
}
void psynth_set_chunk_flags( uint mod_num, uint num, psynth_net* pnet, uint32_t fset, uint32_t freset )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    c->flags |= fset;
		    c->flags &= ~freset;
		}
	    }
	}
    }
}
void* psynth_resize_chunk( uint mod_num, uint num, size_t new_size, psynth_net* pnet )
{
    void* retval = 0;
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    if( c->data )
			c->data = SMEM_ZRESIZE( c->data, new_size );
		    retval = c->data;
		}
	    }
	}
    }
    return retval;
}
void psynth_remove_chunk( uint mod_num, uint num, psynth_net* pnet )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
	    if( num < count )
	    {
		psynth_chunk* c = mod->chunks[ num ];
		if( c )
		{
		    smem_free( c->data );
		    smem_free( c );
		    mod->chunks[ num ] = 0;
		}
	    }
	}
    }
}
void psynth_remove_chunks( uint mod_num, psynth_net* pnet )
{
    if( pnet->mods_num && mod_num < pnet->mods_num )
    {
	psynth_module* mod = &pnet->mods[ mod_num ];
	if( mod->chunks )
	{
	    for( size_t cn = 0; cn < smem_get_size( mod->chunks ) / sizeof( psynth_chunk* ); cn++ )
	    {
    		psynth_chunk* c = mod->chunks[ cn ];
		if( c )
		{
		    smem_free( c->data );
		    smem_free( c );
		}
	    }
	    smem_free( mod->chunks );
	    mod->chunks = 0;
	}
    }
}
int psynth_get_number_of_outputs( uint mod_num, psynth_net* pnet )
{
    int retval = 0;
    if( pnet->mods_num )
    if( mod_num < pnet->mods_num )
    {
	retval = psynth_get_number_of_outputs( &pnet->mods[ mod_num ] );
    }
    return retval;
}
int psynth_get_number_of_inputs( uint mod_num, psynth_net* pnet )
{
    int retval = 0;
    if( pnet->mods_num )
    if( mod_num < pnet->mods_num )
    {
	retval = psynth_get_number_of_inputs( &pnet->mods[ mod_num ] );
    }
    return retval;
}
void psynth_set_number_of_outputs( int num, uint mod_num, psynth_net* pnet )
{
    if( pnet->mods_num )
    if( mod_num < pnet->mods_num )
    {
	psynth_module* ss = &pnet->mods[ mod_num ];
	if( ss->output_channels != num )
	{
	    ss->output_channels = num;
	    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MODULE_CHANNELS ) )
		for( int c = num; c < PSYNTH_MAX_CHANNELS; c++ )
		{
		    PS_STYPE* ch = ss->channels_out[ c ];
		    if( !ch ) continue;
		    for( int i = ss->out_empty[ c ]; i < pnet->max_buf_size; i++ ) ch[ i ] = 0;
		    ss->out_empty[ c ] = pnet->max_buf_size;
		}
	}
    }
}
void psynth_set_number_of_inputs( int num, uint mod_num, psynth_net* pnet )
{
    if( pnet->mods_num )
    if( mod_num < pnet->mods_num )
    {
	psynth_module* ss = &pnet->mods[ mod_num ];
	if( ss->input_channels != num )
	{
	    ss->input_channels = num;
	    if( !( pnet->flags & PSYNTH_NET_FLAG_NO_MODULE_CHANNELS ) )
		for( int c = num; c < PSYNTH_MAX_CHANNELS; c++ )
		{
		    PS_STYPE* ch = ss->channels_in[ c ];
		    if( !ch ) continue;
		    for( int i = ss->in_empty[ c ]; i < pnet->max_buf_size; i++ ) ch[ i ] = 0;
		    ss->in_empty[ c ] = pnet->max_buf_size;
		}
	}
    }
}
int psynth_str2note( const char* note_str )
{
    if( note_str )
    {
	while( 1 )
	{
	    char c = *note_str;
	    if( c >= 0x41 && c <= 0x7A ) break;
	    if( c == 0 ) break;
	    note_str++;
	}
	if( strlen( note_str ) >= 2 )
	{
	    int n = -1;
	    switch( note_str[ 0 ] )
	    {
		case 'C': n = 0; break;
		case 'c': n = 1; break;
		case 'D': n = 2; break;
		case 'd': n = 3; break;
		case 'E': n = 4; break;
		case 'F': n = 5; break;
		case 'f': n = 6; break;
		case 'G': n = 7; break;
		case 'g': n = 8; break;
		case 'A': n = 9; break;
		case 'a': n = 10; break;
		case 'B': n = 11; break;
	    }
	    if( n >= 0 )
	    {
		int oct = string_to_int_hex( note_str + 1 );
		return oct * 12 + n;
	    }
	}
    }
    return PSYNTH_UNKNOWN_NOTE;
}
int8_t* psynth_get_noise_table()
{
    void* p = atomic_load( &g_noise_table );
    if( p ) return (int8_t*)p;
    int8_t* t = SMEM_ALLOC2( int8_t, PSYNTH_NOISE_TABLE_SIZE );
    if( !t ) return NULL;
    uint32_t r = 12345;
    for( int s = 0; s < PSYNTH_NOISE_TABLE_SIZE; s++ )
    {
        t[ s ] = (int8_t)( (int32_t)pseudo_random( &r ) - 32768 / 2 );
    }
    if( atomic_compare_exchange_strong( &g_noise_table, &p, (void*)t ) )
	return t;
    else
    {
	smem_free( t );
	return (int8_t*)p;
    }
}
void* psynth_get_sine_table( int bytes_per_sample, bool sign, int length_bits, int amp )
{
    void* rv = NULL;
    uint32_t table_id = ( bytes_per_sample - 1 ) | ( sign << 2 ) | ( ( length_bits - 1 ) << 3 ) | ( amp << 8 );
    int len = 1 << length_bits;
    while( 1 )
    {
	int tnum = -1;
	void* tdata = NULL;
	for( int i = 0; i < MAX_SINE_TABLES; i++ )
	{
	    void* t = atomic_load( &g_sine_tables[ i ] );
	    if( t )
	    {
		if( ((uint32_t*)t)[ 0 ] == table_id )
		{
		    tnum = i;
		    tdata = t;
		    break;
		}
	    }
	    else
	    {
		if( tnum == -1 ) tnum = i;
	    }
	}
	if( tnum < 0 )
	{
	    slog( "psynth_get_sine_table() error: too many tables\n" );
	    return NULL;
	}
	if( !tdata )
	{
	    void* t = SMEM_ALLOC( sizeof( uint32_t ) + len * bytes_per_sample );
    	    if( !t ) return NULL;
    	    ((uint32_t*)t)[ 0 ] = table_id;
	    if( atomic_compare_exchange_strong( &g_sine_tables[ tnum ], &tdata, (void*)t ) )
		rv = (uint8_t*)t + sizeof( uint32_t );
	    else
	    {
		smem_free( t );
		continue; 
	    }
	    float add = 0;
	    float amp2 = amp;
	    if( !sign )
	    {
		amp2 /= 2;
		add = amp2;
	    }
	    for( int i = 0; i < len; i++ )
    	    {
    		float v = sinf( (float)i / len * (float)M_PI * 2.0F ) * amp2 + add;
    		int iv = v;
    		if( bytes_per_sample == 1 )
		    ((int8_t*)rv)[ i ] = iv;
	        else
    	    	    ((int16_t*)rv)[ i ] = iv;
    	    }
	}
	else
	{
	    rv = (uint8_t*)tdata + sizeof( uint32_t );
	}
	if( rv ) break;
    }
    return rv;
}
PS_STYPE* psynth_get_base_wavetable()
{
    void* p = atomic_load( &g_base_wavetable );
    if( p ) return (PS_STYPE*)p;
    PS_STYPE* t = SMEM_ALLOC2( PS_STYPE, PSYNTH_BASE_WAVES * PSYNTH_BASE_WAVE_SIZE );
    if( !t ) return NULL;
    PS_STYPE* tt = t;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	int p = ( (i+PSYNTH_BASE_WAVE_SIZE/4) & (PSYNTH_BASE_WAVE_SIZE-1) ) * 4;
	int v = -PSYNTH_BASE_WAVE_SIZE + p;
	if( p >= PSYNTH_BASE_WAVE_SIZE*2 ) v = PSYNTH_BASE_WAVE_SIZE - ( p - PSYNTH_BASE_WAVE_SIZE*2 );
	tt[ i ] = v * PS_STYPE_ONE / (PS_STYPE2)PSYNTH_BASE_WAVE_SIZE;
    }
    PS_STYPE* triangletable = tt;
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	PS_STYPE2 v = triangletable[ i ];
	v = ( ( v * v / PS_STYPE_ONE ) * v ) / PS_STYPE_ONE;
	tt[ i ] = v;
    }
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	int p = ( (i+PSYNTH_BASE_WAVE_SIZE/2) & (PSYNTH_BASE_WAVE_SIZE-1) ) * 2;
	int v = -PSYNTH_BASE_WAVE_SIZE + p;
	tt[ i ] = v * PS_STYPE_ONE / (PS_STYPE2)PSYNTH_BASE_WAVE_SIZE;
    }
    PS_STYPE* sawtable = tt;
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	PS_STYPE2 v = sawtable[ i ];
	v = ( ( v * v / PS_STYPE_ONE ) * v ) / PS_STYPE_ONE;
	tt[ i ] = v;
    }
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	if( i < PSYNTH_BASE_WAVE_SIZE / 2 ) 
	    tt[ i ] = PS_STYPE_ONE;
	else
	    tt[ i ] = -PS_STYPE_ONE;
    }
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE / 2; i++ )
    {
	PS_STYPE2 v = sin( (float)i / PSYNTH_BASE_WAVE_SIZE * 2 * M_PI ) * PS_STYPE_ONE;
	tt[ i ] = v;
    }
    for( int i = PSYNTH_BASE_WAVE_SIZE / 2; i < PSYNTH_BASE_WAVE_SIZE; i++ ) tt[ i ] = -tt[ i - PSYNTH_BASE_WAVE_SIZE / 2 ];
    PS_STYPE* sintable = tt;
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE/2; i++ )
    {
	tt[ i ] = sintable[ i ];
    }
    for( int i = PSYNTH_BASE_WAVE_SIZE / 2; i < PSYNTH_BASE_WAVE_SIZE; i++ ) tt[ i ] = 0;
    tt += PSYNTH_BASE_WAVE_SIZE;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE; i++ )
    {
	tt[ i ] = PS_STYPE_ABS( sintable[ i ] );
    }
    tt += PSYNTH_BASE_WAVE_SIZE;
    uint32_t seed = 17;
    for( int i = 0; i < PSYNTH_BASE_WAVE_SIZE / 2; i++ )
    {
	float v1 = sin( (float)i / PSYNTH_BASE_WAVE_SIZE * 2 * M_PI );
	float v2 = v1 * v1 * v1;
	tt[ i ] = v2 * PS_STYPE_ONE;
    }
    for( int i = PSYNTH_BASE_WAVE_SIZE / 2; i < PSYNTH_BASE_WAVE_SIZE; i++ ) tt[ i ] = -tt[ i - PSYNTH_BASE_WAVE_SIZE / 2 ];
    if( atomic_compare_exchange_strong( &g_base_wavetable, &p, (void*)t ) )
	return t;
    else
    {
	smem_free( t );
	return (PS_STYPE*)p;
    }
}
