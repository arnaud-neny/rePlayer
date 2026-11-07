// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player: the module, the replayer versions, and what the three engines share.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/hippelhip.h>

// The module bytes: reads past the end return 0 (UADE's memory behind the module is clear). The allocation is a
// nuade_paula_padded_copy, so a sample that starts inside the module can run past its end with any AUDxLEN.

// UADE loads the module here. Engine C chooses a repeat delay by the high word of the sample's address.
#define HH_LOAD_ADDRESS 0xf000

// The "TFMX" header and the records of the tables behind it
#define HH_HEADER_SIZE 0x20
#define HH_SEQUENCE_RECORD_SIZE 64 // frequency sequences and volume sequences (instruments)
#define HH_TRACK_ROW_SIZE 12       // one position: 3 bytes for each of 4 voices
#define HH_SUBSONG_RECORD_SIZE 6
#define HH_SAMPLE_RECORD_SIZE 30

static inline uint8_t hhm_u8(const HhModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static inline uint16_t hhm_u16(const HhModule* m, uint32_t offset) {
    return (uint16_t)((hhm_u8(m, offset) << 8) | hhm_u8(m, offset + 1));
}

static inline uint32_t hhm_u32(const HhModule* m, uint32_t offset) {
    return ((uint32_t)hhm_u16(m, offset) << 16) | hhm_u16(m, offset + 2);
}

// Offset of table record `number` for a table of `record_size` byte records, as the replayer's word arithmetic forms it
static inline uint32_t hhm_record(uint32_t table, uint32_t number, uint32_t record_size) {
    return table + (uint32_t)(uint16_t)(number * record_size);
}

// Frequency and volume sequence commands: a byte from 0xe0 up is a command, anything below is a value

typedef enum HhCommand {
    HhCommand_FirstCommand = 0xe0,
    HhCommand_Jump = 0xe0,
    HhCommand_Hold = 0xe1,
    HhCommand_Sample = 0xe2,
    HhCommand_Vibrato = 0xe3,
    HhCommand_SampleNoReset = 0xe4,
    HhCommand_E5 = 0xe5, // A: start a sample sweep, B: start a sample from a bank of records
    HhCommand_E6 = 0xe6, // A: change the sweep, B: the same as E4
    HhCommand_E7 = 0xe7, // A and B: start a sample unless it plays already, C: switch to another frequency sequence
    HhCommand_Wait = 0xe8,
    HhCommand_PackedSample = 0xe9,
} HhCommand;

// Replayer engines and versions

extern const HhEngine k_hh_engine_a; // wings of death, dragonflight: sweeping sample windows, sample packs
extern const HhEngine k_hh_engine_b; // leaving teramis, seven gates of jambala: repeat registers every tick
extern const HhEngine k_hh_engine_c; // shaolin, warp, battle valley: repeat registers a tick after the start

// hh_common.c: what the engines share

// AUDxLC = `offset` in the module, AUDxLEN = `words` as the replayer writes it (0 plays 65536 words)
HhRegisters hh_registers(const HhModule* m, uint32_t offset, uint16_t words);

static inline void hh_write_registers(NuadePaulaChannel* paula, HhRegisters registers) {
    paula->lc = registers.lc;
    paula->len_words = registers.len_words;
}

// The replayer switches a channel's DMA off
static inline void hh_channel_stop(HhVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
}

// ... and switches it on again at the end of the tick
static inline void hh_channel_restart(HhVoice* v) {
    hh_channel_stop(v);
    v->dma_restart = true;
}

static inline void hh_restart_volume(HhVoice* v) {
    v->vseq_pos = 0;
    v->vseq_counter = 1;
}

// A sample table record
typedef struct HhSample {
    uint32_t start;       // into the sample data
    uint16_t length;      // words
    uint16_t loop_offset; // bytes
    uint16_t loop_length; // words
} HhSample;

HhSample hh_sample(const HhModule* m, uint8_t number);

// A sample pack: a sample whose data is a record count, an extra size, records of start/end offsets, then the data
typedef struct HhPack {
    bool magic;       // the data starts with "SSMP"
    uint32_t records; // module offset of the records
    uint32_t data;    // module offset of the sample data
} HhPack;

HhPack hh_sample_pack(const HhModule* m, uint8_t number);

typedef struct HhPackEntry {
    uint32_t start; // into the pack's data
    uint32_t end;
} HhPackEntry;

HhPackEntry hh_pack_entry(const HhModule* m, const HhPack* pack, uint8_t record);

// The frequency sequence commands that engines A and B share. Each moves the sequence on by the command's length.
void hh_command_sample(const HhModule* m, HhVoice* v, uint8_t number);          // E2
void hh_command_sample_no_reset(const HhModule* m, HhVoice* v, uint8_t number); // E4
void hh_command_sample_if_new(const HhModule* m, HhVoice* v, uint8_t number);   // E7
void hh_command_vibrato(const HhModule* m, HhVoice* v, uint32_t at);            // E3 at the command

// What a command of the chained frequency sequences (engines B and C) asks for next
typedef enum HhNext {
    HhNext_Value = 0, // the byte behind the sequence position is the value of this tick
    HhNext_Switch,    // a new sequence was selected: read it from its start, without counting its wait
} HhNext;

typedef HhNext (*HhCommandFn)(const HhModule* m, const HhVersion* version, HhVoice* v, uint8_t command, uint32_t at);

// Engines B and C read one command of the sequence at a time; `command` runs the commands that differ
void hh_run_fseq_chain(const HhModule* m, const HhVersion* version, HhVoice* v, HhCommandFn command);

// The song start (the replayer's init with the subsong in song->subsong) and one tick of every engine
void hh_song_start(HhSong* song, const HhModule* m, const HhVersion* version);
void hh_song_tick(HhSong* song, const HhModule* m, const HhVersion* version, NuadeVoices* voices);

// The note of a voice record that the module file holds, for the engines whose song start leaves it
uint8_t hh_file_note(const HhModule* m, const HhVersion* version, int voice);
