// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player: the module. A file is Hippel's "TFMX" header and the song data behind it, with the
// replayer of the game in front of it in most files (the eagleplayer has its own replayer and finds the header through
// the game's code). The header holds the sizes of the tables that follow it: frequency sequences, volume sequences
// (instruments), patterns, track positions, subsongs, sample records and the sample data.

#include "h7v_internal.h"

#include "nuade_utils.h"

#include <string.h>

#define TFMX_MAGIC 0x54464d58
#define HEADER_SIZE 0x20
#define OPCODE_BRA_W 0x6000
#define OPCODE_MOVE_W_D1_A0 0x3081 // move.w d1,(a0)
#define OPCODE_LEA_PC_A0 0x41fa    // lea d16(pc),a0: the displacement of this lea leads to the header
#define SEARCH_WORDS 11            // how far into the replayer the module check looks for the pair
#define ENTRY_SIZE 20              // the game's entry code in front of the replayer's `bra.w` in some files

#define FSEQ_SIZE 64
#define VSEQ_SIZE 64
#define SUBSONG_SIZE 8
#define SAMPLE_RECORD_SIZE 0x1e
#define MAX_MODULE_SIZE 0x10000000

// Reads of a file that has not been accepted yet: one past the end fails the check.

typedef struct Raw {
    const uint8_t* data;
    size_t size;
    bool bad;
} Raw;

static uint16_t raw_word(Raw* r, uint64_t at) {
    if (at + 2 > r->size) {
        r->bad = true;
        return 0;
    }
    return nuade_read_be16(r->data + at);
}

static uint32_t raw_long(Raw* r, uint64_t at) {
    if (at + 4 > r->size) {
        r->bad = true;
        return 0;
    }
    return nuade_read_be32(r->data + at);
}

// The entry code some games put in front of the replayer: moveq #1,d0; moveq #20,d1; bra.b +14; move.w #$f,$dff096;
// moveq #0,d0; moveq #0,d1; nop. The replayer's `bra.w` follows it at offset 20. Check2 looks at offset 0 only, so
// it refuses such a file; the port takes this exact entry and nothing else.

static const uint8_t ENTRY_CODE[ENTRY_SIZE] = { 0x70, 0x01, 0x72, 0x14, 0x60, 0x0e, 0x33, 0xfc, 0x00, 0x0f,
                                                0x00, 0xdf, 0xf0, 0x96, 0x70, 0x00, 0x72, 0x00, 0x4e, 0x71 };

// The eagleplayer's module check (DTP_Check2) up to the header: a file that starts with `bra.w` has the replayer in
// front, whose code holds `move.w d1,(a0)` + `lea d16(pc),a0` within the first words; that displacement leads to the
// header. Any other file has to start with the header. `base` is where the replayer starts (0, or ENTRY_SIZE behind the
// entry code).

static bool check_find_header(Raw* r, uint64_t base, uint64_t* header) {
    if (raw_word(r, base) != OPCODE_BRA_W) {
        *header = 0;
        return base == 0 && !r->bad;
    }
    const uint16_t branch = raw_word(r, base + 2);
    if (r->bad || branch == 0 || (branch & 0x8000) || (branch & 1)) {
        return false;
    }
    uint64_t at = base + 2 + (uint64_t)branch;
    for (int i = 0; i < SEARCH_WORDS; i++, at += 2) {
        if (raw_long(r, at) == ((uint32_t)OPCODE_MOVE_W_D1_A0 << 16 | OPCODE_LEA_PC_A0)) {
            break;
        }
        if (i == SEARCH_WORDS - 1) {
            return false;
        }
    }
    at += 4;
    const uint16_t displacement = raw_word(r, at);
    if (r->bad || displacement == 0 || (displacement & 0x8000) || (displacement & 1)) {
        return false;
    }
    *header = at + displacement;
    return true;
}

// The eagleplayer's init finds the header on its own way: a "TFMX" file starts with it, any other file has it at the
// displacement of the first `lea` that follows a `move.w d1,(a0)` word anywhere in the file.

static bool init_find_header(Raw* r, uint64_t* header) {
    if (raw_long(r, 0) == TFMX_MAGIC) {
        *header = 0;
        return true;
    }
    r->bad = false;
    for (uint64_t at = 0; at + 6 <= r->size; at += 2) {
        if (raw_word(r, at) == OPCODE_MOVE_W_D1_A0) {
            const uint64_t displacement_at = at + 4;
            const int16_t displacement = (int16_t)raw_word(r, displacement_at);
            const int64_t target = (int64_t)displacement_at + displacement;
            if (target < 0) {
                return false;
            }
            *header = (uint64_t)target;
            return true;
        }
    }
    return false;
}

// The "TFMX" header as the file holds it (counts are one less than the number of records).

typedef struct TfmxHeader {
    uint32_t magic;
    uint16_t fseq_last;
    uint16_t vseq_last;
    uint16_t pattern_last;
    uint16_t track_last;
    uint16_t pattern_size;
    uint16_t subsong_last; // the last subsong number
    uint16_t sample_count;
} TfmxHeader;

static TfmxHeader read_header(Raw* r, uint64_t at) {
    return (TfmxHeader) { .magic = raw_long(r, at),
                          .fseq_last = raw_word(r, at + 4),
                          .vseq_last = raw_word(r, at + 6),
                          .pattern_last = raw_word(r, at + 8),
                          .track_last = raw_word(r, at + 0xa),
                          .pattern_size = raw_word(r, at + 0xc),
                          .subsong_last = raw_word(r, at + 0x10),
                          .sample_count = raw_word(r, at + 0x12) };
}

// The rest of the module check: the arithmetic over the header's counts has to land on the first sample record, whose
// start is 0 and whose length is not 0, and the second sample has to start where the first ends.

static bool check_header(Raw* r, uint64_t header) {
    const TfmxHeader h = read_header(r, header);
    if (h.magic != TFMX_MAGIC || h.fseq_last > 0xff || r->bad) {
        return false;
    }
    const uint64_t sequences = (uint64_t)((2 + h.fseq_last + h.vseq_last) & 0xffff) << 6;
    const uint64_t patterns = (uint64_t)((1 + h.pattern_last) & 0xffff) * h.pattern_size;
    const uint64_t tracks = (uint64_t)((1 + h.track_last) & 0xffff) * H7V_TRACK_ROW_SIZE;
    const uint64_t subsongs = (uint64_t)((1 + h.subsong_last) & 0xffff) * SUBSONG_SIZE;
    // The first sample record's start field is at the end of the tables plus 0x12
    const uint64_t first = header + 0x12 + sequences + patterns + tracks + subsongs + HEADER_SIZE;
    if (raw_long(r, first) != 0) {
        return false;
    }
    const uint32_t length = raw_word(r, first + 4);
    if (length == 0) {
        return false;
    }
    return !r->bad && raw_long(r, first + 4 + 0x1a) == length * 2;
}

// Both ways to the header have to agree, and the header has to pass the check.

static bool find_checked_header(Raw* r, uint64_t* header) {
    uint64_t by_check = 0;
    uint64_t by_init = 0;
    const bool has_entry = r->size >= ENTRY_SIZE && memcmp(r->data, ENTRY_CODE, ENTRY_SIZE) == 0;
    if (r->size < HEADER_SIZE || !check_find_header(r, has_entry ? ENTRY_SIZE : 0, &by_check)
        || !init_find_header(r, &by_init) || by_check != by_init || !check_header(r, by_check)) {
        return false;
    }
    *header = by_check;
    return true;
}

// A table's end from its start and a size the replayer computes in 16 bits and adds sign-extended (adda.w).

static uint32_t add_word(uint32_t base, uint32_t value) {
    return base + (uint32_t)(int32_t)(int16_t)(uint16_t)value;
}

// The layout of an accepted module (the offsets of its tables; `data` is not set). Probe and open both go through here.

static bool module_layout(const uint8_t* data, size_t size, H7vModule* module) {
    *module = (H7vModule) { 0 };
    Raw r = { .data = data, .size = size };
    uint64_t found = 0;
    if (data == nullptr || size > MAX_MODULE_SIZE || !find_checked_header(&r, &found)) {
        return false;
    }
    const uint32_t header = (uint32_t)found;
    const TfmxHeader h = read_header(&r, header);
    // The replayer counts in words: 0xFFFF tables are none
    const uint32_t fseq_count = (h.fseq_last + 1u) & 0xffff;
    const uint32_t vseq_count = (h.vseq_last + 1u) & 0xffff;
    const uint32_t pattern_count = (h.pattern_last + 1u) & 0xffff;
    const uint32_t track_count = (h.track_last + 1u) & 0xffff;
    module->pattern_size = h.pattern_size;
    module->subsong_count = h.subsong_last;
    module->sample_count = h.sample_count;

    // The replayer's init adds the sizes as words, sign-extended
    module->fseqs = header + HEADER_SIZE;
    module->vseqs = add_word(module->fseqs, fseq_count << 6);
    module->patterns = add_word(module->vseqs, vseq_count << 6);
    module->tracks = add_word(module->patterns, pattern_count * module->pattern_size);
    module->subsongs = add_word(module->tracks, track_count * H7V_TRACK_ROW_SIZE);
    module->samples = add_word(module->subsongs, ((uint32_t)module->subsong_count + 1) << 3);
    module->sample_data = add_word(module->samples, module->sample_count * SAMPLE_RECORD_SIZE);
    module->size = (uint32_t)size;
    return module->subsong_count != 0 && module->sample_count != 0 && module->sample_data <= size;
}

bool h7v_probe(const uint8_t* data, size_t size) {
    H7vModule layout;
    return module_layout(data, size, &layout);
}

bool h7v_module_open(H7vModule* module, const uint8_t* data, size_t size) {
    if (!module_layout(data, size, module)) {
        *module = (H7vModule) { 0 };
        return false;
    }
    module->data = nuade_paula_padded_copy(data, size);
    if (module->data == nullptr) {
        *module = (H7vModule) { 0 };
        return false;
    }
    return true;
}

void h7v_module_close(H7vModule* module) {
    free(module->data);
    *module = (H7vModule) { 0 };
}

uint8_t h7v_byte(const H7vModule* module, uint32_t offset) {
    return offset < module->size ? module->data[offset] : 0;
}

uint16_t h7v_word(const H7vModule* module, uint32_t offset) {
    return (uint16_t)(h7v_byte(module, offset) << 8 | h7v_byte(module, offset + 1));
}

uint32_t h7v_long(const H7vModule* module, uint32_t offset) {
    return (uint32_t)h7v_word(module, offset) << 16 | h7v_word(module, offset + 2);
}

H7vTrackEntry h7v_track_entry(const H7vModule* module, uint32_t offset) {
    const uint8_t effect = h7v_byte(module, offset + 3);
    return (H7vTrackEntry) { .pattern = h7v_byte(module, offset),
                             .note_transpose = h7v_byte(module, offset + 1),
                             .instrument_transpose = h7v_byte(module, offset + 2),
                             .effect = effect >> 4,
                             .param = effect & 0x0F,
                             .tempo = (int8_t)effect };
}

H7vMixerSettings h7v_mixer_settings(const H7vModule* module) {
    return (H7vMixerSettings) { .hard_clip = h7v_byte(module, module->tracks + 3) != 0,
                                .tempo_offset = (int8_t)h7v_byte(module, module->tracks + 2) };
}

// Subsong `number` is 1-based, the way the replayer numbers them.

H7vSubsong h7v_subsong(const H7vModule* module, uint32_t number) {
    const uint32_t at = module->subsongs + (number - 1) * SUBSONG_SIZE;
    return (H7vSubsong) { .first = h7v_word(module, at),
                          .last = h7v_word(module, at + 2),
                          .speed = h7v_word(module, at + 6) };
}

H7vSampleRecord h7v_sample_record(const H7vModule* module, uint8_t number) {
    const uint32_t at = module->samples + (uint32_t)number * SAMPLE_RECORD_SIZE;
    return (H7vSampleRecord) { .start = h7v_long(module, at + 0x12),
                               .length = h7v_word(module, at + 0x16),
                               .loop_offset = h7v_word(module, at + 0x1a),
                               .loop_length = h7v_word(module, at + 0x1c) };
}

uint32_t h7v_sample_offset(const H7vModule* module, uint64_t start) {
    const uint64_t offset = module->sample_data + start;
    return offset > UINT32_MAX ? UINT32_MAX : (uint32_t)offset;
}

H7vSampleRange h7v_sample_range(const H7vModule* module, uint64_t offset, uint16_t len_words) {
    static const _Alignas(2) int8_t silence[2] = { 0, 0 };
    if (offset <= module->size) {
        return (H7vSampleRange) { .lc = (const int8_t*)module->data + offset, .len_words = len_words };
    }
    return (H7vSampleRange) { .lc = silence, .len_words = sizeof(silence) / 2 };
}

void h7v_patch_second_byte(H7vModule* module, uint32_t offset) {
    if (offset < module->size - 1) {
        module->data[offset + 1] = module->data[offset];
    }
}
