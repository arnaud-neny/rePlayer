// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: finds the replayer's parts in a module's code. The replayers of different modules
// differ in a few instructions (where the tempo and the note offset come from, where the period table is, a handful of
// switches); the locator reads those out of the code and rejects a replayer it does not know.

#include "beathoven_code.h"

#define FLAG_ONE_SHOT_BIT 0x0002
#define FLAGS_ONE_SHOT_OR_HOLD 0x000C

// The voice routine is about 0x240 bytes long
#define ROUTINE_SPAN 0x300u
// Where the silent sample is written, from the start of the routine
#define SILENT_SPAN 0x80u
// A bsr or lea with a displacement word, an address and so on
#define WORD_BYTES 2u

// The address of a voice record: a lea of its address or the load of a pointer to it. Returns the address after the
// instruction, 0 if there is no such instruction at `at`. The play routine only calls the voice routine with it.
static uint32_t skip_record_load(const BthModule* m, uint32_t at) {
    switch (bthm_u16(m, at)) {
        case OP_LEA_ABS_A0:
            return at + 6;
        case OP_LEA_PC_A0:
        case OP_MOVEA_PC_A0:
            return at + 4;
        default:
            return 0;
    }
}

// The play routine: save the registers, then for every voice load its record into a0 and run the voice routine (a
// bsr; the last voice may be a bra, or the routine itself follows). A module's play routine may test a word of the
// module first and do nothing while it is zero; another runs the last voice only while a word is nonzero (or zero).
// Returns the voice routine's address.
static uint32_t locate_play(const BthModule* m, uint32_t play, BthReplayer* out) {
    uint32_t at = play;
    // One module's play routine starts by testing a word that the init routine sets while it runs: lea var(pc),a0;
    // tst.w (a0); bne
    if (bthm_u16(m, at) == OP_LEA_PC_A0 && bthm_u16(m, at + 4) == OP_TST_W_A0 && bthm_is_branch(m, at + 6, OP_BNE)) {
        bthm_branch_target(m, at + 6, &at);
    }
    if (bthm_u16(m, at) == OP_TST_ABS_W && bthm_is_branch(m, at + 6, OP_BEQ)) {
        out->play_flag = bthm_u32(m, at + 2);
        bthm_branch_target(m, at + 6, &at);
    }
    if (bthm_u16(m, at) == OP_MOVEM_PUSH) {
        at += 4;
    }
    uint32_t routine = NOT_FOUND;
    while (out->voice_count < BTH_VOICES) {
        if (bthm_u16(m, at) == OP_TST_ABS_W
            && (bthm_is_branch(m, at + 6, OP_BEQ) || bthm_is_branch(m, at + 6, OP_BNE))) {
            out->last_voice_flag = bthm_u32(m, at + 2);
            out->last_voice_runs_when_set = bthm_is_branch(m, at + 6, OP_BEQ);
            bthm_branch_target(m, at + 6, &at);
        }
        at = skip_record_load(m, at);
        if (at == 0) {
            return NOT_FOUND;
        }
        out->voice_count++;
        uint32_t target = at; // the routine follows the last record load
        bool tail = true;
        if (bthm_is_branch(m, at, OP_BSR)) {
            target = bthm_branch_target(m, at, &at);
            tail = false;
        } else if (bthm_is_branch(m, at, OP_BRA)) {
            target = bthm_branch_target(m, at, &at);
        }
        if (routine != NOT_FOUND && routine != target) {
            return NOT_FOUND;
        }
        routine = target;
        if (tail) {
            break;
        }
    }
    return routine;
}

// The operand of an instruction that takes an immediate word, a word at a pc-relative address or a word at an
// absolute address, depending on the opcode word at `at`
static bool read_word_operand(const BthModule* m, uint32_t at, uint16_t immediate, uint16_t pc_relative,
                              uint16_t absolute, BthWord* out) {
    const uint16_t op = bthm_u16(m, at);
    if (op == immediate) {
        *out = (BthWord) { .constant = bthm_u16(m, at + 2) };
    } else if (op == pc_relative) {
        *out = (BthWord) { .in_module = true, .address = bthm_pc16(m, at + 2) };
    } else if (op == absolute) {
        *out = (BthWord) { .in_module = true, .address = bthm_u32(m, at + 2) };
    } else {
        return false;
    }
    return true;
}

// The first multiplication in the routine scales a note command's length: mulu.w tempo,d0
static bool locate_tempo(const BthModule* m, uint32_t routine, BthReplayer* out) {
    for (uint32_t at = routine; at < routine + ROUTINE_SPAN; at += WORD_BYTES) {
        if (read_word_operand(m, at, OP_MULU_IMM_D0, OP_MULU_PC_D0, OP_MULU_ABS_D0, &out->tempo)) {
            return true;
        }
    }
    return false;
}

// An address in the code that the loader relocated; one it did not relocate is chip memory outside the module, which
// reads as zeros (a module ripped from another address)
static uint32_t code_pointer(const BthImage* image, uint32_t at) {
    return bthi_is_pointer(image, at) ? bthm_u32(&image->module, at) : image->module.size;
}

// The note handler: subq.w #1,d0; add.w note_base(a0),d0; [add the note offset]; lsl.w #1,d0; lea.l period_table,a2
static bool locate_note_handler(const BthImage* image, uint32_t routine, BthReplayer* out) {
    const BthModule* m = &image->module;
    static const uint32_t handler[] = { OP_SUBQ_1_D0, OP_ADD_REC_D0, RECORD_NOTE_BASE };
    uint32_t at = BTH_FIND(m, routine, routine + ROUTINE_SPAN, handler);
    if (at == NOT_FOUND) {
        return false;
    }
    at += 6;
    const uint16_t op = bthm_u16(m, at);
    if (op == OP_ADD_REC_D0 && bthm_u16(m, at + 2) == RECORD_NOTE_BIAS) {
        out->bias_in_record = true;
        at += 4;
    } else if ((op & OP_ADDQ_W_D0_MASK) == OP_ADDQ_W_D0) {
        out->note_bias.constant = (uint16_t)((op >> 9) & 7 ? (op >> 9) & 7 : 8);
        at += 2;
    } else if (read_word_operand(m, at, OP_ADDI_W_D0, OP_ADD_PC_D0, OP_ADD_ABS_D0, &out->note_bias)) {
        at += op == OP_ADD_ABS_D0 ? 6 : 4;
    }
    if (bthm_u16(m, at) != OP_LSL_W_1_D0) {
        return false;
    }
    at += 2;
    if (bthm_u16(m, at) == OP_LEA_ABS_A2) {
        out->period_table = code_pointer(image, at + 2);
    } else if (bthm_u16(m, at) == OP_LEA_PC_A2) {
        out->period_table = bthm_pc16(m, at + 2);
    } else {
        return false;
    }
    return true;
}

// A slide that overshoots its target is clamped: the comparison after adding the step is signed in some replayers
static bool locate_slide(const BthModule* m, uint32_t routine, BthReplayer* out) {
    static const uint32_t add_step[] = { OP_ADD_REC_D0, RECORD_SLIDE_STEP };
    static const uint32_t compare[] = { OP_CMP_REC_D0, RECORD_TARGET_PERIOD };
    const uint32_t step = BTH_FIND(m, routine, routine + ROUTINE_SPAN, add_step);
    if (step == NOT_FOUND) {
        return false;
    }
    const uint32_t cmp = BTH_FIND(m, step, step + 0x10, compare);
    if (cmp == NOT_FOUND) {
        return false;
    }
    const uint16_t branch = bthm_u16(m, cmp + 4) & OP_BRANCH_MASK;
    out->slide_signed = branch == OP_BLT || branch == OP_BLE || branch == OP_BGE || branch == OP_BGT;
    return true;
}

// The sample a held voice repeats: written to the channel's address register early in the routine, as an immediate
// pointer, or one read from a word of the module. An address that is not a relocated pointer is chip memory outside
// the module.
static uint32_t locate_silent(const BthImage* image, uint32_t routine) {
    const BthModule* m = &image->module;
    for (uint32_t at = routine; at < routine + SILENT_SPAN; at += WORD_BYTES) {
        const uint16_t op = bthm_u16(m, at);
        if (op == OP_MOVE_L_IMM_REC_A1 || op == OP_MOVE_L_IMM_A1) {
            return code_pointer(image, at + 2);
        }
        if (op == OP_MOVE_L_PC_REC_A1 || op == OP_MOVE_L_PC_A1) {
            const uint32_t variable = bthm_pc16(m, at + 2);
            return bthi_is_pointer(image, variable) ? bthm_u32(m, variable) : m->size;
        }
    }
    return m->size;
}

// The instrument command loads the sample address with move.l $8(a2),d0; NEW replayers add a base to it
static uint32_t locate_sample_base(const BthModule* m, uint32_t routine) {
    static const uint32_t load[] = { OP_MOVE_L_REC_A2_D0, BTH_INSTRUMENT_SAMPLE_FIELD };
    const uint32_t at = BTH_FIND(m, routine, routine + ROUTINE_SPAN, load);
    if (at != NOT_FOUND && bthm_u16(m, at + 4) == OP_ADD_L_PC_D0) {
        return bthm_u32(m, bthm_pc16(m, at + 6));
    }
    return 0;
}

// The replayer's first instructions tell the engines apart: clr.b d2 then btst.b #2 (a note command with no hold or
// arpeggio), or move.b flags,d0 / andi.b #$c,d0 (the extended replayer); one module sets a4 first.
static bool locate_engine(const BthModule* m, uint32_t routine, BthReplayer* out) {
    static const uint32_t basic[] = { OP_CLR_D2, OP_BTST_REC, FLAG_ONE_SHOT_BIT, RECORD_FLAGS };
    static const uint32_t extended[] = { OP_MOVE_B_FLAGS_D0, RECORD_FLAGS, OP_ANDI_B_D0, FLAGS_ONE_SHOT_OR_HOLD };
    uint32_t start = routine;
    if (bthm_u16(m, start) == OP_LEA_ABS_A4) {
        start += 6;
    }
    if (BTH_MATCH(m, start, basic)) {
        out->engine = BthEngine_Basic;
        return true;
    }
    // Some extended replayers load the channel's address into a1 before the flag test
    uint32_t flags = start + 2;
    if (bthm_u16(m, flags) == OP_MOVEA_REC_A1) {
        flags += 4;
    } else if (bthm_u16(m, flags) == OP_MOVEA_IND_A1) {
        flags += 2;
    }
    if (bthm_u16(m, start) == OP_CLR_D2 && BTH_MATCH(m, flags, extended)) {
        out->engine = BthEngine_Extended;
        return true;
    }
    return false;
}

// What the BEATHOVEN100 hardware routine does with a channel: move.w #1,4(a6); move.l #silent,(a6). The play routine
// of most modules does it for all four channels first (after lea.l hw,a6), and the routine after it has started a
// sample.
static const uint32_t k_silence_channel[] = { OP_MOVE_W_IMM_REC_A6, 1, 4, OP_MOVE_L_IMM_IND_A6 };

// BEATHOVEN100. Some play routines point the four channels at the silent sample first. Then the voice routine runs for
// each voice: [move.l #offset,d7;] lea.l record,a0; bsr routine (d7 is the voice's note offset). Returns the voice
// routine's address.
static uint32_t locate_legacy_play(const BthModule* m, uint32_t play, BthReplayer* out) {
    uint32_t at = play;
    if (bthm_u16(m, at) == OP_MOVEM_PUSH) {
        at += 4;
    }
    while (bthm_u16(m, at) == OP_LEA_ABS_A6 && BTH_MATCH(m, at + 6, k_silence_channel)) {
        out->play_silences_channels = true;
        at += 6 + 2 * nuade_sizeof_array(k_silence_channel) + 4;
    }
    uint32_t routine = NOT_FOUND;
    while (out->voice_count < BTH_VOICES) {
        int32_t note_offset = 0;
        if (bthm_u16(m, at) == OP_MOVE_L_IMM_D7) {
            note_offset = (int32_t)bthm_u32(m, at + 2);
            at += 6;
        }
        if (bthm_u16(m, at) != OP_LEA_ABS_A0 || !bthm_is_branch(m, at + 6, OP_BSR)) {
            break;
        }
        out->note_offsets[out->voice_count++] = note_offset;
        const uint32_t target = bthm_branch_target(m, at + 6, &at);
        if (routine != NOT_FOUND && routine != target) {
            return NOT_FOUND;
        }
        routine = target;
    }
    return out->voice_count == 0 ? NOT_FOUND : routine;
}

// The silent sample is the one the replayer's hardware routine sets the repeat registers to after it started a sample
static uint32_t locate_legacy_silent(const BthModule* m) {
    const uint32_t at = BTH_FIND(m, 0, m->size, k_silence_channel);
    return at == NOT_FOUND ? m->size : bthm_u32(m, at + 2 * nuade_sizeof_array(k_silence_channel));
}

// A note number is turned into an index of the period table: the voice's note base is added, the module's note offset
// (add.l d7,d0) may be, the origin is subtracted (subi.l #1,d0) and the index is scaled to a byte offset with a 16 bit
// multiplication or a long shift. The arpeggio step does the same with the number from its table, in d1.
typedef struct LegacyIndex {
    bool uses_note_offset;
    uint32_t origin;
    bool scaled_unsigned;
} LegacyIndex;

// The instructions after the note base is added, at `at`; the registers are those of the note handler or the arpeggio
static bool read_legacy_index(const BthModule* m, uint32_t at, uint16_t add_offset, uint16_t subtract_origin,
                              LegacyIndex* out) {
    out->uses_note_offset = bthm_u16(m, at) == add_offset;
    at += out->uses_note_offset ? 2 : 0;
    if (bthm_u16(m, at) != subtract_origin) {
        return false;
    }
    out->origin = bthm_u32(m, at + 2);
    out->scaled_unsigned = bthm_u16(m, at + 6) == OP_MULU_IMM_D0 || bthm_u16(m, at + 6) == OP_MULU_IMM_D1;
    return true;
}

// The legacy replayers differ in the index of the period table (see above) and in whether lengths are scaled. The
// period table is loaded right before the note handler adds the note base.
static bool locate_legacy_features(const BthImage* image, uint32_t routine, BthReplayer* out) {
    const BthModule* m = &image->module;
    static const uint32_t add_base[] = { OP_ADD_L_REC_D0, RECORD_LEGACY_NOTE_BASE };
    static const uint32_t add_arpeggio_base[] = { OP_ADD_L_REC_D1, RECORD_LEGACY_NOTE_BASE };
    static const uint32_t scale_lengths[] = { OP_LSR_W_2_D0, OP_MULU_IMM_D0, 5 };
    const uint32_t note = BTH_FIND(m, routine, routine + ROUTINE_SPAN, add_base);
    const uint32_t arpeggio = BTH_FIND(m, routine, routine + ROUTINE_SPAN, add_arpeggio_base);
    LegacyIndex note_index;
    LegacyIndex arpeggio_index;
    if (note == NOT_FOUND || arpeggio == NOT_FOUND || bthm_u16(m, note - 6) != OP_LEA_ABS_A2
        || !read_legacy_index(m, note + 4, OP_ADD_L_D7_D0, OP_SUBI_L_D0, &note_index)
        || !read_legacy_index(m, arpeggio + 4, OP_ADD_L_D7_D1, OP_SUBI_L_D1, &arpeggio_index)) {
        return false;
    }
    out->period_table = code_pointer(image, note - 4);
    out->uses_note_offset = note_index.uses_note_offset;
    out->note_origin = note_index.origin;
    out->arpeggio_origin = arpeggio_index.origin;
    out->index_unsigned = note_index.scaled_unsigned;
    out->durations_scaled = BTH_FIND(m, routine, routine + ROUTINE_SPAN, scale_lengths) != NOT_FOUND;
    return true;
}

bool bth_locate(const BthImage* image, const BthHeader* header, BthReplayer* out) {
    const BthModule* m = &image->module;
    *out = (BthReplayer) { 0 };
    if (image->version == BTH_VERSION_100) {
        out->engine = BthEngine_Legacy;
        const uint32_t legacy = locate_legacy_play(m, header->play, out);
        out->silent = locate_legacy_silent(m);
        return legacy != NOT_FOUND && locate_legacy_features(image, legacy, out);
    }
    const uint32_t routine = locate_play(m, header->play, out);
    if (routine == NOT_FOUND || !locate_engine(m, routine, out) || !locate_tempo(m, routine, out)
        || !locate_note_handler(image, routine, out) || !locate_slide(m, routine, out)) {
        return false;
    }
    static const uint32_t note_bias_command[] = { OP_CMPI_W_D0, BTH_COMMAND_NOTE_BIAS };
    out->bias_command = BTH_FIND(m, routine, routine + ROUTINE_SPAN, note_bias_command) != NOT_FOUND;
    out->silent = locate_silent(image, routine);
    out->sample_base = locate_sample_base(m, routine);
    return true;
}
