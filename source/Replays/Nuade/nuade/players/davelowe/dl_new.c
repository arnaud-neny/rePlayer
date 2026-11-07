// SPDX-License-Identifier: MIT
// Dave Lowe player: the ".dln" modules of the "Dave Lowe New" eagleplayer.
//
// A module starts with a header word (4 or 8) and a word with the size of the header; the subsong table follows at the
// offset in the first word. Two engines exist, told apart by a long in the header (non-zero: the first engine, Beneath
// a Steel Sky and King's Quest 6; zero: the second, with a tempo skip, the Street Fighter 2 songs):
//   first engine:  16 bytes per subsong, the four list offsets
//   second engine: 32 bytes per subsong, a tick limit and the four list offsets (and 12 bytes the engine ignores)

#include "dl_internal.h"
#include "nuade_utils.h"
#include <string.h>

#define HEADER_ENGINE_FLAG_AT 0x18
#define HEADER_MARKER_AT 0x38
#define HEADER_MIN_SIZE 0x20

#define FIRST_ENGINE_ENTRY_SIZE 16
#define SECOND_ENGINE_ENTRY_SIZE 32
// The second engine counts its subsongs from the size word, which includes this many bytes before the table
#define SECOND_ENGINE_TABLE_BIAS 8

// The eagleplayer patches one module: the one with these sample records, subsongs and marker
#define PATCHED_RECORDS 18
#define PATCHED_SUBSONGS 5
#define PATCHED_MARKER 0x74

// The header word values
#define HEADER_TABLE_FIRST 4
#define HEADER_TABLE_SECOND 8

typedef struct DlNewHeader {
    uint16_t table; // offset of the subsong table: 4 or 8
    uint16_t size;
    uint32_t engine_flag;
    uint32_t marker;
} DlNewHeader;

static DlNewHeader read_header(const DlImage* image) {
    DlCursor cursor = { .image = image, .address = 0 };
    DlNewHeader h;
    h.table = dlc_u16(&cursor);
    h.size = dlc_u16(&cursor);
    h.engine_flag = dli_u32(image, HEADER_ENGINE_FLAG_AT);
    h.marker = dli_u32(image, HEADER_MARKER_AT);
    return h;
}

// A list offset: a long whose high word is 0 and whose low word is positive, non-zero and even
static bool is_list_offset(const DlImage* image, uint32_t at) {
    const uint16_t low = dli_u16(image, at + 2);
    return dli_u16(image, at) == 0 && low != 0 && (low & 0x8001) == 0;
}

// The modules' sample records all have the looping state
static bool is_looping_record(const DlImage* image, uint32_t record) {
    return dl_read_sample_record(image, record).state == DlSampleState_Looping;
}

// The first pattern of a list starts with an envelope and a sample command (after an optional filter command): the
// eagleplayer finds the first sample record there. Returns its address, or 0 if the pattern starts otherwise.
static uint32_t sample_record_of_pattern(const DlImage* image, uint32_t pattern) {
    DlCursor cursor = { .image = image, .address = pattern };
    if (dli_u16(image, cursor.address) == DlCommandWord_Filter) {
        cursor.address += 2;
    }
    if (dlc_u16(&cursor) != DlCommandWord_Envelope) {
        return 0;
    }
    dlc_u32(&cursor); // the envelope pointer
    if (dlc_u16(&cursor) != DlCommandWord_Sample) {
        return 0;
    }
    return dlc_u32(&cursor);
}

// The eagleplayer's check walks the lists of the first subsong; if none starts with an envelope and a sample command it
// goes on reading the longs after them as list offsets. Fails on the first malformed list.
static bool find_first_sample_record(const DlImage* image, uint32_t lists, uint32_t* record) {
    for (uint32_t entry = 0; entry < image->module_size / 4; entry++) {
        const uint32_t list = dli_u32(image, lists + entry * 4);
        if (!is_list_offset(image, list)) {
            return false;
        }
        *record = sample_record_of_pattern(image, dli_u32(image, list));
        if (*record != 0) {
            return true;
        }
    }
    return false;
}

// The sample records of a module follow each other (looping state). The table starts at the lowest record of the run
// that holds the one the check found; a record after a word other than the next pattern command (the end of a pattern)
// and 0 is not part of it.
static uint32_t first_record_of_table(const DlImage* image, uint32_t found) {
    uint32_t start;
    uint32_t at = found;
    do {
        start = at;
        at -= DL_SAMPLE_RECORD_SIZE;
    } while (is_looping_record(image, at));
    const uint16_t before = dli_u16(image, start - 2);
    return before == DlCommandWord_NextPattern || before == 0 ? start : start + DL_SAMPLE_RECORD_SIZE;
}

static uint32_t count_sample_records(const DlImage* image, uint32_t first) {
    uint32_t count = 0;
    for (uint32_t at = first; is_looping_record(image, at) && at < image->module_size; at += DL_SAMPLE_RECORD_SIZE) {
        count++;
    }
    return count;
}

// The patched module (Beneath a Steel Sky) gets its fifth subsong to end: an end marker in the pattern lists of three
// voices, each followed by a long 1
static void shorten_lists(DlImage* image) {
    static const uint32_t s_list_ends[] = { 0x7c, 0xbc, 0x11c };
    for (uint32_t i = 0; i < nuade_sizeof_array(s_list_ends); i++) {
        dli_set32(image, s_list_ends[i], 0);
        dli_set32(image, s_list_ends[i] + 4, 1);
    }
}

// Where the list offsets of the first subsong start
static uint32_t first_lists(const DlNewHeader* header) {
    return header->table == HEADER_TABLE_FIRST && header->engine_flag != 0 ? HEADER_TABLE_FIRST : HEADER_TABLE_SECOND;
}

// Subsongs of the first engine: the table is a run of longs below 0x10000 that are even and not 0, four per subsong
static uint32_t count_first_engine_subsongs(const DlImage* image, uint32_t table) {
    uint32_t entries = 0;
    for (uint32_t at = table; at < image->module_size; at += 4) {
        const uint16_t low = dli_u16(image, at + 2);
        if (dli_u16(image, at) != 0 || low == 0 || (low & 1) != 0) {
            break;
        }
        entries++;
    }
    return entries / DL_VOICES;
}

bool dl_new_locate(DlImage* image, DlNewModule* module) {
    const DlNewHeader header = read_header(image);
    if (image->module_size < HEADER_MIN_SIZE
        || (header.table != HEADER_TABLE_FIRST && header.table != HEADER_TABLE_SECOND)) {
        return false;
    }
    const uint32_t lists = first_lists(&header);
    for (uint32_t i = 0; i < DL_VOICES; i++) {
        if (!is_list_offset(image, lists + i * 4)) {
            return false;
        }
    }
    uint32_t record = 0;
    if (!find_first_sample_record(image, lists, &record) || !is_looping_record(image, record)) {
        return false;
    }
    module->engine_a = header.engine_flag != 0;
    module->table = header.table;
    module->first_sample_record = first_record_of_table(image, record);
    if (module->engine_a) {
        module->subsong_count = count_first_engine_subsongs(image, module->table);
    } else {
        module->subsong_count = header.size >= SECOND_ENGINE_TABLE_BIAS
                                    ? (header.size - SECOND_ENGINE_TABLE_BIAS) / SECOND_ENGINE_ENTRY_SIZE
                                    : 0;
    }
    if (count_sample_records(image, module->first_sample_record) == PATCHED_RECORDS
        && module->subsong_count == PATCHED_SUBSONGS && header.marker == PATCHED_MARKER) {
        shorten_lists(image);
    }
    return module->subsong_count != 0;
}

DlSubsong dl_new_subsong(const DlImage* image, const DlNewModule* module, uint32_t index) {
    DlSubsong subsong = { 0 };
    DlCursor cursor = { .image = image,
                        .address = module->table
                                   + index * (module->engine_a ? FIRST_ENGINE_ENTRY_SIZE : SECOND_ENGINE_ENTRY_SIZE) };
    if (!module->engine_a) {
        subsong.tick_limit = dlc_u32(&cursor);
    }
    for (uint32_t v = 0; v < DL_VOICES; v++) {
        subsong.lists[v] = dlc_u32(&cursor);
    }
    return subsong;
}

DlVariant dl_new_variant(const DlNewModule* module) {
    static const DlCommand s_commands[]
        = { DlCommand_Freeze,  DlCommand_Sample,   DlCommand_NextPattern, DlCommand_Envelope, DlCommand_Slide,
            DlCommand_Vibrato, DlCommand_SlideOff, DlCommand_VibratoOff,  DlCommand_Rest,     DlCommand_Stop,
            DlCommand_Stop,    DlCommand_Filter,   DlCommand_Filter,      DlCommand_BonusOn,  DlCommand_Jump };
    DlVariant variant = { .command_count = nuade_sizeof_array(s_commands),
                          .tick_skip = !module->engine_a,
                          .envelope_header_words = 2,
                          .vibrato_delay = true };
    memcpy(variant.commands, s_commands, sizeof(s_commands));
    return variant;
}
