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

#define AVP_VOICES 4

// The player's tick rate: one avp_tick per VBlank.
#define AVP_TICK_HZ 50

typedef struct AvpModule AvpModule;

// The last note started on a voice: its Paula period (0 if none) and instrument (-1 if none).
typedef struct AvpVoiceState {
    uint16_t period;
    int instrument;
} AvpVoiceState;

typedef enum AvpPortamentoVibratoType {
    AVP_PORTAMENTO_VIBRATO_ONLY_ONE = 0,
    AVP_PORTAMENTO_VIBRATO_BOTH_TOGETHER = 1
} AvpPortamentoVibratoType;

typedef struct AvpEnvelopePoint {
    uint8_t ticks_to_wait;
    int8_t volume_increment_value;
    uint8_t times_to_repeat;
} AvpEnvelopePoint;

typedef struct AvpEnvelope {
    AvpEnvelopePoint points[6];
} AvpEnvelope;

typedef struct AvpInstrument {
    uint8_t sample_number;
    uint8_t envelope_number;
    uint8_t volume;
    uint8_t enabled_effect_flags;
    uint8_t portamento_add;
    int16_t fine_tune;
    uint8_t stop_reset_effect_delay;
    uint8_t sample_number2;
    uint16_t sample_start_offset;
    int8_t arpeggio_table[4];
    uint8_t fixed_or_transposed_note;
    int8_t transpose;
    uint8_t vibrato_number;
    uint8_t vibrato_delay;
} AvpInstrument;

typedef struct AvpSample {
    uint16_t length;
    uint16_t loop_start;
    uint16_t loop_length;
    int8_t* sample_data;
    uint32_t data_size; // bytes allocated at sample_data
} AvpSample;

typedef struct AvpSongInfo {
    uint8_t* position_lists[4];
    int position_list_lengths[4];
    int8_t speed_variation[8];
} AvpSongInfo;

typedef struct AvpVoiceInfo {
    uint8_t speed_counter;
    uint8_t speed_counter2;
    uint8_t max_speed_counter;
    uint8_t tick_counter;

    uint8_t* position_list;
    int position_list_length;
    int8_t position_list_position;
    bool position_list_loop_enabled;
    uint8_t position_list_loop_count;
    int8_t position_list_loop_start;

    uint8_t track_number;
    uint8_t track_position;
    uint8_t loop_track_counter;

    uint8_t note;
    int8_t transpose;
    int16_t fine_tune;
    uint8_t note_and_flag;
    uint16_t period;

    int8_t instrument_number;
    AvpInstrument* instrument;

    uint8_t sample_number;
    int8_t* sample_data;
    uint32_t sample_data_size;
    uint16_t sample_length;
    uint16_t sample_loop_start;
    uint16_t sample_loop_length;

    uint8_t enabled_effects_flag;
    bool stop_reset_effect;
    uint8_t stop_reset_effect_delay;

    const AvpEnvelope* envelope;
    uint8_t envelope_position;
    int8_t envelope_wait_counter;
    uint8_t envelope_loop_count;

    bool mute;
    uint16_t volume;
    uint8_t track_volume;

    uint8_t portamento_value;

    uint16_t vibrato_speed;
    uint8_t vibrato_delay;
    int8_t vibrato_depth;
    bool vibrato_direction;
    bool vibrato_count_direction;
    uint8_t vibrato_counter_max;
    uint8_t vibrato_counter;
} AvpVoiceInfo;

// Paula-style channel registers the tick writes; avp_sync_voice hands them to the voice.
typedef struct AvpChannel {
    bool active;
    bool play_pending; // a note started this tick
    int8_t* sample_data;
    uint32_t data_size;
    uint32_t sample_offset;
    uint32_t sample_length; // end of the first pass, in frames from the start of the data
    uint32_t loop_start;
    uint32_t loop_length;
    uint16_t period;
    uint16_t volume;         // 0-64
    uint16_t started_period; // the period of the last note started, 0 if none
    int started_instrument;  // its instrument, -1 if none
} AvpChannel;

struct AvpModule {
    NuadeVoices* nuade_voices;

    int subsong; // the subsong avp_start began

    bool has_ended;
    uint8_t ended_channels; // bitmask of channels that have reached end

    // Extracted from player code
    int sub_song_list_offset;
    int position_lists_offset;
    int track_offsets_offset;
    int tracks_offset;
    int envelopes_offset;
    int instruments_offset;
    int sample_info_offset;
    int sample_start_offsets_offset;
    int sample_data_offset;
    int speed_variation_speed_increment_offset;

    int instrument_format_version;
    int parse_track_version;
    int speed_variation_version;
    uint8_t speed_variation_speed_init;
    AvpPortamentoVibratoType portamento_vibrato_type;
    int vibrato_version;
    bool have_separate_sample_info;
    bool have_set_note;
    bool have_set_fixed_sample;
    bool have_set_arpeggio;
    bool have_set_sample;
    bool have_arpeggio;
    bool have_envelope;
    bool reset_volume;

    AvpSongInfo* song_info_list;
    int num_song_infos;

    uint8_t** tracks; // a track the module does not have is null
    int* track_lengths;
    int num_tracks;

    AvpEnvelope* envelopes;
    int num_envelopes;

    AvpInstrument* instruments;
    int num_instruments;

    AvpSample* samples;
    int num_samples;

    AvpSongInfo* current_song_info;

    // Playing state
    int8_t speed_variation_counter;
    int8_t speed_index;
    uint8_t speed_variation2_counter;
    uint8_t speed_variation2_speed;

    int8_t master_volume_fade_counter;
    int8_t master_volume_fade_speed;
    uint16_t master_volume;

    int8_t global_transpose;

    AvpVoiceInfo voices[AVP_VOICES];
    AvpChannel channels[AVP_VOICES];
};

// True when the data is an Activision Pro (Martin Walker) module: its embedded player code is recognised and the
// module tables load.
bool avp_probe(const uint8_t* data, size_t size);

// `data` must stay valid for the lifetime of the module.
AvpModule* avp_create(const uint8_t* data, size_t size);
void avp_destroy(AvpModule* module);

int avp_subsong_count(const AvpModule* module);
// (Re)start a subsong from its beginning. The voices must outlive the module's playing.
bool avp_start(AvpModule* module, int subsong, NuadeVoices* voices);

// Run one player tick and hand the channel registers to the voices. False once every channel reached the end of the
// song (it does not loop).
bool avp_tick(AvpModule* module);

// Ticks one track step lasts at the song's start speed (0 if the song has none); a note lasts a whole number of steps.
uint32_t avp_ticks_per_step(const AvpModule* module, int subsong);

AvpVoiceState avp_voice_state(const AvpModule* module, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
AvpModule* avp_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
