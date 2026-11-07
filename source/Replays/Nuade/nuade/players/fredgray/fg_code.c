// SPDX-License-Identifier: MIT
// Fred Gray player: reading the module's 68000 code (see fg_code.h).

#include "fg_code.h"
#include <stdlib.h>
#include <string.h>

#define MAX_WALK_STACK 4096

// Instruction decoding

// Extension words of an effective address whose own extension words start at `at`; -1 for a mode that does not exist
// on the 68000 (the full extension word of the 68020 included). `immediate` is the words of a #imm operand.
static int ea_words(const FgModule* m, uint32_t at, unsigned mode, unsigned reg, int immediate) {
    switch (mode) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            return 0;
        case 5:
            return 1;
        case 6:
            return (fg_u16(m, at) & 0x100) == 0 ? 1 : -1;
        default:
            break;
    }
    switch (reg) {
        case 0:
            return 1;
        case 1:
            return 2;
        case 2:
            return 1;
        case 3:
            return (fg_u16(m, at) & 0x100) == 0 ? 1 : -1;
        case 4:
            return immediate;
        default:
            return -1;
    }
}

// Words of an immediate operand of the size field (bits 7-6): byte and word take one, long takes two
static int immediate_words(unsigned size) {
    return size == 2 ? 2 : 1;
}

// The length in bytes of the instruction at `at`, 0 if it is not a 68000 instruction
static uint32_t instruction_length(const FgModule* m, uint32_t at) {
    const uint16_t w = fg_u16(m, at);
    const unsigned mode = (w >> 3) & 7;
    const unsigned reg = w & 7;
    const unsigned size = (w >> 6) & 3;
    int ext = 0;
    switch (w >> 12) {
        case 0x0: {
            if ((w & 0x0100) != 0) {
                if (mode == 1) {
                    return 4; // movep
                }
                ext = ea_words(m, at + 2, mode, reg, 1);
            } else if ((w & 0x0f00) == 0x0800) {
                ext = ea_words(m, at + 4, mode, reg, 1);
                ext = ext < 0 ? ext : ext + 1;
            } else if (size == 3) {
                return 0;
            } else if (mode == 7 && reg == 4) {
                return 4; // ori, andi, eori to ccr or sr
            } else {
                const int imm = immediate_words(size);
                ext = ea_words(m, at + 2 + 2 * (uint32_t)imm, mode, reg, imm);
                ext = ext < 0 ? ext : ext + imm;
            }
            break;
        }
        case 0x1:
        case 0x2:
        case 0x3: {
            const unsigned line = w >> 12;
            const int imm = line == 2 ? 2 : 1;
            const unsigned dst_mode = (w >> 6) & 7;
            const unsigned dst_reg = (w >> 9) & 7;
            const int src = ea_words(m, at + 2, mode, reg, imm);
            if (src < 0 || (dst_mode == 1 && line == 1) || (dst_mode == 7 && dst_reg > 1)) {
                return 0;
            }
            const int dst = ea_words(m, at + 2 + 2 * (uint32_t)src, dst_mode, dst_reg, 0);
            ext = dst < 0 ? dst : src + dst;
            break;
        }
        case 0x4: {
            if ((w & 0xfff0) == 0x4e40 || w == 0x4e71 || w == 0x4e70 || w == OP_RTE || w == OP_RTS || w == 0x4e76
                || w == OP_RTR || (w & 0xfff8) == 0x4e58 || (w & 0xfff8) == 0x4e60 || (w & 0xfff8) == 0x4840
                || (w & 0xfff8) == 0x4880 || (w & 0xfff8) == 0x48c0) {
                return 2; // trap, nop, reset, rte, rts, trapv, rtr, unlk, move usp, swap, ext
            }
            if ((w & 0xfff8) == 0x4e50 || w == 0x4e72) {
                return 4;
            }
            if ((w & 0xfb80) == 0x4880) {
                ext = ea_words(m, at + 4, mode, reg, 0);
                ext = ext < 0 ? ext : ext + 1;
            } else if ((w & 0xf1c0) == 0x41c0 || (w & OP_JMP_JSR_MASK) == OP_JSR || (w & OP_JMP_JSR_MASK) == OP_JMP
                       || (w & 0xffc0) == 0x4840) {
                ext = ea_words(m, at + 2, mode, reg, 0);
            } else if ((w & 0xf1c0) == 0x4180) {
                ext = ea_words(m, at + 2, mode, reg, 1);
            } else if ((w & 0xf9c0) == 0x40c0 || (w & 0xffc0) == 0x4ac0) {
                ext = ea_words(m, at + 2, mode, reg, 1);
            } else if ((w & 0xf000) == 0x4000 && (w & 0x0f00) <= 0x0a00 && (w & 0x0100) == 0 && size != 3) {
                ext = ea_words(m, at + 2, mode, reg, immediate_words(size));
            } else {
                return 0;
            }
            break;
        }
        case 0x5:
            if (size == 3 && mode == 1) {
                return 4; // dbcc
            }
            ext = ea_words(m, at + 2, mode, reg, -1);
            break;
        case 0x6: {
            const unsigned displacement = w & 0xff;
            return displacement == 0 ? 4 : displacement == 0xff ? 0 : 2;
        }
        case 0x7:
            return (w & 0x0100) == 0 ? 2 : 0;
        case 0x8:
        case 0x9:
        case 0xb:
        case 0xc:
        case 0xd: {
            const unsigned line = w >> 12;
            const bool bit8 = (w & 0x0100) != 0;
            if (size != 3 && ((line == 0x8 || line == 0xc) && (w & 0x01f0) == 0x0100)) {
                return 2; // sbcd, abcd
            }
            if (size != 3 && (line == 0x9 || line == 0xd) && (w & 0x0130) == 0x0100) {
                return 2; // subx, addx
            }
            if (line == 0xc && bit8 && ((w & 0xf8) == 0x40 || (w & 0xf8) == 0x48 || (w & 0xf8) == 0x88)) {
                return 2; // exg
            }
            if (size != 3 && line == 0xb && (w & 0x0138) == 0x0108) {
                return 2; // cmpm
            }
            int imm = immediate_words(size);
            if (size == 3) {
                imm = (line == 0x9 || line == 0xb || line == 0xd) && bit8 ? 2 : 1;
            }
            ext = ea_words(m, at + 2, mode, reg, imm);
            break;
        }
        case 0xe:
            if (size != 3) {
                return 2;
            }
            ext = ea_words(m, at + 2, mode, reg, 0);
            break;
        default:
            return 0;
    }
    return ext < 0 ? 0 : 2 + 2 * (uint32_t)ext;
}

FgInsn fg_decode(const FgModule* m, uint32_t at) {
    FgInsn insn = { .length = 0, .flow = FgFlow_Next, .target = 0 };
    if ((at & 1) != 0 || !fg_in_module(m, at, 2)) {
        return insn;
    }
    const uint16_t w = fg_u16(m, at);
    insn.length = instruction_length(m, at);
    if (insn.length == 0 || !fg_in_module(m, at, insn.length)) {
        insn.length = 0;
        return insn;
    }
    if ((w & OP_MASK_BCC) == OP_BCC) {
        const unsigned condition = (w >> OP_COND_SHIFT) & OP_COND_MASK;
        const int32_t displacement = (w & 0xff) == 0 ? (int16_t)fg_u16(m, at + 2) : (int8_t)(w & 0xff);
        insn.target = at + 2 + (uint32_t)displacement;
        insn.flow = condition == COND_TRUE ? FgFlow_Jump : condition == COND_SUBROUTINE ? FgFlow_Call : FgFlow_Branch;
    } else if ((w & OP_DBCC_MASK) == OP_DBCC) {
        insn.target = at + 2 + (uint32_t)(int32_t)(int16_t)fg_u16(m, at + 2);
        insn.flow = FgFlow_Branch;
    } else if (w == OP_RTS || w == OP_RTE || w == OP_RTR) {
        insn.flow = FgFlow_Stop;
    } else if ((w & OP_JMP_JSR_MASK) == OP_JMP || (w & OP_JMP_JSR_MASK) == OP_JSR) {
        const bool call = (w & OP_JMP_JSR_MASK) == OP_JSR;
        bool known = true;
        switch (w & 0x3f) {
            case 0x39: // abs.l
                insn.target = fg_u32(m, at + 2);
                break;
            case 0x38: // abs.w
                insn.target = (uint32_t)(int32_t)(int16_t)fg_u16(m, at + 2);
                break;
            case 0x3a: // d16(pc)
                insn.target = at + 2 + (uint32_t)(int32_t)(int16_t)fg_u16(m, at + 2);
                break;
            default:
                known = false;
                break;
        }
        insn.flow = call ? (known ? FgFlow_Call : FgFlow_Next) : (known ? FgFlow_Jump : FgFlow_Stop);
    }
    return insn;
}

// Instruction cursor

uint16_t fg_cursor_peek(const FgCursor* c) {
    return fg_u16(c->m, c->at);
}

bool fg_cursor_take(FgCursor* c, uint16_t word) {
    if (fg_u16(c->m, c->at) != word) {
        return false;
    }
    c->at += 2;
    return true;
}

uint16_t fg_cursor_u16(FgCursor* c) {
    const uint16_t value = fg_u16(c->m, c->at);
    c->at += 2;
    return value;
}

uint32_t fg_cursor_u32(FgCursor* c) {
    const uint32_t value = fg_u32(c->m, c->at);
    c->at += 4;
    return value;
}

bool fg_cursor_call(FgCursor* c, uint32_t* target) {
    const FgInsn insn = fg_decode(c->m, c->at);
    if (insn.length == 0 || insn.flow != FgFlow_Call) {
        return false;
    }
    *target = insn.target;
    c->at += insn.length;
    return true;
}

bool fg_voice_of_offset(uint32_t value, uint32_t base, uint32_t stride, uint8_t* voice) {
    if (value < base || (value - base) % stride != 0 || (value - base) / stride >= FG_ACTIVE_VOICES) {
        return false;
    }
    *voice = (uint8_t)((value - base) / stride);
    return true;
}

bool fg_is_table_load(uint16_t word) {
    return word == OP_LEA_ABS_A0 || word == OP_MOVEA_IMM_A0;
}

bool fg_read_voice_offset_load(const FgModule* m, uint32_t at, uint32_t* value, uint32_t* length) {
    const uint16_t w = fg_u16(m, at);
    if (w == OP_CLR_L_D6) {
        *value = 0;
        *length = 2;
    } else if ((w & 0xff80) == OP_MOVEQ_D6) { // moveq #n,d6 with n >= 0
        *value = w & 0x7f;
        *length = 2;
    } else if (w == OP_MOVE_L_IMM_D6) {
        *value = fg_u32(m, at + 2);
        *length = 6;
    } else {
        return false;
    }
    return true;
}

// Walking the code

bool fg_walk_init(FgWalk* w, const FgModule* m, bool follow_calls) {
    w->m = m;
    w->follow_calls = follow_calls;
    w->seen = (uint8_t*)calloc(m->size / 2 + 1, 1);
    return w->seen != nullptr;
}

void fg_walk_free(FgWalk* w) {
    free(w->seen);
    w->seen = nullptr;
}

bool fg_walk_from(FgWalk* w, uint32_t root) {
    uint32_t stack[MAX_WALK_STACK];
    uint32_t depth = 0;
    stack[depth++] = root;
    while (depth > 0) {
        uint32_t at = stack[--depth];
        for (;;) {
            if (!fg_in_module(w->m, at, 2) || (at & 1) != 0) {
                return false;
            }
            if (w->seen[at / 2] != 0) {
                break;
            }
            const FgInsn insn = fg_decode(w->m, at);
            if (insn.length == 0) {
                return false;
            }
            w->seen[at / 2] = 1;
            const bool follow = insn.flow == FgFlow_Branch || insn.flow == FgFlow_Jump
                                || (insn.flow == FgFlow_Call && w->follow_calls);
            if (follow) {
                if (depth >= MAX_WALK_STACK) {
                    return false;
                }
                stack[depth++] = insn.target;
            }
            if (insn.flow == FgFlow_Jump || insn.flow == FgFlow_Stop) {
                break;
            }
            at += insn.length;
        }
    }
    return true;
}

bool fg_walk_has(const FgWalk* w, uint32_t at) {
    return (at & 1) == 0 && at < w->m->size && w->seen[at / 2] != 0;
}

// Routines

bool fg_routine_open(FgRoutine* r, const FgModule* m, uint32_t start) {
    r->open = fg_walk_init(&r->walk, m, false);
    if (r->open && !fg_walk_from(&r->walk, start)) {
        fg_walk_free(&r->walk);
        r->open = false;
    }
    return r->open;
}

void fg_routine_close(FgRoutine* r) {
    if (r->open) {
        fg_walk_free(&r->walk);
        r->open = false;
    }
}

uint32_t fg_routine_find(const FgRoutine* r, uint16_t opcode, uint32_t nth) {
    if (!r->open) {
        return FG_NOT_FOUND;
    }
    for (uint32_t at = 0; at < r->walk.m->size; at += 2) {
        if (fg_walk_has(&r->walk, at) && fg_u16(r->walk.m, at) == opcode && nth-- == 0) {
            return at;
        }
    }
    return FG_NOT_FOUND;
}

uint32_t fg_routine_long(const FgRoutine* r, uint16_t opcode, uint32_t nth) {
    const uint32_t at = fg_routine_find(r, opcode, nth);
    return at == FG_NOT_FOUND ? FG_NOT_FOUND : fg_u32(r->walk.m, at + 2);
}

uint32_t fg_routine_word(const FgRoutine* r, uint16_t opcode, uint32_t nth) {
    const uint32_t at = fg_routine_find(r, opcode, nth);
    return at == FG_NOT_FOUND ? FG_NOT_FOUND : fg_u16(r->walk.m, at + 2);
}

uint32_t fg_routine_first_call(const FgRoutine* r) {
    if (!r->open) {
        return FG_NOT_FOUND;
    }
    for (uint32_t at = 0; at < r->walk.m->size; at += 2) {
        if (fg_walk_has(&r->walk, at)) {
            const FgInsn insn = fg_decode(r->walk.m, at);
            if (insn.flow == FgFlow_Call) {
                return insn.target;
            }
        }
    }
    return FG_NOT_FOUND;
}

uint32_t fg_routine_table(const FgRoutine* r, bool second) {
    const uint32_t lea = fg_routine_long(r, second ? OP_LEA_ABS_A1 : OP_LEA_ABS_A0, 0);
    return lea != FG_NOT_FOUND ? lea : fg_routine_long(r, second ? OP_MOVEA_IMM_A1 : OP_MOVEA_IMM_A0, 0);
}

uint32_t fg_find(const FgModule* m, uint32_t start, uint16_t opcode, uint32_t nth) {
    FgRoutine r = { 0 };
    const uint32_t at = fg_routine_open(&r, m, start) ? fg_routine_find(&r, opcode, nth) : FG_NOT_FOUND;
    fg_routine_close(&r);
    return at;
}

uint32_t fg_find_long(const FgModule* m, uint32_t start, uint16_t opcode, uint32_t nth) {
    FgRoutine r = { 0 };
    const uint32_t value = fg_routine_open(&r, m, start) ? fg_routine_long(&r, opcode, nth) : FG_NOT_FOUND;
    fg_routine_close(&r);
    return value;
}

uint32_t fg_scan_code(const FgModule* m, uint32_t from, uint16_t opcode, uint32_t gap, uint16_t tail) {
    for (uint32_t at = from; fg_in_module(m, at, 4 + gap); at += 2) {
        if (fg_u16(m, at) == opcode && (gap == 0 || fg_u16(m, at + 2 + gap) == tail)) {
            return at;
        }
    }
    return FG_NOT_FOUND;
}
