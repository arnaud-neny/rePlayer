// SPDX-License-Identifier: MIT
// EarAche: locating the header (the eagleplayer's Check2), the typed readers of the module records and the bounds
// check that keeps the replayer inside the tables it models.

#include "ea_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>

#define EA_MARK 0x4541534Fu // "EASO"
#define EA_BRANCH_WORD 0x6000u
#define EA_ROWS_OFFSET 0x18u
#define EA_ROW_BYTES 8u
#define EA_POSITION_BYTES 4u
#define EA_INSTRUMENT_BYTES 16u
#define EA_SAMPLE_BYTES 4u
#define EA_TABLE_BYTES 64u
#define EA_HEADER_BYTES 24u
#define EA_MAX_ROW 0xFFFu
#define EA_SPEED_SHIFT 12
#define EA_JUMP_CHAIN_LIMIT 16
#define EA_FETCH_LIMIT 256

// The period table of the eagleplayer, as indexed by note numbers (and note + arpeggio offset): entry 0 is its first
// word (0), entries 60..70 repeat the lowest period. Index 71 and up read the replayer's own variables and code.
static const uint16_t k_periods[EA_PERIOD_COUNT] = {
    0x0000, 0x0D60, 0x0CA0, 0x0BEA, 0x0B40, 0x0AA0, 0x09FC, 0x0976, 0x08EE, 0x086E, 0x07F4, 0x0782,
    0x0716, 0x06B0, 0x0650, 0x05F4, 0x05A0, 0x054E, 0x0502, 0x04BA, 0x0476, 0x0436, 0x03F9, 0x03C0,
    0x038A, 0x0358, 0x0328, 0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C, 0x023A, 0x021A, 0x01FC, 0x01E0,
    0x01C5, 0x01AC, 0x0194, 0x017D, 0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0,
    0x00E2, 0x00D6, 0x00CA, 0x00BE, 0x00B4, 0x00AA, 0x00A0, 0x0097, 0x008F, 0x0087, 0x007F, 0x0078,
    0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071,
};

uint16_t ea_period_entry(uint32_t index) {
    return index < EA_PERIOD_COUNT ? k_periods[index] : 0;
}

// The file bytes at `position`, or null when `count` bytes do not fit.

static const uint8_t* file_at(const uint8_t* bytes, size_t size, uint64_t position, size_t count) {
    return position + count <= size ? bytes + position : nullptr;
}

// The replayer's own checks on a branch displacement: positive and even.

static bool is_branch_offset(int16_t displacement) {
    return displacement > 0 && (displacement & 1) == 0;
}

// A module with its own player starts with two branches; the first leads to a DMACON write and a PC-relative load of
// the header address.

static bool find_header_behind_branches(const uint8_t* bytes, size_t size, size_t* base) {
    const uint8_t* start = file_at(bytes, size, 0, 8);
    if (start == nullptr || nuade_read_be16(start) != EA_BRANCH_WORD || nuade_read_be16(start + 4) != EA_BRANCH_WORD) {
        return false;
    }
    const int16_t first = (int16_t)nuade_read_be16(start + 2);
    const int16_t second = (int16_t)nuade_read_be16(start + 6);
    if (!is_branch_offset(first) || !is_branch_offset(second)) {
        return false;
    }
    const uint64_t target = 2u + (uint64_t)first;
    const uint8_t* code = file_at(bytes, size, target, 12);
    if (code == nullptr || nuade_read_be32(code) != 0x33FC000Fu || nuade_read_be32(code + 4) != 0x00DFF096u
        || nuade_read_be16(code + 8) != 0x43FAu) {
        return false;
    }
    const int16_t header = (int16_t)nuade_read_be16(code + 10);
    if (!is_branch_offset(header)) {
        return false;
    }
    *base = (size_t)(target + 10u + (uint64_t)header);
    return true;
}

static bool find_header(const uint8_t* bytes, size_t size, size_t* base) {
    if (size < 2) {
        return false;
    }
    if (nuade_read_be16(bytes) == 0) {
        *base = 0;
        return true;
    }
    if (size >= 4 && nuade_read_be32(bytes) == EA_MARK) {
        *base = 4;
        return true;
    }
    return find_header_behind_branches(bytes, size, base);
}

// Check2: the first offset is 0x18, the row table is a positive multiple of 8 bytes and the instrument table a positive
// multiple of 16 bytes below the volume tables. Only the low bits of the differences are checked.

static bool read_header(const uint8_t* bytes, size_t size, size_t base, EaHeader* header) {
    const uint8_t* p = file_at(bytes, size, base, EA_HEADER_BYTES);
    if (p == nullptr) {
        return false;
    }
    *header = (EaHeader) { .rows = nuade_read_be32(p),
                           .positions = nuade_read_be32(p + 4),
                           .instruments = nuade_read_be32(p + 8),
                           .samples = nuade_read_be32(p + 12),
                           .volumes = nuade_read_be32(p + 16),
                           .pitches = nuade_read_be32(p + 20) };
    const int32_t row_bytes = (int32_t)(header->positions - header->rows);
    const int32_t instrument_bytes = (int32_t)(header->volumes - header->instruments);
    return header->rows == EA_ROWS_OFFSET && row_bytes > 0 && (row_bytes & 7) == 0 && instrument_bytes > 0
           && (instrument_bytes & 15) == 0;
}

uint8_t ea_byte(const EaModule* module, uint64_t offset) {
    const uint64_t position = (uint64_t)module->base + offset;
    return position < module->size ? module->bytes[position] : 0;
}

static uint16_t module_word(const EaModule* module, uint64_t offset) {
    const uint64_t position = (uint64_t)module->base + offset;
    return position + 2 <= module->size ? nuade_read_be16(module->bytes + position) : 0;
}

EaPositionEntry ea_position_entry(const EaModule* module, uint32_t index) {
    const uint64_t offset = (uint64_t)module->header.positions + (uint64_t)index * EA_POSITION_BYTES;
    return (EaPositionEntry) { .first = module_word(module, offset), .second = module_word(module, offset + 2) };
}

uint8_t ea_entry_speed(EaPositionEntry entry) {
    return (uint8_t)(entry.second >> EA_SPEED_SHIFT);
}

// A jump mark's second word names an entry; when that is a mark too its second word names the next, up to 16 times,
// then the sequencer reads on after the last entry it looked at.

bool ea_pattern_at(const EaModule* module, uint32_t index, EaPattern* pattern) {
    bool jumped = false;
    for (int attempt = 0; attempt < EA_FETCH_LIMIT; attempt++) {
        EaPositionEntry entry = ea_position_entry(module, index);
        index++;
        if (entry.first == EA_JUMP_MARK) {
            jumped = true;
            uint32_t target = entry.second & EA_MAX_ROW;
            bool found = false;
            for (int hop = 0; hop < EA_JUMP_CHAIN_LIMIT; hop++) {
                entry = ea_position_entry(module, target);
                index = target + 1;
                if (entry.first != EA_JUMP_MARK) {
                    found = true;
                    break;
                }
                target = entry.second & EA_MAX_ROW;
            }
            if (!found) {
                continue;
            }
        }
        *pattern = (EaPattern) { .first_row = entry.first & EA_MAX_ROW,
                                 .last_row = entry.second & EA_MAX_ROW,
                                 .speed = ea_entry_speed(entry),
                                 .next_index = index,
                                 .jumped = jumped };
        return true;
    }
    return false;
}

EaCell ea_cell(const EaModule* module, uint32_t row, uint32_t voice) {
    const uint64_t offset = (uint64_t)module->header.rows + (uint64_t)row * EA_ROW_BYTES + (uint64_t)voice * 2u;
    const uint16_t word = module_word(module, offset);
    return (EaCell) { .note = (uint8_t)(word >> 8), .instrument = (uint8_t)word };
}

EaInstrument ea_instrument(const EaModule* module, uint32_t index) {
    const uint64_t start = (uint64_t)module->header.instruments + (uint64_t)index * EA_INSTRUMENT_BYTES;
    uint8_t field[EA_INSTRUMENT_BYTES];
    for (uint32_t i = 0; i < EA_INSTRUMENT_BYTES; i++) {
        field[i] = ea_byte(module, start + i);
    }
    return (EaInstrument) { .sample = field[0] & 0x7Fu,
                            .gate_ticks = field[1],
                            .volume_table = module->header.volumes + (uint32_t)(field[2] & 0x7Fu) * EA_TABLE_BYTES,
                            .arpeggio = (field[3] & 0x80u) != 0,
                            .arpeggio_step = field[3] & 0x7Fu,
                            .pitch_table = module->header.pitches + (uint32_t)(field[3] & 0x7Fu) * EA_TABLE_BYTES,
                            .volume_ticks = field[4],
                            .volume_reload = field[5],
                            .pitch_ticks = field[6],
                            .pitch_reload = field[7],
                            .volume = field[9],
                            .fine_tune = (int8_t)field[10] };
}

// A sample table entry: AUDxLC and AUDxLEN as the replayer writes them.

EaSample ea_sample(const EaModule* module, uint32_t index) {
    const uint64_t entry = (uint64_t)module->header.samples + (uint64_t)index * EA_SAMPLE_BYTES;
    const uint64_t position = (uint64_t)module->base + (uint64_t)module_word(module, entry) * 4u;
    if (entry + EA_SAMPLE_BYTES > module->size - module->base || position >= module->size) {
        return (EaSample) { 0 };
    }
    return (EaSample) { .data = (const int8_t*)(module->bytes + position),
                        .length_words = module_word(module, entry + 2) };
}

// The port plays a sample only when all of it lies inside the file (a length of 0 is 65536 words).

static bool sample_inside_file(const EaModule* module, uint32_t index) {
    const EaSample sample = ea_sample(module, index);
    const uint64_t bytes = (sample.length_words != 0 ? (uint64_t)sample.length_words : 65536u) * 2u;
    return sample.data != nullptr && (uint64_t)((const uint8_t*)sample.data - module->bytes) + bytes <= module->size;
}

// The period table index the note start reads and the highest one the arpeggio can add to it.

static bool cell_is_modelled(const EaModule* module, EaCell cell) {
    const EaInstrument instrument = ea_instrument(module, cell.instrument);
    uint32_t highest = cell.note;
    if (instrument.arpeggio) {
        uint32_t offset = instrument.pitch_ticks;
        offset = instrument.pitch_reload > offset ? instrument.pitch_reload : offset;
        offset = instrument.arpeggio_step > offset ? instrument.arpeggio_step : offset;
        highest += offset;
    }
    return highest < EA_PERIOD_COUNT && sample_inside_file(module, instrument.sample);
}

// The rows of a pattern lie inside the row table and every note in them is modelled.

static bool pattern_is_modelled(const EaModule* module, const EaPattern* pattern) {
    // A pattern whose last row lies before its first plays the first row only.
    const uint32_t end = pattern->last_row > pattern->first_row ? pattern->last_row : pattern->first_row;
    if (end >= module->row_count) {
        return false;
    }
    for (uint32_t row = pattern->first_row; row <= end; row++) {
        for (uint32_t voice = 0; voice < EA_VOICES; voice++) {
            const EaCell cell = ea_cell(module, row, voice);
            if ((cell.note != 0 || cell.instrument != 0) && !cell_is_modelled(module, cell)) {
                return false;
            }
        }
    }
    return true;
}

// Follows the sequencer from the first entry through every pattern it can reach (the table may hold unused entries).

static bool reachable_patterns_are_modelled(const EaModule* module) {
    bool* visited = calloc(module->position_count, sizeof(bool));
    if (visited == nullptr) {
        return false;
    }
    bool modelled = true;
    uint32_t index = 0;
    while (modelled && index < module->position_count && !visited[index]) {
        visited[index] = true;
        EaPattern pattern = { 0 };
        modelled = ea_pattern_at(module, index, &pattern) && pattern_is_modelled(module, &pattern);
        index = pattern.next_index;
    }
    free(visited);
    // The walk must end at an entry it has seen: one past the table would read the instruments as positions.
    return modelled && index < module->position_count;
}

bool ea_module_parse(EaModule* module, const uint8_t* bytes, size_t size) {
    size_t base = 0;
    EaHeader header;
    if (!find_header(bytes, size, &base) || !read_header(bytes, size, base, &header)) {
        return false;
    }
    const size_t available = size - base;
    const uint32_t tables[] = { header.positions, header.instruments, header.samples, header.volumes, header.pitches };
    for (size_t i = 0; i < nuade_sizeof_array(tables); i++) {
        if (tables[i] > available) {
            return false;
        }
    }
    if (header.positions >= header.instruments) {
        return false;
    }
    *module = (EaModule) { .bytes = bytes,
                           .size = size,
                           .base = base,
                           .header = header,
                           .row_count = (header.positions - header.rows) / EA_ROW_BYTES,
                           .position_count = (header.instruments - header.positions) / EA_POSITION_BYTES };
    return module->position_count > 0 && reachable_patterns_are_modelled(module);
}
