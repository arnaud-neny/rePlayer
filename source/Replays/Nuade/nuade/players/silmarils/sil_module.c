// SPDX-License-Identifier: MIT
// Silmarils player: the memory the module lives in, the container check, the block and header readers, the sample
// unpacker and the module patch.

#include "sil_internal.h"

#include <stdlib.h>

// The first words of a file: the form (with or without the 6 byte prefix), five zero words, two width marks
#define SIL_PREFIX 6
#define SIL_FORM_MARK 0x16 // the word at +4 of the form without the prefix
#define SIL_WIDTH_MARK 0x20
#define SIL_FORM_ZERO_WORDS 5
#define SIL_FORM_LENGTH 24    // bytes the form check reads from the base
#define SIL_HEADER_LONG 0xE   // the long that finds the header, from the base
#define SIL_HEADER_FIRST 0x58 // the header starts with this long, and the table follows at this distance
#define SIL_MUSIC_MARK 0xFF02 // the word at +6 of a music block
#define SIL_PACKED_SAMPLE 1   // the byte at +6 of a packed sample block
#define SIL_HEADER_LENGTH 0x12
#define SIL_BLOCK_CHECKED 8 // bytes of a block the check reads

// The deltas of the packed samples: a byte holds two nibbles, each a step of the running value
static const uint8_t s_deltas[16]
    = { 0xDE, 0xEB, 0xF3, 0xF8, 0xFB, 0xFD, 0xFE, 0xFF, 0x01, 0x02, 0x03, 0x05, 0x08, 0x0D, 0x15, 0x22 };

bool sil_memory_init(SilMemory* memory, uint32_t size) {
    memory->data = nuade_paula_padded_copy(nullptr, size);
    memory->size = size;
    return memory->data != nullptr;
}

void sil_memory_free(SilMemory* memory) {
    free(memory->data);
    memory->data = nullptr;
    memory->size = 0;
}

bool sil_in_memory(const SilMemory* memory, uint64_t address, uint64_t length) {
    return address + length <= memory->size;
}

uint8_t sil_u8(const SilMemory* memory, uint32_t address) {
    return address < memory->size ? memory->data[address] : 0;
}

uint16_t sil_u16(const SilMemory* memory, uint32_t address) {
    return sil_in_memory(memory, address, 2) ? nuade_read_be16(memory->data + address) : 0;
}

uint32_t sil_u32(const SilMemory* memory, uint32_t address) {
    return sil_in_memory(memory, address, 4) ? nuade_read_be32(memory->data + address) : 0;
}

void sil_put8(SilMemory* memory, uint32_t address, uint8_t value) {
    if (address < memory->size) {
        memory->data[address] = value;
    }
}

bool sil_block_read(const SilMemory* memory, uint32_t address, SilBlock* block) {
    if (!sil_in_memory(memory, address, SIL_SAMPLE_HEADER)) {
        return false;
    }
    const uint8_t* p = memory->data + address;
    *block = (SilBlock) {
        .type = (SilBlockType)p[0],
        .size = nuade_read_be32(p + 2),
        .mark = nuade_read_be16(p + 6),
        .packed = p[6] == SIL_PACKED_SAMPLE,
        .loop_start = nuade_read_be32(p + 8),
        .loop_end = nuade_read_be32(p + 12),
    };
    return true;
}

bool sil_block_is_sample(const SilBlock* block) {
    return block->type == SilBlockType_SampleA || block->type == SilBlockType_SampleB;
}

bool sil_block_is_music(const SilBlock* block) {
    return block->type == SilBlockType_MusicA || block->type == SilBlockType_MusicB;
}

// The header of the container: its first long, where its table starts (from the header) and the number of blocks.

typedef struct SilHeader {
    uint32_t first;
    uint32_t table;
    uint16_t blocks;
} SilHeader;

// The header the long at `base` + 0xE finds.

static bool header_read(const SilMemory* memory, uint64_t base, uint64_t* address, SilHeader* header) {
    if (!sil_in_memory(memory, base, SIL_HEADER_LONG + 4)) {
        return false;
    }
    *address = base + sil_u32(memory, (uint32_t)base + SIL_HEADER_LONG);
    if (!sil_in_memory(memory, *address, SIL_HEADER_LENGTH)) {
        return false;
    }
    const uint8_t* p = memory->data + *address;
    *header = (SilHeader) { .first = nuade_read_be32(p),
                            .table = nuade_read_be32(p + 0xC),
                            .blocks = nuade_read_be16(p + 0x10) };
    return true;
}

// The check of the first words of a file: the form (with or without the prefix) and the header.

static bool container_form(const SilMemory* memory, uint32_t start, uint32_t size, uint32_t* base) {
    if (size < 2 * SIL_PREFIX) {
        return false;
    }
    if (sil_u16(memory, start + 4) == SIL_FORM_MARK) {
        *base = 0;
    } else if (sil_u16(memory, start + 4 + SIL_PREFIX) == SIL_FORM_MARK) {
        *base = SIL_PREFIX;
    } else {
        return false;
    }
    if ((uint64_t)*base + SIL_FORM_LENGTH > size) {
        return false;
    }
    // Five zero words, then two words that are skipped and two that read 0x20
    const uint32_t at = start + *base;
    for (uint32_t i = 0; i < SIL_FORM_ZERO_WORDS; i++) {
        if (sil_u16(memory, at + 6 + 2 * i) != 0) {
            return false;
        }
    }
    return sil_u16(memory, at + 18) == SIL_WIDTH_MARK && sil_u16(memory, at + 22) == SIL_WIDTH_MARK;
}

// The eagleplayer's check and the sums of its InitPlayer. Offsets in `out` count from `start`.

static bool container_parse(const SilMemory* memory, uint32_t start, uint32_t size, bool need_music,
                            SilContainer* out) {
    SilContainer c = { 0 };
    uint64_t header_address = 0;
    SilHeader header;
    if (!container_form(memory, start, size, &c.base)
        || !header_read(memory, (uint64_t)start + c.base, &header_address, &header)
        || header_address + SIL_HEADER_LENGTH > (uint64_t)start + size || header.first != SIL_HEADER_FIRST
        || header.table != SIL_HEADER_FIRST) {
        return false;
    }
    c.header = (uint32_t)(header_address - start);
    c.table = c.header + SIL_HEADER_FIRST;
    uint64_t last_block = 0; // the block with the highest address
    uint32_t blocks = 0;
    for (;; blocks++) {
        const uint64_t entry = (uint64_t)start + c.table + 4u * blocks;
        if (entry + 4 > (uint64_t)start + size) {
            return false; // the table has no end inside the file
        }
        const uint32_t offset = sil_u32(memory, (uint32_t)entry);
        if (offset == 0 || (offset >> 24) != 0) {
            break;
        }
        const uint64_t address = entry + offset;
        SilBlock block;
        if ((offset & 1) != 0 || address + SIL_BLOCK_CHECKED > (uint64_t)start + size
            || !sil_block_read(memory, (uint32_t)address, &block)) {
            return false;
        }
        if (block.mark == SIL_MUSIC_MARK) {
            c.music_blocks++;
        } else {
            c.sample_blocks++;
        }
        if (address > last_block) {
            last_block = address;
        }
    }
    SilBlock last;
    // The file holds the last block whole (the eagleplayer checks this one block only)
    if (blocks == 0 || header.blocks != blocks || (need_music && c.music_blocks == 0)
        || !sil_block_read(memory, (uint32_t)last_block, &last) || last_block + last.size > (uint64_t)start + size) {
        return false;
    }
    *out = c;
    return true;
}

bool sil_container_parse(const uint8_t* file, uint32_t size, bool need_music, SilContainer* out) {
    // A read-only view of the file: the check only reads, through the bounded readers
    const SilMemory view = { .data = (uint8_t*)(uintptr_t)file, .size = size };
    return container_parse(&view, 0, size, need_music, out);
}

uint32_t sil_image_base(const SilImage* image) {
    return image->start + image->container.base;
}

// The block lookup of the script: the header and the table are read again from the memory, so a script that patches
// them finds what the eagleplayer would.

bool sil_image_block(const SilMemory* memory, const SilImage* image, uint32_t index, uint32_t* address) {
    const SilContainer* c = &image->container;
    uint64_t header_address = 0;
    SilHeader header;
    if (index >= c->music_blocks + c->sample_blocks
        || !header_read(memory, sil_image_base(image), &header_address, &header)) {
        return false;
    }
    const uint64_t table = header_address + header.table;
    if (table != (uint64_t)image->start + c->table || !sil_in_memory(memory, table + 4u * index, 4)) {
        return false;
    }
    const uint32_t entry = (uint32_t)table + 4u * index;
    const uint64_t block = (uint64_t)entry + sil_u32(memory, entry);
    if (!sil_in_memory(memory, block, SIL_SAMPLE_HEADER)) {
        return false;
    }
    *address = (uint32_t)block;
    return true;
}

static void put_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

// One packed block: the first byte is the first sample, then every nibble adds its delta. The unpacker reads the
// packed bytes from a copy, writes the samples over the block's data and clears the last two, so a packed block of an
// odd length never uses the byte after its data. The block's size shrinks by 2.

static bool unpack_block(SilMemory* memory, uint32_t address, const SilBlock* block) {
    if (block->size < SIL_SAMPLE_HEADER + 4 || !sil_in_memory(memory, address, block->size)) {
        return false;
    }
    const uint32_t packed = (block->size - SIL_SAMPLE_HEADER) / 2;
    uint8_t* source = malloc(packed);
    if (source == nullptr) {
        return false;
    }
    uint8_t* data = memory->data + address + SIL_SAMPLE_HEADER;
    memcpy(source, data, packed);
    uint8_t value = source[0];
    uint32_t out = 0;
    data[out++] = value;
    for (uint32_t i = 1; i < packed; i++) {
        value = (uint8_t)(value + s_deltas[source[i] >> 4]);
        data[out++] = value;
        value = (uint8_t)(value + s_deltas[source[i] & 15]);
        data[out++] = value;
    }
    data[out - 2] = 0;
    data[out - 1] = 0;
    free(source);
    const uint32_t new_size = block->size - 2;
    put_be32(memory->data + address + 2, new_size);
    return true;
}

// The unpacker finds its table from the first byte of the file, not from the base: in the form with the prefix that
// is a table of no blocks.

bool sil_image_unpack(SilMemory* memory, const SilImage* image) {
    uint64_t header_address = 0;
    SilHeader header;
    if (!header_read(memory, image->start, &header_address, &header)) {
        return false;
    }
    const uint64_t table = header_address + header.table;
    for (uint32_t i = 0; i < header.blocks; i++) {
        const uint64_t entry = table + 4u * i;
        if (!sil_in_memory(memory, entry, 4)) {
            return false;
        }
        const uint64_t address = entry + sil_u32(memory, (uint32_t)entry);
        SilBlock block;
        if (!sil_block_read(memory, (uint32_t)address, &block)) {
            return false;
        }
        if (sil_block_is_sample(&block) && block.packed && !unpack_block(memory, (uint32_t)address, &block)) {
            return false;
        }
    }
    return true;
}

SilPatch sil_patch_find(const SilMemory* memory, const SilImage* song) {
    static const struct {
        uint32_t offset; // of the signature, from the start of the file
        uint32_t signature;
    } known[] = { { 0x3E78, 0x9BFF020A }, { 0x3410, 0xCFFF020A } };
    for (uint32_t i = 0; i < nuade_sizeof_array(known); i++) {
        const uint32_t at = song->start + known[i].offset;
        if (sil_in_memory(memory, at, 4) && sil_u32(memory, at) == known[i].signature) {
            return (SilPatch) { .active = true, .address = at + 2 };
        }
    }
    return (SilPatch) { 0 };
}
