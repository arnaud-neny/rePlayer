// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: the module file. A hunk executable with one code hunk; the eagleplayer's Check2 and the
// parts of its InitPlayer that read the file.

#include "beathoven_internal.h"
#include "nuade_utils.h"
#include <stdlib.h>

// AmigaDOS hunk file format
#define HUNK_HEADER 0x3F3u
#define HUNK_CODE 0x3E9u
#define HUNK_RELOC32 0x3ECu

// Check2 reads the code at a fixed place: the header words, the size table of one hunk, the hunk type and its size
#define CODE_OFFSET 0x20u
// ... and tests the high byte of the hunk's size word, the memory flags: the module is loaded to chip memory
#define MEMORY_FLAGS_OFFSET 0x14u

// The first instructions of the code: moveq #-1,d0; rts, then the signature and the replayer header
#define SIGNATURE_CODE 0x70FF4E75u
#define SIGNATURE_BEAT 0x42454154u
#define SIGNATURE_HOVE 0x484F5645u
#define SIGNATURE_VERSION_OFFSET 12u
#define HEADER_OFFSET 16u
#define HEADER_LONGS 15u

// The hunk header: the signature word, an empty list of library names, then the number of hunks in the table and the
// first and last hunk number, and one size per hunk
#define HUNK_COUNT_OFFSET 8u
#define HUNK_FIRST_OFFSET 12u
#define HUNK_LAST_OFFSET 16u
#define HUNK_SIZES_OFFSET 20u
#define HUNK_SIZE_MASK 0x3FFFFFFFu
#define HUNK_TYPE_MASK 0x3FFFFFFFu

// A module is a few hundred KB; the limit keeps the sums of sizes in 32 bits
#define MAX_FILE_BYTES 0x10000000u

static uint32_t file_u32(const uint8_t* data, size_t size, size_t at) {
    return at + 4 <= size ? nuade_read_be32(data + at) : 0;
}

// The eagleplayer's Check2: the file layout, the signature and three nonzero header longwords (play, init and the
// subsong count)
static bool check2(const uint8_t* data, size_t size) {
    if (size < CODE_OFFSET + HEADER_OFFSET + 4 * HEADER_LONGS || file_u32(data, size, 0) != HUNK_HEADER
        || data[MEMORY_FLAGS_OFFSET] == 0) {
        return false;
    }
    const uint8_t* code = data + CODE_OFFSET;
    const uint32_t version = nuade_read_be32(code + SIGNATURE_VERSION_OFFSET);
    return nuade_read_be32(code) == SIGNATURE_CODE && nuade_read_be32(code + 4) == SIGNATURE_BEAT
           && nuade_read_be32(code + 8) == SIGNATURE_HOVE
           && (version == BTH_VERSION_100 || version == BTH_VERSION_109 || version == BTH_VERSION_NEW)
           && nuade_read_be32(code + HEADER_OFFSET) != 0 && nuade_read_be32(code + HEADER_OFFSET + 4) != 0
           && nuade_read_be32(code + HEADER_OFFSET + 8) != 0;
}

// The relocation table: HUNK_RELOC32 blocks of (count, hunk, offsets...) up to a zero count. Only offsets inside the
// hunk are kept. Advances *position past the blocks; false if memory ran out.
static bool read_relocs(BthImage* image, const uint8_t* data, size_t size, size_t* position) {
    size_t capacity = 0;
    size_t at = *position;
    while (at + 4 <= size) {
        const uint32_t count = file_u32(data, size, at);
        at += 4;
        if (count == 0) {
            break;
        }
        at += 4; // the hunk the offsets point into
        for (uint32_t i = 0; i < count && at + 4 <= size; i++, at += 4) {
            const uint32_t offset = file_u32(data, size, at);
            if (offset + 4 > image->module.size) {
                continue;
            }
            if (image->reloc_count == capacity) {
                capacity = capacity == 0 ? 256 : capacity * 2;
                uint32_t* grown = realloc(image->relocs, capacity * sizeof(uint32_t));
                if (grown == nullptr) {
                    return false;
                }
                image->relocs = grown;
            }
            image->relocs[image->reloc_count++] = offset;
        }
    }
    *position = at;
    return true;
}

static int compare_offsets(const void* a, const void* b) {
    const uint32_t x = *(const uint32_t*)a;
    const uint32_t y = *(const uint32_t*)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

// Where UADE puts the hunk. The loader reads the file to a fixed address, LoadSeg's buffer follows it (aligned, with a
// gap), and the hunk is allocated behind a second copy of the file. Only the words of relocated pointers that the
// replayers read as data depend on it.
#define UADE_FILE_ADDRESS 0xF000u
#define UADE_BUFFER_ALIGNMENT 0x100u
#define UADE_BUFFER_GAP 0x300u
#define UADE_HUNK_OFFSET 0x18u

static uint32_t uade_load_address(uint32_t file_size) {
    const uint32_t buffer = ((UADE_FILE_ADDRESS + file_size + UADE_BUFFER_ALIGNMENT - 1) & ~(UADE_BUFFER_ALIGNMENT - 1))
                            + UADE_BUFFER_GAP;
    return ((buffer + file_size) & ~0xFu) + UADE_HUNK_OFFSET;
}

BthImage* bthi_load(const uint8_t* data, size_t size) {
    if (size > MAX_FILE_BYTES || !check2(data, size) || file_u32(data, size, HUNK_COUNT_OFFSET) != 1
        || file_u32(data, size, HUNK_FIRST_OFFSET) != 0 || file_u32(data, size, HUNK_LAST_OFFSET) != 0) {
        return nullptr;
    }
    // The size table holds one entry; the hunk follows as type and size in longwords
    const size_t hunk = HUNK_SIZES_OFFSET + 4;
    const uint32_t bytes = (file_u32(data, size, hunk + 4) & HUNK_SIZE_MASK) * 4;
    if ((file_u32(data, size, hunk) & HUNK_TYPE_MASK) != HUNK_CODE || hunk + 8 != CODE_OFFSET
        || bytes > size - CODE_OFFSET || bytes < HEADER_OFFSET + 4 * HEADER_LONGS) {
        return nullptr;
    }
    BthImage* image = calloc(1, sizeof(*image));
    uint8_t* memory = nuade_paula_padded_copy(data + CODE_OFFSET, bytes);
    if (image == nullptr || memory == nullptr) {
        free(image);
        free(memory);
        return nullptr;
    }
    image->module = (BthModule) { .data = memory, .size = bytes };
    image->version = nuade_read_be32(memory + SIGNATURE_VERSION_OFFSET);
    image->load_address = uade_load_address((uint32_t)size);

    // The relocation table follows the code; symbol and debug hunks after it are not needed
    size_t at = CODE_OFFSET + bytes;
    while (file_u32(data, size, at) == HUNK_RELOC32) {
        at += 4;
        if (!read_relocs(image, data, size, &at)) {
            bthi_free(image);
            return nullptr;
        }
    }
    if (image->reloc_count > 1) {
        qsort(image->relocs, image->reloc_count, sizeof(uint32_t), compare_offsets);
    }
    return image;
}

void bthi_free(BthImage* image) {
    if (image != nullptr) {
        free(image->module.data);
        free(image->relocs);
        free(image);
    }
}

bool bthi_is_pointer(const BthImage* image, uint32_t address) {
    return image->reloc_count != 0
           && bsearch(&address, image->relocs, image->reloc_count, sizeof(uint32_t), compare_offsets) != nullptr;
}

BthHeader bthi_header(const BthImage* image) {
    const BthModule* m = &image->module;
    const uint32_t h = HEADER_OFFSET;
    BthHeader header = { .play = bthm_u32(m, h),
                         .init = bthm_u32(m, h + 4),
                         .subsong_count = bthm_u32(m, h + 8),
                         .once = bthm_u32(m, h + 16),
                         .title = bthm_u32(m, h + 20),
                         .author = bthm_u32(m, h + 24),
                         .samples = bthm_u32(m, h + 32),
                         .samples_end = bthm_u32(m, h + 36) };
    // The eagleplayer's InitSound makes the last longword the interrupt's CIA timer value when its high word is 0. A
    // relocated pointer has a high word (the load address is above 0x10000).
    const uint32_t last = bthm_u32(m, h + 56);
    header.timer = !bthi_is_pointer(image, h + 56) && last <= 0xFFFF ? last : 0;
    return header;
}
