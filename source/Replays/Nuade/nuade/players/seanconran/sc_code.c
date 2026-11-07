// SPDX-License-Identifier: MIT
// Instruction decoder and code hash for the replayers of the Sean Conran modules (see scr_code.h).
//
// The replayers are a few thousand bytes of 68000 code that keeps its variables inside its own instructions: moves,
// adds, compares, shifts, branches and jumps through a table of `bra.w`. The decoder knows the length of each such
// instruction and which of its words are operands that belong to the module and not to the replayer. Anything else (an
// unknown opcode, a 68020 addressing mode) fails the scan.

#include "sc_code.h"

#include "nuade_utils.h"

#define MAX_INSTRUCTIONS 4096
#define MAX_INSTRUCTION_WORDS 6

#define OP_BRA_W 0x6000
#define OP_JMP_PC_INDEX 0x4EFB
#define OP_LEA_PC_A1 0x43FA
#define OP_LEA_DISPLACEMENT_A1 0x43E8 // lea d16(an),a1: the low three bits are an
#define OP_MOVE_BYTE_POSTINC 0x12D8   // move.b (a0)+,(a1)+
#define MAX_TABLE_ENTRIES 64
#define MAX_TABLE_INLINE_BYTES 4
// The scratch bytes are 3 or 4 bytes copied into by `move.b (a0)+,(a1)+`; the code ends 3 bytes in front of the first.
#define SCRATCH_TO_CODE_END 3

#define SIZE_BYTE 1
#define SIZE_WORD 2
#define SIZE_LONG 4

// Address modes of an effective address (bits 3..5 of the opcode; mode 7 selects by the register field).
#define MODE_ADDRESS_REG 1
#define MODE_DISPLACEMENT 5
#define MODE_INDEX 6
#define MODE_OTHER 7
#define REG_ABSOLUTE_WORD 0
#define REG_ABSOLUTE_LONG 1
#define REG_PC_DISPLACEMENT 2
#define REG_PC_INDEX 3
#define REG_IMMEDIATE 4

#define BRIEF_EXTENSION_FULL_FORMAT 0x0100 // bit 8 of an index extension word: 68020 formats
#define CHIP_REGISTER_PAGE_HIGH_BYTE 0xBF  // an absolute address from 0xBF0000 up is the CIAs or the custom chips

// What a word of an instruction is, for the hash.
typedef enum WordKind {
    WordKind_Plain,   // part of the replayer
    WordKind_Address, // a word of an absolute address that is overwritten before it is read
} WordKind;

typedef struct Instruction {
    uint32_t at;
    int word_count;
    uint16_t word[MAX_INSTRUCTION_WORDS];
    WordKind kind[MAX_INSTRUCTION_WORDS];
} Instruction;

typedef struct Cursor {
    const uint8_t* data;
    size_t size;
    Instruction* insn;
} Cursor;

// Takes the next word of the instruction.
static bool take_word(Cursor* c, WordKind kind) {
    Instruction* in = c->insn;
    const uint32_t at = in->at + 2u * (uint32_t)in->word_count;
    if (in->word_count == MAX_INSTRUCTION_WORDS || at > c->size || c->size - at < 2) {
        return false;
    }
    in->word[in->word_count] = nuade_read_be16(c->data + at);
    in->kind[in->word_count] = kind;
    in->word_count++;
    return true;
}

// The brief extension word of an indexed address; the 68020 formats are not part of the replayers.
static bool take_index_extension(Cursor* c) {
    return take_word(c, WordKind_Plain) && (c->insn->word[c->insn->word_count - 1] & BRIEF_EXTENSION_FULL_FORMAT) == 0;
}

// An absolute long address. One in the chip register pages is part of the replayer, any other one belongs to the
// module.
static bool take_absolute_long(Cursor* c) {
    Instruction* in = c->insn;
    if (!take_word(c, WordKind_Address) || !take_word(c, WordKind_Address)) {
        return false;
    }
    if ((in->word[in->word_count - 2] & 0xFF) >= CHIP_REGISTER_PAGE_HIGH_BYTE) {
        in->kind[in->word_count - 2] = WordKind_Plain;
        in->kind[in->word_count - 1] = WordKind_Plain;
    }
    return true;
}

// The extension words of an effective address for an operand of `size` bytes.
static bool take_address_extension(Cursor* c, int mode, int reg, int size) {
    switch (mode) {
        case MODE_DISPLACEMENT:
            return take_word(c, WordKind_Plain);
        case MODE_INDEX:
            return take_index_extension(c);
        case MODE_OTHER:
            switch (reg) {
                case REG_ABSOLUTE_WORD:
                    return take_word(c, WordKind_Plain);
                case REG_ABSOLUTE_LONG:
                    return take_absolute_long(c);
                case REG_PC_DISPLACEMENT:
                    return take_word(c, WordKind_Plain);
                case REG_PC_INDEX:
                    return take_index_extension(c);
                case REG_IMMEDIATE:
                    return take_word(c, WordKind_Plain) && (size != SIZE_LONG || take_word(c, WordKind_Plain));
                default:
                    return false;
            }
        default:
            return true; // register direct, indirect, post-increment, pre-decrement
    }
}

static int ea_mode(uint16_t op) {
    return (op >> 3) & 7;
}

static int ea_reg(uint16_t op) {
    return op & 7;
}

static bool take_ea(Cursor* c, uint16_t op, int size) {
    return take_address_extension(c, ea_mode(op), ea_reg(op), size);
}

// The operand size in bits 6..7 of the opcode; 0 for the encoding that selects another instruction.
static int size_of_field(uint16_t op) {
    switch ((op >> 6) & 3) {
        case 0:
            return SIZE_BYTE;
        case 1:
            return SIZE_WORD;
        case 2:
            return SIZE_LONG;
        default:
            return 0;
    }
}

static bool decode_immediate_and_bit_operations(Cursor* c, uint16_t op) {
    if ((op & 0x0100) != 0) {
        // btst/bchg/bclr/bset Dn,<ea>; mode 1 would be movep
        return ea_mode(op) != MODE_ADDRESS_REG && take_ea(c, op, SIZE_BYTE);
    }
    if ((op & 0xFF00) == 0x0800) {
        // the same with a bit number in the next word
        return take_word(c, WordKind_Plain) && take_ea(c, op, SIZE_BYTE);
    }
    const int size = size_of_field(op);
    const int operation = (op >> 9) & 7; // ori, andi, subi, addi, (bit), eori, cmpi
    if (size == 0 || operation == 4 || operation == 7) {
        return false;
    }
    return take_word(c, WordKind_Plain) && (size != SIZE_LONG || take_word(c, WordKind_Plain)) && take_ea(c, op, size);
}

static bool decode_move(Cursor* c, uint16_t op) {
    int size = 0;
    switch (op >> 12) {
        case 1:
            size = SIZE_BYTE;
            break;
        case 3:
            size = SIZE_WORD;
            break;
        default:
            size = SIZE_LONG;
            break;
    }
    const int destination_mode = (op >> 6) & 7;
    const int destination_reg = (op >> 9) & 7;
    if (!take_ea(c, op, size)) {
        return false;
    }
    return take_address_extension(c, destination_mode, destination_reg, size);
}

static bool decode_miscellaneous(Cursor* c, uint16_t op) {
    if ((op & 0xF1C0) == 0x41C0) {
        return take_ea(c, op, SIZE_LONG); // lea
    }
    if (op == 0x4E75 || op == 0x4E73 || op == 0x4E71) {
        return true; // rts, rte, nop
    }
    if ((op & 0xFFB8) == 0x4880) {
        return true; // ext.w, ext.l
    }
    if ((op & 0xFF00) == 0x4200 || (op & 0xFF00) == 0x4A00) {
        const int size = size_of_field(op); // clr, tst
        return size != 0 && take_ea(c, op, size);
    }
    if ((op & 0xFF80) == 0x4E80) {
        return take_ea(c, op, SIZE_LONG); // jsr, jmp
    }
    return false;
}

static bool decode_quick_and_conditions(Cursor* c, uint16_t op) {
    if ((op & 0x00C0) == 0x00C0) {
        if (ea_mode(op) == MODE_ADDRESS_REG) {
            return take_word(c, WordKind_Plain); // dbcc
        }
        return take_ea(c, op, SIZE_BYTE); // scc
    }
    const int size = size_of_field(op);
    return take_ea(c, op, size); // addq, subq
}

static bool decode_branch(Cursor* c, uint16_t op) {
    const int displacement = op & 0xFF;
    if (displacement == 0xFF) {
        return false; // 68020 long branch
    }
    return displacement != 0 || take_word(c, WordKind_Plain);
}

// The or, and, add, sub, cmp and eor group with its address register, multiply and divide forms.
static bool decode_arithmetic(Cursor* c, uint16_t op) {
    const int group = op >> 12;
    const int field = (op >> 6) & 7;
    if (field == 3 || field == 7) {
        // divu, divs, mulu, muls work on words; suba, cmpa, adda on the word or long of bit 8
        const bool word_operand = group == 0x8 || group == 0xC || field == 3;
        return take_ea(c, op, word_operand ? SIZE_WORD : SIZE_LONG);
    }
    // sbcd, abcd, addx, subx and exg take registers (mode 0 or 1) where cmpm and eor are the only ones with such modes
    const bool register_form = field >= 4 && ea_mode(op) <= MODE_ADDRESS_REG;
    if (register_form && (group != 0xB || ea_mode(op) == MODE_ADDRESS_REG)) {
        return false;
    }
    return take_ea(c, op, size_of_field(op));
}

static bool decode_instruction(Cursor* c) {
    if (!take_word(c, WordKind_Plain)) {
        return false;
    }
    const uint16_t op = c->insn->word[0];
    switch (op >> 12) {
        case 0x0:
            return decode_immediate_and_bit_operations(c, op);
        case 0x1:
        case 0x2:
        case 0x3:
            return decode_move(c, op);
        case 0x4:
            return decode_miscellaneous(c, op);
        case 0x5:
            return decode_quick_and_conditions(c, op);
        case 0x6:
            return decode_branch(c, op);
        case 0x7:
            return (op & 0x0100) == 0; // moveq
        case 0x8:
        case 0x9:
        case 0xB:
        case 0xC:
        case 0xD:
            return decode_arithmetic(c, op);
        case 0xE:
            return (op & 0x00C0) != 0x00C0 || take_ea(c, op, SIZE_WORD);
        default:
            return false;
    }
}

static uint64_t hash_byte(uint64_t hash, uint8_t value) {
    return (hash ^ value) * 0x100000001B3ull;
}

static uint64_t hash_token(uint64_t hash, WordKind kind, uint16_t value) {
    hash = hash_byte(hash, (uint8_t)kind);
    hash = hash_byte(hash, (uint8_t)(value >> 8));
    return hash_byte(hash, (uint8_t)value);
}

static uint64_t hash_instruction(uint64_t hash, const Instruction* in) {
    for (int w = 0; w < in->word_count; w++) {
        hash = hash_token(hash, in->kind[w], in->kind[w] == WordKind_Address ? 0 : in->word[w]);
    }
    return hash;
}

// The offset of the scratch byte a `lea` names for `move.b (a0)+,(a1)+`; 0 when the instruction is not that.
static uint32_t scratch_target(const Instruction* in, const uint8_t* data, size_t size) {
    const uint32_t after = in->at + 4;
    if (in->word_count != 2 || (in->word[0] != OP_LEA_PC_A1 && (in->word[0] & 0xFFF8) != OP_LEA_DISPLACEMENT_A1)
        || size - after < 2 || nuade_read_be16(data + after) != OP_MOVE_BYTE_POSTINC) {
        return 0;
    }
    return in->word[0] == OP_LEA_PC_A1 ? in->at + 2u + (uint32_t)(int32_t)(int16_t)in->word[1] : in->word[1];
}

// Hashes the words of a table jump: the bytes the instruction skips over and the `bra.w` entries behind it. Returns
// the offset behind the table, 0 when the layout is not the replayer's.
static uint32_t hash_jump_table(const uint8_t* data, size_t size, const Instruction* jump, uint64_t* hash) {
    const uint32_t extension_at = jump->at + 2;
    const uint32_t table = extension_at + (uint32_t)(int32_t)(int8_t)(jump->word[1] & 0xFF);
    const uint32_t after_jump = jump->at + 4;
    if (table < after_jump || table - after_jump > MAX_TABLE_INLINE_BYTES || table > size || size - table < 4) {
        return 0;
    }
    uint32_t at = after_jump;
    for (; at < table; at++) {
        *hash = hash_token(*hash, WordKind_Plain, data[at]);
    }
    int entries = 0;
    while (size - at >= 4 && nuade_read_be16(data + at) == OP_BRA_W && entries < MAX_TABLE_ENTRIES) {
        *hash = hash_token(*hash, WordKind_Plain, OP_BRA_W);
        *hash = hash_token(*hash, WordKind_Plain, nuade_read_be16(data + at + 2));
        at += 4;
        entries++;
    }
    return entries > 0 ? at : 0;
}

bool scr_scan_code(const uint8_t* data, size_t size, uint32_t start, ScrCodeScan* scan) {
    Cursor cursor = { .data = data, .size = size };
    Instruction in = { 0 };
    uint64_t hash = 0xCBF29CE484222325ull;
    uint32_t at = start;
    uint32_t end = 0; // known once the code has named its scratch bytes
    int count = 0;
    while (count++ < MAX_INSTRUCTIONS) {
        if (end != 0 && at >= end) {
            break;
        }
        in = (Instruction) { .at = at };
        cursor.insn = &in;
        if (!decode_instruction(&cursor)) {
            return false;
        }
        hash = hash_instruction(hash, &in);
        at += 2u * (uint32_t)in.word_count;
        const uint32_t scratch = scratch_target(&in, data, size);
        if (scratch > SCRATCH_TO_CODE_END && (end == 0 || scratch - SCRATCH_TO_CODE_END < end)) {
            end = scratch - SCRATCH_TO_CODE_END;
        }
        if (in.word[0] == OP_JMP_PC_INDEX && in.word_count == 2) {
            at = hash_jump_table(data, size, &in, &hash);
            if (at == 0) {
                return false;
            }
        }
    }
    if (end == 0 || at != end) {
        return false;
    }
    *scan = (ScrCodeScan) { .end = end, .hash = hash };
    return true;
}
