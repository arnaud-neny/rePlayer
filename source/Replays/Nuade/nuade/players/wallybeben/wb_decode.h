// SPDX-License-Identifier: MIT
// Wally Beben: the replayer's code as a module carries it, decoded far enough to tell versions apart.
//
// The module starts with the replayer: `bra.w` to the reset routine at +0 and the tick wrapper at +4. The decoder
// follows both entries through the branches of the 68000 subset the replayers use and records, per instruction, where
// its absolute data operands and its branch targets are.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WB_MAX_INSNS 4096u
#define WB_MAX_SLOTS 2048u

// FNV-1a, 64 bit: the hash of the decoded code and of the operand partition
#define WB_FNV_OFFSET 0xCBF29CE484222325ULL
#define WB_FNV_PRIME 0x100000001B3ULL

typedef enum WbTargetKind {
    WbTarget_None,
    WbTarget_Relative, // a branch: the displacement is part of the instruction
    WbTarget_Absolute, // jmp / jsr to an absolute address
} WbTargetKind;

typedef struct WbInsn {
    uint32_t at;        // offset in the module
    uint8_t len;        // bytes
    uint8_t slot_count; // absolute data operands (custom chip addresses are not data)
    uint8_t slot_at[2]; // their byte offsets in the instruction
    uint8_t target_kind;
    uint8_t target_at; // byte offset of the displacement word (relative, 4 byte instructions) or of the address
    uint32_t target;   // offset in the module (an absolute jump: the operand, before the origin is removed)
} WbInsn;

typedef struct WbCode {
    WbInsn* insn; // in address order
    uint32_t count;
    uint16_t* index_at; // [limit]: instruction index + 1 at each instruction start, 0 elsewhere
    uint32_t limit;     // the code lies in [0, limit)
} WbCode;

// Decodes the code reachable from the entries at 0 and 4 inside [0, limit); `origin` is the address the module was
// assembled at (an absolute jump leaves it). False for anything outside the subset or a jump out of the code.
bool wb_code_decode(WbCode* code, const uint8_t* mod, uint32_t size, uint32_t limit, uint32_t origin);
void wb_code_free(WbCode* code);

// FNV-1a over the instructions with their absolute data operands zeroed and branch targets replaced by the index of
// the target instruction. The instruction at `masked_at` (a `move.l #n,d1`) has its immediate zeroed too.
uint64_t wb_code_hash(const WbCode* code, const uint8_t* mod, uint32_t masked_at);

// The absolute data operands in code order, as the module has them (the origin is still in them). `out` holds
// WB_MAX_SLOTS; returns the count.
uint32_t wb_code_slots(const WbCode* code, const uint8_t* mod, uint32_t* out);

// The instruction that starts at `at`, or null.
const WbInsn* wb_code_insn_at(const WbCode* code, uint32_t at);
