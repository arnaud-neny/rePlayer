// SPDX-License-Identifier: MIT
// Maximum Effect: the song file and the sample file, the checks of the eagleplayer and the walk that makes sure the
// replayer stays inside the files.

#include "mxe_internal.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECORD_BYTES 0x12
#define RECORD_FIRST 2 // the record table follows the word that counts the records
#define CHECK2_MIN_FIRST_LONG 0xF
#define CHECK2_VOICE_LISTS 3 // Check2 looks at the lists of the first record's voices 1 to 3
#define MAX_SUBSONGS 0xFFFFu
#define MAX_WALK_ITEMS 0x400000u
#define INSTRUMENT_BYTES 16
#define SAMPLES_FIRST 2 // the instrument table follows the word that counts the instruments
#define SAMPLES_MAX_COUNT 0x8000u
#define ONE_SHOT_FLAG 0x10
#define NOTE_KEEP_VOLUME 1

#define MODULE_PREFIX "MAX."
#define SAMPLES_PREFIX "SMP."
#define SHARED_SAMPLES "SMP.set"
#define PREFIX_LENGTH 4

static const uint16_t period_table[MXE_NOTE_COUNT]
    = { 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
        302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 };

static const int16_t sine_table[64]
    = { 0,    24,   49,   74,   97,   120,  141,  161,  180,  197,  212,  224,  235,  244,  250,  253,
        255,  253,  250,  244,  235,  224,  212,  197,  180,  161,  141,  120,  97,   74,   49,   24,
        0,    -24,  -49,  -74,  -97,  -120, -141, -161, -180, -197, -212, -224, -235, -244, -250, -253,
        -255, -253, -250, -244, -235, -224, -212, -197, -180, -161, -141, -120, -97,  -74,  -49,  -24 };

// `n` bytes at `offset` lie inside `size` bytes; written so that it cannot wrap.

static bool in_range(size_t size, uint64_t offset, uint64_t n) {
    return n <= size && offset <= size - n;
}

static uint16_t module_u16(const MxeModule* module, uint32_t offset) {
    return in_range(module->size, offset, 2) ? nuade_read_be16(module->bytes + offset) : 0;
}

static uint32_t module_u32(const MxeModule* module, uint32_t offset) {
    return in_range(module->size, offset, 4) ? nuade_read_be32(module->bytes + offset) : 0;
}

uint16_t mxe_period(uint8_t note_offset) {
    const uint32_t index = note_offset / 2u;
    return index < MXE_NOTE_COUNT ? period_table[index] : 0;
}

int16_t mxe_sine(uint8_t offset) {
    return sine_table[(offset / 2u) % nuade_sizeof_array(sine_table)];
}

// Check2 of one of the first record's voice lists: 0 (no list), or an even offset inside the file whose six bytes
// before it are the end of the previous list.

static bool check2_list(const MxeModule* module, uint32_t list) {
    if (list == 0) {
        return true;
    }
    return (list & 1) == 0 && list >= MXE_LIST_ENTRY_BYTES && list <= module->size
           && module_u32(module, list - MXE_LIST_ENTRY_BYTES) == 0;
}

// Check2 and the header part of InitPlayer. The record count in the first word (the replayer relocates that many
// records) must equal the count that the first list offset implies (the number of subsongs).

bool mxe_module_parse(MxeModule* module, const uint8_t* bytes, size_t size) {
    memset(module, 0, sizeof(*module));
    module->bytes = bytes;
    module->size = size;
    if (size < RECORD_FIRST + RECORD_BYTES || size > INT32_MAX) {
        return false;
    }
    if (module_u32(module, 0) < CHECK2_MIN_FIRST_LONG) {
        return false;
    }
    const uint32_t first_list = module_u32(module, 4);
    if ((first_list & 1) != 0 || first_list > size || first_list <= RECORD_FIRST
        || (first_list - RECORD_FIRST) % RECORD_BYTES != 0) {
        return false;
    }
    const uint32_t count = (first_list - RECORD_FIRST) / RECORD_BYTES;
    if (count > MAX_SUBSONGS || module_u16(module, 0) + 1u != count) {
        return false;
    }
    for (uint32_t i = 0; i < CHECK2_VOICE_LISTS; i++) {
        const uint32_t list = module_u32(module, 8 + 4 * i);
        if (list > size || !check2_list(module, list)) {
            return false;
        }
    }
    module->subsong_count = count;
    return true;
}

MxeRecord mxe_record(const MxeModule* module, uint32_t subsong) {
    const uint32_t at = RECORD_FIRST + subsong * RECORD_BYTES;
    const uint8_t mask = (uint8_t)module_u16(module, at); // the word is {priority, voice mask}
    MxeRecord record = { .one_shot = (mask & ONE_SHOT_FLAG) != 0 };
    for (uint32_t v = 0; v < MXE_VOICES; v++) {
        record.voice_used[v] = (mask >> v & 1) != 0;
        record.list[v] = module_u32(module, at + 2 + 4 * v);
    }
    return record;
}

MxeListEntry mxe_list_entry(const MxeModule* module, uint32_t offset) {
    const uint32_t pattern = module_u32(module, offset);
    return (MxeListEntry) { .end = pattern == 0, .pattern = pattern, .value = module_u16(module, offset + 4) };
}

// The commands that carry two parameter bytes behind their word.

static bool command_has_extra(MxeCommand command) {
    return command == MxeCommand_VibratoRestart || command == MxeCommand_Vibrato || command == MxeCommand_Portamento
           || command == MxeCommand_Arpeggio;
}

MxeRead mxe_read_item(const MxeModule* module, uint32_t* pos, bool first_of_event, MxeItem* item) {
    if (!in_range(module->size, *pos, 2)) {
        return MxeRead_Invalid;
    }
    const uint16_t word = nuade_read_be16(module->bytes + *pos);
    *pos += 2;
    *item = (MxeItem) { 0 };
    if ((word & 0x8000) != 0 && !first_of_event) {
        item->kind = MxeItem_Duration;
        item->duration = word;
        return MxeRead_Item;
    }
    if ((word & 0x8000) != 0) {
        item->kind = MxeItem_Note;
        item->instrument = (word >> 8) & 0x7F;
        item->note_offset = word & 0xFE;
        item->keep_volume = (word & NOTE_KEEP_VOLUME) != 0;
        return MxeRead_Item;
    }
    const uint32_t command = word >> 8;
    if (command >= MxeCommand_Count) {
        return MxeRead_Invalid; // the replayer would jump out of its command table
    }
    item->kind = MxeItem_Command;
    item->command = (MxeCommand)command;
    item->param = word & 0xFF;
    if (command_has_extra(item->command)) {
        if (!in_range(module->size, *pos, 2)) {
            return MxeRead_Invalid;
        }
        item->extra[0] = module->bytes[*pos];
        item->extra[1] = module->bytes[*pos + 1];
        *pos += 2;
    }
    return MxeRead_Item;
}

// A byte offset into the period table: even (the replayer reads words) and inside the table.

static bool period_offset_ok(uint8_t offset) {
    return (offset & 1) == 0 && offset < MXE_PERIOD_TABLE_BYTES;
}

static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        const uint32_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

// What one item may ask of the replayer.

static bool walk_item(MxeModule* module, const MxeItem* item) {
    switch (item->kind) {
        case MxeItem_Note:
            module->uses_instrument[item->instrument] = true;
            return period_offset_ok(item->note_offset);
        case MxeItem_Duration:
            module->grid_ticks = gcd(module->grid_ticks, (item->duration & 0x7FFFu) + 1u);
            return true;
        case MxeItem_Command:
            switch (item->command) {
                case MxeCommand_Portamento:
                    return period_offset_ok(item->param);
                case MxeCommand_Arpeggio:
                    return period_offset_ok(item->param) && period_offset_ok(item->extra[0])
                           && period_offset_ok(item->extra[1]);
                default:
                    return true;
            }
    }
    return false;
}

// The events of one list entry: `count` times note, commands, duration, from the start of the pattern.

static bool walk_pattern(MxeModule* module, const MxeListEntry* entry, uint32_t* budget) {
    if ((entry->pattern & 1) != 0 || entry->value == 0) {
        return false;
    }
    uint32_t pos = entry->pattern;
    for (uint32_t event = 0; event < entry->value; event++) {
        bool first = true;
        for (;;) {
            MxeItem item;
            if (*budget == 0 || mxe_read_item(module, &pos, first, &item) != MxeRead_Item
                || !walk_item(module, &item)) {
                return false;
            }
            (*budget)--;
            first = false;
            if (item.kind == MxeItem_Duration) {
                break;
            }
        }
    }
    return true;
}

// Relocation adds the load address to the pattern of every entry of every list, so a longword that two lists (or
// overlapping entries) share ends up with the address added twice. `relocated` marks the longwords done.

static bool mark_relocated(uint8_t* relocated, uint32_t offset) {
    for (uint32_t i = 0; i < 4; i++) {
        if (relocated[offset + i] != 0) {
            return false;
        }
        relocated[offset + i] = 1;
    }
    return true;
}

// A voice's position list: entries up to the end mark, each relocated once. A list that is played also has its
// patterns walked, and its end mark must send it back to an entry unless the record stops its voices.

static bool walk_list(MxeModule* module, uint8_t* relocated, uint32_t list, bool played, bool one_shot,
                      uint32_t* budget) {
    if (list == 0) {
        return !played;
    }
    if ((list & 1) != 0) {
        return false;
    }
    uint32_t at = list;
    for (;;) {
        if (at - list >= MXE_MAX_LIST_BYTES - MXE_LIST_ENTRY_BYTES
            || !in_range(module->size, at, MXE_LIST_ENTRY_BYTES)) {
            return false;
        }
        const MxeListEntry entry = mxe_list_entry(module, at);
        if (entry.end) {
            const uint32_t entries = (at - list) / MXE_LIST_ENTRY_BYTES;
            if (!played || one_shot) {
                return true;
            }
            return entries > 0 && entry.value % MXE_LIST_ENTRY_BYTES == 0
                   && entry.value / MXE_LIST_ENTRY_BYTES < entries;
        }
        if (!mark_relocated(relocated, at) || (played && !walk_pattern(module, &entry, budget))) {
            return false;
        }
        at += MXE_LIST_ENTRY_BYTES;
    }
}

bool mxe_module_validate(MxeModule* module) {
    uint8_t* relocated = calloc(module->size, 1);
    if (relocated == nullptr) {
        return false;
    }
    memset(module->uses_instrument, 0, sizeof(module->uses_instrument));
    module->grid_ticks = 0;
    uint32_t budget = MAX_WALK_ITEMS;
    bool ok = true;
    for (uint32_t subsong = 0; ok && subsong < module->subsong_count; subsong++) {
        const MxeRecord record = mxe_record(module, subsong);
        for (uint32_t v = 0; ok && v < MXE_VOICES; v++) {
            ok = walk_list(module, relocated, record.list[v], record.voice_used[v], record.one_shot, &budget);
        }
    }
    free(relocated);
    return ok && module->grid_ticks != 0;
}

// Sample file

static uint16_t samples_u16(const MxeSamples* samples, uint32_t offset) {
    return in_range(samples->size, offset, 2) ? nuade_read_be16(samples->bytes + offset) : 0;
}

static uint32_t samples_u32(const MxeSamples* samples, uint32_t offset) {
    return in_range(samples->size, offset, 4) ? nuade_read_be32(samples->bytes + offset) : 0;
}

MxeInstrument mxe_instrument(const MxeSamples* samples, uint32_t index) {
    const uint32_t at = SAMPLES_FIRST + index * INSTRUMENT_BYTES;
    return (MxeInstrument) { .start = samples_u32(samples, at),
                             .length_words = samples_u16(samples, at + 4),
                             .volume = samples_u16(samples, at + 6),
                             .loop_start = samples_u32(samples, at + 8),
                             .loop_words = samples_u16(samples, at + 12) };
}

// InitPlayer: the word at the start counts the instruments, the first sample follows the table, and the sample that
// the 16 bytes before it describe (the last instrument) must end inside the file.

bool mxe_samples_load(MxeSamples* samples, const uint8_t* bytes, size_t size) {
    memset(samples, 0, sizeof(*samples));
    if (size < SAMPLES_FIRST + 4 || size > INT32_MAX) {
        return false;
    }
    samples->bytes = nuade_paula_padded_copy(bytes, size);
    if (samples->bytes == nullptr) {
        return false;
    }
    samples->size = size;
    const uint32_t count = nuade_read_be16(bytes) + 1u;
    const uint32_t first_sample = samples_u32(samples, 2);
    bool ok = count <= SAMPLES_MAX_COUNT && in_range(size, SAMPLES_FIRST, (uint64_t)count * INSTRUMENT_BYTES)
              && first_sample >= INSTRUMENT_BYTES && first_sample <= size;
    if (ok) {
        const uint64_t end = (uint64_t)samples_u32(samples, first_sample - INSTRUMENT_BYTES)
                             + 2u * samples_u16(samples, first_sample - INSTRUMENT_BYTES + 4);
        ok = end <= size;
    }
    if (!ok) {
        mxe_samples_free(samples);
        return false;
    }
    samples->instrument_count = count;
    return true;
}

void mxe_samples_free(MxeSamples* samples) {
    free(samples->bytes);
    memset(samples, 0, sizeof(*samples));
}

// `words` words from `start`; AUDxLEN 0 plays 65536 words.

static bool sound_ok(const MxeSamples* samples, uint32_t start, uint16_t words) {
    const uint64_t bytes = words == 0 ? NUADE_PAULA_MAX_SAMPLE_BYTES : 2u * words;
    return in_range(samples->size, start, bytes);
}

bool mxe_samples_check(const MxeSamples* samples, const MxeModule* module) {
    for (uint32_t i = 0; i < MXE_MAX_INSTRUMENTS; i++) {
        if (!module->uses_instrument[i]) {
            continue;
        }
        if (i >= samples->instrument_count) {
            return false;
        }
        const MxeInstrument instrument = mxe_instrument(samples, i);
        if (!sound_ok(samples, instrument.start, instrument.length_words)
            || !sound_ok(samples, instrument.loop_start, instrument.loop_words)) {
            return false;
        }
    }
    return true;
}

// Reads `name` from the song's directory (any case). A bare file name has no directory: the current one.

static MxeLoad load_named(const char* song_path, const NuadeIo* io, const char* name, MxeSamples* samples) {
    char dir[1024] = { 0 };
    if (!nuade_path_parent(song_path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), ".");
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    MxeLoad result = MxeLoad_Missing;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            result = mxe_samples_load(samples, file.data, file.size) ? MxeLoad_Loaded : MxeLoad_Invalid;
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return result;
}

// ExtLoad: the song's name with "MAX." changed to "SMP."; a name of another form loses its last character. When that
// file cannot be read, "SMP.set" is read; a file that is read but refused by InitPlayer ends the load.

MxeLoad mxe_load_sample_file(const char* song_path, const NuadeIo* io, MxeSamples* samples) {
    const char* base = song_path != nullptr ? nuade_path_basename(song_path) : nullptr;
    if (base == nullptr || io == nullptr) {
        return MxeLoad_Missing;
    }
    char name[512];
    const size_t length = strlen(base);
    if (length >= sizeof(name)) {
        return MxeLoad_Missing;
    }
    if (length >= PREFIX_LENGTH && nuade_strncasecmp(base, MODULE_PREFIX, PREFIX_LENGTH) == 0) {
        snprintf(name, sizeof(name), SAMPLES_PREFIX "%s", base + PREFIX_LENGTH);
    } else {
        memcpy(name, base, length);
        name[length > 0 ? length - 1 : 0] = '\0';
    }
    if (name[0] != '\0') {
        const MxeLoad own = load_named(song_path, io, name, samples);
        if (own != MxeLoad_Missing) {
            return own;
        }
    }
    return load_named(song_path, io, SHARED_SAMPLES, samples);
}
