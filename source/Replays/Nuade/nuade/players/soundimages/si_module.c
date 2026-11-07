// SPDX-License-Identifier: MIT
// Sound Images: the eagleplayer's module check, the proof that the module's code is a driver the port models, and the
// reads of the module's data.

#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "si_internal.h"
#include <stdlib.h>
#include <string.h>

// The eagleplayer's Check2 looks for the end of the driver's interrupt routine at the target of the first branch: an
// `rte` and the `movem.l` that starts the next routine, and the last instruction of the routine before it.
#define CHECK_RTE_MOVEM 0x4E7348E7u
#define CHECK_TAIL 0x009C4CDFu
#define CHECK_TAIL_DISTANCE 6u

// The voice records hold the hardware address of their channel, the DMA word and the interrupt word that the driver
// writes: nothing in the port can differ from them.
#define CUSTOM_AUD0 0x00DFF0A0u
#define CUSTOM_AUD_STRIDE 0x10u
#define DMA_SET_ENABLE 0x8200u
#define INTERRUPT_SET 0x8000u
#define INTERRUPT_AUD0 0x0080u

// The extents of the samples (the end of the furthest one) that change what the eagleplayer does (player.asm $5f2,
// $896).
#define SAMPLE_END_DIVIDES_BY_EIGHT 0xF186u // it divides the song count by 8
#define SAMPLE_END_MOVES_SONG_FLAG 0x75D6u  // it moves the byte it tests for the song end

// The header words the first tick writes to DMACON and INTENA.
#define HEADER_PENDING_DMA 0x0C
#define HEADER_PENDING_INTERRUPT 0x0E

// The eagleplayer's Check2

static bool check2(const uint8_t* data, size_t size) {
    if (size < 4 || nuade_read_be16(data) != SI_BRA_W) {
        return false;
    }
    const uint16_t displacement = nuade_read_be16(data + 2);
    if ((displacement & 0x8000u) != 0 || (displacement & 1u) != 0) {
        return false;
    }
    if (displacement < CHECK_TAIL_DISTANCE || (size_t)displacement + 6 > size) {
        return false;
    }
    if (nuade_read_be32(data + displacement) != CHECK_RTE_MOVEM) {
        return false;
    }
    if (nuade_read_be32(data + displacement - CHECK_TAIL_DISTANCE) != CHECK_TAIL) {
        return false;
    }
    return true;
}

// The proof that the code is the image's: where the segments sit, and what the operands that leave them point to.

typedef struct Placement {
    int64_t base[SI_MAX_SEGMENTS];
    bool placed[SI_MAX_SEGMENTS];
} Placement;

static bool word_at(const uint8_t* data, size_t size, int64_t offset, uint16_t* word) {
    if (offset < 0 || (uint64_t)offset > (uint64_t)size - 2 || size < 2) {
        return false;
    }
    *word = nuade_read_be16(data + offset);
    return true;
}

// The place a hole's operand points to. False if the operand is outside the module.

static bool hole_target(const SiHole* hole, const Placement* place, const uint8_t* data, size_t size, int64_t* target) {
    const int64_t at = place->base[hole->segment] + 2 * (int64_t)hole->word;
    uint16_t word = 0;
    if (!word_at(data, size, at, &word)) {
        return false;
    }
    switch (hole->kind) {
        case SiHole_Branch8:
            *target = at + 2 + (int8_t)(word & 0xFF);
            return true;
        case SiHole_Branch16:
        case SiHole_PcRel16:
            *target = at + (int16_t)word;
            return true;
        case SiHole_PcIndex8:
            *target = at + (int8_t)(word & 0xFF);
            return true;
        case SiHole_Parameter:
            break;
    }
    return false;
}

static uint16_t hole_mask(SiHoleKind kind) {
    switch (kind) {
        case SiHole_Branch8:
        case SiHole_PcIndex8:
        case SiHole_Parameter:
            return 0x00FF;
        case SiHole_Branch16:
        case SiHole_PcRel16:
            break;
    }
    return 0xFFFF;
}

static int64_t ref_offset(const SiRef* ref, const Placement* place) {
    return place->base[ref->segment] + ref->offset;
}

static bool place_segments(const SiImage* image, const uint8_t* data, size_t size, Placement* place) {
    memset(place, 0, sizeof(*place));
    place->base[0] = image->first_offset;
    place->placed[0] = true;
    bool progress = true;
    while (progress) {
        progress = false;
        for (size_t i = 0; i < image->hole_count; i++) {
            const SiHole* hole = &image->holes[i];
            if (!place->placed[hole->segment] || hole->to_segment < 0 || place->placed[hole->to_segment]) {
                continue;
            }
            int64_t target = 0;
            if (!hole_target(hole, place, data, size, &target)) {
                return false;
            }
            place->base[hole->to_segment] = target - hole->to_offset;
            place->placed[hole->to_segment] = true;
            progress = true;
        }
        // A handler that only the command table reaches is placed by its entry.
        if (!place->placed[image->command_table.segment]) {
            continue;
        }
        const int64_t table = ref_offset(&image->command_table, place);
        for (size_t i = 0; i < image->command_count; i++) {
            const SiTableEntry* entry = &image->commands[i];
            uint16_t word = 0;
            if (place->placed[entry->to_segment]) {
                continue;
            }
            if (!word_at(data, size, table + 2 * (int64_t)i, &word)) {
                return false;
            }
            place->base[entry->to_segment] = table + (int16_t)word - entry->to_offset;
            place->placed[entry->to_segment] = true;
            progress = true;
        }
    }
    for (size_t i = 0; i < image->segment_count; i++) {
        if (!place->placed[i]) {
            return false;
        }
    }
    return true;
}

static uint64_t hash_segment(const SiImage* image, size_t segment, const Placement* place, const uint8_t* data,
                             size_t size) {
    const SiSegment* seg = &image->segments[segment];
    uint64_t hash = 0xCBF29CE484222325ULL;
    size_t hole = 0;
    while (hole < image->hole_count && image->holes[hole].segment < segment) {
        hole++;
    }
    for (uint32_t w = 0; w < seg->words; w++) {
        uint16_t word = 0;
        if (!word_at(data, size, place->base[segment] + 2 * (int64_t)w, &word)) {
            return 0;
        }
        while (hole < image->hole_count && image->holes[hole].segment == segment && image->holes[hole].word < w) {
            hole++;
        }
        if (hole < image->hole_count && image->holes[hole].segment == segment && image->holes[hole].word == w) {
            word &= (uint16_t)~hole_mask(image->holes[hole].kind);
        }
        hash = (hash ^ (word >> 8)) * 0x100000001B3ULL;
        hash = (hash ^ (word & 0xFF)) * 0x100000001B3ULL;
    }
    return hash;
}

// Does the module's code have the image's segments, and does every operand that leaves a segment point where the image
// says? The holes are zero in the hash, so the targets are what pins the data the code uses.

static bool match_image(const SiImage* image, const uint8_t* data, size_t size, Placement* place) {
    if (!place_segments(image, data, size, place)) {
        return false;
    }
    for (size_t i = 0; i < image->segment_count; i++) {
        const int64_t end = place->base[i] + 2 * (int64_t)image->segments[i].words;
        if (place->base[i] < 0 || end > (int64_t)size) {
            return false;
        }
        if (hash_segment(image, i, place, data, size) != image->segments[i].hash) {
            return false;
        }
    }
    for (size_t i = 0; i < image->hole_count; i++) {
        const SiHole* hole = &image->holes[i];
        if (hole->to_segment == SI_TARGET_FREE || hole->kind == SiHole_Parameter) {
            continue;
        }
        int64_t target = 0;
        if (!hole_target(hole, place, data, size, &target)) {
            return false;
        }
        if (target != place->base[hole->to_segment] + hole->to_offset) {
            return false;
        }
    }
    // The command table: each entry is an offset from the table that leads to the handler the image names.
    const int64_t table = ref_offset(&image->command_table, place);
    for (size_t i = 0; i < image->command_count; i++) {
        uint16_t entry = 0;
        if (!word_at(data, size, table + 2 * (int64_t)i, &entry)) {
            return false;
        }
        const SiTableEntry* want = &image->commands[i];
        if (table + (int16_t)entry != place->base[want->to_segment] + want->to_offset) {
            return false;
        }
    }
    return true;
}

// The data of the driver

// A range of the module the port reads as data or keeps in a structure.
typedef struct Range {
    int64_t start;
    int64_t end;
} Range;

static bool overlaps(Range a, Range b) {
    return a.start < b.end && b.start < a.end;
}

// What the eagleplayer does to the module (player.asm). InitPlayer takes the first bsr.w behind the tick entry for the
// song start (when its displacement is at most $900), counts the songs from it and patches its `cmp.b`; it rewrites the
// first rte word of the module (the audio handler's) to rts. EP_ModuleChange rewrites every match of its ten patterns
// that starts in the first $A5A bytes. The port models the driver as patched at the places the image names, so the
// module must have the driver's entry, the song start and the rte exactly there, and the patterns must match at exactly
// the image's patch sites.

#define PATCH_LIMIT 0xA5Au
#define PATCH_FLAG_LONG 0x3D4000D8u // `move.w d0,$d8(a6)` in the entry routine makes InitSound set a header byte
#define OPCODE_BSR_W 0x6100u
#define OPCODE_RTE 0x4E73u
#define SONG_CALL_MAX_DISPLACEMENT 0x900

typedef struct PatchPattern {
    uint8_t length; // in words
    uint16_t words[5];
} PatchPattern;

static const PatchPattern k_patch_patterns[] = {
    { 5, { 0x21C8, 0x0070, 0x3D7C, 0x00FF, 0x009E } }, // move.l a0,$70.w; move.w #$ff,$9e(a6): the audio servers
    { 3, { 0x24D0, 0x34A8, 0x0004 } },                 // move.l (a0),(a2)+; move.w 4(a0),(a2): AUDxLC / AUDxLEN
    { 3, { 0x3368, 0x0006, 0x0006 } },                 // move.w 6(a0),6(a1): AUDxPER
    { 4, { 0x3340, 0x0008, 0x2268, 0x0012 } },         // move.w d0,8(a1); movea.l $12(a0),a1: AUDxVOL
    { 3, { 0xEC48, 0x3D40, 0x00A8 } },                 // lsr.w #6,d0; move.w d0,$a8(a6) (the other drivers' volume)
    { 3, { 0xEC48, 0x3D40, 0x00B8 } },
    { 3, { 0xEC48, 0x3D40, 0x00C8 } },
    { 3, { 0xEC48, 0x3D40, 0x00D8 } },
    { 3, { 0x2268, 0x000E, 0x3019 } }, // movea.l $e(a0),a1; move.w (a1)+,d0: the list read, the song end hook
    { 3, { 0x2B78, 0x0070, 0x000A } }, // move.l $70.w,$a(a5)
};

static bool add_patched(SiLayout* layout, int64_t start, int64_t end) {
    if (layout->patched_count == SI_MAX_PATCHED || start < 0) {
        return false;
    }
    layout->patched[layout->patched_count++] = (SiRange) { (uint32_t)start, (uint32_t)end };
    return true;
}

static bool pattern_at(const SiModule* module, size_t at, const PatchPattern* pattern) {
    if (at + 2u * pattern->length > module->size) {
        return false;
    }
    for (uint32_t j = 0; j < pattern->length; j++) {
        if (nuade_read_be16(module->memory + at + 2u * j) != pattern->words[j]) {
            return false;
        }
    }
    return true;
}

// The place of the image's Parameter hole: the song count immediate of the driver's `cmp.b`. -1 if there is none.

static int64_t parameter_offset(const SiImage* image, const Placement* place) {
    for (size_t i = 0; i < image->hole_count; i++) {
        if (image->holes[i].kind == SiHole_Parameter) {
            return place->base[image->holes[i].segment] + 2 * (int64_t)image->holes[i].word;
        }
    }
    return -1;
}

static bool word_is(const SiModule* module, int64_t offset, uint16_t value) {
    uint16_t word = 0;
    return word_at(module->memory, module->size, offset, &word) && word == value;
}

// InitPlayer finds the places it counts the songs and measures the samples from by scanning words from the song start
// routine (player.asm $7ea, $840): the first `cmp.b #imm` ($B03C) holds the count, the first `lea` ($43FA / $43FB)
// behind it the song table (three list words per entry if the word before the `lea` is not $0A, four if it is), and the
// first `subq.b` ($532D) behind the call the sample records. They must land where the image has the count immediate,
// the song table and the first voice record, or the eagleplayer counts something else.

#define SCAN_COUNT_COMPARE 0xB03Cu
#define SCAN_LEA_PC_INDEX 0x43FBu
#define SCAN_LEA_PC 0x43FAu
#define SCAN_LEA_FOUR_LISTS 0x000Au
#define SCAN_SUBQ 0x532Du

static bool check_scans(const SiImage* image, const Placement* place, const SiModule* module, size_t call) {
    const int64_t size = (int64_t)module->size;
    const int64_t start = (int64_t)call + 2 + (int16_t)nuade_read_be16(module->memory + call + 2);
    // The scans step by words: an odd place would see other words than the image's.
    for (size_t i = 0; i < image->segment_count; i++) {
        if (place->base[i] % 2 != 0) {
            return false;
        }
    }
    int64_t at = start;
    while (at >= 0 && at + 2 <= size && !word_is(module, at, SCAN_COUNT_COMPARE)) {
        at += 2;
    }
    const int64_t count = at + 2;
    if (count + 2 > size || count != parameter_offset(image, place)) {
        return false;
    }
    at = count;
    while (at + 2 <= size && !word_is(module, at, SCAN_LEA_PC_INDEX) && !word_is(module, at, SCAN_LEA_PC)) {
        at += 2;
    }
    uint16_t displacement = 0;
    if (at < 4 || !word_at(module->memory, module->size, at + 2, &displacement)
        || !word_is(module, at - 2, SCAN_LEA_FOUR_LISTS)
        || at + 2 + (int16_t)displacement != ref_offset(&image->song_table, place)) {
        return false;
    }
    at = (int64_t)call + 2;
    while (at + 2 <= size && !word_is(module, at, SCAN_SUBQ)) {
        at += 2;
    }
    const int64_t records = at + 6;
    return records + 2 <= size
           && records + (int16_t)nuade_read_be16(module->memory + records) == ref_offset(&image->record[0], place);
}

static bool check_eagleplayer(const SiImage* image, const Placement* place, SiModule* module) {
    const uint8_t* d = module->memory;
    const size_t size = module->size;
    const int64_t entry = 2 + (int64_t)nuade_read_be16(d + 2);
    if (entry != ref_offset(&image->entry, place)) {
        return false;
    }
    // The first bsr.w behind the entry is the song start, and a short displacement keeps it so.
    size_t call = (size_t)entry;
    for (;; call += 2) {
        if (call + 4 > size) {
            return false;
        }
        if (nuade_read_be16(d + call) == OPCODE_BSR_W) {
            break;
        }
        if (nuade_read_be32(d + call) == PATCH_FLAG_LONG) {
            return false;
        }
    }
    if ((int64_t)call != ref_offset(&image->song_call, place)
        || (int16_t)nuade_read_be16(d + call + 2) > SONG_CALL_MAX_DISPLACEMENT) {
        return false;
    }
    if (!check_scans(image, place, module, call)) {
        return false;
    }
    // The first rte of the module.
    size_t rte = 0;
    while (rte + 2 <= size && nuade_read_be16(d + rte) != OPCODE_RTE) {
        rte += 2;
    }
    if ((int64_t)rte != ref_offset(&image->rte, place)
        || !add_patched(&module->layout, (int64_t)rte, (int64_t)rte + 2)) {
        return false;
    }
    // The pattern matches in the first $A5A bytes are the image's patch sites, none across the limit.
    size_t matches = 0;
    for (size_t at = 0; at < PATCH_LIMIT && at < size; at++) {
        for (size_t p = 0; p < nuade_sizeof_array(k_patch_patterns); p++) {
            const PatchPattern* pattern = &k_patch_patterns[p];
            if (!pattern_at(module, at, pattern)) {
                continue;
            }
            if (at + 2u * pattern->length > PATCH_LIMIT) {
                return false;
            }
            bool expected = false;
            for (size_t i = 0; i < image->patch_site_count; i++) {
                const SiPatchSite* site = &image->patch_sites[i];
                expected = expected || (site->pattern == p && ref_offset(&site->at, place) == (int64_t)at);
            }
            if (!expected || !add_patched(&module->layout, (int64_t)at, (int64_t)at + 2 * pattern->length)) {
                return false;
            }
            matches++;
        }
    }
    return matches == image->patch_site_count;
}

// The header of a module that was saved from a running game holds the DMACON and INTENA words of a note start that the
// first tick writes: the voices of the DMACON word start (DMA on, interrupt on) at the first tick. Only the form the
// driver itself writes is modelled: both words set the same voices.

static bool read_pending_starts(SiModule* module) {
    const uint16_t dma = nuade_read_be16(module->memory + SI_HEADER_OFFSET + HEADER_PENDING_DMA);
    const uint16_t interrupt = nuade_read_be16(module->memory + SI_HEADER_OFFSET + HEADER_PENDING_INTERRUPT);
    if (dma == 0 && interrupt == 0) {
        return true;
    }
    if ((dma & 0xFFF0u) != DMA_SET_ENABLE || (interrupt & 0xF87Fu) != INTERRUPT_SET) {
        return false;
    }
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        const bool by_dma = (dma & (1u << i)) != 0;
        const bool by_interrupt = (interrupt & (INTERRUPT_AUD0 << i)) != 0;
        if (by_dma != by_interrupt) {
            return false;
        }
        module->layout.start_pending[i] = by_dma;
    }
    return true;
}

// Where the tables and the voice records are. The image pins each place; this checks that it lies in the module and
// that what the port assumes of it holds.

static bool resolve_layout(const SiImage* image, const Placement* place, SiModule* module) {
    SiLayout* layout = &module->layout;
    layout->record_bytes = image->record_bytes;
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        const int64_t offset = ref_offset(&image->record[i], place);
        if (offset < 0 || (uint64_t)offset > module->size || module->size - (size_t)offset < layout->record_bytes) {
            return false;
        }
        layout->record[i] = (uint32_t)offset;
    }
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        // The hardware address, the DMA word and the interrupt word of the record are the channel's.
        const SiRecord record = si_module_record(module, i);
        // The volume index (volume times master volume / 16) must stay a byte: the high byte of the volume is 0.
        if (record.volume_high != 0 || record.hardware != CUSTOM_AUD0 + CUSTOM_AUD_STRIDE * i
            || record.dma != (DMA_SET_ENABLE | (1u << i))
            || record.interrupt != (INTERRUPT_SET | (INTERRUPT_AUD0 << i))) {
            return false;
        }
    }
    const int64_t tables[] = { ref_offset(&image->volume_table, place),   ref_offset(&image->period_table, place),
                               ref_offset(&image->sample_table, place),   ref_offset(&image->song_table, place),
                               ref_offset(&image->arpeggio_table, place), ref_offset(&image->command_table, place) };
    for (size_t i = 0; i < nuade_sizeof_array(tables); i++) {
        if (tables[i] < 0 || tables[i] > (int64_t)module->size) {
            return false;
        }
    }
    layout->volume_table = (uint32_t)tables[0];
    layout->period_table = (uint32_t)tables[1];
    layout->sample_table = (uint32_t)tables[2];
    layout->song_table = (uint32_t)tables[3];
    layout->arpeggio_table = (uint32_t)tables[4];
    layout->command_table = (uint32_t)tables[5];
    // The sample table ends where the first voice record starts.
    if (layout->record[0] <= layout->sample_table
        || (layout->record[0] - layout->sample_table) % SI_SAMPLE_BYTES != 0) {
        return false;
    }
    layout->sample_count = (layout->record[0] - layout->sample_table) / SI_SAMPLE_BYTES;
    // The end command is data the driver reads. Inside a voice record it can only be the last byte, which the version
    // without voice flags never writes.
    const int64_t end_command = ref_offset(&image->end_command, place);
    if (end_command < 0 || end_command >= (int64_t)module->size) {
        return false;
    }
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        const Range record = { layout->record[i], (int64_t)layout->record[i] + layout->record_bytes };
        if (overlaps(record, (Range) { end_command, end_command + 1 })
            && (image->voice_flags || end_command != record.end - 1)) {
            return false;
        }
    }
    layout->end_command = module->memory[end_command];
    return read_pending_starts(module);
}

// The eagleplayer counts the songs of a module: the lowest non-zero list offset of the first `count` song table
// entries (the count that the driver's own song check holds), divided by the size of an entry. The eagleplayer patches
// the driver's check to what it counted, so the module has exactly that many songs.

static bool count_songs(const SiImage* image, const Placement* place, SiModule* module) {
    // The count is the `cmp.b` immediate of the driver, which the eagleplayer patches.
    const int64_t immediate = parameter_offset(image, place);
    uint16_t word = 0;
    if (immediate < 0 || !word_at(module->memory, module->size, immediate, &word)) {
        return false;
    }
    const uint32_t entries = word & 0xFF;
    uint32_t lowest = 0;
    for (uint32_t entry = 0; entry < entries; entry++) {
        SiSong song;
        if (!si_module_song(module, entry, &song)) {
            return false;
        }
        if (entry == 0) {
            lowest = song.list[0];
        }
        for (uint32_t voice = 0; voice < SI_VOICES; voice++) {
            if (song.list[voice] != 0 && lowest >= song.list[voice]) {
                lowest = song.list[voice];
            }
        }
    }
    // The eagleplayer divides in 16 bits: the remainder would become the high word of the count; the count plus one is
    // written into a byte; and two sample ends make it divide by 8 (player.asm $896).
    module->layout.song_count = lowest / SI_SONG_BYTES;
    if (lowest % SI_SONG_BYTES != 0 || module->layout.song_count == 0 || module->layout.song_count > 254) {
        return false;
    }
    // The extent of the samples: the end of the furthest one, from the sample's offset and length (not its loop).
    uint32_t extent = 0;
    for (uint32_t i = 0; i < module->layout.sample_count; i++) {
        const SiSample sample = si_module_sample(module, i);
        const uint32_t end = (uint32_t)(sample.offset - SI_HEADER_OFFSET) + ((sample.length_words + 2u) & 0xFFFFu) * 2u;
        extent = end > extent ? end : extent;
    }
    if (extent == SAMPLE_END_DIVIDES_BY_EIGHT || extent == SAMPLE_END_MOVES_SONG_FLAG) {
        return false;
    }
    return add_patched(&module->layout, immediate, immediate + 2);
}

// The driver writes its header and its voice records, which the port keeps in structures: no data the driver reads
// may lie in them. The data it reads at fixed places: the tables (as far as an index can reach), the samples.

static bool check_data_ranges(const SiModule* module) {
    const SiLayout* layout = &module->layout;
    const Range live[] = {
        { SI_HEADER_OFFSET, SI_HEADER_OFFSET + SI_HEADER_BYTES },
        { layout->record[0], (int64_t)layout->record[0] + layout->record_bytes },
        { layout->record[1], (int64_t)layout->record[1] + layout->record_bytes },
        { layout->record[2], (int64_t)layout->record[2] + layout->record_bytes },
        { layout->record[3], (int64_t)layout->record[3] + layout->record_bytes },
    };
    // The volume index is a byte (the volume times the master volume / 16), the period index 0 .. $7e, the command
    // table is as long as the image says, the sample table as the records start.
    const Range data[] = {
        { layout->volume_table, (int64_t)layout->volume_table + 2 * 256 },
        { layout->period_table, (int64_t)layout->period_table + 2 * 64 },
        { layout->sample_table, (int64_t)layout->sample_table + (int64_t)layout->sample_count * SI_SAMPLE_BYTES },
        { layout->command_table, (int64_t)layout->command_table + 2 * (int64_t)module->image->command_count },
    };
    for (size_t i = 0; i < nuade_sizeof_array(live); i++) {
        for (size_t j = 0; j < nuade_sizeof_array(data); j++) {
            if (overlaps(live[i], data[j])) {
                return false;
            }
        }
        for (size_t j = i + 1; j < nuade_sizeof_array(live); j++) {
            if (overlaps(live[i], live[j])) {
                return false;
            }
        }
    }
    // The samples: every entry starts in the module and lies clear of the variables.
    for (uint32_t i = 0; i < layout->sample_count; i++) {
        const SiSample sample = si_module_sample(module, i);
        const int64_t length = sample.length_words == 0 ? 0x20000 : 2 * (int64_t)sample.length_words;
        const int64_t loop = sample.loop_length_words == 0 ? 0x20000 : 2 * (int64_t)sample.loop_length_words;
        const Range ranges[]
            = { { sample.offset, sample.offset + length }, { sample.loop_offset, sample.loop_offset + loop } };
        for (size_t r = 0; r < nuade_sizeof_array(ranges); r++) {
            // A sample that runs past the end of the module plays the zeros that follow it in chip memory.
            if (ranges[r].start < SI_HEADER_OFFSET + SI_HEADER_BYTES || ranges[r].start >= (int64_t)module->size
                || ranges[r].end > (int64_t)module->size + NUADE_PAULA_MAX_SAMPLE_BYTES) {
                return false;
            }
            for (size_t j = 0; j < nuade_sizeof_array(live); j++) {
                if (overlaps(live[j], ranges[r])) {
                    return false;
                }
            }
            // Paula reads the file's bytes where the driver sees the eagleplayer's patches.
            for (uint32_t j = 0; j < layout->patched_count; j++) {
                if (overlaps((Range) { layout->patched[j].start, layout->patched[j].end }, ranges[r])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool si_module_open(SiModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (!check2(data, size)) {
        return false;
    }
    for (size_t i = 0; i < si_image_count; i++) {
        Placement place;
        if (!match_image(&si_images[i], data, size, &place)) {
            continue;
        }
        module->image = &si_images[i];
        module->size = size;
        module->memory = nuade_paula_padded_copy(data, size);
        if (module->memory == nullptr || !resolve_layout(module->image, &place, module)
            || !check_eagleplayer(module->image, &place, module) || !count_songs(module->image, &place, module)
            || !check_data_ranges(module) || !si_module_walk(module)) {
            si_module_close(module);
            return false;
        }
        return true;
    }
    return false;
}

void si_module_close(SiModule* module) {
    free(module->memory);
    memset(module, 0, sizeof(*module));
}

// The places the port does not read from the file: the driver's variables (kept in structures) and the words that the
// eagleplayer rewrites (the driver sees its patches there).

static bool unreadable(const SiModule* module, int64_t offset) {
    if (offset >= SI_HEADER_OFFSET && offset < (int64_t)SI_HEADER_OFFSET + SI_HEADER_BYTES) {
        return true;
    }
    for (uint32_t i = 0; i < module->layout.patched_count; i++) {
        if (offset >= module->layout.patched[i].start && offset < module->layout.patched[i].end) {
            return true;
        }
    }
    for (size_t i = 0; i < SI_VOICES; i++) {
        if (offset >= module->layout.record[i]
            && offset < (int64_t)module->layout.record[i] + module->layout.record_bytes) {
            return true;
        }
    }
    return false;
}

bool si_module_u8(const SiModule* module, int64_t offset, uint8_t* value) {
    if (offset < 0 || offset >= (int64_t)module->size || unreadable(module, offset)) {
        return false;
    }
    *value = module->memory[offset];
    return true;
}

bool si_module_u16(const SiModule* module, int64_t offset, uint16_t* value) {
    uint8_t high = 0;
    uint8_t low = 0;
    if (!si_module_u8(module, offset, &high) || !si_module_u8(module, offset + 1, &low)) {
        return false;
    }
    *value = (uint16_t)((high << 8) | low);
    return true;
}

// A song table entry: the offsets of the four voices' lists and the tempo, the high byte of the fifth word.

bool si_module_song(const SiModule* module, uint32_t index, SiSong* song) {
    const int64_t entry = (int64_t)module->layout.song_table + (int64_t)index * SI_SONG_BYTES;
    for (uint32_t voice = 0; voice < SI_VOICES; voice++) {
        if (!si_module_u16(module, entry + 2 * (int64_t)voice, &song->list[voice])) {
            return false;
        }
    }
    return si_module_u8(module, entry + 2 * SI_VOICES, &song->tempo);
}

// The sample table entries hold offsets from the header, where the driver's address register points.

SiSample si_module_sample(const SiModule* module, uint32_t index) {
    const uint8_t* entry = module->memory + module->layout.sample_table + (size_t)index * SI_SAMPLE_BYTES;
    return (SiSample) {
        .offset = (int64_t)nuade_read_be32(entry) + SI_HEADER_OFFSET,
        .length_words = nuade_read_be16(entry + 4),
        .loop_offset = (int64_t)nuade_read_be32(entry + 6) + SI_HEADER_OFFSET,
        .loop_length_words = nuade_read_be16(entry + 10),
    };
}

static uint8_t record_u8(const SiModule* module, const uint8_t* record, uint32_t offset) {
    return record[module->layout.record_bytes == SI_RECORD_BYTES_NARROW && offset >= 0x20 ? offset - 2 : offset];
}

static uint16_t record_u16(const SiModule* module, const uint8_t* record, uint32_t offset) {
    return (uint16_t)((record_u8(module, record, offset) << 8) | record_u8(module, record, offset + 1));
}

static uint32_t record_u32(const SiModule* module, const uint8_t* record, uint32_t offset) {
    return ((uint32_t)record_u16(module, record, offset) << 16) | record_u16(module, record, offset + 2);
}

// The fields of a voice record; the offsets are those of the wide record.

SiRecord si_module_record(const SiModule* module, uint32_t voice) {
    const uint8_t* r = module->memory + module->layout.record[voice];
    return (SiRecord) {
        .length_words = record_u16(module, r, 0x04),
        .period = record_u16(module, r, 0x06),
        .volume_high = record_u8(module, r, 0x08),
        .volume = record_u8(module, r, 0x09),
        .hardware = record_u32(module, r, 0x0A),
        .duration = record_u8(module, r, 0x17),
        .note = record_u8(module, r, 0x18),
        .note_step = record_u8(module, r, 0x19),
        .loop_length_words = record_u16(module, r, 0x20),
        .dma = record_u16(module, r, 0x22),
        .volume_base = record_u8(module, r, 0x24),
        .attack_step = record_u8(module, r, 0x25),
        .attack_count = record_u8(module, r, 0x26),
        .decay_step = record_u8(module, r, 0x27),
        .decay_count = record_u8(module, r, 0x28),
        .release_step = record_u8(module, r, 0x29),
        .release_start = record_u8(module, r, 0x2A),
        .release_on = record_u8(module, r, 0x2B),
        .attack_left = record_u8(module, r, 0x2C),
        .decay_left = record_u8(module, r, 0x2D),
        .vibrato_depth = record_u8(module, r, 0x2E),
        .vibrato_rate = record_u8(module, r, 0x2F),
        .vibrato_left = record_u8(module, r, 0x30),
        .slide_left = record_u8(module, r, 0x31),
        .slide_step = record_u16(module, r, 0x32),
        .bend_offset = record_u16(module, r, 0x34),
        .bend_ticks = record_u8(module, r, 0x36),
        .note_length = record_u8(module, r, 0x37),
        .interrupt = record_u16(module, r, 0x38),
        .attack_wait = record_u8(module, r, 0x3A),
        .attack_rate = record_u8(module, r, 0x3B),
        .decay_wait = record_u8(module, r, 0x3C),
        .decay_rate = record_u8(module, r, 0x3D),
        .release_wait = record_u8(module, r, 0x3E),
        .release_rate = record_u8(module, r, 0x3F),
        .vibrato_delay = record_u8(module, r, 0x40),
        .arpeggio_index = record_u8(module, r, 0x41),
        .transpose = record_u8(module, r, 0x46),
        .active = record_u8(module, r, 0x47),
    };
}
