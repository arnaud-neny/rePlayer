// SPDX-License-Identifier: MIT
// Darius Zendeh player: finds the replayer's tables and the songs the init code sets up in a module's code.
//
// The code is generated from one source, so the routines of a version always have the same instruction sequence and
// only the pc-relative operands differ. The locator walks the routines as that sequence: `expect` names the opcode
// words that must be there, `take_pc` and `take_branch` read the operands. A sequence that does not match means the
// module is not a variant this player knows.

#include "dz_internal.h"

#define SEARCH_LIMIT 0x800 // patterns are searched at even offsets below this
#define MAX_PATTERN_WORDS 24

// Opcode word patterns: words of four hex digits separated by spaces, an "x" digit matches any value
// ("43fa xxxx", "66xx").

typedef struct Pattern {
    uint16_t words[MAX_PATTERN_WORDS];
    uint16_t masks[MAX_PATTERN_WORDS];
    int count;
} Pattern;

static unsigned hex_value(char digit) {
    if (digit >= '0' && digit <= '9') {
        return (unsigned)(digit - '0');
    }
    return (unsigned)((digit | 0x20) - 'a' + 10);
}

static Pattern parse_pattern(const char* text) {
    Pattern pattern = { 0 };
    while (*text != '\0' && pattern.count < MAX_PATTERN_WORDS) {
        if (*text == ' ') {
            text++;
            continue;
        }
        uint16_t word = 0;
        uint16_t mask = 0;
        for (int digit = 0; digit < 4 && *text != '\0'; digit++, text++) {
            word = (uint16_t)(word << 4);
            mask = (uint16_t)(mask << 4);
            if (*text != 'x') {
                word |= (uint16_t)hex_value(*text);
                mask |= 0xf;
            }
        }
        pattern.words[pattern.count] = word;
        pattern.masks[pattern.count] = mask;
        pattern.count++;
    }
    return pattern;
}

static bool pattern_matches(const DzModule* m, uint32_t at, const Pattern* pattern) {
    if (at + 2u * (uint32_t)pattern->count > m->size) {
        return false;
    }
    for (int i = 0; i < pattern->count; i++) {
        if ((dzm_u16(m, at + 2u * (uint32_t)i) & pattern->masks[i]) != pattern->words[i]) {
            return false;
        }
    }
    return true;
}

static bool match_at(const DzModule* m, uint32_t at, const char* text) {
    const Pattern pattern = parse_pattern(text);
    return pattern_matches(m, at, &pattern);
}

// The first even offset in [from, to] where the pattern matches
static bool find_pattern(const DzModule* m, uint32_t from, uint32_t to, const char* text, uint32_t* found) {
    const Pattern pattern = parse_pattern(text);
    for (uint32_t at = from; at <= to; at += 2) {
        if (pattern_matches(m, at, &pattern)) {
            *found = at;
            return true;
        }
    }
    return false;
}

// Instruction reader

typedef struct Reader {
    const DzModule* m;
    uint32_t pos;
    bool ok;
} Reader;

static uint16_t peek(const Reader* r) {
    return dzm_u16(r->m, r->pos);
}

// The next word if it is `word`; consumed
static bool accept(Reader* r, uint16_t word) {
    if (!r->ok || peek(r) != word) {
        return false;
    }
    r->pos += 2;
    return true;
}

// The next words are the words of the pattern `text`
static void expect(Reader* r, const char* text) {
    const Pattern pattern = parse_pattern(text);
    r->ok = r->ok && pattern_matches(r->m, r->pos, &pattern);
    r->pos += 2u * (uint32_t)pattern.count;
}

// An immediate word
static uint16_t take_word(Reader* r) {
    const uint16_t word = peek(r);
    r->pos += 2;
    return word;
}

// A d16(pc) operand: the address it names
static uint32_t take_target(Reader* r) {
    const uint32_t extension = r->pos;
    return extension + (uint32_t)(int32_t)(int16_t)take_word(r);
}

// An instruction `opcode d16(pc)`: the address the operand names
static uint32_t take_pc(Reader* r, uint16_t opcode) {
    if (!accept(r, opcode)) {
        r->ok = false;
        return 0;
    }
    return take_target(r);
}

// An instruction `opcode d16(pc)` that must name `address`
static void expect_pc(Reader* r, uint16_t opcode, uint32_t address) {
    if (take_pc(r, opcode) != address) {
        r->ok = false;
    }
}

// A branch whose opcode high byte is `condition` (0x60 bra, 0x66 bne, 0x67 beq): its target. The assembler used the
// byte, word and long displacement forms.
static uint32_t take_branch(Reader* r, uint8_t condition) {
    if (!r->ok || dzm_u8(r->m, r->pos) != condition) {
        r->ok = false;
        return 0;
    }
    const uint32_t at = r->pos;
    const uint8_t byte_displacement = dzm_u8(r->m, at + 1);
    int32_t displacement = (int8_t)byte_displacement;
    r->pos += 2;
    if (byte_displacement == 0) {
        displacement = (int16_t)dzm_u16(r->m, at + 2);
        r->pos += 2;
    } else if (byte_displacement == 0xff) {
        displacement = (int32_t)dzm_u32(r->m, at + 2);
        r->pos += 4;
    }
    return at + 2 + (uint32_t)displacement;
}

// Check2

static bool check_version(const DzModule* m, DzVersion* version) {
    if (match_at(m, 0, "48e7 78f0 41fa xxxx 4cd8 0600 0c00 00ff")) {
        *version = DzVersion_S;
        return true;
    }
    // movem.l; the shutdown sequence (moveq #0,d0; move.w #$f,$dff096; lea) within the first 0x121 words
    uint32_t shutdown = 0;
    if (!match_at(m, 0, "48e7 00f0 xxxx xxxx 4cd8 0600")
        || !find_pattern(m, 0, 0x120 * 2, "7000 33fc 000f 00df f096 41fa", &shutdown)) {
        return false;
    }
    if (match_at(m, 12, "4a44")) { // tst.w d4
        *version = DzVersion_1;
    } else if (match_at(m, 12, "4a00")) { // tst.b d0
        *version = DzVersion_2;
    } else if (match_at(m, 12, "0c00")) { // cmpi.b #xx,d0
        *version = DzVersion_3;
    } else {
        return false;
    }
    return true;
}

// What the code leaves to the init code: the module's own variable words (offsets, 0 for what the version keeps as a
// constant in the code) and the constants.

typedef struct Variables {
    uint32_t start_list;
    uint32_t loop_list;
    uint32_t pattern_base;
    uint32_t record_size;
    uint32_t one_shot;
} Variables;

typedef struct CodeFacts {
    Variables variables;
    uint32_t constant_pattern_base;
    uint32_t constant_record_size;
    uint32_t constant_start_list; // version S
} CodeFacts;

// The pattern base the row routine loads into a2: `lea` of a constant, or `movea.l` of a variable
typedef struct PatternBase {
    bool variable;
    uint32_t address;
} PatternBase;

// What the row routine takes from the init code
typedef struct RowAdvance {
    uint32_t step_bytes; // the variable with the bytes one list step takes
    PatternBase pattern_base;
    uint32_t one_shot;
} RowAdvance;

static PatternBase take_pattern_base(Reader* r) {
    if (peek(r) == 0x45fa) {
        return (PatternBase) { .variable = false, .address = take_pc(r, 0x45fa) };
    }
    return (PatternBase) { .variable = true, .address = take_pc(r, 0x247a) };
}

static void expect_pattern_base(Reader* r, PatternBase pattern_base) {
    const PatternBase found = take_pattern_base(r);
    if (found.variable != pattern_base.variable || found.address != pattern_base.address) {
        r->ok = false;
    }
}

// The tick that alternates with the row tick: the module's phase word, and the word with the channels whose DMA it
// stops. bsr clear_registers; lea phase,a0; cmpi.w #0,(a0); bne row; move.w #1,(a0); move.w stop_mask(pc),d0;
// move.w d0,DMACON; rts
static bool locate_stop_tick(const DzModule* m, DzLayout* layout, uint32_t* stop_mask) {
    uint32_t at = 0;
    if (!find_pattern(m, 0, SEARCH_LIMIT - 2, "41fa xxxx 0c50 0000 66xx 30bc 0001 303a xxxx 33c0 00df f096 4e75",
                      &at)) {
        return false;
    }
    Reader r = { m, at, true };
    layout->phase = take_pc(&r, 0x41fa);
    expect(&r, "0c50 0000");
    take_branch(&r, 0x66);
    expect(&r, "30bc 0001");
    *stop_mask = take_pc(&r, 0x303a);
    expect(&r, "33c0 00df f096 4e75");
    return r.ok;
}

// The routine that moves on to the next row:
// clr.w (a0); addq.w #4,a2; suba.w step(pc),a1; cmpi.b #$ff,(a2); bne row; lea/movea pattern_base,a2;
// adda.w step(pc),a1; cmpi.b #$ff,(a1); bne row; lea end_flag,a0; move.w #1,(a0); lea one_shot,a0;
// cmpi.w #1,(a0); bne loop; rts
static RowAdvance read_row_advance(Reader* r) {
    RowAdvance a = { 0 };
    expect(r, "4250 584a");
    a.step_bytes = take_pc(r, 0x92fa);
    expect(r, "0c12 00ff");
    take_branch(r, 0x66);
    a.pattern_base = take_pattern_base(r);
    expect_pc(r, 0xd2fa, a.step_bytes);
    expect(r, "0c11 00ff");
    take_branch(r, 0x66);
    take_pc(r, 0x41fa);
    expect(r, "30bc 0001");
    a.one_shot = take_pc(r, 0x41fa);
    expect(r, "0c50 0001");
    take_branch(r, 0x66);
    expect(r, "4e75");
    return a;
}

// The entry of the row routine: the list a song starts from and the pattern base.
// lea start,a1 [bra.b; lea loop,a1]; movea.l (a1),a1; lea/movea pattern_base,a2; move.w voices(pc),d4;
// lea stop_mask,a0; clr.w (a0); move.w #1,-2(a0); lea AUD0,a3
static bool read_row_entry(Reader* r, const RowAdvance* advance, uint32_t stop_mask, CodeFacts* facts) {
    facts->variables.start_list = take_pc(r, 0x43fa);
    facts->variables.loop_list = facts->variables.start_list;
    if (accept(r, 0x6004)) {
        facts->variables.loop_list = take_pc(r, 0x43fa);
    }
    expect(r, "2251");
    expect_pattern_base(r, advance->pattern_base);
    take_pc(r, 0x383a);
    expect_pc(r, 0x41fa, stop_mask);
    expect(r, "4250 317c 0001 fffe 47f9 00df f0a0");
    return r->ok;
}

// The same in version S, where the list and the pattern base are constants: clr.w (a0); addq.w #4,a2;
// subq.w #8,a1; cmpi.b #$ff,(a2); bne row; lea pattern_base,a2; addq.w #8,a1; cmpi.b #$ff,(a1); bne row;
// lea list,a1; lea pattern_base,a2; moveq #3,d4; lea stop_mask,a0; clr.w (a0); move.w #1,-2(a0); lea AUD0,a3
static bool read_row_routine_s(Reader* r, uint32_t stop_mask, CodeFacts* facts) {
    expect(r, "4250 584a 5149 0c12 00ff");
    take_branch(r, 0x66);
    facts->constant_pattern_base = take_pc(r, 0x45fa);
    expect(r, "5049 0c11 00ff");
    take_branch(r, 0x66);
    facts->constant_start_list = take_pc(r, 0x43fa);
    expect_pc(r, 0x45fa, facts->constant_pattern_base);
    expect(r, "7803");
    expect_pc(r, 0x41fa, stop_mask);
    expect(r, "4250 317c 0001 fffe 47f9 00df f0a0");
    return r->ok;
}

// The loop over the voices of a row, up to its dbra back to the start.
static bool read_row_body(Reader* r, uint32_t stop_mask, DzLayout* layout, CodeFacts* facts) {
    const uint32_t loop_start = r->pos;
    // d0 = pattern, d3 = transpose of the list entry; d0 *= record size
    expect(r, "1011 1629 0001");
    if (accept(r, 0xc0fc)) {
        facts->constant_record_size = take_word(r);
    } else {
        facts->variables.record_size = take_pc(r, 0x41fa);
        expect(r, "c0d0");
    }
    // d2 = instrument, d1 = note of the row
    expect(r, "1432 0800 1232 0801");
    layout->wide_instruments = accept(r, 0xe742); // asl.w #3,d2; version S: asl.w #2,d2
    if (!layout->wide_instruments) {
        expect(r, "e542");
    }
    layout->instrument_table = take_pc(r, 0x41fa);
    // AUDxLEN from the entry, AUDxLC from the sample base plus the entry's offset
    expect(r, layout->wide_instruments ? "3770 2806 0004 2430 2800" : "3770 2802 0004 3430 2800");
    layout->sample_base = take_pc(r, 0x41fa);
    expect(r, "d1c2 2688");

    // The period: with a slide, a note with bit 7 slides from the voice's last period
    layout->slide = peek(r) == 0x7400;
    if (layout->slide) {
        expect(r, "7400 3401 d243 e341");
        layout->period_table = take_pc(r, 0x41fa);
        expect(r, "3230 1800 7600 c543 0883 0007 671e");
        layout->last_period_table = take_pc(r, 0x41fa);
        expect(r, "3404 e342 3430 2800 0832 0007 0803 6704 d642 6004 9443 c742 3203");
        expect_pc(r, 0x41fa, layout->last_period_table);
        expect(r, "3404 e342 3181 2800 3741 0006 7600 7400 7200");
    } else {
        expect(r, "d243 e341");
        layout->period_table = take_pc(r, 0x41fa);
        expect(r, "3770 1800 0006 7400");
    }
    // d2 = volume, d1 = flags of the row; volume minus the fade (always 0), at least 1
    expect(r, "1432 0802 1232 0803");
    take_pc(r, 0x303a);
    expect(r, "b440 6e06 343c 0001 6002 9440 3742 0008 d6fc 0010 5449");
    // the stop mask collects the channel bits of the rows whose flags ask for it
    expect_pc(r, 0x303a, stop_mask - 2);
    layout->stop_on_zero_flags = peek(r) == 0x0c01;
    if (layout->stop_on_zero_flags) {
        expect(r, "0c01 0000");
        expect_pc(r, 0x41fa, stop_mask);
    } else {
        expect_pc(r, 0x41fa, stop_mask);
        expect(r, "0801 0000");
    }
    expect(r, "6602 d150 e340 3140 fffe");
    // dbra d4,loop
    if (!accept(r, 0x51cc) || take_target(r) != loop_start) {
        r->ok = false;
    }
    // DMACON: set bits from the variable, or from a constant in version S
    if (peek(r) == 0x33fa) {
        take_pc(r, 0x33fa);
        expect(r, "00df f096");
    } else if (peek(r) == 0x307a) {
        take_pc(r, 0x307a);
        expect(r, "33c8 00df f096");
    } else {
        expect(r, "33fc 820f 00df f096");
    }
    expect(r, "47f9 00df f0a0 4e75");
    return r->ok;
}

// The init code: assignments of pointers and words to the module's variables

// What an init path leaves in the variables, built up statement by statement. The init paths consist only of
// "lea X; move.l a0,d0" (X becomes the value), "lea V; move.l d0,(a0)" (V := value), the same with a second
// "lea W; move.l d0,(a1)", "lea V; move.w #imm,(a0)" (V := imm), and a last "lea X" that the next block takes.
typedef struct Assignments {
    const Variables* variables;
    DzSong song;
    uint32_t value;   // the pointer the last "move.l a0,d0" took
    uint32_t pending; // the pointer of the last lea
} Assignments;

static void assign_pointer(Assignments* a, uint32_t variable) {
    if (variable == a->variables->start_list) {
        a->song.start_list = a->value;
    }
    if (variable == a->variables->loop_list) {
        a->song.loop_list = a->value;
    }
    if (variable == a->variables->pattern_base) {
        a->song.pattern_base = a->value;
    }
}

static void assign_word(Assignments* a, uint32_t variable, uint16_t value) {
    if (variable == a->variables->record_size) {
        a->song.record_size = value;
    }
    if (variable == a->variables->one_shot) {
        a->song.one_shot = value == 1;
    }
}

// Runs the statements at the reader until it reaches an instruction that is none of them; false if the reader failed
static bool run_assignments(Reader* r, Assignments* a) {
    while (r->ok) {
        if (accept(r, 0x2008)) { // move.l a0,d0 of the pointer the previous block left
            a->value = a->pending;
            continue;
        }
        if (peek(r) != 0x41fa) {
            break;
        }
        const uint32_t target = take_pc(r, 0x41fa);
        a->pending = target;
        if (accept(r, 0x2008)) {
            a->value = target;
        } else if (accept(r, 0x2080)) {
            assign_pointer(a, target);
        } else if (peek(r) == 0x43fa) {
            const uint32_t second = take_pc(r, 0x43fa);
            expect(r, "2080 2280");
            assign_pointer(a, target);
            assign_pointer(a, second);
        } else if (accept(r, 0x30bc)) {
            assign_word(a, target, take_word(r));
        } else {
            break;
        }
    }
    return r->ok;
}

// The song the variables hold as the file has them, before any init code ran
static Assignments assignments_from_file(const DzModule* m, const CodeFacts* facts) {
    const Variables* v = &facts->variables;
    Assignments a = { .variables = v };
    a.song = (DzSong) {
        .start_list = dzm_u32(m, v->start_list),
        .loop_list = dzm_u32(m, v->loop_list),
        .pattern_base = v->pattern_base != 0 ? dzm_u32(m, v->pattern_base) : facts->constant_pattern_base,
        .record_size = v->record_size != 0 ? dzm_u16(m, v->record_size) : facts->constant_record_size,
        .one_shot = dzm_u16(m, v->one_shot) == 1,
        .voices = 4,
    };
    return a;
}

// Version 2: the init command stores the one song. cmpi.b #$ff,d0; bne other; <assignments>; bra init
static bool locate_song_2(const DzModule* m, const CodeFacts* facts, DzLayout* layout) {
    Reader r = { m, 0, true };
    expect(&r, "48e7 00f0");
    take_pc(&r, 0x41fa);
    expect(&r, "4cd8 0600 4a00");
    take_branch(&r, 0x67);
    expect(&r, "0c00 00ff");
    take_branch(&r, 0x66);
    Assignments a = assignments_from_file(m, facts);
    run_assignments(&r, &a);
    take_branch(&r, 0x60);
    layout->songs[0] = a.song;
    layout->song_count = 1;
    return r.ok;
}

// The subsongs of version 1 are the eagleplayer's SubSongRange: 0 .. 2 + the immediate of the first
// "cmpi.b #n,d4; bne; lea" of the code (2 when there is none). Songs 1 and 2 are the two voice songs; subsong n >= 3
// goes to the chain "d4 == n - 2", and every other subsong takes the last entry.
static int subsong_count_1(const DzModule* m) {
    int max_subsong = 2;
    uint32_t chain = 0;
    if (find_pattern(m, 0, 0x80 * 2, "0c04 xxxx 66xx 41fa", &chain)) {
        max_subsong += dzm_u16(m, chain + 2);
    }
    return max_subsong + 1 < DZ_MAX_SONGS ? max_subsong + 1 : DZ_MAX_SONGS;
}

typedef struct Leaf {
    uint16_t selector;
    uint32_t address; // the pointer the leaf loads before the shared tail
} Leaf;

#define MAX_LEAVES 8

// Version 1: the init command is in d4 (the song number as the high byte, 0xff in the low byte) for the four voice
// songs, or in d3 (0xff with d1 = 0 or not 0) for the two voice songs.
static bool locate_songs_1(const DzModule* m, const CodeFacts* facts, DzLayout* layout) {
    Reader r = { m, 0, true };
    expect(&r, "48e7 00f0");
    take_pc(&r, 0x41fa);
    expect(&r, "4cd8 0600 4a44");
    const uint32_t two_voice_commands = take_branch(&r, 0x67);

    // d4 commands: assignments for every song, then the chain of "d4 == n" leaves and the last leaf, then the tail
    // that every leaf shares
    const Assignments file = assignments_from_file(m, facts);
    Assignments common = file;
    run_assignments(&r, &common);
    expect(&r, "7000 1004 e04c 0c00 00ff");
    take_branch(&r, 0x66);
    Leaf leaves[MAX_LEAVES];
    int leaf_count = 0;
    while (r.ok && leaf_count < MAX_LEAVES && accept(&r, 0x0c04)) {
        leaves[leaf_count].selector = take_word(&r);
        take_branch(&r, 0x66);
        leaves[leaf_count].address = take_pc(&r, 0x41fa);
        take_branch(&r, 0x60);
        leaf_count++;
    }
    const uint32_t last_leaf = take_pc(&r, 0x41fa);
    const uint32_t tail = r.pos;
    if (!r.ok) {
        return false;
    }

    // d3 commands: tst.b d3; beq done; move.w d3,d0; cmpi.b #$ff,d0; bne other; tst.w d1; bne second; <first song>
    // bra tail; <second song, which runs into the tail>
    Reader two_voice = { m, two_voice_commands, true };
    expect(&two_voice, "7000 4a03");
    take_branch(&two_voice, 0x67);
    expect(&two_voice, "3003 0c00 00ff");
    take_branch(&two_voice, 0x66);
    expect(&two_voice, "4a41");
    const uint32_t second_song = take_branch(&two_voice, 0x66);
    Assignments first = file;
    run_assignments(&two_voice, &first);
    const uint32_t first_tail = take_branch(&two_voice, 0x60);
    Assignments second = file; // the second song's statements run into the shared tail
    Reader second_code = { m, second_song, true };
    Reader first_tail_code = { m, first_tail, true };
    if (!two_voice.ok || !run_assignments(&second_code, &second) || !run_assignments(&first_tail_code, &first)) {
        return false;
    }
    first.song.voices = 2;
    second.song.voices = 2;

    layout->song_count = subsong_count_1(m);
    for (int i = 0; i < layout->song_count; i++) {
        if (i == 1 || i == 2) {
            layout->songs[i] = i == 1 ? first.song : second.song;
            continue;
        }
        Assignments a = common;
        a.pending = last_leaf;
        for (int j = 0; i >= 3 && j < leaf_count; j++) {
            if (leaves[j].selector == i - 2) {
                a.pending = leaves[j].address;
            }
        }
        Reader tail_code = { m, tail, true };
        if (!run_assignments(&tail_code, &a)) {
            return false;
        }
        layout->songs[i] = a.song;
    }
    return true;
}

bool dz_locate(const DzModule* m, DzLayout* layout) {
    *layout = (DzLayout) { 0 };
    if (!check_version(m, &layout->version) || layout->version == DzVersion_3) {
        return false;
    }
    uint32_t stop_mask = 0;
    if (!locate_stop_tick(m, layout, &stop_mask)) {
        return false;
    }

    // The row routine follows the stop tick's code
    uint32_t row_routine = 0;
    const char* advance_pattern = layout->version == DzVersion_S ? "4250 584a 5149" : "4250 584a 92fa";
    if (!find_pattern(m, 0, SEARCH_LIMIT - 2, advance_pattern, &row_routine)) {
        return false;
    }
    Reader r = { m, row_routine, true };
    CodeFacts facts = { 0 };
    if (layout->version == DzVersion_S) {
        if (!read_row_routine_s(&r, stop_mask, &facts)) {
            return false;
        }
    } else {
        const RowAdvance advance = read_row_advance(&r);
        facts.variables.pattern_base = advance.pattern_base.variable ? advance.pattern_base.address : 0;
        facts.variables.one_shot = advance.one_shot;
        facts.constant_pattern_base = advance.pattern_base.variable ? 0 : advance.pattern_base.address;
        if (!r.ok || !read_row_entry(&r, &advance, stop_mask, &facts)) {
            return false;
        }
    }
    if (!read_row_body(&r, stop_mask, layout, &facts)) {
        return false;
    }

    layout->silent_init = layout->version != DzVersion_S;
    bool located = false;
    switch (layout->version) {
        case DzVersion_S:
            layout->songs[0] = (DzSong) { .start_list = facts.constant_start_list,
                                          .loop_list = facts.constant_start_list,
                                          .pattern_base = facts.constant_pattern_base,
                                          .record_size = facts.constant_record_size,
                                          .voices = 4 };
            layout->song_count = 1;
            located = true;
            break;
        case DzVersion_1:
            located = locate_songs_1(m, &facts, layout);
            break;
        case DzVersion_2:
            located = locate_song_2(m, &facts, layout);
            break;
        case DzVersion_3:
            break;
    }
    // The version 1 eagleplayer stops a four voice song for good when its list ran out
    for (int i = 0; i < layout->song_count; i++) {
        layout->songs[i].ends_at_list_end = layout->version == DzVersion_1 && layout->songs[i].voices == 4;
    }
    return located;
}
