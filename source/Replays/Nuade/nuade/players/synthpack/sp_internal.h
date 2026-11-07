// SPDX-License-Identifier: MIT
// Synth Pack player: what the module loader and the two replayers share.
//
// Karsten Obarski's Synth Pack is a four voice sequencer over synthesised instruments: an instrument is a short
// waveform (one-shot or looped) with an envelope, two LFOs, a pitch slide and a wave morph. A song ("OSP") holds the
// positions, patterns and notes; the instruments are in a separate sample set ("SMP") that several songs share.
// There are two generations of the replayer, told apart by the song file (see sp_module.c):
//  - the word layout: instruments are addressed by a table of 16-bit offsets and the notes are fixed 64 row blocks;
//  - the long layout: instruments are addressed by a table of 32-bit offsets and the notes are lists of events.
// The replayer keeps its voice records in its own data; here they are structs, and the song and the instruments are
// read from the file bytes through typed readers.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/synthpack.h>

#define SP_PERIOD_COUNT 72
#define SP_INSTRUMENTS 64 // the tables of both layouts reach this many

// "OBISYNTHPACK" starts a song, and a sample set of the long layout.
#define SP_MAGIC "OBISYNTHPACK"
#define SP_MAGIC_SIZE 12

// Positions of a song, the first one at SP_POSITIONS_OFFSET; they wrap at SP_POSITION_COUNT.
#define SP_POSITIONS_OFFSET 0x0C
#define SP_POSITION_COUNT 64
#define SP_PATTERN_LINES_OFFSET 0x10C
#define SP_PATTERN_LINE_SIZE 16
#define SP_NOTES_OFFSET 0x50C
#define SP_EVENT_SIZE 6 // of a note list and of a note block row

// The high nibble of a position's voices marks the start of a subsong.
#define SP_SUBSONG_MARK 0xF0

// The first row comes after the speed's worth of ticks, the counter starting at 1.
#define SP_DEFAULT_SPEED 6
#define SP_START_TICK 1

// An LFO delay counts down by this much a tick while its high byte is not 0; a depth of SP_LFO_OFF is no LFO.
#define SP_LFO_DELAY_UNIT 0x40
#define SP_LFO_OFF 8

// A waveform of at most this many bytes is morphed (long layout) or swept (word layout): AUDxLC moves through the
// instrument's memory, SP_MORPH_POSITIONS places, an odd one from SP_MORPH_ALTERNATE bytes on.
#define SP_MORPH_MAX_SIZE 0x80
#define SP_MORPH_POSITIONS 128
#define SP_MORPH_ALTERNATE 0x100

// Bytes of a file: a read past the end is 0 (the replayer would read whatever follows in memory; the loader refuses
// songs that do). The allocation is a nuade_paula_padded_copy, so any AUDxLC inside the file can play any length.

bool sp_memory_init(SpMemory* memory, const uint8_t* data, size_t size);
void sp_memory_free(SpMemory* memory);

// Do `length` bytes at `offset` lie inside the file? No wrap for any offset.
bool sp_memory_has(const SpMemory* memory, uint32_t offset, uint32_t length);

uint8_t sp_u8(const SpMemory* memory, uint32_t offset);
int8_t sp_s8(const SpMemory* memory, uint32_t offset);
uint16_t sp_u16(const SpMemory* memory, uint32_t offset);
uint32_t sp_u32(const SpMemory* memory, uint32_t offset);

// Tables and shifts

extern const uint16_t sp_periods[SP_PERIOD_COUNT];

// The period of a note; a note outside the table (the loader refuses songs that reach one) has none.
uint16_t sp_period(unsigned note, int8_t tuning);

// The 68000 shifts a register by the count modulo 64: counts of 16 and more empty a word.
uint16_t sp_lsr16(uint16_t value, unsigned count);
int16_t sp_asr16(int16_t value, unsigned count);

// Paula. A note starts in two ticks: the first writes AUDxLC/AUDxLEN and switches the channel's DMA off, the second
// switches it on and writes the loop part (the repeat) after it.

// The replayer's start-up: every channel runs one silent word at volume 0.
void sp_hardware_reset(SpHardware* hardware);
void sp_hardware_begin_tick(SpHardware* hardware);
void sp_hardware_end_tick(SpHardware* hardware);
void sp_hardware_sync(SpHardware* hardware, NuadeVoices* voices);

// AUDxLC of an offset into the sample set. AUDxLC ignores its lowest bit.
void sp_channel_set_lc(SpChannel* channel, const SpMemory* set, uint32_t offset);

// AUDxLC / AUDxLEN of a run of the sample set.
void sp_channel_set_sample(SpChannel* channel, const SpMemory* set, uint32_t offset, uint16_t length_words);

// AUDxVOL of a volume: the replayer keeps 7 bits (the channel applies Paula's rule to them).
uint8_t sp_paula_volume(unsigned volume);

// Envelope, the same in both layouts

typedef struct SpEnvelope {
    uint8_t attack_level;
    uint8_t attack_rate;
    uint8_t decay_rate;
    uint8_t sustain_level;
    uint8_t release_rate;
} SpEnvelope;

void sp_adsr_step(SpAdsr* adsr, const SpEnvelope* envelope, bool released);

// Forces the phase to Sustain, from any phase: with the key released the next step enters the release.
void sp_adsr_release(SpAdsr* adsr);

// A note starts: the attack from level 0.
void sp_adsr_start(SpAdsr* adsr);

// Instrument parts the layouts share

typedef struct SpLfoSettings {
    uint8_t delay;
    uint8_t speed;
    uint8_t depth; // SP_LFO_OFF: none
    uint8_t wave;
    bool after_release; // works only once the key is released
} SpLfoSettings;

// Five bytes at `at`.
SpLfoSettings sp_lfo_settings(const SpMemory* set, uint32_t at);

// The waveform of an instrument: a run from `header + data`, and a loop inside it (loop_start and loop_end 0: none).
typedef struct SpWave {
    uint32_t header; // offset of the instrument's header in the set
    uint32_t data;   // offset of the waveform from the header
    uint16_t size;   // bytes
    uint16_t loop_start;
    uint16_t loop_end;
} SpWave;

SpWave sp_wave_read(const SpMemory* set, uint32_t header, uint32_t data);

// Does the waveform get morphed or swept?
bool sp_wave_scanned(const SpWave* wave);

// AUDxLEN of the first run, and of the repeat (1 without a loop).
uint16_t sp_wave_first_length(const SpWave* wave);
uint16_t sp_wave_repeat_length(const SpWave* wave);

// Where the morph or sweep points AUDxLC at a position (0..SP_MORPH_POSITIONS - 1).
uint32_t sp_wave_scan_offset(const SpWave* wave, unsigned position);

// Do the first run and the loop lie inside the set?
bool sp_wave_in_set(const SpMemory* set, const SpWave* wave);

// Do the morph or sweep addresses of positions 0..last_position, each for `length` words, lie inside the set?
bool sp_wave_scan_in_set(const SpMemory* set, const SpWave* wave, unsigned last_position, uint16_t length);

typedef struct SpSlide {
    int16_t start;
    int16_t step;
    int16_t target;
    int16_t release_step;
} SpSlide;

// The part of a voice both layouts have

SpVoice sp_voice_new(bool released);

void sp_voice_release(SpVoice* voice);

// The envelope, LFOs and slide of a note start over, and its sample is written on the next voice step.
void sp_voice_note_on(SpVoice* voice, unsigned note, int8_t tuning, const SpSlide* slide);

// The voice's output has died away for good: released, and idle or at level 0 of the release.
bool sp_voice_silent(const SpVoice* voice);

// The pitch slide: toward 0 while the key is held, toward the instrument's target after the release.
void sp_voice_slide(SpVoice* voice, const SpSlide* slide);

// The two ticks of a note start. A voice with a pending restart writes the loop or the first run.
void sp_voice_restart(SpVoice* voice, const SpWave* wave, SpChannel* channel, const SpMemory* set);

// Song records, the same in both layouts

typedef enum SpControl {
    SpControl_Loop = 0xFC,    // jump to the loop point and call SongEnd
    SpControl_SetLoop = 0xFD, // the loop point is this position; go on
    SpControl_Stop = 0xFE,    // stop the sequencer, release the voices and call SongEnd
    SpControl_SongEnd = 0xFF, // call SongEnd and play the position again
} SpControl;

#define SP_NO_LOOP_POINT 0xFFFF

SpPosition sp_position(const SpMemory* song, unsigned index);

// A pattern line has a cell for each sequencer column.
typedef struct SpCell {
    uint8_t notes;           // the note list (longs) or block (words) the column plays
    int8_t instrument_shift; // added to the instrument numbers of the notes
    int8_t note_shift;       // added to the notes
    uint8_t tempo;           // the first column: ticks per row (the words layout keeps the low nibble)
} SpCell;

SpCell sp_cell(const SpMemory* song, unsigned pattern, unsigned column);

// The instrument number of a note event: the event's number minus 1 as a byte, plus the cell's shift; negative is 0.
unsigned sp_event_instrument(uint8_t event_instrument, int8_t instrument_shift);

// The note of an event: the note plus the instrument's note offset as a byte, plus the cell's shift (signed).
int sp_event_note(uint8_t note, int8_t instrument_note_offset, int8_t note_shift);

void sp_sequencer_start(SpSequencer* sequencer, unsigned position, unsigned loop_point);

// One tick: true when it is the tick of a row (the tick counter starts over).
bool sp_sequencer_tick(SpSequencer* sequencer);

// A row is done: the next one, or the end of the position.
void sp_sequencer_row_end(SpSequencer* sequencer, SpSongEvents* events);

// Open-time proof. The song's reads follow from its positions alone, so a walk over the (position, loop point) states
// all subsongs can reach, with one work budget, visits everything a layout must check; then the sample set's
// instruments that the events named are checked once each.

#define SP_WORK_BUDGET 20000000u

typedef struct SpCheck {
    const SpMemory* song;
    const SpMemory* set; // 0: only the song is checked
    uint32_t work_left;
    bool named[SP_INSTRUMENTS]; // instruments that events select
} SpCheck;

// Spends `amount` of the budget; false once it is used up.
bool sp_check_spend(SpCheck* check, uint32_t amount);

typedef struct SpRules {
    int max_subsongs;
    unsigned loop_point; // of a song at its start
    unsigned (*start_position)(const SpMemory* song, int subsong);
    // The reads of one position: pattern line, notes, and the events' instruments and notes.
    bool (*check_position)(SpCheck* check, const SpPosition* entry);
    // The instruments the events named (and the ones the voices start on), against the sample set.
    bool (*check_instruments)(const SpCheck* check);
} SpRules;

bool sp_validate(const SpRules* rules, const SpMemory* song, const SpMemory* set, int subsong_count);

// The note checks that need no sample set: a note's shift and arpeggio reach alone can put it outside the periods.
bool sp_check_note_reach(int8_t note_shift, unsigned arpeggio_reach);

// Module

// The eagleplayer's check, which tells the generations apart.
bool sp_song_layout(const uint8_t* data, size_t size, SpLayout* layout);

// The subsong count the eagleplayer's InitPlayer computes (at least 1: UADE clamps the range).
int sp_count_subsongs(const SpMemory* song);

// Does the sample set of the long layout start with the magic?
bool sp_set_has_magic(const SpMemory* set);
