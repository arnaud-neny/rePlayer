//
// Linux: sound
//

#include <pthread.h>

enum
{
#ifndef NOALSA
    SDRIVER_ALSA,
#endif
#ifdef JACK_AUDIO
    SDRIVER_JACK,
#endif
#ifdef SDL
    SDRIVER_SDL,
#endif
#ifndef NOOSS
    SDRIVER_OSS,
#endif
    NUMBER_OF_SDRIVERS
};
#define DEFAULT_SDRIVER 0

const char* g_sdriver_infos[] =
{
#ifndef NOALSA
    "ALSA",
#endif
#ifdef JACK_AUDIO
    "JACK",
#endif
#ifdef SDL
    "SDL",
#endif
#ifndef NOOSS
    "OSS",
#endif
};

const char* g_sdriver_names[] =
{
#ifndef NOALSA
    "alsa",
#endif
#ifdef JACK_AUDIO
    "jack",
#endif
#ifdef SDL
    "sdl",
#endif
#ifndef NOOSS
    "oss",
#endif
};

//ALSA:
#ifndef NOALSA
    #include <alsa/asoundlib.h>
#endif

//OSS:
#ifndef NOOSS
    #include <linux/soundcard.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

//SDL:
#ifdef SDL
    #ifdef SDL12
	#include "SDL/SDL.h"
    #else
	#include "SDL2/SDL.h"
    #endif
#endif

const int g_default_buffer_size = 2048;

struct device_sound //device-specific data of the sundog_sound object
{
    int				buffer_size;
    void* 			output_buffer;
#ifndef NOALSA
    snd_pcm_t* 			alsa_playback_handle;
    snd_pcm_t* 			alsa_capture_handle;
    snd_pcm_format_t		alsa_pcm_format[ 2 ]; //[0] = output; [1] = input;
    int				alsa_pcm_smp_size[ 2 ];
#endif
    int				oss_stream;
    pthread_t 			thread;
    volatile int 		thread_exit_request;
    pthread_t 			input_thread;
    volatile int 		input_thread_exit_request;
#ifdef COMMON_SOUNDINPUT_VARIABLES
    COMMON_SOUNDINPUT_VARIABLES;
#endif
#ifdef COMMON_JACK_VARIABLES
    COMMON_JACK_VARIABLES;
#endif
};

//
// ALSA, OSS, SDL, JACK sound
//

#ifndef NOALSA

#define ALSA_FMTS 4
static const snd_pcm_format_t g_alsa_fmts[ ALSA_FMTS ] = { SND_PCM_FORMAT_S16_LE, SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_FORMAT_S32_LE };

int xrun_recovery( snd_pcm_t* handle, int err )
{
    if( err == -EPIPE ) 
    { //under-run:
	printf( "ALSA EPIPE (underrun)\n" );
	err = snd_pcm_prepare( handle );
	if( err < 0 )
	    printf( "ALSA Can't recovery from underrun, prepare failed: %s\n", snd_strerror( err ) );
	return 0;
    }
    else if( err == -ESTRPIPE )
    {
	printf( "ALSA ESTRPIPE\n" );
	while( ( err = snd_pcm_resume( handle ) ) == -EAGAIN )
	    sleep( 1 ); //wait until the suspend flag is released
	if( err < 0 )
	{
	    err = snd_pcm_prepare( handle );
	    if( err < 0 )
		 printf( "ALSA Can't recovery from suspend, prepare failed: %s\n", snd_strerror( err ) );
	}
	return 0;
    }
    return err;
}

#endif

void* sound_thread( void* arg )
{
    sundog_denormal_numbers_check();
    sundog_sound* ss = (sundog_sound*)arg;
    device_sound* d = (device_sound*)ss->device_specific;
    int err;
    void* buf = d->output_buffer;
    while( 1 )
    {
	int len = d->buffer_size;
	ss->out_buffer = buf;
	ss->out_frames = len;
	ss->out_time = stime_ticks() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq ); //correct value must be with buffer_size*2 ?
	/*
	    stime_ticks_t cur_t = stime_ticks();
            static stime_ticks_t prev_t = 0;
            if( prev_t == 0 ) prev_t = cur_t;
            printf( "%d\n", cur_t - prev_t );
            prev_t = cur_t;
	*/
	get_input_data( ss, ss->out_frames );
	sundog_sound_callback( ss, 0 );
	bool alsa = 0;
	bool oss = 0;
	int frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
#ifndef NOALSA
	if( ss->driver == SDRIVER_ALSA )
	{
	    if( d->alsa_playback_handle != 0 && d->thread_exit_request == 0 )
	    {
		alsa = 1;
	    }
	}
#endif
#ifndef NOOSS
	if( ss->driver == SDRIVER_OSS )
	{
	    if( d->oss_stream >= 0 && d->thread_exit_request == 0 )
	    {
		oss = 1;
	    }
	}
#endif
#ifndef NOALSA
	if( alsa )
	{
	    if( ss->out_type == sound_buffer_float32 )
	    {
		float* fb = (float*)buf;
		if( d->alsa_pcm_format[ 0 ] == SND_PCM_FORMAT_S16_LE )
		{
		    int16_t* ib = (int16_t*)buf;
		    for( int i = 0; i < len * ss->out_channels; i++ )
			SMP_FLOAT32_TO_INT16( ib[ i ], fb[ i ] );
		}
		if( d->alsa_pcm_format[ 0 ] == SND_PCM_FORMAT_S32_LE )
		{
		    int32_t* ib = (int32_t*)buf;
		    for( int i = 0; i < len * ss->out_channels; i++ )
			SMP_FLOAT32_TO_INT32( ib[ i ], fb[ i ] );
		}
	    }
	    if( ss->out_type == sound_buffer_int16 )
	    {
		int16_t* ib = (int16_t*)buf;
		if( d->alsa_pcm_format[ 0 ] == SND_PCM_FORMAT_FLOAT_LE )
		{
		    float* fb = (float*)buf;
		    for( int i = len * ss->out_channels - 1; i >= 0; i-- )
			SMP_INT16_TO_FLOAT32( fb[ i ], ib[ i ] );
		}
		if( d->alsa_pcm_format[ 0 ] == SND_PCM_FORMAT_S32_LE )
		{
		    int32_t* ib2 = (int32_t*)buf;
		    for( int i = len * ss->out_channels - 1; i >= 0; i-- )
			SMP_INT16_TO_INT32( ib2[ i ], ib[ i ] );
		}
	    }
	    int8_t* buf_ptr = (int8_t*)buf;
	    while( len > 0 )
	    {
		err = snd_pcm_writei( d->alsa_playback_handle, buf_ptr, len );
		if( err == -EAGAIN )
                    continue;
		if( err < 0 ) 
		{
		    printf( "ALSA snd_pcm_writei error: %s\n", snd_strerror( err ) );
		    err = xrun_recovery( d->alsa_playback_handle, err );
		    if( err < 0 )
		    {
			printf( "ALSA xrun_recovery error: %s\n", snd_strerror( err ) );
			goto sound_thread_exit;
		    }
		}
		else
		{
		    len -= err;
		    buf_ptr += err * frame_size;
		}
	    }
	} 
#endif
#ifndef NOOSS
	if( oss )
	{
	    if( ss->out_type == sound_buffer_float32 )
	    {
		float* fb = (float*)buf;
		int16_t* sb = (int16_t*)buf;
		for( int i = 0; i < d->buffer_size * ss->out_channels; i++ )
		    SMP_FLOAT32_TO_INT16( sb[ i ], fb[ i ] );
	    }
	    err = write( d->oss_stream, buf, len * 4 ); 
	}
#endif

	if( alsa == 0 && oss == 0 ) 
	{
	    break;
	}
    }
sound_thread_exit:
    d->thread_exit_request = 0;
    pthread_exit( 0 );
    return 0;
}

void* input_sound_thread( void* arg )
{
    sundog_sound* ss = (sundog_sound*)arg;
    device_sound* d = (device_sound*)ss->device_specific;
#ifndef NOALSA
    int read_buf_size = 128;
    int32_t* tmp_buf32 = NULL;
    if( ( ss->in_type == sound_buffer_int16 && d->alsa_pcm_format[ 1 ] != SND_PCM_FORMAT_S16_LE ) ||
	( ss->in_type == sound_buffer_float32 && d->alsa_pcm_format[ 1 ] != SND_PCM_FORMAT_FLOAT_LE ) )
    {
	tmp_buf32 = (int32_t*)smem_new( sizeof( int32_t ) * read_buf_size * ss->in_channels );
	smem_zero( tmp_buf32 );
    }
    while( d->input_thread_exit_request == 0 ) 
    {
        int len = read_buf_size;
        int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
        volatile uint new_wp = d->input_buffer_wp & ( d->input_buffer_size - 1 );
        d->input_buffer_wp = new_wp;
        if( d->input_buffer_wp + len > d->input_buffer_size )
    	    len = d->input_buffer_size - d->input_buffer_wp;
    	int8_t* buf_ptr = (int8_t*)d->input_buffer + d->input_buffer_wp * frame_size;
    	while( len > 0 && d->input_thread_exit_request == 0 )
    	{
    	    int err;
    	    if( tmp_buf32 )
    	        err = snd_pcm_readi( d->alsa_capture_handle, tmp_buf32, len );
    	    else
    	        err = snd_pcm_readi( d->alsa_capture_handle, buf_ptr, len );
    	    if( err < 0 )
    	    {
    	        if( err == -EAGAIN )
        	    continue;
            	printf( "ALSA INPUT overrun\n" );
            	snd_pcm_prepare( d->alsa_capture_handle );
            }
	    else
	    {
	        if( tmp_buf32 )
	        {
	    	    if( ss->in_type == sound_buffer_int16 )
		    {
            	        int16_t* ib = (int16_t*)buf_ptr;
		        if( d->alsa_pcm_format[ 1 ] == SND_PCM_FORMAT_S32_LE )
		        {
		    	    for( int i = 0; i < err * ss->in_channels; i++ ) SMP_INT32_TO_INT16( ib[ i ], tmp_buf32[ i ] );
			}
			if( d->alsa_pcm_format[ 1 ] == SND_PCM_FORMAT_FLOAT_LE )
			{
			    for( int i = 0; i < err * ss->in_channels; i++ ) SMP_FLOAT32_TO_INT16( ib[ i ], ((float*)tmp_buf32)[ i ] );
			}
		    }
		    if( ss->in_type == sound_buffer_float32 )
		    {
            	        float* fb = (float*)buf_ptr;
		        if( d->alsa_pcm_format[ 1 ] == SND_PCM_FORMAT_S16_LE )
		        {
		    	    for( int i = 0; i < err * ss->in_channels; i++ ) SMP_INT16_TO_FLOAT32( fb[ i ], ((int16_t*)tmp_buf32)[ i ] );
			}
			if( d->alsa_pcm_format[ 1 ] == SND_PCM_FORMAT_S32_LE )
			{
			    for( int i = 0; i < err * ss->in_channels; i++ ) SMP_INT32_TO_FLOAT32( fb[ i ], tmp_buf32[ i ] );
			}
		    }
		}
		len -= err;
		buf_ptr += err * frame_size;
		new_wp = ( d->input_buffer_wp + err ) & ( d->input_buffer_size - 1 );
		d->input_buffer_wp = new_wp;
	    }
    	}
    }
    smem_free( tmp_buf32 );
    tmp_buf32 = NULL;
#endif
    d->input_thread_exit_request = 0;
    pthread_exit( 0 );
    return 0;
}

#ifdef SDL

void sdl_audio_callback( void* udata, Uint8* stream, int len )
{
    sundog_sound* ss = (sundog_sound*)udata;
    device_sound* d = (device_sound*)ss->device_specific;

    ss->out_buffer = stream;
    ss->out_frames = len / 4;
    ss->out_time = stime_ticks() + ( ( (uint64_t)d->buffer_size * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq );
    get_input_data( ss, ss->out_frames );
    sundog_sound_callback( ss, 0 );
}

#endif

int device_sound_init_alsa( sundog_sound* ss, bool input )
{
    device_sound* d = (device_sound*)ss->device_specific;

#ifndef NOALSA
    int rv = 1;
    int err;
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_sw_params_t* sw_params;

    snd_pcm_t** handle;
    const char* key;
    const char* input_label = "";
    snd_pcm_stream_t direction;
    if( input )
    {
	handle = &d->alsa_capture_handle;
	key = APP_CFG_SND_DEV_IN;
	input_label = " INPUT";
	direction = SND_PCM_STREAM_CAPTURE;
    }
    else
    {
	handle = &d->alsa_playback_handle;
	key = APP_CFG_SND_DEV;
	direction = SND_PCM_STREAM_PLAYBACK;
    }
    if( *handle ) return 0; //Already open

    while( 1 )
    {
	snd_pcm_hw_params_malloc( &hw_params );
	snd_pcm_sw_params_malloc( &sw_params );

	const char* device_name = NULL;
	const char* next_device_name = NULL;
	bool device_err = 1;
	device_name = sconfig_get_str_value( key, "", 0 );
	if( device_name[ 0 ] != 0 )
	{
	    err = snd_pcm_open( handle, device_name, direction, 0 );
	    if( err < 0 ) 
	    {
		slog( "ALSA%s ERROR: Can't open audio device %s: %s\n", input_label, device_name, snd_strerror( err ) );
		device_err = 1;
	    }
	    else
	    {
		slog( "ALSA%s: %s\n", input_label, device_name );
		device_err = 0;
	    }
	}
	next_device_name = "pulse";
	if( device_err && strcmp( device_name, next_device_name ) ) 
	{
	    err = snd_pcm_open( handle, next_device_name, direction, 0 );
	    if( err < 0 ) 
	    {
	        slog( "ALSA%s ERROR: Can't open audio device %s: %s\n", input_label, next_device_name, snd_strerror( err ) );
		device_err = 1;
	    }
	    else 
	    {
		slog( "ALSA%s: %s\n", input_label, next_device_name );
		device_err = 0;
	    }
	}
	next_device_name = "default";
	if( device_err && strcmp( device_name, next_device_name ) ) 
	{
	    err = snd_pcm_open( handle, next_device_name, direction, 0 );
	    if( err < 0 )
	    {
		slog( "ALSA%s ERROR: Can't open audio device %s: %s\n", input_label, next_device_name, snd_strerror( err ) );
		device_err = 1;
	    }
	    else 
	    {
		slog( "ALSA%s: %s\n", input_label, next_device_name );
		device_err = 0;
	    }
	}
	if( device_err ) break;

	err = snd_pcm_hw_params_any( *handle, hw_params );
	if( err < 0 )
	{
	    slog( "ALSA%s ERROR: Can't initialize hardware parameter structure: %s\n", input_label, snd_strerror( err ) );
	    break;
	}
	err = snd_pcm_hw_params_set_access( *handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED );
	if( err < 0 )
	{
	    slog( "ALSA%s ERROR: Can't set access type: %s\n", input_label, snd_strerror( err ) );
	    break;
	}

	d->alsa_pcm_format[ input ] = SND_PCM_FORMAT_S16_LE;
	sound_buffer_type smp_type = ss->out_type;
	if( input ) smp_type = ss->in_type;
	if( smp_type == sound_buffer_float32 ) d->alsa_pcm_format[ input ] = SND_PCM_FORMAT_FLOAT_LE;
	int ff = 0; for( ff = 0; ff < ALSA_FMTS; ff++ )
	{
	    if( g_alsa_fmts[ ff ] == d->alsa_pcm_format[ input ] ) break;
	}
	for( int a = 0; a < 3; a++ )
	{
	    err = snd_pcm_hw_params_set_format( *handle, hw_params, d->alsa_pcm_format[ input ] );
	    if( err == 0 ) break;
	    slog( "ALSA%s ERROR: Can't set sample format %d: %s\n", input_label, (int)d->alsa_pcm_format[ input ], snd_strerror( err ) );
	    ff++;
	    if( ff >= ALSA_FMTS ) ff = 0;
	    d->alsa_pcm_format[ input ] = g_alsa_fmts[ ff ]; //S16 -> S32 -> FLOAT -> S32 -> ...
	}
	if( err < 0 ) break;
	d->alsa_pcm_smp_size[ input ] = 4;
	if( d->alsa_pcm_format[ input ] == SND_PCM_FORMAT_S16_LE ) d->alsa_pcm_smp_size[ input ] = 2;

	err = snd_pcm_hw_params_set_rate_near( *handle, hw_params, (unsigned int*)&ss->freq, 0 );
	if( err < 0 ) 
	{
	    slog( "ALSA%s ERROR: Can't set rate: %s\n", input_label, snd_strerror( err ) );
	    break;
	}
	slog( "ALSA%s HW Default rate: %d\n", input_label, ss->freq );

	int ch_cnt = ss->out_channels;
	if( input ) ch_cnt = ss->in_channels;
	err = snd_pcm_hw_params_set_channels( *handle, hw_params, ch_cnt );
	if( err < 0 )
	{
    	    slog( "ALSA%s ERROR: Can't set channel count %d: %s\n", input_label, ch_cnt, snd_strerror( err ) );
    	    if( input && ch_cnt == 2 )
    	    {
    		ch_cnt = 1;
		err = snd_pcm_hw_params_set_channels( *handle, hw_params, ch_cnt );
		if( err == 0 )
		{
		    ss->in_channels = ch_cnt;
		}
		else
		{
    		    slog( "ALSA%s ERROR: Can't set channel count %d: %s\n", input_label, ch_cnt, snd_strerror( err ) );
    		    break;
		}
    	    }
    	    else
    	    {
		break;
	    }
	}

	snd_pcm_uframes_t frames = d->buffer_size * 2;
	err = snd_pcm_hw_params_set_buffer_size_near( *handle, hw_params, &frames );
	if( err < 0 ) 
	{
	    slog( "ALSA%s ERROR: Can't set buffer size: %s\n", input_label, snd_strerror( err ) );
	    break;
	}
	err = snd_pcm_hw_params( *handle, hw_params );
	if( err < 0 ) 
	{
	    slog( "ALSA%s ERROR: Can't set parameters: %s\n", input_label, snd_strerror( err ) );
	    break;
	}

	snd_pcm_hw_params_current( *handle, hw_params );
	snd_pcm_hw_params_get_rate( hw_params, (unsigned int*)&ss->freq, 0 );
	slog( "ALSA%s HW Rate: %d frames\n", input_label, ss->freq );
	snd_pcm_hw_params_get_buffer_size( hw_params, &frames );
	slog( "ALSA%s HW Buffer size: %d frames\n", input_label, frames );
	snd_pcm_hw_params_get_period_size( hw_params, &frames, NULL );
	slog( "ALSA%s HW Period size: %d\n", input_label, (int)frames );
	uint v;
	snd_pcm_hw_params_get_periods( hw_params, &v, NULL );
	slog( "ALSA%s HW Periods: %d\n", input_label, v );
    
	snd_pcm_sw_params_current( *handle, sw_params );
	snd_pcm_sw_params_get_avail_min( sw_params, &frames );
	slog( "ALSA%s SW Avail min: %d\n", input_label, (int)frames );
	snd_pcm_sw_params_get_start_threshold( sw_params, &frames );
	slog( "ALSA%s SW Start threshold: %d\n", input_label, (int)frames );
	snd_pcm_sw_params_get_stop_threshold( sw_params, &frames );
	slog( "ALSA%s SW Stop threshold: %d\n", input_label, (int)frames );
    
	//snd_pcm_sw_params_set_start_threshold( *handle, sw_params, d->buffer_size - 32 );
	//snd_pcm_sw_params_set_avail_min( *handle, sw_params, frames / 8 );
	//snd_pcm_sw_params( *handle, sw_params );
    
	snd_pcm_hw_params_free( hw_params );
	snd_pcm_sw_params_free( sw_params );

	if( input )
	{
	    d->input_buffer_wp = 0;
            d->input_buffer_rp = 0;
	}
	else
	{
	    int frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
	    int frame_size2 = d->alsa_pcm_smp_size[ 0 ] * ss->out_channels;
	    if( frame_size2 > frame_size ) frame_size = frame_size2;
    	    smem_free( d->output_buffer );
	    d->output_buffer = smem_new( d->buffer_size * frame_size );
	}

	err = snd_pcm_prepare( *handle );
	if( err < 0 ) 
	{
	    slog( "ALSA%s ERROR: Can't prepare audio interface for use: %s\n", input_label, snd_strerror( err ) );
	    break;
	}
	//Create sound thread:
	if( input )
	{
	    if( pthread_create( &d->input_thread, NULL, input_sound_thread, ss ) != 0 )
	    {
    		slog( "ALSA%s ERROR: Can't create sound thread!\n", input_label );
    		break;
    	    }
	}
	else
	{
	    if( pthread_create( &d->thread, NULL, sound_thread, ss ) != 0 )
	    {
    		slog( "ALSA%s ERROR: Can't create sound thread!\n", input_label );
    		break;
    	    }
	}

	if( input )
	{
	}
	else
	{
#ifndef NOALSA
	    ss->driver = SDRIVER_ALSA;
#endif
	}

	rv = 0; //Successful

	break;
    }

    if( rv && handle[ 0 ] )
    {
	snd_pcm_close( *handle );
	*handle = 0;
    }

    return rv;
#else
    return 1;
#endif
}

int device_sound_init_oss( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
#ifndef NOOSS
    if( ss->out_channels != 2 )
    {
	slog( "OSS ERROR: channels must be 2\n" );
	return 1;
    }
    //Start first time:
    int temp;
    d->oss_stream = open( sconfig_get_str_value( APP_CFG_SND_DEV, "/dev/dsp", 0 ), O_WRONLY, 0 );
    if( d->oss_stream == -1 )
	d->oss_stream = open( "/dev/.static/dev/dsp", O_WRONLY, 0 );
    if( d->oss_stream == -1 )
    {
        slog( "OSS ERROR: Can't open sound device\n" );
        return 1;
    }
    temp = 1;
    ioctl( d->oss_stream, SNDCTL_DSP_STEREO, &temp );
    temp = 16;
    ioctl( d->oss_stream, SNDCTL_DSP_SAMPLESIZE, &temp );
    temp = ss->freq;
    ioctl( d->oss_stream, SNDCTL_DSP_SPEED, &temp );
    temp = 16 << 16 | 8;
    ioctl( d->oss_stream, SNDCTL_DSP_SETFRAGMENT, &temp );
    ioctl( d->oss_stream, SNDCTL_DSP_GETBLKSIZE, &temp );
    
    int frame_size = g_sample_size[ ss->out_type ] * ss->out_channels;
    smem_free( d->output_buffer );
    d->output_buffer = smem_new( d->buffer_size * frame_size );
    
    //Create sound thread:
    if( pthread_create( &d->thread, NULL, sound_thread, ss ) != 0 )
    {
        slog( "OSS ERROR: Can't create sound thread!\n" );
        return 1;
    }
    
    ss->driver = SDRIVER_OSS;
    
    return 0;
#else
    return 1;
#endif
}

int device_sound_init_sdl( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    
#ifdef SDL
    if( g_sdl_initialized == false )
    {
        g_sdl_initialized = true;
        SDL_Init( 0 );
    }

    SDL_AudioSpec a;
    
    a.freq = ss->freq;
    a.format = AUDIO_S16;
    a.channels = ss->out_channels;
    a.samples = d->buffer_size;
    a.callback = sdl_audio_callback;
    a.userdata = ss;
    
    if( SDL_OpenAudio( &a, NULL ) < 0 ) 
    {
	slog( "SDL AUDIO ERROR: Couldn't open audio: %s\n", SDL_GetError() );
	return -1;
    }

    //16bit mode supported only:
    ss->out_type = sound_buffer_int16;
    ss->in_type = sound_buffer_int16;
    
    SDL_PauseAudio( 0 );
    
    return 0;
#else
    return 1;
#endif
}

int device_sound_init( sundog_sound* ss )
{
    ss->device_specific = smem_new( sizeof( device_sound ) );
    device_sound* d = (device_sound*)ss->device_specific;
    smem_zero( d );

    sundog_sound_set_defaults( ss );

    d->thread_exit_request = 0;
    d->buffer_size = sconfig_get_int_value( APP_CFG_SND_BUF, g_default_buffer_size, 0 );
    slog( "Desired audio buffer size: %d frames\n", d->buffer_size );

    bool sdriver_checked[ NUMBER_OF_SDRIVERS ];
    smem_clear( sdriver_checked, sizeof( sdriver_checked ) );

    while( 1 )
    {
	int prev_buffer_size = d->buffer_size;
	bool sdriver_ok = 0;
	switch( ss->driver )
	{
#ifdef JACK_AUDIO
	    case SDRIVER_JACK:
		if( device_sound_init_jack( ss ) == 0 ) sdriver_ok = 1;
		break;
#endif
#ifndef NOALSA
	    case SDRIVER_ALSA:
		if( device_sound_init_alsa( ss, false ) == 0 ) sdriver_ok = 1;
		break;
#endif
#ifndef NOOSS
	    case SDRIVER_OSS:
		if( device_sound_init_oss( ss ) == 0 ) sdriver_ok = 1;
		break;
#endif
#ifdef SDL
	    case SDRIVER_SDL:
		if( device_sound_init_sdl( ss ) == 0 ) sdriver_ok = 1;
		break;
#endif
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
#ifdef JACK_AUDIO
	case SDRIVER_JACK:
	    device_sound_deinit_jack( ss );
	    break;
#endif
#ifndef NOALSA
	case SDRIVER_ALSA:
	    if( d->alsa_playback_handle )
	    {
		d->thread_exit_request = 1;
		STIME_WAIT_FOR( d->thread_exit_request == 0, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 10, );
		snd_pcm_close( d->alsa_playback_handle );
		d->alsa_playback_handle = 0;
	    }
	    break;
#endif
#ifndef NOOSS
	case SDRIVER_OSS:
	    if( d->oss_stream >= 0 )
	    {
		d->thread_exit_request = 1;
		STIME_WAIT_FOR( d->thread_exit_request == 0, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 10, );
		close( d->oss_stream );
		d->oss_stream = -1;
	    }
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
    d->output_buffer = NULL;
    remove_input_buffers( ss );
    
    REMOVE_DEVICE_SPECIFIC_DATA( ss );
    
    return 0;
}

void device_sound_input( sundog_sound* ss, bool enable )
{
    device_sound* d = (device_sound*)ss->device_specific;
#ifdef JACK_AUDIO
    if( ss->driver == SDRIVER_JACK ) return;
#endif
    int input_driver = ss->driver;
#ifndef NOALSA
#ifndef NOOSS
    if( ss->driver == SDRIVER_OSS ) input_driver = SDRIVER_ALSA;
#endif
#endif
#ifndef NOALSA
#ifdef SDL
    if( ss->driver == SDRIVER_SDL ) input_driver = SDRIVER_ALSA;
#endif
#endif
    if( enable )
    {
#ifndef NOALSA
	set_input_defaults( ss );
	create_input_buffers( ss, d->buffer_size );
	if( input_driver == SDRIVER_ALSA )
	{
	    if( device_sound_init_alsa( ss, true ) == 0 )
		d->input_enabled = true;
	}
#endif
    }
    else
    {
#ifndef NOALSA
	if( input_driver == SDRIVER_ALSA )
	{
	    if( d->input_enabled && d->alsa_capture_handle )
    	    {
    	        d->input_thread_exit_request = 1;
    	        STIME_WAIT_FOR( d->input_thread_exit_request == 0, SUNDOG_SOUND_DEFAULT_TIMEOUT_MS, 10, );
        	snd_pcm_close( d->alsa_capture_handle );
            	d->alsa_capture_handle = 0;
            	d->input_enabled = false;
    	    }
	}
#endif
    }
}

int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input )
{
    int rv = 0;
    
    if( driver == 0 ) return 0;
    
    int drv_num = -1;
    for( int drv = 0; drv < NUMBER_OF_SDRIVERS; drv++ )
    {
	if( smem_strcmp( g_sdriver_names[ drv ], driver ) == 0 )
	{
	    drv_num = drv;
	    break;
	}
	if( smem_strcmp( g_sdriver_infos[ drv ], driver ) == 0 )
	{
	    drv_num = drv;
	    break;
	}
    }
    if( drv_num == -1 ) return 0;
    
    if( input )
    {
#ifndef NOALSA
#ifndef NOOSS
	if( drv_num == SDRIVER_OSS ) drv_num = SDRIVER_ALSA;
#endif
#ifdef SDL
	if( drv_num == SDRIVER_SDL ) drv_num = SDRIVER_ALSA;
#endif
#endif
    }

    *names = 0;
    *infos = 0;
    char* ts = (char*)smem_new( 2048 );

    switch( drv_num )
    {
#ifndef NOALSA
	case SDRIVER_ALSA:
	    {
		snd_ctl_card_info_t* info;
                snd_pcm_info_t* pcminfo;
		snd_ctl_card_info_alloca( &info );
		snd_pcm_info_alloca( &pcminfo );
		int card = -1;
		while( 1 )
		{
		    if( snd_card_next( &card ) != 0 )
			break; //Error
		    if( card < 0 )
			break; //No more cards
		    while( 1 )
		    {
			snd_ctl_t* handle;
			int err;
			char name[ 32 ];
            		sprintf( name, "hw:%d", card );
			err = snd_ctl_open( &handle, name, 0 );
			if( err < 0 )
			{
			    slog( "ALSA ERROR: control open (%d): %s\n", card, snd_strerror( err ) );
			    break;
			}
			err = snd_ctl_card_info( handle, info );
			if( err < 0 )
			{
			    slog( "ALSA ERROR: control hardware info (%d): %s\n", card, snd_strerror( err ) );
			    snd_ctl_close( handle );
			    break;
			}
			int dev = -1;
			while( 1 )
			{
			    if( snd_ctl_pcm_next_device( handle, &dev ) < 0 )
			    {
                                slog( "ALSA ERROR: snd_ctl_pcm_next_device\n" );
                                break;
                            }
                            if( dev < 0 ) break;
                            snd_pcm_info_set_device( pcminfo, dev );
                            snd_pcm_info_set_subdevice( pcminfo, 0 );
                            if( input )
                        	snd_pcm_info_set_stream( pcminfo, SND_PCM_STREAM_CAPTURE );
                    	    else
                        	snd_pcm_info_set_stream( pcminfo, SND_PCM_STREAM_PLAYBACK );
                            err = snd_ctl_pcm_info( handle, pcminfo );
                            if( err < 0 )
                            {
                        	if( err != -ENOENT )
                            	    slog( "ALSA ERROR: control digital audio info (%d): %s\n", card, snd_strerror( err ) );
                        	continue;
                            }
                            //Device name:
                            sprintf( ts, "hw:%d,%d", card, dev );
                            smem_objarray_write_d( (void***)names, ts, true, rv );
                            //Device info:
                            sprintf( ts, "hw:%d,%d %s, %s", card, dev, snd_ctl_card_info_get_name( info ), snd_pcm_info_get_name( pcminfo ) );
			    smem_objarray_write_d( (void***)infos, ts, true, rv );
                            rv++;
                            //Subdevices:
                            if( 0 )
                            {
                        	uint count = snd_pcm_info_get_subdevices_count( pcminfo );
                        	for( int idx = 0; idx < (int)count; idx++ ) 
                        	{
                        	    snd_pcm_info_set_subdevice( pcminfo, idx );
                        	    err = snd_ctl_pcm_info( handle, pcminfo );
                        	    if( err < 0 )
                        		slog( "ALSA ERROR: control digital audio playback info (%d): %s\n", card, snd_strerror( err ) );
                        	    else
                        	    {
                        		sprintf( ts, "hw:%d,%d,%d", card, dev, idx );
                        		(*names)[ rv ] = (char*)smem_new( smem_strlen( ts ) + 1 ); (*names)[ rv ][ 0 ] = 0; smem_strcat_resize( (*names)[ rv ], ts );
                        		//Device info:
                        		sprintf( ts, "hw:%d,%d,%d %s", card, dev, idx, snd_pcm_info_get_subdevice_name( pcminfo ) );
                        		(*infos)[ rv ] = (char*)smem_new( smem_strlen( ts ) + 1 ); (*infos)[ rv ][ 0 ] = 0; smem_strcat_resize( (*infos)[ rv ], ts );
                        		rv++;
                        	    }
                        	}
                    	    }
			}
			snd_ctl_close( handle );
			break;
		    }
		}
	    }
	    break;
#endif
#ifndef NOOSS
	case SDRIVER_OSS:
	    break;
#endif
#ifdef SDL
	case SDRIVER_SDL:
	    break;
#endif
	default:
	    break;
    }

    smem_free( ts );

    return rv;
}
