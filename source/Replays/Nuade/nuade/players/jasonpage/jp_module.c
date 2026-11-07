// SPDX-License-Identifier: MIT
// Jason Page player: module check, module layout, sample file and the memory image the replayer works in.

#include "jp_internal.h"

#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORD_SIZE 2
#define LONG_SIZE 4

// Header of a Graftgold / Virocop module: words at these offsets. The track tables have eight entries each; the
// replayer reads the first four.
#define HEADER_VERSION 0 // = 2
#define HEADER_SAMPLE_TABLE 2
#define HEADER_INSTRUMENT_TABLE 4
#define HEADER_SCRIPT_BASE 6
#define HEADER_SPEED_TABLE 8
#define HEADER_PRIORITY_TABLE 10
#define HEADER_TRACK_INDEX 12
#define HEADER_TRACK_BASE 28
#define HEADER_PATTERN_TABLE 44
#define HEADER_PATTERN_BASE 46
#define HEADER_LENGTH 48 // the module's length
#define HEADER_SIZE 50
#define HEADER_TABLE_WORDS 23    // the words from HEADER_SAMPLE_TABLE to HEADER_PATTERN_BASE
#define PATTERN_FLAG_MASK 0x0F00 // Graftgold patterns start with a byte whose low nibble is F
#define HEADER_VERSION_VALUE 2
#define MAX_OFFSET 0x8000 // the replayer reads these words as signed; no module has a negative one

// Realms: pointer tables at fixed places and four magic longs.
#define REALMS_SAMPLE_TABLE 0
#define REALMS_SAMPLE_COUNT 32
#define REALMS_ZERO_LONG 0x80
#define REALMS_INSTRUMENT_TABLE 0x84 // its first entry is REALMS_FIRST_SCRIPT
#define REALMS_FIRST_SCRIPT 0xCBE
#define REALMS_SPEED_TABLE 0x480
#define REALMS_PRIORITY_TABLE 0x4C0
#define REALMS_TRACK_TABLE 0x6BE
#define REALMS_TRACK_TABLE_STRIDE 0x80
#define REALMS_TRACK_TABLE_ENTRIES 32
#define REALMS_PATTERN_TABLE 0x8BE
#define REALMS_MIN_SIZE 0xCBE
#define REALMS_END_WORD 0xFF00 // a track list ends with this word
#define REALMS_MAGIC_1_AT 0xCB6
#define REALMS_MAGIC_1 0x308BE
#define REALMS_MAGIC_2_AT 0xCBA
#define REALMS_MAGIC_2 0x309BE

// The eagleplayer repairs five known modules (it recognises them by the module length).
#define FIX_SIZE_EMPIRE_SOCCER 7452
#define FIX_OFFSET_EMPIRE_SOCCER 1956
#define FIX_SIZE_VIROCOP_01 7054
#define FIX_OFFSET_VIROCOP_01 3490
#define FIX_SIZE_VIROCOP_03 6736
#define FIX_OFFSET_VIROCOP_03 3604
#define FIX_SIZE_VIROCOP_09 6260
#define FIX_OFFSET_VIROCOP_09 4088
#define FIX_SIZE_REALMS_INGAME 7382
#define REALMS_INGAME_LENGTH_OFFSET 2234
#define REALMS_INGAME_SPLIT_COUNT 4   // track lists cut at every loop word
#define REALMS_INGAME_LIST_STRIDE 194 // bytes between the cut points of consecutive voices' lists
#define REALMS_INGAME_TABLE_STEP 128  // bytes between a subsong's entries of consecutive voices
#define REALMS_INGAME_SUBSONG_STEP 4  // one subsong's entry in a track table

#define MAX_SUBSONGS 16
#define VIROCOP_MAX_SAMPLES 32
#define SAMPLE_PREFIX_LENGTH 3
#define MAX_MODULE_SIZE 0xFFFF
#define MAX_SAMPLE_FILE_SIZE 0x10000000u // the image is addressed with 32 bits
#define SILENT_WORD_SIZE 4

// Graftgold / Virocop: the script of a voice that has no sound sets the volume to 0, clears the envelope, then waits
// one tick at a time. Realms writes its command words multiplied by 4.
static const uint8_t k_idle_script_16[] = { 0x00, 0x0F, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x12, 0x00, 0x07, 0x00, 0x00 };
static const uint8_t k_idle_script_realms[]
    = { 0x00, 0x18, 0x00, 0x00, 0x00, 0x48, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00 };

// Memory image

uint8_t jp_u8(const JpMemory* mem, uint32_t address) {
    return address < mem->size ? mem->data[address] : 0;
}

uint16_t jp_u16(const JpMemory* mem, uint32_t address) {
    return (uint16_t)(jp_u8(mem, address) << 8 | jp_u8(mem, address + 1));
}

int16_t jp_s16(const JpMemory* mem, uint32_t address) {
    return (int16_t)jp_u16(mem, address);
}

uint32_t jp_u32(const JpMemory* mem, uint32_t address) {
    return (uint32_t)jp_u16(mem, address) << 16 | jp_u16(mem, address + 2);
}

bool jp_store_u8(JpMemory* mem, uint32_t address, uint8_t value) {
    if (address >= mem->size) {
        return false;
    }
    mem->data[address] = value;
    return true;
}

bool jp_store_u16(JpMemory* mem, uint32_t address, uint16_t value) {
    return jp_store_u8(mem, address, (uint8_t)(value >> 8)) && jp_store_u8(mem, address + 1, (uint8_t)value);
}

bool jp_store_u32(JpMemory* mem, uint32_t address, uint32_t value) {
    return jp_store_u16(mem, address, (uint16_t)(value >> 16)) && jp_store_u16(mem, address + 2, (uint16_t)value);
}

// Table entries. Realms keeps absolute 32-bit pointers and whole words; the other two keep signed 16-bit offsets from a
// base and use the low byte of a word.

uint32_t jp_table_pointer(const JpPlayer* player, uint32_t table, uint32_t base, uint32_t index) {
    if (player->format == JpFormat_Realms) {
        return jp_u32(&player->mem, table + index * LONG_SIZE);
    }
    return base + (uint32_t)(int32_t)jp_s16(&player->mem, table + index * WORD_SIZE);
}

uint16_t jp_table_value(const JpPlayer* player, uint32_t table, uint32_t index) {
    const uint16_t value = jp_u16(&player->mem, table + index * WORD_SIZE);
    return player->format == JpFormat_Realms ? value : (uint16_t)(value & JP_BYTE_MASK);
}

// The module as the eagleplayer's check and InitPlayer see it: a copy with the repairs applied and its layout.

typedef struct Module {
    JpMemory mem; // the module alone; no padding behind it
    JpFormat format;
    JpLayout layout;
} Module;

// The header of a Graftgold or Virocop module.

typedef struct Header {
    uint16_t sample_table; // the module's table of sample lengths; the module ends with it
    uint16_t instrument_table;
    uint16_t script_base;
    uint16_t speed_table;
    uint16_t priority_table;
    uint16_t track_index[JP_VOICES];
    uint16_t track_base[JP_VOICES];
    uint16_t pattern_table;
    uint16_t pattern_base;
    uint16_t length; // the module's length
} Header;

// DTP_Check2 for the first two formats: reads the header and applies the eagleplayer's checks. The eagleplayer compares
// the words as signed numbers; a word with the top bit set would be a negative offset, which no module has, so the
// check refuses those.

static bool read_header(const JpMemory* mem, Header* header) {
    if (mem->size < HEADER_SIZE || jp_u16(mem, HEADER_VERSION) != HEADER_VERSION_VALUE) {
        return false;
    }
    *header = (Header) {
        .sample_table = jp_u16(mem, HEADER_SAMPLE_TABLE),
        .instrument_table = jp_u16(mem, HEADER_INSTRUMENT_TABLE),
        .script_base = jp_u16(mem, HEADER_SCRIPT_BASE),
        .speed_table = jp_u16(mem, HEADER_SPEED_TABLE),
        .priority_table = jp_u16(mem, HEADER_PRIORITY_TABLE),
        .pattern_table = jp_u16(mem, HEADER_PATTERN_TABLE),
        .pattern_base = jp_u16(mem, HEADER_PATTERN_BASE),
        .length = jp_u16(mem, HEADER_LENGTH),
    };
    for (uint32_t i = 0; i < JP_VOICES; i++) {
        header->track_index[i] = jp_u16(mem, HEADER_TRACK_INDEX + i * WORD_SIZE);
        header->track_base[i] = jp_u16(mem, HEADER_TRACK_BASE + i * WORD_SIZE);
    }
    // The word the instrument table offset points at is 0.
    if ((header->instrument_table & 1) != 0 || header->instrument_table >= MAX_OFFSET
        || header->instrument_table > mem->size - WORD_SIZE || jp_u16(mem, header->instrument_table) != 0
        || header->length >= MAX_OFFSET) {
        return false;
    }
    for (uint32_t i = 0; i < HEADER_TABLE_WORDS; i++) {
        const uint16_t word = jp_u16(mem, HEADER_SAMPLE_TABLE + i * WORD_SIZE);
        if (word == 0 || (word & 1) != 0 || word >= header->length) {
            return false;
        }
    }
    return true;
}

static bool realms_header_ok(const JpMemory* mem) {
    return mem->size >= REALMS_MIN_SIZE && jp_u16(mem, 0) == 0 && jp_u32(mem, REALMS_ZERO_LONG) == 0
           && jp_u32(mem, REALMS_INSTRUMENT_TABLE) == REALMS_FIRST_SCRIPT
           && jp_u32(mem, REALMS_MAGIC_1_AT) == REALMS_MAGIC_1 && jp_u32(mem, REALMS_MAGIC_2_AT) == REALMS_MAGIC_2;
}

// Graftgold patterns start with a byte whose low nibble is F, the Virocop ones do not.

static JpFormat detect_format(const JpMemory* mem, const Header* header) {
    if (header->pattern_base > mem->size - WORD_SIZE) {
        return JpFormat_None;
    }
    const bool flagged = (jp_u16(mem, header->pattern_base) & PATTERN_FLAG_MASK) == PATTERN_FLAG_MASK;
    return flagged ? JpFormat_Graftgold : JpFormat_Virocop;
}

// InitPlayer's repairs of known modules. Every write is bounded; a module too short for it is left alone.

static void repair_byte(JpMemory* mem, uint32_t module_length, uint32_t length, uint32_t offset) {
    if (module_length == length) {
        jp_store_u8(mem, offset, 0xFF);
    }
}

// Realms "ingame" has one track list per voice and subsong with several loop words in it; the eagleplayer cuts every
// such list at the loop words and gives each cut a subsong of its own: the end word goes in place of the loop word and
// the next subsong's table entries point behind it.

static void repair_realms_ingame(JpMemory* mem) {
    if (jp_u32(mem, REALMS_INGAME_LENGTH_OFFSET) + 2 != FIX_SIZE_REALMS_INGAME) {
        return;
    }
    uint32_t cut = jp_u32(mem, REALMS_TRACK_TABLE);
    uint32_t table_shift = 0;
    while (cut < mem->size) {
        const uint8_t pattern = jp_u8(mem, cut);
        if (pattern == JP_TRACK_LOOP_ALT || pattern == JP_TRACK_SKIP || pattern == JP_TRACK_LOOP) {
            for (int voice = 0; voice < REALMS_INGAME_SPLIT_COUNT; voice++) {
                if (!jp_store_u32(mem, REALMS_TRACK_TABLE + REALMS_INGAME_SUBSONG_STEP + table_shift, cut + WORD_SIZE)
                    || !jp_store_u16(mem, cut, REALMS_END_WORD)) {
                    return;
                }
                cut += REALMS_INGAME_LIST_STRIDE;
                table_shift += REALMS_INGAME_TABLE_STEP;
            }
            cut -= REALMS_INGAME_SPLIT_COUNT * REALMS_INGAME_LIST_STRIDE;
            table_shift -= REALMS_INGAME_SPLIT_COUNT * REALMS_INGAME_TABLE_STEP - REALMS_INGAME_SUBSONG_STEP;
        } else if (jp_u16(mem, cut) == REALMS_END_WORD) {
            return;
        }
        cut += WORD_SIZE;
    }
}

// The tables of the Graftgold / Virocop header.

static bool layout_graftgold(const JpMemory* mem, const Header* header, JpLayout* layout) {
    if (header->length > mem->size || header->sample_table >= header->length) {
        return false;
    }
    layout->sample_table = header->sample_table;
    layout->instrument_table = header->instrument_table;
    layout->script_base = header->script_base;
    layout->speed_table = header->speed_table;
    layout->priority_table = header->priority_table;
    for (uint32_t voice = 0; voice < JP_VOICES; voice++) {
        layout->track_index[voice] = header->track_index[voice];
        layout->track_base[voice] = header->track_base[voice];
    }
    layout->pattern_table = header->pattern_table;
    layout->pattern_base = header->pattern_base;

    // The subsong tables of the first voice run up to its track base.
    const int32_t subsongs = ((int32_t)header->track_base[0] - (int32_t)header->track_index[0] - WORD_SIZE) / 2;
    if (subsongs < 1 || subsongs > MAX_SUBSONGS) {
        return false;
    }
    layout->subsongs = subsongs;
    layout->sample_count = (uint32_t)(header->length - header->sample_table) / LONG_SIZE;
    return true;
}

static bool layout_realms(const JpMemory* mem, JpLayout* layout) {
    layout->instrument_table = REALMS_INSTRUMENT_TABLE;
    layout->speed_table = REALMS_SPEED_TABLE;
    layout->priority_table = REALMS_PRIORITY_TABLE;
    for (uint32_t voice = 0; voice < JP_VOICES; voice++) {
        layout->track_index[voice] = REALMS_TRACK_TABLE + voice * REALMS_TRACK_TABLE_STRIDE;
    }
    layout->pattern_table = REALMS_PATTERN_TABLE;
    layout->sample_count = REALMS_SAMPLE_COUNT;
    layout->sample_table = REALMS_SAMPLE_TABLE;

    // A subsong is a table entry up to the first one that points at a track list consisting of the end word.
    int subsongs = 0;
    while (subsongs < REALMS_TRACK_TABLE_ENTRIES
           && jp_u16(mem, jp_u32(mem, REALMS_TRACK_TABLE + (uint32_t)subsongs * LONG_SIZE)) != REALMS_END_WORD) {
        subsongs++;
    }
    if (subsongs < 1 || subsongs >= REALMS_TRACK_TABLE_ENTRIES || subsongs > MAX_SUBSONGS) {
        return false;
    }
    layout->subsongs = subsongs;
    return true;
}

static bool module_read(Module* module, const uint8_t* data, size_t size) {
    if (size > MAX_MODULE_SIZE) {
        return false;
    }
    module->mem.data = malloc(size);
    if (module->mem.data == nullptr) {
        return false;
    }
    memcpy(module->mem.data, data, size);
    module->mem.size = (uint32_t)size;

    JpMemory* mem = &module->mem;
    Header header;
    if (read_header(mem, &header)) {
        module->format = detect_format(mem, &header);
        if (module->format == JpFormat_None) {
            return false;
        }
        repair_byte(mem, header.length, FIX_SIZE_EMPIRE_SOCCER, FIX_OFFSET_EMPIRE_SOCCER);
        repair_byte(mem, header.length, FIX_SIZE_VIROCOP_01, FIX_OFFSET_VIROCOP_01);
        repair_byte(mem, header.length, FIX_SIZE_VIROCOP_03, FIX_OFFSET_VIROCOP_03);
        repair_byte(mem, header.length, FIX_SIZE_VIROCOP_09, FIX_OFFSET_VIROCOP_09);
        return layout_graftgold(mem, &header, &module->layout);
    }
    if (realms_header_ok(mem)) {
        module->format = JpFormat_Realms;
        repair_realms_ingame(mem);
        return layout_realms(mem, &module->layout);
    }
    return false;
}

static void module_free(Module* module) {
    free(module->mem.data);
    *module = (Module) { 0 };
}

bool jp_module_probe(const uint8_t* data, size_t size) {
    Module module = { 0 };
    const bool ok = module_read(&module, data, size);
    module_free(&module);
    return ok;
}

// ExtLoad: the sample file has the song's name with its "jpn" prefix replaced by "smp" ("jpn.name" -> "smp.name"), or
// "jpnd.name" -> "jpns.name", in the song's directory. The suffix forms are handled the same way: "name.jpn" ->
// "name.smp", "name.jpnd" -> "name.jpns".

static bool sample_file_name(const char* path, char* name, size_t name_size) {
    const char* base = nuade_path_basename(path);
    if (strlen(base) + 1 > name_size) {
        return false;
    }
    strcpy(name, base);
    if (nuade_strncasecmp(base, "jpn", SAMPLE_PREFIX_LENGTH) == 0) {
        if (base[SAMPLE_PREFIX_LENGTH] == 'd' || base[SAMPLE_PREFIX_LENGTH] == 'D') {
            name[SAMPLE_PREFIX_LENGTH] = 'S';
            return true;
        }
        if (base[SAMPLE_PREFIX_LENGTH] == '.') {
            memcpy(name, "SMP", SAMPLE_PREFIX_LENGTH);
            return true;
        }
    }
    const char* dot = strrchr(base, '.');
    if (dot == nullptr) {
        return false;
    }
    char* extension = name + (dot - base) + 1;
    if (nuade_strcasecmp(extension, "jpn") == 0) {
        memcpy(extension, "SMP", SAMPLE_PREFIX_LENGTH);
        return true;
    }
    if (nuade_strcasecmp(extension, "jpnd") == 0) {
        extension[SAMPLE_PREFIX_LENGTH] = 'S';
        return true;
    }
    return false;
}

static NuadeIoReadResult read_sample_file(const char* path, const NuadeIo* io) {
    NuadeIoReadResult none = { 0 };
    char dir[1024] = { 0 };
    char name[512];
    if (path == nullptr || io == nullptr || !sample_file_name(path, name, sizeof(name))) {
        return none;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    NuadeIoReadResult file = url != nullptr ? NuadeIo_read_url_to_memory(io, url) : none;
    nuade_file_index_destroy(index);
    return file;
}

// Where every sample starts. Graftgold and Virocop cut the sample file by the module's table of lengths (Virocop uses
// the first 32); Realms has the offsets of the samples in the module, relative to the first.

static bool resolve_samples(const JpMemory* module, JpFormat format, JpLayout* layout, uint32_t sample_base) {
    if (format == JpFormat_Virocop && layout->sample_count > VIROCOP_MAX_SAMPLES) {
        layout->sample_count = VIROCOP_MAX_SAMPLES;
    }
    layout->sample_start = calloc(layout->sample_count != 0 ? layout->sample_count : 1, sizeof(uint32_t));
    if (layout->sample_start == nullptr) {
        return false;
    }
    uint32_t start = sample_base;
    const uint32_t first = jp_u32(module, REALMS_SAMPLE_TABLE);
    for (uint32_t i = 0; i < layout->sample_count; i++) {
        if (format == JpFormat_Realms) {
            layout->sample_start[i] = jp_u32(module, REALMS_SAMPLE_TABLE + i * LONG_SIZE) - first + sample_base;
        } else {
            layout->sample_start[i] = start;
            start += jp_u32(module, layout->sample_table + i * LONG_SIZE);
        }
    }
    return true;
}

// The image: module, the idle script and the silent word (the replayer's own memory, out of reach of any sample
// address), then the sample file. Returns false when memory is out (the sample file is at most MAX_SAMPLE_FILE_SIZE).

static bool build_image(JpPlayer* player, const Module* module, const NuadeIoReadResult* samples,
                        uint32_t* sample_base) {
    const bool realms = module->format == JpFormat_Realms;
    const uint8_t* idle = realms ? k_idle_script_realms : k_idle_script_16;
    const size_t idle_size = realms ? sizeof(k_idle_script_realms) : sizeof(k_idle_script_16);
    const uint32_t idle_at = (module->mem.size + 1u) & ~1u;
    const uint32_t empty_at = idle_at + (uint32_t)idle_size;
    *sample_base = empty_at + SILENT_WORD_SIZE;
    const uint32_t total = *sample_base + (uint32_t)samples->size;

    player->mem.data = nuade_paula_padded_copy(nullptr, total);
    if (player->mem.data == nullptr) {
        return false;
    }
    memcpy(player->mem.data, module->mem.data, module->mem.size);
    memcpy(player->mem.data + idle_at, idle, idle_size);
    memcpy(player->mem.data + *sample_base, samples->data, (size_t)samples->size);
    player->mem.size = total;
    player->idle_script = idle_at;
    player->empty_buffer = empty_at;
    return true;
}

JpStatus jp_module_load(JpPlayer* player, const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    Module module = { 0 };
    if (!module_read(&module, data, size)) {
        module_free(&module);
        return JpStatus_InvalidData;
    }
    const NuadeIoReadResult samples = read_sample_file(path, io);
    if (samples.data == nullptr) {
        module_free(&module);
        return JpStatus_MissingSamples;
    }
    if (samples.size > MAX_SAMPLE_FILE_SIZE) {
        NuadeIo_free_url_to_memory(io, samples.data);
        module_free(&module);
        return JpStatus_InvalidData;
    }
    uint32_t sample_base = 0;
    const bool built = build_image(player, &module, &samples, &sample_base);
    NuadeIo_free_url_to_memory(io, samples.data);
    JpStatus status = JpStatus_Ok;
    if (!built) {
        status = JpStatus_OutOfMemory;
    } else {
        player->format = module.format;
        player->layout = module.layout;
        player->pristine = malloc(player->mem.size);
        if (player->pristine == nullptr || !resolve_samples(&module.mem, module.format, &player->layout, sample_base)) {
            status = JpStatus_OutOfMemory;
        } else {
            memcpy(player->pristine, player->mem.data, player->mem.size);
        }
    }
    module_free(&module);
    return status;
}

void jp_module_free(JpPlayer* player) {
    free(player->mem.data);
    free(player->pristine);
    free(player->layout.sample_start);
}

void jp_module_reset(JpPlayer* player) {
    memcpy(player->mem.data, player->pristine, player->mem.size);
}
