// SPDX-License-Identifier: MIT
// Special FX sound driver (.jd).
//
// A .jd file is a relocatable 68k program: the group's sound driver plus its song data and 8SVX samples. The driver
// differs a little from game to game (about a dozen builds). UADE's eagleplayer finds the driver's entry points by the
// bra.w jump table at the start of the file, relocates the absolute addresses inside it, runs its setup routine
// (which builds the instrument table from the 8SVX forms) and then calls its init and play routines.
//
// This file does the same without a CPU: `locate_*` finds what the eagleplayer and the driver address (tables, version
// flags) from the code bytes, `build_instruments` replays the setup routine, and the rest is the driver's tick.
// Addresses in the file are absolute addresses of the original link; `JdModule.base` turns them into file offsets.

#include <nuade/players/specialfx.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include <stdlib.h>
#include <string.h>

// An address that points nowhere (the driver's null pointer; module offsets 0 are valid). A read at it gives 0, as
// reads of the driver's memory outside the module do: a channel that starts a note before any envelope command reads
// its envelope from address 0 of the Amiga and finds level 0. The envelope's second byte (the hold) is read at NO_ADDR
// + 1, which wraps to file offset 0; only a note waiting longer than that hold would notice, and none does in the
// corpus.
#define NO_ADDR 0xFFFFFFFFu

// Where UADE's DeliTracker glue loads the module. The eagleplayer relocates the absolute addresses in the driver's code
// to it, which shows when the driver reads words of its own code (a note below the period table).
#define LOAD_ADDRESS 0xF000u

// Deepest FORM nesting the IFF walker follows.
#define MAX_FORM_DEPTH 8

// Flag word of a channel.
enum {
    Flag_Arpeggio = 0x01,    // first note plays offset for a few ticks
    Flag_PitchSeq = 0x02,    // pitch sequence
    Flag_Vibrato = 0x04,     // vibrato
    Flag_Slide = 0x08,       // slide down from a higher note
    Flag_Envelope = 0x10,    // volume envelope runs
    Flag_FixedLen = 0x20,    // every note has the same length
    Flag_NoTranspose = 0x40, // instrument plays notes untransposed
    Flag_Rest = 0x80,        // the channel is resting until the next note
    Flag_Sfx = 0x4000,       // the channel plays a sound effect (uses the second volume mask)
    Flag_Music = 0x8000,     // the channel plays music
    Flag_Kind = Flag_Sfx | Flag_Music,
    // Arpeggio, pitch sequence and vibrato replace each other
    Flag_PitchEffects = Flag_Arpeggio | Flag_PitchSeq | Flag_Vibrato,
};

// Commands of the pattern stream (0x80 + n). Bytes the driver's jump table leaves empty do nothing.
enum {
    Cmd_EndPattern = 0,
    Cmd_PatternLoop,
    Cmd_FixedLenOn = 4,
    Cmd_FixedLenOff,
    Cmd_LongWait,
    Cmd_Rest,
    Cmd_Slide,
    Cmd_ArpeggioOn,
    Cmd_ArpeggioOff,
    Cmd_VibratoOn,
    Cmd_VibratoOff,
    Cmd_PitchSeqOn,
    Cmd_PitchSeqOff,
    Cmd_Envelope,
    Cmd_Instrument,
};

// Commands of the track list (0x80 + n); a byte below 0x80 is a pattern number.
enum {
    TrackCmd_End = 0,
    TrackCmd_LoopPoint,
    TrackCmd_Transpose,
    TrackCmd_Repeats,
    TrackCmd_PatternByte, // HH build, song start only: the next byte is the pattern number
};

// Opcodes the code scans look for (68000, as in the module).
enum {
    Op_BraW = 0x6000,
    Op_BsrW = 0x6100,
    Op_LeaA0 = 0x41F9,
    Op_LeaA1 = 0x43F9,
    Op_LeaA2 = 0x45F9,
    Op_LeaA3 = 0x47F9,
    Op_LeaA5 = 0x4BF9,
    Op_Rts = 0x4E75,
    Op_MovemToStack = 0x48E7,
    Op_Moveq0D0 = 0x7000,
    Op_MoveBA3PostD0 = 0x101B, // move.b (a3)+,d0
    Op_MoveWAbsD1 = 0x3239,
    Op_MoveWAbsD2 = 0x3439,
    Op_MoveWD7Abs = 0x33C7,
    Op_AslW3D0 = 0xE740,
    Op_AslW4D0 = 0xE940,
    Op_AsrW1D3 = 0xE243,
    Op_ExtLD0 = 0x48C0,
    Op_MoveBSpeedD1 = 0x122C, // move.b speed(a4),d1 (displacement in the next word)
    Op_TstWWait = 0x4A6C,     // tst.w wait(a4)
    Op_SubqB1D0 = 0x5300,
    Op_MoveBA2PostD0 = 0x101A,
    Op_BneB = 0x6600,
    Op_BcsB = 0x6500,
    Op_CmpWImmD0 = 0x0C40,
    Op_CmpWImmD0Short = 0xB07C,
    Op_MoveqD7Count2 = 0x7E02, // moveq #2,d7 / moveq #3,d7: the voice loop count (dbra: one more voice)
    Op_MoveqD7Count3 = 0x7E03,
    Op_AsrWImmD0 = 0xE040,    // asr.w #0,d0 (the shift count is added in bits 9-11)
    Op_AddaWIndexA0 = 0xD0F0, // adda.w (a0,d0.w),a0
    Op_AddaWIndexA2 = 0xD4F2, // adda.w (a2,d0.w),a2
    Op_AddaWIndexA3 = 0xD6F3, // adda.w (a3,d0.w),a3
};

// Types

// An instrument record of the driver's table as the file holds it: all fields big-endian, addresses absolute.
#define INSTRUMENT_RECORD_SIZE 16

typedef struct InstrumentRecord {
    int16_t base_note;
    uint32_t sample;
    uint16_t sample_words;
    uint32_t repeat;
    uint16_t repeat_words;
    uint16_t flag;
} InstrumentRecord;

// The 2-byte silence the setup routine points an empty repeat at.
static const int8_t s_silence[2] = { 0, 0 };

// JdModule access. The driver reads memory past its tables; out of range reads give 0.

static uint8_t rd8(const JdModule* m, int64_t off) {
    return (off >= 0 && (uint64_t)off < m->size) ? m->data[off] : 0;
}

static uint16_t rd16(const JdModule* m, int64_t off) {
    return (uint16_t)((rd8(m, off) << 8) | rd8(m, off + 1));
}

static int16_t rd16s(const JdModule* m, int64_t off) {
    return (int16_t)rd16(m, off);
}

static uint32_t rd32(const JdModule* m, int64_t off) {
    return ((uint32_t)rd16(m, off) << 16) | rd16(m, off + 2);
}

// The address in a longword of the module (an absolute address of the original link) as a file offset.

static uint32_t addr_at(const JdModule* m, int64_t off) {
    return rd32(m, off) - m->base;
}

static InstrumentRecord read_record(const JdModule* m, int64_t at) {
    return (InstrumentRecord) { .base_note = rd16s(m, at),
                                .sample = rd32(m, at + 2),
                                .sample_words = rd16(m, at + 6),
                                .repeat = rd32(m, at + 8),
                                .repeat_words = rd16(m, at + 12),
                                .flag = rd16(m, at + 14) };
}

static void write16(JdModule* m, int64_t at, uint16_t value) {
    m->data[at] = (uint8_t)(value >> 8);
    m->data[at + 1] = (uint8_t)value;
}

static void write32(JdModule* m, int64_t at, uint32_t value) {
    write16(m, at, (uint16_t)(value >> 16));
    write16(m, at + 2, (uint16_t)value);
}

static void write_record(JdModule* m, int64_t at, const InstrumentRecord* record) {
    write16(m, at, (uint16_t)record->base_note);
    write32(m, at + 2, record->sample);
    write16(m, at + 6, record->sample_words);
    write32(m, at + 8, record->repeat);
    write16(m, at + 12, record->repeat_words);
    write16(m, at + 14, record->flag);
}

// First even offset in [from, to) holding `word`, or -1.

static int64_t find_word(const JdModule* m, int64_t from, int64_t to, uint16_t word) {
    for (int64_t o = (from + 1) & ~(int64_t)1; o + 2 <= to; o += 2) {
        if (rd16(m, o) == word) {
            return o;
        }
    }
    return -1;
}

// A word expected at a byte offset from the start of a code pattern; only the bits in `mask` count.

typedef struct WordAt {
    int offset;
    uint16_t word;
    uint16_t mask;
} WordAt;

#define WORD_AT(offset, word) { (offset), (word), 0xFFFF }

// First even offset in [from, to) where all `count` words of `pattern` match, or -1.

static int64_t find_pattern(const JdModule* m, int64_t from, int64_t to, const WordAt* pattern, int count) {
    int span = 0;
    for (int i = 0; i < count; i++) {
        span = pattern[i].offset + 2 > span ? pattern[i].offset + 2 : span;
    }
    for (int64_t o = (from + 1) & ~(int64_t)1; o + span <= to; o += 2) {
        bool same = true;
        for (int i = 0; i < count && same; i++) {
            same = (rd16(m, o + pattern[i].offset) & pattern[i].mask) == (pattern[i].word & pattern[i].mask);
        }
        if (same) {
            return o;
        }
    }
    return -1;
}

// Target of the n-th bra.w of the jump table at the start of the file.

static int64_t bra_target(const JdModule* m, int n) {
    return 4 * n + 2 + rd16s(m, 4 * n + 2);
}

// A bra.w whose offset is positive and even.

static bool is_bra_forward(const JdModule* m, int64_t off) {
    const int16_t disp = rd16s(m, off + 2);
    return rd16(m, off) == Op_BraW && disp > 0 && (disp & 1) == 0;
}

// The group of bra.w at the start: 4, then optionally 3 more. Returns the number, 0 if the shape is wrong.

static int count_bra_table(const JdModule* m) {
    for (int i = 0; i < 4; i++) {
        if (!is_bra_forward(m, 4 * i)) {
            return 0;
        }
    }
    if (rd16(m, 16) != Op_BraW) {
        return 4;
    }
    for (int i = 4; i < 7; i++) {
        if (!is_bra_forward(m, 4 * i)) {
            return 0;
        }
    }
    return rd16(m, 28) == Op_BraW ? 0 : 7;
}

// The eagleplayer's Check2: the bra.w table, then the setup routine opens with [bsr.w] lea a0, lea a1.

static bool check_module(const JdModule* m) {
    if (m->size < 0x200) {
        return false;
    }
    const int table = count_bra_table(m);
    if (table == 0) {
        return false;
    }
    int64_t setup = bra_target(m, 0);
    if (setup < 0 || setup + 16 > (int64_t)m->size) {
        return false;
    }
    if (table == 4) {
        if (rd16(m, setup) != Op_BsrW) {
            return false;
        }
        setup += 4;
    }
    return rd16(m, setup) == Op_LeaA0 && rd16(m, setup + 6) == Op_LeaA1;
}

bool jd_probe(const uint8_t* data, size_t size) {
    JdModule m = { .data = (uint8_t*)data, .size = size };
    return check_module(&m);
}

// The code of the driver (from `code_start` to its last rts before the data) and the base address of the link. The
// eagleplayer derives the base from the period table: `lea table,a5` names its address, the table's first word is
// found in the file.

typedef struct Layout {
    bool two_groups;
    int64_t code_start;
    int64_t code_end;     // the last rts of the code
    int64_t period_hit;   // file offset of the period table's first word
    int64_t first_record; // the instrument table starts after the period table's last word
} Layout;

static bool locate_code(JdModule* m, Layout* layout) {
    layout->two_groups = rd16(m, 16) == Op_BraW;
    uint32_t marker = 0x5F0166; // high word marks the end of the period table, low word its first period
    int64_t scan;
    if (layout->two_groups) {
        layout->code_start = bra_target(m, 0);
        // The longer period table has 0x7E before its last entry, 0x77.
        const int64_t last = find_word(m, layout->code_start, (int64_t)m->size, 0x0077);
        if (last >= 2 && rd16(m, last - 2) == 0x007E) {
            marker = 0x7701BF;
        }
        scan = layout->code_start;
    } else {
        const int64_t movem = find_word(m, 12, (int64_t)m->size, Op_MovemToStack);
        if (movem < 0) {
            return false;
        }
        layout->code_start = movem;
        scan = movem + 2;
    }
    const int64_t lea = find_word(m, scan, (int64_t)m->size, Op_LeaA5);
    if (lea < 0) {
        return false;
    }
    const uint32_t table_address = rd32(m, lea + 2);
    const int64_t hit = find_word(m, lea + 6, (int64_t)m->size, (uint16_t)(marker & 0xFFFF));
    if (hit < 0) {
        return false;
    }
    m->base = table_address - (uint32_t)hit;
    layout->period_hit = hit;
    int64_t rts = hit;
    do {
        rts -= 2;
    } while (rts > layout->code_start && rd16(m, rts) != Op_Rts);
    if (rts <= layout->code_start) {
        return false;
    }
    layout->code_end = rts;
    // The period table ends with the high word of the marker; the instrument table follows it.
    const int64_t last = find_word(m, rts, (int64_t)m->size, (uint16_t)(marker >> 16));
    if (last < 0) {
        return false;
    }
    layout->first_record = last + 2;
    return true;
}

// True when the few instructions from `from` (the table lookup that follows a lea) index the table with
// `adda.w (an,d0.w),an` (opcode `indexed`, extension word 0) instead of adding the index first.

static bool indexed_adda_follows(const JdModule* m, int64_t from, uint16_t indexed) {
    for (int64_t o = from; o < from + 12; o += 2) {
        if (rd16(m, o) == indexed && rd16(m, o + 2) == 0) {
            return true;
        }
    }
    return false;
}

// Moves the absolute address at `at` from the link address to the load address.

static void relocate_long(JdModule* m, int64_t at) {
    write32(m, at, rd32(m, at) - m->base + LOAD_ADDRESS);
}

// The eagleplayer's relocation of the instrument records in front of the ones the setup routine fills: the sample
// address, and the repeat address with it, of every record that has a sample address.

static void relocate_records(JdModule* m, int64_t first_record, int64_t end) {
    for (int64_t at = first_record; at < end; at += INSTRUMENT_RECORD_SIZE) {
        InstrumentRecord record = read_record(m, at);
        if (record.sample == 0) {
            continue;
        }
        record.sample = record.sample - m->base + LOAD_ADDRESS;
        if (record.repeat != 0) {
            record.repeat = record.repeat - m->base + LOAD_ADDRESS;
        }
        write_record(m, at, &record);
    }
}

// The eagleplayer's relocation of the driver code: every instruction with an absolute long operand (not $dffxxx) gets
// the operand moved from the link address to the load address. The driver only sees this when it reads its own code.

static void relocate_code(JdModule* m, const Layout* layout) {
    static const uint16_t s_with_immediate[] = { 0x0279, 0x13FC, 0x33FC }; // opcode, immediate word, operand
    static const uint16_t s_plain[]
        = { 0x13C0, 0x2079, 0x23C8, 0x3039, 0x3239, 0x33C7, 0x3439, 0x41F9, 0x4279, 0x42B9, 0x43F9,
            0x45F9, 0x47F9, 0x49F9, 0x4A79, 0x4BF9, 0x5279, 0x8179, 0x8379, 0x8F79, 0xB039 };
    int64_t pos = layout->code_start;
    while (pos < layout->code_end) {
        const uint16_t op = rd16(m, pos);
        int64_t operand = -1;
        for (size_t i = 0; i < sizeof(s_with_immediate) / sizeof(s_with_immediate[0]); i++) {
            if (op == s_with_immediate[i]) {
                operand = pos + 4;
            }
        }
        for (size_t i = 0; i < sizeof(s_plain) / sizeof(s_plain[0]); i++) {
            if (op == s_plain[i]) {
                operand = pos + 2;
            }
        }
        if (operand < 0) {
            pos += 2;
            continue;
        }
        pos = operand;
        if (rd16(m, operand) == 0x00DF) {
            continue; // custom chip register
        }
        relocate_long(m, operand);
        pos += 4;
    }
}

// A command handler that reads a table: lea table,a0 / moveq #0,d0 / move.b (a3)+,d0, then the `more` words (offsets
// from the lea). Returns the offset of the lea, or -1.

static int64_t find_table_lookup(const JdModule* m, int64_t from, int64_t to, const WordAt* more, int more_count) {
    WordAt pattern[4] = { WORD_AT(0, Op_LeaA0), WORD_AT(6, Op_Moveq0D0), WORD_AT(8, Op_MoveBA3PostD0) };
    int count = 3;
    for (int i = 0; i < more_count; i++) {
        pattern[count++] = more[i];
    }
    return find_pattern(m, from, to, pattern, count);
}

// The first command handler after `from` that reads a table; its lea offset, or -1.

static int64_t find_next_table_lookup(const JdModule* m, int64_t from, int64_t to) {
    return find_table_lookup(m, from, to, nullptr, 0);
}

// The tables and flags of the driver, read from the operands of its code.

static bool locate_tables(const JdModule* m, const Layout* layout, JdVersion* ver, JdTables* tab) {
    const int64_t lo = layout->code_start;
    const int64_t hi = layout->code_end + 2;
    tab->init = (uint32_t)bra_target(m, layout->two_groups ? 4 : 2);
    tab->play = (uint32_t)bra_target(m, layout->two_groups ? 6 : 3);

    // Init: lea songs / asl.w #3,d0, voice loop count, lea patterns, subsong count.
    const int64_t asl = find_word(m, tab->init, tab->init + 0x100, Op_AslW3D0);
    if (asl < 6) {
        return false;
    }
    tab->songs = addr_at(m, asl - 4);
    const int64_t pattern_lea = find_word(m, asl, asl + 0x100, Op_LeaA2);
    if (pattern_lea < 0) {
        return false;
    }
    tab->patterns = addr_at(m, pattern_lea + 2);
    ver->voices = 0;
    for (int64_t o = asl; o < asl + 0x40 && ver->voices == 0; o += 2) {
        const uint16_t w = rd16(m, o);
        if (w == Op_MoveqD7Count2 || w == Op_MoveqD7Count3) { // the voice loop runs d7 + 1 times
            ver->voices = (w & 0xFF) + 1;
        }
    }
    tab->subsongs = 0;
    for (int64_t o = tab->init; o < tab->init + 0x40 && tab->subsongs == 0; o += 2) {
        const uint16_t op = rd16(m, o);
        // cmp.w #n,d0 / bcs.b
        if ((op == Op_CmpWImmD0 || op == Op_CmpWImmD0Short) && (rd16(m, o + 4) & 0xFF00) == Op_BcsB) {
            tab->subsongs = rd16(m, o + 2);
        }
    }
    if (ver->voices == 0 || tab->subsongs == 0) {
        return false;
    }

    // Play: the period table.
    const int64_t period_lea = find_word(m, tab->play, tab->play + 0x20, Op_LeaA5);
    if (period_lea < 0) {
        return false;
    }
    tab->period = addr_at(m, period_lea + 2);

    // Volume masks: move.w mask1,d1 / move.w mask2,d2
    const WordAt mask_code[] = { WORD_AT(0, Op_MoveWAbsD1), WORD_AT(6, Op_MoveWAbsD2) };
    const int64_t mask = find_pattern(m, lo, hi, mask_code, 2);
    if (mask < 0) {
        return false;
    }
    tab->volume_mask_music = rd16(m, addr_at(m, mask + 2));
    tab->volume_mask_sfx = rd16(m, addr_at(m, mask + 8));

    // Command handlers that read a table. The pitch sequences come first, the envelopes second.
    const int64_t pitch_lea = find_next_table_lookup(m, lo, hi);
    const int64_t envelope_lea = pitch_lea < 0 ? -1 : find_next_table_lookup(m, pitch_lea + 2, hi);
    if (envelope_lea < 0) {
        return false;
    }
    tab->pitch_seq = addr_at(m, pitch_lea + 2);
    tab->envelope = addr_at(m, envelope_lea + 2);
    ver->pitch_seq_relative = indexed_adda_follows(m, pitch_lea + 10, Op_AddaWIndexA0);
    ver->envelope_relative = indexed_adda_follows(m, envelope_lea + 10, Op_AddaWIndexA0);
    ver->patterns_start_relative = indexed_adda_follows(m, pattern_lea + 6, Op_AddaWIndexA2);
    const int64_t play_lea = find_word(m, tab->play, hi, Op_LeaA3); // lea patterns,a3
    ver->patterns_play_relative = play_lea >= 0 && indexed_adda_follows(m, play_lea + 6, Op_AddaWIndexA3);

    // JdInstrument command: the table lookup scaled by 16 (asl.w #4,d0)
    const WordAt scaled[] = { WORD_AT(10, Op_AslW4D0) };
    const int64_t select_lea = find_table_lookup(m, lo, hi, scaled, 1);
    if (select_lea < 0) {
        return false;
    }
    tab->instrument_select = addr_at(m, select_lea + 2);

    // Flags that tell the builds apart.
    const WordAt wait_guard[] = { WORD_AT(0, Op_TstWWait), WORD_AT(2, 0x0052) }; // tst.w wait(a4)
    ver->zero_wait_halts = find_pattern(m, lo, hi, wait_guard, 2) >= 0;
    ver->activity_overwrite = find_word(m, tab->play, tab->play + 0x80, Op_MoveWD7Abs) >= 0; // move.w d7,activity
    ver->volume_half = find_word(m, lo, hi, Op_AsrW1D3) >= 0;
    ver->vibrato_shift = 0;
    for (int shift = 1; shift <= 3; shift++) {
        // asr.w #n,d0 / ext.l d0 / move.b speed(a4),d1
        const WordAt shift_code[] = { WORD_AT(0, (uint16_t)(Op_AsrWImmD0 + (shift << 9))), WORD_AT(2, Op_ExtLD0),
                                      WORD_AT(4, Op_MoveBSpeedD1), WORD_AT(6, 0x0035) };
        if (find_pattern(m, lo, hi, shift_code, 4) >= 0) {
            ver->vibrato_shift = shift;
        }
    }
    if (ver->vibrato_shift == 0) {
        return false;
    }
    // Song start: subq.b #1,d0 / bne / move.b (a2)+,d0 after the repeat command (HH build only)
    const WordAt pattern_byte[] = { WORD_AT(0, Op_SubqB1D0), { 2, Op_BneB, 0xFF00 }, WORD_AT(4, Op_MoveBA2PostD0) };
    ver->track_pattern_byte = find_pattern(m, tab->init, tab->init + 0x100, pattern_byte, 3) >= 0;

    // Setup routine: [bsr.w] lea list,a0 / lea instruments,a1 / [lea extra,a2]
    int64_t s = bra_target(m, 0);
    const bool bsr_first = rd16(m, s) == Op_BsrW;
    if (bsr_first) {
        s += 4;
    }
    tab->setup_list = addr_at(m, s + 2);
    tab->instruments = addr_at(m, s + 8);
    const bool third = rd16(m, s + 12) == Op_LeaA2;
    tab->setup_extra = third ? addr_at(m, s + 14) : NO_ADDR;
    if (bsr_first) {
        ver->setup_kind = third ? JdSetupKind_PointerList : JdSetupKind_WordList;
    } else {
        ver->setup_kind = third ? JdSetupKind_FormStream : JdSetupKind_RelativeList;
    }
    return true;
}

// Setup routine: instrument table from the 8SVX forms

#define TAG(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

// Chunk sizes from this on cannot be a real chunk of a file this size.
#define MAX_CHUNK_SIZE 0x7FFFFFF0u

// The IFF walker keeps the data of the last VHDR and BODY chunk it saw; a form it cannot read leaves them alone.
typedef struct FormWalk {
    const JdModule* module;
    uint32_t vhdr;
    uint32_t body;
    bool error;
} FormWalk;

static void walk_form(FormWalk* w, uint32_t* pos, int32_t* remaining, int depth);

// Chunk handler of the walker. `pos` is just after the chunk's tag; it moves past the chunk.

static void walk_chunk(FormWalk* w, uint32_t tag, uint32_t* pos, int32_t* remaining, int depth) {
    if (tag == TAG('F', 'O', 'R', 'M')) {
        walk_form(w, pos, remaining, depth);
        return;
    }
    const uint32_t size = rd32(w->module, *pos);
    if (size >= MAX_CHUNK_SIZE) {
        w->error = true;
        *remaining = 0;
        return;
    }
    const uint32_t data = *pos + 4;
    if (tag == TAG('V', 'H', 'D', 'R')) {
        w->vhdr = data;
    } else if (tag == TAG('B', 'O', 'D', 'Y')) {
        w->body = data;
    }
    const uint32_t advance = ((size + 1) & ~1u) + 4;
    *pos += advance;
    *remaining -= (int32_t)advance;
}

// A FORM chunk: its type must be 8SVX, its chunks are walked.

static void walk_form(FormWalk* w, uint32_t* pos, int32_t* remaining, int depth) {
    const uint32_t size = rd32(w->module, *pos);
    if (size >= MAX_CHUNK_SIZE || depth >= MAX_FORM_DEPTH) {
        w->error = true;
        *remaining = 0;
        return;
    }
    const uint32_t advance = ((size + 1) & ~1u) + 4;
    uint32_t inner = *pos + 4;
    *pos += advance;
    *remaining -= (int32_t)advance;
    if (rd32(w->module, inner) != TAG('8', 'S', 'V', 'X')) {
        w->error = true;
        return;
    }
    inner += 4;
    int32_t left = (int32_t)size - 4;
    // Past the module only zero chunks follow, which change nothing: stop there (a truncated form would otherwise
    // walk up to 2 GB of them).
    while (left > 0 && inner < w->module->size) {
        const uint32_t tag = rd32(w->module, inner);
        inner += 4;
        left -= 4;
        walk_chunk(w, tag, &inner, &left, depth + 1);
    }
}

// The form at `form`, `length` bytes of it to walk. The instrument's sample and repeat come from the VHDR (one-shot
// and repeat sample counts) and the BODY the walk left behind.

static void walk_instrument(FormWalk* w, uint32_t form, int32_t length, JdInstrument* out) {
    const JdModule* m = w->module;
    w->error = false;
    uint32_t pos = form;
    int32_t left = length;
    while (left > 0 && !w->error) {
        const uint32_t tag = rd32(m, pos);
        pos += 4;
        left -= 4;
        if (tag != TAG('F', 'O', 'R', 'M')) {
            w->error = true; // the driver leaves the instrument with the previous form's chunks
            break;
        }
        walk_chunk(w, tag, &pos, &left, 0);
    }
    const uint32_t one_shot = rd32(m, w->vhdr) & ~1u;
    const uint32_t repeat = rd32(m, w->vhdr + 4) & ~1u;
    const uint32_t body = w->body;
    out->sample = (const int8_t*)(m->data + (body < m->size ? body : m->size));
    out->sample_words = (uint16_t)((one_shot + repeat) >> 1);
    if (repeat == 0) {
        out->repeat = s_silence;
        out->repeat_words = 1;
    } else {
        const uint32_t start = body + one_shot;
        out->repeat = (const int8_t*)(m->data + (start < m->size ? start : m->size));
        out->repeat_words = (uint16_t)(repeat >> 1);
    }
}

// A record of the instrument table as it is in the file (the default record, or one the setup did not fill). The
// addresses in it are absolute; `address_bias` is what to take off to get a file offset.

static JdInstrument instrument_from_file(const JdModule* m, uint32_t at, uint32_t address_bias) {
    const InstrumentRecord record = read_record(m, at);
    const uint32_t sample = record.sample - address_bias;
    const uint32_t repeat = record.repeat - address_bias;
    return (
        JdInstrument) { .base_note = record.base_note,
                        .untransposed = record.flag != 0,
                        .sample = record.sample != 0 && sample < m->size ? (const int8_t*)(m->data + sample) : nullptr,
                        .sample_words = record.sample_words,
                        .repeat = record.repeat != 0 && repeat < m->size ? (const int8_t*)(m->data + repeat) : nullptr,
                        .repeat_words = record.repeat_words };
}

// One entry of the setup list: where its form is, how long to walk, and the record's base note and flag.

typedef struct SetupEntry {
    uint32_t form;
    int32_t length;
    int16_t base_note;
    bool untransposed;
} SetupEntry;

// Reads the next entry of the list the setup routine walks. `list` and `extra` move on. False at the list's end.

static bool next_setup_entry(const JdPlayer* p, uint32_t* list, uint32_t* extra, int index, SetupEntry* entry) {
    const JdModule* m = &p->module;
    switch (p->version.setup_kind) {
        case JdSetupKind_RelativeList: {
            const int32_t offset = (int32_t)rd32(m, *list);
            if (offset == 0) {
                return false;
            }
            entry->base_note = rd16s(m, *list + 4);
            entry->untransposed = rd16(m, *list + 6) != 0;
            entry->form = (uint32_t)((int64_t)*list + offset);
            entry->length = (int32_t)(rd32(m, entry->form + 4) + 8u); // add.l: wraps
            *list += 8;
            return true;
        }
        case JdSetupKind_FormStream: {
            if (rd32(m, *list) == 0) {
                return false;
            }
            const InstrumentRecord record
                = read_record(m, p->tables.instruments + INSTRUMENT_RECORD_SIZE * (uint32_t)index);
            entry->base_note = record.base_note;
            entry->untransposed = record.flag != 0;
            entry->form = *extra;
            entry->length = (int32_t)(rd32(m, entry->form + 4) + 8u); // add.l: wraps
            *extra += (uint32_t)entry->length;
            *list += 4;
            return true;
        }
        case JdSetupKind_PointerList: {
            if (rd16(m, *list) == 0) {
                return false;
            }
            entry->base_note = rd16s(m, *list + 2);
            entry->untransposed = rd16(m, *list + 4) != 0;
            entry->form = addr_at(m, *extra) + 8;
            entry->length = (int32_t)(addr_at(m, *extra + 4) - entry->form);
            *list += 6;
            *extra += 4;
            return true;
        }
        case JdSetupKind_WordList: {
            const int16_t offset = rd16s(m, *list);
            if (offset == 0) {
                return false;
            }
            entry->base_note = rd16s(m, *list + 2);
            entry->untransposed = rd16(m, *list + 4) != 0;
            entry->form = (uint32_t)((int64_t)*list + offset);
            entry->length = (int32_t)(int16_t)rd32(m, entry->form + 4) + 8; // ext.l of a long keeps the low word
            *list += 6;
            return true;
        }
    }
    return false;
}

static bool build_instruments(JdPlayer* p) {
    const JdModule* m = &p->module;
    FormWalk walk = { .module = m };
    uint32_t list = p->tables.setup_list;
    uint32_t extra = p->tables.setup_extra;
    int capacity = 0;
    SetupEntry entry;
    while (next_setup_entry(p, &list, &extra, p->instrument_count, &entry)) {
        if (p->instrument_count == capacity) {
            capacity = capacity ? capacity * 2 : 32;
            JdInstrument* grown = realloc(p->instruments, (size_t)capacity * sizeof(JdInstrument));
            if (grown == nullptr) {
                return false;
            }
            p->instruments = grown;
        }
        JdInstrument* inst = &p->instruments[p->instrument_count++];
        walk_instrument(&walk, entry.form, entry.length, inst);
        inst->base_note = entry.base_note;
        inst->untransposed = entry.untransposed;
        if (p->instrument_count > 4096) {
            return false;
        }
    }
    return true;
}

// JdInstrument n as the driver finds it: the setup's records, or what the file holds in the table. The instrument
// command and the setup routine address the table separately; some builds leave records in front of the setup's.

static JdInstrument instrument_get(const JdPlayer* p, uint8_t n) {
    const uint32_t record = p->tables.instrument_select + INSTRUMENT_RECORD_SIZE * n;
    if (record >= p->tables.instruments) {
        const uint32_t offset = record - p->tables.instruments;
        if (offset % INSTRUMENT_RECORD_SIZE == 0 && offset / INSTRUMENT_RECORD_SIZE < (uint32_t)p->instrument_count) {
            return p->instruments[offset / INSTRUMENT_RECORD_SIZE];
        }
    }
    // Records in front of the setup's were relocated at load; the ones after it hold the link addresses
    const uint32_t bias = record < p->tables.instruments ? LOAD_ADDRESS : p->module.base;
    return instrument_from_file(&p->module, record, bias);
}

// The period for a note byte (+ the offsets the caller added): table entry (note - base note) * 2 from the period
// table, with the driver's 16-bit arithmetic.

static uint16_t period_for(const JdPlayer* p, const JdChannel* c, uint8_t note) {
    const int16_t index = (int16_t)((int16_t)(int8_t)note - c->instrument.base_note);
    const int16_t offset = (int16_t)(index * 2);
    return rd16(&p->module, (int64_t)p->tables.period + offset);
}

// divs.w: 32-bit dividend by 16-bit divisor. The result is the quotient's low word; on overflow the dividend's.

static int16_t divs_w(int32_t dividend, int16_t divisor) {
    if (divisor == 0) {
        return 0; // the 68000 traps; no song divides by zero
    }
    const int32_t quotient = dividend / divisor;
    if (quotient < -32768 || quotient > 32767) {
        return (int16_t)dividend;
    }
    return (int16_t)quotient;
}

// The data of entry n of a table of words that hold the offset of their data, from the entry itself or (in some builds)
// from the start of the table.

static uint32_t table_data(const JdModule* m, uint32_t table, uint8_t n, bool relative_to_table) {
    const uint32_t entry = table + 2u * n;
    return (uint32_t)((int64_t)(relative_to_table ? table : entry) + rd16s(m, entry));
}

// Starts the pattern `number` of the track list: remembers where the track list goes on.

static void start_pattern(const JdPlayer* p, JdChannel* c, uint32_t track_pos, uint8_t number, bool at_song_start) {
    c->track_pos = track_pos;
    const bool relative = at_song_start ? p->version.patterns_start_relative : p->version.patterns_play_relative;
    c->pattern_pos = table_data(&p->module, p->tables.patterns, number, relative);
}

// The eagleplayer patches the driver's track-list end (not the one at song start) to count the voices that got there:
// when every voice of the subsong has, the song has played through (it looped, or all voices stopped) and the count
// starts again.

static void track_end_reached(JdPlayer* p, int voice) {
    p->voices_to_end &= (uint8_t)~(1u << voice);
    if (p->voices_to_end == 0) {
        p->looped = true;
        p->voices_to_end = p->used_voices;
    }
}

// Reads the track list from `pos` up to the next pattern number and starts that pattern. False when the list ends
// (end command without a loop point). `at_song_start` is the init routine's copy of the loop, which has the
// pattern-byte command of the HH build and does not count the end.

static bool track_read(JdPlayer* p, JdChannel* c, int voice, uint32_t pos, bool at_song_start) {
    for (int guard = 0; guard < 1 << 20; guard++) {
        const uint8_t byte = rd8(&p->module, pos++);
        if (byte < 0x80) {
            start_pattern(p, c, pos, byte, at_song_start);
            return true;
        }
        switch (byte - 0x80) {
            case TrackCmd_End:
                if (!at_song_start) {
                    track_end_reached(p, voice);
                }
                if (c->track_loop == NO_ADDR) {
                    return false;
                }
                pos = c->track_loop;
                break;
            case TrackCmd_LoopPoint:
                c->track_loop = pos;
                break;
            case TrackCmd_Transpose:
                c->transpose = (int8_t)rd8(&p->module, pos++);
                break;
            case TrackCmd_Repeats:
                c->pattern_repeats = rd8(&p->module, pos++);
                break;
            case TrackCmd_PatternByte:
                if (at_song_start && p->version.track_pattern_byte) {
                    start_pattern(p, c, pos + 1, rd8(&p->module, pos), true);
                    return true;
                }
                break;
            default:
                break;
        }
    }
    return false;
}

// The end of a pattern (command 0x80): the pattern loops, repeats, or the track list goes on. False when the channel
// ends.

static bool pattern_end(JdPlayer* p, JdChannel* c, int voice) {
    if (c->pattern_loop != NO_ADDR) {
        c->pattern_pos = c->pattern_loop;
        return true;
    }
    const uint8_t number = rd8(&p->module, (int64_t)c->track_pos - 1);
    c->pattern_repeats--;
    if (c->pattern_repeats != 0) {
        start_pattern(p, c, c->track_pos, number, false);
        return true;
    }
    c->pattern_repeats = 1;
    c->transpose = 0;
    return track_read(p, c, voice, c->track_pos, false);
}

// Command 0x90: loads instrument `n` into the channel.

static void select_instrument(const JdPlayer* p, JdChannel* c, uint8_t n) {
    c->instrument = instrument_get(p, n);
    c->shown_instrument = n;
    c->flags &= (uint16_t)~Flag_NoTranspose;
    if (c->instrument.untransposed) {
        c->flags |= Flag_NoTranspose;
    }
}

// What a pattern command asks the reader to do next.

typedef enum CommandResult {
    CommandResult_Next = 0, // read the next stream byte
    CommandResult_Wait,     // the command set the wait: finish the note
    CommandResult_End,      // the channel stopped
} CommandResult;

// Command byte `cmd` (0x80 + n) of the pattern stream; `pos` is the stream position after the command.

static CommandResult run_command(JdPlayer* p, JdChannel* c, int voice, int cmd, uint32_t* pos) {
    const JdModule* m = &p->module;
    switch (cmd) {
        case Cmd_EndPattern:
            if (!pattern_end(p, c, voice)) {
                return CommandResult_End;
            }
            *pos = c->pattern_pos;
            return CommandResult_Next;
        case Cmd_PatternLoop:
            c->pattern_loop = *pos;
            return CommandResult_Next;
        case Cmd_FixedLenOn:
            c->flags |= Flag_FixedLen;
            c->fixed_length = rd8(m, (*pos)++);
            return CommandResult_Next;
        case Cmd_FixedLenOff:
            c->flags &= (uint16_t)~Flag_FixedLen;
            return CommandResult_Next;
        case Cmd_LongWait:
            c->wait = (uint16_t)(c->wait + 0x100);
            return CommandResult_Next;
        case Cmd_Rest:
            c->flags &= (uint16_t)~Flag_Envelope;
            c->flags |= Flag_Rest;
            c->volume = 0;
            return CommandResult_Wait;
        case Cmd_Slide:
            c->flags |= Flag_Slide;
            c->slide_offset = rd8(m, (*pos)++);
            c->slide_ticks = rd8(m, (*pos)++);
            return CommandResult_Next;
        case Cmd_ArpeggioOn:
            c->flags = (uint16_t)((c->flags & ~Flag_PitchEffects) | Flag_Arpeggio);
            c->arpeggio_offset = rd8(m, (*pos)++);
            c->arpeggio_ticks = rd8(m, (*pos)++);
            return CommandResult_Next;
        case Cmd_ArpeggioOff:
            c->flags &= (uint16_t)~Flag_Arpeggio;
            return CommandResult_Next;
        case Cmd_VibratoOn:
            c->flags = (uint16_t)((c->flags & ~Flag_PitchEffects) | Flag_Vibrato);
            c->vibrato_delay = rd8(m, (*pos)++);
            c->vibrato_offset = rd8(m, (*pos)++);
            c->vibrato_speed = rd8(m, (*pos)++);
            return CommandResult_Next;
        case Cmd_VibratoOff:
            c->flags &= (uint16_t)~Flag_Vibrato;
            return CommandResult_Next;
        case Cmd_PitchSeqOn: {
            c->flags = (uint16_t)((c->flags & ~Flag_PitchEffects) | Flag_PitchSeq);
            const uint32_t data = table_data(m, p->tables.pitch_seq, rd8(m, (*pos)++), p->version.pitch_seq_relative);
            c->pitch_seq_wait = rd8(m, data);
            c->pitch_seq_repeats = rd8(m, data + 1);
            c->pitch_seq_start = data + 2;
            return CommandResult_Next;
        }
        case Cmd_PitchSeqOff:
            c->flags &= (uint16_t)~Flag_PitchSeq;
            return CommandResult_Next;
        case Cmd_Envelope:
            c->envelope_start = table_data(m, p->tables.envelope, rd8(m, (*pos)++), p->version.envelope_relative);
            return CommandResult_Next;
        case Cmd_Instrument:
            select_instrument(p, c, rd8(m, (*pos)++));
            return CommandResult_Next;
        default:
            return CommandResult_Next; // the LED filter commands of some builds, and bytes the table leaves empty
    }
}

// A note byte: sets the period and the effects, and restarts the channel's DMA at the end of the tick.

static void start_note(JdPlayer* p, JdChannel* c, uint8_t note_byte) {
    uint8_t note = note_byte;
    if (!(c->flags & Flag_NoTranspose)) {
        note = (uint8_t)(note + (uint8_t)c->transpose);
    }
    c->note = note;
    if (c->flags & Flag_Arpeggio) {
        note = (uint8_t)(note + c->arpeggio_offset);
        c->arpeggio_countdown = c->arpeggio_ticks;
    }
    c->played_note = note;
    c->period = period_for(p, c, note);
    c->shown_note = nuade_period_to_note(c->period);
    if (c->flags & Flag_Vibrato) {
        const uint16_t target = period_for(p, c, (uint8_t)(c->played_note + c->vibrato_offset));
        const int16_t difference = (int16_t)((int16_t)(target - c->period) >> p->version.vibrato_shift);
        const int16_t speed = (int16_t)(int8_t)c->vibrato_speed;
        c->vibrato_step = divs_w(difference, speed);
        c->vibrato_countdown = c->vibrato_speed;
        c->vibrato_half_period = (uint8_t)(c->vibrato_speed + c->vibrato_speed);
        c->vibrato_delay_countdown = c->vibrato_delay;
    }
    if (c->flags & Flag_Slide) {
        const uint16_t from = period_for(p, c, (uint8_t)(c->played_note + c->slide_offset));
        const int16_t difference = (int16_t)(from - c->period);
        const int16_t ticks = (int16_t)c->slide_ticks;
        c->slide_step = divs_w(difference, ticks);
        const int16_t negated = (int16_t)-c->slide_step;
        c->period = (uint16_t)(c->period - (uint16_t)((int32_t)negated * ticks));
    }
    if (c->flags & Flag_PitchSeq) {
        c->pitch_seq_countdown = 1;
        c->pitch_seq_pos = c->pitch_seq_start;
        c->pitch_seq_repeat_countdown = c->pitch_seq_repeats;
    }
    c->flags |= Flag_Envelope;
    c->envelope_pos = c->envelope_start;
    c->envelope_wait = 1;
    c->volume = 0;
    p->dma_restart |= c->dma_bit;
}

// The wait that follows a note or rest: the fixed length or the next stream byte.

static void set_wait(const JdPlayer* p, JdChannel* c, uint32_t* pos) {
    uint8_t length = c->fixed_length;
    if (!(c->flags & Flag_FixedLen)) {
        length = rd8(&p->module, (*pos)++);
    }
    c->wait = (uint16_t)(c->wait + length);
    c->pattern_pos = *pos;
}

// Reads the pattern stream until a note or the rest command sets the next wait.

static bool read_pattern(JdPlayer* p, JdChannel* c, int voice) {
    uint32_t pos = c->pattern_pos;
    c->flags &= (uint16_t)~Flag_Rest;
    for (int guard = 0; guard < 1 << 20; guard++) {
        const uint8_t byte = rd8(&p->module, pos++);
        if (byte < 0x80) {
            start_note(p, c, byte);
            set_wait(p, c, &pos);
            return true;
        }
        c->flags &= (uint16_t)~Flag_Slide;
        if (byte >= 0xA0) {
            continue;
        }
        const CommandResult result = run_command(p, c, voice, byte - 0x80, &pos);
        if (result == CommandResult_End) {
            return false;
        }
        if (result == CommandResult_Wait) {
            set_wait(p, c, &pos);
            return true;
        }
    }
    return false;
}

// What changes the period in the ticks between notes, and the volume envelope.

static void effects_tick(const JdPlayer* p, JdChannel* c) {
    const JdModule* m = &p->module;
    if (c->flags & Flag_Rest) {
        return;
    }
    uint16_t period = c->period;
    if (c->flags & Flag_Slide) {
        c->slide_ticks--;
        if (c->slide_ticks == 0) {
            c->flags &= (uint16_t)~Flag_Slide;
        }
        period = (uint16_t)(period - (uint16_t)c->slide_step);
    } else if (c->flags & Flag_Arpeggio) {
        if (c->arpeggio_countdown-- == 0) {
            c->played_note = c->note;
            period = period_for(p, c, c->note);
        }
    } else if (c->flags & Flag_PitchSeq) {
        c->pitch_seq_countdown--;
        if (c->pitch_seq_countdown == 0) {
            uint32_t pos = c->pitch_seq_pos;
            const uint8_t offset = rd8(m, pos++);
            c->pitch_seq_repeat_countdown--;
            if (c->pitch_seq_repeat_countdown == 0) {
                pos = c->pitch_seq_start;
                c->pitch_seq_repeat_countdown = c->pitch_seq_repeats;
            }
            c->pitch_seq_pos = pos;
            c->pitch_seq_countdown = c->pitch_seq_wait;
            period = period_for(p, c, (uint8_t)(offset + c->played_note));
        }
    } else if (c->flags & Flag_Vibrato) {
        if (c->vibrato_delay_countdown > 0) {
            c->vibrato_delay_countdown--;
        } else {
            c->vibrato_countdown--;
            if (c->vibrato_countdown == 0) {
                c->vibrato_step = (int16_t)-c->vibrato_step;
                c->vibrato_countdown = c->vibrato_half_period;
            }
            period = (uint16_t)(period + (uint16_t)c->vibrato_step);
        }
    }
    c->period = period;

    if (!(c->flags & Flag_Envelope)) {
        return;
    }
    c->envelope_wait--;
    if (c->envelope_wait == 0) {
        uint32_t pos = c->envelope_pos;
        const uint8_t byte = rd8(m, pos++);
        if (byte == 0) {
            // End marker: the next byte decides whether the envelope holds until the note's wait is over
            const uint8_t next = rd8(m, pos);
            uint8_t hold = next;
            if (next != 0) {
                if (next & 0x80) {
                    hold = (uint8_t)-next;
                }
                const int16_t left = (int16_t)(hold - c->wait);
                if (left < 0) {
                    c->envelope_wait = (uint16_t)-left;
                    c->envelope_every = 0;
                    c->envelope_countdown = 0;
                    c->envelope_step = 0;
                    c->envelope_pos = pos;
                    return;
                }
            }
            c->flags &= (uint16_t)~Flag_Envelope;
            return;
        }
        if (byte & 0x80) {
            // Slow ramp: duration, then the rate (its sign gives the direction)
            c->envelope_wait = (uint8_t)-byte;
            c->envelope_step = 1;
            uint8_t rate = rd8(m, pos++);
            if (rate & 0x80) {
                rate = (uint8_t)-rate;
                c->envelope_step = -1;
            }
            c->envelope_every = rate;
            c->envelope_countdown = 1;
            c->envelope_pos = pos;
        } else {
            c->envelope_wait = byte;
            c->envelope_every = 1;
            c->envelope_countdown = 1;
            c->envelope_step = (int8_t)rd8(m, pos++);
            c->envelope_pos = pos;
        }
    }
    c->envelope_countdown--;
    if (c->envelope_countdown == 0) {
        c->envelope_countdown = c->envelope_every;
        c->volume = (uint16_t)(c->volume + (uint16_t)(int16_t)c->envelope_step);
    }
}

// One channel's tick. Returns the new flag word (0 when the channel ended).

static uint16_t channel_tick(JdPlayer* p, JdChannel* c, int voice) {
    bool reads_pattern;
    if (p->version.zero_wait_halts) {
        reads_pattern = c->wait != 0 && --c->wait == 0;
    } else {
        c->wait--;
        reads_pattern = c->wait == 0;
    }
    if (reads_pattern && !read_pattern(p, c, voice)) {
        c->period = 1;
        c->volume = 0;
        return 0;
    }
    effects_tick(p, c);
    return c->flags;
}

// The end of the tick: restarts the DMA of the channels that started a note, then writes volume, period and the
// sample or repeat registers of every channel.

static void write_hardware(JdPlayer* p) {
    const uint16_t restart = p->dma_restart;
    p->dma_restart = 0;
    // The driver also parks the restarting channels at period 1 and volume 0 while their DMA is off and waits for
    // Paula to take it; nothing of that is audible per tick, so only the DMA stop is modelled.
    for (int i = 0; i < p->version.voices; i++) {
        if (restart & (1u << i)) {
            nuade_paula_channel_dma_off(&p->channels[i].paula);
        }
    }
    for (int i = 0; i < p->version.voices; i++) {
        JdChannel* c = &p->channels[i];
        const uint16_t mask = (c->flags & Flag_Sfx) ? p->tables.volume_mask_sfx : p->tables.volume_mask_music;
        int16_t volume = (int16_t)(mask & c->volume);
        if (p->version.volume_half) {
            volume = (int16_t)(volume >> 1);
        }
        c->paula.volume = (uint8_t)volume;
        c->paula.period = c->period;
        if (restart & (1u << i)) {
            c->paula.lc = c->instrument.sample;
            c->paula.len_words = c->instrument.sample_words;
        } else {
            c->paula.lc = c->instrument.repeat;
            c->paula.len_words = c->instrument.repeat_words;
        }
    }
    for (int i = 0; i < p->version.voices; i++) {
        if (restart & (1u << i)) {
            nuade_paula_channel_dma_on(&p->channels[i].paula);
        }
    }
}

// The play routine: one tick.

static void driver_tick(JdPlayer* p) {
    if (p->activity != 0) {
        for (int i = 0; i < p->version.voices; i++) {
            JdChannel* c = &p->channels[i];
            if (c->flags == 0) {
                continue;
            }
            const uint16_t flags = channel_tick(p, c, i);
            c->flags = flags;
            if (p->version.activity_overwrite && i == 0) {
                p->activity = flags;
            } else {
                p->activity |= flags;
            }
        }
        p->activity &= Flag_Kind;
    }
    write_hardware(p);
}

// The stop routine the setup runs: every channel idle, period 1, volume 0.

static void stop_all(JdPlayer* p) {
    for (int i = 0; i < JD_MAX_VOICES; i++) {
        JdChannel* c = &p->channels[i];
        c->flags = 0;
        c->period = 1;
        c->volume = 0;
    }
    p->activity = 0;
}

// The init routine: starts the music of subsong `number` (1-based).

static void song_start(JdPlayer* p, int number) {
    const int index = number - 1;
    if (index < 0 || index >= p->tables.subsongs) {
        stop_all(p);
        return;
    }
    const uint32_t entry = p->tables.songs + 8u * (uint32_t)index;
    p->used_voices = 0;
    for (int i = 0; i < JD_MAX_VOICES; i++) {
        if (rd16(&p->module, entry + 2u * (uint32_t)i) != 0) {
            p->used_voices |= (uint8_t)(1u << i);
        }
    }
    p->voices_to_end = p->used_voices;
    for (int i = 0; i < p->version.voices; i++) {
        const uint32_t word = entry + 2u * (uint32_t)i;
        const int16_t offset = rd16s(&p->module, word);
        if (offset == 0) {
            continue;
        }
        JdChannel* c = &p->channels[i];
        c->volume = 0;
        c->track_loop = NO_ADDR;
        c->pattern_loop = NO_ADDR;
        c->transpose = 0;
        c->pattern_repeats = 1;
        c->flags = Flag_Music;
        if (track_read(p, c, i, (uint32_t)((int64_t)word + offset), true)) {
            c->wait = 1;
        } else {
            c->flags = 0;
        }
    }
    p->activity |= Flag_Music;
}

// Public API

JdPlayer* jd_create(const uint8_t* data, size_t size) {
    JdPlayer* p = calloc(1, sizeof(JdPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->module.data = nuade_paula_padded_copy(data, size);
    if (p->module.data == nullptr) {
        free(p);
        return nullptr;
    }
    p->module.size = size;
    Layout layout;
    if (!check_module(&p->module) || !locate_code(&p->module, &layout)
        || !locate_tables(&p->module, &layout, &p->version, &p->tables) || !build_instruments(p)) {
        jd_destroy(p);
        return nullptr;
    }
    relocate_code(&p->module, &layout);
    relocate_records(&p->module, layout.first_record, p->tables.instruments);
    return p;
}

void jd_destroy(JdPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->instruments);
    free(player->module.data);
    free(player);
}

int jd_subsong_count(const JdPlayer* player) {
    return player->tables.subsongs;
}

int jd_voice_count(const JdPlayer* player) {
    return player->version.voices;
}

bool jd_start(JdPlayer* player, int subsong, NuadeVoices* voices) {
    if (voices == nullptr || subsong < 0 || subsong >= player->tables.subsongs) {
        return false;
    }
    memset(player->channels, 0, sizeof(player->channels));
    for (int i = 0; i < JD_MAX_VOICES; i++) {
        JdChannel* c = &player->channels[i];
        c->track_loop = NO_ADDR;
        c->pattern_loop = NO_ADDR;
        c->envelope_start = NO_ADDR;
        c->dma_bit = (uint16_t)(1u << i);
        c->shown_note = -1;
        c->shown_instrument = -1;
        nuade_voice_stop(voices, (uint32_t)i);
        nuade_voice_set_volume(voices, (uint32_t)i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, (uint32_t)i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    player->dma_restart = 0;
    stop_all(player); // the setup routine's stop: period 1 on every channel
    song_start(player, subsong + 1);
    return true;
}

JdTickResult jd_tick(JdPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    driver_tick(player);
    bool any = false;
    for (int i = 0; i < player->version.voices; i++) {
        nuade_paula_channel_sync(&player->channels[i].paula, (uint32_t)i, voices);
        any = any || player->channels[i].flags != 0;
    }
    return (JdTickResult) { .looped = player->looped, .ended = !any };
}

JdVoiceState jd_voice_state(const JdPlayer* player, int voice) {
    if (voice < 0 || voice >= player->version.voices) {
        return (JdVoiceState) { .note = -1, .instrument = -1 };
    }
    const JdChannel* c = &player->channels[voice];
    return (JdVoiceState) { .note = c->shown_note, .instrument = c->shown_instrument };
}
