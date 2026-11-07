// SPDX-License-Identifier: MIT
// Sound Master player (Michiel J. Soede's music driver, 1991-94: .sm, .sm1-.sm3, .smpro), translated from the UADE
// eagleplayer "SoundMaster". A module is the replayer and the song in one file: a jump table, the 68k code, its
// variables and the song's tables. The eagleplayer locates the init and play entries, the position where the song
// wraps and, for some versions, a subsong table. The player recognises the seven replayer versions it knows by their
// code and runs that version's replayer on the module's tables; any other code is refused, so UADE plays it.

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

#define SMST_VOICES 4

// The CIA clock: the timebase of SMST_TICK_LENGTH.
#define SMST_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / SMST_TIMEBASE_HZ seconds.
#define SMST_TICK_LENGTH 14187

#define SMST_MAX_WRITES 3

typedef struct SmstPlayer SmstPlayer;

typedef struct SmstTickResult {
    bool looped; // the song wrapped to its start position: the eagleplayer calls SongEnd
} SmstTickResult;

typedef struct SmstVoiceState {
    int note;       // the note the voice last started, -1 if none yet
    int instrument; // its instrument number, -1 if none yet
} SmstVoiceState;

// How the song's tables are laid out. Offsets: a header of table offsets before the data (later versions). Fixed: the
// tables sit at fixed distances from the variables.
typedef enum SmstLayout {
    SmstLayout_Offsets,
    SmstLayout_Fixed,
} SmstLayout;

// What init does with the subsong number.
typedef enum SmstInit {
    SmstInit_NoSubsongs, // one song
    SmstInit_Window,     // two subsongs, each a range of positions the init writes into the header
    SmstInit_Table,      // a table of start position and range, before the data
    SmstInit_Mode,       // the init takes a mode byte; the eagleplayer always passes the song mode, one song
} SmstInit;

// The order of the DMA writes of one tick.
typedef enum SmstDmaFlow {
    SmstDmaFlow_RowThenNote, // DMA off of the last tick's wave switches first, then the new notes' before the voices
    SmstDmaFlow_Combined,    // the voices' DMA off in one step, after the rows are read
} SmstDmaFlow;

// Which byte decides that a new note starts in the loop part of its sample.
typedef enum SmstLoopStart {
    SmstLoopStart_Never,
    SmstLoopStart_Instrument,   // bit 7 of the instrument's wave byte
    SmstLoopStart_SelectedWave, // bit 7 of the wave value the note selected (the wave table's, with a wave sequence)
} SmstLoopStart;

// Byte range relative to the start of the variable block.
typedef struct SmstRange {
    int32_t from;
    int32_t to;
} SmstRange;

typedef struct SmstVersion {
    uint32_t vars;      // the variable block, as an offset in the module
    uint32_t code_size; // bytes before the variables: the signature covers them
    uint64_t code_hash; // FNV-1a of those bytes
    SmstLayout layout;
    SmstInit init;
    SmstDmaFlow dma_flow;
    SmstLoopStart loop_start;
    uint32_t voice_size;        // bytes of one voice record
    uint32_t instrument_size;   // bytes of one instrument record
    uint32_t speed_at;          // vars + this: the header (speed, pattern length, first and last position, filter)
    uint32_t data_at;           // vars + this: the first position (Offsets layout; the Fixed layout derives it)
    uint32_t dummy_at;          // vars + this: the silent word a voice without a loop plays
    uint32_t period_at;         // the period table in the code
    uint32_t sample_index_mask; // Offsets layout: instrument sample number bits that select a sample slot
    SmstRange writes[SMST_MAX_WRITES]; // the bytes of the module memory the replayer writes (its variables)
    uint32_t builtin_at; // Offsets layout, versions with built-in samples: slot table + this: the slots the init fills
    uint32_t builtin_count;
    int32_t builtin_template;      // vars + this: the template the init copies
    int32_t builtin_origin;        // the constant its offsets are relocated by
    bool voice_fade;               // each voice has its own fade volume (else one global)
    bool row_commands;             // row bytes $FD (speed) and $FE (end of pattern) are commands
    bool dummy_voices;             // the init starts every voice on the silent word
    bool sweep;                    // instrument bytes $E and $F: the loop start moves
    bool release;                  // instrument byte 8: the vibrato also moves the volume
    bool arpeggio_hold;            // arpeggio value $82 keeps the pitch and stops the sweep
    bool wave_skip_bit5;           // a wave table value with bit 5 does not change the loop
    bool wave_repeat_check;        // a wave table value equal to the one before changes nothing
    bool period_sets_last_note;    // the period routine stores the note, not its caller
    bool period_wide;              // 64 periods, no octave offset (the first version)
    bool rest_after_release;       // a release note sets the envelope from the info byte, like a rest
    bool release_picks_instrument; // a release note selects the instrument (and remembers it)
    bool vibrato_limit;            // the vibrato depth stops growing above $40
} SmstVersion;

typedef struct SmstTables {
    uint32_t positions;
    uint32_t blocks;
    uint32_t instruments;
    uint32_t arpeggio;
    uint32_t wave;
    uint32_t patterns;
    uint32_t samples;     // Offsets layout: the slot table; Fixed: the pointer column
    uint32_t sample_bias; // Offsets layout: added to a slot's offset; Fixed: the sample data
    uint32_t dummy;
} SmstTables;

typedef struct SmstModule {
    const SmstVersion* version;
    uint8_t* memory; // the image, then zeros: nuade_paula_padded_copy
    uint32_t size;   // bytes of the module
    uint32_t limit;  // size of the image
    SmstTables tables;
    uint8_t speed; // the header
    uint8_t pattern_length;
    uint8_t first_position;
    uint8_t last_position;
    uint32_t voice_column[SMST_VOICES]; // the byte offset of the voice's column in a block
} SmstModule;

typedef struct SmstVoice {
    uint32_t column; // module constant: the voice's byte offset in a block record
    // DMA: the voice's channel is restarted at the end of the tick / switched off at the start of the next
    bool dma_restart;
    bool dma_off;      // off at the start of this tick (set by a wave switch of the last one)
    bool dma_off_next; // set this tick
    // the sequencer
    uint32_t pattern; // address of the pattern the voice plays
    uint8_t note;     // the row's note byte
    uint8_t info;     // the row's second byte
    uint8_t pattern_transpose;
    uint8_t position_instrument; // added to the instrument number: the position's byte for this voice
    uint8_t fade;                // volume factor, 64 = full
    // the note
    uint8_t instrument;
    uint8_t last_note;
    uint16_t period; // four times the Paula period
    uint16_t slide_target;
    int16_t slide_step;
    uint16_t loop_length; // words; 0: a silent word
    uint32_t loop_start;
    // the volume envelope: a 12-bit volume in the high bits of a word
    uint16_t env;
    uint16_t env_attack; // ENVELOPE_DONE once the peak was reached
    uint16_t env_peak;
    uint16_t env_decay;
    uint16_t env_sustain;
    // vibrato
    uint8_t vib_delay;
    uint8_t vib_depth;
    uint8_t vib_step;
    uint8_t vib_period;
    uint8_t vib_count;
    int8_t vib_dir; // 0 off, 1 down, -1 up
    uint8_t release;
    // arpeggio / wave sequence
    uint8_t arp_pos;
    uint8_t arp_length;
    uint8_t arp_repeat;
    uint8_t arp_note;
    uint8_t note_offset;
    // sweep
    int16_t sweep_step;
    uint8_t sweep_period;
    uint8_t sweep_count;
    uint8_t sweep_mask;
    // shown
    int shown_note;
    int shown_instrument;
    NuadePaulaChannel paula;
} SmstVoice;

struct SmstPlayer {
    SmstModule module;
    uint8_t* original; // the module as loaded: a restart rebuilds the image from it
    int subsongs;
    // the position in the song
    uint8_t position;
    uint8_t block;
    uint8_t row; // twice the row number: the pattern holds two bytes a row
    uint8_t tick;
    uint8_t speed;
    uint8_t block_first;
    uint8_t block_last;
    uint8_t position_transpose;
    uint8_t start_position;
    uint8_t end_position;
    bool end_of_pattern;
    // the fade
    uint8_t fade_count;
    uint8_t fade_control;
    uint8_t fade; // Fixed layout: one fade for all voices
    // The arpeggio step a sweep sees when no voice ran an arpeggio before it in the tick: the interrupt leaves the
    // program it interrupted unchanged, and that program holds what the init's position load left.
    uint8_t idle_step;
    bool looped;
    SmstVoice voices[SMST_VOICES];
};

// The eagleplayer's Check2 plus what this port needs: a replayer whose code it knows, and tables that the replayer's
// own writes cannot reach.
bool smst_probe(const uint8_t* data, size_t size);

// Copies the module and locates its tables; null when smst_probe rejects it.
SmstPlayer* smst_create(const uint8_t* data, size_t size);
void smst_destroy(SmstPlayer* player);

int smst_subsong_count(const SmstPlayer* player);

// (Re)start a subsong (0-based) from its beginning; false when the subsong does not exist.
bool smst_start(SmstPlayer* player, int subsong);

// One interrupt; the player drives `voices` (Paula channel n = voice n).
SmstTickResult smst_tick(SmstPlayer* player, NuadeVoices* voices);

// Ticks per row at the start of the song (a row command can change it later).
int smst_ticks_per_row(const SmstPlayer* player);

SmstVoiceState smst_voice_state(const SmstPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
SmstPlayer* smst_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
