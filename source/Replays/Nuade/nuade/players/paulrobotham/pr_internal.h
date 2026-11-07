// SPDX-License-Identifier: MIT
// Paul Robotham: the song file, its typed record readers and the replayer's state.
//
// Song file (all offsets from the start of the file, big-endian):
//   u16 voice_count (1..4), u16 vibrato_count, u16 envelope_count, u16 instrument_count
//   u32 stream[voice_count]      the voice streams
//   u32 vibrato[vibrato_count]   tables of signed bytes, indexed by the vibrato position
//   u32 envelope[envelope_count] tables of volume bytes, indexed by the envelope position
//   instrument records, 12 bytes each, then the data the offsets point to
// Instrument record: u32 sample (offset in the sample file), u16 length (words), u32 loop (offset in the sample file),
// u16 loop_length (words; 0 = the loop is 16 zero bytes: the replayer's zeroed memory behind its hunk).

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_paula_channel.h"
#include <nuade/players/paulrobotham.h>

#define PR_HEADER_BYTES 8
#define PR_INSTRUMENT_BYTES 12
#define PR_PERIOD_COUNT 60 // the replayer indexes its period table by note, 0x7E at most; the model stops at 59

// A stream byte: 0 ends the stream, 1..0x7E is a note (followed by a duration byte), 0x7F a tie (followed by a duration
// byte), 0x80..0xFF a command (the low five bits select it).
#define PR_STREAM_END 0
#define PR_STREAM_TIE 0x7F
#define PR_STREAM_COMMAND 0x80
#define PR_COMMAND_MASK 0x1F

typedef enum PrCommand {
    PrCommand_LoopStart = 1,     // arg: repeat count
    PrCommand_LoopEnd = 2,       // jump back to the loop start until the count is used up
    PrCommand_VibratoDepth = 7,  // arg: depth divisor, 0 = vibrato off
    PrCommand_VibratoSpeed = 8,  // arg: position step per tick
    PrCommand_Portamento = 9,    // arg: step / 4, 0 = off
    PrCommand_VibratoTable = 11, // arg: table number
    PrCommand_EnvelopeTable = 12,
    PrCommand_Volume = 13,
    PrCommand_Tempo = 14, // arg: tempo / 16
    PrCommand_Instrument = 15,
    PrCommand_Options = 19,    // arg: bit 0 legato, bit 1 keep the envelope, bit 2 keep the vibrato
    PrCommand_Unused20 = 20,   // arg stored in a variable that nothing reads
    PrCommand_Unused21 = 21,   // same
    PrCommand_MasterFade = 22, // args: target volume, ticks per step (0 = at once)
    PrCommand_FadeStart = 23,  // start a fade-out from the voice volume
    PrCommand_FadeStep = 24,   // arg: volume taken off per tick by the fade-out
    PrCommand_LedOff = 25,     // CIA LED bit
    PrCommand_LedOn = 26,
    // The other numbers run a routine that only flashes the screen colour: no argument, no effect.
} PrCommand;

// The song

typedef struct PrInstrument {
    uint32_t sample;
    uint16_t length_words;
    uint32_t loop;
    uint16_t loop_length_words;
} PrInstrument;

uint8_t pr_song_u8(const PrSong* song, uint32_t offset);

uint32_t pr_song_stream(const PrSong* song, uint32_t voice);
uint32_t pr_song_vibrato(const PrSong* song, uint32_t table);
uint32_t pr_song_envelope(const PrSong* song, uint32_t table);
PrInstrument pr_song_instrument(const PrSong* song, uint32_t number);

// The long after the first stream offset, minus one: the stream end that the eagleplayer's SongEnd check compares the
// first voice's position with (the next stream starts right after the first one's 0 byte).
uint32_t pr_song_end_marker(const PrSong* song);

// AUDxLC of a sample file offset (bit 0 dropped as the hardware does); null when the offset is outside the file.
const int8_t* pr_song_sample(const PrSong* song, uint32_t offset);

// Whether a stream byte below 0x80 is a note or tie the replayer models (the period table has PR_PERIOD_COUNT entries).
bool pr_stream_note_ok(uint8_t byte);

// Arguments the stream carries after a command byte (0 for the numbers that run the colour flash).
uint32_t pr_command_arg_count(uint32_t command);
