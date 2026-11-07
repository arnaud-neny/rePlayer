// SPDX-License-Identifier: MIT
// Mark Cooksey player, old format, translated from the replayer of the ST game conversions: the "Mark Cooksey Old"
// eagleplayer (Wanted Team) holds it for mco. songs; for mcr. songs the replayer is the mcr. file itself and the
// "Mark Cooksey" eagleplayer calls into it.
//
// Data (an Atari ST executable: 28 header bytes, then): two longs that are the offsets of the sub-sequence table
// (words, each the distance of a sequence from the table) and of the sample table (longs, each the distance of a
// sample from the table; a sample is a word count and the 8-bit data); then 16 bytes per subsong, one long per voice:
// the offset of the voice's stream.
//
// A voice reads commands from its stream (see McoCommand). A note or a rest sets the number of ticks until the next
// command; the voice runs commands until one does.
//
// The replayer exists in two versions. Version 2 (Overlander's mcr.) also holds a second command stream per voice for
// sound effects, which the games start and no song does, and scales volumes by a master volume.
//
// What differs between the replayers is resolved when the song is opened (see McoModule): the period table (and the
// words before it, which a transposed note reads), the master volume, the volumes the voices start with, and two
// behaviours. The eagleplayer patches an mcr. replayer when it loads it: the note start does not wait for the hardware
// and stops the DMA every time (the replayer in the eagleplayer silences a running channel first: period 2, volume 0),
// retune writes the period only (the original writes a long, which sets the volume too), and the end and goto commands
// call the eagleplayer, which ends the song when all four voices have run one of them. The replayer in the eagleplayer
// ends the song when all voices are stopped.

#include <nuade/players/markcooksey_old.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// An Atari ST executable: 0x601A, the text size, and zeros to the code at 0x1C.
#define ST_HEADER_SIZE 0x1C
#define ST_MAGIC 0x601A
#define OP_BRA_B 0x60
#define OP_BRA_W 0x6000
#define OP_LEA_A2_PC 0x45FA
#define OP_MULU_D4_PC 0xC8FA
#define OP_MOVEM_D0_A0_A3 0x48E780F0u // movem.l d0/a0-a3,-(a7)

// Check2 of the eagleplayer for the data file: the size and the text size agree but for the low byte, and byte 0x20 of
// the file is 0. The file name has the prefix mco. (or the suffix .mco).
#define DATA_CHECK_BYTE 0x20

// The replayer of an mcr. file: four `bra.w` entries at 0x1C (init, stop, tick, running mask), the init starts with
// movem.l d0/a0-a3,-(a7). Version 2 has more entries. The file name has the prefix mcr.; its data is the mcs. file.
#define DRIVER_PREFIX "mcr."
#define DATA_PREFIX "mcs."
#define PREFIX_LENGTH 4
#define ENTRY_SIZE 4 // a `bra.w`
#define DRIVER_ENTRIES 4
#define DRIVER_ENTRY_TICK 2
#define DRIVER_ENTRY_V2_ONLY 5
#define DRIVER_SEARCH_SPAN 0x100

// Data layout.
#define SUBSONG_TABLE 8
#define SUBSONG_RECORD_SIZE 16
#define SUBSONG_MAX 255
#define VOICE_ENTRY_SIZE 4  // a long per voice in the subsong table
#define SAMPLE_ENTRY_SIZE 4 // a long in the sample table

// Commands are `bra.w` entries: the command byte is the offset in the table.
#define COMMAND_ENTRY_SIZE 4
#define COMMANDS_PER_TICK 4096

#define STACK_FRAME_SIZE_LOOP 6

#define VOLUME_UNIT 4 // the volume command's byte is multiplied by this

// A stopped voice has a negative count.
#define COUNT_STOPPED (-1)

// The master volume of version 2; version 1 plays at 1.
#define VOLUME_SCALE_ONE 0x8000

// A sample that has played out repeats two words of silence (the patched note start points the channel at memory that
// holds zeros).
static const int8_t k_playout_block[4] = { 0 };
#define PLAYOUT_WORDS 2

// A sample starts with its word count. Its data can start up to this many bytes past the end of the file (a sample
// offset at the end of the file), so the data copy reaches that far before its Paula padding.
#define SAMPLE_HEADER_BYTES 2

// The replayer in the eagleplayer: ten words of its code before the table, then the table, which starts with two
// entries of the octave below that negative transposes reach.
static const uint16_t k_eagleplayer_code_before_table[MCO_PERIOD_WORDS_BEFORE - 2]
    = { 0x0096, 0x2F08, 0x41FA, 0x016E, 0x4644, 0xC950, 0x205F, 0x254B, 0x0002, 0x4E75 };
static const uint16_t k_eagleplayer_periods[2 + MCO_PERIOD_WORDS_AFTER] = {
    0x0358, 0x0328, 0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C, 0x023A, 0x021A, 0x01FC, 0x01E0, 0x01C5, 0x01AC, 0x0194,
    0x017D, 0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0, 0x00E2, 0x00D6, 0x00CA, 0x00BE, 0x00B4,
    0x00A9, 0x00A0, 0x0097, 0x008E, 0x0086, 0x007F, 0x0078, 0x0071, 0x006B, 0x0065, 0x005F, 0x005A,
};

// The voice records of the replayer in the eagleplayer hold a snapshot of the state when the replayer was ripped; only
// the volume matters (it plays until the first volume command).
static const uint16_t k_eagleplayer_volumes[MCO_VOICES] = { 0x14, 0x14, 0x1C, 0x30 };

typedef enum McoCommand {
    McoCommand_Note = 0,
    McoCommand_Retune = 1,
    McoCommand_Instrument = 2,
    McoCommand_Volume = 3,
    McoCommand_Rest = 4,
    McoCommand_RestSilent = 5,
    McoCommand_End = 6,
    McoCommand_Call = 7,
    McoCommand_Return = 8,
    McoCommand_LoopBegin = 9,
    McoCommand_LoopEnd = 10,
    McoCommand_CallTransposed = 11,
    McoCommand_Mark = 12,
    McoCommand_Goto = 13,
    McoCommand_Count,
} McoCommand;

// Reads. A read outside the file gives 0.

static uint8_t data_u8(const McoModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t data_u16(const McoModule* m, uint32_t offset) {
    return (uint16_t)((data_u8(m, offset) << 8) | data_u8(m, offset + 1));
}

static uint32_t data_u32(const McoModule* m, uint32_t offset) {
    return ((uint32_t)data_u16(m, offset) << 16) | data_u16(m, offset + 2);
}

// The address `base` plus the sign-extended 16-bit distance `distance`.
static uint32_t relative_target(uint32_t base, uint16_t distance) {
    return base + (uint32_t)(int32_t)(int16_t)distance;
}

// The period word `bytes` bytes from the start of the period table; 0 outside the copy (and at an odd address, where
// the 68000 would trap).
static uint16_t period_at(const McoModule* m, int bytes) {
    const int index = MCO_PERIOD_WORDS_BEFORE + bytes / 2;
    return (bytes & 1) == 0 && index >= 0 && index < MCO_PERIOD_WORDS ? m->periods[index] : 0;
}

// The sample at data offset `offset`: a word count and the data; null if the offset is outside the file. The data
// starts at most SAMPLE_HEADER_BYTES past the end of the file.
static const int8_t* sample_data(const McoModule* m, uint32_t offset, uint16_t* words) {
    if (offset == 0 || offset > m->size) {
        *words = 0;
        return nullptr;
    }
    *words = data_u16(m, offset);
    return (const int8_t*)(m->data + offset + SAMPLE_HEADER_BYTES);
}

// Probe

// The replayer's Check2 for the data file: an ST executable whose text size agrees with the file size but for the low
// byte.
static bool check_data(const uint8_t* data, size_t size) {
    if (size <= DATA_CHECK_BYTE || data[0] != OP_BRA_B) {
        return false;
    }
    return (nuade_read_be32(data + 2) & ~0xFFu) == (size & ~(size_t)0xFF) && data[DATA_CHECK_BYTE] == 0;
}

// The target of the bra.w at `offset`; 0 if it is not one, goes backwards or is odd (Check2 accepts only forward even
// branches).
static uint32_t entry_target(const uint8_t* data, size_t size, size_t offset) {
    if (offset + ENTRY_SIZE > size || nuade_read_be16(data + offset) != OP_BRA_W) {
        return 0;
    }
    const int16_t distance = (int16_t)nuade_read_be16(data + offset + 2);
    return distance < 0 || (distance & 1) != 0 ? 0 : (uint32_t)(offset + 2 + (size_t)distance);
}

static uint32_t driver_entry(const uint8_t* data, size_t size, int index) {
    return entry_target(data, size, ST_HEADER_SIZE + (uint32_t)(index * ENTRY_SIZE));
}

// The replayer's Check2: an ST header, entries `bra.w` and an init that starts with movem.l d0/a0-a3,-(a7).
static bool check_driver(const uint8_t* data, size_t size) {
    if (size < ST_HEADER_SIZE + DRIVER_ENTRIES * ENTRY_SIZE || nuade_read_be16(data) != ST_MAGIC) {
        return false;
    }
    const uint32_t text_size = nuade_read_be32(data + 2);
    if ((text_size & 0x80000000u) != 0 || (text_size & 1) != 0 || nuade_read_be16(data + 6) != 0) {
        return false;
    }
    for (size_t at = 8; at < ST_HEADER_SIZE; at += 4) {
        if (nuade_read_be32(data + at) != 0) {
            return false;
        }
    }
    for (int i = 0; i < DRIVER_ENTRIES; i++) {
        if (driver_entry(data, size, i) == 0) {
            return false;
        }
    }
    const uint32_t init = driver_entry(data, size, 0);
    return init + 4 <= size && nuade_read_be32(data + init) == OP_MOVEM_D0_A0_A3;
}

// A file name with the prefix `prefix` (PREFIX_LENGTH characters), any case.
static bool has_prefix(const char* filename, const char* prefix) {
    return nuade_strncasecmp(nuade_path_basename(filename), prefix, PREFIX_LENGTH) == 0;
}

McoKind mco_probe(const uint8_t* data, size_t size, const char* filename) {
    if (filename == nullptr) {
        return McoKind_None;
    }
    if (nuade_path_has_ext(filename, "mco") && check_data(data, size)) {
        return McoKind_Data;
    }
    if (has_prefix(filename, DRIVER_PREFIX) && check_driver(data, size)) {
        return McoKind_Driver;
    }
    return McoKind_None;
}

// McoModule

static bool parse_data(McoModule* m) {
    if (m->size < SUBSONG_TABLE + SUBSONG_RECORD_SIZE) {
        return false;
    }
    m->sequences = data_u32(m, 0);
    m->samples = data_u32(m, SAMPLE_ENTRY_SIZE);
    const uint32_t first_stream = data_u32(m, SUBSONG_TABLE);
    const uint32_t count = first_stream >= SUBSONG_TABLE ? (first_stream - SUBSONG_TABLE) / SUBSONG_RECORD_SIZE : 0;
    if (count == 0 || count > SUBSONG_MAX || SUBSONG_TABLE + count * SUBSONG_RECORD_SIZE > m->size) {
        return false;
    }
    m->subsong_count = (int)count;
    return true;
}

// The target of the first instruction with the opcode `opcode` and a pc-relative operand in [from, from + span).
static uint32_t find_pc_relative(const uint8_t* d, size_t size, uint32_t from, uint32_t span, uint16_t opcode) {
    for (uint32_t at = from; at + 4 <= size && at < from + span; at += 2) {
        if (nuade_read_be16(d + at) == opcode) {
            return relative_target(at + 2, nuade_read_be16(d + at + 2));
        }
    }
    return 0;
}

// What a voice's record in an mcr. replayer of version 1 holds at the start: the replayer's variables, a snapshot of
// the state when the file was ripped. Only the volume matters (it plays until the first volume command).
typedef struct DriverVoiceRecord {
    uint16_t volume;
} DriverVoiceRecord;

static DriverVoiceRecord read_driver_voice_record(const uint8_t* d, size_t size, int voice) {
    enum { RECORDS = 0x2C, RECORD_SIZE = 0x42, VOLUME = 0x12 }; // offsets in the file
    const size_t at = RECORDS + (size_t)voice * RECORD_SIZE + VOLUME;
    return (DriverVoiceRecord) { .volume = at + 2 <= size ? nuade_read_be16(d + at) : 0 };
}

// The period words around the table at `table` of the replayer file.
static void copy_driver_periods(McoModule* m, const uint8_t* d, size_t size, uint32_t table) {
    for (int i = 0; i < MCO_PERIOD_WORDS; i++) {
        const int64_t at = (int64_t)table + 2 * (i - MCO_PERIOD_WORDS_BEFORE);
        m->periods[i] = at >= 0 && (uint64_t)at + 2 <= size ? nuade_read_be16(d + at) : 0;
    }
}

// Reads what the replayer file holds that the data does not: the period table (the first lea d16(pc),a2 of the init),
// and in version 2 (told by its longer entry table) the master volume (the operand of the mulu of the volume command)
// and no snapshot volumes.
static bool read_driver(McoModule* m, const uint8_t* d, size_t size) {
    const bool version_2 = driver_entry(d, size, DRIVER_ENTRY_V2_ONLY) != 0;
    const uint32_t table = find_pc_relative(d, size, driver_entry(d, size, 0), DRIVER_SEARCH_SPAN, OP_LEA_A2_PC);
    if (table == 0) {
        return false;
    }
    copy_driver_periods(m, d, size, table);
    m->volume_scale = VOLUME_SCALE_ONE;
    if (version_2) {
        const uint32_t tick = driver_entry(d, size, DRIVER_ENTRY_TICK);
        const uint32_t scale = find_pc_relative(d, size, tick, 4 * DRIVER_SEARCH_SPAN, OP_MULU_D4_PC);
        if (scale == 0 || size < 2 || scale > size - 2) {
            return false;
        }
        m->volume_scale = nuade_read_be16(d + scale);
    }
    for (int i = 0; i < MCO_VOICES; i++) {
        m->start_volume[i] = version_2 ? 0 : read_driver_voice_record(d, size, i).volume;
    }
    m->retune_sets_volume = false;
    m->reports_song_end = true;
    return true;
}

// The replayer in the eagleplayer.
static void read_eagleplayer(McoModule* m) {
    memcpy(m->periods, k_eagleplayer_code_before_table, sizeof(k_eagleplayer_code_before_table));
    memcpy(m->periods + nuade_sizeof_array(k_eagleplayer_code_before_table), k_eagleplayer_periods,
           sizeof(k_eagleplayer_periods));
    m->volume_scale = VOLUME_SCALE_ONE;
    memcpy(m->start_volume, k_eagleplayer_volumes, sizeof(k_eagleplayer_volumes));
    m->retune_sets_volume = true;
    m->reports_song_end = false;
}

// The data of the song: the file after its ST header, followed by zeros. A sample may run past the end of the file
// into them.
static bool keep_data(McoModule* m, const uint8_t* file, size_t size) {
    if (size <= ST_HEADER_SIZE) {
        return false;
    }
    m->size = size - ST_HEADER_SIZE;
    m->data = nuade_paula_padded_copy(nullptr, m->size + SAMPLE_HEADER_BYTES);
    if (m->data == nullptr) {
        return false;
    }
    memcpy(m->data, file + ST_HEADER_SIZE, m->size);
    return true;
}

// ExtLoad of the replayer: the file mcs.<name> next to mcr.<name>, any case. The data of an mcr. replayer is an ST
// executable too.
static McoStatus load_data_file(McoModule* m, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char data_name[256];
    if (path == nullptr || io == nullptr || !has_prefix(path, DRIVER_PREFIX)) {
        return McoStatus_MissingData;
    }
    const char* base = nuade_path_basename(path);
    if (snprintf(data_name, sizeof(data_name), DATA_PREFIX "%s", base + PREFIX_LENGTH) >= (int)sizeof(data_name)) {
        return McoStatus_MissingData;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, data_name) : nullptr;
    McoStatus status = McoStatus_MissingData;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            status = file.size > ST_HEADER_SIZE && nuade_read_be16(file.data) == ST_MAGIC
                             && keep_data(m, file.data, (size_t)file.size)
                         ? McoStatus_Ok
                         : McoStatus_InvalidData;
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return status;
}

void mco_destroy(McoPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
    }
    free(player);
}

// `data` (the mco. or mcr. file) is not kept: everything the player needs is copied.
McoCreateResult mco_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    McoCreateResult result = { .status = McoStatus_InvalidData };
    const McoKind kind = mco_probe(data, size, path);
    if (kind == McoKind_None) {
        return result;
    }
    McoPlayer* player = (McoPlayer*)calloc(1, sizeof(McoPlayer));
    if (player == nullptr) {
        result.status = McoStatus_OutOfMemory;
        return result;
    }
    McoModule* m = &player->module;
    if (kind == McoKind_Data) {
        read_eagleplayer(m);
        result.status = keep_data(m, data, size) ? McoStatus_Ok : McoStatus_OutOfMemory;
    } else {
        result.status = read_driver(m, data, size) ? load_data_file(m, path, io) : McoStatus_InvalidData;
    }
    if (result.status != McoStatus_Ok || !parse_data(m)) {
        result.status = result.status == McoStatus_Ok ? McoStatus_InvalidData : result.status;
        mco_destroy(player);
        return result;
    }
    result.player = player;
    return result;
}

int mco_subsong_count(const McoPlayer* player) {
    return player->module.subsong_count;
}

// Paula

static void play_out(McoVoice* v) {
    v->paula.lc = k_playout_block;
    v->paula.len_words = PLAYOUT_WORDS;
}

// Commands

// The stream of a sub-sequence: the table holds the distance of each sequence from the table.
static uint32_t sequence_start(const McoModule* m, uint8_t index) {
    return relative_target(m->sequences, data_u16(m, m->sequences + index));
}

static bool push_frame(McoVoice* v, McoFrameKind kind, uint32_t position, uint16_t repeats) {
    const int size = kind == McoFrameKind_Loop ? STACK_FRAME_SIZE_LOOP : MCO_STACK_FRAME_SIZE_LONG;
    if (v->stack_bytes + size > MCO_STACK_BYTES) {
        return false;
    }
    v->stack[v->depth++] = (McoFrame) { .kind = kind, .position = position, .repeats = repeats };
    v->stack_bytes += size;
    return true;
}

static void pop_frame(McoVoice* v) {
    v->depth--;
    v->stack_bytes -= v->stack[v->depth].kind == McoFrameKind_Loop ? STACK_FRAME_SIZE_LOOP : MCO_STACK_FRAME_SIZE_LONG;
}

typedef enum StepResult {
    StepResult_Continue, // read the next command
    StepResult_Done,     // the voice is done for this tick
    StepResult_Broken,   // the stream cannot be played on: the voice stops
} StepResult;

// The sample goes to the Paula channel and plays once; then the channel repeats the playout block.
static void start_note(const McoModule* m, McoVoice* v, uint16_t period) {
    uint16_t words = 0;
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = sample_data(m, v->sample, &words);
    v->paula.len_words = words;
    v->paula.volume = (uint8_t)v->volume;
    v->paula.period = period;
    nuade_paula_channel_dma_on(&v->paula);
    play_out(v);
}

// The volume the volume command sets: its byte times 4, scaled by the master volume.
static uint16_t command_volume(const McoModule* m, uint8_t value) {
    const uint16_t raw = (uint16_t)(value * VOLUME_UNIT);
    return (uint16_t)(((uint32_t)raw * m->volume_scale * 2) >> 16);
}

// The end and goto commands call the eagleplayer; when all voices have reported one, the song has ended and the reports
// start over.
static void report_end_command(McoPlayer* p, McoVoice* v) {
    if (!p->module.reports_song_end) {
        return;
    }
    v->reached_end = true;
    for (int i = 0; i < MCO_VOICES; i++) {
        if (!p->voices[i].reached_end) {
            return;
        }
    }
    p->song_ended = true;
    for (int i = 0; i < MCO_VOICES; i++) {
        p->voices[i].reached_end = false;
    }
}

static StepResult step_command(McoPlayer* p, McoVoice* v) {
    const McoModule* m = &p->module;
    const uint8_t code = data_u8(m, v->stream);
    const uint32_t args = v->stream + 1;
    if (code % COMMAND_ENTRY_SIZE != 0 || code >= McoCommand_Count * COMMAND_ENTRY_SIZE) {
        return StepResult_Broken;
    }
    switch ((McoCommand)(code / COMMAND_ENTRY_SIZE)) {
        case McoCommand_Note: {
            const uint16_t period = period_at(m, v->transpose + data_u8(m, args));
            v->count = data_u8(m, args + 1);
            v->stream = args + 2;
            v->shown_note = period != 0 ? nuade_period_to_note(period) : -1;
            start_note(m, v, period);
            return StepResult_Done;
        }
        case McoCommand_Retune: {
            const int at = v->transpose + data_u8(m, args);
            v->count = data_u8(m, args + 1);
            v->stream = args + 2;
            v->paula.period = period_at(m, at);
            if (m->retune_sets_volume) {
                v->paula.volume = (uint8_t)period_at(m, at + 2);
            }
            return StepResult_Done;
        }
        case McoCommand_Instrument:
            v->sample = m->samples + data_u32(m, m->samples + data_u8(m, args));
            v->stream = args + 1;
            v->shown_instrument = data_u8(m, args) / SAMPLE_ENTRY_SIZE;
            return StepResult_Continue;
        case McoCommand_Volume:
            v->volume = command_volume(m, data_u8(m, args));
            v->paula.volume = (uint8_t)v->volume;
            v->stream = args + 1;
            return StepResult_Continue;
        case McoCommand_Rest:
            v->count = (int8_t)data_u8(m, args);
            v->stream = args + 1;
            return StepResult_Done;
        case McoCommand_RestSilent:
            v->count = data_u8(m, args);
            v->stream = args + 1;
            nuade_paula_channel_dma_off(&v->paula);
            return StepResult_Done;
        case McoCommand_End:
            report_end_command(p, v);
            v->count = COUNT_STOPPED;
            v->stream = args;
            nuade_paula_channel_dma_off(&v->paula);
            return StepResult_Done;
        case McoCommand_Call:
            if (!push_frame(v, McoFrameKind_Return, args + 1, 0)) {
                return StepResult_Broken;
            }
            v->stream = sequence_start(m, data_u8(m, args));
            return StepResult_Continue;
        case McoCommand_Return:
            if (v->depth == 0 || v->stack[v->depth - 1].kind == McoFrameKind_Loop) {
                return StepResult_Broken;
            }
            v->stream = v->stack[v->depth - 1].position;
            pop_frame(v);
            v->transpose = 0;
            return StepResult_Continue;
        case McoCommand_LoopBegin:
            v->stream = args + 1;
            return push_frame(v, McoFrameKind_Loop, v->stream, data_u8(m, args)) ? StepResult_Continue
                                                                                 : StepResult_Broken;
        case McoCommand_LoopEnd: {
            if (v->depth == 0 || v->stack[v->depth - 1].kind != McoFrameKind_Loop) {
                return StepResult_Broken;
            }
            McoFrame* frame = &v->stack[v->depth - 1];
            if (--frame->repeats == 0) {
                pop_frame(v);
                v->stream = args;
            } else {
                v->stream = frame->position;
            }
            return StepResult_Continue;
        }
        case McoCommand_CallTransposed:
            if (!push_frame(v, McoFrameKind_Return, args + 2, 0)) {
                return StepResult_Broken;
            }
            v->transpose = (int8_t)data_u8(m, args);
            v->stream = sequence_start(m, data_u8(m, args + 1));
            return StepResult_Continue;
        case McoCommand_Mark:
            v->stream = args;
            return push_frame(v, McoFrameKind_Mark, args, 0) ? StepResult_Continue : StepResult_Broken;
        case McoCommand_Goto:
            report_end_command(p, v);
            if (v->depth == 0 || v->stack[v->depth - 1].kind == McoFrameKind_Loop) {
                return StepResult_Broken;
            }
            v->stream = v->stack[v->depth - 1].position;
            return StepResult_Continue;
        case McoCommand_Count:
            break;
    }
    return StepResult_Broken;
}

// The tick

static void run_voice(McoPlayer* p, McoVoice* v) {
    if (v->count < 0) {
        return;
    }
    v->count--;
    if (v->count != 0) {
        return;
    }
    StepResult result = StepResult_Continue;
    for (int steps = 0; result == StepResult_Continue; steps++) {
        result = steps < COMMANDS_PER_TICK ? step_command(p, v) : StepResult_Broken;
    }
    if (result == StepResult_Broken) {
        v->count = COUNT_STOPPED;
    }
}

McoTickResult mco_tick(McoPlayer* player, NuadeVoices* voices) {
    bool running = false;
    player->song_ended = false;
    for (int i = 0; i < MCO_VOICES; i++) {
        run_voice(player, &player->voices[i]);
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
        running = running || player->voices[i].count >= 0;
    }
    return (McoTickResult) { .looped = player->song_ended, .ended = !running };
}

bool mco_start(McoPlayer* player, int subsong, NuadeVoices* voices) {
    const McoModule* m = &player->module;
    if (subsong < 0 || subsong >= m->subsong_count) {
        return false;
    }
    for (int i = 0; i < MCO_VOICES; i++) {
        const uint32_t entry = SUBSONG_TABLE + (uint32_t)subsong * SUBSONG_RECORD_SIZE + VOICE_ENTRY_SIZE * (uint32_t)i;
        player->voices[i] = (McoVoice) { .count = 1,
                                         .stream = data_u32(m, entry),
                                         .volume = m->start_volume[i],
                                         .shown_note = -1,
                                         .shown_instrument = -1 };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

McoVoiceState mco_voice_state(const McoPlayer* player, int voice) {
    const McoVoice* v = &player->voices[voice];
    return (McoVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}
