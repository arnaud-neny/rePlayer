// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the file (a FORM "MMV8" or an old song with a separate instrument file) and the checks of
// the eagleplayers on it.

#include "mm_internal.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <string.h>

#define FORM_ID 0x464F524Du // "FORM"
#define MMV8_ID 0x4D4D5638u // "MMV8"
#define SDAT_ID 0x53444154u // "SDAT"
#define INST_ID 0x494E5354u // "INST"
#define PINS_ID 0x50494E53u // "PINS"

#define FORM_HEADER 12
#define CHUNK_HEADER 8
#define SDAT_HEADER 4 // a long in front of the song inside the chunk
#define OLD_SONG_EXTENSION ".sdata"
#define OLD_INSTRUMENTS_UNPACKED ".i"
#define OLD_INSTRUMENTS_PACKED ".ip"
#define MIN_INSTRUMENT_FILE 8

// The chunk lookup of the eagleplayers: chunks follow each other without padding, and the walk stops at the first
// chunk that starts at or past the end of the FORM. Returns whether the chunk was found (inside the file).

static bool find_chunk(const MmBlock* file, uint32_t id, MmBlock* chunk) {
    const uint64_t form_end = CHUNK_HEADER + (uint64_t)mm_u32(file, 4);
    uint64_t at = FORM_HEADER;
    do {
        if (at > file->size || file->size - at < CHUNK_HEADER) {
            return false;
        }
        const uint32_t length = mm_u32(file, (uint32_t)at + 4);
        if (mm_u32(file, (uint32_t)at) == id) {
            if (length > file->size - at - CHUNK_HEADER) {
                return false;
            }
            *chunk = (MmBlock) { .data = file->data + at + CHUNK_HEADER, .size = length };
            return true;
        }
        at += CHUNK_HEADER + (uint64_t)length;
    } while (at < form_end);
    return false;
}

// Whether `filename` is called "name.sdata" (without regard to case).

static bool has_old_song_name(const char* filename) {
    if (filename == nullptr) {
        return false;
    }
    const char* base = nuade_path_basename(filename);
    const size_t extension = strlen(OLD_SONG_EXTENSION);
    const size_t length = strlen(base);
    return length > extension && nuade_strcasecmp(base + length - extension, OLD_SONG_EXTENSION) == 0;
}

// What the file is made of: the song, the instrument chunk of a FORM, and whether that is packed.

typedef struct Parts {
    MmBlock song;
    MmBlock instruments; // a FORM's INST or PINS chunk; empty for an old song
    bool packed;
    bool iff;
} Parts;

// Check2: a FORM needs an SDAT chunk and an INST or PINS chunk; a bare song has to be called "name.sdata" (its
// instruments come from a second file).

static bool split_file(const NuadeSongSource* source, Parts* parts) {
    if (source->data == nullptr || source->size > UINT32_MAX) {
        return false;
    }
    const MmBlock file = { .data = source->data, .size = (uint32_t)source->size };
    *parts = (Parts) { 0 };
    if (mm_u32(&file, 0) == FORM_ID && mm_u32(&file, 8) == MMV8_ID) {
        MmBlock sdat;
        if (!find_chunk(&file, SDAT_ID, &sdat) || sdat.size < SDAT_HEADER) {
            return false;
        }
        parts->song = (MmBlock) { .data = sdat.data + SDAT_HEADER, .size = sdat.size - SDAT_HEADER };
        parts->iff = true;
        parts->packed = !find_chunk(&file, INST_ID, &parts->instruments);
        return !parts->packed || find_chunk(&file, PINS_ID, &parts->instruments);
    }
    parts->song = file;
    return has_old_song_name(source->path);
}

// The instruments of an old song: the file "name.i" (used as it is), else "name.ip" (packed), next to "name.sdata". The
// name is searched without regard to case; a bare file name looks in the current directory.

static bool load_old_instruments(const NuadeSongSource* source, MmInstruments* instruments) {
    if (source->path == nullptr || source->io == nullptr) {
        return false;
    }
    char dir[1024] = { 0 };
    char name[512];
    const char* base = nuade_path_basename(source->path);
    const size_t stem = strlen(base) - strlen(OLD_SONG_EXTENSION);
    if (!nuade_path_parent(source->path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), ".");
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(source->io, dirs, 1);
    if (index == nullptr) {
        return false;
    }
    bool ok = false;
    static const char* const suffixes[] = { OLD_INSTRUMENTS_UNPACKED, OLD_INSTRUMENTS_PACKED };
    for (int i = 0; i < 2 && !ok; i++) {
        if (stem + strlen(suffixes[i]) + 1 > sizeof(name)) {
            break;
        }
        snprintf(name, sizeof(name), "%.*s%s", (int)stem, base, suffixes[i]);
        const char* url = nuade_file_index_find(index, name);
        if (url == nullptr) {
            continue;
        }
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(source->io, url);
        if (file.data != nullptr) {
            const MmBlock block = { .data = file.data, .size = file.size <= UINT32_MAX ? (uint32_t)file.size : 0 };
            ok = block.size >= MIN_INSTRUMENT_FILE
                 && mm_instruments_open(instruments, &block, i == 1, false); // 4 voice: not halved
            NuadeIo_free_url_to_memory(source->io, file.data);
        }
        break; // the replayer takes the first name that exists
    }
    nuade_file_index_destroy(index);
    return ok;
}

bool mm_file_open(const NuadeSongSource* source, bool external, MmSong* song, MmInstruments* instruments,
                  MmVariant* variant) {
    Parts parts;
    if (!split_file(source, &parts) || !mm_song_identify(&parts.song, variant)) {
        return false;
    }
    // Only the 4 voice eagleplayer is reached by the ".sdata" name; it rejects an 8 voice song and UADE plays nothing
    if (!parts.iff && *variant != MmVariant_4V) {
        return false;
    }
    const bool halve = *variant == MmVariant_8V;
    *instruments = (MmInstruments) { 0 };
    if (parts.iff ? !mm_instruments_open(instruments, &parts.instruments, parts.packed, halve)
                  : external && !load_old_instruments(source, instruments)) {
        return false;
    }
    if (!mm_song_open(song, &parts.song, *variant)) {
        mm_instruments_close(instruments);
        return false;
    }
    return true;
}
