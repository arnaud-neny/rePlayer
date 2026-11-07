// SPDX-License-Identifier: MIT
// Synth Dream player: the memory the replayer sees, the module's records, the format check and the sample file.

#include "nuade_file_index.h"
#include "nuade_path.h"
#include "sdr_internal.h"
#include <stdio.h>
#include <string.h>

#define SUBSONG_TABLE 0x0C
#define SUBSONG_ENTRY_SIZE 16
#define MIN_HEADER_SIZE 0x1C // the header and the first subsong's four track offsets
#define FIXED_FIRST_TRACK 0x28
#define TRACK_END_LOOP 0xFEFF
#define TRACK_END 0xFFFF
#define SAMPLES_PREFIX "smp."
#define SAMPLES_DEFAULT "smp.set"
#define PREFIX_LENGTH 4
#define MODULE_PREFIX "sdr."

// UADE's memory as a song sees it. The chip memory is 2 MB; below ZERO_FROM it holds UADE's own data and the player,
// above it only the module and the sample file. From 0x200000 up it reads zeros (nothing, or the Kickstart area),
// except for the custom chips and the CIAs.
#define ZERO_FROM 0xFC00u
#define CHIP_END 0x200000u
#define SLOW_END 0xA00000u
#define KICKSTART_START 0xF80000u
#define SAMPLE_GAP 0x400u // the sample file starts this far behind the module's end, rounded down to 0x100
#define SAMPLE_ALIGN 0x100u
#define MAX_SAMPLE_BYTES 0x20000u // AUDxLEN 0 plays 65536 words

typedef enum SdrRegion {
    SdrRegion_Module,
    SdrRegion_Samples,
    SdrRegion_Zero,
    SdrRegion_Stray,
} SdrRegion;

static SdrRegion region_of(const SdrMemory* memory, uint32_t address) {
    if (address - SDR_MODULE_BASE < memory->module_size) {
        return SdrRegion_Module;
    }
    if (address - memory->sample_base < memory->sample_size
        || (memory->samples_unknown && address >= memory->sample_base && address < CHIP_END)) {
        return SdrRegion_Samples;
    }
    const bool zero = (address >= ZERO_FROM && address < SLOW_END) || address >= KICKSTART_START;
    return zero ? SdrRegion_Zero : SdrRegion_Stray;
}

uint8_t sdr_u8(SdrMemory* memory, uint32_t offset) {
    const uint32_t address = SDR_MODULE_BASE + offset;
    switch (region_of(memory, address)) {
        case SdrRegion_Module:
            return memory->module[offset];
        case SdrRegion_Samples:
            break;
        case SdrRegion_Zero:
            return 0;
        case SdrRegion_Stray:
            break;
    }
    memory->stray = true;
    return 0;
}

uint16_t sdr_u16(SdrMemory* memory, uint32_t offset) {
    return (uint16_t)(sdr_u8(memory, offset) << 8 | sdr_u8(memory, offset + 1));
}

uint32_t sdr_u32(SdrMemory* memory, uint32_t offset) {
    return (uint32_t)sdr_u16(memory, offset) << 16 | sdr_u16(memory, offset + 2);
}

// AUDxLC has no bit 0. The sound is read from the sample file, or from zeros; it has to end in the same region where
// it starts (a buffer that runs on into another region would play what the memory between holds).

const int8_t* sdr_sample_pointer(SdrMemory* memory, uint32_t offset, uint16_t length_words) {
    const uint32_t address = (memory->sample_base + offset) & ~1u;
    const uint32_t bytes = length_words != 0 ? (uint32_t)length_words * 2u : MAX_SAMPLE_BYTES;
    const uint64_t end = (uint64_t)address + bytes - 1;
    if (end < CHIP_END) {
        const uint32_t module_end = SDR_MODULE_BASE + memory->module_size;
        const uint32_t samples_end = memory->sample_base + memory->sample_size;
        // Behind the file the memory is zero up to the end of the chip memory (which wraps in UADE).
        if (address >= memory->sample_base && (memory->samples_unknown || address < samples_end)) {
            return memory->samples_unknown ? (const int8_t*)memory->module
                                           : (const int8_t*)memory->samples + (address - memory->sample_base);
        }
        const bool zero_gap = (address >= ZERO_FROM && end < SDR_MODULE_BASE)
                              || (address >= module_end && end < memory->sample_base)
                              || (!memory->samples_unknown && address >= samples_end);
        if (zero_gap) {
            return memory->samples_unknown ? (const int8_t*)memory->module
                                           : (const int8_t*)memory->samples + memory->sample_size; // the padding
        }
    }
    memory->stray = true;
    return nullptr;
}

SdrHeader sdr_read_header(SdrMemory* memory) {
    return (SdrHeader) {
        .instruments = sdr_u32(memory, 0),
        .volume_switches = sdr_u32(memory, 4),
        .pitch_switches = sdr_u32(memory, 8),
    };
}

// The subsong table sits between the header and the volume switch table.

int sdr_module_subsong_count(SdrMemory* memory) {
    const SdrHeader header = sdr_read_header(memory);
    if (header.volume_switches < MIN_HEADER_SIZE || header.volume_switches > memory->module_size) {
        return 0;
    }
    return (int)((header.volume_switches - SUBSONG_TABLE) / SUBSONG_ENTRY_SIZE);
}

uint32_t sdr_read_track_offset(SdrMemory* memory, uint32_t subsong, uint32_t voice) {
    return sdr_u32(memory, SUBSONG_TABLE + SUBSONG_ENTRY_SIZE * subsong + 4 * voice);
}

SdrTrackEntry sdr_read_track_entry(SdrMemory* memory, uint32_t offset) {
    return (SdrTrackEntry) {
        .commands = sdr_u32(memory, offset),
        .transpose = sdr_u8(memory, offset + 4),
        .repeat = sdr_u8(memory, offset + 5),
        .substitute_of = sdr_u8(memory, offset + 6),
        .substitute_by = sdr_u8(memory, offset + 7),
        .volume_cut = sdr_u8(memory, offset + 9),
    };
}

uint8_t sdr_track_first_byte(SdrMemory* memory, uint32_t offset) {
    return sdr_u8(memory, offset);
}

// The instrument number is shifted into a 16-bit word, which then counts as signed. A record that starts below the
// module and reads memory this port does not model (UADE's own data, which a real 68000 reads as an odd-address word
// or as nonsense) is a junk record: the note it belongs to is muted, and the record's fields are not read.

SdrInstrument sdr_read_instrument(SdrMemory* memory, uint32_t table, uint16_t number) {
    const uint32_t at = table + (uint32_t)(int32_t)(int16_t)(uint16_t)(number << 5);
    const bool below_module = (int32_t)at < 0;
    const bool stray_before = memory->stray;
    memory->stray = false;
    const SdrInstrument instrument = {
        .fixed_pitch = sdr_u8(memory, at + 1) != 0,
        .detune = sdr_u16(memory, at + 2),
        .volume_table = sdr_u32(memory, at + 4),
        .pitch_table = sdr_u32(memory, at + 8),
        .arpeggio = sdr_u32(memory, at + 12),
        .waveform = sdr_u32(memory, at + 16),
        .sample = {
            .start = sdr_u32(memory, at + 20),
            .length_words = sdr_u16(memory, at + 24),
            .loop_start = sdr_u32(memory, at + 26),
            .loop_length_words = sdr_u16(memory, at + 30),
        },
    };
    if (memory->stray && below_module) {
        memory->stray = stray_before;
        return (SdrInstrument) { .muted = true };
    }
    memory->stray |= stray_before;
    return instrument;
}

// A table offset of the header as Check2 reads it: a long whose upper word is 0 and whose lower word is a positive,
// even number.

static bool read_table_offset(SdrMemory* memory, uint32_t at, uint16_t* offset) {
    const uint16_t value = sdr_u16(memory, at + 2);
    if (sdr_u16(memory, at) != 0 || value == 0 || (value & 0x8000) != 0 || (value & 1) != 0) {
        return false;
    }
    *offset = value;
    return true;
}

// Check2, and InitPlayer's test that the module reaches past its volume switch table (a signed comparison).

bool sdr_check_module(SdrMemory* memory) {
    const uint32_t instruments = sdr_u32(memory, 0);
    if (memory->module_size <= instruments || instruments == 0 || (instruments & 0x80000000u) != 0
        || (instruments & 1) != 0 || sdr_u16(memory, instruments - 2) != 0xFFFF) {
        return false;
    }
    for (uint32_t at = 4; at <= 8; at += 4) {
        uint16_t offset = 0;
        if (!read_table_offset(memory, at, &offset) || offset % 4 != 0) {
            return false;
        }
    }
    for (uint32_t voice = 0; voice < SDR_VOICES; voice++) {
        uint16_t offset = 0;
        if (!read_table_offset(memory, SUBSONG_TABLE + 4 * voice, &offset)) {
            return false;
        }
        if (offset != FIXED_FIRST_TRACK) {
            const uint16_t before = sdr_u16(memory, (uint32_t)offset - 2);
            if (before != TRACK_END_LOOP && before != TRACK_END) {
                return false;
            }
        }
    }
    const SdrHeader header = sdr_read_header(memory);
    return sdr_module_subsong_count(memory) > 0 && memory->module_size >= 8
           && memory->module_size - 8 >= header.volume_switches
           && (int32_t)memory->module_size > (int32_t)sdr_u32(memory, header.volume_switches + 4);
}

bool sdr_name_matches(const char* filename) {
    return filename != nullptr && nuade_strncasecmp(nuade_path_basename(filename), MODULE_PREFIX, PREFIX_LENGTH) == 0;
}

// UADE puts the sample file behind the module.

uint32_t sdr_sample_base(uint32_t module_size) {
    return SDR_MODULE_BASE + ((module_size + SAMPLE_GAP) & ~(SAMPLE_ALIGN - 1));
}

// The sample file name: the song's with "sdr." changed to "smp."; a song without that prefix loses its last
// character instead.

static bool samples_file_name(const char* song, char* name, size_t capacity) {
    const size_t length = strlen(song);
    if (length >= PREFIX_LENGTH && nuade_strncasecmp(song, MODULE_PREFIX, PREFIX_LENGTH) == 0) {
        return (size_t)snprintf(name, capacity, SAMPLES_PREFIX "%s", song + PREFIX_LENGTH) < capacity;
    }
    if (length == 0 || length >= capacity) {
        return false;
    }
    memcpy(name, song, length - 1);
    name[length - 1] = '\0';
    return true;
}

static bool load_file(SdrMemory* memory, const NuadeIo* io, const char* url) {
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    if (file.data == nullptr) {
        return false;
    }
    bool ok = false;
    if (file.size <= CHIP_END) {
        memory->samples = nuade_paula_padded_copy(file.data, (size_t)file.size);
        memory->sample_size = (uint32_t)file.size;
        ok = memory->samples != nullptr;
    }
    NuadeIo_free_url_to_memory(io, file.data);
    return ok;
}

// ExtLoad: the sample file of the song, else "smp.set", from the song's directory. UADE puts it behind the module
// (sdr_sample_base).

bool sdr_load_samples(SdrMemory* memory, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char name[512];
    const char* song = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (song == nullptr || io == nullptr) {
        return false;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    if (index == nullptr) {
        return false;
    }
    bool ok = false;
    if (samples_file_name(song, name, sizeof(name))) {
        const char* url = nuade_file_index_find(index, name);
        ok = url != nullptr && load_file(memory, io, url);
    }
    if (!ok) {
        const char* url = nuade_file_index_find(index, SAMPLES_DEFAULT);
        ok = url != nullptr && load_file(memory, io, url);
    }
    nuade_file_index_destroy(index);
    return ok;
}
