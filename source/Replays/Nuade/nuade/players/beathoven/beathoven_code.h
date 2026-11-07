// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: 68k instruction words, voice record offsets and the pattern matcher that the replayer
// locator and the song decoder share. They read the module's code to find what the replayers do; none of it runs.

#pragma once

#include "beathoven_internal.h"
#include "nuade_utils.h"

#define NOT_FOUND 0xFFFFFFFFu

// Instruction words. A name says what the instruction does with a0 = the voice record, a1 = the channel's hardware
// registers, d0 and d1 = scratch, as the replayers use them.

// Address loads and moves
#define OP_LEA_ABS_A0 0x41F9   // lea.l address.l,a0
#define OP_LEA_PC_A0 0x41FA    // lea.l d16(pc),a0
#define OP_LEA_ABS_A1 0x43F9   // lea.l address.l,a1
#define OP_LEA_PC_A1 0x43FA    // lea.l d16(pc),a1
#define OP_LEA_ABS_A2 0x45F9   // lea.l address.l,a2
#define OP_LEA_PC_A2 0x45FA    // lea.l d16(pc),a2
#define OP_LEA_ABS_A3 0x47F9   // lea.l address.l,a3
#define OP_LEA_PC_A4 0x49FA    // lea.l d16(pc),a4
#define OP_LEA_ABS_A4 0x49F9   // lea.l address.l,a4
#define OP_LEA_ABS_A6 0x4DF9   // lea.l address.l,a6
#define OP_MOVEA_PC_A0 0x207A  // movea.l d16(pc),a0
#define OP_MOVEA_A0_A2 0x2448  // movea.l a0,a2
#define OP_MOVEA_REC_A1 0x2268 // movea.l d16(a0),a1
#define OP_MOVEA_IND_A1 0x2250 // movea.l (a0),a1
#define OP_MOVEA_D1_A1 0x2241  // movea.l d1,a1
#define OP_MOVEA_D2_A2 0x2442  // movea.l d2,a2
#define OP_MOVEM_PUSH 0x48E7   // movem.l <list>,-(a7)
#define OP_MOVEM_POP 0x4CDF    // movem.l (a7)+,<list>
#define OP_MOVEM_ABS 0x4CF9    // movem.l address.l,<list>
#define MOVEM_A3_A6 0x7800
#define MOVEM_D3_D6 0x0078
#define MOVEM_PUSH_D0_A6 0xFFFE // d0-d7/a0-a6 in the predecrement form
#define MOVEM_POP_D0_A6 0x7FFF
#define OP_RTS 0x4E75

// Moves of longwords
#define OP_MOVE_L_IMM_REC 0x217C       // move.l #imm,d16(a0)
#define OP_MOVE_L_IMM_A0 0x20BC        // move.l #imm,(a0)
#define OP_MOVE_L_PC_REC 0x217A        // move.l d16(pc),d16(a0)
#define OP_MOVE_L_PC_IND_A0 0x20BA     // move.l d16(pc),(a0)
#define OP_MOVE_L_PC_D0 0x203A         // move.l d16(pc),d0
#define OP_MOVE_L_IMM_D0 0x203C        // move.l #imm,d0
#define OP_MOVE_L_IMM_D7 0x2E3C        // move.l #imm,d7
#define OP_MOVE_L_INC_A0_D1 0x2218     // move.l (a0)+,d1
#define OP_MOVE_L_INC_A0_D2 0x2418     // move.l (a0)+,d2
#define OP_MOVE_L_INC_A0_A1 0x22D8     // move.l (a0)+,(a1)+
#define OP_MOVE_L_INC_A3_D0 0x201B     // move.l (a3)+,d0
#define OP_MOVE_L_A4_INC_A3 0x26CC     // move.l a4,(a3)+
#define OP_MOVE_L_D2_REC_A1 0x2342     // move.l d2,d16(a1)
#define OP_MOVE_L_A2_IND_REC_A1 0x2352 // move.l (a2),d16(a1)
#define OP_MOVE_L_REC_A2_D0 0x202A     // move.l d16(a2),d0
#define OP_MOVE_L_IMM_REC_A1 0x237C    // move.l #imm,d16(a1): the silent sample's address
#define OP_MOVE_L_IMM_A1 0x22BC        // move.l #imm,(a1)
#define OP_MOVE_L_PC_REC_A1 0x237A     // move.l d16(pc),d16(a1)
#define OP_MOVE_L_PC_A1 0x22BA         // move.l d16(pc),(a1)
#define OP_MOVE_L_IMM_IND_A6 0x2CBC    // move.l #imm,(a6)
#define OP_MOVE_L_D0_D1 0x2200         // move.l d0,d1
#define OP_MOVEA_L_4_A0_D1_A1 0x2270   // movea.l 4(a0,d1.l),a1 (followed by its extension word)
#define OP_MOVEA_L_A0_D1_A0 0x2070     // movea.l (a0,d1.l),a0 (followed by its extension word)

// Moves of words
#define OP_MOVE_W_IMM_ABS 0x33FC       // move.w #imm,address.l
#define OP_MOVE_W_D0_ABS 0x33C0        // move.w d0,address.l
#define OP_MOVE_W_IMM_A0 0x30BC        // move.w #imm,(a0)
#define OP_MOVE_W_IMM_A0_REC 0x317C    // move.w #imm,d16(a0)
#define OP_MOVE_W_IMM_REC_A6 0x3D7C    // move.w #imm,d16(a6)
#define OP_MOVE_W_REC_A2_REC_A1 0x336A // move.w d16(a2),d16(a1)
#define OP_MOVE_W_INC_A0_ABS 0x33D8    // move.w (a0)+,address.l
#define OP_MOVE_W_IMM_D0 0x303C        // move.w #imm,d0
#define OP_MOVE_W_INC_A0_A1 0x32D8     // move.w (a0)+,(a1)+
#define OP_MOVE_B_FLAGS_D0 0x1028      // move.b d16(a0),d0
#define OP_MOVEQ_7_D0 0x7007

// Tests, clears and arithmetic
#define OP_TST_ABS_W 0x4A79  // tst.w address.l
#define OP_TST_W_A0 0x4A50   // tst.w (a0)
#define OP_TST_W_D0 0x4A40   // tst.w d0
#define OP_TST_L_D2 0x4A82   // tst.l d2
#define OP_CLR_W_ABS 0x4279  // clr.w address.l
#define OP_CLR_D2 0x4202     // clr.b d2
#define OP_BTST_REC 0x0828   // btst.b #bit,d16(a0)
#define OP_ANDI_B_D0 0x0200  // andi.b #imm,d0
#define OP_ANDI_L_D0 0x0280  // andi.l #imm,d0
#define OP_SUBQ_1_D0 0x5340  // subq.w #1,d0
#define OP_CMPI_W_D0 0x0C40  // cmpi.w #imm,d0
#define OP_CMP_REC_D0 0xB068 // cmp.w d16(a0),d0
#define OP_ADD_REC_D0 0xD068 // add.w d16(a0),d0
#define OP_ADDI_W_D0 0x0640  // addi.w #imm,d0
#define OP_ADD_PC_D0 0xD07A  // add.w d16(pc),d0
#define OP_ADD_ABS_D0 0xD079 // add.w address.l,d0
#define OP_ADDQ_W_D0 0x5040  // addq.w #n,d0 (n in bits 9..11)
#define OP_ADDQ_W_D0_MASK 0xF1FF
#define OP_ADD_L_PC_D0 0xD0BA   // add.l d16(pc),d0
#define OP_ADD_L_REC_D0 0xD0A8  // add.l d16(a0),d0
#define OP_ADD_L_D7_D0 0xD087   // add.l d7,d0
#define OP_ADD_L_REC_D1 0xD2A8  // add.l d16(a0),d1
#define OP_ADD_L_D7_D1 0xD287   // add.l d7,d1
#define OP_ADD_L_D0_D2 0xD480   // add.l d0,d2
#define OP_ADDA_L_D0_A0 0xD1C0  // adda.l d0,a0
#define OP_ADDA_L_D0_A4 0xD9C0  // adda.l d0,a4
#define OP_ADDA_W_IMM_A1 0xD2FC // adda.w #imm,a1
#define OP_SUBI_L_D0 0x0480     // subi.l #imm,d0
#define OP_SUBI_L_D1 0x0481     // subi.l #imm,d1
#define OP_MULU_IMM_D0 0xC0FC   // mulu.w #imm,d0
#define OP_MULU_PC_D0 0xC0FA
#define OP_MULU_ABS_D0 0xC0F9
#define OP_MULU_IMM_D1 0xC2FC // mulu.w #imm,d1
#define OP_LSL_W_1_D0 0xE348  // lsl.w #1,d0
#define OP_LSL_L_3_D1 0xE789  // lsl.l #3,d1
#define OP_LSR_W_2_D0 0xE448  // lsr.w #2,d0
#define OP_DBRA_D0 0x51C8     // dbra d0,<displacement>

// Branches. The kind is the opcode word with a zero displacement; a displacement of 0 means a word follows.
#define OP_BRANCH_MASK 0xFF00
#define OP_BRA 0x6000
#define OP_BSR 0x6100
#define OP_BNE 0x6600
#define OP_BEQ 0x6700
#define OP_BGE 0x6C00
#define OP_BLT 0x6D00
#define OP_BGT 0x6E00
#define OP_BLE 0x6F00

// Voice record fields (offsets from a0) that the locator and the song decoder test for; the replayer's own use of
// them is in the voice routines

#define RECORD_TRACK 0x0004
#define RECORD_LOOP 0x0008
#define RECORD_FLAGS 0x000E
#define RECORD_NOTE_BASE 0x0016
#define RECORD_TARGET_PERIOD 0x0018
#define RECORD_SLIDE_STEP 0x001C
#define RECORD_NOTE_BIAS 0x0032
// BEATHOVEN100
#define RECORD_LEGACY_TRACK 0x000A
#define RECORD_LEGACY_LOOP 0x000E
#define RECORD_LEGACY_NOTE_BASE 0x0012

// Patterns of instruction words. BTH_ANY matches any word (an operand).

#define BTH_ANY 0x10000u

// Whether the `count` words at `at` are those of `pattern`
static inline bool bthm_match(const BthModule* m, uint32_t at, const uint32_t* pattern, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (pattern[i] != BTH_ANY && pattern[i] != bthm_u16(m, at + 2 * i)) {
            return false;
        }
    }
    return true;
}

// The first even address in [from, to) where the words of `pattern` follow one another, NOT_FOUND if none
static inline uint32_t bthm_find(const BthModule* m, uint32_t from, uint32_t to, const uint32_t* pattern,
                                 uint32_t count) {
    for (uint32_t at = (from + 1) & ~1u; at < to && at + 2 * count <= m->size; at += 2) {
        if (bthm_match(m, at, pattern, count)) {
            return at;
        }
    }
    return NOT_FOUND;
}

#define BTH_MATCH(m, at, pattern) bthm_match((m), (at), (pattern), nuade_sizeof_array(pattern))
#define BTH_FIND(m, from, to, pattern) bthm_find((m), (from), (to), (pattern), nuade_sizeof_array(pattern))

// Whether the word at `at` is a branch of the given kind
static inline bool bthm_is_branch(const BthModule* m, uint32_t at, uint16_t kind) {
    const uint16_t op = bthm_u16(m, at);
    return (op & OP_BRANCH_MASK) == kind && (op & 0xFF) != 0xFF;
}

// The target of the branch at `at` and the address after it
static inline uint32_t bthm_branch_target(const BthModule* m, uint32_t at, uint32_t* next) {
    const int8_t displacement = (int8_t)(bthm_u16(m, at) & 0xFF);
    if (displacement == 0) {
        *next = at + 4;
        return bthm_pc16(m, at + 2);
    }
    *next = at + 2;
    return at + 2 + (uint32_t)(int32_t)displacement;
}
