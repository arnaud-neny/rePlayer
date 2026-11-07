// SPDX-License-Identifier: MIT
// Desire player: the module check, the code scan that finds the replayer's tables, and the readers of the module's
// records. The eagleplayer scans the module's code for instructions and reads the offsets out of them; so does this.

#include "dsr_internal.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define NOT_FOUND 0xFFFFFFFFu
#define MIN_SIZE 0x9C4
#define RECORD_SIZE 0x10         // bytes of the data block per voice: the fields of the voices are interleaved
#define RECORD_MARKER 0x00010101 // at byte 8 of each record
#define RECORD_MARKER_AT 8
#define CHECK_SCAN_START 0x48
#define CHECK_SCAN_SIZE 0x190
#define INIT_SCAN_START 0xF0
#define MAX_SAMPLE_OFFSET 0xFFFF // the sample offset table holds words
#define ANY 0x10000u             // in a code pattern: any word

// 68000 instruction words, by mnemonic. `d16` is a 16 bit displacement word that follows, `(a3,d1.w)` an index
// extension word (EXT_*).
#define OP_LEA_PC_A0 0x41FA // lea d16(pc),a0
#define OP_LEA_PC_A1 0x43FA
#define OP_LEA_PC_A3 0x47FA
#define OP_LEA_PC_A4 0x49FA
#define OP_LEA_ABS_A1 0x43F9 // lea abs.l,a1: the long follows
#define OP_LEA_ABS_A2 0x45F9
#define OP_BSR_W 0x6100
#define OP_BRA_W 0x6000
#define OP_BEQ_W 0x6700
#define OP_CMPI_B_D16_A0 0x0C28 // cmpi.b #imm,d16(a0)
#define OP_MOVE_W_IMM_D16_A2 0x357C
#define OP_MOVE_W_IMM_D16_A0 0x317C
#define OP_MOVE_B_IMM_D16_A0 0x117C
#define OP_MOVE_B_D16_A0_D1 0x1228 // move.b d16(a0),d1
#define OP_MOVE_W_D16_A0_D1 0x3228
#define OP_MOVE_B_IDX_A3_D1 0x1233 // move.b (a3,xn.w),d1
#define OP_MOVE_W_IDX_A3_D1 0x3233
#define OP_MOVE_W_IDX_A3_D2 0x3433
#define OP_MOVE_W_IDX_A3_D3 0x3633
#define OP_MOVE_B_IDX_A3_D16_A0 0x1173 // move.b (a3,xn.w),d16(a0)
#define OP_MOVE_W_IDX_A3_D16_A0 0x3173
#define OP_MOVE_W_IDX_A3_D16_A1 0x3373
#define OP_MOVE_L_D2_D16_A1 0x2342
#define OP_CLR_W_D2 0x4242
#define OP_CLR_L_D1 0x4281
#define OP_CLR_W_D16_A0 0x4268
#define OP_SUBI_W_D3 0x0443
#define OP_SUBI_W_D16_A4 0x046C
#define OP_ADDI_L_D2 0x0682
#define OP_ADD_L_A3_D2 0xD48B
#define OP_ASL_L_D2 0xE182 // asl.l #n,d2: the count n (1 to 8, 8 as 0) is in bits 9 to 11
#define OP_ASL_L_D2_MASK 0xF1FF
#define AUDXLEN 0xA4 // d16 of a store to AUDxLEN
#define EXT_D1_W 0x1000
#define EXT_D2_W 0x2000

uint8_t dsr_u8(const DsrMemory* m, int64_t offset) {
    return offset >= 0 && offset < (int64_t)m->size ? m->data[offset] : 0;
}

uint16_t dsr_u16(const DsrMemory* m, int64_t offset) {
    return (uint16_t)(dsr_u8(m, offset) << 8 | dsr_u8(m, offset + 1));
}

int16_t dsr_s16(const DsrMemory* m, int64_t offset) {
    return (int16_t)dsr_u16(m, offset);
}

uint32_t dsr_u32(const DsrMemory* m, int64_t offset) {
    return (uint32_t)dsr_u16(m, offset) << 16 | dsr_u16(m, offset + 2);
}

int dsr_record_count(const DsrVersion* version) {
    return version->has_effect ? DSR_MAX_RECORDS : DSR_VOICES;
}

DsrMode dsr_decode_mode(uint8_t state) {
    if (state == 0x80) {
        return DsrMode_Idle;
    }
    if ((state & 0x80) != 0) {
        return DsrMode_Running;
    }
    if (state == 0x00) {
        return DsrMode_Stop;
    }
    return state == 0x40 ? DsrMode_Fade : DsrMode_Start;
}

DsrRequest dsr_decode_request(uint8_t request) {
    const bool from_song_list = (request & 0x40) != 0;
    return (DsrRequest) { .pending = request != 0,
                          .from_song_list = from_song_list,
                          .index = from_song_list ? request & 0x3F : request };
}

// The index registers of the replayer are words that the 68000 sign extends when it adds them to an address.

static uint32_t indexed(uint32_t table, uint16_t index) {
    return table + (uint32_t)(int32_t)(int16_t)index;
}

uint8_t dsr_read_song_list(const DsrModule* module, uint16_t index) {
    return dsr_u8(&module->memory, indexed(module->tables.song_lists, index));
}

uint16_t dsr_read_order(const DsrModule* module, uint16_t at) {
    return dsr_u16(&module->memory, indexed(module->tables.orders, at));
}

int8_t dsr_read_modulation(const DsrModule* module, uint16_t index) {
    return (int8_t)dsr_u8(&module->memory, indexed(module->tables.modulation, index));
}

uint16_t dsr_read_period(const DsrModule* module, uint16_t index) {
    return dsr_u16(&module->memory, module->tables.periods + 2 * (uint32_t)index);
}

DsrInstrument dsr_read_instrument(const DsrModule* module, uint8_t number) {
    const DsrMemory* m = &module->memory;
    const DsrTables* t = &module->tables;
    const DsrVersion* v = &module->version;
    const uint16_t length = dsr_u16(m, t->sample_lengths + 2 * (uint32_t)number);
    return (DsrInstrument) {
        .sample = ((uint32_t)dsr_u16(m, t->sample_offsets + 2 * (uint32_t)number) << v->sample_shift) + v->sample_skip,
        .length = (uint16_t)(length - v->length_trim),
        .transpose = dsr_u8(m, t->instrument_transposes + number),
    };
}

DsrCommandArgs dsr_read_command_args(const DsrModule* module, uint16_t at) {
    DsrCommandArgs args;
    for (uint32_t i = 0; i < DSR_COMMAND_ARGS; i++) {
        args.byte[i] = dsr_u8(&module->memory, (uint32_t)at + 1 + i);
    }
    return args;
}

// The data block. The fields of the voice records sit in groups, each group RECORD_SIZE * records bytes long and
// holding one RECORD_SIZE slice per voice; a field is at voice * RECORD_SIZE + group start + its place in the slice.
// The places are named once here, for the record reader and for the code patterns.

typedef enum DsrGroup {
    DsrGroup_Sequence,
    DsrGroup_Modulation,
    DsrGroup_Envelope,
    DsrGroup_Pitch,
    DsrGroup_Effect, // after the pitch group; the places below it are negative
} DsrGroup;

typedef enum DsrPlace {
    // Sequence
    DsrPlace_State = 0,
    DsrPlace_Request = 1,
    DsrPlace_PitchBase = 2,
    DsrPlace_Speed = 9,
    DsrPlace_Counter = 0xA,
    DsrPlace_LoopCount = 0xB,
    DsrPlace_LoopPos = 0xC,
    DsrPlace_Pos = 0xE,
    // Modulation
    DsrPlace_DmaMask = 6,
    DsrPlace_Modulation = 8,
    DsrPlace_ModulationPos = 0xA,
    DsrPlace_ModulationLength = 0xB,
    DsrPlace_ModulationBase = 0xC,
    DsrPlace_Hold = 0xE,
    DsrPlace_HoldCount = 0xF,
    // Envelope
    DsrPlace_Volume = 0,
    DsrPlace_Envelope = 2,
    DsrPlace_Attack = 4,
    DsrPlace_Decay = 6,
    DsrPlace_Sustain = 8,
    DsrPlace_Release = 0xA,
    DsrPlace_Level = 0xC,
    DsrPlace_Transpose = 0xE,
    DsrPlace_KeepEnvelope = 0xF,
    // Pitch
    DsrPlace_SlideSpeed = 0,
    DsrPlace_Pitch = 2,
    DsrPlace_Target = 4,
    DsrPlace_PitchOffset = 6,
    DsrPlace_InstrumentTranspose = 8,
    DsrPlace_DmaPending = 0xA,
    DsrPlace_OrderPos = 0xC,
    DsrPlace_OrderBase = 0xE,
    // Effect: voice 3's variables in the later replayers
    DsrPlace_MuteTicks = -4,
    DsrPlace_SavedSample = 0,
    DsrPlace_SavedLength = 4,
} DsrPlace;

// The offset of a field of voice 0.

static uint32_t field(const DsrVersion* version, DsrGroup group, DsrPlace place) {
    const uint32_t group_size = RECORD_SIZE * (uint32_t)dsr_record_count(version);
    return group * group_size + (uint32_t)place;
}

// The Paula channel whose DMA a voice's record switches: its own, and the sound effect voice's is voice 3's.

static int dma_channel_of(int voice) {
    return voice < DSR_VOICES ? voice : DSR_VOICES - 1;
}

typedef struct RecordReader {
    const DsrMemory* m;
    const DsrVersion* version;
    uint32_t at; // the voice's record
} RecordReader;

static uint32_t record_field(const RecordReader* r, DsrGroup group, DsrPlace place) {
    return r->at + field(r->version, group, place);
}

static uint8_t record_u8(const RecordReader* r, DsrGroup group, DsrPlace place) {
    return dsr_u8(r->m, record_field(r, group, place));
}

static uint16_t record_u16(const RecordReader* r, DsrGroup group, DsrPlace place) {
    return dsr_u16(r->m, record_field(r, group, place));
}

static int16_t record_s16(const RecordReader* r, DsrGroup group, DsrPlace place) {
    return dsr_s16(r->m, record_field(r, group, place));
}

// A voice's DMACON bit: its channel's, or 0 for a voice that does not start DMA (the module is checked for both).

static uint16_t read_dma_mask(const DsrModule* module, int voice) {
    const RecordReader r = { &module->memory, &module->version, RECORD_SIZE * (uint32_t)voice };
    return record_u16(&r, DsrGroup_Modulation, DsrPlace_DmaMask);
}

DsrVoice dsr_read_voice(const DsrModule* module, int voice) {
    const RecordReader r = { &module->memory, &module->version, RECORD_SIZE * (uint32_t)voice };
    const bool effect = module->version.has_effect;
    const uint8_t state = record_u8(&r, DsrGroup_Sequence, DsrPlace_State);
    const DsrMode mode = dsr_decode_mode(state);
    return (DsrVoice) {
        .mode = mode,
        .request
        = dsr_decode_request(mode == DsrMode_Start ? state : record_u8(&r, DsrGroup_Sequence, DsrPlace_Request)),
        .pitch_base = record_s16(&r, DsrGroup_Sequence, DsrPlace_PitchBase),
        .speed = record_u8(&r, DsrGroup_Sequence, DsrPlace_Speed),
        .counter = record_u8(&r, DsrGroup_Sequence, DsrPlace_Counter),
        .loop_count = record_u8(&r, DsrGroup_Sequence, DsrPlace_LoopCount),
        .loop_pos = record_u16(&r, DsrGroup_Sequence, DsrPlace_LoopPos),
        .pos = record_u16(&r, DsrGroup_Sequence, DsrPlace_Pos),
        .dma_channel = dma_channel_of(voice),
        .dma_enabled = read_dma_mask(module, voice) != 0,
        .modulation = record_s16(&r, DsrGroup_Modulation, DsrPlace_Modulation),
        .modulation_pos = record_u8(&r, DsrGroup_Modulation, DsrPlace_ModulationPos),
        .modulation_length = record_u8(&r, DsrGroup_Modulation, DsrPlace_ModulationLength),
        .modulation_base = record_u16(&r, DsrGroup_Modulation, DsrPlace_ModulationBase),
        .hold = record_u8(&r, DsrGroup_Modulation, DsrPlace_Hold),
        .hold_count = record_u8(&r, DsrGroup_Modulation, DsrPlace_HoldCount),
        .volume = record_u16(&r, DsrGroup_Envelope, DsrPlace_Volume),
        .envelope = record_u16(&r, DsrGroup_Envelope, DsrPlace_Envelope),
        .attack = record_u16(&r, DsrGroup_Envelope, DsrPlace_Attack),
        .decay = record_u16(&r, DsrGroup_Envelope, DsrPlace_Decay),
        .sustain = record_u16(&r, DsrGroup_Envelope, DsrPlace_Sustain),
        .release = record_u16(&r, DsrGroup_Envelope, DsrPlace_Release),
        .level = record_u16(&r, DsrGroup_Envelope, DsrPlace_Level),
        .transpose = record_u8(&r, DsrGroup_Envelope, DsrPlace_Transpose),
        .keep_envelope = record_u8(&r, DsrGroup_Envelope, DsrPlace_KeepEnvelope) != 0,
        .slide_speed = record_u16(&r, DsrGroup_Pitch, DsrPlace_SlideSpeed),
        .pitch = record_u16(&r, DsrGroup_Pitch, DsrPlace_Pitch),
        .target = record_u16(&r, DsrGroup_Pitch, DsrPlace_Target),
        .pitch_offset = record_s16(&r, DsrGroup_Pitch, DsrPlace_PitchOffset),
        .instrument_transpose = record_u8(&r, DsrGroup_Pitch, DsrPlace_InstrumentTranspose),
        .dma_pending = record_u16(&r, DsrGroup_Pitch, DsrPlace_DmaPending) != 0,
        .order_pos = record_u16(&r, DsrGroup_Pitch, DsrPlace_OrderPos),
        .order_base = record_u16(&r, DsrGroup_Pitch, DsrPlace_OrderBase),
        .mute_ticks = effect ? record_u8(&r, DsrGroup_Effect, DsrPlace_MuteTicks) : 0,
        .saved_sample = effect ? dsr_u32(r.m, record_field(&r, DsrGroup_Effect, DsrPlace_SavedSample)) : 0,
        .saved_length = effect ? record_u16(&r, DsrGroup_Effect, DsrPlace_SavedLength) : 0,
        .display_note = -1,
        .display_instrument = -1,
    };
}

// Code scan. A pattern is a list of instruction words, ANY for a word that does not matter.

// The address that the pc relative operand of the instruction at `at` (`lea d16(pc),an`, `bsr.w`, `bra.w`) names.

static uint32_t pc_target(const DsrMemory* m, uint32_t at) {
    return at + 2 + (uint32_t)(int32_t)dsr_s16(m, at + 2);
}

static bool words_match(const DsrMemory* m, uint32_t at, const uint32_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (words[i] != ANY && dsr_u16(m, at + 2 * i) != words[i]) {
            return false;
        }
    }
    return true;
}

// The offset of the first match of the pattern in [from, to), or NOT_FOUND.

static uint32_t find_code(const DsrMemory* m, uint32_t from, uint32_t to, const uint32_t* words, uint32_t count) {
    for (uint32_t at = from; at + 2 * count <= to; at += 2) {
        if (words_match(m, at, words, count)) {
            return at;
        }
    }
    return NOT_FOUND;
}

// The address that the `lea d16(pc),a3` at the start of a match names, or NOT_FOUND.

static uint32_t find_table(const DsrMemory* m, uint32_t from, uint32_t to, const uint32_t* words, uint32_t count) {
    const uint32_t at = find_code(m, from, to, words, count);
    return at == NOT_FOUND ? NOT_FOUND : pc_target(m, at);
}

// Check2: four records marked in the data block, then the replayer's first instructions within CHECK_SCAN_SIZE bytes:
// `lea d16(pc),a4; lea $dff000,a2; move.w #$ff,$9e(a2); lea module(pc),a0`. Returns the offset of the first one.

static uint32_t check_entry(const DsrMemory* m) {
    if (m->size <= MIN_SIZE) {
        return NOT_FOUND;
    }
    for (uint32_t i = 0; i < DSR_VOICES; i++) {
        if (dsr_u32(m, RECORD_MARKER_AT + RECORD_SIZE * i) != RECORD_MARKER) {
            return NOT_FOUND;
        }
    }
    static const uint32_t prologue[]
        = { OP_LEA_PC_A4, ANY, OP_LEA_ABS_A2, 0x00DF, 0xF000, OP_MOVE_W_IMM_D16_A2, 0x00FF, 0x009E, OP_LEA_PC_A0 };
    for (uint32_t at = CHECK_SCAN_START; at < CHECK_SCAN_START + CHECK_SCAN_SIZE; at += 2) {
        if (dsr_u16(m, at) == OP_LEA_PC_A4) {
            const bool ok = words_match(m, at, prologue, nuade_sizeof_array(prologue)) && pc_target(m, at + 16) == 0;
            return ok ? at : NOT_FOUND;
        }
    }
    return NOT_FOUND;
}

// InitPlayer's entry: the first `lea d16(pc),a4` from INIT_SCAN_START on.

static uint32_t init_entry(const DsrMemory* m) {
    static const uint32_t lea[] = { OP_LEA_PC_A4 };
    return find_code(m, INIT_SCAN_START, m->size, lea, nuade_sizeof_array(lea));
}

// The entry routine sets up the voices: `lea voice(pc),a0; lea $dff0n0,a1; bsr step` for voices 0 to 2, then voice 3,
// either the same with a `bra` (four records), or with a sound effect (five records) that wraps the step in its own
// code. Returns the `step` routine's offset, or NOT_FOUND.

static uint32_t parse_entry(const DsrMemory* m, uint32_t entry, DsrVersion* version) {
    uint32_t step = NOT_FOUND;
    uint32_t at = entry + 16;
    for (uint32_t voice = 0; voice < DSR_VOICES - 1; voice++, at += 14) {
        const uint32_t setup[]
            = { OP_LEA_PC_A0, ANY, OP_LEA_ABS_A1, 0x00DF, 0xF000 + RECORD_SIZE * voice, OP_BSR_W, ANY };
        if (!words_match(m, at, setup, nuade_sizeof_array(setup)) || pc_target(m, at) != RECORD_SIZE * voice) {
            return NOT_FOUND;
        }
        const uint32_t target = pc_target(m, at + 10);
        if (step != NOT_FOUND && step != target) {
            return NOT_FOUND;
        }
        step = target;
    }
    const uint32_t voice3 = RECORD_SIZE * (DSR_VOICES - 1);
    // Four records: `lea voice3(pc),a0; lea $dff030,a1; bra step`
    static const uint32_t direct[] = { OP_LEA_PC_A0, ANY, OP_LEA_ABS_A1, 0x00DF, 0xF030, OP_BRA_W, ANY };
    // Five records: `lea voice3(pc),a0; move.w #8,mask(a0); lea dummy(pc),a1; move.b mute(a0),d1`
    const DsrVersion effect_version = { .has_effect = true };
    const uint32_t effect[] = { OP_LEA_PC_A0,
                                ANY,
                                OP_MOVE_W_IMM_D16_A0,
                                0x0008,
                                field(&effect_version, DsrGroup_Modulation, DsrPlace_DmaMask),
                                OP_LEA_PC_A1,
                                ANY,
                                OP_MOVE_B_D16_A0_D1,
                                field(&effect_version, DsrGroup_Effect, DsrPlace_MuteTicks) };
    if (words_match(m, at, direct, nuade_sizeof_array(direct)) && pc_target(m, at) == voice3) {
        version->has_effect = false;
        return step == pc_target(m, at + 10) ? step : NOT_FOUND;
    }
    if (words_match(m, at, effect, nuade_sizeof_array(effect)) && pc_target(m, at) == voice3) {
        version->has_effect = true;
        return step;
    }
    return NOT_FOUND;
}

// The step routine of a voice starts `cmpi.b #$80,(a0); beq rts; move.w pending(a0),d1`: the field is the pending DMA
// word, which confirms the layout of the data block.

static bool check_step(const DsrMemory* m, uint32_t step, const DsrVersion* version) {
    const uint32_t words[] = { OP_CMPI_B_D16_A0,
                               0x0080,
                               0x0000,
                               OP_BEQ_W,
                               ANY,
                               OP_MOVE_W_D16_A0_D1,
                               field(version, DsrGroup_Pitch, DsrPlace_DmaPending) };
    return words_match(m, step, words, nuade_sizeof_array(words));
}

// The instrument's sample start: `lea offsets(pc),a3; move.w (a3,d1.w),d2; lea module(pc),a3; asl.l #n,d2;
// add.l a3,d2`, optionally `addi.l #skip,d2`, then `move.l d2,AUDxLC`. Returns the offset table.

static uint32_t parse_sample_start(const DsrMemory* m, uint32_t from, uint32_t to, DsrVersion* version) {
    static const uint32_t head[] = { OP_LEA_PC_A3, ANY, OP_MOVE_W_IDX_A3_D2, EXT_D1_W, OP_LEA_PC_A3, ANY };
    const uint32_t at = find_code(m, from, to, head, nuade_sizeof_array(head));
    if (at == NOT_FOUND || pc_target(m, at + 8) != 0) {
        return NOT_FOUND;
    }
    const uint16_t shift_op = dsr_u16(m, at + 12);
    if ((shift_op & OP_ASL_L_D2_MASK) != OP_ASL_L_D2 || dsr_u16(m, at + 14) != OP_ADD_L_A3_D2) {
        return NOT_FOUND;
    }
    const uint32_t count = (shift_op >> 9) & 7;
    version->sample_shift = (uint8_t)(count == 0 ? 8 : count);
    version->sample_skip = 0;
    uint32_t store = at + 16;
    if (dsr_u16(m, store) == OP_ADDI_L_D2) {
        if (dsr_u32(m, store + 2) > 0xFFFF) {
            return NOT_FOUND;
        }
        version->sample_skip = (uint16_t)dsr_u32(m, store + 2);
        store += 6;
    }
    return dsr_u16(m, store) == OP_MOVE_L_D2_D16_A1 ? pc_target(m, at) : NOT_FOUND;
}

// The instrument length table: `move.w (a3,d1.w),d3; subi.w #trim,d3` (trim words off the length), or the length
// stored straight to AUDxLEN.

static uint32_t find_length_table(const DsrMemory* m, uint32_t from, uint32_t to, uint16_t* trim) {
    static const uint32_t direct[] = { OP_LEA_PC_A3, ANY, OP_MOVE_W_IDX_A3_D16_A1, EXT_D1_W, AUDXLEN };
    static const uint32_t trimmed[] = { OP_LEA_PC_A3, ANY, OP_MOVE_W_IDX_A3_D3, EXT_D1_W, OP_SUBI_W_D3, ANY };
    const uint32_t at_direct = find_code(m, from, to, direct, nuade_sizeof_array(direct));
    if (at_direct != NOT_FOUND) {
        *trim = 0;
        return pc_target(m, at_direct);
    }
    const uint32_t at_trimmed = find_code(m, from, to, trimmed, nuade_sizeof_array(trimmed));
    if (at_trimmed == NOT_FOUND) {
        return NOT_FOUND;
    }
    *trim = dsr_u16(m, at_trimmed + 10);
    return pc_target(m, at_trimmed);
}

// The sound effect's end clears voice 3's pending DMA and sets its mute counter: `clr.w mask(a0)`, in the later
// replayer `clr.w` of the envelope state and level, then `move.b #n,mute(a0)`.

static bool parse_effect_end(const DsrMemory* m, uint32_t from, uint32_t to, DsrVersion* version) {
    const uint32_t clear_mask[] = { OP_CLR_W_D16_A0, field(version, DsrGroup_Modulation, DsrPlace_DmaMask) };
    const uint32_t clear_envelope[] = { OP_CLR_W_D16_A0, field(version, DsrGroup_Envelope, DsrPlace_Envelope),
                                        OP_CLR_W_D16_A0, field(version, DsrGroup_Envelope, DsrPlace_Level) };
    const uint32_t mute[] = { OP_MOVE_B_IMM_D16_A0, ANY, field(version, DsrGroup_Effect, DsrPlace_MuteTicks) };
    const uint32_t at = find_code(m, from, to, mute, nuade_sizeof_array(mute));
    if (at == NOT_FOUND) {
        return false;
    }
    version->effect_mute_ticks = (uint8_t)dsr_u16(m, at + 2);
    if (words_match(m, at - 4, clear_mask, nuade_sizeof_array(clear_mask))) {
        version->effect_resets_envelope = false;
        return true;
    }
    version->effect_resets_envelope = words_match(m, at - 8, clear_envelope, nuade_sizeof_array(clear_envelope))
                                      && words_match(m, at - 12, clear_mask, nuade_sizeof_array(clear_mask));
    return version->effect_resets_envelope;
}

// The fade: each voice's volume stage drops the counter by the immediate of `subi.w #step,fade(a4)`.

static uint16_t find_fade_step(const DsrMemory* m, uint32_t from, uint32_t to) {
    static const uint32_t subi[] = { OP_SUBI_W_D16_A4, ANY, 2 };
    const uint32_t at = find_code(m, from, to, subi, nuade_sizeof_array(subi));
    return at == NOT_FOUND ? 0 : dsr_u16(m, at + 2);
}

// The number of subsongs: InitPlayer counts the bytes of the first voice's list after its first entry, up to a zero
// or a repeat.

static uint32_t count_subsongs(const DsrMemory* m, uint32_t list) {
    uint32_t count = 0;
    for (uint32_t at = list + 1; at < m->size; at++) {
        const uint8_t value = dsr_u8(m, at);
        if (value == 0) {
            break;
        }
        count++;
        if (value == dsr_u8(m, at + 1)) {
            break;
        }
    }
    return count;
}

// A sample can start up to 0xFFFF << shift (+ skip) bytes into the module, past its end in a short module.

static uint32_t sample_start_reach(const DsrVersion* v) {
    return ((uint32_t)MAX_SAMPLE_OFFSET << v->sample_shift) + v->sample_skip;
}

// Each voice's DMA bit is its own channel's, or 0 for a voice that does not start DMA.

static bool check_dma_masks(const DsrModule* module) {
    for (int voice = 0; voice < dsr_record_count(&module->version); voice++) {
        const uint16_t mask = read_dma_mask(module, voice);
        if (mask != 0 && mask != 1u << dma_channel_of(voice)) {
            return false;
        }
    }
    return true;
}

bool dsr_module_locate(DsrModule* module, const uint8_t* data, size_t size) {
    *module = (DsrModule) { .memory = { .data = data, .size = (uint32_t)size, .capacity = (uint32_t)size } };
    if (size > 0x7FFFFFFF) {
        return false;
    }
    const DsrMemory* m = &module->memory;
    const uint32_t entry = check_entry(m);
    if (entry == NOT_FOUND || init_entry(m) != entry) {
        return false;
    }
    const uint32_t code_end = pc_target(m, entry); // the replayer's own variables follow its code
    if (code_end <= entry || code_end > m->size) {
        return false;
    }
    DsrVersion* version = &module->version;
    const uint32_t step = parse_entry(m, entry, version);
    if (step == NOT_FOUND || !check_step(m, step, version) || !check_dma_masks(module)) {
        return false;
    }

    DsrTables* t = &module->tables;
    const uint32_t song_list[] = { OP_LEA_PC_A3, ANY, OP_MOVE_B_IDX_A3_D1, EXT_D2_W };
    const uint32_t order[]
        = { OP_LEA_PC_A3, ANY, OP_MOVE_W_IDX_A3_D16_A0, EXT_D1_W, field(version, DsrGroup_Sequence, DsrPlace_Pos) };
    const uint32_t modulation[] = { OP_LEA_PC_A3,        ANY,
                                    OP_CLR_W_D2,         OP_CLR_L_D1,
                                    OP_MOVE_B_D16_A0_D1, field(version, DsrGroup_Modulation, DsrPlace_ModulationPos) };
    const uint32_t period[] = { OP_LEA_PC_A3, ANY, OP_MOVE_W_IDX_A3_D1, EXT_D1_W };
    const uint32_t transpose[] = { OP_LEA_PC_A3, ANY, OP_MOVE_B_IDX_A3_D16_A0, EXT_D1_W,
                                   field(version, DsrGroup_Pitch, DsrPlace_InstrumentTranspose) };
    t->song_lists = find_table(m, entry, code_end, song_list, nuade_sizeof_array(song_list));
    t->orders = find_table(m, entry, code_end, order, nuade_sizeof_array(order));
    t->modulation = find_table(m, entry, code_end, modulation, nuade_sizeof_array(modulation));
    t->periods = find_table(m, entry, code_end, period, nuade_sizeof_array(period));
    t->instrument_transposes = find_table(m, entry, code_end, transpose, nuade_sizeof_array(transpose));
    t->sample_lengths = find_length_table(m, entry, code_end, &version->length_trim);
    t->sample_offsets = parse_sample_start(m, entry, code_end, version);
    if (t->song_lists == NOT_FOUND || t->orders == NOT_FOUND || t->modulation == NOT_FOUND || t->periods == NOT_FOUND
        || t->sample_offsets == NOT_FOUND || t->instrument_transposes == NOT_FOUND || t->sample_lengths == NOT_FOUND) {
        return false;
    }
    version->fade_step = find_fade_step(m, entry, code_end);
    if (version->fade_step == 0) {
        return false;
    }
    if (version->has_effect && !parse_effect_end(m, entry, code_end, version)) {
        return false;
    }
    module->subsongs = count_subsongs(m, t->song_lists);
    if (module->subsongs > DSR_MAX_SUBSONGS) {
        module->subsongs = DSR_MAX_SUBSONGS;
    }
    return module->subsongs > 0;
}

DsrStatus dsr_module_load(DsrModule* module, const uint8_t* data, size_t size) {
    if (!dsr_module_locate(module, data, size)) {
        return DsrStatus_InvalidData;
    }
    // zeros up to the furthest sample start, then the Paula tail: every sample start has room for any AUDxLEN
    const uint32_t reach = sample_start_reach(&module->version);
    const uint32_t capacity = size > reach ? (uint32_t)size : reach;
    module->owned = nuade_paula_padded_copy(nullptr, capacity);
    if (module->owned == nullptr) {
        return DsrStatus_OutOfMemory;
    }
    memcpy(module->owned, data, size);
    module->memory.data = module->owned;
    module->memory.capacity = capacity;
    return DsrStatus_Ok;
}

void dsr_module_free(DsrModule* module) {
    free(module->owned);
    module->owned = nullptr;
}
