// SPDX-License-Identifier: MIT
// AProSys player: the module and its records, shared by the player and the wave generator.
//
// A module is "ADRVPACK" and nine length-prefixed sections. The player uses the first eight:
//   order        two bytes per entry: a command and its argument
//   steps        26 bytes: ticks per row, row count and four 6-byte cells (one per voice)
//   tracks       4 bytes per row and voice: note offset, instrument offset, volume, slide speed
//   arpeggios    byte streams of note offsets
//   instruments  26 bytes: sample record number, envelope speeds and offsets, pitch and volume modulation
//   envelopes    pairs of (duration, target level)
//   samples      6-byte records: how a voice's sound is played or synthesised
//   sample data  8-bit samples, addressed in words by the sample records
// The 68k player reads these through pointers into the loaded file, past the end of a section into the next one, and
// past the end of the file into whatever follows. Here an address is a byte offset into the file (every record
// reader below takes one) and a read past the end of the file gives 0.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/aprosys.h>

#define APS_STEP_SIZE 26
#define APS_INSTRUMENT_SIZE 26
#define APS_SAMPLE_RECORD_SIZE 6
#define APS_TRACK_ENTRY_SIZE 4
#define APS_NO_NOTE 0x3f
#define APS_NO_REPEAT 0xff

// The wave memory: one 128-byte buffer pair per voice (the one being built and the one Paula repeats), as the player
// allocates it in chip memory. Generators write up to 8 bytes past the end of a buffer; APS_WAVE_PADDING keeps the
// last buffer's overrun inside the allocation.
#define APS_WAVE_BUFFER_BYTES 128
#define APS_WAVE_BUFFERS (APS_VOICES * 2)
#define APS_WAVE_PADDING 16
#define APS_WAVE_BYTES (APS_WAVE_BUFFERS * APS_WAVE_BUFFER_BYTES + APS_WAVE_PADDING)
// The 2-byte block the player allocates for the silent sound lies this far before the wave memory
#define APS_SILENCE_DISTANCE 16
#define APS_CHIP_BYTES (APS_SILENCE_DISTANCE + APS_WAVE_BYTES)

// Addresses are offsets from the start of the file, as the 68k player sees memory: the wave memory the player allocates
// lies behind the file (APS_WAVE_BYTES at ApsModule.wave_address), and a read of any address that nothing holds gives
// 0. Two addresses are the player's own: the sample record that stops a voice, and Amiga address 0 (where the pointers
// of a voice that has not started a note yet point; UADE's exception vector table lies there).
#define APS_ADDR_SILENT_RECORD 0x62000000u

// UADE loads the module here; the addresses of a module are offsets from it
#define APS_MODULE_BASE 0x13000u
#define APS_ADDR_VECTORS (0u - APS_MODULE_BASE)
#define APS_VECTOR_BYTES 0x100

// A signed 16-bit division as the 68k does it: dividend / divisor truncated towards zero, or the low word of the
// dividend when the quotient does not fit in 16 bits. The divisor is not 0.
int16_t aps_divs16(int32_t dividend, int16_t divisor);

// The module

// Split a module into its sections; false when InitPlayer would refuse it.
bool aps_module_parse(ApsModule* module, const uint8_t* data, size_t size, const int8_t* wave);

// Raw reads: a byte of the file, of the wave memory, of the stop record, or 0.
uint8_t aps_u8(const ApsModule* module, uint32_t address);
uint16_t aps_u16(const ApsModule* module, uint32_t address);

// Records

typedef enum ApsOrderCommand {
    ApsOrder_End = 0x00,    // the song is over
    ApsOrder_Steps = 0x20,  // play `count` steps starting at step `param`
    ApsOrder_Repeat = 0x40, // jump by `param` entries `count` times, then go on
    ApsOrder_Jump = 0x60,   // jump by `param` entries
    ApsOrder_Octave = 0x80, // select the period table position `param & 0x3f`
    ApsOrder_Speed = 0xa0,  // ticks per duration unit
    ApsOrder_Filter = 0xc0, // the LED filter (not part of the sound path here)
} ApsOrderCommand;

typedef struct ApsOrderEntry {
    uint8_t command; // an ApsOrderCommand, or 0xe0 for the unused one
    uint8_t count;
    uint8_t param;
} ApsOrderEntry;

ApsOrderEntry aps_order_entry(const ApsModule* module, uint8_t position);

typedef struct ApsStep {
    uint8_t duration; // ticks per row = duration * speed
    uint8_t rows;
} ApsStep;

ApsStep aps_step(const ApsModule* module, uint32_t step);

typedef struct ApsCell {
    uint8_t note; // APS_NO_NOTE for none
    bool restart_env3;
    uint8_t instrument;
    uint8_t volume_step; // 0..3: how fast the channel volume moves towards the target
    uint8_t volume_target;
    uint8_t arpeggio; // added to the track entry's slide speed to address the arpeggio table
    uint16_t track;   // first track entry of this voice in the step, in entries
} ApsCell;

ApsCell aps_cell(const ApsModule* module, uint32_t step, uint32_t voice);

typedef struct ApsTrackEntry {
    uint8_t note;  // APS_NO_NOTE for none
    bool arpeggio; // `param` addresses an arpeggio, otherwise it is the slide speed
    bool key;      // start the note (restart the envelopes, stop the DMA of a chained sound)
    uint8_t instrument;
    bool instrument_direct; // the cell's instrument is used as it is; the entry's one picks the sample
    bool tempo;             // `level` is a new row length (times the speed), the voice volume stays
    uint8_t level;
    uint8_t param;
} ApsTrackEntry;

ApsTrackEntry aps_track_entry(const ApsModule* module, uint32_t entry);

typedef struct ApsInstrument {
    uint8_t sample;         // sample record number of the note
    uint8_t release_sample; // and of the release
    uint8_t env_speed[APS_ENVELOPES];
    uint16_t env_offset[APS_ENVELOPES]; // envelope streams, in words from the envelope section
    uint16_t release_env;               // the stream envelope 0 switches to at a release
    uint8_t env1_modulation;            // how far envelope 2 scales the length of envelope 1's steps
    int8_t period_offset;
    int8_t pitch_amount_a;
    int8_t pitch_amount_b;
    uint8_t pitch_select; // envelopes for the three pitch terms: bits 7..4, 3..2, 1..0
    int8_t volume_base;
    int8_t volume_amount_a;
    int8_t volume_amount_b;
    uint8_t volume_select; // envelopes for the two volume terms: bits 7..2, 1..0
} ApsInstrument;

ApsInstrument aps_instrument(const ApsModule* module, uint32_t instrument);

typedef struct ApsEnvPoint {
    uint8_t duration;
    int8_t target;
} ApsEnvPoint;

ApsEnvPoint aps_env_point(const ApsModule* module, uint32_t point);

typedef enum ApsSampleKind {
    ApsKind_Loop = 0,    // chained: loop this sample, no more interrupts
    ApsKind_Once = 1,    // chained: play once, then the silent record
    ApsKind_Chain = 2,   // chained: play, go on with the next record
    ApsKind_Synth = 3,   // generate a wave every tick
    ApsKind_Window = 4,  // a sample whose start moves with envelopes (two factors)
    ApsKind_Window2 = 5, // the same with one factor
    ApsKind_Silence = 0xd,
} ApsSampleKind;

// A sample record as the different kinds read it: the same 48 bits are an address and a length, or the parameters of a
// wave generator or of a filter pass.
typedef struct ApsSampleRecord {
    uint8_t kind;  // ApsSampleKind
    uint8_t env_a; // envelope numbers of the two amounts
    uint8_t env_b;
    bool filter_inverse; // filter record: the second filter form
    bool filter_reset;   // filter record: restart the filter memory with a new note
    uint8_t mode;        // generator record: shape, copy or combine
    uint8_t op;
    uint8_t filter2_count;      // records after this one that hold the second filter
    uint8_t filter1_count;      // and the first
    uint8_t synth_length_words; // generator record: the wave length for Paula (bits 7..0 of the first long)
    uint8_t filter_offset_a;    // filter record: what the first amount adds to (bits 15..8)
    uint8_t filter_offset_b;    // and the second (bits 7..0)
    uint8_t wave_bytes;         // generator record: wave length in bytes (bits 5..0 times 2, 0 = 128)
    uint32_t address;           // sample address in words from the sample data
    uint16_t length_words;      // the second word, for the chained kinds
    uint8_t amount_a;           // its two bytes: generator, window and filter amounts
    uint8_t amount_b;
    uint8_t window_length_words; // window record: the length for Paula (the second byte again)
    uint8_t filter_gain;         // filter record: the top nibble of the second word
} ApsSampleRecord;

ApsSampleRecord aps_sample_record(const ApsModule* module, uint32_t record);

// The wave generator

typedef struct ApsWaveJob {
    const ApsModule* module;
    int8_t* memory;                // APS_WAVE_BYTES of wave memory
    uint32_t dst;                  // the buffer to build, an offset in `memory`
    uint32_t record;               // the generator's sample record
    int16_t levels[APS_ENVELOPES]; // the voice's envelope levels
    bool fresh;                    // a note started since the last build
    ApsWaveFilter* filter;
} ApsWaveJob;

// One tick of a synthesised voice: generate into job->dst, then run the filter records.
void aps_wave_build(const ApsWaveJob* job);

// Offset (in words) that moves the start of a window-kind sample; `record` is the window record.
int32_t aps_window_offset(const ApsSampleRecord* record, const int16_t levels[APS_ENVELOPES]);

// The player

// The period of a note: the table past the end reads the 68k player's data block
uint16_t aps_period_lookup(const ApsPlayer* player, uint32_t note);

// Start the voice's sample record playing (plain DMA for the synthesised and window kinds, an interrupt request for the
// chained ones with the key flag)
void aps_start_sound(ApsPlayer* player, ApsVoice* voice, const ApsTrackEntry* entry);

void aps_restart_envelopes(const ApsPlayer* player, ApsVoice* voice, const ApsCell* cell);
void aps_release_envelope(const ApsPlayer* player, ApsVoice* voice);

// One tick of all voices
void aps_update_voices(ApsPlayer* player);

// Audio interrupts: the handler for what is pending, and the block starts the mixer played since the last tick
void aps_service_interrupts(ApsPlayer* player, ApsVoice* voice);
void aps_answer_wraps(ApsPlayer* player, ApsVoice* voice, uint32_t index, const NuadeVoices* voices);
