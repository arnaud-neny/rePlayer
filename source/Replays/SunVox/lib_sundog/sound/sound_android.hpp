//
// Android: sound
//

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "main/android/sundog_bridge.h"

#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <cstring>

enum
{
    SDRIVER_OPENSLES,
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
    "OpenSL ES"
};

const char* g_sdriver_names[] =
{
    "opensles"
};

#define MAX_NUMBER_INTERFACES 4
#define MAX_NUMBER_INPUT_DEVICES 4

const int g_input_buffers_count = 8;

struct device_sound //device-specific data of the sundog_sound object
{
    int 			buffer_size; //frames
    uint* 			output_buffer;
    int 			output_buffer_offset;
    volatile int 		output_callback_stop_request;

    volatile uint 		input_buffer_wp;
    volatile uint 		input_buffer_rp;
    uint* 			input_buffer;
    uint* 			input_silent_buffer;
    bool 			input_enabled;
    int 			input_fadein;

    SLObjectItf 		sl;
    SLObjectItf 		sl_output_mix;
    SLObjectItf 		sl_player;
    SLObjectItf 		sl_recorder;
    SLAndroidConfigurationItf 	sl_config;
    SLDataFormat_PCM 		sl_pcm;

    bool			set_player_thread_affinity;
    bool			player_thread_affinity;
    cpu_set_t 			cpu_set;

#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
};

static int sl_check_error( SLresult res, const char* fname )
{
    if( res != SL_RESULT_SUCCESS )
    {
	slog( "OpenSLES ERROR %d (%s)\n", (int)res, fname );
	return 1;
    }
    return 0;
}

void playback_callback( SLAndroidSimpleBufferQueueItf queueItf, void* context )
{
    sundog_sound* ss = (sundog_sound*)context;
    device_sound* d = (device_sound*)ss->device_specific;

    if( d->set_player_thread_affinity )
    {
	if( d->player_thread_affinity == false )
	{
	    d->player_thread_affinity = true;
	    pid_t current_thread_id = gettid();
	    sched_setaffinity( current_thread_id, sizeof( cpu_set_t ), &d->cpu_set );
	}
    }

    /*static char cores[ 128 ];
    static int core_ptr = 0;
    cores[ core_ptr ] = '0' + sched_getcpu();
    core_ptr++;
    if( core_ptr >= sizeof( cores ) )
    {
	cores[ core_ptr - 1 ] = 0;
	core_ptr = 0;
	slog( "%s\n", cores );
    }*/

    ss->out_buffer = d->output_buffer + d->output_buffer_offset;
    ss->out_frames = d->buffer_size;
    ss->out_time = stime_ticks() + ( ( ( ( d->buffer_size << 15 ) / ss->freq ) * stime_ticks_per_second() ) >> 15 );
    if( d->input_fadein == 0 )
        ss->in_buffer = d->input_silent_buffer;
    else
        ss->in_buffer = d->input_buffer + d->input_buffer_rp;
    if( d->input_enabled )
    {
        if( d->input_fadein > 0 )
        {
    	    //Input is ready and we can't lose input packets now:
	    int step = 1; //ms
	    int t = 0;
	    int timeout = 100;
	    while( d->input_buffer_rp == d->input_buffer_wp )
	    {
	        int cur_step = timeout - t;
	        if( cur_step > step ) cur_step = step;
	        stime_sleep( cur_step );
	        t += cur_step;
	        if( t >= timeout ) break; //One input packet lost :(
	    }
	}
	if( d->input_buffer_rp != d->input_buffer_wp )
	{
	    d->input_buffer_rp += d->buffer_size;
	    d->input_buffer_rp %= d->buffer_size * g_input_buffers_count;
	    if( d->input_fadein < 32768 )
	    {
	        int16_t* inbuf = (int16_t*)ss->in_buffer;
	        for( int i = 0; i < ss->out_frames * 2; i++ )
	        {
	    	    int v = inbuf[ i ];
		    v *= d->input_fadein;
		    v /= 32768;
		    inbuf[ i ] = (int16_t)v;
		    d->input_fadein++;
		    if( d->input_fadein >= 32768 ) break;
		}
	    }
	}
    }
    sundog_sound_callback( ss, 0 );

    if( d->output_callback_stop_request > 0 )
    {
	d->output_callback_stop_request = 2;
	return;
    }

    (*queueItf)->Enqueue( queueItf, d->output_buffer + d->output_buffer_offset, d->buffer_size * 4 );
    d->output_buffer_offset += d->buffer_size;
    d->output_buffer_offset %= d->buffer_size * 2;
}

void recording_callback( SLAndroidSimpleBufferQueueItf queueItf, void* context )
{
    sundog_sound* ss = (sundog_sound*)context;
    device_sound* d = (device_sound*)ss->device_specific;

    //Re-use this buffer:
    (*queueItf)->Enqueue( 
	queueItf, 
	d->input_buffer 
	    + ( d->input_buffer_wp + ( g_input_buffers_count / 2 ) * d->buffer_size ) % ( d->buffer_size * g_input_buffers_count ), 
	d->buffer_size * 4 );

    volatile uint wp = d->input_buffer_wp;
    wp += d->buffer_size;
    wp %= d->buffer_size * g_input_buffers_count;
    d->input_buffer_wp = wp;
}

int device_sound_init( sundog_sound* ss )
{
    SLresult res = SL_RESULT_SUCCESS;

    slog( "device_sound_init() begin\n" );

    ss->device_specific = SMEM_ZALLOC( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;

    int android_audio_buf_size = 0;
    int android_audio_sample_rate = 0;

#ifndef NOMAIN
    android_audio_buf_size = android_sundog_get_audio_buffer_size( ss->sd );
    android_audio_sample_rate = android_sundog_get_audio_sample_rate( ss->sd );
    if( android_audio_sample_rate )
    {
	if( ss->freq <= 0 && sconfig_get_int_value( APP_CFG_SND_FREQ, 0, 0 ) <= 0 )
	{
	    ss->freq = android_audio_sample_rate;
	}
    }
#endif
    sundog_sound_set_defaults( ss );

    d->buffer_size = sconfig_get_int_value( APP_CFG_SND_BUF, 0, 0 );
#ifndef NOMAIN
    if( ss->freq == android_audio_sample_rate )
    {
	//Sample rate is optimal:
	if( android_audio_buf_size > 0 && android_audio_buf_size < 2048 )
	{
	    if( d->buffer_size == 0 )
	    {
		//Set the smallest latency:
		d->buffer_size = android_audio_buf_size;
		/*while( d->buffer_size < 256 )
		{
		    d->buffer_size *= 2;
		}*/
	    }
	    else
	    {
		//Correct the desired latency:
		int res = android_audio_buf_size;
		int cur = android_audio_buf_size;
		int min_delta = 100000;
		while( 1 )
		{
		    int delta = abs( d->buffer_size - cur );
		    if( delta < min_delta )
		    {
    			min_delta = delta;
    			res = cur;
		    }
		    else break;
		    cur += android_audio_buf_size;
		}
		d->buffer_size = res;
	    }
	}
    }
#endif
    if( d->buffer_size == 0 ) d->buffer_size = 1024;
    d->output_buffer = SMEM_ZALLOC2( uint, d->buffer_size * 2 ); // *2 = double buffered
    ss->out_latency = d->buffer_size;
    ss->out_latency2 = d->buffer_size;
    ss->in_type = sound_buffer_int16;
    ss->in_channels = ss->out_channels;
    d->output_callback_stop_request = 0;
    d->input_buffer_wp = 0;
    d->input_buffer_rp = 0;
    d->input_enabled = false;

    slog( "SLES: %dHz %d; optimal: %dHz %d\n", ss->freq, d->buffer_size, android_audio_sample_rate, android_audio_buf_size );

#ifndef NOMAIN
    android_sundog_set_sustained_performance_mode( ss->sd, 1 ); //need more tests...
    size_t cores_num = 0;
    int* cores = android_sundog_get_exclusive_cores( ss->sd, &cores_num );
    if( cores )
    {
	//On some devices, the foreground process may have one or more CPU cores exclusively reserved for it.
        //This method can be used to retrieve which cores that are (if any),
	//so the calling process can then use sched_setaffinity() to lock a thread to these cores.
	d->set_player_thread_affinity = true;
	slog( "SLES: exclusive cores:\n" );
	CPU_ZERO( &d->cpu_set );
	for( size_t i = 0; i < cores_num; i++ )
	{
	    CPU_SET( cores[ i ], &d->cpu_set );
	    slog( "%d\n", cores[ i ] );
	}
	free( cores );
    }
#else
    char* cores = sconfig_get_str_value( "exclcores", NULL, NULL );
    if( cores )
    {
	d->set_player_thread_affinity = true;
	slog( "SLES: exclusive cores:\n" );
	CPU_ZERO( &d->cpu_set );
	char core_str[ 32 ];
	const char* next = cores;
	while( 1 )
	{
	    core_str[ 0 ] = 0;
	    next = smem_split_str( core_str, sizeof( core_str ), next, ',', 0 );
	    if( core_str[ 0 ] != 0 )
	    {
		int core = string_to_int( core_str );
		CPU_SET( core, &d->cpu_set );
		slog( "%d\n", core );
	    }
	    if( !next ) break;
	}
    }
#endif

    while( 1 )
    {
	//SLEngineOption EngineOption[] = { (SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE };

	slog( "SLES: slCreateEngine\n" );
	//res = slCreateEngine( &d->sl, 1, EngineOption, 0, NULL, NULL );
	res = slCreateEngine( &d->sl, 0, NULL, 0, NULL, NULL );
	if( sl_check_error( res, "slCreateEngine" ) ) break;

	// Realizing the SL Engine in synchronous mode:
	//slog( "SLES: Realizing the SL Engine in synchronous mode\n" );
	res = (*d->sl)->Realize( d->sl, SL_BOOLEAN_FALSE );
	if( sl_check_error( res, "Realizing the SL Engine" ) ) break;

	// Get the SL Engine Interface which is implicit:
	SLEngineItf EngineItf;
	//slog( "SLES: Get the SL Engine Interface\n" );
	res = (*d->sl)->GetInterface( d->sl, SL_IID_ENGINE, (void*)&EngineItf );
	if( sl_check_error( res, "Get the SL Engine Interface which is implicit" ) ) break;

	// Create Output Mix object to be used by player:
	slog( "SLES: Create Output Mix object\n" );
	res = (*EngineItf)->CreateOutputMix( EngineItf, &d->sl_output_mix, 0, 0, 0 );
	if( sl_check_error( res, "Create Output Mix object" ) ) break;

	// Realizing the Output Mix object:
	//slog( "SLES: Realizing the Output Mix object\n" );
	res = (*d->sl_output_mix)->Realize( d->sl_output_mix, SL_BOOLEAN_FALSE );
	if( sl_check_error( res, "Realizing the Output Mix object" ) ) break;

	// Setup the data source structure for the buffer queue:
	SLDataLocator_BufferQueue bufferQueue;
	bufferQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
	bufferQueue.numBuffers = 2;

	// Setup the format of the content in the buffer queue:
	d->sl_pcm.formatType = SL_DATAFORMAT_PCM;
	d->sl_pcm.numChannels = 2;
	d->sl_pcm.samplesPerSec = ss->freq * 1000;
	d->sl_pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	d->sl_pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	d->sl_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
	d->sl_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

	SLDataSource audioSource;
	audioSource.pFormat = &d->sl_pcm;
	audioSource.pLocator = &bufferQueue;

	SLDataSink audioSink;
	SLDataLocator_OutputMix locator_outputmix;

	// Setup the data sink structure:
	locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	locator_outputmix.outputMix = d->sl_output_mix;
	audioSink.pLocator = (void*)&locator_outputmix;
	audioSink.pFormat = NULL;

	// Set arrays required[] and iidArray[] for SEEK interface (PlayItf is implicit):
	const SLInterfaceID iidArray[] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
	const SLboolean required[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

 	// Create the music player:
	slog( "SLES: Create the music player\n" );
	res = (*EngineItf)->CreateAudioPlayer( EngineItf, &d->sl_player, &audioSource, &audioSink, 2, iidArray, required );
	if( sl_check_error( res, "Create the music player" ) ) break;

	// Realizing the player in synchronous mode:
	//slog( "SLES: Realizing the player\n" );
	res = (*d->sl_player)->Realize( d->sl_player, SL_BOOLEAN_FALSE );
	if( sl_check_error( res, "Realizing the player" ) ) break;

	// Get config interface:
	//slog( "SLES: Get config interface\n" );
	res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_ANDROIDCONFIGURATION, (void*)&d->sl_config );
	if( sl_check_error( res, "Get config interface" ) ) break;

	// Get seek and play interfaces:
	SLPlayItf playItf;
	//slog( "SLES: Get seek and play interfaces\n" );
	res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_PLAY, (void*)&playItf );
	if( sl_check_error( res, "Get seek and play interfaces" ) ) break;

	SLAndroidSimpleBufferQueueItf bufferQueueItf;
	//slog( "SLES: Get buffer queue interface\n" );
	res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, (void*)&bufferQueueItf );
	if( sl_check_error( res, "bufferQueueItf" ) ) break;

	// Setup to receive buffer queue event callbacks:
	slog( "SLES: Setup to receive buffer queue event callbacks\n" );
	res = (*bufferQueueItf)->RegisterCallback( bufferQueueItf, playback_callback, ss );
	if( sl_check_error( res, "Setup to receive buffer queue event callback" ) ) break;

	// Play the PCM samples using a buffer queue:
	slog( "SLES: Play\n" );
	res = (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PLAYING );
	if( sl_check_error( res, "Play" ) ) break;

	//slog( "SLES: Enqueue...\n" );
	res = (*bufferQueueItf)->Enqueue( bufferQueueItf, d->output_buffer, d->buffer_size * 4 );
	if( sl_check_error( res, "Start Enqueue 1" ) ) break;
	res = (*bufferQueueItf)->Enqueue( bufferQueueItf, d->output_buffer + d->buffer_size, d->buffer_size * 4 );
	if( sl_check_error( res, "Start Enqueue 2" ) ) break;

	break;
    }

    slog( "device_sound_init() end\n" );

    if( res != SL_RESULT_SUCCESS )
    {
	if( d->sl )
	{
	    (*d->sl)->Destroy( d->sl );
	    d->sl = 0;
	}
	REMOVE_DEVICE_SPECIFIC_DATA( ss );
	return 1;
    }
    else
    {
	return 0;
    }
}

int device_sound_deinit( sundog_sound* ss )
{
    SLresult res;
    device_sound* d = (device_sound*)ss->device_specific;
    if( d == 0 ) return 1;

    if( d->sl )
    {
	device_sound_input( ss, false );

	if( d->output_callback_stop_request == 0 )
	{
	    //SetPlayState( playItf, SL_PLAYSTATE_STOPPED ) does not stop the callback immediately - tested on LG Optimus Hub.
	    //But we must be sure that this callback is not uses SL functions anymore (Enqueue() in particular).
	    d->output_callback_stop_request = 1;
	    int step = 1;
	    int timeout = 1000;
	    int t = 0;
	    while( d->output_callback_stop_request != 2 )
	    {
		stime_sleep( step );
		t += step;
		if( t >= timeout ) break;
	    }
	}

	if( d->sl_player )
	{
	    SLPlayItf playItf;
	    res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_PLAY, (void*)&playItf );
	    if( res == SL_RESULT_SUCCESS )
	    {
		(*playItf)->SetPlayState( playItf, SL_PLAYSTATE_STOPPED );
	    }
	    (*d->sl_player)->Destroy( d->sl_player );
	    d->sl_player = 0;
	}

	if( d->sl_output_mix )
	{
	    (*d->sl_output_mix)->Destroy( d->sl_output_mix );
	    d->sl_output_mix = 0;
	}

	(*d->sl)->Destroy( d->sl );
	d->sl = 0;
    }

    smem_free( d->output_buffer );
    d->output_buffer = 0;

    smem_free( d->input_buffer );
    d->input_buffer = 0;

    smem_free( d->input_silent_buffer );
    d->input_silent_buffer = 0;

#ifndef NOMAIN
    android_sundog_set_sustained_performance_mode( ss->sd, 0 );
#endif

    REMOVE_DEVICE_SPECIFIC_DATA( ss );

    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( d == 0 ) return;
    if( enable )
    {
#ifndef NOMAIN
	android_sundog_check_for_permissions( ss->sd, ANDROID_PERM_RECORD_AUDIO );
#endif

	if( !d->input_buffer )
	{
	    d->input_buffer = SMEM_ZALLOC2( uint, d->buffer_size * g_input_buffers_count );
	    d->input_silent_buffer = SMEM_ZALLOC2( uint, d->buffer_size );
	}

	if( !d->sl_recorder )
	{
	    SLresult res = SL_RESULT_SUCCESS;
	    
	    while( 1 )
	    {
		// Get the SL Engine Interface which is implicit:
		SLEngineItf EngineItf;
		res = (*d->sl)->GetInterface( d->sl, SL_IID_ENGINE, (void*)&EngineItf );
		if( sl_check_error( res, "Get the SL Engine Interface which is implicit" ) ) break;
	
		// Initialize arrays required[] and iidArray[]:
		SLboolean required[ MAX_NUMBER_INTERFACES ];
		SLInterfaceID iidArray[ MAX_NUMBER_INTERFACES ];
		for( int i = 0; i < MAX_NUMBER_INTERFACES; i++ )
		{
		    required[ i ] = SL_BOOLEAN_FALSE;
		    iidArray[ i ] = SL_IID_NULL;
		}
	
		// Setup the data source structure:
		SLDataLocator_IODevice locator_mic;
		locator_mic.locatorType = SL_DATALOCATOR_IODEVICE;
		locator_mic.deviceType = SL_IODEVICE_AUDIOINPUT;
		locator_mic.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT;
		locator_mic.device = NULL;
		
		SLDataSource audioSource;
		audioSource.pLocator = &locator_mic;
		audioSource.pFormat = &d->sl_pcm;
		
		SLDataLocator_AndroidSimpleBufferQueue locator_bufferqueue;
		locator_bufferqueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
		locator_bufferqueue.numBuffers = 2;
		
		SLDataSink audioSink;
		audioSink.pLocator = &locator_bufferqueue;
		audioSink.pFormat = &d->sl_pcm;

		// Create audio recorder:
		required[ 0 ] = SL_BOOLEAN_TRUE;
		iidArray[ 0 ] = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
		res = (*EngineItf)->CreateAudioRecorder( EngineItf, &d->sl_recorder, &audioSource, &audioSink, 1, iidArray, required );
		if( sl_check_error( res, "CreateAudioRecorder" ) ) break;
		
		res = (*d->sl_recorder)->Realize( d->sl_recorder, SL_BOOLEAN_FALSE );
		if( sl_check_error( res, "Recorder Realize" ) ) break;
		
		// Register callback:
		SLRecordItf recorderRecord;
		res = (*d->sl_recorder)->GetInterface( d->sl_recorder, SL_IID_RECORD, &recorderRecord );
		if( sl_check_error( res, "Get SL_IID_RECORD interface" ) ) break;
		SLAndroidSimpleBufferQueueItf recorderBufferQueue;
		res = (*d->sl_recorder)->GetInterface( d->sl_recorder, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &recorderBufferQueue );
		if( sl_check_error( res, "Get SL_IID_ANDROIDSIMPLEBUFFERQUEUE interface" ) ) break;
		res = (*recorderBufferQueue)->RegisterCallback( recorderBufferQueue, recording_callback, ss );
		if( sl_check_error( res, "Recorder RegisterCallback" ) ) break;
		
		for( int i = 0; i < g_input_buffers_count / 2; i++ )
		{
		    (*recorderBufferQueue)->Enqueue( recorderBufferQueue, d->input_buffer + d->buffer_size * i, d->buffer_size * 4 );
		}
		
		// Start recording:
		d->input_buffer_wp = 0;
		d->input_buffer_rp = 0;
		d->input_fadein = 0;
		d->input_enabled = true;
		res = (*recorderRecord)->SetRecordState( recorderRecord, SL_RECORDSTATE_RECORDING );
		if( sl_check_error( res, "Start recording" ) ) break;
	    
		break;
	    }
	    
	    if( res != SL_RESULT_SUCCESS )
	    {
		if( d->sl_recorder )
		{
		    d->input_enabled = false;
		    (*d->sl_recorder)->Destroy( d->sl_recorder );
		    d->sl_recorder = NULL;
		}
	    }
	}
    }
    else 
    {
	if( d->sl_recorder )
	{
	    d->input_enabled = false;
	    (*d->sl_recorder)->Destroy( d->sl_recorder );
	    d->sl_recorder = NULL;
	}
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    return 0;
}
