#pragma once

//PSynth (PsyTexxSynth) - SunVox audio core
//Use this header in the module code

#include "sundog.h"

//#define PSYNTH_MULTITHREADED

//Type of the controller value:
typedef int32_t		PS_CTYPE;
#define PSYNTH_MAX_CTLS 127 //Controller's storage (dynamic memory block) capacity = 0...PSYNTH_MAX_CTLS
			    //Can't be larger, because 128+ values are reserved for MIDI controllers

//Type of the module return value:
typedef size_t		PS_RETTYPE;

//Global defines: PS_STYPE_FLOAT32; PS_STYPE_INT16; ...

//Possible sample types:
//==== Float ====
//Sample type:
#ifdef PS_STYPE_FLOAT32
typedef float		PS_STYPE;
typedef float		PS_STYPE2;
typedef float		PS_STYPE3;
typedef float		PS_STYPE_F;
#define PS_STYPE_DESCRIPTION "32bit floating-point"
#define PS_STYPE_FLOATINGPOINT
#define PS_STYPE_ONE 1.0F
#define PS_INT16_TO_STYPE( res, val ) { res = (float)(val) / (float)32768; }
#define PS_FLOAT_TO_STYPE( res, val ) { res = val; }
#define PS_STYPE_TO_FLOAT( res, val ) { res = val; }
#define PS_STYPE_TO_INT16( res, val ) { \
    int temp_res; \
    temp_res = (int)( (val) * (float)32768 ); \
    if( temp_res > 32767 ) res = 32767; else \
    if( temp_res < -32768 ) res = -32768; else \
    res = (int16_t)temp_res; \
}
#define PS_STYPE_SIN( V ) sinf( V )
#define PS_STYPE_ABS( V ) fabs( V )
inline PS_STYPE2 PS_NORM_STYPE( PS_STYPE2 val, PS_STYPE2 norm_val ) //normalize PS_STYPE2 value
{
    return val / norm_val;
}
inline PS_STYPE2 PS_NORM_STYPE_MUL( PS_STYPE2 val1, PS_STYPE2 val2, PS_STYPE2 norm_val ) //val1 * val2 (normalized)
{
    return val1 * val2;
}
inline PS_STYPE2 PS_NORM_STYPE_MUL2( PS_STYPE2 val1, PS_STYPE2 val2, PS_STYPE2 norm_val, PS_STYPE2 max_val ) //val1 * val2 (normalized, max > 32768)
{
    return val1 * val2;
}
#endif
//==== Int16 (fixed point. 1.0 = 4096) ====
//Sample type:
#ifdef PS_STYPE_INT16
typedef int16_t		PS_STYPE;
typedef int32_t		PS_STYPE2; //Wider version of PS_STYPE. May be different from PS_STYPE, but only in fixed-point mode.
typedef int64_t		PS_STYPE3; //...
typedef float		PS_STYPE_F; //Floating point version of PS_STYPE.
#define PS_STYPE_DESCRIPTION "4.12 fixed-point"
#define PS_STYPE_BITS 12
#define PS_STYPE_ONE 4096
#define PS_INT16_TO_STYPE( res, val ) { res = (int16_t)( (val) >> 3 ); }
#define PS_FLOAT_TO_STYPE( res, val ) { res = val * (float)PS_STYPE_ONE; }
#define PS_STYPE_TO_FLOAT( res, val ) { res = (float)(val) / (float)4096; }
#define PS_STYPE_TO_INT16( res, val ) { \
    int temp_res; \
    temp_res = (int)(val) << 3; \
    if( temp_res < -32768 ) temp_res = -32768; else \
    if( temp_res > 32767 ) temp_res = 32767; \
    res = (int16_t)temp_res; \
}
#define PS_STYPE_SIN( V ) sinf( V )
#define PS_STYPE_ABS( V ) abs( V )
inline PS_STYPE2 PS_NORM_STYPE( PS_STYPE2 val, PS_STYPE2 norm_val ) //normalize PS_STYPE2 value
{
    return val;
}
inline PS_STYPE2 PS_NORM_STYPE_MUL( PS_STYPE2 val1, PS_STYPE2 val2, PS_STYPE2 norm_val ) //val1 * val2 (normalized)
{
    return val1 * val2 / norm_val;
}
inline PS_STYPE2 PS_NORM_STYPE_MUL2( PS_STYPE2 val1, PS_STYPE2 val2, PS_STYPE2 norm_val, PS_STYPE2 max_val ) //val1 * val2 (normalized, max > 32768)
{
    val2 /= max_val / norm_val;
    norm_val /= max_val / norm_val;
    return val1 * val2 / norm_val;
}
#endif

//Recommended limits for the note visualization (the real range may be wider):
#define PS_MAX_OCTAVES            	10
#define PS_MAX_NOTES               	( PS_MAX_OCTAVES * 12 )

//pitch256 (or just pitch): 1 semitone = 256; default format for SunVox events and parameters;
//pitch64: 1 semitone = 64; used in PSYNTH_GET_FREQ() (for some generators);
#define PS_NOTE0_PITCH			30720 //pitch for C0 (note 0); (C1 = PS_NOTE0_PITCH - 256; C2 = PS_NOTE0_PITCH - 256*2);
#define PS_NOTE0_FREQ			16.333984375F //frequency (in Hz) for C0

//Pitch conversion:
#define PS_PITCH_TO_NOTE( pitch )	( ( PS_NOTE0_PITCH - ( pitch ) ) / 256 )
#define PS_NOTE_TO_PITCH( note ) 	( PS_NOTE0_PITCH - ( note ) * 256 )
#define PS_PITCH_TO_GFREQ( pitch ) 	( powf( 2.0F, ( 30720.0F - (pitch) ) / 3072.0F ) * 16.333984375F ) //pitch -> frequency of the Generator
#define PS_GFREQ_TO_PITCH( freq ) 	( 30720.0F - LOG2( (freq) / 16.333984375F ) * 3072.0F ) //frequency of the Generator
#define PS_SFREQ_TO_PITCH( freq ) 	( 30720.0F - LOG2( (freq) / 522.6875F ) * 3072.0F ) //frequency of the Sampler

/*
SunVox pitch notation is almost equivalent to Scientific Pitch Notation (SPN) - https://en.wikipedia.org/wiki/Scientific_pitch_notation
SPN:
  C0 = 16.352 Hz;         A4 = 440 Hz;
SunVox:
  C0 = 16.333984375 Hz;   A4 = 439.526062 Hz;
  formulas:
    freq = pow( 2, note / 12 ) * 522.6875 / 32;
    freq = pow( 2, ( 30720 - pitch256 ) / 256 / 12 ) * 522.6875 / 32;
    pitch256 = 30720 - LOG2( freq / 16.333984375 ) * 256 * 12;
SunVox with errors in integer calculations of PSYNTH_GET_FREQ() (the real frequency of most generators; may be fixed in future updates):
  C0 = 16.3125 Hz;        A4 = 439.5 Hz;
*/

/*
PSYNTH_GET_FREQ():
Get int frequency (in Hz) from the PITCH64 (one semitone = 64)
(the final frequency of the generator will be 4 or 5 octaves lower)
...
522 Hz - C0 (note 0, octave 0, pitch64 = 7680); real frequency of the sine generator = 16.3125 Hz (5 octaves lower);
1045 Hz - C1 (note 1, octave 0, pitch64 = 7680-64);
2090 Hz - C2
4181 Hz - C3
8363 Hz - C4
...
14064 Hz - A4 (note 57); real frequency = 439.5 Hz (close to 440 standard);
...
16726 Hz - C5
33452 Hz - C6
66904 Hz - C7
133808 Hz - C8
267616 Hz - C9
535232 Hz - C10
...
*/
#define PSYNTH_GET_FREQ( FREQTABLE, FREQ, PITCH64 ) \
{ \
    if( (PITCH64) >= 0 ) \
        FREQ = ( FREQTABLE[ (PITCH64) % 768 ] >> ( (PITCH64) / 768 ) ); \
    else \
    { \
        /* negative pitch */ \
        if( (PITCH64) < -7680 ) \
            FREQ = ( FREQTABLE[ (7680*4-7680) % 768 ] << -( ( (7680*4-7680) / 768 ) - (7680*4)/768 ) ); \
        else \
            FREQ = ( FREQTABLE[ (7680*4+(PITCH64)) % 768 ] << -( ( (7680*4+(PITCH64)) / 768 ) - (7680*4)/768 ) ); \
    } \
}
//LQ versions (loss of two bits) - for correct playback of old songs;
//Get 48bit delta (HIGH - 32, LOW - 16):
#define PSYNTH_GET_DELTA( SMPRATE, FREQ, HIGH, LOW ) { uint64_t D = ((uint64_t)(FREQ)<<16) / (unsigned)(SMPRATE); LOW = (uint32_t)D & (65535^3); HIGH = D >> 16; }
#define PSYNTH_GET_DELTA_HQ( SMPRATE, FREQ, HIGH, LOW ) { uint64_t D = ((uint64_t)(FREQ)<<16) / (unsigned)(SMPRATE); LOW = (uint32_t)D & 65535; HIGH = D >> 16; }
//Get 64bit delta (HIGH - 48, LOW - 16, packed in single uint64_t):
#define PSYNTH_GET_DELTA64( SMPRATE, FREQ, D ) { D = ( ((uint64_t)(FREQ)<<16) / (unsigned)(SMPRATE) ) & (uint64_t)(0xFFFFFFFFFFFFFFFFll^3); }
#define PSYNTH_GET_DELTA64_HQ( SMPRATE, FREQ, D ) { D = ((uint64_t)(FREQ)<<16) / (unsigned)(SMPRATE); }

//One tick size (frame = 256):
#define PSYNTH_TICK_SIZE( SFREQ, BPM ) ( ( ( ( (uint64_t)(SFREQ) * (uint64_t)60 ) << 8 ) / (BPM) ) / 24 )

//64bit fixed point math (must be replaced with standard C/C++ int64?)
#define PSYNTH_FP64_PREC 16 //Fixed point precision: 32.X
#define PSYNTH_FP64_SUB( v1, v1_p, v2, v2_p ) \
{ \
    v1 -= v2; v1_p -= v2_p; \
    if( (signed)v1_p < 0 ) { v1--; v1_p += ( (int)1 << PSYNTH_FP64_PREC ); } \
}
#define PSYNTH_FP64_ADD( v1, v1_p, v2, v2_p ) \
{ \
    v1 += v2; v1_p += v2_p; \
    if( v1_p > ( (int)1 << PSYNTH_FP64_PREC ) - 1 ) { v1++; v1_p -= ( (int)1 << PSYNTH_FP64_PREC ); } \
}
//11.11 *= 00.22
#define PSYNTH_FP64_UMUL( v1, v1_p, v2_p ) \
{ \
    v1 *= ( v2_p ); \
    v1_p *= ( v2_p ); \
    v1_p >>= PSYNTH_FP64_PREC; \
    v1_p += v1 & ( ( 1 << PSYNTH_FP64_PREC ) - 1 ); \
    v1 >>= PSYNTH_FP64_PREC; \
    v1 += v1_p >> PSYNTH_FP64_PREC; \
    v1_p &= ( 1 << PSYNTH_FP64_PREC ) - 1; \
}

enum psynth_buf_state
{
    PS_BUF_SILENCE = 0, 	//silence; buffer is filled with unknown data
    PS_BUF_FILLED = 1, 		//buffer is filled with unknown data
    PS_BUF_FILLED_SILENCE = 2, 	//buffer is filled with zeros
    PS_BUF_FILLED_DETECT = 3,	//buffer is filled with unknown data; silence detection required
};

enum psynth_command
{
    PS_CMD_NOP = 0,
    PS_CMD_GET_DATA_SIZE,
    PS_CMD_GET_NAME,
    PS_CMD_GET_INFO,
    PS_CMD_GET_COLOR,
    PS_CMD_GET_INPUTS_NUM,	//Get maximum number of input channels
    PS_CMD_GET_OUTPUTS_NUM,	//Get maximum number of output channels
    PS_CMD_GET_FLAGS,
    PS_CMD_GET_FLAGS2,
    PS_CMD_INIT,
    PS_CMD_SETUP_FINISHED,	//Module created/loaded, INIT finished, ready for work
    PS_CMD_BEFORE_SAVE,		//!!! May be called when the module is active. Check thread safety !!!
    PS_CMD_AFTER_SAVE,		//!!! May be called when the module is active. Check thread safety !!!
    PS_CMD_CLEAN,              	//Clear all internal buffers
    PS_CMD_RENDER_SETUP,	//Render setup: beginning of the render callback, event buffers are empty (only if PSYNTH_FLAG_GET_RENDER_SETUP_COMMANDS exist)
    PS_CMD_RENDER_REPLACE,	//Replace data in the output buffer; retval = PS_BUF_*
    PS_CMD_NOTE_ON,
    PS_CMD_SET_FREQ,
    PS_CMD_SET_VELOCITY,
    PS_CMD_SET_SAMPLE_OFFSET,	//In frames
    PS_CMD_SET_SAMPLE_OFFSET2, 	//In fractions (0 - start ... 32768 - end)
    PS_CMD_NOTE_OFF,		//Note OFF; optional if a new note replaces the old one on the same track (same evt ID);
				//(psynth_event_note must be ignored for this command)
    PS_CMD_ALL_NOTES_OFF,
    PS_CMD_SET_LOCAL_CONTROLLER,
    PS_CMD_SET_GLOBAL_CONTROLLER,
    PS_CMD_APPLY_CONTROLLERS,	//If possible, immediately apply the latest ctl values to the module engine, without delay (if module has a Response parameter)
    PS_CMD_SET_MSB,		//Set Mute/Solo/Bypass: evt->mute.flags: PSYNTH_EVENT_MUTE / PSYNTH_EVENT_SOLO / PSYNTH_EVENT_BYPASS
    PS_CMD_RESET_MSB,		//Reset Mute/Solo/Bypass: evt->mute.flags: PSYNTH_EVENT_MUTE / PSYNTH_EVENT_SOLO / PSYNTH_EVENT_BYPASS
			        //if rv == 0, the engine will set the Mute/Solo/Bypass module flags and realtime_flags
    PS_CMD_MUTED,
    PS_CMD_FINETUNE,
    PS_CMD_SPEED_CHANGED,	//Only module with PSYNTH_FLAG_GET_SPEED_CHANGES flag can receive this command
    PS_CMD_PLAY,		//Global PLAY command (only if PSYNTH_FLAG_GET_PLAY_COMMANDS exist)
    PS_CMD_STOP,		//Global STOP command (only if PSYNTH_FLAG_GET_STOP_COMMANDS exist)
    PS_CMD_INPUT_LINKS_CHANGED,
    PS_CMD_OUTPUT_LINKS_CHANGED,
    PS_CMD_CLOSE,

    //Additional commands:

    PS_CMD_READ_CURVE,		//input: evt.curve - data ptr; .id = curve num; .offset = number of samples to write/read; output: number of samples processed successfully
    PS_CMD_WRITE_CURVE,

    /*
    Enable/Disable additional MIDI OUT message support.
    By default, the app can receive MIDI messages "Program Change", "Channel Pressure" and "Pitch Bend Change", but can't send them.
    This command allows you to temporarily enable support for these messages for the specific module.
    After enabling if you change the value of the controller evt->midimsg_support.ctl, the MIDI message evt->midimsg_support.msg will be sent instead of the "Control Change" command.
    */
    PS_CMD_MIDIMSG_SUPPORT,

    PSYNTH_COMMANDS
};

enum psynth_midi_type
{
    psynth_midi_none = 0,
    psynth_midi_note,
    psynth_midi_key_pressure,
    psynth_midi_ctl,
    psynth_midi_nrpn,
    psynth_midi_rpn,
    psynth_midi_prog,
    psynth_midi_channel_pressure,
    psynth_midi_pitch,
    psynth_midi_types
};

enum psynth_midi_mode
{
    psynth_midi_lin = 0,
    psynth_midi_exp1,
    psynth_midi_exp2,
    psynth_midi_spline,
    psynth_midi_trig,
    psynth_midi_toggle,
    psynth_midi_modes
};

#define psynth_midi_val_14bit_flag	(uint16_t)( 1 << 15 )

struct psynth_midi_evt
{
    psynth_midi_type type;
    uint16_t val1; //note, ctl number, etc.
    uint16_t val2; //ctl value or velocity
    uint8_t ch;
};

#define PSYNTH_CTL_FLAG_EXP2	( 1 << 0 ) //display exponential
#define PSYNTH_CTL_FLAG_EXP3	( 1 << 1 ) //display exponential

#define PSYNTH_CTL_MIDI_PARS1( type, ch, mode ) \
    ( ( ( type & 255 ) << 0 ) | ( ( ch & 255 ) << 8 ) | ( ( mode & 255 ) << 16 ) )
#define PSYNTH_CTL_MIDI_PARS2( par, min, max ) \
    ( ( ( par & 0xFFFF ) << 0 ) | ( ( min & 255 ) << 16 ) | ( ( max & 255 ) << 24 ) )
#define PSYNTH_CTL_MIDI_PARS1_DEFAULT PSYNTH_CTL_MIDI_PARS1( 0, 0, 0 )
#define PSYNTH_CTL_MIDI_PARS2_DEFAULT PSYNTH_CTL_MIDI_PARS2( 0, 0, 255 )
#define PSYNTH_MIDI_PARS1_GET_TYPE( pars ) ( ( pars >> 0 ) & 255 )
#define PSYNTH_MIDI_PARS1_GET_CH( pars ) ( ( pars >> 8 ) & 255 )
#define PSYNTH_MIDI_PARS1_GET_MODE( pars ) ( ( pars >> 16 ) & 255 )
#define PSYNTH_MIDI_PARS2_GET_PAR( pars ) ( ( pars >> 0 ) & 0xFFFF )
#define PSYNTH_MIDI_PARS2_GET_MIN( pars ) ( ( pars >> 16 ) & 255 ) //0..200
#define PSYNTH_MIDI_PARS2_GET_MAX( pars ) ( ( pars >> 24 ) & 255 ) //0..200

struct psynth_ctl
{
    const char*	name;  //For example: "Delay", "Feedback";
    const char*	label; //For example: "dB", "samples", "off/on"
		       //(external string pointers (no autoremove))
    PS_CTYPE	min;
    PS_CTYPE	max;
    PS_CTYPE	def;
    PS_CTYPE*	val;
    PS_CTYPE	show_offset; //displayed value = val + show_offset;
    PS_CTYPE	normal_value; //100% or some normal point (e.g. middle value for panning);
    uint32_t	flags;
    uint8_t	type; //0 - normal (volume, frequency, resonance etc.);
		      //    scaled range 0...32768 must be used when sending automation commands;
		      //1 - selector (number of harmonics, waveform type etc.);
    uint8_t	group;
    uint32_t	midi_pars1; //MIDI IN
    uint32_t	midi_pars2; //MIDI IN
    uint8_t	midi_val; //only for psynth_midi_toggle handling
};

//3 bits - smp type:
#define PS_CHUNK_SMP_INT8	    	1
#define PS_CHUNK_SMP_INT16	    	2
#define PS_CHUNK_SMP_INT32	    	3
#define PS_CHUNK_SMP_FLOAT32    	4
#define PS_CHUNK_SMP_TYPE_MASK  	7
//2 bits - number of channels:
#define PS_CHUNK_SMP_STEREO		( 1 << 3 )
#define PS_CHUNK_SMP_CH_OFFSET  	3
#define PS_CHUNK_SMP_CH_MASK  		( 3 << PS_CHUNK_SMP_CH_OFFSET )
//temp flags:
#define PS_CHUNK_FLAG_DONT_SAVE	    	( 1 << 5 )
struct psynth_chunk
{
    void* 		data;
    uint32_t		flags; //PS_CHUNK_SMP_* | PS_CHUNK_FLAG_*
    int32_t		freq; //sample rate
};

struct psynth_event;
struct psynth_net;

#define PSYNTH_SCOPE_PARTS		8
#if HEAPSIZE <= 16 || defined(OS_ANDROID)
    //Unknown scope_buf_start_time
    //For systems with the following issues:
    // * heap size is too small for the scope buffers;
    // * system audio buffer size is unknown (e.g. Android 4.1 and lower);
    //   example: system buffer size = 4096; SunVox buffer size = 1024; expected rendering algorithm: 1 pause 2 pause 3 pause 4; but actual: 1234 long pause 1234 ....
    #define PSYNTH_SCOPE_MODE_FAST_LQ
    #define PSYNTH_SCOPE_SIZE		4096
#else
    //High Quality + Ring scope buffer:
    #define PSYNTH_SCOPE_MODE_SLOW_HQ
    #define PSYNTH_SCOPE_SIZE		16384
#endif

#if CPUMARK < 10
    #if HEAPSIZE <= 16
	#define PSYNTH_DEF_FFT_SIZE	256
    #else
	#define PSYNTH_DEF_FFT_SIZE	1024
    #endif
#endif
#ifndef PSYNTH_DEF_FFT_SIZE
    #define PSYNTH_DEF_FFT_SIZE		2048
#endif

enum
{
    psynth_level_mode_off = 0,
    psynth_level_mode_mono,
    psynth_level_mode_stereo,
    psynth_level_mode_color,
    psynth_level_mode_glow,
};

#define PSYNTH_LEVEL_FLAG_VERTICAL	( 1 << 0 )
#define PSYNTH_LEVEL_FLAG_DB		( 1 << 1 ) //logarithmic scale
#define PSYNTH_LEVEL_FLAG_PEAK		( 1 << 2 ) //show peak levels and values

enum
{
    psynth_oscill_mode_off = 0,
    psynth_oscill_mode_points,
    psynth_oscill_mode_lines,
    psynth_oscill_mode_bars,
    psynth_oscill_mode_bars2,
    psynth_oscill_mode_phase_scope_x1,
    psynth_oscill_mode_phase_scope_x2,
    psynth_oscill_mode_xy,
};

#define PSYNTH_OSCILL_FLAG_SYNC		( 1 << 0 ) //zero crossing _/-
#define PSYNTH_OSCILL_FLAG_unused1	( 1 << 1 )
#define PSYNTH_OSCILL_FLAG_unused2	( 1 << 2 )

#define PSYNTH_VIS_FLAG_NO_BG_OUTLINE	( 1 << 0 )
#define PSYNTH_VIS_FLAG_NO_BG_FILL	( 1 << 1 )
#define PSYNTH_VIS_FLAG_NO_PEAK_VALS	( 1 << 2 )
#define PSYNTH_VIS_FLAG_LEVEL_RMS	( 1 << 3 )

#define PSYNTH_VIS_PARS( level_mode, level_flags, oscill_mode, oscill_flags, oscill_size, bg_transp, shadow_opacity, flags ) \
    ( ( level_mode << 0 ) | ( level_flags << 5 ) | ( oscill_mode << 8 ) | ( oscill_flags << 13 ) | ( oscill_size << 16 ) | ( bg_transp << 24 ) | ( shadow_opacity << 26 ) | ( flags << 28 ) )
#define PSYNTH_VIS_PARS_DEFAULT PSYNTH_VIS_PARS( psynth_level_mode_mono, 0, psynth_oscill_mode_points, 0, 12, 0, 0, 0 )
#define PSYNTH_GET_VIS_LEVEL_MODE( pars ) ( pars & 31 )
#define PSYNTH_GET_VIS_LEVEL_FLAGS( pars ) ( ( pars >> 5 ) & 7 )
#define PSYNTH_GET_VIS_OSCILL_MODE( pars ) ( ( pars >> 8 ) & 31 )
#define PSYNTH_GET_VIS_OSCILL_FLAGS( pars ) ( ( pars >> 13 ) & 7 )
#define PSYNTH_GET_VIS_OSCILL_SIZE( pars ) ( ( pars >> 16 ) & 255 ) //in milliseconds
#define PSYNTH_GET_VIS_BG_TRANSP( pars ) ( ( pars >> 24 ) & 3 ) //0 - visible; 3 - invisible;
#define PSYNTH_GET_VIS_SHADOW_OPACITY( pars ) ( ( pars >> 26 ) & 3 ) //0 - invisible; 3 - visible;
#define PSYNTH_GET_VIS_FLAGS( pars ) ( ( pars >> 28 ) & 15 )

//Module flags:
//(static or rarely mutable; can be changed from UI - need to be fixed? maybe add flags2 for SELECTED, SELECTED2, JUST_LOADED, LAST?)
#define PSYNTH_FLAG_EXISTS			( 1 << 0 )
#define PSYNTH_FLAG_OUTPUT			( 1 << 1 ) //Main project output
#define PSYNTH_FLAG_GENERATOR			( 1 << 3 ) //Note input + Sound output
#define PSYNTH_FLAG_EFFECT			( 1 << 4 ) //Sound input + Sound output
#define PSYNTH_FLAG_INITIALIZED			( 1 << 6 )
#define PSYNTH_FLAG_MUTE			( 1 << 7 ) //Mute/Solo/Bypass flags -> render -> set realtimg flags (actual state) -> set UI flags
#define PSYNTH_FLAG_SOLO			( 1 << 8 ) //If active, PSYNTH_RT_FLAG_MUTE will be set for all other modules
#define PSYNTH_FLAG_GET_SPEED_CHANGES		( 1 << 10 )
#define PSYNTH_FLAG_HIDDEN			( 1 << 11 )
//#define PSYNTH_FLAG_MULTI			( 1 << 12 ) //DEPRECATED. Can send commands to multiple modules
#define PSYNTH_FLAG_DONT_FILL_INPUT		( 1 << 13 ) //Don't fill the input signal buffers; BUT in_empty[] will be set; WARNING: channel noise possible after this flag reset!
#define PSYNTH_FLAG_BYPASS			( 1 << 14 )
#define PSYNTH_FLAG_MSB				( PSYNTH_FLAG_MUTE | PSYNTH_FLAG_SOLO | PSYNTH_FLAG_BYPASS )
#define PSYNTH_FLAG_USE_MUTEX			( 1 << 15 ) //Mutex for render lock/unlock
#define PSYNTH_FLAG_IGNORE_MUTE			( 1 << 16 )
#define PSYNTH_FLAG_NO_SCOPE_BUF		( 1 << 17 )
#define PSYNTH_FLAG_OUTPUT_IS_EMPTY		( 1 << 18 ) //No sound output
#define PSYNTH_FLAG_OPEN			( 1 << 19 ) //Set by MetaModule after EDIT
#define PSYNTH_FLAG_GET_PLAY_COMMANDS		( 1 << 20 )
#define PSYNTH_FLAG_GET_RENDER_SETUP_COMMANDS	( 1 << 21 )
#define PSYNTH_FLAG_FEEDBACK			( 1 << 22 )
#define PSYNTH_FLAG_GET_STOP_COMMANDS		( 1 << 23 )
#define PSYNTH_FLAG_NO_RENDER			( 1 << 24 ) //No sound input/output (don't call RENDER_REPLACE)
#define PSYNTH_FLAG_SELECTED			( 1 << 25 ) //Multiselection (addition to the sv->selected_module)
//The following flags will not be saved:
#define PSYNTH_FLAG2_GET_MUTED_COMMANDS		( 1 << 0 )
#define PSYNTH_FLAG2_NOTE_SENDER		( 1 << 1 ) //Note output
#define PSYNTH_FLAG2_NOTE_RECEIVER		( 1 << 2 ) //Note input
#define PSYNTH_FLAG2_NOTE_IO			( PSYNTH_FLAG2_NOTE_SENDER | PSYNTH_FLAG2_NOTE_RECEIVER )
#define PSYNTH_FLAG2_JUST_LOADED		(unsigned)( 1 << 29 ) //MUST BE cleared automatically!
#define PSYNTH_FLAG2_SELECTED2			(unsigned)( 1 << 30 ) //MUST BE cleared automatically! (temp selection, not visible for the user; used in SUNVOX_ACTION_PROJ_AFTERMERGE)
#define PSYNTH_FLAG2_LAST			(unsigned)( 1 << 31 ) //MUST BE cleared automatically!
//Realtime flags:
//(don't read/write these flags outside the main audio thread)
#define PSYNTH_RT_FLAG_LOCKED			( 1 << 0 )
#define PSYNTH_RT_FLAG_RENDERED			( 1 << 1 )
#define PSYNTH_RT_FLAG_MUTE			( 1 << 2 )
#define PSYNTH_RT_FLAG_SOLO			( 1 << 3 )
#define PSYNTH_RT_FLAG_BYPASS			( 1 << 4 )
#define PSYNTH_RT_FLAG_DONT_CLEAN_INPUT		( 1 << 5 ) //Used by MetaModule and psynth_sunvox
//UI output flags:
//(you can read it in the UI thread)
#define PSYNTH_UI_FLAG_MUTE			( 1 << 0 )
#define PSYNTH_UI_FLAG_SOLO			( 1 << 1 )
#define PSYNTH_UI_FLAG_BYPASS			( 1 << 2 )
#define PSYNTH_UI_FLAG_CTLS_WITH_MIDI_IN	( 1 << 3 )
#define PSYNTH_UI_FLAG_MIDI_IN_ACTIVITY		( 1 << 4 )
//MIDI IN flags:
#define PSYNTH_MIDI_IN_FLAG_ALWAYS_ACTIVE	( 1 << 0 ) //Even when module is not selected
#define PSYNTH_MIDI_IN_FLAG_CHANNEL_OFFSET	1
#define PSYNTH_MIDI_IN_FLAG_CHANNEL_MASK	( 31 << PSYNTH_MIDI_IN_FLAG_CHANNEL_OFFSET ) //5 bits: 0 - ANY; 1 - first channel; ... 16 - last channel;
#define PSYNTH_MIDI_IN_FLAG_NEVER		( 1 << ( PSYNTH_MIDI_IN_FLAG_CHANNEL_OFFSET + 5 ) ) //Never accept MIDI IN data

#define PSYNTH_MODULE_HANDLER_PARAMETERS int mod_num, psynth_event* event, psynth_net* pnet
typedef PS_RETTYPE (*psynth_handler_t)( PSYNTH_MODULE_HANDLER_PARAMETERS );

//MIDI OUT message map:
#define PSYNTH_MIDIMSG_MAP_PROG			0 //Program Change
#define PSYNTH_MIDIMSG_MAP_PRESSURE		1 //Channel Pressure
#define PSYNTH_MIDIMSG_MAP_PITCH		2 //Pitch Bend Change
#define PSYNTH_MIDIMSG_MAP_SIZE			3
struct psynth_midimsg_map
{
    uint8_t		ctl[ PSYNTH_MIDIMSG_MAP_SIZE ]; //0 - default; 0x80... - some MIDI ctl that will be redirected to PSYNTH_MIDIMSG_MAP_*
    bool 		active;
};

#define PSYNTH_MAX_CHANNELS			2
struct psynth_module
{
    psynth_net*		pnet;
    uint32_t	    	flags;
    //The following flags will not be saved:
    uint32_t	    	flags2;
    uint8_t		realtime_flags; //Can be used in the main audio thread only
    uint8_t		ui_flags;
#ifdef PSYNTH_MULTITHREADED
    uint8_t		th_id; //Current rendering thread
#endif

    PS_RETTYPE	    	(*handler)(
			    PSYNTH_MODULE_HANDLER_PARAMETERS
			);
    void*	    	data_ptr; //User data
    PS_STYPE*		channels_in[ PSYNTH_MAX_CHANNELS ]; //Read-only data for the module (if PSYNTH_FLAG_DONT_FILL_INPUT is not set).
							    //Otherwise psynth_sunvox_apply_module() will produce wrong input data for the mono signal
    PS_STYPE*		channels_out[ PSYNTH_MAX_CHANNELS ];
    int		    	in_empty[ PSYNTH_MAX_CHANNELS ]; //Number of zero frames
    int		    	out_empty[ PSYNTH_MAX_CHANNELS ]; //Number of zero frames

    //Number of channels:
    int		    	input_channels;
    int		    	output_channels;

    //Parameters for PS_CMD_RENDER_REPLACE: (should be placed in psynth_event??????)
    int                 frames; //Buffer size
    int                 offset; //Time offset in frames
    //==============================================================================

    int*      		events;
    uint		events_num;

    //Standard properties:
    int		    	finetune; //-256...256
    int		    	relative_note;

    smutex        	mutex; //Render lock/unlock

    //Links to the input synths:
    int*		input_links; //Links can be changed in the main UI thread only! Not in the audio thread!
    int		    	input_links_num; //Number of the input slots (some slots may be empty (-1))

    //Links to the output synths:
    int*		output_links;
    int			output_links_num; //Number of the output slots (some slots may be empty (-1))

    //Scope buffers:
    PS_STYPE*		scope_buf[ PSYNTH_MAX_CHANNELS ];

    volatile float	cpu_usage; //In percents (0..100)
    int             	cpu_usage_ticks;

    //Controllers:
    psynth_ctl*  	ctls;
    uint	    	ctls_num;
    int			ctls_yoffset; //for UI only

    //Data chunks:
    psynth_chunk**	chunks; //In future updates we should add optional symtab (~100 elements), which will be enabled for modules with large amount of chunks

    //MIDI:
    uint		midi_in_flags;
    uint		render_counter; //for MIDI IN (psynth_midi_toggle)
    char*		midi_out_name; //Device name
    int			midi_out;
    int			midi_out_ch;
    int			midi_out_bank;
    int			midi_out_prog;
    psynth_midimsg_map	midi_out_msg_map;
#ifndef NOMIDI
    uint		midi_out_note_id[ 16 ];
    uint8_t		midi_out_note[ 16 ];
#endif

    //Gfx:

    int		    	x, y; //0 .. 1024 (100%)
    uint8_t		z;
    uint8_t	    	color[ 3 ];
    uint16_t		scale; //Normal = 256

    uint32_t		visualizer_pars;

    char	    	name[ 32 + 1 ]; //Last char is always NULL
    char*		name2; //External string pointer (no autoremove)
    uint32_t		id;

#ifdef SUNVOX_GUI
    //Visual (optional) - window with some specific module elements (graphics)
    WINDOWPTR	    	visual;
    int			visual_min_ysize;
#endif

    //Requests:
    volatile uint16_t	draw_request; //REDRAW Signal to the UI host (window with controllers).
    uint16_t		draw_request_answer; //Must be equal to draw_request.
    volatile uint16_t	full_redraw_request; //FULL REDRAW Signal to the UI host (window with controllers). With recalc_regions().
    uint16_t		full_redraw_request_answer; //...

};

struct psynth_event_note
{
    int16_t		root_note;      //DEPRECATED. Don't use it! Note number without finetune and relative note
    int16_t		velocity;       //Velocity (0..256)
    int32_t		pitch;      	//Pitch:
			                //  256 = one note; 256*12 = octave;
					//  PS_NOTE0_PITCH = note 0;
					//  PS_NOTE0_PITCH-256 = note 1;
					//  0 = note 120;
					//  -256 = note 121;
};

struct psynth_event_controller
{
    uint16_t		ctl_num;        //Controller number: 0..126 - module ctls; 127..254 - MIDI ctls;
    uint16_t		ctl_val;        //Controller value (0..32768)
};

#define PSYNTH_EVENT_MUTE		( 1 << 0 )
#define PSYNTH_EVENT_SOLO		( 1 << 1 )
#define PSYNTH_EVENT_BYPASS		( 1 << 2 )
struct psynth_event_mute
{
    uint8_t		flags;		//PSYNTH_EVENT_*
};

struct psynth_event_finetune
{
    int16_t		finetune; //-256...256; values < -256 will be ignored
    int16_t	    	relative_note; //values < -256 will be ignored
};

struct psynth_event_sample_offset
{
    uint32_t		sample_offset;  //Sample offset in frames (frame = channels * number_of_bits_per_sample)
};

struct psynth_event_speed
{
    int			tick_size;      //One tick size (256 - one sample)
    uint16_t		bpm;
    uint8_t		ticks_per_line;
};

struct psynth_event_curve
{
    float*		data;
};

struct psynth_event_midimsg_support
{
    uint8_t		msg; //PSYNTH_MIDIMSG_MAP_*
    uint8_t		ctl; //0x80...
    //ctl:
    // 0 - OFF (default);
    // 0x80 - MIDI controller 0;
    // 0x81 - MIDI controller 1;
    // 0x82 - MIDI controller 2;
    // ...
};

//Get a unique (to some extent) event clone:
#define PSYNTH_EVT_ID_INC( evt_id, mod_num ) { evt_id+=((mod_num)<<5); evt_id^=((mod_num&15)<<12); evt_id^=((mod_num)<<(16+10+(mod_num&3))); }

//Event for the module of the sound net:
struct psynth_event
{
    psynth_command	command;
    uint32_t		id; //0xAAAABBBB:
			    //  AAAA - pattern state pointer or SUNVOX_VIRTUAL_PATTERN_*;
			    //  BBBB - pattern track;
			    //Only for the following events: note on/off, sample_offset, local controller.
			    //MAY BE CHANGED by some modules (multisynth, delay, etc.) using PSYNTH_EVT_ID_INC().
    int32_t		offset; //Time offset in frames
    union 
    {
	psynth_event_note	    	note;
	psynth_event_controller	    	controller;
	psynth_event_mute		mute;
	psynth_event_finetune		finetune;
	psynth_event_sample_offset  	sample_offset;
	psynth_event_speed	    	speed;
	psynth_event_curve		curve; //id = curve num; offset = number of items to write/read;
	psynth_event_midimsg_support	midimsg_support;
    };
};

struct psynth_thread
{
    int			n;
    psynth_net*		pnet;
    sthread 		th;

    //Temp buffers (use it in the module code):

    PS_STYPE*		temp_buf[ PSYNTH_MAX_CHANNELS ]; //for PSYNTH_GET_RENDERBUF() and psynth_renderbuf2output()
    PS_STYPE*		resamp_buf[ PSYNTH_MAX_CHANNELS * 2 ]; //resampler input buffers:
    // mode0:                        [ TAIL:previous frames (from the previous resampling iteration) ] [ NEW FRAMES with additional interpolation frames at the end ] [ .. ]
    // +PSYNTH_MAX_CHANNELS (mode1): [ TAIL:previous frames (from the previous resampling iteration) ] [ INPUT DELAY ] [ NEW FRAMES ] [ .. ]
};

//Sound net flags:
#define PSYNTH_NET_FLAG_MAIN			( 1 << 0 )
#define PSYNTH_NET_FLAG_CREATE_MODULES		( 1 << 1 )
#define PSYNTH_NET_FLAG_NO_SCOPE		( 1 << 2 )
#define PSYNTH_NET_FLAG_NO_MIDI      		( 1 << 3 )
#define PSYNTH_NET_FLAG_NO_MODULE_CHANNELS	( 1 << 4 ) //no psynth_render_*; the module will be rendered manually with user-defined (external) channel buffers;

//Sound net (created by host):
struct psynth_net
{
    uint 		flags;

    //Modules:

    psynth_module*	mods;
    uint		mods_num; //mods capacity (may be larger than number of modules in project)
    smutex		mods_mutex; //May be used by synths for access to some global data
    psynth_event*	events_heap;
#ifdef PSYNTH_MULTITHREADED
    std::atomic_int	events_num;
#else
    uint		events_num;
#endif

    //MIDI:

    sundog_midi_client	midi_client; //MIDI client (I/O)
    ssymtab*		midi_in_map; //For controllers: Channel+Type+Par -> Set of links to the modules
    uint*		midi_in_mods; //For notes: modules with PSYNTH_MIDI_IN_FLAG_ALWAYS_ACTIVE flag
    uint		midi_in_mods_num;
    bool		midi_out_7bit; //Send 7-bit controller values instead of 14-bit

    //FFT analyzer:

    PS_STYPE*		fft;
    int			fft_size;
    int			fft_ptr;
    volatile int	fft_mod;
    volatile bool	fft_locked;

    //Some info:

    int			sampling_freq;
    int			max_buf_size; //in frames
    int			global_volume;	//1.0 = 256
    int			all_modules_muted;
    int			buf_size;
    //uint32_t		frame_cnt; //increases at the end of psynth_render_all()
    stime_ticks_t	out_time;
    uint8_t		cpu_usage_enable; //bits: 1<<0 - modules+net; 1<<1 - net;
    volatile float	cpu_usage1; //for monitor1 (cpu+modules): in percents (0..100)
    volatile float	cpu_usage2; //for monitor2 (cpu+graph): in percents (0..100)
    stime_ticks_t	cpu_usage_t1;
    void*		host; //sunvox_engine
    uint32_t		base_host_version;
    uint		render_counter;
    int			change_counter; //any changes
    int			change_counter2; //connection configuration, MSB, add/delete module
    int			prev_change_counter2;

    //Threads:

    psynth_thread*	th;
    int			th_num; //number of threads (min 1)
    volatile bool	th_exit_request;
    volatile stime_ticks_t	th_work_t;
#ifdef PSYNTH_MULTITHREADED
    volatile bool	th_work;
    std::atomic_int	th_work_cnt;
#endif

    //Global input (microphone / line-in):

    void*		in_buf;
    sound_buffer_type	in_buf_type;
    int			in_buf_channels;

    //Scope buffers:

    volatile int	scope_buf_cur_ptr;
#ifdef PSYNTH_SCOPE_MODE_SLOW_HQ
    int			scope_buf_current; //current part
    int			scope_buf_ptr[ PSYNTH_SCOPE_PARTS ];
    stime_ticks_t	scope_buf_start_time[ PSYNTH_SCOPE_PARTS ];
#endif
};

#define GET_SD_FROM_PSYNTH_NET( PNET, SD ) \
{ \
    SD = nullptr; \
    sunvox_engine* SV = (sunvox_engine*)PNET->host; \
    if( SV && SV->win ) SD = SV->win->wm->sd; \
}

//Global tables/variables:
#include "psynth_freq_table.h"
extern bool g_loading_builtin_sv_template;
extern const char g_n2c[ 12 ];
#define g_module_colors_num 128
extern const uint8_t g_module_colors[ 128 * 3 + 1 ];

//Controllers:
void psynth_resize_ctls_storage( uint mod_num, uint ctls_num, psynth_net* pnet );
void psynth_change_ctls_num( uint mod_num, uint ctls_num, psynth_net* pnet );
int psynth_register_ctl( 
    uint mod_num, 
    const char* ctl_name, //Examples: "Delay", "Feedback"
    const char* ctl_label, //Examples: "dB", "ms", "samples", ""
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_def,
    uint8_t ctl_type, //0 - level (input values 0..0x8000); 1 - numeric
    PS_CTYPE* ctl_value,
    PS_CTYPE ctl_normal_value,
    uint8_t ctl_group,
    psynth_net* pnet );
void psynth_change_ctl( 
    uint mod_num, 
    uint ctl_num,
    const char* ctl_name, //Examples: "Delay", "Feedback"
    const char* ctl_label, //Examples: "dB", "ms", "samples", ""
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_def,
    int ctl_type, //0 - level (input values 0..0x8000); 1 - numeric
    PS_CTYPE* ctl_value,
    PS_CTYPE ctl_normal_value,
    int ctl_group,
    psynth_net* pnet );
void psynth_change_ctl_limits(
    uint mod_num,
    uint ctl_num,
    PS_CTYPE ctl_min,
    PS_CTYPE ctl_max,
    PS_CTYPE ctl_normal_value,
    psynth_net* pnet );
void psynth_set_ctl_show_offset( uint mod_num, uint ctl_num, PS_CTYPE ctl_show_offset, psynth_net* pnet );
void psynth_set_ctl_flags( uint mod_num, uint ctl_num, uint32_t flags, psynth_net* pnet );
int psynth_get_scaled_ctl_value( //Input = normal decimal ctl value; Output = scaled (0..32768) value for the pattern
    uint mod_num,
    uint ctl_num,
    int val,
    bool positive_val_without_offset,
    psynth_net* pnet );
int psynth_get_scaled_ctl_value( uint mod_num, uint ctl_num, psynth_net* pnet ); //Output = scaled (0..32768) value for the pattern
void psynth_get_ctl_val_str( //Input = 0...32768 (for any ctl types); Output = value string
    uint mod_num,
    uint ctl_num,
    int val, //0...32768
    char* out_str,
    psynth_net* pnet );
inline psynth_ctl* psynth_get_ctl( psynth_module* mod, uint ctl_num, psynth_net* pnet )
{
    if( !mod ) return NULL;
    if( (unsigned)ctl_num >= (unsigned)mod->ctls_num ) return NULL;
    return &mod->ctls[ ctl_num ];
}

//Temp buffers:
PS_STYPE* psynth_get_temp_buf( uint mod_num, psynth_net* pnet, uint buf_num );

//Stream resampler:
#if defined(PS_STYPE_FLOATINGPOINT) && CPUMARK >= 10
    #define PSYNTH_RESAMP_INTERP_SPLINE
    #define PSYNTH_RESAMP_INTERP_AFTER 2
    #define PSYNTH_RESAMP_INTERP_BEFORE 1
#else
    #define PSYNTH_RESAMP_INTERP_AFTER 1 //Number of additional frames (after the current frame) required for interpolation
    #define PSYNTH_RESAMP_INTERP_BEFORE 0 //Number of additional frames (before the current frame) required for interpolation
#endif
#define PSYNTH_RESAMP_BUF_TAIL ( PSYNTH_RESAMP_INTERP_AFTER + PSYNTH_RESAMP_INTERP_BEFORE + 1 ) //Additional frames in the begin/end of the resampler buffer
#define PSYNTH_RESAMP_FLAG_MODE0	0 //input len = auto;   output len = manual;
#define PSYNTH_RESAMP_FLAG_MODE1	1 //input len = manual; output len = manual;
#define PSYNTH_RESAMP_FLAG_MODE2	2 //input len = manual; output len = auto;
#define PSYNTH_RESAMP_FLAG_MODE		3 //mode mask
#define PSYNTH_RESAMP_FLAG_DONT_RESET_WHEN_EMPTY	( 1 << 4 ) //instead switch to state -1
struct psynth_resampler
{
    psynth_net* 		pnet;
    psynth_module* 		mod;
    uint32_t			flags;
    int				in_smprate;
    int				out_smprate;
    uint			ratio_fp; //low freq -> high freq: 0...65536
				          //high freq -> low freq: 65536...+
				          //ratio = input buffer step length

    int				input_buf_size; //resamp_buf capacity (in frames)
    uint                    	input_frames; //required
    uint	                input_frames_fp; //fp = fixed point 16.16; for uint32_t: input_frames MUST BE <= 65353!
    uint	                input_ptr_fp;
    PS_STYPE			input_buf_tail[ PSYNTH_RESAMP_BUF_TAIL * PSYNTH_MAX_CHANNELS ]; //last filled frames from the input buffer
    uint			input_empty_frames;
    uint			input_empty_frames_max;

    //for mode1, mode2:
    int				input_frames_avail; //actual (available) number of input frames
    int				input_delay; //additional input delay
    PS_STYPE*			input_delay_bufs[ PSYNTH_MAX_CHANNELS ];
    bool			input_delay_bufs_empty;
    int				input_offset;

    uint	                output_frames;

    int8_t                      state;
				// -1 - ready for disabling; can be switched to state 0 manually;
				//  0 - disabled (waiting for non-zero data on the input);
				//  1 - enabled;
};
psynth_resampler* psynth_resampler_new( psynth_net* pnet, uint mod_num, int in_smprate, int out_smprate, int ratio_fp, uint32_t flags );
int psynth_resampler_change( psynth_resampler* r, int in_smprate, int out_smprate, int ratio_fp, uint32_t flags );
PS_STYPE* psynth_resampler_input_buf( psynth_resampler* r, uint buf_num );
inline int psynth_resampler_input_buf_offset( psynth_resampler* r ) { return PSYNTH_RESAMP_BUF_TAIL + r->input_delay; } //call this before writing data to the input buffer
void psynth_resampler_remove( psynth_resampler* r );
void psynth_resampler_reset( psynth_resampler* r );
int psynth_resampler_begin( //Return value: number of the input frames, that user must put to the resamp_buf + psynth_resampler_input_buf_offset()
    psynth_resampler* r,
    int input_frames, //available input frames or 0
    int output_frames //required output frames or 0
    );
int psynth_resampler_end(
    psynth_resampler* r,
    int input_filled,
    PS_STYPE** inputs, //resampling buffers
    PS_STYPE** outputs,
    int output_count,
    int output_offset );

//Data chunks - storage for some module-specific data (samples, envelopes, etc.).
//This data will be saved to the SunVox project file.
void psynth_new_chunk( uint mod_num, uint num, size_t size, uint flags, int freq, psynth_net* pnet ); //Create new chunk of "size" bytes
void psynth_new_chunk( uint mod_num, uint num, psynth_chunk* c, psynth_net* pnet );
void psynth_replace_chunk_data( uint mod_num, uint num, void* data, psynth_net* pnet ); //Replace by new memory block (allocated by smem_alloc())
void* psynth_get_chunk_data( uint mod_num, uint num, psynth_net* pnet ); //Get chunk data, or NULL if chunk is not exists
inline void* psynth_get_chunk_data( psynth_module* mod, uint num ) //Get chunk data (fast version)
{
    void* retval = NULL;
    if( mod->chunks )
    {
        uint count = smem_get_size( mod->chunks ) / sizeof( psynth_chunk* );
        if( num < count )
        {
    	    psynth_chunk* c = mod->chunks[ num ];
    	    if( c )
    	    { 
    		retval = c->data;
    	    }
    	}
    }
    return retval;
}
#define PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE		( 1 << 0 )
void* psynth_get_chunk_data_autocreate( uint mod_num, uint num, size_t size, bool* created, uint flags, psynth_net* pnet );
int psynth_get_chunk_info( uint mod_num, uint num, psynth_net* pnet, size_t* size, uint* flags, int* freq );
void psynth_set_chunk_info( uint mod_num, uint num, psynth_net* pnet, uint flags, int freq );
void psynth_set_chunk_flags( uint mod_num, uint num, psynth_net* pnet, uint32_t fset, uint32_t freset );
void* psynth_resize_chunk( uint mod_num, uint num, size_t new_size, psynth_net* pnet );
void psynth_remove_chunk( uint mod_num, uint num, psynth_net* pnet );
void psynth_remove_chunks( uint mod_num, psynth_net* pnet ); //Remove all chunks in module

//Number of inputs/outputs:
int psynth_get_number_of_outputs( uint mod_num, psynth_net* pnet );
inline int psynth_get_number_of_outputs( psynth_module* mod ) //(fast version)
{
    return mod->output_channels;
}
int psynth_get_number_of_inputs( uint mod_num, psynth_net* pnet );
inline int psynth_get_number_of_inputs( psynth_module* mod ) //(fast version)
{
    return mod->input_channels;
}
void psynth_set_number_of_outputs( int num, uint mod_num, psynth_net* pnet );
void psynth_set_number_of_inputs( int num, uint mod_num, psynth_net* pnet );

//Misc:
#define PSYNTH_NOISE_TABLE_SIZE 	32768
#define PSYNTH_BASE_WAVE_SIZE		256
enum
{
    PSYNTH_BASE_WAVE_TRI = 0,
    PSYNTH_BASE_WAVE_TRI3, //triangle ^ 3
    PSYNTH_BASE_WAVE_SAW,
    PSYNTH_BASE_WAVE_SAW3, //saw ^ 3
    PSYNTH_BASE_WAVE_SQ,
    PSYNTH_BASE_WAVE_SIN,
    PSYNTH_BASE_WAVE_HSIN,
    PSYNTH_BASE_WAVE_ASIN,
    PSYNTH_BASE_WAVE_SIN3, //sin ^ 3
    PSYNTH_BASE_WAVES
};
#define PSYNTH_UNKNOWN_NOTE		(-999999)
int psynth_str2note( const char* note_str ); //return note number or PSYNTH_UNKNOWN_NOTE
int8_t* psynth_get_noise_table(); //int8_t * PSYNTH_NOISE_TABLE_SIZE
void* psynth_get_sine_table( int bytes_per_sample, bool sign, int length_bits, int amp );
PS_STYPE* psynth_get_base_wavetable();

#if defined(PS_STYPE_FLOATINGPOINT)
    //1/(1<<24) = 1/pow(2,24) = 0x1p-24;
    //white noise amplitude from denorm_add_white_noise() = 1.349959e-20; 0x1.FEp-67;
    //exact range (contiguous) of integers that can be expressed as float32 is: signed 25bit int -16777216...16777215
    #if CPUMARK >= 10
	#define PS_MIN_STYPE_VALUE ( 0x1p-24 )
    #else
	#define PS_MIN_STYPE_VALUE ( 0x1p-20 )
    #endif
    #define PS_STYPE_IS_MIN( vvv ) ( fabs( vvv ) <= PS_MIN_STYPE_VALUE )
    #define PS_STYPE_IS_NOT_MIN( vvv ) ( fabs( vvv ) > PS_MIN_STYPE_VALUE )
#else
    #define PS_MIN_STYPE_VALUE 0
    #define PS_STYPE_IS_MIN( vvv ) ( vvv == 0 )
    #define PS_STYPE_IS_NOT_MIN( vvv ) ( vvv != 0 )
#endif

#if defined(PS_STYPE_FLOATINGPOINT) && defined(DENORMAL_NUMBERS)
    #define psynth_denorm_add_white_noise( val ) denorm_add_white_noise( val )
#else
    #define psynth_denorm_add_white_noise( val ) /**/
#endif

#include "dsp.h"
#include "psynth_dsp.h"
#include "psynth_strings.h"
#include "psynth_gui_utils.h"
