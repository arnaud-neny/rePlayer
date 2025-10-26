/*
    sound.cpp - sound and MIDI in/out
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

int g_sample_size[ sound_buffer_max ] = 
{
    0,
    sizeof( int16_t ),
    sizeof( float )
};

#ifndef SUNDOG_MODULE
    volatile int g_snd_play_request = 0;
    volatile int g_snd_stop_request = 0;
    volatile int g_snd_rewind_request = 0;
    volatile bool g_snd_play_status = 0;
#endif

int g_sundog_sound_cnt = 0; //number of sound streams per process;
smutex g_sundog_sound_mutex; //for init/deinit/input:
			     //these functions can use some global variables and global audio session init/deinit in the device-dependent code;
			     //  ?? future fix: not required on some systems ??

int sundog_sound_set_defaults( sundog_sound* ss );

//#define SHOW_DEBUG_MESSAGES

#ifndef NOSOUND

#include "sound_common.hpp"
#ifdef JACK_AUDIO
    #include "sound_common_jack.hpp"
#endif

#ifdef OS_LINUX
    #ifdef OS_ANDROID
	#include "sound_android.hpp"
	#include "sound_android_midi.hpp"
    #else
	#include "sound_linux.hpp"
	#include "sound_linux_midi.hpp"
    #endif
#endif

#ifdef OS_MACOS
    #include "sound_macos.hpp"
    #include "sound_apple_midi.hpp"
#endif

#ifdef OS_IOS
    #define IOS_SOUND_CODE
    #ifdef AUDIOUNIT_EXTENSION
	#include "sound_ios_auext.hpp"
    #else
	#include "sound_ios.hpp"
	#include "sound_apple_midi.hpp"
    #endif
#endif

#if defined(OS_WIN) || defined(OS_WINCE)
    #include "sound_win.hpp"
    #include "sound_win_midi.hpp"
#endif

#ifdef OS_EMSCRIPTEN
    #include "sound_emscripten.hpp"
#endif

#define COMMON_CODE
#include "sound_common.hpp"
#ifdef JACK_AUDIO
    #include "sound_common_jack.hpp"
#endif

#endif

//
// Sound stream
//

int user_controlled_sound_callback( 
    sundog_sound* ss, 
    void* buffer, 
    int frames, 
    int latency, 
    stime_ticks_t output_time )
{
    if( !ss ) return 0; 
    if( !ss->initialized ) return 0;
    ss->out_buffer = buffer;
    ss->out_frames = frames;
    ss->out_time = output_time;
    ss->out_latency = latency;
    ss->out_latency2 = latency;
    return sundog_sound_callback( ss, SUNDOG_SOUND_CALLBACK_FLAG_DONT_LOCK ); // rePlayer
}

int user_controlled_sound_callback( 
    sundog_sound* ss, 
    void* out_buffer, 
    int frames, int latency, stime_ticks_t out_time,
    sound_buffer_type in_type,
    int in_channels,
    void* in_buffer )
{
    if( !ss ) return 0; 
    if( !ss->initialized ) return 0;
    ss->out_buffer = out_buffer;
    ss->out_frames = frames;
    ss->out_time = out_time;
    ss->out_latency = latency;
    ss->out_latency2 = latency;
    ss->in_type = in_type;
    ss->in_channels = in_channels;
    ss->in_buffer = in_buffer;
    return sundog_sound_callback( ss, 0 );
}

int sundog_sound_callback( sundog_sound* ss, uint32_t flags )
{
#ifdef NOSOUND
    return 0;
#else
    bool not_filled = true;
    bool silence = true;

    int frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
    int in_frame_size = 0;
    void* in_buffer = NULL;
    uint32_t rendered_slots = 0;

    if( ( flags & SUNDOG_SOUND_CALLBACK_FLAG_DONT_LOCK ) == 0 )
    {
	if( smutex_lock( &ss->mutex ) ) goto mutex_end;
    }

    if( ss->in_enabled && ss->in_buffer )
    {
	in_frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
	in_buffer = ss->in_buffer;
    }

    for( int a = 0; a < 2; a++ )
    {
	for( int slot_num = 0, slot_bit = 1; slot_num < ss->slot_cnt; slot_num++, slot_bit<<=1 )
	{
	    sundog_sound_slot* slot = &ss->slots[ slot_num ];
	    if( !slot->callback || slot->suspended || ( rendered_slots & slot_bit ) ) continue;
	    sundog_sound_slot_callback_t callback = slot->callback;
	    slot->out_buf_ptr = 0;
	    if( slot->wait_for_sync )
	    {
		if( ss->slot_sync == 0 ) continue;
		slot->out_buf_ptr = ss->slot_sync - 1;
		//printf( "SLOT %d SYNC %d / %d\n", slot_num, slot->out_buf_ptr, ss->out_frames );
		if( slot->out_buf_ptr < 0 || slot->out_buf_ptr >= ss->out_frames ) continue;
		slot->wait_for_sync = 0;
		if( not_filled )
		{
		    smem_clear( ss->out_buffer, ss->out_frames * frame_size );
		    not_filled = false;
		}
	    }
	    slot->in_buffer = NULL;
	    if( not_filled )
	    {
		slot->in_buffer = in_buffer;
		slot->buffer = ss->out_buffer;
		slot->frames = ss->out_frames;
		slot->time = ss->out_time;
		int r = callback( ss, slot_num );
		// r == 0 : silence, buffer is not filled;
		// r == 1 : buffer is filled;
		// r == 2 : silence, buffer is filled;
		if( r ) not_filled = false;
		if( r == 1 ) silence = false;
	    }
	    else
	    {
		slot->buffer = ss->slot_buffer;
	        while( 1 )
	        {
	            int size = ss->out_frames - slot->out_buf_ptr;
	            if( size > ss->slot_buffer_size )
	    		size = ss->slot_buffer_size;
	    	    if( in_buffer )
			slot->in_buffer = (int8_t*)in_buffer + slot->out_buf_ptr * in_frame_size;
		    slot->frames = size;
	    	    slot->time = ss->out_time;
		    if( slot->out_buf_ptr )
		        slot->time += (int64_t)slot->out_buf_ptr * stime_ticks_per_second() / ss->freq;
	    	    int r = callback( ss, slot_num );
	    	    if( r == 1 ) silence = false;
	    	    if( r )
		    {
	    		//Add result to the main buffer:
			int size2 = size * ss->out_channels;
	    		int dest_offset = slot->out_buf_ptr * ss->out_channels;
		        if( ss->out_type == sound_buffer_int16 )
	    		{
	    	    	    int16_t* dest = (int16_t*)ss->out_buffer + dest_offset;
	    	    	    int16_t* src = (int16_t*)ss->slot_buffer;
		            for( int i = 0; i < size2; i++ )
			    {
	    			int v = (int)dest[ i ] + src[ i ];
	    		        LIMIT_NUM( v, -32768, 32767 );
				dest[ i ] = (int16_t)v;
	    		    }
			}
	    		if( ss->out_type == sound_buffer_float32 )
			{
	    		    float* dest = (float*)ss->out_buffer + dest_offset;
			    float* src = (float*)ss->slot_buffer;
			    for( int i = 0; i < size2; i++ )
	    	    	    {
    		    		dest[ i ] += src[ i ];
    			    }
    			}
		    }
	    	    slot->out_buf_ptr += size;
    		    if( slot->out_buf_ptr >= ss->out_frames )
			break;
		}
	    }
	    rendered_slots |= slot_bit;
	}
	if( ss->slot_sync == 0 ) break;
        if( ss->slot_sync - 1 >= ss->out_frames ) break;
	for( int slot_num = 0; slot_num < SUNDOG_SOUND_SLOTS; slot_num++ )
	{
	    sundog_sound_slot* slot = &ss->slots[ slot_num ];
	    if( slot->callback && slot->suspended && slot->wait_for_sync )
		sundog_sound_play( ss, slot_num );
	}
    }
    ss->slot_sync -= ss->out_frames;
    if( ss->slot_sync < 0 ) ss->slot_sync = 0;

    if( ss->out_file )
    {
	uint8_t* src = (uint8_t*)ss->out_buffer;
	bool from_input = false;
        if( in_buffer && ( ss->out_file_flags & SCAP_FLAG_INPUT ) != 0 )
        {
	    uint8_t* src = (uint8_t*)in_buffer;
    	    from_input = true;
	    frame_size = in_frame_size;
        }
	size_t size = ss->out_frames * frame_size;
	size_t buf_size = smem_get_size( ss->out_file_buf );
	size_t src_ptr = 0;
	while( size )
	{
	    size_t avail = buf_size - ss->out_file_buf_wp;
	    if( avail > size )
		avail = size;
    	    if( !from_input && silence )
    	        memset( ss->out_file_buf + ss->out_file_buf_wp, 0, avail );
    	    else
    	    {
    	        smem_copy( ss->out_file_buf + ss->out_file_buf_wp, src + src_ptr, avail );
    	    }
	    size -= avail;
	    src_ptr += avail;
	    volatile size_t new_wp = ( ss->out_file_buf_wp + avail ) & ( buf_size - 1 );
	    COMPILER_MEMORY_BARRIER();
	    ss->out_file_buf_wp = new_wp;
	}
    }

    if( ( flags & SUNDOG_SOUND_CALLBACK_FLAG_DONT_LOCK ) == 0 )
    {
	smutex_unlock( &ss->mutex );
    }

mutex_end:

    if( not_filled )
    {
	smem_clear( ss->out_buffer, ss->out_frames * frame_size );
    }

    if( silence )
    {
#ifndef SUNDOG_MODULE
	if( !in_buffer
	    && ss->sd
	    && ss == ss->sd->ss
#ifdef AUDIOBUS
	    && !g_ab_connected
#endif
	    )
        {
    	    ss->sd->ss_idle_frame_counter += ss->out_frames;
	}
#endif
	return 0;
    }
    else
    {
#ifndef SUNDOG_MODULE
	if( ss->sd && ss == ss->sd->ss )
	    ss->sd->ss_idle_frame_counter = 0;
#endif
	return 1;
    }
#endif
}

int sundog_sound_global_init()
{
#ifndef NOSOUND
    smutex_init( &g_sundog_sound_mutex, 0 );
#endif
    return 0;
}

int sundog_sound_global_deinit()
{
#ifndef NOSOUND
    smutex_destroy( &g_sundog_sound_mutex );
#endif
    return 0;
}

//Check the initial values (type,freq,channels,flags) and set defaults
//Use this function in device_sound_init():
//  1) set device-specific defaults;
//  2) sundog_sound_set_defaults();
//  3) if the device can't work with these parameters, set the correct values;
int sundog_sound_set_defaults( sundog_sound* ss ) 
{
    if( !ss ) return -1;

    if( ss->out_type == sound_buffer_default ) ss->out_type = sound_buffer_int16;
    if( ss->in_type == sound_buffer_default ) ss->in_type = sound_buffer_int16;
    if( ss->freq <= 0 ) ss->freq = sconfig_get_int_value( APP_CFG_SND_FREQ, 44100, 0 );
    if( ss->out_channels <= 0 ) ss->out_channels = 2;
    if( ss->in_channels <= 0 ) ss->in_channels = 2;

#ifdef MIN_SAMPLE_RATE
    if( ss->freq < MIN_SAMPLE_RATE )
    {
        slog( "WARNING. Wrong sample rate %d (must be >= %d). Using %d.\n", ss->freq, MIN_SAMPLE_RATE, MIN_SAMPLE_RATE );
        ss->freq = MIN_SAMPLE_RATE;
    }
#endif
#ifdef ONLY44100
    if( ss->freq != 44100 ) 
    {
        slog( "WARNING. Sample rate must be 44100 for this device. Using 44100.\n" );
	ss->freq = 44100;
    }
#endif

    return 0;
}

int sundog_sound_init( sundog_sound* ss, sundog_engine* sd, sound_buffer_type type, int freq, int channels, uint32_t flags )
{
    int rv = 0;

#ifndef NOSOUND

    if( !ss ) return -1;
    if( ss->initialized ) return 0;

    smutex_lock( &g_sundog_sound_mutex );

    rv = -1;
    while( 1 )
    {
	smem_clear( ss, sizeof( sundog_sound ) );
	ss->out_type = type;
	ss->in_type = type;
	ss->flags = flags;
	ss->freq = freq;
	ss->out_channels = channels;
	ss->in_channels = channels;
	ss->sd = sd;

	for( int i = 0; i < SUNDOG_SOUND_SLOTS; i++ ) ss->slots[ i ].suspended = true;

	ss->driver = DEFAULT_SDRIVER;
	const char* driver_name = NULL;
	driver_name = sconfig_get_str_value( APP_CFG_SND_DRIVER, "", 0 );
        if( driver_name )
	{
    	    for( int i = 0; i < NUMBER_OF_SDRIVERS; i++ )
    	    {
        	if( strcmp( driver_name, g_sdriver_names[ i ] ) == 0 )
        	{
            	    ss->driver = i;
            	    break;
        	}
    	    }
	}

	smutex_init( &ss->mutex, 0 );
	smutex_init( &ss->in_mutex, 0 );

	if( flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
	{
	    sundog_sound_set_defaults( ss );
	    rv = 0;
	}
	else
	{
	    rv = device_sound_init( ss );
	    if( rv ) break;
	    if( ( flags & SUNDOG_SOUND_FLAG_DEFERRED_INIT ) == 0 )
		ss->device_initialized = true;
	}

	int frame = g_sample_size[ ss->out_type ] * ss->out_channels;
	ss->slot_buffer_size = 1024 * 8;
	ss->slot_buffer = SMEM_ALLOC( ss->slot_buffer_size * frame );

	if( sd )
	{
	    if( sd->ss == NULL )
	    {
		sd->ss = ss;
		sd->ss_sample_rate = ss->freq;
	    }
	}

	COMPILER_MEMORY_BARRIER();
	ss->initialized = true;

	break;
    }

    if( rv == 0 )
    {
	g_sundog_sound_cnt++;
    }
    smutex_unlock( &g_sundog_sound_mutex );

#endif

    return rv;
}

int sundog_sound_init_deferred( sundog_sound* ss )
{
    int rv = 0;

#ifndef NOSOUND

    if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    if( ( ss->flags & SUNDOG_SOUND_FLAG_DEFERRED_INIT ) == 0 ) return -1;
    if( ss->device_initialized ) return 0;

    rv = -1;
    while( 1 )
    {
	rv = device_sound_init( ss );
	if( rv ) break;
	ss->device_initialized = true;

	break;
    }

#endif
    
    return rv;
}

int sundog_sound_deinit( sundog_sound* ss )
{
    int rv = 0;

#ifndef NOSOUND 

    if( !ss ) return -1;
    if( !ss->initialized ) return -1;

    slog( "SOUND: sundog_sound_deinit() begin\n" );

    smutex_lock( &g_sundog_sound_mutex );
    
    sundog_sound_capture_stop( ss );

    if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
    {
    }
    else
    {
	if( ss->device_initialized )
	{
	    rv = device_sound_deinit( ss );
	    ss->device_initialized = false;
	}
    }

    if( ss->slot_buffer )
	smem_free( ss->slot_buffer );

    smutex_destroy( &ss->mutex );
    smutex_destroy( &ss->in_mutex );
    
    if( ss->sd )
    {
	if( ss->sd->ss == ss )
	    ss->sd->ss = NULL;
    }
    
    slog( "SOUND: sundog_sound_deinit() end\n" );
    ss->initialized = false;

    g_sundog_sound_cnt--;
    smutex_unlock( &g_sundog_sound_mutex );

#endif

    //if rv != 0: some errors occurred, but the stream object is closed anyway
    return rv;
}

int sundog_sound_get_free_slot( sundog_sound* ss )
{
#ifndef NOSOUND

    if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    int slot;
    for( slot = 0; slot < SUNDOG_SOUND_SLOTS; slot++ )
    {
        sundog_sound_slot_callback_t c = sundog_sound_get_slot_callback( ss, slot );
        if( !c ) break;
    }
    if( slot >= SUNDOG_SOUND_SLOTS ) slot = -1;
    return slot;

#endif
}

void sundog_sound_set_slot_callback( sundog_sound* ss, int slot, sundog_sound_slot_callback_t callback, void* user_data )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "SET SOUND SLOT CALLBACK %d\n", slot );
#endif
    sundog_sound_stop( ss, slot );
    ss->slots[ slot ].callback = callback;
    ss->slots[ slot ].user_data = user_data;

#endif
}

sundog_sound_slot_callback_t sundog_sound_get_slot_callback( sundog_sound* ss, int slot )
{
#ifndef NOSOUND

    if( !ss ) return 0;
    if( !ss->initialized ) return 0;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return 0;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "GET SOUND SLOT CALLBACK %d\n", slot );
#endif
    return ss->slots[ slot ].callback;

#endif
}

void sundog_sound_remove_slot_callback( sundog_sound* ss, int slot )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "REMOVE SOUND SLOT CALLBACK %d\n", slot );
#endif
    sundog_sound_stop( ss, slot );
    ss->slots[ slot ].callback = NULL;
    
#endif
}

void sundog_sound_lock( sundog_sound* ss )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "LOCK\n" );
#endif
    smutex_lock( &ss->mutex );
#ifdef SHOW_DEBUG_MESSAGES
    slog( "LOCK OK\n" );
#endif

#endif
}

void sundog_sound_unlock( sundog_sound* ss )
{
#ifndef NOSOUND 
    
    if( !ss ) return;
    if( !ss->initialized ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "UNLOCK\n" );
#endif
    smutex_unlock( &ss->mutex );
#ifdef SHOW_DEBUG_MESSAGES
    slog( "UNLOCK OK\n" );
#endif

#endif
}

void sundog_sound_play( sundog_sound* ss, int slot )
{
#ifndef NOSOUND 

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "PLAY %d\n", slot );
#endif
    if( ss->slots[ slot ].callback )
    {
	if( ss->slots[ slot ].suspended )
	{
	    if( !( ss->flags & SUNDOG_SOUND_FLAG_ONE_THREAD ) ) sundog_sound_lock( ss );
	    ss->slots[ slot ].suspended = false;
	    ss->slot_cnt = 0;
	    for( int i = 0; i < SUNDOG_SOUND_SLOTS; i++ ) if( ss->slots[ i ].callback ) ss->slot_cnt = i + 1;
	    if( !( ss->flags & SUNDOG_SOUND_FLAG_ONE_THREAD ) ) sundog_sound_unlock( ss );
	}
    }
#ifdef SHOW_DEBUG_MESSAGES
    slog( "PLAY OK %d\n", slot );
#endif
    
#endif
}

void sundog_sound_stop( sundog_sound* ss, int slot )
{
#ifndef NOSOUND 
    
    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
#ifdef SHOW_DEBUG_MESSAGES
    slog( "STOP %d\n", slot );
#endif
    if( ss->slots[ slot ].callback )
    {
	if( !ss->slots[ slot ].suspended )
	{
	    if( !( ss->flags & SUNDOG_SOUND_FLAG_ONE_THREAD ) ) sundog_sound_lock( ss );
	    ss->slots[ slot ].suspended = true;
	    if( !( ss->flags & SUNDOG_SOUND_FLAG_ONE_THREAD ) ) sundog_sound_unlock( ss );
	}
    }
#ifdef SHOW_DEBUG_MESSAGES
    slog( "STOP OK %d\n", slot );
#endif

#endif
}

int sundog_sound_is_slot_suspended( sundog_sound* ss, int slot )
{
    int rv = 0;

#ifndef NOSOUND

    if( !ss ) return 0;
    if( !ss->initialized ) return 0;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return 0;
    sundog_sound_slot* s = &ss->slots[ slot ];
    rv = s->suspended;
    if( s->wait_for_sync ) rv = 0; //bacause it can be resumed at any time

#endif

    return rv;
}

//Can be called from the sound callback only!
//slot - the slot from which we call this signal;
//frame_number - signal position (relative to user callback buffer);
void sundog_sound_slot_sync( sundog_sound* ss, int slot, int frame_number )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
    ss->slot_sync = ss->slots[ slot ].out_buf_ptr + frame_number + 1;
    //printf( "SLOT %d SET SYNC %d + %d + 1\n", slot, ss->slots[ slot ].out_buf_ptr, frame_number );

#endif
}

void sundog_sound_sync_play( sundog_sound* ss, int slot, bool wait_for_sync )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( (unsigned)slot >= SUNDOG_SOUND_SLOTS ) return;
    ss->slots[ slot ].wait_for_sync = wait_for_sync;

#endif
}

int sundog_sound_device_play( sundog_sound* ss )
{
    int rv = 0;

    if( !ss ) return -1;

#ifndef NOSOUND

    smutex_lock( &g_sundog_sound_mutex );
    while( 1 )
    {
	if( !ss->initialized ) { rv = -1; break; }
	if( !ss->device_initialized ) break;
	if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
	{
	}
	else
	{
	    int res = device_sound_init( ss );
	    if( res )
		rv = res;
	}
	break;
    }
    smutex_unlock( &g_sundog_sound_mutex );
    
#endif
    
    return rv;
}

void sundog_sound_device_stop( sundog_sound* ss )
{
    if( !ss ) return;
    
#ifndef NOSOUND
    
    smutex_lock( &g_sundog_sound_mutex );
    while( 1 )
    {
	if( !ss->initialized ) break;
	if( !ss->device_initialized ) break;
	if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
	{
	}
	else
	{
	    device_sound_deinit( ss );
	}
	break;
    }
    smutex_unlock( &g_sundog_sound_mutex );
    
#endif
}

//Call it wihtin the main app thread, where the sound stream is not locked
//or use sundog_sound_input_request()
void sundog_sound_input( sundog_sound* ss, bool enable )
{
#ifndef NOSOUND
    
    if( !ss ) return;
    if( !ss->initialized ) return;
    if( enable ) 
	ss->in_enabled++;
    else
	ss->in_enabled--;
    if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
    {
    }
    else
    {
	smutex_lock( &g_sundog_sound_mutex );
	if( ss->in_enabled == 0 )
	    device_sound_input( ss, false );
	if( enable && ss->in_enabled == 1 )
	    device_sound_input( ss, true );
	smutex_unlock( &g_sundog_sound_mutex );
    }

#endif
}

void sundog_sound_input_request( sundog_sound* ss, bool enable )
{
#ifndef NOSOUND
    
    if( !ss ) return;
    if( !ss->initialized ) return;
    if( enable ) 
	ss->in_request++;
    else
	ss->in_request--;

#endif
}

//Call it wihtin the main app thread, where the sound stream is not locked
void sundog_sound_handle_input_requests( sundog_sound* ss )
{
#ifndef NOSOUND
    
    if( !ss ) return;
    if( !ss->initialized ) return;
    if( ss->in_request_answer < ss->in_request )
    {
	if( ss->in_request_answer == 0 )
	    sundog_sound_input( ss, true );
    }
    if( ss->in_request_answer > ss->in_request )
    {
	if( ss->in_request_answer > 0 && ss->in_request <= 0 )
	    sundog_sound_input( ss, false );
    }
    ss->in_request_answer = ss->in_request;

#endif
}

const char* sundog_sound_get_driver_name( sundog_sound* ss )
{
#ifndef NOSOUND
    
    if( !ss ) return "";
    if( !ss->initialized ) return "";
    if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
	return "unknown";
    else
    {
	if( (unsigned)ss->driver < NUMBER_OF_SDRIVERS )
    	    return g_sdriver_names[ ss->driver ];
    	else
	    return "unknown";
    }
    
#else

    return "nosound";
    
#endif
}

const char* sundog_sound_get_driver_info( sundog_sound* ss )
{
#ifndef NOSOUND
    
    if( !ss ) return "";
    if( !ss->initialized ) return "";
    if( ss->flags & SUNDOG_SOUND_FLAG_USER_CONTROLLED )
	return "Unknown (user-defined)";
    else
    {
	if( (unsigned)ss->driver < NUMBER_OF_SDRIVERS )
    	    return g_sdriver_infos[ ss->driver ];
    	else
	    return "Unknown (wrong driver number)";
    }
    
#else

    return "No sound";
    
#endif
}

const char* sundog_sound_get_default_driver()
{
#ifndef NOSOUND
    
    return g_sdriver_names[ DEFAULT_SDRIVER ];
    
#else

    return "nosound";
    
#endif
}

int sundog_sound_get_drivers( char*** names, char*** infos )
{
#ifndef NOSOUND

    if( NUMBER_OF_SDRIVERS <= 1 ) return 0;
    char** n = SMEM_ALLOC2( char*, NUMBER_OF_SDRIVERS );
    char** i = SMEM_ALLOC2( char*, NUMBER_OF_SDRIVERS );
    for( int d = 0; d < NUMBER_OF_SDRIVERS; d++ )
    {
        n[ d ] = SMEM_ALLOC2( char, smem_strlen( g_sdriver_names[ d ] ) + 1 );
        n[ d ][ 0 ] = 0;
        SMEM_STRCAT_D( n[ d ], g_sdriver_names[ d ] );
        i[ d ] = SMEM_ALLOC2( char, smem_strlen( g_sdriver_infos[ d ] ) + 1 );
        i[ d ][ 0 ] = 0;
        SMEM_STRCAT_D( i[ d ], g_sdriver_infos[ d ] );
    }
    *names = n;
    *infos = i;
    return NUMBER_OF_SDRIVERS;

#else

    return 0;
    
#endif
}

int sundog_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
#ifndef NOSOUND

    return device_sound_get_devices( driver, names, infos, input );

#else

    return 0;
    
#endif
}

void* sundog_sound_capture_thread( void* data )
{
    sundog_sound* ss = (sundog_sound*)data;
    size_t buf_size = smem_get_size( ss->out_file_buf );
    while( ss->out_file_exit_request == 0 )
    {
	size_t rp = ss->out_file_buf_rp;
	size_t wp = ss->out_file_buf_wp;
	if( rp != wp )
	{
	    size_t size = ( wp - rp ) & ( buf_size - 1 );
	    while( size )
	    {
		size_t avail = buf_size - rp;
		if( avail > size )
            	    avail = size;
            	ss->out_file_size += sfs_write( ss->out_file_buf + rp, 1, avail, ss->out_file );
            	rp = ( rp + avail ) & ( buf_size - 1 );
            	size -= avail;
	    }
	    ss->out_file_buf_rp = rp;
	}
	else
	{
	    stime_sleep( 50 );
	}
    }
    ss->out_file_exit_request = 0;
    return 0;
}

int sundog_sound_capture_start( sundog_sound* ss, const char* filename, uint32_t flags )
{
#ifndef NOSOUND

    if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    if( ss->out_file ) return -1;
    int rv = -1;
    
    sfs_file f = sfs_open( filename, "wb" );
    if( f )
    {
	sound_buffer_type type;
        uint32_t bits = 16;
        int channels = 2;
        if( flags & SCAP_FLAG_INPUT )
	{
	    type = ss->in_type;
    	    channels = ss->in_channels;
	}
	else
	{        
	    type = ss->out_type;
    	    channels = ss->out_channels;
    	}
    	if( type == sound_buffer_float32 ) bits = 32;
	
	//WAV HEADER:
	uint32_t n;
	sfs_write( "RIFF", 1, 4, f );
	n = 4 + 24 + 8; sfs_write( &n, 1, 4, f );
        sfs_write( "WAVE", 1, 4, f );
        
        //WAV FORMAT:
        sfs_write( "fmt ", 1, 4, f );
        n = 16; sfs_write( &n, 1, 4, f );
        n = 1; if( type == sound_buffer_float32 ) n = 3; sfs_write( &n, 1, 2, f ); //format
        n = channels; sfs_write( &n, 1, 2, f ); //channels
        n = ss->freq; sfs_write( &n, 1, 4, f ); //frames per second
        n = ss->freq * channels * ( bits / 8 ); sfs_write( &n, 1, 4, f ); //bytes per second
        n = channels * ( bits / 8 ); sfs_write( &n, 1, 2, f ); //block align
        sfs_write( &bits, 1, 2, f ); //bits
        
        //WAV DATA:
        sfs_write( "data", 1, 4, f );
        sfs_write( &n, 1, 4, f ); //size

	int frame_size = g_sample_size[ type ] * channels;
        uint8_t* buf = SMEM_ALLOC2( uint8_t, round_to_power_of_two( 2 * ss->freq * frame_size ) );
        
	sundog_sound_lock( ss );
	ss->out_file = f;
	ss->out_file_flags = flags;
	ss->out_file_size = 0;
	ss->out_file_buf = buf;
	ss->out_file_buf_wp = 0;
	ss->out_file_buf_rp = 0;
        sundog_sound_unlock( ss );

        sthread_create( &ss->out_file_thread, ss->sd, sundog_sound_capture_thread, ss, 0 );
        
        slog( "Audio capturer started.\n" );
	rv = 0;
    }
    else
    {
	slog( "Can't open %s for writing\n", filename );
    }
    
    return rv;

#else

    return -1;
    
#endif
}

void sundog_sound_capture_stop( sundog_sound* ss )
{
#ifndef NOSOUND

    if( !ss ) return;
    if( !ss->initialized ) return;
    if( ss->out_file == 0 ) return;

    ss->out_file_exit_request = 1;
    sthread_destroy( &ss->out_file_thread, 5000 );

    sfs_file f = ss->out_file;

    sundog_sound_lock( ss );
    ss->out_file = 0;
    smem_free( ss->out_file_buf );
    ss->out_file_buf = 0;
    sundog_sound_unlock( ss );

    //Fix WAV data:
    uint32_t n;
    sfs_seek( f, 4, SFS_SEEK_SET );
    n = 4 + 24 + 8 + ss->out_file_size; sfs_write( &n, 1, 4, f );
    sfs_seek( f, 36 + 4, SFS_SEEK_SET );
    n = ss->out_file_size; sfs_write( &n, 1, 4, f );

    sfs_close( f );

    slog( "Audio capturer stopped. Received %d bytes\n", (int)ss->out_file_size );

#endif
}

//
// MIDI
//

int sundog_midi_global_init()
{
#ifndef NOMIDI
#endif
    return 0;
}

int sundog_midi_global_deinit()
{
#ifndef NOMIDI
#endif
    return 0;
}

int sundog_midi_client_open( sundog_midi_client* c, sundog_engine* sd, sundog_sound* ss, const char* name, uint32_t flags )
{
    int rv = 0;

#ifndef NOMIDI

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_open() begin\n" );
#endif

#if defined(OS_APPLE) && !defined(AUDIOUNIT_EXTENSION)
    flags |= MIDI_CLIENT_FLAG_PORTS_MUTEX; //for midi_notify_callback() that can change the ports (remove/reopen) in bg
					   //But is it really necessary? Please test it in future updates!
#endif

    smem_clear( c, sizeof( sundog_midi_client ) );
    c->sd = sd;
    c->ss = ss;
    c->flags = flags;
    c->name = SMEM_STRDUP( name );

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	smutex_init( &c->ports_mutex, 0 );
    }

    c->driver = DEFAULT_MDRIVER;
    if( ss )
    {
        for( int i = 0; i < NUMBER_OF_MDRIVERS; i++ )
        {
            if( strcmp( g_sdriver_names[ ss->driver ], g_mdriver_names[ i ] ) == 0 )
            {
                c->driver = i;
                break;
            }
        }
    }
    const char* driver_name = NULL;
    driver_name = sconfig_get_str_value( APP_CFG_MIDI_DRIVER, "", 0 );
    if( driver_name )
    {
        for( int i = 0; i < NUMBER_OF_MDRIVERS; i++ )
        {
            if( strcmp( driver_name, g_mdriver_names[ i ] ) == 0 )
            {
                c->driver = i;
                break;
            }
        }
    }

    rv = device_midi_client_open( c, name );

    if( rv == 0 )
    {
	c->flags |= MIDI_CLIENT_FLAG_INITIALIZED;
    }
    else
    {
	slog( "sundog_midi_client_open() error %d\n", rv );
	smem_free( c->name ); c->name = NULL;
	if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX ) smutex_destroy( &c->ports_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_open() end %d\n", rv );
#endif

#endif

    return rv;
}

int sundog_midi_client_close( sundog_midi_client* c )
{
    int rv = 0;

#ifndef NOMIDI
    if( c && !( c->flags & MIDI_CLIENT_FLAG_INITIALIZED ) ) return 0;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close() begin\n" );
#endif

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	smutex_lock( &c->ports_mutex );

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close() locked\n" );
#endif

    for( int i = 0; i < SUNDOG_MIDI_PORTS; i++ )
    {
	if( c->ports[ i ] && c->ports[ i ]->active )
	{
    	    sundog_midi_client_close_port( c, i );
	}
	smem_free( c->ports[ i ] );
	c->ports[ i ] = NULL;
    }

    rv = device_midi_client_close( c );

    c->flags &= ~MIDI_CLIENT_FLAG_INITIALIZED;

    smem_free( c->name );
    c->name = NULL;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close() unlocking...\n" );
#endif

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	smutex_unlock( &c->ports_mutex );
	smutex_destroy( &c->ports_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close() end\n" );
#endif

#endif

    return rv;
}

int sundog_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags )
{
    int rv = 0;

#ifndef NOMIDI
    if( c && !( c->flags & MIDI_CLIENT_FLAG_INITIALIZED ) ) return 0;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_get_devices() begin\n" );
#endif

    rv = device_midi_client_get_devices( c, devices, flags );

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_get_devices() end\n" );
#endif

#endif

    return rv;
}

static void recalc_active_ports( sundog_midi_client* c )
{
    int cnt = 0;
    for( int i = SUNDOG_MIDI_PORTS - 1; i >= 0; i-- )
    {
	if( c->ports[ i ] )
	{
	    if( c->ports[ i ]->active )
	    {
		cnt = i + 1;
		break;
	    }
	}
    }
    c->ports_cnt = cnt;
}

int sundog_midi_client_open_port( sundog_midi_client* c, const char* port_name, const char* dev_name, uint32_t flags )
{
    int rv = -1;

#ifndef NOMIDI
    if( c && !( c->flags & MIDI_CLIENT_FLAG_INITIALIZED ) ) return -1;

    if( port_name == NULL ) return -1;
    if( dev_name == NULL ) return -1;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_open_port() begin\n" );
#endif
    
    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	smutex_lock( &c->ports_mutex );
    }
    
    while( 1 )
    {
	for( int i = 0; i < SUNDOG_MIDI_PORTS; i++ )
	{
	    if( c->ports[ i ] )
	    {
		if( c->ports[ i ]->active == 0 )
		{
		    rv = i;
		    break;
		}
	    }
	    else
	    {
		sundog_midi_port* port = SMEM_ZALLOC2( sundog_midi_port, 1 );
		if( port )
		{
		    COMPILER_MEMORY_BARRIER(); //guarantee that the cleared (empty) port will be written below
		    c->ports[ i ] = port;
		    rv = i;
		    break;
		}
	    }
	}
	if( rv < 0 )
	{
	    slog( "Not enough free slots for the new MIDI port.\n" );
	    break;
	}
	
	sundog_midi_port* port = c->ports[ rv ];
	
	port->flags = flags;
	
	if( device_midi_client_open_port( c, rv, port_name, dev_name, flags ) )
	{
	    port->flags |= MIDI_NO_DEVICE;
	    port->device_specific = 0;
	}
	else
	{
	    port->flags &= ~MIDI_NO_DEVICE;
	}
	
	smem_free( port->port_name );
	port->port_name = SMEM_STRDUP( port_name );

	smem_free( port->dev_name );
	port->dev_name = SMEM_STRDUP( dev_name );
	
	COMPILER_MEMORY_BARRIER();
	port->active = 1;
	recalc_active_ports( c );
	
	break;
    }
    
    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	smutex_unlock( &c->ports_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_open_port() end %d\n", rv );
#endif
    
#endif
    
    return rv;
}

int sundog_midi_client_reopen_port( sundog_midi_client* c, int pnum )
{
    int rv = -1;

#ifndef NOMIDI
    if( !c ) return -1;
    if( !( c->flags & MIDI_CLIENT_FLAG_INITIALIZED ) ) return -1;
    if( (unsigned)pnum >= (unsigned)SUNDOG_MIDI_PORTS ) return -1;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_reopen_port() begin\n" );
#endif

    bool locked = false;

    while( 1 )
    {
	if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	{
	    smutex_lock( &c->ports_mutex );
	    locked = true;
	}

	sundog_midi_port* port = c->ports[ pnum ];
	if( port == NULL ) break;
	if( port->active == 0 ) break;

	if( ( port->flags & MIDI_NO_DEVICE ) == 0 )
	    device_midi_client_close_port( c, pnum );
	if( device_midi_client_open_port( c, pnum, port->port_name, port->dev_name, port->flags ) )
	{
	    port->flags |= MIDI_NO_DEVICE;
	    port->device_specific = 0;
	}
	else
	{
	    port->flags &= ~MIDI_NO_DEVICE;
	}

	rv = 0;
	break;
    }

    if( locked )
    {
	smutex_unlock( &c->ports_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_reopen_port() end %d\n", rv );
#endif

#endif

    return rv;
}

int sundog_midi_client_close_port( sundog_midi_client* c, int pnum )
{
    int rv = -1;

#ifndef NOMIDI
    if( !c ) return -1;
    if( !( c->flags & MIDI_CLIENT_FLAG_INITIALIZED ) ) return -1;
    if( (unsigned)pnum >= (unsigned)SUNDOG_MIDI_PORTS ) return -1;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close_port(%d) begin\n", pnum );
#endif

    bool locked = false;

    while( 1 )
    {
	if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	{
	    smutex_lock( &c->ports_mutex );
	    locked = true;
	}

	sundog_midi_port* port = c->ports[ pnum ];
	if( port == NULL ) break;
	if( port->active == 0 ) break;

	if( ( port->flags & MIDI_NO_DEVICE ) == 0 )
	    rv = device_midi_client_close_port( c, pnum );

	smem_free( port->port_name );
	smem_free( port->dev_name );
	port->port_name = NULL;
	port->dev_name = NULL;

	COMPILER_MEMORY_BARRIER();
	port->active = 0;
	recalc_active_ports( c );

	rv = 0;
	break;
    }
    
    if( locked )
    {
	smutex_unlock( &c->ports_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "sundog_midi_client_close_port() end %d\n", rv );
#endif

#endif

    return rv;
}

sundog_midi_event* sundog_midi_client_get_event( sundog_midi_client* c, int pnum )
{
    sundog_midi_event* evt = NULL;

#ifndef NOMIDI

    if( !c ) return NULL;
    if( (unsigned)pnum >= (unsigned)SUNDOG_MIDI_PORTS ) return NULL;

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	if( smutex_trylock( &c->ports_mutex ) ) return NULL;
    }

    sundog_midi_port* port = c->ports[ pnum ];
    if( port && port->active )
    {
	evt = device_midi_client_get_event( c, pnum );
    }

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	smutex_unlock( &c->ports_mutex );

#endif

    return evt;
}

int sundog_midi_client_next_event( sundog_midi_client* c, int pnum )
{
    int rv = -1;

#ifndef NOMIDI

    if( !c ) return -1;
    if( (unsigned)pnum >= (unsigned)SUNDOG_MIDI_PORTS ) return -1;

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
    {
	if( smutex_trylock( &c->ports_mutex ) ) return 0;
    }

    sundog_midi_port* port = c->ports[ pnum ];
    if( port && port->active )
    {
	rv = device_midi_client_next_event( c, pnum );
    }

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	smutex_unlock( &c->ports_mutex );

#endif

    return rv;
}

int sundog_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt )
{
    int rv = -1;

#ifndef NOMIDI

    if( !c ) return -1;
    if( (unsigned)pnum >= (unsigned)SUNDOG_MIDI_PORTS ) return -1;
    if( evt == NULL || evt->size == 0 || evt->data == NULL ) return -1;

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	smutex_lock( &c->ports_mutex );

    sundog_midi_port* port = c->ports[ pnum ];
    if( port && port->active )
    {
	c->midi_out_activity = true;
#ifndef SUNDOG_MODULE
	if( c->sd && c->sd ) c->sd->ss_idle_frame_counter = 0;
#endif
	rv = device_midi_client_send_event( c, pnum, evt );
    }

    if( c->flags & MIDI_CLIENT_FLAG_PORTS_MUTEX )
	smutex_unlock( &c->ports_mutex );

#endif

    return rv;
}
