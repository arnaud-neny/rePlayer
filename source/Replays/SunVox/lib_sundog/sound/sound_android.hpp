//
// Android: sound
//

/*
nov 2025:

Galaxy A56; Android 13:
OpenSL работает нормально, если буфер и частота - оптимальные (PROPERTY_OUTPUT_FRAMES_PER_BUFFER, PROPERTY_OUTPUT_SAMPLE_RATE)
Если изменить буфер, но оставить оптимальную частоту (48000), то начинает заикаться.
Если изменить буфер и частоту (например, 44100), то работает стабильно.
На оптимальных параметрах иногда все-таки заикается. Если я правильно помню, раньше такого не происходило (на этом же устройстве)...
Аудио thread постоянно переключается между ядрами. Эксклюзивных ядер система не дает.
Принудительный вызов sched_setaffinity() не имеет эффекта.

Добавил базовую поддержку AAudio (Android 8+).
Работает стабильно на любом буфере и частоте.
Но 192+48000 все равно иногда заикается. Возможно, чуть реже, чем OpenSL?
Thread так же переключается между ядрами.
Еще не реализовано:
  * input;
  * перезапуск input при disconnect;
  * выбор устройства (первый в списке - default, система сама решает, куда подключаться);
*/

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include "main/android/sundog_bridge.h"

#if __ANDROID_MIN_SDK_VERSION__ >= 26 //Android 8+
    #define WITH_AAUDIO
#endif

#ifdef WITH_AAUDIO
    #include <aaudio/AAudio.h>
#endif

#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <cstring>

enum
{
#ifdef WITH_AAUDIO
    SDRIVER_AAUDIO,
#endif
    SDRIVER_OPENSLES,
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
#ifdef WITH_AAUDIO
    "AAudio",
#endif
    "OpenSL ES"
};

const char* g_sdriver_names[] =
{
#ifdef WITH_AAUDIO
    "aaudio",
#endif
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

#ifdef WITH_AAUDIO
    AAudioStream* 		aa_stream;
#endif

    SLObjectItf 		sl;
    SLObjectItf 		sl_output_mix;
    SLObjectItf 		sl_player;
    SLObjectItf 		sl_recorder;
    SLAndroidConfigurationItf 	sl_config;
    SLDataFormat_PCM 		sl_pcm;

    bool			set_player_thread_affinity;
    bool			player_thread_affinity;
    cpu_set_t 			cpu_set;

    volatile uint 		input_buffer_wp;
    volatile uint 		input_buffer_rp;
    uint* 			input_buffer;
    uint* 			input_silent_buffer;
    bool 			input_enabled;
    int 			input_fadein;

#ifdef WITH_AAUDIO
    smutex			init_mutex;
#endif
};

static int sl_check_error( SLresult res, const char* fn_name )
{
    if( res != SL_RESULT_SUCCESS )
    {
	slog( "OpenSLES ERROR %d (%s)\n", (int)res, fn_name );
	return 1;
    }
    return 0;
}

static void playback_callback( SLAndroidSimpleBufferQueueItf queueItf, void* context )
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
	    //Если система не предоставляет экслюзивные ядра, то этот вызов просто не работает: поток и дальше продолжает скакать по ядрам :(
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
	//slog( "%s\n", cores );
	LOGI( "%s\n", cores );
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

static void recording_callback( SLAndroidSimpleBufferQueueItf queueItf, void* context )
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

static int device_sound_init_opensles( sundog_sound* ss )
{
    SLresult res = SL_RESULT_SUCCESS;

    device_sound* d = (device_sound*)ss->device_specific;

    smem_free( d->output_buffer );
    d->output_buffer = SMEM_ZALLOC2( uint, d->buffer_size * 2 ); // *2 = double buffered
    ss->in_type = sound_buffer_int16;
    ss->in_channels = ss->out_channels;
    d->output_callback_stop_request = 0;
    d->input_buffer_wp = 0;
    d->input_buffer_rp = 0;
    d->input_enabled = false;

    while( 1 )
    {
	//SLEngineOption EngineOption[] = { (SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE };

	slog( "SLES: slCreateEngine\n" );
	//res = slCreateEngine( &d->sl, 1, EngineOption, 0, NULL, NULL );
	res = slCreateEngine( &d->sl, 0, NULL, 0, NULL, NULL );
	if( sl_check_error( res, "slCreateEngine" ) ) break;

	// Realizing the SL Engine in synchronous mode:
	res = (*d->sl)->Realize( d->sl, SL_BOOLEAN_FALSE );
	if( sl_check_error( res, "Realizing the SL Engine" ) ) break;

	// Get the SL Engine Interface which is implicit:
	SLEngineItf EngineItf;
	res = (*d->sl)->GetInterface( d->sl, SL_IID_ENGINE, (void*)&EngineItf );
	if( sl_check_error( res, "Get the SL Engine Interface which is implicit" ) ) break;

	// Create Output Mix object to be used by player:
	slog( "SLES: Create Output Mix object\n" );
	res = (*EngineItf)->CreateOutputMix( EngineItf, &d->sl_output_mix, 0, 0, 0 );
	if( sl_check_error( res, "Create Output Mix object" ) ) break;

	// Realizing the Output Mix object:
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
	res = (*d->sl_player)->Realize( d->sl_player, SL_BOOLEAN_FALSE );
	if( sl_check_error( res, "Realizing the player" ) ) break;

	// Get config interface:
	res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_ANDROIDCONFIGURATION, (void*)&d->sl_config );
	if( sl_check_error( res, "Get config interface" ) ) break;

	// Get seek and play interfaces:
	SLPlayItf playItf;
	res = (*d->sl_player)->GetInterface( d->sl_player, SL_IID_PLAY, (void*)&playItf );
	if( sl_check_error( res, "Get seek and play interfaces" ) ) break;

	SLAndroidSimpleBufferQueueItf bufferQueueItf;
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

    if( res != SL_RESULT_SUCCESS )
    {
	if( d->sl )
	{
	    (*d->sl)->Destroy( d->sl );
	    d->sl = 0;
	}
	return 1;
    }
    else
    {
	return 0;
    }
}

static void device_sound_deinit_opensles( sundog_sound* ss )
{
    SLresult res;
    device_sound* d = (device_sound*)ss->device_specific;
    if( !d->sl ) return;

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
	d->sl_player = nullptr;
    }

    if( d->sl_output_mix )
    {
	(*d->sl_output_mix)->Destroy( d->sl_output_mix );
	d->sl_output_mix = nullptr;
    }

    (*d->sl)->Destroy( d->sl );
    d->sl = nullptr;
}

static int device_sound_input_opensles( sundog_sound* ss, bool enable )
{
    int rv = 0;
    device_sound* d = (device_sound*)ss->device_specific;

    if( !enable )
    {
    	if( d->sl_recorder )
	{
	    d->input_enabled = false;
	    (*d->sl_recorder)->Destroy( d->sl_recorder );
	    d->sl_recorder = nullptr;
	}
	return 0;
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
	        d->sl_recorder = nullptr;
	    }
	    rv = 1;
	}
    }

    return rv;
}

#ifdef WITH_AAUDIO

static int device_sound_init_aaudio( sundog_sound* ss );
static void device_sound_deinit_aaudio( sundog_sound* ss );

static int aa_check_error( aaudio_result_t res, const char* fn_name )
{
    if( res != AAUDIO_OK )
    {
	slog( "AAudio ERROR %d (%s)\n", (int)res, fn_name );
	return 1;
    }
    return 0;
}

static aaudio_data_callback_result_t aa_playback_callback(
    AAudioStream* stream,
    void* userData,
    void* audioData,
    int32_t numFrames )
{
    sundog_sound* ss = (sundog_sound*)userData;
    device_sound* d = (device_sound*)ss->device_specific;

    ss->out_buffer = audioData;
    ss->out_frames = numFrames;
    ss->out_time = stime_ticks() + ( ( ( ( d->buffer_size << 15 ) / ss->freq ) * stime_ticks_per_second() ) >> 15 );
    sundog_sound_callback( ss, 0 );

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static std::atomic_int g_aa_error_threads_active = 0;
static void* aa_error_thread( void* user_data )
{
    sundog_sound* ss = (sundog_sound*)user_data;
    device_sound* d = (device_sound*)ss->device_specific;

    smutex_lock( &d->init_mutex );
    device_sound_deinit_aaudio( ss );
    device_sound_init_aaudio( ss );
    smutex_unlock( &d->init_mutex );

    g_aa_error_threads_active--;

    return nullptr;
}

static void aa_error_callback( AAudioStream* stream, void* userData, aaudio_result_t error )
{
    sundog_sound* ss = (sundog_sound*)userData;
    device_sound* d = (device_sound*)ss->device_specific;
    if( error == AAUDIO_ERROR_DISCONNECTED )
    {
	slog( "AAUDIO_ERROR_DISCONNECTED\n" );
	pthread_t th;
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	int err = pthread_create( &th, &attr, aa_error_thread, ss );
	if( err == 0 )
	{
	    g_aa_error_threads_active++;
	    pthread_detach( th );
	}
    }
    else
    {
	slog( "AAudio error callback: %d\n", error );
    }
}

static int device_sound_init_aaudio( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    int rv = 1;
    AAudioStreamBuilder* builder = nullptr;

    smutex_lock( &d->init_mutex );

    ss->in_type = sound_buffer_int16;
    ss->in_channels = ss->out_channels;
    d->input_buffer_wp = 0;
    d->input_buffer_rp = 0;
    d->input_enabled = false;

    while( 1 )
    {
	aaudio_result_t res = AAudio_createStreamBuilder( &builder );
	if( aa_check_error( res, "createStreamBuilder" ) ) break;
	if( !builder ) break;

	//AAudioStreamBuilder_setDeviceId( builder, deviceId );
	//AAudioStreamBuilder_setDirection( builder, direction );

	AAudioStreamBuilder_setSharingMode( builder, AAUDIO_SHARING_MODE_SHARED );
	AAudioStreamBuilder_setSampleRate( builder, ss->freq );
	AAudioStreamBuilder_setChannelCount( builder, ss->out_channels );
	aaudio_format_t fmt = AAUDIO_FORMAT_PCM_I16;
	if( ss->out_type == sound_buffer_float32 ) fmt = AAUDIO_FORMAT_PCM_FLOAT;
	AAudioStreamBuilder_setFormat( builder, fmt );
	AAudioStreamBuilder_setBufferCapacityInFrames( builder, d->buffer_size * 2 );
	AAudioStreamBuilder_setPerformanceMode( builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY );
	AAudioStreamBuilder_setDataCallback( builder, aa_playback_callback, ss );
	AAudioStreamBuilder_setErrorCallback( builder, aa_error_callback, ss );

	d->aa_stream = nullptr;
	res = AAudioStreamBuilder_openStream( builder, &d->aa_stream );
	if( aa_check_error( res, "openStream" ) ) break;
	if( !d->aa_stream ) break;

	AAudioStream_setBufferSizeInFrames( d->aa_stream, d->buffer_size );

	uint32_t buf_size = AAudioStream_getBufferSizeInFrames( d->aa_stream );
	uint32_t buf_cap = AAudioStream_getBufferCapacityInFrames( d->aa_stream );
	uint32_t frames_per_burst = AAudioStream_getFramesPerBurst( d->aa_stream );
	slog( "AAudio: buf_size %d; buf_cap %d; burst %d;\n", buf_size, buf_cap, frames_per_burst );
	//default: 768; 1536; 96;
	d->buffer_size = buf_size;

	AAudioStream_requestStart( d->aa_stream );

	rv = 0;
	break;
    }

    if( builder ) AAudioStreamBuilder_delete( builder );

    smutex_unlock( &d->init_mutex );

    return rv;
}

static void device_sound_deinit_aaudio( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    smutex_lock( &d->init_mutex );

    if( d->aa_stream )
    {
	AAudioStream_close( d->aa_stream );
	d->aa_stream = nullptr;
    }

    smutex_unlock( &d->init_mutex );
}

static int device_sound_input_aaudio( sundog_sound* ss, bool enable )
{
    int rv = 0;
    device_sound* d = (device_sound*)ss->device_specific;

    smutex_lock( &d->init_mutex );
    smutex_unlock( &d->init_mutex );

    return rv;
}

#endif //WITH_AAUDIO

int device_sound_init( sundog_sound* ss )
{
    slog( "device_sound_init() begin\n" );

    ss->device_specific = SMEM_ZALLOC( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;

    int android_audio_buf_size = 0; //optimal buffer size
    int android_audio_sample_rate = 0; //optimal sample rate

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

    slog( "SND: %dHz %d; optimal: %dHz %d\n", ss->freq, d->buffer_size, android_audio_sample_rate, android_audio_buf_size );

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
	slog( "SND: exclusive cores:\n" );
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
	slog( "SND: exclusive cores:\n" );
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

#ifdef WITH_AAUDIO
    smutex_init( &d->init_mutex, 0 );
#endif

    bool sdriver_checked[ NUMBER_OF_SDRIVERS ];
    smem_clear( sdriver_checked, sizeof( sdriver_checked ) );
    while( 1 )
    {
	int prev_buf_size = d->buffer_size;
	int sound_err = 0;
	switch( ss->driver )
	{
#ifdef WITH_AAUDIO
	    case SDRIVER_AAUDIO:
		sound_err = device_sound_init_aaudio( ss );
		break;
#endif
	    case SDRIVER_OPENSLES:
		sound_err = device_sound_init_opensles( ss );
		break;
	}
	if( sound_err == 0 ) 
	{
	    //Driver found and initialized:
	    break;
	}
	//Driver init error:
	slog( "Can't open sound stream (driver: %s; error: %d)\n", g_sdriver_names[ ss->driver ], sound_err );
	d->buffer_size = prev_buf_size;
	if( ss->driver < NUMBER_OF_SDRIVERS ) sdriver_checked[ ss->driver ] = true;
	ss->driver = 0;
	for( ss->driver = 0; ss->driver < NUMBER_OF_SDRIVERS; ss->driver++ )
	{
	    if( sdriver_checked[ ss->driver ] == 0 ) 
	    {
		slog( "Switching to %s...\n", g_sdriver_names[ ss->driver ] );
		break;
	    }
	}
	if( ss->driver == NUMBER_OF_SDRIVERS )
	{
	    //No sound driver found:
	    REMOVE_DEVICE_SPECIFIC_DATA( ss );
	    return 1;
	}
    }

    ss->out_latency = d->buffer_size;
    ss->out_latency2 = d->buffer_size;

    slog( "device_sound_init() end\n" );

    return 0;
}

int device_sound_deinit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( !d ) return 1;

    device_sound_input( ss, false );
    switch( ss->driver )
    {
#ifdef WITH_AAUDIO
	case SDRIVER_AAUDIO:
	    device_sound_deinit_aaudio( ss );
	    break;
#endif
	case SDRIVER_OPENSLES:
	    device_sound_deinit_opensles( ss );
	    break;
    }

    smem_free( d->output_buffer );
    d->output_buffer = nullptr;

    smem_free( d->input_buffer );
    d->input_buffer = nullptr;

    smem_free( d->input_silent_buffer );
    d->input_silent_buffer = nullptr;

#ifndef NOMAIN
    android_sundog_set_sustained_performance_mode( ss->sd, 0 );
#endif

#ifdef WITH_AAUDIO
    while( g_aa_error_threads_active != 0 )
    {
	stime_sleep( 10 );
    }
    smutex_destroy( &d->init_mutex );
#endif

    REMOVE_DEVICE_SPECIFIC_DATA( ss );

    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( !d ) return;
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

        int err = 0;
        switch( ss->driver )
        {
#ifdef WITH_AAUDIO
	    case SDRIVER_AAUDIO:
		err = device_sound_input_aaudio( ss, true );
		break;
#endif
	    case SDRIVER_OPENSLES:
		err = device_sound_input_opensles( ss, true );
		break;
        }
        if( err == 0 )
        {
            d->input_enabled = true;
        }
        else
        {
	    slog( "Can't open sound input (driver: %s; error: %d)\n", g_sdriver_names[ ss->driver ], err );
        }
    }
    else
    {
        switch( ss->driver )
        {
#ifdef WITH_AAUDIO
	    case SDRIVER_AAUDIO:
		device_sound_input_aaudio( ss, false );
		break;
#endif
	    case SDRIVER_OPENSLES:
		device_sound_input_opensles( ss, false );
		break;
        }
        d->input_enabled = false;
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    return 0;
}
