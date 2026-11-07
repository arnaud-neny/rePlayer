// SPDX-License-Identifier: MIT
// The eagleplayer's checks, the hunk loader and the identification of the replayer build (see spl_module.h).

#include "spl_module.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define HUNK_HEADER 0x3F3
#define HUNK_CODE 0x3E9
#define HUNK_RELOC32 0x3EC
#define HUNK_END 0x3F2
#define HUNK_SIZE_MASK 0x3FFFFFFFu
#define HUNK_FLAGS_MASK 0xC0000000u

// The code hunk starts with `moveq #-1,d0; rts`, the identification, and the header the eagleplayer reads.
#define CODE_START_WORDS 0x70FF4E75u
#define IDENTIFICATION_AT 4
#define HEADER_PLAY_AT 0x0C
#define HEADER_INIT_AT 0x10
#define HEADER_SUBSONGS_AT 0x14
#define HEADER_TIMER_AT 0x34
#define HEADER_BYTES (HEADER_TIMER_AT + 4)
static const char s_identification[8] = { '!', 'S', 'O', 'P', 'R', 'O', 'L', '!' };

#define MAX_RELOCATIONS 4096

#define FNV_OFFSET 0xCBF29CE484222325ull
#define FNV_PRIME 0x100000001B3ull

// The header of the code hunk: what the eagleplayer reads.
typedef struct SplHeader {
    uint32_t play;     // offset of the replayer's tick routine
    uint32_t init;     // offset of its init routine
    uint32_t subsongs; // number of subsongs
    uint32_t timer;    // CIA timer value of the tick
} SplHeader;

typedef struct Hunk {
    const uint8_t* code;
    uint32_t size;
    const uint8_t* relocations; // offsets, big endian, as in the file
    uint32_t relocation_count;
} Hunk;

static uint64_t hash_bytes(uint64_t hash, const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ data[i]) * FNV_PRIME;
    }
    return hash;
}

static uint64_t hash_u32(uint64_t hash, uint32_t value) {
    const uint8_t bytes[4] = { (uint8_t)(value >> 24), (uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value };
    return hash_bytes(hash, bytes, sizeof(bytes));
}

// The identity of a build: the hunk size, the module up to the end of the replayer's tables and sequence data, and the
// relocations (they decide which longs are addresses).
static uint64_t build_hash(const Hunk* hunk, uint32_t prefix_end) {
    uint64_t hash = hash_u32(FNV_OFFSET, hunk->size);
    hash = hash_bytes(hash, hunk->code, prefix_end);
    hash = hash_u32(hash, hunk->relocation_count);
    return hash_bytes(hash, hunk->relocations, 4ull * hunk->relocation_count);
}

static bool read_u32(const uint8_t* data, size_t size, size_t* at, uint32_t* value) {
    if (*at > size || size - *at < 4) {
        return false;
    }
    *value = nuade_read_be32(data + *at);
    *at += 4;
    return true;
}

// The hunk file as the loader takes it: one code hunk, relocations against hunk 0, then the end of the file.
static bool parse_hunk_file(const uint8_t* data, size_t size, Hunk* hunk) {
    size_t at = 0;
    uint32_t word;
    uint32_t hunk_size;
    if (!read_u32(data, size, &at, &word) || word != HUNK_HEADER) {
        return false;
    }
    // resident names (none), table size 1, first and last hunk 0
    for (int i = 0; i < 4; i++) {
        const uint32_t expected = i == 1 ? 1 : 0;
        if (!read_u32(data, size, &at, &word) || word != expected) {
            return false;
        }
    }
    if (!read_u32(data, size, &at, &hunk_size) || (hunk_size & HUNK_FLAGS_MASK) == 0) {
        return false; // the eagleplayer wants the memory flags of the hunk
    }
    hunk_size &= HUNK_SIZE_MASK;
    uint32_t tag;
    uint32_t code_words;
    if (!read_u32(data, size, &at, &tag) || (tag & HUNK_SIZE_MASK) != HUNK_CODE
        || !read_u32(data, size, &at, &code_words) || code_words != hunk_size || code_words > (UINT32_MAX >> 2)) {
        return false;
    }
    const size_t code_bytes = 4u * (size_t)code_words;
    if (at > size || size - at < code_bytes) {
        return false;
    }
    hunk->code = data + at;
    hunk->size = (uint32_t)code_bytes;
    at += code_bytes;
    hunk->relocations = nullptr;
    hunk->relocation_count = 0;
    if (!read_u32(data, size, &at, &tag)) {
        return false;
    }
    if ((tag & HUNK_SIZE_MASK) == HUNK_RELOC32) {
        uint32_t count;
        uint32_t target;
        if (!read_u32(data, size, &at, &count) || count == 0 || count > MAX_RELOCATIONS
            || !read_u32(data, size, &at, &target) || target != 0 || at > size || size - at < 4ull * count) {
            return false;
        }
        hunk->relocations = data + at;
        hunk->relocation_count = count;
        for (uint32_t i = 0; i < count; i++) {
            const uint32_t offset = nuade_read_be32(hunk->relocations + 4u * i);
            if (offset > hunk->size || hunk->size - offset < 4) {
                return false;
            }
        }
        at += 4u * count;
        if (!read_u32(data, size, &at, &word) || word != 0 || !read_u32(data, size, &at, &tag)) {
            return false;
        }
    }
    return (tag & HUNK_SIZE_MASK) == HUNK_END && at == size;
}

// The header and the eagleplayer's module check: the identification and the three longs it requires to be set.
static bool read_header(const Hunk* hunk, SplHeader* header) {
    const uint8_t* code = hunk->code;
    if (hunk->size < HEADER_BYTES || nuade_read_be32(code) != CODE_START_WORDS
        || memcmp(code + IDENTIFICATION_AT, s_identification, sizeof(s_identification)) != 0) {
        return false;
    }
    header->play = nuade_read_be32(code + HEADER_PLAY_AT);
    header->init = nuade_read_be32(code + HEADER_INIT_AT);
    header->subsongs = nuade_read_be32(code + HEADER_SUBSONGS_AT);
    header->timer = nuade_read_be32(code + HEADER_TIMER_AT);
    return header->play != 0 && header->init != 0 && header->subsongs != 0;
}

static const SplBuild* find_build(const Hunk* hunk) {
    for (uint32_t i = 0; i < spl_build_count; i++) {
        const SplBuild* build = &spl_builds[i];
        if (build->prefix_end <= hunk->size && build_hash(hunk, build->prefix_end) == build->hash) {
            return build;
        }
    }
    return nullptr;
}

SplModule* spl_module_open(const uint8_t* data, size_t size) {
    Hunk hunk;
    SplHeader header;
    if (!parse_hunk_file(data, size, &hunk) || !read_header(&hunk, &header)) {
        return nullptr;
    }
    const SplBuild* build = find_build(&hunk);
    if (build == nullptr) {
        return nullptr;
    }
    if (header.subsongs != build->song_count) {
        return nullptr;
    }
    SplModule* module = (SplModule*)calloc(1, sizeof(SplModule));
    if (module == nullptr) {
        return nullptr;
    }
    module->chip = nuade_paula_padded_copy(hunk.code, hunk.size);
    module->original = (uint8_t*)malloc(hunk.size);
    if (module->chip == nullptr || module->original == nullptr) {
        spl_module_close(module);
        return nullptr;
    }
    for (uint32_t i = 0; i < hunk.relocation_count; i++) {
        uint8_t* slot = module->chip + nuade_read_be32(hunk.relocations + 4u * i);
        const uint32_t address = nuade_read_be32(slot) + build->load_base;
        slot[0] = (uint8_t)(address >> 24);
        slot[1] = (uint8_t)(address >> 16);
        slot[2] = (uint8_t)(address >> 8);
        slot[3] = (uint8_t)address;
    }
    memcpy(module->original, module->chip, hunk.size);
    module->size = hunk.size;
    module->build = build;
    module->subsong_count = header.subsongs;
    module->timer = header.timer;
    return module;
}

void spl_module_close(SplModule* module) {
    if (module != nullptr) {
        free(module->chip);
        free(module->original);
    }
    free(module);
}

void spl_module_reset(SplModule* module) {
    memcpy(module->chip, module->original, module->size);
}

uint8_t spl_module_u8(const SplModule* module, uint32_t at) {
    return at >= module->build->load_base && at - module->build->load_base < module->size
               ? module->chip[at - module->build->load_base]
               : 0;
}

uint16_t spl_module_u16(const SplModule* module, uint32_t at) {
    return (uint16_t)((spl_module_u8(module, at) << 8) | spl_module_u8(module, at + 1));
}

uint32_t spl_module_u32(const SplModule* module, uint32_t at) {
    return ((uint32_t)spl_module_u16(module, at) << 16) | spl_module_u16(module, at + 2);
}

void spl_module_write32(SplModule* module, uint32_t at, uint32_t value) {
    for (uint32_t i = 0; i < 4; i++) {
        const uint32_t byte = at + i;
        if (byte >= module->build->load_base && byte - module->build->load_base < module->size) {
            module->chip[byte - module->build->load_base] = (uint8_t)(value >> (24 - 8 * i));
        }
    }
}

const int8_t* spl_module_chip_pointer(const SplModule* module, uint32_t at) {
    const uint32_t even = at & ~1u;
    return even >= module->build->load_base && even - module->build->load_base <= module->size
               ? (const int8_t*)module->chip + (even - module->build->load_base)
               : nullptr;
}
