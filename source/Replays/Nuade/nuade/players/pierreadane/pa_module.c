// SPDX-License-Identifier: MIT
// Pierre Adane Packer: the song file (the eagleplayer's Check2 and InitPlayer) and its sample set (ExtLoad).

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "pa_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_COUNT 4
#define HEADER_BYTES 8 // the four list offsets
#define INSTRUMENT_BYTES 8
#define INSTRUMENT_END 0xFF // first byte of the record that ends the instrument table
#define TRACK_LENGTH_MARKER 0xFF
#define MARKER_BYTES 2
#define NOTE_BYTES 4
#define MAX_OFFSET 0x7FFF // the replayer adds offsets as signed words
// A song of this size whose second instrument has this length is a broken rip: the eagleplayer patches the length.
#define PATCH_SONG_SIZE 0xA2C
#define PATCH_INSTRUMENT 2
#define PATCH_BAD_LENGTH 0x6FC
#define PATCH_GOOD_LENGTH 0x6AE
#define EFFECT_BITS 4 // a track entry's second byte: instrument in the high nibble, effect in the low one
#define EFFECT_MASK 0x0F
#define LENGTH_HIGH_MASK 0xFFFF0000u
#define MAX_LOOP_WORDS 0x10000 // loop length 0 plays 65536 words
#define SAMPLE_PREFIX "PAP."
#define SAMPLE_PREFIX_LENGTH 4
#define SAMPLE_SET_PREFIX "SMP."
#define SAMPLE_SET_FALLBACK "SMP.set"
#define CLEARED_BYTES 4 // the replayer clears the first long of every sample

static bool module_has(const PaModule* module, int64_t offset, uint32_t bytes) {
    return offset >= 0 && offset <= (int64_t)module->size - (int64_t)bytes;
}

static uint16_t module_u16(const PaModule* module, uint32_t offset) {
    return nuade_read_be16(module->data + offset);
}

uint16_t pa_module_list_start(const PaModule* module, uint32_t voice) {
    return module_u16(module, 2 * voice);
}

bool pa_module_list_entry(const PaModule* module, uint32_t voice, int16_t position, uint16_t* track) {
    const uint16_t base = pa_module_list_start(module, voice);
    const int16_t offset = (int16_t)(uint16_t)(base + (uint16_t)position);
    if (!module_has(module, offset, 2)) {
        return false;
    }
    *track = module_u16(module, (uint32_t)offset);
    return true;
}

bool pa_module_instrument(const PaModule* module, uint32_t number, PaInstrument* out) {
    const uint32_t offset = INSTRUMENT_BYTES * number;
    if (!module_has(module, offset, INSTRUMENT_BYTES)) {
        return false;
    }
    *out = (PaInstrument) { .length_words = module_u16(module, offset),
                            .volume = module_u16(module, offset + 2),
                            .loop_offset = module_u16(module, offset + 4),
                            .loop_length_words = module_u16(module, offset + 6) };
    return true;
}

static PaNote note_at(const uint8_t* p) {
    return (PaNote) {
        .period = nuade_read_be16(p), .instrument = p[2] >> 4, .effect = (PaEffect)(p[2] & 15), .param = p[3]
    };
}

bool pa_module_track_entry(const PaModule* module, int16_t position, PaTrackEntry* out) {
    if (!module_has(module, position, 1)) {
        return false;
    }
    const uint8_t* p = module->data + position;
    const bool marker = p[0] == TRACK_LENGTH_MARKER;
    const uint32_t bytes = (marker ? MARKER_BYTES : 0) + NOTE_BYTES;
    if (!module_has(module, position, bytes)) {
        return false;
    }
    *out = (PaTrackEntry) { .sets_length = marker,
                            .length = marker ? p[1] : 0,
                            .note = note_at(p + (marker ? MARKER_BYTES : 0)),
                            .bytes = (uint8_t)bytes };
    return true;
}

// Check2: the four list offsets are positive, even, and the lists follow each other (list 0 has one entry more than
// the others, its 0 terminator); the first track starts with a length marker; every list entry is even and not beyond
// the first list's offset.

static bool check2(const PaModule* module) {
    if (module->size < HEADER_BYTES) {
        return false;
    }
    uint16_t list[LIST_COUNT];
    for (uint32_t i = 0; i < LIST_COUNT; i++) {
        list[i] = pa_module_list_start(module, i);
        if (list[i] == 0 || list[i] > MAX_OFFSET || (list[i] & 1) != 0) {
            return false;
        }
    }
    const int32_t size3 = list[3] - list[2];
    const int32_t size2 = list[2] - list[1];
    const int32_t size1 = list[1] - list[0];
    if (size3 < 0 || size2 < 0 || size1 < 0 || size2 != size3 || size1 - 2 != size3) {
        return false;
    }
    const uint32_t end = (uint32_t)list[3] + (uint32_t)size3;
    if (end > MAX_OFFSET || end > module->size) {
        return false;
    }
    const uint16_t first_track = module_u16(module, list[0]);
    if (!module_has(module, first_track, 1) || module->data[first_track] != TRACK_LENGTH_MARKER) {
        return false;
    }
    for (uint32_t offset = list[0]; offset < end; offset += 2) {
        const uint16_t track = module_u16(module, offset);
        if (track > MAX_OFFSET || (track & 1) != 0 || track > list[0]) {
            return false;
        }
    }
    return true;
}

// The number of instrument records before the 0xFF terminator, or -1 when the table does not end inside the file or
// has no room in the replayer's sample pointer table.

static int32_t instrument_table_count(const PaModule* module) {
    for (uint32_t count = 0; count <= PA_MAX_INSTRUMENTS; count++) {
        const uint32_t offset = INSTRUMENT_BYTES * (count + 1);
        if (!module_has(module, offset, INSTRUMENT_BYTES)) {
            return -1;
        }
        if (module->data[offset] == INSTRUMENT_END) {
            return (int32_t)count;
        }
    }
    return -1;
}

// The checks of the song file alone; fills in the instrument count.

static bool module_ok(PaModule* module) {
    const int32_t count = instrument_table_count(module);
    if (count < 0 || !check2(module)) {
        return false;
    }
    module->instrument_count = (uint32_t)count;
    return pa_song_modeled(module);
}

bool pa_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > UINT32_MAX) {
        return false;
    }
    PaModule module = { .data = data, .size = (uint32_t)size };
    return module_ok(&module);
}

// InitPlayer's fix for one broken rip, found by the song's size: the length of its second instrument.

static void module_patch(uint8_t* data, const PaModule* module) {
    const uint32_t end
        = pa_module_list_start(module, 3) + (pa_module_list_start(module, 3) - pa_module_list_start(module, 2));
    PaInstrument instrument;
    if (end == PATCH_SONG_SIZE && pa_module_instrument(module, PATCH_INSTRUMENT, &instrument)
        && instrument.length_words == PATCH_BAD_LENGTH) {
        uint8_t* length = data + INSTRUMENT_BYTES * PATCH_INSTRUMENT;
        length[0] = PATCH_GOOD_LENGTH >> 8;
        length[1] = PATCH_GOOD_LENGTH & 0xFF;
    }
}

const int8_t* pa_samples_lc(const PaSamples* samples, uint32_t offset) {
    const uint32_t even = offset & ~1u;
    return samples->data != nullptr && even < samples->size ? (const int8_t*)samples->data + even : nullptr;
}

// InitPlayer lays the samples out one after the other. Its length register keeps the upper word between records, so
// after a length of 0x8000 words or more every later sample moves by at least 0x20000 bytes more.

void pa_module_sample_layout(const PaModule* module, uint32_t start[PA_MAX_INSTRUMENTS], uint32_t* end) {
    uint32_t length_register = 0;
    uint64_t offset = 0;
    for (uint32_t i = 0; i < module->instrument_count; i++) {
        PaInstrument instrument = { 0 };
        pa_module_instrument(module, i + 1, &instrument);
        length_register = (length_register & LENGTH_HIGH_MASK) | instrument.length_words;
        length_register += length_register;
        start[i] = offset > UINT32_MAX ? UINT32_MAX : (uint32_t)offset;
        offset += length_register;
    }
    *end = offset > UINT32_MAX ? UINT32_MAX : (uint32_t)offset;
}

// ExtLoad: "SMP.<name>" for a song called "PAP.<name>" (any case), else the song's name without its last character,
// both in the song's directory; when that file is missing, "SMP.set".

static bool has_sample_prefix(const char* name) {
    return nuade_strncasecmp(name, SAMPLE_PREFIX, SAMPLE_PREFIX_LENGTH) == 0;
}

static uint8_t* read_url(const NuadeIo* io, NuadeFileIndex* index, const char* name, uint64_t* size) {
    const char* url = nuade_file_index_find(index, name);
    if (url == nullptr) {
        return nullptr;
    }
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    *size = file.size;
    return file.data;
}

static uint8_t* read_sample_set(const char* path, const NuadeIo* io, uint64_t* size) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || base[0] == 0 || strlen(base) + sizeof(SAMPLE_SET_PREFIX) > sizeof(name)) {
        return nullptr;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    if (index == nullptr) {
        return nullptr;
    }
    if (has_sample_prefix(base)) {
        snprintf(name, sizeof(name), "%s%s", SAMPLE_SET_PREFIX, base + SAMPLE_PREFIX_LENGTH);
    } else {
        snprintf(name, sizeof(name), "%.*s", (int)strlen(base) - 1, base);
    }
    uint8_t* data = read_url(io, index, name, size);
    if (data == nullptr) {
        data = read_url(io, index, SAMPLE_SET_FALLBACK, size);
    }
    nuade_file_index_destroy(index);
    return data;
}

// The set must hold every sample, the cleared long of each, and the part of a looped instrument that repeats: the
// replayer does not check and would read the memory behind the set.

static bool loop_fits(const PaInstrument* instrument, uint32_t start, uint32_t set_size) {
    if (instrument->loop_length_words == PA_NO_LOOP_WORDS) {
        return true;
    }
    const uint32_t words = instrument->loop_length_words != 0 ? instrument->loop_length_words : MAX_LOOP_WORDS;
    const uint64_t loop_start = (uint64_t)start + instrument->loop_offset;
    return loop_start + 2u * words <= set_size;
}

static bool samples_layout(PaPlayer* player, uint32_t set_size) {
    PaSamples* samples = &player->samples;
    uint32_t end = 0;
    pa_module_sample_layout(&player->module, samples->start, &end);
    if (player->module.instrument_count > 0 && (set_size < CLEARED_BYTES || end > set_size)) {
        return false;
    }
    for (uint32_t i = 0; i < player->module.instrument_count; i++) {
        PaInstrument instrument;
        if (!pa_module_instrument(&player->module, i + 1, &instrument) || samples->start[i] > set_size - CLEARED_BYTES
            || !loop_fits(&instrument, samples->start[i], set_size)) {
            return false;
        }
        memset(samples->data + samples->start[i], 0, CLEARED_BYTES);
    }
    return true;
}

static PaStatus load_samples(PaPlayer* player, const char* path, const NuadeIo* io) {
    uint64_t size = 0;
    uint8_t* file = read_sample_set(path, io, &size);
    if (file == nullptr) {
        return PaStatus_MissingSamples;
    }
    PaStatus status = PaStatus_MissingSamples;
    if (size <= INT32_MAX) {
        player->samples.data = nuade_paula_padded_copy(file, (size_t)size);
        player->samples.size = (uint32_t)size;
        if (player->samples.data == nullptr) {
            status = PaStatus_OutOfMemory;
        } else if (samples_layout(player, (uint32_t)size)) {
            status = PaStatus_Ok;
        }
    }
    NuadeIo_free_url_to_memory(io, file);
    return status;
}

PaCreateResult pa_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    PaCreateResult result = { .status = PaStatus_InvalidData };
    if (data == nullptr || size > UINT32_MAX) {
        return result;
    }
    PaPlayer* player = calloc(1, sizeof(PaPlayer));
    if (player == nullptr) {
        result.status = PaStatus_OutOfMemory;
        return result;
    }
    PaModule* module = &player->module;
    player->module_copy = malloc(size);
    if (player->module_copy == nullptr) {
        pa_destroy(player);
        result.status = PaStatus_OutOfMemory;
        return result;
    }
    memcpy(player->module_copy, data, size);
    *module = (PaModule) { .data = player->module_copy, .size = (uint32_t)size };
    if (!module_ok(module)) {
        pa_destroy(player);
        return result;
    }
    module_patch(player->module_copy, module);
    result.status = load_samples(player, path, io);
    if (result.status != PaStatus_Ok) {
        pa_destroy(player);
        return result;
    }
    result.player = player;
    return result;
}

void pa_destroy(PaPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module_copy);
    free(player->samples.data);
    free(player);
}
