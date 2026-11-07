// SPDX-License-Identifier: MIT
// The eagleplayer's checks and the builds of the replayer (see scr_module.h).

#include "sc_module.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "sc_code.h"
#include <stdlib.h>
#include <string.h>

// The module starts with a period table: 146 words, or 226 in the builds that follow it with the volume table. The
// eagleplayer tests its first words and the 128 words behind the table.
#define SHORT_TABLE_SIZE 0x124
#define LONG_TABLE_SIZE 0x1C4
#define TABLE_SCAN_WORDS 128
#define VOLUME_TABLE_WORDS 16
#define VOLUME_TABLE_SEARCH_FROM 0x100
#define VOLUME_TABLE_SEARCH_TO 0x200
// The volume table of the replayers; one of them starts its code right behind it.
static const uint16_t s_volume_table[VOLUME_TABLE_WORDS] = { 0, 1, 3, 4, 5, 6, 8, 10, 13, 16, 20, 26, 32, 41, 51, 64 };

// The bytes behind the code: the fade counter (a word), a byte that is zero and takes the high byte of an address the
// replayer forms from three bytes, and the three bytes the replayer copies a command's parameters into.
#define BUILD_FIELDS_END_BYTES 6
#define SCRATCH_ZERO_BYTE 2

#define OP_LEA_PC_A2 0x45FA
#define OP_LEA_ABSOLUTE_A1 0x43F9
#define OP_LEA_ABSOLUTE_A0 0x41F9
#define OP_MOVE_BYTE_POSTINC_D0 0x1018 // move.b (a0)+,d0
#define OP_BMI_BYTE 0x6B               // the high byte of bmi.b
#define OP_JMP_ABSOLUTE 0x4EF9
#define OP_RTE 0x4E73
#define OP_RTS 0x4E75
#define FIRST_PATCHED_RTE 0x1C2
// The stub InitPlayer writes over the copyright text: jmp <the eagleplayer's hook>. The hook lies at this address in
// UADE's memory.
#define EAGLEPLAYER_STUB_BYTES 6
#define EAGLEPLAYER_HOOK_ADDRESS 0x0000D68Eu

// clang-format off
static const ScrBuild s_builds[] = {
    // Builds with a word pitch index and a volume that is not masked: a length reads commands at -1, a note ends at 0.
    // Top Cat
    { .code_hash = 0xDC4F79136AC1FF29ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xB3E, .track_tables = { 0xB58, 0xB60, 0xB68, 0xB70 }, .track_entries_doubled = true,
      .commands = { "NTVSPQRCAAXXEKO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Mega Twins
    { .code_hash = 0xD95A72623E7712F4ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xC86, .track_tables = { 0xCC4, 0xCD0, 0xCDC, 0xCE8 },
      .audio_interrupts = true,
      .handler_loops = { { SCR_OUTSIDE_MODULE, 128 }, { SCR_OUTSIDE_MODULE, 128 }, { 0x2702, 128 },
                         { SCR_OUTSIDE_MODULE, 128 } },
      .commands = { "NTVSPQRCLLLXEKO", "NTVSPQRCLLLXEJO", "NTVSPQRCMBXXEJO", "NTVSPQRCBBLXEJO" } },
    // Wacky Races
    { .code_hash = 0xAD55EC1BA24162C2ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xB08, .track_tables = { 0xB34, 0xB3C, 0xB46, 0xB4E }, .track_entries_doubled = true,
      .commands = { "NTVSPQRCAAEEEKO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAEEEJO", "NTVSPQRCAAEEEJO" } },
    // WWF Wrestlemania
    { .code_hash = 0x74D436726BE7A044ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xB36, .track_tables = { 0xB74, 0xB80, 0xB8C, 0xB98 }, .track_entries_doubled = true,
      .pitch_shifts_ascend = true, .volume_start_zero = true, .release_clears_volume = true,
      .commands = { "NTVSPQRCAAXXEKO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAEEEJO", "NTVSPQRCAAXXEJO" } },
    // Yogi Bear
    { .code_hash = 0xBFDE674FB128FC73ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xB40, .track_tables = { 0xB8A, 0xB9A, 0xBAA, 0xBBA }, .track_entries_doubled = true,
      .commands = { "NTVSPQRCAAXXEHO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Video Kid (in game)
    { .code_hash = 0xB58B408A405CEEC0ull, .code_start = 0x1E6, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1C6,
      .song_table = 0xB24, .track_tables = { 0xB56, 0xB60, 0xB6A, 0xB74 }, .track_entries_doubled = true,
      .pitch_shifts_ascend = true, .volume_start_zero = true, .release_clears_volume = true,
      .commands = { "NTVSPQRCAAXXEGO", "NTVSPQRCAAXXEFO", "NTVSPQRCAAXXEFO", "NTVSPQRCAAXXEFO" } },
    // Video Kid (title)
    { .code_hash = 0x53CC44527994D6BDull, .code_start = 0x1C4, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x1A4,
      .song_table = 0xB24, .track_tables = { 0xB9A, 0xBA8, 0xBB6, 0xBC6 }, .track_entries_doubled = true,
      .pitch_shifts_ascend = true, .volume_start_zero = true, .release_clears_volume = true,
      .commands = { "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Builds with a 4 bit volume and a byte offset pitch index. All but Hong Kong Phooey read commands when a length
    // reaches 0 and end a note at 1; the last five test every track list entry for a marker, also the first.
    // Ruff and Reddy
    { .code_hash = 0x2CC3DC469EB85F01ull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .song_table = 0xA7A, .track_tables = { 0xAA2, 0xAAA, 0xAB4, 0xABC }, .track_entries_doubled = true,
      .length_ends_at_zero = true, .volume_nibble = true, .pitch_index_bytes = true,
      .commands = { "NTVSPQRCAAXXEHO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Hong Kong Phooey
    { .code_hash = 0x492705F507649434ull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .song_table = 0xA62, .track_tables = { 0xA86, 0xA90, 0xA9A, 0xAA4 }, .track_entries_doubled = true,
      .volume_nibble = true, .pitch_index_bytes = true,
      .commands = { "NTVSPQRCAAXXEHO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Count Duckula
    { .code_hash = 0x5690E470CA747D75ull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .track_lists = { 0xA38, 0xA3B, 0xA3E, 0xA4C }, .track_tables = { 0xA58, 0xA5A, 0xA5C, 0xA66 },
      .track_entries_doubled = true, .length_ends_at_zero = true, .volume_nibble = true, .pitch_index_bytes = true,
      .marker_rechecked = true,
      .commands = { "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Count Duckula (1)
    { .code_hash = 0x11DBCFE4846CEB4Dull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .song_table = 0x8FE, .track_tables = { 0x914, 0x918, 0x91C, 0x920 }, .track_entries_doubled = true,
      .length_ends_at_zero = true, .volume_nibble = true, .pitch_index_bytes = true, .marker_rechecked = true,
      .commands = { "NTVSPQAAAAEEEDO", "NTVSPQAAAAEEEZO", "NTVSPQAAAAEEEZO", "NTVSPQAAAAEEEZO" } },
    // Count Duckula (2)
    { .code_hash = 0x3B68B0425D29FD87ull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .song_table = 0x95E, .track_tables = { 0x980, 0x986, 0x98C, 0x992 }, .track_entries_doubled = true,
      .length_ends_at_zero = true, .volume_nibble = true, .pitch_index_bytes = true, .marker_rechecked = true,
      .commands = { "NTVSPQAAAAXXEDO", "NTVSPQAAAAXXEZO", "NTVSPQAAAAXXEZO", "NTVSPQAAAAXXEZO" } },
    // It carries audio interrupt handlers, but the eagleplayer does not install them: it finds a `lea` before the
    // instruction that sets the vector. Its master volume starts low and its pitch bias at 0x80.
    // Infestation
    { .code_hash = 0xADFBD4E7E4670756ull, .code_start = 0x124, .start_master_volume = 2, .start_pitch_bias = 0x80,
      .volume_table = 0x104, .track_lists = { 0x118C, 0x1209, 0x126C, 0x12CF },
      .track_tables = { 0x12FC, 0x1308, 0x1312, 0x131A }, .track_entries_doubled = true, .length_ends_at_zero = true,
      .volume_nibble = true, .pitch_index_bytes = true, .marker_rechecked = true,
      .commands = { "N-VSPQRCMAXX-JO", "NTVSPQRCAAEEEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
    // Defenders of the Earth
    { .code_hash = 0x1205E7C3D1E13530ull, .code_start = 0x124, .start_master_volume = 15, .start_pitch_bias = 0x7F, .volume_table = 0x104,
      .song_table = 0xA60, .track_tables = { 0xAAE, 0xABE, 0xACE, 0xADE }, .track_entries_doubled = true,
      .length_ends_at_zero = true, .volume_nibble = true, .pitch_index_bytes = true, .marker_rechecked = true,
      .commands = { "NTVSPQRCAAXXEHO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO", "NTVSPQRCAAXXEJO" } },
};
// clang-format on

static bool read_long_at(const uint8_t* data, size_t size, size_t at, uint32_t* value) {
    if (at > size || size - at < 4) {
        return false;
    }
    *value = nuade_read_be32(data + at);
    return true;
}

// The first test of the eagleplayer's Check2: the table's first words, and where the 128 words behind it start.
static bool find_table_end(const uint8_t* data, size_t size, size_t* table_end) {
    uint32_t first = 0;
    uint32_t second = 0;
    uint32_t third = 0;
    if (!read_long_at(data, size, 0, &first) || !read_long_at(data, size, 4, &second)
        || !read_long_at(data, size, 8, &third)) {
        return false;
    }
    if (first == 0x0FFF0FE2 || first == 0x10000FE2) {
        if (second != 0x0FC40FA7 || third != 0x0F8B0F6E) {
            return false;
        }
        *table_end = SHORT_TABLE_SIZE;
        return true;
    }
    if (first != 0x0F1C0F0E || second != 0x0F000EF2 || third != 0x0EE40ED6) {
        return false;
    }
    *table_end = LONG_TABLE_SIZE;
    return true;
}

// The rest of Check2: none of the 128 words behind the table is 0xFFFF, and none of them starts a long 0x7F7F7F7F.
static bool check_table_end(const uint8_t* data, size_t size, size_t table_end) {
    for (size_t i = 0; i < TABLE_SCAN_WORDS; i++) {
        const size_t at = table_end + 2 * i;
        if (at > size || size - at < 4) {
            return false;
        }
        if (nuade_read_be32(data + at) == 0x7F7F7F7F || nuade_read_be16(data + at) == 0xFFFF) {
            return false;
        }
    }
    return true;
}

// Where the code starts: behind the volume table when the period table is the long one and the volume table follows
// it, else right behind the short period table. 0 when there is no such place.
static uint32_t code_start(const uint8_t* data, size_t size, size_t table_end) {
    if (table_end == SHORT_TABLE_SIZE) {
        return (uint32_t)table_end;
    }
    for (size_t at = VOLUME_TABLE_SEARCH_FROM; at + sizeof(s_volume_table) <= size && at < VOLUME_TABLE_SEARCH_TO;
         at += 2) {
        bool same = true;
        for (size_t i = 0; i < VOLUME_TABLE_WORDS && same; i++) {
            same = nuade_read_be16(data + at + 2 * i) == s_volume_table[i];
        }
        if (same) {
            return (uint32_t)(at + sizeof(s_volume_table));
        }
    }
    return 0;
}

// The offset of the replayer's init, which InitPlayer finds as the first `lea` behind the table (or the first long of
// zeros: no replayer).
static bool find_init(const uint8_t* data, size_t size, size_t table_end, uint32_t* init) {
    uint32_t at = (uint32_t)table_end;
    while (at <= size && size - at >= 4) {
        const uint16_t op = nuade_read_be16(data + at);
        if (op == OP_LEA_PC_A2 || op == OP_LEA_ABSOLUTE_A1 || op == OP_LEA_ABSOLUTE_A0) {
            *init = at;
            return true;
        }
        if (nuade_read_be32(data + at) == 0) {
            return false;
        }
        at += 2;
    }
    return false;
}

// InitPlayer hooks the first voice's track list: the first `move.b (a0)+,d0; bmi.b` of the replayer is replaced by a
// call to a stub, which it writes over the copyright text ("Sean Conran") behind the replayer as a jump to its own
// code. That text can lie inside sample data: the offset of the stub, 0 when InitPlayer finds no text and refuses the
// module.
static uint32_t find_text_patch(const uint8_t* data, size_t size, uint32_t init) {
    uint32_t at = init;
    for (;; at += 2) {
        if (at > size || size - at < 4) {
            return 0;
        }
        if (nuade_read_be16(data + at) == OP_MOVE_BYTE_POSTINC_D0 && data[at + 2] == OP_BMI_BYTE) {
            break;
        }
    }
    const uint32_t after_branch = at + 4;
    const uint32_t handler = after_branch + (uint32_t)(int32_t)(int8_t)data[at + 3];
    const uint64_t limit = handler + 32767ull < size ? handler + 32767ull : size;
    for (uint64_t text = handler; text < limit; text++) {
        if (text + 3 < size && memcmp(data + text, "Sean", 4) == 0) {
            const uint32_t patch = (uint32_t)(text - 1) & ~1u;
            return size - patch >= EAGLEPLAYER_STUB_BYTES ? patch : 0;
        }
    }
    return 0;
}

static void write_eagleplayer_stub(uint8_t* at) {
    at[0] = OP_JMP_ABSOLUTE >> 8;
    at[1] = OP_JMP_ABSOLUTE & 0xFF;
    at[2] = EAGLEPLAYER_HOOK_ADDRESS >> 24;
    at[3] = (EAGLEPLAYER_HOOK_ADDRESS >> 16) & 0xFF;
    at[4] = (EAGLEPLAYER_HOOK_ADDRESS >> 8) & 0xFF;
    at[5] = EAGLEPLAYER_HOOK_ADDRESS & 0xFF;
}

// InitPlayer turns every `rte` into `rts` from 0x1C2 on (the audio interrupt handlers become servers). The part that
// lies in front of the code is data of the module: the period table's last words.
static void patch_return_from_interrupt(uint8_t* data, uint32_t code_start_offset) {
    for (uint32_t at = FIRST_PATCHED_RTE; at + 2 <= code_start_offset; at += 2) {
        if (nuade_read_be16(data + at) == OP_RTE) {
            data[at + 1] = OP_RTS & 0xFF;
        }
    }
}

static const ScrBuild* find_build(uint64_t hash) {
    for (size_t i = 0; i < nuade_sizeof_array(s_builds); i++) {
        if (s_builds[i].code_hash == hash) {
            return &s_builds[i];
        }
    }
    return nullptr;
}

ScrModule* scr_module_open(const uint8_t* data, size_t size) {
    size_t table_end = 0;
    if (!find_table_end(data, size, &table_end) || !check_table_end(data, size, table_end)) {
        return nullptr;
    }
    const uint32_t start = code_start(data, size, table_end);
    ScrCodeScan scan;
    if (start == 0 || !scr_scan_code(data, size, start, &scan)) {
        return nullptr;
    }
    const ScrBuild* build = find_build(scan.hash);
    uint32_t init = 0;
    if (build == nullptr || start != build->code_start || scan.end > size || size - scan.end < BUILD_FIELDS_END_BYTES
        || data[scan.end + SCRATCH_ZERO_BYTE] != 0 || !find_init(data, size, table_end, &init)) {
        return nullptr;
    }
    // The stub is written into the module: over the replayer's own bytes it would be executed
    const uint32_t stub = find_text_patch(data, size, init);
    const uint32_t written_to = scan.end + BUILD_FIELDS_END_BYTES;
    if (stub == 0 || (stub < written_to && stub + EAGLEPLAYER_STUB_BYTES > start)) {
        return nullptr;
    }
    ScrModule* module = (ScrModule*)calloc(1, sizeof(ScrModule));
    if (module == nullptr) {
        return nullptr;
    }
    module->data = nuade_paula_padded_copy(data, size);
    if (module->data == nullptr) {
        free(module);
        return nullptr;
    }
    write_eagleplayer_stub(module->data + stub);
    if (build->audio_interrupts) {
        patch_return_from_interrupt(module->data, start);
    }
    module->size = size;
    module->build = build;
    module->written_from = start;
    module->written_to = written_to;
    module->subsong_count = 1;
    if (build->song_table != 0) {
        const uint32_t first = build->song_table <= size - 2 ? nuade_read_be16(data + build->song_table) : 0;
        module->subsong_count = first > build->song_table ? (first - build->song_table) >> 3 : 0;
    }
    if (module->subsong_count == 0) {
        scr_module_close(module);
        return nullptr;
    }
    return module;
}

void scr_module_close(ScrModule* module) {
    if (module != nullptr) {
        free(module->data);
    }
    free(module);
}

uint8_t scr_module_u8(const ScrModule* module, uint32_t offset) {
    return offset < module->size ? module->data[offset] : 0;
}

uint16_t scr_module_u16(const ScrModule* module, uint32_t offset) {
    return (uint16_t)((scr_module_u8(module, offset) << 8) | scr_module_u8(module, offset + 1));
}
