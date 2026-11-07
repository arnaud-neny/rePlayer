// SPDX-License-Identifier: MIT
// Ashley Hogg player: module checks and table locating. The eagleplayer scans the driver's code for instruction
// patterns and reads the pc-relative offsets out of them; so does this. "xxxx" in a pattern is any word, an "x" digit
// any nibble.

#include "ah_internal.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Instruction words of the checks
#define BRA_W 0x6000
#define MOVE_W_IMMEDIATE_D0 0x303C      // move.w #n,d0
#define BNE_B_22 0x6622                 // bne.b +$22
#define MOVE_W_D0_ABSOLUTE 0x33C0       // move.w d0,xxx.l
#define CMP_W_IMMEDIATE_D0 0xB07C       // cmp.w #n,d0
#define MOVEM_L_ALL_TO_STACK 0x48E7FFFE // movem.l d0-d7/a0-a6,-(a7)
#define BSR_W 0x6100
#define LEA_ABSOLUTE_A6 0x4DF9 // lea xxx.l,a6
#define CUSTOM_BASE 0xDFF000

#define OLD_BRANCHES 4
#define OLD_CHECK_OFFSET (4 * OLD_BRANCHES) // the code after the branches
#define NEW_BRANCHES 6
#define NEW_LAST_BRANCH (NEW_BRANCHES - 1) // leads to `movem.l d0-d7/a0-a6,-(a7)` and a `bsr.w`
#define OLD_SEARCH_LIMIT 0x900
#define NEW_SEARCH_LIMIT 0x1800
#define MAX_PATTERN_WORDS 16
#define MAX_PADDED_END 0x1000000u // a sample beyond the 68000's address space is no sample
#define MAX_RECORDS 256
#define AH_NOT_FOUND 0xFFFFFFFFu
#define MIN_MODULE_SIZE 0x20
#define SILENT_WORD_END 6    // the long that holds the address of the silent word, and the word
#define OLD_WAVE_SIZE 0x4000 // the wave-table voice's waveforms: 128 bytes each, selected by 7 bits

#define SMP_PREFIX "smp."
#define MODULE_PREFIX "ash."
#define PREFIX_LENGTH 4

bool ah_memory_init(AhMemory* m, const uint8_t* data, size_t size, uint32_t capacity) {
    if (size > UINT32_MAX) {
        return false;
    }
    if (capacity < size) {
        capacity = (uint32_t)size;
    }
    m->data = nuade_paula_padded_copy(nullptr, capacity);
    if (m->data == nullptr) {
        return false;
    }
    memcpy(m->data, data, size);
    m->size = (uint32_t)size;
    m->capacity = capacity;
    return true;
}

uint8_t ah_u8(const AhMemory* m, uint32_t offset) {
    return offset < m->capacity ? m->data[offset] : 0;
}

uint16_t ah_u16(const AhMemory* m, uint32_t offset) {
    return (uint16_t)(ah_u8(m, offset) << 8 | ah_u8(m, offset + 1));
}

int16_t ah_s16(const AhMemory* m, uint32_t offset) {
    return (int16_t)ah_u16(m, offset);
}

uint32_t ah_u32(const AhMemory* m, uint32_t offset) {
    return (uint32_t)ah_u16(m, offset) << 16 | ah_u16(m, offset + 2);
}

// Pattern scan

static int parse_pattern(const char* pattern, uint16_t* words, uint16_t* masks) {
    int count = 0;
    while (*pattern != '\0' && count < MAX_PATTERN_WORDS) {
        if (*pattern == ' ') {
            pattern++;
            continue;
        }
        words[count] = 0;
        masks[count] = 0;
        for (int digit = 0; digit < 4 && *pattern != '\0'; digit++, pattern++) {
            words[count] = (uint16_t)(words[count] << 4);
            masks[count] = (uint16_t)(masks[count] << 4);
            if (*pattern != 'x') {
                const char text[2] = { *pattern, '\0' };
                words[count] |= (uint16_t)strtoul(text, nullptr, 16);
                masks[count] |= 0xF;
            }
        }
        count++;
    }
    return count;
}

// The offset of the first match of `pattern` in [from, to), or AH_NOT_FOUND.

static uint32_t find_pattern(const AhMemory* m, uint32_t from, uint32_t to, const char* pattern) {
    uint16_t words[MAX_PATTERN_WORDS];
    uint16_t masks[MAX_PATTERN_WORDS];
    const uint32_t count = (uint32_t)parse_pattern(pattern, words, masks);
    for (uint32_t at = from; at + 2 * count <= m->size && at < to; at += 2) {
        uint32_t i = 0;
        while (i < count && (ah_u16(m, at + 2 * i) & masks[i]) == words[i]) {
            i++;
        }
        if (i == count) {
            return at;
        }
    }
    return AH_NOT_FOUND;
}

// The table that the d16(pc) operand in word `operand` of a match refers to; AH_NOT_FOUND if there is no match.

static uint32_t find_table16(const AhMemory* m, uint32_t from, uint32_t to, const char* pattern, uint32_t operand) {
    const uint32_t at = find_pattern(m, from, to, pattern);
    if (at == AH_NOT_FOUND) {
        return AH_NOT_FOUND;
    }
    const uint32_t extension = at + 2 * operand;
    return extension + (uint32_t)(int32_t)ah_s16(m, extension);
}

// The same for a d8(pc,Xn) operand: the brief extension word carries the displacement in its low byte.

static uint32_t find_table8(const AhMemory* m, uint32_t from, uint32_t to, const char* pattern, uint32_t operand) {
    const uint32_t at = find_pattern(m, from, to, pattern);
    if (at == AH_NOT_FOUND) {
        return AH_NOT_FOUND;
    }
    const uint32_t extension = at + 2 * operand;
    return extension + (uint32_t)(int32_t)(int8_t)ah_u8(m, extension + 1);
}

// A branch table entry: `bra.w` to a positive even offset. Returns the target or AH_NOT_FOUND.

static uint32_t branch_target(const AhMemory* m, uint32_t entry) {
    const int16_t offset = ah_s16(m, entry + 2);
    if (ah_u16(m, entry) != BRA_W || offset <= 0 || (offset & 1) != 0) {
        return AH_NOT_FOUND;
    }
    return entry + 2 + (uint32_t)offset;
}

// The eagleplayer's Check2 for the new family: six branches, the last to `movem.l d0-d7/a0-a6,-(a7); bsr.w`, whose
// target starts with `lea $dff000,a6`.

static bool probe_new(const AhMemory* m) {
    for (uint32_t i = 0; i < NEW_BRANCHES; i++) {
        if (branch_target(m, 4 * i) == AH_NOT_FOUND) {
            return false;
        }
    }
    const uint32_t routine = branch_target(m, 4 * NEW_LAST_BRANCH);
    if (ah_u32(m, routine) != MOVEM_L_ALL_TO_STACK || ah_u16(m, routine + 4) != BSR_W) {
        return false;
    }
    const uint32_t target = routine + 6 + (uint32_t)(int32_t)ah_s16(m, routine + 6);
    return ah_u16(m, target) == LEA_ABSOLUTE_A6 && ah_u32(m, target + 2) == CUSTOM_BASE;
}

// Check2 for the old family: four branches, then `move.w #0,d0; bne.b +$22; move.w d0,xxx.l`. Stuntman Seymour's driver
// has `cmp.w #0,d0` between the first two instructions, which the eagleplayer's check refuses (UADE plays the song
// because of its name); the port accepts that form as well.

static bool probe_old(const AhMemory* m) {
    for (uint32_t i = 0; i < OLD_BRANCHES; i++) {
        if (branch_target(m, 4 * i) == AH_NOT_FOUND) {
            return false;
        }
    }
    if (ah_u16(m, OLD_CHECK_OFFSET) != MOVE_W_IMMEDIATE_D0 || ah_u16(m, OLD_CHECK_OFFSET + 2) != 0) {
        return false;
    }
    const uint32_t next = OLD_CHECK_OFFSET + 4;
    const bool check2 = ah_u16(m, next) == BNE_B_22 && ah_u16(m, next + 2) == MOVE_W_D0_ABSOLUTE;
    const bool stuntman
        = ah_u16(m, next) == CMP_W_IMMEDIATE_D0 && ah_u16(m, next + 2) == 0 && ah_u16(m, next + 4) == BNE_B_22;
    return check2 || stuntman;
}

bool ah_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < MIN_MODULE_SIZE || size > UINT32_MAX) {
        return false;
    }
    const AhMemory m = { .data = (uint8_t*)data, .size = (uint32_t)size, .capacity = (uint32_t)size };
    return probe_new(&m) || probe_old(&m);
}

static bool tables_inside(const AhMemory* m, const uint32_t* offsets, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (offsets[i] == AH_NOT_FOUND || offsets[i] >= m->size) {
            return false;
        }
    }
    return true;
}

// The furthest AUDxLC of the old family: the attack or the loop of an instrument (the Paula tail covers the lengths).

uint64_t ah_old_samples_end(const AhOld* old, const AhMemory* module) {
    const AhOldTables* t = &old->tables;
    uint64_t end = module->size;
    const uint32_t first = t->instruments;
    const uint32_t last = t->arp_offsets > first ? t->arp_offsets : first + MAX_RECORDS * OLD_INSTRUMENT_SIZE;
    for (uint32_t record = first; record + OLD_INSTRUMENT_SIZE <= last; record += OLD_INSTRUMENT_SIZE) {
        const AhOldInstrument in = ah_old_read_instrument(module, record);
        const uint64_t ends[2]
            = { in.wave_table ? 0 : (uint64_t)t->samples + in.sample, (uint64_t)t->samples + in.loop };
        for (size_t i = 0; i < nuade_sizeof_array(ends); i++) {
            if (ends[i] < MAX_PADDED_END && ends[i] > end) {
                end = ends[i];
            }
        }
    }
    const uint64_t extras[2] = { (uint64_t)t->wave_base + OLD_WAVE_SIZE, (uint64_t)t->silent + SILENT_WORD_END };
    for (size_t i = 0; i < nuade_sizeof_array(extras); i++) {
        if (extras[i] > end) {
            end = extras[i];
        }
    }
    return end;
}

// InitPlayer of the old family: the table addresses and the version of the driver.

bool ah_old_locate(AhOld* old, const AhMemory* m) {
    AhOldTables* t = &old->tables;
    const uint32_t limit = OLD_SEARCH_LIMIT;
    t->periods = find_table16(m, 0, limit, "49fa xxxx e34a 3434 2000", 1);
    t->instruments = find_table16(m, 0, limit, "c2fc 002c 47fa xxxx d7c1", 3);
    t->arp_base = find_table16(m, 0, limit, "47fa xxxx 49fa xxxx 3c28 0006", 1);
    t->arp_offsets = find_table16(m, 0, limit, "47fa xxxx 49fa xxxx 3c28 0006", 3);
    t->samples = find_table16(m, 0, limit, "47fa xxxx d7e8 0020", 1);
    t->silent = find_table16(m, 0, limit, "2c3a xxxx 7a01", 1);
    t->track_base = find_table16(m, 0, limit, "45fa xxxx b5e8 0032", 1);
    t->pattern_offsets = find_table16(m, 0, limit, "47fa xxxx 2173 1000 0036 2173 1000 003e 47fa xxxx 220b", 1);
    t->pattern_base = find_table16(m, 0, limit, "47fa xxxx 2173 1000 0036 2173 1000 003e 47fa xxxx 220b", 9);
    t->speeds = find_table16(m, 0, limit, "41fa xxxx 1970 0000 002f", 1);
    t->songs = find_table16(m, 0, limit, "41fa xxxx e948 2970 0000 0032", 1);
    t->wave_base = find_table16(m, 0, limit, "49fa xxxx d9c6", 1);
    const uint32_t offsets[]
        = { t->periods,    t->instruments,     t->arp_base,     t->arp_offsets, t->samples, t->silent,
            t->track_base, t->pattern_offsets, t->pattern_base, t->speeds,      t->songs,   t->wave_base };
    if (!tables_inside(m, offsets, nuade_sizeof_array(offsets)) || t->speeds <= t->songs) {
        return false;
    }
    // SubSongRange: the song table and the speed table are adjacent.
    t->subsong_count = (t->speeds - t->songs) / OLD_SONG_ENTRY_SIZE;
    if (t->subsong_count == 0) {
        return false;
    }

    AhOldVersion* v = &old->version;
    v->music_on_all_voices = find_pattern(m, 0, limit, "be3c 0008") == AH_NOT_FOUND;
    v->restart_offset = find_pattern(m, 0, limit, "2168 003a 0032") == AH_NOT_FOUND;
    if (find_pattern(m, 0, limit, "0c12 0082") != AH_NOT_FOUND) {
        v->fade_command = 0x82;
    } else if (find_pattern(m, 0, limit, "0c12 003e") != AH_NOT_FOUND) {
        v->fade_command = 0x3E;
    }
    return true;
}

// InitPlayer of the new family: the table addresses.

bool ah_new_locate(AhNew* modern, const AhMemory* m) {
    AhNewTables* t = &modern->tables;
    const uint32_t limit = NEW_SEARCH_LIMIT;
    t->songs = find_table16(m, 0, limit, "45fa xxxx d040 d040 3232 0000 3432 0002", 1);
    t->periods = find_table16(m, 0, limit, "49fa xxxx 3829 000e 3a28 00f2", 1);
    t->remap = find_table16(m, 0, limit, "e949 45fa xxxx d4c1", 2);
    t->transpose = find_table8(m, 0, limit, "e34d d87b 50xx", 2);
    t->durations = find_table8(m, 0, limit, "137b 20xx 0011", 1);
    const uint32_t offsets[] = { t->songs, t->periods, t->remap, t->transpose, t->durations };
    if (!tables_inside(m, offsets, nuade_sizeof_array(offsets))) {
        return false;
    }
    // SubSongRange: the first order list follows the song table.
    t->subsong_count = ah_u16(m, t->songs) / 4;
    return t->subsong_count != 0;
}

// The furthest AUDxLC of the new family (the Paula tail covers the lengths). The sample records are at the start of the
// file.

uint64_t ah_new_samples_end(const AhMemory* samples) {
    uint64_t end = samples->size;
    const uint32_t first = ah_u32(samples, 0);
    const uint32_t records = first >= NEW_RECORD_SIZE && first <= samples->size ? first / NEW_RECORD_SIZE : MAX_RECORDS;
    for (uint32_t i = 0; i < records && i < MAX_RECORDS; i++) {
        const AhNewSample sample = ah_new_read_sample(samples, (uint8_t)i);
        const uint64_t ends[2] = { sample.start, sample.loop_start };
        for (size_t j = 0; j < nuade_sizeof_array(ends); j++) {
            if (ends[j] < MAX_PADDED_END && ends[j] > end) {
                end = ends[j];
            }
        }
    }
    return end;
}

// ExtLoad: the sample file is the song's file name with its "ash." prefix changed to "smp.", in the song's directory.

static bool load_samples(AhNew* modern, const char* path, const NuadeIo* io) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || nuade_strncasecmp(base, MODULE_PREFIX, PREFIX_LENGTH) != 0
        || strlen(base) + 1 > sizeof(name)) {
        return false;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    snprintf(name, sizeof(name), SMP_PREFIX "%s", base + PREFIX_LENGTH);
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    bool ok = false;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            const AhMemory raw
                = { .data = (uint8_t*)file.data, .size = (uint32_t)file.size, .capacity = (uint32_t)file.size };
            ok = ah_memory_init(&modern->samples, file.data, file.size, (uint32_t)ah_new_samples_end(&raw));
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return ok;
}

AhCreateResult ah_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    AhCreateResult result = { .status = AhStatus_InvalidData };
    if (!ah_probe(data, size)) {
        return result;
    }
    AhPlayer* p = calloc(1, sizeof(AhPlayer));
    if (p == nullptr) {
        result.status = AhStatus_OutOfMemory;
        return result;
    }
    if (!ah_memory_init(&p->module, data, size, (uint32_t)size)) {
        ah_destroy(p);
        result.status = AhStatus_OutOfMemory;
        return result;
    }
    p->family = probe_new(&p->module) ? AhFamily_New : AhFamily_Old;
    bool ok = false;
    if (p->family == AhFamily_New) {
        ok = ah_new_locate(&p->new_family, &p->module);
        if (ok && !load_samples(&p->new_family, path, io)) {
            ah_destroy(p);
            result.status = AhStatus_MissingSamples;
            return result;
        }
    } else {
        ok = ah_old_locate(&p->old_family, &p->module);
        if (ok) {
            AhMemory padded;
            ok = ah_memory_init(&padded, data, size, (uint32_t)ah_old_samples_end(&p->old_family, &p->module));
            if (ok) {
                free(p->module.data);
                p->module = padded;
            }
        }
    }
    if (!ok) {
        ah_destroy(p);
        return result;
    }
    result.player = p;
    result.status = AhStatus_Ok;
    return result;
}

void ah_destroy(AhPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player->new_family.samples.data);
    free(player);
}

int ah_subsong_count(const AhPlayer* player) {
    return (int)(player->family == AhFamily_New ? player->new_family.tables.subsong_count
                                                : player->old_family.tables.subsong_count);
}
