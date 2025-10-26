//
// iOS: sound
//

// Audio Unit + Audiobus:
//   app init (outside of SunDog): g_au = new;
//   SunDog app: init/reinit/deinit g_au;
//     + we can create new Audio Units (not linked to Audiobus)
//   app deinit (outside of SunDog): remove g_au; g_au = 0;

// Audio Unit:
//   any number of Audio Units per app;

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreMIDI/CoreMIDI.h>

int audio_session_init( bool record, int preferred_sample_rate, int preferred_buffer_size );
int audio_session_deinit();
int get_audio_session_freq();
#ifdef AUDIOBUS
    int audiobus_init( void* observer ); //Called from sundog_bridge.m (main thread)
    void audiobus_deinit(); //Called from sundog_bridge.m (main thread)
    void audiobus_enable_ports( bool enable );
    void audiobus_connection_state_update( sundog_engine* sd );
    void audiobus_triggers_update();
    #ifdef AUDIOBUS_MIDI_IN
	struct device_midi_port;
	typedef void (*osx_midiin_callback_t)( device_midi_port* port, const MIDIPacketList* packetList );
        int audiobus_add_midiin_callback( osx_midiin_callback_t f, device_midi_port* port );
	bool audiobus_remove_midiin_callback( osx_midiin_callback_t f, device_midi_port* port );
        extern volatile bool g_ab_disable_coremidi_in;
    #endif
    #ifdef AUDIOBUS_MIDI_OUT
        void audiobus_midiout( int out_port, const MIDIPacketList* plist ); //out_port: 1,2,3...; 0 - none
	extern const char* g_ab_midi_out_names2[];
        extern volatile bool g_ab_disable_coremidi_out;
    #endif
    extern bool g_ab_connected;
    extern void* kAudiobusConnectedChanged;
#endif

#ifdef IOS_SOUND_CODE

enum
{
#ifdef JACK_AUDIO
    SDRIVER_JACK,
#endif
    SDRIVER_AUDIOUNIT,
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
#ifdef JACK_AUDIO
    "JACK",
#endif
#ifdef AUDIOBUS
    "Audio Unit / Audiobus",
#else
    "Audio Unit",
#endif
};

const char* g_sdriver_names[] =
{
#ifdef JACK_AUDIO
    "JACK",
#endif
    "audiounit",
};

struct device_sound //device-specific data of the sundog_sound object
{
    AudioComponentInstance 	au; //Audio Unit
    volatile int 		au_pause_request;
    void* 			input_buf_lowlevel;
    uint* 			input_buf;
    void* 			input_buf2;
    volatile uint 		input_buf_wp;
    uint 			input_buf_rp;
#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
#ifdef COMMON_JACK_VARIABLES
    COMMON_JACK_VARIABLES;
#endif
};

//
// Audio Unit / Audiobus
//

#define kOutputBus 0
#define kInputBus 1

#define INPUT_BUF_FRAMES ( 64 * 1024 )

extern bool g_no_audiobus;
extern AudioComponentInstance g_au; //Audio Unit for Audiobus (sound_ios.mm)
extern bool g_au_inuse;

OSStatus au_playback_callback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData )
{
    stime_ticks_t cur_t = stime_ticks();
    sundog_sound* ss = (sundog_sound*)inRefCon;
    device_sound* d = (device_sound*)ss->device_specific;

    if( ioData->mNumberBuffers >= 1 )
    {
	AudioBuffer* buf = &ioData->mBuffers[ 0 ];
	ss->out_buffer = buf->mData;
	ss->out_frames = buf->mDataByteSize / ( 2 * ss->out_channels );
	if( ss->out_frames >= 16 && ss->out_frames <= 8192 )
	{
	    ss->out_latency = ss->out_frames;
	    ss->out_latency2 = ss->out_frames;
	}
	//out_latency and out_latency2 may be incorrect if the system splits the render of a large block into several short fragments!
	//Ideally, we need some system API to get the actual audio latency.
	if( inTimeStamp->mFlags & kAudioTimeStampHostTimeValid )
	{
	    //For correct MIDI IN evt processing:

	    ss->out_time = convert_mach_absolute_time_to_sundog_ticks( inTimeStamp->mHostTime, 1 );
	    //mHostTime represents the time at which the first output sample in the buffer will be consumed by the hardware (before the driver):
	    //https://www.mail-archive.com/coreaudio-api@lists.apple.com/msg01234.html
	    //It's true for pure stand-alone apps.
	    //But sometimes mHostTime gives some strange values. For example, it's too large (x2/x3) in Audiobus...

	    //Additional tests are required for this code? (unstable out_latency2)
#ifdef AUDIOBUS
	    //if( !g_ab_connected )
#endif
	    {
            	ss->out_latency2 = (uint64_t)( ss->out_time - cur_t ) * stime_ticks_per_second() / ss->freq;
            	if( ss->out_latency2 < ss->out_latency || ss->out_latency2 > ss->freq )
		    ss->out_latency2 = ss->out_latency;
	    }
	}
	else
	{
	    ss->out_time = cur_t + ( ( (uint64_t)ss->out_frames * stime_ticks_per_second() ) / ss->freq );
	    //out_time may be incorrect if the system splits the render of a large block into several short fragments!
	}
        ss->in_buffer = d->input_buf2;
        if( d->input_buf2 && ( ( d->input_buf_wp - d->input_buf_rp ) & ( INPUT_BUF_FRAMES - 1 ) ) >= ss->out_frames )
        {
            for( int i = 0; i < ss->out_frames; i++ )
            {
                ((uint*)d->input_buf2)[ i ] = d->input_buf[ d->input_buf_rp ];
                d->input_buf_rp++;
                d->input_buf_rp &= ( INPUT_BUF_FRAMES - 1 );
            }
        }

	if( d->au_pause_request > 0 )
	{
	    d->au_pause_request = 2;
	    memset( ss->out_buffer, 0, buf->mDataByteSize );
            if( ioActionFlags ) *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
	}
	else
	{
	    if( sundog_sound_callback( ss, 0 ) == 0 )
            {
		//Silence:
                if( ioActionFlags ) *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
            }
	}
    }
    
    return noErr;
}

OSStatus au_recording_callback(
    void* inRefCon, 
    AudioUnitRenderActionFlags* ioActionFlags, 
    const AudioTimeStamp* inTimeStamp, 
    UInt32 inBusNumber, 
    UInt32 inNumberFrames, 
    AudioBufferList* ioData ) 
{
    sundog_sound* ss = (sundog_sound*)inRefCon;
    device_sound* d = (device_sound*)ss->device_specific;
    
    if( inNumberFrames > 8192 ) return noErr;
    
    AudioBufferList bufs;
    SMEM_CLEAR_STRUCT( bufs );
    bufs.mNumberBuffers = 1;
    AudioBuffer* buf = &bufs.mBuffers[ 0 ];
    buf->mNumberChannels = ss->in_channels;
    buf->mDataByteSize = inNumberFrames * ( 2 * ss->in_channels );
    buf->mData = d->input_buf_lowlevel;
    
    //Obtain recorded samples:
    AudioUnitRender(
        d->au, 
        ioActionFlags, 
        inTimeStamp, 
        inBusNumber, 
        inNumberFrames, 
        &bufs );
    
    volatile uint wp = d->input_buf_wp;
    for( int i = 0; i < inNumberFrames; i++ )
    {
	d->input_buf[ wp ] = ((uint*)buf->mData)[ i ];
	wp++;
	wp &= ( INPUT_BUF_FRAMES - 1 );
    }
    d->input_buf_wp = wp;

    return noErr;
}

int au_new( sundog_sound* ss, bool with_input, int sample_rate, int channels )
{
    OSStatus status;
    bool input = false;
    device_sound* d = (device_sound*)ss->device_specific;
    
    size_t size;
    size = 8192 * 4; d->input_buf_lowlevel = malloc( size ); memset( d->input_buf_lowlevel, 0, size );
    size = INPUT_BUF_FRAMES * 4; d->input_buf = (uint*)malloc( size ); memset( d->input_buf, 0, size );
    size = 8192 * 4; d->input_buf2 = malloc( size ); memset( d->input_buf2, 0, size );
    d->input_buf_wp = 0;
    d->input_buf_rp = 0;

    if( g_au && g_au_inuse == false )
    {
	//Audio Unit for Audiobus is available - we must use it:
	d->au = g_au;
	g_au_inuse = true;
    }
    if( d->au == 0 )
    {
	//Create new RemoteIO Audio Unit (not for Audiobus!):
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
	desc.componentFlags = 0;
        desc.componentFlagsMask = 0;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        //Get component:
        AudioComponent comp = AudioComponentFindNext( NULL, &desc );
        if( comp == 0 )
        {
            slog( "AUDIO UNIT ERROR: AudioComponent not found\n" );
            return -2;
        }
        //Get audio unit:
        status = AudioComponentInstanceNew( comp, &d->au );
        if( status != noErr )
	{
    	    slog( "AUDIO UNIT ERROR: AudioComponentInstanceNew failed with code %d\n", status );
	    d->au = 0;
            return -3;
        }
    }

    //Enable IO for playback:
    UInt32 val = 1;
    status = AudioUnitSetProperty(
	d->au,
	kAudioOutputUnitProperty_EnableIO,
	kAudioUnitScope_Output,
	kOutputBus,
	&val,
	sizeof( val ) );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (output) failed with code %d\n", status );
	if( status == kAudioUnitErr_Initialized )
	{
	    slog( "AU is already initialized\n" ); //but not by the SunDog... Maybe by IAA host?
#ifdef AUDIOBUS
	    audiobus_enable_ports( 0 );
#endif
	    AudioOutputUnitStop( d->au );
	    AudioUnitUninitialize( d->au );
	    status = AudioUnitSetProperty(
		d->au,
		kAudioOutputUnitProperty_EnableIO,
		kAudioUnitScope_Output,
		kOutputBus,
		&val,
		sizeof( val ) );
	    if( status != noErr )
	    {
		slog( "failed\n" );
		return -4;
	    }
	}
	else
	    return -4;
    }

    //Enable IO for recording:
    if( with_input )
    {
	val = 1;
	input = 1;
    }
    else
    {
	val = 0;
	input = 0;
    }
    status = AudioUnitSetProperty(
	d->au,
	kAudioOutputUnitProperty_EnableIO,
	kAudioUnitScope_Input,
	kInputBus,
	&val,
	sizeof( val ) );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (input) failed with code %d\n", status );
	input = 0;
    }
    
    //Describe audio stream format:
    AudioStreamBasicDescription af;
    af.mSampleRate = sample_rate;
    af.mFormatID = kAudioFormatLinearPCM;
    af.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    af.mFramesPerPacket = 1;
    af.mChannelsPerFrame = channels;
    af.mBitsPerChannel = 16;
    af.mBytesPerPacket = 2 * channels;
    af.mBytesPerFrame = 2 * channels;
    
    //Set output stream format:
    status = AudioUnitSetProperty(
	d->au,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        kOutputBus,
        &af,
        sizeof( af ) );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (output,streamformat) failed with code %d\n", status );
	return -5;
    }
    
    //Set input stream format:
    if( input )
    {
	AudioStreamBasicDescription input_af;
	UInt32 input_af_size = sizeof( input_af );
	status = AudioUnitGetProperty(
	    d->au,
	    kAudioUnitProperty_StreamFormat,
	    kAudioUnitScope_Output,
	    kInputBus,
	    &input_af,
	    &input_af_size );
	if( status != noErr )
	{
	    slog( "AUDIO UNIT ERROR: AudioUnitGetProperty (input,streamformat) failed with code %d\n", status );
	}
	else
	{
	    slog( "AUDIO INPUT Default Sample Rate: %d\n", (int)input_af.mSampleRate );
	    slog( "AUDIO INPUT Channels: %d\n", (int)input_af.mChannelsPerFrame );
	    slog( "AUDIO INPUT Bits Per Channel: %d\n", (int)input_af.mBitsPerChannel );
	}
	status = AudioUnitSetProperty(
	    d->au,
	    kAudioUnitProperty_StreamFormat,
	    kAudioUnitScope_Output,
	    kInputBus,
	    &af,
	    sizeof( af ) );
	if( status != noErr )
	{
	    slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (input,streamformat) failed with code %d\n", status );
	    input = 0;
	}
    }
    
    //Set output callback:
    AURenderCallbackStruct cs;
    cs.inputProc = au_playback_callback;
    cs.inputProcRefCon = ss;
    status = AudioUnitSetProperty(
	d->au,
	kAudioUnitProperty_SetRenderCallback,
	kAudioUnitScope_Global,
	kOutputBus,
	&cs,
	sizeof( cs ) );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (output,setrendercallback) failed with code %d\n", status );
	return -6;
    }
    
    //Set input callback:
    if( input )
    {
	AURenderCallbackStruct cs;
	cs.inputProc = au_recording_callback;
	cs.inputProcRefCon = ss;
	status = AudioUnitSetProperty(
	    d->au,
	    kAudioOutputUnitProperty_SetInputCallback,
	    kAudioUnitScope_Global,
	    kInputBus,
	    &cs,
	    sizeof( cs ) );
	if( status != noErr )
	{
	    slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (input,setinputcallback) failed with code %d\n", status );
	    input = 0;
	}
    }
    
    //Initialize:
    status = AudioUnitInitialize( d->au );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioUnitInitialize failed with code %d\n", status );
	return -7;
    }
    
    d->au_pause_request = 0;
    
    status = AudioOutputUnitStart( d->au );
    if( status != noErr )
    {
	slog( "AUDIO UNIT ERROR: AudioOutputUnitStart failed with code %d\n", status );
	return -8;
    }

    HANDLE_THREAD_EVENTS;

#ifdef AUDIOBUS
    audiobus_enable_ports( 1 );
#endif

    return 0;
}

void au_remove( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( d->au == 0 ) return;

    d->au_pause_request = 1;
    int t = 0;
    int step = 5;
    int timeout = SUNDOG_SOUND_DEFAULT_TIMEOUT_MS;
    while( d->au_pause_request == 1 )
    {
        stime_sleep( step );
        HANDLE_THREAD_EVENTS;
	t += step;
	if( t >= timeout ) break;
    }

    slog( "AUDIO UNIT: deinit...\n" );
#ifdef AUDIOBUS
    audiobus_enable_ports( 0 );
#endif
    AudioOutputUnitStop( d->au );
    HANDLE_THREAD_EVENTS;
    AudioUnitUninitialize( d->au );
    HANDLE_THREAD_EVENTS;
#ifdef AUDIOBUS
    if( g_no_audiobus == false && d->au == g_au )
    {
	//Audio Unit created for Audiobus; just mark it as unused:
	g_au_inuse = false;
    }
    else
    {
	//Close Audio Unit:
	AudioComponentInstanceDispose( d->au );
	HANDLE_THREAD_EVENTS;
    }
#else
    //Close Audio Unit:
    AudioComponentInstanceDispose( d->au );
    HANDLE_THREAD_EVENTS;
#endif
    d->au = 0;

    free( d->input_buf_lowlevel );
    free( d->input_buf );
    free( d->input_buf2 );
    d->input_buf_lowlevel = 0;
    d->input_buf = 0;
    d->input_buf2 = 0;

    HANDLE_THREAD_EVENTS;
}

int device_sound_init_audiounit( sundog_sound* ss )
{
    return au_new( ss, ss->in_enabled != 0, ss->freq, ss->out_channels );
}

void device_sound_deinit_audiounit( sundog_sound* ss )
{
    au_remove( ss );
}

int device_sound_reinit_audiounit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( d->au == 0 ) return -1;
    OSStatus status;
    status = AudioOutputUnitStart( d->au );
    if( status != noErr )
    {
        slog( "AUDIO UNIT ERROR (restart): AudioOutputUnitStart failed with code %d\n", status );
        return -1;
    }
    return 0;
}

int device_sound_init( sundog_sound* ss )
{
    sundog_sound_set_defaults( ss );

    audio_session_init( ss->in_enabled != 0, ss->freq, sconfig_get_int_value( APP_CFG_SND_BUF, 0, 0 ) ); //init or reinit

    if( ss->device_specific )
    {
        //Audio device is already initialized. We just need to restart it:
	switch( ss->driver )
	{
	    case SDRIVER_AUDIOUNIT:
	        device_sound_reinit_audiounit( ss );
	        break;
	    default:
		break;
	}
	return 0;
    }

    ss->device_specific = SMEM_ZALLOC( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;

    ss->in_type = sound_buffer_int16;
    ss->in_channels = ss->out_channels;
    ss->freq = get_audio_session_freq();

    bool sdriver_checked[ NUMBER_OF_SDRIVERS ];
    smem_clear( sdriver_checked, sizeof( sdriver_checked ) );

    while( 1 )
    {
	bool sdriver_ok = false;
	switch( ss->driver )
	{
#ifdef JACK_AUDIO
	    case SDRIVER_JACK:
	        if( device_sound_init_jack( ss ) == 0 ) sdriver_ok = true;
	        break;
#endif
	    case SDRIVER_AUDIOUNIT:
	        if( device_sound_init_audiounit( ss ) == 0 ) sdriver_ok = true;
	        break;
	    default:
	        break;
	}
	if( sdriver_ok ) break;
	if( ss->driver < NUMBER_OF_SDRIVERS ) sdriver_checked[ ss->driver ] = true;
	ss->driver = 0;
	for( ss->driver = 0; ss->driver < NUMBER_OF_SDRIVERS; ss->driver++ )
	{
	    if( sdriver_checked[ ss->driver ] == false )
	    {
		slog( "Switching to %s\n", g_sdriver_names[ ss->driver ] );
		break;
	    }
	}
	if( ss->driver == NUMBER_OF_SDRIVERS )
	{
	    //Sound driver not found:
	    REMOVE_DEVICE_SPECIFIC_DATA( ss );
	    return 1;
	}
    }

    return 0;
}

int device_sound_deinit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    switch( ss->driver )
    {
#ifdef JACK_AUDIO
        case SDRIVER_JACK:
            {
                device_sound_deinit_jack( ss );
            }
            break;
#endif
        case SDRIVER_AUDIOUNIT:
            {
        	device_sound_deinit_audiounit( ss );
            }
            break;
    }

    slog( "Disabling audio session...\n" );
    if( g_no_audiobus )
    {
	if( g_sundog_sound_cnt == 1 ) //current stream is the last inside of the app, so we can deinit the system audio session
	{
	    audio_session_deinit();
	    //When Audiobus enabled, audio session will be closed in the main thread (in audiobus_deinit())
	}
    }

    REMOVE_DEVICE_SPECIFIC_DATA( ss );
    HANDLE_THREAD_EVENTS;

    slog( "device_sound_deinit() ok\n" );
    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    switch( ss->driver )
    {
        case SDRIVER_AUDIOUNIT:
            device_sound_deinit( ss );
            device_sound_init( ss );
            break;
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    return 0;
}

#endif //...IOS_SOUND_CODE
