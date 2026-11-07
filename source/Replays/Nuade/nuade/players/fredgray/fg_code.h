// SPDX-License-Identifier: MIT
// Fred Gray player: reading the module's 68000 code. The instruction words the locator and the section decoder look
// for, the instruction decoder, and queries over a routine's instructions. None of it runs the code.

#pragma once

#include "fg_internal.h"

// Instruction words. A name says what the instruction does; "ABS" is an absolute long address operand, "IMM" an
// immediate, "REC" d16(a6) with a6 = the voice records.

// Control flow
#define OP_RTS 0x4e75 // rts
#define OP_RTE 0x4e73 // rte
#define OP_RTR 0x4e77 // rtr
#define OP_JMP_JSR_MASK 0xffc0
#define OP_JMP 0x4ec0      // jmp <ea>
#define OP_JSR 0x4e80      // jsr <ea>
#define OP_MASK_BCC 0xf000 // bra, bsr and the Bcc family share the high nibble 6
#define OP_BCC 0x6000
#define OP_COND_SHIFT 8 // the condition code of a Bcc
#define OP_COND_MASK 0xf
#define COND_TRUE 0x0       // bra
#define COND_SUBROUTINE 0x1 // bsr
#define COND_FIRST_CONDITIONAL 0x2
#define COND_CARRY_CLEAR 0x4
#define COND_CARRY_SET 0x5
#define COND_NOT_EQUAL 0x6
#define COND_EQUAL 0x7
#define COND_PLUS 0xa
#define OP_DBCC_MASK 0xf0f8
#define OP_DBCC 0x50c8

// Address and register loads
#define OP_LEA_ABS_A0 0x41f9     // lea.l address.l,a0
#define OP_LEA_ABS_A1 0x43f9     // lea.l address.l,a1
#define OP_LEA_ABS_A5 0x4bf9     // lea.l address.l,a5
#define OP_LEA_ABS_A6 0x4df9     // lea.l address.l,a6
#define OP_MOVEA_IMM_A0 0x207c   // movea.l #imm,a0
#define OP_MOVEA_IMM_A1 0x227c   // movea.l #imm,a1
#define OP_MOVEA_IMM_A6 0x2c7c   // movea.l #imm,a6
#define OP_MOVE_L_IMM_D0 0x203c  // move.l #imm,d0
#define OP_MOVE_L_IMM_D6 0x2c3c  // move.l #imm,d6
#define OP_MOVE_W_IMM_D0 0x303c  // move.w #imm,d0
#define OP_MOVE_W_IMM_D1 0x323c  // move.w #imm,d1
#define OP_MOVE_B_IMM_D0 0x103c  // move.b #imm,d0
#define OP_MOVE_B_IMM_D1 0x123c  // move.b #imm,d1
#define OP_MOVEQ_REG_MASK 0xff00 // moveq #n,d1 is 0x72nn, d2 0x74nn, d6 0x7cnn
#define OP_MOVEQ_D1 0x7200
#define OP_MOVEQ_D2 0x7400
#define OP_MOVEQ_D6 0x7c00
#define OP_CLR_W_D2 0x4242 // clr.w d2
#define OP_CLR_W_D6 0x4246 // clr.w d6
#define OP_CLR_L_D4 0x4284 // clr.l d4
#define OP_CLR_L_D6 0x4286 // clr.l d6
#define OP_CLR_L_D7 0x4287 // clr.l d7
#define OP_CLR_W_D0 0x4240 // clr.w d0

// Loads and stores of the variables
#define OP_MOVE_B_ABS_D0 0x1039    // move.b address.l,d0
#define OP_MOVE_B_ABS_D1 0x1239    // move.b address.l,d1
#define OP_MOVE_B_ABS_D2 0x1439    // move.b address.l,d2
#define OP_MOVE_B_ABS_D5 0x1a39    // move.b address.l,d5
#define OP_MOVE_W_ABS_D0 0x3039    // move.w address.l,d0
#define OP_MOVE_W_ABS_D1 0x3239    // move.w address.l,d1
#define OP_MOVE_W_ABS_D2 0x3439    // move.w address.l,d2
#define OP_MOVE_W_ABS_D7 0x3e39    // move.w address.l,d7
#define OP_MOVE_B_D0_ABS 0x13c0    // move.b d0,address.l
#define OP_MOVE_B_D1_ABS 0x13c1    // move.b d1,address.l
#define OP_MOVE_W_D0_ABS 0x33c0    // move.w d0,address.l
#define OP_MOVE_W_D1_ABS 0x33c1    // move.w d1,address.l
#define OP_MOVE_W_D7_ABS 0x33c7    // move.w d7,address.l
#define OP_MOVE_B_IMM_ABS 0x13fc   // move.b #imm,address.l
#define OP_MOVE_W_IMM_ABS 0x33fc   // move.w #imm,address.l
#define OP_MOVE_L_IMM_ABS 0x23fc   // move.l #imm,address.l
#define OP_MOVE_B_PCIDX_ABS 0x13fb // move.b d8(pc,d0.w),address.l
#define OP_CLR_B_ABS 0x4239        // clr.b address.l
#define OP_TST_B_ABS 0x4a39        // tst.b address.l
#define OP_TST_W_ABS 0x4a79        // tst.w address.l
#define OP_CLR_W_ABS 0x4279        // clr.w address.l
#define OP_MOVE_L_IMM_KICK 0x2bbc  // move.l #imm,(a5,d3.w)

// d16(a6): the voice records in the sections, the custom chip registers in the init routine
#define OP_MOVE_L_A0_D16A6 0x2d48    // move.l a0,d16(a6)
#define OP_MOVE_W_IMM_D16A6 0x3d7c   // move.w #imm,d16(a6)
#define OP_CLR_W_D16A6 0x426e        // clr.w d16(a6)
#define OP_MOVE_B_IMM_D16A6 0x1d7c   // move.b #imm,d16(a6)
#define OP_MOVE_B_TABLE_D16A6 0x1d70 // move.b (a0,d7.w),d16(a6)
#define TABLE_INDEX_D7_W 0x7000      // the extension word of (a0,d7.w)

// The step (d7) and the arithmetic around it
#define OP_ANDI_W_D7 0x0247     // andi.w #imm,d7
#define OP_ANDI_W_D2 0x0242     // andi.w #imm,d2
#define OP_ANDI_W_D4 0x0244     // andi.w #imm,d4
#define OP_TST_W_D7 0x4a47      // tst.w d7
#define OP_TST_B_D7 0x4a07      // tst.b d7
#define OP_CMPI_W_D7 0x0c47     // cmpi.w #imm,d7
#define OP_BTST_B_D7 0x0807     // btst.b #n,d7
#define OP_BTST_B_D4 0x0804     // btst.b #n,d4
#define OP_LSR_W_D7_MASK 0xf1ff // lsr.w #n,d7 is 0xe04f | n << 9
#define OP_LSR_W_D7 0xe04f
#define OP_LSR_COUNT_SHIFT 9 // the count field; 0 means 8
#define OP_LSR_COUNT_MASK 7
#define OP_LSR_COUNT_ZERO_MEANS 8
#define OP_ASL_L_D6 0xe586     // asl.l #2,d6
#define OP_ADDQ_B_D0 0x5200    // addq.b #1,d0
#define OP_ADD_W_ABS_D4 0xd879 // add.w address.l,d4
#define OP_ADD_W_D2_D4 0xd842  // add.w d2,d4
#define OP_ADD_W_A1_D4 0xd849  // add.w a1,d4
#define OP_ADDI_L_D1 0x0681    // addi.l #imm,d1
#define OP_ADDI_W_D0 0x0640    // addi.w #imm,d0
#define OP_EORI_W_D0 0x0a40    // eori.w #imm,d0
#define OP_ADDI_B_D0 0x0600    // addi.b #imm,d0
#define OP_ANDI_B_D0 0x0200    // andi.b #imm,d0
#define OP_LSR_B_D0_4 0xe808   // lsr.b #4,d0
#define OP_MOVE_B_D7_D0 0x1007 // move.b d7,d0

// The status register moves that the eagleplayer removes
#define OP_MOVE_SR_PUSH 0x40e7 // move.w sr,-(a7)
#define OP_MOVE_SR_POP 0x46df  // move.w (a7)+,sr
#define OP_BNE_HIGH 0x66       // high byte of bne

// The custom chip registers the module writes through absolute addresses or a6: the base and the audio channels
// (channel n is n * CUSTOM_CHANNEL_STRIDE on), and DMACON

#define CUSTOM_BASE 0xdff000u
#define CUSTOM_AUD0LC 0xdff0a0u
#define CUSTOM_AUD0LEN 0xdff0a4u
#define CUSTOM_AUD0PER 0xdff0a6u
#define CUSTOM_AUD0VOL 0xdff0a8u
#define CUSTOM_DMACON 0xdff096u
#define CUSTOM_CHANNEL_STRIDE 0x10u
// DMACON write: set bit plus the three audio channels and the master enable
#define DMACON_AUDIO_ON 0x8007

// The voice that `value` addresses when it is `base` + voice * stride; false if it is none of the voices
bool fg_voice_of_offset(uint32_t value, uint32_t base, uint32_t stride, uint8_t* voice);

// Instruction decoding, as far as the locator needs it: the length, and where control goes

typedef enum FgFlow {
    FgFlow_Next = 0, // continues with the next instruction
    FgFlow_Branch,   // conditional branch: both ways
    FgFlow_Call,     // bsr/jsr: continues after the call, and the target is a routine
    FgFlow_Jump,     // bra/jmp to `target`: does not continue
    FgFlow_Stop,     // rts, or a jump through a register
} FgFlow;

typedef struct FgInsn {
    uint32_t length; // bytes; 0 for a word the decoder does not know or that is not inside the module
    FgFlow flow;
    uint32_t target; // Branch, Call, Jump
} FgInsn;

FgInsn fg_decode(const FgModule* m, uint32_t at);

// Instruction cursor over the code: reads the words at `at` and moves on

typedef struct FgCursor {
    const FgModule* m;
    uint32_t at;
} FgCursor;

uint16_t fg_cursor_peek(const FgCursor* c);
// Consumes the word if it is `word`
bool fg_cursor_take(FgCursor* c, uint16_t word);
uint16_t fg_cursor_u16(FgCursor* c);
uint32_t fg_cursor_u32(FgCursor* c);
// A call instruction: consumes it and gives its target
bool fg_cursor_call(FgCursor* c, uint32_t* target);

// `lea.l abs,a0` or `movea.l #abs,a0`: the load of a table address into the first table register
bool fg_is_table_load(uint16_t word);
// The load of the voice offset into d6 at `at` (clr.l, moveq or move.l): its value and length in bytes
bool fg_read_voice_offset_load(const FgModule* m, uint32_t at, uint32_t* value, uint32_t* length);

// Walking the code: the instructions reachable from a routine, by branches (a routine) or also by calls (the engine)

typedef struct FgWalk {
    const FgModule* m;
    uint8_t* seen; // one byte per code word: 1 where an instruction starts
    bool follow_calls;
} FgWalk;

bool fg_walk_init(FgWalk* w, const FgModule* m, bool follow_calls);
void fg_walk_free(FgWalk* w);
// Marks every instruction reachable from `root`; false if the flow runs into a word that is not an instruction
bool fg_walk_from(FgWalk* w, uint32_t root);
bool fg_walk_has(const FgWalk* w, uint32_t at);

// One routine: the instructions reachable from its start by branches. Queries find an instruction by its first word.
typedef struct FgRoutine {
    FgWalk walk;
    bool open;
} FgRoutine;

bool fg_routine_open(FgRoutine* r, const FgModule* m, uint32_t start);
void fg_routine_close(FgRoutine* r);
// The address of the `nth` (0-based) instruction that starts with `opcode`, FG_NOT_FOUND if there is none
uint32_t fg_routine_find(const FgRoutine* r, uint16_t opcode, uint32_t nth);
// The long after that instruction's opcode (an absolute address or an immediate), FG_NOT_FOUND if there is none
uint32_t fg_routine_long(const FgRoutine* r, uint16_t opcode, uint32_t nth);
// The word after that instruction's opcode
uint32_t fg_routine_word(const FgRoutine* r, uint16_t opcode, uint32_t nth);
// The first call target, by address
uint32_t fg_routine_first_call(const FgRoutine* r);
// The address the routine loads first into the first (or second) table register
uint32_t fg_routine_table(const FgRoutine* r, bool second);

// One-shot forms of the queries for a routine at `start`
uint32_t fg_find(const FgModule* m, uint32_t start, uint16_t opcode, uint32_t nth);
uint32_t fg_find_long(const FgModule* m, uint32_t start, uint16_t opcode, uint32_t nth);

// The first address from `from` on where `opcode` stands and, `gap` bytes after the next word, `tail` (gap 0: no tail)
uint32_t fg_scan_code(const FgModule* m, uint32_t from, uint16_t opcode, uint32_t gap, uint16_t tail);
