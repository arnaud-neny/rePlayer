// SPDX-License-Identifier: MIT
// Paul Robotham: the song file (the eagleplayer's Check2 and InitPlayer) and its sample file (ExtLoad).

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "pr_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_SONG_BYTES 16u
#define ENVELOPE0_WORDS 127u // Check2 compares the first envelope table's start with this many words
#define ENVELOPE0_WORD 0x3F3F
#define SONG_SUFFIX ".dat"
#define SAMPLE_SUFFIX ".ssd"
#define FALLBACK_SAMPLES "mdtest.ssd"
#define SUFFIX_LENGTH 4

uint8_t pr_song_u8(const PrSong* song, uint32_t offset) {
    return offset < song->size ? song->data[offset] : 0;
}

static uint16_t song_u16(const PrSong* song, uint32_t offset) {
    return (uint16_t)(pr_song_u8(song, offset) << 8 | pr_song_u8(song, offset + 1));
}

static uint32_t song_u32(const PrSong* song, uint32_t offset) {
    return (uint32_t)song_u16(song, offset) << 16 | song_u16(song, offset + 2);
}

uint32_t pr_song_stream(const PrSong* song, uint32_t voice) {
    return song_u32(song, PR_HEADER_BYTES + 4 * voice);
}

uint32_t pr_song_vibrato(const PrSong* song, uint32_t table) {
    return song_u32(song, PR_HEADER_BYTES + 4 * ((uint32_t)song->voice_count + table));
}

uint32_t pr_song_envelope(const PrSong* song, uint32_t table) {
    return song_u32(song, PR_HEADER_BYTES + 4 * ((uint32_t)song->voice_count + song->vibrato_count + table));
}

PrInstrument pr_song_instrument(const PrSong* song, uint32_t number) {
    const uint32_t records
        = PR_HEADER_BYTES + 4 * ((uint32_t)song->voice_count + song->vibrato_count + song->envelope_count);
    const uint32_t at = records + PR_INSTRUMENT_BYTES * number;
    return (PrInstrument) { .sample = song_u32(song, at),
                            .length_words = song_u16(song, at + 4),
                            .loop = song_u32(song, at + 6),
                            .loop_length_words = song_u16(song, at + 10) };
}

uint32_t pr_song_end_marker(const PrSong* song) {
    return pr_song_stream(song, 1) - 1;
}

const int8_t* pr_song_sample(const PrSong* song, uint32_t offset) {
    offset &= ~1u;
    return offset < song->sample_size ? song->samples + offset : nullptr;
}

bool pr_stream_note_ok(uint8_t byte) {
    return byte == PR_STREAM_TIE || byte < PR_PERIOD_COUNT;
}

uint32_t pr_command_arg_count(uint32_t command) {
    switch (command) {
        case PrCommand_LoopStart:
        case PrCommand_VibratoDepth:
        case PrCommand_VibratoSpeed:
        case PrCommand_Portamento:
        case PrCommand_VibratoTable:
        case PrCommand_EnvelopeTable:
        case PrCommand_Volume:
        case PrCommand_Tempo:
        case PrCommand_Instrument:
        case PrCommand_Options:
        case PrCommand_Unused20:
        case PrCommand_Unused21:
        case PrCommand_FadeStep:
            return 1;
        case PrCommand_MasterFade:
            return 2;
        default:
            return 0;
    }
}

// Check2. Every read is bounded by the file: the eagleplayer reads on into memory.

static bool offset_table_ok(const PrSong* song, uint32_t from, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t offset = song_u32(song, from + 4 * i);
        if (offset == 0 || (offset & 0x80000001u) != 0) {
            return false;
        }
    }
    return true;
}

static bool check2(const PrSong* song) {
    const uint32_t voices = song->voice_count;
    if (voices == 0 || voices > PR_VOICES) {
        return false;
    }
    // The high byte of each count word must be 0 (counts below 256).
    for (uint32_t at = 2; at < PR_HEADER_BYTES; at += 2) {
        if (pr_song_u8(song, at) != 0) {
            return false;
        }
    }
    const uint32_t vibratos = song->vibrato_count;
    const uint32_t envelopes = song->envelope_count;
    const uint32_t instruments = song->instrument_count;
    if (vibratos == 0 || envelopes == 0) {
        return false; // a count of 0 makes the eagleplayer's loops run 65536 times
    }
    for (uint32_t i = 0; i < voices; i++) {
        const uint32_t at = PR_HEADER_BYTES + 4 * i;
        if (song_u16(song, at) != 0 || song_u32(song, at) == 0) {
            return false;
        }
    }
    const uint32_t vibrato_at = PR_HEADER_BYTES + 4 * voices;
    const uint32_t envelope_at = vibrato_at + 4 * vibratos;
    const uint32_t records_at = envelope_at + 4 * envelopes;
    if (!offset_table_ok(song, vibrato_at, vibratos) || !offset_table_ok(song, envelope_at, envelopes)) {
        return false;
    }
    // The first envelope table starts where the instrument records end, and holds 127 words of 0x3F3F.
    const uint32_t first_envelope = song_u32(song, envelope_at);
    if (first_envelope != records_at + PR_INSTRUMENT_BYTES * instruments || first_envelope > song->size
        || song->size - first_envelope < 2 * ENVELOPE0_WORDS) {
        return false;
    }
    for (uint32_t i = 0; i < ENVELOPE0_WORDS; i++) {
        if (song_u16(song, first_envelope + 2 * i) != ENVELOPE0_WORD) {
            return false;
        }
    }
    return true;
}

// The model's own limits: walk every stream the way the replayer reads it, and refuse what it does not model (a note
// past the period table, an instrument or table that does not exist, loops nested deeper than the loop stack). Loops
// only jump back to the position after their start command, so the linear walk sees every byte the replayer can read.

static bool stream_ok(const PrSong* song, uint32_t stream) {
    uint32_t depth = 0;
    uint32_t pos = stream;
    for (;;) {
        const uint8_t b = pr_song_u8(song, pos++);
        if (b == PR_STREAM_END) {
            return true;
        }
        if (b < PR_STREAM_COMMAND) {
            if (!pr_stream_note_ok(b)) {
                return false;
            }
            pos++; // duration
            continue;
        }
        const uint32_t command = b & PR_COMMAND_MASK;
        const uint8_t arg = pr_song_u8(song, pos);
        pos += pr_command_arg_count(command);
        switch (command) {
            case PrCommand_LoopStart:
                if (++depth > PR_LOOP_DEPTH) {
                    return false;
                }
                break;
            case PrCommand_LoopEnd:
                if (depth == 0) {
                    return false;
                }
                depth--;
                break;
            case PrCommand_VibratoTable:
                if (arg >= song->vibrato_count) {
                    return false;
                }
                break;
            case PrCommand_EnvelopeTable:
                if (arg >= song->envelope_count) {
                    return false;
                }
                break;
            case PrCommand_Instrument:
                if (arg >= song->instrument_count) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
}

static void song_parse_header(PrSong* song) {
    song->voice_count = song_u16(song, 0);
    song->vibrato_count = song_u16(song, 2);
    song->envelope_count = song_u16(song, 4);
    song->instrument_count = song_u16(song, 6);
}

static bool song_ok(PrSong* song) {
    if (song->size < MIN_SONG_BYTES) {
        return false;
    }
    song_parse_header(song);
    if (!check2(song)) {
        return false;
    }
    if (song->instrument_count == 0) {
        return false;
    }
    for (uint32_t voice = 0; voice < song->voice_count; voice++) {
        if (!stream_ok(song, pr_song_stream(song, voice))) {
            return false;
        }
    }
    return true;
}

bool pr_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > UINT32_MAX) {
        return false;
    }
    PrSong song = { .data = (uint8_t*)data, .size = (uint32_t)size };
    return song_ok(&song);
}

// InitPlayer wants the sample file to cover every instrument's first run: the furthest sample + 2 * length, taken as a
// signed long as the eagleplayer compares it.

static int32_t samples_needed(const PrSong* song) {
    int32_t needed = 0;
    for (uint32_t i = 0; i < song->instrument_count; i++) {
        const PrInstrument in = pr_song_instrument(song, i);
        const int32_t end = (int32_t)(in.sample + 2u * in.length_words);
        if (end > needed) {
            needed = end;
        }
    }
    return needed;
}

static bool has_suffix(const char* name, const char* suffix) {
    const size_t length = strlen(name);
    const size_t suffix_length = strlen(suffix);
    return length > suffix_length && nuade_strcasecmp(name + length - suffix_length, suffix) == 0;
}

// ExtLoad: "<name>.ssd" for a song called "<name>.dat", else (or when that file does not exist) "mdtest.ssd", both in
// the song's directory; a file that cannot be read is replaced by the fixed name too.

static uint8_t* read_url(const NuadeIo* io, NuadeFileIndex* index, const char* name, uint64_t* size) {
    const char* url = nuade_file_index_find(index, name);
    if (url == nullptr) {
        return nullptr;
    }
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    *size = file.size;
    return file.data;
}

static uint8_t* read_sample_file(const char* path, const NuadeIo* io, uint64_t* size) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || strlen(base) + 1 > sizeof(name)) {
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
    uint8_t* data = nullptr;
    if (has_suffix(base, SONG_SUFFIX)) {
        snprintf(name, sizeof(name), "%s", base);
        memcpy(name + strlen(name) - SUFFIX_LENGTH, SAMPLE_SUFFIX, SUFFIX_LENGTH + 1);
        data = read_url(io, index, name, size);
    }
    if (data == nullptr) { // missing or unreadable: the fixed name
        data = read_url(io, index, FALLBACK_SAMPLES, size);
    }
    nuade_file_index_destroy(index);
    return data;
}

static PrStatus load_samples(PrSong* song, const char* path, const NuadeIo* io) {
    uint64_t size = 0;
    uint8_t* file = read_sample_file(path, io, &size);
    if (file == nullptr) {
        return PrStatus_MissingSamples;
    }
    PrStatus status = PrStatus_MissingSamples;
    if (size <= INT32_MAX && (int64_t)size >= samples_needed(song)) {
        song->samples = (int8_t*)nuade_paula_padded_copy(file, (size_t)size);
        song->sample_size = (uint32_t)size;
        status = song->samples != nullptr ? PrStatus_Ok : PrStatus_OutOfMemory;
    }
    NuadeIo_free_url_to_memory(io, file);
    return status;
}

PrCreateResult pr_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    PrCreateResult result = { .status = PrStatus_InvalidData };
    if (data == nullptr || size > UINT32_MAX) {
        return result;
    }
    PrPlayer* player = calloc(1, sizeof(PrPlayer));
    if (player == nullptr) {
        result.status = PrStatus_OutOfMemory;
        return result;
    }
    PrSong* song = &player->song;
    song->data = malloc(size);
    if (song->data == nullptr) {
        pr_destroy(player);
        result.status = PrStatus_OutOfMemory;
        return result;
    }
    memcpy(song->data, data, size);
    song->size = (uint32_t)size;
    if (!song_ok(song)) {
        pr_destroy(player);
        return result;
    }
    result.status = load_samples(song, path, io);
    if (result.status != PrStatus_Ok) {
        pr_destroy(player);
        return result;
    }
    result.player = player;
    return result;
}

void pr_destroy(PrPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->song.data);
    free(player->song.samples);
    free(player);
}
