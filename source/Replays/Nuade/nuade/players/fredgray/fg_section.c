// SPDX-License-Identifier: MIT
// Fred Gray player: decoding the song sections (see fg_section.h). The step tests are decoded with the branch that
// follows them, so no flag is carried from one operation to the next.

#include "fg_section.h"
#include <stdlib.h>
#include <string.h>

#define MAX_SECTION_OPS 256
#define MAX_SECTION_INSNS (MAX_SECTION_OPS * 4)
#define MAX_REGISTER_CACHE 8

// The bytes of a routine's start that classify_callee looks at
#define CALLEE_PREFIX_BYTES 8

// The immediate of the `andi.b #$f,d0` and `eori.w #$f,d0` in the cap decoders
#define NIBBLE_MASK 0x000f

// What a routine the sections call is

typedef enum Role {
    Role_Unknown = 0,
    Role_Tempo,
    Role_Setup,
    Role_Track,
    Role_TrackNibble,
    Role_Mask,
    Role_Reset,
    Role_TailNext,
    Role_TailAdvance,
} Role;

// What a routine starts with decides what it is (the engine signature checks the rest). For a track routine, `voice` is
// the voice whose offset it loads into d6.
static Role classify_callee(const FgModule* m, uint32_t stride, uint32_t target, uint32_t* voice) {
    if (!fg_in_module(m, target, CALLEE_PREFIX_BYTES)) {
        return Role_Unknown;
    }
    const uint16_t w0 = fg_u16(m, target);
    if (w0 == OP_MOVE_B_ABS_D0 && fg_u16(m, target + 6) == OP_ADDQ_B_D0) {
        return Role_Tempo; // move.b counter,d0; addq.b #1,d0
    }
    if (w0 == OP_MOVE_B_IMM_D16A6 && fg_u16(m, target + 2) == 1 && fg_u16(m, target + 4) == FgRecordField_Command) {
        return Role_Reset; // move.b #1,command(a6)
    }
    if (w0 == OP_CLR_W_D6 || w0 == OP_TST_B_ABS) {
        return Role_Setup; // clr.w d6, or tst.b gate
    }
    if (w0 == OP_CLR_L_D6 && fg_u16(m, target + 2) == OP_CLR_L_D4) {
        return Role_TrackNibble;
    }
    uint32_t offset = 0;
    uint32_t length = 0;
    if (fg_read_voice_offset_load(m, target, &offset, &length)) {
        uint8_t track_voice = 0;
        if (!fg_voice_of_offset(offset, 0, stride, &track_voice)) {
            return Role_Unknown;
        }
        *voice = track_voice;
        return Role_Track;
    }
    if (w0 == OP_MOVE_W_IMM_ABS || w0 == OP_MOVE_W_ABS_D2) {
        return Role_Mask; // move.w #n,AUDxLEN, or move.w position,d2
    }
    if (w0 == OP_CLR_W_D0 && fg_u16(m, target + 2) == OP_MOVE_B_ABS_D0) {
        // The two section tails count the step and request a section when the section's length is reached. One
        // requests the section after this one (it loads the section number into d1), the other the section in d1.
        if (fg_find(m, target, OP_MOVE_B_ABS_D1, 0) != FG_NOT_FOUND) {
            return Role_TailNext;
        }
        if (fg_find(m, target, OP_MOVE_B_D1_ABS, 0) != FG_NOT_FOUND) {
            return Role_TailAdvance;
        }
    }
    return Role_Unknown;
}

// Decoder state

// The register a track routine adds the note offset from: the load of an offset says which one it fills
typedef enum OffsetRegister {
    OffsetRegister_None = 0,
    OffsetRegister_A1,
    OffsetRegister_D2,
} OffsetRegister;

// What the last instruction that wrote d0 left in it: the width decides which calls may read it
typedef enum D0Value {
    D0Value_None = 0,
    D0Value_Long, // move.l #n,d0
    D0Value_Word, // move.w #n,d0
    D0Value_Byte, // move.b #n,d0
    D0Value_Computed,
} D0Value;

// The arguments the instructions before a call have loaded; the 68000 keeps them in registers, so they are only
// meaningful until the call or until the flow merges with another path
typedef struct Pending {
    bool has_table;
    uint32_t table;
    OffsetRegister offset_register;
    uint16_t offset;
    bool has_count;
    uint8_t count;
    D0Value d0_kind;
    uint32_t d0;
} Pending;

typedef struct Fixup {
    uint32_t op;     // the operation whose `jump` is to be set
    uint32_t target; // code address it jumps to
} Fixup;

typedef struct RegisterCache {
    uint32_t routine;
    OffsetRegister offset_register;
} RegisterCache;

typedef struct SectionDecoder {
    const FgSectionContext* ctx;
    FgCallees* callees;
    FgEngineCalls* calls;
    FgOp ops[MAX_SECTION_OPS];
    uint32_t op_count;
    uint32_t instruction_addr[MAX_SECTION_INSNS]; // the code address of every instruction decoded ...
    uint32_t instruction_op[MAX_SECTION_INSNS];   // ... and the index of the operation it starts
    uint32_t instruction_count;
    Fixup fixups[MAX_SECTION_OPS];
    uint32_t fixup_count;
    uint32_t furthest; // the furthest address a branch of the section jumps to
    RegisterCache registers[MAX_REGISTER_CACHE];
    uint32_t register_count;
} SectionDecoder;

// What a decoder function did with the instruction word
typedef enum DecodeResult {
    Decode_NotMine = 0, // not one of its instructions: nothing consumed
    Decode_Ok,
    Decode_Failed,
} DecodeResult;

static DecodeResult result_of(bool ok) {
    return ok ? Decode_Ok : Decode_Failed;
}

// The low byte of a big-endian word variable
static uint32_t low_byte_of(uint32_t word_var) {
    return word_var + 1;
}

static FgOp* emit(SectionDecoder* d, FgOpKind kind) {
    if (d->op_count >= MAX_SECTION_OPS) {
        return nullptr;
    }
    FgOp* op = &d->ops[d->op_count++];
    memset(op, 0, sizeof(*op));
    op->kind = kind;
    op->jump = FG_NO_JUMP;
    return op;
}

static FgOp* emit_value(SectionDecoder* d, FgOpKind kind, uint16_t value) {
    FgOp* op = emit(d, kind);
    if (op != nullptr) {
        op->value = value;
    }
    return op;
}

static FgOp* emit_voice(SectionDecoder* d, FgOpKind kind, uint16_t value, uint8_t voice) {
    FgOp* op = emit_value(d, kind, value);
    if (op != nullptr) {
        op->voice = voice;
    }
    return op;
}

// An operation on the voice whose record field `field` the displacement `displacement(a6)` addresses
static FgOp* emit_record(SectionDecoder* d, FgOpKind kind, uint16_t value, uint32_t displacement, FgRecordField field) {
    uint8_t voice = 0;
    return fg_voice_of_offset(displacement, field, d->ctx->stride, &voice) ? emit_voice(d, kind, value, voice)
                                                                           : nullptr;
}

// Makes the last emitted operation jump to the code at `target`. Branches go forward and stay inside the section.
static bool add_fixup(SectionDecoder* d, uint32_t from, uint32_t target) {
    if (d->fixup_count >= MAX_SECTION_OPS || target <= from || target >= d->ctx->engine_start) {
        return false;
    }
    d->fixups[d->fixup_count++] = (Fixup) { .op = d->op_count - 1, .target = target };
    d->furthest = target > d->furthest ? target : d->furthest;
    return true;
}

static bool is_fixup_target(const SectionDecoder* d, uint32_t at) {
    for (uint32_t i = 0; i < d->fixup_count; i++) {
        if (d->fixups[i].target == at) {
            return true;
        }
    }
    return false;
}

// Remembers an engine routine the sections call, for the signature
static bool note_engine_call(SectionDecoder* d, uint32_t target) {
    FgEngineCalls* calls = d->calls;
    for (uint32_t i = 0; i < calls->count; i++) {
        if (calls->routine[i] == target) {
            return true;
        }
    }
    if (calls->count >= FG_MAX_ENGINE_CALLS) {
        return false;
    }
    calls->routine[calls->count++] = target;
    return true;
}

// Sets a role's routine, or checks that every section calls the same one
static bool bind_role(uint32_t* slot, uint32_t target) {
    if (*slot != FG_NOT_FOUND && *slot != target) {
        return false;
    }
    *slot = target;
    return true;
}

// The register the track routine at `target` adds the note offset from: its note start holds `add.w a1,d4` (Eco, Star
// Goose, Victory Road) or `add.w d2,d4` (Black Lamp)
static OffsetRegister track_offset_register(SectionDecoder* d, uint32_t target) {
    for (uint32_t i = 0; i < d->register_count; i++) {
        if (d->registers[i].routine == target) {
            return d->registers[i].offset_register;
        }
    }
    FgRoutine routine = { 0 };
    OffsetRegister found = OffsetRegister_None;
    if (fg_routine_open(&routine, d->ctx->m, target)) {
        if (fg_routine_find(&routine, OP_ADD_W_A1_D4, 0) != FG_NOT_FOUND) {
            found = OffsetRegister_A1;
        } else if (fg_routine_find(&routine, OP_ADD_W_D2_D4, 0) != FG_NOT_FOUND) {
            found = OffsetRegister_D2;
        }
    }
    fg_routine_close(&routine);
    if (d->register_count < MAX_REGISTER_CACHE) {
        d->registers[d->register_count++] = (RegisterCache) { .routine = target, .offset_register = found };
    }
    return found;
}

// The conditional branch that follows an instruction that set the flags: the test it makes of the step for each
// condition code (FgTest_None: not one this instruction is decoded with)

typedef struct BranchTests {
    FgTest equal;       // beq
    FgTest not_equal;   // bne
    FgTest carry_set;   // bcs
    FgTest carry_clear; // bcc
    FgTest plus;        // bpl
    uint16_t value;
    uint8_t then_shift;
} BranchTests;

typedef enum BranchResult {
    BranchResult_Failed = 0,
    BranchResult_None, // the next instruction is not a conditional branch
    BranchResult_Taken,
} BranchResult;

static BranchResult take_branch(SectionDecoder* d, FgCursor* c, const BranchTests* tests) {
    const uint16_t w = fg_cursor_peek(c);
    const unsigned condition = (w >> OP_COND_SHIFT) & OP_COND_MASK;
    if ((w & OP_MASK_BCC) != OP_BCC || condition < COND_FIRST_CONDITIONAL) {
        return BranchResult_None;
    }
    FgTest test = FgTest_None;
    switch (condition) {
        case COND_CARRY_CLEAR:
            test = tests->carry_clear;
            break;
        case COND_CARRY_SET:
            test = tests->carry_set;
            break;
        case COND_NOT_EQUAL:
            test = tests->not_equal;
            break;
        case COND_EQUAL:
            test = tests->equal;
            break;
        case COND_PLUS:
            test = tests->plus;
            break;
        default:
            break;
    }
    const FgInsn branch = fg_decode(c->m, c->at);
    FgOp* op = test == FgTest_None || branch.length == 0 ? nullptr : emit_value(d, FgOp_Branch, tests->value);
    if (op == nullptr || !add_fixup(d, c->at, branch.target)) {
        return BranchResult_Failed;
    }
    op->test = test;
    op->then_shift = tests->then_shift;
    c->at += branch.length;
    return BranchResult_Taken;
}

// The call of a routine: the operation it is, from the arguments collected before it
static bool decode_call(SectionDecoder* d, Pending* p, uint32_t target) {
    uint32_t voice = 0;
    const Role role = classify_callee(d->ctx->m, d->ctx->stride, target, &voice);
    FgCallees* callees = d->callees;
    FgOp* op = nullptr;
    switch (role) {
        case Role_Tempo:
            if (!p->has_count || !bind_role(&callees->tempo, target)) {
                return false;
            }
            op = emit_value(d, FgOp_Tempo, p->count);
            break;
        case Role_Setup:
            // the setup reads all three instrument offsets out of one long, so the whole long must have been loaded
            if (p->d0_kind != D0Value_Long || p->d0 > 0xffffffu || !bind_role(&callees->setup, target)) {
                return false;
            }
            op = emit(d, FgOp_Instruments);
            for (int i = 0; op != nullptr && i < FG_ACTIVE_VOICES; i++) {
                op->instrument[i] = (uint8_t)(p->d0 >> (8 * i));
                if ((op->instrument[i] & 3) != 0) {
                    return false; // the setup reads a long at that offset
                }
            }
            break;
        case Role_Track:
        case Role_TrackNibble: {
            const bool nibble = role == Role_TrackNibble;
            uint32_t* slot = nibble ? &callees->track_nibble : &callees->track[voice];
            if (!p->has_table || p->offset_register == OffsetRegister_None
                || p->offset_register != track_offset_register(d, target) || !bind_role(slot, target)) {
                return false;
            }
            op = emit_voice(d, FgOp_Track, 0, nibble ? 0 : (uint8_t)voice);
            if (op != nullptr) {
                op->nibble = nibble;
                op->table = p->table;
                op->offset = p->offset;
            }
            break;
        }
        case Role_Mask:
            if (p->d0_kind != D0Value_Byte || !bind_role(&callees->mask, target)) {
                return false;
            }
            op = emit_value(d, FgOp_ChannelMask, (uint16_t)p->d0);
            break;
        case Role_Reset:
            op = bind_role(&callees->reset, target) ? emit(d, FgOp_Reset) : nullptr;
            break;
        default:
            return false;
    }
    memset(p, 0, sizeof(*p));
    return op != nullptr && note_engine_call(d, target);
}

// A jump into the engine: how the section ends
static bool decode_tail(SectionDecoder* d, const Pending* p, uint32_t target) {
    uint32_t voice = 0;
    const Role role = classify_callee(d->ctx->m, d->ctx->stride, target, &voice);
    FgOp* op = emit(d, FgOp_End);
    if (op == nullptr) {
        return false;
    }
    switch (role) {
        case Role_TailNext:
            op->end = FgEnd_Advance;
            if (!bind_role(&d->callees->tail_next, target)) {
                return false;
            }
            break;
        case Role_TailAdvance:
            if (!p->has_count || !bind_role(&d->callees->tail_advance, target)) {
                return false;
            }
            op->end = FgEnd_AdvanceTo;
            op->value = p->count;
            break;
        case Role_Reset:
            op->end = FgEnd_Reset;
            if (!bind_role(&d->callees->reset, target)) {
                return false;
            }
            break;
        default:
            return false;
    }
    return note_engine_call(d, target);
}

// move.w #n,abs.l (or move.w d0,abs.l after move.w #n,d0): the transpose, or AUDxLEN of a voice
static bool decode_store_word(SectionDecoder* d, uint16_t value, uint32_t address) {
    uint8_t voice = 0;
    if (fg_voice_of_offset(address, CUSTOM_AUD0LEN, CUSTOM_CHANNEL_STRIDE, &voice)) {
        return emit_voice(d, FgOp_SetLen, value, voice) != nullptr;
    }
    return d->ctx->transpose_var != FG_NOT_FOUND && address == d->ctx->transpose_var
           && emit_value(d, FgOp_SetTranspose, value) != nullptr;
}

// move.w transpose,d0; eori.w #n,d0 (or addi.w); move.w d0,transpose
static bool decode_update_transpose(SectionDecoder* d, Pending* p, FgCursor* c) {
    const uint32_t transpose = d->ctx->transpose_var;
    if (transpose == FG_NOT_FOUND || fg_cursor_u32(c) != transpose) {
        return false;
    }
    const uint16_t arithmetic = fg_cursor_u16(c);
    const uint16_t value = fg_cursor_u16(c);
    if ((arithmetic != OP_EORI_W_D0 && arithmetic != OP_ADDI_W_D0) || fg_cursor_u16(c) != OP_MOVE_W_D0_ABS
        || fg_cursor_u32(c) != transpose) {
        return false;
    }
    p->d0_kind = D0Value_Computed;
    return emit_value(d, arithmetic == OP_EORI_W_D0 ? FgOp_XorTranspose : FgOp_AddTranspose, value) != nullptr;
}

// The store of the cap in d0 into the cap variable: move.b d0,cap
static bool store_cap(const SectionDecoder* d, FgCursor* c) {
    return fg_cursor_u16(c) == OP_MOVE_B_D0_ABS && d->ctx->cap_var != FG_NOT_FOUND
           && fg_cursor_u32(c) == d->ctx->cap_var;
}

// move.b d7,d0; lsr.b #4,d0; addi.b #n,d0; andi.b #$f,d0; move.b d0,cap: the cap is the step's high nibble plus n
static bool decode_cap_from_step(SectionDecoder* d, Pending* p, FgCursor* c) {
    if (fg_cursor_u16(c) != OP_LSR_B_D0_4 || fg_cursor_u16(c) != OP_ADDI_B_D0) {
        return false;
    }
    const uint16_t add = fg_cursor_u16(c);
    if (add > 0xff || fg_cursor_u16(c) != OP_ANDI_B_D0 || fg_cursor_u16(c) != NIBBLE_MASK || !store_cap(d, c)) {
        return false;
    }
    p->d0_kind = D0Value_Computed;
    return emit_value(d, FgOp_CapFromStep, add) != nullptr;
}

// move.b position (low byte),d0; lsr.b #4,d0; andi.b #$f,d0; eori.w #$f,d0; move.b d0,cap
static bool decode_cap_from_position(SectionDecoder* d, Pending* p, FgCursor* c) {
    if (fg_cursor_u32(c) != low_byte_of(d->ctx->position_var) || fg_cursor_u16(c) != OP_LSR_B_D0_4
        || fg_cursor_u16(c) != OP_ANDI_B_D0 || fg_cursor_u16(c) != NIBBLE_MASK || fg_cursor_u16(c) != OP_EORI_W_D0
        || fg_cursor_u16(c) != NIBBLE_MASK || !store_cap(d, c)) {
        return false;
    }
    p->d0_kind = D0Value_Computed;
    return emit(d, FgOp_CapFromPosition) != nullptr;
}

// lsr.w #n,d7: with a bcs after it the branch tests the bit that was shifted out
static bool decode_shift(SectionDecoder* d, FgCursor* c, uint16_t w) {
    const uint16_t field = (w >> OP_LSR_COUNT_SHIFT) & OP_LSR_COUNT_MASK;
    const uint16_t count = field != 0 ? field : OP_LSR_COUNT_ZERO_MEANS;
    const BranchTests tests
        = { .carry_set = FgTest_BitSet, .value = (uint16_t)(count - 1), .then_shift = (uint8_t)count };
    const BranchResult result = take_branch(d, c, &tests);
    return result == BranchResult_Taken
           || (result == BranchResult_None && emit_value(d, FgOp_StepShift, count) != nullptr);
}

// An instruction that loads an argument of a call
static DecodeResult decode_load(SectionDecoder* d, FgCursor* c, Pending* p, uint16_t w) {
    (void)d;
    if (fg_is_table_load(w)) {
        p->has_table = true;
        p->table = fg_cursor_u32(c);
        return Decode_Ok;
    }
    switch (w) {
        case OP_MOVEA_IMM_A1:
            p->offset_register = OffsetRegister_A1;
            p->offset = (uint16_t)fg_cursor_u32(c);
            return Decode_Ok;
        case OP_CLR_W_D2:
            p->offset_register = OffsetRegister_D2;
            p->offset = 0;
            return Decode_Ok;
        case OP_MOVE_L_IMM_D0:
            p->d0_kind = D0Value_Long;
            p->d0 = fg_cursor_u32(c);
            return Decode_Ok;
        case OP_MOVE_W_IMM_D0:
            p->d0_kind = D0Value_Word;
            p->d0 = fg_cursor_u16(c);
            return Decode_Ok;
        case OP_MOVE_B_IMM_D0:
            p->d0_kind = D0Value_Byte;
            p->d0 = fg_cursor_u16(c) & 0xff;
            return Decode_Ok;
        case OP_MOVE_B_IMM_D1:
        case OP_MOVE_W_IMM_D1:
            p->has_count = true;
            p->count = (uint8_t)fg_cursor_u16(c);
            return Decode_Ok;
        default:
            break;
    }
    if ((w & OP_MOVEQ_REG_MASK) == OP_MOVEQ_D1) { // moveq #n,d1
        p->has_count = true;
        p->count = (uint8_t)w;
        return Decode_Ok;
    }
    if ((w & OP_MOVEQ_REG_MASK) == OP_MOVEQ_D2) { // moveq #n,d2: sign extended
        p->offset_register = OffsetRegister_D2;
        p->offset = (uint16_t)(int16_t)(int8_t)(w & 0xff);
        return Decode_Ok;
    }
    return Decode_NotMine;
}

// An instruction that stores into the record of a voice: the command, the slide step
static DecodeResult decode_record_store(SectionDecoder* d, FgCursor* c, Pending* p, uint16_t w) {
    switch (w) {
        case OP_MOVE_B_TABLE_D16A6: { // move.b (a0,d7.w),command(a6)
            if (fg_cursor_u16(c) != TABLE_INDEX_D7_W || !p->has_table) {
                return Decode_Failed;
            }
            FgOp* op = emit_record(d, FgOp_CommandFromTable, 0, fg_cursor_u16(c), FgRecordField_Command);
            if (op == nullptr) {
                return Decode_Failed;
            }
            op->table = p->table;
            p->has_table = false;
            return Decode_Ok;
        }
        case OP_MOVE_B_IMM_D16A6: { // move.b #n,command(a6)
            const uint16_t value = fg_cursor_u16(c) & 0xff;
            return result_of(emit_record(d, FgOp_Command, value, fg_cursor_u16(c), FgRecordField_Command) != nullptr);
        }
        case OP_MOVE_W_IMM_D16A6: { // move.w #n,slide step(a6)
            const uint16_t value = fg_cursor_u16(c);
            return result_of(emit_record(d, FgOp_SlideStep, value, fg_cursor_u16(c), FgRecordField_SlideStep)
                             != nullptr);
        }
        default:
            return Decode_NotMine;
    }
}

// An instruction that stores into a variable of the module or a custom register
static DecodeResult decode_variable_store(SectionDecoder* d, FgCursor* c, Pending* p, uint16_t w) {
    const FgSectionContext* ctx = d->ctx;
    switch (w) {
        case OP_MOVE_W_IMM_ABS: { // move.w #n,abs.l
            const uint16_t value = fg_cursor_u16(c);
            return result_of(decode_store_word(d, value, fg_cursor_u32(c)));
        }
        case OP_MOVE_W_D0_ABS: { // move.w d0,abs.l after move.w #n,d0
            const bool known = p->d0_kind == D0Value_Word;
            p->d0_kind = D0Value_None;
            return result_of(known && decode_store_word(d, (uint16_t)p->d0, fg_cursor_u32(c)));
        }
        case OP_MOVE_B_IMM_ABS: { // move.b #n,abs.l: the request for the next section
            const uint16_t value = fg_cursor_u16(c) & 0xff;
            return result_of(fg_cursor_u32(c) == fg_request_var(ctx->request_vars, FgRequestVar_NextSection)
                             && emit_value(d, FgOp_SetNextState, value) != nullptr);
        }
        case OP_MOVE_L_IMM_ABS: { // move.l #n,abs.l: AUDxLC
            const uint32_t value = fg_cursor_u32(c);
            uint8_t voice = 0;
            if (!fg_voice_of_offset(fg_cursor_u32(c), CUSTOM_AUD0LC, CUSTOM_CHANNEL_STRIDE, &voice)) {
                return Decode_Failed;
            }
            FgOp* op = emit_voice(d, FgOp_SetLc, 0, voice);
            if (op != nullptr) {
                op->table = value;
            }
            return result_of(op != nullptr);
        }
        case OP_CLR_W_ABS: { // clr.w abs.l: the transpose, or the section and the byte after it
            const uint32_t address = fg_cursor_u32(c);
            if (ctx->transpose_var != FG_NOT_FOUND && address == ctx->transpose_var) {
                return result_of(emit_value(d, FgOp_SetTranspose, 0) != nullptr);
            }
            return result_of(address == fg_request_var(ctx->request_vars, FgRequestVar_Section)
                             && emit(d, FgOp_ClearState) != nullptr);
        }
        case OP_CLR_B_ABS: // clr.b abs.l: the section
            return result_of(fg_cursor_u32(c) == fg_request_var(ctx->request_vars, FgRequestVar_Section)
                             && emit(d, FgOp_ClearState) != nullptr);
        case OP_MOVE_W_ABS_D0:
            return result_of(decode_update_transpose(d, p, c));
        case OP_MOVE_B_D7_D0:
            return result_of(decode_cap_from_step(d, p, c));
        case OP_MOVE_B_ABS_D0:
            return result_of(decode_cap_from_position(d, p, c));
        default:
            return Decode_NotMine;
    }
}

// An instruction that works on the step (d7)
static DecodeResult decode_step(SectionDecoder* d, FgCursor* c, Pending* p, uint16_t w) {
    (void)p;
    const BranchTests zero_tests = { .equal = FgTest_Zero, .not_equal = FgTest_NonZero };
    switch (w) {
        case OP_MOVE_W_ABS_D7: // move.w position,d7
            return result_of(fg_cursor_u32(c) == d->ctx->position_var && emit(d, FgOp_LoadStep) != nullptr);
        case OP_ANDI_W_D7: // andi.w #n,d7; the branch tests the result
            return result_of(emit_value(d, FgOp_StepAnd, fg_cursor_u16(c)) != nullptr
                             && take_branch(d, c, &zero_tests) != BranchResult_Failed);
        case OP_TST_W_D7:
            return result_of(take_branch(d, c, &zero_tests) != BranchResult_Failed);
        case OP_TST_B_D7: { // tst.b d7; bpl: the branch is taken when bit 7 of the low byte is clear
            const BranchTests tests = { .plus = FgTest_BitClear, .value = 7 };
            return result_of(take_branch(d, c, &tests) == BranchResult_Taken);
        }
        case OP_CMPI_W_D7: {
            const uint16_t value = fg_cursor_u16(c);
            const BranchTests tests = {
                .equal = FgTest_Equal, .not_equal = FgTest_NotEqual, .carry_clear = FgTest_AtLeast, .value = value
            };
            return result_of(take_branch(d, c, &tests) != BranchResult_Failed);
        }
        case OP_BTST_B_D7: {
            const uint16_t bit = fg_cursor_u16(c);
            const BranchTests tests = { .equal = FgTest_BitClear, .not_equal = FgTest_BitSet, .value = bit };
            return result_of(bit <= 7 && take_branch(d, c, &tests) != BranchResult_Failed);
        }
        default:
            break;
    }
    if ((w & OP_LSR_W_D7_MASK) == OP_LSR_W_D7) {
        return result_of(decode_shift(d, c, w));
    }
    return Decode_NotMine;
}

typedef DecodeResult (*DecodeFn)(SectionDecoder* d, FgCursor* c, Pending* p, uint16_t w);

static const DecodeFn s_decoders[] = { decode_load, decode_record_store, decode_variable_store, decode_step };

// Decodes the instruction (or the fixed run of instructions that make one operation) at c->at. Sets *terminal when
// it ends the section's flow.
static bool decode_one(SectionDecoder* d, FgCursor* c, Pending* p, bool* terminal) {
    const uint32_t at = c->at;
    const FgInsn insn = fg_decode(d->ctx->m, at);
    if (insn.length == 0) {
        return false;
    }
    const uint16_t w = fg_cursor_u16(c);
    *terminal = false;
    if (w == OP_RTS) {
        FgOp* op = emit(d, FgOp_End);
        if (op != nullptr) {
            op->end = FgEnd_Return;
        }
        *terminal = true;
        return op != nullptr;
    }
    for (size_t i = 0; i < nuade_sizeof_array(s_decoders); i++) {
        const DecodeResult result = s_decoders[i](d, c, p, w);
        if (result != Decode_NotMine) {
            return result == Decode_Ok;
        }
    }
    if (insn.flow == FgFlow_Call) {
        c->at = at + insn.length;
        return decode_call(d, p, insn.target);
    }
    if (insn.flow == FgFlow_Jump) {
        c->at = at + insn.length;
        *terminal = true;
        if (insn.target >= d->ctx->engine_start) {
            return decode_tail(d, p, insn.target);
        }
        return emit(d, FgOp_Jump) != nullptr && add_fixup(d, at, insn.target);
    }
    return false;
}

// Decodes the section routine at `entry` into d->ops. The flow ends at a return or a jump out of the section once no
// branch jumps to the code after it or further on.
static bool decode_section(SectionDecoder* d, uint32_t entry) {
    d->op_count = 0;
    d->fixup_count = 0;
    d->instruction_count = 0;
    d->furthest = 0;
    Pending pending = { 0 };
    FgCursor c = { .m = d->ctx->m, .at = entry };
    for (;;) {
        if (c.at >= d->ctx->engine_start || d->instruction_count >= MAX_SECTION_INSNS) {
            return false;
        }
        if (is_fixup_target(d, c.at)) {
            memset(&pending, 0, sizeof(pending)); // another path arrives here: the loaded arguments are not certain
        }
        d->instruction_addr[d->instruction_count] = c.at;
        d->instruction_op[d->instruction_count] = d->op_count;
        d->instruction_count++;
        bool terminal = false;
        if (!decode_one(d, &c, &pending, &terminal)) {
            return false;
        }
        if (terminal) {
            memset(&pending, 0, sizeof(pending));
            if (c.at > d->furthest) {
                break;
            }
        }
    }
    for (uint32_t i = 0; i < d->fixup_count; i++) {
        uint32_t index = 0;
        while (index < d->instruction_count && d->instruction_addr[index] != d->fixups[i].target) {
            index++;
        }
        if (index == d->instruction_count) {
            return false; // a branch into the middle of an instruction
        }
        d->ops[d->fixups[i].op].jump = (uint16_t)d->instruction_op[index];
    }
    return true;
}

static void init_callees(FgCallees* callees) {
    callees->tempo = callees->setup = callees->track_nibble = callees->mask = callees->reset = FG_NOT_FOUND;
    callees->tail_next = callees->tail_advance = FG_NOT_FOUND;
    for (int i = 0; i < FG_ACTIVE_VOICES; i++) {
        callees->track[i] = FG_NOT_FOUND;
    }
}

bool fg_decode_sections(const FgSectionContext* ctx, const uint32_t* entries, uint32_t count, FgLayout* layout,
                        FgCallees* callees, FgEngineCalls* calls) {
    init_callees(callees);
    calls->count = 0;
    layout->section_count = count;
    layout->sections = (FgSection*)calloc(count, sizeof(FgSection));
    SectionDecoder* d = (SectionDecoder*)calloc(1, sizeof(SectionDecoder));
    bool ok = layout->sections != nullptr && d != nullptr;
    uint32_t capacity = 0;
    uint32_t used = 0;
    if (ok) {
        d->ctx = ctx;
        d->callees = callees;
        d->calls = calls;
    }
    for (uint32_t section = 1; ok && section < count; section++) {
        uint32_t same = 0;
        for (uint32_t earlier = 1; earlier < section && same == 0; earlier++) {
            if (entries[earlier] == entries[section]) {
                same = earlier;
            }
        }
        if (same != 0) {
            layout->sections[section] = layout->sections[same];
        } else if (entries[section] == ctx->engine_start) {
            // Victory Road: the unused sections at the end of the table run the first engine routine, which only
            // passes on requests nothing makes; they do nothing
            layout->sections[section] = (FgSection) { .first = used, .count = 0 };
        } else {
            ok = decode_section(d, entries[section]);
            if (ok && used + d->op_count > capacity) {
                capacity = (used + d->op_count) * 2;
                FgOp* grown = (FgOp*)realloc(layout->op_pool, capacity * sizeof(FgOp));
                ok = grown != nullptr;
                layout->op_pool = ok ? grown : layout->op_pool;
            }
            if (ok) {
                memcpy(layout->op_pool + used, d->ops, d->op_count * sizeof(FgOp));
                layout->sections[section] = (FgSection) { .first = used, .count = d->op_count };
                used += d->op_count;
            }
        }
    }
    free(d);
    return ok;
}
