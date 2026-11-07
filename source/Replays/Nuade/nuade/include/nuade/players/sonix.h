// SPDX-License-Identifier: MIT
//
// The Sonix player's own state, for tools that need to look at what the player is doing (displays, debuggers).
// This is the live internal state, not a stable interface: fields follow the player's implementation and may change
// between releases. Treat it as read-only; writing to it while the song plays corrupts playback.

#pragma once

#include <nuade/nuade.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SNX_MAX_ZONES 32
#define SONIX_NUM_CHANNELS 4

typedef struct SonixSong SonixSong;

typedef enum SonixFormat {
    SONIX_FORMAT_UNKNOWN = 0,
    SONIX_FORMAT_SNX,
    SONIX_FORMAT_SMUS,
    SONIX_FORMAT_TINY,
} SonixFormat;

typedef struct SonixSongMetadata {
    uint8_t num_channels;
    SonixFormat format;
    bool has_form_header;
    bool valid;
    uint32_t num_ins_chunks;
    uint32_t num_real_samples;
    uint32_t num_track_chunks;
} SonixSongMetadata;

typedef struct {
    int8_t* pcm;
    uint32_t pcm_len;
    uint32_t loop_start;
    uint32_t loop_len;
    uint8_t base_note;
    uint8_t low_key;
    uint8_t high_key;
    uint32_t source_rate;
    uint16_t attack_time;
    uint16_t decay_time;
} SonixInstZone;

struct SonixSong {
    SonixSongMetadata metadata;
    const uint8_t* data;
    uint32_t size;
    bool running;
    char error[128];
    char instrument_names[64][64];
    int8_t* instrument_pcm[64];
    uint32_t instrument_pcm_len[64];
    uint32_t instrument_loop_start[64];
    uint32_t instrument_loop_len[64];
    uint8_t instrument_base_note[64];
    uint32_t instrument_base_period[64];
    uint32_t instrument_source_rate[64];
    uint16_t instrument_vib_depth[64]; // SS: signed 16-bit depth (file 0x7A)
    uint16_t instrument_vib_speed[64];
    uint16_t instrument_vib_delay[64];
    SonixInstZone inst_zones[64][SNX_MAX_ZONES];
    uint8_t inst_zone_count[64];
    bool instrument_is_ss[64];
    bool instrument_is_iff[64];   // 8SVX/IFF tech instruments (IFFTech pitch + volume)
    uint32_t iff_vhdr_volume[64]; // VHDR volume field (32-bit fixed point, 0x10000 = 1.0)
    bool instrument_is_synth[64];
    uint16_t synth_base_vol[64];                   // $1AC: Synttech base volume
    uint16_t synth_port_flag[64];                  // $1AE: non-zero → portamento modulates volume
    int8_t synth_wave[64][128];                    // $24: 128-byte base waveform for rendering
    bool synth_wave_set[64];                       // whether synth_wave has been loaded
    uint16_t synth_c2[64];                         // $1C2: blend/ring rate (0=simple copy)
    uint16_t synth_c4[64];                         // $1C4: ring mod depth (0=blend, >0=ring)
    int16_t synth_blend_accum[SONIX_NUM_CHANNELS]; // $16(A2): per-tick accumulator
    int16_t synth_ring_dir[SONIX_NUM_CHANNELS];    // $18(A2): ring mod direction
    int8_t synth_output[SONIX_NUM_CHANNELS][128];  // rendered waveform per channel (next buffer)
    int8_t synth_buf[SONIX_NUM_CHANNELS][2][128]; // the two buffers Paula alternates between (one loops, one is queued)
    bool synth_pending[SONIX_NUM_CHANNELS];       // a rendered buffer is queued on the voice, not yet looping

    // Synthesis filter bank: 64 filtered copies of the 128-byte waveform.
    // Assembly: OneFilter (line 2866) pre-computes these during instrument init.
    int8_t* synth_filter_bank[64];      // 64×128 = 8192 bytes per instrument, NULL if not synth
    int8_t synth_env_table[64][256];    // envelope table (256 signed bytes, index = accum >> 8, from $A4/file 0xC4)
    uint16_t synth_filter_base[64];     // $19A: filter base offset (XOR 0xFF)
    uint16_t synth_filter_range[64];    // $19C: portamento -> filter scaling
    uint16_t synth_filter_env_sens[64]; // $19E: envelope -> filter scaling
    uint16_t synth_env_scan_rate[64];   // $1A0: envelope phase scan rate
    int16_t synth_env_loop_mode[64];    // $1A2: 0=one-shot, positive=loop, negative=done
    uint16_t synth_env_delay_init[64];  // $1A4: initial envelope delay
    uint16_t synth_env_vol_scale[64];   // $194: envelope -> volume scaling
    uint16_t synth_env_pitch_scale[64]; // $198: envelope -> pitch scaling
    uint16_t synth_slide_rate[64];      // $1B2: note-to-note period slide rate

    // Per-channel synthesis state
    uint16_t synth_slide_counter[SONIX_NUM_CHANNELS]; // 2(A2): period slide countdown
    int16_t synth_slide_delta[SONIX_NUM_CHANNELS];    // 4(A2): period change per tick

    // Per-channel synthesis envelope state
    uint16_t synth_env_accum[SONIX_NUM_CHANNELS];       // puVar15[8]: envelope phase accumulator
    int16_t synth_env_delay_ctr[SONIX_NUM_CHANNELS];    // puVar15[9]: delay counter (-1=done)
    int16_t synth_env_value[SONIX_NUM_CHANNELS];        // puVar15[10]: current envelope value
    uint16_t synth_cur_period[SONIX_NUM_CHANNELS];      // 0(A2): current synth period
    uint8_t synth_decimation_shift[SONIX_NUM_CHANNELS]; // 8(A2): decimation shift from render tick

    // AIFF tech attack/decay per instrument (from AIFF NAME chunk, assembly: 8(A5), 10(A5))
    uint16_t inst_attack_time[64];
    uint16_t inst_decay_time[64];

    // Per-channel AIFF tech envelope state (assembly: 2(A2), 4(A2), 6(A2), 10(A1))
    uint16_t aiff_tech_vol[SONIX_NUM_CHANNELS];     // 2(A2): current envelope value (0-0xFF00)
    uint16_t aiff_attack_speed[SONIX_NUM_CHANNELS]; // 4(A2): 0xFF00 / (attack_time+1)
    uint16_t aiff_decay_speed[SONIX_NUM_CHANNELS];  // 6(A2): 0xFF00 / (decay_time+1)
    bool aiff_note_active[SONIX_NUM_CHANNELS];      // 10(A1): true=attack/sustain, false=decay

    // SS portamento envelope params per instrument (from .instr file)
    uint16_t ss_inst_vol[64];       // $48: instrument volume
    uint16_t ss_port_target[64][4]; // $4A,$4C,$4E,$50: targets per phase
    uint16_t ss_port_speed[64][4];  // $52,$54,$56,$58: speeds per phase

    // SS portamento per-channel runtime state
    uint16_t ss_port_phase[SONIX_NUM_CHANNELS]; // 0, 2, 4, or 6
    uint32_t ss_port_value[SONIX_NUM_CHANNELS]; // 32-bit 16.16 fixed-point

    // SMUS runtime (assembly offsets in parentheses, relative to A6/A5 base)
    uint16_t smus_prescaler;                         // $32(A6): tick subdivision counter
    uint16_t smus_tpb;                               // $38(A6): ticks-per-beat from tempo table
    uint16_t smus_last_tempo_idx;                    // $34(A6): cached tempo index for change detection
    uint16_t smus_note_pitch_word;                   // $04(A2): pitch word from SNX1 for note computation
    uint16_t smus_pitch_scaling;                     // $04(A6): pitch scaling from SNX1 word 1, used by synth pitch env
    uint32_t smus_note_wait[SONIX_NUM_CHANNELS];     // $24A(A5): note wait counter per channel
    uint32_t smus_release_wait[SONIX_NUM_CHANNELS];  // $23A(A5): release counter per channel
    uint16_t smus_velocity[SONIX_NUM_CHANNELS];      // $24C(A5): running velocity per channel
    uint16_t smus_velocity_init[SONIX_NUM_CHANNELS]; // $23C(A5): initial velocity shift per channel
    uint8_t smus_cur_inst[SONIX_NUM_CHANNELS];       // current instrument index per channel (from 0x81xx)
    bool smus_has_inst[SONIX_NUM_CHANNELS];          // whether channel has a valid instrument pointer
    bool smus_inst_present[SONIX_NUM_CHANNELS];      // $3A(A5): per-channel instrument presence from SNX1
    uint16_t smus_inst_present_raw[SONIX_NUM_CHANNELS]; // lower word of $3A(A5): controls velocity halving
    bool tiny_inst_present[SONIX_NUM_CHANNELS];         // TINY: per-channel instrument gate from file offset 0x20
    uint32_t smus_stream_pos[SONIX_NUM_CHANNELS];       // $27A(A5): current read position in track data
    uint8_t smus_init_inst[SONIX_NUM_CHANNELS];         // $28A: initial instrument per channel (from scan)
    uint8_t smus_init_wait[SONIX_NUM_CHANNELS];         // $28B: initial note wait per channel (from scan)
    uint32_t smus_tick_count;                           // $10(A6): current tick for song length check
    int32_t smus_max_ticks;                             // $14(A6): max ticks (-1 = use all-ended check)
    uint16_t smus_ramp_target;                          // 0(A6): score master volume (from SHDR vol byte)
    uint16_t smus_base_velocity[SONIX_NUM_CHANNELS];    // $12(A1): per-channel base velocity from score
    uint16_t smus_snx1_offset;                          // file offset of SNX1 data within the SMUS file
    uint16_t smus_snx1_len;                             // length of SNX1 data
    uint16_t smus_tempo_divisor;                        // 8(A5): ((xx>>3)&0x1F)+1 from 0x82xx commands
    uint16_t smus_tempo_bitmask;                        // 10(A5): 1<<(xx&7) from 0x82xx commands

    // SNX runtime
    bool runtime_track_engine;
    uint32_t snx_track_start[SONIX_NUM_CHANNELS];
    uint32_t snx_track_end[SONIX_NUM_CHANNELS];
    uint32_t snx_track_pos[SONIX_NUM_CHANNELS];
    uint32_t snx_wait[SONIX_NUM_CHANNELS];
    uint8_t snx_cmd81[SONIX_NUM_CHANNELS];
    uint8_t snx_cmd80[SONIX_NUM_CHANNELS];
    int8_t snx_cmd83[SONIX_NUM_CHANNELS];
    bool snx_track_ended[SONIX_NUM_CHANNELS];
    bool snx_note_on[SONIX_NUM_CHANNELS];
    uint8_t snx_note_velocity[SONIX_NUM_CHANNELS];
    uint8_t snx_hw_vol[SONIX_NUM_CHANNELS];

    // SNX StartNOTE only records the request (asm status 1); the tech tick starts it after all tracks ran.
    // A note-off (ReleaseNote: status 0/2) in between cancels the request.
    struct {
        bool valid;
        int note;
        int velocity;
        uint8_t cmd80;
        uint8_t cmd81;
    } snx_pending_start[SONIX_NUM_CHANNELS];

    // Volume ramp slots - 8 slots (4 channels x 2), 8.8 fixed point.
    // Assembly: $5A(A6) current, $6A(A6) target, $7A(A6) speed.
    uint16_t snx_ramp_current[8];
    uint16_t snx_ramp_target[8];
    uint16_t snx_ramp_speed[8];
    uint16_t snx_ticks_per_beat;
    bool snx_use_sample[SONIX_NUM_CHANNELS];
    uint8_t snx_inst_index[SONIX_NUM_CHANNELS];
    const int8_t* snx_active_pcm[SONIX_NUM_CHANNELS];
    uint32_t snx_active_pcm_len[SONIX_NUM_CHANNELS];
    uint32_t snx_active_loop_start[SONIX_NUM_CHANNELS];
    uint32_t snx_active_loop_len[SONIX_NUM_CHANNELS];
    double snx_sample_hz[SONIX_NUM_CHANNELS];
    double snx_sample_base_hz[SONIX_NUM_CHANNELS];
    double snx_vib_phase[SONIX_NUM_CHANNELS];
    uint8_t snx_vib_delay[SONIX_NUM_CHANNELS];
    // SMUS SStech per-voice state (asm: A2 = $464(A6) + ch*$14).
    uint16_t ss_base_period[SONIX_NUM_CHANNELS]; // 6(A2): (0xD5C8 * ratio) >> 15
    uint16_t ss_vib_index[SONIX_NUM_CHANNELS];   // 14(A2): triangle vibrato phase
    uint16_t ss_vib_delay[SONIX_NUM_CHANNELS];   // $10(A2): ticks before vibrato starts
    int32_t shown_note[SONIX_NUM_CHANNELS];      // the score's note last started on the channel, -1 if none
    int32_t shown_inst[SONIX_NUM_CHANNELS];      // the instrument it played, -1 if none
    uint32_t tick_length;                        // CIA cycles until the next tick
    uint32_t first_tick_length;                  // CIA cycles before the first tick
    bool first_tick;                             // the next tick call only waits for the first period
    NuadeVoices* voices;
    bool voice_play[SONIX_NUM_CHANNELS];  // start the active sample from its first frame
    bool voice_queue[SONIX_NUM_CHANNELS]; // (re)queue voice_queued
    NuadeSample voice_queued[SONIX_NUM_CHANNELS];
    uint8_t snx_header_tempo_base;
    uint16_t snx_pitch_offset; // 2(A6): global pitch scaling, from song header offset 2
    uint8_t tempo_base;
    uint8_t tempo_cur;
    uint32_t debug_note_events;
    uint32_t debug_total_note_events;
    uint32_t tick_count;
    uint32_t debug_wait_commands;
    uint32_t debug_loop_resets;
    uint32_t debug_total_ticks;
    FILE* dump_file;
};

// The Sonix state behind a song opened with the sonix player; nullptr when the song is another player's.
SonixSong* sonix_from_nuade(NuadeSong* song);

// Write the per-tick channel state to `path` ("-" = stderr, "" = off), for A/B against the original. False if the file
// cannot be opened.
bool sonix_set_dump_file(SonixSong* song, const char* path);

#ifdef __cplusplus
}
#endif
