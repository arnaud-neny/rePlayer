// SPDX-License-Identifier: MIT
// Fred Gray player: finds the engine in a module's code.
//
// The module's code is the replayer. Its interrupt routine calls a request routine (which starts the next song
// section by jumping through a table of section routines), runs five voice routines for each voice and writes the
// Paula registers. The locator follows that structure from the header, reads the table addresses out of the engine
// routines' operands and has the section decoder turn the section routines into operations. Code it does not know
// makes the locator fail: a hash over the engine's instruction words (absolute addresses left out) must match one of
// the four versions, the eagleplayer's own code patches must land where the engine expects them, and every address
// the engine will use must be inside the module.

#include "fg_section.h"
#include <stdlib.h>
#include <string.h>

#define MAX_SECTIONS 256

// The eagleplayer scans for the status register moves this far (bytes) from the start of the interrupt routine
#define EAGLEPLAYER_SCAN_LENGTH 0xd48

// The versions: what differs between them (see FgFeatures), and the signature of each engine's code

typedef struct VersionEntry {
    FgTraits traits;
    uint32_t signature;
} VersionEntry;

static const VersionEntry s_versions[FgVersion_Count] = {
    [FgVersion_Eco] = { .traits = { .name = "Eco",
                                    .features = { .voice_stride = 0x24,
                                                  .note_mask = FG_NOTE_MASK_7BITS,
                                                  .has_transpose = true,
                                                  .has_cap = true,
                                                  .has_noise = true,
                                                  .wave_select = true,
                                                  .request_len = true,
                                                  .has_mask = true } },
                        .signature = 0x7363d28e },
    [FgVersion_BlackLamp]
    = { .traits
        = { .name = "Black Lamp",
            .features = { .voice_stride = 0x20, .note_mask = FG_NOTE_MASK_CLEAR_BIT7, .step_preloaded = true } },
        .signature = 0x6dd6d329 },
    [FgVersion_StarGoose] = { .traits = { .name = "Star Goose",
                                          .features = { .voice_stride = 0x20,
                                                        .note_mask = FG_NOTE_MASK_7BITS,
                                                        .step_preloaded = true,
                                                        .has_transpose = true,
                                                        .has_cap = true,
                                                        .has_kick = true,
                                                        .gated_setup = true,
                                                        .has_mask = true,
                                                        .mask_sets_setup = true } },
                              .signature = 0xe2525587 },
    [FgVersion_VictoryRoad] = { .traits = { .name = "Victory Road",
                                            .features = { .voice_stride = 0x20,
                                                          .note_mask = FG_NOTE_MASK_7BITS,
                                                          .has_transpose = true,
                                                          .has_cap = true,
                                                          .has_kick = true,
                                                          .has_rest = true,
                                                          .gated_setup = true } },
                                .signature = 0xae7ef2b9 },
};

const FgTraits* fg_traits(FgVersion version) {
    return &s_versions[version].traits;
}

// The interrupt routine

typedef struct TickPlan {
    uint32_t noise; // FG_NOT_FOUND if the tick does not start with a call
    uint32_t voice_base;
    uint32_t stride;
    bool step_preloaded;
    uint32_t preload_var; // the position variable the interrupt loads into d7 first
    uint32_t request;
    uint32_t sequence; // the per voice routine
    uint32_t ext;      // the routine for requests from outside; the engine region starts here
    uint32_t hardware;
} TickPlan;

// The interrupt routine, in the shape all four versions share: an optional call (the noise), the voice base, the
// request call, the voice routine for the three voices with d6 = voice offset, and the calls of ext and hardware.
static bool parse_tick(const FgModule* m, TickPlan* plan) {
    memset(plan, 0, sizeof(*plan));
    plan->noise = FG_NOT_FOUND;
    FgCursor c = { .m = m, .at = fg_header_tick(m) };
    uint32_t target = 0;
    if (fg_cursor_call(&c, &target)) {
        plan->noise = target;
    }
    fg_cursor_take(&c, OP_CLR_L_D7);
    if (fg_cursor_take(&c, OP_LEA_ABS_A5) && fg_cursor_u32(&c) != CUSTOM_BASE) {
        return false;
    }
    const uint16_t base = fg_cursor_u16(&c); // lea.l abs,a6 or movea.l #abs,a6
    if (base != OP_LEA_ABS_A6 && base != OP_MOVEA_IMM_A6) {
        return false;
    }
    plan->voice_base = fg_cursor_u32(&c);
    if (fg_cursor_take(&c, OP_MOVE_W_ABS_D7)) {
        plan->preload_var = fg_cursor_u32(&c);
        plan->step_preloaded = true;
    }
    if (!fg_cursor_call(&c, &plan->request)) {
        return false;
    }
    for (uint32_t voice = 0; voice < FG_ACTIVE_VOICES; voice++) {
        uint32_t offset = 0;
        uint32_t length = 0;
        if (!fg_read_voice_offset_load(m, c.at, &offset, &length)) {
            return false;
        }
        c.at += length;
        if (voice == 1) {
            plan->stride = offset;
        }
        if (offset != voice * plan->stride || (voice == 1 && (offset == 0 || offset > 0x7f))) {
            return false;
        }
        uint32_t routine = 0;
        if (!fg_cursor_call(&c, &routine) || (voice > 0 && routine != plan->sequence)) {
            return false;
        }
        plan->sequence = routine;
    }
    if (!fg_cursor_call(&c, &plan->ext)) {
        return false;
    }
    const FgInsn last = fg_decode(m, c.at);
    if (last.length == 0 || (last.flow != FgFlow_Call && last.flow != FgFlow_Jump)) {
        return false;
    }
    plan->hardware = last.target;
    c.at += last.length;
    return last.flow == FgFlow_Jump || fg_cursor_peek(&c) == OP_RTS;
}

// The five engine routines each voice runs per tick, in order

typedef struct VoiceRoutines {
    uint32_t envelope;
    uint32_t slide;
    uint32_t vibrato;
    uint32_t command;
    uint32_t pitch;
} VoiceRoutines;

// The per voice routine: the five calls, then either a return, or a sixth routine that only copies the pitch to a
// variable nobody reads
static bool parse_sequence(const FgModule* m, uint32_t start, VoiceRoutines* routines) {
    FgCursor c = { .m = m, .at = start };
    uint32_t* const slots[]
        = { &routines->envelope, &routines->slide, &routines->vibrato, &routines->command, &routines->pitch };
    for (size_t i = 0; i < nuade_sizeof_array(slots); i++) {
        if (!fg_cursor_call(&c, slots[i])) {
            return false;
        }
    }
    uint32_t extra = 0;
    if (fg_cursor_call(&c, &extra)) {
        return fg_cursor_peek(&c) == OP_RTS;
    }
    const FgInsn tail = fg_decode(m, c.at);
    return fg_cursor_peek(&c) == OP_RTS || (tail.length != 0 && tail.flow == FgFlow_Jump);
}

// What the request routine yields: the request variables, the table of section routines and the place of the
// `asl.l #2,d6` that indexes it

typedef struct RequestPlan {
    uint32_t variables;
    uint32_t table;
    uint32_t shift; // the address of the asl.l
} RequestPlan;

// The request routine loads the address of the request variables and of the section table (`asl.l #2,d6` follows that
// load)
static bool parse_request(const FgModule* m, uint32_t start, RequestPlan* plan) {
    FgRoutine request = { 0 };
    plan->variables = FG_NOT_FOUND;
    plan->table = FG_NOT_FOUND;
    plan->shift = FG_NOT_FOUND;
    if (fg_routine_open(&request, m, start)) {
        plan->variables = fg_routine_table(&request, false);
        plan->shift = fg_routine_find(&request, OP_ASL_L_D6, 0);
    }
    fg_routine_close(&request);
    // the load of the table address is the instruction before: lea.l abs,a0 / movea.l #abs,a0 (6 bytes)
    if (plan->shift != FG_NOT_FOUND && plan->shift >= 6 && fg_is_table_load(fg_u16(m, plan->shift - 6))) {
        plan->table = fg_u32(m, plan->shift - 4);
    }
    return plan->variables != FG_NOT_FOUND && plan->table != FG_NOT_FOUND;
}

// The table's longs up to the first that is not the address of code (the eagleplayer stops at a zero long or at the
// rts that follows the table)
static uint32_t read_section_table(const FgModule* m, uint32_t table, uint32_t* entries) {
    uint32_t count = 0;
    for (uint32_t at = table; count < MAX_SECTIONS && fg_in_module(m, at, 4); at += 4) {
        const uint32_t entry = fg_u32(m, at);
        if (entry == 0 || !fg_in_module(m, entry, 2) || (entry & 1) != 0) {
            break;
        }
        entries[count++] = entry;
    }
    return count;
}

void fg_layout_free(FgLayout* layout) {
    free(layout->sections);
    free(layout->op_pool);
    memset(layout, 0, sizeof(*layout));
}

// The init routine

// The registers an init routine writes: per voice what was written, and the DMA enable
typedef struct InitWrites {
    bool lc[FG_ACTIVE_VOICES];
    bool len[FG_ACTIVE_VOICES];
    bool vol[FG_ACTIVE_VOICES];
    bool per[FG_ACTIVE_VOICES];
    bool dma;
} InitWrites;

// A word written to the custom register `reg`: AUDxLEN, AUDxPER, AUDxVOL (volume 0) of a voice, or DMACON
static bool init_word_write(FgInit* init, InitWrites* seen, uint32_t reg, uint16_t value) {
    uint8_t voice = 0;
    if (reg == CUSTOM_DMACON) {
        seen->dma = value == DMACON_AUDIO_ON && !seen->dma;
        return seen->dma;
    }
    if (fg_voice_of_offset(reg, CUSTOM_AUD0LEN, CUSTOM_CHANNEL_STRIDE, &voice)) {
        if (seen->len[voice] || (voice > 0 && value != init->len_words)) {
            return false;
        }
        init->len_words = value;
        seen->len[voice] = true;
    } else if (fg_voice_of_offset(reg, CUSTOM_AUD0PER, CUSTOM_CHANNEL_STRIDE, &voice)) {
        if (seen->per[voice] || (voice > 0 && value != init->period)) {
            return false;
        }
        init->period = value;
        init->writes_period = true;
        seen->per[voice] = true;
    } else if (fg_voice_of_offset(reg, CUSTOM_AUD0VOL, CUSTOM_CHANNEL_STRIDE, &voice)) {
        if (seen->vol[voice] || value != 0) {
            return false;
        }
        seen->vol[voice] = true;
    } else {
        return false;
    }
    return true;
}

// AUDxLC of a voice, once
static bool init_lc_write(FgInit* init, InitWrites* seen, uint32_t reg, uint32_t lc) {
    uint8_t voice = 0;
    if (!fg_voice_of_offset(reg, CUSTOM_AUD0LC, CUSTOM_CHANNEL_STRIDE, &voice) || seen->lc[voice]) {
        return false;
    }
    init->lc[voice] = lc;
    seen->lc[voice] = true;
    return true;
}

// What the init routine writes: the first request from the subsong table, and the audio registers of the three voices
// (AUDxLC, AUDxLEN, AUDxVOL, in the Eco version AUDxPER) and the DMA. The Eco version writes through absolute
// addresses, the others through a6 = the custom chip base.
static bool parse_init(const FgModule* m, uint32_t request_var, uint32_t subsongs, FgInit* init) {
    memset(init, 0, sizeof(*init));
    FgCursor c = { .m = m, .at = fg_header_init(m) };
    if (!fg_cursor_take(&c, OP_MOVE_B_PCIDX_ABS)) {
        return false;
    }
    // move.b d8(pc,d0.w),request: the table is d8 from the extension word
    const uint32_t extension = c.at;
    const uint16_t brief = fg_cursor_u16(&c);
    const int32_t displacement = (int8_t)(brief & 0xff);
    if ((brief & 0xff00) != 0 || fg_cursor_u32(&c) != request_var
        || (displacement < 0 && (uint32_t)-displacement > extension)) {
        return false;
    }
    init->subsong_map = extension + (uint32_t)displacement;
    if (!fg_in_module(m, init->subsong_map, subsongs)) {
        return false;
    }
    InitWrites seen = { 0 };
    bool custom_base = false;
    uint32_t wave = FG_NOT_FOUND; // lea.l wave,a0
    uint16_t loaded = 0;          // move.w #n,d0
    for (;;) {
        const uint16_t w = fg_cursor_u16(&c);
        bool ok = true;
        switch (w) {
            case OP_RTS: {
                for (int i = 0; i < FG_ACTIVE_VOICES; i++) {
                    if (!seen.lc[i] || !seen.len[i] || !seen.vol[i] || seen.per[i] != init->writes_period) {
                        return false;
                    }
                }
                return seen.dma;
            }
            case OP_MOVE_L_IMM_ABS: { // move.l #n,AUDxLC
                const uint32_t lc = fg_cursor_u32(&c);
                ok = init_lc_write(init, &seen, fg_cursor_u32(&c), lc);
                break;
            }
            case OP_MOVE_W_IMM_D0:
                loaded = fg_cursor_u16(&c);
                break;
            case OP_LEA_ABS_A6:
                custom_base = fg_cursor_u32(&c) == CUSTOM_BASE;
                ok = custom_base;
                break;
            case OP_LEA_ABS_A0:
                wave = fg_cursor_u32(&c);
                break;
            case OP_MOVE_L_A0_D16A6: // move.l a0,AUDxLC(a6)
                ok = custom_base && wave != FG_NOT_FOUND
                     && init_lc_write(init, &seen, CUSTOM_BASE + fg_cursor_u16(&c), wave);
                break;
            case OP_MOVE_W_D0_ABS: // move.w d0,abs
                ok = init_word_write(init, &seen, fg_cursor_u32(&c), loaded);
                break;
            case OP_MOVE_W_IMM_ABS: { // move.w #n,abs
                const uint16_t value = fg_cursor_u16(&c);
                ok = init_word_write(init, &seen, fg_cursor_u32(&c), value);
                break;
            }
            case OP_MOVE_W_IMM_D16A6: { // move.w #n,d16(a6)
                const uint16_t value = fg_cursor_u16(&c);
                ok = custom_base && init_word_write(init, &seen, CUSTOM_BASE + fg_cursor_u16(&c), value);
                break;
            }
            case OP_CLR_W_D16A6:
                ok = custom_base && init_word_write(init, &seen, CUSTOM_BASE + fg_cursor_u16(&c), 0);
                break;
            default:
                ok = false;
                break;
        }
        if (!ok) {
            return false;
        }
    }
}

// The signature of the engine

#define FNV_OFFSET 2166136261u
#define FNV_PRIME 16777619u

// The word at `word` of the instruction at `at` is part of a relocated long (an absolute address or an immediate
// address) that is an operand: the long never covers the opcode word
static bool is_relocated_operand(const FgModule* m, uint32_t at, uint32_t word) {
    return (word >= at + 2 && fg_module_is_reloc(m, word)) || (word >= at + 4 && fg_module_is_reloc(m, word - 2));
}

// FNV-1a over the words of every instruction the roots reach (calls followed), in address order. The operand words of
// relocated longs (absolute addresses, which depend on where the module keeps its tables) count as 0xffff.
static uint32_t engine_signature(const FgModule* m, const uint32_t* roots, uint32_t root_count) {
    FgWalk w;
    if (!fg_walk_init(&w, m, true)) {
        return 0;
    }
    bool ok = true;
    for (uint32_t i = 0; i < root_count && ok; i++) {
        ok = fg_walk_from(&w, roots[i]);
    }
    uint32_t hash = FNV_OFFSET;
    for (uint32_t at = 0; ok && at < m->size; at += 2) {
        if (!fg_walk_has(&w, at)) {
            continue;
        }
        const FgInsn insn = fg_decode(m, at);
        for (uint32_t word = at; word < at + insn.length; word += 2) {
            const uint16_t value = is_relocated_operand(m, at, word) ? 0xffff : fg_u16(m, word);
            hash = (hash ^ (value >> 8)) * FNV_PRIME;
            hash = (hash ^ (value & 0xff)) * FNV_PRIME;
        }
    }
    fg_walk_free(&w);
    return ok ? hash : 0;
}

// The version whose engine has the signature of the code the interrupt routine and the sections reach
static bool match_version(const FgModule* m, const FgEngineCalls* calls, FgLayout* layout) {
    uint32_t roots[1 + FG_MAX_ENGINE_CALLS];
    uint32_t root_count = 0;
    roots[root_count++] = fg_header_tick(m);
    for (uint32_t i = 0; i < calls->count; i++) {
        roots[root_count++] = calls->routine[i];
    }
    const uint32_t signature = engine_signature(m, roots, root_count);
    for (int v = 0; v < FgVersion_Count && signature != 0; v++) {
        if (s_versions[v].signature == signature) {
            layout->version = (FgVersion)v;
            layout->features = s_versions[v].traits.features;
            return true;
        }
    }
    return false;
}

// The channel mask routine: the table, the mask of the position, and the command it starts
static bool parse_mask(const FgModule* m, uint32_t routine, uint32_t stride, FgChannelMask* mask) {
    FgRoutine r = { 0 };
    bool ok = fg_routine_open(&r, m, routine);
    if (ok) {
        mask->table = fg_routine_table(&r, false);
        const uint32_t index_mask = fg_routine_word(&r, OP_ANDI_W_D2, 0);
        const uint32_t command = fg_routine_find(&r, OP_MOVE_B_IMM_D16A6, 0);
        ok = mask->table != FG_NOT_FOUND && index_mask != FG_NOT_FOUND && command != FG_NOT_FOUND
             && fg_voice_of_offset(fg_u16(m, command + 4), FgRecordField_Command, stride, &mask->command_voice);
        if (ok) {
            mask->index_mask = (uint16_t)index_mask;
            mask->command_value = (uint8_t)fg_u16(m, command + 2);
            // The Eco version also writes AUDxLEN of the voice: the short length before the test, the long one on a hit
            const uint32_t off = fg_routine_find(&r, OP_MOVE_W_IMM_ABS, 0);
            const uint32_t on = fg_routine_find(&r, OP_MOVE_W_IMM_ABS, 1);
            mask->writes_len = off != FG_NOT_FOUND;
            if (mask->writes_len) {
                uint8_t voice = 0;
                ok = on != FG_NOT_FOUND
                     && fg_voice_of_offset(fg_u32(m, off + 4), CUSTOM_AUD0LEN, CUSTOM_CHANNEL_STRIDE, &voice)
                     && voice == mask->command_voice && fg_u32(m, on + 4) == fg_u32(m, off + 4);
                mask->len_off = fg_u16(m, off + 2);
                mask->len_on = fg_u16(m, on + 2);
            }
        }
    }
    fg_routine_close(&r);
    return ok;
}

// What the request routine does besides starting the section: AUDxLEN of one voice (Eco), the setup gate cleared
static bool parse_request_effects(const FgModule* m, const TickPlan* plan, FgLayout* layout) {
    const FgFeatures* f = &layout->features;
    FgRoutine r = { 0 };
    bool ok = fg_routine_open(&r, m, plan->request);
    if (ok) {
        const uint32_t len = fg_routine_find(&r, OP_MOVE_W_IMM_ABS, 0);
        ok = (len != FG_NOT_FOUND) == f->request_len;
        if (ok && f->request_len) {
            layout->request_len_words = fg_u16(m, len + 2);
            ok = fg_voice_of_offset(fg_u32(m, len + 4), CUSTOM_AUD0LEN, CUSTOM_CHANNEL_STRIDE,
                                    &layout->request_len_voice);
        }
        ok = ok && (fg_routine_find(&r, OP_CLR_B_ABS, 0) != FG_NOT_FOUND) == f->gated_setup;
    }
    fg_routine_close(&r);
    return ok;
}

// The engine routines and variables the locator needs before the sections can be decoded

typedef struct EngineVars {
    uint32_t position;     // the section position
    uint32_t transpose;    // FG_NOT_FOUND if the engine has none
    uint32_t cap;          // FG_NOT_FOUND if the engine has none
    uint32_t kick_mask;    // the byte the hardware routine reads (FG_NOT_FOUND if it reads none)
    uint32_t kick_routine; // the routine the command routine calls: the one that sets the bits of the mask
} EngineVars;

// The first routine that the routine at `routine` calls, FG_NOT_FOUND if it calls none
static uint32_t first_call_of(const FgModule* m, uint32_t routine) {
    FgRoutine r = { 0 };
    const uint32_t target = fg_routine_open(&r, m, routine) ? fg_routine_first_call(&r) : FG_NOT_FOUND;
    fg_routine_close(&r);
    return target;
}

// The variables the sections read and write: the position (from the tempo routine), the transpose (from the note start)
// and the envelope cap (from the envelope routine); the kick mask byte is the one the hardware routine reads, and the
// routine that sets its bits is the one the command routine calls. The tempo routine and the note start are called from
// the sections only, so they are found by scanning the engine region.
static bool read_engine_vars(const FgModule* m, const TickPlan* plan, const VoiceRoutines* routines, EngineVars* vars) {
    const uint32_t tempo = fg_scan_code(m, plan->ext, OP_MOVE_B_ABS_D0, 4, OP_ADDQ_B_D0);
    const uint32_t transpose = fg_scan_code(m, plan->ext, OP_ADD_W_ABS_D4, 0, 0);
    vars->position = tempo == FG_NOT_FOUND ? FG_NOT_FOUND : fg_find_long(m, tempo, OP_MOVE_W_ABS_D7, 0);
    vars->transpose = transpose == FG_NOT_FOUND ? FG_NOT_FOUND : fg_u32(m, transpose + 2);
    vars->cap = fg_find_long(m, routines->envelope, OP_MOVE_B_ABS_D2, 0);
    vars->kick_mask = fg_find_long(m, plan->hardware, OP_MOVE_B_ABS_D5, 0);
    vars->kick_routine = first_call_of(m, routines->command);
    return vars->position != FG_NOT_FOUND;
}

// The note start every track routine ends in: where the routine at `track` jumps to
static uint32_t note_start_of(const FgModule* m, uint32_t track) {
    uint32_t value = 0;
    uint32_t length = 0;
    if (!fg_read_voice_offset_load(m, track, &value, &length)) {
        return FG_NOT_FOUND;
    }
    const FgInsn jump = fg_decode(m, track + length);
    return jump.length != 0 && jump.flow == FgFlow_Jump ? jump.target : FG_NOT_FOUND;
}

// Every track routine ends in the same note start: the one of the first voice's track routine the sections call
static uint32_t note_start_of_callees(const FgModule* m, const FgCallees* callees) {
    const uint32_t track = callees->track[0] != FG_NOT_FOUND ? callees->track[0] : callees->track[1];
    return track == FG_NOT_FOUND ? FG_NOT_FOUND : note_start_of(m, track);
}

// The table a routine loads first into the first (or the second) table register
static uint32_t table_of(const FgModule* m, uint32_t routine, bool second) {
    FgRoutine r = { 0 };
    const uint32_t table = fg_routine_open(&r, m, routine) ? fg_routine_table(&r, second) : FG_NOT_FOUND;
    fg_routine_close(&r);
    return table;
}

// The tables the engine reads, from the operands of the routines that use them
static bool read_tables(const FgModule* m, const TickPlan* plan, const VoiceRoutines* routines,
                        const FgCallees* callees, FgLayout* layout) {
    const FgFeatures* f = &layout->features;
    layout->voice_base = plan->voice_base;
    layout->vibrato = table_of(m, routines->vibrato, false);
    layout->envelope = table_of(m, routines->envelope, false);
    layout->commands = table_of(m, routines->command, false);
    layout->wave_base = fg_find_long(m, routines->command, OP_ADDI_L_D1, 0);
    layout->periods = table_of(m, routines->pitch, true);
    layout->instruments = table_of(m, first_call_of(m, callees->setup), false);
    layout->section_lengths = table_of(m, callees->tail_next, false);
    if (table_of(m, callees->tail_advance, false) != layout->section_lengths) {
        return false;
    }
    if (f->has_mask && !parse_mask(m, callees->mask, f->voice_stride, &layout->mask)) {
        return false;
    }
    layout->noise_wave = layout->noise_table = layout->noise_phase = FG_NOT_FOUND;
    layout->kick_silence = layout->kick_sample = FG_NOT_FOUND;
    FgRoutine r = { 0 };
    if (f->has_noise && fg_routine_open(&r, m, plan->noise)) {
        layout->noise_wave = fg_routine_table(&r, true);
        layout->noise_table = fg_routine_table(&r, false);
        layout->noise_phase = fg_routine_long(&r, OP_MOVE_W_ABS_D1, 0);
        fg_routine_close(&r);
    }
    if (f->has_kick && fg_routine_open(&r, m, plan->hardware)) {
        layout->kick_silence = fg_routine_long(&r, OP_MOVE_L_IMM_KICK, 0);
        layout->kick_sample = fg_routine_long(&r, OP_MOVE_L_IMM_KICK, 1);
        fg_routine_close(&r);
    }
    return true;
}

// The features the version table lists are in the code: each is read off the instructions it needs
static bool check_features(const FgModule* m, const TickPlan* plan, const FgCallees* callees, const EngineVars* vars,
                           uint32_t note_start, const FgLayout* layout) {
    const FgFeatures* f = &layout->features;
    FgRoutine note = { 0 };
    const bool note_ok = fg_routine_open(&note, m, note_start);
    const bool has_rest = fg_routine_word(&note, OP_BTST_B_D4, 0) == 6;
    const uint16_t note_mask
        = fg_routine_find(&note, OP_ANDI_W_D4, 0) != FG_NOT_FOUND ? FG_NOTE_MASK_7BITS : FG_NOTE_MASK_CLEAR_BIT7;
    fg_routine_close(&note);
    FgRoutine hardware = { 0 };
    const bool hardware_ok = fg_routine_open(&hardware, m, plan->hardware);
    const bool has_kick = fg_routine_find(&hardware, OP_MOVE_L_IMM_KICK, 0) != FG_NOT_FOUND;
    fg_routine_close(&hardware);
    return note_ok && hardware_ok && plan->stride == f->voice_stride && plan->step_preloaded == f->step_preloaded
           && (plan->noise != FG_NOT_FOUND) == f->has_noise && (vars->transpose != FG_NOT_FOUND) == f->has_transpose
           && (vars->cap != FG_NOT_FOUND) == f->has_cap && (callees->mask != FG_NOT_FOUND) == f->has_mask
           && (layout->wave_base != FG_NOT_FOUND) == f->wave_select && has_kick == f->has_kick
           && has_rest == f->has_rest && note_mask == f->note_mask
           && (fg_u16(m, callees->setup) == OP_TST_B_ABS) == f->gated_setup;
}

#define OPERAND_ABS 2     // the address follows the opcode word
#define OPERAND_IMM_ABS 4 // the address follows an immediate word

// Every instruction of the routine that starts with `opcode` has `value` as the long at `operand` bytes after its start
static bool operands_equal(const FgModule* m, uint32_t routine, uint16_t opcode, uint32_t operand, uint32_t value) {
    FgRoutine r = { 0 };
    bool ok = fg_routine_open(&r, m, routine);
    for (uint32_t nth = 0; ok; nth++) {
        const uint32_t at = fg_routine_find(&r, opcode, nth);
        if (at == FG_NOT_FOUND) {
            break;
        }
        ok = fg_u32(m, at + operand) == value;
    }
    fg_routine_close(&r);
    return ok;
}

// Every instruction of `routine` that starts with `opcode` has `value` at `operand`, and there is at least one
static bool operands_are(const FgModule* m, uint32_t routine, uint16_t opcode, uint32_t operand, uint32_t value) {
    return operands_equal(m, routine, opcode, operand, value) && fg_find(m, routine, opcode, 0) != FG_NOT_FOUND;
}

// The clr.w instructions of the request routine clear the song variables the engine clears: the tempo counter, the
// position and, where there is one, the transpose; each of them, and nothing else
static bool request_clears(const FgModule* m, uint32_t request, uint32_t counter, uint32_t position,
                           uint32_t transpose) {
    FgRoutine r = { 0 };
    bool ok = fg_routine_open(&r, m, request);
    bool cleared_counter = false;
    bool cleared_position = false;
    bool cleared_transpose = false;
    for (uint32_t nth = 0; ok; nth++) {
        const uint32_t at = fg_routine_find(&r, OP_CLR_W_ABS, nth);
        if (at == FG_NOT_FOUND) {
            break;
        }
        const uint32_t operand = fg_u32(m, at + OPERAND_ABS);
        cleared_counter = cleared_counter || operand == counter;
        cleared_position = cleared_position || operand == position;
        cleared_transpose = cleared_transpose || (transpose != FG_NOT_FOUND && operand == transpose);
        ok = operand == counter || operand == position || (transpose != FG_NOT_FOUND && operand == transpose);
    }
    fg_routine_close(&r);
    return ok && cleared_counter && cleared_position && cleared_transpose == (transpose != FG_NOT_FOUND);
}

// The engine models each of these as one variable, but the module names it in several routines with a relocated address
// the signature does not compare: the routines must name the same address.
//   position: the interrupt's preload, the tempo routine, both tails, the channel mask, the setup (gated versions),
//             the request's clear
//   tempo counter: the tempo routine, the request's clear
//   section and next section: both tails (the request variables)
//   transpose: the note start, the request's clear
//   cap: the reset routine (the envelope routine is where it comes from)
//   noise phase: the noise routine, the note start's clear
//   setup gate: the setup, the request's clear, the rest in the note start, the channel mask
//   kick mask: the kick routine (load and store) and the hardware routine's read
// Everything else that is relocated is either read from the code and used as an address (the voice records, the request
// variables, the section table, the tables, the kick samples), decoded by the section decoder, or not modelled; the
// pending log lists the last group.
static bool check_shared_variables(const FgModule* m, const TickPlan* plan, const FgCallees* callees,
                                   const EngineVars* vars, uint32_t note_start, const FgLayout* layout,
                                   uint32_t request_vars) {
    const FgFeatures* f = &layout->features;
    const uint32_t position = vars->position;
    const uint32_t counter = fg_find_long(m, callees->tempo, OP_MOVE_B_ABS_D0, 0);
    const uint32_t section = fg_request_var(request_vars, FgRequestVar_Section);
    const uint32_t next_section = fg_request_var(request_vars, FgRequestVar_NextSection);
    const uint32_t reset = callees->reset;
    bool ok = !f->step_preloaded || plan->preload_var == position;
    ok = ok && counter != FG_NOT_FOUND && operands_are(m, callees->tempo, OP_MOVE_W_ABS_D7, OPERAND_ABS, position)
         && operands_are(m, callees->tempo, OP_MOVE_B_ABS_D0, OPERAND_ABS, counter)
         && operands_are(m, callees->tempo, OP_MOVE_B_D0_ABS, OPERAND_ABS, counter)
         && request_clears(m, plan->request, counter, position, vars->transpose);
    const uint32_t tails[] = { callees->tail_next, callees->tail_advance };
    for (size_t i = 0; i < nuade_sizeof_array(tails); i++) {
        ok = ok && operands_are(m, tails[i], OP_MOVE_W_ABS_D7, OPERAND_ABS, position)
             && operands_are(m, tails[i], OP_MOVE_W_D7_ABS, OPERAND_ABS, position)
             && operands_are(m, tails[i], OP_MOVE_B_ABS_D0, OPERAND_ABS, section)
             && operands_equal(m, tails[i], OP_MOVE_B_ABS_D1, OPERAND_ABS, section)
             && operands_are(m, tails[i], OP_MOVE_B_D1_ABS, OPERAND_ABS, next_section);
    }
    ok = ok && (!f->has_mask || operands_equal(m, callees->mask, OP_MOVE_W_ABS_D2, OPERAND_ABS, position));
    if (f->has_transpose) {
        ok = ok && operands_are(m, note_start, OP_ADD_W_ABS_D4, OPERAND_ABS, vars->transpose);
    }
    // The reset routine stores the full cap; a version without a cap has no such store
    if (reset != FG_NOT_FOUND) {
        ok = ok
             && (f->has_cap ? operands_are(m, reset, OP_MOVE_B_IMM_ABS, OPERAND_IMM_ABS, vars->cap)
                            : fg_find(m, reset, OP_MOVE_B_IMM_ABS, 0) == FG_NOT_FOUND);
    }
    if (f->has_noise) {
        const uint32_t phase = layout->noise_phase;
        ok = ok && operands_are(m, plan->noise, OP_MOVE_W_ABS_D1, OPERAND_ABS, phase)
             && operands_are(m, plan->noise, OP_MOVE_W_D1_ABS, OPERAND_ABS, phase)
             && operands_are(m, note_start, OP_CLR_W_ABS, OPERAND_ABS, phase);
    }
    if (f->gated_setup) {
        const uint32_t gate = fg_find_long(m, callees->setup, OP_TST_B_ABS, 0);
        ok = ok && gate != FG_NOT_FOUND && operands_are(m, callees->setup, OP_TST_B_ABS, OPERAND_ABS, gate)
             && operands_equal(m, callees->setup, OP_CLR_B_ABS, OPERAND_ABS, gate)
             && operands_are(m, callees->setup, OP_TST_W_ABS, OPERAND_ABS, position)
             && operands_are(m, plan->request, OP_CLR_B_ABS, OPERAND_ABS, gate)
             && (!f->has_rest || operands_are(m, note_start, OP_MOVE_B_IMM_ABS, OPERAND_IMM_ABS, gate))
             && (!f->mask_sets_setup || operands_are(m, callees->mask, OP_MOVE_B_D0_ABS, OPERAND_ABS, gate));
    }
    if (f->has_kick) {
        ok = ok && vars->kick_mask != FG_NOT_FOUND && vars->kick_routine != FG_NOT_FOUND
             && operands_are(m, vars->kick_routine, OP_MOVE_B_ABS_D0, OPERAND_ABS, vars->kick_mask)
             && operands_are(m, vars->kick_routine, OP_MOVE_B_D0_ABS, OPERAND_ABS, vars->kick_mask)
             && operands_are(m, plan->hardware, OP_MOVE_B_ABS_D5, OPERAND_ABS, vars->kick_mask);
    } else {
        ok = ok && vars->kick_mask == FG_NOT_FOUND;
    }
    return ok;
}

// The eagleplayer finds the section table by scanning the interrupt routine's code for `asl.l #2,d6`, and removes the
// status register moves of the note start by scanning on from there. The engine assumes both patches: they must land
// where the scans land.
static bool check_eagleplayer_patches(const FgModule* m, const RequestPlan* request, uint32_t note_start) {
    const uint32_t tick = fg_header_tick(m);
    uint32_t at = tick;
    while (fg_in_module(m, at, 2) && fg_u16(m, at) != OP_ASL_L_D6) {
        at += 2;
    }
    if (at != request->shift) {
        return false;
    }
    const uint32_t limit = tick + EAGLEPLAYER_SCAN_LENGTH;
    uint32_t push = request->shift + 2;
    while (push < limit && fg_in_module(m, push, 2) && fg_u16(m, push) != OP_MOVE_SR_PUSH) {
        push += 2;
    }
    uint32_t pop = push + 2;
    while (pop < limit && fg_in_module(m, pop, 2) && fg_u16(m, pop) != OP_MOVE_SR_POP) {
        pop += 2;
    }
    FgRoutine note = { 0 };
    bool ok = fg_routine_open(&note, m, note_start) && push < limit && pop < limit;
    ok = ok && fg_routine_find(&note, OP_MOVE_SR_PUSH, 0) == push && fg_routine_find(&note, OP_MOVE_SR_POP, 0) == pop
         && (fg_u16(m, pop + 2) >> 8) == OP_BNE_HIGH;
    fg_routine_close(&note);
    return ok;
}

// The Eco voice records hold the address of their AUDxLC register: the voice's own
static bool check_register_pointers(const FgModule* m, const FgLayout* layout) {
    if (!layout->features.wave_select) {
        return true;
    }
    for (uint32_t voice = 0; voice < FG_ACTIVE_VOICES; voice++) {
        const uint32_t record = layout->voice_base + voice * layout->features.voice_stride;
        if (fg_u32(m, record + FgRecordField_LcRegister) != CUSTOM_AUD0LC + voice * CUSTOM_CHANNEL_STRIDE) {
            return false;
        }
    }
    return true;
}

// Every address and table the engine reads or writes is inside the module, so damaged operands fail the open
static bool check_layout_addresses(const FgModule* m, const FgLayout* l) {
    const FgFeatures* f = &l->features;
    bool ok = fg_in_module(m, l->voice_base, FG_ACTIVE_VOICES * f->voice_stride) && fg_in_module(m, l->vibrato, 1)
              && fg_in_module(m, l->envelope, 1) && fg_in_module(m, l->periods, FG_PERIOD_TABLE_BYTES)
              && fg_in_module(m, l->commands, FgCommandField_Size)
              && fg_in_module(m, l->instruments, FG_INSTRUMENT_SIZE) && fg_in_module(m, l->section_lengths, 2)
              && fg_in_module(m, l->init.subsong_map, 1);
    for (int i = 0; i < FG_ACTIVE_VOICES; i++) {
        ok = ok && fg_in_module(m, l->init.lc[i], (uint32_t)l->init.len_words * 2);
    }
    ok = ok && (!f->has_mask || fg_in_module(m, l->mask.table, 1));
    ok = ok && (!f->wave_select || fg_in_module(m, l->wave_base, FG_WAVE_TABLE_BYTES));
    ok = ok
         && (!f->has_noise
             || (fg_in_module(m, l->noise_wave, FG_WAVE_BYTES) && fg_in_module(m, l->noise_table, 1)
                 && fg_in_module(m, l->noise_phase, 2)));
    ok = ok
         && (!f->has_kick
             || (fg_in_module(m, l->kick_silence, FG_WAVE_BYTES) && fg_in_module(m, l->kick_sample, FG_SAMPLE_BYTES)));
    return ok;
}

bool fg_locate(const FgModule* m, FgLayout* layout) {
    memset(layout, 0, sizeof(*layout));
    TickPlan plan;
    VoiceRoutines routines;
    RequestPlan request;
    EngineVars vars;
    uint32_t entries[MAX_SECTIONS];
    if (!fg_in_module(m, fg_header_tick(m), 2) || !fg_in_module(m, fg_header_init(m), 2) || !parse_tick(m, &plan)
        || !parse_sequence(m, plan.sequence, &routines) || !parse_request(m, plan.request, &request)
        || !read_engine_vars(m, &plan, &routines, &vars)) {
        return false;
    }
    const uint32_t count = read_section_table(m, request.table, entries);
    // The sections are code before the engine routines; entry 0 is the song end the eagleplayer patches in. The unused
    // tail of a table can point at the first engine routine itself.
    bool ok = count >= 2;
    for (uint32_t i = 1; ok && i < count; i++) {
        ok = entries[i] <= plan.ext;
    }
    FgCallees callees;
    FgEngineCalls calls;
    const FgSectionContext context = { .m = m,
                                       .stride = plan.stride,
                                       .engine_start = plan.ext,
                                       .position_var = vars.position,
                                       .transpose_var = vars.transpose,
                                       .cap_var = vars.cap,
                                       .request_vars = request.variables };
    ok = ok && fg_decode_sections(&context, entries, count, layout, &callees, &calls) && callees.tempo != FG_NOT_FOUND
         && callees.setup != FG_NOT_FOUND && callees.tail_next != FG_NOT_FOUND && callees.tail_advance != FG_NOT_FOUND
         && match_version(m, &calls, layout) && read_tables(m, &plan, &routines, &callees, layout);
    const uint32_t note_start = ok ? note_start_of_callees(m, &callees) : FG_NOT_FOUND;
    ok = ok && note_start != FG_NOT_FOUND && check_features(m, &plan, &callees, &vars, note_start, layout)
         && check_shared_variables(m, &plan, &callees, &vars, note_start, layout, request.variables)
         && check_eagleplayer_patches(m, &request, note_start) && parse_request_effects(m, &plan, layout)
         && parse_init(m, fg_request_var(request.variables, FgRequestVar_Request), fg_header_subsongs(m), &layout->init)
         && check_layout_addresses(m, layout) && check_register_pointers(m, layout);
    if (!ok) {
        fg_layout_free(layout);
    }
    return ok;
}
