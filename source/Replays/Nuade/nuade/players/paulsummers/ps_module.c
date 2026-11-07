// SPDX-License-Identifier: MIT
// Paul Summers: the song image. The eagleplayer's Check2 recognises the game's start-up code, its InitPlayer finds the
// song table, the period table and the subsong count by scanning the code for instruction patterns.

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "ps_internal.h"

#define PS_MIN_SIZE 0xBB8u        // Check2 rejects images up to this size
#define PS_CODE_START 0x28Au      // where both scans start
#define PS_CHECK_WORDS 10u        // words Check2 tries for the first pattern
#define PS_PATCH_MIN_SIZE 0x36B0u // images above this get the word patch below
#define PS_PATCH_AT 0x360Cu
#define PS_PATCH_WORD 0xF87Fu
#define PS_ENTRY_BYTES 20u
#define PS_VOICE_NONE 0x1EF8u

#define PS_OP_MOVE_SR_2700 0x46FC2700u // move #$2700,sr
#define PS_OP_RTE 0x4E73u
#define PS_OP_LEA_PC 0x41FAu // lea d16(pc),a0
#define PS_OP_LEA_PC_A2 0x45FAu
#define PS_OP_MULU_IMM 0xC0FCu // mulu.w #imm,d0
#define PS_OP_NEG_L_D0 0x4480u
#define PS_OP_MOVE_B_IMM_88 0x117C0088u // move.b #$88,d16(a0)
#define PS_SETUP_MOVES 4u

static const int8_t PS_IDLE_SAMPLE[PS_IDLE_WORDS * 2] = { 0 };

const int8_t* ps_idle_sample(void) {
    return PS_IDLE_SAMPLE;
}

bool ps_u8(const PsModule* module, uint32_t at, uint8_t* out) {
    if (at >= module->size) {
        return false;
    }
    *out = module->mem[at];
    return true;
}

bool ps_u16(const PsModule* module, uint32_t at, uint16_t* out) {
    if (module->size < 2 || at > module->size - 2) {
        return false;
    }
    *out = nuade_read_be16(module->mem + at);
    return true;
}

bool ps_u32(const PsModule* module, uint32_t at, uint32_t* out) {
    if (module->size < 4 || at > module->size - 4) {
        return false;
    }
    *out = nuade_read_be32(module->mem + at);
    return true;
}

// The address a signed 16 bit displacement word at `at` leads to.

static bool displacement_target(const PsModule* module, uint32_t at, uint32_t* out) {
    uint16_t displacement = 0;
    if (!ps_u16(module, at, &displacement)) {
        return false;
    }
    *out = at + (uint32_t)(int32_t)(int16_t)displacement;
    return true;
}

// Scans words from `*at` for `opcode`; `*at` ends on the word behind it.

static bool find_word(const PsModule* module, uint32_t* at, uint16_t opcode) {
    uint16_t word = 0;
    while (ps_u16(module, *at, &word)) {
        *at += 2;
        if (word == opcode) {
            return true;
        }
    }
    return false;
}

// Check2, from the lea that loads the image's own address: the image must be addressed from its first byte.

static bool check_start_address(const PsModule* module, uint32_t at) {
    if (!find_word(module, &at, PS_OP_LEA_PC)) {
        return false;
    }
    uint32_t target = 0;
    return displacement_target(module, at, &target) && target == 0;
}

// Check2 after the interrupt prologue (move #$2700,sr ... rte): the first lea behind the rte.

static bool check_after_rte(const PsModule* module, uint32_t at) {
    for (;;) {
        uint32_t next = 0;
        uint16_t word = 0;
        if (!ps_u32(module, at, &next) || next == 0 || !ps_u16(module, at, &word)) {
            return false;
        }
        at += 2;
        if (word != PS_OP_RTE) {
            continue;
        }
        if (!ps_u16(module, at, &word)) {
            return false;
        }
        at += 2;
        if (word == PS_OP_LEA_PC) {
            return check_start_address(module, at);
        }
    }
}

// Check2 after a lea followed by four "move.b #$88,d16(a0)": the same lea scan.

static bool check_after_setup(const PsModule* module, uint32_t at) {
    at += 4;
    for (uint32_t i = 0; i < PS_SETUP_MOVES; i++) {
        uint32_t word = 0;
        if (!ps_u32(module, at, &word) || word != PS_OP_MOVE_B_IMM_88) {
            return false;
        }
        at += 6;
    }
    return check_start_address(module, at);
}

static bool check_image(const PsModule* module) {
    if (module->size <= PS_MIN_SIZE) {
        return false;
    }
    uint32_t at = PS_CODE_START;
    for (uint32_t i = 0; i < PS_CHECK_WORDS; i++, at += 2) {
        uint32_t long_word = 0;
        uint16_t word = 0;
        if (ps_u32(module, at, &long_word) && long_word == PS_OP_MOVE_SR_2700) {
            return check_after_rte(module, at);
        }
        if (ps_u16(module, at, &word) && word == PS_OP_LEA_PC) {
            return check_after_setup(module, at);
        }
    }
    return false;
}

bool ps_song_entry(const PsModule* module, uint32_t subsong, PsSongEntry* out) {
    uint32_t tempo_timer = 0;
    const uint32_t entry = module->song_table + (subsong & 0xFu) * PS_ENTRY_BYTES;
    if (!ps_u32(module, entry + 16, &tempo_timer)) {
        return false;
    }
    *out = (PsSongEntry) { .tempo = (uint8_t)(tempo_timer >> 24),
                           .tempo_init = (uint8_t)(tempo_timer >> 16),
                           .timer = (uint16_t)tempo_timer };
    return true;
}

bool ps_voice_stream(const PsModule* module, uint32_t subsong, uint32_t voice, uint32_t* out) {
    uint32_t offset = 0;
    const uint32_t entry = module->song_table + (subsong & 0xFu) * PS_ENTRY_BYTES;
    if (!ps_u32(module, entry + voice * 4, &offset) || offset == 0 || offset == PS_VOICE_NONE) {
        return false;
    }
    const uint32_t stream = module->song_table + offset;
    if ((int32_t)stream <= 0) {
        return false;
    }
    *out = stream;
    return true;
}

bool ps_range(const PsModule* module, uint32_t at, PsRange* out) {
    uint32_t sample_offset = 0;
    PsRange range = { 0 };
    if (!ps_u32(module, at, &sample_offset) || !ps_u16(module, at + 6, &range.length_words)
        || !ps_u16(module, at + 8, &range.base) || !ps_u16(module, at + 10, &range.low)
        || !ps_u16(module, at + 12, &range.high)) {
        return false;
    }
    range.sample_offset = (int32_t)sample_offset;
    *out = range;
    return true;
}

bool ps_period_pair(const PsModule* module, int32_t note, PsPeriodPair* out) {
    const uint32_t at = module->period_table - ((uint32_t)note << 2);
    return ps_u32(module, at, &out->current) && ps_u32(module, at + 4, &out->next);
}

// InitPlayer: the subsong count and the two tables, found in the code behind the start-up code.

static bool locate_tables(PsModule* module) {
    uint32_t at = PS_CODE_START;
    if (!find_word(module, &at, PS_OP_LEA_PC_A2)) {
        return false;
    }
    uint32_t table_after_songs = 0;
    if (!displacement_target(module, at, &table_after_songs)) {
        return false;
    }
    if (!find_word(module, &at, PS_OP_MULU_IMM)) {
        return false;
    }
    uint16_t song_factor = 0;
    if (!ps_u16(module, at - 8, &song_factor)) {
        return false;
    }
    at += 4;
    const uint32_t song_table_operand = at;
    if (!displacement_target(module, song_table_operand, &module->song_table)) {
        return false;
    }
    // The song table ends where the next table starts. A table that starts before it falls back to the factor.
    uint32_t subsongs = 0;
    const uint32_t table_bytes = table_after_songs - module->song_table;
    if ((int32_t)table_bytes < 0) {
        subsongs = (uint32_t)song_factor - 1u;
    } else {
        if (table_bytes % PS_ENTRY_BYTES == 0 && table_bytes / PS_ENTRY_BYTES <= 0xFFFFu) {
            subsongs = table_bytes / PS_ENTRY_BYTES - 1u;
        }
    }
    if (subsongs < 1 || subsongs > PS_MAX_SUBSONGS) {
        return false;
    }
    module->subsong_count = subsongs;
    at = song_table_operand;
    if (!find_word(module, &at, PS_OP_NEG_L_D0)) {
        return false;
    }
    return displacement_target(module, at + 2, &module->period_table);
}

// Every subsong has to start with a tempo the engine survives: a positive tempo runs its tick body twice and returns
// through a clobbered stack.

static bool tempos_supported(const PsModule* module) {
    for (uint32_t subsong = 1; subsong <= module->subsong_count; subsong++) {
        PsSongEntry entry = { 0 };
        if (!ps_song_entry(module, subsong, &entry) || (entry.tempo != 0 && entry.tempo < 0x80)) {
            return false;
        }
    }
    return true;
}

bool ps_module_parse(PsModule* module, const uint8_t* bytes, size_t size) {
    *module = (PsModule) { 0 };
    if (bytes == nullptr || size <= PS_MIN_SIZE || size > UINT32_MAX / 2) {
        return false;
    }
    module->mem = nuade_paula_padded_copy(bytes, size);
    if (module->mem == nullptr) {
        return false;
    }
    module->size = (uint32_t)size;
    if (size > PS_PATCH_MIN_SIZE && nuade_read_be16(module->mem + PS_PATCH_AT) == PS_PATCH_WORD) {
        module->mem[PS_PATCH_AT] = 0;
        module->mem[PS_PATCH_AT + 1] = 0;
    }
    if (!check_image(module) || !locate_tables(module) || !tempos_supported(module)) {
        ps_module_free(module);
        return false;
    }
    return true;
}

void ps_module_free(PsModule* module) {
    free(module->mem);
    *module = (PsModule) { 0 };
}
