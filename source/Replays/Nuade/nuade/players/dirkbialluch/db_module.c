// SPDX-License-Identifier: MIT
// Dirk Bialluch player: the song and sample files, see db_internal.h.

#include "db_internal.h"

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG_SONG 0x4D534E47u     // "MSNG"
#define TAG_SEQUENCE 0x4D534551u // "MSEQ"
#define TAG_PATTERNS 0x4D50544Eu // "MPTN"
#define TAG_PROGRAMS 0x4D505247u // "MPRG"
#define TAG_GROOVE 0x4D475256u   // "MGRV"
#define TAG_ARPEGGIO 0x4D415250u // "MARP"
#define TAG_SAMPLES 0x4D534D50u  // "MSMP"

#define TAG_BYTES 4
#define SONG_HEADER_BYTES 4
// Check2 looks for the MSEQ tag at the even offsets up to here.
#define LAST_SEQUENCE_TAG_OFFSET 0x204
#define SUBSONG_TABLE_LAST_WORD_BYTES 2
#define FIRST_SUBSONG_TABLE_BYTES 8 // the song table has at least one word before MSEQ
#define STEP_BYTES 8
#define PROGRAM_HEADER_BYTES 4
// The tables of MPTN and MPRG hold the count + 2 offsets after the count word.
#define TABLE_EXTRA_ENTRIES 2

#define SONG_PREFIX "tpu."
#define SAMPLE_PREFIX "smp."
#define SAMPLE_COUNT_BYTES 2

// Bytes of the files: 0 past the end

static uint8_t song_u8(const DbModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t song_u16(const DbModule* m, uint32_t offset) {
    return (uint16_t)((song_u8(m, offset) << 8) | song_u8(m, offset + 1));
}

static uint32_t song_u32(const DbModule* m, uint32_t offset) {
    return ((uint32_t)song_u16(m, offset) << 16) | song_u16(m, offset + 2);
}

static uint32_t samples_u32(const DbModule* m, uint32_t offset) {
    if (m->samples == nullptr || offset + 4 > m->samples_size || offset + 4 < offset) {
        return 0;
    }
    return nuade_read_be32(m->samples + offset);
}

// Command bytes. The classification the replayer reads from its 256 byte tables.

DbPatternCommand db_pattern_command(uint8_t code) {
    if (code >= 0x01 && code <= 0x30) {
        return DbPatternCommand_Note;
    }
    if (code >= 0x41 && code <= 0x70) {
        return DbPatternCommand_NoteWait;
    }
    switch (code) {
        case 0x80:
            return DbPatternCommand_Wait;
        case 0x81:
            return DbPatternCommand_SetGroove;
        case 0x82:
            return DbPatternCommand_FilterOn;
        case 0x83:
            return DbPatternCommand_FilterOff;
        case 0x90:
            return DbPatternCommand_Slide;
        case 0xA0:
            return DbPatternCommand_MasterVolume;
        case 0xA1:
            return DbPatternCommand_VolumeWait;
        case 0xA2:
            return DbPatternCommand_VolumeAddWait;
        case 0xA3:
            return DbPatternCommand_VolumeSubWait;
        case 0xA4:
            return DbPatternCommand_FadeParameters;
        case 0xB0:
            return DbPatternCommand_Signal;
        case 0xB1:
            return DbPatternCommand_SignalWait;
        case 0xB2:
            return DbPatternCommand_Unused;
        case 0xC0:
            return DbPatternCommand_Arpeggio;
        case 0xD0:
            return DbPatternCommand_FadeToSubsong;
        case 0xF0:
            return DbPatternCommand_Loop;
        case 0xF1:
            return DbPatternCommand_JumpPattern;
        case 0xFC:
            return DbPatternCommand_GotoPosition;
        case 0xFD:
            return DbPatternCommand_Stop;
        case 0xFE:
            return DbPatternCommand_NextStep;
        default:
            return DbPatternCommand_Idle;
    }
}

DbProgramOp db_program_op(uint8_t code) {
    switch (code) {
        case 0x01:
            return DbProgramOp_StartSample;
        case 0x02:
            return DbProgramOp_SampleAdd;
        case 0x03:
            return DbProgramOp_SetLength;
        case 0x04:
            return DbProgramOp_SetSample;
        case 0x05:
            return DbProgramOp_SampleSub;
        case 0x10:
            return DbProgramOp_VolumeSetWait;
        case 0x11:
            return DbProgramOp_VolumeAddWait;
        case 0x12:
            return DbProgramOp_VolumeSubWait;
        case 0x20:
            return DbProgramOp_VolumeSet;
        case 0x21:
            return DbProgramOp_VolumeAdd;
        case 0x22:
            return DbProgramOp_VolumeSub;
        case 0x30:
            return DbProgramOp_BendSubWait;
        case 0x31:
            return DbProgramOp_BendAddWait;
        case 0x40:
            return DbProgramOp_BendSub;
        case 0x41:
            return DbProgramOp_BendAdd;
        case 0x50:
            return DbProgramOp_FixedNote;
        case 0x80:
            return DbProgramOp_Wait;
        case 0xB0:
            return DbProgramOp_JumpOnSignal;
        case 0xC0:
            return DbProgramOp_Arpeggio;
        case 0xF0:
            return DbProgramOp_Loop;
        case 0xFE:
            return DbProgramOp_RequestProgram;
        case 0xFF:
            return DbProgramOp_End;
        default:
            return DbProgramOp_Idle;
    }
}

// Check2: "MSNG", then the MSEQ tag at an even offset up to LAST_SEQUENCE_TAG_OFFSET, every word before it below
// 0x1000. Returns the offset of the tag, 0 if there is none.

static uint32_t find_sequence_tag(const DbModule* m) {
    if (m->size < SONG_HEADER_BYTES || song_u32(m, 0) != TAG_SONG) {
        return 0;
    }
    for (uint32_t offset = SONG_HEADER_BYTES; offset <= LAST_SEQUENCE_TAG_OFFSET; offset += 2) {
        if (offset + TAG_BYTES > m->size) {
            return 0;
        }
        if (song_u32(m, offset) == TAG_SEQUENCE) {
            return offset;
        }
        if ((song_u16(m, offset) & 0xF000) != 0) {
            return 0;
        }
    }
    return 0;
}

// Tags sit at even offsets. 0 when there is none.

static uint32_t find_tag(const DbModule* m, uint32_t from, uint32_t tag) {
    for (uint32_t offset = from; offset + TAG_BYTES <= m->size; offset += 2) {
        if (song_u32(m, offset) == tag) {
            return offset;
        }
    }
    return 0;
}

// The data after the count word and the n + 2 entries of a table of `entry_bytes` bytes. The sum is a word.

static uint32_t table_data_offset(uint32_t table, uint16_t count, uint32_t entry_bytes) {
    const uint16_t bytes = (uint16_t)((count + TABLE_EXTRA_ENTRIES) * entry_bytes);
    return table + bytes;
}

bool db_module_locate(DbModule* m) {
    const uint32_t sequence_tag = find_sequence_tag(m);
    if (sequence_tag < FIRST_SUBSONG_TABLE_BYTES) {
        return false;
    }
    const uint32_t patterns_tag = find_tag(m, sequence_tag, TAG_PATTERNS);
    const uint32_t programs_tag = patterns_tag != 0 ? find_tag(m, patterns_tag, TAG_PROGRAMS) : 0;
    const uint32_t groove_tag = programs_tag != 0 ? find_tag(m, programs_tag, TAG_GROOVE) : 0;
    const uint32_t arpeggio_tag = groove_tag != 0 ? find_tag(m, groove_tag, TAG_ARPEGGIO) : 0;
    if (arpeggio_tag == 0) {
        return false;
    }
    m->song_table = SONG_HEADER_BYTES;
    // SubSongRange: the words between the header and MSEQ, less the last one.
    m->subsong_count = (sequence_tag - SONG_HEADER_BYTES - SUBSONG_TABLE_LAST_WORD_BYTES) / 2;
    m->sequence = sequence_tag + TAG_BYTES;
    const uint32_t pattern_count_offset = patterns_tag + TAG_BYTES;
    m->pattern_table = pattern_count_offset + 2;
    m->pattern_data = table_data_offset(m->pattern_table, song_u16(m, pattern_count_offset), 2);
    const uint32_t program_count_offset = programs_tag + TAG_BYTES;
    m->program_table = program_count_offset + 2;
    m->program_data = table_data_offset(m->program_table, song_u16(m, program_count_offset), 2);
    m->groove = groove_tag + TAG_BYTES;
    m->arpeggio = arpeggio_tag + TAG_BYTES;
    return true;
}

bool db_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > DB_MAX_FILE_BYTES) {
        return false;
    }
    DbModule m = { .data = data, .size = (uint32_t)size };
    return db_module_locate(&m);
}

// ExtLoad: the sample file is the song's file name with its "tpu." prefix changed to "smp." in the song's directory.

static bool sample_file_name(const char* path, char* name, size_t name_cap) {
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || nuade_strncasecmp(base, SONG_PREFIX, strlen(SONG_PREFIX)) != 0
        || snprintf(name, name_cap, SAMPLE_PREFIX "%s", base + strlen(SONG_PREFIX)) >= (int)name_cap) {
        return false;
    }
    return true;
}

static DbStatus load_samples(DbModule* m, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char name[512];
    if (io == nullptr || !sample_file_name(path, name, sizeof(name))) {
        return DbStatus_MissingSamples;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    DbStatus status = DbStatus_MissingSamples;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            if (file.size >= TAG_BYTES + SAMPLE_COUNT_BYTES && file.size <= DB_MAX_FILE_BYTES
                && nuade_read_be32(file.data) == TAG_SAMPLES) {
                uint8_t* copy = nuade_paula_padded_copy(file.data, file.size);
                if (copy == nullptr) {
                    status = DbStatus_OutOfMemory;
                } else {
                    m->samples = copy;
                    m->samples_size = (uint32_t)file.size;
                    m->sample_table = TAG_BYTES + SAMPLE_COUNT_BYTES;
                    m->sample_data
                        = table_data_offset(m->sample_table, nuade_read_be16(file.data + TAG_BYTES), sizeof(uint32_t));
                    status = DbStatus_Ok;
                }
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return status;
}

DbStatus db_module_open(DbModule* m, const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    *m = (DbModule) { 0 };
    if (data == nullptr || size > DB_MAX_FILE_BYTES) {
        return DbStatus_InvalidData;
    }
    uint8_t* copy = (uint8_t*)malloc(size > 0 ? size : 1);
    if (copy == nullptr) {
        return DbStatus_OutOfMemory;
    }
    memcpy(copy, data, size);
    m->data = copy;
    m->size = (uint32_t)size;
    DbStatus status = DbStatus_InvalidData;
    if (db_module_locate(m)) {
        status = load_samples(m, path, io);
    }
    if (status != DbStatus_Ok) {
        db_module_close(m);
    }
    return status;
}

void db_module_close(DbModule* m) {
    free((void*)m->data);
    free((void*)m->samples);
    *m = (DbModule) { 0 };
}

DbSequenceStep db_sequence_step(const DbModule* m, uint16_t position) {
    const uint32_t step = m->sequence + (uint32_t)(uint16_t)(position * STEP_BYTES);
    DbSequenceStep out;
    for (uint32_t v = 0; v < DB_VOICES; v++) {
        out.voice[v].pattern = song_u8(m, step + v * 2);
        out.voice[v].transpose = song_u8(m, step + v * 2 + 1);
    }
    return out;
}

uint16_t db_subsong_position(const DbModule* m, uint8_t subsong) {
    return song_u16(m, m->song_table + (uint32_t)subsong * 2);
}

uint32_t db_pattern_address(const DbModule* m, uint8_t pattern) {
    return m->pattern_data + song_u16(m, m->pattern_table + (uint32_t)pattern * 2);
}

uint32_t db_program_address(const DbModule* m, uint8_t program) {
    return m->program_data + song_u16(m, m->program_table + (uint32_t)program * 2);
}

DbPatternEvent db_pattern_event(const DbModule* m, uint32_t pattern, uint16_t position) {
    const uint32_t at = pattern + position;
    return (DbPatternEvent) { .code = song_u8(m, at), .first = song_u8(m, at + 1), .second = song_u8(m, at + 2) };
}

uint8_t db_program_priority(const DbModule* m, uint32_t program) {
    return song_u8(m, program);
}

DbProgramCommand db_program_command(const DbModule* m, uint32_t program, uint16_t position) {
    const uint32_t at = program + PROGRAM_HEADER_BYTES + position;
    return (DbProgramCommand) {
        .code = song_u8(m, at), .first = song_u8(m, at + 1), .second = song_u8(m, at + 2), .third = song_u8(m, at + 3)
    };
}

uint8_t db_groove_step(const DbModule* m, uint32_t offset) {
    return song_u8(m, m->groove + offset);
}

uint8_t db_arpeggio_step(const DbModule* m, uint8_t index) {
    return song_u8(m, m->arpeggio + index);
}

uint32_t db_sample_address(const DbModule* m, uint8_t index) {
    return DB_SAMPLE_FILE_ADDRESS + m->sample_data + samples_u32(m, m->sample_table + (uint32_t)index * 4);
}

const int8_t* db_sample_lc(const DbModule* m, uint32_t address) {
    if (address < DB_SAMPLE_FILE_ADDRESS || address - DB_SAMPLE_FILE_ADDRESS >= m->samples_size) {
        return nullptr;
    }
    return (const int8_t*)(m->samples + (address - DB_SAMPLE_FILE_ADDRESS));
}
