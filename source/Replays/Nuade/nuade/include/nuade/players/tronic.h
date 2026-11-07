// SPDX-License-Identifier: MIT
// Tronic player (TronicTracker)

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRONIC_VOICES 4

// The CIA clock: the timebase of TRONIC_TICK_LENGTH.
#define TRONIC_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA rate), in 1 / TRONIC_TIMEBASE_HZ seconds.
#define TRONIC_TICK_LENGTH 14187

// Rows in a pattern.
#define TRONIC_ROWS 16

typedef struct TronicPlayer TronicPlayer;

typedef struct TronicTickResult {
    bool looped; // a voice's sequence passed its end during this tick (the eagleplayer's SongEnd)
} TronicTickResult;

typedef struct TronicPosition {
    int order; // sequence step of voice 0 (step 0 = the first pattern of every voice)
    int row;   // 0 .. TRONIC_ROWS - 1
    int speed; // ticks per row
} TronicPosition;

typedef struct TronicVoiceState {
    int period;     // period-table value of the last note started on the voice (transpose added), -1 if none
    int instrument; // its instrument, 0-based, -1 if none
} TronicVoiceState;

// A pattern entry, the bytes as stored.
typedef struct TronicCell {
    uint8_t note;       // period-table index, 0 = none
    uint8_t instrument; // 1-based
    uint8_t effect;     // 0 = none
    uint8_t effect_arg;
} TronicCell;

// One voice's step list: pairs of (pattern number, transpose). The song restarts at `restart` after `length` bytes.
typedef struct TronicSequence {
    uint32_t at;
    uint16_t restart;
    uint16_t length;
} TronicSequence;

typedef struct TronicModule {
    uint8_t* data; // the file, writable (the replayer clears a word of a sample) and followed by zeros for Paula
    size_t size;
    // Set by a read or a sample pointer outside the file; the open-time check proves that the song never does.
    bool overrun;

    TronicSequence sequence[TRONIC_VOICES];
    uint32_t wave_tables;      // 16 bytes each: note offsets added to the note, 0x80 restarts the table
    uint32_t instrument_table; // one word per instrument: its record's offset from `instruments`
    uint32_t instrument_count;
    uint32_t instruments;
    uint32_t waveform_index; // one word per waveform step: offset of the waveform from `waveforms`
    uint32_t waveforms;
    uint32_t patterns;
    uint32_t pattern_bytes;
    uint32_t sample_table; // TRONIC_SAMPLE_COUNT longs: sample offsets from `samples`
    uint32_t samples;
} TronicModule;

// An instrument record (88 bytes). The replayer indexes the envelope, vibrato and waveform step lists past the fixed
// layout, so those are read with tronic_instrument_byte.
typedef struct TronicInstrument {
    uint32_t at;
    uint16_t length_words; // AUDxLEN of the sample or waveform
    uint16_t loop_offset;  // sample: the repeat starts this many bytes after the sample start
    uint16_t loop_words;   // sample: the repeat's AUDxLEN
    uint16_t detune;       // subtracted from the voice's pitch slide every tick
    bool is_sample;        // else a list of waveforms
    uint8_t param;         // sample: the sample number; waveform list: ticks per step
} TronicInstrument;

typedef struct TronicVoice {
    int index;             // the voice number
    uint16_t sequence_pos; // byte offset of the next step
    uint16_t row_offset;   // byte offset of the next row in the pattern; 0 fetches the next step
    uint32_t pattern;      // the current step's pattern
    uint8_t transpose;     // the current step's
    uint8_t note;          // last note started (period-table index before the transpose)
    uint16_t target_period;
    uint16_t period;       // the voice's period before the vibrato; moves towards target_period while sliding
    uint8_t slide_to_note; // effect 5: period change per tick towards the note; 0 = off
    uint8_t max_volume;    // effect 6

    bool has_instrument; // false until the first note
    int instrument_number;
    TronicInstrument instrument;
    bool sample_repeat_pending; // the tick after a sample starts, AUDxLC/AUDxLEN are set to its repeat

    uint8_t waveform_ticks; // ticks to the next waveform of the list
    uint8_t waveform_pos;
    int16_t volume; // envelope level, 0..255 plays as 0..63
    uint16_t envelope_pos;
    uint8_t envelope_hold;
    bool vibrato_down;
    uint16_t vibrato;
    uint8_t vibrato_ticks;
    uint8_t vibrato_stage;
    uint8_t vibrato_hold;
    uint32_t wave_table;
    uint16_t wave_table_pos;

    NuadePaulaChannel paula;

    int shown_order; // the display's view of the last row read
    int shown_row;
} TronicVoice;

typedef struct TronicSong {
    uint8_t speed;      // ticks per row minus one
    int8_t counter;     // counts down to the next row tick
    uint8_t max_volume; // effect 7
    bool looped;        // a voice passed the end of its sequence during this tick
    TronicVoice voices[TRONIC_VOICES];
} TronicSong;

struct TronicPlayer {
    TronicModule module;
    TronicSong song;
};

// True when the data is a module the player models completely: the layout the eagleplayer's check accepts, and every
// pattern, instrument, sample and table the song can reach read inside the file.
bool tronic_probe(const uint8_t* data, size_t size);

TronicPlayer* tronic_create(const uint8_t* data, size_t size);
void tronic_destroy(TronicPlayer* player);

// (Re)start from the first sequence step; the player drives `voices` (Paula channel n = voice n).
bool tronic_start(TronicPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
TronicTickResult tronic_tick(TronicPlayer* player, NuadeVoices* voices);

// The row whose entry was read by the last row tick (step 0, row 0 before the first one).
TronicPosition tronic_position(const TronicPlayer* player);
int tronic_order_count(const TronicPlayer* player);

TronicVoiceState tronic_voice_state(const TronicPlayer* player, int voice);

// `order` 0 .. tronic_order_count - 1, `voice` 0..3, `row` 0..15.
TronicCell tronic_pattern_cell(const TronicPlayer* player, int order, int voice, int row);

// The player's state behind a song it opened; 0 when the song is another player's.
TronicPlayer* tronic_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
