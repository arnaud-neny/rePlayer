// SPDX-License-Identifier: MIT
// The Musical Enlightenment player (N.J. Luuring jr, 1989-90), translated from the UADE eagleplayer "The Musical
// Enlightenment 2.0 player module" (Wanted Team).

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

#define TME_VOICES 4
#define TME_ROWS 32
#define TME_INSTRUMENTS 32

// The CIA clock: the timebase of TME_TICK_LENGTH.
#define TME_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / TME_TIMEBASE_HZ seconds.
#define TME_TICK_LENGTH 14187

#define TME_MAX_SONGS 16
#define TME_LIST_COUNT 256
#define TME_ENVELOPE_PHASES 3

typedef struct TmePlayer TmePlayer;

typedef struct TmeTickResult {
    bool looped; // the last position of the song was loaded during this tick (where the eagleplayer calls SongEnd)
} TmeTickResult;

typedef struct TmePosition {
    int order; // position being played
    int row;
    int speed; // ticks per row
} TmePosition;

typedef struct TmeVoiceState {
    int note;       // period table index of the last note started, -1 if none
    int instrument; // 0-based instrument of the voice, -1 if none
} TmeVoiceState;

// One pattern step as stored (no transposes applied).
typedef struct TmeCell {
    uint8_t note; // 1 .. 0x7D period table index + 1, 0 none, 0x7E release, 0x7F stop; bit 7 keeps the playing note
    uint8_t instrument; // 1 .. 32, 0 keeps the voice's
    uint8_t list;       // command list, 0 the instrument's own
    int8_t volume;      // added to the instrument's volume
    uint8_t flags;
    uint8_t param;
} TmeCell;

// Where a voice's sample registers stand.
typedef enum TmeDmaState {
    TmeDmaState_Idle,   // the loop registers are written
    TmeDmaState_Repeat, // the sample registers were written, the loop registers follow
    TmeDmaState_Start,  // a note started: the sample registers are due
} TmeDmaState;

typedef struct TmeInstrument {
    bool has_sample;
    uint32_t sample_size;
    uint32_t sample_offset; // in the module
    uint16_t length_words;
    uint32_t loop_start_words;
    uint16_t loop_length_words;
    int8_t volume;
    uint8_t env_time[TME_ENVELOPE_PHASES];  // ticks - 1 to reach the level
    uint8_t env_level[TME_ENVELOPE_PHASES]; // 0 .. 255
    uint32_t list_offset;                   // the instrument's own command list
} TmeInstrument;

typedef struct TmeSong {
    uint8_t start;
    uint8_t end; // exclusive: the position list wraps when it has loaded the one before
    uint8_t speed;
    bool voice_enabled[TME_VOICES]; // the low bits of the song's mask; bit 4 (LED filter) is not modelled
} TmeSong;

typedef struct TmeModule {
    uint8_t* image; // the file, then zeros
    uint32_t size;
    uint32_t position_count;
    uint32_t steps_offset;
    uint32_t list_offset[TME_LIST_COUNT];
    TmeInstrument instruments[TME_INSTRUMENTS];
    TmeSong songs[TME_MAX_SONGS];
    int song_count;
} TmeModule;

// One voice's entry of a position.
typedef struct TmeTrack {
    uint8_t track;
    uint8_t instrument_transpose;
    uint8_t note_transpose;
} TmeTrack;

// Two of these modulate a voice: a wave table, its depth and rate, each ramping to a target.
typedef struct TmeLfo {
    uint16_t base;
    uint16_t out;
    uint8_t depth;     // target
    uint8_t depth_now; // ramping to depth
    uint8_t rate;
    uint8_t rate_now;
    uint8_t phase; // 0 .. 127
    uint16_t depth_ramp;
    uint16_t rate_ramp;
    uint32_t wave_offset; // 0 until a command selects one
} TmeLfo;

// The sample registers the replayer keeps per voice before it copies them to Paula.
typedef struct TmeRegisters {
    uint64_t lc; // module offset
    bool lc_set;
    uint16_t len_words;
    uint16_t period;
    uint16_t volume;
} TmeRegisters;

typedef struct TmeVoice {
    TmeLfo pitch;
    TmeLfo volume;
    int instrument; // -1 none
    uint32_t step_offset;
    TmeTrack track;
    uint32_t list; // module offset of the next command
    bool list_active;
    uint16_t list_delay;
    uint16_t env_phase;
    uint32_t env_ticks;
    uint16_t env_level;
    uint16_t env_target;
    int16_t volume_scale;
    TmeDmaState dma_state;
    bool start_dma; // DMACON is set at the start of the next tick
    int note;       // period table index of the last note, -1 if none
    int16_t slide;
    uint32_t porta_ticks;
    uint16_t porta_target;
    bool porta_skip; // the next row is the portamento target, not a step to play
    uint8_t arp_left;
    uint8_t arp_once;
    bool arp_restart;
    uint8_t arp_counter;
    uint8_t arp_speed;
    uint8_t arp_length;
    uint16_t arp_index;
    uint32_t arp_offset; // the 8 note offsets in the module
    TmeRegisters regs;
    NuadePaulaChannel paula;
} TmeVoice;

struct TmePlayer {
    TmeModule module;
    const TmeSong* song;
    TmeVoice voices[TME_VOICES];
    uint32_t position; // the next position to load
    uint32_t shown_order;
    uint32_t shown_row;
    uint32_t rows_left;   // rows before the next position is loaded, plus one
    uint8_t tick_counter; // ticks before the next row
    bool looped;
    bool unmodelled; // the replayer read something the format does not define, or the dry run ran out of work
    uint64_t budget; // work the dry run may still do: ticks and list commands
};

// True when the data is a module this player plays exactly: the eagleplayer's check, a layout that fits the file and
// a dry run of every subsong, until its state repeats, that meets nothing the player does not model.
bool tme_probe(const uint8_t* data, size_t size);

// Null when tme_probe would refuse the data.
TmePlayer* tme_create(const uint8_t* data, size_t size);
void tme_destroy(TmePlayer* player);

int tme_subsong_count(const TmePlayer* player);

// (Re)start a subsong from its first position (InitSound). tme_tick drives `voices` (Paula channel n = voice n).
bool tme_start(TmePlayer* player, uint32_t subsong, NuadeVoices* voices);

TmeTickResult tme_tick(TmePlayer* player, NuadeVoices* voices);

// The row decoded by the last row tick (the first position, row 0 before the first one).
TmePosition tme_position(const TmePlayer* player);
int tme_order_count(const TmePlayer* player);

TmeVoiceState tme_voice_state(const TmePlayer* player, int voice);

// An order, voice or row out of range reads as an empty cell.
TmeCell tme_pattern_cell(const TmePlayer* player, uint32_t order, uint32_t voice, uint32_t row);

// The player's state behind a song it opened; 0 when the song is another player's.
TmePlayer* tme_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
