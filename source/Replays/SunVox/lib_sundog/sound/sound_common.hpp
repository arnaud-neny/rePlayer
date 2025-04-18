//
// Common sound and MIDI functions/variables/defines for device-specific code
//

#if !defined(OS_ANDROID) && !defined(OS_IOS) && !defined(OS_EMSCRIPTEN)
    #define WITH_COMMON_INPUT_FUNCTIONS
#endif


#ifndef COMMON_CODE
//
// Header
//


#define MIDI_EVENTS ( 2048 )
#define MIDI_BYTES ( MIDI_EVENTS * 16 )
#ifndef NOMIDI
struct device_midi_port;
#define COMMON_MIDIPORT_VARIABLES \
    uint8_t			data[ MIDI_BYTES ]; \
    sundog_midi_event		events[ MIDI_EVENTS ]; \
    std::atomic_uint 		data_rp; \
    volatile uint 		data_wp; \
    std::atomic_uint		evt_rp; \
    std::atomic_uint 		evt_wp;
static void init_common_midiport_vars( device_midi_port* port );
static void write_received_midi_data( device_midi_port* port, uint8_t* bytes, int size, stime_ticks_t t );
static void next_midi_event( device_midi_port* port );
static sundog_midi_event* get_midi_event( device_midi_port* port );
#endif //!NOMIDI

#define REMOVE_DEVICE_SPECIFIC_DATA( OBJ ) { smem_free( d ); OBJ->device_specific = NULL; }

#ifdef WITH_COMMON_INPUT_FUNCTIONS
//With common input functions:

//For the device_sound structure:
#define COMMON_SOUNDINPUT_VARIABLES \
    uint 			input_buffer_size; /*in frames*/ \
    volatile uint 		input_buffer_wp; \
    volatile uint		input_buffer_rp; \
    void*			input_buffer; /*source*/ \
    void* 			input_buffer2; /*destination*/ \
    bool 			input_buffer2_is_empty; \
    bool 			input_enabled;

const int g_input_buffers_count = 8;
static void get_input_data( sundog_sound* ss, uint frames );
static void set_input_defaults( sundog_sound* ss );
static void create_input_buffers( sundog_sound* ss, uint frames );
static void remove_input_buffers( sundog_sound* ss );

#endif //...WITH_COMMON_INPUT_FUNCTIONS


#else
//
// Code
//


#ifndef NOMIDI

//Single Writer - Single Reader

static void init_common_midiport_vars( device_midi_port* port )
{
    atomic_init( &port->evt_wp, (uint)0 );
}

static void write_received_midi_data( device_midi_port* port, uint8_t* bytes, int size, stime_ticks_t t )
{
    uint empty_data = 0;
    if( port->data_wp + size > MIDI_BYTES )
	empty_data = MIDI_BYTES - port->data_wp;
    uint data_rp = atomic_load_explicit( &port->data_rp, std::memory_order_relaxed );
    uint can_write = ( data_rp - port->data_wp ) & ( MIDI_BYTES - 1 );
    if( can_write == 0 ) can_write = MIDI_BYTES;
    if( ( ( port->data_wp + can_write ) & ( MIDI_BYTES - 1 ) ) == data_rp ) can_write--;
    if( empty_data + size <= can_write )
    {
	uint evt_rp = atomic_load_explicit( &port->evt_rp, std::memory_order_relaxed );
	uint evt_wp = atomic_load_explicit( &port->evt_wp, std::memory_order_relaxed );
        if( ( ( evt_rp - evt_wp ) & ( MIDI_EVENTS - 1 ) ) != 1 )
        {
            for( int b = 0; b < size; b++ )
            {
                port->data[ ( port->data_wp + b + empty_data ) & ( MIDI_BYTES - 1 ) ] = bytes[ b ];
            }
            sundog_midi_event* evt = &port->events[ evt_wp ];
            evt->t = t;
            evt->size = size;
            evt->data = port->data + ( ( port->data_wp + empty_data ) & ( MIDI_BYTES - 1 ) );
            uint new_data_wp = ( port->data_wp + size + empty_data ) & ( MIDI_BYTES - 1 );
            port->data_wp = new_data_wp;
            COMPILER_MEMORY_BARRIER(); //for compatibility with older compilers and devices
	    atomic_store_explicit( &port->evt_wp, ( evt_wp + 1 ) & ( MIDI_EVENTS - 1 ), std::memory_order_release );
        }
    }
}

static void next_midi_event( device_midi_port* port ) //only after sucessful get_midi_event()
{
    uint evt_rp = atomic_load_explicit( &port->evt_rp, std::memory_order_relaxed );
    uint evt_wp = atomic_load_explicit( &port->evt_wp, std::memory_order_relaxed );
    if( evt_rp == evt_wp ) return; //No events
    sundog_midi_event* evt = &port->events[ evt_rp ];
    evt_rp = ( evt_rp + 1 ) & ( MIDI_EVENTS - 1 );
    size_t p = evt->data - port->data;
    p += evt->size;
    atomic_store_explicit( &port->evt_rp, evt_rp, std::memory_order_relaxed );
    atomic_store_explicit( &port->data_rp, (uint)p & ( MIDI_BYTES - 1 ), std::memory_order_relaxed );
}

static sundog_midi_event* get_midi_event( device_midi_port* port )
{
    uint evt_rp = atomic_load_explicit( &port->evt_rp, std::memory_order_relaxed );
    uint evt_wp = atomic_load_explicit( &port->evt_wp, std::memory_order_relaxed );
    if( evt_rp == evt_wp ) return NULL; //No events
    //Here evt_wp may be equal to some old cached value...
    //All writes before port->evt_wp=evt_wp (memory_order_release) (in other thread) will be available after this fence:
    atomic_thread_fence( std::memory_order_acquire );
    //Here we can read the actual event data (corresponding to the evt_wp)...
    sundog_midi_event* evt = &port->events[ evt_rp ];
    return evt;
}

#endif //!NOMIDI

#ifdef WITH_COMMON_INPUT_FUNCTIONS
//With common input functions:

static void get_input_data( sundog_sound* ss, uint frames )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( d->input_enabled )
    {
        d->input_buffer2_is_empty = false;
        ss->in_buffer = d->input_buffer2;
	if( ( ( d->input_buffer_wp - d->input_buffer_rp ) & ( d->input_buffer_size - 1 ) ) >= frames )
        {
	    int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
	    int size = frames;
	    int ptr = 0;
	    while( size > 0 )
	    {
		int size2 = size;
		if( d->input_buffer_rp + size2 > d->input_buffer_size )
		    size2 = d->input_buffer_size - d->input_buffer_rp;
		smem_copy( (int8_t*)d->input_buffer2 + ptr * frame_size, (int8_t*)d->input_buffer + d->input_buffer_rp * frame_size, size2 * frame_size );
		d->input_buffer_rp += size2;
		d->input_buffer_rp &= ( d->input_buffer_size - 1 );
		size -= size2;
		ptr += size2;
	    }
        }
    }
    else
    {
	if( d->input_buffer2 )
	{
    	    if( d->input_buffer2_is_empty == 0 )
    	    {
        	smem_zero( d->input_buffer2 );
        	d->input_buffer2_is_empty = true;
    	    }
    	}
        ss->in_buffer = d->input_buffer2;
    }
}

static void set_input_defaults( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    ss->in_channels = ss->out_channels;
    if( ss->in_channels > 2 ) ss->in_channels = 2;
    ss->in_type = ss->out_type;
    d->input_buffer_wp = 0;
    d->input_buffer_rp = 0;
    d->input_enabled = false;
}

static void create_input_buffers( sundog_sound* ss, uint frames )
{
    device_sound* d = (device_sound*)ss->device_specific;
    if( d->input_buffer == 0 )
    {
        int frame_size = g_sample_size[ ss->in_type ] * ss->in_channels;
        d->input_buffer_size = round_to_power_of_two( frames * g_input_buffers_count );
        d->input_buffer = smem_new( d->input_buffer_size * frame_size );
        smem_zero( d->input_buffer );
        d->input_buffer2 = smem_new( frames * frame_size );
        smem_zero( d->input_buffer2 );
        d->input_buffer2_is_empty = true;
    }
}

static void remove_input_buffers( sundog_sound* ss )
{
    device_sound* d = (device_sound*)ss->device_specific;
    smem_free( d->input_buffer );
    smem_free( d->input_buffer2 );
    d->input_buffer = 0;
    d->input_buffer2 = 0;
}

#endif //...WITH_COMMON_INPUT_FUNCTIONS

#endif //...COMMON_CODE
