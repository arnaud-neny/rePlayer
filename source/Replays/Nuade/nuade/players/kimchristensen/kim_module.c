// SPDX-License-Identifier: MIT
// Kim Christensen: the eagleplayer's Check2 and InitPlayer, the period table and the typed readers of the song
// records.

#include "kim_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"

// Check2 needs this many bytes.
#define KIM_MIN_SIZE 0x708u

#define KIM_NOTE_OFFSET_NONE 0x18   // the pitch stage byte that leaves the note as it is
#define KIM_VIBRATO_DEPTH_NONE 0x63 // the vibrato byte for no movement
#define KIM_OPCODE_BYTES 2u         // the word the scans look for
#define KIM_OPERAND_BYTES 4u        // the long behind it
#define KIM_FIRST_SCAN_WORDS 100u
#define KIM_CHAIN_MISSES 800u

#define KIM_HEADER_DEFAULT 0x02000077u // speed, unused, first and last position when the data has none
#define KIM_PATTERN_BYTES 0x84u
#define KIM_PATTERN_HEADER_BYTES 4u
#define KIM_TRACK_BYTES 0x20u
#define KIM_INSTRUMENT_BYTES 0x20u
#define KIM_SAMPLE_ENTRY_BYTES 6u
#define KIM_SILENCE_BYTES 4u
#define KIM_MAX_SAMPLE_ENTRIES 0xFFFFu

// The words the scans find, in the order they are found. The operand of a movea/addi/ori follows its word; the asl.w
// is preceded by the instruction that loads the address of the game that the data was linked at.
typedef enum KimScan {
    KimScan_OrderList,   // movea.l #order list,a0
    KimScan_Patterns,    // addi.l #patterns
    KimScan_LinkMark,    // asl.w #1,d1, after a long that holds an address near the data's end
    KimScan_Instruments, // movea.l #instruments,a1
    KimScan_SampleTable, // addi.l #sample table
    KimScan_LinkPoint,   // ori.l #,d7: the address in front of its word is the one before the asl.w
    KimScan_Count,
} KimScan;

static const uint16_t k_scan_words[KimScan_Count] = {
    0x207C, 0x0680, 0xE341, 0x227C, 0x0680, 0x0087,
};

// The replayer's period table, notes 0 to KIM_LAST_NOTE.
static const uint16_t k_periods[KIM_LAST_NOTE + 1] = {
    0x000, 0x087, 0x08F, 0x097, 0x0A0, 0x0AA, 0x0B4, 0x0BE, 0x0CA, 0x0D6, 0x0E2, 0x0F0, 0x0FE, 0x10D, 0x11D,
    0x12E, 0x140, 0x153, 0x168, 0x17D, 0x194, 0x1AC, 0x1C5, 0x1E0, 0x1FC, 0x21A, 0x23A, 0x25C, 0x280, 0x2A8,
    0x2D0, 0x30E, 0x328, 0x358, 0x38A, 0x3C0, 0x3F8, 0x434, 0x474, 0x4B8, 0x500, 0x550, 0x5A0,
};

bool kim_period(uint8_t note, uint16_t* out) {
    if (note > KIM_LAST_NOTE) {
        return false;
    }
    *out = k_periods[note];
    return true;
}

// `count` bytes at a 32-bit offset, inside the data.

static const uint8_t* bytes_at(const KimModule* module, uint32_t offset, uint32_t count) {
    if ((uint64_t)offset + count > module->size) {
        return nullptr;
    }
    return module->data + offset;
}

bool kim_range_valid(const KimModule* module, uint32_t offset, uint32_t words) {
    const uint64_t bytes = words == 0 ? 0x20000u : (uint64_t)words * 2u;
    return (uint64_t)offset + bytes <= module->size;
}

bool kim_order_byte(const KimModule* module, uint32_t index, uint8_t* out) {
    const uint8_t* p = bytes_at(module, module->order_list + index, 1);
    if (p == nullptr) {
        return false;
    }
    *out = p[0];
    return true;
}

// A cell is two bytes, 16 rows per voice, four voices per pattern after a four byte header.

bool kim_pattern_cell(const KimModule* module, uint8_t pattern, uint32_t voice, uint32_t row, KimCell* out) {
    const uint32_t offset = module->patterns + (uint32_t)pattern * KIM_PATTERN_BYTES + KIM_PATTERN_HEADER_BYTES
                            + voice * KIM_TRACK_BYTES + row * 2u;
    const uint8_t* p = bytes_at(module, offset, 2);
    if (p == nullptr) {
        return false;
    }
    *out = (KimCell) { .note = p[0], .instrument = p[1] };
    return true;
}

bool kim_instrument(const KimModule* module, uint8_t number, KimInstrument* out) {
    const uint8_t* p
        = bytes_at(module, module->instruments + (uint32_t)number * KIM_INSTRUMENT_BYTES, KIM_INSTRUMENT_BYTES);
    if (p == nullptr) {
        return false;
    }
    KimInstrument instrument = { 0 };
    for (uint32_t i = 0; i < KIM_VOLUME_STAGES; i++) {
        instrument.volume[i] = (KimVolumeStage) { .target = p[i * 2], .ticks = p[i * 2 + 1] };
    }
    for (uint32_t i = 0; i < KIM_PITCH_STAGES; i++) {
        instrument.pitch[i] = (KimPitchStage) { .hold = p[8 + i * 3],
                                                .ramp = p[9 + i * 3],
                                                .note_offset = (int8_t)(p[10 + i * 3] - KIM_NOTE_OFFSET_NONE) };
    }
    instrument.vibrato_delay = p[0x0E];
    instrument.vibrato_rate = p[0x0F];
    instrument.vibrato_step = (int16_t)(p[0x10] - KIM_VIBRATO_DEPTH_NONE);
    instrument.loops = p[0x12] != 0;
    instrument.sample = p[0x13];
    instrument.start = nuade_read_be32(p + 0x14);
    instrument.length_words = nuade_read_be16(p + 0x18);
    instrument.loop_start = nuade_read_be32(p + 0x1A);
    instrument.loop_words = nuade_read_be16(p + 0x1E);
    *out = instrument;
    return true;
}

bool kim_sample_offset(const KimModule* module, uint8_t number, uint32_t* out) {
    const uint8_t* p = bytes_at(module, module->sample_table + (uint32_t)number * KIM_SAMPLE_ENTRY_BYTES, 4);
    if (p == nullptr) {
        return false;
    }
    *out = nuade_read_be32(p) - module->origin;
    return true;
}

// Check2: the words of `k_scan_words`, each found by reading on from the one before. The first is searched in the
// first KIM_FIRST_SCAN_WORDS words, the others share a budget of KIM_CHAIN_MISSES words that are not the one looked
// for.

static bool check2_scan(const uint8_t* data, size_t size) {
    uint32_t pos = 0;
    uint32_t misses = 0;
    for (uint32_t i = 0; i < KimScan_Count; i++) {
        const uint32_t budget = i == 0 ? KIM_FIRST_SCAN_WORDS : KIM_CHAIN_MISSES;
        uint32_t spent = i == 0 ? 0 : misses;
        for (;;) {
            if ((uint64_t)pos + 2 > size) {
                return false;
            }
            const uint16_t word = nuade_read_be16(data + pos);
            pos += 2;
            if (word == k_scan_words[i]) {
                break;
            }
            if (++spent >= budget) {
                return false;
            }
        }
        if (i != 0) {
            misses = spent;
        }
    }
    return true;
}

// The long at `offset`, false outside the data.

static bool read_long(const uint8_t* data, size_t size, uint32_t offset, uint32_t* out) {
    if ((uint64_t)offset + 4 > size) {
        return false;
    }
    *out = nuade_read_be32(data + offset);
    return true;
}

// InitPlayer's scan: the same words, but it searches on behind the operand of the instruction it took an operand from
// (all but the asl.w and the last), and it has no budget; a search that runs out of the data refuses the song.
// `at[i]` gets the offset of the word that matched.

static bool init_scan(const uint8_t* data, size_t size, uint32_t at[KimScan_Count]) {
    static const bool k_takes_operand[KimScan_Count] = { true, true, false, true, true, false };
    uint32_t pos = 0;
    for (uint32_t i = 0; i < KimScan_Count; i++) {
        while ((uint64_t)pos + 2 <= size && nuade_read_be16(data + pos) != k_scan_words[i]) {
            pos += 2;
        }
        if ((uint64_t)pos + 2 > size) {
            return false;
        }
        at[i] = pos;
        pos += KIM_OPCODE_BYTES + (k_takes_operand[i] ? KIM_OPERAND_BYTES : 0);
    }
    return true;
}

// InitPlayer's sample table check: the table runs up to the first sample, the samples must end inside the data.
// `table_address` is the table's address in the game.

static bool check_sample_table(const KimModule* module, uint32_t table_address) {
    uint32_t first_address = 0;
    if (!read_long(module->data, module->size, module->sample_table, &first_address) || (int32_t)first_address <= 0) {
        return false;
    }
    const uint32_t entries = (first_address - table_address) / KIM_SAMPLE_ENTRY_BYTES;
    if (entries == 0 || entries > KIM_MAX_SAMPLE_ENTRIES) {
        return false;
    }
    uint32_t end = 0;
    for (uint32_t i = 0; i < entries; i++) {
        const uint8_t* entry
            = bytes_at(module, module->sample_table + i * KIM_SAMPLE_ENTRY_BYTES, KIM_SAMPLE_ENTRY_BYTES);
        if (entry == nullptr) {
            return false;
        }
        const uint32_t address = nuade_read_be32(entry);
        if (!((int32_t)end > (int32_t)address)) {
            end = address + nuade_read_be16(entry + 4);
        }
    }
    const uint32_t first_offset = first_address - module->origin;
    const uint32_t end_offset = end - first_address + first_offset;
    return !((int32_t)end_offset > (int32_t)module->size);
}

bool kim_module_parse(KimModule* module, const uint8_t* data, size_t size) {
    *module = (KimModule) { .data = data, .size = size };
    uint32_t at[KimScan_Count];
    if (size < KIM_MIN_SIZE || size > UINT32_MAX / 2 || !check2_scan(data, size) || !init_scan(data, size, at)) {
        return false;
    }
    uint32_t order_list = 0;
    uint32_t patterns = 0;
    uint32_t link_mark = 0;
    uint32_t instruments = 0;
    uint32_t sample_table = 0;
    // The operand follows its opcode; the address that the asl.w works on is loaded by the instruction before it.
    if (!read_long(data, size, at[KimScan_OrderList] + 2, &order_list)
        || !read_long(data, size, at[KimScan_Patterns] + 2, &patterns)
        || !read_long(data, size, at[KimScan_LinkMark] - 4, &link_mark)
        || !read_long(data, size, at[KimScan_Instruments] + 2, &instruments)
        || !read_long(data, size, at[KimScan_SampleTable] + 2, &sample_table)) {
        return false;
    }
    module->origin = link_mark - (at[KimScan_LinkPoint] - 2);
    module->order_list = order_list - module->origin;
    module->patterns = patterns - module->origin;
    module->instruments = instruments - module->origin;
    module->sample_table = sample_table - module->origin;
    module->silence = module->order_list - 8;

    if (bytes_at(module, module->silence, KIM_SILENCE_BYTES) == nullptr) {
        return false;
    }
    uint32_t header = 0;
    if (!read_long(data, size, module->order_list - 4, &header)) {
        return false;
    }
    if (header == 0) {
        header = KIM_HEADER_DEFAULT;
    }
    module->speed = (uint8_t)(header >> 24);
    module->first_position = (uint8_t)(header >> 8);
    module->last_position = (uint8_t)header;
    return check_sample_table(module, sample_table);
}
