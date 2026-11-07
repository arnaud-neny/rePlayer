// SPDX-License-Identifier: MIT
// SUN-Tronic modules. The eagleplayer's Check2 accepts a 68k program that starts with the replayer's interrupt routine
// (`movem.l d0-d7/a0-a6,-(a7)`, `lea vars(pc),a6`, `tst.b $18(a6)` or `$10(a6)`). InitPlayer then relocates the
// module's pointer tables by scanning the code for the `lea` instructions that address the tables, and patches the
// code to call SongEnd. The module is the replayer: this file models what InitPlayer does to the data, recognises the
// builds of the replayer that the player (suntronic.c) translates, and proves that a song stays inside them.

#include "sun_internal.h"

#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>

// The longest module the port accepts; the module, its relocation and the zeros above it fit 32 bits easily.
#define SUN_MAX_SIZE 0x10000u

// The step table a song may have: A and B index it with 16 bit arithmetic (index * 20), C likewise (index * 16).
#define SUN_MAX_STEPS 1024u

// The replayer builds the port models, one per module of the corpus (shades2.sun is left out below). Offsets are
// from the variable block.
static const SunBuild s_builds[] = {
    // msx-cracktro 2.sun, generation C (the oldest)
    { .code_hash = 0xc406f7a46437f04fULL,
      .vars = 0x412,
      .gen = SunGen_C,
      .period_table = 0x1a,
      .adjust_table = 0x2e7,
      .synth_table = 0x92,
      .sample_table = 0x102,
      .step_table = 0x106 },
    // msx-cracktro.sun, generation B
    { .code_hash = 0x6f64ce564a426bc7ULL,
      .vars = 0x46e,
      .gen = SunGen_B,
      .period_table = 0x46,
      .adjust_table = 0x5a2,
      .synth_table = 0xd6,
      .sample_table = 0x22a,
      .step_table = 0x22e },
    // msx-intro.sun, generation A
    { .code_hash = 0x3e7a1c435d2ae2cdULL,
      .vars = 0x6e6,
      .gen = SunGen_A,
      .period_table = 0x64,
      .adjust_table = 0xccf,
      .synth_table = 0xf4,
      .sample_table = 0x2a4,
      .step_table = 0x2c4,
      .master_volume = 0x19,
      .filter_scale = 0xc000 },
    // shades2.sun (code hash 0x48db2bcdbe2d0d78) is a generation A build too, but its song points a voice at address 1
    // (UADE's exception vectors) in step 83; the build stays out until a song of it can be modelled.
    // shades.sun, generation A (the master volume is addressed one byte off)
    { .code_hash = 0xb1d9ecaed54111f7ULL,
      .vars = 0x6a4,
      .gen = SunGen_A,
      .period_table = 0x54,
      .adjust_table = 0x4f3,
      .synth_table = 0xe4,
      .sample_table = 0x198,
      .step_table = 0x19c,
      .master_volume = 0x18,
      .fixed_master_volume = true },
};

// Memory

uint8_t sun_mem_u8(const SunMemory* mem, uint32_t address) {
    const uint32_t offset = address - SUN_MODULE_BASE;
    const uint64_t limit = (uint64_t)mem->size + NUADE_PAULA_MAX_SAMPLE_BYTES;
    return address >= SUN_MODULE_BASE && offset < limit ? mem->bytes[offset] : 0;
}

uint16_t sun_mem_u16(const SunMemory* mem, uint32_t address) {
    return (uint16_t)(((uint16_t)sun_mem_u8(mem, address) << 8) | sun_mem_u8(mem, address + 1));
}

uint32_t sun_mem_u32(const SunMemory* mem, uint32_t address) {
    return ((uint32_t)sun_mem_u16(mem, address) << 16) | sun_mem_u16(mem, address + 2);
}

static void mem_set_u32(SunMemory* mem, uint32_t address, uint32_t value) {
    const uint32_t offset = address - SUN_MODULE_BASE;
    if (address >= SUN_MODULE_BASE && offset <= mem->size && mem->size - offset >= 4) {
        mem->bytes[offset] = (uint8_t)(value >> 24);
        mem->bytes[offset + 1] = (uint8_t)(value >> 16);
        mem->bytes[offset + 2] = (uint8_t)(value >> 8);
        mem->bytes[offset + 3] = (uint8_t)value;
    }
}

// True when the replayer reads [address, address + length) as constant data: inside the module or the zeros above it,
// behind the code (which InitPlayer patches) and the variable bytes that InitSound and the replayer write.
static bool data_readable(const SunModule* module, uint32_t address, uint32_t length) {
    if (address < module->vars + SUN_VARS_VOLATILE) {
        return false;
    }
    const uint64_t limit = (uint64_t)module->memory.size + NUADE_PAULA_MAX_SAMPLE_BYTES;
    return (uint64_t)(address - SUN_MODULE_BASE) + length <= limit;
}

// The eagleplayer's check and the build

static uint64_t code_hash(const uint8_t* code, uint32_t size) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (uint32_t i = 0; i < size; i++) {
        hash = (hash ^ code[i]) * 0x100000001b3ULL;
    }
    return hash;
}

// Check2: `movem.l d0-d7/a0-a6,-(a7)`, `lea x(pc),a6`, then `tst.b $18(a6)` (generation A) or `tst.b $10(a6)`.
static bool eagleplayer_check(const uint8_t* data, size_t size, uint8_t* id) {
    if (size < 12 || nuade_read_be32(data) != 0x48e7fffe || nuade_read_be16(data + 4) != 0x4dfa) {
        return false;
    }
    const uint32_t tst = nuade_read_be32(data + 8);
    if (tst != 0x4a2e0018 && tst != 0x4a2e0010) {
        return false;
    }
    *id = (uint8_t)tst;
    return true;
}

static const SunBuild* find_build(const uint8_t* data, size_t size) {
    const uint32_t displacement = nuade_read_be16(data + 6);
    const uint32_t vars = (6u + displacement) & 0xffffu; // lea d16(pc): sign extended; the builds are all forward
    if (displacement >= 0x8000 || vars >= size) {
        return NULL;
    }
    const uint64_t hash = code_hash(data, vars);
    for (size_t i = 0; i < nuade_sizeof_array(s_builds); i++) {
        if (s_builds[i].code_hash == hash && s_builds[i].vars == vars) {
            return &s_builds[i];
        }
    }
    return NULL;
}

// InitPlayer: find the table operands in the code and relocate the tables

typedef struct Operands {
    uint32_t step_table;
    uint32_t synth_table;
    uint32_t sample_table;
} Operands;

// The scan loop `cmpi.w #opcode,(a0)+ / beq / dbra d0`: on a match *address is the word after the opcode.
static bool find_opcode(const SunMemory* mem, uint16_t opcode, uint32_t* address, uint16_t* budget) {
    for (;;) {
        const uint16_t word = sun_mem_u16(mem, *address);
        *address += 2;
        if (word == opcode) {
            return true;
        }
        if (*budget == 0) {
            return false;
        }
        (*budget)--;
    }
}

// `lea d16(a6),a1` / `lea d16(a6),a2`: the table address is the variable block plus the sign extended operand.
static uint32_t operand_target(const SunMemory* mem, uint32_t vars, uint32_t operand_address) {
    return vars + (uint32_t)(int32_t)(int16_t)sun_mem_u16(mem, operand_address);
}

// The step table is the first `lea d16(a6),a1`; the synth and sample instrument tables are the second and third
// `lea d16(a6),a2` (the first is the period table).
static bool find_operands(const SunMemory* mem, uint32_t vars, Operands* out) {
    const uint16_t words = (uint16_t)(sun_mem_u16(mem, SUN_MODULE_BASE + 6) >> 1);

    uint32_t address = SUN_MODULE_BASE;
    uint16_t budget = words;
    if (!find_opcode(mem, 0x43ee, &address, &budget)) {
        return false;
    }
    out->step_table = operand_target(mem, vars, address);

    address = SUN_MODULE_BASE;
    budget = words;
    if (!find_opcode(mem, 0x45ee, &address, &budget) || !find_opcode(mem, 0x45ee, &address, &budget)) {
        return false;
    }
    out->synth_table = operand_target(mem, vars, address);
    if (!find_opcode(mem, 0x45ee, &address, &budget)) {
        return false;
    }
    out->sample_table = operand_target(mem, vars, address);
    return true;
}

// Pointers from the original link address range [low, high) move to the module's address.
typedef struct Rebase {
    uint32_t low;
    uint32_t high;
} Rebase;

static bool rebase_in_range(const Rebase* rebase, uint32_t value) {
    return value >= rebase->low && value < rebase->high;
}

static void rebase_long(SunMemory* mem, uint32_t address, const Rebase* rebase) {
    const uint32_t value = sun_mem_u32(mem, address);
    if (rebase_in_range(rebase, value)) {
        mem_set_u32(mem, address, value - rebase->low + SUN_MODULE_BASE);
    }
}

static void rebase_longs(SunMemory* mem, uint32_t address, uint32_t count, const Rebase* rebase) {
    for (uint32_t i = 0; i < count; i++) {
        rebase_long(mem, address + 4 * i, rebase);
    }
}

// The instrument tables and the step table follow each other, and the sample table is whole longs (the replayer
// relocates it long by long).
static bool tables_ordered(const Operands* ops) {
    return ops->synth_table <= ops->sample_table && ops->sample_table <= ops->step_table
           && (ops->step_table - ops->sample_table) % 4 == 0;
}

// Generation A (id 0x18). The step table is followed by a fixed footer; the first synth instrument's envelope pointer
// is the address right behind the footer's 0x7f marker, which gives the link address of the module.
static bool relocate_a(SunMemory* mem, const Operands* ops) {
    const uint32_t end = SUN_MODULE_BASE + mem->size;

    uint32_t address = ops->step_table;
    uint16_t budget = (uint16_t)(((end - address) & 0xffffu) >> 1);
    for (;;) {
        const uint32_t value = sun_mem_u32(mem, address);
        if (value == 0 || value == 0xffffffffu) {
            break;
        }
        address += 4;
        if (budget == 0) {
            return false;
        }
        budget--;
    }
    address += 4;
    for (int i = 0; i < 3; i++) {
        if (sun_mem_u32(mem, address) != 0) {
            return false;
        }
        address += 4;
    }
    if (sun_mem_u32(mem, address) != 0x0002060a || sun_mem_u16(mem, address + 4) != 0x0f14
        || sun_mem_u8(mem, address + 6) != 0x19) {
        return false;
    }
    address += 7;
    bool marker = false;
    for (int i = 0; i < 0x80 && !marker; i++) {
        marker = sun_mem_u8(mem, address++) == 0x7f;
    }
    if (!marker || !tables_ordered(ops)) {
        return false;
    }

    const uint32_t low = sun_mem_u32(mem, ops->synth_table) - (address - SUN_MODULE_BASE);
    const Rebase rebase = { .low = low, .high = mem->size + low };
    // Synth instruments: pointers at +0, +8, +0x12, +0x1a, +0x1e are found by sliding a window of four longs over the
    // table in steps of 18 bytes.
    uint32_t window = ops->synth_table;
    do {
        rebase_longs(mem, window, 4, &rebase);
        window += 18;
    } while (window < ops->sample_table);
    if (window != ops->sample_table) {
        return false;
    }
    rebase_longs(mem, ops->sample_table, (ops->step_table - ops->sample_table) / 4, &rebase);
    // Steps: four pattern pointers, then the four transpositions, until a zero or 0xffffffff pattern.
    uint32_t step = ops->step_table;
    for (uint32_t i = 0; i < SUN_MAX_STEPS; i++) {
        rebase_longs(mem, step, 4, &rebase);
        step += 20;
        const uint32_t next = sun_mem_u32(mem, step);
        if (next == 0 || next == 0xffffffffu) {
            return true;
        }
    }
    return false;
}

// Generations B and C (id 0x10). The link address is found from the lowest pointer of the synth instrument table that
// lies within 0x100 of its first one, which the step table's end (the first zero long) is assumed to precede directly.
static bool relocate_bc(SunMemory* mem, const Operands* ops) {
    const uint32_t end = SUN_MODULE_BASE + mem->size;
    if (!tables_ordered(ops)) {
        return false;
    }

    uint32_t lowest = sun_mem_u32(mem, ops->synth_table);
    const uint32_t first = lowest;
    uint32_t address = ops->synth_table + 4;
    for (;;) {
        const uint32_t value = sun_mem_u32(mem, address);
        if (value >= first - 0x100 && value < first + 0x100 && value < lowest) {
            lowest = value;
            address += 4;
        } else {
            address += 2;
        }
        if (address >= ops->sample_table) {
            break;
        }
    }
    if (address != ops->sample_table) {
        return false;
    }

    address = ops->step_table;
    uint16_t budget = (uint16_t)(((end - address) & 0xffffu) >> 1);
    for (;;) {
        const uint32_t value = sun_mem_u32(mem, address);
        address += 4;
        if (value == 0) {
            break;
        }
        if (budget == 0) {
            return false;
        }
        budget--;
    }

    const uint32_t low = lowest - (address - SUN_MODULE_BASE);
    const Rebase rebase = { .low = low, .high = mem->size + low };
    address = ops->synth_table;
    do {
        const uint32_t value = sun_mem_u32(mem, address);
        if (rebase_in_range(&rebase, value)) {
            mem_set_u32(mem, address, value - low + SUN_MODULE_BASE);
            address += 4;
        } else {
            address += 2;
        }
    } while (address < ops->sample_table);
    if (address != ops->sample_table) {
        return false;
    }
    rebase_longs(mem, ops->sample_table, (ops->step_table - ops->sample_table) / 4, &rebase);
    address = ops->step_table;
    for (uint32_t i = 0; i < SUN_MAX_SIZE; i++) {
        rebase_long(mem, address, &rebase);
        address += 4;
        if (sun_mem_u32(mem, address) == 0) {
            return true;
        }
    }
    return false;
}

// Records

static uint32_t synth_record_size(SunGen gen) {
    switch (gen) {
        case SunGen_A:
            return 0x24;
        case SunGen_B:
            return 0x22;
        case SunGen_C:
            return 0x1c;
    }
    return 0;
}

static uint32_t step_stride(SunGen gen) {
    return gen == SunGen_C ? 16 : 20;
}

SunInstrument sun_module_instrument(const SunModule* module, uint32_t index) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    const uint32_t record = module->vars + module->build->synth_table + index * synth_record_size(gen);
    SunInstrument inst = { 0 };
    inst.envelope = sun_mem_u32(mem, record);
    inst.envelope_end = sun_mem_u16(mem, record + 4);
    inst.envelope_loop = sun_mem_u16(mem, record + 6);
    if (gen == SunGen_C) {
        inst.envelope_stop = sun_mem_u16(mem, record + 8);
        inst.morph = sun_mem_u32(mem, record + 0xa);
        inst.morph_end = sun_mem_u16(mem, record + 0xe);
        inst.morph_loop = sun_mem_u16(mem, record + 0x10);
        inst.morph_stop = sun_mem_u16(mem, record + 0x12);
        inst.wave_a = sun_mem_u32(mem, record + 0x14);
        inst.wave_b = sun_mem_u32(mem, record + 0x18);
        inst.words = SUN_WAVE_BYTES_BC / 2;
        return inst;
    }
    inst.vibrato = sun_mem_u32(mem, record + 8);
    inst.vibrato_end = sun_mem_u16(mem, record + 0xc);
    inst.vibrato_loop = sun_mem_u16(mem, record + 0xe);
    inst.vibrato_step = (int16_t)sun_mem_u16(mem, record + 0x10);
    inst.morph = sun_mem_u32(mem, record + 0x12);
    inst.morph_end = sun_mem_u16(mem, record + 0x16);
    inst.morph_loop = sun_mem_u16(mem, record + 0x18);
    inst.wave_a = sun_mem_u32(mem, record + 0x1a);
    inst.wave_b = sun_mem_u32(mem, record + 0x1e);
    if (gen == SunGen_A) {
        inst.words = sun_mem_u8(mem, record + 0x22);
        inst.morph_type = sun_mem_u8(mem, record + 0x23);
    } else {
        inst.words = SUN_WAVE_BYTES_BC / 2;
    }
    return inst;
}

SunStep sun_module_step(const SunModule* module, uint32_t index, bool* end) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    // The replayer indexes the table with a 16 bit register (index * stride, sign extended).
    const int16_t offset = (int16_t)(index * step_stride(gen));
    const uint32_t entry = module->vars + module->build->step_table + (uint32_t)(int32_t)offset;
    SunStep step = { 0 };
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        step.pattern[v] = sun_mem_u32(mem, entry + 4 * v);
        if (gen != SunGen_C) {
            step.transpose[v] = (int8_t)sun_mem_u8(mem, entry + 0x10 + v);
        }
    }
    *end = step.pattern[0] == 0;
    return step;
}

uint32_t sun_module_start_step(const SunModule* module) {
    switch (module->build->gen) {
        case SunGen_A:
            return 0;
        case SunGen_B:
            return 0x1f;
        case SunGen_C:
            return 0x40;
    }
    return 0;
}

// Pattern items

static uint8_t first_note(SunGen gen) {
    return gen == SunGen_C ? 0xc4 : 0xb8;
}

// Argument bytes of a command byte, 0 for a byte the generation does not handle.
static uint32_t command_arguments(SunGen gen, uint8_t command) {
    switch (command) {
        case SunCommand_PeriodRow:
        case SunCommand_VolumeSlide:
        case SunCommand_Volume:
        case SunCommand_Speed:
            return 1;
        case SunCommand_Slide:
            return gen == SunGen_A ? 2 : 1;
        default:
            break;
    }
    if (gen != SunGen_A) {
        return 0;
    }
    switch (command) {
        case SunCommand_Seed:
        case SunCommand_Fade:
            return 2;
        case SunCommand_SetPitch:
        case SunCommand_MasterVolume:
            return 1;
        default:
            return 0;
    }
}

SunItem sun_read_item(const SunMemory* mem, SunGen gen, uint32_t* address) {
    const uint8_t byte = sun_mem_u8(mem, (*address)++);
    SunItem item = { .value = byte };
    if (byte == 0) {
        item.kind = SunItem_RowEnd;
    } else if (byte < 0x80) {
        item.kind = SunItem_Instrument;
    } else if (byte >= first_note(gen)) {
        item.kind = SunItem_Note;
    } else {
        item.kind = SunItem_Command;
        const uint32_t count = command_arguments(gen, byte);
        for (uint32_t i = 0; i < count; i++) {
            item.args[i] = sun_mem_u8(mem, (*address)++);
        }
    }
    return item;
}

// The proof that a song stays inside the model

// A position that counts up and returns to `loop` on reaching `end` visits only positions below `end`. The replayer
// indexes with a sign extended word, so the positions stay below 0x8000.
static bool position_loops(uint16_t end, uint16_t loop) {
    return end >= 1 && end <= SUN_MAX_POSITION && loop < end;
}

// Generation C's two stage position: up to `end`, then between `loop` and `stop`.
static bool position_loops_c(uint16_t end, uint16_t loop, uint16_t stop) {
    return end >= 1 && end <= SUN_MAX_POSITION && stop <= SUN_MAX_POSITION && loop < stop && loop < end;
}

// Whether an instrument the song starts is one the port models.
static bool instrument_modelled(const SunModule* module, uint32_t index) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    const SunInstrument inst = sun_module_instrument(module, index);
    const uint32_t bytes = (uint32_t)inst.words * 2;

    if (inst.words < 1 || inst.words > SUN_WAVE_BYTES / 2) {
        return false;
    }
    if (gen == SunGen_C) {
        const uint32_t envelope = inst.envelope_end > inst.envelope_stop ? inst.envelope_end : inst.envelope_stop;
        const uint32_t morph = inst.morph_end > inst.morph_stop ? inst.morph_end : inst.morph_stop;
        if (!position_loops_c(inst.envelope_end, inst.envelope_loop, inst.envelope_stop)
            || !position_loops_c(inst.morph_end, inst.morph_loop, inst.morph_stop)
            || !data_readable(module, inst.envelope, envelope) || !data_readable(module, inst.morph, morph)) {
            return false;
        }
        return data_readable(module, inst.wave_a, bytes) && data_readable(module, inst.wave_b, bytes);
    }

    if (!position_loops(inst.envelope_end, inst.envelope_loop) || !position_loops(inst.vibrato_end, inst.vibrato_loop)
        || !position_loops(inst.morph_end, inst.morph_loop) || !data_readable(module, inst.envelope, inst.envelope_end)
        || !data_readable(module, inst.vibrato, inst.vibrato_end) || !data_readable(module, inst.morph, inst.morph_end)
        || !data_readable(module, inst.wave_a, bytes)) {
        return false;
    }
    if (gen == SunGen_B) {
        return inst.wave_b == 0 || data_readable(module, inst.wave_b, bytes);
    }
    // Generation A morph types: 1 smooth, 2 splice, 4 filter, 5 and up keep the first waveform. Types 0 (blend) and 3
    // (resample) occur in no song of the corpus and are not modelled.
    if (inst.morph_type == 0 || inst.morph_type == 3) {
        return false;
    }
    if (inst.morph_type == 2) {
        // The splice copies as many bytes from the second waveform as the morph position says.
        if (!data_readable(module, inst.wave_b, bytes)) {
            return false;
        }
        for (uint32_t i = 0; i < inst.morph_end; i++) {
            if (sun_mem_u8(mem, inst.morph + i) > bytes) {
                return false;
            }
        }
    }
    return true;
}

// Walks one voice's pattern of a step, 16 rows, and checks what the items ask of the replayer.
static bool pattern_modelled(const SunModule* module, uint32_t pattern, bool used[]) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    uint32_t address = pattern;
    for (uint32_t row = 0; row < (uint32_t)SUN_ROWS_PER_STEP; row++) {
        for (uint32_t bytes = 0;; bytes++) {
            if (bytes >= (uint32_t)SUN_MAX_ROW_BYTES || !data_readable(module, address, 4)) {
                return false;
            }
            const SunItem item = sun_read_item(mem, gen, &address);
            if (item.kind == SunItem_RowEnd) {
                break;
            }
            if (item.kind == SunItem_Instrument) {
                // Bit 6 selects the synth table; the sample table is not modelled
                const uint32_t index = (uint32_t)item.value - 0x40u;
                if (item.value < 0x40 || index >= module->synth_count) {
                    return false;
                }
                used[index] = true;
            } else if (item.kind == SunItem_Command && item.value == SunCommand_Fade
                       && module->build->fixed_master_volume) {
                return false;
            }
        }
    }
    return true;
}

// Counts the steps and checks the patterns of every step the replayer plays.
static bool song_modelled(SunModule* module) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    const uint32_t record_size = synth_record_size(gen);
    const uint32_t synth_bytes = module->build->sample_table - module->build->synth_table;
    if (synth_bytes % record_size != 0 || synth_bytes == 0) {
        return false;
    }
    module->synth_count = synth_bytes / record_size;
    // A builds that read their master volume from a variable stop the replayer while the halt flag is set
    if (gen == SunGen_A && !module->build->fixed_master_volume
        && sun_mem_u8(mem, module->vars + SUN_A_HALT_FLAG) != 0) {
        return false;
    }
    // B and C pick the discriminator InitSound tests: B for a set byte, C for a clear one
    const uint32_t step_table = module->vars + module->build->step_table;
    if (gen != SunGen_A && (sun_mem_u8(mem, step_table + 0x10) != 0) != (gen == SunGen_B)) {
        return false;
    }

    bool used[0x40] = { false };
    uint32_t count = 0;
    for (;; count++) {
        if (count >= SUN_MAX_STEPS) {
            return false;
        }
        bool end = false;
        const SunStep step = sun_module_step(module, count, &end);
        if (end) {
            break;
        }
        if (step.pattern[0] == 0xffffffffu) {
            return false;
        }
    }
    module->step_count = count;
    if (count == 0) {
        return false;
    }
    // Besides the table, the step InitSound's index leads to must be a step of the table or the end
    const uint32_t start = sun_module_start_step(module) + 1;
    if (start > count) {
        bool end = false;
        sun_module_step(module, start, &end);
        if (!end) {
            return false;
        }
    }
    for (uint32_t s = 0; s < count; s++) {
        bool end = false;
        const SunStep step = sun_module_step(module, s, &end);
        for (uint32_t v = 0; v < SUN_VOICES; v++) {
            if (!pattern_modelled(module, step.pattern[v], used)) {
                return false;
            }
        }
    }
    for (uint32_t i = 0; i < module->synth_count && i < nuade_sizeof_array(used); i++) {
        if (used[i] && !instrument_modelled(module, i)) {
            return false;
        }
    }
    return true;
}

bool sun_module_open(SunModule* module, const uint8_t* data, size_t size) {
    *module = (SunModule) { 0 };
    uint8_t id = 0;
    if (size > SUN_MAX_SIZE || !eagleplayer_check(data, size, &id)) {
        return false;
    }
    const SunBuild* build = find_build(data, size);
    if (build == NULL || (build->gen == SunGen_A) != (id == 0x18)) {
        return false;
    }
    module->memory.bytes = nuade_paula_padded_copy(data, size);
    if (module->memory.bytes == NULL) {
        return false;
    }
    module->memory.size = (uint32_t)size;
    module->build = build;
    module->vars = SUN_MODULE_BASE + build->vars;

    Operands ops;
    const bool relocated
        = find_operands(&module->memory, module->vars, &ops) && ops.step_table == module->vars + build->step_table
          && ops.synth_table == module->vars + build->synth_table
          && ops.sample_table == module->vars + build->sample_table
          && (build->gen == SunGen_A ? relocate_a(&module->memory, &ops) : relocate_bc(&module->memory, &ops));
    if (!relocated || !song_modelled(module)) {
        sun_module_close(module);
        return false;
    }
    return true;
}

void sun_module_close(SunModule* module) {
    free(module->memory.bytes);
    *module = (SunModule) { 0 };
}
