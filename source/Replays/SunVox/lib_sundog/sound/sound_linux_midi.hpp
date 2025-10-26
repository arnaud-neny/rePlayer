//
// Linux: MIDI (ALSA and JACK)
//

#ifndef NOMIDI

enum
{
#ifndef NOALSA
    MDRIVER_ALSA,
#endif
#ifdef JACK_AUDIO
    MDRIVER_JACK,
#endif
    NUMBER_OF_MDRIVERS
};
#define DEFAULT_MDRIVER 0

const char* g_mdriver_infos[] =
{
#ifndef NOALSA
    "ALSA",
#endif
#ifdef JACK_AUDIO
    "JACK",
#endif
};

const char* g_mdriver_names[] =
{
#ifndef NOALSA
    "alsa",
#endif
#ifdef JACK_AUDIO
    "jack",
#endif
};

bool g_midi_info = false;

struct device_midi_client
{
    snd_seq_t* 				handle;
    int 				queue;
    stime_ticks_t 			time_offset;
    sthread 				thread;
    int 				thread_control_pipe[ 2 ];
    volatile bool 			thread_exit_request;
};

struct device_midi_port
{
    int 				id;
    snd_seq_port_subscribe_t* 		subs;
    snd_midi_event_t* 			event_decoder;

    COMMON_MIDIPORT_VARIABLES;
};

void* midi_client_thread( void* user_data )
{
    sundog_midi_client* c = (sundog_midi_client*)user_data;
    if( c == 0 )
    {
        slog( "midi_client_thread(): null client\n" );
        return 0;
    }
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 )
    {
        slog( "midi_client_thread(): null client (device_specific)\n" );
        return 0;
    }
    int npfd;
    struct pollfd* pfd;
    bool reset_request;

    while( 1 )
    {
	reset_request = 0;
	npfd = snd_seq_poll_descriptors_count( d->handle, POLLIN );
	pfd = SMEM_ALLOC2( struct pollfd, npfd + 1 );
	snd_seq_poll_descriptors( d->handle, pfd, npfd, POLLIN );
	pfd[ npfd ].fd = d->thread_control_pipe[ 0 ];
	pfd[ npfd ].events = POLLIN;
	int rv = 0;
	while( d->thread_exit_request == 0 && reset_request == 0 )
	{
	    if( rv > 0 || poll( pfd, npfd + 1, 1000 ) > 0 ) 
	    {
		snd_seq_event_t* ev = 0;
		rv = snd_seq_event_input( d->handle, &ev );
		if( rv >= 0 && ev && ev->dest.client == snd_seq_client_id( d->handle ) )
		{
		    c->midi_in_activity = true;
		    device_midi_port* port = NULL;
		    for( int i = 0; i < c->ports_cnt; i++ )
		    {
		        sundog_midi_port* sd_port = c->ports[ i ];
		        if( sd_port && sd_port->active )
		        {
		    	    port = (device_midi_port*)sd_port->device_specific;
			    if( port && port->id == ev->dest.port )
			    {
			        //Port found.
			        break;
			    }
			}
		    }
		    if( port )
		    {
			uint8_t decoder_buf[ 256 ];
			int data_size = snd_midi_event_decode( port->event_decoder, decoder_buf, 256, ev );
			if( data_size > 0 )
			{
			    /*printf( "%d >> ", rv );
			    for( int b = 0; b < data_size; b++ )
			    {
				printf( "%02x ", decoder_buf[ b ] );
			    }
			    printf( "\n" );*/
			    stime_ticks_t t = (stime_ticks_t)( ev->time.time.tv_nsec / ( 1000000000 / stime_ticks_per_second() ) ) + ev->time.time.tv_sec * stime_ticks_per_second();
			    t += d->time_offset;
			    write_received_midi_data( port, decoder_buf, data_size, t );
			}
		    }
		    snd_seq_free_event( ev );
		}
		if( pfd[ npfd ].revents & POLLIN )
		{
		    char buf = 0;
		    read( d->thread_control_pipe[ 0 ], &buf, 1 );
		    if( buf == 1 ) reset_request = 1;
		}
	    }
	}
	smem_free( pfd );
	if( reset_request == 0 ) break;
    }

    return 0;
}

void find_midi_client( sundog_midi_client* c, const char* dev_name, int* client_id, int* port_id )
{
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return;
    
    *client_id = -1;
    *port_id = -1;
    
    snd_seq_client_info_t* cinfo = 0;
    snd_seq_client_info_alloca( &cinfo );
    snd_seq_client_info_set_client( cinfo, -1 );
    snd_seq_port_info_t* pinfo = 0;
    snd_seq_port_info_alloca( &pinfo );
    while( snd_seq_query_next_client( d->handle, cinfo ) >= 0 ) 
    {
	int cid = snd_seq_client_info_get_client( cinfo );
	snd_seq_port_info_set_client( pinfo, cid );
	snd_seq_port_info_set_port( pinfo, -1 );
	while( snd_seq_query_next_port( d->handle, pinfo ) >= 0 ) 
	{
	    int pid = snd_seq_port_info_get_port( pinfo );
	    const char* port_name = snd_seq_port_info_get_name( pinfo );
	    char ts[ 32 ];
	    sprintf( ts, "%d:%d", cid, pid );
	    if( smem_strcmp( ts, dev_name ) == 0 )
	    {
		*client_id = cid;
		*port_id = pid;
		return;
	    }
	    char* port_name2 = SMEM_ALLOC2( char, smem_strlen( port_name ) + 1 );
	    port_name2[ 0 ] = 0;
	    SMEM_STRCAT_D( port_name2, port_name );
	    for( int i = smem_strlen( port_name2 ) - 1; i >= 0; i-- )
	    {
		if( port_name2[ i ] == ' ' ) 
		    port_name2[ i ] = 0;
		else 
		    break;
	    }
	    if( smem_strcmp( port_name2, dev_name ) == 0 )
	    {
		*client_id = cid;
		*port_id = pid;
		smem_free( port_name2 );
		return;
	    }
	    smem_free( port_name2 );
	}
    }
}

int device_midi_client_open( sundog_midi_client* c, const char* name )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
	return device_midi_client_open_jack( c, name );
#endif

    //ALSA:

    int rv = 0;
    c->device_specific = SMEM_ZALLOC( sizeof( device_midi_client ) );
    device_midi_client* d = (device_midi_client*)c->device_specific;
    
    while( 1 )
    {
        //Open client:
	int err = snd_seq_open( &d->handle, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK );
        if( err < 0 )
	{
	    slog( "snd_seq_open() error %d: %s\n", err, snd_strerror( err ) );
	    rv = -1;
	    break;
	}
        snd_seq_set_client_name( d->handle, name );
	d->queue = snd_seq_alloc_named_queue( d->handle, "queue" );
        snd_seq_start_queue( d->handle, d->queue, 0 );
        snd_seq_drain_output( d->handle );
	snd_seq_sync_output_queue( d->handle );

        d->time_offset = stime_ticks();
	/*
	snd_seq_queue_status_t* qstatus;
	snd_seq_queue_status_alloca( &qstatus );
        const snd_seq_real_time_t* rt = snd_seq_queue_status_get_real_time( qstatus );
        printf( "START ALSA MIDI TIME: %d sec; %d nsec;\n", (int)rt->tv_sec, (int)rt->tv_nsec );
        //rt = 0s+0ns
        */

	//Open client thread:
        if( pipe( d->thread_control_pipe ) )
	{
	    slog( "pipe() error %d\n", errno );
	    rv = -2;
	    break;
	}
	sthread_create( &d->thread, c->sd, midi_client_thread, c, 0 );
    
	if( g_midi_info )
	{
	    snd_seq_client_info_t* cinfo = 0;
	    snd_seq_client_info_alloca( &cinfo );
    	    snd_seq_client_info_set_client( cinfo, -1 );
	    snd_seq_port_info_t* pinfo = 0;
	    snd_seq_port_info_alloca( &pinfo );
	    while( snd_seq_query_next_client( d->handle, cinfo ) >= 0 ) 
	    {
		int id = snd_seq_client_info_get_client( cinfo );
		const char* name = snd_seq_client_info_get_name( cinfo );
		slog( "CLIENT %d: %s\n", id, name );
		snd_seq_port_info_set_client( pinfo, id );
		snd_seq_port_info_set_port( pinfo, -1 );
		while( snd_seq_query_next_port( d->handle, pinfo ) >= 0 ) 
		{
		    int port_id = snd_seq_port_info_get_port( pinfo );
		    const char* port_name = snd_seq_port_info_get_name( pinfo );
		    slog( "  PORT %d: %s\n", port_id, port_name );
		}
	    }
	    g_midi_info = false;
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
    if( c->driver == MDRIVER_JACK )
	return device_midi_client_close_jack( c );
#endif

    //ALSA:

    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return -1;

    while( 1 )
    {
	if( d->handle == 0 ) break;
    
        //Close thread:
	d->thread_exit_request = 1;
        char buf = 0;
	write( d->thread_control_pipe[ 1 ], &buf, 1 );
        sthread_destroy( &d->thread, 1000 );
	close( d->thread_control_pipe[ 0 ] );
        close( d->thread_control_pipe[ 1 ] );
    
	//Close client:
        snd_seq_free_queue( d->handle, d->queue );
	int err = snd_seq_close( d->handle );
        if( err < 0 )
	{
	    slog( "snd_seq_close() error %d: %s\n", err, snd_strerror( err ) );
	    rv = -1;
	    break;
        }
    
	break;
    }
        
    REMOVE_DEVICE_SPECIFIC_DATA( c );

    return rv;
}

int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_get_devices_jack( c, devices, flags );
#endif

    //ALSA:

    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return 0;
    if( d->handle == 0 ) return 0;
    
    *devices = 0;
    
    snd_seq_client_info_t* cinfo = 0;
    snd_seq_client_info_alloca( &cinfo );
    snd_seq_client_info_set_client( cinfo, -1 );
    snd_seq_port_info_t* pinfo = 0;
    snd_seq_port_info_alloca( &pinfo );
    while( snd_seq_query_next_client( d->handle, cinfo ) >= 0 ) 
    {
	int cid = snd_seq_client_info_get_client( cinfo );
	snd_seq_port_info_set_client( pinfo, cid );
	snd_seq_port_info_set_port( pinfo, -1 );
	while( snd_seq_query_next_port( d->handle, pinfo ) >= 0 ) 
	{
	    const char* port_name = snd_seq_port_info_get_name( pinfo );
	    int caps = snd_seq_port_info_get_capability( pinfo );
	    bool ignore = true;
	    if( ( flags & MIDI_PORT_READ ) && ( caps & SND_SEQ_PORT_CAP_READ ) ) ignore = 0;
	    if( ( flags & MIDI_PORT_WRITE ) && ( caps & SND_SEQ_PORT_CAP_WRITE ) ) ignore = 0;
	    if( ignore == false )
	    {
		rv++;
		char* port_name2 = SMEM_STRDUP( port_name );
		for( int i = smem_strlen( port_name2 ) - 1; i >= 0; i-- )
		{
		    if( port_name2[ i ] == ' ' ) 
			port_name2[ i ] = 0;
		    else 
			break;
		}
		SMEM_OBJARRAY_WRITE_D( (void***)devices, port_name2, false, rv - 1 );
	    }
	}
    }
    
    return rv;
}

int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_open_port_jack( c, pnum, port_name, dev_name, flags );
#endif

    //ALSA:

    int rv = 0;
    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return -1;
    if( d->handle == 0 ) return -1;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    sd_port->device_specific = SMEM_ZALLOC( sizeof( device_midi_port ) );
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
#ifndef NOMIDI
    init_common_midiport_vars( port );
#endif
    char* port_name_long = SMEM_ALLOC2( char, smem_get_size( c->name ) + 1 + smem_strlen( port_name ) + 1 );
    port_name_long[ 0 ] = 0;
    SMEM_STRCAT_D( port_name_long, c->name );
    SMEM_STRCAT_D( port_name_long, " " );
    SMEM_STRCAT_D( port_name_long, port_name );
    
    while( 1 )
    {
	uint dflags = 0;
	if( flags & MIDI_PORT_READ )
	    dflags |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	else 
	    dflags |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
	port->id = snd_seq_create_simple_port( d->handle, port_name_long, dflags, SND_SEQ_PORT_TYPE_MIDI_GENERIC );
	if( port->id < 0 ) 
	{
	    slog( "snd_seq_create_simple_port error %d\n", port->id );
	    rv = -1;
	    break;
	}
	int client_id = -1;
	int port_id = -1;
	find_midi_client( c, dev_name, &client_id, &port_id );
	if( client_id >= 0 && port_id >= 0 )
	{
	    snd_seq_addr_t sender;
	    snd_seq_addr_t dest;
	    if( flags & MIDI_PORT_READ )
	    {
		sender.client = client_id;
		sender.port = port_id;
		dest.client = snd_seq_client_id( d->handle );
		dest.port = port->id;
	    }
	    else 
	    {
		sender.client = snd_seq_client_id( d->handle );
		sender.port = port->id;
		dest.client = client_id;
		dest.port = port_id;
	    }
	    snd_seq_port_subscribe_alloca( &port->subs );
	    snd_seq_port_subscribe_set_sender( port->subs, &sender );
	    snd_seq_port_subscribe_set_dest( port->subs, &dest );
	    snd_seq_port_subscribe_set_queue( port->subs, d->queue );
	    snd_seq_port_subscribe_set_time_update( port->subs, 1 );
	    snd_seq_port_subscribe_set_time_real( port->subs, 1 );
	    int v = snd_seq_subscribe_port( d->handle, port->subs );
	    if( v < 0 )
	    {
		snd_seq_delete_simple_port( d->handle, port->id );
		slog( "snd_seq_subscribe_port error %d\n", v );
		rv = -1;
		break;
	    }
	}
	
	snd_midi_event_new( 256, &port->event_decoder );
	
	char buf = 1; //reinit the thread
	write( d->thread_control_pipe[ 1 ], &buf, 1 );
	
	break;
    }
    
    smem_free( port_name_long );
    
    if( rv )
    {
	if( port )
	{
	    if( port->id >= 0 )
		snd_seq_delete_simple_port( d->handle, port->id );
	    smem_free( port );
	    sd_port->device_specific = 0;
	}
    }
    
    return rv;
}

int device_midi_client_close_port( sundog_midi_client* c, int pnum )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_close_port_jack( c, pnum );
#endif

    //ALSA:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return -1;
    if( d->handle == 0 ) return -1;
    
    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( port == 0 ) return 0;
    
    snd_seq_delete_simple_port( d->handle, port->id );
    
    snd_midi_event_free( port->event_decoder );

    char buf = 1; //reinit the thread
    write( d->thread_control_pipe[ 1 ], &buf, 1 );
    
    smem_free( sd_port->device_specific );
    sd_port->device_specific = 0;
    return 0;
}

sundog_midi_event* device_midi_client_get_event( sundog_midi_client* c, int pnum )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_get_event_jack( c, pnum );
#endif

    //ALSA:

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
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_next_event_jack( c, pnum );
#endif

    //ALSA:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( !d ) return -1;
    if( !d->handle ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( !port ) return 0;

    next_midi_event( port );

    return 0;
}

int device_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt )
{
#ifdef JACK_AUDIO
    if( c->driver == MDRIVER_JACK )
        return device_midi_client_send_event_jack( c, pnum, evt );
#endif

    //ALSA:

    device_midi_client* d = (device_midi_client*)c->device_specific;
    if( d == 0 ) return -1;
    if( d->handle == 0 ) return -1;

    sundog_midi_port* sd_port = c->ports[ pnum ];
    device_midi_port* port = (device_midi_port*)sd_port->device_specific;
    if( port == 0 ) return 0;

    snd_seq_event_t alsa_event;
    size_t data_size = evt->size;
    size_t sent = 0;
    while( sent < data_size ) 
    { 
	size_t size = data_size - sent;
	snd_seq_ev_clear( &alsa_event );
	long s = snd_midi_event_encode( port->event_decoder, evt->data + sent, size, &alsa_event );
	if( s > 0 )
	{
	    if( alsa_event.type != SND_SEQ_EVENT_NONE ) 
	    {
		snd_seq_ev_set_source( &alsa_event, port->id );
		snd_seq_ev_set_subs( &alsa_event );
		snd_seq_real_time t;
		stime_ticks_t evt_t = evt->t - d->time_offset;
		/*
		static stime_ticks_t prev_t = 0;
		printf( "%d\n", evt_t - prev_t );
		prev_t = evt_t;
		*/
		int64_t nsec = (int64_t)( evt_t % stime_ticks_per_second() ) * 1000000000 / stime_ticks_per_second();
		t.tv_sec = evt_t / stime_ticks_per_second();
		t.tv_nsec = (uint)nsec;
		snd_seq_ev_schedule_real( &alsa_event, d->queue, 0, &t );
		snd_seq_event_output( d->handle, &alsa_event );
		snd_seq_drain_output( d->handle );
	    }
	    else
	    {
		slog( "send_event: SND_SEQ_EVENT_NONE\n" );
	    }
	    sent += s;
	}
	else
	{
	    slog( "snd_midi_event_encode() error %d\n", (int)s );
	    break;
	}
    }

    return 0;
}

#endif
