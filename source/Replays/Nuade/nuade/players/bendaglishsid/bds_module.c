// SPDX-License-Identifier: MIT
// Ben Daglish game sound driver: the module check, the hunk loader and the table locating. The eagleplayer only checks
// the header; the tables are found here by the instructions that address them, so an unknown driver is refused rather
// than guessed. "xxxx" in a pattern is any word.

#include "bds_internal.h"
#include <stdlib.h>
#include <string.h>

#define HUNK_HEADER 0x3F3
#define HUNK_CODE 0x3E9
#define HUNK_DATA 0x3EA
#define HUNK_RELOC32 0x3EC
#define HUNK_DEBUG 0x3F1
#define HUNK_END 0x3F2
#define HUNK_TYPE_MASK 0x3FFFFFFFu // the top bits of a hunk's type and size are memory flags
#define HUNK_FLAGS_SHIFT 24
#define HUNK_HEADER_LONGS 8

// The driver starts with `moveq #-1,d0; rts`, "DAGLISH!" and the entry table of BdsHeader.
#define MAGIC_MOVEQ_RTS 0x70FF4E75
#define MAGIC_DAGL 0x4441474C
#define MAGIC_ISH 0x49534821
#define MAGIC_LONGS 3
#define HEADER_ENTRIES 7

#define MAX_HUNK_WORDS 0x400000u
#define MAX_PATTERN_WORDS 20
#define NOT_FOUND 0xFFFFFFFFu
#define SEARCH_PLAY 0xC00 // the tick routine, with its subroutines, is shorter than this
#define SEARCH_INIT 0x100
#define SEARCH_IRQ 0x200

uint8_t bds_u8(const BdsMemory* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

uint16_t bds_u16(const BdsMemory* m, uint32_t offset) {
    return (uint16_t)(bds_u8(m, offset) << 8 | bds_u8(m, offset + 1));
}

uint32_t bds_u32(const BdsMemory* m, uint32_t offset) {
    return (uint32_t)bds_u16(m, offset) << 16 | bds_u16(m, offset + 2);
}

void bds_put_u8(BdsMemory* m, uint32_t offset, uint8_t value) {
    if (offset < m->size) {
        m->data[offset] = value;
    }
}

BdsInstrument bds_read_instrument(const BdsMemory* m, uint32_t offset) {
    const uint8_t flags = bds_u8(m, offset + 2);
    const uint8_t envelope = bds_u8(m, offset + 3);
    const uint8_t sustain = bds_u8(m, offset + 4);
    return (BdsInstrument) { .wave_flags = flags,
                             .muted = (flags & 0x80) != 0,
                             .is_sample = (flags & 0x80) != 0,
                             .sample_index = (flags & 0x7E) >> 1,
                             .attack_nibble = envelope >> 4,
                             .decay_nibble = envelope & 0xF,
                             .sustain_level = sustain >> 4,
                             .release_nibble = sustain & 0xF,
                             .gate_ticks = bds_u8(m, offset + 5),
                             .vibrato = bds_u8(m, offset + 7),
                             .volume_max = bds_u8(m, offset + 8) };
}

BdsSample bds_read_sample(const BdsMemory* m, uint32_t offset) {
    return (
        BdsSample) { .lc = bds_u32(m, offset), .len_words = bds_u16(m, offset + 4), .period = bds_u16(m, offset + 6) };
}

// The start of the executable: the hunk header of AmigaDOS, then the first hunk's type, size and data.

typedef struct BdsHunkHeader {
    uint32_t magic;
    uint32_t library_names; // 0: no resident libraries
    uint32_t hunk_count;
    uint32_t first;
    uint32_t last;
    uint32_t hunk_size; // in longs, the top byte holds memory flags
    uint32_t type;
    uint32_t data_longs; // in the file
} BdsHunkHeader;

static BdsHunkHeader read_hunk_header(const uint8_t* data) {
    return (BdsHunkHeader) { .magic = nuade_read_be32(data),
                             .library_names = nuade_read_be32(data + 4),
                             .hunk_count = nuade_read_be32(data + 8),
                             .first = nuade_read_be32(data + 12),
                             .last = nuade_read_be32(data + 16),
                             .hunk_size = nuade_read_be32(data + 20),
                             .type = nuade_read_be32(data + 24),
                             .data_longs = nuade_read_be32(data + 28) };
}

// The hunk of the executable, as AmigaDOS loads it. Relocation is not needed: the offsets in the hunk are the driver's
// addresses, so the hunk is kept at base 0. The eagleplayer's check (Check2) also wants the memory flags of the hunk
// to be set.

static bool load_hunk(BdsModule* module, const uint8_t* data, size_t size) {
    if (size < 4 * HUNK_HEADER_LONGS) {
        return false;
    }
    const BdsHunkHeader header = read_hunk_header(data);
    const uint32_t hunk_words = header.hunk_size & HUNK_TYPE_MASK;
    const uint32_t type = header.type & HUNK_TYPE_MASK;
    const size_t data_offset = 4 * HUNK_HEADER_LONGS;
    if (header.magic != HUNK_HEADER || header.library_names != 0 || header.hunk_count != 1 || header.first != 0
        || header.last != 0 || (header.hunk_size >> HUNK_FLAGS_SHIFT) == 0 || (type != HUNK_CODE && type != HUNK_DATA)
        || hunk_words > MAX_HUNK_WORDS || header.data_longs > hunk_words
        || data_offset + 4 * (size_t)header.data_longs + 4 > size) {
        return false;
    }
    const uint32_t end_type = nuade_read_be32(data + data_offset + 4 * (size_t)header.data_longs);
    if (end_type != HUNK_RELOC32 && end_type != HUNK_DEBUG && end_type != HUNK_END) {
        return false;
    }
    BdsMemory* m = &module->memory;
    m->size = hunk_words * 4;
    m->data = nuade_paula_padded_copy(nullptr, m->size);
    module->pristine = malloc(m->size != 0 ? m->size : 1);
    if (m->data == nullptr || module->pristine == nullptr) {
        return false;
    }
    memcpy(m->data, data + data_offset, 4 * (size_t)header.data_longs);
    memcpy(module->pristine, m->data, m->size);
    return true;
}

// Pattern scan

static int parse_pattern(const char* pattern, uint16_t* words, bool* any) {
    int count = 0;
    while (*pattern != '\0' && count < MAX_PATTERN_WORDS) {
        if (*pattern == ' ') {
            pattern++;
            continue;
        }
        char digits[5] = { pattern[0], pattern[1], pattern[2], pattern[3], '\0' };
        any[count] = strcmp(digits, "xxxx") == 0;
        words[count] = any[count] ? 0 : (uint16_t)strtoul(digits, nullptr, 16);
        pattern += 4;
        count++;
    }
    return count;
}

// The offset of the first match of `pattern` in [from, to), or NOT_FOUND.

static uint32_t find_pattern(const BdsMemory* m, uint32_t from, uint32_t to, const char* pattern) {
    uint16_t words[MAX_PATTERN_WORDS];
    bool any[MAX_PATTERN_WORDS];
    const uint32_t count = (uint32_t)parse_pattern(pattern, words, any);
    for (uint32_t at = from; at + 2 * count <= m->size && at < to; at += 2) {
        uint32_t i = 0;
        while (i < count && (any[i] || bds_u16(m, at + 2 * i) == words[i])) {
            i++;
        }
        if (i == count) {
            return at;
        }
    }
    return NOT_FOUND;
}

// Locating. Each table is found by the instruction that addresses it, searched in the routine that uses it. An operand
// is an absolute address or the address that a d16(pc) operand refers to; word `word` of the match holds it.

typedef enum BdsOperand {
    BdsOperand_Absolute = 0,
    BdsOperand_PcRelative,
} BdsOperand;

// The part of the hunk a pattern is searched in: one routine of the driver.
typedef struct BdsSpan {
    uint32_t start;
    uint32_t end;
} BdsSpan;

static bool pattern_at(const BdsMemory* m, uint32_t at, const char* pattern) {
    return find_pattern(m, at, at + 2, pattern) == at;
}

static uint32_t operand_value(const BdsMemory* m, uint32_t match, BdsOperand kind, uint32_t word) {
    const uint32_t at = match + 2 * word;
    return kind == BdsOperand_Absolute ? bds_u32(m, at) : at + (uint32_t)(int32_t)(int16_t)bds_u16(m, at);
}

// The operand of the first match of `pattern` in `span`. False if there is none.

static bool find_operand(const BdsMemory* m, BdsSpan span, const char* pattern, BdsOperand kind, uint32_t word,
                         uint32_t* out) {
    const uint32_t match = find_pattern(m, span.start, span.end, pattern);
    if (match == NOT_FOUND) {
        return false;
    }
    *out = operand_value(m, match, kind, word);
    return true;
}

// The same, for a table that a second instruction addresses as well: both must give the same address.

static bool operand_agrees(const BdsMemory* m, BdsSpan span, const char* pattern, BdsOperand kind, uint32_t word,
                           uint32_t expected) {
    uint32_t value;
    return find_operand(m, span, pattern, kind, word, &value) && value == expected;
}

static bool has_magic(const BdsMemory* m) {
    return bds_u32(m, 0) == MAGIC_MOVEQ_RTS && bds_u32(m, 4) == MAGIC_DAGL && bds_u32(m, 8) == MAGIC_ISH;
}

static BdsHeader read_header(const BdsMemory* m) {
    uint32_t entry[HEADER_ENTRIES];
    for (uint32_t i = 0; i < HEADER_ENTRIES; i++) {
        entry[i] = bds_u32(m, 4 * (MAGIC_LONGS + i));
    }
    return (BdsHeader) { .play = entry[0],
                         .audio_irq = entry[1],
                         .init = entry[2],
                         .subsongs = entry[3],
                         .title = entry[4],
                         .author = entry[5],
                         .notice = entry[6] };
}

// The eagleplayer wants the entry points and the subsong count non-zero (Check2); the tick, the interrupt and the
// start routine must lie in the hunk.

static bool header_checked(const BdsHeader* h, uint32_t size) {
    return h->play != 0 && h->audio_irq != 0 && h->init != 0 && h->subsongs != 0 && h->play < size
           && h->audio_irq < size && h->init < size;
}

// The routines the patterns are searched in

typedef struct BdsRoutines {
    BdsSpan play;
    BdsSpan init;
    BdsSpan irq;
} BdsRoutines;

static BdsRoutines routines_of(const BdsHeader* h) {
    return (BdsRoutines) { .play = { h->play, h->play + SEARCH_PLAY },
                           .init = { h->init, h->init + SEARCH_INIT },
                           .irq = { h->audio_irq, h->audio_irq + SEARCH_IRQ } };
}

// The tables both revisions have: the wave forms and the instruments, with the tick's reads of them.

static bool locate_tables(const BdsMemory* m, const BdsRoutines* r, BdsTables* t) {
    const BdsSpan play = r->play;
    const BdsSpan init = r->init;
    const uint32_t instrument_use = find_pattern(m, play.start, play.end, "d047 41fa xxxx 1030 0000 45fa xxxx d4c0");
    if (instrument_use == NOT_FOUND) {
        return false;
    }
    t->instrument_map = operand_value(m, instrument_use, BdsOperand_PcRelative, 2);
    t->instruments = operand_value(m, instrument_use, BdsOperand_PcRelative, 6);

    return find_operand(m, play, "41fa xxxx 2670 0000 298b 1000", BdsOperand_PcRelative, 1, &t->tracks)
           && find_operand(m, play, "082a 0004 0002 xxxx 2d7c xxxx xxxx 0008", BdsOperand_Absolute, 5,
                           &t->wave_tables[0])
           && find_operand(m, play, "082a 0005 0002 xxxx 2d7c xxxx xxxx 0008", BdsOperand_Absolute, 5,
                           &t->wave_tables[1])
           && find_operand(m, play, "082a 0006 0002 xxxx 2d7c xxxx xxxx 0008", BdsOperand_Absolute, 5,
                           &t->wave_tables[2])
           && find_operand(m, play, "43f9 xxxx xxxx 3d71 0000 000c", BdsOperand_Absolute, 1, &t->wave_lengths)
           && find_operand(m, play, "41fa xxxx 3230 1000 4240", BdsOperand_PcRelative, 1, &t->periods)
           && find_operand(m, play, "e848 41fa xxxx 1030 0000 6b06", BdsOperand_PcRelative, 2, &t->attack_rates)
           && find_operand(m, play, "0240 000f xxxx 41fa xxxx 1030 0000 6b06", BdsOperand_PcRelative, 4,
                           &t->decay_rates)
           && operand_agrees(m, play, "0240 000f xxxx 41f9 xxxx xxxx 1030 0000 6b06", BdsOperand_Absolute, 4,
                             t->decay_rates)
           && find_operand(m, play, "41fa xxxx 1d70 0000 xxxx", BdsOperand_PcRelative, 1, &t->vibratos)
           && operand_agrees(m, play, "41fa xxxx 1030 0000 1200 1940", BdsOperand_PcRelative, 1, t->vibratos)
           && find_operand(m, play, "41fa xxxx 1030 0000 0240 0078", BdsOperand_PcRelative, 1, &t->arpeggios)
           && operand_agrees(m, play, "41f9 xxxx xxxx d0c0", BdsOperand_Absolute, 1, t->arpeggios)
           && operand_agrees(m, play, "41fa xxxx 1230 0000 0241", BdsOperand_PcRelative, 1, t->arpeggios)
           && find_operand(m, init, "297c xxxx xxxx 0000 297c", BdsOperand_Absolute, 1, &t->default_track)
           && find_operand(m, init, "203c xxxx xxxx 23fc 0000 0000", BdsOperand_Absolute, 1, &t->idle_wave_table)
           && operand_agrees(m, init, "43f9 xxxx xxxx 45f9 xxxx xxxx 303c 0017", BdsOperand_Absolute, 1,
                             t->instrument_map)
           && find_operand(m, init, "43f9 xxxx xxxx 45f9 xxxx xxxx 303c 0017", BdsOperand_Absolute, 4,
                           &t->instrument_map_default);
}

// The early driver (Terramex): fixed order lists, the volume set by the interrupt from the envelope alone. The first
// instruction of the tick and the init set it apart.

static bool locate_early(const BdsMemory* m, const BdsRoutines* r, BdsTables* t) {
    const BdsSpan play = r->play;
    const BdsSpan irq = r->irq;
    return pattern_at(m, play.start, "49fa xxxx 43fa xxxx 45fa xxxx 47fa xxxx 1029 0017")
           && pattern_at(m, r->init.start, "49fa xxxx 41fa xxxx 303c 00b9")
           && find_pattern(m, play.start, play.end, "4240 102e 0014 522e 0014 1035 0000 0c00 00ff 6606 584f 6000 xxxx")
                  != NOT_FOUND
           && find_operand(m, play, "3e3c 0000 4bf9 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, &t->order_lists[0])
           && find_operand(m, play, "3e3c 0008 4bf9 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, &t->order_lists[1])
           && find_operand(m, play, "3e3c 0010 4bf9 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, &t->order_lists[2])
           && find_operand(m, play, "2e3c xxxx xxxx 2829 0004", BdsOperand_Absolute, 1, &t->silence_lc)
           && operand_agrees(m, play, "0ca9 xxxx xxxx 0000", BdsOperand_Absolute, 1, t->silence_lc)
           && find_operand(m, irq, "4a39 xxxx xxxx 6602 e359", BdsOperand_Absolute, 1, &t->volume_unscaled)
           && find_operand(m, irq, "4a79 xxxx xxxx 6602 4241", BdsOperand_Absolute, 1, &t->volume_enable);
}

// The late driver (Chubby Gristle, The Flintstones): order lists per subsong, sample instruments, a volume cap.

static bool locate_late(const BdsMemory* m, const BdsRoutines* r, BdsTables* t) {
    static const char* init_pattern
        = "0280 0000 ffff c0fc 000c 49fa xxxx d9c0 23dc xxxx xxxx 23dc xxxx xxxx 23dc xxxx xxxx";
    const BdsSpan play = r->play;
    const BdsSpan irq = r->irq;
    const BdsSpan init = r->init;
    return pattern_at(m, play.start, "49fa xxxx 43fa xxxx 45fa xxxx 47fa xxxx 1029 001b")
           && find_pattern(m, play.start, play.end,
                           "4240 102e 0018 522e 0018 1035 0000 0c00 00ff 660a 584f 4239 xxxx xxxx 4e75")
                  != NOT_FOUND
           && pattern_at(m, init.start, init_pattern)
           && find_operand(m, init, init_pattern, BdsOperand_PcRelative, 6, &t->subsong_orders)
           && find_operand(m, init, init_pattern, BdsOperand_Absolute, 9, &t->order_lists[0])
           && find_operand(m, init, init_pattern, BdsOperand_Absolute, 12, &t->order_lists[1])
           && find_operand(m, init, init_pattern, BdsOperand_Absolute, 15, &t->order_lists[2])
           && operand_agrees(m, play, "3e3c 0000 2a79 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, t->order_lists[0])
           && operand_agrees(m, play, "3e3c 0008 2a79 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, t->order_lists[1])
           && operand_agrees(m, play, "3e3c 0010 2a79 xxxx xxxx 4dfa", BdsOperand_Absolute, 3, t->order_lists[2])
           && find_operand(m, play, "41f9 xxxx xxxx 7200 122a 0002 0201 007e", BdsOperand_Absolute, 1, &t->sample_table)
           && find_operand(m, play, "1439 xxxx xxxx 4a69 000e", BdsOperand_Absolute, 1, &t->volume_cap)
           && operand_agrees(m, irq, "1439 xxxx xxxx 41f9 00df f000", BdsOperand_Absolute, 1, t->volume_cap)
           && find_operand(m, play, "4a39 xxxx xxxx 6604 e55a e559", BdsOperand_Absolute, 1, &t->volume_unscaled)
           && operand_agrees(m, irq, "4a39 xxxx xxxx 6604 e35a e359", BdsOperand_Absolute, 1, t->volume_unscaled)
           && find_operand(m, play, "4a39 xxxx xxxx xxxx 4241", BdsOperand_Absolute, 1, &t->volume_enable)
           && operand_agrees(m, irq, "4a39 xxxx xxxx xxxx 4241", BdsOperand_Absolute, 1, t->volume_enable);
}

// The revision is the one whose patterns all exist.

static bool locate(BdsModule* module) {
    const BdsRoutines routines = routines_of(&module->header);
    const BdsMemory* m = &module->memory;
    memset(&module->tables, 0, sizeof(module->tables));
    if (!locate_tables(m, &routines, &module->tables)) {
        return false;
    }
    BdsTables common = module->tables;
    if (locate_early(m, &routines, &module->tables)) {
        module->revision = BdsRevision_Early;
        return true;
    }
    module->tables = common;
    if (locate_late(m, &routines, &module->tables)) {
        module->revision = BdsRevision_Late;
        return true;
    }
    return false;
}

// Every table the revision uses must lie inside the hunk. The subsong count is a byte for the driver's init.

static bool tables_inside(const BdsModule* module) {
    const BdsTables* t = &module->tables;
    const uint32_t size = module->memory.size;
    const bool late = module->revision == BdsRevision_Late;
    const uint32_t tables[] = { t->tracks,       t->wave_tables[0], t->wave_tables[1],  t->wave_tables[2],
                                t->wave_lengths, t->periods,        t->instrument_map,  t->instrument_map_default,
                                t->instruments,  t->attack_rates,   t->decay_rates,     t->vibratos,
                                t->arpeggios,    t->default_track,  t->idle_wave_table, t->volume_unscaled,
                                t->volume_enable };
    for (size_t i = 0; i < nuade_sizeof_array(tables); i++) {
        if (tables[i] >= size) {
            return false;
        }
    }
    if (late) {
        return t->subsong_orders < size && t->sample_table < size && t->volume_cap < size && t->order_lists[0] < size
               && t->order_lists[1] < size && t->order_lists[2] < size && module->header.subsongs <= BDS_MAX_SUBSONGS;
    }
    return t->silence_lc < size && t->order_lists[0] < size && t->order_lists[1] < size && t->order_lists[2] < size;
}

bool bds_module_load(BdsModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (load_hunk(module, data, size)) {
        module->header = read_header(&module->memory);
        if (has_magic(&module->memory) && header_checked(&module->header, module->memory.size) && locate(module)
            && tables_inside(module)) {
            return true;
        }
    }
    bds_module_free(module);
    return false;
}

// The eagleplayer's check (Check2), and that the tables can be found

bool bds_probe(const uint8_t* data, size_t size) {
    BdsModule module;
    const bool loaded = bds_module_load(&module, data, size);
    if (loaded) {
        bds_module_free(&module);
    }
    return loaded;
}

void bds_module_free(BdsModule* module) {
    free(module->memory.data);
    free(module->pristine);
    module->memory.data = nullptr;
    module->pristine = nullptr;
}
