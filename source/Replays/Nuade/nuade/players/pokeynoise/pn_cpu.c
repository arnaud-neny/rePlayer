// SPDX-License-Identifier: MIT
// The eagleplayer's 6502 interpreter, as it behaves: the 151 documented opcodes (anything else ends the routine as a
// fault), no decimal mode, BRK as a two byte no-operation, and the operating system's vertical blank calls
// (SETVBV, SYSVBV, XITVBV) done in the interpreter. The Atari binary is loaded into 64 KB that start cleared.
//
// The original keeps the 6502 flags in the 68000 condition codes. The 68000 ADDX/SUBX never set Z, they only clear it:
// after an ADC or SBC with a zero result, Z is therefore the Z that the previous flag-setting arithmetic, compare,
// shift or PLP/RTI left behind (`adc_zero`), not 1. (The carry of ADC and SBC is the 68000 extend flag, which equals
// the carry; UADE's 68000 core does not set X on SUB, so three songs differ from UADE.)

#include "pn_internal.h"

#include <string.h>

#define PN_GUARD_INSTRUCTIONS (1u << 20) // a routine that runs longer is a runaway (the original would hang the host)

// The stack lives at $0100..$01FF; its pointer is kept as the original keeps it, as an offset of 0x101 from the
// address of the lowest free byte, so a push writes at 0x100 + sp.
#define PN_STACK_PAGE 0x100u
#define PN_STACK_EMPTY 0xFF
#define PN_STACK_LOWEST (-1)
#define PN_STACK_HIGHEST 0xFF

// The deferred vertical blank vector of the operating system, where SETVBV puts a vector (the immediate one is
// PN_VVBLKI).
#define PN_VVBLKD 0x224u
#define PN_SETVBV_IMMEDIATE 6u
#define PN_SETVBV_DEFERRED 7u

// The hardware area of the Atari. Loads see two of its registers: RANDOM and VCOUNT.
#define PN_HW_FIRST 0xD000u
#define PN_HW_LAST 0xD7FFu
#define PN_RANDOM 0xD20Au
#define PN_VCOUNT 0xD40Bu
#define PN_BEAM_LINES 312u

// Every run starts by setting two bytes of the hardware area: a one at $D01E and the vertical blank flag in NMIST.
#define PN_CONSOL_SHADOW 0xD01Eu
#define PN_NMIST 0xD40Fu
#define PN_NMIST_VBI 0x40u

// The seed of the generator and its constants.
#define PN_RANDOM_SUM_SEED 0x3F3F751Fu
#define PN_RANDOM_MIX_SEED 0x17179834u
#define PN_RANDOM_STEP 0x56565311u
#define PN_RANDOM_NOISE 0xFFFFFFFFu // what the generator mixes in: two reads of the unimplemented register $DFF014

typedef enum PnOp {
    PnOp_Invalid,
    PnOp_Adc,
    PnOp_And,
    PnOp_Asl,
    PnOp_Bcc,
    PnOp_Bcs,
    PnOp_Beq,
    PnOp_Bit,
    PnOp_Bmi,
    PnOp_Bne,
    PnOp_Bpl,
    PnOp_Brk,
    PnOp_Bvc,
    PnOp_Bvs,
    PnOp_Clc,
    PnOp_Cld,
    PnOp_Cli,
    PnOp_Clv,
    PnOp_Cmp,
    PnOp_Cpx,
    PnOp_Cpy,
    PnOp_Dec,
    PnOp_Dex,
    PnOp_Dey,
    PnOp_Eor,
    PnOp_Inc,
    PnOp_Inx,
    PnOp_Iny,
    PnOp_Jmp,
    PnOp_Jsr,
    PnOp_Lda,
    PnOp_Ldx,
    PnOp_Ldy,
    PnOp_Lsr,
    PnOp_Nop,
    PnOp_Ora,
    PnOp_Pha,
    PnOp_Php,
    PnOp_Pla,
    PnOp_Plp,
    PnOp_Rol,
    PnOp_Ror,
    PnOp_Rti,
    PnOp_Rts,
    PnOp_Sbc,
    PnOp_Sec,
    PnOp_Sed,
    PnOp_Sei,
    PnOp_Sta,
    PnOp_Stx,
    PnOp_Sty,
    PnOp_Tax,
    PnOp_Tay,
    PnOp_Tsx,
    PnOp_Txa,
    PnOp_Txs,
    PnOp_Tya,
} PnOp;

typedef enum PnMode {
    PnMode_Implied,
    PnMode_Accumulator,
    PnMode_Immediate,
    PnMode_ZeroPage,
    PnMode_ZeroPageX,
    PnMode_ZeroPageY,
    PnMode_Absolute,
    PnMode_AbsoluteX,
    PnMode_AbsoluteY,
    PnMode_IndirectX,
    PnMode_IndirectY,
    PnMode_Indirect,
    PnMode_Relative,
} PnMode;

typedef struct PnOpcode {
    PnOp op;
    PnMode mode;
} PnOpcode;

#define OPCODE(op, mode) { PnOp_##op, PnMode_##mode }

// The documented 6502 opcodes by opcode number; every other entry is zero, PnOp_Invalid.
static const PnOpcode s_opcodes[256] = {
    [0x00] = OPCODE(Brk, Implied),     [0x01] = OPCODE(Ora, IndirectX), [0x05] = OPCODE(Ora, ZeroPage),
    [0x06] = OPCODE(Asl, ZeroPage),    [0x08] = OPCODE(Php, Implied),   [0x09] = OPCODE(Ora, Immediate),
    [0x0A] = OPCODE(Asl, Accumulator), [0x0D] = OPCODE(Ora, Absolute),  [0x0E] = OPCODE(Asl, Absolute),
    [0x10] = OPCODE(Bpl, Relative),    [0x11] = OPCODE(Ora, IndirectY), [0x15] = OPCODE(Ora, ZeroPageX),
    [0x16] = OPCODE(Asl, ZeroPageX),   [0x18] = OPCODE(Clc, Implied),   [0x19] = OPCODE(Ora, AbsoluteY),
    [0x1D] = OPCODE(Ora, AbsoluteX),   [0x1E] = OPCODE(Asl, AbsoluteX), [0x20] = OPCODE(Jsr, Absolute),
    [0x21] = OPCODE(And, IndirectX),   [0x24] = OPCODE(Bit, ZeroPage),  [0x25] = OPCODE(And, ZeroPage),
    [0x26] = OPCODE(Rol, ZeroPage),    [0x28] = OPCODE(Plp, Implied),   [0x29] = OPCODE(And, Immediate),
    [0x2A] = OPCODE(Rol, Accumulator), [0x2C] = OPCODE(Bit, Absolute),  [0x2D] = OPCODE(And, Absolute),
    [0x2E] = OPCODE(Rol, Absolute),    [0x30] = OPCODE(Bmi, Relative),  [0x31] = OPCODE(And, IndirectY),
    [0x35] = OPCODE(And, ZeroPageX),   [0x36] = OPCODE(Rol, ZeroPageX), [0x38] = OPCODE(Sec, Implied),
    [0x39] = OPCODE(And, AbsoluteY),   [0x3D] = OPCODE(And, AbsoluteX), [0x3E] = OPCODE(Rol, AbsoluteX),
    [0x40] = OPCODE(Rti, Implied),     [0x41] = OPCODE(Eor, IndirectX), [0x45] = OPCODE(Eor, ZeroPage),
    [0x46] = OPCODE(Lsr, ZeroPage),    [0x48] = OPCODE(Pha, Implied),   [0x49] = OPCODE(Eor, Immediate),
    [0x4A] = OPCODE(Lsr, Accumulator), [0x4C] = OPCODE(Jmp, Absolute),  [0x4D] = OPCODE(Eor, Absolute),
    [0x4E] = OPCODE(Lsr, Absolute),    [0x50] = OPCODE(Bvc, Relative),  [0x51] = OPCODE(Eor, IndirectY),
    [0x55] = OPCODE(Eor, ZeroPageX),   [0x56] = OPCODE(Lsr, ZeroPageX), [0x58] = OPCODE(Cli, Implied),
    [0x59] = OPCODE(Eor, AbsoluteY),   [0x5D] = OPCODE(Eor, AbsoluteX), [0x5E] = OPCODE(Lsr, AbsoluteX),
    [0x60] = OPCODE(Rts, Implied),     [0x61] = OPCODE(Adc, IndirectX), [0x65] = OPCODE(Adc, ZeroPage),
    [0x66] = OPCODE(Ror, ZeroPage),    [0x68] = OPCODE(Pla, Implied),   [0x69] = OPCODE(Adc, Immediate),
    [0x6A] = OPCODE(Ror, Accumulator), [0x6C] = OPCODE(Jmp, Indirect),  [0x6D] = OPCODE(Adc, Absolute),
    [0x6E] = OPCODE(Ror, Absolute),    [0x70] = OPCODE(Bvs, Relative),  [0x71] = OPCODE(Adc, IndirectY),
    [0x75] = OPCODE(Adc, ZeroPageX),   [0x76] = OPCODE(Ror, ZeroPageX), [0x78] = OPCODE(Sei, Implied),
    [0x79] = OPCODE(Adc, AbsoluteY),   [0x7D] = OPCODE(Adc, AbsoluteX), [0x7E] = OPCODE(Ror, AbsoluteX),
    [0x81] = OPCODE(Sta, IndirectX),   [0x84] = OPCODE(Sty, ZeroPage),  [0x85] = OPCODE(Sta, ZeroPage),
    [0x86] = OPCODE(Stx, ZeroPage),    [0x88] = OPCODE(Dey, Implied),   [0x8A] = OPCODE(Txa, Implied),
    [0x8C] = OPCODE(Sty, Absolute),    [0x8D] = OPCODE(Sta, Absolute),  [0x8E] = OPCODE(Stx, Absolute),
    [0x90] = OPCODE(Bcc, Relative),    [0x91] = OPCODE(Sta, IndirectY), [0x94] = OPCODE(Sty, ZeroPageX),
    [0x95] = OPCODE(Sta, ZeroPageX),   [0x96] = OPCODE(Stx, ZeroPageY), [0x98] = OPCODE(Tya, Implied),
    [0x99] = OPCODE(Sta, AbsoluteY),   [0x9A] = OPCODE(Txs, Implied),   [0x9D] = OPCODE(Sta, AbsoluteX),
    [0xA0] = OPCODE(Ldy, Immediate),   [0xA1] = OPCODE(Lda, IndirectX), [0xA2] = OPCODE(Ldx, Immediate),
    [0xA4] = OPCODE(Ldy, ZeroPage),    [0xA5] = OPCODE(Lda, ZeroPage),  [0xA6] = OPCODE(Ldx, ZeroPage),
    [0xA8] = OPCODE(Tay, Implied),     [0xA9] = OPCODE(Lda, Immediate), [0xAA] = OPCODE(Tax, Implied),
    [0xAC] = OPCODE(Ldy, Absolute),    [0xAD] = OPCODE(Lda, Absolute),  [0xAE] = OPCODE(Ldx, Absolute),
    [0xB0] = OPCODE(Bcs, Relative),    [0xB1] = OPCODE(Lda, IndirectY), [0xB4] = OPCODE(Ldy, ZeroPageX),
    [0xB5] = OPCODE(Lda, ZeroPageX),   [0xB6] = OPCODE(Ldx, ZeroPageY), [0xB8] = OPCODE(Clv, Implied),
    [0xB9] = OPCODE(Lda, AbsoluteY),   [0xBA] = OPCODE(Tsx, Implied),   [0xBC] = OPCODE(Ldy, AbsoluteX),
    [0xBD] = OPCODE(Lda, AbsoluteX),   [0xBE] = OPCODE(Ldx, AbsoluteY), [0xC0] = OPCODE(Cpy, Immediate),
    [0xC1] = OPCODE(Cmp, IndirectX),   [0xC4] = OPCODE(Cpy, ZeroPage),  [0xC5] = OPCODE(Cmp, ZeroPage),
    [0xC6] = OPCODE(Dec, ZeroPage),    [0xC8] = OPCODE(Iny, Implied),   [0xC9] = OPCODE(Cmp, Immediate),
    [0xCA] = OPCODE(Dex, Implied),     [0xCC] = OPCODE(Cpy, Absolute),  [0xCD] = OPCODE(Cmp, Absolute),
    [0xCE] = OPCODE(Dec, Absolute),    [0xD0] = OPCODE(Bne, Relative),  [0xD1] = OPCODE(Cmp, IndirectY),
    [0xD5] = OPCODE(Cmp, ZeroPageX),   [0xD6] = OPCODE(Dec, ZeroPageX), [0xD8] = OPCODE(Cld, Implied),
    [0xD9] = OPCODE(Cmp, AbsoluteY),   [0xDD] = OPCODE(Cmp, AbsoluteX), [0xDE] = OPCODE(Dec, AbsoluteX),
    [0xE0] = OPCODE(Cpx, Immediate),   [0xE1] = OPCODE(Sbc, IndirectX), [0xE4] = OPCODE(Cpx, ZeroPage),
    [0xE5] = OPCODE(Sbc, ZeroPage),    [0xE6] = OPCODE(Inc, ZeroPage),  [0xE8] = OPCODE(Inx, Implied),
    [0xE9] = OPCODE(Sbc, Immediate),   [0xEA] = OPCODE(Nop, Implied),   [0xEC] = OPCODE(Cpx, Absolute),
    [0xED] = OPCODE(Sbc, Absolute),    [0xEE] = OPCODE(Inc, Absolute),  [0xF0] = OPCODE(Beq, Relative),
    [0xF1] = OPCODE(Sbc, IndirectY),   [0xF5] = OPCODE(Sbc, ZeroPageX), [0xF6] = OPCODE(Inc, ZeroPageX),
    [0xF8] = OPCODE(Sed, Implied),     [0xF9] = OPCODE(Sbc, AbsoluteY), [0xFD] = OPCODE(Sbc, AbsoluteX),
    [0xFE] = OPCODE(Inc, AbsoluteX),
};

#undef OPCODE

// The registers and flags of one run. `misc` holds the two status bits that no instruction uses but PHP and PLP
// carry; `adc_zero` is the Z that the next ADC or SBC leaves when its result is zero.
typedef struct PnCpu {
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    int32_t sp;
    bool carry;
    bool negative;
    bool zero;
    bool overflow;
    bool decimal;
    bool interrupt_disable;
    bool adc_zero;
    uint8_t misc;
    PnRunResult result;
    bool finished;
} PnCpu;

#define PN_STATUS_CARRY 0x01u
#define PN_STATUS_ZERO 0x02u
#define PN_STATUS_INTERRUPT 0x04u
#define PN_STATUS_DECIMAL 0x08u
#define PN_STATUS_BREAK 0x10u
#define PN_STATUS_UNUSED 0x20u
#define PN_STATUS_OVERFLOW 0x40u
#define PN_STATUS_NEGATIVE 0x80u

void pn_machine_reset(PnMachine* machine) {
    memset(machine, 0, sizeof(*machine));
    machine->random_sum = PN_RANDOM_SUM_SEED;
    machine->random_mix = PN_RANDOM_MIX_SEED;
}

static uint8_t file_byte(const uint8_t* file, size_t size, size_t at) {
    return at < size ? file[at] : 0;
}

// The segment loop of the eagleplayer: an optional 0xFFFF marker, the first and the last address, then the bytes; a
// last address below the first one still loads one byte, and the address counter wraps at 64 KB. It stops when the file
// is used up, which it only checks before a byte, so the header of a segment can run past the end.

void pn_machine_load(PnMachine* machine, const uint8_t* file, size_t size) {
    size_t at = 0;
    for (;;) {
        if (file_byte(file, size, at) == 0xFF && file_byte(file, size, at + 1) == 0xFF) {
            at += 2;
        }
        const uint16_t first = (uint16_t)(file_byte(file, size, at) | (file_byte(file, size, at + 1) << 8));
        const uint16_t last = (uint16_t)(file_byte(file, size, at + 2) | (file_byte(file, size, at + 3) << 8));
        at += 4;
        uint16_t address = first;
        for (;;) {
            if (at >= size) {
                return;
            }
            machine->ram[address] = file[at++];
            address = (uint16_t)(address + 1);
            if (last < address) {
                break;
            }
        }
    }
}

uint16_t pn_machine_u16(const PnMachine* machine, uint16_t address) {
    return (uint16_t)(machine->ram[address] | (machine->ram[(uint16_t)(address + 1)] << 8));
}

// RANDOM: one step of the eagleplayer's generator. The word it mixes in from $DFF014 is 0xFFFF on every read.

static uint32_t random_step(PnMachine* machine) {
    uint32_t value = machine->random_sum + machine->random_mix;
    machine->random_sum = value;
    value = (value >> 4) | (value << 28);
    value = (value & 0xFFFF0000u) | (uint16_t)((uint16_t)value - (uint16_t)(machine->random_mix >> 16));
    value ^= PN_RANDOM_NOISE;
    machine->random_mix ^= value;
    machine->random_sum += PN_RANDOM_STEP;
    return value;
}

uint8_t pn_machine_random_byte(PnMachine* machine) {
    return (uint8_t)random_step(machine);
}

// A read of the hardware area $D000..$D7FF: RANDOM and VCOUNT are answered, the rest is the memory.

static uint8_t hardware_read(PnMachine* machine, uint16_t address) {
    if (address == PN_RANDOM) {
        const uint16_t word = (uint16_t)random_step(machine);
        return (uint8_t)(word >> 5);
    }
    if (address == PN_VCOUNT) {
        // The beam position in UADE depends on how long the original has run; this advances one line per read.
        const uint16_t line = machine->beam_line;
        machine->beam_line = (uint16_t)((line + 1) % PN_BEAM_LINES);
        return (uint8_t)(line >> 1);
    }
    return machine->ram[address];
}

static bool is_hardware(uint16_t address) {
    return address >= PN_HW_FIRST && address <= PN_HW_LAST;
}

static uint8_t fetch(const PnMachine* machine, PnCpu* cpu) {
    const uint8_t value = machine->ram[cpu->pc];
    cpu->pc = (uint16_t)(cpu->pc + 1);
    return value;
}

static uint16_t fetch_word(const PnMachine* machine, PnCpu* cpu) {
    const uint8_t low = fetch(machine, cpu);
    const uint8_t high = fetch(machine, cpu);
    return (uint16_t)(low | (high << 8));
}

// The address an instruction works on. The pointer of the indirect modes is read without the page wrap of the 6502.

static uint16_t effective_address(const PnMachine* machine, PnCpu* cpu, PnMode mode) {
    uint32_t sum;
    switch (mode) {
        case PnMode_ZeroPage:
            return fetch(machine, cpu);
        case PnMode_ZeroPageX:
            sum = (uint32_t)fetch(machine, cpu) + cpu->x;
            return (uint8_t)sum;
        case PnMode_ZeroPageY:
            sum = (uint32_t)fetch(machine, cpu) + cpu->y;
            return (uint8_t)sum;
        case PnMode_Absolute:
            return fetch_word(machine, cpu);
        case PnMode_AbsoluteX:
            sum = (uint32_t)fetch_word(machine, cpu) + cpu->x;
            return (uint16_t)sum;
        case PnMode_AbsoluteY:
            sum = (uint32_t)fetch_word(machine, cpu) + cpu->y;
            return (uint16_t)sum;
        case PnMode_IndirectX:
            sum = (uint32_t)fetch(machine, cpu) + cpu->x;
            return pn_machine_u16(machine, (uint8_t)sum);
        case PnMode_IndirectY:
            sum = (uint32_t)pn_machine_u16(machine, fetch(machine, cpu)) + cpu->y;
            return (uint16_t)sum;
        default:
            return 0;
    }
}

// Loads from the absolute and indirect modes see RANDOM and VCOUNT; the zero page and immediate modes do not.

static bool load_reads_hardware(PnMode mode) {
    return mode == PnMode_Absolute || mode == PnMode_AbsoluteX || mode == PnMode_AbsoluteY || mode == PnMode_IndirectX
           || mode == PnMode_IndirectY;
}

static uint8_t read_operand(PnMachine* machine, PnCpu* cpu, PnMode mode) {
    if (mode == PnMode_Immediate) {
        return fetch(machine, cpu);
    }
    return machine->ram[effective_address(machine, cpu, mode)];
}

static uint8_t read_load_operand(PnMachine* machine, PnCpu* cpu, PnMode mode) {
    if (mode == PnMode_Immediate) {
        return fetch(machine, cpu);
    }
    const uint16_t address = effective_address(machine, cpu, mode);
    if (load_reads_hardware(mode) && is_hardware(address)) {
        return hardware_read(machine, address);
    }
    return machine->ram[address];
}

static void set_negative_zero(PnCpu* cpu, uint8_t value) {
    cpu->negative = (value & 0x80) != 0;
    cpu->zero = value == 0;
}

// ADC and SBC: binary arithmetic. A zero result leaves Z as the last flag-setting instruction had it (see above).

static void add_with_carry(PnCpu* cpu, uint8_t operand) {
    const uint32_t sum = (uint32_t)cpu->a + operand + (cpu->carry ? 1u : 0u);
    const uint8_t result = (uint8_t)sum;
    cpu->overflow = ((cpu->a ^ result) & (operand ^ result) & 0x80) != 0;
    cpu->carry = sum > 0xFF;
    cpu->a = result;
    cpu->negative = (result & 0x80) != 0;
    cpu->adc_zero = result == 0 && cpu->adc_zero;
    cpu->zero = cpu->adc_zero;
}

static void subtract_with_borrow(PnCpu* cpu, uint8_t operand) {
    const int32_t difference = (int32_t)cpu->a - operand - (cpu->carry ? 0 : 1);
    const uint8_t result = (uint8_t)difference;
    cpu->overflow = ((cpu->a ^ operand) & (cpu->a ^ result) & 0x80) != 0;
    cpu->carry = difference >= 0;
    cpu->a = result;
    cpu->negative = (result & 0x80) != 0;
    cpu->adc_zero = result == 0 && cpu->adc_zero;
    cpu->zero = cpu->adc_zero;
}

// Flags of a compare, a shift or a rotate: N and Z of the result, and Z is what a later ADC or SBC falls back on.

static void set_flags_with_snapshot(PnCpu* cpu, uint8_t result) {
    set_negative_zero(cpu, result);
    cpu->adc_zero = cpu->zero;
}

static void compare(PnCpu* cpu, uint8_t reg, uint8_t operand) {
    cpu->carry = reg >= operand;
    set_flags_with_snapshot(cpu, (uint8_t)(reg - operand));
}

// CMP, CPX and CPY with an absolute address also see RANDOM and VCOUNT.

static void compare_absolute(PnMachine* machine, PnCpu* cpu, uint8_t reg) {
    const uint16_t address = fetch_word(machine, cpu);
    compare(cpu, reg, is_hardware(address) ? hardware_read(machine, address) : machine->ram[address]);
}

static uint8_t shift(PnCpu* cpu, PnOp op, uint8_t value) {
    uint8_t result;
    switch (op) {
        case PnOp_Asl:
            cpu->carry = (value & 0x80) != 0;
            result = (uint8_t)(value << 1);
            break;
        case PnOp_Lsr:
            cpu->carry = (value & 0x01) != 0;
            result = (uint8_t)(value >> 1);
            break;
        case PnOp_Rol:
            result = (uint8_t)((value << 1) | (cpu->carry ? 1 : 0));
            cpu->carry = (value & 0x80) != 0;
            break;
        default: // PnOp_Ror
            result = (uint8_t)((value >> 1) | (cpu->carry ? 0x80 : 0));
            cpu->carry = (value & 0x01) != 0;
            break;
    }
    set_flags_with_snapshot(cpu, result);
    return result;
}

static void branch(const PnMachine* machine, PnCpu* cpu, bool taken) {
    const int8_t offset = (int8_t)fetch(machine, cpu);
    if (taken) {
        cpu->pc = (uint16_t)(cpu->pc + offset);
    }
}

// The stack. A push writes before it checks for the overflow, so the fault leaves the byte behind.

static bool push(PnMachine* machine, PnCpu* cpu, uint8_t value) {
    machine->ram[(uint16_t)(PN_STACK_PAGE + cpu->sp)] = value;
    cpu->sp--;
    return cpu->sp >= PN_STACK_LOWEST;
}

static uint8_t pop_byte(const PnMachine* machine, PnCpu* cpu) {
    const uint8_t value = machine->ram[(uint16_t)(PN_STACK_PAGE + 1 + cpu->sp)];
    cpu->sp++;
    return value;
}

// PHP also overwrites the original's snapshot of the condition codes, so Z in it is the Z of the moment.

static uint8_t status_byte(const PnCpu* cpu) {
    return (uint8_t)(cpu->misc | (cpu->overflow ? PN_STATUS_OVERFLOW : 0) | (cpu->decimal ? PN_STATUS_DECIMAL : 0)
                     | (cpu->interrupt_disable ? PN_STATUS_INTERRUPT : 0) | (cpu->negative ? PN_STATUS_NEGATIVE : 0)
                     | (cpu->zero ? PN_STATUS_ZERO : 0) | (cpu->carry ? PN_STATUS_CARRY : 0));
}

// PLP and RTI: bit 5 is always set; Z is the Z of the next ADC or SBC with a zero result as well.

static void set_status(PnCpu* cpu, uint8_t status) {
    const uint8_t bits = status | PN_STATUS_UNUSED;
    cpu->misc = bits & (PN_STATUS_UNUSED | PN_STATUS_BREAK);
    cpu->overflow = (bits & PN_STATUS_OVERFLOW) != 0;
    cpu->decimal = (bits & PN_STATUS_DECIMAL) != 0;
    cpu->interrupt_disable = (bits & PN_STATUS_INTERRUPT) != 0;
    cpu->negative = (bits & PN_STATUS_NEGATIVE) != 0;
    cpu->zero = (bits & PN_STATUS_ZERO) != 0;
    cpu->carry = (bits & PN_STATUS_CARRY) != 0;
    cpu->adc_zero = cpu->zero;
}

static void finish(PnCpu* cpu, PnRunResult result) {
    cpu->finished = true;
    cpu->result = result;
}

// A call of SETVBV with A = 6 or 7: X and Y are the high and low byte of the vector.

static void set_vertical_blank(PnMachine* machine, const PnCpu* cpu) {
    if (cpu->a == PN_SETVBV_IMMEDIATE) {
        machine->ram[PN_VVBLKI + 1] = cpu->x;
        machine->ram[PN_VVBLKI] = cpu->y;
    } else if (cpu->a == PN_SETVBV_DEFERRED) {
        machine->ram[PN_VVBLKD + 1] = cpu->x;
        machine->ram[PN_VVBLKD] = cpu->y;
    }
}

// RTS. The routine is finished when the stack holds less than a return address: there is nothing to return to.

static void return_from_subroutine(const PnMachine* machine, PnCpu* cpu) {
    const uint16_t target = pn_machine_u16(machine, (uint16_t)(PN_STACK_PAGE + 1 + cpu->sp));
    cpu->sp += 2;
    if (cpu->sp > PN_STACK_HIGHEST) {
        finish(cpu, PnRun_Done);
        return;
    }
    cpu->pc = (uint16_t)(target + 1);
}

static void return_from_interrupt(const PnMachine* machine, PnCpu* cpu) {
    set_status(cpu, pop_byte(machine, cpu));
    const uint16_t target = pn_machine_u16(machine, (uint16_t)(PN_STACK_PAGE + 1 + cpu->sp));
    cpu->sp += 2;
    if (cpu->sp > PN_STACK_HIGHEST) {
        finish(cpu, PnRun_Done);
        return;
    }
    cpu->pc = target;
}

// JMP absolute. The operating system's vertical blank exits end the routine; SETVBV sets the vector and returns to the
// caller of the routine.

static void jump_absolute(PnMachine* machine, PnCpu* cpu) {
    const uint16_t target = fetch_word(machine, cpu);
    if (target == PN_OS_XITVBV || target == PN_OS_SYSVBV) {
        finish(cpu, PnRun_Done);
    } else if (target == PN_OS_SETVBV) {
        set_vertical_blank(machine, cpu);
        return_from_subroutine(machine, cpu);
    } else {
        cpu->pc = target;
    }
}

// JMP (pointer). The high byte of a pointer at the end of a page is read from the start of the same page.

static void jump_indirect(PnMachine* machine, PnCpu* cpu) {
    const uint16_t pointer = fetch_word(machine, cpu);
    const uint16_t high_at = (pointer & 0xFFu) == 0xFFu ? (uint16_t)(pointer & 0xFF00u) : (uint16_t)(pointer + 1);
    const uint16_t target = (uint16_t)(machine->ram[pointer] | (machine->ram[high_at] << 8));
    if (target == PN_OS_XITVBV || target == PN_OS_SYSVBV) {
        finish(cpu, PnRun_Done);
    } else {
        cpu->pc = target;
    }
}

// JSR pushes the address of its last byte. A call of SETVBV does what it does for the song and goes on.

static void jump_subroutine(PnMachine* machine, PnCpu* cpu) {
    const uint16_t target = fetch_word(machine, cpu);
    if (target == PN_OS_SETVBV) {
        set_vertical_blank(machine, cpu);
        return;
    }
    const uint16_t last_byte = (uint16_t)(cpu->pc - 1);
    machine->ram[(uint16_t)(PN_STACK_PAGE + cpu->sp)] = (uint8_t)(last_byte >> 8);
    machine->ram[(uint16_t)(PN_STACK_PAGE + cpu->sp - 1)] = (uint8_t)last_byte;
    cpu->sp -= 2;
    if (cpu->sp < PN_STACK_LOWEST) {
        finish(cpu, PnRun_Fault);
        return;
    }
    cpu->pc = target;
}

static void push_checked(PnMachine* machine, PnCpu* cpu, uint8_t value) {
    if (!push(machine, cpu, value)) {
        finish(cpu, PnRun_Fault);
    }
}

// PLA and PLP: popping past the stack's top is a fault, popping the one byte below it is not.

static bool pop_checked(const PnMachine* machine, PnCpu* cpu, uint8_t* value) {
    if (cpu->sp > PN_STACK_HIGHEST) {
        finish(cpu, PnRun_Fault);
        return false;
    }
    *value = pop_byte(machine, cpu);
    return true;
}

// The operations that read an operand and combine it with the accumulator.

static void accumulator_operation(PnMachine* machine, PnCpu* cpu, PnOp op, PnMode mode) {
    const uint8_t operand = read_operand(machine, cpu, mode);
    switch (op) {
        case PnOp_And:
            cpu->a &= operand;
            break;
        case PnOp_Ora:
            cpu->a |= operand;
            break;
        case PnOp_Eor:
            cpu->a ^= operand;
            break;
        case PnOp_Adc:
            add_with_carry(cpu, operand);
            return;
        default: // PnOp_Sbc
            subtract_with_borrow(cpu, operand);
            return;
    }
    set_negative_zero(cpu, cpu->a);
}

static void compare_operation(PnMachine* machine, PnCpu* cpu, PnOp op, PnMode mode) {
    const uint8_t reg = op == PnOp_Cmp ? cpu->a : (op == PnOp_Cpx ? cpu->x : cpu->y);
    if (mode == PnMode_Absolute) {
        compare_absolute(machine, cpu, reg);
        return;
    }
    const uint8_t operand = read_operand(machine, cpu, mode);
    compare(cpu, reg, operand);
}

// ASL, LSR, ROL and ROR on the accumulator or on memory; INC and DEC on memory.

static void memory_operation(PnMachine* machine, PnCpu* cpu, PnOp op, PnMode mode) {
    if (mode == PnMode_Accumulator) {
        cpu->a = shift(cpu, op, cpu->a);
        return;
    }
    const uint16_t address = effective_address(machine, cpu, mode);
    const uint8_t value = machine->ram[address];
    uint8_t result;
    if (op == PnOp_Inc) {
        result = (uint8_t)(value + 1);
        set_negative_zero(cpu, result);
    } else if (op == PnOp_Dec) {
        result = (uint8_t)(value - 1);
        set_negative_zero(cpu, result);
    } else {
        result = shift(cpu, op, value);
    }
    machine->ram[address] = result;
}

static void bit_test(PnMachine* machine, PnCpu* cpu, PnMode mode) {
    const uint8_t value = read_operand(machine, cpu, mode);
    cpu->overflow = (value & 0x40) != 0;
    cpu->negative = (value & 0x80) != 0;
    cpu->zero = (value & cpu->a) == 0;
}

static void store(PnMachine* machine, PnCpu* cpu, PnOp op, PnMode mode) {
    const uint16_t address = effective_address(machine, cpu, mode);
    const uint8_t value = op == PnOp_Sta ? cpu->a : (op == PnOp_Stx ? cpu->x : cpu->y);
    machine->ram[address] = value;
}

static void load(PnMachine* machine, PnCpu* cpu, PnOp op, PnMode mode) {
    const uint8_t value = read_load_operand(machine, cpu, mode);
    uint8_t* target = op == PnOp_Lda ? &cpu->a : (op == PnOp_Ldx ? &cpu->x : &cpu->y);
    *target = value;
    set_negative_zero(cpu, value);
}

static void step_register(PnCpu* cpu, uint8_t* reg, int delta) {
    *reg = (uint8_t)(*reg + delta);
    set_negative_zero(cpu, *reg);
}

static void transfer(PnCpu* cpu, PnOp op) {
    switch (op) {
        case PnOp_Tax:
            cpu->x = cpu->a;
            set_negative_zero(cpu, cpu->x);
            break;
        case PnOp_Tay:
            cpu->y = cpu->a;
            set_negative_zero(cpu, cpu->y);
            break;
        case PnOp_Txa:
            cpu->a = cpu->x;
            set_negative_zero(cpu, cpu->a);
            break;
        case PnOp_Tya:
            cpu->a = cpu->y;
            set_negative_zero(cpu, cpu->a);
            break;
        case PnOp_Tsx:
            cpu->x = (uint8_t)cpu->sp;
            set_negative_zero(cpu, cpu->x);
            break;
        default: // PnOp_Txs
            cpu->sp = cpu->x;
            break;
    }
}

static void execute(PnMachine* machine, PnCpu* cpu, PnOpcode opcode) {
    const PnOp op = opcode.op;
    const PnMode mode = opcode.mode;
    uint8_t value;
    switch (op) {
        case PnOp_Invalid:
            finish(cpu, PnRun_Fault);
            break;
        case PnOp_Adc:
        case PnOp_And:
        case PnOp_Eor:
        case PnOp_Ora:
        case PnOp_Sbc:
            accumulator_operation(machine, cpu, op, mode);
            break;
        case PnOp_Cmp:
        case PnOp_Cpx:
        case PnOp_Cpy:
            compare_operation(machine, cpu, op, mode);
            break;
        case PnOp_Asl:
        case PnOp_Lsr:
        case PnOp_Rol:
        case PnOp_Ror:
        case PnOp_Inc:
        case PnOp_Dec:
            memory_operation(machine, cpu, op, mode);
            break;
        case PnOp_Bit:
            bit_test(machine, cpu, mode);
            break;
        case PnOp_Lda:
        case PnOp_Ldx:
        case PnOp_Ldy:
            load(machine, cpu, op, mode);
            break;
        case PnOp_Sta:
        case PnOp_Stx:
        case PnOp_Sty:
            store(machine, cpu, op, mode);
            break;
        case PnOp_Bcc:
            branch(machine, cpu, !cpu->carry);
            break;
        case PnOp_Bcs:
            branch(machine, cpu, cpu->carry);
            break;
        case PnOp_Beq:
            branch(machine, cpu, cpu->zero);
            break;
        case PnOp_Bne:
            branch(machine, cpu, !cpu->zero);
            break;
        case PnOp_Bmi:
            branch(machine, cpu, cpu->negative);
            break;
        case PnOp_Bpl:
            branch(machine, cpu, !cpu->negative);
            break;
        case PnOp_Bvs:
            branch(machine, cpu, cpu->overflow);
            break;
        case PnOp_Bvc:
            branch(machine, cpu, !cpu->overflow);
            break;
        case PnOp_Brk: // a two byte no-operation
            cpu->pc = (uint16_t)(cpu->pc + 1);
            break;
        case PnOp_Nop:
            break;
        case PnOp_Clc:
            cpu->carry = false;
            break;
        case PnOp_Sec:
            cpu->carry = true;
            break;
        case PnOp_Cld:
            cpu->decimal = false;
            break;
        case PnOp_Sed:
            cpu->decimal = true;
            break;
        case PnOp_Cli:
            cpu->interrupt_disable = false;
            break;
        case PnOp_Sei:
            cpu->interrupt_disable = true;
            break;
        case PnOp_Clv:
            cpu->overflow = false;
            break;
        case PnOp_Inx:
            step_register(cpu, &cpu->x, 1);
            break;
        case PnOp_Iny:
            step_register(cpu, &cpu->y, 1);
            break;
        case PnOp_Dex:
            step_register(cpu, &cpu->x, -1);
            break;
        case PnOp_Dey:
            step_register(cpu, &cpu->y, -1);
            break;
        case PnOp_Tax:
        case PnOp_Tay:
        case PnOp_Tsx:
        case PnOp_Txa:
        case PnOp_Txs:
        case PnOp_Tya:
            transfer(cpu, op);
            break;
        case PnOp_Pha:
            push_checked(machine, cpu, cpu->a);
            break;
        case PnOp_Php:
            cpu->adc_zero = cpu->zero;
            push_checked(machine, cpu, status_byte(cpu));
            break;
        case PnOp_Pla:
            if (pop_checked(machine, cpu, &value)) {
                cpu->a = value;
                set_negative_zero(cpu, value);
            }
            break;
        case PnOp_Plp:
            if (pop_checked(machine, cpu, &value)) {
                set_status(cpu, value);
            }
            break;
        case PnOp_Jmp:
            if (mode == PnMode_Indirect) {
                jump_indirect(machine, cpu);
            } else {
                jump_absolute(machine, cpu);
            }
            break;
        case PnOp_Jsr:
            jump_subroutine(machine, cpu);
            break;
        case PnOp_Rts:
            return_from_subroutine(machine, cpu);
            break;
        case PnOp_Rti:
            return_from_interrupt(machine, cpu);
            break;
    }
}

// A routine starts with all flags clear, bit 5 of the status set, and the stack empty.

PnRunResult pn_machine_run(PnMachine* machine, uint16_t pc, uint8_t a, uint8_t x, uint8_t y) {
    machine->ram[PN_CONSOL_SHADOW] = 1;
    machine->ram[PN_NMIST] = PN_NMIST_VBI;
    PnCpu cpu = { .pc = pc, .a = a, .x = x, .y = y, .sp = PN_STACK_EMPTY, .misc = PN_STATUS_UNUSED };
    for (uint32_t steps = 0; !cpu.finished; steps++) {
        if (steps >= PN_GUARD_INSTRUCTIONS) {
            return PnRun_Fault;
        }
        const uint8_t opcode = fetch(machine, &cpu);
        execute(machine, &cpu, s_opcodes[opcode]);
    }
    return cpu.result;
}
