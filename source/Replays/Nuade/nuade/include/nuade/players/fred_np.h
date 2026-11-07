// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)

#pragma once

#include <nuade/nuade.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The player's tick rate: one fred_tick per VBlank.
#define FRED_TICK_HZ 50

// Entries in an instrument's arpeggio table; a longer arpeggio count is cut to it.
#define FRED_ARPEGGIO_SIZE 16

typedef struct FredModule FredModule;

// The note (index into the period table) and instrument number a channel last started; -1 when none yet.
typedef struct FredVoiceState {
    int note;
    int instrument;
} FredVoiceState;

typedef enum FredInstrumentType {
    FRED_INST_SAMPLE = 0x00,
    FRED_INST_PULSE = 0x01,
    FRED_INST_BLEND = 0x02,
    FRED_INST_UNUSED = 0xff
} FredInstrumentType;

typedef enum FredEnvelopeState {
    FRED_ENV_ATTACK = 0,
    FRED_ENV_DECAY = 1,
    FRED_ENV_SUSTAIN = 2,
    FRED_ENV_RELEASE = 3,
    FRED_ENV_DONE = 4
} FredEnvelopeState;

typedef struct FredInstrument {
    int16_t instrument_number;
    uint16_t repeat_len;
    uint16_t length;
    uint16_t period;
    uint8_t vib_delay;
    int8_t vib_speed;
    int8_t vib_ampl;
    uint8_t env_vol;
    uint8_t attack_speed;
    uint8_t attack_volume;
    uint8_t decay_speed;
    uint8_t decay_volume;
    uint8_t sustain_delay;
    uint8_t release_speed;
    uint8_t release_volume;
    int8_t arpeggio[FRED_ARPEGGIO_SIZE];
    uint8_t arp_speed;
    FredInstrumentType inst_type;
    int8_t pulse_rate_min;
    int8_t pulse_rate_plus;
    uint8_t pulse_speed;
    uint8_t pulse_start;
    uint8_t pulse_end;
    uint8_t pulse_delay;
    uint8_t inst_sync; // SynchronizeFlag bitmask
    uint8_t blend;
    uint8_t blend_delay;
    uint8_t pulse_shot_counter;
    uint8_t blend_shot_counter;
    uint8_t arp_count;

    int8_t* sample_addr;
    uint32_t sample_size;
} FredInstrument;

typedef struct FredChannelInfo {
    int chan_num;
    int8_t* position_table; // sbyte[256] per sub-song per channel
    uint8_t* track_table;   // pointer to current track data
    uint16_t position;
    uint16_t track_position;
    uint16_t track_duration;
    uint8_t track_note;
    uint16_t track_period;
    int16_t track_volume;
    FredInstrument* instrument;
    uint8_t vib_flags; // FredVibFlags bitmask
    uint8_t vib_delay;
    int8_t vib_speed;
    int8_t vib_ampl;
    int8_t vib_value;
    bool port_running;
    uint16_t port_delay;
    uint16_t port_limit;
    uint8_t port_target_note;
    uint16_t port_start_period;
    int16_t period_diff;
    uint16_t port_counter;
    uint16_t port_speed;
    FredEnvelopeState env_state;
    uint8_t sustain_delay;
    uint8_t arp_position;
    uint8_t arp_speed;
    bool pulse_way;
    uint8_t pulse_position;
    uint8_t pulse_delay;
    uint8_t pulse_speed;
    uint8_t pulse_shot;
    bool blend_way;
    uint16_t blend_position;
    uint8_t blend_delay;
    uint8_t blend_shot;
    int8_t synth_sample[64];
} FredChannelInfo;

// Paula registers of a channel: written during a tick, handed to the voice at its end
typedef struct FredMixChannel {
    bool active;       // the channel plays (read back from the voice at the start of a tick)
    bool play_pending; // the sample restarts from its start at the end of the tick
    int8_t* sample_data;
    uint32_t data_size; // bytes readable at sample_data
    uint32_t sample_length;
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t volume; // 0-256 (NP uses SetVolume(0-256))
    uint32_t period;
} FredMixChannel;

struct FredModule {
    NuadeVoices* voices;

    // Original file data for export
    uint8_t* original_data;
    size_t original_size;
    uint16_t sub_song_num;
    uint16_t inst_num;

    uint8_t* start_tempos;
    int8_t*** positions; // [subSongNum][4][256]
    uint8_t** tracks;    // [128] each variable-sized
    int* track_sizes;
    FredInstrument* instruments;

    // has_notes[subSong * 4 + chan]
    bool* has_notes;

    int current_song;
    uint8_t current_tempo; // GlobalPlayingInfo.CurrentTempo
    FredChannelInfo channels[4];
    FredMixChannel mix_channels[4];

    uint32_t loop_count; // times a voice ran off the end of its position list
    bool end_reached[4]; // per-channel end detection

    int last_note[4];       // the note each channel last started, -1 if none
    int last_instrument[4]; // its instrument number, -1 if none
};

// True when the data parses as a Fred Editor module (clean, Final or hunk wrapped).
bool fred_probe(const uint8_t* data, size_t size);

FredModule* fred_create(const uint8_t* data, size_t size);
void fred_destroy(FredModule* module);

int fred_subsong_count(const FredModule* module);
// (Re)start a subsong from its beginning; the player drives `voices` from then on.
bool fred_start(FredModule* module, int subsong, NuadeVoices* voices);

// The tempo a subsong starts with: ticks per track line until a tempo command changes it (0 for a bad subsong).
int fred_start_tempo(const FredModule* module, int subsong);
FredVoiceState fred_voice_state(const FredModule* module, int ch);

int fred_channel_count(const FredModule* module);

// Run one player tick and set the voices. Returns true when a voice ran off the end of its position list during it.
bool fred_tick(FredModule* module);

// Edit API
int fred_get_instrument_count(const FredModule* module);
const char* fred_get_instrument_name(const FredModule* module, int inst);
float fred_get_instrument_param(const FredModule* module, int inst, const char* param);
void fred_set_instrument_param(FredModule* module, int inst, const char* param, float value);
size_t fred_export(const FredModule* module, uint8_t* out, size_t max_size);

// Replace the song data with a re-encoded module (grid edits), keeping every voice's place. 1 on success.
int fred_replace_module(FredModule* module, const uint8_t* data, size_t size);
// The period voice `ch` plays now (0 when silent).
int fred_get_channel_period(const FredModule* module, int ch);

// The player's state behind a song it opened; 0 when the song is another player's.
FredModule* fred_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
