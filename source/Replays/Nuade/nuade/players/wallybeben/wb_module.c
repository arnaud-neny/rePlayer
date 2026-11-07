// SPDX-License-Identifier: MIT
// Wally Beben: recognising a module's replayer build and reading its tables.
//
// The eagleplayer finds everything by scanning the module for instruction words, rewrites the addresses and patches
// the code with its patch table. Loading a module here repeats those scans (so that what UADE does to the module is
// known), then recognises the build by its decoded code (the hash and the partition of the absolute operands) and
// takes the table and variable addresses from the operands the build's table names. Everything the scans found must
// agree with those operands; a module that matches no build, or disagrees anywhere, is refused (UADE plays it).

#include "wb_internal.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "wb_decode.h"
#include <stdlib.h>
#include <string.h>

// Words the eagleplayer scans for.
#define OP_RTS 0x4E75u
#define OP_MOVE_B_ABS_D0 0x1039u
#define OP_MOVE_L_IMM_D1 0x223Cu
#define OP_LEA_ABS_A0 0x41F9u
#define OP_LEA_ABS_A1 0x43F9u
#define OP_LEA_ABS_A6 0x4DF9u
#define OP_ASL_L_2_D4 0xE584u
#define OP_MOVE_W_D0_A1 0x3280u
#define OP_MOVEQ_0_D0 0x7000u
#define OP_CLR_L_D0 0x4280u
#define OP_ADDA_L_ABS_A1 0xD3F9u
#define OP_MOVEM_ABS 0x4CF9u
#define OP_MOVE_B_IMM_D4 0x183Cu
#define OP_ADDI_B_IMM_D0 0x0600u
#define OP_BRA_W 0x6000u
#define OP_BSR_W 0x6100u
#define OP_CLR_B_ABS 0x4239u
#define OP_MOVEM_SAVE_ALL 0x48E7FFFEu
#define LONG_PERIOD_PAIR 0x03580328u  // two entries of the period table, behind the code
#define LONG_MOVE_W_D3_D6 0x3C363000u // move.w (a6,d3.w),d6
#define LONG_ADD_L_D2_D6 0xDCB62000u  // add.l (a6,d2.w),d6
#define LONG_MOVEM_D0_D7 0x4CF900FFu

// The song data of one game that the eagleplayer repairs: the words it checks, the amount it subtracts.
#define FIX_SONG_COUNT 6u
#define FIX_LONG_AT 0xC12u
#define FIX_LONG_VALUE 0x9298u
#define FIX_WORD_AT 0xC38u
#define FIX_WORD_VALUE 0x0FA0u
#define FIX_AMOUNT 0x50u

#define INSTRUMENT_BYTES 8u
#define ENVELOPE_BYTES 8u
#define ARP_TABLE_BYTES 8u
#define ZERO_REGS_BYTES 32u

// The patch table of the eagleplayer (WallyBeben, code offset 0x900): each entry is a run of instruction words searched
// in the module code; the first match becomes a call of the entry's patch routine.

typedef struct PatchPattern {
    uint8_t words;
    uint16_t pattern[14];
} PatchPattern;

static const PatchPattern k_patches[] = {
    // ori.l #$8200,d1; move.w $dff002,d0; andi.w #$5f0,d0; or.w d0,d1; move.w d1,$dff096; eori.w #$ffff,d1
    { 14,
      { 0x0081, 0x0000, 0x8200, 0x3039, 0x00DF, 0xF002, 0x0240, 0x05F0, 0x8240, 0x33C1, 0x00DF, 0xF096, 0x0A41,
        0xFFFF } },
    // the four AUDxVOL writes of the output routine
    { 10, { 0x3480, 0x1019, 0x3540, 0x0010, 0x1019, 0x3540, 0x0020, 0x1011, 0x3540, 0x0030 } },
    // move.w d0,$10(a5); move.w d0,$20(a5): the volume reset routine
    { 4, { 0x3B40, 0x0010, 0x3B40, 0x0020 } },
    // andi.l #$ff,d6: the voice offset of a note start
    { 3, { 0x0286, 0x0000, 0x00FF } },
    // move.w d1,$dff096; eori.w #$820f,d1; move.w d1,$dff096
    { 8, { 0x33C1, 0x00DF, 0xF096, 0x0A41, 0x820F, 0x33C1, 0x00DF, 0xF096 } },
    // move.b $c(a0),d0; addq.b #1,$c(a0); adda.l d0,a1: the read of a sequence list
    { 5, { 0x1028, 0x000C, 0x5228, 0x000C, 0xD3C0 } },
    // move.w d1,$dff096
    { 3, { 0x33C1, 0x00DF, 0xF096 } },
    // lea $dff0a0,a5; move.l (a1,d4.w),(a5,d6.w): the sample start of a note
    { 6, { 0x4BF9, 0x00DF, 0xF0A0, 0x2BB1, 0x4000, 0x6000 } },
    // lea $dff0a0,a5; adda.l d6,a5
    { 4, { 0x4BF9, 0x00DF, 0xF0A0, 0xDBC6 } },
    // andi.l #$fff,d6: the loop offset
    { 3, { 0x0286, 0x0000, 0x0FFF } },
};
#define PATCH_COUNT ((uint32_t)(sizeof(k_patches) / sizeof(k_patches[0])))

static bool in_range(uint32_t size, uint32_t at, uint32_t bytes) {
    return bytes <= size && at <= size - bytes;
}

bool wb_budget_take(WbBudget* budget, uint64_t units) {
    if (units > budget->left) {
        budget->left = 0;
        return false;
    }
    budget->left -= units;
    return true;
}

// The scan of the eagleplayer for a word: `*pos` moves to the word after the match (the post-incrementing compare).
static bool scan_word(const uint8_t* mod, uint32_t size, WbBudget* budget, uint32_t* pos, uint16_t word) {
    if (*pos > size || !wb_budget_take(budget, (size - *pos) / 2 + 1)) {
        return false;
    }
    for (uint32_t at = *pos; in_range(size, at, 2); at += 2) {
        if (nuade_read_be16(mod + at) == word) {
            *pos = at + 2;
            return true;
        }
    }
    return false;
}

// The scan for a long: `*pos` stays at the match.
static bool scan_long(const uint8_t* mod, uint32_t size, WbBudget* budget, uint32_t* pos, uint32_t value) {
    if (*pos > size || !wb_budget_take(budget, (size - *pos) / 2 + 1)) {
        return false;
    }
    for (uint32_t at = *pos; in_range(size, at, 4); at += 2) {
        if (nuade_read_be32(mod + at) == value) {
            *pos = at;
            return true;
        }
    }
    return false;
}

// The eagleplayer's Check2: a bra.w to a short reset routine, the register save, a bsr.w to the tick routine.

static bool check_header(const uint8_t* mod, uint32_t size) {
    if (size < 0x40 || nuade_read_be16(mod) != OP_BRA_W) {
        return false;
    }
    const uint32_t disp = nuade_read_be16(mod + 2);
    if (disp == 0 || (disp & 0x8000u) != 0 || (disp & 1u) != 0) {
        return false;
    }
    uint32_t at = 2 + disp;
    if (!in_range(size, at, 4) || mod[at] != 0x61) {
        return false;
    }
    at++;
    at++;
    if (mod[at - 1] == 0) {
        at += 2;
    }
    // clr.b abs.l; clr.b abs.l; rts
    if (!in_range(size, at, 14) || nuade_read_be16(mod + at) != OP_CLR_B_ABS
        || nuade_read_be16(mod + at + 6) != OP_CLR_B_ABS || nuade_read_be16(mod + at + 12) != OP_RTS) {
        return false;
    }
    if (nuade_read_be32(mod + 4) != OP_MOVEM_SAVE_ALL || nuade_read_be16(mod + 8) != OP_BSR_W) {
        return false;
    }
    const uint32_t tick = 10 + (uint32_t)(int32_t)(int16_t)nuade_read_be16(mod + 10);
    if (!in_range(size, tick, 10)) {
        return false;
    }
    // the tick routine starts with movem.l abs,d0-d7 (a table of zeros), or four bytes of a call, then move.b abs,d0
    return nuade_read_be32(mod + tick) == LONG_MOVEM_D0_D7 || nuade_read_be16(mod + tick + 4) == OP_MOVE_B_ABS_D0;
}

// What the eagleplayer's InitPlayer finds by scanning (see the order of its scans).

typedef struct Scan {
    uint32_t flag_at; // the command byte: the word behind the first rts
    uint32_t origin;
    uint32_t song_limit;
    uint32_t song_count_at; // the move.l #n,d1 instruction (the song count is a parameter of the build)
    uint32_t songs;
    uint32_t samples;
    uint32_t instr;
    uint32_t silence; // movem builds
    uint32_t code_rts;
    bool movem;
} Scan;

static bool scan_module(Scan* s, const uint8_t* mod, uint32_t size, WbBudget* budget) {
    memset(s, 0, sizeof(*s));
    s->movem = nuade_read_be16(mod + 0x14) == OP_MOVEM_ABS;
    uint32_t p = 0;
    if (!scan_word(mod, size, budget, &p, OP_RTS)) {
        return false;
    }
    s->flag_at = p;
    if (!scan_word(mod, size, budget, &p, OP_MOVE_B_ABS_D0) || !in_range(size, p, 4)) {
        return false;
    }
    s->origin = nuade_read_be32(mod + p) - s->flag_at;
    if (!scan_word(mod, size, budget, &p, OP_MOVE_L_IMM_D1) || !in_range(size, p, 4)) {
        return false;
    }
    s->song_count_at = p - 2;
    s->song_limit = nuade_read_be32(mod + p);
    if (!scan_word(mod, size, budget, &p, OP_LEA_ABS_A0) || !in_range(size, p, 4)) {
        return false;
    }
    s->songs = nuade_read_be32(mod + p) - s->origin;
    p += 4;
    if (!scan_word(mod, size, budget, &p, 0xD1C1u)) {
        return false;
    }
    if (s->movem) {
        if (!scan_long(mod, size, budget, &p, LONG_MOVE_W_D3_D6) || !in_range(size, p, 14)) {
            return false;
        }
        // lea abs,a6 follows; the add.l that the eagleplayer replaces comes right after it
        if (nuade_read_be16(mod + p + 4) != OP_LEA_ABS_A6 || nuade_read_be32(mod + p + 10) != LONG_ADD_L_D2_D6) {
            return false;
        }
        s->silence = nuade_read_be32(mod + p + 6) - s->origin;
    }
    if (!scan_word(mod, size, budget, &p, OP_ASL_L_2_D4) || p < 6 || !in_range(size, p - 6, 4)) {
        return false;
    }
    s->samples = nuade_read_be32(mod + p - 6) - s->origin;
    if (!scan_long(mod, size, budget, &p, LONG_PERIOD_PAIR)) {
        return false;
    }
    // back to the rts in front of the period table, then to the last `move.w d0,(a1)` before it
    if (!wb_budget_take(budget, 2 * (uint64_t)p)) {
        return false;
    }
    uint32_t q = p;
    do {
        if (q < 2) {
            return false;
        }
        q -= 2;
    } while (nuade_read_be16(mod + q) != OP_RTS);
    s->code_rts = q;
    uint32_t r = q;
    do {
        if (r < 2) {
            return false;
        }
        r -= 2;
    } while (nuade_read_be16(mod + r) != OP_MOVE_W_D0_A1);
    if (r < 6) {
        return false;
    }
    const uint16_t clear_d0 = nuade_read_be16(mod + r - 6) == OP_ADDA_L_ABS_A1 ? OP_MOVEQ_0_D0 : OP_CLR_L_D0;
    // back to the start of the routine behind an rts that clears d0, then forward to its lea abs,a1
    uint32_t t = r;
    do {
        if (t < 4) {
            return false;
        }
        t -= 2;
    } while (nuade_read_be16(mod + t) != clear_d0 || nuade_read_be16(mod + t - 2) != OP_RTS);
    uint32_t u = t;
    if (!scan_word(mod, size, budget, &u, OP_LEA_ABS_A1) || !in_range(size, u, 4)) {
        return false;
    }
    s->instr = nuade_read_be32(mod + u) - s->origin;
    return true;
}

static uint64_t fnv_u16(uint64_t h, uint32_t v) {
    h = (h ^ ((v >> 8) & 0xFFu)) * WB_FNV_PRIME;
    return (h ^ (v & 0xFFu)) * WB_FNV_PRIME;
}

// The equalities between the operands: per operand the number of its value among the distinct values so far.
static uint64_t partition_hash(const uint32_t* values, uint32_t count) {
    uint32_t distinct[WB_MAX_SLOTS];
    uint32_t distinct_count = 0;
    uint64_t h = WB_FNV_OFFSET;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t id = 0;
        while (id < distinct_count && distinct[id] != values[i]) {
            id++;
        }
        if (id == distinct_count) {
            distinct[distinct_count++] = values[i];
        }
        h = fnv_u16(h, id);
    }
    return h;
}

// The eagleplayer's patches, applied in the order of its table: the first match of each pattern in the code (as the
// earlier patches left it) becomes a jsr to the patch routine, padded with nops. Bit n of the result: patch n + 1 is
// found. `on_instruction`: every match starts an instruction of the decoded code.
static bool find_patches(const uint8_t* mod, uint32_t code_len, const WbCode* code, WbBudget* budget,
                         uint32_t* found_out) {
    if (!wb_budget_take(budget, (uint64_t)PATCH_COUNT * code_len)) {
        return false;
    }
    uint8_t* patched = malloc(code_len);
    if (patched == nullptr) {
        return false;
    }
    memcpy(patched, mod, code_len);
    uint32_t found = 0;
    bool on_instruction = true;
    for (uint32_t n = 0; n < PATCH_COUNT; n++) {
        const PatchPattern* pat = &k_patches[n];
        for (uint32_t at = 0; at + (uint32_t)pat->words * 2 <= code_len; at += 2) {
            bool match = true;
            for (uint32_t k = 0; k < pat->words && match; k++) {
                match = nuade_read_be16(patched + at + k * 2) == pat->pattern[k];
            }
            if (match) {
                found |= 1u << n;
                on_instruction = on_instruction && wb_code_insn_at(code, at) != nullptr;
                patched[at] = 0x4E;
                patched[at + 1] = 0xB9;
                memset(patched + at + 2, 0, 4);
                for (uint32_t k = 3; k < pat->words; k++) {
                    patched[at + k * 2] = 0x4E;
                    patched[at + k * 2 + 1] = 0x71;
                }
                break;
            }
        }
    }
    free(patched);
    *found_out = found;
    return on_instruction;
}

typedef struct RoleInfo {
    uint16_t bytes;
    bool table; // a data table (else a variable)
} RoleInfo;

static const RoleInfo k_role_info[WbRole_Count] = {
    [WbRole_Cmd] = { 1, false },
    [WbRole_SlideSpeedA] = { 1, false },
    [WbRole_SlideSpeedB] = { 1, false },
    [WbRole_SlideTarget] = { 1, false },
    [WbRole_ZeroRegs] = { ZERO_REGS_BYTES, true },
    [WbRole_Periods] = { 0, true },
    [WbRole_PeriodOut] = { 8, false },
    [WbRole_Voices] = { WB_VOICE_RECORD_BYTES, false },
    [WbRole_VolumeOut] = { 4, false },
    [WbRole_InstFlags] = { 4, false },
    [WbRole_VoiceIdx] = { 1, false },
    [WbRole_Playing] = { 1, false },
    [WbRole_Tempo] = { 1, false },
    [WbRole_TempoCtr] = { 1, false },
    [WbRole_VibLimit] = { 1, false },
    [WbRole_VibShift] = { 1, false },
    [WbRole_ArpMask] = { 1, false },
    [WbRole_StreamPos] = { 1, false },
    [WbRole_VoiceHw] = { 1, false },
    [WbRole_SlideDir] = { 1, false },
    [WbRole_StopMask] = { 1, false },
    [WbRole_PeriodBase] = { 8, false },
    [WbRole_SlideTargetPeriod] = { 8, false },
    [WbRole_SlideStep] = { 2, false },
    [WbRole_LoopLc] = { 16, false },
    [WbRole_LoopLen] = { 8, false },
    [WbRole_LoopOff] = { 8, false },
    [WbRole_StreamPtr] = { 16, false },
    [WbRole_SongSeq] = { 16, false },
    [WbRole_SeqPtr] = { 4, false },
    [WbRole_PatPtr] = { 4, false },
    [WbRole_VibDelta] = { 2, false },
    [WbRole_VibAcc] = { 2, false },
    [WbRole_Voice4] = { 4, false },
    [WbRole_Voice2] = { 4, false },
    [WbRole_ArpBuf] = { 40, false },
    [WbRole_Samples] = { 0, true },
    [WbRole_Silence] = { 4, true },
    [WbRole_Instr] = { 0, true },
    [WbRole_ArpTable] = { ARP_TABLE_BYTES, true },
    [WbRole_Envs] = { 0, true },
    [WbRole_Songs] = { 0, true },
    [WbRole_Patterns] = { 0, true },
    [WbRole_SlideSum] = { 1, false },
    [WbRole_VoiceTop] = { 1, false },
};

static uint32_t role_bytes(const WbModule* m, WbRole role) {
    return role == WbRole_Voices ? m->voice_bytes : k_role_info[role].bytes;
}

static bool overlap(uint32_t a, uint32_t a_bytes, uint32_t b, uint32_t b_bytes) {
    return (uint64_t)a < (uint64_t)b + b_bytes && (uint64_t)b < (uint64_t)a + a_bytes;
}

// A role the replayer keeps as a variable in the data area (the code's immediates and the command byte apart).
static bool is_variable(const WbModule* m, WbRole role) {
    return m->has[role] && !k_role_info[role].table && role != WbRole_Cmd && m->addr[role] >= m->code_end;
}

// The code operands of the self-modifying slide are immediates of two instructions; true when the operand at `at` is
// the byte of the immediate (the third byte) of an instruction with opcode `opcode`.
static bool is_immediate_of(const WbCode* code, uint32_t at, uint16_t opcode, const uint8_t* mod) {
    const WbInsn* insn = at >= 3 ? wb_code_insn_at(code, at - 3) : nullptr;
    return insn != nullptr && insn->len == 4 && nuade_read_be16(mod + insn->at) == opcode;
}

// Roles, their addresses and everything the replayer's layout must satisfy.
static bool bind_roles(WbModule* m, const WbCode* code, const uint32_t* values, uint32_t slot_count, const Scan* scan) {
    const WbVersion* version = m->version;
    for (uint32_t i = 0; i < version->slot_count; i++) {
        const WbSlotRef ref = version->slots[i];
        if (ref.slot >= slot_count || ref.role >= WbRole_Count || m->has[ref.role]) {
            return false;
        }
        const uint32_t value = values[ref.slot] - m->origin;
        if (value > m->size) {
            return false;
        }
        m->has[ref.role] = true;
        m->addr[ref.role] = value;
    }
    static const WbRole k_required[]
        = { WbRole_Cmd,      WbRole_Periods, WbRole_PeriodOut, WbRole_Voices, WbRole_Tempo,
            WbRole_StopMask, WbRole_ArpBuf,  WbRole_Samples,   WbRole_Instr,  WbRole_ArpTable,
            WbRole_Envs,     WbRole_Songs,   WbRole_Patterns };
    for (uint32_t i = 0; i < nuade_sizeof_array(k_required); i++) {
        if (!m->has[k_required[i]]) {
            return false;
        }
    }
    if (version->quirks.loop_mode == WbLoop_FlagSilence && (!m->has[WbRole_Silence] || !m->has[WbRole_InstFlags])) {
        return false;
    }
    if (m->addr[WbRole_Cmd] != scan->flag_at || m->addr[WbRole_Songs] != scan->songs
        || m->addr[WbRole_Samples] != scan->samples || m->addr[WbRole_Instr] != scan->instr
        || (scan->movem && (!m->has[WbRole_Silence] || m->addr[WbRole_Silence] != scan->silence))) {
        return false;
    }
    if (m->has[WbRole_VolumeOut]
        && m->addr[WbRole_VolumeOut] != m->addr[WbRole_Voices] + offsetof(WbVoiceBlock, volume)) {
        return false;
    }
    if (m->has[WbRole_InstFlags]
        && m->addr[WbRole_InstFlags] != m->addr[WbRole_Voices] + offsetof(WbVoiceBlock, inst_flags)) {
        return false;
    }
    // the first voice of the voice loop is the header byte behind the command byte
    if (m->has[WbRole_VoiceTop]
        && (m->addr[WbRole_VoiceTop] != scan->flag_at + 1 || m->image[m->addr[WbRole_VoiceTop]] >= WB_VOICES)) {
        return false;
    }
    // the period table runs up to the variable behind it
    const uint32_t periods = m->addr[WbRole_Periods];
    const uint32_t period_out = m->addr[WbRole_PeriodOut];
    if (period_out <= periods || ((period_out - periods) & 1u) != 0
        || !in_range(m->size, periods, period_out - periods)) {
        return false;
    }
    m->period_count = (period_out - periods) / 2;
    if (m->has[WbRole_ZeroRegs]) {
        if (!in_range(m->size, m->addr[WbRole_ZeroRegs], ZERO_REGS_BYTES)) {
            return false;
        }
        for (uint32_t i = 0; i < ZERO_REGS_BYTES; i++) {
            if (m->image[m->addr[WbRole_ZeroRegs] + i] != 0) {
                return false;
            }
        }
    }
    // the slide's operands are immediates of the code in the builds that patch themselves, plain variables otherwise;
    // nothing else addressed by the code lies inside it
    for (uint32_t role = 0; role < WbRole_Count; role++) {
        if (!m->has[(WbRole)role] || m->addr[role] >= m->code_end || role == WbRole_Cmd) {
            continue;
        }
        if (role == WbRole_VoiceTop) {
            continue; // a header byte
        }
        const bool target = role == WbRole_SlideTarget;
        if (!(target || role == WbRole_SlideSpeedA || role == WbRole_SlideSpeedB)
            || !is_immediate_of(code, m->addr[role], target ? OP_MOVE_B_IMM_D4 : OP_ADDI_B_IMM_D0, m->image)) {
            return false;
        }
    }
    // the variables: each lies outside the tables and the code, and no two overlap (the aliases of the voice records
    // apart)
    m->var_lo = UINT32_MAX;
    m->var_hi = 0;
    for (uint32_t a = 0; a < WbRole_Count; a++) {
        if (!is_variable(m, (WbRole)a)) {
            continue;
        }
        const bool alias_a = a == WbRole_VolumeOut || a == WbRole_InstFlags;
        if (!in_range(m->size, m->addr[a], role_bytes(m, (WbRole)a))) {
            return false;
        }
        m->var_lo = m->addr[a] < m->var_lo ? m->addr[a] : m->var_lo;
        m->var_hi
            = m->addr[a] + role_bytes(m, (WbRole)a) > m->var_hi ? m->addr[a] + role_bytes(m, (WbRole)a) : m->var_hi;
        for (uint32_t b = a + 1; b < WbRole_Count; b++) {
            const bool alias_b = b == WbRole_VolumeOut || b == WbRole_InstFlags;
            if (!is_variable(m, (WbRole)b)) {
                continue;
            }
            if (overlap(m->addr[a], role_bytes(m, (WbRole)a), m->addr[b], role_bytes(m, (WbRole)b))
                && !((alias_a && b == WbRole_Voices) || (alias_b && a == WbRole_Voices))) {
                return false;
            }
        }
    }
    return true;
}

// A pointer of the song data as a module offset. An address behind the module and below the end of UADE's chip memory
// reads as zeros: its offset is kept (the module reads past its end as zeros). WB_BAD_POINTER for what the port does
// not hold: UADE's own memory below the module and the part of the address space above the zeros.
static uint32_t to_offset(uint32_t address, uint32_t origin) {
    const uint32_t absolute = (WB_UADE_LOAD_ADDRESS + (address - origin)) & WB_ADDRESS_MASK;
    return absolute >= WB_UADE_LOAD_ADDRESS && absolute < WB_UNMAPPED_HIGH ? absolute - WB_UADE_LOAD_ADDRESS
                                                                           : WB_BAD_POINTER;
}

#define LAYOUT_EXTENTS 7u

// A span of the module's bytes that some table or variable occupies.
typedef struct Extent {
    uint32_t at;
    uint32_t bytes;
    bool relocated; // the eagleplayer rewrites the span at load
} Extent;

// The tables the port reads from the file, the variables it keeps in structs and the code must not share a byte: the
// original reads its own writes, the port reads the file's initial bytes.
static bool check_layout(const WbModule* m, uint32_t instrument_count) {
    Extent extents[LAYOUT_EXTENTS];
    uint32_t count = 0;
    extents[count++] = (Extent) { m->addr[WbRole_Periods], m->period_count * 2, false };
    if (m->has[WbRole_ZeroRegs]) {
        extents[count++] = (Extent) { m->addr[WbRole_ZeroRegs], ZERO_REGS_BYTES, false };
    }
    extents[count++] = (Extent) { m->addr[WbRole_ArpTable], ARP_TABLE_BYTES, false };
    if (m->has[WbRole_Silence]) {
        extents[count++] = (Extent) { m->addr[WbRole_Silence], 4, true };
    }
    extents[count++]
        = (Extent) { m->addr[WbRole_Songs], m->subsong_count * WB_SONG_BYTES + m->pattern_count * 4, true };
    extents[count++] = (Extent) { m->addr[WbRole_Samples], m->sample_count * 4, true };
    extents[count++] = (Extent) { m->addr[WbRole_Instr], instrument_count * INSTRUMENT_BYTES, false };
    // the record that ends the count: its first long is read after the tables are relocated
    const Extent end_record = { m->addr[WbRole_Instr] + instrument_count * INSTRUMENT_BYTES, 4, false };
    for (uint32_t i = 0; i < count; i++) {
        if (!in_range(m->size, extents[i].at, extents[i].bytes) || extents[i].at < m->code_end) {
            return false;
        }
        for (uint32_t role = 0; role < WbRole_Count; role++) {
            if (is_variable(m, (WbRole)role)
                && overlap(extents[i].at, extents[i].bytes, m->addr[role], role_bytes(m, (WbRole)role))) {
                return false;
            }
        }
        for (uint32_t j = i + 1; j < count; j++) {
            if (overlap(extents[i].at, extents[i].bytes, extents[j].at, extents[j].bytes)) {
                return false;
            }
        }
        if (extents[i].relocated && overlap(extents[i].at, extents[i].bytes, end_record.at, end_record.bytes)) {
            return false;
        }
    }
    return true;
}

static bool load_tables(WbModule* m, const Scan* scan, WbBudget* budget) {
    const uint8_t* img = m->image;
    const uint32_t songs = m->addr[WbRole_Songs];
    const uint32_t song_count = m->subsong_count;
    const uint32_t patterns = m->addr[WbRole_Patterns];
    // the pattern table starts where the song table ends
    if (song_count == 0 || !in_range(m->size, songs, song_count * WB_SONG_BYTES)
        || songs + song_count * WB_SONG_BYTES != patterns || !wb_budget_take(budget, m->size / 2)) {
        return false;
    }
    m->song_seq = calloc((size_t)song_count * 4, sizeof(uint32_t));
    if (m->song_seq == nullptr) {
        return false;
    }
    // the eagleplayer relocates the pattern table up to the lowest address a song names
    uint32_t lowest = UINT32_MAX;
    for (uint32_t i = 0; i < song_count * 4; i++) {
        const uint32_t off = nuade_read_be32(img + songs + i * 4) - m->origin;
        if (off >= m->size || off <= patterns) {
            return false;
        }
        m->song_seq[i] = off;
        lowest = off < lowest ? off : lowest;
    }
    if (lowest <= patterns || ((lowest - patterns) & 3u) != 0) {
        return false;
    }
    m->pattern_count = (lowest - patterns) / 4;
    m->pattern_ptr = calloc(m->pattern_count, sizeof(uint32_t));
    if (m->pattern_ptr == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < m->pattern_count; i++) {
        const uint32_t off = to_offset(nuade_read_be32(img + patterns + i * 4), m->origin);
        m->pattern_ptr[i] = off != WB_BAD_POINTER && off >= m->size ? WB_ZERO_STREAM : off;
    }
    // the sample table runs up to the instrument records (or, in the builds with a silent repeat, up to its pointer)
    const uint32_t samples = m->addr[WbRole_Samples];
    const uint32_t end = scan->movem ? m->addr[WbRole_Silence] : m->addr[WbRole_Instr];
    if (end <= samples || ((end - samples) & 3u) != 0 || !in_range(m->size, samples, end - samples)) {
        return false;
    }
    m->sample_count = (end - samples) / 4;
    m->sample_ptr = calloc(m->sample_count, sizeof(uint32_t));
    if (m->sample_ptr == nullptr) {
        return false;
    }
    for (uint32_t i = 0; i < m->sample_count; i++) {
        m->sample_ptr[i] = to_offset(nuade_read_be32(img + samples + i * 4), m->origin);
    }
    if (m->has[WbRole_Silence]) {
        if (!in_range(m->size, m->addr[WbRole_Silence], 4)) {
            return false;
        }
        m->silence_ptr = to_offset(nuade_read_be32(img + m->addr[WbRole_Silence]), m->origin);
    }
    // the instrument records the eagleplayer counts: the first byte of record n is n and its length is set. The count
    // runs up to a record that does not qualify, which must lie in the file.
    const uint32_t instr = m->addr[WbRole_Instr];
    uint32_t count = 0;
    for (;;) {
        if (!in_range(m->size, instr + count * INSTRUMENT_BYTES, INSTRUMENT_BYTES) || count > 0xFFu) {
            return false;
        }
        const uint8_t* record = img + instr + count * INSTRUMENT_BYTES;
        if (record[0] != (uint8_t)count || nuade_read_be16(record + 2) == 0) {
            break;
        }
        count++;
    }
    if (count == 0 || count > m->sample_count) {
        return false;
    }
    // the eagleplayer makes the sample pointers even by adding one to all of them when the lowest is odd. It takes the
    // lowest as a signed 32-bit number of the relocated longs: one that does not fit 24 bits is not modelled.
    uint32_t lowest_sample = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t relocated = nuade_read_be32(img + samples + i * 4) - m->origin + WB_UADE_LOAD_ADDRESS;
        if (m->sample_ptr[i] == WB_BAD_POINTER || relocated > WB_ADDRESS_MASK) {
            return false;
        }
        lowest_sample = m->sample_ptr[i] < lowest_sample ? m->sample_ptr[i] : lowest_sample;
    }
    if ((lowest_sample & 1u) != 0) {
        for (uint32_t i = 0; i < count; i++) {
            m->sample_ptr[i]++;
        }
    }
    return check_layout(m, count);
}

// The voice records must be what the port's voice n = Paula channel n assumes.
static bool check_voices(const WbModule* m) {
    const uint32_t voices = m->addr[WbRole_Voices];
    for (uint32_t v = 0; v < WB_VOICES; v++) {
        if (m->image[voices + offsetof(WbVoiceBlock, hw) + v] != v * 16
            || m->image[voices + offsetof(WbVoiceBlock, dma_bit) + v] != (1u << v)) {
            return false;
        }
    }
    return true;
}

// The eagleplayer's relocation: it scans the words of the code for the opcodes that carry an absolute address and adds
// the load address to the long behind each (one opcode has two longs, a long that names a custom chip register is left
// alone). Every long it moves must be an operand the decoder found, and the other way round, or the port and UADE
// disagree about the module's own code.
static bool check_relocation(const WbCode* code, const uint8_t* mod, uint32_t size, uint32_t code_rts,
                             WbBudget* budget) {
    static const uint16_t k_skip4[] = { 0x0079, 0x0239, 0x13E8, 0x13EA, 0x13FC, 0x4CF9 };
    static const uint16_t k_skip2[]
        = { 0x03B9, 0x03F9, 0x1039, 0x1239, 0x13C0, 0x13C4, 0x13C7, 0x13D0, 0x13D3, 0x1839, 0x1C39, 0x1E39,
            0x23C2, 0x23D1, 0x2679, 0x3039, 0x33C0, 0x33C4, 0x33C6, 0x33C5, 0x33D1, 0x3839, 0x3A39, 0x3C39,
            0x41F9, 0x4239, 0x43F9, 0x45F9, 0x4A39, 0x4BF9, 0x4DF9, 0x4EF9, 0x5239, 0x5279, 0x5339, 0x9039,
            0x9079, 0xB039, 0xD039, 0xD079, 0xD3F9, 0xD5F9, 0xDBF9, 0xDDF9, 0xE2F9 };
    static const uint16_t k_two_longs = 0x13F9; // move.b abs,abs
    const uint32_t limit = code_rts + 2;
    if (!wb_budget_take(budget, 2 * (uint64_t)limit)) {
        return false;
    }
    uint8_t* moved = calloc(limit, 1);
    uint8_t* found = calloc(limit, 1);
    bool ok = moved != nullptr && found != nullptr;
    for (uint32_t i = 0; ok && i < code->count; i++) {
        const WbInsn* insn = &code->insn[i];
        for (uint32_t k = 0; k < insn->slot_count; k++) {
            moved[insn->at + insn->slot_at[k]] = 1;
        }
        if (insn->target_kind == WbTarget_Absolute) {
            moved[insn->at + insn->target_at] = 1;
        }
    }
    uint32_t at = 0;
    // the loop ended on a step that matched no opcode: the only place the original tests the end
    bool plain_end = false;
    while (ok && at < code_rts) {
        const uint16_t word = nuade_read_be16(mod + at);
        uint32_t longs = 0; // the longs behind the opcode to consider
        at += 2;
        for (uint32_t i = 0; i < nuade_sizeof_array(k_skip4) && longs == 0; i++) {
            if (word == k_skip4[i]) {
                at += 2; // an opcode with an immediate word in front of the address
                longs = 1;
            }
        }
        for (uint32_t i = 0; i < nuade_sizeof_array(k_skip2) && longs == 0; i++) {
            longs = word == k_skip2[i] ? 1 : 0;
        }
        if (longs == 0 && word == k_two_longs) {
            longs = 2;
        }
        for (uint32_t k = 0; ok && k < longs; k++) {
            if (!in_range(size, at, 4) || at + 4 > limit) {
                ok = false;
            } else if (nuade_read_be16(mod + at) != 0x00DFu) {
                found[at] = 1;
                at += 4;
            } else if (longs == 2 && k == 0) {
                at += 4; // the first long names a register: the second is still looked at
            } else {
                break;
            }
        }
        plain_end = longs == 0 && at == code_rts;
    }
    ok = ok && plain_end && memcmp(moved, found, limit) == 0;
    free(moved);
    free(found);
    return ok;
}

// Decodes the module's code and recognises the build. Everything the eagleplayer does to the code (the scans, the
// relocation, the patches) must agree with what the build's table says.
static bool identify(WbModule* m, const Scan* scan, WbBudget* budget) {
    WbCode code = { 0 };
    uint32_t* values = malloc(sizeof(uint32_t) * WB_MAX_SLOTS);
    bool ok = values != nullptr && wb_budget_take(budget, 16u * (uint64_t)m->code_end)
              && wb_code_decode(&code, m->image, m->size, m->code_end, m->origin)
              && wb_code_insn_at(&code, scan->song_count_at) != nullptr;
    if (ok) {
        // the song count is a parameter of the build: hash with it zeroed
        const uint64_t hash = wb_code_hash(&code, m->image, scan->song_count_at);
        const uint32_t slot_count = wb_code_slots(&code, m->image, values);
        ok = wb_budget_take(budget, (uint64_t)slot_count * slot_count);
        const uint64_t partition = ok ? partition_hash(values, slot_count) : 0;
        for (uint32_t i = 0; ok && i < wb_version_count; i++) {
            if (wb_versions[i].hash == hash && wb_versions[i].partition == partition) {
                m->version = &wb_versions[i];
            }
        }
        uint32_t patches = 0;
        ok = ok && m->version != nullptr;
        if (ok) {
            m->voice_bytes = m->version->quirks.vibrato_flag ? WB_VOICE_RECORD_BYTES : WB_VOICE_RECORD_BYTES - 4;
        }
        ok = ok && bind_roles(m, &code, values, slot_count, scan)
             && check_relocation(&code, m->image, m->size, scan->code_rts, budget)
             && find_patches(m->image, m->code_end, &code, budget, &patches) && patches == m->version->patches;
    }
    wb_code_free(&code);
    free(values);
    return ok;
}

// The eagleplayer repairs the song data of one game. The repair lands in the data: a file whose code region contains
// the words it looks at is refused.
static bool repair_song_data(WbModule* m) {
    if (m->subsong_count != FIX_SONG_COUNT || !in_range(m->size, FIX_LONG_AT, 4) || !in_range(m->size, FIX_WORD_AT, 2)
        || nuade_read_be32(m->image + FIX_LONG_AT) != FIX_LONG_VALUE
        || nuade_read_be16(m->image + FIX_WORD_AT) != FIX_WORD_VALUE) {
        return true;
    }
    if (FIX_LONG_AT < m->code_end || FIX_WORD_AT < m->code_end) {
        return false;
    }
    const uint32_t fixed_long = FIX_LONG_VALUE - FIX_AMOUNT;
    const uint16_t fixed_word = FIX_WORD_VALUE - FIX_AMOUNT;
    m->image[FIX_LONG_AT + 2] = (uint8_t)(fixed_long >> 8);
    m->image[FIX_LONG_AT + 3] = (uint8_t)fixed_long;
    m->image[FIX_WORD_AT] = (uint8_t)(fixed_word >> 8);
    m->image[FIX_WORD_AT + 1] = (uint8_t)fixed_word;
    return true;
}

bool wb_module_load(WbModule* m, const uint8_t* data, size_t size, WbBudget* budget) {
    memset(m, 0, sizeof(*m));
    if (data == nullptr || size > WB_MAX_MODULE_BYTES || !check_header(data, (uint32_t)size)) {
        return false;
    }
    m->size = (uint32_t)size;
    Scan scan;
    if (!scan_module(&scan, data, m->size, budget)) {
        return false;
    }
    m->origin = scan.origin;
    m->song_limit = scan.song_limit;
    m->code_end = scan.code_rts + 2;
    if (scan.song_limit < 2 || scan.song_limit > WB_MAX_SONGS) {
        return false;
    }
    m->subsong_count = scan.song_limit - 1;
    m->image = nuade_paula_padded_copy(data, size);
    const bool ok = m->image != nullptr && identify(m, &scan, budget) && repair_song_data(m)
                    && load_tables(m, &scan, budget) && check_voices(m);
    if (!ok) {
        wb_module_free(m);
    }
    return ok;
}

void wb_module_free(WbModule* m) {
    free(m->image);
    free(m->song_seq);
    free(m->pattern_ptr);
    free(m->sample_ptr);
    memset(m, 0, sizeof(*m));
}

bool wb_module_pattern(const WbModule* m, uint32_t number, uint32_t* stream) {
    *stream = WB_BAD_POINTER;
    if (m->version->quirks.pattern_index_wraps) {
        number = (uint8_t)(number * 4u) / 4u;
    }
    if (number < m->pattern_count) {
        *stream = m->pattern_ptr[number];
        return *stream != WB_BAD_POINTER;
    }
    // behind the relocated table the replayer reads the file's own longs: an address the eagleplayer did not move,
    // unless the long lies in memory the port does not hold the live value of (code, variables, the moved tables). One
    // that lies in the part of the address space UADE has no memory in reads as zeros.
    const uint32_t at = m->addr[WbRole_Patterns] + number * 4;
    uint8_t b[4];
    for (uint32_t i = 0; i < 4; i++) {
        if (!wb_module_data_u8(m, at + i, &b[i])) {
            return false;
        }
    }
    const uint32_t address = nuade_read_be32(b) & WB_ADDRESS_MASK;
    if (address >= WB_UADE_LOAD_ADDRESS + m->size && address < WB_UNMAPPED_HIGH) {
        *stream = WB_ZERO_STREAM;
        return true;
    }
    return false;
}

bool wb_module_data_u8(const WbModule* m, uint32_t at, uint8_t* out) {
    if (at < m->code_end || at >= m->size || (at >= m->var_lo && at < m->var_hi)) {
        return false;
    }
    const uint32_t songs = m->addr[WbRole_Songs];
    const uint32_t patterns_end = m->addr[WbRole_Patterns] + m->pattern_count * 4;
    const uint32_t samples = m->addr[WbRole_Samples];
    const uint32_t samples_end = samples + m->sample_count * 4;
    if ((at >= songs && at < patterns_end) || (at >= samples && at < samples_end)
        || (m->has[WbRole_Silence] && at >= m->addr[WbRole_Silence] && at < m->addr[WbRole_Silence] + 4)) {
        return false;
    }
    *out = m->image[at];
    return true;
}

static bool read_record(const WbModule* m, uint32_t at, uint8_t* out, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes; i++) {
        if (!wb_module_data_u8(m, at + i, &out[i])) {
            return false;
        }
    }
    return true;
}

bool wb_module_instrument(const WbModule* m, uint8_t wave, WbInstrument* out) {
    uint8_t b[INSTRUMENT_BYTES];
    if (!read_record(m, m->addr[WbRole_Instr] + wave, b, INSTRUMENT_BYTES)) {
        return false;
    }
    *out = (WbInstrument) { .sample = b[0],
                            .flags = b[1],
                            .length = nuade_read_be16(b + 2),
                            .loop_offset = nuade_read_be16(b + 4),
                            .loop_length = nuade_read_be16(b + 6) };
    return true;
}

bool wb_module_envelope(const WbModule* m, uint8_t env_sel, WbEnvelope* out) {
    uint8_t b[ENVELOPE_BYTES];
    if (!read_record(m, m->addr[WbRole_Envs] + env_sel, b, ENVELOPE_BYTES)) {
        return false;
    }
    *out = (WbEnvelope) { .volume = b[0],
                          .target = b[1],
                          .attack = b[2],
                          .release = b[3],
                          .sustain = b[4],
                          .vibrato = b[5],
                          .arp_mask = b[6],
                          .flags = b[7] };
    return true;
}

bool wb_module_arp_byte(const WbModule* m, uint32_t index, uint8_t* out) {
    if (index >= ARP_TABLE_BYTES || !in_range(m->size, m->addr[WbRole_ArpTable], ARP_TABLE_BYTES)) {
        return false;
    }
    *out = m->image[m->addr[WbRole_ArpTable] + index];
    return true;
}

bool wb_module_period(const WbModule* m, uint32_t byte_index, uint16_t* out) {
    if (!in_range(m->period_count * 2, byte_index, 2)) {
        return false;
    }
    *out = nuade_read_be16(m->image + m->addr[WbRole_Periods] + byte_index);
    return true;
}
