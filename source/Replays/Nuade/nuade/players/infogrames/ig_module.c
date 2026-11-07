// SPDX-License-Identifier: MIT
// Infogrames player: the file check, the loading of the song and of its ".ins" sample file, and the readers of the
// file's records.

#include "ig_internal.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_EXTENSION ".ins"
#define SONG_EXTENSION ".dum"
#define SEGMENT_SIZE 3 // repeats, delta, ticks
#define ENVELOPE_CHECK_FIRST_BYTE 0x00
#define ENVELOPE_CHECK_SECOND_BYTE 0x0F
#define DEFAULT_TIMER 0x1A00

// The eagleplayer's table of known songs (the sum of their longs, their size) and the timer it sets for them. Any
// other song runs at DEFAULT_TIMER.
typedef struct KnownSong {
    uint32_t checksum;
    uint32_t size;
    uint16_t timer;
} KnownSong;

static const KnownSong known_songs[] = {
    { 0x752AB846, 2850, 0x24FF }, { 0xC0691337, 3634, 0x24FF }, { 0xE19FE7A1, 4211, 0x24FF },
    { 0x0B0F2EBA, 3305, 0x24FF }, { 0xABDFF091, 3433, 0x24FF }, { 0xC55B87E3, 2605, 0x24FF },
    { 0xF8AC972A, 2180, 0x24FF }, { 0xBFF9C6C1, 3234, 0x1B66 }, { 0xCD059F22, 3160, 0x24FF },
    { 0x0990733E, 2614, 0x24FF }, { 0xEB38A5F3, 2183, 0x24FF }, { 0xB3E5589E, 2179, 0x24FF },
};

// The notes' periods, from the table at the end of the eagleplayer's code. The words behind the last note belong to
// other tables of the eagleplayer.
static const uint16_t periods[] = {
    27340, 25804, 24357, 22990, 21699, 20483, 19334, 18247, 17222, 16267, 15347, 14482, 13672, 12905, 12179, 11498,
    10854, 10241, 9574,  9125,  8623,  8132,  7678,  7246,  6844,  6454,  6092,  5750,  5427,  5121,  4836,  4565,
    4308,  4067,  3838,  3622,  3419,  3227,  3045,  2875,  2715,  2562,  2418,  2281,  2153,  2033,  1919,  1811,
    1709,  1613,  1522,  1437,  1357,  1280,  1208,  1141,  1077,  1016,  959,   906,   854,   806,   761,   719,
    678,   640,   604,   570,   538,   508,   480,   453,   427,   403,   381,   359,   339,   320,   302,   285,
    269,   254,   240,   226,   214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   112,
    96,    80,    64,    48,    32,    16,    0,     0,     32,    8224,  8224,  8224,  8224,  12336, 12336, 12320,
    8224,  8224,  8224,  8224,  8224,  8224,  8224,  8224,  8336,  16448, 16448, 16448, 16448, 16448, 16448, 16448,
    16396, 3084,  3084,  3084,  3084,  3136,  16448, 16448, 16448, 2313,  2313,  2313,  257,   257,   257,   257,
    257,   257,   257,   257,   257,   257,   16448, 16448, 16448, 2570,  2570,  2570,  514,   514,   514,   514,
    514,   514,   514,   514,   514,   514,   16448, 16448, 8192,
};

uint16_t ig_period(unsigned index) {
    return index < nuade_sizeof_array(periods) ? periods[index] : 0;
}

uint8_t ig_u8(const IgFile* f, int32_t offset) {
    return offset >= 0 && (uint32_t)offset < f->size ? f->data[offset] : 0;
}

uint16_t ig_u16(const IgFile* f, int32_t offset) {
    return (uint16_t)(ig_u8(f, offset) << 8 | ig_u8(f, offset + 1));
}

int16_t ig_s16(const IgFile* f, int32_t offset) {
    return (int16_t)ig_u16(f, offset);
}

uint32_t ig_u32(const IgFile* f, int32_t offset) {
    return (uint32_t)ig_u16(f, offset) << 16 | ig_u16(f, offset + 2);
}

IgSongHeader ig_read_song_header(const IgFile* song) {
    const int32_t song_count = ig_u16(song, 0) / 2;
    const int32_t base = ig_s16(song, (song_count - 1) * 2);
    IgSongHeader h = { .base = base, .tempo = (uint8_t)ig_u16(song, base) };
    h.amplitude_envelopes = base + ig_s16(song, base + 2);
    h.pitch_envelopes = base + ig_s16(song, base + 4);
    for (int i = 0; i < IG_VOICES; i++) {
        h.track_lists[i] = base + ig_s16(song, base + 6 + 2 * i);
    }
    h.pattern_offsets = base + 16;
    return h;
}

IgEnvelopeRecord ig_read_envelope_record(const IgFile* song, int32_t table, unsigned index) {
    const int32_t at = table + (int32_t)(index * IG_ENVELOPE_RECORD_SIZE);
    const uint8_t first = ig_u8(song, at);
    return (IgEnvelopeRecord) { .loops = (first & 0x80) != 0, .loop_step = (int8_t)(first & 0x7F), .segments = at + 1 };
}

IgEnvelopeSegment ig_read_envelope_segment(const IgFile* song, int32_t segments, unsigned index) {
    const int32_t at = segments + (int32_t)(index * SEGMENT_SIZE);
    return (IgEnvelopeSegment) { .repeats = ig_u8(song, at),
                                 .delta = (int8_t)ig_u8(song, at + 1),
                                 .ticks = ig_u8(song, at + 2) };
}

bool ig_read_sample(const IgFile* samples, unsigned index, IgSample* out) {
    const uint64_t at = IG_SAMPLE_HEADER_SIZE + (uint64_t)index * IG_SAMPLE_ENTRY_SIZE;
    if (at + IG_SAMPLE_ENTRY_SIZE > samples->size) {
        return false;
    }
    const int32_t entry = (int32_t)at;
    *out = (IgSample) { .start = ig_u32(samples, entry),
                        .repeat_start = ig_u32(samples, entry + 4),
                        .length_words = ig_u16(samples, entry + 12),
                        .repeat_length_words = ig_u16(samples, entry + 14) };
    return true;
}

bool ig_probe(const uint8_t* data, size_t size) {
    if (size < 2 || size > INT32_MAX) {
        return false;
    }
    const uint32_t first = nuade_read_be16(data);
    if (first == 0 || (first & 1) != 0 || first >= size || first > INT16_MAX) {
        return false;
    }
    const IgFile song = { .data = (uint8_t*)(uintptr_t)data, .size = (uint32_t)size };
    const int32_t header = (int32_t)first;
    const int32_t envelope = header + ig_s16(&song, header + 2);
    return envelope >= 0 && (uint32_t)envelope + 1 < size && ig_u8(&song, envelope) == ENVELOPE_CHECK_FIRST_BYTE
           && ig_u8(&song, envelope + 1) == ENVELOPE_CHECK_SECOND_BYTE;
}

// The sum of the song's longs; the last, partial one counts only the bytes that are in the file.

uint32_t ig_song_tick_length(const uint8_t* data, size_t size) {
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 4 <= size; i += 4) {
        sum += nuade_read_be32(data + i);
    }
    for (size_t shift = 24; i < size; i++, shift -= 8) {
        sum += (uint32_t)data[i] << shift;
    }
    for (size_t k = 0; k < nuade_sizeof_array(known_songs); k++) {
        if (known_songs[k].checksum == sum && known_songs[k].size == size) {
            return known_songs[k].timer;
        }
    }
    return DEFAULT_TIMER;
}

// ExtLoad: the file name with a trailing ".dum" (any case) cut off, plus ".ins". When that file does not load, the same
// with one more character cut off.

bool ig_sample_file_names(const char* song_name, char names[IG_CANDIDATES][IG_NAME_MAX], int* count) {
    size_t stem = strlen(song_name);
    const size_t extension = sizeof(SONG_EXTENSION) - 1;
    if (stem >= extension && nuade_strncasecmp(song_name + stem - extension, SONG_EXTENSION, extension) == 0) {
        stem -= extension;
    }
    if (stem == 0 || stem + sizeof(SAMPLE_EXTENSION) > IG_NAME_MAX) {
        return false;
    }
    *count = 0;
    for (size_t length = stem; length >= 1 && *count < IG_CANDIDATES; length--) {
        snprintf(names[*count], IG_NAME_MAX, "%.*s" SAMPLE_EXTENSION, (int)length, song_name);
        (*count)++;
    }
    return true;
}

static bool load_samples(IgFile* samples, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char names[IG_CANDIDATES][IG_NAME_MAX];
    int name_count = 0;
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || !ig_sample_file_names(base, names, &name_count)) {
        return false;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    bool ok = false;
    for (int i = 0; index != nullptr && i < name_count && !ok; i++) {
        const char* url = nuade_file_index_find(index, names[i]);
        if (url == nullptr) {
            continue;
        }
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            if (file.size >= IG_SAMPLE_HEADER_SIZE && file.size <= INT32_MAX) {
                samples->data = nuade_paula_padded_copy(file.data, file.size);
                samples->size = (uint32_t)file.size;
                ok = samples->data != nullptr;
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return ok;
}

IgCreateResult ig_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    if (!ig_probe(data, size)) {
        return (IgCreateResult) { .status = IgStatus_InvalidData };
    }
    IgPlayer* p = (IgPlayer*)calloc(1, sizeof(*p));
    if (p == nullptr) {
        return (IgCreateResult) { .status = IgStatus_OutOfMemory };
    }
    p->module.data = (uint8_t*)malloc(size);
    if (p->module.data == nullptr) {
        free(p);
        return (IgCreateResult) { .status = IgStatus_OutOfMemory };
    }
    memcpy(p->module.data, data, size);
    p->module.size = (uint32_t)size;
    if (!load_samples(&p->samples, path, io)) {
        ig_destroy(p);
        return (IgCreateResult) { .status = IgStatus_MissingSamples };
    }
    p->tick_length = ig_song_tick_length(data, size);
    return (IgCreateResult) { .player = p, .status = IgStatus_Ok };
}

void ig_destroy(IgPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player->samples.data);
    free(player);
}

uint32_t ig_tick_length(const IgPlayer* player) {
    return player->tick_length;
}
