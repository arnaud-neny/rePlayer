// SPDX-License-Identifier: MIT
// Quartet: the song file (the eagleplayer's Check2 and InitPlayer) and its sample set (ExtLoad).

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "qt_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_BYTES 2 // tempo and the 0x50 byte
#define SONG_MARK 0x50
#define MAX_TEMPO 0x1E
#define TEMPO_DIVIDEND 3000 // Check2 wants the tempo to divide this
#define END_WORD 0xFFFF
#define END_WORDS 4 // the file ends with this many END_WORDs, followed by at most MAX_TRAILING_ZERO_WORDS zero words
#define MAX_TRAILING_ZERO_WORDS 15 // Check2 reads at most 16 words from the end
#define END_LONG 0xFFFFFFFFu
#define LONG_BYTES 4
#define GLIDE_MARKER 0xFE
#define LOOP_MIN_PAGES 2 // the replayer restarts a song whose end is more than this many 256-byte pages in
#define LOOP_END_BIAS 6  // InitPlayer takes (offset + 6) >> 8
#define OCTAVE_SHIFT 6
#define VOLUME_CODE_MASK 0x3F
#define NIBBLE_BITS 4
#define NIBBLE_MASK 0x0F
#define SET_HEADER_BYTES 0x162
#define SET_LOCK_OFFSET 4
#define SET_LOCK 0x6C6F636Bu // "lock"
#define SET_LOOP_MASK_OFFSET 0x150
#define SET_LENGTHS_FROM_END 0x60
#define SET_PAIRS_FROM_END 0x40
#define SET_MIN_BYTES (SET_HEADER_BYTES + SET_LENGTHS_FROM_END) // header and the two tables
#define SET_MIN_WORDS 0x32 // a sample of at most this many words plays this many and takes no room
#define SET_SLACK_WORDS 2  // every sample is followed by two words
#define PAIR_BYTES 4
#define SONG_PREFIX "QPA."
#define SONG_PREFIX_LENGTH 4
#define SAMPLE_SET_PREFIX "SMP."
#define SAMPLE_SET_FALLBACK "SMP.set"

static bool module_has(const QtModule* module, uint64_t offset, uint32_t bytes) {
    return module->size >= bytes && offset <= module->size - bytes;
}

static uint64_t voice_word_offset(uint32_t row, uint32_t voice) {
    return HEADER_BYTES + (uint64_t)QT_ROW_BYTES * row + 2u * voice;
}

bool qt_module_voice_word(const QtModule* module, uint32_t row, uint32_t voice, QtVoiceWord* out) {
    const uint64_t offset = voice_word_offset(row, voice);
    if (!module_has(module, offset, 2)) {
        return false;
    }
    const uint8_t first = module->data[offset];
    const uint8_t second = module->data[offset + 1];
    *out = (QtVoiceWord) { .octave = first >> OCTAVE_SHIFT,
                           .volume_code = first & VOLUME_CODE_MASK,
                           .sample = second >> NIBBLE_BITS,
                           .note = second & NIBBLE_MASK,
                           .plays = first != 0 && second != GLIDE_MARKER,
                           .glide_marker = second == GLIDE_MARKER };
    return true;
}

// The replayer tests the four words in order and stops at the first END_WORD.

bool qt_module_row_ends(const QtModule* module, uint32_t row, bool* ends) {
    for (uint32_t voice = 0; voice < QT_VOICES; voice++) {
        const uint64_t offset = voice_word_offset(row, voice);
        if (!module_has(module, offset, 2)) {
            return false;
        }
        if (nuade_read_be16(module->data + offset) == END_WORD) {
            *ends = true;
            return true;
        }
    }
    *ends = false;
    return true;
}

// Check2: the tempo byte, the 0x50 byte, and four END_WORDs at the end of the file (behind at most 15 zero words).

static bool check2(const QtModule* module) {
    if (module->size < HEADER_BYTES) {
        return false;
    }
    const uint8_t tempo = module->data[0];
    if (tempo == 0 || tempo > MAX_TEMPO || TEMPO_DIVIDEND % tempo != 0 || module->data[1] != SONG_MARK) {
        return false;
    }
    uint32_t end = module->size & ~1u;
    for (uint32_t zero_words = 0; zero_words <= MAX_TRAILING_ZERO_WORDS; zero_words++) {
        if (end < 2) {
            return false;
        }
        end -= 2;
        const uint16_t word = nuade_read_be16(module->data + end);
        if (word == END_WORD) {
            return end >= 2 * END_WORDS - 2 && nuade_read_be32(module->data + end - 2) == END_LONG
                   && nuade_read_be32(module->data + end - 6) == END_LONG;
        }
        if (word != 0) {
            return false;
        }
    }
    return false;
}

// InitPlayer looks for the first long of ones, in steps of four from the first row: a song whose end is far enough
// in is restarted by the replayer when it reaches its end. Check2 guarantees that the long exists.

static int16_t restart_pages(uint32_t offset) {
    return (int16_t)(uint16_t)(((offset + LOOP_END_BIAS) >> 8) + 1); // the tick compares the low word, signed
}

static bool module_scan_end(QtModule* module) {
    for (uint32_t offset = HEADER_BYTES; module_has(module, offset, LONG_BYTES); offset += LONG_BYTES) {
        if (nuade_read_be32(module->data + offset) == END_LONG) {
            module->loops = restart_pages(offset) > LOOP_MIN_PAGES;
            return true;
        }
    }
    return false;
}

static bool module_ok(QtModule* module) {
    if (!check2(module) || !module_scan_end(module)) {
        return false;
    }
    module->tempo = module->data[0];
    const QtSamples none = { 0 };
    return qt_song_modeled(module, &none);
}

bool qt_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > UINT32_MAX) {
        return false;
    }
    QtModule module = { .data = data, .size = (uint32_t)size };
    return module_ok(&module);
}

const int8_t* qt_samples_lc(const QtSamples* samples, uint32_t offset, uint16_t length_words) {
    const uint32_t even = offset & ~1u;
    const uint64_t bytes = length_words != 0 ? 2u * length_words : NUADE_PAULA_MAX_SAMPLE_BYTES;
    if (samples->data == nullptr || even > samples->size || bytes > samples->size - even) {
        return nullptr;
    }
    return (const int8_t*)samples->data + even;
}

// InitPlayer: the set is accepted when its size is exactly what its 16 length words add up to.

bool qt_samples_parse(QtSamples* samples, const uint8_t* file, uint32_t size) {
    if (size <= SET_MIN_BYTES || (size & 1) != 0 || nuade_read_be32(file + SET_LOCK_OFFSET) != SET_LOCK) {
        return false;
    }
    const uint8_t* lengths = file + size - SET_LENGTHS_FROM_END;
    const uint8_t* pairs = file + size - SET_PAIRS_FROM_END;
    uint64_t words = 0;
    for (uint32_t i = 0; i < QT_SAMPLES; i++) {
        const uint16_t length = nuade_read_be16(lengths + 2 * i);
        if (length != 0) {
            words += (uint64_t)length + SET_SLACK_WORDS;
        }
    }
    if (2 * words - 2 + SET_MIN_BYTES != size) {
        return false;
    }
    samples->size = size;
    samples->loop_mask = nuade_read_be16(file + SET_LOOP_MASK_OFFSET);
    uint32_t start = SET_HEADER_BYTES;
    for (uint32_t i = 0; i < QT_SAMPLES; i++) {
        const uint16_t length = nuade_read_be16(lengths + 2 * i);
        samples->entry[i] = (QtSampleEntry) { .start = start,
                                              .length_words = length > SET_MIN_WORDS ? length : SET_MIN_WORDS,
                                              .offset = nuade_read_be16(pairs + PAIR_BYTES * i),
                                              .length_override = nuade_read_be16(pairs + PAIR_BYTES * i + 2) };
        if (length > SET_MIN_WORDS) {
            start += (uint16_t)(2u * ((uint32_t)length + SET_SLACK_WORDS)); // the replayer's advance is a word
        }
    }
    return true;
}

// ExtLoad: "SMP.<name>" for a song called "QPA.<name>" (any case), else the song's name without its last character,
// both in the song's directory; when that file is missing, "SMP.set".

static bool has_song_prefix(const char* name) {
    return nuade_strncasecmp(name, SONG_PREFIX, SONG_PREFIX_LENGTH) == 0;
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
    if (has_song_prefix(base)) {
        snprintf(name, sizeof(name), "%s%s", SAMPLE_SET_PREFIX, base + SONG_PREFIX_LENGTH);
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

static QtStatus load_samples(QtPlayer* player, const char* path, const NuadeIo* io) {
    uint64_t size = 0;
    uint8_t* file = read_sample_set(path, io, &size);
    if (file == nullptr) {
        return QtStatus_MissingSamples;
    }
    QtStatus status = QtStatus_MissingSamples;
    if (size <= INT32_MAX && qt_samples_parse(&player->samples, file, (uint32_t)size)) {
        player->samples.data = nuade_paula_padded_copy(file, (size_t)size);
        status = player->samples.data != nullptr ? QtStatus_Ok : QtStatus_OutOfMemory;
    }
    NuadeIo_free_url_to_memory(io, file);
    return status;
}

QtCreateResult qt_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    QtCreateResult result = { .status = QtStatus_InvalidData };
    if (data == nullptr || size > UINT32_MAX) {
        return result;
    }
    QtPlayer* player = calloc(1, sizeof(QtPlayer));
    if (player == nullptr) {
        result.status = QtStatus_OutOfMemory;
        return result;
    }
    player->module_copy = malloc(size);
    if (player->module_copy == nullptr) {
        qt_destroy(player);
        result.status = QtStatus_OutOfMemory;
        return result;
    }
    memcpy(player->module_copy, data, size);
    player->module = (QtModule) { .data = player->module_copy, .size = (uint32_t)size };
    if (!module_ok(&player->module)) {
        qt_destroy(player);
        return result;
    }
    result.status = load_samples(player, path, io);
    if (result.status == QtStatus_Ok && !qt_song_modeled(&player->module, &player->samples)) {
        result.status = QtStatus_InvalidData;
    }
    if (result.status != QtStatus_Ok) {
        qt_destroy(player);
        return result;
    }
    result.player = player;
    return result;
}

void qt_destroy(QtPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module_copy);
    free(player->samples.data);
    free(player);
}
