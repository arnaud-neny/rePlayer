// SPDX-License-Identifier: MIT
// Scott Johnston player: the instruments the eagleplayers load for a song. The ScottJohnston eagleplayer loads the
// 8SVX files its header names from "instruments"; the SoundPlayer eagleplayer loads one bundle of 8SVX forms.

#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "sj_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The replayer's silent word: where a voice without a loop repeats.
#define SILENT_WORD_BYTES 4

// Where the replayer looks for the chunks of an instrument file, from the start of its form.
#define VHDR_WINDOW 0x400
#define BODY_WINDOW 0x800
#define CHUNK_TAG_BYTES 4
#define CHUNK_LENGTH_BYTES 4
#define CHUNK_HEADER_BYTES (CHUNK_TAG_BYTES + CHUNK_LENGTH_BYTES)
#define VHDR_ONE_SHOT_OFFSET 8 // from the start of the tag
#define VHDR_REPEAT_OFFSET 12
#define VHDR_READ_BYTES 16
#define MAX_INSTRUMENT_FILE_BYTES 0x400000u
#define MAX_ARENA_BYTES 0x1000000u

// The SoundPlayer bundle: the forms back to back; its name is the song's with "smp" for "sjs".
#define BUNDLE_PREFIX "smp"
#define FORM_TAG "FORM"

// The replayer's search for a chunk tag: the first place in the first `window` bytes where the four bytes occur. The
// result is the offset behind the tag; false when there is no such tag (the replayer then reads other memory).

static bool find_chunk(const uint8_t* data, size_t size, const char tag[CHUNK_TAG_BYTES], size_t window,
                       size_t* after) {
    for (size_t at = 0; at < window && size >= CHUNK_TAG_BYTES && at <= size - CHUNK_TAG_BYTES; at++) {
        if (memcmp(data + at, tag, CHUNK_TAG_BYTES) == 0) {
            *after = at + CHUNK_TAG_BYTES;
            return true;
        }
    }
    return false;
}

// InstallSamples for one form: the slot of an 8SVX form that starts at `form_at` in `data`. `base` is where `data`
// sits in the arena. The data the replayer hands to Paula (the body up to the one-shot length, the loop) must lie in
// `data`.

static bool read_slot(const uint8_t* data, size_t size, size_t form_at, uint32_t base, SjSampleSlot* slot) {
    size_t vhdr = 0;
    size_t body = 0;
    if (!find_chunk(data + form_at, size - form_at, "VHDR", VHDR_WINDOW, &vhdr)
        || !find_chunk(data + form_at, size - form_at, "BODY", BODY_WINDOW, &body)) {
        return false;
    }
    const size_t header_start = form_at + vhdr - CHUNK_TAG_BYTES;
    if (size - header_start < VHDR_READ_BYTES || size - (form_at + body) < CHUNK_LENGTH_BYTES) {
        return false;
    }
    const uint32_t one_shot = nuade_read_be32(data + header_start + VHDR_ONE_SHOT_OFFSET);
    const uint32_t repeat = nuade_read_be32(data + header_start + VHDR_REPEAT_OFFSET);
    const size_t body_start = form_at + body - CHUNK_TAG_BYTES + CHUNK_HEADER_BYTES;
    // The lengths are halved into words of 16 bits, and the loop starts at the one-shot length as a signed word.
    const int64_t loop_start = (int64_t)body_start + (int16_t)(uint16_t)one_shot;
    slot->length_words = (uint16_t)(one_shot >> 1);
    slot->loop_length_words = (uint16_t)(repeat >> 1);
    const int64_t start_end = (int64_t)body_start + (int64_t)slot->length_words * 2;
    const int64_t loop_end = loop_start + (int64_t)slot->loop_length_words * 2;
    if (start_end > (int64_t)size || (slot->loop_length_words != 0 && (loop_start < 0 || loop_end > (int64_t)size))) {
        return false;
    }
    slot->start = base + (uint32_t)body_start;
    slot->loop_start = base + (uint32_t)(loop_start < 0 ? 0 : loop_start);
    return true;
}

// The index of directories next to the song (a bare file name: the current directory): the song's own directory for a
// null entry of `subdirs`, else the subdirectory of that name.

static NuadeFileIndex* index_directories(const NuadeIo* io, const char* path, const char* const* subdirs,
                                         size_t count) {
    char dir[1024] = { 0 };
    char joined[2][1100];
    const char* dirs[2];
    if (count > nuade_sizeof_array(dirs)) {
        return nullptr;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), ".");
    }
    for (size_t i = 0; i < count; i++) {
        if (subdirs[i] == nullptr) {
            snprintf(joined[i], sizeof(joined[i]), "%s", dir);
        } else if (!nuade_path_join(dir, subdirs[i], joined[i], sizeof(joined[i]))) {
            return nullptr;
        }
        dirs[i] = joined[i];
    }
    return nuade_file_index_create(io, dirs, count);
}

// Room for `needed` bytes in the arena.

static bool reserve_arena(SjSamples* samples, size_t* capacity, size_t needed) {
    if (needed <= *capacity) {
        return true;
    }
    size_t grown = *capacity * 2u;
    while (grown < needed) {
        grown *= 2u;
    }
    uint8_t* arena = realloc(samples->arena, grown);
    if (arena == nullptr) {
        return false;
    }
    memset(arena + *capacity, 0, grown - *capacity);
    samples->arena = arena;
    *capacity = grown;
    return true;
}

// One file of the ScottJohnston header: its slot, and its bytes appended to the arena.

static SjLoadStatus load_file(SjSamples* samples, size_t* capacity, const NuadeIo* io, const char* url,
                              SjSampleSlot* slot) {
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    if (file.data == nullptr) {
        return SjLoad_Missing;
    }
    SjLoadStatus status = SjLoad_Invalid;
    const size_t base = (samples->arena_size + 1u) & ~(size_t)1u;
    if (file.size <= MAX_INSTRUMENT_FILE_BYTES && base + file.size <= MAX_ARENA_BYTES
        && read_slot(file.data, (size_t)file.size, 0, (uint32_t)base, slot)) {
        if (reserve_arena(samples, capacity, base + (size_t)file.size)) {
            memcpy(samples->arena + base, file.data, (size_t)file.size);
            samples->arena_size = base + (size_t)file.size;
            status = SjLoad_Ok;
        } else {
            status = SjLoad_OutOfMemory;
        }
    }
    NuadeIo_free_url_to_memory(io, file.data);
    return status;
}

// The files back to back behind the silent word, then the padding Paula needs behind any sample.

static SjLoadStatus finish_arena(SjSamples* samples) {
    uint8_t* padded = nuade_paula_padded_copy(samples->arena, samples->arena_size);
    free(samples->arena);
    samples->arena = padded;
    return padded != nullptr ? SjLoad_Ok : SjLoad_OutOfMemory;
}

// The ScottJohnston eagleplayer's ExtLoad and InstallSamples: the files of the header from "instruments" next to the
// song. An entry of 0 repeats the file before it, as the replayer's list repeats the last pointer.

static SjLoadStatus load_header_files(SjSamples* samples, const SjModule* module, const char* path, const NuadeIo* io) {
    size_t capacity = 0x10000;
    samples->arena = calloc(1, capacity);
    if (samples->arena == nullptr) {
        return SjLoad_OutOfMemory;
    }
    samples->arena_size = SILENT_WORD_BYTES;
    const char* subdirs[] = { "instruments", "Instruments" };
    NuadeFileIndex* index = index_directories(io, path, subdirs, nuade_sizeof_array(subdirs));
    SjSampleSlot slot = { 0 };
    SjLoadStatus status = index != nullptr ? SjLoad_Ok : SjLoad_Missing;
    for (uint32_t i = 0; i < SJ_FILE_SLOTS && status == SjLoad_Ok; i++) {
        if (module->file_index[i] != 0) {
            const char* url = nuade_file_index_find(index, sj_instrument_name(module->file_index[i]));
            status = url != nullptr ? load_file(samples, &capacity, io, url, &slot) : SjLoad_Missing;
        }
        samples->slots[i] = slot;
    }
    nuade_file_index_destroy(index);
    return status == SjLoad_Ok ? finish_arena(samples) : status;
}

// The SoundPlayer eagleplayer's InstallSamples over the bundle: it steps through the bundle from the start, a form
// takes the next slot and the next step is behind it, anything else takes the next slot and the next step is 4 bytes
// on. At most SJ_INSTRUMENT_SLOTS slots.

static SjLoadStatus install_bundle(SjSamples* samples, const uint8_t* bundle, size_t size) {
    size_t at = 0;
    for (uint32_t slot = 0; slot < SJ_INSTRUMENT_SLOTS && at < size; slot++) {
        if (size - at < CHUNK_TAG_BYTES) {
            return SjLoad_Invalid;
        }
        if (memcmp(bundle + at, FORM_TAG, CHUNK_TAG_BYTES) != 0) {
            at += CHUNK_TAG_BYTES;
            continue;
        }
        if (size - at < CHUNK_HEADER_BYTES) {
            return SjLoad_Invalid;
        }
        const uint32_t form_bytes = nuade_read_be32(bundle + at + CHUNK_TAG_BYTES);
        if ((form_bytes & 1u) != 0 || form_bytes > size - at - CHUNK_HEADER_BYTES
            || !read_slot(bundle, size, at, SILENT_WORD_BYTES, &samples->slots[slot])) {
            return SjLoad_Invalid;
        }
        at += CHUNK_HEADER_BYTES + (size_t)form_bytes;
    }
    return SjLoad_Ok;
}

// The SoundPlayer eagleplayer's ExtLoad: "sjs.<name>" loads "smp.<name>" from the song's directory.

static SjLoadStatus load_bundle(SjSamples* samples, const char* path, const NuadeIo* io) {
    const char* song_name = nuade_path_basename(path);
    char bundle_name[1024];
    if (snprintf(bundle_name, sizeof(bundle_name), "%s%s", BUNDLE_PREFIX, song_name + strlen(SJS_EXTENSION))
        >= (int)sizeof(bundle_name)) {
        return SjLoad_Invalid;
    }
    const char* subdirs[] = { nullptr };
    NuadeFileIndex* index = index_directories(io, path, subdirs, nuade_sizeof_array(subdirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, bundle_name) : nullptr;
    const NuadeIoReadResult file = url != nullptr ? NuadeIo_read_url_to_memory(io, url) : (NuadeIoReadResult) { 0 };
    nuade_file_index_destroy(index);
    if (file.data == nullptr) {
        return SjLoad_Missing;
    }
    SjLoadStatus status = SjLoad_Invalid;
    if (file.size >= CHUNK_HEADER_BYTES && file.size <= MAX_ARENA_BYTES - SILENT_WORD_BYTES) {
        status = install_bundle(samples, file.data, (size_t)file.size);
    }
    if (status == SjLoad_Ok) {
        samples->arena_size = SILENT_WORD_BYTES + (size_t)file.size;
        samples->arena = calloc(1, samples->arena_size);
        if (samples->arena != nullptr) {
            memcpy(samples->arena + SILENT_WORD_BYTES, file.data, (size_t)file.size);
            status = finish_arena(samples);
        } else {
            status = SjLoad_OutOfMemory;
        }
    }
    NuadeIo_free_url_to_memory(io, file.data);
    return status;
}

SjLoadStatus sj_samples_load(SjSamples* samples, const SjModule* module, const char* path, const NuadeIo* io) {
    memset(samples, 0, sizeof(*samples));
    if (path == nullptr || io == nullptr) {
        return SjLoad_Missing;
    }
    const SjLoadStatus status = module->variant->format == SjFormat_SoundPlayer
                                    ? load_bundle(samples, path, io)
                                    : load_header_files(samples, module, path, io);
    if (status != SjLoad_Ok) {
        sj_samples_free(samples);
    }
    return status;
}

void sj_samples_free(SjSamples* samples) {
    free(samples->arena);
    memset(samples, 0, sizeof(*samples));
}
