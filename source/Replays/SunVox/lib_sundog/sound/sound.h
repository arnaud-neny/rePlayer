#pragma once

//Structures and defines:

//default = option is missing (or commented out) in the config file;
#define APP_CFG_SND_BUF			"buffer" //sound buffer size (in frames); default = auto;
#define APP_CFG_SND_FREQ		"frequency" //sample rate (frames per second); default = auto;
#define APP_CFG_SND_DEV			"audiodevice" //output sound device name; default = auto;
#define APP_CFG_SND_DEV_IN		"audiodevice_in" //input sound device name; default = auto;
#define APP_CFG_SND_DRIVER         	"audiodriver" //sound driver name; default = auto;
#define APP_CFG_MIDI_DRIVER          	"mididriver" //MIDI driver name; default = auto (corresponds to the sound driver);
#define APP_CFG_JACK_NO_DEF_IN		"jack_nodefin" //don't set default JACK input connections: default = auto; any value = don't set;
#define APP_CFG_JACK_NO_DEF_OUT		"jack_nodefout" //don't set default JACK output connections: default = auto; any value = don't set;
#define APP_CFG_JACK_DONT_RESTORE_MIDIIN "jack_drmin" //don't restore JACK MIDI IN connections: default = auto; any value = don't restore;

#define SUNDOG_SOUND_SLOTS			16
#define SUNDOG_SOUND_DEFAULT_TIMEOUT_MS		400
#define SUNDOG_MIDI_PORTS			64

//Sound stream flags:
#define SUNDOG_SOUND_FLAG_USER_CONTROLLED	( 1 << 0 ) //Offline mode: user calls user_controlled_sound_callback() to get the next piece of sound stream
#define SUNDOG_SOUND_FLAG_ONE_THREAD		( 1 << 1 )
#define SUNDOG_SOUND_FLAG_DEFERRED_INIT		( 1 << 2 ) //Deferred device_sound_init(); must be supported by device-dependent part

//Sound callback flags:
#define SUNDOG_SOUND_CALLBACK_FLAG_DONT_LOCK	( 1 << 0 )

//MIDI client flags:
#define MIDI_CLIENT_FLAG_PORTS_MUTEX		( 1 << 0 ) //Thread-safe access to the ports (new, remove, read, write)
#define MIDI_CLIENT_FLAG_INITIALIZED		( 1 << 1 )

//MIDI port flags (for open_port() and get_devices()):
#define MIDI_PORT_READ 				( 1 << 0 ) //open for reading
#define MIDI_PORT_WRITE 			( 1 << 1 ) //open for writing
#define MIDI_NO_DEVICE 				( 1 << 2 )

//Sound stream capture flags:
#define SCAP_FLAG_INPUT				( 1 << 0 )

//Sample type conversion:
#define SMP_FLOAT32_TO_INT16( res, val ) \
{ \
    float temp = val; \
    temp *= 32768.0f; \
    if( temp > 32767.0f ) \
        temp = 32767.0f; \
    if( temp < -32768.0f ) \
        temp = -32768.0f; \
    res = (int16_t)temp; \
}
#define SMP_FLOAT32_TO_INT24( res, val ) \
{ \
    float temp = val; \
    temp *= 8388608.0F; \
    if( temp > 8388607.0F ) \
        temp = 8388607.0F; \
    if( temp < -8388608.0F ) \
        temp = -8388608.0F; \
    res = (int32_t)temp; \
}
#define SMP_FLOAT32_TO_INT32( res, val ) \
{ \
    float temp = val; \
    temp *= 2147483648.0F; \
    if( temp > 2147483647.0F ) \
        temp = 2147483647.0F; \
    if( temp < -2147483648.0F ) \
        temp = -2147483648.0F; \
    res = (int32_t)temp; \
}
#define SMP_INT32_TO_INT16( res, val ) { res = (val) >> 16; }
#define SMP_INT32_TO_FLOAT32( res, val ) { res = (float)(val) / 2147483648.0F; }
#define SMP_INT24_TO_FLOAT32( res, val ) { res = (float)(val) / 8388608.0F; }
#define SMP_INT16_TO_FLOAT32( res, val ) { res = (float)(val) / 32768.0F; }
#define SMP_INT16_TO_INT32( res, val ) { res = (int32_t)(val) << 16; }
#define SMP_INT8_TO_FLOAT32( res, val ) { res = (float)(val) / 128.0F; }

enum sound_buffer_type
{
    sound_buffer_default,
    sound_buffer_int16,
    sound_buffer_float32,
    sound_buffer_max
};
extern int g_sample_size[ sound_buffer_max ];

struct sundog_sound;
typedef int (*sundog_sound_slot_callback_t)( sundog_sound* ss, int slot );
/*
callback retval:
    0 : silence, buffer is not filled;
    1 : buffer is filled;
    2 : silence, buffer is filled;
*/

struct sundog_sound_slot
{
    sundog_sound_slot_callback_t	callback;
    void*				user_data;

    void*		in_buffer;
    void*		buffer;
    int 		frames;
    int			out_buf_ptr; //can be used for slot_sync (slot_sync = out_buf_ptr + user_callback_ptr + 1)
    stime_ticks_t	time; //output time

    bool		suspended;
    bool		wait_for_sync; //suspended until slot_sync
};

/*
  [ Level 1 ]
  .. | .. main buffer length .. | .. .. | .. .. | ..
     C1                         C2      C3      C4   : callbacks
     P0                         P1      P2      P3   : playback (P1 uses data from C1, P2 from C2, etc.)
  [ Level 2 - additional delay: latency compensation, system, driver, etc. ]
       L0                         L1      L2      L3
  [ Level 2.1, 2.2 ... - other possible delays, not counted by SunDog ]
  [ Level 3 - DAC output ]

  out_latency = Px - Cx (desired);
  out_latency2 = Lx - Cx (actual); required for correct MIDI event (with nonzero t) processing;
  out_time = L1; required for visualization and for correct MIDI event (with nonzero t) processing;
*/

//Latency for correct MIDI event (with nonzero t) processing:
#define SUNDOG_MIDI_LATENCY( out_latency, out_latency2 ) ( (out_latency) + (out_latency2) )

struct sundog_sound
{
    sundog_engine*      sd; //Parent SunDog engine (may be null)

    bool		initialized;
    bool		device_initialized;
    uint32_t 		flags; //SUNDOG_SOUND_FLAG_*
    int			freq;
    int			driver; //Driver ID (SDRIVER_*)
    void*		device_specific; //Device-specific data

    sundog_sound_slot	slots[ SUNDOG_SOUND_SLOTS ];
    int			slot_cnt; //number of active slots
    void*		slot_buffer;
    int			slot_buffer_size;
    int			slot_sync; //global (for all slots) sync signal (frame number + 1); can be assigned from the sound callback code only!

    sound_buffer_type	in_type;
    int			in_channels;
    void*		in_buffer;
    int			in_enabled;
    smutex		in_mutex;
    int                 in_request; // >0 - enable INPUT; ==0 - disable INPUT
    int                 in_request_answer; //Must be equal to input_request
    
    sound_buffer_type	out_type;
    int			out_channels;
    int                 out_latency; //desired output latency (frames); see description above
    int			out_latency2; //actual output latency (frames); see description above
    void*		out_buffer;
    int 		out_frames;
    stime_ticks_t	out_time; //output time; see description above
    
    sfs_file		out_file;
    uint32_t		out_file_flags; //SCAP_*
    uint32_t		out_file_size; //File size without WAV headers (in bytes)
    uint8_t*		out_file_buf;
    volatile size_t	out_file_buf_wp;
    volatile size_t	out_file_buf_rp;
    sthread		out_file_thread;
    volatile int	out_file_exit_request;

    smutex		mutex;
};

struct sundog_midi_event
{
    stime_ticks_t	t; //the time when the event should happen (or already happened);
                           //actual time depends on the receiver that can delay the events;
                           //0 = as soon as possible (unknown time);
    int			size;
    uint8_t*		data;
};

struct sundog_midi_port
{
    bool		active;
    uint32_t		flags; //MIDI_PORT_*
    char*		port_name;
    char*		dev_name;
    void*		device_specific;
};

struct sundog_midi_client
{
    sundog_engine*      sd; //Parent SunDog engine (may be null)
    sundog_sound* 	ss; //Parent sound stream (may be null)

    uint32_t		flags; //MIDI_CLIENT_FLAG_*
    sundog_midi_port*	ports[ SUNDOG_MIDI_PORTS ];
    volatile int	ports_cnt; //Active region of ports (for example, for midi_client_thread)
    char*		name;
    int			driver; //Driver ID (MDRIVER_*)
    void*		device_specific; //Device-specific data

    smutex 		ports_mutex; //Thread-safe access to the ports (new, remove, read, write);
                            	     //for device-dependent (internal) event threads - use other mutexes;
    bool 		midi_in_activity;
    bool 		midi_out_activity;
};

//Variables:

#ifndef SUNDOG_MODULE
    //AUDIOBUS ONLY?
    extern volatile int g_snd_play_request; //optional; play request from some audio framework
    extern volatile int g_snd_stop_request; //optional
    extern volatile int g_snd_rewind_request; //optional
    extern volatile bool g_snd_play_status; //optional
#endif

//Functions (sound stream):

//Call it if you have your own audio thread:
int user_controlled_sound_callback( sundog_sound* ss, void* buffer, int frames, int latency, stime_ticks_t output_time );
int user_controlled_sound_callback(
    sundog_sound* ss,
    void* out_buffer,
    int frames, int latency, stime_ticks_t out_time,
    sound_buffer_type in_type,
    int in_channels,
    void* in_buffer );

//Main sound callback + slot mixer (sound_player.cpp):
//can be called automatically (from device_sound_*) or by user (from user_controlled_sound_callback);
//retval: 0 - buffer is filled with zeros; 1 - buffer is filled with some signal;
int sundog_sound_callback( sundog_sound* ss, uint32_t flags );

int sundog_sound_global_init();
int sundog_sound_global_deinit();
//Sound stream is an object associated with the selected sound card (driver->device);
//you can use it for the sound output and input (optional).
//Init sound stream:
//  freq - desired sample rate or -1 (auto);
//  channels - desired number of channels or -1 (auto);
int sundog_sound_init( sundog_sound* ss, sundog_engine* sd, sound_buffer_type type, int freq, int channels, uint32_t flags );
int sundog_sound_init_deferred( sundog_sound* ss );
int sundog_sound_deinit( sundog_sound* ss ); //Deinit sound stream
int sundog_sound_get_free_slot( sundog_sound* ss );
void sundog_sound_set_slot_callback( sundog_sound* ss, int slot, sundog_sound_slot_callback_t callback, void* user_data );
sundog_sound_slot_callback_t sundog_sound_get_slot_callback( sundog_sound* ss, int slot );
void sundog_sound_remove_slot_callback( sundog_sound* ss, int slot );
void sundog_sound_lock( sundog_sound* ss );
void sundog_sound_unlock( sundog_sound* ss );
void sundog_sound_play( sundog_sound* ss, int slot );
void sundog_sound_stop( sundog_sound* ss, int slot );
int sundog_sound_is_slot_suspended( sundog_sound* ss, int slot );
void sundog_sound_slot_sync( sundog_sound* ss, int slot, int frame_number ); //Broadcast a sync signal (for other slots); can be called from the sound callback only!
void sundog_sound_sync_play( sundog_sound* ss, int slot, bool wait_for_sync ); //Wait (non-blocking) for the global sync signal and resume the audio stream on the specified slot
int sundog_sound_device_play( sundog_sound* ss ); //NOT FOR ALL SYSTEMS. Tested in iOS only!
void sundog_sound_device_stop( sundog_sound* ss ); //NOT FOR ALL SYSTEMS. Tested in iOS only!
void sundog_sound_input( sundog_sound* ss, bool enable ); //Call this function in the main app thread, where the sound stream is not locked. Or...
void sundog_sound_input_request( sundog_sound* ss, bool enable ); //... or use this function + sundog_sound_handle_input_requests() in the main thread
void sundog_sound_handle_input_requests( sundog_sound* ss );
const char* sundog_sound_get_driver_name( sundog_sound* ss );
const char* sundog_sound_get_driver_info( sundog_sound* ss );
const char* sundog_sound_get_default_driver();
int sundog_sound_get_drivers( char*** names, char*** infos );
int sundog_sound_get_devices( const char* driver, char*** names, char*** infos, bool input );
int sundog_sound_capture_start( sundog_sound* ss, const char* filename, uint32_t flags ); //capture sound to the WAV file
void sundog_sound_capture_stop( sundog_sound* ss );

//Functions (MIDI):

int sundog_midi_global_init();
int sundog_midi_global_deinit();
//MIDI client is an object with several MIDI IN/OUT ports connected to different MIDI devices:
//some clients (JACK,AUv3) can't work without the sound stream (ss);
int sundog_midi_client_open( sundog_midi_client* c, sundog_engine* sd, sundog_sound* ss, const char* name, uint32_t flags ); //Open MIDI client
int sundog_midi_client_close( sundog_midi_client* c ); //Close MIDI client
int sundog_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags );
//MIDI ports:
//it's safe to open/close the port, when the other ports are active (reading/writing) in other thread;
//dev_name: MIDI device name, or "public port" (virtual port, visible for all other apps);
int sundog_midi_client_open_port( sundog_midi_client* c, const char* port_name, const char* dev_name, uint32_t flags );
int sundog_midi_client_reopen_port( sundog_midi_client* c, int pnum );
int sundog_midi_client_close_port( sundog_midi_client* c, int pnum );
sundog_midi_event* sundog_midi_client_get_event( sundog_midi_client* c, int pnum );
int sundog_midi_client_next_event( sundog_midi_client* c, int pnum ); //only after successful get_event()
int sundog_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt );

int device_midi_client_open( sundog_midi_client* c, const char* name );
int device_midi_client_close( sundog_midi_client* c );
int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags );
int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags );
int device_midi_client_close_port( sundog_midi_client* c, int pnum );
sundog_midi_event* device_midi_client_get_event( sundog_midi_client* c, int pnum );
int device_midi_client_next_event( sundog_midi_client* c, int pnum );
int device_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt );

//Device dependent part:

int device_sound_init( sundog_sound* ss );
int device_sound_deinit( sundog_sound* ss );
void device_sound_input( sundog_sound* ss, bool enable );
int device_sound_get_devices( const char* driver, char*** names, char*** infos, bool input );

int device_midi_client_open( sundog_midi_client* c, const char* name );
int device_midi_client_close( sundog_midi_client* c );
int device_midi_client_get_devices( sundog_midi_client* c, char*** devices, uint32_t flags );
int device_midi_client_open_port( sundog_midi_client* c, int pnum, const char* port_name, const char* dev_name, uint32_t flags );
int device_midi_client_close_port( sundog_midi_client* c, int pnum );
sundog_midi_event* device_midi_client_get_event( sundog_midi_client* c, int pnum );
int device_midi_client_next_event( sundog_midi_client* c, int pnum );
int device_midi_client_send_event( sundog_midi_client* c, int pnum, sundog_midi_event* evt );
