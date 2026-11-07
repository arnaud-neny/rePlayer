// SPDX-License-Identifier: MIT
// Silmarils player: the script interpreter. The eagleplayer runs a small script when it sets the song up; it names the
// samples of the instrument slots, and picks the music block and the tempo of the subsong. Statements are a byte
// (the operation) and its operands; operands are expressions, evaluated into an accumulator, and a binary expression
// works on the accumulator as it was before its operand was evaluated.

#include "sil_internal.h"

#define SIL_SCRIPT_MARK 0xA142 // an optional word before the script; it clears `slot_parameters`
#define SIL_FLAG_VARIABLE 0xC  // the variable the song script finds its flag + subsong in
#define SIL_SLOT_MASK 0x1F
#define SIL_MAX_NESTING 64

#define SIL_MUSIC_OFFSET 6 // from the block to the music stream

typedef enum SilOp {
    SilOp_CallByte = 0x05,
    SilOp_CallWord = 0x06,
    SilOp_CallLong = 0x07,
    SilOp_JumpLong = 0x0A,
    SilOp_Return = 0x11,
    SilOp_JumpIfZero = 0x14,
    SilOp_JumpIfNonZero = 0x15,
    SilOp_Assign = 0x1E,
    SilOp_Evaluate = 0x1F,
    SilOp_Add = 0x20,
    SilOp_Search = 0x2E,
    SilOp_Index = 0x2F,
    SilOp_Stop = 0x3F,
    SilOp_AssignMinusOne = 0x40,
    SilOp_End = 0x42,
    SilOp_Music = 0x95,
    SilOp_Tempo = 0x97,
    SilOp_Nop = 0xA1,
    SilOp_Sample = 0xCD,
    SilOp_Ignore = 0xD5,
} SilOp;

// Expressions. The byte is an even offset into a table of words in the original, so odd bytes are not expressions.
typedef enum SilExpr {
    SilExpr_Byte = 0x00,
    SilExpr_Word = 0x02,
    SilExpr_VariableByte = 0x12,
    SilExpr_VariableWord = 0x14,
    SilExpr_ModuleByte = 0x1E,
    SilExpr_List = 0x38,
    SilExpr_ListEnd = 0x3A,
    SilExpr_Equal = 0x4A,
    SilExpr_LessEqual = 0x4E,
    SilExpr_Plus = 0x56,
    SilExpr_Minus = 0x58,
} SilExpr;

// The target of an assignment
typedef enum SilTarget {
    SilTarget_VariableByte = 0x12,
    SilTarget_VariableWord = 0x14,
    SilTarget_ModuleByte = 0x1E,
} SilTarget;

void sil_script_init(SilScript* script, SilMemory* memory, const SilImage* song, uint32_t* steps_left) {
    *script = (SilScript) { .memory = memory, .song = song, .blocks = song, .steps_left = steps_left };
}

static void fail(SilScript* s) {
    s->failed = true;
}

static uint8_t fetch8(SilScript* s) {
    if (!sil_in_memory(s->memory, s->pc, 1)) {
        fail(s);
        return 0;
    }
    return s->memory->data[s->pc++];
}

static uint16_t fetch16(SilScript* s) {
    const uint16_t high = fetch8(s);
    return (uint16_t)((high << 8) | fetch8(s));
}

// A signed 24-bit operand: the sign extends through the top byte.

static int32_t fetch24(SilScript* s) {
    const uint32_t high = fetch8(s);
    const uint32_t value = (high << 16) | fetch16(s);
    return (int32_t)(value | ((high & 0x80) != 0 ? 0xFF000000u : 0));
}

static void branch(SilScript* s, int32_t displacement) {
    s->pc = (uint32_t)((int64_t)s->pc + displacement);
}

// Word operands and tables sit on even addresses: a script that is at an odd address skips one byte first.

static void align(SilScript* s) {
    s->pc += s->pc & 1;
}

static int16_t variable_read(SilScript* s, uint8_t index, uint32_t width) {
    if (index + width > SIL_VARIABLES) {
        fail(s);
        return 0;
    }
    return width == 1 ? (int8_t)s->variables[index] : (int16_t)nuade_read_be16(&s->variables[index]);
}

static void variable_write(SilScript* s, uint8_t index, uint32_t width, int16_t value) {
    if (index + width > SIL_VARIABLES) {
        fail(s);
        return;
    }
    if (width == 1) {
        s->variables[index] = (uint8_t)value;
    } else {
        s->variables[index] = (uint8_t)((uint16_t)value >> 8);
        s->variables[index + 1] = (uint8_t)value;
    }
}

// Module-relative accesses count from the song's base and stay inside the song file.

static bool module_address(SilScript* s, uint16_t offset, uint32_t* address) {
    const SilImage* song = s->song;
    const int64_t at = (int64_t)sil_image_base(song) + (int16_t)offset;
    if (at < song->start || at >= (int64_t)song->start + song->size) {
        fail(s);
        return false;
    }
    *address = (uint32_t)at;
    return true;
}

static void evaluate(SilScript* s);

// The expression after the accumulator was saved: a binary expression's left operand.

static void evaluate_operand(SilScript* s) {
    s->left = s->value;
    evaluate(s);
}

static void evaluate(SilScript* s) {
    if (s->nesting >= SIL_MAX_NESTING) {
        fail(s);
        return;
    }
    s->nesting++;
    switch (fetch8(s)) {
        case SilExpr_Byte:
            s->value = (int8_t)fetch8(s);
            break;
        case SilExpr_Word:
            s->value = (int16_t)fetch16(s);
            break;
        case SilExpr_VariableByte:
            s->value = variable_read(s, fetch8(s), 1);
            break;
        case SilExpr_VariableWord:
            s->value = variable_read(s, fetch8(s), 2);
            break;
        case SilExpr_ModuleByte: {
            uint32_t address = 0;
            if (module_address(s, fetch16(s), &address)) {
                s->value = (int8_t)sil_u8(s->memory, address);
            }
            break;
        }
        case SilExpr_List:
            // Expressions one after the other until the end mark; the value is the last one's
            while (!s->failed && sil_u8(s->memory, s->pc) != SilExpr_ListEnd) {
                evaluate(s);
            }
            s->pc++;
            break;
        case SilExpr_Equal:
            evaluate_operand(s);
            s->value = s->left == s->value ? -1 : 0;
            break;
        case SilExpr_LessEqual:
            evaluate_operand(s);
            s->value = s->left <= s->value ? -1 : 0;
            break;
        case SilExpr_Plus:
            evaluate_operand(s);
            s->value = (int16_t)(s->left + s->value);
            break;
        case SilExpr_Minus:
            evaluate_operand(s);
            // The subtraction leaves its result in the left operand too, which an enclosing binary expression then uses
            s->left = (int16_t)(s->left - s->value);
            s->value = s->left;
            break;
        default:
            fail(s);
            break;
    }
    s->nesting--;
}

static void assign(SilScript* s) {
    switch (fetch8(s)) {
        case SilTarget_VariableByte:
            variable_write(s, fetch8(s), 1, s->value);
            break;
        case SilTarget_VariableWord:
            variable_write(s, fetch8(s), 2, s->value);
            break;
        case SilTarget_ModuleByte: {
            uint32_t address = 0;
            if (module_address(s, fetch16(s), &address)) {
                sil_put8(s->memory, address, (uint8_t)s->value);
            }
            break;
        }
        default:
            fail(s);
            break;
    }
}

static void add_to_variable(SilScript* s) {
    if (fetch8(s) != SilTarget_VariableWord) {
        fail(s);
        return;
    }
    const uint8_t index = fetch8(s);
    variable_write(s, index, 2, (int16_t)(variable_read(s, index, 2) + s->value));
}

static void call(SilScript* s, int32_t displacement) {
    if (s->depth >= SIL_CALL_DEPTH) {
        fail(s);
        return;
    }
    s->calls[s->depth++] = s->pc;
    branch(s, displacement);
}

static void return_from_call(SilScript* s) {
    if (s->depth == 0) {
        fail(s);
        return;
    }
    s->pc = s->calls[--s->depth];
}

// Search: the value against a sorted table of (value, displacement) pairs. The table starts on an even address; a
// match jumps, a value below an entry ends the search and skips the entries left, a value above all of them falls
// through.

static void search(SilScript* s) {
    evaluate(s);
    int32_t left = fetch8(s);
    align(s);
    for (;;) {
        const int16_t entry = (int16_t)fetch16(s);
        if (s->failed) {
            return;
        }
        if (s->value == entry) {
            const int16_t displacement = (int16_t)fetch16(s);
            branch(s, displacement);
            return;
        }
        s->pc += 2;
        if (s->value < entry) {
            s->pc += 4u * (uint32_t)left;
            return;
        }
        if (--left < 0) {
            return;
        }
    }
}

// Index: the value plus a base against a count; in range it picks one of the displacements that follow, else the
// statement skips the table.

static void index_jump(SilScript* s) {
    evaluate(s);
    const uint32_t last = fetch8(s);
    align(s);
    s->value = (int16_t)(s->value + (int16_t)fetch16(s));
    if (s->value < 0 || (uint32_t)s->value > last) {
        s->pc += 2 * last + 2;
        return;
    }
    s->pc += 2u * (uint32_t)s->value;
    const int16_t displacement = (int16_t)fetch16(s);
    branch(s, displacement);
}

static bool block_of(SilScript* s, int16_t index, uint32_t* block) {
    if (index < 0 || !sil_image_block(s->memory, s->blocks, (uint32_t)index, block)) {
        fail(s);
        return false;
    }
    return true;
}

// Sample: slot, block, note base. A block that is not a sample leaves the slot as it is; a negative block empties it.

static void define_sample(SilScript* s) {
    evaluate(s);
    const int16_t slot = s->value;
    evaluate(s);
    const int16_t index = s->value;
    evaluate(s);
    const uint16_t note_base = (uint16_t)s->value;
    if (s->failed) {
        return;
    }
    SilSlot* target = &s->slots[(uint16_t)slot & SIL_SLOT_MASK];
    if (index < 0) {
        *target = (SilSlot) { 0 };
        return;
    }
    uint32_t block = 0;
    if (!block_of(s, index, &block)) {
        return;
    }
    SilBlock header;
    if (!sil_block_read(s->memory, block, &header)) {
        fail(s);
    } else if (sil_block_is_sample(&header)) {
        *target = (SilSlot) { .sample = block + SIL_SAMPLE_HEADER, .note_base = note_base };
    }
}

// Music: the block, then five values: the first is of no use to the player, the second is the tempo.

static void choose_music(SilScript* s) {
    evaluate(s);
    uint32_t block = 0;
    if (s->failed || !block_of(s, s->value, &block)) {
        return;
    }
    SilBlock header;
    if (!sil_block_read(s->memory, block, &header)) {
        fail(s);
        return;
    }
    const bool is_music = sil_block_is_music(&header);
    if (is_music) {
        s->music = block + SIL_MUSIC_OFFSET;
    }
    for (uint32_t i = 0; i < 5; i++) {
        evaluate(s);
        if (is_music && i == 1) {
            s->tempo = (uint8_t)s->value;
        }
    }
}

// One statement. Returns false when the script has ended.

static bool statement(SilScript* s) {
    switch (fetch8(s)) {
        case SilOp_CallByte:
            call(s, (int8_t)fetch8(s));
            break;
        case SilOp_CallWord:
            call(s, (int16_t)fetch16(s));
            break;
        case SilOp_CallLong:
            call(s, fetch24(s));
            break;
        case SilOp_JumpLong:
            branch(s, fetch24(s));
            break;
        case SilOp_Return:
            return_from_call(s);
            break;
        case SilOp_JumpIfZero: {
            const int32_t displacement = fetch24(s);
            if (s->value == 0) {
                branch(s, displacement);
            }
            break;
        }
        case SilOp_JumpIfNonZero: {
            const int32_t displacement = (int8_t)fetch8(s);
            if (s->value != 0) {
                branch(s, displacement);
            }
            break;
        }
        case SilOp_Assign:
            evaluate_operand(s);
            assign(s);
            break;
        case SilOp_Evaluate:
            evaluate_operand(s);
            break;
        case SilOp_Add:
            evaluate_operand(s);
            add_to_variable(s);
            break;
        case SilOp_Search:
            search(s);
            break;
        case SilOp_Index:
            index_jump(s);
            break;
        case SilOp_Stop:
        case SilOp_End:
            return false;
        case SilOp_AssignMinusOne:
            fetch16(s);
            s->value = -1;
            assign(s);
            break;
        case SilOp_Music:
            choose_music(s);
            break;
        case SilOp_Tempo:
            evaluate(s);
            s->tempo = (uint8_t)(s->value == 0 ? 1 : s->value);
            break;
        case SilOp_Nop:
            break;
        case SilOp_Sample:
            define_sample(s);
            break;
        case SilOp_Ignore:
            evaluate(s);
            break;
        default:
            fail(s);
            break;
    }
    return true;
}

static bool run(SilScript* s) {
    while (!s->failed) {
        if (*s->steps_left == 0) {
            return false;
        }
        (*s->steps_left)--;
        s->nesting = 0;
        if (!statement(s)) {
            return !s->failed;
        }
    }
    return false;
}

static bool run_at(SilScript* s, uint32_t pc) {
    s->pc = pc;
    s->depth = 0;
    s->value = 0;
    return run(s);
}

bool sil_script_run_song(SilScript* s, uint16_t flag, uint16_t subsong) {
    memset(s->variables, 0, sizeof(s->variables));
    variable_write(s, SIL_FLAG_VARIABLE, 2, (int16_t)(flag + subsong));
    s->blocks = s->song;
    uint32_t pc = sil_image_base(s->song) + SIL_SCRIPT_START;
    if (sil_u16(s->memory, pc) == SIL_SCRIPT_MARK) {
        pc += 2;
        s->slot_parameters = false;
    }
    return run_at(s, pc);
}

bool sil_script_run_main(SilScript* s, const SilImage* main) {
    s->blocks = main;
    return run_at(s, sil_image_base(main) + SIL_SCRIPT_START);
}

bool sil_script_continue(SilScript* s) {
    s->value = 0;
    return run(s);
}

bool sil_script_slots_unset(const SilScript* s) {
    for (uint32_t i = 1; i < SIL_SLOTS; i++) {
        if (s->slots[i].sample != s->slots[0].sample) {
            return false;
        }
    }
    return true;
}

void sil_script_note_parameters(SilScript* s) {
    s->slot_parameters = false;
    for (uint32_t i = 0; i < SIL_SLOTS; i++) {
        if (s->slots[i].note_base != 0) {
            s->slot_parameters = true;
        }
    }
}

bool sil_script_starts_with_sample(const SilScript* s) {
    const uint32_t at = s->song->start + SIL_SCRIPT_START;
    return sil_u8(s->memory, at) == SilOp_Sample && sil_u8(s->memory, at + 1) == SilExpr_Byte;
}
