// SPDX-License-Identifier: MIT
// Dynamic Synthesizer player: the module check, the code scan that finds the replayer's tables, and the readers of the
// module's records. Addresses in the code are link addresses; `link_base` is subtracted to get offsets.

#include "dns_internal.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define NOT_FOUND 0xFFFFFFFFu
#define MIN_MODULE_SIZE 0x40
#define ENTRY_POINTS 5 // the `bra.w` at the start
#define MAX_PATTERNS 256
#define MAX_SUBSONGS 32
#define MAX_MODULE_SIZE 0x1000000u
#define MAX_SAMPLES_SIZE 0x10000000u
#define SONG_FIELD_SIZE 0x20 // bytes of a field of the song table: 16 songs of one word
#define INSTRUMENT_SIZE 96
#define ROW_SIZE 24 // bytes of a row: six tracks of four bytes
#define PATTERN_SIZE 0x60
#define PERIOD_COUNT 64

// 68000 instruction words, by mnemonic. `abs` is an absolute long that follows.
#define OP_BRA_W 0x6000
#define OP_BSR_W 0x6100
#define OP_RTS 0x4E75
#define OP_MOVE_L_A0_ABS 0x23C8 // move.l a0,abs
#define OP_CLR_W_ABS 0x4279
#define OP_CMPI_W_ABS 0x0C79 // cmpi.w #n,abs
#define OP_ANDI_W_7E 0x007E  // the immediate of `andi.w #$7e,d0`, which precedes `lea abs,a1`
#define OP_LEA_ABS_A0 0x41F9
#define OP_LEA_ABS_A1 0x43F9
#define OP_LEA_ABS_A2 0x45F9
#define OP_LEA_ABS_A3 0x47F9
#define OP_LEA_ABS_A4 0x49F9
#define OP_LEA_ABS_A5 0x4BF9
#define OP_ASL_W_1_D0 0xE340
#define OP_ASL_L_1_D0 0xE380
#define OP_ANDI_L_D0 0x0280      // andi.l #n,d0 (the long follows)
#define OP_ANDI_L_FFFE_D0 0xFFFE // the last word of `andi.l #$fffe,d0`, which precedes `lea abs,a0`
#define OP_SUBI_W_60_D2 0x0442   // subi.w #$60,d2 (the immediate follows)
#define OP_ADDI_L_D2 0x0682
#define OP_MULU_W_D0 0xC0FC // mulu.w #n,d0
#define OP_ADDA_L_D0_A0 0xD1C0
#define OP_MOVE_B_D7_D5 0x1A07
#define OP_LSR_B_2_D5 0xE40D
#define OP_LSR_B_4_D5 0xE80D
#define OP_ANDI_B_D5 0x0205
#define OP_CMPI_W_IDX_A1_D7 0x0C71
#define OP_MOVE_W_IMM_IDX_A1_D7 0x33BC
#define EXT_D7_W 0x7000        // the index extension word (a1,d7.w)
#define PERIOD_MARKER_0 0x06AE // the first periods
#define PERIOD_MARKER_1 0x064E
#define CHECK_BRANCH_SIZE 4

uint8_t dns_u8(const DnsMemory* m, int64_t offset) {
    return offset >= 0 && offset < (int64_t)m->size ? m->data[offset] : 0;
}

uint16_t dns_u16(const DnsMemory* m, int64_t offset) {
    return (uint16_t)(dns_u8(m, offset) << 8 | dns_u8(m, offset + 1));
}

uint32_t dns_u32(const DnsMemory* m, int64_t offset) {
    return (uint32_t)dns_u16(m, offset) << 16 | dns_u16(m, offset + 2);
}

bool dns_samples_init(DnsSamples* samples, const uint8_t* data, size_t size) {
    if (size > MAX_SAMPLES_SIZE) {
        return false;
    }
    // the Paula tail also holds the first long that the replayer writes when the file is shorter
    samples->data = nuade_paula_padded_copy(data, size);
    if (samples->data == nullptr) {
        return false;
    }
    memcpy(samples->head, samples->data, sizeof(samples->head));
    samples->size = (uint32_t)size;
    return true;
}

void dns_samples_free(DnsSamples* samples) {
    free(samples->data);
    samples->data = nullptr;
}

// The eagleplayer's Check2: five `bra.w` with even, positive displacements; the last one leads to `move.l a0,abs`
// and `rts` (the entry that sets the sample base).

static bool check_entry_points(const DnsMemory* m) {
    uint32_t at = 0;
    for (int i = 0; i < ENTRY_POINTS; i++) {
        if (dns_u16(m, at) != OP_BRA_W) {
            return false;
        }
        const int16_t displacement = (int16_t)dns_u16(m, at + 2);
        at += CHECK_BRANCH_SIZE;
        if (displacement < 0 || (displacement & 1) != 0) {
            return false;
        }
    }
    const uint32_t last = at - 2;
    const uint32_t target = last + (uint32_t)(int32_t)(int16_t)dns_u16(m, last);
    return dns_u16(m, target) == OP_MOVE_L_A0_ABS && dns_u16(m, target + 6) == OP_RTS;
}

#define ANY 0x10000u // in a code pattern: any word

// The first even offset in [from, to) where the words of `pattern` follow each other.

static uint32_t find_words(const DnsMemory* m, uint32_t from, uint32_t to, const uint32_t* pattern, size_t count) {
    for (uint32_t at = from & ~1u; at + count * 2 <= to && at + count * 2 <= m->size; at += 2) {
        size_t i = 0;
        while (i < count && (pattern[i] == ANY || pattern[i] == dns_u16(m, at + (uint32_t)i * 2))) {
            i++;
        }
        if (i == count) {
            return at;
        }
    }
    return NOT_FOUND;
}

// A link address in the code, as an offset into the module; NOT_FOUND for an address below the link base.

static uint32_t operand_offset(const DnsModule* module, uint32_t at) {
    const uint32_t address = dns_u32(&module->memory, at);
    return address >= module->link_base ? address - module->link_base : NOT_FOUND;
}

// The lea sequence at the end of a sequencer step that loads the six tracks' tables:
//   lea notes,a0; lea var,a1; lea note_adds,a2; lea var,a3; lea sample_adds,a4; lea var,a5; bsr

static bool find_track_tables(DnsModule* module, uint32_t code_end) {
    static const uint32_t pattern[] = { OP_LEA_ABS_A0, ANY, ANY, OP_LEA_ABS_A1, ANY, ANY, OP_LEA_ABS_A2, ANY, ANY,
                                        OP_LEA_ABS_A3, ANY, ANY, OP_LEA_ABS_A4, ANY, ANY, OP_LEA_ABS_A5 };
    uint32_t at = 0;
    for (int track = 0; track < DNS_TRACKS; track++) {
        at = find_words(&module->memory, at, code_end, pattern, nuade_sizeof_array(pattern));
        if (at == NOT_FOUND) {
            return false;
        }
        DnsTrackTables* t = &module->tracks[track];
        t->patterns = operand_offset(module, at + 2);
        t->note_adds = operand_offset(module, at + 14);
        t->sample_adds = operand_offset(module, at + 26);
        if (t->patterns == NOT_FOUND || t->note_adds == NOT_FOUND || t->sample_adds == NOT_FOUND) {
            return false;
        }
        at += nuade_sizeof_array(pattern) * 2;
    }
    return true;
}

// The sequencer's tables. The newer replayers have a table of pattern pointers: the long that follows the `rts` that
// ends the song start routine points to it, and its first entry (the first pattern) ends it.

static bool find_pattern_table(DnsModule* module, uint32_t first_variable) {
    const DnsMemory* m = &module->memory;
    if (first_variable < 6 || dns_u16(m, first_variable - 6) != OP_RTS) {
        return false;
    }
    module->pattern_table = operand_offset(module, first_variable - 4);
    if (module->pattern_table == NOT_FOUND) {
        return false;
    }
    module->first_pattern = operand_offset(module, module->pattern_table);
    if (module->first_pattern == NOT_FOUND || module->first_pattern <= module->pattern_table) {
        return false;
    }
    module->pattern_count = (module->first_pattern - module->pattern_table) / 4;
    return module->pattern_count <= MAX_PATTERNS;
}

// The rows version: `lea rows,a0; lea dest,a2; moveq #0,d0; move.w pos,d0; mulu.w #24,d0; adda.l d0,a0`

static bool find_rows(DnsModule* module, uint32_t code_end) {
    static const uint32_t pattern[] = { OP_MULU_W_D0, ROW_SIZE, OP_ADDA_L_D0_A0 };
    const uint32_t at = find_words(&module->memory, 0, code_end, pattern, nuade_sizeof_array(pattern));
    if (at == NOT_FOUND || at < 20 || dns_u16(&module->memory, at - 20) != OP_LEA_ABS_A0) {
        return false;
    }
    module->rows = operand_offset(module, at - 18);
    return module->rows != NOT_FOUND;
}

// The fixed patterns version: `subi.w #$60,d2; addi.l #first,d2` turns the pattern number into an address.

static bool find_fixed_patterns(DnsModule* module, uint32_t code_end) {
    static const uint32_t pattern[] = { OP_SUBI_W_60_D2, PATTERN_SIZE, OP_ADDI_L_D2 };
    const uint32_t at = find_words(&module->memory, 0, code_end, pattern, nuade_sizeof_array(pattern));
    if (at == NOT_FOUND) {
        return false;
    }
    module->first_pattern = operand_offset(module, at + 6);
    return module->first_pattern != NOT_FOUND;
}

// The note's volume offset: `move.b d7,d5` is followed by `lsr.b #2,d5`, `lsr.b #4,d5` or `andi.b #$f0,d5; lsr.b
// #2,d5`.

static bool find_volume_decode(DnsVersion* version, const DnsMemory* m, uint32_t code_end) {
    static const uint32_t marker[] = { OP_MOVE_B_D7_D5 };
    const uint32_t at = find_words(m, 0, code_end, marker, nuade_sizeof_array(marker));
    if (at == NOT_FOUND) {
        return false;
    }
    const uint16_t next = dns_u16(m, at + 2);
    if (next == OP_LSR_B_2_D5) {
        version->volume_mask = 0xFF;
        version->volume_shift = 2;
    } else if (next == OP_LSR_B_4_D5) {
        version->volume_mask = 0xFF;
        version->volume_shift = 4;
    } else if (next == OP_ANDI_B_D5 && dns_u16(m, at + 6) == OP_LSR_B_2_D5) {
        version->volume_mask = (uint8_t)dns_u16(m, at + 4);
        version->volume_shift = 2;
    } else {
        return false;
    }
    return true;
}

// The state that the note sets in the DMA step: `lea dma_states,a1; move.w #n,(a1,d7.w)`.

static bool find_dma_note_state(DnsVersion* version, const DnsMemory* m, uint32_t code_end) {
    static const uint32_t pattern[] = { OP_LEA_ABS_A1, ANY, ANY, OP_MOVE_W_IMM_IDX_A1_D7, ANY, EXT_D7_W };
    const uint32_t at = find_words(m, 0, code_end, pattern, nuade_sizeof_array(pattern));
    if (at == NOT_FOUND) {
        return false;
    }
    const uint16_t state = dns_u16(m, at + 8);
    if (state != DnsDmaState_Start && state != DnsDmaState_Delay) {
        return false;
    }
    version->dma_note_state = (DnsDmaState)state;
    return true;
}

// The replayer versions, from the sequencer and the one instruction that tells the two table versions apart (the
// later one skips a note-off for a voice that has been released: `cmpi.w #$7fff,(a1,d7.w)`).

static DnsVersion base_version(DnsSequencer sequencer, bool has_age_guard) {
    DnsVersion v = { .sequencer = sequencer,
                     .tick_length = DNS_TICK_LENGTH_50HZ,
                     .note_clears_sample_base = true,
                     .age_counts_in_dma_step = true,
                     .silent_age = 0xFFF0,
                     .retrigger_releases = true,
                     .note_off_guard = DnsNoteOffGuard_None };
    if (sequencer == DnsSequencer_FixedPatterns) {
        v.tick_length = DNS_TICK_LENGTH_100HZ;
        v.end_is_exact = true;
        v.retrigger_releases = false;
    } else if (sequencer == DnsSequencer_Rows) {
        v.note_clears_sample_base = false;
        v.age_counts_in_dma_step = false;
        v.silent_age = 0xFFFF;
        v.ages_start_silent = true;
        v.note_off_guard = DnsNoteOffGuard_Map;
        v.note_unmaps_tracks = true;
    } else if (has_age_guard) {
        v.new_position_skips_voices = true;
        v.note_clears_sample_base = false;
        v.age_counts_in_dma_step = false;
        v.silent_age = 0xFFFF;
        v.ages_start_silent = true;
        v.note_off_guard = DnsNoteOffGuard_Age;
    }
    return v;
}

// SubSongRange: the eagleplayer counts the songs of the first field of the song table up to the first pair of equal
// words, at most 32.

static uint32_t count_subsongs(const DnsModule* module) {
    uint32_t count = 1;
    uint32_t at = module->songs + 2;
    while (dns_u16(&module->memory, at) != dns_u16(&module->memory, at + 2) && count < MAX_SUBSONGS) {
        count++;
        at += 2;
    }
    return count;
}

// What the code gives besides the module's tables.
typedef struct CodeAddresses {
    uint32_t code_end;
    uint32_t first_variable;
    uint32_t song_lea;
    uint16_t song_offset_mask; // `andi.l #n,d0` after `lea songs,a0`: the mask of the doubled song number
} CodeAddresses;

// The addresses that the code gives: the link base from the period table, the first two variables (the code ends at
// the second), the song table, the sample records.

static bool find_addresses(DnsModule* module, CodeAddresses* out) {
    const DnsMemory* m = &module->memory;
    // The period table: `andi.w #$7e,d0; lea periods,a1`; its link address minus its offset is the link base.
    static const uint32_t lea_periods[] = { OP_ANDI_W_7E, OP_LEA_ABS_A1 };
    const uint32_t lea = find_words(m, 0, m->size, lea_periods, nuade_sizeof_array(lea_periods));
    static const uint32_t period_start[] = { PERIOD_MARKER_0, PERIOD_MARKER_1 };
    const uint32_t periods = lea != NOT_FOUND ? find_words(m, lea + 4, m->size, period_start, 2) : NOT_FOUND;
    if (periods == NOT_FOUND) {
        return false;
    }
    const uint32_t address = dns_u32(m, lea + 4);
    if (address < periods) {
        return false;
    }
    module->link_base = address - periods;
    module->periods = periods;
    // The hardware init routine (the second entry point) begins with two `clr.w abs`: the play flag and the flag that
    // a note has played, the first variables.
    const uint32_t start = 6 + (uint32_t)(int32_t)(int16_t)dns_u16(m, 6);
    if (dns_u16(m, start) != OP_CLR_W_ABS || dns_u16(m, start + 6) != OP_CLR_W_ABS) {
        return false;
    }
    out->first_variable = operand_offset(module, start + 2);
    out->code_end = operand_offset(module, start + 8);
    if (out->first_variable == NOT_FOUND || out->code_end == NOT_FOUND || out->code_end > m->size) {
        return false;
    }
    // The song table: `asl #1,d0; lea songs,a0; andi.l #mask,d0`.
    uint32_t at = 0;
    for (;; at += 2) {
        if (at + 6 > out->code_end) {
            return false;
        }
        const uint16_t shift = dns_u16(m, at);
        if ((shift == OP_ASL_W_1_D0 || shift == OP_ASL_L_1_D0) && dns_u16(m, at + 2) == OP_LEA_ABS_A0) {
            break;
        }
    }
    out->song_lea = at + 2;
    module->songs = operand_offset(module, at + 4);
    if (dns_u16(m, at + 8) != OP_ANDI_L_D0) {
        return false;
    }
    out->song_offset_mask = (uint16_t)dns_u32(m, at + 10);
    // The sample records: `andi.l #$fffe,d0; lea instruments,a0`.
    static const uint32_t lea_instruments[] = { OP_ANDI_L_FFFE_D0, OP_LEA_ABS_A0 };
    at = find_words(m, out->song_lea, out->code_end, lea_instruments, nuade_sizeof_array(lea_instruments));
    if (at == NOT_FOUND) {
        return false;
    }
    module->instruments = operand_offset(module, at + 4);
    return module->songs != NOT_FOUND && module->instruments != NOT_FOUND;
}

// The sequencer's kind and its tables: no pattern pointer table means fixed patterns; the rows version addresses a
// table of 24 byte rows with `mulu.w #24,d0`. The first two kinds have the six tracks' tables.

static bool find_sequencer(DnsModule* module, uint32_t first_variable, uint32_t code_end, DnsSequencer* out) {
    static const uint32_t row_index[] = { OP_MULU_W_D0, ROW_SIZE };
    if (!find_pattern_table(module, first_variable)) {
        *out = DnsSequencer_FixedPatterns;
        return find_fixed_patterns(module, code_end) && find_track_tables(module, code_end);
    }
    if (find_words(&module->memory, 0, code_end, row_index, nuade_sizeof_array(row_index)) != NOT_FOUND) {
        *out = DnsSequencer_Rows;
        return find_rows(module, code_end);
    }
    *out = DnsSequencer_PatternTable;
    return find_track_tables(module, code_end);
}

bool dns_module_locate(DnsModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (size < MIN_MODULE_SIZE || size > MAX_MODULE_SIZE) {
        return false;
    }
    module->memory = (DnsMemory) { .data = data, .size = (uint32_t)size };
    const DnsMemory* m = &module->memory;
    CodeAddresses addresses;
    if (!check_entry_points(m) || !find_addresses(module, &addresses)) {
        return false;
    }
    const uint32_t code_end = addresses.code_end;
    // The tick routine starts with the voices' update (a `bsr.w`) or with the play flag test.
    const uint32_t tick = 2 + (uint32_t)(int32_t)(int16_t)dns_u16(m, 2);
    const uint16_t tick_op = dns_u16(m, tick);
    if (tick_op != OP_BSR_W && tick_op != OP_CMPI_W_ABS) {
        return false;
    }
    DnsSequencer sequencer;
    if (!find_sequencer(module, addresses.first_variable, code_end, &sequencer)) {
        return false;
    }
    static const uint32_t age_guard[] = { OP_CMPI_W_IDX_A1_D7, 0x7FFF, EXT_D7_W };
    const bool has_age_guard = find_words(m, 0, code_end, age_guard, nuade_sizeof_array(age_guard)) != NOT_FOUND;
    module->version = base_version(sequencer, has_age_guard);
    module->version.voices_first = tick_op == OP_BSR_W;
    module->version.song_offset_mask = addresses.song_offset_mask;
    if (!find_volume_decode(&module->version, m, code_end) || !find_dma_note_state(&module->version, m, code_end)) {
        return false;
    }
    module->subsongs = count_subsongs(module);
    return true;
}

bool dns_has_module_prefix(const char* name) {
    return name != nullptr && nuade_strncasecmp(name, DNS_MODULE_PREFIX, sizeof(DNS_MODULE_PREFIX) - 1) == 0;
}

bool dns_probe(const uint8_t* data, size_t size, const char* filename) {
    if (filename == nullptr || !dns_has_module_prefix(nuade_path_basename(filename))) {
        return false;
    }
    DnsModule module;
    return dns_module_locate(&module, data, size);
}

DnsStatus dns_module_load(DnsModule* module, const uint8_t* data, size_t size) {
    if (!dns_module_locate(module, data, size)) {
        return DnsStatus_InvalidData;
    }
    module->owned = malloc(size);
    if (module->owned == nullptr) {
        return DnsStatus_OutOfMemory;
    }
    memcpy(module->owned, data, size);
    module->memory.data = module->owned;
    return DnsStatus_Ok;
}

void dns_module_free(DnsModule* module) {
    free(module->owned);
    module->owned = nullptr;
}

uint16_t dns_read_period(const DnsModule* module, unsigned index) {
    return dns_u16(&module->memory, module->periods + 2 * (index % PERIOD_COUNT));
}

// The song table has a field per word of a song, 16 songs each.

DnsSong dns_read_song(const DnsModule* module, unsigned song) {
    const DnsMemory* m = &module->memory;
    const DnsVersion* version = &module->version;
    const uint32_t entry = module->songs + ((song * 2) & version->song_offset_mask);
    DnsSong result = { .position = dns_u16(m, entry),
                       .loop_position = dns_u16(m, entry + 1 * SONG_FIELD_SIZE),
                       .end_position = dns_u16(m, entry + 2 * SONG_FIELD_SIZE),
                       .speed = dns_u16(m, entry + 3 * SONG_FIELD_SIZE) };
    const uint16_t mute_mask = dns_u16(m, entry + 4 * SONG_FIELD_SIZE);
    for (int track = 0; track < DNS_TRACKS; track++) {
        result.muted[track] = ((mute_mask >> track) & 1) != 0;
    }
    if (version->sequencer == DnsSequencer_FixedPatterns) {
        result.step_limit = dns_u16(m, entry + 7 * SONG_FIELD_SIZE);
    } else if (version->sequencer == DnsSequencer_PatternTable) {
        result.step_limit = (uint16_t)(dns_u16(m, entry + 6 * SONG_FIELD_SIZE) * 2);
    }
    return result;
}

DnsInstrument dns_read_instrument(const DnsModule* module, unsigned number) {
    const DnsMemory* m = &module->memory;
    const uint32_t at = module->instruments + (number & 0xFF) * INSTRUMENT_SIZE;
    return (DnsInstrument) { .start = dns_u32(m, at),
                             .start_words = dns_u16(m, at + 0x06),
                             .loop = dns_u32(m, at + 0x08),
                             .loop_words = dns_u16(m, at + 0x0E),
                             .interval = dns_u16(m, at + 0x18),
                             .attack_step = (int16_t)dns_u16(m, at + 0x1A),
                             .attack_from = (int16_t)dns_u16(m, at + 0x1C),
                             .attack_to = (int16_t)dns_u16(m, at + 0x1E),
                             .decay_interval = dns_u16(m, at + 0x20),
                             .decay_step = (int16_t)dns_u16(m, at + 0x22),
                             .release_jumps = dns_u16(m, at + 0x24),
                             .sustain = (int16_t)dns_u16(m, at + 0x26),
                             .release_interval = dns_u16(m, at + 0x28),
                             .release_step = (int16_t)dns_u16(m, at + 0x2A),
                             .detune = dns_u16(m, at + 0x4A) };
}

DnsTrackRow dns_read_track_row(const DnsModule* module, unsigned track, uint16_t position) {
    const DnsMemory* m = &module->memory;
    if (module->version.sequencer == DnsSequencer_Rows) {
        const uint32_t at = module->rows + (uint32_t)position * ROW_SIZE + track * 4;
        const DnsTrackRow row
            = { .pattern = dns_u8(m, at), .note_add = dns_u8(m, at + 1), .sample_add = dns_u8(m, at + 2) };
        return row;
    }
    const DnsTrackTables* t = &module->tracks[track];
    const int64_t index = (int16_t)position;
    return (DnsTrackRow) { .pattern = dns_u8(m, t->patterns + index),
                           .note_add = dns_u8(m, t->note_adds + index),
                           .sample_add = dns_u8(m, t->sample_adds + index) };
}

uint32_t dns_pattern_offset(const DnsModule* module, uint8_t pattern) {
    if (pattern == 0) {
        return 0;
    }
    if (module->version.sequencer == DnsSequencer_FixedPatterns) {
        return module->first_pattern + (uint32_t)(pattern - 1) * PATTERN_SIZE;
    }
    if (pattern > module->pattern_count) {
        return 0; // the replayer would read past the table; no song does
    }
    const uint32_t entry = module->pattern_table + (uint32_t)(pattern - 1) * 4;
    const uint32_t address = dns_u32(&module->memory, entry);
    return address >= module->link_base ? address - module->link_base : 0;
}

uint16_t dns_read_pattern_word(const DnsModule* module, uint32_t pattern, int16_t step) {
    return dns_u16(&module->memory, (int64_t)pattern + step);
}
