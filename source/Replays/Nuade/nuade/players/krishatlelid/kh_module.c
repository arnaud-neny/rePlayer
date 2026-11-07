// SPDX-License-Identifier: MIT
// Kris Hatlelid: the song file. It is an Amiga executable of three hunks (stub or driver, song data, one long of bss);
// the eagleplayer loads it with LoadSeg and, for a song without a driver, loads "songplay" the same way. Both are
// loaded here into one memory at KH_LOAD_BASE, relocated, and checked against the known driver versions.

#include "kh_internal.h"
#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hunk file format
#define HUNK_HEADER 0x3F3u
#define HUNK_CODE 0x3E9u
#define HUNK_DATA 0x3EAu
#define HUNK_BSS 0x3EBu
#define HUNK_RELOC32 0x3ECu
#define HUNK_SYMBOL 0x3F0u
#define HUNK_DEBUG 0x3F1u
#define HUNK_END 0x3F2u
#define HUNK_SIZE_MASK 0x3FFFFFFFu
#define HUNK_MEMORY_FLAGS 0xC0000000u
#define HUNK_CHIP_FLAG 0x40000000u
#define MAX_EXECUTABLE_BYTES 0x400000u

// The file name of an external driver
#define DRIVER_FILE "songplay"

// What the eagleplayer's Check2 looks for in the first hunk of the song
#define LAYOUT_BRA_W 0x60000016u
#define LAYOUT_MARK 0x0000ABCDu
#define LAYOUT_DISPATCH 0xB07C0000u // cmp.w #0,d0: the first instruction of a driver
#define LAYOUT_LEA_ABSOLUTE_A0 0x41F90000u
#define LAYOUT_RTS 0x00004E75u
#define LAYOUT_DISPATCH_OFFSET 0x18u
#define LAYOUT_STUB_RTS_OFFSET 0x1Cu
#define LAYOUT_STUB_OPERAND_OFFSET 0x1Au
#define LAYOUT_SHORT_OPERAND_OFFSET 2u
#define LAYOUT_HEADER_BYTES 40u // ten longs: the hunk header up to the first hunk's code
#define LAYOUT_MIN_BYTES (LAYOUT_HEADER_BYTES + 0x20u)

// A driver in the song: its data hunk starts with the driver's variables and tables, the song follows
#define EMBEDDED_SONG_OFFSET 0x3D2u

// Chunks of an 8SVX
#define TAG_VHDR 0x56484452u
#define TAG_BODY 0x424F4459u
#define CHUNK_HEADER_BYTES 8u
#define VHDR_BYTES 0x0Fu
#define OCTAVE_SPAN_SEMITONES 83u // the driver divides this by the number of octaves
#define MAX_INSTRUMENT_BYTES 0x30u

// The song's records
#define MAX_SUBSONGS 1024u
#define MAX_INSTRUMENTS 256u

#define FNV_OFFSET 0xCBF29CE484222325ull
#define FNV_PRIME 0x100000001B3ull

typedef struct KhExecutable {
    KhMemory memory;
    uint32_t base[KH_MAX_HUNKS];
    const uint8_t* body[KH_MAX_HUNKS];
    uint32_t body_bytes[KH_MAX_HUNKS];
    uint32_t type[KH_MAX_HUNKS];
} KhExecutable;

// Memory

static bool memory_has(const KhMemory* memory, uint32_t address, uint32_t bytes) {
    const uint64_t offset = (uint64_t)address - KH_LOAD_BASE;
    return address >= KH_LOAD_BASE && offset + bytes <= memory->size;
}

uint8_t kh_u8(const KhMemory* memory, uint32_t address) {
    return memory_has(memory, address, 1) ? memory->bytes[address - KH_LOAD_BASE] : 0;
}

uint16_t kh_u16(const KhMemory* memory, uint32_t address) {
    return (uint16_t)((kh_u8(memory, address) << 8) | kh_u8(memory, address + 1));
}

int16_t kh_s16(const KhMemory* memory, uint32_t address) {
    return (int16_t)kh_u16(memory, address);
}

uint32_t kh_u32(const KhMemory* memory, uint32_t address) {
    return ((uint32_t)kh_u16(memory, address) << 16) | kh_u16(memory, address + 2);
}

void kh_put_u16(KhMemory* memory, uint32_t address, uint16_t value) {
    if (memory_has(memory, address, 2)) {
        memory->bytes[address - KH_LOAD_BASE] = (uint8_t)(value >> 8);
        memory->bytes[address - KH_LOAD_BASE + 1] = (uint8_t)value;
    }
}

void kh_put_u32(KhMemory* memory, uint32_t address, uint32_t value) {
    kh_put_u16(memory, address, (uint16_t)(value >> 16));
    kh_put_u16(memory, address + 2, (uint16_t)value);
}

const int8_t* kh_chip_pointer(const KhMemory* memory, uint32_t address) {
    return memory_has(memory, address, 1) ? (const int8_t*)(memory->bytes + (address - KH_LOAD_BASE)) : nullptr;
}

// Byte offsets of the fields of an instrument record and of a VHDR chunk

typedef enum RecordField {
    RecordField_Vhdr = 0x00,
    RecordField_Body = 0x04,
    RecordField_OctaveSpan = 0x08,
    RecordField_Form = 0x0A,
    RecordField_VibratoMax = 0x12,
    RecordField_VibratoMin = 0x14,
    RecordField_VibratoStep = 0x16,
    RecordField_VibratoRate = 0x18,
    RecordField_VibratoDelay = 0x1A,
    RecordField_VibratoCounter = 0x1C,
    RecordField_VibratoOn = 0x1E,
    RecordField_ArpeggioOn = 0x22,
    RecordField_Arpeggio = 0x24,
    RecordField_ArpeggioPos = 0x2C,
    RecordField_Transpose = 0x2E,
} RecordField;

typedef enum VhdrField {
    VhdrField_OneShot = 0x00,
    VhdrField_Repeat = 0x04,
    VhdrField_Cycle = 0x08,
    VhdrField_Octaves = 0x0E,
} VhdrField;

// Records

KhInstrument kh_instrument_read(const KhMemory* memory, uint32_t address) {
    KhInstrument instrument = {
        .vhdr = kh_u32(memory, address + RecordField_Vhdr),
        .body = kh_u32(memory, address + RecordField_Body),
        .octave_span = kh_u16(memory, address + RecordField_OctaveSpan),
        .form = kh_u32(memory, address + RecordField_Form),
        .vibrato_max = kh_u16(memory, address + RecordField_VibratoMax),
        .vibrato_min = kh_u16(memory, address + RecordField_VibratoMin),
        .vibrato_step = kh_s16(memory, address + RecordField_VibratoStep),
        .vibrato_rate = kh_u16(memory, address + RecordField_VibratoRate),
        .vibrato_delay = kh_u16(memory, address + RecordField_VibratoDelay),
        .vibrato_counter = kh_u16(memory, address + RecordField_VibratoCounter),
        .vibrato_on = kh_u16(memory, address + RecordField_VibratoOn),
        .arpeggio_on = kh_u16(memory, address + RecordField_ArpeggioOn),
        .arpeggio_pos = kh_u16(memory, address + RecordField_ArpeggioPos),
        .transpose = kh_s16(memory, address + RecordField_Transpose),
    };
    for (uint32_t i = 0; i < KH_ARPEGGIO_STEPS; i++) {
        instrument.arpeggio[i] = kh_u16(memory, address + RecordField_Arpeggio + 2 * i);
    }
    return instrument;
}

void kh_instrument_write_vibrato_counter(KhMemory* memory, uint32_t address, uint16_t value) {
    kh_put_u16(memory, address + RecordField_VibratoCounter, value);
}

void kh_instrument_write_arpeggio_pos(KhMemory* memory, uint32_t address, uint16_t value) {
    kh_put_u16(memory, address + RecordField_ArpeggioPos, value);
}

void kh_instrument_write_iff(KhMemory* memory, uint32_t address, uint32_t vhdr, uint32_t body, uint16_t octave_span) {
    kh_put_u32(memory, address + RecordField_Vhdr, vhdr);
    kh_put_u32(memory, address + RecordField_Body, body);
    kh_put_u16(memory, address + RecordField_OctaveSpan, octave_span);
}

KhVhdr kh_vhdr_read(const KhMemory* memory, uint32_t address) {
    return (KhVhdr) {
        .one_shot = kh_u32(memory, address + VhdrField_OneShot),
        .repeat = kh_u32(memory, address + VhdrField_Repeat),
        .cycle = kh_u32(memory, address + VhdrField_Cycle),
        .octaves = kh_u8(memory, address + VhdrField_Octaves),
    };
}

KhListEntry kh_list_entry_read(const KhMemory* memory, uint32_t list, uint32_t offset) {
    // The driver adds the low word of the offset, sign extended
    const uint32_t address = list + (uint32_t)(int32_t)(int16_t)offset;
    return (KhListEntry) { .stream = kh_u32(memory, address), .transpose = kh_s16(memory, address + 4) };
}

uint32_t kh_song_list(const KhMemory* memory, uint32_t song, uint32_t voice) {
    return kh_u32(memory, song + 4 * voice);
}

uint32_t kh_song_instrument(const KhMemory* memory, uint32_t song, uint32_t index) {
    return kh_u32(memory, song + KH_INSTRUMENT_TABLE + 4 * index);
}

// Driver versions. The signature covers the whole code hunk with the offsets of the variables in it, and the data hunk
// with the tables and the initial variables.

static const KhVariant kVariants[] = {
    {
        .name = "first (Fiendish Freddy's, Wings of Fury)",
        .signature = 0xDD386F0FB2F8BD8Cull,
        .code_bytes = 0xA88,
        .data_bytes = 988,
        .external = true,
        .tempo = KhTempo_HighLow,
    },
    {
        .name = "second (The Cycles)",
        .signature = 0x562B49D154A31943ull,
        .code_bytes = 0xAD8,
        .data_bytes = 1020,
        .external = true,
        .tempo = KhTempo_HighLow,
    },
    {
        .name = "compact (Super C, Hardball 2)",
        .signature = 0x0CE2577E040043B8ull,
        .code_bytes = 0xB08,
        .data_bytes = 1020,
        .external = true,
        .compact_notes = true,
        .prepare_first = true,
        .duration_formula = true,
        .tempo = KhTempo_High,
    },
    {
        .name = "stop (Teenage Mutant Ninja Turtles)",
        .signature = 0x26D3D74B674098EEull,
        .code_bytes = 0xB24,
        .data_bytes = 1020,
        .external = true,
        .compact_notes = true,
        .prepare_first = true,
        .stop_song_command = true,
        .tempo = KhTempo_HighScaled,
    },
    {
        .name = "embedded (Test Drive 2)",
        .signature = 0x3B9AB89D038C7E36ull,
        .code_bytes = 0xB80,
        .data_bytes = EMBEDDED_SONG_OFFSET,
        .tempo = KhTempo_HighLow,
    },
    {
        .name = "embedded (Grand Prix Circuit)",
        .signature = 0xB53F0867B5E56F99ull,
        .code_bytes = 0xB80,
        .data_bytes = EMBEDDED_SONG_OFFSET,
        .tempo = KhTempo_HighLow,
    },
};

static uint64_t fnv1a(uint64_t hash, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        hash = (hash ^ bytes[i]) * FNV_PRIME;
    }
    return hash;
}

// The version whose code and data hunk the executable has, or null.

static const KhVariant* find_variant(const KhExecutable* executable, bool external) {
    for (size_t i = 0; i < nuade_sizeof_array(kVariants); i++) {
        const KhVariant* variant = &kVariants[i];
        if (variant->external != external || executable->body_bytes[0] != variant->code_bytes
            || executable->body_bytes[1] < variant->data_bytes
            || (external && executable->body_bytes[1] != variant->data_bytes)) {
            continue;
        }
        uint64_t hash = fnv1a(FNV_OFFSET, executable->body[0], executable->body_bytes[0]);
        hash = fnv1a(hash, executable->body[1], variant->data_bytes);
        if (hash == variant->signature) {
            return variant;
        }
    }
    return nullptr;
}

// Loading an executable

typedef struct Reader {
    const uint8_t* data;
    size_t size;
    size_t at;
    bool failed;
} Reader;

static uint32_t read_long(Reader* reader) {
    if (reader->failed || reader->size - reader->at < 4) {
        reader->failed = true;
        return 0;
    }
    const uint32_t value = nuade_read_be32(reader->data + reader->at);
    reader->at += 4;
    return value;
}

// Skip `count` longs.

static void skip_longs(Reader* reader, uint64_t count) {
    if (reader->failed || count > (reader->size - reader->at) / 4) {
        reader->failed = true;
        return;
    }
    reader->at += (size_t)count * 4;
}

// The blocks after a hunk's body up to HUNK_END: relocations are applied, symbols and debug data are skipped.

static bool read_hunk_tail(Reader* reader, KhExecutable* executable, uint32_t hunk) {
    for (;;) {
        const uint32_t block = read_long(reader);
        if (reader->failed) {
            return false;
        }
        if (block == HUNK_END) {
            return true;
        }
        if (block == HUNK_RELOC32) {
            for (;;) {
                const uint32_t count = read_long(reader);
                if (reader->failed) {
                    return false;
                }
                if (count == 0) {
                    break;
                }
                const uint32_t target = read_long(reader);
                if (target >= KH_MAX_HUNKS) {
                    return false;
                }
                for (uint32_t i = 0; i < count; i++) {
                    const uint32_t offset = read_long(reader);
                    if (reader->failed || executable->type[hunk] == HUNK_BSS || offset > executable->body_bytes[hunk]
                        || executable->body_bytes[hunk] - offset < 4) {
                        return false;
                    }
                    const uint32_t at = executable->base[hunk] + offset;
                    kh_put_u32(&executable->memory, at, kh_u32(&executable->memory, at) + executable->base[target]);
                }
            }
        } else if (block == HUNK_SYMBOL) {
            for (;;) {
                const uint32_t name_longs = read_long(reader);
                if (reader->failed) {
                    return false;
                }
                if (name_longs == 0) {
                    break;
                }
                skip_longs(reader, (uint64_t)name_longs + 1);
            }
        } else if (block == HUNK_DEBUG) {
            skip_longs(reader, read_long(reader));
        } else {
            return false;
        }
    }
}

// The three hunks the eagleplayer's check requires: code, code or data, bss. The memory is allocated from the table of
// sizes in the header; a hunk's body may not be bigger than its entry.

static bool parse_executable(KhExecutable* executable, const uint8_t* data, size_t size) {
    Reader reader = { .data = data, .size = size };
    uint32_t alloc_bytes[KH_MAX_HUNKS];
    if (read_long(&reader) != HUNK_HEADER || read_long(&reader) != 0) {
        return false;
    }
    const uint32_t table = read_long(&reader);
    const uint32_t first = read_long(&reader);
    const uint32_t last = read_long(&reader);
    if (reader.failed || table != KH_MAX_HUNKS || first != 0 || last != KH_MAX_HUNKS - 1) {
        return false;
    }
    uint64_t total = 0;
    for (uint32_t i = 0; i < KH_MAX_HUNKS; i++) {
        const uint32_t entry = read_long(&reader);
        if ((entry & HUNK_MEMORY_FLAGS) == HUNK_MEMORY_FLAGS) {
            return false;
        }
        alloc_bytes[i] = (entry & HUNK_SIZE_MASK) * 4;
        executable->base[i] = KH_LOAD_BASE + (uint32_t)total;
        total += alloc_bytes[i];
        if (reader.failed || total > MAX_EXECUTABLE_BYTES) {
            return false;
        }
    }
    executable->memory.size = (uint32_t)total;
    executable->memory.bytes = nuade_paula_padded_copy(nullptr, total);
    if (executable->memory.bytes == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < KH_MAX_HUNKS; i++) {
        const uint32_t type = read_long(&reader);
        const uint32_t body_bytes = (read_long(&reader) & HUNK_SIZE_MASK) * 4;
        if (reader.failed || (type != HUNK_CODE && type != HUNK_DATA && type != HUNK_BSS)
            || body_bytes > alloc_bytes[i]) {
            return false;
        }
        executable->type[i] = type;
        executable->body_bytes[i] = body_bytes;
        if (type != HUNK_BSS) {
            if (body_bytes > reader.size - reader.at) {
                return false;
            }
            executable->body[i] = data + reader.at;
            memcpy(executable->memory.bytes + (executable->base[i] - KH_LOAD_BASE), data + reader.at, body_bytes);
            reader.at += body_bytes;
        }
        if (!read_hunk_tail(&reader, executable, i)) {
            return false;
        }
    }
    // Code, then code or data with a body (the driver is hashed from it), then bss
    return executable->type[0] == HUNK_CODE && executable->type[1] != HUNK_BSS && executable->type[2] == HUNK_BSS;
}

// On failure nothing stays allocated.

static bool load_executable(KhExecutable* executable, const uint8_t* data, size_t size) {
    *executable = (KhExecutable) { 0 };
    if (parse_executable(executable, data, size)) {
        return true;
    }
    free(executable->memory.bytes);
    *executable = (KhExecutable) { 0 };
    return false;
}

static void free_executable(KhExecutable* executable) {
    free(executable->memory.bytes);
    executable->memory.bytes = nullptr;
}

// The eagleplayer's Check2: the header of the executable, then the first instructions of the code hunk.

KhLayout kh_layout(const uint8_t* data, size_t size) {
    if (data == nullptr || size < LAYOUT_MIN_BYTES) {
        return KhLayout_None;
    }
    Reader reader = { .data = data, .size = size };
    const bool header = read_long(&reader) == HUNK_HEADER && read_long(&reader) == 0
                        && read_long(&reader) == KH_MAX_HUNKS && read_long(&reader) == 0
                        && read_long(&reader) == KH_MAX_HUNKS - 1;
    const uint32_t first_size = read_long(&reader);
    const uint32_t second_size = read_long(&reader);
    const bool sizes = (second_size & HUNK_MEMORY_FLAGS) == HUNK_CHIP_FLAG && read_long(&reader) == 1
                       && read_long(&reader) == HUNK_CODE && read_long(&reader) == (first_size & ~HUNK_CHIP_FLAG);
    if (!header || !sizes || reader.failed) {
        return KhLayout_None;
    }
    const uint8_t* code = data + reader.at;
    if (nuade_read_be32(code) == LAYOUT_BRA_W) {
        if (nuade_read_be32(code + 4) != LAYOUT_MARK) {
            return KhLayout_None;
        }
        const uint32_t at_dispatch = nuade_read_be32(code + LAYOUT_DISPATCH_OFFSET);
        if (at_dispatch == LAYOUT_DISPATCH) {
            return KhLayout_Embedded;
        }
        if (at_dispatch == LAYOUT_LEA_ABSOLUTE_A0 && nuade_read_be32(code + LAYOUT_STUB_RTS_OFFSET) == LAYOUT_RTS) {
            return KhLayout_External;
        }
        return KhLayout_None;
    }
    if (nuade_read_be32(code) == LAYOUT_LEA_ABSOLUTE_A0 && nuade_read_be32(code + 4) == LAYOUT_RTS) {
        return KhLayout_ExternalShort;
    }
    return KhLayout_None;
}

// The first instruction address of the stub in the code hunk loads the song table: `lea table,a0; rts`.

static uint32_t stub_table(const KhExecutable* executable, KhLayout layout) {
    const uint32_t operand = layout == KhLayout_External ? LAYOUT_STUB_OPERAND_OFFSET : LAYOUT_SHORT_OPERAND_OFFSET;
    return kh_u32(&executable->memory, executable->base[0] + operand);
}

uint8_t kh_driver_u8(const KhModule* module, uint32_t offset) {
    return offset < module->driver_data_bytes ? module->driver_data[offset] : 0;
}

uint32_t kh_driver_u32(const KhModule* module, uint32_t offset) {
    return ((uint32_t)kh_driver_u8(module, offset) << 24) | ((uint32_t)kh_driver_u8(module, offset + 1) << 16)
           | ((uint32_t)kh_driver_u8(module, offset + 2) << 8) | kh_driver_u8(module, offset + 3);
}

static void keep_driver_data(KhModule* module, const KhExecutable* executable) {
    module->driver_data_bytes
        = executable->body_bytes[1] < KH_DRIVER_DATA_BYTES ? executable->body_bytes[1] : KH_DRIVER_DATA_BYTES;
    memcpy(module->driver_data, executable->body[1], module->driver_data_bytes);
}

// The subsongs of a song with an external driver: the entries of the table of song addresses, up to the first song
// (which follows the table) or a zero.

static uint32_t count_subsongs(const KhMemory* memory, uint32_t table) {
    const uint32_t first = kh_u32(memory, table);
    uint32_t count = 0;
    uint32_t at = table;
    while (count < MAX_SUBSONGS && memory_has(memory, at, 4)) {
        const uint32_t song = kh_u32(memory, at);
        at += 4;
        if (song == 0) {
            break;
        }
        count++;
        if (at == first) {
            break;
        }
    }
    return count;
}

uint32_t kh_module_song(const KhModule* module, const KhMemory* memory, uint32_t subsong) {
    if (module->external) {
        return kh_u32(memory, module->song_table + 4 * subsong);
    }
    return module->embedded_song;
}

// The search for a chunk tag, a word at a time from `from`; the driver would run on through memory. Returns the
// address of the chunk's data, 0 if the tag is not in the memory or the work budget is used up.

static uint32_t find_chunk(const KhMemory* memory, uint32_t from, uint32_t tag, uint64_t* budget) {
    for (uint64_t at = from; at < (uint64_t)KH_LOAD_BASE + memory->size; at += 2) {
        if (*budget == 0) {
            return 0;
        }
        (*budget)--;
        if (memory_has(memory, (uint32_t)at, 4) && kh_u32(memory, (uint32_t)at) == tag) {
            return (uint32_t)at + CHUNK_HEADER_BYTES;
        }
    }
    return 0;
}

uint32_t kh_first_prepared_instrument(const KhVariant* variant) {
    // A song whose driver is not loaded yet is checked for the entries every version prepares
    return variant != nullptr && variant->prepare_first ? 0 : 1;
}

bool kh_prepare_instruments(KhMemory* memory, uint32_t song, uint32_t first, uint64_t* budget) {
    // The instrument command takes a byte: a record further down the table is never used
    for (uint32_t index = first; index < MAX_INSTRUMENTS; index++) {
        const uint32_t record = kh_song_instrument(memory, song, index);
        if (record == 0) {
            return true;
        }
        if (!memory_has(memory, record, MAX_INSTRUMENT_BYTES)) {
            return false;
        }
        const uint32_t form = kh_instrument_read(memory, record).form;
        if (form == 0) {
            continue;
        }
        const uint32_t vhdr = (form & 1) == 0 ? find_chunk(memory, form, TAG_VHDR, budget) : 0;
        if (vhdr == 0 || !memory_has(memory, vhdr, VHDR_BYTES)) {
            return false;
        }
        const uint32_t octaves = kh_vhdr_read(memory, vhdr).octaves;
        const uint32_t body = octaves != 0 ? find_chunk(memory, vhdr, TAG_BODY, budget) : 0;
        if (body == 0 || octaves > OCTAVE_SPAN_SEMITONES) {
            return false;
        }
        kh_instrument_write_iff(memory, record, vhdr, body, (uint16_t)(OCTAVE_SPAN_SEMITONES / octaves));
    }
    return true;
}

// The song start has to work for every subsong.

static bool songs_prepare(const KhModule* module) {
    const uint32_t first = kh_first_prepared_instrument(module->variant);
    KhMemory copy = { .bytes = malloc(module->song.size), .size = module->song.size };
    if (copy.bytes == nullptr) {
        return false;
    }
    uint64_t budget = KH_PREPARE_BUDGET_WORDS;
    bool ok = true;
    for (uint32_t subsong = 0; ok && subsong < module->subsong_count; subsong++) {
        memcpy(copy.bytes, module->song.bytes, copy.size);
        ok = kh_prepare_instruments(&copy, kh_module_song(module, &copy, subsong), first, &budget);
    }
    free(copy.bytes);
    return ok;
}

// ExtLoad: the driver is the file "songplay" in the song's directory.

static KhStatus load_driver(KhModule* module, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    if (path == nullptr || io == nullptr) {
        return KhStatus_MissingDriver;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, DRIVER_FILE) : nullptr;
    KhStatus status = KhStatus_MissingDriver;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            KhExecutable driver;
            status = KhStatus_InvalidData;
            if (file.size <= MAX_EXECUTABLE_BYTES && load_executable(&driver, file.data, (size_t)file.size)) {
                module->variant = find_variant(&driver, true);
                if (module->variant != nullptr) {
                    keep_driver_data(module, &driver);
                    status = KhStatus_Ok;
                }
                free_executable(&driver);
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return status;
}

KhStatus kh_module_load(KhModule* module, const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    // io is null for the probe, which checks the song side of the file only
    *module = (KhModule) { 0 };
    const KhLayout layout = kh_layout(data, size);
    KhExecutable song;
    if (layout == KhLayout_None || !load_executable(&song, data, size)) {
        return KhStatus_InvalidData;
    }
    KhStatus status = KhStatus_InvalidData;
    module->external = layout != KhLayout_Embedded;
    if (layout == KhLayout_Embedded) {
        module->variant = find_variant(&song, false);
        if (module->variant != nullptr) {
            keep_driver_data(module, &song);
            module->embedded_song = song.base[1] + EMBEDDED_SONG_OFFSET;
            module->subsong_count = 1;
            status = KhStatus_Ok;
        }
    } else {
        module->song_table = stub_table(&song, layout);
        module->subsong_count = count_subsongs(&song.memory, module->song_table);
        if (module->subsong_count != 0) {
            status = io != nullptr ? load_driver(module, path, io) : KhStatus_Ok;
        }
    }
    module->song = song.memory;
    if (status == KhStatus_Ok && !songs_prepare(module)) {
        status = KhStatus_InvalidData;
    }
    if (status != KhStatus_Ok) {
        kh_module_free(module);
    }
    return status;
}

void kh_module_free(KhModule* module) {
    free(module->song.bytes);
    *module = (KhModule) { 0 };
}
