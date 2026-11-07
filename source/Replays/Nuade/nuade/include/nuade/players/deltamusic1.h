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

// The player's tick rate: one dm1_tick per VBlank.
#define DM1_TICK_HZ 50

#define DM1_BLOCK_LINES 16
// The lowest note (period 6848) sits this many octaves below C-0.
#define DM1_NOTE_OCTAVE_SHIFT (-2)

typedef struct Dm1Module Dm1Module;

// What the display shows. A voice runs its own track (order list) of blocks of 16 lines; the line shown is the one
// played by the last tick that advanced the voice, not the one coming next.
typedef struct Dm1VoiceState {
    int note; // semitones from C-0 + DM1_NOTE_OCTAVE_SHIFT octaves (the note index through the period table), -1 if
              // none
    int instrument; // instrument number of the note, -1 if none
    int order;      // track position of the block being played
    int row;        // line of the block that was played
    int speed;      // ticks per line of the whole song
} Dm1VoiceState;

// One block line of a voice's track, as the player reads it (the track transpose is added to the note).
typedef struct Dm1Cell {
    int note; // as in Dm1VoiceState, -1 if none
    int instrument;
    int effect; // 0 = none
    int effect_arg;
} Dm1Cell;

typedef enum Dm1Effect {
    DM1_EFFECT_NONE = 0x00,
    DM1_EFFECT_SET_SPEED = 0x01,
    DM1_EFFECT_SLIDE_UP = 0x02,
    DM1_EFFECT_SLIDE_DOWN = 0x03,
    DM1_EFFECT_SET_FILTER = 0x04,
    DM1_EFFECT_SET_VIBRATO_WAIT = 0x05,
    DM1_EFFECT_SET_VIBRATO_STEP = 0x06,
    DM1_EFFECT_SET_VIBRATO_LEN = 0x07,
    DM1_EFFECT_SET_BEND_RATE = 0x08,
    DM1_EFFECT_SET_PORTAMENTO = 0x09,
    DM1_EFFECT_SET_VOLUME = 0x0A,
    DM1_EFFECT_SET_ARP1 = 0x0B,
    DM1_EFFECT_SET_ARP2 = 0x0C,
    DM1_EFFECT_SET_ARP3 = 0x0D,
    DM1_EFFECT_SET_ARP4 = 0x0E,
    DM1_EFFECT_SET_ARP5 = 0x0F,
    DM1_EFFECT_SET_ARP6 = 0x10,
    DM1_EFFECT_SET_ARP7 = 0x11,
    DM1_EFFECT_SET_ARP8 = 0x12,
    DM1_EFFECT_SET_ARP1_5 = 0x13,
    DM1_EFFECT_SET_ARP2_6 = 0x14,
    DM1_EFFECT_SET_ARP3_7 = 0x15,
    DM1_EFFECT_SET_ARP4_8 = 0x16,
    DM1_EFFECT_SET_ATTACK_STEP = 0x17,
    DM1_EFFECT_SET_ATTACK_DELAY = 0x18,
    DM1_EFFECT_SET_DECAY_STEP = 0x19,
    DM1_EFFECT_SET_DECAY_DELAY = 0x1A,
    DM1_EFFECT_SET_SUSTAIN1 = 0x1B,
    DM1_EFFECT_SET_SUSTAIN2 = 0x1C,
    DM1_EFFECT_SET_RELEASE_STEP = 0x1D,
    DM1_EFFECT_SET_RELEASE_DELAY = 0x1E
} Dm1Effect;

typedef struct Dm1Track {
    uint8_t block_number;
    int8_t transpose;
} Dm1Track;

typedef struct Dm1BlockLine {
    uint8_t instrument;
    uint8_t note;
    Dm1Effect effect;
    uint8_t effect_arg;
} Dm1BlockLine;

typedef struct Dm1Instrument {
    short number;

    uint8_t attack_step;
    uint8_t attack_delay;
    uint8_t decay_step;
    uint8_t decay_delay;
    uint16_t sustain;
    uint8_t release_step;
    uint8_t release_delay;
    uint8_t volume;
    uint8_t vibrato_wait;
    uint8_t vibrato_step;
    uint8_t vibrato_length;
    int8_t bend_rate;
    uint8_t portamento;
    bool is_sample;
    uint8_t table_delay;
    uint8_t arpeggio[8];
    uint16_t sample_length;
    uint16_t repeat_start;
    uint16_t repeat_length;
    uint8_t* table; // 48 bytes, NULL for sample instruments
    int8_t* sample_data;
    uint32_t sample_data_size;
} Dm1Instrument;

typedef struct Dm1ChannelInfo {
    Dm1Instrument* sound_data;
    uint16_t period;
    uint8_t* sound_table;
    uint8_t sound_table_counter;
    uint8_t sound_table_delay;
    Dm1Track* track;
    uint16_t track_length;
    uint16_t track_counter;
    Dm1BlockLine* block;
    uint32_t block_counter;
    uint8_t vibrato_wait;
    uint8_t vibrato_length;
    uint8_t vibrato_position;
    uint8_t vibrato_compare;
    uint16_t vibrato_frequency;
    uint8_t frequency_data;
    uint8_t actual_volume;
    uint8_t attack_delay;
    uint8_t decay_delay;
    uint16_t sustain;
    uint8_t release_delay;
    uint8_t play_speed;
    short bend_rate_frequency;
    int8_t transpose;
    uint8_t status;
    uint8_t arpeggio_counter;
    Dm1Effect effect_number;
    uint8_t effect_data;
    bool retrigger_sound;

    // Paula registers, handed to the voice at the end of the tick
    int8_t* ch_sample_data;
    uint32_t ch_data_size; // bytes behind ch_sample_data; the voice never reads past it
    uint32_t ch_sample_length;
    uint32_t ch_loop_start;
    uint32_t ch_loop_length;
    uint32_t ch_start_offset;
    uint16_t ch_volume;
    uint16_t ch_period;
    bool play_pending; // the sample (re)starts at ch_start_offset
    bool active;
} Dm1ChannelInfo;

typedef struct Dm1GlobalPlayingInfo {
    uint8_t play_speed;
    Dm1Instrument* instruments[20];
} Dm1GlobalPlayingInfo;

struct Dm1Module {
    NuadeVoices* voices; // null until dm1_start

    Dm1Track* tracks[4];
    uint16_t track_lengths[4];

    Dm1BlockLine** blocks;
    uint32_t num_blocks;

    Dm1Instrument* backup_instruments[20];

    Dm1GlobalPlayingInfo playing_info;
    Dm1ChannelInfo channels[4];

    uint32_t loop_count; // ticks in which a track passed its end
    bool end_reached[4]; // per-channel end detection
};

// True when the data is a DeltaMusic 1 module: the "ALL " mark and part lengths that fit the file.
bool dm1_probe(const uint8_t* data, size_t size);

Dm1Module* dm1_create(const uint8_t* data, size_t size);
void dm1_destroy(Dm1Module* module);

int dm1_subsong_count(const Dm1Module* module);
// (Re)start from the first position; the voices play on Paula channels 0..3.
bool dm1_start(Dm1Module* module, int subsong, NuadeVoices* voices);

// Run one player tick and hand the result to the voices. True when a track ran past its end during it.
bool dm1_tick(Dm1Module* module);

// The longest track, in positions.
int dm1_order_count(const Dm1Module* module);
Dm1VoiceState dm1_voice_state(const Dm1Module* module, int voice);
// The line `row` of the block at position `order` of the track of `voice`; empty for a position outside the track or a
// jump entry.
Dm1Cell dm1_cell(const Dm1Module* module, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
Dm1Module* dm1_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
