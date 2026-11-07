// SPDX-License-Identifier: MIT
// Mark II module: Check2 of the eagleplayer, the replayer versions and the typed readers.

#include "mk2_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"

#define MK2_CODE_LEA_PC 0x41FAu     // lea d16(pc),a0
#define MK2_CODE_ASL_D2_3 0xE742u   // asl.w #3,d2
#define MK2_CODE_ADDA_PC 0xD1FAu    // adda.l d16(pc),a0
#define MK2_CODE_MOVE_W_INS 0x3770u // move.w d8(a0,d2.l),d16(a3)
#define MK2_SAMPLE_SLIDE 0x80
#define MK2_SAMPLE_MASK 0x7F
#define MK2_ROW_TRANSPOSE_MASK 0x1F
#define MK2_ROW_DOWN 0x80
#define MK2_ROW_RISE 0x40
#define MK2_ROW_FALL 0x20
#define MK2_NOTE_SLIDE 0x80
#define MK2_NOTE_MASK 0x7F
#define MK2_STEP_SWAP 0x80
#define MK2_STEP_VOLUME_MASK 0x7F
#define MK2_PARAM_UP 0x80
#define MK2_PARAM_KEEP_DMA 0x01
#define MK2_ENV_CELLS_SIZE 16 // 4 falling and 4 rising counters
#define MK2_SCAN_WORDS 0x120u
#define MK2_ADDA_SCAN_WORDS 0x18u
#define MK2_LEA_BACK_SCAN_WORDS 0x10u
#define MK2_LEA_BACK_START 0x14u

// The versions, named by the first lea operand of the code: 0x35e, 0x352, 0x2b6 (two images that differ in two compare
// instructions shaping only the value the module hands back to the eagleplayer) and 0x2ae. The code is the same in
// every module of a version; the variables, which the code addresses pc-relative, hold the song's data. A fifth
// version (0x1ce) takes a stop request where the eagleplayer sends the tick, so UADE plays it silent: it is not here.
static const Mk2Layout mk2_layouts[] = {
    {
        .code_hash = 0x99FECD64AA8E2BA4ull,
        .code_size = 0x330,
        .slides = true,
        .holds_note = true,
        .masks_sample = true,
        .signals_song_end = true,
        .envelope = 0x2B2,
        .param = 0x330,
        .sample_flags = 0x338,
        .note = 0x340,
        .last_period = 0x378,
        .toggle = 0x376,
        .sequence_offset = 0x38A,
        .sample_offset = 0x38E,
        .song_rows = 0x392,
        .block_size = 0x394,
        .instruments = 0x396,
        .periods = 0x556,
        .data = 0x5BA,
    },
    {
        .code_hash = 0x318302C3E5D5F251ull,
        .code_size = 0x324,
        .slides = true,
        .masks_sample = true,
        .signals_song_end = true,
        .envelope = 0x2A6,
        .param = 0x324,
        .sample_flags = 0x32C,
        .note = 0x334,
        .last_period = 0x36C,
        .toggle = 0x36A,
        .sequence_offset = 0x37E,
        .sample_offset = 0x382,
        .song_rows = 0x386,
        .block_size = 0x388,
        .instruments = 0x38A,
        .periods = 0x54A,
        .data = 0x5AE,
    },
    {
        .code_hash = 0xEF3286FED5ACEF71ull,
        .code_size = 0x2A0,
        .signals_song_end = true,
        .envelope = 0x222,
        .last_period = 0x2D0,
        .toggle = 0x2CE,
        .sequence_offset = 0x2E2,
        .sample_offset = 0x2E6,
        .song_rows = 0x2EA,
        .block_size = 0x2EC,
        .instruments = 0x2EE,
        .periods = 0x4AE,
        .data = 0x512,
    },
    {
        .code_hash = 0xD06229BF65B510EEull,
        .code_size = 0x2A0,
        .signals_song_end = true,
        .envelope = 0x222,
        .last_period = 0x2D0,
        .toggle = 0x2CE,
        .sequence_offset = 0x2E2,
        .sample_offset = 0x2E6,
        .song_rows = 0x2EA,
        .block_size = 0x2EC,
        .instruments = 0x2EE,
        .periods = 0x4AE,
        .data = 0x512,
    },
    {
        .code_hash = 0x3095C288A4A4DE55ull,
        .code_size = 0x2A2,
        .envelope = 0x224,
        .last_period = 0x2C8,
        .toggle = 0x2C6,
        .sequence_offset = 0x2DA,
        .sample_offset = 0x2DE,
        .song_rows = 0x2E2,
        .block_size = 0x2E4,
        .instruments = 0x2E6,
        .periods = 0x4A6,
        .data = 0x50A,
    },
};

static bool read_word(const Mk2Module* module, uint32_t offset, uint16_t* out) {
    if (module->size < 2 || offset > module->size - 2) {
        return false;
    }
    *out = nuade_read_be16(module->data + offset);
    return true;
}

static bool read_long(const Mk2Module* module, uint32_t offset, uint32_t* out) {
    if (module->size < 4 || offset > module->size - 4) {
        return false;
    }
    *out = nuade_read_be32(module->data + offset);
    return true;
}

// Check2 of the eagleplayer: the code starts with "lea d16(pc),a0" at 4, an "asl.w #3,d2" is followed by a "lea
// d16(pc),a0" (the instrument table), and an "adda.l d16(pc),a0" after that (or the instrument move) is preceded by
// another lea within 16 words. The scans are bounded by the module.

static bool check2(const Mk2Module* module) {
    uint16_t w = 0;
    if (!read_word(module, 4, &w) || w != MK2_CODE_LEA_PC) {
        return false;
    }
    // dbra counters: a loop of N+1 words, the second one continues with what is left
    int32_t left = MK2_SCAN_WORDS - 1;
    uint32_t at = 0;
    bool found = false;
    for (; left >= 0 && !found; left--) {
        if (!read_word(module, at, &w)) {
            return false;
        }
        found = w == MK2_CODE_ASL_D2_3;
        at += 2;
    }
    if (!found) {
        return false;
    }
    left++;
    found = false;
    for (; left >= 0 && !found; left--) {
        if (!read_word(module, at, &w)) {
            return false;
        }
        found = w == MK2_CODE_LEA_PC;
        at += 2;
    }
    if (!found) {
        return false;
    }
    // `at` is the displacement word of the lea; the word after it starts the next instruction
    uint16_t next = 0;
    if (!read_word(module, at + 2, &next)) {
        return false;
    }
    uint32_t scan = at;
    int32_t scan_left = MK2_ADDA_SCAN_WORDS - 1;
    found = false;
    for (; scan_left >= 0 && !found; scan_left--) {
        if (!read_word(module, scan, &w)) {
            return false;
        }
        found = w == MK2_CODE_ADDA_PC;
        scan += 2;
    }
    int32_t lea_left = 0;
    uint32_t back = 0;
    if (found) {
        scan_left++;
        back = scan;
        lea_left = scan_left;
    } else {
        if (next != MK2_CODE_MOVE_W_INS) {
            return false;
        }
        back = at + MK2_LEA_BACK_START;
        lea_left = MK2_LEA_BACK_SCAN_WORDS - 1;
    }
    for (; lea_left >= 0; lea_left--) {
        if (back < 2) {
            return false;
        }
        back -= 2;
        if (!read_word(module, back, &w)) {
            return false;
        }
        if (w == MK2_CODE_LEA_PC) {
            return true;
        }
    }
    return false;
}

static uint64_t hash_code(const Mk2Module* module, const Mk2Layout* layout) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (uint32_t i = 0; i < layout->code_size; i++) {
        if (i >= layout->envelope && i < layout->envelope + MK2_ENV_CELLS_SIZE) {
            continue;
        }
        hash = (hash ^ module->data[i]) * 0x100000001B3ull;
    }
    return hash;
}

bool mk2_module_parse(Mk2Module* module, const uint8_t* data, size_t size) {
    *module = (Mk2Module) { .data = data, .size = size };
    if (!check2(module)) {
        return false;
    }
    for (uint32_t i = 0; i < nuade_sizeof_array(mk2_layouts); i++) {
        const Mk2Layout* layout = &mk2_layouts[i];
        // the variables, the song header and the tables the layout names lie inside the module
        if (size < layout->data || hash_code(module, layout) != layout->code_hash) {
            continue;
        }
        module->layout = layout;
        return true;
    }
    return false;
}

static bool read_words(const Mk2Module* module, uint32_t offset, uint16_t* out) {
    for (uint32_t i = 0; i < MK2_VOICES; i++) {
        if (!read_word(module, offset + 2 * i, &out[i])) {
            return false;
        }
    }
    return true;
}

bool mk2_read_initial(const Mk2Module* module, Mk2Initial* out) {
    const Mk2Layout* layout = module->layout;
    uint16_t toggle = 0;
    *out = (Mk2Initial) { 0 };
    if (!read_words(module, layout->envelope, out->envelope_fall)
        || !read_words(module, layout->envelope + 2 * MK2_VOICES, out->envelope_rise)
        || !read_words(module, layout->last_period, out->last_period) || !read_word(module, layout->toggle, &toggle)) {
        return false;
    }
    out->toggle = toggle != 0;
    if (!layout->slides) {
        return true;
    }
    uint16_t sample_bytes[MK2_VOICES];
    if (!read_words(module, layout->sample_flags, sample_bytes) || !read_words(module, layout->param, out->param)
        || !read_words(module, layout->note, out->note)) {
        return false;
    }
    for (uint32_t i = 0; i < MK2_VOICES; i++) {
        out->slide[i] = (sample_bytes[i] & MK2_SAMPLE_SLIDE) != 0;
    }
    return true;
}

bool mk2_read_song(const Mk2Module* module, Mk2Song* out) {
    const Mk2Layout* layout = module->layout;
    uint32_t sequence_offset = 0;
    uint32_t sample_offset = 0;
    *out = (Mk2Song) { .data_start = layout->data };
    if (!read_long(module, layout->sequence_offset, &sequence_offset)
        || !read_long(module, layout->sample_offset, &sample_offset)
        || !read_word(module, layout->song_rows, &out->rows)
        || !read_word(module, layout->block_size, &out->block_size)) {
        return false;
    }
    out->sequence_start = layout->data + sequence_offset;
    out->sample_start = layout->data + sample_offset;
    return true;
}

bool mk2_row_cell(const Mk2Module* module, uint32_t offset, Mk2RowCell* out) {
    if (module->size < 2 || offset > module->size - 2) {
        return false;
    }
    const uint8_t flags = module->data[offset + 1];
    *out = (Mk2RowCell) {
        .sequence = module->data[offset],
        .transpose = flags & MK2_ROW_TRANSPOSE_MASK,
        .transpose_down = (flags & MK2_ROW_DOWN) != 0,
        .volume_rise = (flags & MK2_ROW_RISE) != 0,
        .volume_fall = (flags & MK2_ROW_FALL) != 0,
    };
    return true;
}

bool mk2_step(const Mk2Module* module, uint32_t offset, Mk2Step* out) {
    if (module->size < 4 || offset > module->size - 4) {
        return false;
    }
    const uint8_t* p = module->data + offset;
    const bool masks_sample = module->layout->masks_sample;
    *out = (Mk2Step) {
        .sample = masks_sample ? p[0] & MK2_SAMPLE_MASK : p[0],
        .slide = masks_sample && (p[0] & MK2_SAMPLE_SLIDE),
        .note = p[1],
        .note_slide = (p[1] & MK2_NOTE_SLIDE) != 0,
        .slide_amount = p[1] & MK2_NOTE_MASK,
        .volume = p[2] & MK2_STEP_VOLUME_MASK,
        .swap = (p[2] & MK2_STEP_SWAP) != 0,
        .param = p[3],
        .slide_up = (p[3] & MK2_PARAM_UP) != 0,
        .keep_dma = (p[3] & MK2_PARAM_KEEP_DMA) != 0,
    };
    return true;
}

bool mk2_instrument(const Mk2Module* module, uint32_t sample, Mk2Instrument* out) {
    const uint32_t base = module->layout->instruments + sample * 8u;
    uint16_t length = 0;
    if (!read_long(module, base, &out->offset) || !read_word(module, base + 6, &length)) {
        return false;
    }
    out->length_words = length;
    return true;
}

bool mk2_period(const Mk2Module* module, int16_t byte_offset, uint16_t* out) {
    const int64_t at = (int64_t)module->layout->periods + byte_offset;
    if (at < 0) {
        return false;
    }
    return read_word(module, (uint32_t)at, out);
}
