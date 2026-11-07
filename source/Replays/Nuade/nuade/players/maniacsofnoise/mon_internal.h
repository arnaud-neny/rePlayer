// SPDX-License-Identifier: MIT
// Maniacs of Noise player: types shared by the module scan (mon_module.c, mon_walk.c), the song level
// (maniacsofnoise.c), the voice code all versions share (mon_voice.c, mon_commands.c) and the two voice replayers
// (mon_old.c, mon_new.c).
//
// A module is a memory image: three entry jumps, the replayer's code, a data block (jump tables, replayer variables,
// voice records, period table) and the song data. The code addresses the data block through one base register and the
// song data through another; the player keeps the replayer's variables and voice records in structs and reads the rest
// of the image, which no routine writes, through the readers below.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/maniacsofnoise.h>

// A read outside the image returns 0 (the original would read whatever is in memory there).
#define MON_NO_ADDRESS UINT32_MAX

// The bytes of an instrument record.
#define MON_INSTRUMENT_BYTES 64

// The loop start of an instrument that plays its sample once.
#define MON_NO_LOOP 0xFFFF

// The address of the byte whose bits enable the voices in MonVersion_DragonWars.
#define MON_VOICE_MASK_ADDRESS 0xC

// A list byte: jump back by the byte that follows (the older versions: to that position).
#define MON_LIST_JUMP 0x80

// Iteration limits of one voice's tick: commands read from a pattern or sequence, and steps of one list lookup.
#define MON_COMMAND_GUARD 4096
#define MON_LIST_GUARD 1024

// The five versions of the replayer, in the order the formats grew: the game of the first module known with it.

uint8_t mon_u8(const MonModule* m, uint32_t address);
uint16_t mon_u16(const MonModule* m, uint32_t address);
uint32_t mon_u32(const MonModule* m, uint32_t address);

// The Check2 of the eagleplayer, the signature test and the walk of every song (mon_walk.c); false when the image is
// not a module of a known version or a song reaches a command the replayer cannot run. `bytes` is kept in `out`.
bool mon_locate(const uint8_t* bytes, size_t size, MonModule* out);

// Walks every voice of every song (mon_walk.c); false when one reaches a command behind the command tables.
bool mon_walk_songs(const MonModule* m);

// A word of the song data used as a signed offset from the song data (adda.w).
uint32_t mon_song_address(const MonModule* m, uint16_t offset);

// The chip memory address of a sample: null when it is outside the image. AUDxLC ignores bit 0.
const int8_t* mon_sample(const MonModule* m, uint32_t address);

// The period table entry of a note (byte offset note * 2 from the table).
uint16_t mon_period_entry(const MonModule* m, unsigned note);

// One song record
typedef struct MonSong {
    uint16_t tracks[MON_VOICES]; // offsets of the voices' sequences from the song data, 0: the voice is off
    uint8_t master_volume;       // the fade target
    uint8_t tempo;
} MonSong;

bool mon_read_song(const MonModule* m, uint32_t number, MonSong* out);

// One instrument record
typedef struct MonInstrument {
    uint8_t kind;
    uint8_t wave_ticks;    // the older versions: ticks each wave of a wave list plays
    bool restart_waves;    // MonVersion_MysteryWorldsEnd: a note-on restarts the wave list
    uint16_t tuning;       // multiplies the period table
    uint32_t sample;       // offset from the song data
    uint16_t wave_list;    // the wave list of the instruments that play one: offset from the song data
    uint16_t length_words; // of the sample, or of the wave of a synthetic sound
    uint16_t loop_start;   // MON_NO_LOOP: the sample plays once
    uint16_t envelope;     // offsets from the song data of the lists below, 0: none
    uint8_t volume;
    uint8_t envelope_speed;
    uint16_t vibrato;
    uint8_t vibrato_delay;
    uint8_t vibrato_speed;
    uint16_t arpeggio;
    uint8_t arpeggio_speed;
} MonInstrument;

MonInstrument mon_read_instrument(const MonModule* m, uint32_t address);

// The wave table of MonVersion_MysteryWorldsEnd: entry `index` holds the length in words and an offset from the song
// data.
typedef struct MonWave {
    uint16_t length_words;
    uint16_t offset;
} MonWave;

MonWave mon_read_wave(const MonModule* m, uint8_t index);

// The commands of the sequences and patterns (mon_commands.c)

typedef enum MonSequenceCommand {
    MonSequenceCommand_Transpose = 0, // transpose
    MonSequenceCommand_Repeat,        // plays of each pattern
    MonSequenceCommand_FadeIn,        // ticks per volume step
    MonSequenceCommand_FadeOut,       // ticks per volume step
    MonSequenceCommand_Tempo,
    MonSequenceCommand_Jump, // word index in the sequence
    MonSequenceCommand_Stop,
    MonSequenceCommand_Unknown, // behind the command table, the original jumps into its variables
} MonSequenceCommand;

// A sequence word below 0xF000 is a pattern (a signed offset from the song data); from there on it is a command with
// a parameter byte.
bool mon_sequence_is_pattern(uint16_t word);
MonSequenceCommand mon_sequence_command(uint16_t word);
uint8_t mon_sequence_parameter(uint16_t word);

typedef enum MonPatternCommand {
    MonPatternCommand_Note = 0, // a note byte, then the length
    MonPatternCommand_End,      // the pattern ends: play it again or take the next sequence word
    MonPatternCommand_Instrument,
    MonPatternCommand_Volume,
    MonPatternCommand_Slide,     // slide target note and slide length
    MonPatternCommand_SlideNote, // note, slide length, length: a new note that slides from the old one
    MonPatternCommand_Release,   // length: silence the voice
    MonPatternCommand_Arpeggio,  // arpeggio list offset
    MonPatternCommand_Wait,      // length in ticks: stop the DMA
    MonPatternCommand_Swing,     // ticks the next length loses
    MonPatternCommand_Rest,      // length: the envelope leaves its hold
    MonPatternCommand_Unknown,   // behind the command table, the original jumps into its variables
} MonPatternCommand;

MonPatternCommand mon_pattern_command(MonVersion version, uint8_t byte);

// The bytes that follow the command byte.
unsigned mon_pattern_arguments(MonPatternCommand command);

// The sequencer shared by the versions (maniacsofnoise.c)

// Reads sequence words of a voice until one selects a pattern, which becomes the voice's pattern: true. False when a
// sequence command ends the interrupt. `repeating`: the pattern has just ended, so the voice plays it again while it
// has repeats left.
bool mon_sequence_next(MonPlayer* p, int voice, bool repeating);

// One voice of an interrupt (mon_voice.c): false when a sequence command ended the interrupt.
bool mon_step_voice(MonPlayer* p, int voice);

// What the two versions' replayers implement (mon_old.c, mon_new.c): a voice whose note has run out reads its pattern,
// every other voice runs its sustain.

bool mon_old_read_pattern(MonPlayer* p, int voice);
void mon_old_sustain(MonPlayer* p, int voice);
bool mon_new_read_pattern(MonPlayer* p, int voice);
void mon_new_sustain(MonPlayer* p, int voice);

// The parts of a voice's tick the versions share (mon_voice.c)

// The older versions (the first two) keep lists as absolute positions and play other kinds of instrument as wave lists.
bool mon_is_old_version(MonVersion version);

// The period table entry of a note scaled by the instrument's tuning: the long shifts of the note-on and the word
// shifts of the arpeggio.
uint16_t mon_scaled_period(const MonModule* m, uint8_t note, uint16_t tuning);
uint16_t mon_scaled_period_word(const MonModule* m, uint8_t note, uint16_t tuning);

// The AUDxVOL of an envelope byte.
uint16_t mon_envelope_level(const MonPlayer* p, const MonVoice* v, uint8_t entry);

// 68000 divs.w: the register after the instruction, remainder in the high word. A quotient that does not fit 16 bits
// leaves the dividend.
uint32_t mon_divs(int32_t dividend, int16_t divisor);

// The slide towards the new note's period (not MonVersion_Gyroscope).
void mon_run_slide(MonVoice* v);

// The byte of a list at the voice's position; a byte 0x80 moves the position back by the byte behind it.
uint8_t mon_list_byte(const MonModule* m, uint32_t list, uint8_t* pos);

// The vibrato list's offset to the voice's period.
void mon_run_vibrato(MonPlayer* p, int voice, const MonInstrument* ins);

// The instrument command: the voice takes the instrument and its volume.
void mon_select_instrument(MonPlayer* p, int voice, uint8_t number);

// The part of a note-on all versions share: the voice's note and period, the restart of its lists, the sample start.
void mon_start_note(MonPlayer* p, int voice, uint8_t note, const MonInstrument* ins);

// The DMA start and the loop of an instrument that plays a sample.
void mon_run_sample(MonPlayer* p, int voice, const MonInstrument* ins);
