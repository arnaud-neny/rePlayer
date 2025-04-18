//
// MacOS and iOS: MIDI (CoreMIDI and JACK)
//

#include <CoreMIDI/CoreMIDI.h>

#ifndef kCFCoreFoundationVersionNumber_iPhoneOS_4_2
    // NOTE: THIS IS NOT THE FINAL NUMBER
    // 4.2 is not out of beta yet, so took the beta 1 build number
    #define kCFCoreFoundationVersionNumber_iPhoneOS_4_2 550.47
#endif

#ifndef NOMIDI

enum
{
    MDRIVER_COREMIDI,
#ifdef JACK_AUDIO
    MDRIVER_JACK,
#endif
    NUMBER_OF_MDRIVERS
};
#define DEFAULT_MDRIVER 0

const char* g_mdriver_infos[] =
{
    "Core MIDI",
#ifdef JACK_AUDIO
    "JACK",
#endif
};

const char* g_mdriver_names[] =
{
    "coremidi",
#ifdef JACK_AUDIO
    "jack",
#endif
};

struct device_midi_client
{
    MIDIClientRef 		client;
    CFRunLoopTimerRef           notify_timer;
    CFRunLoopTimerContext       notify_timer_ctx;
    uint32_t                    notify_cmds_r;
};

enum endpoint_type_enum
{
    ENDPOINT_VIRTUAL_SRC, //MIDI source in the client, created with MIDISourceCreate() for the "public port"
    ENDPOINT_VIRTUAL_DEST, //MIDI destination in the client, created with MIDIDestinationCreate() for the "public port"
    ENDPOINT_SRC, //MIDI source in the system
    ENDPOINT_DEST, //MIDI destination in the system
};

struct device_midi_port
{
    sundog_midi_client*		c;
    MIDIPortRef 		port;
    MIDIEndpointRef 		endpoint; //Source or destination
    endpoint_type_enum		endpoint_type;
#ifdef AUDIOBUS_MIDI_OUT
    int				audiobus_out; //1,2,3...; 0 - not connected
#endif

    COMMON_MIDIPORT_VARIABLES;
};

static bool is_private( MIDIEndpointRef endpoint )
{
    OSStatus result;
    SInt32 isPrivate;
    result = MIDIObjectGetIntegerProperty( endpoint, kMIDIPropertyPrivate, &isPrivate );
    if( result == noErr )
	return isPrivate != 0;
    else
	return false;
}

static void midi_port_receive( device_midi_port* port, const MIDIPacketList* packetList )
{
    if( !port ) return;

    port->c->last_midi_in_activity = stime_ticks();

    MIDIPacket* packet = (MIDIPacket*)packetList->packet;
    uint count = packetList->numPackets;
    for( uint i = 0; i < count; i++ )
    {
	stime_ticks_t t; //the time at which the events occurred
	if( packet->timeStamp == 0 )
	{
	    //Now (empty timestamp):
	    t = stime_ticks();
	}
	else
	{
	    //A little before the current time, or in future:
	    t = convert_mach_absolute_time_to_sundog_ticks( packet->timeStamp, 1 );
	}
	write_received_midi_data( port, packet->data, packet->length, t );
	packet = MIDIPacketNext( packet );
    }
}

void midi_in_callback( const MIDIPacketList* packetList, void* readProcRefCon, void* srcConnRefCon )
{
#ifdef AUDIOBUS_MIDI_IN
    if( g_ab_disable_coremidi_in ) return;
#endif

    sundog_midi_client* c = (sundog_midi_client*)readProcRefCon;
    if( !c ) return;
    size_t pnum = (size_t)srcConnRefCon;
    sundog_midi_port* sd_port = c->ports[ pnum ];
    if( !sd_port ) return;
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return;

    midi_port_receive( port, packetList );
}

//Special callback for the virtual destination (we can't set srcConnRefCon for it):
void midi_in_virtualdest_callback( const MIDIPacketList* packetList, void* readProcRefCon, void* srcConnRefCon )
{
#ifdef AUDIOBUS_MIDI_IN
    if( g_ab_disable_coremidi_in ) return;
#endif

    device_midi_port* port = (device_midi_port*)readProcRefCon;
    if( !port ) return;

    midi_port_receive( port, packetList );
}

void midi_notify_timer( CFRunLoopTimerRef timer, void* info )
{
    sundog_midi_client* c = (sundog_midi_client*)info;
    if( !c ) return;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return;
    sundog_sound* ss = c->ss;
    if( !ss ) return;
    sundog_engine* sd = c->sd;
    if( !sd ) return;

    while( sd->midi_notify_cmds_w != d->notify_cmds_r )
    {
        printf( "MIDI notify timer W:%d R:%d\n", sd->midi_notify_cmds_w, d->notify_cmds_r );
        if( smutex_trylock( &c->ports_mutex ) ) break;
        switch( sd->midi_notify_cmds[ d->notify_cmds_r ].cmd )
        {
            case kMIDIMsgObjectRemoved:
                printf( "MIDI Object Removed\n" );
                for( int i = 0; i < SUNDOG_MIDI_PORTS; i++ )
                {
                    sundog_midi_port* sd_port = c->ports[ i ];
                    if( !sd_port ) continue;
                    if( sd_port->active == 0 ) continue;
                    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
                    if( !port ) continue;
                    if( smem_strcmp( sd_port->dev_name, "public port" ) == 0 ) continue;
                    if( port->endpoint == sd->midi_notify_cmds[ d->notify_cmds_r ].arg )
                    {
                        printf( " * port %d: %s\n", i, sd_port->dev_name );
                        device_midi_client_close_port( c, i );
                        sd_port->flags |= MIDI_NO_DEVICE;
                    }
                }
                break;
            case kMIDIMsgObjectAdded:
                printf( "MIDI Object Added\n" );
                for( int i = 0; i < SUNDOG_MIDI_PORTS; i++ )
                {
                    sundog_midi_port* sd_port = c->ports[ i ];
                    if( !sd_port ) continue;
                    if( sd_port->active == 0 ) continue;
                    if( sd_port->flags & MIDI_NO_DEVICE )
                    {
                        printf( " * port %d: %s. Trying to reopen...\n", i, sd_port->dev_name );
                        sundog_midi_client_reopen_port( c, i );
                    }
                }
                break;
        }
        smutex_unlock( &c->ports_mutex );
        d->notify_cmds_r = ( d->notify_cmds_r + 1 ) & ( MIDI_NOTIFY_CMDS - 1 );
    }
}

void midi_notify_callback( const MIDINotification* message, void* refCon )
{
    sundog_engine* sd = (sundog_engine*)refCon;
    uint32_t w = sd->midi_notify_cmds_w;
    switch( message->messageID )
    {
	case kMIDIMsgSetupChanged:
	    //printf( "MIDI Setup Changed\n" );
	    break;
	case kMIDIMsgObjectRemoved:
	    {
		//printf( "MIDI Object Removed\n" );
		MIDIObjectAddRemoveNotification* msg = (MIDIObjectAddRemoveNotification*)message;

                sd->midi_notify_cmds[ w ].arg = msg->child;
                sd->midi_notify_cmds[ w ].cmd = message->messageID;
                COMPILER_MEMORY_BARRIER();
                sd->midi_notify_cmds_w = ( w + 1 ) & ( MIDI_NOTIFY_CMDS - 1 );
	    }
	    break;
	case kMIDIMsgObjectAdded:
	    {
		//printf( "MIDI Object Added\n" );

                sd->midi_notify_cmds[ w ].cmd = message->messageID;
                COMPILER_MEMORY_BARRIER();
                sd->midi_notify_cmds_w = ( w + 1 ) & ( MIDI_NOTIFY_CMDS - 1 );
	    }
	    break;
	default:
	    //printf( "MIDI Notify %d\n", (int)message->messageID );
	    break;
    }
}

CFStringRef device_midi_get_endpoint_name( MIDIEndpointRef endpoint )
{
    CFMutableStringRef name = CFStringCreateMutable( NULL, 0 );
    CFStringRef ts;

    MIDIEntityRef entity = 0;
    MIDIEndpointGetEntity( endpoint, &entity );
    
    MIDIDeviceRef device = 0;
    MIDIEntityGetDevice( entity, &device );
    
    bool text = 0;
    
    if( device )
    {
	ts = 0;
	MIDIObjectGetStringProperty( device, kMIDIPropertyName, &ts );
	if( ts )
	{
	    text = 1;
	    CFStringAppend( name, ts );
	    CFRelease( ts );
	}
    }
    
    if( entity )
    {
	ts = 0;
	MIDIObjectGetStringProperty( entity, kMIDIPropertyName, &ts );
	if( ts )
	{
	    bool ignore = 0;
	    if( CFStringGetLength( name ) > 0 )
	    {
		if( CFStringCompare( name, ts, 0 ) == 0 )
		{
		    ignore = 1;
		}
	    }
	    if( ignore == 0 )
	    {
		if( text )
		{
		    CFStringAppend( name, CFSTR( " " ) );
		}
		text = 1;
		CFStringAppend( name, ts );
	    }
	    CFRelease( ts );
	}
    }
    
    ts = 0;
    MIDIObjectGetStringProperty( endpoint, kMIDIPropertyName, &ts );
    if( ts ) 
    {
	if( text )
	{
	    CFStringAppend( name, CFSTR( " " ) );
	}
	text = 1;
	CFStringAppend( name, ts );
	CFRelease( ts );
    }
    
    if( CFStringGetLength( name ) == 0 )
    {
	CFStringAppend( name, CFSTR( "Unnamed" ) );
    }

    return name;
}

static std::atomic_int g_midi_create_counter;

int device_midi_client_open( sundog_midi_client* c, const char* name )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_open_jack( c, name );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    int rv = 0;
    c->device_specific = smem_new( sizeof( device_midi_client ) );
    device_midi_client* d = (device_midi_client*)c->device_specific;
    smem_zero( d );
    
    int prev_cnt = g_midi_create_counter.fetch_add( 1 );
    if( prev_cnt == 0 )
    {
        //The first MIDI client in the process:
        //Create a background client that will prevent the MIDI server from exiting:
        MIDIClientRef empty_client = 0;
        OSStatus status = MIDIClientCreate( CFStringCreateWithCString( NULL, "SunDog empty MIDI client", kCFStringEncodingUTF8 ), nullptr, nullptr, &empty_client ); //without MIDIClientDispose()!
        if( status )
        {
            slog( "Error trying to create the first MIDI Client: %d\n", status );
        }
    }
   
    while( 1 )
    {
        void* notifyRefCon = NULL;
        if( c->sd ) notifyRefCon = c->sd;
	OSStatus status = MIDIClientCreate( CFStringCreateWithCString( NULL, name, kCFStringEncodingUTF8 ), &midi_notify_callback, notifyRefCon, &d->client );
	if( status )
	{
	    slog( "Error trying to create MIDI Client structure: %d\n", status );
            if( status == -50 )
            {
                //macOS/iOS bug?
                //outClient is NULL or the MIDI server has an issue getting a process ID back internally
                //see MIDIClientDispose() description...
            }
	    rv = -1;
	    break;
	}
        //notifyProc will always be called on the run loop which was current when MIDIClientCreate was first called.
        //If AudioBus enabled, the first call is in the main app thread, not in the SunDog thread!
        //We can get something like this:
        //  thread 1: [client_open]    [client_close]
        //  thread 2:                [notifyProc]      [notifyProc] (client data is invalid here!)
	//So we will forward messages from notifyProc to the timer that only works between client_open() and client_close():
        if( c->sd )
        {
            d->notify_cmds_r = c->sd->midi_notify_cmds_w;
            d->notify_timer_ctx.info = c;
            d->notify_timer = CFRunLoopTimerCreate( NULL, CFAbsoluteTimeGetCurrent() + 1, 1, 0, 0, midi_notify_timer, &d->notify_timer_ctx );
            CFRunLoopAddTimer( CFRunLoopGetMain(), d->notify_timer, kCFRunLoopCommonModes );
        }

	break;
    }

    if( rv )
    {
        REMOVE_DEVICE_SPECIFIC_DATA( c );
    }

    return rv;
}

int device_midi_client_close( sundog_midi_client* c )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
	return device_midi_client_close_jack( c );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    if( d->notify_timer )
    {
        CFRunLoopRemoveTimer( CFRunLoopGetMain(), d->notify_timer, kCFRunLoopCommonModes );
        CFRelease( d->notify_timer );
    }
    MIDIClientDispose( d->client );
    /*
     From Apple docs:
     Don’t explicitly dispose of your client; the system automatically disposes all clients when an app terminates.
     However, if you call this method to dispose the last or only client owned by an app,
     the MIDI server may exit if there are no other clients remaining in the system.
     If this occurs, all subsequent calls by your app to MIDIClientCreate and MIDIClientCreateWithBlock fail.
     */

    REMOVE_DEVICE_SPECIFIC_DATA( c );

    return rv;
}

int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_get_devices_jack( c, devices, flags );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return 0;

    HANDLE_THREAD_EVENTS; //handle MIDI notifications to get the correct list of sources/destinations; required if you are not in the main thread;

    *devices = NULL;

    CFStringRef name;
    const uint name_size = 2048;
    char* name_utf8 = (char*)smem_new( name_size );
    int sources = (int)MIDIGetNumberOfSources();
    int dests = (int)MIDIGetNumberOfDestinations();
    if( flags & MIDI_PORT_READ )
    {
	for( int j = 0; j < sources; j++ )
	{
	    MIDIEndpointRef source = MIDIGetSource( j );
	    if( source )
	    {
		if( is_private( source ) ) continue;
		name = 0;
		name = device_midi_get_endpoint_name( source );
		if( !name ) continue;
		CFStringGetCString( name, name_utf8, name_size, kCFStringEncodingUTF8 );
		CFRelease( name );
		smem_objarray_write_d( (void***)devices, name_utf8, true, rv );
		rv++;
	    }
	}
    }
    if( flags & MIDI_PORT_WRITE )
    {
	for( int j = 0; j < dests; j++ )
	{
	    MIDIEndpointRef dest = MIDIGetDestination( j );
	    if( dest )
	    {
		if( is_private( dest ) ) continue;
		name = 0;
		name = device_midi_get_endpoint_name( dest );
		if( !name ) continue;
		CFStringGetCString( name, name_utf8, name_size, kCFStringEncodingUTF8 );
		CFRelease( name );
		smem_objarray_write_d( (void***)devices, name_utf8, true, rv );
		rv++;
	    }
	}
#ifdef AUDIOBUS_MIDI_OUT
	for( int i = 0; i < AUDIOBUS_MIDI_OUT; i++ )
	{
	    smem_objarray_write_d( (void***)devices, (void*)g_ab_midi_out_names2[ i ], true, rv );
	    rv++;
	}
#endif
    }
    smem_free( name_utf8 );

    return rv;
}

int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_open_port_jack( c, pnum, port_name, dev_name, flags );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    int rv = -1;
    OSStatus status;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    sd_port->device_specific = smem_znew( sizeof( device_midi_port ) );
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    port->c = c;
#ifndef NOMIDI
    init_common_midiport_vars( port );
#endif

    HANDLE_THREAD_EVENTS;

    if( flags & MIDI_PORT_READ )
	status = MIDIInputPortCreate( d->client, CFStringCreateWithCString( NULL, port_name, kCFStringEncodingUTF8 ), midi_in_callback, c, &port->port );
    else
	status = MIDIOutputPortCreate( d->client, CFStringCreateWithCString( NULL, port_name, kCFStringEncodingUTF8 ), &port->port );
    if( status )
    {
	slog( "Error trying to create MIDI port: %d\n", status );
	goto open_end;
    }
    port->endpoint = 0;
    if( smem_strcmp( dev_name, "public port" ) == 0 )
    {
        //Connect this port to the public port (visible to others):
        char* ts = (char*)smem_new( smem_strlen( port_name ) + 128 );
        if( flags & MIDI_PORT_READ )
        {
#ifdef AUDIOBUS_MIDI_IN
    	    if( audiobus_add_midiin_callback( midi_port_receive, port ) == 0 )
    	    {
    		rv = 0;
    	    }
#endif
            sprintf( ts, "%s", port_name );
            int idd = 'SDIn';
            for( int i = 0; i < smem_strlen( ts ); i++ ) { idd += ts[ i ]; }
            MIDIEndpointRef dest;
            status = MIDIDestinationCreate( d->client, CFStringCreateWithCString( NULL, ts, kCFStringEncodingUTF8 ), midi_in_virtualdest_callback, port, &dest );
            if( status )
            {
                slog( "Error trying to create MIDI Virtual Input: %d\n", status );
		port->endpoint = 0;
            }
            else
            {
                MIDIObjectSetIntegerProperty( dest, kMIDIPropertyUniqueID, idd );
		rv = 0;
		port->endpoint = dest;
		port->endpoint_type = ENDPOINT_VIRTUAL_DEST;
            }
        }
        else
        {
            sprintf( ts, "%s", port_name );
            int idd = 'SDOu';
            for( int i = 0; i < smem_strlen( ts ); i++ ) { idd += ts[ i ]; }
            MIDIEndpointRef source;
            status = MIDISourceCreate( d->client, CFStringCreateWithCString( NULL, ts, kCFStringEncodingUTF8 ), &source );
            if( status )
            {
                slog( "Error trying to create MIDI Virtual Output: %d\n", status );
            }
            else
            {
                MIDIObjectSetIntegerProperty( source, kMIDIPropertyUniqueID, idd );
                rv = 0;
                port->endpoint = source;
		port->endpoint_type = ENDPOINT_VIRTUAL_SRC;
            }
        }
        smem_free( ts );
    }
    else
    {
        //Connect this port to selected source or destination:
	CFStringRef name;
	char name_utf8[ 512 ];
	size_t sources = MIDIGetNumberOfSources();
	size_t dests = MIDIGetNumberOfDestinations();
	if( flags & MIDI_PORT_READ )
	{
	    for( size_t j = 0; j < sources; j++ )
	    {
		MIDIEndpointRef source = MIDIGetSource( j );
		if( source )
		{
		    name = 0;
		    name = device_midi_get_endpoint_name( source );
		    if( !name ) continue;
		    CFStringGetCString( name, name_utf8, 512, kCFStringEncodingUTF8 );
		    CFRelease( name );
		    if( smem_strcmp( name_utf8, dev_name ) == 0 )
		    {
			status = MIDIPortConnectSource( port->port, source, (void*)((size_t)pnum) );
			if( status )
			{
			    slog( "Error trying to connect source: %d\n", status );
			    goto open_end;
			}
			rv = 0;
			port->endpoint = source;
			port->endpoint_type = ENDPOINT_SRC;
			break;
		    }
		}
	    }
	}
	else
	{
	    for( size_t j = 0; j < dests; j++ )
	    {
		MIDIEndpointRef dest = MIDIGetDestination( j );
		if( dest )
		{
		    name = 0;
		    name = device_midi_get_endpoint_name( dest );
		    if( !name ) continue;
		    CFStringGetCString( name, name_utf8, 512, kCFStringEncodingUTF8 );
		    CFRelease( name );
		    if( smem_strcmp( name_utf8, dev_name ) == 0 )
		    {
			rv = 0;
			port->endpoint = dest;
			port->endpoint_type = ENDPOINT_DEST;
			break;
		    }
		}
	    }
#ifdef AUDIOBUS_MIDI_OUT
	    if( rv != 0 )
	    {
		for( int i = 0; i < AUDIOBUS_MIDI_OUT; i++ )
		{
		    if( smem_strcmp( g_ab_midi_out_names2[ i ], dev_name ) == 0 )
		    {
			rv = 0;
			port->audiobus_out = i + 1;
			break;
		    }
		}
	    }
#endif
	}
    }

open_end:

    if( rv != 0 )
    {
	//Open error:
	smem_free( port );
    }

    return rv;
}

int device_midi_client_close_port( sundog_midi_client* c, int pnum )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_close_port_jack( c, pnum );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    MIDIPortDispose( port->port );
    
    if( smem_strcmp( sd_port->dev_name, "public port" ) == 0 )
    {
#ifdef AUDIOBUS_MIDI_IN
        audiobus_remove_midiin_callback( midi_port_receive, port );
#endif
        MIDIEndpointDispose( port->endpoint );
    }
    
    smem_free( sd_port->device_specific );
    sd_port->device_specific = NULL;
    return 0;
}

sundog_midi_event* device_midi_client_get_event( sundog_midi_client* c, int pnum )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_get_event_jack( c, pnum );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return NULL;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return NULL;

    return get_midi_event( port );
}

int device_midi_client_next_event( sundog_midi_client* c, int pnum )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_next_event_jack( c, pnum );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return -1;

    next_midi_event( port );

    return 0;
}

int device_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt )
{
#ifdef JACK_AUDIO
    if( ss->driver == MDRIVER_JACK )
        return device_midi_client_send_event_jack( c, pnum, evt );
#endif
#ifdef OS_IOS
    if( kCFCoreFoundationVersionNumber < kCFCoreFoundationVersionNumber_iPhoneOS_4_2 )
	return 0;
#endif

    //CoreMIDI:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    c->last_midi_out_activity = stime_ticks();

#ifdef AUDIOBUS_MIDI_OUT
    if( port->audiobus_out <= 0 && g_ab_disable_coremidi_out ) return 0;
#endif

    MIDIPacketList list;
    list.numPackets = 1;
    list.packet[ 0 ].timeStamp = (MIDITimeStamp)convert_sundog_ticks_to_mach_absolute_time( evt->t, 1 );
    //...timeStamp will be broken after the max time for ticks_ht_t (due to overflow)
    size_t data_size = evt->size;
    size_t sent = 0;
    while( sent < data_size ) 
    {
	size_t size = data_size - sent;
	if( size > 256 ) size = 256;
	list.packet[ 0 ].length = size;
	smem_copy( list.packet[ 0 ].data, evt->data + sent, size );
	sent += size;
#ifdef AUDIOBUS_MIDI_OUT
	if( port->audiobus_out > 0 )
	{
	    audiobus_midiout( port->audiobus_out, &list );
	}
	else
#endif
	{
	    if( port->endpoint_type = ENDPOINT_VIRTUAL_SRC )
		MIDIReceived( port->endpoint, &list );
	    else
		MIDISend( port->port, port->endpoint, &list );
	}
    }

    return 0;
}

#endif
