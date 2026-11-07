// SPDX-License-Identifier: MIT
// UFO player: the sample bank. The eagleplayer loads "<song>.BANK" for a song called "<song>.mus" (the extension in any
// case), and "SMP.set" from the song's directory when that is not there. The bank is
//   FORM <size> ADAT BODY <size>, 40 headers of 10 bytes (length, loop offset, 1 unused byte, volume), 0x54 unused
//   bytes, then the samples in header order.

#include "nuade_file_index.h"
#include "nuade_path.h"
#include "ufo_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define BANK_TABLE 0x14
#define BANK_ENTRY_SIZE 10
#define BANK_GAP 0x54
#define BANK_SAMPLES (BANK_TABLE + UFO_INSTRUMENTS * BANK_ENTRY_SIZE + BANK_GAP)
#define BANK_SUFFIX ".BANK"
#define BANK_FALLBACK_NAME "SMP.set"
#define SONG_EXTENSION_LENGTH 4 // ".mus"

typedef struct BankEntry {
    uint32_t length;
    uint32_t loop_offset;
    uint8_t volume;
} BankEntry;

static BankEntry read_bank_entry(const uint8_t* bank, unsigned index) {
    const uint8_t* p = bank + BANK_TABLE + index * BANK_ENTRY_SIZE;
    return (BankEntry) { .length = nuade_read_be32(p), .loop_offset = nuade_read_be32(p + 4), .volume = p[9] };
}

bool ufo_bank_load(UfoSong* song, uint8_t* module, uint8_t* bank, uint32_t bank_size) {
    if (bank_size < BANK_SAMPLES || memcmp(bank, "FORM", 4) != 0 || memcmp(bank + 8, "ADAT", 4) != 0
        || memcmp(bank + 12, "BODY", 4) != 0 || (uint64_t)8 + nuade_read_be32(bank + 4) > bank_size) {
        return false;
    }
    // the replayer clears FORM: the first four bytes are the silent sample of an instrument that has no loop
    memset(bank, 0, 4);
    song->silence = (const int8_t*)bank;
    uint64_t sample = BANK_SAMPLES;
    for (unsigned i = 0; i < UFO_INSTRUMENTS; i++) {
        const BankEntry e = read_bank_entry(bank, i);
        if (e.length == 0) {
            continue;
        }
        if (sample > UINT32_MAX
            || !ufo_instrument_make(&song->instruments[i], bank, bank_size, (uint32_t)sample, e.length, e.loop_offset,
                                    e.volume)) {
            return false;
        }
        sample += e.length;
    }
    for (unsigned i = 0; i < UFO_INSTRUMENTS; i++) {
        const UfoSongInstrument* s = &song->song_instruments[i];
        if (s->present
            && !ufo_instrument_make(&song->instruments[i], module, song->size, s->sample_offset, s->length,
                                    s->loop_offset, s->volume)) {
            return false;
        }
    }
    return true;
}

// "name.mus" -> "name.BANK" when the last four characters are ".mus" in any case and something precedes them.

static bool bank_name_for(const char* song_name, char* out, size_t cap) {
    const size_t length = strlen(song_name);
    if (length <= SONG_EXTENSION_LENGTH) {
        return false;
    }
    const char* ext = song_name + length - SONG_EXTENSION_LENGTH;
    if (ext[0] != '.' || tolower((unsigned char)ext[1]) != 'm' || tolower((unsigned char)ext[2]) != 'u'
        || tolower((unsigned char)ext[3]) != 's') {
        return false;
    }
    const size_t stem = length - SONG_EXTENSION_LENGTH;
    if (stem + sizeof(BANK_SUFFIX) > cap) {
        return false;
    }
    memcpy(out, song_name, stem);
    memcpy(out + stem, BANK_SUFFIX, sizeof(BANK_SUFFIX));
    return true;
}

uint8_t* ufo_bank_read(const char* path, const NuadeIo* io, uint32_t* size) {
    if (path == nullptr) {
        return nullptr;
    }
    char dir[1024];
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    if (index == nullptr) {
        return nullptr;
    }
    const char* url = nullptr;
    char name[256];
    if (bank_name_for(nuade_path_basename(path), name, sizeof(name))) {
        url = nuade_file_index_find(index, name);
    }
    if (url == nullptr) {
        url = nuade_file_index_find(index, BANK_FALLBACK_NAME);
    }
    uint8_t* padded = nullptr;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            if (file.size < UINT32_MAX / 2) {
                padded = nuade_paula_padded_copy(file.data, (size_t)file.size);
                *size = (uint32_t)file.size;
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return padded;
}
