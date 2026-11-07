// SPDX-License-Identifier: MIT
// Richard Joseph Player: the eagleplayer's module check (Check2), InitPlayer (the sections), ExtLoad (the sample file)
// and the readers of the module's records.

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "rjp_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_MAGIC_LENGTH 3
#define MODULE_MIN_SIZE 16
#define SMOD_OFFSET 4
#define SECTIONS_OFFSET 8
#define SECTION_COUNT 7
#define SONG_ENTRY_SIZE 4
#define SUBSONG_MASK 0x3F // the replayer starts subsong `n & 0x3F`
#define ENVELOPE_BLOCK_SIZE 6
#define PATTERN_ENTRY_SIZE 4
#define MAX_INSTRUMENTS 256        // an instrument number is a byte
#define MAX_SAMPLE_END (16u << 20) // a sample beyond the 68000's address space is no sample

#define MODULE_PREFIX "rjp."
#define MODULE_PREFIX_LENGTH 4
#define SAMPLE_SUFFIX ".sng"
#define SAMPLE_SUFFIX_LENGTH 4
#define FALLBACK_SAMPLES "smp.set"

// InitPlayer repairs two known modules, found by the total size of their sections.
#define FIX_SIZE_A 0x2522
#define FIX_OFFSET_A 0xA5A // a word in the sequence section, 2 -> 0
#define FIX_VALUE_A 2
#define FIX_SIZE_B 0xBAD
#define FIX_OFFSET_B 0x4A8 // a long in the song table, 0x08050706 -> 0
#define FIX_VALUE_B 0x08050706

uint8_t rjp_u8(const RjpImage* image, uint64_t offset) {
    return offset < image->size ? image->data[offset] : 0;
}

uint16_t rjp_u16(const RjpImage* image, uint64_t offset) {
    return (uint16_t)(rjp_u8(image, offset) << 8 | rjp_u8(image, offset + 1));
}

uint32_t rjp_u32(const RjpImage* image, uint64_t offset) {
    return (uint32_t)rjp_u16(image, offset) << 16 | rjp_u16(image, offset + 2);
}

// The sample file name that ExtLoad builds from the song's file name: "rjp.name" -> "smp.name", "name.sng" ->
// "name.ins". Any other name loses its last character.

static bool sample_file_name(const char* base, char* out, size_t capacity) {
    const size_t length = strlen(base);
    int written;
    if (length >= MODULE_PREFIX_LENGTH && nuade_strncasecmp(base, MODULE_PREFIX, MODULE_PREFIX_LENGTH) == 0) {
        written = snprintf(out, capacity, "smp.%s", base + MODULE_PREFIX_LENGTH);
    } else if (length >= SAMPLE_SUFFIX_LENGTH
               && nuade_strcasecmp(base + length - SAMPLE_SUFFIX_LENGTH, SAMPLE_SUFFIX) == 0) {
        written = snprintf(out, capacity, "%.*s.ins", (int)(length - SAMPLE_SUFFIX_LENGTH), base);
    } else {
        written = snprintf(out, capacity, "%.*s", (int)(length > 0 ? length - 1 : 0), base);
    }
    return written >= 0 && (size_t)written < capacity;
}

// InitPlayer's walk over the seven sections. The sections must lie inside the file; the instrument section holds whole
// records (the relocation loop of the original runs on otherwise), and the song table has an entry.

static bool parse_layout(const uint8_t* data, size_t size, RjpModule* module) {
    const RjpImage image = { .data = (uint8_t*)data, .size = (uint32_t)size };
    RjpSection* sections[SECTION_COUNT]
        = { &module->instruments,     &module->envelopes, &module->songs,   &module->sequence_offsets,
            &module->pattern_offsets, &module->sequences, &module->patterns };
    uint64_t offset = SECTIONS_OFFSET;
    for (size_t i = 0; i < SECTION_COUNT; i++) {
        if (offset + 4 > size) {
            return false;
        }
        const uint32_t section_size = rjp_u32(&image, offset);
        offset += 4;
        if (offset + section_size > size) {
            return false;
        }
        *sections[i] = (RjpSection) { .offset = (uint32_t)offset, .size = section_size };
        offset += section_size;
    }
    module->subsong_count = module->songs.size / SONG_ENTRY_SIZE;
    return module->instruments.size != 0 && module->instruments.size % RJP_INSTRUMENT_SIZE == 0
           && module->subsong_count != 0;
}

// Check2, and the layout that InitPlayer needs. Check2 also wants the first long of the first instrument record (its
// sample offset) to be 0; the replayer does not need it, and `aquatic games` has 0x000A3340 there and plays (user
// decision of 2026-10-11: the port plays it), so the probe leaves that test out.

bool rjp_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < MODULE_MIN_SIZE || size > UINT32_MAX) {
        return false;
    }
    RjpModule layout;
    return memcmp(data, "RJP", MODULE_MAGIC_LENGTH) == 0 && memcmp(data + SMOD_OFFSET, "SMOD", 4) == 0
           && parse_layout(data, size, &layout);
}

RjpInstrument rjp_read_instrument(const RjpModule* module, uint8_t number) {
    uint32_t record = (uint32_t)number * RJP_INSTRUMENT_SIZE;
    if (record >= module->instruments.size) {
        record = 0;
    }
    const RjpImage* m = &module->image;
    const uint64_t at = (uint64_t)module->instruments.offset + record;
    const uint32_t pitch_lfo = rjp_u32(m, at + 4);
    const uint32_t volume_lfo = rjp_u32(m, at + 8);
    return (RjpInstrument) { .sample = rjp_u32(m, at) + RJP_SAMPLE_BASE,
                             .has_pitch_lfo = pitch_lfo != 0,
                             .pitch_lfo = pitch_lfo + RJP_SAMPLE_BASE,
                             .has_volume_lfo = volume_lfo != 0,
                             .volume_lfo = volume_lfo + RJP_SAMPLE_BASE,
                             .envelope = (int16_t)rjp_u16(m, at + 0x0C),
                             .volume = rjp_u16(m, at + 0x0E),
                             .attack_offset = (uint16_t)(rjp_u16(m, at + 0x10) * 2),
                             .attack_words = rjp_u16(m, at + 0x12),
                             .loop_offset_words = rjp_u16(m, at + 0x14),
                             .loop_words = rjp_u16(m, at + 0x16),
                             .pitch_lfo_restart = rjp_u16(m, at + 0x18),
                             .pitch_lfo_words = rjp_u16(m, at + 0x1A),
                             .volume_lfo_restart = rjp_u16(m, at + 0x1C),
                             .volume_lfo_words = rjp_u16(m, at + 0x1E) };
}

uint32_t rjp_envelope_block(const RjpModule* module, const RjpInstrument* instrument) {
    return module->envelopes.offset + (uint32_t)(int32_t)instrument->envelope;
}

RjpEnvelopeShape rjp_read_envelope(const RjpModule* module, uint32_t block) {
    const RjpImage* m = &module->image;
    return (RjpEnvelopeShape) { .start = rjp_u8(m, block),
                                .peak = rjp_u8(m, (uint64_t)block + 1),
                                .attack_ticks = rjp_u8(m, (uint64_t)block + 2),
                                .sustain = rjp_u8(m, (uint64_t)block + 3),
                                .decay_ticks = rjp_u8(m, (uint64_t)block + 4),
                                .release_ticks = rjp_u8(m, (uint64_t)block + 5) };
}

uint32_t rjp_pattern_start(const RjpModule* module, uint8_t pattern) {
    const uint64_t entry = (uint64_t)module->pattern_offsets.offset + (uint64_t)pattern * PATTERN_ENTRY_SIZE;
    return module->patterns.offset + rjp_u32(&module->image, entry);
}

uint32_t rjp_sequence_start(const RjpModule* module, uint8_t sequence) {
    const uint64_t entry = (uint64_t)module->sequence_offsets.offset + (uint64_t)sequence * PATTERN_ENTRY_SIZE;
    return module->sequences.offset + rjp_u32(&module->image, entry);
}

uint8_t rjp_song_sequence(const RjpModule* module, uint32_t subsong, uint32_t voice) {
    const uint64_t entry = (uint64_t)module->songs.offset + (subsong & SUBSONG_MASK) * SONG_ENTRY_SIZE;
    return rjp_u8(&module->image, entry + voice);
}

static void module_free(RjpModule* module) {
    free(module->image.data);
    module->image = (RjpImage) { 0 };
}

// The two known broken modules are repaired in the copy.

static void repair_known_modules(RjpModule* module) {
    const RjpSection* last = &module->patterns;
    const uint64_t total = (uint64_t)last->offset + last->size;
    RjpImage* m = &module->image;
    if (total == FIX_SIZE_A && rjp_u16(m, FIX_OFFSET_A) == FIX_VALUE_A) {
        m->data[FIX_OFFSET_A] = 0;
        m->data[FIX_OFFSET_A + 1] = 0;
    }
    if (total == FIX_SIZE_B && rjp_u32(m, FIX_OFFSET_B) == FIX_VALUE_B) {
        memset(m->data + FIX_OFFSET_B, 0, 4);
    }
}

// The end of the furthest byte the module can make Paula or an LFO read from the sample file.

static uint64_t samples_end(const RjpModule* module, uint32_t file_size) {
    uint64_t end = file_size;
    const uint32_t count = module->instruments.size / RJP_INSTRUMENT_SIZE;
    for (uint32_t i = 0; i < count && i < MAX_INSTRUMENTS; i++) {
        const RjpInstrument in = rjp_read_instrument(module, (uint8_t)i);
        // AUDxLEN 0 plays 65536 words
        const uint32_t attack_words = in.attack_words != 0 ? in.attack_words : RJP_ZERO_LENGTH_WORDS;
        const uint32_t loop_words = in.loop_words != 0 ? in.loop_words : RJP_ZERO_LENGTH_WORDS;
        const uint64_t ends[4] = { (uint64_t)in.sample + in.attack_offset + attack_words * 2u,
                                   (uint64_t)in.sample + (in.loop_offset_words + loop_words) * 2u,
                                   in.has_pitch_lfo ? (uint64_t)in.pitch_lfo + in.pitch_lfo_words * 2u : 0,
                                   in.has_volume_lfo ? (uint64_t)in.volume_lfo + in.volume_lfo_words * 2u : 0 };
        for (size_t j = 0; j < nuade_sizeof_array(ends); j++) {
            if (ends[j] < MAX_SAMPLE_END && ends[j] > end) {
                end = ends[j];
            }
        }
    }
    return end;
}

// Reads `name` from the directory `dir` (any case). The data is copied to a zero padded buffer of `samples_end` bytes.

static bool read_sample_file(RjpPlayer* player, const NuadeIo* io, const char* dir, const char* name) {
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    bool ok = false;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr && file.size <= UINT32_MAX) {
            const uint64_t padded = samples_end(&player->module, (uint32_t)file.size);
            uint8_t* working = nuade_paula_padded_copy(nullptr, padded);
            uint8_t* original = calloc(padded != 0 ? padded : 1, 1);
            if (working != nullptr && original != nullptr) {
                memcpy(original, file.data, file.size);
                player->samples = (RjpImage) { .data = working, .size = (uint32_t)padded };
                player->samples_original = original;
                ok = true;
            } else {
                free(working);
                free(original);
            }
        }
        if (file.data != nullptr) {
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return ok;
}

// ExtLoad: the sample file next to the song (named as above), else "smp.set" there.

static bool load_samples(RjpPlayer* player, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char name[512];
    if (path == nullptr || io == nullptr || !sample_file_name(nuade_path_basename(path), name, sizeof(name))) {
        return false;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    return read_sample_file(player, io, dir, name) || read_sample_file(player, io, dir, FALLBACK_SAMPLES);
}

RjpCreateResult rjp_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    RjpCreateResult result = { .status = RjpStatus_InvalidData };
    if (!rjp_probe(data, size)) {
        return result;
    }
    RjpPlayer* player = calloc(1, sizeof(RjpPlayer));
    if (player == nullptr) {
        result.status = RjpStatus_OutOfMemory;
        return result;
    }
    parse_layout(data, size, &player->module);
    player->module.image = (RjpImage) { .data = malloc(size), .size = (uint32_t)size };
    if (player->module.image.data == nullptr) {
        rjp_destroy(player);
        result.status = RjpStatus_OutOfMemory;
        return result;
    }
    memcpy(player->module.image.data, data, size);
    repair_known_modules(&player->module);
    if (!load_samples(player, path, io)) {
        rjp_destroy(player);
        result.status = RjpStatus_MissingSamples;
        return result;
    }
    result.player = player;
    result.status = RjpStatus_Ok;
    return result;
}

void rjp_destroy(RjpPlayer* player) {
    if (player == nullptr) {
        return;
    }
    module_free(&player->module);
    free(player->samples.data);
    free(player->samples_original);
    free(player);
}

uint32_t rjp_subsong_count(const RjpPlayer* player) {
    return player->module.subsong_count;
}
