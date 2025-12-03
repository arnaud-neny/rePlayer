#pragma once

#include "psynth/psynth_net.h"

#define SUNVOX_ENGINE_VERSION ( ( 2 << 24 ) | ( 1 << 16 ) | ( 4 << 8 ) | ( 0 << 0 ) )
#define SUNVOX_ENGINE_VERSION_STR "2.1.4" // rePlayer

//Main external defines:
//SUNVOX_LIB - cropped version (portable library) with some limitations (no WAV export, no recording);
//SUNVOX_GUI - full version with GUI functions;
//PS_STYPE_* - sample type;

#define MAX_PLAYING_PATS	64 //number of simultaneously playing patterns & number of supertracks
#if HEAPSIZE <= 32
    #define MAX_USER_COMMANDS	256
    #define MAX_USER_COMMANDS_FOR_METAMODULE	256
    #define MAX_UI_COMMANDS	256
    #define MAX_KBD_EVENTS	256
#else
    #define MAX_USER_COMMANDS	512
    #define MAX_USER_COMMANDS_FOR_METAMODULE	256
    #define MAX_UI_COMMANDS	512
    #define MAX_KBD_EVENTS	512
#endif
#define MAX_KBD_SLOTS		64
#define REC_BUF_BYTES		(16*1024)
#define MAX_PATTERN_TRACKS_BITS	5
#define MAX_PATTERN_TRACKS	32
typedef uint32_t 		sv_pat_track_bits; //Pattern tracks (bit per track): 1 - after noteON; 0 - after noteOFF;
#define MODULE_LAYERS_BITS	3
#define MAX_MODULE_LAYERS	( 1 << MODULE_LAYERS_BITS )
#define SUNVOX_TPB		24 //ticks per beat

extern PS_RETTYPE psynth_drumsynth( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_fm( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_fm2( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_generator( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_generator2( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_input( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_kicker( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_vplayer( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_sampler( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_spectravoice( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_amplifier( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_compressor( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_dc_blocker( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_delay( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_distortion( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_echo( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_eq( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_fft( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_filter( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_filter2( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_flanger( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_lfo( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_loop( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_modulator( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_pitch_shifter( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_reverb( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_smooth( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_vfilter( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_vibrato( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_waveshaper( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_adsr( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_ctl2note( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_feedback( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_glide( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_gpio( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_metamodule( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_multictl( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_multisynth( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_pitch2ctl( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_pitch_detector( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_sound2ctl( PSYNTH_MODULE_HANDLER_PARAMETERS );
extern PS_RETTYPE psynth_velocity2ctl( PSYNTH_MODULE_HANDLER_PARAMETERS );

extern PS_RETTYPE ( *g_psynths [] )( PSYNTH_MODULE_HANDLER_PARAMETERS ); //All SunVox built-in modules
extern PS_RETTYPE ( *g_psynths_eff [] )( PSYNTH_MODULE_HANDLER_PARAMETERS ); //Effects that can be used inside the other modules
extern const int g_psynths_num;
extern int g_psynths_eff_num;

enum 
{
    SUNVOX_ACTION_PAT_CHANGE_NOTE = 0,
    SUNVOX_ACTION_PAT_CHANGE_NOTE_WITH_SCROLLDOWN,
    SUNVOX_ACTION_PAT_CHANGE_NOTES,
    SUNVOX_ACTION_PAT_INSERT,
    SUNVOX_ACTION_PAT_BACKSPACE,
    SUNVOX_ACTION_PAT_OP,
    SUNVOX_ACTION_PAT_CLEAR,
    SUNVOX_ACTION_PAT_SHRINK,
    SUNVOX_ACTION_PAT_EXPAND,
    SUNVOX_ACTION_PAT_SLICE,
    SUNVOX_ACTION_PAT_CHANGE_TRACK,
    SUNVOX_ACTION_PAT_CHANGE_SIZE,
    SUNVOX_ACTION_PAT_CHANGE_ICON,
    SUNVOX_ACTION_PAT_CHANGE_ICON_COLOR,
    SUNVOX_ACTION_PAT_CHANGE_NAME,
    SUNVOX_ACTION_PAT_XOR_FLAGS,
    SUNVOX_ACTION_PATS_CHANGE_POSITION,
    SUNVOX_ACTION_PATS_UNMUTE_ALL,
    SUNVOX_ACTION_PATS_INFO_XOR_FLAGS,
    SUNVOX_ACTION_PAT_NEW,
    SUNVOX_ACTION_PATS_DEL,
    SUNVOX_ACTION_PATS_CLONE,
    SUNVOX_ACTION_PATS_DETACH,
    SUNVOX_ACTION_PATS_INJECT,
    SUNVOX_ACTION_MODULES_MOVE,
    SUNVOX_ACTION_MODULE_SET_FLAGS,
    SUNVOX_ACTION_MODULES_UNMUTE_ALL,
    SUNVOX_ACTION_MODULE_SET_FINETUNE,
    SUNVOX_ACTION_MODULE_SET_NAME,
    SUNVOX_ACTION_MODULE_SET_COLOR,
    SUNVOX_ACTION_MODULE_SET_Z,
    SUNVOX_ACTION_MODULE_SET_SCALE,
    SUNVOX_ACTION_MODULE_SET_VISUAL_PARS,
    SUNVOX_ACTION_MODULE_SET_MIDI_IN_FLAGS,
    SUNVOX_ACTION_MODULE_MAKE_LINK, //par[0] -> par[1]
    SUNVOX_ACTION_MODULE_MAKE_LINKS, //data[]: 0(mod),1(slot) -> 2(mod),3(slot) ; ...
    SUNVOX_ACTION_MODULE_REMOVE_LINKS, //data[]: 0(mod),1(slot) -> 2(mod),3(slot) ; ...
    SUNVOX_ACTION_MODULE_NEW,
    SUNVOX_ACTION_MODULES_DEL,
    SUNVOX_ACTION_MODULE_BEFORECHANGE, //before some module changes (ctls, internal data, etc.)
    SUNVOX_ACTION_MODULE_AFTERLOAD,
    SUNVOX_ACTION_MODULE_EXCHANGE,
    SUNVOX_ACTION_CTL_SET_MIDI,
    SUNVOX_ACTION_PROJ_AFTERMERGE, //Save SunVox items (modules and patterns) with ...JUST_LOADED flag (will be cleared);
				   //pars[]: active module or -1; active pat or -1;
    SUNVOX_ACTION_PROJ_SET_RESTART_POSITION,
    SUNVOX_ACTION_PROJ_SET_SYNC_FLAGS,
    SUNVOX_ACTION_PROJ_TOGGLE_SUPERTRACKS,
    SUNVOX_ACTION_PROJ_TOGGLE_SUPERTRACK_MUTE,
    SUNVOX_ACTIONS,
};
//Don't forget to change STR_SV_ACTION_* strings in SunVox code!

#define NOTECMD_NOTE_OFF	128
#define NOTECMD_ALL_NOTES_OFF	129 // send "note off" to all modules; for SunVox library only?
#define NOTECMD_CLEAN_MODULES	130 // stop all modules - clear their internal buffers and put them into standby mode
#define NOTECMD_STOP		131
#define NOTECMD_PLAY		132
#define NOTECMD_SET_PITCH	133 // set pitch ctl_val
#define NOTECMD_PREV_TRACK	134 // apply effects to the previous track
#define NOTECMD_PATPLAY_OFF	135 // disable single pattern play mode (without STOP)
//don't use these command from the pattern (jump_request variable must be used): ===============
#define NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP 136 	// prepare for an immediate jump to line XXYY;
						// use it ONLY before the NOTECMD_PLAY!
#define NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2 141 // ... for negative t
#define NOTECMD_JUMP		137 // jump to line XXYY
//==============================================================================================
#define NOTECMD_CLEAN_MODULE	140 // stop the module - clear its internal buffers and put it into standby mode
//#define NOTECMD_*		142

struct sunvox_note
{
    uint8_t	    	note;		//NN: 0 - nothing; 1..127 - note num + 1; 128 - note off; 129, 130... - see NOTECMD_* defines
    uint8_t	    	vel;		//VV: velocity 1..129; 0 - default
    uint16_t	    	mod;		//MM: 0 - nothing; 1..65535 - module number + 1
    uint16_t	    	ctl;		//0xCCEE: CC: 1..127 - controller number + 1; 128... - MIDI controller + 128 
					//        EE: std effect
    uint16_t	    	ctl_val;	//0xXXYY: controller value or effect parameter
};

#ifdef OPENGL
    #define SUNVOX_ICON_MAP_XSIZE 1024
#else
    #define SUNVOX_ICON_MAP_XSIZE 256
#endif

#define SUNVOX_VIRTUAL_PATTERN			0xFFFF
#define SUNVOX_VIRTUAL_PATTERN_GPIO		(SUNVOX_VIRTUAL_PATTERN-1)
#define SUNVOX_VIRTUAL_PATTERN_CTL2NOTE		(SUNVOX_VIRTUAL_PATTERN-2)
#define SUNVOX_VIRTUAL_PATTERN_PITCH_DETECTOR	(SUNVOX_VIRTUAL_PATTERN-3)
//If you want to use values > 0xFFFF - change the psynth_event->id structure.

#define SUNVOX_PATTERN_FLAG_NO_ICON		( 1 << 0 )
#define SUNVOX_PATTERN_FLAG_NO_NOTES_OFF	( 1 << 1 )
#define SUNVOX_PATTERN_DEFAULT_YSIZE( SV )	( 32 )
#define SUNVOX_PATTERN_DEFAULT_LINES( SV )	( SV->tgrid * SV->tgrid2 * 2 )
#define SUNVOX_PATTERN_DEFAULT_TRACKS( SV )	( 4 )

struct sunvox_pattern_short
{
    size_t		data_size;
    int			data_xsize;
    int			data_ysize;

    uint32_t		id;

    int			channels;
    int			lines;
    int			ysize;

    uint32_t 		flags;

    size_t		name_size;

    uint16_t	   	icon[ 16 ];
    uint8_t	   	fg[ 3 ];
    uint8_t	    	bg[ 3 ];

    //... other sunvox_pattern data will not be saved in the UNDO stack ...
};

struct sunvox_pattern
{
    sunvox_note*        data;
    int		    	data_xsize;	//Number of tracks. May be != channels
    int		    	data_ysize; 	//Number of lines.  May be != lines

    uint32_t 		id; 		//unique pattern ID; will not be saved with the project

    int		    	channels;	//Number of tracks (visible for user)
    int		    	lines;		//Number of lines (visible for user)
    int		    	ysize;

    uint32_t		flags;

    char*		name;

    uint16_t	   	icon[ 16 ]; 	//Pattern icon 16x16
    uint8_t	   	fg[ 3 ];	//Foreground color
    uint8_t	    	bg[ 3 ];	//Background color
    int			icon_num;	//Icon number in the map
};

#define SUNVOX_PATTERN_INFO_FLAG_CLONE		( 1 << 0 )
#define SUNVOX_PATTERN_INFO_FLAG_SELECTED	( 1 << 1 )
#define SUNVOX_PATTERN_INFO_FLAG_UNUSED		( 1 << 2 )
#define SUNVOX_PATTERN_INFO_FLAG_MUTE		( 1 << 3 )
#define SUNVOX_PATTERN_INFO_FLAG_SOLO		( 1 << 4 )
//The following flags will not be saved:
#define SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED	(unsigned)( 1 << 30 ) //MUST BE cleared automatically!
#define SUNVOX_PATTERN_INFO_FLAG_SELECTED2	(unsigned)( 1 << 31 ) //MUST BE cleared automatically!
#define SUNVOX_PATTERN_INFO_SAVE_FLAGS_MASK	(unsigned)( ~( SUNVOX_PATTERN_INFO_FLAG_JUST_LOADED | SUNVOX_PATTERN_INFO_FLAG_SELECTED2 ) )
//SUNVOX_PATTERN_INFO_FLAG_SELECTED2 - temporary selection not visible for the user; used in:
// timeline editor (delete patterns).

struct sunvox_pattern_info_short
{
    uint32_t	    	flags;
    int		    	parent_num;
    int		    	x, y;
};

struct sunvox_pattern_info
{
    uint32_t	    	flags;
    int		    	parent_num;
    int		    	x, y;			//Pattern position: X - line; Y - supertrack/32
    int		    	start_x, start_y;	//Used in timeline editor when user drags the pattern
    int		    	state_ptr;		//Pointer to structure in pat_state[] array
    sv_pat_track_bits	track_status; 		//Pattern tracks (bit per track): 1 - after noteON; 0 - after noteOFF;
};

#define EFF_FLAG_TONE_PORTA			( 1 << 0 )
#define EFF_FLAG_TONE_VIBRATO			( 1 << 1 )
#define EFF_FLAG_ARPEGGIO_IN_PREVIOUS_TICK	( 1 << 2 )
#define EFF_FLAG_TIMER_RETRIG			( 1 << 3 )
#define EFF_FLAG_TIMER_CUT			( 1 << 4 ) //note off
#define EFF_FLAG_TIMER_CUT2			( 1 << 5 ) //volume 0
#define EFF_FLAG_TIMER_DELAY			( 1 << 6 )
#define EFF_FLAG_TIMER_HANDLING			( 1 << 7 )
#define EFF_FLAG_EVT_PITCH			( 1 << 8 ) //evt_pitch defined (some note selected in the event)
#define EFF_FLAG_PITCH_CHANGED			( 1 << 9 ) //cur_pitch changed
#define EFF_FLAG_VEL_CHANGED			( 1 << 10 ) //cur_vel changed

struct sunvox_track_eff
{
    int			evt_pitch0; //Same as evt_pitch, but unchanged by 05/06 (pitch up/down) effects
    int			evt_pitch; //Initial pitch described by the event - you can use it later (in eff.03) during processing the cmd "prev.track"
    int		    	cur_pitch; //Current pitch without vibrato and arpeggio effects
    int	    		target_pitch; //for EFF_FLAG_TONE_PORTA
    uint16_t	    	porta_speed; //per tick; semitone = 256
    uint16_t	    	flags;
    int16_t	    	cur_vel;
    int16_t	    	vel_speed;
    uint16_t	    	arpeggio;
    uint8_t		vib_amp;
    uint8_t		vib_freq;
    int			vib_phase;
    uint8_t		timer; //0...33 - tick counter; 0x40...0x5F - in percents (request only; will be converted to ticks);
    uint8_t		timer_init; //initial timer value (for the Retrigger effect 19)
};

struct sunvox_pattern_state
{
    sunvox_track_eff	effects[ MAX_PATTERN_TRACKS ];
    sv_pat_track_bits	track_status; //Pattern tracks (bit per track): 1 - after noteON; 0 - after noteOFF;
    int16_t	    	track_module[ MAX_PATTERN_TRACKS ]; //Module number (for noteON/noteOFF) for each channel; -1 = undefined
    bool 		busy;
    uint8_t		mutable_tracks; //The number of tracks, the state of which may change at the moment (current line)
};
#if MAX_PATTERN_TRACKS > 32
    #error Not enough bits for tracks in sunvox_pattern_state, PSYNTH_EVT_ID_INC() and sunvox_pattern_info
#endif

#define SUNVOX_KBD_FLAG_ON			( 1 << 0 ) //note ON / OFF
#define SUNVOX_KBD_FLAG_NOREPEAT		( 1 << 1 ) //don't play the same repeated note ON (without note OFF) on the same slot; just ignore it;
							   //not compatible with SUNVOX_KBD_VTYPE_PITCH;
//#define SUNVOX_KBD_FLAG_ALWAYS_SET_VELOCITY	( 1 << 2 )
#define SUNVOX_KBD_FLAG_SPECIFIC_MODULE_ONLY	( 1 << 3 ) //only for .mod; otherwise the slots will not be divided by mod_num
#define SUNVOX_KBD_FLAG_PLAY_ONLY		( 1 << 4 ) //no edit/save for this event
#define SUNVOX_KBD_FLAG_TYPE_OFFSET		5
#define SUNVOX_KBD_FLAG_TYPE_BITS		3
#define SUNVOX_KBD_FLAG_TYPE_MASK		( ( ( 1 << SUNVOX_KBD_FLAG_TYPE_BITS ) - 1 ) << SUNVOX_KBD_FLAG_TYPE_OFFSET )
    #define SUNVOX_KBD_TYPE_PC				0 //PC keyboard (default)
    #define SUNVOX_KBD_TYPE_ONSCREEN			1 //on-screen keyboard
    #define SUNVOX_KBD_TYPE_THEREMIN			2 //Touch Theremin (on-screen)
    #define SUNVOX_KBD_TYPE_MIDI			3 //external MIDI source
    #define SUNVOX_KBD_TYPE_BRUSH			4 //brush preview
#define SUNVOX_KBD_FLAG_VTYPE_OFFSET		8
#define SUNVOX_KBD_FLAG_VTYPE_BITS		2
#define SUNVOX_KBD_FLAG_VTYPE_MASK		( ( ( 1 << SUNVOX_KBD_FLAG_VTYPE_BITS ) - 1 ) << SUNVOX_KBD_FLAG_VTYPE_OFFSET )
    #define SUNVOX_KBD_VTYPE_NOTE			0 //.v = note (default)
    #define SUNVOX_KBD_VTYPE_PITCH			1 //.v = pitch
#define SUNVOX_KBD_FLAG_CHANNEL_OFFSET		10
#define SUNVOX_KBD_FLAG_CHANNEL_MASK		( ( MAX_PATTERN_TRACKS - 1 ) << SUNVOX_KBD_FLAG_CHANNEL_OFFSET )
#define SUNVOX_KBD_FLAG_FTRACK_FIRST		( 1 << ( SUNVOX_KBD_FLAG_CHANNEL_OFFSET + MAX_PATTERN_TRACKS_BITS ) )
#define SUNVOX_KBD_FLAG_FTRACK_OFFSET		( SUNVOX_KBD_FLAG_CHANNEL_OFFSET + MAX_PATTERN_TRACKS_BITS + 1 )
#define SUNVOX_KBD_FLAG_FTRACK_MASK		( ( MAX_PATTERN_TRACKS - 1 ) << SUNVOX_KBD_FLAG_FTRACK_OFFSET )
/*
CHANNEL:
  if vtype == NOTE: channel = source ID or MIDI channel;
  if vtype == PITCH: channel = voice number;
FTRACK_FIRST: (0/1) first track of the new chord (virtuat_pat was empty before this note)
FTRACK: final virtual_pat track number (assigned in sunvox_handle_kbd_event()).
*/

struct sunvox_kbd_event
{
    int 		v; //note number or some other value (see the flags)
    uint8_t 		vel; //0...128
    uint16_t 		mod;
    uint 		flags;
    stime_ticks_t 	t; //0 = as soon as possible
};

struct sunvox_kbd_slot
{
    int			v;
    uint16_t		mod;
    uint		flags;
    int8_t		track; //Track number (in virtual_pat) for note/voice
    bool		active;
};

struct sunvox_kbd_events
{
    sunvox_kbd_event	events[ MAX_KBD_EVENTS ];
    int		    	events_rp;
    int		    	events_wp;
    int			prev_track; //For free pattern track searching (for keyboard notes only)
    int			prev_event_line; //Line number for the prev. event (for keyboard notes only)
    sunvox_kbd_slot	slots[ MAX_KBD_SLOTS ];
    uint		slots_count;
    uint		vel; //global keyboard velocity (normal = 128)
};

struct sunvox_psynth_event
{
    uint16_t mod;
    psynth_event evt;
};

struct sunvox_user_cmd
{
    sunvox_note n;
    uint8_t ch;
    stime_ticks_t t; //0 = now
};

enum sunvox_ui_evt_type //some event from SunVox to UI
{
    SUNVOX_UI_EVT_KBD, //keyboard event
    SUNVOX_UI_EVT_STATE, //state changed
    SUNVOX_UI_EVT_WMEVT, //send event to WM
};

enum sunvox_state
{
    SUNVOX_STATE_PLAY, //playing=1 (but audio rendering is not finished)
    SUNVOX_STATE_STOP, //playing=0 (...)
    SUNVOX_STATE_CLEAN_MODULES, //all modules will be cleaned very soon (...)
    SUNVOX_STATE_PATPLAY_OFF,
};

struct sunvox_wmevt
{
    uint16_t            type;
    int16_t             x;
    int16_t             y;
    uint16_t            key;
    uint16_t            pressure;
};

union sunvox_ui_evt_data
{
    sunvox_kbd_event 	kbd;
    sunvox_state 	state;
    sunvox_wmevt	wmevt;
};

struct sunvox_ui_evt
{
    sunvox_ui_evt_type type;
    sunvox_ui_evt_data data;
};

#define REC_FILE_NAME           		"3:/sunvox_rec_data"
#define WRITE_TIME_FLAG_AUTOTIME		( 1 << 0 )
#define REC_EVT_PAT_OFFSET			3
enum //pattern types for recording
{
    REC_PAT_KEYBOARD = 0,
    REC_PAT_THEREMIN,
    REC_PAT_CONTROLLERS,
    REC_PAT_SOUND2CTL,
    REC_PAT_CTL2NOTE,
    REC_PAT_PITCH_DETECTOR,
    REC_NUM_PATS
};
enum //events
{
    rec_evt_noteon = 0,
    rec_evt_noteoff,
    rec_evt_pitchon,
    rec_evt_pitchslide,
    rec_evt_ctl,
    rec_num_evts
};

#define SUNVOX_MIDI_IN_PORTS 		( 4 + 1 ) //last one - public port
#define SUNVOX_MIDI_IN_PORT_PUBLIC	( SUNVOX_MIDI_IN_PORTS - 1 ) //public port (other apps can bind it)
extern const char* g_sunvox_midi_in_keys[]; //profile key names for MIDI IN devices
extern const char* g_sunvox_midi_in_ch_keys[]; //profile key names for MIDI IN channels
extern const char* g_sunvox_midi_in_names[];

struct sunvox_midi
{
    int			kbd[ SUNVOX_MIDI_IN_PORTS ]; //MIDI port (client is defined in psynth_net)
    int8_t		kbd_ch[ SUNVOX_MIDI_IN_PORTS ]; //MIDI channel (-1 = any channel)
    uint8_t		kbd_status[ SUNVOX_MIDI_IN_PORTS ]; //MIDI status byte
    char		kbd_prev_ctl[ SUNVOX_MIDI_IN_PORTS ];
    uint16_t		kbd_ctl_rpn[ SUNVOX_MIDI_IN_PORTS ];
    uint16_t		kbd_ctl_nrpn[ SUNVOX_MIDI_IN_PORTS ];
    int16_t		kbd_ctl_data[ SUNVOX_MIDI_IN_PORTS ];
    bool		kbd_ignore_velocity;
    int			kbd_octave_offset;
    int			kbd_sync; //accept external MIDI sync: -1 - all ports; 0... - port number; 256 - none;
    psynth_midi_evt	kbd_cap[ 4 ];
    volatile int	kbd_cap_req; //number of events to capture
    volatile int	kbd_cap_cnt; //number of events captured
};

/*
MIDI IN:

Commands coming from all external MIDI devices are combined into a single stream, and modules decide which commands to accept from this stream.
In the module properties, you can specify what to accept (all MIDI commands at once, or only commands from a selected MIDI channel)
and when to accept them (always, never, or when a module is selected).

Limitation:
commands from external MIDI devices, commands from on-screen keyboards, and commands from a physical PC keyboard
all work with a single set of 32 tracks. This means the total polyphony for all musical input devices is limited to 32.

MIDI OUT:

Each module can open one MIDI OUT port to send MIDI commands to the specified device. See MIDI OUT in the module properties.
The polyphony for MIDI OUT per module is 16.

Limitation for Android:
the same MIDI device can't be opened twice; that is, only one module can send commands to a single MIDI device.
*/

//Visualization frames:
//SUNVOX_VF_BUF_SRATE - frames per buffer (buffer length = 1 second)
#if CPUMARK >= 10
    #define SUNVOX_VF_BUF_SRATE		128
#else
    #define SUNVOX_VF_BUF_SRATE		64
#endif
#define SUNVOX_VF_BUFS			4
#define SUNVOX_VF_BUFS_MASK		( SUNVOX_VF_BUFS - 1 )

struct sunvox_engine;
enum sunvox_stream_command
{
    SUNVOX_STREAM_LOCK,
    SUNVOX_STREAM_UNLOCK,
    SUNVOX_STREAM_STOP,
    SUNVOX_STREAM_PLAY,
    SUNVOX_STREAM_SYNC_PLAY, //wait (non-blocking) for the sync signal (from any slot) and resume the audio stream
    SUNVOX_STREAM_SYNC, //broadcast a sync signal (for other slots)
    SUNVOX_STREAM_ENABLE_INPUT,
    SUNVOX_STREAM_DISABLE_INPUT,
    SUNVOX_STREAM_IS_SUSPENDED, //is the audio stream suspended? (if true, we don't have to wait for some operations to finish)
};
typedef int (*tsunvox_sound_stream_control)( sunvox_stream_command cmd, void* user_data, sunvox_engine* s );

#define SUNVOX_SYNC_FLAG_MIDI1		( 1 << 0 ) //receive Start/Stop/Continue from MIDI
#define SUNVOX_SYNC_FLAG_MIDI2		( 1 << 1 ) //receive Tempo (clock) from MIDI
#define SUNVOX_SYNC_FLAG_MIDI3		( 1 << 2 ) //receive Position changes from MIDI
#define SUNVOX_SYNC_FLAG_OTHER1		( 1 << 3 ) // ... from other sources
#define SUNVOX_SYNC_FLAG_OTHER2		( 1 << 4 ) // ...
#define SUNVOX_SYNC_FLAG_OTHER3		( 1 << 5 ) // ...
#define SUNVOX_SYNC_FLAGS_DEFAULT	( SUNVOX_SYNC_FLAG_MIDI1 | SUNVOX_SYNC_FLAG_OTHER1 )

#define SUPERTRACK_BITARRAY_SIZE	( ( MAX_PLAYING_PATS + 31 ) / 32 )

struct sunvox_engine
{
    WINDOWPTR		win; //base window (optional);
    sundog_sound*	ss; //base sound stream (optional);

    volatile int    	initialized;

    uint32_t		flags; //SUNVOX_FLAG_* ; it's not recommended to change these flags directly from the UI thread! At least use LOCK/UNLOCK
    uint32_t		sync_flags;
    int 		freq;

    int			level1_offset;

    tsunvox_sound_stream_control	stream_control;
    void*				stream_control_data;
    int					stream_control_par_sync; //level1 + level2 frame number for SUNVOX_STREAM_SYNC

    uint		base_version; //SunVox version, in which the project was created;

    sunvox_midi*	midi;

    uint8_t		midi_import_tpl;
    bool		midi_import_quantization;

    volatile int    	playing;
    volatile int	recording;
    stime_ms_t	    	start_time; //Time in ms
    stime_ms_t	    	cur_time; //
    int		    	single_pattern_play; //Number of pattern or -1.
    int		    	next_single_pattern_play; //Number of pattern or -1.
    int			restart_pos; //Project restart position
    bool	    	stop_at_the_end_of_proj;

    int8_t		sync_mode; //0 - internal; 1 - external (slave) MIDI clock;
    int			sync_cnt; //number of ticks to process
    int			sync_resolution; //number of frames per subtick (only for sync_mode!=0)
    uint	    	tick_counter; //From 0 to tick_size << 8
    int		    	line_counter; //Current line
    int		    	speed_counter;
    bool		jump_request;
    uint8_t		jump_request_mode; 	//0 - absolute; (default)
						//1 - pat.begin + val;
						//2 - pat.begin - val;
						//3 - next line + val;
						//4 - next line - val;
    int			jump_request_line;

    uint16_t		prev_bpm;
    uint8_t		prev_speed;
    uint16_t	    	bpm; //Beats Per Minute (One beat = 4 lines (24 ticks) on TPL 6)
    uint8_t	    	speed; //Ticks Per line
    uint8_t		tgrid; //It is actually number of lines per beat, but it only affects the UI. In future verions tgrid+TPL will be replaced by LPB (lines per beat)
    uint8_t		tgrid2;
    int			pars_changed;
    int			tgrid_changed;
    
    char*          	proj_name;

    //For sunvox_sort_patterns() and sunvox_select_current_playing_patterns():
    int*                sorted_pats;
    int	    		sorted_pats_num;
    int		    	cur_playing_pats[ MAX_PLAYING_PATS ]; //Current active patterns; pat_num = sorted_pats[ cur_playing_pats[ p ] ]
    int		    	temp_pats[ MAX_PLAYING_PATS ];
    int		    	last_sort_pat; //Number of the last pattern (in sorted table) with X less then the cursor position

    int		    	proj_lines; //Project length (number of lines). Calculated in sunvox_sort_patterns()
    uint	    	proj_len; //Project length (in frames). Calculated in play(), play_from_beginning()

    //Handling events from the patterns:
    sunvox_pattern_state*	pat_state; //[ MAX_PLAYING_PATS ];
    int				pat_state_size; //pat_state[] capacity (maximum number of simultaneously playing patterns)
    uint32_t			supertrack_mute[ SUPERTRACK_BITARRAY_SIZE ]; //bits

    bool			pats_solo_mode;
    sunvox_pattern**	    	pats; //Empty areas (NULL patterns; or - holes) are ALLOWED. Be careful when using pattern REDO/UNDO operations...
    sunvox_pattern_info*    	pats_info;
    int				pats_num;
    uint			pat_id_counter;
    int				pat_num; //For pattern editor
    int				pat_track; //For pattern editor
    int				pat_line; //For pattern editor
    psynth_net*             	net;
    int				selected_module;
    int				last_selected_generator;
    int				module_scale; //1.0 = 256
    int				module_zoom; //1.0 = 256
    int				xoffset; //relative to window center
    int				yoffset; //relative to window center
    uint			layers_mask;
    int				cur_layer;

#ifdef SUNVOX_GUI
    sdwm_image*       		icons_map;
    int8_t*               	icons_flags;
    volatile bool		check_timeline_cursor;
    volatile bool 		center_timeline_request;
#endif

    sunvox_pattern_state	virtual_pat_state;
    sunvox_pattern_info		virtual_pat_info;
    int				virtual_pat_tracks; //number of active tracks in the virtual pattern

    //Any user commands (usually Stop/Play/TPL/BPM/Ctl/...) ->
    // -> sunvox_send_user_command() ->
    // -> SunVox Engine (handle command for the virtual_pat) ->
    // -> out_ui_events buffer for futher handling by UI:
    //(some events may be recorded)
    sring_buf*			user_commands; //sunvox_user_cmd 

    //    Notes from the external keyboards (UI THREAD: PC, ribbon/theremin, ...) ->
    // -> sunvox_send_kbd_event() ->
    // -> SunVox Engine : sunvox_handle_kbd_event() (final module events generation) ->
    // -> out_ui_events buffer for futher handling by UI (adding to the pattern, for example) ->
    // -> handle_kbd_out_event() in UI THREAD

    //    Notes from the MIDI keyboards (AUDIO THREAD) ->
    // -> SunVox Engine : sunvox_handle_kbd_event() (final module events generation) ->
    // -> out_ui_events buffer for futher handling by UI ->
    // -> handle_kbd_out_event() in UI THREAD

    //sunvox_handle_kbd_event() will distribute the incoming events into separate independent slots (voices):
    //slot0 -> trackX; slot1 -> trackX; ...

    //(some events may be recorded)
    //(in real time all such events will be mixed in the virtual_pat, but during REC they may be placed in different patterns)
    //(in case of lack of available tracks in virtual_pat - increase MAX_PATTERN_TRACKS or use different virtual_pats)

    sunvox_kbd_events*		kbd;

    sring_buf*			out_ui_events; //(sunvox_ui_evt) output for UI

    sunvox_psynth_event*	psynth_events; //Pack of events from another sound network. Used by MetaModule
    uint			psynth_events_count;
    int				pitch_offset; //Global pitch offset
    int				velocity; //Global velocity (0..256)

#ifndef SUNVOX_LIB
    uint8_t*		rec_bytes; //rec buffer
    bool		rec_no_bytes;
    volatile int	rec_rp;
    volatile int	rec_wp;
    int			rec_start_time; //line * 32 + line_position(0..31)
    int			rec_prev_event_time; //line * 32 ...
    int			rec_tracks[ REC_NUM_PATS ]; //Number of tracks for each pattern
    int			rec_start[ REC_NUM_PATS ]; //First event time (line number)
    int			rec_len[ REC_NUM_PATS ]; //Length (number of lines)
    bool		rec_data[ REC_NUM_PATS ]; //1 - bytes available
    int			rec_metronome;
    bool		rec_note_q;
    bool		rec_ctl_q;
    sthread		rec_thread;
    smutex		rec_mutex;
    int			rec_session_id;

    //Undo/redo manager:
    undo_data		undo;
#endif //!SUNVOX_LIB

    int			change_counter;
    int			prev_change_counter1; //SunVox engine
    int			prev_change_counter2; //PSynth engine

    bool 		cancel_export_to_wav;

    //Random generator:
    uint32_t 		rand_next;

    int			clipping_counter;

    //Visualization frames: (scope drawing, realtime proj pointers ...)
    uint8_t	    	f_volume_l          [ SUNVOX_VF_BUFS * SUNVOX_VF_BUF_SRATE ]; //Volume level
    uint8_t	    	f_volume_r          [ SUNVOX_VF_BUFS * SUNVOX_VF_BUF_SRATE ]; //Volume level
    int		    	f_line              [ SUNVOX_VF_BUFS * SUNVOX_VF_BUF_SRATE ]; //Line counter in the 27.5 fixed point format
    uint	    	f_buffer_size       [ SUNVOX_VF_BUFS ]; //for each audio callback: buf size (max = SUNVOX_VF_BUF_SRATE)
    stime_ticks_t    	f_buffer_start_time [ SUNVOX_VF_BUFS ];	//for each audio callback: buf output time
    int		    	f_current_buffer;

    int type; // rePlayer: 0:SunVox,1:XM,2:MOD,3:MIDI
};

struct sunvox_render_data
{
    sound_buffer_type	in_type;
    void*		in_buffer;
    int			in_channels;
    sound_buffer_type	buffer_type;
    void*		buffer;
    int			frames;
    int			channels;
    int			out_latency; //desired output latency (frames); see description in sundog_sound (sound.h)
    int			out_latency2; //actual output latency (frames); see description in sundog_sound (sound.h)
    stime_ticks_t		out_time; //output time; see description in sundog_sound (sound.h)
    sfs_sound_encoder_data**	out_file_encoders;
    bool		silence;
};

inline int SUNVOX_SOUND_STREAM_CONTROL( sunvox_engine* s, sunvox_stream_command cmd )
{
    int rv = 0;
    if( s && s->stream_control ) rv = s->stream_control( cmd, s->stream_control_data, s );
    return rv;
}

struct psynth_sunvox //SunVox object inside the PSynth module; use it in modules like MetaModule
{
    psynth_net*		pnet; //Parent PSynth network
    uint		mod_num; //Number of the parent module
    uint		flags; //SUNVOX_FLAG_* for init/deinit
    PS_STYPE*		temp_bufs[ PSYNTH_MAX_CHANNELS ];
    sunvox_engine**	s;
    uint		s_cnt; //Number of SunVox engines
    int			ui_reinit_request;
};

//Action handler:

int sunvox_action_handler( UNDO_HANDLER_PARS );

//Main Init/Deinit:

#define SUNVOX_FLAG_CREATE_PATTERN		( 1 << 0 )
#define SUNVOX_FLAG_CREATE_MODULES		( 1 << 1 )
#define SUNVOX_FLAG_DEFAULT_CONTENT		( SUNVOX_FLAG_CREATE_PATTERN | SUNVOX_FLAG_CREATE_MODULES )
#define SUNVOX_FLAG_DONT_START			( 1 << 2 )
#define SUNVOX_FLAG_PLAYER_ONLY			( 1 << 3 ) //no record, no undo buffer
#define SUNVOX_FLAG_NO_GUI			( 1 << 4 )
#define SUNVOX_FLAG_NO_SCOPE			( 1 << 5 )
#define SUNVOX_FLAG_NO_MIDI			( 1 << 6 )
#define SUNVOX_FLAG_NO_GLOBAL_SYS_EVENTS	( 1 << 7 )
#define SUNVOX_FLAG_NO_KBD_EVENTS		( 1 << 8 )
#define SUNVOX_FLAG_NO_MODULE_CHANNELS		( 1 << 9 ) //every module will be rendered manually with user-defined channel buffers
#define SUNVOX_FLAG_STATIC_TIMELINE		( 1 << 10 ) //timeline will not be changed anymore (patterns are sorted, proj_lines is filled)
#define SUNVOX_FLAG_DYNAMIC_PATTERN_STATE	( 1 << 11 )
#define SUNVOX_FLAG_ONE_THREAD			( 1 << 12 )
#define SUNVOX_FLAG_MAIN			( 1 << 13 )
#define SUNVOX_FLAG_IGNORE_SLOT_SYNC		( 1 << 14 ) //don't call SUNVOX_STREAM_SYNC (pattern effect 0x33)
#define SUNVOX_FLAG_SUPERTRACKS                 ( 1 << 15 ) //0 - classic timeline; 1 - supertracks (SunVox 2.0);
//#define SUNVOX_FLAG_DONT_STOP_ON_JUMP           ( 1 << ? ) //don't turn off all notes before jumping to a certain point (line number) in the Timeline
#define SUNVOX_FLAG_NO_TONE_PORTA_ON_TICK0	( 1 << 16 ) //for compatibility with old trackers
#define SUNVOX_FLAG_NO_VOL_SLIDE_ON_TICK0	( 1 << 17 ) //for compatibility with old trackers
#define SUNVOX_FLAG_KBD_ROUNDROBIN		( 1 << 18 ) //virtual pattern track allocation algorithm = Round-robin; instead of default tight packing;
#define SUNVOX_FLAG_EXPORT			( 1 << 19 ) //set during sunvox_export_to_wav()
#define SUNVOX_FLAG_IGNORE_EFF31		( 1 << 20 ) //ignore effect 31

int sunvox_global_init();
int sunvox_global_deinit();
void sunvox_engine_init( 
    uint flags,
    int freq,
    WINDOWPTR win, //optional (may be null); required for UI;
    sundog_sound* ss, //optional (may be null); required for JACK+MIDI;
    tsunvox_sound_stream_control stream_control,
    void* stream_control_data,
    sunvox_engine* s );
void sunvox_engine_close( sunvox_engine* s );

void sunvox_rename( sunvox_engine* s, const char* proj_name );

//SunVox engine inside the PSynth module:
psynth_sunvox* psynth_sunvox_new( psynth_net* pnet, uint mod_num, uint sunvox_engines_count, uint flags ); //flags - additional flags (SUNVOX_FLAG_*)
void psynth_sunvox_remove( psynth_sunvox* ps );
void psynth_sunvox_clear( psynth_sunvox* ps );
int psynth_sunvox_set_module(
    PS_RETTYPE (*mod_handler)( PSYNTH_MODULE_HANDLER_PARAMETERS ),
    const char* mod_filename,
    sfs_file mod_file,
    uint mod_count,
    psynth_sunvox* ps );
psynth_module* psynth_sunvox_get_module( psynth_sunvox* ps );
int psynth_sunvox_save_module( sfs_file f, psynth_sunvox* ps );
//Apply module (added by psynth_sunvox_set_module()) to the piece of sound in channels[]:
void psynth_sunvox_apply_module(
    uint mod_num, //1, 2, 3, 4...
    PS_STYPE** channels, int channels_count, uint offset, //IO audio channels (buffers)
    uint size, //number of frames
    psynth_sunvox* ps );
PS_RETTYPE psynth_sunvox_handle_module_event(
    uint mod_num, //1, 2, 3, 4... or 0 for all
    psynth_event* evt,
    psynth_sunvox* ps );

PS_RETTYPE ( *get_module_handler_by_name( const char* name, sunvox_engine* s ) ) ( PSYNTH_MODULE_HANDLER_PARAMETERS );

//File IO:

enum sunvox_block_id
{
    //DON'T CHANGE THE ORDER!
    BID_BVER = 0,
    BID_VERS,
    BID_SFGS,
    BID_BPM,
    BID_SPED,
    BID_TGRD,
    BID_TGD2,
    BID_NAME,
    BID_GVOL,
    BID_MSCL,
    BID_MZOO,
    BID_MXOF,
    BID_MYOF,
    BID_LMSK,
    BID_CURL,
    BID_TIME,
    BID_REPS,
    BID_SELS,
    BID_LGEN,
    BID_PATN,
    BID_PATT,
    BID_PATL,
    BID_PDTA,
    BID_PNME,
    BID_PLIN,
    BID_PCHN,
    BID_PYSZ,
    BID_PICO,
    BID_PPAR,
    BID_PPAR2,
    BID_PFLG,
    BID_PFFF,
    BID_PXXX,
    BID_PYYY,
    BID_PFGC,
    BID_PBGC,
    BID_PEND,
    BID_SFFF,
    BID_SNAM,
    BID_STYP,
    BID_SFIN,
    BID_SREL,
    BID_SXXX,
    BID_SYYY,
    BID_SZZZ,
    BID_SSCL,
    BID_SVPR,
    BID_SCOL,
    BID_SMII,
    BID_SMIN,
    BID_SMIC,
    BID_SMIB,
    BID_SMIP,
    BID_SLNK,
    BID_SLnK,
    BID_CVAL,
    BID_CMID,
    BID_CHNK,
    BID_CHNM,
    BID_CHDT,
    BID_CHFF,
    BID_CHFR,
    BID_SEND,
    BID_SLNk,
    BID_SLnk,
    BID_FLGS, //since SunVox 2.0
    BID_STMT,
    BID_JAMD, //jump address mode
    BID_COUNT //number of block IDs
};

extern const uint8_t g_sunvox_sign[];
extern const uint8_t g_sunvox_module_sign[];

struct sunvox_load_state
{
    sunvox_engine*	s;
    sfs_file 		f;
    char 		block_id_name[ 5 ];
    int			block_id;
    size_t 		block_size;
    void* 		block_data;
    int			block_data_int;
};

struct sunvox_save_state
{
    sunvox_engine*	s;
    sfs_file 		f;
    int*		mod_remap; //mod_remap[ from ] = to
    int*		pat_remap; //pat_remap[ from ] = to
};

sunvox_load_state* sunvox_new_load_state( sunvox_engine* s, sfs_file f );
sunvox_save_state* sunvox_new_save_state( sunvox_engine* s, sfs_file f );
void sunvox_remove_load_state( sunvox_load_state* state );
void sunvox_remove_save_state( sunvox_save_state* state );
int save_block( sunvox_block_id id, size_t size, void* ptr, sunvox_save_state* state );
int load_block( sunvox_load_state* state );

//Load project:

#define SUNVOX_PROJ_LOAD_WITHOUT_NAME		( 1 << 0 )
#define SUNVOX_PROJ_LOAD_WITHOUT_VISUALIZER	( 1 << 1 )
#define SUNVOX_PROJ_LOAD_CREATE_PATTERN		( 1 << 2 )
#define SUNVOX_PROJ_LOAD_SET_BASE_VERSION_TO_CURRENT	( 1 << 3 )
#define SUNVOX_PROJ_LOAD_MERGE_MODULES		( 1 << 4 )
#define SUNVOX_PROJ_LOAD_MERGE_PATTERNS		( 1 << 5 )
#define SUNVOX_PROJ_LOAD_MERGE			( SUNVOX_PROJ_LOAD_MERGE_MODULES | SUNVOX_PROJ_LOAD_MERGE_PATTERNS )
#define SUNVOX_PROJ_LOAD_KEEP_JUSTLOADED	( 1 << 6 ) //Don't reset *_JUST_LOADED flags in the SunVox items (modules and patterns)
#define SUNVOX_PROJ_LOAD_MAKE_TIMELINE_STATIC	( 1 << 7 )
#define SUNVOX_PROJ_LOAD_BACKUP_REQUIRED	( 1 << 8 )
#define SUNVOX_PROJ_LOAD_IGNORE_EMPTY_NAME	( 1 << 9 )
#define SUNVOX_PROJ_SAVE_SELECTED2		( 1 << 0 ) //Save only items (modules and patterns) with the *_SELECTED2 flag
#define SUNVOX_PROJ_SAVE_OPTIMIZE_PAT_SIZE	( 1 << 1 ) //Cut unused tracks
#define SUNVOX_PROJ_SAVE_OPTIMIZE_SLOTS		( 1 << 2 ) //Delete empty module/pattern slots; (the IDs will be changed)
#define SUNVOX_MODULE_SAVE_ADD_INFO		( 1 << 0 ) //Save X,Y,Z,LINKS
#define SUNVOX_MODULE_SAVE_WITHOUT_VISUALIZER	( 1 << 1 )
#define SUNVOX_MODULE_SAVE_WITHOUT_FILE_MARKERS	( 1 << 2 )
#define SUNVOX_MODULE_SAVE_WITHOUT_OUT_LINKS	( 1 << 3 )
#define SUNVOX_MODULE_SAVE_WITHOUT_SEL_FLAG	( 1 << 4 ) //without PSYNTH_FLAG_SELECTED
#define SUNVOX_MODULE_SAVE_LINKS_SELECTED2	( 1 << 5 ) //Remove links without the *_SELECTED2 flag
#define SUNVOX_MODULE_LOAD_ADD_INFO		( 1 << 0 )
#define SUNVOX_MODULE_LOAD_IGNORE_MSB_FLAGS	( 1 << 1 ) //Ignore Mute/Solo/Bypass

int sunvox_load_proj_from_fd( sfs_file f, uint load_flags, sunvox_engine* s );
int sunvox_load_proj( const char* name, uint load_flags, sunvox_engine* s );
bool sunvox_proj_filefmt_supported( sfs_file_fmt fmt );

//Save project:

int* sunvox_save_get_mod_remap_table( sunvox_engine* s, uint32_t save_flags ); //mod_remap[ from ] = to
int* sunvox_save_get_pat_remap_table( sunvox_engine* s, uint32_t save_flags ); //pat_remap[ from ] = to
int sunvox_save_proj_to_fd( sfs_file f, uint32_t save_flags, sunvox_engine* s );
int sunvox_save_proj( const char* name, uint32_t save_flags, sunvox_engine* s );

//Load module:

int sunvox_load_module_from_fd( int mod_num, int x, int y, int z, sfs_file f, uint32_t load_flags, sunvox_engine* s );
int sunvox_load_module( int mod_num, int x, int y, int z, const char* name, uint32_t load_flags, sunvox_engine* s );

//Save module:

int sunvox_save_module_to_fd( int mod_num, sfs_file f, uint32_t save_flags, sunvox_engine* s, sunvox_save_state* st );
int sunvox_save_module( int mod_num, const char* name, uint32_t save_flags, sunvox_engine* s );

//Export:

enum sunvox_export_mode
{
    export_mode_master = 0,
    export_mode_selected_module,
    export_mode_all_modules,
    export_mode_effects,
    export_mode_connected_to_out,
    export_mode_connected_to_selected,
};

enum sunvox_export_loop
{
    export_loop_off = 0,
    export_loop_seamless,
    export_loop_seamless2, //two cycles
};

struct sunvox_time_map_item
{
    uint16_t    bpm;
    uint8_t     tpl;
};

uint sunvox_get_time_map( sunvox_time_map_item* map, uint32_t* frame_map, int start_line, int len, sunvox_engine* s );
int sunvox_get_proj_lines( sunvox_engine* s );
//start_line, line_cnt - timeline segment specified in lines;
//zero line_cnt = whole project;
uint32_t sunvox_get_proj_frames( int start_line, int line_cnt, sunvox_engine* s ); //get project length in frames
int sunvox_export_to_wav(
    const char* name,
    sound_buffer_type buf_type,
    sfs_file_fmt file_format,
    int q,
    sunvox_export_mode mode,
    int mode_par,
    sunvox_export_loop loop,
    int start_line,
    int line_cnt, //0 = whole project
    void (*status_handler)( void*, int ),
    void* status_data,
    sunvox_engine* s );
void sunvox_export_to_midi( const char* name, sunvox_engine* s );

//Patterns:

void sunvox_sort_patterns( sunvox_engine* s );
int sunvox_get_mpp( sunvox_engine* s ); //Get the maximum number of simultaneously playing patterns
void sunvox_select_current_playing_patterns( int first_sorted_pat, sunvox_engine* s );

int sunvox_get_free_pattern_num( sunvox_engine* s );
void sunvox_icon_generator( uint16_t* icon, uint icon_seed );
void sunvox_new_pattern_with_num( int pat_num, int lines, int channels, int x, int y, uint icon_seed, sunvox_engine* s );
int sunvox_new_pattern( int lines, int channels, int x, int y, uint icon_seed, sunvox_engine* s );
int sunvox_new_pattern_empty_clone( int parent, int x, int y, sunvox_engine* s );
int sunvox_new_pattern_clone( int pat_num, int x, int y, sunvox_engine* s );
void sunvox_detach_clone( int pat_num, sunvox_engine* s );
void sunvox_convert_to_clone( int pat_num, int parent, sunvox_engine* s );
void sunvox_remove_pattern( int pat_num, sunvox_engine* s );
void sunvox_change_pattern_flags( int pat_num, uint pat_flags, uint pat_info_flags, bool reset_set, sunvox_engine* s );
void sunvox_rename_pattern( int pat_num, const char* name, sunvox_engine* s );
int sunvox_get_pattern_num_by_name( const char* name, sunvox_engine* s );
inline sunvox_pattern* sunvox_get_pattern( int pat_num, sunvox_engine* s )
{
    if( (unsigned)pat_num < (unsigned)s->pats_num )
        return s->pats[ pat_num ];
    return NULL;
}
inline sunvox_pattern_info* sunvox_get_pattern_info( int pat_num, sunvox_engine* s )
{
    if( (unsigned)pat_num < (unsigned)s->pats_num )
        return &s->pats_info[ pat_num ];
    return NULL;
}
sunvox_note* sunvox_get_pattern_event( int pat_num, int track, int line, sunvox_engine* s );
int sunvox_get_free_icon_number( sunvox_engine* s );
void sunvox_remove_icon( int num, sunvox_engine* s );
void sunvox_make_icon( int pat_num, sunvox_engine* s );
void sunvox_update_icons( sunvox_engine* s );
void sunvox_print_patterns( sunvox_engine* s );
void sunvox_restore_holes( size_t holes_size, int* holes, sunvox_engine* s );
int* sunvox_optimize_patterns_and_save_holes( size_t* holes_size, sunvox_engine* s );
void sunvox_optimize_patterns( sunvox_engine* s );
void sunvox_pattern_set_number_of_channels( int pat_num, int cnum, sunvox_engine* s );
int sunvox_pattern_set_number_of_lines( int pat_num, int lnum, bool rescale_content, sunvox_engine* s );
void sunvox_check_solo_mode( sunvox_engine* s );
int sunvox_pattern_shift( int pat_num, int track, int line, int lines, int cnt, int polyrhythm_len, sunvox_engine* s );
int sunvox_check_pattern_evts( int pat_num, int x, int y, int xsize, int ysize, sunvox_engine* sv ); //retval: column bits (NN,VV,MM,CC,EE,XX,YY)
int sunvox_save_pattern_buf( const char* filename, sunvox_note* buf, int xsize, int ysize );
sunvox_note* sunvox_load_pattern_buf( const char* filename, int* xsize, int* ysize );

//Playing:

void sunvox_set_position( int pos, sunvox_engine* s );
void clean_pattern_state( sunvox_pattern_state* state, sunvox_engine* s );
int sunvox_get_playing_status( sunvox_engine* s );
void sunvox_play_stop( sunvox_engine* s );
void sunvox_play( int pos, bool jump_to_pos, int pat_num, sunvox_engine* s );
void sunvox_rewind( int pos, int pat_num, sunvox_engine* s );
int sunvox_stop( sunvox_engine* s );
int sunvox_stop_and_cancel_record( sunvox_engine* s );

//Recording:

void sunvox_record_write_byte( uint8_t v, sunvox_engine* s );
void sunvox_record_write_int( uint v, sunvox_engine* s );
void sunvox_record_write_time( int rtype, int t, uint flags, sunvox_engine* s );
bool sunvox_record_is_ready_for_event( sunvox_engine* s );
void sunvox_record_stop( bool cancel, sunvox_engine* s );
void sunvox_record( sunvox_engine* s );

//Audio callback:

void sunvox_send_user_command( sunvox_user_cmd* cmd, sunvox_engine* s ); //Stop/Play/TPL/BPM/Ctl/...; some events may be recorded; for kbd use send_kbd_event!
void sunvox_send_kbd_event( sunvox_kbd_event* evt, sunvox_engine* s ); //Call this in the main UI thread only!
void sunvox_add_psynth_event_UNSAFE( int mod_num, psynth_event* evt, sunvox_engine* s );
void sunvox_handle_all_commands_UNSAFE( sunvox_engine* s ); //For the single-threaded mode only!

bool sunvox_render_piece_of_sound( sunvox_render_data* rdata, sunvox_engine* s ); //rdata may be changed!

#define SUNVOX_VF_CHAN_VOL0 		0
#define SUNVOX_VF_CHAN_VOL1 		1
#define SUNVOX_VF_CHAN_LINENUM		2 //Line number in the 27.5 fixed point format

int sunvox_frames_get_value( int channel, stime_ticks_t t, sunvox_engine* s );
