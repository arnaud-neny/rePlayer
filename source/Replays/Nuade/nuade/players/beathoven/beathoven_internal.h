// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: types shared by the module loader, the replayer locator, the song decoder and the
// replayer.
//
// A module is the hunk of an Amiga program: the replayer's code, its voice records and variables, the song (track
// lists and command streams), the instrument records and the samples. The player keeps the voice records in structs and
// reads everything else from the module bytes, at the addresses the locator found in the code. An address is an offset
// into the hunk, which is what a relocated pointer of the file is before the loader adds the segment address.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/beathoven.h>

static inline uint8_t bthm_u8(const BthModule* m, uint32_t address) {
    return address < m->size ? m->data[address] : 0;
}

static inline uint16_t bthm_u16(const BthModule* m, uint32_t address) {
    return (uint16_t)((bthm_u8(m, address) << 8) | bthm_u8(m, address + 1));
}

static inline int16_t bthm_s16(const BthModule* m, uint32_t address) {
    return (int16_t)bthm_u16(m, address);
}

static inline uint32_t bthm_u32(const BthModule* m, uint32_t address) {
    return ((uint32_t)bthm_u16(m, address) << 16) | bthm_u16(m, address + 2);
}

static inline void bthm_set16(BthModule* m, uint32_t address, uint16_t value) {
    if (address < m->size && m->size - address >= 2) {
        m->data[address] = (uint8_t)(value >> 8);
        m->data[address + 1] = (uint8_t)value;
    }
}

static inline void bthm_set32(BthModule* m, uint32_t address, uint32_t value) {
    if (address < m->size && m->size - address >= 4) {
        bthm_set16(m, address, (uint16_t)(value >> 16));
        bthm_set16(m, address + 2, (uint16_t)value);
    }
}

// A paula pointer to `address` with AUDxLEN `length_words` readable there (65536 words for 0); null if it does not fit.
// An address outside the module is chip memory the module does not own, which reads as silence.
static inline const int8_t* bthm_sample(const BthModule* m, uint32_t address, uint16_t length_words) {
    const uint32_t bytes = length_words != 0 ? 2u * length_words : NUADE_PAULA_MAX_SAMPLE_BYTES;
    return address <= m->size + NUADE_PAULA_MAX_SAMPLE_BYTES - bytes ? (const int8_t*)(m->data + address) : nullptr;
}

// The target of a 16 bit pc-relative displacement whose extension word is at `extension`
static inline uint32_t bthm_pc16(const BthModule* m, uint32_t extension) {
    return extension + (uint32_t)(int32_t)bthm_s16(m, extension);
}

// The loaded hunk: the bytes and which longwords the hunk's relocation table marks as pointers

#define BTH_VERSION_100 0x4E313030u // "N100"
#define BTH_VERSION_109 0x4E313039u // "N109"
#define BTH_VERSION_NEW 0x4E4E4557u // "NNEW"

// bthi_load: the hunk of the file (relocations read, not applied); null if the file is not a module
BthImage* bthi_load(const uint8_t* data, size_t size);
void bthi_free(BthImage* image);
bool bthi_is_pointer(const BthImage* image, uint32_t address);
BthHeader bthi_header(const BthImage* image);

// The replayer of a module, as the locator read it from the code. The replayers of different modules differ in a
// few instructions: where the tempo and the note offset come from, the period table, and a handful of switches.

// Finds the replayer in the code; false if the module has one this port does not know
bool bth_locate(const BthImage* image, const BthHeader* header, BthReplayer* out);

// The songs: what the module's init routine sets up for each subsong

// Runs the routine the eagleplayer calls once after it loads the module (the header's `once`), if the module has one
// and it is of a known form: it turns the sample offsets of the instrument records into addresses. False if the
// routine is not known.
bool bth_apply_once(BthModule* module, const BthHeader* header);

void bth_songs_free(BthSong* songs, uint32_t count);

// Reads the subsongs out of the init routine; returns how many it decoded (0 if it has a form this port does not
// know)
uint32_t bth_decode_songs(const BthImage* image, const BthHeader* header, const BthReplayer* replayer, BthSong* songs,
                          uint32_t capacity);

// The player: the voices (the replayer's voice records) and the module the replayers run on

// An instrument record of BEATHOVEN109 and BEATHOVENNEW. The init routines of two modules rewrite its sample address
// and length, hence the field address functions.
#define BTH_INSTRUMENT_BYTES 14
#define BTH_INSTRUMENT_ONE_SHOT_LENGTH_FIELD 2
#define BTH_INSTRUMENT_SAMPLE_FIELD 8

typedef struct BthInstrument {
    uint16_t volume;
    uint16_t one_shot_length; // bytes
    int16_t loop_start;       // offsets into the sample, bytes; also the length of the part played before the loop
    int16_t loop_end;
    uint32_t sample;
    bool one_shot; // the sample plays once, then the voice repeats silence
} BthInstrument;

static inline uint32_t bthm_instrument_sample_address(uint32_t record) {
    return record + BTH_INSTRUMENT_SAMPLE_FIELD;
}

static inline uint32_t bthm_instrument_one_shot_length_address(uint32_t record) {
    return record + BTH_INSTRUMENT_ONE_SHOT_LENGTH_FIELD;
}

static inline BthInstrument bthm_instrument(const BthModule* m, uint32_t record) {
    return (BthInstrument) { .volume = bthm_u16(m, record),
                             .one_shot_length = bthm_u16(m, bthm_instrument_one_shot_length_address(record)),
                             .loop_start = bthm_s16(m, record + 4),
                             .loop_end = bthm_s16(m, record + 6),
                             .sample = bthm_u32(m, bthm_instrument_sample_address(record)),
                             .one_shot = bthm_u16(m, record + 12) != 0 };
}

// An instrument record of BEATHOVEN100: the sample plays once
#define BTH_LEGACY_INSTRUMENT_BYTES 12

typedef struct BthLegacyInstrument {
    uint16_t volume;
    uint32_t length; // bytes
    uint32_t sample;
} BthLegacyInstrument;

static inline BthLegacyInstrument bthm_legacy_instrument(const BthModule* m, uint32_t record) {
    return (BthLegacyInstrument) { .volume = bthm_u16(m, record),
                                   .length = bthm_u32(m, record + 2),
                                   .sample = bthm_u32(m, record + 6) };
}

// The words of a command in a command stream: the command word and what follows. Which are operands depends on the
// command. The size of a command in bytes: a note or short command, an instrument command (with a record address) and
// an arpeggio command's header (command, length, count; the note numbers follow).
#define BTH_COMMAND_SHORT_BYTES 4
#define BTH_COMMAND_INSTRUMENT_BYTES 6
#define BTH_COMMAND_ARPEGGIO_HEADER_BYTES 6

// The command words of the command streams; any other word is a note number. Instrument is common to all replayers,
// the others differ (BEATHOVEN100 has its own).
#define BTH_COMMAND_END 0x00
#define BTH_COMMAND_INSTRUMENT 0x80
#define BTH_COMMAND_SLIDE 0x81
#define BTH_COMMAND_LEGATO 0x82
#define BTH_COMMAND_HOLD 0x83
#define BTH_COMMAND_ARPEGGIO 0x84
#define BTH_COMMAND_NOTE_BIAS 0x85

// A tick that reads more commands than this (a track list of empty streams) is a broken module: the voice stops
#define BTH_MAX_COMMANDS_PER_TICK 65536

typedef struct BthCommandWords {
    uint16_t word;
    uint16_t operand; // the word after the command word
    uint16_t count;   // the word after that
    uint32_t record;  // the longword after the command word
} BthCommandWords;

static inline BthCommandWords bthm_command_words(const BthModule* m, uint32_t address) {
    return (BthCommandWords) { .word = bthm_u16(m, address),
                               .operand = bthm_u16(m, address + 2),
                               .count = bthm_u16(m, address + 4),
                               .record = bthm_u32(m, address + 2) };
}

// What the replayers write to AUDxVOL. The eagleplayer's patch masks the volume with 0x7F (the channel applies Paula's
// rule to it).
static inline uint8_t bth_paula_volume(uint16_t written) {
    return (uint8_t)(written & 0x7F);
}

// A track list is an array of entries: the address of a command stream and the voice's note offset (a longword; the
// basic replayers use its low word), ended by an entry whose address is 0
#define TRACK_ENTRY_BYTES 8

typedef struct BthTrackEntry {
    uint32_t stream;
    uint32_t note_base;
} BthTrackEntry;

static inline BthTrackEntry bthm_track_entry(const BthModule* m, uint32_t address) {
    return (BthTrackEntry) { .stream = bthm_u32(m, address), .note_base = bthm_u32(m, address + 4) };
}

// The arpeggio goes on to the next note number of its table, and wraps
static inline void bth_advance_arpeggio(BthVoice* v) {
    v->arpeggio_index++;
    if (v->arpeggio_index == v->arpeggio_count) {
        v->arpeggio_index = 0;
    }
}

// AUDxLC and AUDxLEN of a voice's channel; a sample that does not fit in memory reads as silence
void bth_set_address(const BthPlayer* p, BthVoice* v, uint32_t address, uint16_t length_words);

// The voice's track list returns to its loop entry; SongEnd comes when every voice has done so
void bth_wrap_track(BthVoice* v);

// The index of the instrument record at `record` in the module's instrument table, -1 if it is not in it
int bth_instrument_index(const BthPlayer* p, uint32_t record, uint32_t record_bytes);

// The 16 bit word of the period table at `address`. The replayers read it with an index that can run past the table
// into whatever follows; when that is a relocated longword, the word is that of the pointer UADE has loaded.
uint16_t bth_table_word(const BthPlayer* p, uint32_t address);

// The voice routine of the basic and the extended replayer: one voice's work in one tick of the play routine
void bth_update_voice(BthPlayer* p, BthVoice* v);

// The voice routine of BEATHOVEN100, with the voice's note offset
void bth_update_voice_legacy(BthPlayer* p, BthVoice* v, int32_t note_offset);
