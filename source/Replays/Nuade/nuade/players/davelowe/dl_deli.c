// SPDX-License-Identifier: MIT
// Dave Lowe player: the Deli modules of the "Dave Lowe Deli" eagleplayer (Darkness / Synergy).
//
// The engine is in the eagleplayer; a module is a hunk executable of one code hunk with the tag "DAVELOWE" after the
// usual first instruction, two version bytes (at most 1 and 0), the number of subsongs (a word) and the subsong table
// (22 bytes per subsong, see DlDeliEntry). The pattern lists, patterns, envelopes and sample records are the ones of
// the game drivers; the engine is the drivers' with a fade-in on the volume, 17 commands (the last two end the song),
// a DMA start at the end of every tick and no sound effects.

#include "dl_internal.h"
#include "nuade_utils.h"
#include <string.h>

#define TAG_DAVE 0x44415645u // "DAVE"
#define TAG_LOWE 0x4c4f5745u // "LOWE"

#define MAX_VERSION_FIRST 1
#define MAX_VERSION_SECOND 0

// The header after the tag, at `start` (the code hunk of an image starts at 0, in the file at the code offset): two
// version bytes and the number of subsongs; the subsong table follows
typedef struct DlDeliHeader {
    uint8_t version_first;
    uint8_t version_second;
    uint16_t subsong_count;
    uint32_t table;
} DlDeliHeader;

// A subsong: four list addresses, four bytes the engine does not use, the speed of the fade-in
typedef struct DlDeliEntry {
    uint32_t lists[DL_VOICES];
    uint16_t fade_speed;
} DlDeliEntry;

#define ENTRY_SIZE 0x16
#define ENTRY_UNUSED_BYTES 4

static DlDeliHeader read_header(const DlImage* image, uint32_t start) {
    DlCursor cursor = { .image = image, .address = start + DL_MODULE_TAG_AT + DL_MODULE_TAG_SIZE };
    DlDeliHeader h;
    const uint16_t versions = dlc_u16(&cursor);
    h.version_first = (uint8_t)(versions >> 8);
    h.version_second = (uint8_t)versions;
    h.subsong_count = dlc_u16(&cursor);
    h.table = cursor.address;
    return h;
}

static DlDeliEntry read_entry(const DlImage* image, uint32_t table, uint32_t index) {
    DlCursor cursor = { .image = image, .address = table + index * ENTRY_SIZE };
    DlDeliEntry e = { 0 };
    for (uint32_t v = 0; v < DL_VOICES; v++) {
        e.lists[v] = dlc_u32(&cursor);
    }
    cursor.address += ENTRY_UNUSED_BYTES;
    e.fade_speed = dlc_u16(&cursor);
    return e;
}

bool dl_deli_extract(const DlImage* file, uint32_t* code_size) {
    DlCursor cursor = { .image = file, .address = DL_HUNK_CODE_OFFSET + DL_MODULE_TAG_AT };
    if (!dl_hunk_code(file, code_size) || dlc_u32(&cursor) != TAG_DAVE || dlc_u32(&cursor) != TAG_LOWE) {
        return false;
    }
    const DlDeliHeader h = read_header(file, DL_HUNK_CODE_OFFSET);
    return h.version_first <= MAX_VERSION_FIRST && h.version_second <= MAX_VERSION_SECOND;
}

bool dl_deli_locate(const DlImage* image, DlDeliModule* module) {
    const DlDeliHeader h = read_header(image, 0);
    module->subsong_count = h.subsong_count;
    module->table = h.table;
    return h.subsong_count != 0 && h.table + (uint64_t)h.subsong_count * ENTRY_SIZE <= image->module_size;
}

DlSubsong dl_deli_subsong(const DlImage* image, const DlDeliModule* module, uint32_t index) {
    const DlDeliEntry e = read_entry(image, module->table, index);
    DlSubsong subsong = { .fade_speed = e.fade_speed };
    for (uint32_t v = 0; v < DL_VOICES; v++) {
        subsong.lists[v] = e.lists[v];
    }
    return subsong;
}

DlVariant dl_deli_variant(void) {
    static const DlCommand s_commands[]
        = { DlCommand_Freeze,  DlCommand_Sample,   DlCommand_NextPattern, DlCommand_Envelope, DlCommand_Slide,
            DlCommand_Vibrato, DlCommand_SlideOff, DlCommand_VibratoOff,  DlCommand_Rest,     DlCommand_Freeze,
            DlCommand_Freeze,  DlCommand_Filter,   DlCommand_Filter,      DlCommand_BonusOn,  DlCommand_Jump,
            DlCommand_SongEnd, DlCommand_SongEnd };
    DlVariant variant = { .command_count = nuade_sizeof_array(s_commands),
                          .fade = true,
                          .dma_every_tick = true,
                          .envelope_adds_volume = true,
                          .envelope_header_words = 2,
                          .vibrato_delay = true };
    memcpy(variant.commands, s_commands, sizeof(s_commands));
    return variant;
}
