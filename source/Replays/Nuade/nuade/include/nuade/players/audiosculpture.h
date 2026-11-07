// SPDX-License-Identifier: MIT
// Audio Sculpture player, translated from the UADE eagleplayer "AudioSculpture / Startrekker 4AM player" (ASREPLAY by
// The Synchron Assembly / Expose Software). A song is a four channel ProTracker style module ("FLT4" or "EXO4") plus
// the ".AS" (or ".NT") file next to it, which holds the amplitude modulation instruments: a small synthesizer with an
// envelope, a vibrato and four wave forms. The module's instruments with length 0 and an "AM" entry in that file play
// as synth voices; the others are plain samples.

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

#define ASC_VOICES 4

// Instruments of the module (numbered 1..31), and the sizes of the strings asc_title and asc_sample_name write.
#define ASC_INSTRUMENTS 31
#define ASC_TITLE_SIZE 21
#define ASC_NAME_SIZE 23

// The CIA clock: the timebase of ASC_TICK_LENGTH.
#define ASC_TIMEBASE_HZ 709379
// One player tick (the eagleplayer sets the CIA timer to 14187), in 1 / ASC_TIMEBASE_HZ seconds.
#define ASC_TICK_LENGTH 14187

#define ASC_PATTERN_ROWS 64

#define ASC_MOD_ORDER_COUNT 128
// The restart position is a byte: past the 128 orders the original reads the tag and patterns
#define ASC_ORDER_READ_COUNT 256
// The wave forms: sine, saw, square and noise, 32 bytes each. The wave form number is in the instrument.
#define ASC_WAVE_SIZE 32
#define ASC_WAVE_COUNT 4

typedef struct AscPlayer AscPlayer;

typedef struct AscTickResult {
    bool looped; // the row just played was played before: the eagleplayer's SongEnd
} AscTickResult;

typedef struct AscPosition {
    uint32_t order;
    uint32_t row;
    uint32_t speed; // ticks per row
} AscPosition;

typedef struct AscVoiceState {
    uint16_t period;    // of the last note started on the voice, 0 if none
    uint8_t instrument; // its instrument number (1..31), 0 if none
} AscVoiceState;

typedef struct AscCell {
    uint16_t period;
    uint8_t instrument;
    uint8_t effect;
    uint8_t effect_arg;
} AscCell;

// A file image. A read past its end gives 0: the original reads whatever follows the file in memory.
typedef struct AscBlob {
    const uint8_t* data;
    size_t size;
} AscBlob;

// One pattern cell; the instrument number is split over the first and third byte.
typedef struct AscRawCell {
    uint16_t period;
    uint8_t instrument;
    uint8_t effect;
    uint8_t param;
} AscRawCell;

// The header of instrument n in the module (30 bytes). The finetune byte is not decoded: InitSound clears it in every
// header, so the replayer never sees one.
typedef struct AscSampleHeader {
    char name[ASC_NAME_SIZE];
    uint16_t length_words;
    uint8_t volume;
    uint16_t loop_start_words;
    uint16_t loop_length_words;
} AscSampleHeader;

typedef struct AscAmStage {
    int16_t level; // the envelope slides to this level ...
    int16_t rate;  // ... this many units each tick
} AscAmStage;

// The AM instrument n of the bank; `present` is false when the entry does not start with "AM".
typedef struct AscAmInstrument {
    bool present;
    uint16_t start_level; // the envelope starts here (256 = full volume)
    AscAmStage stages[3]; // attack, decay, sustain
    int16_t hold_ticks;   // the sustain level is kept until this count, counted down once a tick, is below 0
    int16_t release_rate;
    uint16_t wave;          // wave form number
    int16_t period_slide;   // added to the period every tick
    uint16_t vibrato_depth; // 0 = no vibrato
    uint16_t vibrato_speed; // steps (of 2 bytes) of the vibrato table each tick
    uint16_t octave_shift;  // shifts the note's period left by this much
} AscAmInstrument;

typedef enum AscAmPhase {
    AscAmPhase_Off = 0,
    AscAmPhase_Attack,
    AscAmPhase_Decay,
    AscAmPhase_Sustain,
    AscAmPhase_Hold,
    AscAmPhase_Release,
} AscAmPhase;

// Where Paula's AUDxLC points. The two memories are the module (the sample data) and the wave forms.
typedef enum AscMemory {
    AscMemory_None,
    AscMemory_Module,
    AscMemory_Waves,
} AscMemory;

typedef struct AscAddress {
    AscMemory memory;
    uint32_t offset;
} AscAddress;

// One channel's replayer state.
typedef struct AscVoice {
    NuadePaulaChannel paula;
    AscRawCell cell;

    AscAddress sample;     // what Paula plays when a note starts
    uint16_t length_words; // 0: the instrument has no sample, a note stops the channel
    AscAddress loop;       // what Paula repeats
    uint16_t loop_words;

    uint16_t period;
    uint8_t volume;

    bool porta_period_falls;
    uint8_t porta_speed;
    uint16_t porta_target; // 0 = no tone portamento running
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    uint8_t vibrato_pos;

    uint8_t instrument; // number of the last instrument assigned
    bool am;            // the instrument is a synth voice
    AscAmPhase am_phase;
    int16_t am_level;
    int16_t am_hold;
    uint16_t am_vibrato_pos; // byte offset over the whole wave of the vibrato table
    uint16_t base_period;    // the period the AM vibrato is added to

    bool dma_started; // this voice's DMA was set by the last row
    uint16_t started_period;
    uint8_t started_instrument;
} AscVoice;

struct AscPlayer {
    uint8_t* module_copy; // the module, with the first bytes of each sample cleared, and NUADE_PAULA_MAX_SAMPLE_BYTES
                          // zero bytes behind it (nuade_paula_padded_copy)
    AscBlob module;       // patterns and sample data; the header is decoded below

    char title[ASC_TITLE_SIZE];
    AscSampleHeader samples[ASC_INSTRUMENTS];
    uint32_t sample_offsets[ASC_INSTRUMENTS]; // of the sample data in the module
    uint8_t orders[ASC_ORDER_READ_COUNT];
    uint8_t song_length;
    uint8_t restart;
    AscAmInstrument am[ASC_INSTRUMENTS + 1]; // from the bank, by instrument number

    uint8_t subsong_starts[ASC_MOD_ORDER_COUNT + 1];
    uint32_t subsong_count;

    AscVoice voices[ASC_VOICES];
    uint8_t beam; // stands in for the beam position the noise is made from
    int8_t waves[ASC_WAVE_COUNT * ASC_WAVE_SIZE];

    uint8_t speed;
    uint8_t counter;
    uint16_t row_offset; // of the row in the pattern, 0 .. 0x3F0
    uint8_t order;
    bool break_pending;
    bool visited[ASC_ORDER_READ_COUNT][ASC_PATTERN_ROWS];

    AscPosition shown;
};

// The eagleplayer's Check2: a module with the tag "EXO4" or "FLT4" and an instrument with length 0 and a volume.
bool asc_probe(const uint8_t* data, size_t size);

// `module` and `bank` (the ".AS" file) are copied. Null if the module is not valid or the bank is empty.
AscPlayer* asc_create(const uint8_t* module, size_t module_size, const uint8_t* bank, size_t bank_size);
void asc_destroy(AscPlayer* player);

// Subsongs are the sections of the order list that end in a position jump (the eagleplayer's SubSongRange).
uint32_t asc_subsong_count(const AscPlayer* player);

// (Re)start a subsong (0-based) from its first position; the player drives `voices` (Paula channel n = voice n).
bool asc_start(AscPlayer* player, uint32_t subsong, NuadeVoices* voices);

AscTickResult asc_tick(AscPlayer* player, NuadeVoices* voices);

// Module name, "" if none.
void asc_title(const AscPlayer* player, char out[ASC_TITLE_SIZE]);
// Instrument name for index 0..30, "" if out of range.
void asc_sample_name(const AscPlayer* player, int index, char out[ASC_NAME_SIZE]);

// The row whose notes were started by the last tick that played a row.
AscPosition asc_position(const AscPlayer* player);
// Entries of the order list.
uint32_t asc_position_count(const AscPlayer* player);

AscVoiceState asc_voice_state(const AscPlayer* player, int voice);

// `order` 0 .. asc_position_count - 1, `channel` 0..3, `row` 0..63.
AscCell asc_pattern_cell(const AscPlayer* player, uint32_t order, uint32_t channel, uint32_t row);

// The player's state behind a song it opened; 0 when the song is another player's.
AscPlayer* asc_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
