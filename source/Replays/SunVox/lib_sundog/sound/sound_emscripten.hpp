//
// Emscripten: sound
//

enum
{
#ifdef SDL
    SDRIVER_SDL,
#endif
#ifndef NOWEBAUDIO
    SDRIVER_WEBAUDIO,
#endif
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
#ifdef SDL
    "SDL"
#endif
#ifndef NOWEBAUDIO
    "Web Audio",
#endif
};

const char* g_sdriver_names[] =
{
#ifdef SDL
    "sdl"
#endif
#ifndef NOWEBAUDIO
    "webaudio",
#endif
};

//SDL:
#ifdef SDL
    #include "SDL/SDL_mixer.h"
#endif

//#define USE_RESAMPLER 1 //1 - worst

const int g_default_buffer_size = 2048;

struct device_sound //device-specific data of the sundog_sound object
{
    int				buffer_size;
    void* 			output_buffer;
#ifdef USE_RESAMPLER
    int				real_sample_rate;
#endif
#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
#ifdef COMMON_JACK_VARIABLES
    COMMON_JACK_VARIABLES;
#endif
};

//
// Web Audio
//

#ifndef NOWEBAUDIO

extern "C" void* webaudio_callback( sundog_sound* ss, int len, float latency_sec ) //len in frames
{
    device_sound* d = (device_sound*)ss->device_specific;
    
    /*stime_ticks_t t2 = stime_ticks();
    float t3 = (float)( t2 - t1 ) / (float)stime_ticks_per_second();
    float perc = t3 / ( (float)frames / s->net->sampling_freq ) * 100.0F;
    printf( "%d: %f, %f%%\n", frames, t3, perc );*/
    
    ss->out_buffer = d->output_buffer;
    ss->out_frames = len;
    //ss->out_time = stime_ticks() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq );
    ss->out_time = stime_ticks() + latency_sec * stime_ticks_per_second();
#if USE_RESAMPLER == 1
    float resample_coef = 1;
    if( ss->freq != d->real_sample_rate )
    {
	resample_coef = (float)ss->freq / d->real_sample_rate;
	ss->out_frames = ss->out_frames * resample_coef;
	if( ss->out_frames < 1 ) ss->out_frames = 1;
    }
#endif
    sundog_sound_callback( ss, 0 );
    
    if( ss->out_type == sound_buffer_float32 && ss->out_channels == 2 )
    {
	//Split data
	// from LRLRLRLR???????? to LLLL????RRRR????
	float* buf1 = (float*)d->output_buffer;
	float* buf2 = &buf1[ len * 2 ];
	for( int i = 0; i < ss->out_frames; i++ )
	{
	    buf1[ i ] = buf1[ i * 2 ];
	    buf2[ i ] = buf1[ i * 2 + 1 ];
	}
#if USE_RESAMPLER == 1
	if( resample_coef != 1 )
	{
	    for( int i = len - 1; i >= 0; i-- )
	    {
		buf1[ i ] = buf1[ (int)( i * resample_coef ) ];
	    }
	    for( int i = len - 1; i >= 0; i-- )
	    {
		buf2[ i ] = buf2[ (int)( i * resample_coef ) ];
	    }
	}
#endif
    }

    return d->output_buffer;
}

int device_sound_init_webaudio( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    int init_parts = 1 | 2; //1 - context init (get sample rate); 2 - create some audio nodes (may be deferred);
    if( ss->flags & SUNDOG_SOUND_FLAG_DEFERRED_INIT )
    {
	if( !ss->initialized )
	    init_parts = 1;
	else
	    init_parts = 2;
    }

    if( init_parts & 1 )
    {
	int v = EM_ASM_INT( {
	    sda_data = $0;
	    if( typeof(AudioContext) !== 'undefined' )
		sda_ctx = new AudioContext();
    	    else 
	    {
    		if( typeof(webkitAudioContext) !== 'undefined' ) 
    		    sda_ctx = new webkitAudioContext();
    	    }
    	    if( sda_ctx != undefined ) //!= null && != undefined
    	    {
    		return sda_ctx.sampleRate;
	    }
	    return 0;
	}, ss );
	if( v <= 0 )
	{
	    slog( "WEB AUDIO ERROR: Couldn't open audio context\n" );
	    return -1;
	}
	else
	{
#ifdef USE_RESAMPLER
	    d->real_sample_rate = v;
#else
	    ss->freq = v;
#endif
	}
	ss->out_type = sound_buffer_float32; //32bit float mode supported only:
	ss->in_type = sound_buffer_float32;
	ss->flags |= SUNDOG_SOUND_FLAG_ONE_THREAD;
    }
    
    if( init_parts & 2 )
    {
	d->buffer_size = EM_ASM_INT( {
	    sda_ctx.resume();
	    sda_node = sda_ctx.createScriptProcessor( 0, 0, $1 );
	    if( sda_node.bufferSize != $0 )
	    {
		var newSize = $0;
		if( sda_node.bufferSize > 4096 ) //Chromium + Android?
		    newSize = 4096;
		sda_node = sda_ctx.createScriptProcessor( newSize, 0, $1 );
	    }
	    return sda_node.bufferSize;
	}, d->buffer_size, ss->out_channels );
	d->output_buffer = smem_new( sizeof( float ) * d->buffer_size * ss->out_channels * ss->out_channels ); //*ss->out_channels - for data split in webaudio_callback()
	EM_ASM(
	    sda_node.onaudioprocess = function(e) 
	    {
		var out = e.outputBuffer;
		var buf_ptr1 = _webaudio_callback( sda_data, out.length, e.playbackTime - sda_ctx.currentTime );
		var buf_ptr2 = buf_ptr1 + out.length * out.numberOfChannels * 4;
		var buf1 = HEAPF32.subarray( buf_ptr1 >> 2, ( buf_ptr1 >> 2 ) + out.length );
		var buf2 = HEAPF32.subarray( buf_ptr2 >> 2, ( buf_ptr2 >> 2 ) + out.length );
		out.getChannelData( 0 ).set( buf1 );
		if( out.numberOfChannels >= 2 )
		    out.getChannelData( 1 ).set( buf2 );
	    };
	    sda_node.connect( sda_ctx.destination );
	);
#ifdef USE_RESAMPLER
	slog( "WEB AUDIO: %dHz -> %dHz; %d\n", ss->freq, d->real_sample_rate, d->buffer_size );
#else
	slog( "WEB AUDIO: %dHz; %d\n", ss->freq, d->buffer_size );
#endif
    }

    return 0;
}

void device_sound_deinit_webaudio( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    EM_ASM(
	if( sda_node != undefined ) 
	{
            sda_node.disconnect();
            sda_node = undefined;
        }
	if( sda_ctx != undefined ) 
	{
    	    sda_ctx.close();
            sda_ctx = undefined;
        }
    );
}

#endif

//
// SDL sound
//

#ifdef SDL

void sdl_audio_callback( void* udata, Uint8* stream, int len )
{
    sundog_sound* ss = (sundog_sound*)udata;
    device_sound* d = (device_sound*)ss->device_specific;

    int frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
    ss->out_buffer = stream;
    ss->out_frames = len / frame_size;
    ss->out_time = stime_ticks() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq );
    sundog_sound_callback( ss, 0 );
}

int device_sound_init_sdl( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    if( ss->flags & SUNDOG_SOUND_FLAG_DEFERRED_INIT )
    {
	if( !ss->initialized )
	    return 0;
    }

    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    desired.freq = ss->freq;
    desired.format = AUDIO_F32;
    desired.channels = ss->out_channels;
    desired.samples = d->buffer_size;
    desired.callback = sdl_audio_callback;
    desired.userdata = ss;
    
    if( SDL_OpenAudio( &desired, &obtained ) < 0 ) 
    {
	slog( "SDL AUDIO ERROR: Couldn't open audio: %s\n", SDL_GetError() );
	return -1;
    }
    
    slog( "SDL desired/obtained: %d/%d; %d/%d; %x/%x\n", desired.freq, obtained.freq, desired.samples, obtained.samples, desired.format, obtained.format );
    if( desired.freq != obtained.freq ) ss->freq = obtained.freq;
    if( desired.samples != obtained.samples ) d->buffer_size = obtained.samples;

    //32bit float mode supported only:
    ss->out_type = sound_buffer_float32;
    ss->in_type = sound_buffer_float32;
    
    SDL_PauseAudio( 0 );
    
    ss->flags |= SUNDOG_SOUND_FLAG_ONE_THREAD;
    
    return 0;
}

#endif

int device_sound_init( sundog_sound* ss )
{
    if( !ss->device_specific )
    {
	ss->device_specific = smem_new( sizeof( device_sound ) );
	smem_zero( ss->device_specific );
    }
    device_sound* d = (device_sound*)ss->device_specific;

    sundog_sound_set_defaults( ss );

    bool deferred = false;
    if( ss->flags & SUNDOG_SOUND_FLAG_DEFERRED_INIT )
	if( ss->initialized )
	    deferred = true;

    if( !deferred )
    {
	d->buffer_size = sconfig_get_int_value( APP_CFG_SND_BUF, g_default_buffer_size, 0 );
	slog( "Desired audio buffer size: %d frames\n", d->buffer_size );
    }

    bool sdriver_checked[ NUMBER_OF_SDRIVERS ];
    smem_clear( sdriver_checked, sizeof( sdriver_checked ) );

    while( 1 )
    {
	int prev_buffer_size = d->buffer_size;
	bool sdriver_ok = false;
	switch( ss->driver )
	{
#ifndef NOWEBAUDIO
	    case SDRIVER_WEBAUDIO:
		if( device_sound_init_webaudio( ss ) == 0 ) sdriver_ok = true;
		break;
#endif
#ifdef SDL
	    case SDRIVER_SDL:
		if( device_sound_init_sdl( ss ) == 0 ) sdriver_ok = true;
		break;
#endif
	    default:
		break;
	}
	if( sdriver_ok ) break;
	d->buffer_size = prev_buffer_size;
	if( ss->driver < NUMBER_OF_SDRIVERS ) sdriver_checked[ ss->driver ] = 1;
	ss->driver = 0;
	for( ss->driver = 0; ss->driver < NUMBER_OF_SDRIVERS; ss->driver++ )
	{
	    if( sdriver_checked[ ss->driver ] == 0 ) 
	    {
		slog( "Switching to %s\n", g_sdriver_names[ ss->driver ] );
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

    return 0;
}

int device_sound_deinit( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
    device_sound_input( ss, false );
    switch( ss->driver )
    {
#ifndef NOWEBAUDIO
	case SDRIVER_WEBAUDIO:
	    device_sound_deinit_webaudio( ss );
	    break;
#endif
#ifdef SDL
	case SDRIVER_SDL:
	    SDL_CloseAudio();
	    break;
#endif
	default:
	    break;
    }
    smem_free( d->output_buffer );
    d->output_buffer = 0;
    
    REMOVE_DEVICE_SPECIFIC_DATA( ss );
    
    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    return 0;
}
