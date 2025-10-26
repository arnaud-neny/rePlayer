//
// macOS: sound
//

#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioComponent.h>

enum
{
    SDRIVER_AUDIOUNIT,
#ifdef JACK_AUDIO
    SDRIVER_JACK,
#endif
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
    "Audio Unit",
#ifdef JACK_AUDIO
    "JACK",
#endif
};

const char* g_sdriver_names[] =
{
    "audiounit",
#ifdef JACK_AUDIO
    "jack",
#endif
};

const int g_default_buffer_size = 4096;

#define kOutputBus 0
#define kInputBus 1

struct device_sound //device-specific data of the sundog_sound object
{
    AudioUnit			au; //Audio Unit
    AudioUnit			input_au;
    AudioBufferList 		input_buffer_list;
#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
};

static OSStatus output_callback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData )
{
    stime_ticks_t cur_t = stime_ticks();
    sundog_sound* ss = (sundog_sound*)inRefCon;
    if( ioData->mNumberBuffers >= 1 )
    {
	AudioBuffer* buf = &ioData->mBuffers[ 0 ];
	ss->out_buffer = buf->mData;
	ss->out_frames = buf->mDataByteSize / ( 4 * ss->out_channels );
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

            //Additional tests are required for this code? (unstable out_latency2)
	    ss->out_latency2 = (uint64_t)( ss->out_time - cur_t ) * stime_ticks_per_second() / ss->freq;
	    if( ss->out_latency2 < ss->out_latency || ss->out_latency2 > ss->freq )
		ss->out_latency2 = ss->out_latency;
	}
	else
	{
	    ss->out_time = cur_t + ( ( (uint64_t)ss->out_frames * stime_ticks_per_second() ) / ss->freq );
	    //out_time may be incorrect if the system splits the render of a large block into several short fragments!
	}
        get_input_data( ss, ss->out_frames );
        if( sundog_sound_callback( ss, 0 ) == 0 )
        {
            *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        }
        if( ss->out_type == sound_buffer_int16 )
        {
            float* fb = (float*)ss->out_buffer;
            int16_t* sb = (int16_t*)ss->out_buffer;
            for( int i = ss->out_frames * ss->out_channels - 1; i >= 0; i-- )
                SMP_INT16_TO_FLOAT32( fb[ i ], sb[ i ] );
        }
    }
    return noErr;
}

static OSStatus input_callback(
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
    d->input_buffer_list.mNumberBuffers = 1;
    d->input_buffer_list.mBuffers[ 0 ].mNumberChannels = ss->in_channels;
    d->input_buffer_list.mBuffers[ 0 ].mDataByteSize = inNumberFrames * 4 * ss->in_channels;
    OSStatus err = AudioUnitRender(
        d->input_au,
        ioActionFlags,
        inTimeStamp,
        inBusNumber,
        inNumberFrames,
        &d->input_buffer_list );
    if( err == noErr )
    {
        int size = inNumberFrames;
        int8_t* ptr = (int8_t*)d->input_buffer_list.mBuffers[ 0 ].mData;
        if( size == 0 ) return noErr;
        int au_frame_size = 4 * ss->in_channels;
        int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
        while( size > 0 )
        {
            int size2 = size;
            if( d->input_buffer_wp + size2 > d->input_buffer_size )
                size2 = d->input_buffer_size - d->input_buffer_wp;
            int8_t* buf_ptr = (int8_t*)d->input_buffer + d->input_buffer_wp * frame_size;
            if( ss->in_type == sound_buffer_int16 )
            {
                int16_t* sb = (int16_t*)buf_ptr;
                float* fb = (float*)ptr;
                for( int i = 0; i < size2 * ss->in_channels; i++ )
                    SMP_FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
            }
            if( ss->in_type == sound_buffer_float32 )
            {
                smem_copy( buf_ptr, ptr, size2 * frame_size );
            }
            volatile uint new_wp = ( d->input_buffer_wp + size2 ) & ( d->input_buffer_size - 1 );
            d->input_buffer_wp = new_wp;
            size -= size2;
            ptr += size2 * au_frame_size;
        }
    }
    return noErr;
}

int device_sound_init( sundog_sound* ss )
{
    OSStatus status;
    UInt32 enableIO;
    int rv = 0;

    if( ss->device_specific ) return -1; //Already open

    ss->device_specific = SMEM_ZALLOC( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;

    if( ss->out_type == sound_buffer_default ) ss->out_type = sound_buffer_float32;
    if( ss->in_type == sound_buffer_default ) ss->in_type = sound_buffer_float32;
    sundog_sound_set_defaults( ss );

    ss->out_latency = sconfig_get_int_value( APP_CFG_SND_BUF, g_default_buffer_size, 0 );
    ss->out_latency2 = ss->out_latency;

    while( 1 )
    {
        //Describe audio component:
        AudioComponentDescription desc;
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        //Get component:
        AudioComponent comp = AudioComponentFindNext( NULL, &desc );
        if( comp == 0 )
        {
            slog( "AUDIO UNIT ERROR: AudioComponent not found\n" );
            rv = -1;
            break;
        }

        //Get audio unit:
        status = AudioComponentInstanceNew( comp, &d->au );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: OpenAComponent failed with code %d\n", (int)status );
            rv = -2;
            break;
        }

        //Disable input:
        enableIO = 0;
        status = AudioUnitSetProperty(
            d->au,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input,
            kInputBus,
            &enableIO,
            sizeof( enableIO ) );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (disable input) failed with code %d\n", (int)status );
        }

        //Set output device:
        AudioDeviceID outputDevice = 0;
        UInt32 size = sizeof( AudioDeviceID );
        AudioObjectPropertyAddress property_address =
        {
            kAudioHardwarePropertyDefaultOutputDevice,  // mSelector
            kAudioObjectPropertyScopeGlobal,           // mScope
            kAudioObjectPropertyElementMaster          // mElement
        };
        status = AudioObjectGetPropertyData( kAudioObjectSystemObject, &property_address, 0, 0, &size, &outputDevice );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioHardwareGetProperty failed with code %d\n", (int)status );
            rv = -3;
            break;
        }
        status = AudioUnitSetProperty(
            d->au,
            kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global,
            kOutputBus,
            &outputDevice,
            sizeof( outputDevice ) );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (set output device) failed with code %d\n", (int)status );
            rv = -4;
            break;
        }

        //Describe output format:
        AudioStreamBasicDescription af;
        af.mSampleRate = ss->freq;
        af.mFormatID = kAudioFormatLinearPCM;
        af.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        af.mFramesPerPacket = 1;
        af.mChannelsPerFrame = ss->out_channels;
        af.mBitsPerChannel = 32;
        af.mBytesPerPacket = 4 * ss->out_channels;
        af.mBytesPerFrame = 4 * ss->out_channels;

        //Apply format:
        status = AudioUnitSetProperty(
            d->au,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            kOutputBus,
            &af,
            sizeof( af ) );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (StreamFormat for output) failed with code %d\n", (int)status );
            rv = -5;
            break;
        }
    
        //Set output callback:
        AURenderCallbackStruct cs;
        cs.inputProc = output_callback;
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
            slog( "AUDIO UNIT ERROR: AudioUnitSetProperty (SetRenderCallback,Global) failed with code %d\n", (int)status );
            rv = -6;
            break;
        }
    
        //Initialize:
        status = AudioUnitInitialize( d->au );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioUnitInitialize failed with code %d\n", (int)status );
            rv = -7;
            break;
        }
    
        //Start:
        status = AudioOutputUnitStart( d->au );
        if( status != noErr )
        {
            slog( "AUDIO UNIT ERROR: AudioOutputUnitStart failed with code %d\n", (int)status );
            rv = -8;
            break;
        }

        break;
    }
    
    if( rv )
    {
        if( d->au )
        {
            AudioUnitUninitialize( d->au );
            AudioComponentInstanceDispose( d->au );
            d->au = 0;
        }
        REMOVE_DEVICE_SPECIFIC_DATA( ss );
    }
    
    return rv;
}

int device_sound_deinit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    device_sound_input( ss, false );
    AudioUnitUninitialize( d->au );
    AudioComponentInstanceDispose( d->au );
    d->au = 0;
    remove_input_buffers( ss );

    REMOVE_DEVICE_SPECIFIC_DATA( ss );

    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    OSStatus status;
    UInt32 enableIO;
    bool err = 0;
    device_sound* d = (device_sound*)ss->device_specific;
    
    if( enable )
    {
        while( 1 )
        {
	    if( d->input_enabled ) break;
            if( d->input_au ) break; //Already open
         
            set_input_defaults( ss );
            create_input_buffers( ss, 4096 );
            if( d->input_buffer_list.mBuffers[ 0 ].mData == NULL )
            {
        	SMEM_CLEAR_STRUCT( d->input_buffer_list );
                d->input_buffer_list.mNumberBuffers = 1;
                d->input_buffer_list.mBuffers[ 0 ].mNumberChannels = 2;
                d->input_buffer_list.mBuffers[ 0 ].mDataByteSize = 8192 * 4 * 2;
                d->input_buffer_list.mBuffers[ 0 ].mData = SMEM_ZALLOC( 8192 * 4 * 2 );
            }
            
            //Describe audio component:
            AudioComponentDescription desc;
            desc.componentType = kAudioUnitType_Output;
            desc.componentSubType = kAudioUnitSubType_HALOutput;
            desc.componentFlags = 0;
            desc.componentFlagsMask = 0;
            desc.componentManufacturer = kAudioUnitManufacturer_Apple;
            
            //Get component:
            AudioComponent comp = AudioComponentFindNext( NULL, &desc );
            if( comp == 0 )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioComponent not found\n" );
                err = 1;
                break;
            }
            
            //Get audio unit:
            status = AudioComponentInstanceNew( comp, &d->input_au );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: OpenAComponent failed with code %d\n", (int)status );
                err = 2;
                break;
            }
            
            //Enable input:
            enableIO = 1;
            status = AudioUnitSetProperty(
                d->input_au,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Input,
                kInputBus,
                &enableIO,
                sizeof( enableIO ) );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitSetProperty (enable input) failed with code %d\n", (int)status );
                err = 3;
                break;
            }
            
            //Disable output:
            enableIO = 0;
            status = AudioUnitSetProperty(
                d->input_au,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output,
                kOutputBus,
                &enableIO,
                sizeof( enableIO ) );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitSetProperty (disable output) failed with code %d\n", (int)status );
                err = 4;
                break;
            }
            
            //Get input device:
            AudioDeviceID inputDevice = 0;
            UInt32 size = sizeof( AudioDeviceID );
            AudioObjectPropertyAddress property_address =
            {
                kAudioHardwarePropertyDefaultInputDevice,  // mSelector
                kAudioObjectPropertyScopeGlobal,           // mScope
                kAudioObjectPropertyElementMaster          // mElement
            };
            status = AudioObjectGetPropertyData( kAudioObjectSystemObject, &property_address, 0, 0, &size, &inputDevice );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioObjectGetPropertyData failed with code %d\n", (int)status );
                err = 5;
                break;
            }
	    //Get available freqs:
	    property_address.mSelector = kAudioDevicePropertyAvailableNominalSampleRates;
	    status = AudioObjectGetPropertyDataSize( inputDevice, &property_address, 0, 0, &size );
	    if( status != noErr )
	    {
		slog( "AUDIO UNIT INPUT ERROR: AudioObjectGetPropertyDataSize failed with code %d\n", (int)status );
		err = 6;
		break;
	    }
	    int val_count = size / (int)sizeof( AudioValueRange );
	    slog( "AUDIO UNIT INPUT: Available %d Sample Rates\n", val_count );
	    AudioValueRange* freqs = (AudioValueRange*)SMEM_ALLOC( size );
	    status = AudioObjectGetPropertyData( inputDevice, &property_address, 0, 0, &size, freqs );
	    if( status != noErr )
	    {
		slog( "AUDIO UNIT INPUT ERROR: AudioObjectGetPropertyData (2) failed with code %d\n", (int)status );
		err = 7;
		break;
	    }
	    for( int i = 0; i < val_count; i++ )
	    {
		slog( "%d\n", (int)freqs[ i ].mMinimum );
	    }
	    smem_free( freqs );
	    //Set device sample rate:
	    AudioValueRange srate;
	    srate.mMinimum = ss->freq;
	    srate.mMaximum = ss->freq;
	    property_address.mSelector = kAudioDevicePropertyNominalSampleRate;
	    AudioObjectSetPropertyData( inputDevice, &property_address, 0, 0, sizeof( srate ), &srate );
	    property_address.mSelector = kAudioDevicePropertyLatency;
	    //Set input device:
            status = AudioUnitSetProperty(
                d->input_au,
                kAudioOutputUnitProperty_CurrentDevice,
                kAudioUnitScope_Global,
                kInputBus,
                &inputDevice,
                sizeof( inputDevice ) );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitSetProperty (set input device) failed with code %d\n", (int)status );
                err = 8;
                break;
            }
            
            //Describe input format:
            AudioStreamBasicDescription af;
            af.mSampleRate = ss->freq;
            af.mFormatID = kAudioFormatLinearPCM;
            af.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
            af.mFramesPerPacket = 1;
            af.mChannelsPerFrame = ss->in_channels;
            af.mBitsPerChannel = 32;
            af.mBytesPerPacket = 4 * ss->in_channels;
            af.mBytesPerFrame = 4 * ss->in_channels;
            
            //Apply input format:
            status = AudioUnitSetProperty(
                d->input_au,
                kAudioUnitProperty_StreamFormat,
                kAudioUnitScope_Output,
                kInputBus,
                &af,
                sizeof( af ) );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitSetProperty (StreamFormat) failed with code %d\n", (int)status );
                err = 9;
                break;
            }
            
            //Set input callback:
            AURenderCallbackStruct cs;
            cs.inputProc = input_callback;
            cs.inputProcRefCon = ss;
            status = AudioUnitSetProperty(
                d->input_au,
                kAudioOutputUnitProperty_SetInputCallback,
                kAudioUnitScope_Global,
                kInputBus,
                &cs,
                sizeof( cs ) );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitSetProperty (SetInputCallback) failed with code %d\n", (int)status );
                err = 10;
                break;
            }
            
            //Initialize:
            status = AudioUnitInitialize( d->input_au );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioUnitInitialize failed with code %d\n", (int)status );
                err = 11;
                break;
            }
            
            //Start:
            status = AudioOutputUnitStart( d->input_au );
            if( status != noErr )
            {
                slog( "AUDIO UNIT INPUT ERROR: AudioOutputUnitStart failed with code %d\n", (int)status );
                err = 12;
                break;
            }
	    
	    d->input_enabled = true;
            
            break;
        }
        if( err )
        {
            if( d->input_au )
            {
                AudioUnitUninitialize( d->input_au );
                AudioComponentInstanceDispose( d->input_au );
                d->input_au = 0;
            }
        }
    }
    else
    {
	if( d->input_enabled == false ) return;
        if( d->input_au == 0 ) return;
        AudioUnitUninitialize( d->input_au );
        AudioComponentInstanceDispose( d->input_au );
        d->input_au = 0;
        if( d->input_buffer_list.mBuffers[ 0 ].mData )
        {
            smem_free( d->input_buffer_list.mBuffers[ 0 ].mData );
            d->input_buffer_list.mBuffers[ 0 ].mData = 0;
        }
	d->input_enabled = true;
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    return 0;
}
