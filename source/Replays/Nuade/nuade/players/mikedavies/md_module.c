// SPDX-License-Identifier: MIT
// Mike Davies module: Check2 of the eagleplayer, its search for the tables, the replayer versions and the typed
// readers. The module is a memory image of the game's driver, linked at some address `image_base`: the eagleplayer
// reads the table addresses from the operands of the code, relocates every pointer of the data to the address the
// module is loaded at (MD_LOAD_ADDRESS in UADE) and calls the routines of the module. The port does the same to its
// copy of the module, so a read past a table sees the same words.

#include "md_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"

// The instructions the eagleplayer's searches look for (one name per opcode).
#define MD_OP_MOVEM_ALL 0x48E7FFFEu // movem.l d0-d7/a0-a6,-(a7)
#define MD_OP_BRA_W 0x6000u
#define MD_OP_BSR_W 0x6100u
#define MD_OP_CMPI_W_D0 0x0C40u // cmpi.w #imm,d0
#define MD_OP_MOVE_W_IMM_ABS 0x33FCu
#define MD_OP_MOVE_W_D0_ABS 0x33C0u // move.w d0,abs.l
#define MD_OP_LEA_ABS_A0 0x41F9u
#define MD_OP_CMPA_L_IMM_A0 0xB1FCu
#define MD_OP_MOVE_W_IMM_D0 0x303Cu
#define MD_OP_LEA_ABS_A5 0x4BF9u
#define MD_OP_MOVE_L_PREINC 0x2D58u // move.l (a0)+,d16(a6): the instrument load
// move.w #1,abs.l: the first word of the module's own variables
#define MD_OP_ENABLE_LONG ((MD_OP_MOVE_W_IMM_ABS << 16) | 1u)
// Check2: the init routine, "movem; bsr.b; movem; rts", ends with the compare at 0xF2 or, in the other form, six bytes
// earlier with the store of the song number
#define MD_CHECK_OFFSET 0xF2u
#define MD_CHECK_TAIL 0x7FFF4E75u // movem.l (a7)+,d0-d7/a0-a6; rts
#define MD_CHECK_MID 0x61064CDFu

#define MD_VOICE_RECORD_BYTES 0x36u
#define MD_VOICE_MASK_BYTE 1u     // the low byte of the voice record's DMA bit word
#define MD_VOICE_REGISTER_BYTE 3u // the low byte of its register offset word
#define MD_VOICE_REGISTER_STEP 0x10u
#define MD_SFX_BYTES MD_INSTRUMENT_BYTES
#define MD_MAX_SUBSONGS 0x800u                  // the init routine indexes the subsong table with a signed word
#define MD_PATCH_INSTRUMENTS_MARKER 0x4654443Cu // "FTD<"
#define MD_PATCH_INSTRUMENTS_AT 0x7D8u
#define MD_PATCH_INSTRUMENTS_FROM 0x764u // three longs move up by four bytes from here
#define MD_PATCH_MERCS_SIZE 0x77AEu
#define MD_PATCH_MERCS_TRACK 0x7318u
#define MD_PATCH_MERCS_TRACK_VALUE 0x00FE00FEu
#define MD_PATCH_MERCS_RECORD 0x864u
#define MD_PATCH_MERCS_RECORD_VALUE 0x1E8u
// the call command's code, seen from the end of the first cmpi.w #imm,d0 after the tick
#define MD_CALL_COMMAND_OFFSET 0x16u
#define MD_RELOC_HARDWARE 0x00DFu // the high word of a custom chip address
#define MD_MAX_CODE_BYTES 0x1000u
#define MD_MAX_INSTRUMENTS 0xFFFFu
#define MD_PERIOD_BACK_MAX 0x40u // the code before the period table, after the last place the eagleplayer patches
#define MD_FNV_OFFSET 0xCBF29CE484222325ull
#define MD_FNV_PRIME 0x100000001B3ull
#define MD_ROLE_BASE 0xFFFF0000u

// The versions, one per game. The hash covers the code from the init routine to code_end with the operands that
// the eagleplayer relocates replaced: a variable (below the init routine) by its offset in the module, a pointer into
// the tables by the number of its target in order of appearance (so a code that addresses the tables differently is
// another version).
static const MdLayout md_layouts[] = {
    // Mercs
    { .code_hash = 0xE4A495FC4664D171ull,
      .code_end = 0x5AE,
      .periods = 0x5AE,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1,
      .rest_note = true,
      .floor_clamps = true },
    // Alien Storm
    { .code_hash = 0xFD403FBB69F31B2Full,
      .code_end = 0x5AA,
      .periods = 0x5C0,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1,
      .byte_tracks = true,
      .rest_note = true,
      .floor_clamps = true },
    // Strider
    { .code_hash = 0xA0C7FF3CC3A53157ull,
      .code_end = 0x598,
      .periods = 0x598,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1 },
    // Strider II
    { .code_hash = 0x3461699A0CD3E3CFull,
      .code_end = 0x59C,
      .periods = 0x59C,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1 },
    // Dynasty Wars
    { .code_hash = 0xCB418E6E54696898ull,
      .code_end = 0x4AA,
      .periods = 0x4AA,
      .command_end = 0x94,
      .min_period = 0x70,
      .passes = 1 },
    // Die Hard II
    { .code_hash = 0xA9C8C1ACDA55D117ull,
      .code_end = 0x4AC,
      .periods = 0x4AC,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1 },
    // Indiana Jones and the Last Crusade (action)
    { .code_hash = 0x1C538E09D19A6EADull,
      .code_end = 0x4AE,
      .periods = 0x4AE,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 1 },
    // Italy 1990: three passes per tick
    { .code_hash = 0x9741314B3E5896F2ull,
      .code_end = 0x4C0,
      .periods = 0x4C0,
      .command_end = 0x94,
      .min_period = 0x7C,
      .passes = 3 },
    // World Cup USA 94: the commands moved up by 0x50, the periods are interpolated
    { .code_hash = 0xE0715A98CC8308C3ull,
      .code_end = 0x4BA,
      .periods = 0x4BA,
      .command_end = 0xE4,
      .min_period = 0x7C,
      .passes = 1,
      .interpolates = true,
      .loop_back_ends_song = true },
};

// What the eagleplayer's InitPlayer finds in the code: the offsets of the operands that name the tables, and the
// routine addresses the later searches start from.
typedef struct MdScan {
    uint32_t init;                    // the init routine: the first movem.l d0-d7/a0-a6,-(a7)
    uint32_t image_base;              // the image address of module offset 0
    uint32_t subsongs_operand;        // lea.l subsong_table,a0
    uint32_t subsongs_end_operand;    // cmpa.l #end,a0
    uint32_t sfx_operand;             // lea.l sfx_table,a0
    uint32_t sfx_end_operand;         // cmpa.l #end,a0
    uint32_t tick;                    // the tick routine: a movem.l whose second bsr is at +8
    uint32_t instruments_operand;     // lea.l instrument_table,a0
    uint32_t instruments_end_operand; // cmpa.l #end,a0
    uint32_t periods_operand;         // lea.l period_table,a5
} MdScan;

static bool read_word(const MdModule* module, uint32_t at, uint16_t* out) {
    if (module->size < 2 || at > module->size - 2) {
        return false;
    }
    *out = nuade_read_be16(module->data + at);
    return true;
}

static bool read_long(const MdModule* module, uint32_t at, uint32_t* out) {
    if (module->size < 4 || at > module->size - 4) {
        return false;
    }
    *out = nuade_read_be32(module->data + at);
    return true;
}

static void put_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static bool write_long(MdModule* module, uint32_t at, uint32_t value) {
    if (module->size < 4 || at > module->size - 4) {
        return false;
    }
    put_be32(module->data + at, value);
    return true;
}

// The first word-aligned position at or after `from` that holds `word`.

static bool find_word(const MdModule* module, uint32_t from, uint16_t word, uint32_t* out) {
    for (uint32_t at = from; at <= module->size && module->size - at >= 2; at += 2) {
        if (nuade_read_be16(module->data + at) == word) {
            *out = at;
            return true;
        }
    }
    return false;
}

static bool find_long(const MdModule* module, uint32_t from, uint32_t value, uint32_t* out) {
    for (uint32_t at = from; at <= module->size && module->size - at >= 4; at += 2) {
        if (nuade_read_be32(module->data + at) == value) {
            *out = at;
            return true;
        }
    }
    return false;
}

// Check2 of the eagleplayer: the init routine is "movem; bsr.b; movem; rts" around 0xF2 (the code of the other form
// starts six bytes earlier), and the first bra.w after it jumps to the instrument load.

static bool check2(const MdModule* module) {
    uint16_t word = 0;
    uint32_t at = MD_CHECK_OFFSET;
    if (!read_word(module, at, &word)) {
        return false;
    }
    if (word != MD_OP_CMPI_W_D0) {
        at -= 6;
        if (!read_word(module, at, &word) || word != MD_OP_MOVE_W_D0_ABS) {
            return false;
        }
    }
    const uint32_t expected[] = { MD_CHECK_TAIL, MD_CHECK_MID, MD_OP_MOVEM_ALL };
    for (uint32_t i = 0; i < nuade_sizeof_array(expected); i++) {
        uint32_t value = 0;
        at -= 4;
        if (!read_long(module, at, &value) || value != expected[i]) {
            return false;
        }
    }
    uint32_t bra = 0;
    uint16_t displacement = 0;
    if (!find_word(module, at, MD_OP_BRA_W, &bra) || !read_word(module, bra + 2, &displacement)) {
        return false;
    }
    if (displacement == 0 || (displacement & 0x8000u) != 0 || (displacement & 1u) != 0) {
        return false;
    }
    return read_word(module, bra + 2 + displacement, &word) && word == MD_OP_MOVE_L_PREINC;
}

// The searches of InitPlayer for the operands: each starts where the previous one stopped.

static bool locate(const MdModule* module, MdScan* out) {
    uint32_t at = 0;
    *out = (MdScan) { 0 };
    if (!find_long(module, 0, MD_OP_MOVEM_ALL, &out->init) || !find_long(module, out->init, MD_OP_ENABLE_LONG, &at)
        || !read_long(module, at + 4, &out->image_base)) {
        return false;
    }
    at += 4;
    if (!find_word(module, at, MD_OP_LEA_ABS_A0, &at)) {
        return false;
    }
    out->subsongs_operand = at + 2;
    if (!find_word(module, out->subsongs_operand, MD_OP_CMPA_L_IMM_A0, &at)) {
        return false;
    }
    out->subsongs_end_operand = at + 2;
    if (!find_word(module, out->subsongs_end_operand, MD_OP_LEA_ABS_A0, &at)) {
        return false;
    }
    out->sfx_operand = at + 2;
    if (!find_word(module, out->sfx_operand, MD_OP_CMPA_L_IMM_A0, &at)) {
        return false;
    }
    out->sfx_end_operand = at + 2;
    if (!find_word(module, out->sfx_end_operand, MD_OP_MOVE_W_IMM_D0, &at)) {
        return false;
    }
    // the tick routine: a movem.l with a bsr.w at +8
    bool found = false;
    uint32_t from = at + 2;
    while (!found && find_long(module, from, MD_OP_MOVEM_ALL, &out->tick)) {
        uint16_t word = 0;
        found = read_word(module, out->tick + 8, &word) && word == MD_OP_BSR_W;
        from = out->tick + 2;
    }
    if (!found) {
        return false;
    }
    at = out->tick;
    if (!find_word(module, at, MD_OP_LEA_ABS_A0, &at)) {
        return false;
    }
    out->instruments_operand = at + 2;
    if (!find_word(module, out->instruments_operand, MD_OP_CMPA_L_IMM_A0, &at)) {
        return false;
    }
    out->instruments_end_operand = at + 2;
    if (!find_word(module, out->instruments_end_operand, MD_OP_LEA_ABS_A5, &at)) {
        return false;
    }
    out->periods_operand = at + 2;
    return true;
}

// The operands the eagleplayer relocates between the init routine and the period table: it steps a word at a time,
// and an instruction that takes an absolute address (or an immediate pointer) is followed by an operand that it
// relocates, unless that is a custom chip address.

static bool find_operands(const MdModule* module, uint32_t init, uint32_t periods_at, MdOperands* out) {
    static const struct {
        uint16_t opcode;
        uint8_t skip; // bytes between the opcode and the operand
        bool two;     // a second absolute address follows (move.w abs.l,abs.l)
    } forms[] = {
        { 0x3039, 0, false },
        { MD_OP_MOVE_W_D0_ABS, 0, false },
        { 0x33C1, 0, false },
        { 0x33EE, 2, false },
        { 0x33F9, 0, true },
        { MD_OP_MOVE_W_IMM_ABS, 2, false },
        { 0x3C39, 0, false },
        { MD_OP_LEA_ABS_A0, 0, false },
        { 0x4279, 0, false },
        { 0x4A79, 0, false },
        { MD_OP_LEA_ABS_A5, 0, false },
        { 0x4DF9, 0, false },
        { 0x5379, 0, false },
        { MD_OP_CMPA_L_IMM_A0, 0, false },
    };
    out->count = 0;
    uint32_t at = init;
    while (at != periods_at) {
        uint16_t word = 0;
        if (at > periods_at || !read_word(module, at, &word)) {
            return false;
        }
        uint32_t form = 0;
        while (form < nuade_sizeof_array(forms) && forms[form].opcode != word) {
            form++;
        }
        at += 2;
        if (form == nuade_sizeof_array(forms)) {
            continue;
        }
        at += forms[form].skip;
        const uint32_t operand_count = forms[form].two ? 2u : 1u;
        for (uint32_t operand = 0; operand < operand_count; operand++) {
            uint16_t high = 0;
            if (!read_word(module, at, &high)) {
                return false;
            }
            const bool last = operand + 1 == operand_count;
            if (high == MD_RELOC_HARDWARE) {
                if (last) {
                    break; // the loop goes on at the operand and reads it as instructions
                }
                at += 4; // the first of two: stepped over
                continue;
            }
            if (out->count == MD_MAX_OPERANDS) {
                return false;
            }
            out->at[out->count++] = at;
            at += 4;
        }
    }
    return true;
}

// FNV-1a over the code of a version, with the relocated operands replaced.

static bool hash_code(const MdModule* module, const MdScan* scan, const MdOperands* operands, uint32_t code_end,
                      uint64_t* out) {
    uint8_t code[MD_MAX_CODE_BYTES];
    if (code_end <= scan->init || code_end > module->size || code_end - scan->init > sizeof(code)) {
        return false;
    }
    const uint32_t length = code_end - scan->init;
    memcpy(code, module->data + scan->init, length);
    uint32_t roles[MD_MAX_OPERANDS];
    uint32_t role_count = 0;
    for (uint32_t i = 0; i < operands->count; i++) {
        const uint32_t at = operands->at[i];
        if (at < scan->init || (uint64_t)at - scan->init + 4u > length) {
            continue;
        }
        const uint32_t target = nuade_read_be32(module->data + at) - scan->image_base;
        uint32_t replacement = target;
        if (target >= scan->init) {
            uint32_t role = 0;
            while (role < role_count && roles[role] != target) {
                role++;
            }
            if (role == role_count) {
                roles[role_count++] = target;
            }
            replacement = MD_ROLE_BASE + role;
        }
        put_be32(code + (at - scan->init), replacement);
    }
    uint64_t hash = MD_FNV_OFFSET;
    for (uint32_t i = 0; i < length; i++) {
        hash = (hash ^ code[i]) * MD_FNV_PRIME;
    }
    *out = hash;
    return true;
}

static const MdLayout* find_layout(const MdModule* module, const MdScan* scan, const MdOperands* operands) {
    for (uint32_t i = 0; i < nuade_sizeof_array(md_layouts); i++) {
        uint64_t hash = 0;
        if (hash_code(module, scan, operands, md_layouts[i].code_end, &hash) && hash == md_layouts[i].code_hash) {
            return &md_layouts[i];
        }
    }
    return nullptr;
}

// The variables below the init routine: the voice records (a mask and a register offset each), everything else zero.
// The replayer starts from this image, so the port does not read it again.

static bool check_variables(const MdModule* module, const MdScan* scan) {
    const uint32_t voices_bytes = MD_VOICES * MD_VOICE_RECORD_BYTES;
    if (scan->init < voices_bytes) {
        return false;
    }
    const uint32_t first_voice = scan->init - voices_bytes;
    for (uint32_t at = 0; at < scan->init; at++) {
        uint8_t expected = 0;
        if (at >= first_voice) {
            const uint32_t voice = (at - first_voice) / MD_VOICE_RECORD_BYTES;
            const uint32_t field = (at - first_voice) % MD_VOICE_RECORD_BYTES;
            if (field == MD_VOICE_MASK_BYTE) {
                expected = (uint8_t)(1u << voice);
            } else if (field == MD_VOICE_REGISTER_BYTE) {
                expected = (uint8_t)(MD_VOICE_REGISTER_STEP * voice);
            }
        }
        if (module->data[at] != expected) {
            return false;
        }
    }
    return true;
}

// The relocation of the long at `at`: image address to module offset.

static bool relocate(MdModule* module, uint32_t image_base, uint32_t at, uint32_t* out) {
    uint32_t value = 0;
    if (!read_long(module, at, &value)) {
        return false;
    }
    value = value - image_base + MD_LOAD_ADDRESS;
    *out = value;
    return write_long(module, at, value);
}

// The tables the module's own searches find: the offsets and sizes InitPlayer works out of the operands.

typedef struct MdTables {
    uint32_t subsongs;
    uint32_t subsong_count;
    uint32_t sfx;
    uint32_t sfx_count;
    uint32_t instruments;
    uint32_t instruments_limit;
    uint32_t instrument_count;
    uint32_t periods;
    bool shift_instruments; // the module is short of four bytes in the instrument table: three longs move up
} MdTables;

static bool read_tables(const MdModule* module, const MdScan* scan, MdTables* out) {
    uint32_t subsongs = 0;
    uint32_t subsongs_end = 0;
    uint32_t sfx = 0;
    uint32_t sfx_end = 0;
    uint32_t instruments = 0;
    uint32_t instruments_end = 0;
    uint32_t periods = 0;
    if (!read_long(module, scan->subsongs_operand, &subsongs)
        || !read_long(module, scan->subsongs_end_operand, &subsongs_end) || !read_long(module, scan->sfx_operand, &sfx)
        || !read_long(module, scan->sfx_end_operand, &sfx_end)
        || !read_long(module, scan->instruments_operand, &instruments)
        || !read_long(module, scan->instruments_end_operand, &instruments_end)
        || !read_long(module, scan->periods_operand, &periods)) {
        return false;
    }
    *out = (MdTables) { 0 };
    out->subsongs = subsongs - scan->image_base;
    out->sfx = sfx - scan->image_base;
    out->instruments = instruments - scan->image_base;
    out->instruments_limit = instruments_end - scan->image_base;
    out->periods = periods - scan->image_base;
    out->subsong_count = (subsongs_end - subsongs) / MD_SUBSONG_BYTES;
    const uint32_t sfx_bytes = sfx_end - sfx;
    const uint32_t instrument_bytes = instruments_end - instruments;
    // divu.w by 0x18: the quotient has to fit a word
    if (sfx_bytes > MD_SFX_BYTES * MD_MAX_INSTRUMENTS || instrument_bytes > MD_INSTRUMENT_BYTES * MD_MAX_INSTRUMENTS
        || sfx_bytes % MD_SFX_BYTES != 0) {
        return false;
    }
    out->sfx_count = sfx_bytes / MD_SFX_BYTES;
    out->instrument_count = instrument_bytes / MD_INSTRUMENT_BYTES;
    if (instrument_bytes % MD_INSTRUMENT_BYTES != 0) {
        // a table that is four bytes short counts the last record whole
        if ((instrument_bytes + 4) % MD_INSTRUMENT_BYTES != 0) {
            return false;
        }
        out->instrument_count = (instrument_bytes + 4) / MD_INSTRUMENT_BYTES;
        uint32_t marker = 0;
        out->shift_instruments
            = read_long(module, MD_PATCH_INSTRUMENTS_AT, &marker) && marker == MD_PATCH_INSTRUMENTS_MARKER;
    }
    // fewer than two instruments move the eagleplayer's later variables
    return out->subsong_count > 0 && out->subsong_count <= MD_MAX_SUBSONGS && out->instrument_count >= 2;
}

// The extents of the samples the relocation of the instrument and effect tables saw: the lowest sample address
// and the highest end, as the code compares them (signed, an update on a tie).

typedef struct MdSampleExtent {
    int32_t lowest;
    int32_t end;
} MdSampleExtent;

// One record of the instrument or effect table: the sample pointer is relocated, the extents follow.

static bool relocate_record(MdModule* module, uint32_t image_base, uint32_t at, bool first, MdSampleExtent* extent) {
    uint32_t sample = 0;
    uint32_t length = 0;
    if (!relocate(module, image_base, at, &sample) || !read_long(module, at + 4, &length)) {
        return false;
    }
    const int32_t end = (int32_t)(sample + 2u * length);
    if (first) {
        extent->lowest = (int32_t)sample;
        extent->end = end;
        return true;
    }
    if (extent->end <= (int32_t)sample) {
        extent->end = end;
    }
    if (extent->lowest >= (int32_t)sample) {
        extent->lowest = (int32_t)sample;
    }
    return true;
}

// The words the eagleplayer's search takes for the end of a track. They are fixed in its code, not the commands of
// the version (World Cup USA 94 ends tracks with 0xE4).
static const uint16_t md_track_end_words[] = { 0x94, 0xFE, 0xFF, 0x1FE, 0x1FF };

// A track ends at the first of these words, which the search takes with it.

static bool find_track_end(const MdModule* module, uint32_t from, uint32_t* out) {
    for (uint32_t at = from;; at += 2) {
        uint16_t word = 0;
        if (!read_word(module, at, &word)) {
            return false;
        }
        for (uint32_t i = 0; i < nuade_sizeof_array(md_track_end_words); i++) {
            if (word == md_track_end_words[i]) {
                *out = at + 2;
                return true;
            }
        }
    }
}

// Relocates the track pointers that the call commands hold. The search compares every word of the data between the
// tables and the samples (or after the samples, up to the end of the last track) with the call command; a hit takes
// the long six bytes on and goes on eight bytes later. `from` and `to` are load addresses.

static bool relocate_calls(MdModule* module, uint32_t image_base, uint32_t from, uint32_t to, uint16_t call,
                           int32_t* highest_track) {
    const uint32_t end = to - MD_LOAD_ADDRESS;
    uint32_t at = from - MD_LOAD_ADDRESS;
    do {
        uint16_t word = 0;
        if (at > end || (at & 1u) != 0 || !read_word(module, at, &word)) {
            return false;
        }
        if (word == call) {
            uint32_t track = 0;
            at += 6;
            if (!relocate(module, image_base, at, &track)) {
                return false;
            }
            if (highest_track != nullptr && *highest_track <= (int32_t)track) {
                *highest_track = (int32_t)track;
            }
        }
        at += 2;
    } while (at != end);
    return true;
}

// The call command: the code of the first compare after the tick routine's start, which the eagleplayer reads at a
// fixed distance from there.

static bool find_call_command(const MdModule* module, const MdScan* scan, uint16_t* out) {
    uint32_t compare = 0;
    return find_word(module, scan->tick, MD_OP_CMPI_W_D0, &compare)
           && read_word(module, compare + 2 + MD_CALL_COMMAND_OFFSET, out);
}

// The address of the end of a table, in the load addresses the relocation produces.

static uint32_t table_end_address(uint32_t offset, uint32_t count, uint32_t record_bytes) {
    return MD_LOAD_ADDRESS + offset + count * record_bytes;
}

// The sample pointers of the instrument and effect tables, relocated. Returns the extents of the samples and the
// address of the end of the tables (the later of the two, in load addresses as the code compares them).

static bool relocate_samples(MdModule* module, uint32_t image_base, const MdTables* tables, MdSampleExtent* extent,
                             uint32_t* tables_end) {
    *tables_end = 0;
    for (uint32_t i = 0; i < tables->instrument_count; i++) {
        if (!relocate_record(module, image_base, tables->instruments + i * MD_INSTRUMENT_BYTES, i == 0, extent)) {
            return false;
        }
    }
    if (tables->instrument_count >= 2) {
        *tables_end = table_end_address(tables->instruments, tables->instrument_count, MD_INSTRUMENT_BYTES);
    }
    for (uint32_t i = 0; i < tables->sfx_count; i++) {
        if (!relocate_record(module, image_base, tables->sfx + i * MD_SFX_BYTES, false, extent)) {
            return false;
        }
    }
    const uint32_t sfx_end = table_end_address(tables->sfx, tables->sfx_count, MD_SFX_BYTES);
    if ((int32_t)sfx_end >= (int32_t)*tables_end) {
        *tables_end = sfx_end;
    }
    return true;
}

// The track pointers of the subsongs (all but the -1 of a voice that is off), relocated. Returns the highest.

static bool relocate_subsongs(MdModule* module, uint32_t image_base, const MdTables* tables, int32_t* highest_track) {
    *highest_track = 0;
    for (uint32_t i = 0; i < tables->subsong_count * MD_VOICES; i++) {
        const uint32_t at = tables->subsongs + MD_TRACK_POINTER_BYTES * i;
        uint32_t track = 0;
        uint32_t pointer = 0;
        if (!read_long(module, at, &pointer)) {
            return false;
        }
        if (pointer == MD_NO_TRACK) {
            continue;
        }
        if (!relocate(module, image_base, at, &track)) {
            return false;
        }
        if (*highest_track <= (int32_t)track) {
            *highest_track = (int32_t)track;
        }
    }
    return true;
}

// InitPlayer after the searches: the data of the song is relocated to the load address of the module. Everything
// below is in load addresses, as the code compares them. Returns the end of the data the module uses, as an offset.

static bool relocate_module(MdModule* module, const MdScan* scan, const MdTables* tables, uint32_t* used_end) {
    const uint32_t base = scan->image_base;
    MdSampleExtent extent = { 0 };
    uint32_t tables_end = 0;
    int32_t highest_track = 0;
    uint16_t call = 0;
    if (tables->instruments > module->size
        || module->size - tables->instruments < tables->instrument_count * MD_INSTRUMENT_BYTES
        || !find_call_command(module, scan, &call) || !relocate_samples(module, base, tables, &extent, &tables_end)
        || !relocate_subsongs(module, base, tables, &highest_track)) {
        return false;
    }
    const int64_t module_end = (int64_t)MD_LOAD_ADDRESS + module->size;
    if (highest_track < extent.end) {
        // the tracks start before the end of the samples: the search covers what lies between the tables and the
        // first sample
        if (module_end < extent.end
            || !relocate_calls(module, base, tables_end, (uint32_t)extent.lowest, call, nullptr)) {
            return false;
        }
        *used_end = (uint32_t)extent.end - MD_LOAD_ADDRESS;
        return true;
    }
    uint32_t end = 0;
    if (module_end < highest_track || !find_track_end(module, (uint32_t)highest_track - MD_LOAD_ADDRESS, &end)
        || !relocate_calls(module, base, (uint32_t)extent.end, end + MD_LOAD_ADDRESS, call, &highest_track)) {
        return false;
    }
    if ((int32_t)(end + MD_LOAD_ADDRESS) <= highest_track
        && !find_track_end(module, (uint32_t)highest_track - MD_LOAD_ADDRESS, &end)) {
        return false;
    }
    *used_end = end;
    return true;
}

// The repairs of broken rips that InitPlayer applies to the module's bytes move the three longs from `from` up by four
// bytes and put `fill` in the gap.

static bool shift_longs_up(MdModule* module, uint32_t from, uint32_t fill) {
    uint32_t value = 0;
    for (uint32_t at = from + 3 * sizeof(uint32_t); at > from; at -= sizeof(uint32_t)) {
        if (!read_long(module, at - sizeof(uint32_t), &value) || !write_long(module, at, value)) {
            return false;
        }
    }
    return write_long(module, from, fill);
}

// The end of Mercs: a track word, then the shift.

static bool repair_mercs(MdModule* module) {
    return write_long(module, MD_PATCH_MERCS_TRACK, MD_PATCH_MERCS_TRACK_VALUE)
           && shift_longs_up(module, MD_PATCH_MERCS_RECORD, MD_PATCH_MERCS_RECORD_VALUE);
}

// The operands that name the tables are among the ones the relocation scan reached (a scan that lost its step on the
// way would leave them unrelocated).

static bool has_operand(const MdOperands* operands, uint32_t at) {
    for (uint32_t i = 0; i < operands->count; i++) {
        if (operands->at[i] == at) {
            return true;
        }
    }
    return false;
}

static bool tables_in_module(const MdModule* module, const MdTables* tables) {
    const uint64_t subsongs_bytes = (uint64_t)MD_SUBSONG_BYTES * tables->subsong_count;
    const uint64_t instruments_bytes = (uint64_t)MD_INSTRUMENT_BYTES * tables->instrument_count;
    // the tables lie behind the code, where the port keeps the words the eagleplayer would have changed
    return tables->periods == module->layout->periods && tables->periods <= module->size
           && tables->subsongs >= tables->periods && tables->subsongs + subsongs_bytes <= module->size
           && tables->instruments >= tables->periods && tables->instruments + instruments_bytes <= module->size
           && tables->instruments_limit >= tables->instruments && tables->sfx >= tables->periods;
}

bool md_module_parse(MdModule* module, uint8_t* data, uint32_t size) {
    MdScan scan;
    MdOperands operands;
    MdTables tables;
    uint32_t used_end = 0;
    *module = (MdModule) { .data = data, .size = size };
    if (!check2(module) || !locate(module, &scan) || !read_tables(module, &scan, &tables)
        || !find_operands(module, scan.init, tables.periods, &operands)) {
        return false;
    }
    module->layout = find_layout(module, &scan, &operands);
    if (module->layout == nullptr || !check_variables(module, &scan) || !has_operand(&operands, scan.subsongs_operand)
        || !has_operand(&operands, scan.subsongs_end_operand) || !has_operand(&operands, scan.sfx_operand)
        || !has_operand(&operands, scan.sfx_end_operand) || !has_operand(&operands, scan.instruments_operand)
        || !has_operand(&operands, scan.instruments_end_operand) || !has_operand(&operands, scan.periods_operand)) {
        return false;
    }
    if (!tables_in_module(module, &tables)) {
        return false;
    }
    if (tables.shift_instruments && !shift_longs_up(module, MD_PATCH_INSTRUMENTS_FROM, 0)) {
        return false;
    }
    if (!relocate_module(module, &scan, &tables, &used_end)) {
        return false;
    }
    if (used_end == MD_PATCH_MERCS_SIZE && !repair_mercs(module)) {
        return false;
    }
    module->operands = operands;
    module->subsong_count = tables.subsong_count;
    module->subsongs = tables.subsongs;
    module->instruments = tables.instruments;
    module->instruments_limit = tables.instruments_limit;
    module->periods = tables.periods;
    return true;
}

bool md_read_subsong(const MdModule* module, uint32_t index, MdSubsong* out) {
    if (index >= module->subsong_count) {
        return false;
    }
    for (uint32_t voice = 0; voice < MD_VOICES; voice++) {
        if (!read_long(module, module->subsongs + MD_SUBSONG_BYTES * index + MD_TRACK_POINTER_BYTES * voice,
                       &out->track[voice])) {
            return false;
        }
    }
    return true;
}

// The six longs of an instrument record; the words are the low halves.

bool md_read_instrument(const MdModule* module, uint32_t index, MdInstrument* out) {
    uint32_t fields[6];
    const uint32_t at = module->instruments + index * MD_INSTRUMENT_BYTES;
    for (uint32_t i = 0; i < nuade_sizeof_array(fields); i++) {
        if (!read_long(module, at + 4u * i, &fields[i])) {
            return false;
        }
    }
    *out = (MdInstrument) {
        .sample = fields[0],
        .length_words = (uint16_t)fields[1],
        .period = (uint16_t)fields[2],
        .volume = (uint16_t)fields[3],
        .transpose = (int16_t)fields[4],
        .loop_offset = (int16_t)fields[5],
    };
    return true;
}

MdCommand md_command(const MdModule* module, uint16_t code) {
    const uint16_t end = module->layout->command_end;
    if (code < MD_FIRST_COMMAND) {
        return MdCommand_Note;
    }
    if (code == end) {
        return MdCommand_End;
    }
    if (code == end + MD_COMMAND_CALL_OFFSET) {
        return MdCommand_Call;
    }
    if (code == end + MD_COMMAND_LOOP_NEXT_OFFSET) {
        return MdCommand_LoopNext;
    }
    if (code == end + MD_COMMAND_LOOP_START_OFFSET) {
        return MdCommand_LoopStart;
    }
    if (code == MD_COMMAND_RESTART) {
        return MdCommand_Restart;
    }
    if (code == MD_COMMAND_STOP) {
        return MdCommand_Stop;
    }
    return MdCommand_Instrument;
}

uint16_t md_unit(const MdModule* module) {
    return module->layout->byte_tracks ? 1 : 2;
}

bool md_offset(const MdModule* module, uint32_t address, uint32_t* out) {
    const uint32_t offset = address - MD_LOAD_ADDRESS;
    if (address < MD_LOAD_ADDRESS || offset >= module->size) {
        return false;
    }
    *out = offset;
    return true;
}

bool md_read_code(const MdModule* module, uint32_t address, uint16_t* out) {
    uint32_t offset = 0;
    if (!md_offset(module, address, &offset)) {
        return false;
    }
    if (module->layout->byte_tracks) {
        *out = module->data[offset];
        return true;
    }
    return read_word(module, offset, out);
}

bool md_read_note(const MdModule* module, uint32_t address, MdNote* out) {
    return md_read_code(module, address, &out->note) && md_read_code(module, address + md_unit(module), &out->duration);
}

// The call command reads words and a long, also in the byte tracks.

bool md_read_call(const MdModule* module, uint32_t address, MdCall* out) {
    uint32_t offset = 0;
    uint16_t transpose = 0;
    if (!md_offset(module, address, &offset) || !read_word(module, offset + 2, &out->repeats)
        || !read_word(module, offset + 4, &transpose) || !read_long(module, offset + 6, &out->track)) {
        return false;
    }
    out->transpose = (int16_t)transpose;
    return true;
}

bool md_read_loop_count(const MdModule* module, uint32_t address, uint16_t* out) {
    return md_read_code(module, address + md_unit(module), out);
}

bool md_read_period(const MdModule* module, int16_t byte_offset, uint16_t* out) {
    const int64_t at = (int64_t)module->periods + byte_offset;
    if (byte_offset < -(int32_t)MD_PERIOD_BACK_MAX) {
        return false; // the code before the table: its hooks and the variables of the replayer are not modelled
    }
    // inside the code the eagleplayer has relocated its operands to the load address of the module
    for (uint32_t i = 0; i < module->operands.count; i++) {
        const int64_t operand = module->operands.at[i];
        if (operand + 4 > at && operand < at + 2) {
            return false;
        }
    }
    return read_word(module, (uint32_t)at, out);
}
