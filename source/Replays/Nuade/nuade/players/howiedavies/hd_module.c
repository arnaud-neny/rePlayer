// SPDX-License-Identifier: MIT
// Howie Davies: the eagleplayer's Check2, the hunk loader, the identification of the replayer version and the typed
// readers of the module records. A pointer stored in the file is a code offset: the loader adds the hunk's address.

#include "hd_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define HD_HUNK_HEADER 0x3F3u
#define HD_HUNK_CODE 0x3E9u
#define HD_HUNK_RELOC32 0x3ECu
#define HD_HUNK_END 0x3F2u

// The file offset of the code hunk's first byte: header (3 words + first, last), 1 size word, hunk id and size.
#define HD_CODE_FILE_OFFSET 0x20u

// The four bytes of the program the eagleplayer's Check2 expects at the start of the code: moveq #-1,d0; rts.
#define HD_CODE_PROLOGUE 0x70FF4E75u

#define HD_RECORD_BYTES 16u
#define HD_SILENCE_BYTES 4u
#define HD_MAX_COUNT 0xFFFFu

// The header of the code, after the prologue and the "H.DAVIES" mark.
typedef struct HdHeader {
    uint32_t play;
    uint32_t dma_on;
    uint32_t init;
    uint32_t sample_table;
    uint32_t sample_end;
    uint32_t texts[3];
    uint32_t subsong_table;
} HdHeader;

#define HD_HEADER_BYTES 0x40u

static const uint8_t k_silence[HD_SILENCE_BYTES] = { 0, 0, 0, 0 };

// Blazing Thunder is the version the eagleplayer's patches were written for. The Future Bike Simulator one keeps the
// sample loops out of the record and fades with the other envelope order, Yogi's Great Escape adds a global volume
// limit, rests that move the list pointer one byte further and a position list command that leaves the step.
static const HdDriver k_drivers[] = {
    {
        .signature = 0x7064AA33AE089418ULL,
        .play = 0xCE,
        .dma_on = 0x182,
        .sample_table = 0x54A,
        .period_table = 0x478,
        .command_table = 0x528,
        .list_command_table = 0x53C,
        .envelope_table = 0x544,
        .pattern_table_operand = 0x2C6,
        .default_stream_operand = 0x9C,
        .default_envelope_operand = 0xAE,
        .subsong_table_operand = 0x30,
        .variables_begin = 0x46E,
        .variables_end = 0x478,
        .handlers = { { 0x2A4, HdHandler_Repeat },
                      { 0x2EA, HdHandler_Instrument },
                      { 0x334, HdHandler_Speed },
                      { 0x342, HdHandler_Delay },
                      { 0x34C, HdHandler_MaxVolume },
                      { 0x36E, HdHandler_ListRestart },
                      { 0x378, HdHandler_ListLoopBack } },
        .envelope = HdEnvelope_WriteThenStep,
        .volume_limit = true,
        .master_limit = false,
        .loop_from_instrument = true,
        .list_loop_extra = 0,
        .subsong_stride = 4,
    },
    {
        .signature = 0x265DFEF7A24571C1ULL,
        .play = 0xC0,
        .dma_on = 0x16C,
        .sample_table = 0x548,
        .period_table = 0x472,
        .command_table = 0x526,
        .list_command_table = 0x536,
        .envelope_table = 0x542,
        .pattern_table_operand = 0x29A,
        .default_stream_operand = 0x9C,
        .default_envelope_operand = 0xAE,
        .subsong_table_operand = 0x48,
        .variables_begin = 0x468,
        .variables_end = 0x472,
        .handlers = { { 0x278, HdHandler_Repeat },
                      { 0x2BE, HdHandler_Instrument },
                      { 0x32E, HdHandler_Speed },
                      { 0x33C, HdHandler_Delay },
                      { 0x35A, HdHandler_ListRestart },
                      { 0x364, HdHandler_ListLoopBack },
                      { 0x37A, HdHandler_ListRestartOff } },
        .envelope = HdEnvelope_StepThenWrite,
        .volume_limit = false,
        .master_limit = false,
        .loop_from_instrument = false,
        .list_loop_extra = 1,
        .subsong_stride = 16,
    },
    {
        .signature = 0xE8866939570E4A5EULL,
        .play = 0xA4,
        .dma_on = 0x174,
        .sample_table = 0x5AE,
        .period_table = 0x4D8,
        .command_table = 0x588,
        .list_command_table = 0x59C,
        .envelope_table = 0x5A8,
        .pattern_table_operand = 0x2DC,
        .default_stream_operand = 0x80,
        .default_envelope_operand = 0x92,
        .subsong_table_operand = 0x20,
        .variables_begin = 0x4CA,
        .variables_end = 0x4D4,
        .handlers = { { 0x2BA, HdHandler_Repeat },
                      { 0x300, HdHandler_Instrument },
                      { 0x388, HdHandler_Speed },
                      { 0x396, HdHandler_Delay },
                      { 0x3A0, HdHandler_MaxVolume },
                      { 0x3C2, HdHandler_ListRestart },
                      { 0x3CC, HdHandler_ListLoopBack },
                      { 0x2A2, HdHandler_Return } },
        .envelope = HdEnvelope_WriteThenStep,
        .volume_limit = true,
        .master_limit = true,
        .loop_from_instrument = true,
        .list_loop_extra = 1,
        .subsong_stride = 16,
    },
};

const int8_t* hd_silence(void) {
    return (const int8_t*)k_silence;
}

bool hd_byte(const HdModule* module, uint32_t at, uint8_t* out) {
    if (at >= module->size) {
        return false;
    }
    *out = module->bytes[at];
    return true;
}

static bool fits(const HdModule* module, uint64_t at, uint64_t bytes) {
    return module->size >= bytes && at <= module->size - bytes;
}

static bool is_relocated(const HdModule* module, uint32_t at) {
    size_t low = 0;
    size_t high = module->relocation_count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (module->relocations[middle] == at) {
            return true;
        }
        if (module->relocations[middle] < at) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return false;
}

bool hd_pointer(const HdModule* module, uint32_t at, uint32_t* out) {
    if (!fits(module, at, 4) || !is_relocated(module, at)) {
        return false;
    }
    const uint32_t target = nuade_read_be32(module->bytes + at);
    if (target >= module->size) {
        return false;
    }
    *out = target;
    return true;
}

bool hd_period(const HdModule* module, uint32_t note, uint16_t* out) {
    const uint64_t at = (uint64_t)module->init + module->driver->period_table + (uint64_t)note * 2;
    if (note >= HD_PERIOD_COUNT || !fits(module, at, 2)) {
        return false;
    }
    *out = nuade_read_be16(module->bytes + at);
    return true;
}

// The start of a sample: a relocated pointer. AUDxLC ignores its lowest bit, and the rip leaves samples at odd
// addresses.

static bool sample_start(const HdModule* module, uint32_t at, const int8_t** out) {
    uint32_t offset;
    if (!hd_pointer(module, at, &offset)) {
        return false;
    }
    *out = (const int8_t*)(module->bytes + (offset & ~1u));
    return true;
}

// The repeat of a sample: a relocated pointer, or a plain 0 that the DMA routine's patch makes silence.

static bool sample_loop(const HdModule* module, uint32_t at, const int8_t** out) {
    if (fits(module, at, 4) && !is_relocated(module, at) && nuade_read_be32(module->bytes + at) == 0) {
        *out = hd_silence();
        return true;
    }
    return sample_start(module, at, out);
}

// The block of `words` words at `start` lies inside the hunk (or is the silence).

static bool block_usable(const HdModule* module, const int8_t* start, uint16_t words) {
    if (words == 0) {
        return false;
    }
    if (start == (const int8_t*)k_silence) {
        return words * 2u <= HD_SILENCE_BYTES;
    }
    const size_t offset = (size_t)((const uint8_t*)start - module->bytes);
    return (size_t)words * 2 <= module->size - offset;
}

bool hd_sample(const HdModule* module, uint32_t index, HdSample* out) {
    if (index >= module->sample_count) {
        return false;
    }
    const uint32_t at = module->sample_table + index * HD_RECORD_BYTES;
    if (!fits(module, at, HD_RECORD_BYTES)) {
        return false;
    }
    const uint8_t* record = module->bytes + at;
    HdSample sample = { .words = nuade_read_be16(record + 4),
                        .loop_words = nuade_read_be16(record + 10),
                        .volume = nuade_read_be16(record + 12),
                        .envelope = nuade_read_be16(record + 14) };
    // The eagleplayer overwrites the address of the first sample with the silence
    if (index == 0) {
        sample.start = hd_silence();
    } else if (!sample_start(module, at, &sample.start)) {
        return false;
    }
    if (!sample_loop(module, at + 6, &sample.loop) || !block_usable(module, sample.start, sample.words)
        || !block_usable(module, sample.loop, sample.loop_words)) {
        return false;
    }
    *out = sample;
    return true;
}

bool hd_pattern(const HdModule* module, uint32_t index, uint32_t* out) {
    return index < module->pattern_count && hd_pointer(module, module->pattern_table + index * 4, out);
}

bool hd_envelope(const HdModule* module, uint32_t index, uint32_t* out) {
    return index < module->envelope_count && hd_pointer(module, module->envelope_table + index * 4, out);
}

bool hd_voice_list(const HdModule* module, uint32_t subsong, uint32_t voice, uint32_t* out) {
    if (subsong >= module->subsong_count || voice >= HD_VOICES) {
        return false;
    }
    return hd_pointer(module, module->subsong_table + subsong * module->driver->subsong_stride + voice * 4, out);
}

bool hd_handler(const HdModule* module, uint32_t table, uint8_t command, HdHandler* out) {
    uint32_t target;
    if (!hd_pointer(module, table + (uint32_t)(UINT8_MAX - command) * 4u, &target) || target < module->init) {
        return false;
    }
    for (size_t i = 0; i < HD_MAX_HANDLERS; i++) {
        const HdHandlerAddress* entry = &module->driver->handlers[i];
        if (entry->address != 0 && module->init + entry->address == target) {
            *out = entry->handler;
            return true;
        }
    }
    return false;
}

// The relocation table: HUNK_RELOC32 groups of (count, hunk, offsets) up to HUNK_END. The file has one hunk, so every
// group relocates against hunk 0.

static bool read_relocations(HdModule* module, const uint8_t* file, size_t size, size_t at) {
    size_t capacity = 0;
    while (size - at >= 4) {
        const uint32_t id = nuade_read_be32(file + at) & 0x3FFFFFFFu;
        at += 4;
        if (id == HD_HUNK_END) {
            return true;
        }
        if (id != HD_HUNK_RELOC32) {
            return false;
        }
        for (;;) {
            if (size - at < 4) {
                return false;
            }
            const uint32_t count = nuade_read_be32(file + at);
            at += 4;
            if (count == 0) {
                break;
            }
            if (size - at < 4 || nuade_read_be32(file + at) != 0 || (size - at - 4) / 4 < count) {
                return false;
            }
            at += 4;
            if (module->relocation_count + count > capacity) {
                capacity = module->relocation_count + count;
                uint32_t* grown = realloc(module->relocations, capacity * sizeof(*grown));
                if (grown == nullptr) {
                    return false;
                }
                module->relocations = grown;
            }
            for (uint32_t i = 0; i < count; i++, at += 4) {
                const uint32_t offset = nuade_read_be32(file + at);
                // Sorted without repeats, the whole word inside the hunk
                if (!fits(module, offset, 4)
                    || (module->relocation_count > 0 && offset <= module->relocations[module->relocation_count - 1])) {
                    return false;
                }
                module->relocations[module->relocation_count++] = offset;
            }
        }
    }
    return false;
}

// The executable: HUNK_HEADER with one chip memory hunk, HUNK_CODE, the relocations, HUNK_END. Check2 also wants a
// memory flag in the size word.

static bool load_hunks(HdModule* module, const uint8_t* file, size_t size) {
    if (size < HD_CODE_FILE_OFFSET + 8 || nuade_read_be32(file) != HD_HUNK_HEADER || nuade_read_be32(file + 4) != 0
        || nuade_read_be32(file + 8) != 1 || nuade_read_be32(file + 12) != 0 || nuade_read_be32(file + 16) != 0
        || file[0x14] == 0 || nuade_read_be32(file + 0x18) != HD_HUNK_CODE) {
        return false;
    }
    const uint32_t longs = nuade_read_be32(file + 0x1C);
    if ((nuade_read_be32(file + 0x14) & 0x3FFFFFFFu) != longs || longs < HD_HEADER_BYTES / 4
        || (size - HD_CODE_FILE_OFFSET) / 4 < longs) {
        return false;
    }
    module->bytes = file + HD_CODE_FILE_OFFSET;
    module->size = (size_t)longs * 4;
    return read_relocations(module, file, size, HD_CODE_FILE_OFFSET + module->size);
}

// The header of the code: the prologue, the "H.DAVIES" mark and the pointers that the eagleplayer's Check2 requires
// to be set.

static bool read_header(const HdModule* module, HdHeader* header) {
    static const uint8_t mark[] = { 'H', '.', 'D', 'A', 'V', 'I', 'E', 'S' };
    if (nuade_read_be32(module->bytes) != HD_CODE_PROLOGUE || memcmp(module->bytes + 4, mark, sizeof(mark)) != 0) {
        return false;
    }
    const bool pointers_ok
        = hd_pointer(module, 0x0C, &header->play) && hd_pointer(module, 0x10, &header->dma_on)
          && hd_pointer(module, 0x14, &header->init) && hd_pointer(module, 0x18, &header->sample_table)
          && hd_pointer(module, 0x1C, &header->sample_end) && hd_pointer(module, 0x20, &header->texts[0])
          && hd_pointer(module, 0x24, &header->texts[1]) && hd_pointer(module, 0x28, &header->texts[2])
          && hd_pointer(module, 0x3C, &header->subsong_table);
    return pointers_ok && header->play != 0 && header->dma_on != 0 && header->init != 0;
}

// FNV-1a over the replayer and its tables, from the init routine to the sample table. A relocated word that points
// into that region counts as its target counted from the init routine, one that points into the song's data and the
// variables that the init routine writes count as zero.

static bool replayer_signature(const HdModule* module, const HdDriver* driver, uint64_t* out) {
    const uint32_t begin = module->init;
    const uint32_t length = driver->sample_table;
    uint8_t* image = malloc(length);
    if (image == nullptr) {
        return false;
    }
    memcpy(image, module->bytes + begin, length);
    for (size_t i = 0; i < module->relocation_count; i++) {
        const uint32_t at = module->relocations[i];
        if (at + 4 <= begin || at >= begin + length) {
            continue;
        }
        const uint32_t target = nuade_read_be32(module->bytes + at);
        const uint32_t inside = target >= begin && target - begin <= length ? target - begin : 0;
        for (uint32_t k = 0; k < 4; k++) {
            if (at + k >= begin && at + k < begin + length) {
                image[at + k - begin] = (uint8_t)(inside >> (24 - 8 * k));
            }
        }
    }
    memset(image + driver->variables_begin, 0, driver->variables_end - driver->variables_begin);
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (uint32_t i = 0; i < length; i++) {
        hash = (hash ^ image[i]) * 0x100000001B3ULL;
    }
    free(image);
    *out = hash;
    return true;
}

static const HdDriver* find_driver(const HdModule* module, const HdHeader* header) {
    uint64_t signature;
    for (size_t i = 0; i < nuade_sizeof_array(k_drivers); i++) {
        const HdDriver* driver = &k_drivers[i];
        if (header->play == header->init + driver->play && header->dma_on == header->init + driver->dma_on
            && header->sample_table == header->init + driver->sample_table
            && fits(module, (uint64_t)header->init + driver->sample_table, 0)
            && replayer_signature(module, driver, &signature) && signature == driver->signature) {
            return driver;
        }
    }
    return nullptr;
}

// The number of entries of a table of pointers that ends where its first target starts (16 bytes or 4 per entry).

static bool table_count(const HdModule* module, uint32_t table, uint32_t entry_bytes, uint32_t* count) {
    uint32_t first;
    if (!hd_pointer(module, table, &first) || first <= table || (first - table) % entry_bytes != 0) {
        return false;
    }
    *count = (first - table) / entry_bytes;
    return *count <= HD_MAX_COUNT;
}

bool hd_module_parse(HdModule* module, const uint8_t* bytes, size_t size) {
    *module = (HdModule) { 0 };
    HdHeader header;
    if (!load_hunks(module, bytes, size) || !read_header(module, &header)) {
        hd_module_free(module);
        return false;
    }
    module->init = header.init;
    module->driver = find_driver(module, &header);
    if (module->driver == nullptr || header.sample_end < header.sample_table
        || (header.sample_end - header.sample_table) % HD_RECORD_BYTES != 0 || header.sample_end > module->size) {
        hd_module_free(module);
        return false;
    }
    const HdDriver* driver = module->driver;
    uint32_t subsong_table;
    // The init routine loads the subsong table: it must be the table the header names
    if (!hd_pointer(module, module->init + driver->subsong_table_operand, &subsong_table)
        || subsong_table != header.subsong_table) {
        hd_module_free(module);
        return false;
    }
    module->sample_table = header.sample_table;
    module->sample_count = (header.sample_end - header.sample_table) / HD_RECORD_BYTES;
    module->envelope_table = module->init + driver->envelope_table;
    module->subsong_table = header.subsong_table;
    memcpy(module->texts, header.texts, sizeof(module->texts));
    if (!hd_pointer(module, module->init + driver->pattern_table_operand, &module->pattern_table)
        || !hd_pointer(module, module->init + driver->default_stream_operand, &module->default_stream)
        || !hd_pointer(module, module->init + driver->default_envelope_operand, &module->default_envelope)
        || !table_count(module, module->pattern_table, 4, &module->pattern_count)
        || !table_count(module, module->envelope_table, 4, &module->envelope_count)
        || !table_count(module, module->subsong_table, 16, &module->subsong_count)) {
        hd_module_free(module);
        return false;
    }
    return true;
}

void hd_module_free(HdModule* module) {
    free(module->relocations);
    module->relocations = nullptr;
    module->relocation_count = 0;
}
