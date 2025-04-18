//
// JACK Audio Connection Kit: sound and MIDI
//

/*
dev_name is ignored in device_midi_client_open_port( c, pnum, port_name, dev_name, flags )
A new MIDI port "port_name" will be created and will appear in the JACK node.
*/

#ifdef JACK_AUDIO


#ifndef COMMON_CODE
//
// Header
//


#ifdef OS_LINUX
    #define JACK_DYNAMIC_LINK
    #include <dlfcn.h>
#endif

#include <jack/jack.h>
#include <jack/midiport.h>

//For the device_sound structure:
#define COMMON_JACK_VARIABLES \
    jack_client_t* 		jack_client; \
    jack_port_t* 		jack_in_ports[ 2 ]; \
    void* 			jack_temp_input; \
    jack_port_t*		jack_out_ports[ 2 ]; \
    void* 			jack_temp_output; \
    jack_nframes_t 		jack_callback_nframes; \
    stime_ticks_t 		jack_callback_output_time; \
    uint8_t 			jack_midi_out_data[ MIDI_BYTES ]; \
    device_midi_event_jack 	jack_midi_out_events[ MIDI_EVENTS ]; \
    uint 			jack_midi_out_data_wp; \
    volatile uint 		jack_midi_out_evt_rp; \
    volatile uint 		jack_midi_out_evt_wp; \
    device_midi_port_jack* 	jack_midi_ports_to_clear[ 128 ]; \
    int 			jack_midi_clear_count;

void* g_jack_lib = nullptr;
bool g_jack_restore_midiin_con = true;

struct device_midi_port_jack
{
    jack_port_t* 		port;
    stime_ticks_t 		output_time;
    void* 			buffer;
    //for get_event():
    uint8_t* 			r_data;
    sundog_midi_event* 		r_events;
    volatile uint 		r_data_wp;
    volatile uint 		r_events_wp;
    volatile uint 		r_events_rp;
};

struct device_midi_event_jack
{
    device_midi_port_jack* 	port;
    sundog_midi_event 		evt;
};

extern void jack_publish_client_icon( jack_client_t* client );

int device_sound_init_jack( sundog_sound* ss );
void device_sound_deinit_jack( sundog_sound* ss );
int device_midi_client_open_jack( sundog_midi_client* c, const char* name );
int device_midi_client_close_jack( sundog_midi_client* c );
int device_midi_client_get_devices_jack( sundog_midi_client* c, char*** devices, uint32_t flags );
int device_midi_client_open_port_jack( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags );
int device_midi_client_close_port_jack( sundog_midi_client* c, int pnum );
sundog_midi_event* device_midi_client_get_event_jack( sundog_midi_client* c, int pnum );
int device_midi_client_next_event_jack( sundog_midi_client* c, int pnum );
int device_midi_client_send_event_jack( sundog_midi_client* c, int pnum, sundog_midi_event* evt );


#else
//
// Code
//


#ifdef JACK_DYNAMIC_LINK

#define JACK_GET_FN( NAME ) \
    const char* fn_name = NAME; \
    static void* f = nullptr; \
    if( !f ) f = dlsym( g_jack_lib, fn_name ); \
    if( !f ) \
	slog( "JACK: Function %s() not found.\n", fn_name ); \

const char* jack_get_version_string( void )
{
    JACK_GET_FN( "jack_get_version_string" );
    if( f ) return ( (const char*(*)(void))f ) ();
    return 0;
}

jack_client_t* jack_client_open( const char* client_name, jack_options_t options, jack_status_t* status, ... )
{
    JACK_GET_FN( "jack_client_open" );
    if( f ) return ( (jack_client_t*(*)(const char*,jack_options_t,jack_status_t*,...))f ) ( client_name, options, status );
    return 0;
}

int jack_client_close( jack_client_t* client )
{
    JACK_GET_FN( "jack_client_close" );
    if( f ) return ( (int(*)(jack_client_t*))f ) ( client );
    return 0;
}

int jack_activate( jack_client_t* client )
{
    JACK_GET_FN( "jack_activate" );
    if( f ) return ( (int(*)(jack_client_t*))f ) ( client );
    return 0;
}

int jack_set_process_callback( jack_client_t* client, JackProcessCallback process_callback, void* arg )
{
    JACK_GET_FN( "jack_set_process_callback" );
    if( f ) return ( (int(*)(jack_client_t*,JackProcessCallback,void*))f ) ( client, process_callback, arg );
    return 0;
}

void jack_on_shutdown( jack_client_t* client, JackShutdownCallback shutdown_callback, void* arg )
{
    JACK_GET_FN( "jack_on_shutdown" );
    if( f ) ( (void(*)(jack_client_t*,JackShutdownCallback,void*))f ) ( client, shutdown_callback, arg );
}

jack_port_t* jack_port_register( jack_client_t* client, const char* port_name, const char* port_type, unsigned long flags, unsigned long buffer_size )
{
    JACK_GET_FN( "jack_port_register" );
    if( f ) return ( (jack_port_t*(*)(jack_client_t*,const char*,const char*,unsigned long,unsigned long))f ) ( client, port_name, port_type, flags, buffer_size );
    return 0;
}

int jack_port_unregister( jack_client_t* client, jack_port_t* port )
{
    JACK_GET_FN( "jack_port_unregister" );
    if( f ) return ( (int(*)(jack_client_t*,jack_port_t*))f ) ( client, port );
    return 0;
}

jack_nframes_t jack_port_get_total_latency( jack_client_t* client, jack_port_t* port )
{
    JACK_GET_FN( "jack_port_get_total_latency" );
    if( f ) return ( (jack_nframes_t(*)(jack_client_t*,jack_port_t*))f ) ( client, port );
    return 0;
}

void jack_port_get_latency_range( jack_port_t* port, jack_latency_callback_mode_t mode, jack_latency_range_t* range )
{
    JACK_GET_FN( "jack_port_get_latency_range" );
    if( f ) ( (void(*)(jack_port_t*,jack_latency_callback_mode_t,jack_latency_range_t*))f ) ( port, mode, range );
}

const char* jack_port_name( const jack_port_t* port )
{
    JACK_GET_FN( "jack_port_name" );
    if( f ) return ( (const char*(*)(const jack_port_t*))f ) ( port );
    return 0;
}

jack_time_t jack_frames_to_time( const jack_client_t* client, jack_nframes_t frames )
{
    JACK_GET_FN( "jack_frames_to_time" );
    if( f ) return ( (jack_time_t(*)(const jack_client_t*,jack_nframes_t))f ) ( client, frames );
    return 0;
}

jack_nframes_t jack_last_frame_time( const jack_client_t* client )
{
    JACK_GET_FN( "jack_last_frame_time" );
    if( f ) return ( (jack_nframes_t(*)(const jack_client_t*))f ) ( client );
    return 0;
}

jack_time_t jack_get_time( void )
{
    JACK_GET_FN( "jack_get_time" );
    if( f ) return ( (jack_time_t(*)(void))f ) ();
    return 0;
}

void* jack_port_get_buffer( jack_port_t* port, jack_nframes_t frames )
{
    JACK_GET_FN( "jack_port_get_buffer" );
    if( f ) return ( (void*(*)(jack_port_t*,jack_nframes_t))f ) ( port, frames );
    return 0;
}

int jack_port_flags( const jack_port_t* port )
{
    JACK_GET_FN( "jack_port_flags" );
    if( f ) return ( (int(*)(const jack_port_t*))f ) ( port );
    return 0;
}

const char** jack_port_get_connections( const jack_port_t* port )
{
    JACK_GET_FN( "jack_port_get_connections" );
    if( f ) return ( (const char**(*)(const jack_port_t*))f ) ( port );
    return 0;
}

void jack_midi_clear_buffer( void* port_buffer )
{
    JACK_GET_FN( "jack_midi_clear_buffer" );
    if( f ) ( (void(*)(void*))f ) ( port_buffer );
}

jack_midi_data_t* jack_midi_event_reserve( void* port_buffer, jack_nframes_t time, size_t data_size )
{
    JACK_GET_FN( "jack_midi_event_reserve" );
    if( f ) return ( (jack_midi_data_t*(*)(void*,jack_nframes_t,size_t))f ) ( port_buffer, time, data_size );
    return 0;
}

jack_nframes_t jack_midi_get_event_count( void* port_buffer )
{
    JACK_GET_FN( "jack_midi_get_event_count" );
    if( f ) return ( (jack_nframes_t(*)(void*))f ) ( port_buffer );
    return 0;
}

int jack_midi_event_get( jack_midi_event_t* event, void* port_buffer, jack_nframes_t event_index )
{
    JACK_GET_FN( "jack_midi_event_get" );
    if( f ) return ( (int(*)(jack_midi_event_t*,void*,jack_nframes_t))f ) ( event, port_buffer, event_index );
    return 0;
}

const char** jack_get_ports( jack_client_t* client, const char* port_name_pattern, const char* type_name_pattern, unsigned long flags )
{
    JACK_GET_FN( "jack_get_ports" );
    if( f ) return ( (const char**(*)(jack_client_t*,const char*,const char*,unsigned long))f ) ( client, port_name_pattern, type_name_pattern, flags );
    return 0;
}

int jack_connect( jack_client_t* client, const char* source_port, const char* destination_port )
{
    JACK_GET_FN( "jack_connect" );
    if( f ) return ( (int(*)(jack_client_t*,const char*,const char*))f ) ( client, source_port, destination_port );
    return 0;
}

void jack_free( void* ptr )
{
    JACK_GET_FN( "jack_free" );
    if( f ) ( (void(*)(void*))f ) ( ptr );
}

jack_nframes_t jack_get_sample_rate( jack_client_t* client )
{
    JACK_GET_FN( "jack_get_sample_rate" );
    if( f ) return ( (jack_nframes_t(*)(jack_client_t*))f ) ( client );
    return 0;
}

#endif

static int jack_process_callback( jack_nframes_t nframes, void* arg )
{
    sundog_sound* ss = (sundog_sound*)arg;
    device_sound* d = (device_sound*)ss->device_specific;
    
    d->jack_callback_nframes = nframes;
    
    size_t out_sample_size = 0;
    if( ss->out_type == sound_buffer_float32 ) out_sample_size = 4;
    if( ss->out_type == sound_buffer_int16 ) out_sample_size = 2;
    size_t in_sample_size = 0;
    if( ss->in_type == sound_buffer_float32 ) in_sample_size = 4;
    if( ss->in_type == sound_buffer_int16 ) in_sample_size = 2;

    //Create temp buffers:
    size_t size, old_size;
#ifdef JACK_INPUT
    size = nframes * out_sample_size * ss->out_channels;
    if( !d->jack_temp_input )
    {
        d->jack_temp_input = smem_new( size );
        smem_zero( d->jack_temp_input );
    }
    old_size = smem_get_size( d->jack_temp_input );
    if( old_size < size )
    {
        d->jack_temp_input = smem_resize2( d->jack_temp_input, size );
    }
#endif
    size = nframes * in_sample_size * ss->in_channels;
    if( !d->jack_temp_output )
    {
        d->jack_temp_output = smem_new( size );
        smem_zero( d->jack_temp_output );
    }
    old_size = smem_get_size( d->jack_temp_output );
    if( old_size < size )
    {
        d->jack_temp_output = smem_resize2( d->jack_temp_output, size );
    }
    
    //Get latency:
    jack_nframes_t latency;
//#ifdef OS_LINUX
//    latency = jack_port_get_total_latency( d->jack_client, d->jack_out_ports[ 0 ] );
//#else
    jack_latency_range_t latency_range;
    jack_port_get_latency_range( d->jack_out_ports[ 0 ], JackPlaybackLatency, &latency_range );
    latency = latency_range.max;
//#endif
    ss->out_latency = latency;
    ss->out_latency2 = latency;
    
    //Get time:
    jack_time_t current_usecs = jack_frames_to_time( d->jack_client, jack_last_frame_time( d->jack_client ) );
    jack_time_t cur_jack_t = jack_get_time();
    stime_ticks_t cur_t = stime_ticks();
    jack_time_t output_time = current_usecs + ( latency * 1000000 ) / ss->freq;
    jack_time_t delta = ( ( output_time - cur_jack_t ) * stime_ticks_per_second() ) / 1000000;
    if( ( delta & 0x80000000 ) == 0 ) //delta is positive
        ss->out_time = cur_t + (stime_ticks_t)delta;
    else
        ss->out_time = cur_t;
    d->jack_callback_output_time = ss->out_time;
    
#ifdef JACK_INPUT
    //Fill input buffer:
    for( int c = 0; c < ss->in_channels; c++ )
    {
        float* buf = (float*)jack_port_get_buffer( d->jack_in_ports[ c ], nframes );
        if( ss->in_type == sound_buffer_int16 )
        {
    	    int16_t* input = (int16_t*)d->jack_temp_input;
            for( size_t i = c, i2 = 0; i < nframes * ss->in_channels; i += ss->in_channels, i2++ )
            {
                SMP_FLOAT32_TO_INT16( input[ i ], buf[ i2 ] );
            }
        }
        if( ss->in_type == sound_buffer_float32 )
        {
    	    float* input = (float*)d->jack_temp_input;
            for( size_t i = c, i2 = 0; i < nframes * ss->in_channels; i += ss->in_channels, i2++ )
            {
                input[ i ] = buf[ i2 ];
            }
        }
    }
#endif

    //Render:
    ss->out_buffer = d->jack_temp_output;
    ss->out_frames = nframes;
#ifdef JACK_INPUT
    ss->in_buffer = d->jack_temp_input;
#else
    ss->in_buffer = nullptr;
#endif
    sundog_sound_callback( ss, 0 );
    
    //Fill output buffers:
    for( int c = 0; c < ss->out_channels; c++ )
    {
        float* buf = (float*)jack_port_get_buffer( d->jack_out_ports[ c ], nframes );
        if( ss->out_type == sound_buffer_int16 )
        {
    	    int16_t* output = (int16_t*)d->jack_temp_output;
            for( size_t i = c, i2 = 0; i < nframes * ss->out_channels; i += ss->out_channels, i2++ )
            {
                SMP_INT16_TO_FLOAT32( buf[ i2 ], output[ i ] );
            }
        }
        if( ss->out_type == sound_buffer_float32 )
        {
    	    float* output = (float*)d->jack_temp_output;
            for( size_t i = c, i2 = 0; i < nframes * ss->out_channels; i += ss->out_channels, i2++ )
            {
                buf[ i2 ] = output[ i ];
            }
        }
    }
    
    //Handle MIDI output:
    if( d->jack_midi_clear_count )
    {
        //Clear ports which were used in previous callback:
        for( int p = 0; p < d->jack_midi_clear_count; p++ )
        {
            device_midi_port_jack* port = d->jack_midi_ports_to_clear[ p ];
            if( port && port->port )
            {
                if( port->output_time != d->jack_callback_output_time )
                {
                    port->output_time = d->jack_callback_output_time;
                    jack_midi_clear_buffer( jack_port_get_buffer( port->port, d->jack_callback_nframes ) );
                    port->buffer = jack_port_get_buffer( port->port, d->jack_callback_nframes );
                }
            }
        }
        d->jack_midi_clear_count = 0;
    }
    if( d->jack_midi_out_evt_wp != d->jack_midi_out_evt_rp )
    {
        //Sort:
        //CHANGE IT TO THE INSERTION ALG!
        size_t wp = d->jack_midi_out_evt_wp;
        size_t rp = d->jack_midi_out_evt_rp;
        bool sort_request = 1;
        while( sort_request )
        {
            sort_request = 0;
            for( size_t i = rp; i != ( ( wp - 1 ) & ( MIDI_EVENTS - 1 ) ); i = ( i + 1 ) & ( MIDI_EVENTS - 1 ) )
            {
                device_midi_event_jack* e1 = &d->jack_midi_out_events[ i ];
                device_midi_event_jack* e2 = &d->jack_midi_out_events[ ( i + 1 ) & ( MIDI_EVENTS - 1 ) ];
                if( ( e2->evt.t - e1->evt.t ) & 0x80000000 ) //if e1->evt.t > e2->evt.t
                {
                    device_midi_event_jack e = d->jack_midi_out_events[ i ];
                    d->jack_midi_out_events[ i ] = *e2;
                    d->jack_midi_out_events[ ( i + 1 ) & ( MIDI_EVENTS - 1 ) ] = e;
                    sort_request = 1;
                }
            }
        }
        //Send:
        int latency_ticks = (int)( ( (uint64_t)ss->out_latency * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq );
        int buf_ticks = (int)( ( (uint64_t)nframes * (uint64_t)stime_ticks_per_second() ) / (uint64_t)ss->freq );
        for( ; rp != wp; rp = ( rp + 1 ) & ( MIDI_EVENTS - 1 ) )
        {
            device_midi_event_jack* evt = &d->jack_midi_out_events[ rp ];
            device_midi_port_jack* port = evt->port;
            if( port && port->port )
            {
                if( port->output_time != d->jack_callback_output_time )
                {
                    port->output_time = d->jack_callback_output_time;
                    jack_midi_clear_buffer( jack_port_get_buffer( port->port, d->jack_callback_nframes ) );
                    port->buffer = jack_port_get_buffer( port->port, d->jack_callback_nframes );
                }
                stime_ticks_t evt_t = evt->evt.t + latency_ticks * 2; //output time of this event
                if( ( evt_t - ( d->jack_callback_output_time + buf_ticks ) ) & 0x80000000 ) //if evt_t < ...
                {
                    bool clear_port_later = 1;
                    for( int p = 0; p < d->jack_midi_clear_count; p++ )
                    {
                        if( d->jack_midi_ports_to_clear[ p ] == port )
                        {
                            clear_port_later = 0;
                            break;
                        }
                    }
                    if( clear_port_later )
                    {
                        d->jack_midi_ports_to_clear[ d->jack_midi_clear_count ] = port;
                        d->jack_midi_clear_count++;
                    }
                    jack_nframes_t t;
                    if( ( evt_t - d->jack_callback_output_time ) & 0x80000000 ) //if evt_t < ...
                        t = 0;
                    else
                        t = ( ( evt_t - d->jack_callback_output_time ) * ss->freq ) / stime_ticks_per_second();
                    jack_midi_data_t* midi_data_buffer = jack_midi_event_reserve( port->buffer, t, evt->evt.size );
                    smem_copy( midi_data_buffer, evt->evt.data, evt->evt.size );
                }
                else
                {
                    break;
                }
            }
        }
        //Save pointers:
        d->jack_midi_out_evt_rp = rp;
    }
    
    d->jack_callback_nframes = 0;
    
    return 0;
}

static void jack_shut_down( void* arg )
{
    //sundog_sound* ss = (sundog_sound*)arg;
    //device_sound* d = (device_sound*)ss->device_specific;
}

static void jack_set_default_connections( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    if( sconfig_get_int_value( APP_CFG_JACK_NO_DEF_IN, -1, nullptr ) == -1 )
    {
#ifdef JACK_INPUT
	//JACK OUT PORTS -> APP IN PORTS
	char** physicalOutPorts = (char**)jack_get_ports( d->jack_client, nullptr, nullptr, JackPortIsPhysical | JackPortIsOutput );
	if( physicalOutPorts != nullptr )
	{
    	    for( int i = 0; i < ss->in_channels && physicalOutPorts[ i ]; i++ )
    	    {
    	        jack_connect( d->jack_client, physicalOutPorts[ i ], jack_port_name( d->jack_in_ports[ i ] ) );
    	    }
    	    jack_free( physicalOutPorts );
	}
#endif
    }

    if( sconfig_get_int_value( APP_CFG_JACK_NO_DEF_OUT, -1, nullptr ) == -1 )
    {
	//APP OUT PORTS -> JACK IN PORTS
	char** physicalInPorts = (char**)jack_get_ports( d->jack_client, nullptr, nullptr, JackPortIsPhysical | JackPortIsInput );
	if( physicalInPorts != nullptr )
	{
    	    for( int i = 0; i < ss->out_channels && physicalInPorts[ i ]; i++ )
    	    {
        	jack_connect( d->jack_client, jack_port_name( d->jack_out_ports[ i ] ), physicalInPorts[ i ] );
    	    }
    	    jack_free( physicalInPorts );
	}
    }
}

int device_sound_init_jack( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    g_jack_restore_midiin_con = true;
    if( sconfig_get_int_value( APP_CFG_JACK_DONT_RESTORE_MIDIIN, -1, nullptr ) != -1 )
	g_jack_restore_midiin_con = false;

#ifdef JACK_DYNAMIC_LINK
    //Open library:
    if( !g_jack_lib )
    {
#ifdef OS_LINUX
	g_jack_lib = dlopen( "libjack.so", RTLD_NOW );
	if( !g_jack_lib ) g_jack_lib = dlopen( "libjack.so.0", RTLD_NOW );
#endif
	if( !g_jack_lib )
	{
	    slog( "JACK: Can't open libjack\n" );
	    return -1;
	}
    }
#endif
    
#ifdef OS_IOS
    jack_app_register( JackRegisterDefaults );
#endif

    //Open client:
    jack_status_t status;
    d->jack_client = jack_client_open( g_app_name_short, JackNullOption, &status );
    if( !d->jack_client )
    {
	slog( "JACK: jack_client_open error %x\n", (int)status );
        if( status & JackVersionError )
            slog( "JACK: App not compatible with running JACK version!\n" );
        else
            slog( "JACK: Server app seems not to be running!\n" );
        return -1;
    }

    //Publish client icon:
#ifdef OS_IOS
    jack_publish_client_icon( d->jack_client );
#endif
    
    //Register callback functions:
    jack_set_process_callback( d->jack_client, jack_process_callback, ss );
    jack_on_shutdown( d->jack_client, jack_shut_down, ss );
    
    //Create audio ports:
#ifdef JACK_INPUT
    d->jack_in_ports[ 0 ] = jack_port_register( d->jack_client, "Left In", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
    d->jack_in_ports[ 1 ] = jack_port_register( d->jack_client, "Right In", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
#endif
    d->jack_out_ports[ 0 ] = jack_port_register( d->jack_client, "Left Out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
    d->jack_out_ports[ 1 ] = jack_port_register( d->jack_client, "Right Out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
    
    //Get JACK parameters:
    ss->freq = jack_get_sample_rate( d->jack_client );
#ifdef ONLY44100
    if( ss->freq != 44100 )
    {
        slog( "JACK: Can't set sample rate %d. Only 44100 supported in this version.\n", ss->freq );
        jack_client_close( d->jack_client );
        d->jack_client = nullptr;
        return -1;
    }
#endif
#ifdef MIN_SAMPLE_RATE
    if( ss->freq < MIN_SAMPLE_RATE )
    {
        slog( "JACK: Can't set sample rate %d. Minimum %d supported.\n", ss->freq, MIN_SAMPLE_RATE );
        jack_client_close( d->jack_client );
        d->jack_client = nullptr;
        return -1;
    }
#endif
    
    //Reset MIDI:
    d->jack_midi_clear_count = 0;

#ifndef OS_IOS
#if CPUMARK >= 10
    //Set 32bit mode:
    ss->out_type = sound_buffer_float32;
    ss->in_type = sound_buffer_float32;
#endif
#endif
    
    //Activate client:
    if( jack_activate( d->jack_client ) )
    {
        slog( "JACK: Cannot activate client.\n" );
        jack_client_close( d->jack_client );
        d->jack_client = nullptr;
        return -1;
    }
    
    //Set default connections:
    jack_set_default_connections( ss );
    
    return 0;
}

void device_sound_deinit_jack( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;

    jack_client_close( d->jack_client );
#ifdef JACK_INPUT
    smem_free( d->jack_temp_input );
    d->jack_temp_input = nullptr;
#endif
    smem_free( d->jack_temp_output );
    d->jack_client = nullptr;
    d->jack_temp_output = nullptr;
}

//
// MIDI
//

int device_midi_client_open_jack( sundog_midi_client* c, const char* name )
{
    if( !c->ss || !c->ss->initialized )
    {
	slog( "JACK: MIDI client can not be opened if the sound stream is not specified / not initialized.\n" );
	return -1;
    }
    return 0;
}

int device_midi_client_close_jack( sundog_midi_client* c )
{
    return 0;
}

int device_midi_client_get_devices_jack( sundog_midi_client* c, char*** devices, uint32_t flags )
{
    *devices = (char**)smem_new( sizeof( void* ) * 1 );
    (*devices)[ 0 ] = smem_strdup( "JACK" );
    return 1;
}

int device_midi_client_open_port_jack( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags )
{
    sundog_sound* ss = c->ss; if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    device_sound* ss_d = (device_sound*)ss->device_specific;

    if( !ss_d->jack_client ) return -1;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    sd_port->device_specific = smem_new( sizeof( device_midi_port_jack ) );
    device_midi_port_jack* port = (device_midi_port_jack*)sd_port->device_specific;
    if( !port ) return -1;
    smem_zero( port );
    if( flags & MIDI_PORT_READ )
    {
        port->port = jack_port_register( ss_d->jack_client, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0 );
	if( g_jack_restore_midiin_con )
	{
	    if( c->name && port_name ) //RESTORE CONNECTIONS
	    {
		char* fname = (char*)smem_new( 4096 );
		char* con_name = (char*)smem_new( 4096 );
		sprintf( fname, "2:/.sundog_jackmidi_%s_%s", c->name, port_name );
		sfs_file f = sfs_open( fname, "rb" );
		if( f )
		{
		    int i = 0;
		    while( 1 )
		    {
			int cc = sfs_getc( f );
			if( cc < 0 ) break;
			con_name[ i ] = cc;
			i++;
			if( cc == 0 )
			{
			    slog( "JACK: restoring previous connection %s -> %s ...\n", con_name, jack_port_name( port->port ) );
			    jack_connect( ss_d->jack_client, con_name, jack_port_name( port->port ) );
			    i = 0;
			}
		    }
		    sfs_close( f );
		}
    		smem_free( fname );
    		smem_free( con_name );
	    }
	}
    }
    else
        port->port = jack_port_register( ss_d->jack_client, port_name, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0 );
    if( !port->port ) return 0;
    
    return 0;
}

int device_midi_client_close_port_jack( sundog_midi_client* c, int pnum )
{
    sundog_sound* ss = c->ss; if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    device_sound* ss_d = (device_sound*)ss->device_specific;

    if( !ss_d->jack_client ) return -1;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port_jack* port = (device_midi_port_jack*)sd_port->device_specific;
    if( !port ) return 0;
    
    uint pflags = jack_port_flags( port->port );
    if( pflags & JackPortIsInput )
    {
	if( g_jack_restore_midiin_con )
	{
	    if( c->name && sd_port->port_name ) //SAVE CONNECTIONS
	    {
		char* fname = (char*)smem_new( 4096 );
		sprintf( fname, "2:/.sundog_jackmidi_%s_%s", c->name, sd_port->port_name );
		const char** con = jack_port_get_connections( port->port );
		if( con )
		{
		    sfs_file f = sfs_open( fname, "wb" );
	    	    if( f )
		    {
			for( int i = 0; ; i++ )
			{
			    const char* con_name = con[ i ];
			    if( !con_name ) break;
			    sfs_write( con_name, 1, smem_strlen( con_name ) + 1, f );
			}
			sfs_close( f );
		    }
    		    jack_free( con );
    		}
    		else
    		{
    		    sfs_remove_file( fname );
    		}
    		smem_free( fname );
    	    }
	}
    }
    
    //Remove this port from the output queue:
    sundog_sound_lock( ss );
    for( int p = 0; p < ss_d->jack_midi_clear_count; p++ )
    {
        if( ss_d->jack_midi_ports_to_clear[ p ] == port )
            ss_d->jack_midi_ports_to_clear[ p ] = nullptr;
    }
    size_t wp = ss_d->jack_midi_out_evt_wp;
    size_t rp = ss_d->jack_midi_out_evt_rp;
    for( ; rp != wp; rp = ( rp + 1 ) & ( MIDI_EVENTS - 1 ) )
    {
        device_midi_event_jack* evt = &ss_d->jack_midi_out_events[ rp ];
        if( evt->port == port ) evt->port = nullptr;
    }
    sundog_sound_unlock( ss );
    
    jack_port_unregister( ss_d->jack_client, port->port );

    smem_free( port->r_data );
    smem_free( port->r_events );    
    smem_free( sd_port->device_specific );
    sd_port->device_specific = nullptr;
    return 0;
}

sundog_midi_event* device_midi_client_get_event_jack( sundog_midi_client* c, int pnum )
{
    sundog_sound* ss = c->ss; if( !ss ) return 0;
    if( !ss->initialized ) return 0;
    device_sound* ss_d = (device_sound*)ss->device_specific;

    if( !ss_d->jack_client ) return 0;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port_jack* port = (device_midi_port_jack*)sd_port->device_specific;
    if( !port ) return 0;
    
    if( ss_d->jack_callback_nframes == 0 )
    {
	slog( "JACK: can't receive MIDI events outside of the sound callback.\n" );
	return nullptr;
    }
    
    if( port->output_time != ss_d->jack_callback_output_time )
    {
        port->output_time = ss_d->jack_callback_output_time;
        port->buffer = jack_port_get_buffer( port->port, ss_d->jack_callback_nframes );
    	//Copy all incoming events to the r_data and r_events buffers:
	int event_count = jack_midi_get_event_count( port->buffer );
	if( event_count )
	{
            c->last_midi_in_activity = stime_ticks();
    	    if( !port->r_data )
	        port->r_data = (uint8_t*)smem_new( MIDI_BYTES );
    	    if( !port->r_events )
    		port->r_events = (sundog_midi_event*)smem_new( MIDI_EVENTS * sizeof( sundog_midi_event ) );
	    for( int i = 0; i < event_count; i++ )
	    {
		//Save data:
    		jack_midi_event_t ev;
    		jack_midi_event_get( &ev, port->buffer, i );
    		if( !ev.buffer ) continue;
		if( port->r_data_wp + ev.size > MIDI_BYTES )
    		    port->r_data_wp = 0;
		if( port->r_data_wp + ev.size > MIDI_BYTES )
		{
    		    //No free space:
    		    continue;
		}
		size_t data_p = port->r_data_wp;
		smem_copy( port->r_data + data_p, ev.buffer, ev.size );
		port->r_data_wp += ev.size;
		//Save event:
		size_t wp = port->r_events_wp;
		if( wp == ( ( port->r_events_rp - 1 ) & ( MIDI_EVENTS - 1 ) ) )
		{
		    //No free space:
    		    port->r_data_wp -= ev.size;
    		    continue;
		}
		sundog_midi_event* e = &port->r_events[ wp ];
        	jack_nframes_t t = ( ev.time * stime_ticks_per_second() ) / ss->freq; //res = ticks
        	e->t = port->output_time + t;
        	e->t -= (int)( ( (uint64_t)SUNDOG_MIDI_LATENCY( ss->out_latency, ss->out_latency2 ) * stime_ticks_per_second() ) / (uint64_t)ss->freq );
		e->size = ev.size;
		e->data = port->r_data + data_p;
		wp = ( wp + 1 ) & ( MIDI_EVENTS - 1 );
		COMPILER_MEMORY_BARRIER();
		port->r_events_wp = wp;
	    }
	}
    }
    
    if( port->r_events_rp != port->r_events_wp )
    {
	return &port->r_events[ port->r_events_rp ];
    }
    
    return 0;
}

int device_midi_client_next_event_jack( sundog_midi_client* c, int pnum )
{
    sundog_sound* ss = c->ss; if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    device_sound* ss_d = (device_sound*)ss->device_specific;

    if( !ss_d->jack_client ) return 0;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port_jack* port = (device_midi_port_jack*)sd_port->device_specific;
    if( !port ) return 0;
    
    if( port->r_events_rp != port->r_events_wp )
    {
	port->r_events_rp = ( port->r_events_rp + 1 ) & ( MIDI_EVENTS - 1 );
    }
    
    return 0;
}

int device_midi_client_send_event_jack( sundog_midi_client* c, int pnum, sundog_midi_event* evt )
{
    sundog_sound* ss = c->ss; if( !ss ) return -1;
    if( !ss->initialized ) return -1;
    device_sound* ss_d = (device_sound*)ss->device_specific;

    if( !ss_d->jack_client ) return 0;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port_jack* port = (device_midi_port_jack*)sd_port->device_specific;
    
    c->last_midi_out_activity = stime_ticks();
    
    //Save data:
    if( ss_d->jack_midi_out_data_wp + evt->size > MIDI_BYTES )
        ss_d->jack_midi_out_data_wp = 0;
    if( ss_d->jack_midi_out_data_wp + evt->size > MIDI_BYTES )
    {
        //No free space:
        return -1;
    }
    size_t data_p = ss_d->jack_midi_out_data_wp;
    smem_copy( ss_d->jack_midi_out_data + ss_d->jack_midi_out_data_wp, evt->data, evt->size );
    ss_d->jack_midi_out_data_wp += evt->size;
    
    //Save event:
    size_t wp = ss_d->jack_midi_out_evt_wp;
    if( wp == ( ( ss_d->jack_midi_out_evt_rp - 1 ) & ( MIDI_EVENTS - 1 ) ) )
    {
        //No free space:
        ss_d->jack_midi_out_data_wp -= evt->size;
        return -1;
    }
    device_midi_event_jack* e = &ss_d->jack_midi_out_events[ wp ];
    e->port = port;
    e->evt.t = evt->t;
    e->evt.size = evt->size;
    e->evt.data = ss_d->jack_midi_out_data + data_p;
    wp = ( wp + 1 ) & ( MIDI_EVENTS - 1 );
    COMPILER_MEMORY_BARRIER();
    ss_d->jack_midi_out_evt_wp = wp;
    
    return 0;
}

#endif //...COMMON_CODE

#endif //...JACK_AUDIO
