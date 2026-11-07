// SPDX-License-Identifier: MIT
// Jeroen Tel player: the module check, the scan for the module's tables (the eagleplayer's Check2 and InitPlayer) and
// the readers of the module's records.

#include "jt_internal.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define MIN_MODULE_SIZE 0x6A5 // Check2 needs more than 0x6A4 bytes
#define CHECK_WINDOW 0x28     // the code's first long must start within this many bytes
#define SONG_ENTRY_SIZE 0x12
#define SONG_VOICE_SLOT 0x0C // byte 0x10 of every song of the table
#define PERIOD_TABLE_SIZE (JT_PERIOD_COUNT * 2)
#define INSTRUCTION_ANDI_B_1_ABS 0x02390001u // andi.b #1,abs.l: the first instruction of the code
#define INSTRUCTION_BNE_S 0x66
#define INSTRUCTION_RTS 0x4E75
#define INSTRUCTION_TST_B_ABS 0x4A39
#define INSTRUCTION_TST_B_ABS_SIZE 0x12 // tst.b abs.l, beq.w, move.w #bit,DMACON
#define INSTRUCTION_MOVEQ_0_D4_MOVE_B_ABS 0x78001839u
#define INSTRUCTION_MOVE_L_D0_D2_ASL_B 0x1400E302u // move.b d0,d2; asl.b #1,d2: the note's period lookup
#define PERIOD_TABLE_START 0x03580328u             // the periods 856, 808
#define INSTRUCTION_CMP_B_FF_D1 0xB23C00FFu
#define INSTRUCTION_CMPI_B_FF_D1 0x0C0100FFu
#define INSTRUCTION_MOVE_B_IMM_ABS 0x13FC
#define INSTRUCTION_MOVEA_L_IMM_A3 0x267C
#define INSTRUCTION_LEA_ABS_A4 0x49F9
#define INSTRUCTION_ARPEGGIO_TABLE 0x0026267Cu // the end of one instruction and movea.l #table,a3
#define INSTRUCTION_MOVE_L_A4_D_ABS 0x23F4
#define RESTART_INIT_STRIDE 8     // the four move.b #pos,abs.l that restart the voices are 8 bytes each
#define RESTART_PROBE_OFFSET 6    // the opcode of the first move.b is read at this offset behind the compare
#define RESTART_PROBE_SPAN 10     // bytes the check of that opcode reads from the compare on
#define RESTART_FIRST_IMMEDIATE 9 // the first one starts 6 bytes behind the compare; its immediate byte is at +9
#define RESTART_SEARCH_SPAN (RESTART_FIRST_IMMEDIATE + RESTART_INIT_STRIDE * (JT_VOICES - 1) + 1)

uint8_t jt_u8(const JtModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

uint16_t jt_u16(const JtModule* m, uint32_t offset) {
    return (uint16_t)(jt_u8(m, offset) << 8 | jt_u8(m, offset + 1));
}

uint32_t jt_u32(const JtModule* m, uint32_t offset) {
    return (uint32_t)jt_u16(m, offset) << 16 | jt_u16(m, offset + 2);
}

uint32_t jt_offset_of(const JtModule* m, uint32_t address) {
    const uint32_t offset = address - m->load_address;
    return offset < m->size ? offset : JT_NO_OFFSET;
}

uint32_t jt_table_entry(const JtModule* m, uint32_t table, uint32_t byte_offset) {
    return jt_offset_of(m, jt_u32(m, table + byte_offset));
}

const int8_t* jt_sample_at(const JtModule* m, uint32_t address) {
    const uint32_t offset = jt_offset_of(m, address);
    return offset != JT_NO_OFFSET ? (const int8_t*)(m->data + (offset & ~1u)) : nullptr;
}

uint16_t jt_period(const JtModule* m, unsigned index) {
    return index < JT_PERIOD_COUNT ? jt_u16(m, m->period_table + 2 * index) : 0;
}

// A song table entry as stored: the absolute addresses of the four track lists, the number 12 and the speed.

typedef struct SongEntry {
    uint32_t tracks[JT_VOICES];
    uint8_t voice_slot;
    uint8_t speed;
} SongEntry;

static SongEntry read_song_entry(const JtModule* m, uint32_t at) {
    SongEntry entry = { .voice_slot = jt_u8(m, at + 0x10), .speed = jt_u8(m, at + 0x11) };
    for (int i = 0; i < JT_VOICES; i++) {
        entry.tracks[i] = jt_u32(m, at + 4 * (uint32_t)i);
    }
    return entry;
}

bool jt_read_song(const JtModule* m, uint32_t number, JtSong* out) {
    if (number >= m->song_count) {
        return false;
    }
    const SongEntry entry = read_song_entry(m, m->song_table + number * SONG_ENTRY_SIZE);
    for (int i = 0; i < JT_VOICES; i++) {
        out->tracks[i] = jt_offset_of(m, entry.tracks[i]);
    }
    out->speed = entry.speed;
    return true;
}

JtInstrument jt_read_instrument(const JtModule* m, uint32_t offset) {
    if (offset == JT_NO_OFFSET) {
        return (JtInstrument) { 0 };
    }
    return (JtInstrument) { .max_volume = jt_u16(m, offset + 0x00),
                            .length_words = jt_u16(m, offset + 0x02),
                            .sample = jt_u32(m, offset + 0x04),
                            .loop = jt_u32(m, offset + 0x08),
                            .loop_length_words = jt_u16(m, offset + 0x0C),
                            .attack_rate = jt_u16(m, offset + 0x0E),
                            .decay_rate = jt_u16(m, offset + 0x10),
                            .sustain_volume = jt_u16(m, offset + 0x12),
                            .release_rate = jt_u16(m, offset + 0x14),
                            .vibrato_delay = jt_u16(m, offset + 0x16),
                            .vibrato_length = jt_u16(m, offset + 0x18),
                            .vibrato_step = jt_u16(m, offset + 0x1A),
                            .arpeggio_list = jt_u8(m, offset + 0x1C),
                            .release_tick = jt_u16(m, offset + 0x1E),
                            .sweep = jt_u16(m, offset + 0x20) };
}

// The first bytes of the eagleplayer: moveq #-1,d0 / rts, "DELIRIUM", a pointer, "$VER: Jeroen Tel player ...". The
// instrument pointer of a voice that has not been given one points at them.

JtInstrument jt_default_instrument(void) {
    return (JtInstrument) { .max_volume = 0x70FF,
                            .length_words = 0x4E75,
                            .sample = 0x44454C49,
                            .loop = 0x5249554D,
                            .loop_length_words = 0,
                            .attack_rate = 0x0042,
                            .decay_rate = 0x2456,
                            .sustain_volume = 0x4552,
                            .release_rate = 0x3A20,
                            .vibrato_delay = 0x4A65,
                            .vibrato_length = 0x726F,
                            .vibrato_step = 0x656E,
                            .arpeggio_list = 0x20,
                            .release_tick = 0x656C,
                            .sweep = 0x2070 };
}

// The scans of InitPlayer walk the module a word at a time and read on into memory when the pattern is not there. A
// scan that does not find its pattern inside the module fails the open.

static bool scan_word(const uint8_t* data, uint32_t size, uint32_t* pos, uint16_t value) {
    for (; size >= 2 && *pos <= size - 2; *pos += 2) {
        if (nuade_read_be16(data + *pos) == value) {
            return true;
        }
    }
    return false;
}

static bool scan_long_either(const uint8_t* data, uint32_t size, uint32_t* pos, uint32_t a, uint32_t b) {
    for (; size >= 4 && *pos <= size - 4; *pos += 2) {
        const uint32_t value = nuade_read_be32(data + *pos);
        if (value == a || value == b) {
            return true;
        }
    }
    return false;
}

static bool scan_long(const uint8_t* data, uint32_t size, uint32_t* pos, uint32_t value) {
    return scan_long_either(data, size, pos, value, value);
}

// Check2: the code starts within CHECK_WINDOW bytes with `andi.b #1,abs.l; bne.s x; rts`, and what the branch leads to
// is either `moveq #0,d4; move.b abs.l,d4` or four `tst.b abs.l` blocks (voice enable flags) before it.

static bool check_code(const uint8_t* data, uint32_t size) {
    if (size < MIN_MODULE_SIZE) {
        return false;
    }
    uint32_t at = 0;
    while (at < CHECK_WINDOW && nuade_read_be32(data + at) != INSTRUCTION_ANDI_B_1_ABS) {
        at += 2;
    }
    if (at >= CHECK_WINDOW) {
        return false;
    }
    const uint32_t branch = at + 8;
    const int8_t displacement = (int8_t)data[branch + 1];
    if (data[branch] != INSTRUCTION_BNE_S || displacement <= 0
        || nuade_read_be16(data + branch + 2) != INSTRUCTION_RTS) {
        return false;
    }
    uint32_t target = branch + 2 + (uint32_t)displacement;
    if (nuade_read_be16(data + target) == INSTRUCTION_TST_B_ABS) {
        for (int block = 0; block < JT_VOICES; block++) {
            if (target > size - INSTRUCTION_TST_B_ABS_SIZE || nuade_read_be16(data + target) != INSTRUCTION_TST_B_ABS) {
                return false;
            }
            target += INSTRUCTION_TST_B_ABS_SIZE;
        }
    }
    return target <= size - 4 && nuade_read_be32(data + target) == INSTRUCTION_MOVEQ_0_D4_MOVE_B_ABS;
}

// The songs of the table: entries of SONG_ENTRY_SIZE bytes that have the voice slot 12, up to the first one whose four
// track lists are the same.

static uint32_t count_songs(const JtModule* m) {
    uint32_t count = 0;
    for (uint32_t at = m->song_table; m->size - at >= SONG_ENTRY_SIZE; at += SONG_ENTRY_SIZE) {
        const SongEntry entry = read_song_entry(m, at);
        if (entry.voice_slot != SONG_VOICE_SLOT
            || (entry.tracks[0] == entry.tracks[1] && entry.tracks[0] == entry.tracks[2]
                && entry.tracks[0] == entry.tracks[3])) {
            break;
        }
        count++;
    }
    return count;
}

// The offset of a table from the absolute address that the code loads it from; false when it is not in the module.

static bool table_offset(const JtModule* m, uint32_t address, uint32_t* out) {
    *out = jt_offset_of(m, address);
    return *out != JT_NO_OFFSET;
}

// The voices restart at the track positions of the code's four `move.b #pos,abs.l`, if it has them behind the compare.

static bool read_restart_positions(JtModule* m, uint32_t compare) {
    memset(m->first_track_pos, 0, sizeof(m->first_track_pos));
    if (m->size - compare < RESTART_PROBE_SPAN) {
        return false;
    }
    if (nuade_read_be16(m->data + compare + RESTART_PROBE_OFFSET) != INSTRUCTION_MOVE_B_IMM_ABS) {
        return true;
    }
    if (m->size - compare < RESTART_SEARCH_SPAN) {
        return false;
    }
    for (int i = 0; i < JT_VOICES; i++) {
        m->first_track_pos[i] = m->data[compare + RESTART_FIRST_IMMEDIATE + RESTART_INIT_STRIDE * i];
    }
    return true;
}

// InitPlayer: every table is found by the instruction that loads it, the later scans continuing behind the earlier
// matches.

static bool locate_tables(JtModule* m) {
    const uint8_t* data = m->data;
    const uint32_t size = m->size;
    uint32_t pos = 0;
    if (!scan_long(data, size, &pos, INSTRUCTION_MOVE_L_D0_D2_ASL_B) || size - pos < 10) {
        return false;
    }
    const uint32_t period_address = nuade_read_be32(data + pos + 6);
    if (!scan_long(data, size, &pos, PERIOD_TABLE_START) || size - pos < PERIOD_TABLE_SIZE) {
        return false;
    }
    m->period_table = pos;
    m->load_address = period_address - pos;

    pos = 0;
    if (!scan_long_either(data, size, &pos, INSTRUCTION_CMP_B_FF_D1, INSTRUCTION_CMPI_B_FF_D1)
        || !read_restart_positions(m, pos)) {
        return false;
    }
    uint32_t pattern_address = 0;
    uint32_t instrument_address = 0;
    uint32_t arpeggio_address = 0;
    uint32_t song_address = 0;
    if (!scan_word(data, size, &pos, INSTRUCTION_MOVEA_L_IMM_A3) || size - pos < 6) {
        return false;
    }
    pattern_address = nuade_read_be32(data + pos + 2);
    pos += 6;
    if (!scan_word(data, size, &pos, INSTRUCTION_LEA_ABS_A4) || size - pos < 6) {
        return false;
    }
    instrument_address = nuade_read_be32(data + pos + 2);
    pos += 6;
    if (!scan_long(data, size, &pos, INSTRUCTION_ARPEGGIO_TABLE) || size - pos < 8) {
        return false;
    }
    arpeggio_address = nuade_read_be32(data + pos + 4);
    pos += 8;
    if (!scan_word(data, size, &pos, INSTRUCTION_MOVE_L_A4_D_ABS) || pos < 4) {
        return false;
    }
    song_address = nuade_read_be32(data + pos - 4);
    if (!table_offset(m, pattern_address, &m->pattern_table)
        || !table_offset(m, instrument_address, &m->instrument_table)
        || !table_offset(m, arpeggio_address, &m->arpeggio_table) || !table_offset(m, song_address, &m->song_table)) {
        return false;
    }
    m->song_count = count_songs(m);
    return m->song_count > 0;
}

bool jt_probe(const uint8_t* data, size_t size) {
    if (size > INT32_MAX || !check_code(data, (uint32_t)size)) {
        return false;
    }
    JtModule m = { .data = (uint8_t*)(uintptr_t)data, .size = (uint32_t)size };
    return locate_tables(&m);
}

// InitPlayer clears the first byte of the branch target behind the first rts of the code (a branch that is not the
// usual `bne.s` over the rts).

static void patch_first_branch_target(JtModule* m) {
    uint32_t rts = 0;
    if (!scan_word(m->data, m->size, &rts, INSTRUCTION_RTS) || rts == 0) {
        return;
    }
    const int8_t displacement = (int8_t)m->data[rts - 1];
    if (displacement == 2) {
        return;
    }
    const int64_t target = (int64_t)rts + displacement;
    if (target >= 0 && target < (int64_t)m->size) {
        m->data[target] = 0;
    }
}

JtPlayer* jt_create(const uint8_t* data, size_t size) {
    if (!jt_probe(data, size)) {
        return nullptr;
    }
    JtPlayer* p = (JtPlayer*)calloc(1, sizeof(*p));
    if (p == nullptr) {
        return nullptr;
    }
    p->module.data = nuade_paula_padded_copy(data, size);
    p->module.size = (uint32_t)size;
    if (p->module.data == nullptr || !locate_tables(&p->module)) {
        jt_destroy(p);
        return nullptr;
    }
    patch_first_branch_target(&p->module);
    return p;
}

void jt_destroy(JtPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

int jt_subsong_count(const JtPlayer* player) {
    return (int)player->module.song_count;
}

int jt_ticks_per_step(const JtPlayer* player, int subsong) {
    JtSong song;
    if (subsong < 0 || !jt_read_song(&player->module, (uint32_t)subsong, &song)) {
        return 0;
    }
    return song.speed + 2;
}
