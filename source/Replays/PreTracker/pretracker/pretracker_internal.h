#pragma once

#include "pretracker.h"

#include <stdlib.h>

typedef uint8_t u8;
typedef int8_t i8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;
typedef float f32;
typedef double f64;

static inline u16 read_be16(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}

static inline u32 read_be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static inline f32 pretracker_clamp_sample(f32 x) {
    if (x > 127.0f / 128.0f)
        return 127.0f / 128.0f;
    if (x < -1.0f)
        return -1.0f;
    return x;
}

#ifndef nullptr
#define nullptr ((void*)0)
#endif

// Internal type forward declarations (not exposed in public API)
typedef struct SongState SongState;
typedef struct PlayerState PlayerState;
typedef struct MixerState MixerState;

// Constants matching raspberry_casket.asm
#define MAX_VOLUME 64
#define MAX_SPEED 0x2F
#define MAX_WAVES 24
#define MAX_INSTRUMENTS 32
#define MAX_TRACK_DELAY 32
#define NOTE_OFF_PITCH 0x3D
#define NOTES_IN_OCTAVE 12
#define NUM_CHANNELS 4
#define AMIGA_CLOCK 3546895.0
#define SINC_TAPS 8
#define SINC_PHASES 128

extern const u16 s_log12_table[NOTES_IN_OCTAVE];
extern const u8 s_vib_speed_table[16];
extern const u8 s_vib_depth_table[16];
extern const u8 s_vib_delay_table[16];
extern const u8 s_ramp_up_16[16];
extern const i16 s_fast_roll_off_16[16];
extern const i16 s_roll_off_table[];
extern const i16 s_ramp_up_down_32[32];
extern const u16 s_modulator_ramp_8[8];
extern const u16 s_period_table[3 * NOTES_IN_OCTAVE + 1];
extern const f32 s_blep_table[8][5];
extern const f32 s_sinc_table[SINC_PHASES][SINC_TAPS];

// Amiga base period (samples per waveform cycle at the lowest note)
#define AMIGA_MAX_PERIOD 128
// Sample generation period: AMIGA_MAX_PERIOD = native resolution, 222 = HQ resolution
#define HQ_MAX_PERIOD AMIGA_MAX_PERIOD

// WaveInfo (42 bytes, matches wi_* at raspberry_casket.asm:364-402)
// Packed to match assembly layout exactly.
typedef struct {
    u16 loop_start;       // $00 wi_loop_start_w
    u16 loop_end;         // $02 wi_loop_end_w
    u16 subloop_len;      // $04 wi_subloop_len_w
    u8 allow_9xx;         // $06 wi_allow_9xx_b
    u8 subloop_wait;      // $07 wi_subloop_wait_b
    u16 subloop_step;     // $08 wi_subloop_step_w
    u16 chipram;          // $0A wi_chipram_w
    u16 loop_offset;      // $0C wi_loop_offset_w
    u8 chord_note1;       // $0E wi_chord_note1_b
    u8 chord_note2;       // $0F wi_chord_note2_b
    u8 chord_note3;       // $10 wi_chord_note3_b
    u8 chord_shift;       // $11 wi_chord_shift_b
    u8 osc_unknown;       // $12 wi_osc_unknown_b (always $00?)
    u8 osc_phase_spd;     // $13 wi_osc_phase_spd_b
    u8 flags;             // $14 wi_flags_b
    u8 osc_phase_min;     // $15 wi_osc_phase_min_b
    u8 osc_phase_max;     // $16 wi_osc_phase_max_b
    u8 osc_basenote;      // $17 wi_osc_basenote_b
    u8 osc_gain;          // $18 wi_osc_gain_b
    u8 sam_len;           // $19 wi_sam_len_b (multiples of 128, zero-based)
    u8 mix_wave;          // $1A wi_mix_wave_b
    u8 vol_attack;        // $1B wi_vol_attack_b
    u8 vol_delay;         // $1C wi_vol_delay_b
    u8 vol_decay;         // $1D wi_vol_decay_b
    u8 vol_sustain;       // $1E wi_vol_sustain_b
    u8 flt_type;          // $1F wi_flt_type_b (1=LP, 2=HP, 3=BP, 4=notch)
    u8 flt_resonance;     // $20 wi_flt_resonance_b
    u8 pitch_ramp;        // $21 wi_pitch_ramp_b
    u8 flt_start;         // $22 wi_flt_start_b
    u8 flt_min;           // $23 wi_flt_min_b
    u8 flt_max;           // $24 wi_flt_max_b
    u8 flt_speed;         // $25 wi_flt_speed_b
    u8 mod_wetness;       // $26 wi_mod_wetness_b
    u8 mod_length;        // $27 wi_mod_length_b
    u8 mod_predelay;      // $28 wi_mod_predelay_b
    u8 mod_density;       // $29 wi_mod_density_b (bits 0-2: density, bits 3-4: unisono, bit 5: post)
} WaveInfo;                // $2A = 42 bytes

// WaveInfo flag bits
#define WI_FLAG_OSC_TYPE_MASK 0x03  // bits 0-1: 00=saw, 01=tri, 10=sqr, 11=noise
#define WI_FLAG_EXTRA_OCTAVES 0x04  // bit 2: needs extra octaves
#define WI_FLAG_BOOST         0x08  // bit 3: volume boost
#define WI_FLAG_PITCH_LINEAR  0x10  // bit 4: linear pitch ramp
#define WI_FLAG_VOL_FAST      0x20  // bit 5: fast volume envelope

// Pattern commands (used in process_pattern_effects switch)
typedef enum {
    PAT_CMD_PLAY_2ND_INST   = 0x00, // 0xx: Play 2nd instrument
    PAT_CMD_SLIDE_UP        = 0x01, // 1xx: Slide up
    PAT_CMD_SLIDE_DOWN      = 0x02, // 2xx: Slide down
    PAT_CMD_TONE_PORTAMENTO = 0x03, // 3xx: Tone portamento
    PAT_CMD_SET_VIBRATO     = 0x04, // 4xx: Set vibrato
    PAT_CMD_TRACK_DELAY     = 0x05, // 5xy: Track delay
    PAT_CMD_SET_WAVE_OFFSET = 0x09, // 9xx: Set wave offset
    PAT_CMD_VOLUME_RAMP     = 0x0A, // Axy: Volume ramp
    PAT_CMD_POSITION_JUMP   = 0x0B, // Bxx: Position jump
    PAT_CMD_SET_VOLUME      = 0x0C, // Cxx: Set volume
    PAT_CMD_PATTERN_BREAK   = 0x0D, // Dxx: Pattern break
    PAT_CMD_EXTENDED        = 0x0E, // Exy: Extended commands
    PAT_CMD_SET_SPEED       = 0x0F, // Fxx: Set speed
} PatternCmd;

// Pattern extended sub-commands (Exy)
typedef enum {
    PAT_EXT_FINE_SLIDE_UP   = 0x01, // E1x: Fine slide up
    PAT_EXT_FINE_SLIDE_DOWN = 0x02, // E2x: Fine slide down
    PAT_EXT_NOTE_OFF_DELAY  = 0x0A, // EAx: Note off in x sub steps
    PAT_EXT_NOTE_DELAY      = 0x0D, // EDx: Note delay in x sub steps
} PatternExtCmd;

// Instrument commands (used in process_instrument_steps switch)
typedef enum {
    INST_CMD_SELECT_WAVE        = 0x00, // 0xx: Select wave (with sync)
    INST_CMD_SLIDE_UP           = 0x01, // 1xx: Slide up
    INST_CMD_SLIDE_DOWN         = 0x02, // 2xx: Slide down
    INST_CMD_ADSR               = 0x03, // 30x: ADSR control
    INST_CMD_SELECT_WAVE_NOSYNC = 0x04, // 4xx: Select wave (no sync)
    INST_CMD_VOLUME_SLIDE       = 0x0A, // Axy: Volume slide
    INST_CMD_JUMP_TO_STEP       = 0x0B, // Bxx: Jump to step
    INST_CMD_SET_VOLUME         = 0x0C, // Cxx: Set volume
    INST_CMD_USE_PAT_ARP        = 0x0E, // E0x/E1x: Use pattern arpeggio
    INST_CMD_SET_SPEED          = 0x0F, // Fxx: Set speed
} InstrumentCmd;

// ADSR sub-command values (cmd_data for INST_CMD_ADSR)
typedef enum {
    ADSR_CMD_RELEASE = 1, // 301: Trigger release
    ADSR_CMD_RESTART = 2, // 302: Restart attack from 0
} AdsrCmd;

// ADSR phases
typedef enum {
    ADSR_PHASE_ATTACK  = 0,
    ADSR_PHASE_DECAY   = 1,
    ADSR_PHASE_SUSTAIN = 2,
    ADSR_PHASE_RELEASE = 3,
} AdsrPhase;

// Oscillator types (bits 0-1 of wi_flags_b)
typedef enum {
    OSC_TYPE_SAWTOOTH = 0,
    OSC_TYPE_TRIANGLE = 1,
    OSC_TYPE_SQUARE   = 2,
    OSC_TYPE_NOISE    = 3,
} OscType;

// Filter types (wi_flt_type_b)
typedef enum {
    FILTER_NONE     = 0,
    FILTER_LOWPASS  = 1,
    FILTER_HIGHPASS = 2,
    FILTER_BANDPASS = 3,
    FILTER_NOTCH    = 4,
} FilterType;

// Mod density bit field layout (wi_mod_density_b)
#define MOD_DENSITY_MASK   0x07  // bits 0-2: density count
#define MOD_UNISONO_SHIFT  3     // bits 3-4: unisono detune
#define MOD_UNISONO_MASK   0x03  // mask after shift
#define MOD_POST_FLAG      0x20  // bit 5: post-modulator

// Pitch control byte bits (pdb_pitch_ctrl)
#define PITCH_CTRL_INST_HI   0x80 // bit 7: high bit of instrument number
#define PITCH_CTRL_HAS_ARP   0x40 // bit 6: has arpeggio notes
#define PITCH_CTRL_NOTE_MASK 0x3F // bits 0-5: note value

// InstrumentInfo (8 bytes raw from PRT file, matches ii_* at raspberry_casket.asm:354-362)
typedef struct {
    u8 vibrato_delay;   // $00 ii_vibrato_delay
    u8 vibrato_depth;   // $01 ii_vibrato_depth
    u8 vibrato_speed;   // $02 ii_vibrato_speed
    u8 adsr_attack;     // $03 ii_adsr_attack
    u8 adsr_decay;      // $04 ii_adsr_decay
    u8 adsr_sustain;    // $05 ii_adsr_sustain
    u8 adsr_release;    // $06 ii_adsr_release
    u8 pattern_steps;   // $07 ii_pattern_steps
} InstrumentInfo;       // 8 bytes

// UnpackedInstrumentInfo (matches uii_* at raspberry_casket.asm:406-417)
// Pre-processed values with lookup tables applied.
typedef struct {
    i16 vibrato_delay;   // $00 uii_vibrato_delay (word)
    i16 vibrato_depth;   // $02 uii_vibrato_depth (word)
    i16 vibrato_speed;   // $04 uii_vibrato_speed (word)
    u8 adsr_release;     // $06 uii_adsr_release (byte)
    u8 _pad0;            // $07 padding
    i16 adsr_attack;     // $08 uii_adsr_attack (word)
    i16 adsr_decay;      // $0A uii_adsr_decay (word)
    i16 adsr_sustain;    // $0C uii_adsr_sustain (word)
    u8 pattern_steps;    // $0E uii_pattern_steps (byte)
    u8 _pad1;            // $0F padding
} UnpackedInstrumentInfo; // $10 = 16 bytes

// Oscillator buffers per note (matches owb_* at raspberry_casket.asm:540-545)
typedef struct {
    f32 saw_waves[HQ_MAX_PERIOD];   // $00 owb_saw_waves
    f32 sqr_waves[HQ_MAX_PERIOD];   // $80 owb_sqr_waves
    f32 tri_waves[HQ_MAX_PERIOD];   // $100 owb_tri_waves
    u16 wave_length;                // $180 owb_wave_length (period for this note)
} OscNoteBuffers;

// SongState (matches sv_* at raspberry_casket.asm:421-438)
struct SongState {
    WaveInfo* waveinfo_table[MAX_WAVES];                // sv_waveinfo_table
    u8* inst_patterns_table[MAX_INSTRUMENTS];            // sv_inst_patterns_table
    u32 wavelength_table[MAX_WAVES];                     // sv_wavelength_table
    u32 wavetotal_table[MAX_WAVES];                      // sv_wavetotal_table
    u8 wavegen_order_table[MAX_WAVES];                   // sv_wavegen_order_table
    u8 num_waves;                                        // sv_num_waves_b
    u8 num_steps;                                        // sv_num_steps_b
    u8 num_patterns;                                     // valid entries in sv_pattern_table
    u8 num_subsongs;                                     // V1.5: subsong count from offset 0x5A
    u8* patterns_ptr;                                    // sv_patterns_ptr
    u16 curr_pat_pos;                                    // sv_curr_pat_pos_w
    u16 pat_pos_len;                                     // sv_pat_pos_len_w
    u16 pat_restart_pos;                                 // sv_pat_restart_pos_w
    u8* pos_data_adr;                                    // sv_pos_data_adr
    WaveInfo* waveinfo_ptr;                              // sv_waveinfo_ptr
    u8* pattern_table[256];                              // sv_pattern_table
    UnpackedInstrumentInfo inst_infos[MAX_INSTRUMENTS];  // sv_inst_infos_table
};

// Output channel data (matches ocd_* at raspberry_casket.asm:443-450)
typedef struct {
    u32 sam_ptr_offset;   // ocd_sam_ptr (sample pointer offset)
    u16 length;           // ocd_length
    u16 loop_offset;      // ocd_loop_offset
    u16 period;           // ocd_period
    u8 volume;            // ocd_volume
    u8 trigger;           // ocd_trigger
    u32 unused;           // ocd_unused (padding to 16 bytes)
} OutputChannelData;

// Per-channel data (matches pcd_* at raspberry_casket.asm:453-538)
typedef struct {
    i16 pat_portamento_dest;
    i16 pat_pitch_slide;
    i8 pat_vol_ramp_speed;
    u8 pat_2nd_inst_num4;
    u8 pat_2nd_inst_delay;
    u8 wave_offset;
    i16 inst_pitch_slide;
    i16 inst_sel_arp_note;
    i16 inst_note_pitch;
    i16 inst_curr_port_pitch;
    u8 inst_line_ticks;
    u8 inst_pitch_pinned;
    i8 inst_vol_slide;
    u8 inst_step_pos;
    u16 inst_wave_num;
    u8 track_delay_offset;
    u8 inst_speed_stop;
    i16 inst_pitch;
    i16 inst_vol;
    u8 loaded_inst_vol;
    u8 pat_vol;
    u8 arp_notes[4];
    u16 last_trigger_pos;
    u8 pat_portamento_speed;
    u8 pat_adsr_rel_delay;
    u8 note_off_delay;
    u8 inst_pattern_steps;
    u8 note_delay;
    u8 track_delay_steps;
    u8 track_delay_vol16;
    u8 track_init_delay;
    u16 inst_num4;
    u16 inst_subloop_wait;
    u16 inst_loop_offset;
    UnpackedInstrumentInfo* inst_info_ptr;
    WaveInfo* waveinfo_ptr;
    u8 channel_mask;
    u8 channel_num;
    u16 adsr_phase;
    u16 adsr_volume;
    u8 adsr_phase_speed;
    u8 inst_ping_pong_dir;
    u16 adsr_pos;
    u16 adsr_vol64;
    u8 new_inst_num;
    u8 _pad0;
    u16 vibrato_pos;
    u16 vibrato_delay;
    u16 vibrato_depth;
    u16 vibrato_speed;
    u8 adsr_release;
    u8 _pad1;
    OutputChannelData out;
    OutputChannelData track_delay_buffer[MAX_TRACK_DELAY];
} PerChannelData;

// Per-channel mixer state (PC-specific, not in assembly)
typedef struct {
    f64 frac_pos;       // fractional sample position for resampling
    f64 speed;          // advance per output sample = freq / output_rate
    f32* sample_data;   // resolved pointer to sample data
    f32* loop_data;     // loop buffer (set on trigger, used on first DMA reload)
    u32 sample_length;  // playback length in bytes
    u32 loop_offset;    // loop start offset (0xFFFF = no loop)
    bool active;        // channel producing sound
    f32 volume;         // final volume as float 0..1
    f32 pan_left;       // stereo pan gain
    f32 pan_right;      // stereo pan gain
    // BLEP anti-aliasing state
    f32 prev_sample;        // previous sample value (for discontinuity detection)
    f32 prev_volume;        // previous volume value (for volume BLEP)
    f32 blep_buf[8];        // sample-change BLEP circular buffer
    f32 vol_blep_buf[8];    // volume-change BLEP circular buffer
    i32 blep_read_idx;      // read/drain index for sample BLEP
    i32 blep_count;         // pending taps in sample BLEP
    i32 vol_blep_read_idx;  // read/drain index for volume BLEP
    i32 vol_blep_count;     // pending taps in volume BLEP
    f32 sub_accum;          // sub-sample accumulator (0..1)
    f32 blep_step;          // step rate when last transition occurred
    f32 blep_wrap;          // accumulator value when last transition occurred
} MixerChannel;

// PlayerState (matches pv_* at raspberry_casket.asm:549-598)
struct PlayerState {
    u8 pat_curr_row;
    u8 next_pat_row;
    u8 next_pat_pos;
    u8 pat_speed_even;
    u8 pat_speed_odd;
    u8 pat_line_ticks;
    u8 pat_stopped;
    u8 songend_detected;
    u8 loop_pattern;
    u8 _pad0;
    u16 trigger_mask;
    SongState* my_song;
    f32* sample_buffer_ptr;
    u32 copperlist_ptr;
    f32* wave_sample_table[MAX_WAVES];
    u16 period_table[16 * NOTES_IN_OCTAVE * 3];
    PerChannelData channeldata[NUM_CHANNELS];
    OscNoteBuffers osc_buffers[NOTES_IN_OCTAVE];
    u32 precalc_sample_size;
    u32 precalc_progress_ptr;
    u16 wg_wave_ord_num;
    f32* wg_curr_sample_ptr;
    f32* wg_curr_samend_ptr;
    u16 wg_curr_sample_len;
    u8 wg_chord_note_num;
    u8 wg_unisono_run;
    u16 wg_chord_flag;
    u8 wg_chord_pitches[4];
    i32 wg_osc_speed;
    f32 wg_flt_taps[4];
};

// Top-level mixer state
struct MixerState {
    MixerChannel channels[NUM_CHANNELS];
    u32 output_rate;        // 44100 or 48000
    u32 samples_per_tick;   // output_rate / 50
    u32 samples_until_tick; // countdown
    i32 solo_channel;       // -1 = all channels, 0-3 = solo that channel
    f32 stereo_mix;         // 0.0 = full stereo (Amiga hard-pan), 1.0 = mono
    u8 interp_mode;         // PreInterpMode: 0=BLEP, 1=sinc
    u32 haas_delay_samples; // Haas cross-feed delay in samples (0 = disabled)
    f32 haas_blend;         // cross-feed level (0.3)
    f32 haas_buf_l[64];     // delay ring buffer for left channel
    f32 haas_buf_r[64];     // delay ring buffer for right channel
    u32 haas_write_idx;     // ring buffer write position
    f32* sample_buffer_ptr; // base of sample buffer (for bounds checking)
    u32 sample_buffer_size; // total size of sample buffer
};

// Internal subsystem interfaces (not part of the public API)
bool pretracker_read_name_record(const u8** cursor, const u8* end, char* output, size_t output_size);
u32  pretracker_parse_song(SongState* song, u8* prt_data, u32 prt_size, int subsong);
bool pretracker_apply_subsong(SongState* song, u8* prt_data, u32 prt_size, int subsong);
void pretracker_rebuild_pattern_table(SongState* song);
void pretracker_player_init(PlayerState* player, f32* sample_buffer, SongState* song);
void pretracker_wavegen_generate(PlayerState* player);
void pretracker_player_tick(PlayerState* player);
void pretracker_init_channel(PerChannelData* channel, const SongState* song, u8 channel_num);
void pretracker_player_start(PlayerState* player, SongState* song);
void pretracker_mixer_init(MixerState* mixer, u32 output_rate);
void pretracker_mixer_start(MixerState* mixer, const PlayerState* player, const SongState* song);
int  pretracker_mixer_render(PlayerState* player, MixerState* mixer, f32* buffer, int num_frames, f32** scopes, int num_scopes);
int  pretracker_mixer_muted(PlayerState* player, MixerState* mixer, int num_frames); // rePlayer
bool pretracker_player_is_finished(const PlayerState* player);
