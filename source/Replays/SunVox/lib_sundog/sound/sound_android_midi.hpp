//
// Android: MIDI
//

//LIMITATIONS:
//Android MIDI port can't be opened twice -> SunDog MIDI device can't be opened twice for different ports;
//multiport code must be added (like in sound_win_midi.hpp)...
//In future this code must be optimized for the Native MIDI API (AMidi, Android 10+).

#ifndef NOMIDI

#include "main/android/sundog_bridge.h"

enum
{
    MDRIVER_JAVAMIDI,
    NUMBER_OF_MDRIVERS
};
#define DEFAULT_MDRIVER 0

const char* g_mdriver_infos[] =
{
    "Java MIDI",
};

const char* g_mdriver_names[] =
{
    "javamidi",
};

struct device_midi_client
{
    int client_index;
};

struct device_midi_port
{
    int con_index; //connection index
    stime_ticks_t prev_timestamp; //0 - unknown

    COMMON_MIDIPORT_VARIABLES;
};

//port_id = rw | ( client_index << 8 ) | ( port_index << 16 )

const int MAX_MIDI_CLIENTS = 4;
sundog_midi_client* g_midi_clients[ MAX_MIDI_CLIENTS ];

extern "C" JNIEXPORT jint JNICALL Java_nightradio_androidlib_AndroidLib_midi_1receiver_1callback( 
    JNIEnv* je, jclass jc, jint port_id, jbyteArray data_array, jint offset, jint count, jlong timestamp )
{
    //timestamp - relative to NOW. Must be 0 or negative!
    stime_ticks_t now = stime_ticks();

    int client_index = ( port_id >> 8 ) & 255;
    int port_index = ( port_id >> 16 ) & 255;
    if( (unsigned)client_index >= (unsigned)MAX_MIDI_CLIENTS ) return 0;
    if( (unsigned)port_index >= (unsigned)SUNDOG_MIDI_PORTS ) return 0;

    sundog_midi_client* c = g_midi_clients[ client_index ];
    if( !c ) return 0;

    c->last_midi_in_activity = now;

    if( port_index >= c->ports_cnt ) return 0;
    sundog_midi_port* sd_port = c->ports[ port_index ];
    if( !sd_port ) return 0;
    if( !sd_port->active ) return 0;
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    jbyte* data = je->GetByteArrayElements( data_array, NULL );
    if( data )
    {
	//LOGI( "%f ms", (float)timestamp / ( 1000000000 / 1000 ) );
	//char ts[ 64 ]; for( int i = 0; i < count; i++ ) sprintf( ts + i * 3, "%02x ", ((uint8_t*)data + offset)[i] ); LOGI( "%s", ts );
	stime_ticks_t t = 0;
	if( timestamp <= 0 )
	{
	    //if( timestamp < -1000000000 ) LOGI( "Too large timestamp %d", (int)timestamp );
	    t = now + timestamp / ( 1000000000 / (jlong)stime_ticks_per_second() );
	    if( port->prev_timestamp )
	    {
		if( (signed)( t - port->prev_timestamp ) < 0 )
		{
		    //LOGI( "Wrong timestamp delta %d", t - port->prev_timestamp );
		    t = port->prev_timestamp;
		}
	    }
	    port->prev_timestamp = t;
	}
	//else LOGI( "Wrong timestamp %d", (int)timestamp );
	write_received_midi_data( port, (uint8_t*)data + offset, count, t );
	je->ReleaseByteArrayElements( data_array, data, 0 );
    }

    return 0;
}

int device_midi_client_open( sundog_midi_client* c, const char* name )
{
    int rv = 0;
    c->device_specific = smem_new( sizeof( device_midi_client ) );
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;
    smem_zero( d );

    while( 1 )
    {
        //Open client:
        if( c->sd )
    	    rv = android_sundog_midi_init( c->sd );
    	else
    	    rv = -1;
        if( rv ) break;

	for( int i = 0; i < MAX_MIDI_CLIENTS; i++ )
	{
	    if( g_midi_clients[ i ] == NULL )
	    {
		g_midi_clients[ i ] = c;
		d->client_index = i;
		break;
	    }
	}

	break;
    }

    if( rv )
    {
        slog( "device_midi_client_open error %d\n", rv );
	REMOVE_DEVICE_SPECIFIC_DATA( c );
    }

    return rv;
}

int device_midi_client_close( sundog_midi_client* c )
{
    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    if( (unsigned)d->client_index < (unsigned)MAX_MIDI_CLIENTS && g_midi_clients[ d->client_index ] == c )
    {
        g_midi_clients[ d->client_index ] = NULL;
    }

    REMOVE_DEVICE_SPECIFIC_DATA( c );

    return rv;
}

int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags )
{
    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return 0;

    *devices = NULL;

    char* devs = NULL;
    if( c->sd ) devs = android_sundog_get_midi_devports( c->sd, flags & 3 );
    if( devs )
    {
	if( strlen( devs ) > 1 )
	{
	    const char* next = devs;
	    char dev_name[ 256 ];
	    dev_name[ 0 ] = 0;
	    while( 1 )
	    {
		if( !next ) break;
		next = smem_split_str( dev_name, sizeof( dev_name ), next, '\n', 0 );
		if( dev_name[ 0 ] == 0 ) break;
		smem_objarray_write_d( (void***)devices, smem_strdup( dev_name ), false, rv );
		rv++;
	    }
	}
	free( devs );
    }

    return rv;
}

int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags )
{
    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    sd_port->device_specific = smem_znew( sizeof( device_midi_port ) );
    if( !sd_port->device_specific ) return -1;
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
#ifndef NOMIDI
    init_common_midiport_vars( port );
#endif

    while( 1 )
    {
	int rw = ( flags & MIDI_PORT_WRITE ) != 0;
	int port_id = rw | ( d->client_index << 8 ) | ( pnum << 16 );
	//slog( "MIDI open port %s\n", dev_name );
	if( c->sd ) port->con_index = android_sundog_midi_open_devport( c->sd, dev_name, port_id );
	//slog( "MIDI open port ok; con_index = %d\n", port->con_index );

	break;
    }

    return rv;
}

int device_midi_client_close_port( sundog_midi_client* c, int pnum )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    while( 1 )
    {
	//slog( "MIDI close port %s; con_index = %d\n", sd_port->dev_name, port->con_index );
	if( c->sd ) android_sundog_midi_close_devport( c->sd, port->con_index );
	//slog( "MIDI close port ok\n" );

	break;
    }

    smem_free( sd_port->device_specific );
    sd_port->device_specific = NULL;
    return 0;
}

sundog_midi_event* device_midi_client_get_event( sundog_midi_client* c, int pnum )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return NULL;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return NULL;

    return get_midi_event( port );
}

int device_midi_client_next_event( sundog_midi_client* c, int pnum )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    next_midi_event( port );

    return 0;
}

int device_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;
    if( !c->sd ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    stime_ticks_t now = stime_ticks();
    c->last_midi_out_activity = now;

    stime_ticks_t t = evt->t - now;
    t *= 1000000000 / stime_ticks_per_second();

    for( int i = 0; i < (int)evt->size; i += 4 )
    {
	int w = evt->size - i;
	int w2 = w;
	if( w > 4 ) { w = 4; w2 = 5; }
	uint32_t msg = 0;
	for( int i2 = 0; i2 < w; i2++ )
	{
	    msg |= ( (uint32_t)evt->data[ i + i2 ] << ( i2 * 8 ) );
	}
	android_sundog_midi_send( c->sd, port->con_index, msg, w2, t );
    }

    return 0;
}

#endif
