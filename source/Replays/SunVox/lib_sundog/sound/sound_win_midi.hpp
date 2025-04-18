//
// Windows: MIDI (MMSYSTEM)
//

#ifndef NOMIDI

enum
{
    MDRIVER_MMSYSTEM,
    NUMBER_OF_MDRIVERS
};
#define DEFAULT_MDRIVER 0

const char* g_mdriver_infos[] =
{
    "MMSYSTEM"
};

const char* g_mdriver_names[] =
{
    "mmsystem"
};

#define BYTES_PER_EVENT		16

struct device_midi_event
{
    HMIDIOUT dev;
    stime_ticks_t       t; //the time when the event should happen (or already happened);
                           //actual time depends on the receiver that can delay the events;
                           //0 = as soon as possible (unknown time);
    uint32_t		id;
    int			size;
    uint8_t           	data[ BYTES_PER_EVENT ];
};

struct device_midi_client
{
    smutex 		out_mutex; //for the parallel writing from different threads AND port->link in device_midi_client_close_port()
    sring_buf* 		out_events_buf;
    sthread 		out_thread;
    HANDLE 		out_thread_event;
    std::atomic_int 	out_thread_exit_request;
    uint32_t		out_id; //counter
    smutex 		in_callback_mutex;
};

struct device_midi_port
{
    bool already_opened;
    int link;
    int dev_id;
    HMIDIIN in_dev;
    HMIDIOUT out_dev;
    stime_ticks_t start_time;

    COMMON_MIDIPORT_VARIABLES;
};

void* midi_out_thread( void* user_data )
{
    sundog_midi_client* c = (sundog_midi_client*)user_data;
    if( !c )
    {
	slog( "midi_out_thread(): null client\n" );
	return 0;
    }
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d )
    {
	slog( "midi_out_thread(): null client (device_specific)\n" );
	return 0;
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "midi_out_thread() begin\n" );
#endif

    int out_events_cnt = 0;
    device_midi_event* out_events = (device_midi_event*)smem_znew( sizeof( device_midi_event ) * MIDI_EVENTS );

    bool keep_running = true;
    while( keep_running )
    {
	if( atomic_load_explicit( &d->out_thread_exit_request, std::memory_order_relaxed ) )
	{
	    if( atomic_load_explicit( &d->out_thread_exit_request, std::memory_order_acquire ) )
	    {
		//The actual data from d->out_events_buf can be read now
		keep_running = false;
	    }
	}

	//Read the events:
	if( sring_buf_avail( d->out_events_buf ) )
	{
	    sring_buf_read_lock( d->out_events_buf );
	    device_midi_event evt;
	    while( 1 )
	    {
		int evt_size = sring_buf_read( d->out_events_buf, &evt, sizeof( evt ) );
		if( evt_size != sizeof( evt ) ) break;
		sring_buf_next( d->out_events_buf, evt_size );
		if( evt.size == -1 )
		{
		    //Close MIDI OUT device:
		    midiOutReset( evt.dev );
		    midiOutClose( evt.dev );
		    for( int e = 0; e < out_events_cnt; e++ )
		    {
			device_midi_event* evt2 = &out_events[ e ];
			if( evt2->dev == evt.dev )
			    evt2->size = 0;
		    }
		    continue;
		}
		int free_event = -1;
		if( out_events_cnt == 0 )
		{
		    free_event = 0;
		}
		else
		{
		    //Looking for a free slot for the event:
		    int size = (int)smem_get_size( out_events ) / sizeof( device_midi_event );
		    for( int e = 0; e < size; e++ )
		    {
			if( out_events[ e ].size == 0 )
			{
			    free_event = e;
			    break;
			}
		    }
		}
		if( free_event == -1 )
		{
		    //Resize the storage:
		    int size = (int)smem_get_size( out_events ) / sizeof( device_midi_event );
		    free_event = size;
		    size *= 2;
		    out_events = (device_midi_event*)smem_resize2( out_events, sizeof( device_midi_event ) * size );
		}
		if( evt.t == 0 ) evt.t = stime_ticks() - stime_ticks_per_second() * 60;
		out_events[ free_event ] = evt;
		if( free_event + 1 > out_events_cnt ) out_events_cnt = free_event + 1;
	    }
	    sring_buf_read_unlock( d->out_events_buf );
	}

	bool no_events = true;
	int first_event = -1;

	//Send the events:
	if( out_events_cnt )
	{
	    stime_ticks_t t = stime_ticks();

	    stime_ticks_t first_event_t = t;
	    uint32_t first_event_id = 0xFFFFFFFF;

	    for( int e = 0; e < out_events_cnt; e++ )
	    {
		device_midi_event* evt = &out_events[ e ];
		if( evt->size == 0 ) continue;
		no_events = false;
		int dt = (int)( first_event_t - evt->t );
		if( dt > 0 || ( dt == 0 && evt->id < first_event_id ) )
		{
		    first_event = e;
		    first_event_t = evt->t;
		    first_event_id = evt->id;
		}
	    }

	    if( first_event >= 0 )
	    {
		//Processing the next event:
		device_midi_event* evt = &out_events[ first_event ];
		if( evt->data[ 0 ] == 0xF0 )
		{
		    //SysEx:
		    MIDIHDR mh;
		    smem_clear_struct( mh );
		    mh.lpData = (LPSTR)evt->data;
		    mh.dwBufferLength = evt->size;
		    mh.dwFlags = 0;
		    MMRESULT res = midiOutPrepareHeader( evt->dev, &mh, sizeof( MIDIHDR ) );
		    if( res != MMSYSERR_NOERROR )
		    {
		        slog( "midiOutPrepareHeader error %d\n", (int)res );
		    }
		    else
		    {
		        midiOutLongMsg( evt->dev, &mh, sizeof( MIDIHDR ) );
		        while( midiOutUnprepareHeader( evt->dev, &mh, sizeof( MIDIHDR ) ) == MIDIERR_STILLPLAYING ) stime_sleep( 1 );
		    }
		}
		else
		{
		    int p = 0;
		    DWORD msg = 0;
		    for( int b = 0; b < evt->size; b++ )
		    {
			if( ( evt->data[ b ] & 0x80 ) || ( p > 2 ) )
			{
			    if( p > 0 )
			        midiOutShortMsg( evt->dev, msg );
			    p = 0;
			    msg = 0;
			}
			uint8_t* bytes = (uint8_t*)&msg;
			bytes[ p ] = evt->data[ b ];
			p++;
		    }
		    if( p > 0 )
		        midiOutShortMsg( evt->dev, msg );
		}
		evt->size = 0;
	    }

	    if( no_events )
		out_events_cnt = 0;
	}

	if( first_event == -1 )
	{
	    if( no_events )
		WaitForSingleObject( d->out_thread_event, 200 );
	    else
		stime_sleep( 1 );
	}
    }

    smem_free( out_events );

#ifdef SHOW_DEBUG_MESSAGES
    slog( "midi_out_thread() end\n" );
#endif

    return 0;
}

void CALLBACK midi_in_callback( HMIDIIN handle, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 )
{
    sundog_midi_client* c = (sundog_midi_client*)dwInstance;
    if( !c ) return;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return;

#ifdef SHOW_DEBUG_MESSAGES
    slog( "midi_in_callback() begin\n" );
#endif

    c->last_midi_in_activity = stime_ticks();

    if( uMsg == MIM_DATA )
    {
	uint8_t* msg = (uint8_t*)&dwParam1;
	DWORD timestamp = dwParam2;

	uint8_t status = msg[ 0 ];
	if ( !( status & 0x80 ) ) return;

	int msg_bytes = 1;
	while( 1 )
	{
	    if( status < 0xC0 ) { msg_bytes = 3; break; }
	    if( status < 0xE0 ) { msg_bytes = 2; break; }
	    if( status < 0xF0 ) { msg_bytes = 3; break; }
	    if( status == 0xF1 ) { msg_bytes = 2; break; }
	    if( status == 0xF2 ) { msg_bytes = 3; break; }
	    if( status == 0xF3 ) { msg_bytes = 2; break; }
	    break;
	}

	smutex_lock( &d->in_callback_mutex );

	for( int port_num = 0; port_num < c->ports_cnt; port_num++ )
	{
	    sundog_midi_port* sd_port = c->ports[ port_num ];
            if( sd_port && sd_port->active )
	    {
		device_midi_port* port = (device_midi_port*)sd_port->device_specific;
		if( !port ) continue;
		if( port->already_opened ) port = (device_midi_port*)c->ports[ port->link ]->device_specific;
		if( !port ) continue;
		if( port->in_dev == handle )
		{
		    uint64_t t = port->start_time + ( timestamp * stime_ticks_per_second() ) / 1000;
		    write_received_midi_data( port, msg, msg_bytes, (stime_ticks_t)t );
		}
	    }
	}

	smutex_unlock( &d->in_callback_mutex );
    }

#ifdef SHOW_DEBUG_MESSAGES
    slog( "midi_in_callback() end\n" );
#endif
}

int device_midi_client_open( sundog_midi_client* c, const char* name )
{
    c->device_specific = smem_new( sizeof( device_midi_client ) );
    device_midi_client* d = (device_midi_client*)c->device_specific;
    smem_zero( d );

    //MIDI OUT:
    smutex_init( &d->out_mutex, 0 );
    d->out_events_buf = sring_buf_new( sizeof( device_midi_event ) * MIDI_EVENTS, SRING_BUF_FLAG_SINGLE_RTHREAD | SRING_BUF_FLAG_SINGLE_WTHREAD );
    //SRING_BUF_FLAG_SINGLE_WTHREAD because we have out_mutex
    atomic_init( &d->out_thread_exit_request, 0 );
    d->out_thread_event = CreateEvent( 0, 0, 0, 0 );
    sthread_create( &d->out_thread, c->sd, midi_out_thread, c, 0 );

    //MIDI IN:
    smutex_init( &d->in_callback_mutex, 0 );

    return 0;
}

int device_midi_client_close( sundog_midi_client* c )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;

    //MIDI OUT:
    atomic_store_explicit( &d->out_thread_exit_request, 1, std::memory_order_release );
    SetEvent( d->out_thread_event );
    sthread_destroy( &d->out_thread, 1000 );
    CloseHandle( d->out_thread_event );
    smutex_destroy( &d->out_mutex );
    sring_buf_remove( d->out_events_buf );

    //MIDI IN:
    smutex_destroy( &d->in_callback_mutex ); 

    smem_free( d );
    c->device_specific = NULL;
    return 0;
}

int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags )
{
    uint devs = 0;
    if( flags & MIDI_PORT_READ )
	devs = midiInGetNumDevs();
    else 
	devs = midiOutGetNumDevs();

    char* name = (char*)smem_new( 1024 );
    char* prev_name = NULL;
    int name_cnt = 1;
    
    *devices = NULL;
    
    for( uint i = 0; i < devs; i++ )
    {
	name[ 0 ] = 0;
	if( flags & MIDI_PORT_READ )
	{
	    MIDIINCAPSW caps;
	    if( midiInGetDevCapsW( i, &caps, sizeof( MIDIINCAPSW ) ) == MMSYSERR_NOERROR )
		utf16_to_utf8( name, 1024, (const uint16_t*)caps.szPname );
	}
	else 
	{
	    MIDIOUTCAPSW caps;
	    if( midiOutGetDevCapsW( i, &caps, sizeof( MIDIOUTCAPSW ) ) == MMSYSERR_NOERROR )
		utf16_to_utf8( name, 1024, (const uint16_t*)caps.szPname );
	}
	char* name2 = (char*)smem_new( smem_strlen( name ) + 8 );
	if( prev_name == 0 )
	{
	    prev_name = smem_strdup( name );
	    sprintf( name2, "%s", name );
	}
	else 
	{
	    if( smem_strcmp( prev_name, name ) == 0 )
	    {
	        name_cnt++;
	        sprintf( name2, "%s %d", name, name_cnt );
	    }
	    else
	    {
	        name_cnt = 1;
	        smem_free( prev_name );
	        prev_name = smem_strdup( name );
	        sprintf( name2, "%s", name );
	    }
	}
	smem_objarray_write_d( (void***)devices, name2, false, i );
    }

    smem_free( prev_name );
    smem_free( name );

    return devs;
}

int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags )
{
    int rv = 0;

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;
    sundog_midi_port* sd_port = c->ports[ pnum ];
    sd_port->device_specific = smem_znew( sizeof( device_midi_port ) );
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
#ifndef NOMIDI
    init_common_midiport_vars( port );
#endif

    for( int i = 0; i < c->ports_cnt; i++ )
    {
	if( c->ports[ i ] && c->ports[ i ]->active )
	{
	    if( smem_strcmp( c->ports[ i ]->dev_name, dev_name ) == 0 )
	    {
		if( ( flags & ( MIDI_PORT_READ | MIDI_PORT_WRITE ) ) == ( c->ports[ i ]->flags & ( MIDI_PORT_READ | MIDI_PORT_WRITE ) ) )
		{
		    device_midi_port* port2 = (device_midi_port*)c->ports[ i ]->device_specific;
		    if( port2 && port2->already_opened == 0 )
		    {
			//Already opened:
			port->already_opened = 1;
			port->link = i;
			break;
		    }
		}
	    }
	}
    }
    if( port->already_opened == 0 )
    {
	port->dev_id = -1;

	char** devices = NULL;
	int devs = device_midi_client_get_devices( c, &devices, flags );

	for( int i = 0; i < devs; i++ )
	{
	    if( smem_strcmp( devices[ i ], dev_name ) == 0 )
	    {
		//Device found:
		port->dev_id = i;
		break;
	    }
	}
	for( int i = 0; i < devs; i++ )
	{
	    smem_free( devices[ i ] );
	}
	smem_free( devices );

	if( port->dev_id >= 0 )
	{
	    if( flags & MIDI_PORT_READ )
	    {
		MMRESULT res = midiInOpen( &port->in_dev, port->dev_id, (DWORD_PTR)&midi_in_callback, (DWORD_PTR)c, CALLBACK_FUNCTION );
		if( res != MMSYSERR_NOERROR )
		{
		    slog( "midiInOpen error %d\n", (int)res );
		    rv = -1;
		    goto open_end;
		}
		midiInStart( port->in_dev );
		port->start_time = stime_ticks();
	    }
	    else
	    {
		MMRESULT res = 0;
		stime_ticks_t t1 = stime_ticks();
		while( 1 )
		{
		    res = midiOutOpen( &port->out_dev, port->dev_id, 0, 0, CALLBACK_NULL );
		    if( res == MMSYSERR_ALLOCATED )
		    {
			//Still not closed in the output thread?
			stime_ticks_t t2 = stime_ticks();
			if( t2 - t1 > stime_ticks_per_second() ) break;
			stime_sleep( 1 );
			continue;
		    }
		    break;
		}
		if( res != MMSYSERR_NOERROR )
		{
		    slog( "midiOutOpen error %d\n", (int)res );
		    rv = -1;
		    goto open_end;
		}
	    }
	}
	else
	{
	    slog( "MIDI device not found: %s\n", dev_name );
	    rv = -1;
	    goto open_end;
	}
    }

open_end:

    if( rv != 0 )
    {
	smem_free( sd_port->device_specific );
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

    if( port->already_opened == 0 )
    {
    	HMIDIIN in_dev_to_close = NULL;

	smutex_lock( &d->out_mutex );

	//Are there linked ports?
	int new_parent = -1;
	for( int i = 0; i < c->ports_cnt; i++ )
	{
	    if( c->ports[ i ] && c->ports[ i ]->active )
	    {
		device_midi_port* port2 = (device_midi_port*)c->ports[ i ]->device_specific;
		if( port2 && port2->already_opened && port2->link == pnum )
		{
		    if( new_parent == -1 )
		    {
			//Now port2 is parent
			smem_copy( port2, port, sizeof( device_midi_port ) );
			new_parent = i;
		    }
		    else
		    {
			port2->link = new_parent;
		    }
		}
	    }
	}

	if( new_parent == -1 )
	{
	    //No linked ports. We can remove this parent:
    	    if( sd_port->flags & MIDI_PORT_READ )
    	    {
    		HMIDIIN in_dev = port->in_dev;
    		smutex_lock( &d->in_callback_mutex );
    		port->in_dev = NULL;
    		smutex_unlock( &d->in_callback_mutex );
    		in_dev_to_close = in_dev;
	    }
    	    else
	    {
		device_midi_event out_evt;
	        smem_clear_struct( out_evt );
		sring_buf_write_lock( d->out_events_buf );
		out_evt.dev = port->out_dev;
		out_evt.id = d->out_id; d->out_id++;
		out_evt.size = -1;
		sring_buf_write( d->out_events_buf, &out_evt, sizeof( out_evt ) );
		sring_buf_write_unlock( d->out_events_buf );
    		port->out_dev = NULL;
		SetEvent( d->out_thread_event );
	    }
	}

	smutex_unlock( &d->out_mutex );

	if( in_dev_to_close )
	{
    	    midiInStop( in_dev_to_close );
    	    midiInReset( in_dev_to_close );
	    midiInClose( in_dev_to_close );
	}
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
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    c->last_midi_out_activity = stime_ticks();

    device_midi_event out_evt;
    smem_clear_struct( out_evt );

    smutex_lock( &d->out_mutex );
    sring_buf_write_lock( d->out_events_buf );

    int evt_offset = 0;
    while( 1 )
    {
	int evt_size = evt->size - evt_offset;
	if( evt_size > BYTES_PER_EVENT )
	{
	    //Event too large:
	    evt_size = BYTES_PER_EVENT;
	}
	out_evt.t = evt->t;
	out_evt.dev = NULL;
	out_evt.id = d->out_id; d->out_id++;
	out_evt.size = evt_size;
	smem_copy( out_evt.data, evt->data + evt_offset, evt_size );
	if( port->already_opened == 0 )
	{
	    out_evt.dev = port->out_dev;
	}
	else
	{
	    device_midi_port* port2 = (device_midi_port*)c->ports[ port->link ]->device_specific;
	    if( port2 )
		out_evt.dev = port2->out_dev;
	}
	sring_buf_write( d->out_events_buf, &out_evt, sizeof( out_evt ) );
	evt_offset += evt_size;
	if( evt_offset >= evt->size ) break;
    }

    sring_buf_write_unlock( d->out_events_buf );
    smutex_unlock( &d->out_mutex );

    SetEvent( d->out_thread_event );

    return 0;
}

#endif
